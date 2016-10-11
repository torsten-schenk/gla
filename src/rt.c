#include <apr_env.h>
#include <unistd.h>
#include <assert.h>
#include <apr_strings.h>
#include <apr_file_io.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>

#include "common.h"

#ifndef _PRINT_INT_PREC
#define _PRINT_INT_PREC
#endif

#include "storage/module.h"
#include "xml/module.h"
#include "csv/module.h"
#include "util/module.h"
#include "io/module.h"
#include "sqstdlib/module.h"

#include "log.h"
#include "_mount.h"
#include "mnt_internal.h"
#include "mnt_dir.h"
#include "mnt_registry.h"
#include "mnt_stdio.h"
#include "entityreg.h"
#include "packreg.h"
#include "store.h"
#include "rt.h"

/* TODO check for pool argument names. Rename to 'tmp' and 'pool' or 'tmp' and 'perm' or similar */

/* CAUTION we use 'int' as returned type, even though
 * squirrel has a typedef SQRESULT and apr a typedef
 * apr_status_t. To avoid having multiple return variables,
 * simply assume that they are typedef'd to 'int' henceforth.
 * Maybe add a check here that warns at
 * compile time if the types are not 'int' */

#define VM_INITIAL_STACK_SIZE 65536
#define XML_BUFFER_SIZE 1024

enum {
	METHOD_UNKNOWN,
	METHOD_COMPILE, /* compile entity then call the result */
	METHOD_LOAD, /* load the previously compiled data and call the result */
	METHOD_PUSH, /* use the io->push method to push the object to the stack */
	METHOD_DYNLIB /* use dlopen() to load dynamic library */
};

enum {
	GLA_MOUNT_ACTIVE = 0x80000000
};

typedef struct {
	const char *buffer;
	size_t offset;
	size_t size;
} string_stream_t;

typedef struct {
	const void *key;
	void *data;
} data_entry_t;

typedef struct {
	HSQOBJECT object;
} import_entry_t;

typedef struct mnt_list {
	gla_mount_t *mount;
	struct mnt_list *next;
} mnt_list_t;

static struct {
	const char *extension;
	int method;
} search_extension[] = {
	{ .extension = GLA_ENTITY_EXT_OBJECT, .method = METHOD_PUSH },
	{ .extension = GLA_ENTITY_EXT_COMPILED, .method = METHOD_LOAD },
	{ .extension = GLA_ENTITY_EXT_SCRIPT, .method = METHOD_COMPILE },
	{ .extension = GLA_ENTITY_EXT_DYNLIB, .method = METHOD_DYNLIB }
};

static apr_status_t cleanup_mod(
		void *handle)
{
	dlclose(handle);
	return APR_SUCCESS;
}

static inline int cmp_ptr(
		btree_t *btree,
		const void *a_,
		const void *b_,
		void *group)
{
	const data_entry_t *a = a_;
	const data_entry_t *b = b_;
	return memcmp(&a->key, &b->key, sizeof(const void*));
}

static inline apr_pool_t *begin(
		gla_rt_t *rt)
{
	apr_pool_t *parent = rt->mpstack;

	apr_pool_create(&rt->mpstack, parent);
	if(rt->mpstack == NULL) {
		rt->mpstack = parent;
		return NULL;
	}
	else
		return rt->mpstack;
}

static inline int error_nomem(
		gla_rt_t *rt)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return GLA_ALLOC;
}

static inline int error(
		gla_rt_t *rt,
		int err,
		const char *fmt, ...)
{
	va_list ap;
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	printf("\n");
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return err;
}

static inline void warn(
		gla_rt_t *rt,
		const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	printf("\n");
}

static inline int error_apr(
		gla_rt_t *rt,
		apr_status_t status)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return -status;
}

static inline int error_ret(
		gla_rt_t *rt,
		int ret)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return ret;
}

static inline int error_errno(
		gla_rt_t *rt)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return -errno;
}

static inline int success(
		gla_rt_t *rt)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return 0;
}

static SQInteger string_lexfeed(
		SQUserPointer strstream_)
{
	string_stream_t *strstream = strstream_;
	if(strstream->offset == strstream->size)
		return 0;
	return strstream->buffer[strstream->offset++];
}

static SQInteger io_lexfeed(
		SQUserPointer io_)
{
	gla_io_t *io = io_;
	unsigned char c;
	int ret;
	ret = gla_io_read(io, &c, 1);
	if(ret == 0) {
		ret = gla_io_rstatus(io);
		if(ret == GLA_END)
			return 0;
		else {
			/* TODO return what exactly here? 0 for end or negative value for error? Also rethink fprintf */
			fprintf(stderr, "error reading from script entity");
			return SQ_ERROR;
		}
	}
	return c;
}

static SQInteger file_lexfeed(
		SQUserPointer file_)
{
	FILE *file = file_;
	unsigned char c;
	int ret;

	ret = fread(&c, 1, 1, file);
	if(ret != 1) {
		if(feof(file))
			return 0;
		else {
			/* TODO return what exactly here? 0 for end or negative value for error? Also rethink fprintf */
			fprintf(stderr, "error reading from script file");
			return SQ_ERROR;
		}
	}
	return c;
}

static SQInteger io_write(
		SQUserPointer io_,
		SQUserPointer buffer,
		SQInteger size)
{
	gla_io_t *io = io_;
	int ret;

	if(size == 0)
		return 0;
	else {
		ret = gla_io_write(io, buffer, size);
		if(ret == 0)
			return SQ_ERROR;
		return ret;
	}
}

static void compile_error_handler(
		HSQUIRRELVM vm,
		const SQChar *desc,
		const SQChar *source,
		SQInteger line,
		SQInteger column)
{
/*	gla_rt_t *rt = sq_getforeignptr(vm); */
	/* TODO put error message into rt */
	fprintf(stderr, "Error in %s, line %d, column %d: %s\n", source, (int)line, (int)column, desc);
}

static void print_handler(
		HSQUIRRELVM vm,
		const SQChar *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static void error_handler(
		HSQUIRRELVM vm,
		const SQChar *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	printf("\n");
}

 
/* TODO inspect&improve; copied from internet */
static void dump_execstack(
		HSQUIRRELVM vm,
		const char *info)
{
	SQStackInfos stack_info;
	SQInteger stack_depth = 1;
 
	if(info == NULL)
		printf("---------------------------------------- EXECSTACK DUMP ---------------------------------\n");
	else {
		int minuses = (80 - 19 - strlen(info)) / 2;
		int extra = (80 - 19 - strlen(info)) % 2;
		int i;
		for(i = 0; i < minuses; i++)
			printf("-");
		printf(" EXECSTACK DUMP (%s) ", info);
		for(i = 0; i < minuses + extra; i++)
			printf("-");
		printf("\n");
	}
	while(SQ_SUCCEEDED(sq_stackinfos(vm, stack_depth, &stack_info))) {
		const SQChar *func_name = stack_info.funcname != NULL ? stack_info.funcname : "unknown_function";
		const SQChar *source_file = stack_info.source != NULL ? stack_info.source : "unknown_source_file";
		printf("  [%d]: function [%s()] %s line [%d]\n", (int)stack_depth, func_name, source_file, (int)stack_info.line);
		stack_depth++;
	}
	printf("-----------------------------------------------------------------------------------------\n");
}

static void dump_value_simple(
		gla_rt_t *rt,
		int idx,
		bool full,
		apr_pool_t *pool)
{
	HSQUIRRELVM vm = rt->vm;
	SQObjectType type;
	SQInteger vint;
	SQFloat vfloat;
	SQBool vbool;
	SQUserPointer vup;
	const SQChar *vstring;
	HSQOBJECT obj;
	int ret;

	type = sq_gettype(vm, idx);
	sq_getstackobj(vm, idx, &obj);
	switch(type) {
		case OT_NULL:
			printf("null");
			break;
		case OT_INTEGER:
			sq_getinteger(vm, idx, &vint);
			if(full)
				printf("integer: ");
			printf("%" _PRINT_INT_PREC "d", vint);
			break;
		case OT_FLOAT:
			sq_getfloat(vm, idx, &vfloat);
			if(full)
				printf("float: ");
			printf("%f", vfloat);
			break;
		case OT_BOOL:
			sq_getbool(vm, idx, &vbool);
			if(full)
				printf("bool: ");
			printf("%s", vbool ? "true" : "false");
			break;
		case OT_STRING:
			sq_getstring(vm, idx, &vstring);
			if(full)
				printf("string: '%s' (%p)", vstring, obj._unVal.pString);
			else
				printf("'%s'", vstring);
			break;
		case OT_TABLE:
			if(full)
				printf("table (%p)", obj._unVal.pTable);
			else
				printf("table");
			break;
		case OT_ARRAY:
			if(full) {
				vint = sq_getsize(vm, idx);
				printf("array len=%" _PRINT_INT_PREC "d (%p)", vint, obj._unVal.pArray);
			}
			else
				printf("array");
			break;
		case OT_USERDATA:
			printf("userdata");
			if(full) {
				ret = sq_gettypetag(vm, idx, &vup);
				if(ret == SQ_OK && vup != NULL) {
					gla_path_t path;
					gla_entityreg_path_for(rt->reg_imported, &path, vup, pool);
					path.extension = NULL;
					printf(" %s", gla_path_tostring(&path, pool));
					apr_pool_clear(pool);
				}
				printf(" (%p)", obj._unVal.pUserData);
			}
			break;
		case OT_CLOSURE:
			if(full)
				printf("closure (%p)", obj._unVal.pClosure);
			else
				printf("closure");
			break;
		case OT_NATIVECLOSURE:
			if(full)
				printf("native closure (%p)", obj._unVal.pNativeClosure);
			else
				printf("native closure");
			break;
		case OT_GENERATOR:
			printf("generator (%p)", obj._unVal.pGenerator);
			break;
		case OT_USERPOINTER:
			if(full)
				printf("userpointer: %p", obj._unVal.pUserPointer);
			else
				printf("userpointer");
			break;
		case OT_THREAD:
			if(full)
				printf("thread (%p)", obj._unVal.pThread);
			else
				printf("thread");
			break;
		case OT_CLASS:
			printf("class");
			if(full) {
				ret = sq_gettypetag(vm, idx, &vup);
				if(ret == SQ_OK && vup != NULL) {
					gla_path_t path;
					gla_entityreg_path_for(rt->reg_imported, &path, vup, pool);
					path.extension = NULL;
					printf(" %s", gla_path_tostring(&path, pool));
					apr_pool_clear(pool);
				}
				printf(" (%p)", obj._unVal.pClass);
			}
			break;
		case OT_INSTANCE:
			printf("instance");
			if(full) {
				ret = sq_gettypetag(vm, idx, &vup);
				if(ret == SQ_OK && vup != NULL) {
					gla_path_t path;
					gla_entityreg_path_for(rt->reg_imported, &path, vup, pool);
					path.extension = NULL;
					printf(" %s", gla_path_tostring(&path, pool));
					apr_pool_clear(pool);
				}
				printf(" (%p)", obj._unVal.pInstance);
			}
			break;
		case OT_WEAKREF:
			if(full)
				printf("weak ref (%p)", obj._unVal.pWeakRef);
			else
				printf("weak ref");
			break;
		default:
			printf("[unknown type]");
			break;
	}
}

static void dump_opstack(
		HSQUIRRELVM vm,
		const char *info)
{
	SQInteger i;
	int ret;
	gla_rt_t *rt = sq_getforeignptr(vm);
	apr_pool_t *pool;

	if(info == NULL)
		printf("----------------------------------------- OPSTACK DUMP ----------------------------------\n");
	else {
		int minuses = (80 - 17 - strlen(info)) / 2;
		int extra = (80 - 17 - strlen(info)) % 2;
		int i;
		for(i = 0; i < minuses; i++)
			printf("-");
		printf(" OPSTACK DUMP (%s) ", info);
		for(i = 0; i < minuses + extra; i++)
			printf("-");
		printf("\n");
	}
	ret = apr_pool_create(&pool, rt->mpstack);
	if(ret != APR_SUCCESS)
		printf("  ERROR creating temporary memory pool\n");
	else
		for(i = 1; i <= sq_gettop(vm); i++) {
			printf("% 4" _PRINT_INT_PREC "d % 5" _PRINT_INT_PREC "d  ", i, -sq_gettop(vm) + i - 1);
			dump_value_simple(rt, i, true, pool);
			printf("\n");
			apr_pool_clear(pool);
		}
	printf("--------------------------------------------------------------------------------\n");
}

static void dump_value_recursion(
		gla_rt_t *rt,
		int indent,
		int idx,
		int maxdepth,
		bool full,
		apr_pool_t *pool)
{
	HSQUIRRELVM vm = rt->vm;
	SQObjectType type;
	int ret;
	int i;

	if(idx < 0)
		idx = sq_gettop(vm) + idx + 1;
	if(maxdepth == 0) {
		printf("...\n");
		return;
	}

	dump_value_simple(rt, idx, full, pool);
	apr_pool_clear(pool);
	printf("\n");

	type = sq_gettype(vm, idx);
	switch(type) {
		case OT_ARRAY:
			sq_pushnull(vm);
			for(ret = sq_next(vm, idx); ret == SQ_OK; ret = sq_next(vm, idx)) {
				for(i = 0; i < indent + 1; i++)
					printf("  ");
				dump_value_simple(rt, -2, false, pool);
				apr_pool_clear(pool);
				if(full)
					printf(" --> ");
				else
					printf(": ");
				dump_value_recursion(rt, indent + 1, -1, maxdepth - 1, full, pool);
				sq_pop(vm, 2);
			}
			sq_pop(vm, 1);
			break;
		case OT_TABLE:
		case OT_CLASS:
			sq_pushnull(vm);
			for(ret = sq_next(vm, idx); ret == SQ_OK; ret = sq_next(vm, idx)) {
				for(i = 0; i < indent + 1; i++)
					printf("  ");
				dump_value_simple(rt, -2, full, pool);
				apr_pool_clear(pool);
				if(full)
					printf(" --> ");
				else
					printf(": ");
				dump_value_recursion(rt, indent + 1, -1, maxdepth - 1, full, pool);
				sq_pop(vm, 2);
			}
			sq_pop(vm, 1);
			break;
		default:
			break;
	}
}

static void dump_value(
		HSQUIRRELVM vm,
		int idx,
		int maxdepth,
		bool full,
		const char *info) /* TODO use pool as argument; same for dump_opstack */
{
	apr_pool_t *pool;
	gla_rt_t *rt = sq_getforeignptr(vm);
	int ret;

	ret = apr_pool_create(&pool, rt->mpstack);
	if(ret != APR_SUCCESS)
		printf("  ERROR creating temporary memory pool\n");
	if(info == NULL)
		printf("---------------------------------------- VALUE DUMP ---------------------------------\n");
	else {
		int minuses = (80 - 15 - strlen(info)) / 2;
		int extra = (80 - 15 - strlen(info)) % 2;
		int i;
		for(i = 0; i < minuses; i++)
			printf("-");
		printf(" VALUE DUMP (%s) ", info);
		for(i = 0; i < minuses + extra; i++)
			printf("-");
		printf("\n");
	}
	dump_value_recursion(rt, 0, idx, maxdepth, full, pool);
	printf("-------------------------------------------------------------------------------------\n");
}

/* TODO inspect&improve; copied from internet */
static SQInteger runtime_error_handler(
		HSQUIRRELVM vm)
{
	const SQChar* error_message = NULL;
	if(sq_gettop(vm) >= 1) {
		if(SQ_SUCCEEDED(sq_getstring(vm, 2, &error_message)))
			fprintf(stderr, "sq runtime error: %s.\n", error_message);
		dump_execstack(vm, "error");
	}
	return 0;
}

static int mnt_insert(
		gla_packreg_t *registry,
		const gla_path_t *path,
		gla_mount_t *mount,
		int flags)
{
	gla_id_t package_id;
	mnt_list_t **head;
	mnt_list_t *cur;
	gla_store_t *store = gla_packreg_store(registry);

	if(gla_mount_depth(mount) != -1)
		return GLA_ALREADY;
	else if(path != NULL && gla_path_type(path) != GLA_PATH_PACKAGE)
		return GLA_INVALID_PATH_TYPE;

	package_id = gla_store_path_get(store, path);
	if(package_id == GLA_ID_INVALID && errno != GLA_SUCCESS)
		return errno;
	head = gla_packreg_get_id(registry, package_id);
	if(head == NULL)
		return errno;
	cur = apr_pcalloc(gla_packreg_pool(registry), sizeof(*cur));
	cur->next = *head;
	cur->mount = mount;
	mount->flags = flags | gla_mount_features(mount);
	mount->depth = path == NULL ? 0 : path->size - path->shifted;
	*head = cur;
	return GLA_SUCCESS;
}

static int mnt_resolve(
		gla_packreg_t *registry,
		gla_mount_t **mount,
		gla_path_t *path,
		int flags, /* considered: GLA_MOUNT_SOURCE and GLA_MOUNT_TARGET */
		int *shifted,
		apr_pool_t *pool)
{
	gla_id_t package_id;
	gla_path_t package_path;
	gla_pathinfo_t info;
	int depth;
	gla_store_t *store = gla_packreg_store(registry);
	mnt_list_t **head;
	mnt_list_t *cur;
	int ret;

	if(path == NULL)
		gla_path_root(&package_path);
	else
		package_path = *path;
	gla_path_package(&package_path);
	package_id = gla_store_path_deepest(store, &package_path, &depth);
	gla_path_shift_many(path, depth);
	while(depth >= 0) {
		head = gla_packreg_try_id(registry, package_id);
		if(head != NULL) {
			cur = *head;
			while(cur != NULL) {
				if((cur->mount->flags & GLA_MOUNT_ACTIVE) != 0 && ((flags & GLA_MOUNT_FILESYSTEM) == 0 || (cur->mount->flags & GLA_MOUNT_FILESYSTEM) != 0) && ((flags & GLA_MOUNT_SOURCE) == 0 || (cur->mount->flags & GLA_MOUNT_SOURCE) != 0) && ((flags & GLA_MOUNT_TARGET) == 0 || (cur->mount->flags & GLA_MOUNT_TARGET) != 0)) { /* if a flag is requested (in arg 'flags'), it must also be set for current mount handler (in 'cur->mount->flags') */
					ret = gla_mount_info(cur->mount, &info, path, pool);
					if(ret == GLA_SUCCESS) {
						if(shifted != NULL)
							*shifted = depth;
						if(mount != NULL)
							*mount = cur->mount;
						return GLA_SUCCESS;
					}
					else if(ret == GLA_NOTFOUND) {
						if((flags & GLA_MOUNT_TARGET) != 0 && (cur->mount->flags & GLA_MOUNT_TARGET) != 0 && (flags & GLA_MOUNT_CREATE) != 0 && info.can_create) {
							if(shifted != NULL)
								*shifted = depth;
							if(mount != NULL)
								*mount = cur->mount;
							return GLA_SUCCESS;
						}
					}
				}
				cur = cur->next;
			}
		}
		depth--;
		package_id = gla_store_path_parent(store, package_id);
		gla_path_unshift(path);
	}
	return GLA_NOTFOUND;
}

static int description_run(
		gla_rt_t *rt,
		gla_mount_t *mount,
		const gla_path_t *entity, /* NOTE: this package is NOT absolute but relative to 'mount' */
		const char *entity_name,
		int method,
		apr_pool_t *pool)
{

}

static int mount_run(
		gla_rt_t *rt,
		gla_mount_t *mount,
		const gla_path_t *entity,
		const char *entity_name,
		int method,
		apr_pool_t *pool)
{
	gla_io_t *io;
	int ret;

	switch(method) {
		case METHOD_COMPILE:
			io = gla_mount_open(mount, entity, GLA_MODE_READ, pool);
			if(io == NULL)
				return errno;
			ret = sq_compile(rt->vm, io_lexfeed, io, entity_name, true);
			if(ret != SQ_OK)
				return GLA_SYNTAX_ERROR;

			sq_pushroottable(rt->vm);
			ret = sq_call(rt->vm, 1, true, true);
			if(ret != SQ_OK)
				return GLA_SEMANTIC_ERROR;
			sq_remove(rt->vm, -2);
			return GLA_SUCCESS;
		case METHOD_LOAD:
			return GLA_NOTSUPPORTED;
		case METHOD_PUSH:
			ret = gla_mount_push(mount, rt, entity, pool);
			if(ret != GLA_SUCCESS)
				return ret;
			return GLA_SUCCESS;
		case METHOD_UNKNOWN: /* _init not found, so do nothing */
			return GLA_NOTFOUND;
		default:
			assert(false);
			return GLA_INTERNAL;
	}
}

static int pack_init(
		gla_rt_t *rt,
		const gla_path_t *package,
		apr_pool_t *pool)
{
	gla_packreg_get(rt->reg_initialized, package);
	if(errno == GLA_NOTFOUND) {
		gla_path_t path;
		gla_path_t relpath;
		int i;
		int method = METHOD_UNKNOWN;
		int ret;
		gla_mount_t *mount;

		if(package == NULL)
			gla_path_root(&path);
		else
			path = *package;

		ret = gla_path_parse_append_entity(&path, "_init", pool);
		if(ret != GLA_SUCCESS)
			return ret;
		for(i = 0; i < ARRAY_SIZE(search_extension); i++) {
			path.extension = search_extension[i].extension;
			relpath = path;
			ret = mnt_resolve(rt->reg_mounted, &mount, &relpath, GLA_MOUNT_SOURCE, NULL, pool);
			if(ret == GLA_SUCCESS) { /* yes, create an entry in the import registry */
				method = search_extension[i].method;
				break;
			}
		}
		path.extension = NULL; /* omit extension in entity name */
		ret = mount_run(rt, mount, &relpath, gla_path_tostring(&path, pool), method, pool); /* TODO function name is "mount_run"; either create "init_run" or unify run process */
		if(ret == GLA_SUCCESS) {
			sq_poptop(rt->vm);
			return GLA_SUCCESS;
		}
		else if(ret == GLA_NOTFOUND)
			return GLA_SUCCESS;
		else
			return ret;
	}
	else
		return GLA_SUCCESS;
}

static SQInteger fn_debug_error(
		HSQUIRRELVM vm)
{
	const SQChar *message;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) == 1)
		message = NULL;
	else if(SQ_FAILED(sq_getstring(vm, 2, &message)))
		message = NULL;
	printf("DEBUG error: %s\n", message);
	return gla_rt_vmthrow(rt, message);
}

static SQInteger fn_now(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(rt->vm) != 1)
		return gla_rt_vmthrow(rt, "invalid argument count");
	
	sq_pushinteger(rt->vm, apr_time_now());
	return gla_rt_vmsuccess(rt, true);

}

static SQInteger fn_env_get(
		HSQUIRRELVM vm)
{
	int ret;
	const SQChar *name;
	char *value;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(rt->vm) != 2)
		return gla_rt_vmthrow(rt, "invalid argument count");
	else if(SQ_FAILED(sq_getstring(rt->vm, 2, &name)))
		return gla_rt_vmthrow_null(rt);
	
	ret = apr_env_get(&value, name, rt->mpstack);
	if(ret == APR_SUCCESS)
		sq_pushstring(rt->vm, value, -1);
	else
		sq_pushnull(rt->vm);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_entityof(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	gla_path_t path;
	SQUserPointer up;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	switch(sq_gettype(vm, 2)) {
		case OT_INSTANCE:
		case OT_TABLE:
			sq_pushstring(vm, GLA_ENTITYOF_SLOT, -1);
			if(SQ_FAILED(sq_rawget(vm, 2)))
				return gla_rt_vmthrow(rt, "Invalid argument 1: missing instance/table entityof slot");
			break;
		case OT_CLASS:
		case OT_USERDATA:
			if(SQ_FAILED(sq_gettypetag(vm, 2, &up)))
				return gla_rt_vmthrow(rt, "Error getting typetag");
			if(up == NULL)
				sq_pushnull(vm);
			else {
				gla_entityreg_path_for(rt->reg_imported, &path, up, rt->mpstack);
				path.extension = NULL;
				sq_pushstring(vm, gla_path_tostring(&path, rt->mpstack), -1);
			}
			break;
		default:
			return gla_rt_vmthrow(rt, "Invalid argument 1: expected value that can have an entity assigned to it");
	}
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_sleep(
		HSQUIRRELVM vm)
{
	int argn = sq_gettop(vm) - 1;
	SQInteger value;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(argn != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &value)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	sleep(value);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_usleep(
		HSQUIRRELVM vm)
{
	int argn = sq_gettop(vm) - 1;
	SQInteger value;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(argn != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &value)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	usleep(value);
	return gla_rt_vmsuccess(rt, false);
}

/* compile a script entity TODO move this function into a module */
static SQInteger fn_compile(
		HSQUIRRELVM vm)
{
	const char *entity;
	int ret;
	gla_path_t path;
	gla_mount_t *mount;
	gla_io_t *io;

	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2) /* TODO allow an optional 2nd argument which specifies target entity */
		return gla_rt_vmthrow(rt, "invalid argument count");

	ret = gla_path_get_entity(&path, false, rt, 2, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "invalid entity identifier");
	entity = gla_path_tostring(&path, rt->mpstack);

	if(path.extension == NULL)
		path.extension = GLA_ENTITY_EXT_SCRIPT;
	else if(strcmp(path.extension, "nut") != 0)
		return gla_rt_vmthrow(rt, "currently only <nut> entities may be compiled"); /* TODO remove this limitation */

	ret = mnt_resolve(rt->reg_mounted, &mount, &path, GLA_MOUNT_SOURCE, NULL, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "could not resolve source entity");
	io = gla_mount_open(mount, &path, GLA_MODE_READ, rt->mpstack);
	if(io == NULL)
		return gla_rt_vmthrow(rt, "error opening source entity");
	ret = sq_compile(rt->vm, io_lexfeed, io, entity, true);
	if(ret != SQ_OK)
		return gla_rt_vmthrow(rt, "error compiling script");

	path.extension = GLA_ENTITY_EXT_COMPILED;
	io = gla_mount_open(mount, &path, GLA_MODE_WRITE, rt->mpstack); /* TODO where to put compiled entity? HERE: same directory as entity itself; MAYBE: use target mount */
	if(io == NULL)
		return gla_rt_vmthrow(rt, "error opening target entity");
	ret = sq_writeclosure(vm, io_write, io);
	if(ret != SQ_OK)
		return gla_rt_vmthrow(rt, "error writing compiled entity");
	return gla_rt_vmsuccess(rt, true);
}

/* TODO see path.h, comment
 * - move module io to this directory, i.e. delete module io and consider it as part of kernel 
 * - refactor function fn_open() in packpath.c to here
 * - use gla_path_get_entity() to retrieve entity here */
static SQInteger fn_open(
		HSQUIRRELVM vm)
{
	int i;
	int argn = sq_gettop(vm) - 1;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	sq_pushstring(vm, "_open", -1);
	if(argn < 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Currently only path instances can be opened");
	sq_push(vm, 2);
	for(i = 0; i < argn - 1; i++)
		sq_push(vm, 3 + i);
	if(SQ_FAILED(sq_call(vm, argn, true, true)))
		return gla_rt_vmthrow(rt, "Error calling open function");
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_eval(
		HSQUIRRELVM vm)
{
	const SQChar *string;
	string_stream_t stream;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &string)))
		return gla_rt_vmthrow(rt, "invalid argument 1: expected string");

	memset(&stream, 0, sizeof(stream));
	stream.buffer = string;
	stream.size = strlen(string);
	if(SQ_FAILED(sq_compile(vm, string_lexfeed, &stream, "<eval>", true)))
		return gla_rt_vmthrow(rt, "error compiling script");

	sq_pushroottable(rt->vm);
	if(SQ_FAILED(sq_call(rt->vm, 1, true, true))) /* TODO use arguments?; final bool args: 'retval' and 'vmthrowerror'  */
		return gla_rt_vmthrow(rt, "error running script"); /* TODO call sq_getlasterror() and throw message on top of stack */
	return gla_rt_vmsuccess(rt, true);
}

/* arguments:
 *   entity (string): the entity to run. if no extension is given, a <cnut> file is looked up first.
 *     If no such entity exists, a <nut> entity is looked up. */
static SQInteger fn_run(
		HSQUIRRELVM vm)
{
	int ret;
	int i;
	gla_path_t path; /* absolute path */
	gla_path_t relpath; /* relative path (starting at mount root) */
	gla_path_t packpath;
	const char *entity;
	gla_mount_t *mount;
	gla_io_t *io;
	int method = METHOD_UNKNOWN;

	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2) /* TODO optional 2nd argument: constrain behaviour (see resolve order) */
		return gla_rt_vmthrow(rt, "invalid argument count");
	ret = gla_path_get_entity(&path, false, rt, 2, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "error parsing path for entity");
	packpath = path;
	gla_path_package(&packpath);
	ret = pack_init(rt, &packpath, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "error executing package init");

	if(path.extension != NULL) {
		if(strcmp(path.extension, GLA_ENTITY_EXT_SCRIPT) == 0)
			method = METHOD_COMPILE;
		else if(strcmp(path.extension, GLA_ENTITY_EXT_COMPILED) == 0)
			method = METHOD_LOAD;
		else if(strcmp(path.extension, GLA_ENTITY_EXT_OBJECT) == 0)
			method = METHOD_PUSH;
		else
			return gla_rt_vmthrow(rt, "invalid entity extension");
		relpath = path;
		ret = mnt_resolve(rt->reg_mounted, &mount, &relpath, GLA_MOUNT_SOURCE, NULL, rt->mpstack);
		if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "no such entity");
	}
	else
		for(i = 0; i < ARRAY_SIZE(search_extension); i++) {
			path.extension = search_extension[i].extension;
			relpath = path;
			ret = mnt_resolve(rt->reg_mounted, &mount, &relpath, GLA_MOUNT_SOURCE, NULL, rt->mpstack);
			if(ret == GLA_SUCCESS) {
				method = search_extension[i].method;
				break;
			}
		}
	switch(method) {
		case METHOD_COMPILE:
			io = gla_mount_open(mount, &relpath, GLA_MODE_READ, rt->mpstack);
			if(io == NULL)
				return gla_rt_vmthrow(rt, "error opening entity");
			path.extension = NULL;
			entity = gla_path_tostring(&path, rt->mpstack);
			ret = sq_compile(vm, io_lexfeed, io, entity, true);
			if(ret != SQ_OK)
				return gla_rt_vmthrow(rt, "error compiling script");

			sq_pushroottable(rt->vm);
			ret = sq_call(rt->vm, 1, true, true); /* TODO use arguments?; final bool args: 'retval' and 'vmthrowerror'  */
			if(ret != SQ_OK)
				return gla_rt_vmthrow(rt, "error running script"); /* TODO call sq_getlasterror() and throw message on top of stack */
			sq_remove(rt->vm, -2);
			break;
		case METHOD_LOAD:
			return gla_rt_vmthrow(rt, "loading compiled entity: not yet implemented");
		case METHOD_PUSH:
			ret = gla_mount_push(mount, rt, &relpath, rt->mpstack);
			if(ret != GLA_SUCCESS)
				return gla_rt_vmthrow(rt, "error pushing entity to stack");
			break;
		case METHOD_UNKNOWN:
			return gla_rt_vmthrow(rt, "no such entity");
		default:
			assert(false);
	}

	return gla_rt_vmsuccess(rt, true);
}

/* arguments:
 *   entity/package (string): the entity to import.
 *     If no extension is given, an entity <(c)nut>[arg]._import is treated first.
 *     If no such entity exists, an entity <(c)nut>[arg] is treated next. */
static SQInteger fn_import(
		HSQUIRRELVM vm)
{
	int ret;
	gla_path_t path;

	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2) /* TODO optional 2nd argument: constrain behaviour (see resolve order) */
		return gla_rt_vmthrow(rt, "invalid argument count");
	ret = gla_path_get_entity(&path, false, rt, 2, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "error parsing path for entity");
	if(path.extension != NULL)
		return gla_rt_vmthrow(rt, "entity extension must not be given for imports");

	ret = gla_rt_import(rt, &path, rt->mpstack);
	if(ret == GLA_SUCCESS)
		return gla_rt_vmsuccess(rt, true);
	else
		return gla_rt_vmthrow(rt, "error importing entity");
}

static SQInteger fn_dump_opstack(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	dump_opstack(rt->vm, NULL);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_dump_execstack(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	dump_execstack(rt->vm, NULL);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_dump_value(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	SQInteger maxdepth = 5;
	int ret;
	if(sq_gettop(vm) == 3) {
		ret = sq_getinteger(vm, 3, &maxdepth);
		if(ret != SQ_OK)
			return gla_rt_vmthrow(rt, "invalid argument 2: expected integer");
	}
	else if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "invalid argument count");
	dump_value(rt->vm, 2, maxdepth, false, NULL);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_dump_full(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	SQInteger maxdepth = 5;
	int ret;
	if(sq_gettop(vm) == 3) {
		ret = sq_getinteger(vm, 3, &maxdepth);
		if(ret != SQ_OK)
			return gla_rt_vmthrow(rt, "invalid argument 2: expected integer");
	}
	else if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "invalid argument count");
	dump_value(rt->vm, 2, maxdepth, true, NULL);
	return gla_rt_vmsuccess(rt, false);
}

static const char script_hello_world[] =
	"print(\"Hello world!\\n\");\n";

static int mnt_internal_setup(
		gla_rt_t *rt,
		apr_pool_t *pool,
		apr_pool_t *tmp) /* function may clear pool */
{
	gla_path_t path;
	int ret;

	ret = gla_path_parse_entity(&path, "<nut>gla.HelloWorld", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_mount_read_buffer(rt, &path, script_hello_world, sizeof(script_hello_world) - 1);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.storage", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_storage_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.util", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_util_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.io", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_io_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.bdb", tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.xml", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_xml_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "gla.csv", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_csv_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	ret = gla_path_parse_package(&path, "squirrel", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_mod_sqstdlib_register(rt, &path, pool, tmp);
	if(ret != GLA_SUCCESS)
		return ret;

	return GLA_SUCCESS;
}

static SQInteger fn_mount_namespace(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "invalid argument count");

	for(sq_pushnull(vm); sq_next(vm, 3) == SQ_OK; sq_pop(vm, 2)) {
		gla_rt_dump_opstack(rt, "mnt_ns");
	}
	return gla_rt_vmsuccess(rt, false);
}

/* TODO adding a squirrel object to the mount path has several implications
 * for the typetag.
 * - one object has multiple entity paths: just one typetag possible, how to deal with this?
 * - override (see 'allow_override'): existing entity must be removed from mount path, but is still in cache.
 *   See flag GLA_MOUNT_NOCACHE. What to do with typetag of previous entity, remove it?
 *   Also see flag GLA_MOUNT_NOTYPETAG; set typetag in this function, if NOCACHE is set?
 *   Does NOCACHE imply that userdata and classes are not allowed? (not a good idea...) */
static SQInteger fn_mount_register(
		HSQUIRRELVM vm)
{
	int ret;
	SQBool allow_override = false;
	gla_path_t path;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) < 3 || sq_gettop(vm) > 4)
		return gla_rt_vmthrow(rt, "invalid argument count");
	else if(sq_gettop(vm) >= 4 && SQ_FAILED(sq_getbool(vm, 4, &allow_override)))
		return gla_rt_vmthrow(rt, "Invalid argument 3: expected boolean");
	ret = gla_path_get_entity(&path, false, rt, 2, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Invalid entity name specified");
	else if(path.extension != NULL)
		return gla_rt_vmthrow(rt, "Invalid entity name specified: extension not allowed");
	path.extension = GLA_ENTITY_EXT_OBJECT;
	ret = gla_mount_registry_put(rt->mnt_registry, &path, 3, allow_override);
	if(ret == GLA_ALREADY)
		return gla_rt_vmthrow(rt, "Entity already registered and override not allowed");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error putting entity into registry");

	return gla_rt_vmsuccess(rt, false);
}

static apr_status_t cleanup(
		void *data)
{
	gla_rt_t *rt = data;

	rt->shutdown = true;
	sq_close(rt->vm);
	btree_destroy(rt->data);
	return APR_SUCCESS;
}

gla_rt_t *gla_rt_new(
		const char *const *argv,
		int argn,
		apr_pool_t *minstance,
		apr_pool_t *msuper)
{
	int i;
	int ret;
	gla_rt_t *rt = apr_pcalloc(minstance, sizeof(gla_rt_t));
	gla_path_t path;

	assert(sizeof(SQInteger) >= sizeof(void*));
	assert(NULL == 0);

	rt->msuper = msuper;
	rt->mpool = minstance;

	ret = apr_pool_create(&rt->mpstack, rt->mpool);
	if(ret != APR_SUCCESS) {
		errno = GLA_ALLOC;
		return NULL;
	}
	rt->data = btree_new(GLA_BTREE_ORDER, sizeof(data_entry_t), cmp_ptr, 0);
	if(rt->data == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	rt->store = gla_store_new(rt->mpool);
	if(rt->store == NULL)
		return NULL;
	rt->reg_initialized = gla_packreg_new(rt->store, 0, rt->mpool);
	if(rt->reg_initialized == NULL)
		return NULL;
	rt->reg_imported = gla_entityreg_new(rt->store, sizeof(import_entry_t), rt->mpool);
	if(rt->reg_imported == NULL)
		return NULL;
	rt->reg_mounted = gla_packreg_new(rt->store, sizeof(mnt_list_t*), rt->mpool);
	if(rt->reg_mounted == NULL)
		return NULL;
	rt->mnt_internal = gla_mount_internal_new(rt->store, rt->mpool);
	if(rt->mnt_internal == NULL)
		return NULL;
	rt->mnt_registry = gla_mount_registry_new(rt);
	if(rt->mnt_registry == NULL)
		return NULL;
	rt->mnt_stdio = gla_mount_stdio_new(rt->mpool);
	if(rt->mnt_stdio == NULL)
		return NULL;
	ret = mnt_insert(rt->reg_mounted, NULL, rt->mnt_internal, GLA_MOUNT_SOURCE | GLA_MOUNT_ACTIVE);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	ret = mnt_insert(rt->reg_mounted, NULL, rt->mnt_registry, GLA_MOUNT_SOURCE | GLA_MOUNT_ACTIVE);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	ret = gla_path_parse_package(&path, "gla.stdio", rt->mpool);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	ret = mnt_insert(rt->reg_mounted, &path, rt->mnt_stdio, GLA_MOUNT_SOURCE | GLA_MOUNT_TARGET | GLA_MOUNT_ACTIVE);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}


	/* mnt_internal_setup may clear temp pool */
	ret = mnt_internal_setup(rt, rt->mpool, rt->mpstack);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	apr_pool_clear(rt->mpstack);
	
	rt->vm = sq_open(VM_INITIAL_STACK_SIZE);
	if(rt->vm == NULL)
		return NULL;
	apr_pool_cleanup_register(rt->mpool, rt, cleanup, apr_pool_cleanup_null);

	sq_setforeignptr(rt->vm, rt);
	sq_setprintfunc(rt->vm, print_handler, error_handler);
	sq_setcompilererrorhandler(rt->vm, compile_error_handler);
	sq_newclosure(rt->vm, runtime_error_handler, 0);
	sq_seterrorhandler(rt->vm);
	rt->gather_descriptions = true; /* TODO DEBUG */

	sq_pushroottable(rt->vm);
	sq_pushstring(rt->vm, "import", -1);
	sq_newclosure(rt->vm, fn_import, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "run", -1);
	sq_newclosure(rt->vm, fn_run, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "eval", -1);
	sq_newclosure(rt->vm, fn_eval, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "open", -1);
	sq_newclosure(rt->vm, fn_open, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "rt", -1);
	sq_newtable(rt->vm);

	sq_pushstring(rt->vm, "now", -1);
	sq_newclosure(rt->vm, fn_now, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "getenv", -1);
	sq_newclosure(rt->vm, fn_env_get, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "arg", -1);
	sq_newarray(rt->vm, argn);
	for(i = 0; i < argn; i++) {
		sq_pushinteger(rt->vm, i);
		sq_pushstring(rt->vm, argv[i], -1);
		ret = sq_set(rt->vm, -3);
		if(ret != SQ_OK)
			return NULL;
	}

	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "entityof", -1);
	sq_newclosure(rt->vm, fn_entityof, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "compile", -1); /* TODO move to module */
	sq_newclosure(rt->vm, fn_compile, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "sleep", -1);
	sq_newclosure(rt->vm, fn_sleep, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "usleep", -1);
	sq_newclosure(rt->vm, fn_usleep, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "dump", -1);
	sq_newtable(rt->vm);
	sq_pushstring(rt->vm, "opstack", -1);
	sq_newclosure(rt->vm, fn_dump_opstack, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "execstack", -1);
	sq_newclosure(rt->vm, fn_dump_execstack, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "full", -1);
	sq_newclosure(rt->vm, fn_dump_full, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_pushstring(rt->vm, "value", -1);
	sq_newclosure(rt->vm, fn_dump_value, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;
	sq_newslot(rt->vm, -3, false); /* 'dump' slot */

	sq_pushstring(rt->vm, "debug", -1);
	sq_newtable(rt->vm);
	sq_pushstring(rt->vm, "error", -1);
	sq_newclosure(rt->vm, fn_debug_error, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;
	sq_newslot(rt->vm, -3, false); /* 'debug' slot */

	sq_pushstring(rt->vm, "mount", -1);
	sq_newtable(rt->vm);
	sq_pushstring(rt->vm, "namespace", -1);
	sq_newclosure(rt->vm, fn_mount_namespace, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;
	sq_pushstring(rt->vm, "register", -1);
	sq_newclosure(rt->vm, fn_mount_register, 0);
	ret = sq_newslot(rt->vm, -3, false);
	if(ret != SQ_OK)
		return NULL;

	sq_newslot(rt->vm, -3, false); /* 'mount' slot */
	sq_newslot(rt->vm, -3, false); /* 'rt' slot */
	sq_poptop(rt->vm); /* root table */

	rt->refcnt = 1;
	return rt;
}

#ifdef DEBUG
/*static void dump_mount(
		int indent,
		gla_mount_tree_t *tree)
{
	int i;
	struct gla_mnt_list *list;

	for(i = 0; i < indent; i++)
		printf("  ");
	if(tree->name == NULL)
		printf("::\n");
	else
		printf("%s\n", tree->name);

	list = tree->mounts;
	while(list != NULL) {
		for(i = 0; i < indent; i++)
			printf("  ");
		printf("+ ");
		list->mount->dump(list->mount);
		printf("\n");
		list = list->next;
	}

	tree = tree->first_child;
	while(tree != NULL) {
		dump_mount(indent + 1, tree);
		tree = tree->next;
	}
}*/

void gla_rt_dump_sources(
		gla_rt_t *rt)
{
	printf("----------------------------------------- SOURCES DUMP ----------------------------------\n");
//	dump_mount(0, rt->sources_root);
	printf("-----------------------------------------------------------------------------------------\n");
}
#endif /* DEBUG */

void gla_rt_ref(
		gla_rt_t *rt)
{
	rt->refcnt++;
}

void gla_rt_unref(
		gla_rt_t *rt)
{
	if(!--rt->refcnt) {
		sq_close(rt->vm);
		apr_pool_destroy(rt->mpool);
		free(rt);
	}
}

int gla_rt_mount(
		gla_rt_t *rt,
		gla_mount_t *mnt,
		int flags)
{ /* TODO note: this code is very similar to gla_rt_mount_at */
	int ret;
	int len;
	int i;
	gla_path_t path;
	gla_path_t root;
	int method = METHOD_UNKNOWN;
	const SQChar *string;
	apr_pool_t *pool = begin(rt); /* TODO how to handle pools here? See also begin(), error(), success() */

	gla_path_root(&root);
	gla_path_parse_entity(&path, "_mount", pool);
	for(i = 0; i < ARRAY_SIZE(search_extension); i++) {
		path.extension = search_extension[i].extension;
		ret = gla_mount_info(mnt, NULL, &path, pool);
		if(ret == GLA_SUCCESS) {
			method = search_extension[i].method;
			break;
		}
	}

	ret = mount_run(rt, mnt, &path, "[_mount]", method, pool);
	if(ret == GLA_SUCCESS) {
		if(sq_gettype(rt->vm, -1) == OT_TABLE) {
			sq_pushstring(rt->vm, "package", -1);
			if(!SQ_FAILED(sq_get(rt->vm, -2))) {
				switch(sq_gettype(rt->vm, -1)) {
					case OT_STRING:
						if(SQ_FAILED(sq_getstring(rt->vm, -1, &string)))
							return error(rt, GLA_VM, "error retrieving string");
						else if(gla_path_parse_package(&root, string, pool) != GLA_SUCCESS) /* SEMI-TODO: use gla_path_get_package here? maybe not, since assuming that each _mount.package is a string seems sensible */
							return error(rt, GLA_SEMANTIC_ERROR, "invalid package '%s' returned by _mount script", string);
						break;
					case OT_NULL:
						break;
					default:
						return error(rt, GLA_SEMANTIC_ERROR, "'package' entry in _mount script must be a string or null");
				}
				sq_poptop(rt->vm);
			}

			if(rt->gather_descriptions) {
				sq_pushstring(rt->vm, "descriptions", -1);
				if(!SQ_FAILED(sq_get(rt->vm, -2))) {
					switch(sq_gettype(rt->vm, -1)) {
						case OT_ARRAY:
							len = sq_getsize(rt->vm, -1);
							if(len < 0)
								return error(rt, GLA_VM, "error getting descriptions array size");
							for(i = 0; i < len; i++) {
								sq_pushinteger(rt->vm, i);
								if(SQ_FAILED(sq_get(rt->vm, -2)))
									return error(rt, GLA_VM, "error getting descriptions array element %d", i);
								else if(SQ_FAILED(sq_getstring(rt->vm, -1, &string)))
									return error(rt, GLA_VM, "error in descriptions array: element %d must be a string", i);
//								printf("DESC: %s\n", string);
								sq_poptop(rt->vm);
							}
							break;
						case OT_NULL:
							break;
						default:
							return error(rt, GLA_SEMANTIC_ERROR, "'descriptions' entry in _mount script must be a string, an array of strings or null");
					}
					sq_poptop(rt->vm);
				}
			}
		}
		sq_poptop(rt->vm);
	}
	else if(ret != GLA_NOTFOUND)
		return error(rt, ret, "error running _mount script");

	ret = mnt_insert(rt->reg_mounted, &root, mnt, flags | GLA_MOUNT_ACTIVE);
	if(ret != GLA_SUCCESS)
		return error(rt, ret, "error inserting mount");

	return success(rt);
}

int gla_rt_mount_at(
		gla_rt_t *rt,
		gla_mount_t *mnt,
		int flags,
		const gla_path_t *root)
{
	return mnt_insert(rt->reg_mounted, root, mnt, flags | GLA_MOUNT_ACTIVE);
}

gla_mount_t **gla_rt_mounts_for(
		gla_rt_t *rt,
		int flags,
		const gla_path_t *package,
		apr_pool_t *pool)
{ /* TODO NOTE: this code is very similar to mnt_resolve */
	gla_id_t package_id;
	gla_path_t package_path;
	int depth;
	gla_packreg_t *registry = rt->reg_mounted;
	gla_store_t *store = gla_packreg_store(registry);
	gla_mount_t **result;
	mnt_list_t **head;
	mnt_list_t *cur;
	int n = 0;

	if(package == NULL)
		gla_path_root(&package_path);
	else if(gla_path_type(package) == GLA_PATH_PACKAGE)
		package_path = *package;
	else {
		LOG_ERROR("Cannot get mount points for entity");
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}

	/* count number of mounts */
	package_id = gla_store_path_deepest(store, &package_path, &depth);
	while(depth >= 0) {
		head = gla_packreg_try_id(registry, package_id);
		if(head != NULL) {
			cur = *head;
			while(cur != NULL) {
				if((cur->mount->flags & GLA_MOUNT_ACTIVE) != 0 && ((flags & GLA_MOUNT_FILESYSTEM) == 0 || (cur->mount->flags & GLA_MOUNT_FILESYSTEM) != 0) && ((flags & GLA_MOUNT_SOURCE) == 0 || (cur->mount->flags & GLA_MOUNT_SOURCE) != 0) && ((flags & GLA_MOUNT_TARGET) == 0 || (cur->mount->flags & GLA_MOUNT_TARGET) != 0)) /* if a flag is requested (in arg 'flags'), it must also be set for current mount handler (in 'cur->mount->flags') */
					n++;
				cur = cur->next;
			}
		}
		depth--;
		package_id = gla_store_path_parent(store, package_id);
	}

	/* collect all mounts */
	result = apr_pcalloc(pool, (n + 1) * sizeof(gla_mount_t*));
	if(result == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	n = 0;

	package_id = gla_store_path_deepest(store, &package_path, &depth);
	while(depth >= 0) {
		head = gla_packreg_try_id(registry, package_id);
		if(head != NULL) {
			cur = *head;
			while(cur != NULL) {
				if((cur->mount->flags & GLA_MOUNT_ACTIVE) != 0 && ((flags & GLA_MOUNT_FILESYSTEM) == 0 || (cur->mount->flags & GLA_MOUNT_FILESYSTEM) != 0) && ((flags & GLA_MOUNT_SOURCE) == 0 || (cur->mount->flags & GLA_MOUNT_SOURCE) != 0) && ((flags & GLA_MOUNT_TARGET) == 0 || (cur->mount->flags & GLA_MOUNT_TARGET) != 0)) /* if a flag is requested (in arg 'flags'), it must also be set for current mount handler (in 'cur->mount->flags') */
					result[n++] = cur->mount;
				cur = cur->next;
			}
		}
		depth--;
		package_id = gla_store_path_parent(store, package_id);
	}
	return result;
}

int gla_rt_boot_entity(
		gla_rt_t *rt,
		const gla_path_t *path)
{ /* TODO fn_run, fn_import, gla_rt_import, gla_rt_boot_entity and pack_init all contain similar code, improve */
	gla_path_t tmppath = *path;
	gla_path_t packpath;
	gla_mount_t *mount;
	gla_io_t *io;
	int ret;
	const char *entityname;
	apr_pool_t *pool = begin(rt);

	/* TODO support for other extensions (nutobj, nutcom) */
	if(tmppath.extension == NULL)
		tmppath.extension = "nut";
	entityname = gla_path_tostring(path, pool);

	packpath = *path;
	gla_path_package(&packpath);
	ret = pack_init(rt, &packpath, pool);
	if(ret != GLA_SUCCESS)
		return error(rt, ret, "error executing package init");

	ret = mnt_resolve(rt->reg_mounted, &mount, &tmppath, GLA_MOUNT_SOURCE, NULL, pool);
	if(ret != GLA_SUCCESS)
		return error(rt, ret, "could not resolve script entity");
	io = gla_mount_open(mount, &tmppath, GLA_MODE_READ, pool);
	if(io == NULL)
		return error(rt, ret, "error opening script entity");
	ret = sq_compile(rt->vm, io_lexfeed, io, entityname, true);
	if(ret != SQ_OK)
		return error(rt, GLA_SYNTAX_ERROR, "error compiling script");

	sq_pushroottable(rt->vm);
	ret = sq_call(rt->vm, 1, false, true); /* TODO use arguments?; final bool args: 'retval' and 'vmthrowerror'  */
	if(ret != SQ_OK)
		return error(rt, GLA_SEMANTIC_ERROR, "error running script");
	return success(rt);
}

int gla_rt_boot_file(
		gla_rt_t *rt,
		const char *name,
		FILE *file) /* TODO add another argument specifying the type: nut, nutobj, nutcom */
{ /* TODO fn_run, fn_import, gla_rt_import, gla_rt_boot_entity and pack_init all contain similar code, improve */
	int ret;
	begin(rt);

	ret = sq_compile(rt->vm, file_lexfeed, file, name, true);
	if(ret != SQ_OK)
		return error(rt, GLA_SEMANTIC_ERROR, "error running script");
	sq_pushroottable(rt->vm);
	ret = sq_call(rt->vm, 1, false, true); /* TODO use arguments?; final bool args: 'retval' and 'vmthrowerror'  */
	if(ret != SQ_OK)
		return error(rt, GLA_SEMANTIC_ERROR, "error running script");
	return success(rt);
}

void gla_rt_dump_value_simple(
		gla_rt_t *rt,
		int idx,
		bool full,
		const char *info)
{
	dump_value_simple(rt, idx, full, rt->mpstack);
}

void gla_rt_dump_value(
		gla_rt_t *rt,
		int idx,
		int maxdepth,
		bool full,
		const char *info)
{
	dump_value(rt->vm, idx, maxdepth, full, info);
}

void gla_rt_dump_opstack(
		gla_rt_t *rt,
		const char *info)
{
	dump_opstack(rt->vm, info);
}

void gla_rt_dump_execstack(
		gla_rt_t *rt,
		const char *info)
{
	dump_execstack(rt->vm, info);
}

gla_mount_t *gla_rt_resolve(
		gla_rt_t *rt,
		gla_path_t *path,
		int flags,
		int *shifted,
		apr_pool_t *pool)
{
	gla_mount_t *mount;
	int ret;

	ret = mnt_resolve(rt->reg_mounted, &mount, path, flags, shifted, pool);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	else
		return mount;
}

int gla_rt_init_package(
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	return pack_init(rt, path, pool);
}

/* TODO introduce global entity registry (in main.c) to prevent double initialization of same module */
int gla_rt_import(
		gla_rt_t *rt,
		const gla_path_t *orgpath,
		apr_pool_t *pool)
{
	int ret;
	int i;
	gla_path_t path; /* absolute path */
	gla_path_t relpath; /* relative path (starting at mount root) */
	gla_path_t packpath;
	const char *entity;
	gla_mount_t *mount;
	gla_io_t *io;
	int method = METHOD_UNKNOWN;
	import_entry_t *entry;
	SQInteger type;
	void *mod_handle;
	int (*mod_init)(apr_pool_t*);
	int (*mod_push)(gla_rt_t*, apr_pool_t*);
	const char *mod_file;

	if(orgpath == NULL || gla_path_type(orgpath) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	else if(orgpath->extension != NULL) {
		LOG_ERROR("error importing entity: extension given but not allowed for import");
		return GLA_INVALID_PATH;
	}

	path = *orgpath;
	packpath = *orgpath;
	gla_path_package(&packpath);
	ret = pack_init(rt, &packpath, pool);
	if(ret != GLA_SUCCESS) {
		LOG_DEBUG("error initializing package");
		return GLA_VM;
	}

	entry = gla_entityreg_try(rt->reg_imported, orgpath);
	if(entry != NULL) {
		sq_pushobject(rt->vm, entry->object);
		return GLA_SUCCESS;
	}
	else if(errno != GLA_NOTFOUND) {
		LOG_DEBUG("error trying to get registered entity");
		return errno;
	}

	for(i = 0; i < ARRAY_SIZE(search_extension); i++) {
		path.extension = search_extension[i].extension;
		relpath = path;
		ret = mnt_resolve(rt->reg_mounted, &mount, &relpath, GLA_MOUNT_SOURCE, NULL, pool);
		if(ret == GLA_SUCCESS) {
			method = search_extension[i].method;
			entry = gla_entityreg_get(rt->reg_imported, orgpath);
			if(entry == NULL) {
				LOG_DEBUG("error registering imported entity");
				return errno;
			}
			break;
		}
	}

	/* here: 'entry' contains the registry entry
	 * and 'errno' is set to GLA_SUCCESS in case the entity has already been registered
	 * or GLA_NOTFOUND if the entity must be treated and registered now.
	 * 'method' contains the method how to treat the entity
	 * (METHOD_COMPILE, METHOD_LOAD, METHOD_PUSH) */

	/* so: has the entity already been registered? */
	path.extension = NULL;
	entity = gla_path_tostring(&path, pool);
	switch(method) {
		case METHOD_COMPILE:
			io = gla_mount_open(mount, &relpath, GLA_MODE_READ, pool);
			if(io == NULL) {
				LOG_ERROR("error opening entity");
				return errno;
			}
			ret = sq_compile(rt->vm, io_lexfeed, io, entity, true);
			if(ret != SQ_OK) {
				LOG_ERROR("error compiling script");
				return GLA_VM;
			}

			sq_pushroottable(rt->vm);
			ret = sq_call(rt->vm, 1, true, true); /* TODO use arguments?; final bool args: 'retval' and 'vmthrowerror'  */
			if(ret != SQ_OK) {
				LOG_ERROR("error running script");
				return GLA_VM;
			}
			sq_remove(rt->vm, -2);
			break;
		case METHOD_LOAD:
			LOG_DEBUG("METHOD_LOAD not yet supported");
			return GLA_NOTSUPPORTED;
		case METHOD_PUSH:
			ret = gla_mount_push(mount, rt, &relpath, pool);
			if(ret != GLA_SUCCESS) {
				LOG_ERROR("error pushing entity to stack");
				return ret;
			}
			break;
		case METHOD_DYNLIB:
			mod_file = gla_mount_tofilepath(mount, &relpath, false, pool);
			if(mod_file == NULL) {
				LOG_ERROR("error getting filesystem path of dynamic library");
				return errno;
			}
			mod_handle = dlopen(mod_file, RTLD_NOW);
			if(mod_handle == NULL) {
				LOG_ERROR("error loading dynamic library: %s", dlerror());
				return GLA_IO;
			}
			apr_pool_cleanup_register(rt->msuper, mod_handle, cleanup_mod, apr_pool_cleanup_null);

			mod_init = dlsym(mod_handle, "_gla_init");
			if(mod_init != NULL) {
				ret = mod_init(rt->msuper);
				if(ret != GLA_SUCCESS) {
					LOG_ERROR("error initializing dynamic library");
					return ret;
				}
			}
			mod_push = dlsym(mod_handle, "_gla_push");
			if(mod_push == NULL) {
				LOG_ERROR("error pushing object from dynamic library: missing register symbol");
				return GLA_INVALID_MODULE;
			}
			GLA_RT_SUBSUBFN(mod_push(rt, pool), 1, "error pushing object from dynamic library");
			break;
		case METHOD_UNKNOWN:
			LOG_ERROR("no such entity");
			return GLA_NOTFOUND;
		default:
			assert(false);
	}

	/* set type tag */
	type = sq_gettype(rt->vm, -1);
	if((type == OT_CLASS || type == OT_USERDATA) && (gla_mount_features(mount) & GLA_MOUNT_NOTYPETAG) == 0) {
		SQUserPointer typetag;
		ret = sq_gettypetag(rt->vm, -1, &typetag);
		if(ret != SQ_OK) {
			LOG_DEBUG("error getting current type tag");
			return GLA_VM;
		}
		else if(typetag == NULL)
			sq_settypetag(rt->vm, -1, entry);
/*			if(ret != SQ_OK)
			return gla_rt_vmthrow(rt, "error getting current type tag");
		else if(typetag != NULL)
			return gla_rt_vmthrow(rt, "imported entity returns a class which already has a type tag set");
		sq_settypetag(vm, -1, entry);*/
	}
	else if(type == OT_INSTANCE || type == OT_TABLE) {
		sq_pushstring(rt->vm, GLA_ENTITYOF_SLOT, -1);
		if(!SQ_FAILED(sq_rawget(rt->vm, -2))) {
			sq_poptop(rt->vm);
			sq_pushstring(rt->vm, GLA_ENTITYOF_SLOT, -1);
			sq_pushstring(rt->vm, entity, -1);
			sq_set(rt->vm, -3);
		}
	}

	/* put result into the registry */
	if((gla_mount_features(mount) & GLA_MOUNT_NOCACHE) == 0) {
		ret = sq_getstackobj(rt->vm, -1, &entry->object);
		if(ret != SQ_OK) {
			LOG_DEBUG("error getting compiled closure from stack");
			return GLA_VM;
		}
		sq_addref(rt->vm, &entry->object);
	}

	return GLA_SUCCESS;
}

void *gla_rt_typetag(
		gla_rt_t *rt,
		gla_path_t path)
{
	int i;
	import_entry_t *entry;

	for(i = 0; i < ARRAY_SIZE(search_extension); i++) {
		path.extension = search_extension[i].extension;
		entry = gla_entityreg_try(rt->reg_imported, &path);
		if(entry != NULL)
			return entry;
		else if(errno != GLA_NOTFOUND) {
			LOG_DEBUG("error trying to get registered entity");
			return NULL;
		}
	}
	errno = GLA_NOTFOUND;
	return NULL;
}

int gla_rt_data_put(
		gla_rt_t *rt,
		const void *key,
		void *data)
{
	data_entry_t entry;
	entry.key = key;
	entry.data = data;
	btree_put(rt->data, &entry);
	return GLA_SUCCESS;
}

void *gla_rt_data_get(
		gla_rt_t *rt,
		const void *key)
{
	data_entry_t keyentry;
	data_entry_t *valueentry;
	keyentry.key = key;
	valueentry = btree_get(rt->data, &keyentry);
	assert(valueentry != NULL); /* this is considered a bug since _get should never being called before having _put the corresponding entry */
	return valueentry->data;
}

const char *gla_rt_string_acquire(
		gla_rt_t *rt,
		const char *string)
{
	const SQChar *sqstring;
	HSQOBJECT obj;

	if(string == NULL)
		return NULL;
	sq_pushstring(rt->vm, string, -1);
	sq_getstackobj(rt->vm, -1, &obj);
	sq_addref(rt->vm, &obj);
	sq_getstring(rt->vm, -1, &sqstring);
	sq_poptop(rt->vm);
	return sqstring;
}

void gla_rt_string_release(
		gla_rt_t *rt,
		const char *string)
{
	HSQOBJECT obj;

	if(string == NULL)
		return;
	sq_pushstring(rt->vm, string, -1);
	sq_getstackobj(rt->vm, -1, &obj);
	sq_release(rt->vm, &obj);
	sq_poptop(rt->vm);
}

int gla_rt_mount_object(
		gla_rt_t *rt,
		const gla_path_t *path,
		int (*push)(gla_rt_t *rt, void *user, apr_pool_t *pperm, apr_pool_t *ptemp), /* pperm: permanent storage until mount is deleted; ptemp: temporary storage until 'push' is finished */
		void *user)
{
	return gla_mount_internal_add_object(rt->mnt_internal, path, push, user);
}

int gla_rt_mount_read_buffer(
		gla_rt_t *rt,
		const gla_path_t *path,
		const void *buffer,
		int size)
{
	return gla_mount_internal_add_read_buffer(rt->mnt_internal, path, buffer, size);
}

HSQUIRRELVM gla_rt_vm(
		gla_rt_t *rt)
{
	return rt->vm;
}

bool gla_rt_shutdown(
		gla_rt_t *rt)
{
	return rt->shutdown;
}

apr_pool_t *gla_rt_pool_stack(
		gla_rt_t *rt)
{
	return rt->mpstack;
}

apr_pool_t *gla_rt_pool_global(
		gla_rt_t *rt)
{
	return rt->mpool;
}

gla_rt_t *gla_rt_vmbegin(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = sq_getforeignptr(vm);
	apr_pool_t *parent = rt->mpstack;
	int ret;

	ret = apr_pool_create(&rt->mpstack, parent);
	if(ret != APR_SUCCESS) {
		fprintf(stderr, "error creating stack child pool\n");
		exit(1);
		return NULL;
	}

	return rt;
}

int gla_rt_vmthrow_null(
		gla_rt_t *rt)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	sq_pushnull(rt->vm);
	return sq_throwobject(rt->vm);
}

int gla_rt_vmthrow_top(
		gla_rt_t *rt)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return sq_throwobject(rt->vm);
}

int gla_rt_vmthrow(
		gla_rt_t *rt,
		const char *fmt, ...)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	va_list ap;
	char *message;

	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	va_start(ap, fmt);
	message = apr_pvsprintf(parent, fmt, ap);
	va_end(ap);

	return sq_throwerror(rt->vm, message);
}

int gla_rt_vmsuccess(
		gla_rt_t *rt,
		bool has_ret)
{
	apr_pool_t *parent = apr_pool_parent_get(rt->mpstack);
	apr_pool_destroy(rt->mpstack);
	rt->mpstack = parent;
	return has_ret ? 1 : 0;
}

