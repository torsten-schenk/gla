#ifndef _GLA_CORE_H
#define _GLA_CORE_H

#include <stdio.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <btree/memory.h>

#include <gla/squirrel.h>

/* CALLING A CLOSURE
 * opstack: < closure, this, arg1, arg2 >
 * function: sq_call(vm, 3, false, true); first bool: return value?; second bool: raise error? */

#ifndef APR_IS_BIGENDIAN
#error APR_IS_BIGENDIAN not defined
#endif
#if APR_IS_BIGENDIAN == 0
#elif APR_IS_BIGENDIAN == 1
#else
#error Unexpected value for APR_IS_BIGENDIAN
#endif

#define GLA_BTREE_ORDER 32
#define GLA_PATH_MAX_DEPTH 32

#define GLA_ENTITY_EXT_OBJECT "nutobj"
#define GLA_ENTITY_EXT_COMPILED "nutcom"
#define GLA_ENTITY_EXT_SCRIPT "nut"
#define GLA_ENTITY_EXT_DYNLIB "linux-x86_64" /* TODO x86_64-linux-gnu */
#define GLA_ENTITY_EXT_IO "io"
#define GLA_ENTITYOF_SLOT "_entityof" /* when importing, for instances and tables (rawget) that have this slot, the value will be set to the imported entity name */

#define GLA_LOG_ERROR(RT, MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define GLA_LOG_WARN(RT, MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define GLA_LOG_INFO(RT, MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define GLA_LOG_DEBUG(RT, MSG...) \
	do { \
		fprintf(stderr, "[DEBUG %s, line %d] ", __FILE__, __LINE__); \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

enum {
	GLA_SUCCESS = 0,
	GLA_ALLOC = -40000,
	GLA_MISSING_FILE = -40002,
	GLA_SYNTAX_ERROR = -40003,
	GLA_SEMANTIC_ERROR = -40004,
	GLA_OVERFLOW = -40005,
	GLA_UNDERFLOW = -40006,
	GLA_END = -40007,
	GLA_IO = -40008,
	GLA_INVALID_PATH_TYPE = -40009,
	GLA_INVALID_PATH = -40010,
	GLA_NOTFOUND = -40011,
	GLA_INVALID_ARGUMENT = -40012,
	GLA_NOTSUPPORTED = -40013,
	GLA_ALREADY = -40014,
	GLA_INVALID_ACCESS = -40015,
	GLA_CLOSED = -40016,
	GLA_VM = -40017,
	GLA_INVALID_MODULE = -40018,
	GLA_INTERNAL = -40100,
#ifdef TESTING
	GLA_TEST = -40999
#endif
};

enum {
	GLA_PATH_PACKAGE,
	GLA_PATH_ENTITY
};

enum {
	GLA_MODE_READ = 0x01,
	GLA_MODE_WRITE = 0x02,
	GLA_MODE_APPEND = 0x04
};

enum {
	GLA_MOUNT_SOURCE = 0x00000001,
	GLA_MOUNT_TARGET = 0x00000002,
	GLA_MOUNT_CREATE = 0x00000004, /* create the entity/package if it does not exist. requires GLA_MOUNT_TARGET. */
	GLA_MOUNT_NOTYPETAG = 0x00200000, /* do not set a type tag for classes and userdata */
	GLA_MOUNT_NOCACHE = 0x00400000, /* do not cache the object, search it every time an import occurs; implies GLA_MOUNT_NOTYPETAG */
	GLA_MOUNT_FILESYSTEM = 0x00800000, /* specify this flag when the resolved mountpoint must offer a file name/a directory in the filesystem */
	GLA_MOUNT_FEATURES = 0x00ffff00,
	GLA_MOUNT_PRIVATE = 0xff000000
};

enum {
	GLA_ACCESS_READ = 0x00000001,
	GLA_ACCESS_WRITE = 0x00000002,
	GLA_ACCESS_PUSH = 0x00000004
};

enum {
	GLA_ID_ROOT = 0,
	GLA_ID_INVALID = -1
};

typedef int gla_id_t;

typedef struct {
	int shifted; /* offset in 'package' where path starts */
	int size;
	const char *package[GLA_PATH_MAX_DEPTH]; /* 'size' number of entries used */
	const char *entity; /* NULL <=> path denotes a package */
	const char *extension; /* only used if 'entity' != NULL */
} gla_path_t;

typedef struct {
	gla_id_t package;
	const char *name;
	const char *extension;
	void *data;

	/* private */
	int lower;
	int upper;
	btree_it_t cur;
} gla_entityreg_it_t;

typedef struct {
	/* private */
	btree_it_t btit;	
} gla_packreg_it_t;

typedef struct gla_rt gla_rt_t;
typedef struct gla_io gla_io_t;
typedef struct gla_entityreg gla_entityreg_t;
typedef struct gla_packreg gla_packreg_t;
typedef struct gla_store gla_store_t;
typedef struct gla_mountit gla_mountit_t;
typedef struct gla_mount gla_mount_t;
typedef struct gla_pathinfo gla_pathinfo_t;
typedef struct gla_bridge gla_bridge_t;
typedef struct gla_bridgedef_entry gla_bridgedef_entry_t;
typedef struct gla_bridgedef gla_bridgedef_t;
typedef struct gla_meta_mount gla_meta_mount_t;
typedef struct gla_meta_io gla_meta_io_t;

struct gla_pathinfo {
	int access; /* or'ed GLA_ACCESS_x */
	bool can_create;
};

int gla_io_read(
		gla_io_t *self,
		void *buffer,
		int size);

int gla_io_write(
		gla_io_t *self,
		const void *buffer,
		int size);

int gla_io_rseek(
		gla_io_t *self,
		int64_t off,
		int whence);

int gla_io_wseek(
		gla_io_t *self,
		int64_t off,
		int whence);

int64_t gla_io_rtell(
		gla_io_t *self);

int64_t gla_io_wtell(
		gla_io_t *self);

int64_t gla_io_size(
		gla_io_t *self);

int gla_io_flush(
		gla_io_t *self);

int gla_io_wstatus(
		gla_io_t *self);

int gla_io_rstatus(
		gla_io_t *self);

void gla_io_close(
		gla_io_t *self);

gla_io_t *gla_io_get(
		gla_rt_t *rt,
		int idx);

uint32_t gla_mount_features(
		gla_mount_t *self);

int gla_mount_depth(
		gla_mount_t *self);

uint32_t gla_mount_flags(
		gla_mount_t *self);

/* sets errno in case NULL is returned; if errno == 0, no packages are present (i.e. not an error) */
gla_mountit_t *gla_mount_entities(
		gla_mount_t *self,
		const gla_path_t *path,
		apr_pool_t *petmp);

/* sets errno on NULL return; if errno == 0, no packages are present (i.e. not an error) */
gla_mountit_t *gla_mount_packages(
		gla_mount_t *self,
		const gla_path_t *path,
		apr_pool_t *petmp);

int gla_mount_touch(
		gla_mount_t *self,
		const gla_path_t *path,
		bool mkpackage, /* create package if it does not exist */
		apr_pool_t *petmp);

int gla_mount_erase(
		gla_mount_t *self,
		const gla_path_t *entity,
		apr_pool_t *petmp);

int gla_mount_rename(
		gla_mount_t *self,
		const gla_path_t *path,
		const char *renamed,
		apr_pool_t *ptemp);

/* sets errno on NULL return */
gla_io_t *gla_mount_open(
		gla_mount_t *self,
		const gla_path_t *path,
		int mode,
		apr_pool_t *psession);

int gla_mount_push(
		gla_mount_t *self,
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *ptemp);

int gla_mount_info(
		gla_mount_t *self,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *ptemp);

/* sets errno on NULL return */
const char *gla_mount_tofilepath(
		gla_mount_t *self,
		const gla_path_t *path,
		bool dirname, /* if true and 'path' denotes an entity, return the directory where entity 'path' resides in */
		apr_pool_t *psession);

#ifdef DEBUG
void gla_mount_dump(
		gla_mount_t *self);
#endif

/* TODO change iterator style from Java to C++ (i.e. call next AFTER first element, not BEFORE) */
int gla_mountit_next(
		gla_mountit_t *self,
		const char **name,
		const char **extension);

void gla_mountit_close(
		gla_mountit_t *self);

int gla_path_parse_package(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool);

int gla_path_parse_entity(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool);

int gla_path_get_entity(
		gla_path_t *path,
		bool clone, /* if set to false, don't clone strings where possible. this implies that the strings MUST remain on operand stack (use case example: a PackagePath at 'idx' is ensured to remain on stack until the path is no longer required)*/
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool);

int gla_path_get_package(
		gla_path_t *path,
		bool clone, /* if set to false, don't clone strings where possible. this implies that the strings MUST remain on operand stack (use case example: a PackagePath at 'idx' is ensured to remain on stack until the path is no longer required)*/
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool);

int gla_path_type(
		const gla_path_t *path);

bool gla_path_equal(
		const gla_path_t *a,
		const gla_path_t *b);

int gla_path_parse_append_entity(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool);

const char *gla_path_shift(
		gla_path_t *path);

/* returns amount shifted (i.e. MIN(amount, path->size))*/
int gla_path_shift_many(
		gla_path_t *path,
		int amount);

int gla_path_shift_relative(
		gla_path_t *path,
		const gla_path_t *root);

int gla_path_unshift( /* CAUTION: only unshift previously shifted paths!! */
		gla_path_t *path);

int gla_path_unshift_many(
		gla_path_t *path,
		int amount);

bool gla_path_is_root(
		const gla_path_t *path);

/* converts an entity path into the path representing the package of the entity; NOP if path is already a package */
void gla_path_package(
		gla_path_t *path);

/* sets 'path' to root path */
void gla_path_root(
		gla_path_t *path);

int gla_path_clone(
		gla_path_t *dest,
		const gla_path_t *src,
		apr_pool_t *pool);

const char *gla_path_tostring(
		const gla_path_t *path,
		apr_pool_t *pool);

void gla_path_dump(
		const gla_path_t *path);

#define GLA_RT_SUBFN(FN, NRET, ERROR) \
do { \
	int ret; \
	int _stack_size = sq_gettop(rt->vm); \
	ret = FN; \
	if(ret != GLA_SUCCESS) \
		return gla_rt_vmthrow(rt, ERROR); \
	assert(sq_gettop(rt->vm) >= _stack_size + NRET); \
	sq_pop(rt->vm, sq_gettop(rt->vm) - _stack_size - NRET); \
} while(false)

#define GLA_RT_SUBSUBFN(FN, NRET, ERROR) \
do { \
	int ret; \
	int _stack_size = sq_gettop(rt->vm); \
	ret = FN; \
	if(ret != GLA_SUCCESS) { \
		LOG_ERROR(ERROR); \
		return ret; \
	} \
	assert(sq_gettop(rt->vm) >= _stack_size + NRET); \
	sq_pop(rt->vm, sq_gettop(rt->vm) - _stack_size - NRET); \
} while(false)

gla_rt_t *gla_rt_new(
		const char *const *argv,
		int argn,
		apr_pool_t *pool);

void gla_rt_ref(
		gla_rt_t *rt);

void gla_rt_unref(
		gla_rt_t *rt);

/* mount a given mount handler.
 * checks, if entity '_mount.nut' exists and
 * executes it. if the resulting table contains an entry
 * 'package', the mount handler will be rooted in that
 * package.
 * flags: GLA_MOUNT_x*/
int gla_rt_mount(
		gla_rt_t *rt,
		gla_mount_t *mnt,
		int flags);

/* mount a given mount handler.
 * similar to gla_rt_mount(), but ignores 'package' entry
 * in resulting '_mount.nut' table and uses the given 'package'
 * instead. */
int gla_rt_mount_at(
		gla_rt_t *rt,
		gla_mount_t *mnt,
		int flags,
		const gla_path_t *package);

/* return all mounts for a given package. EOL: NULL
 * sorted by resolve order */
gla_mount_t **gla_rt_mounts_for(
		gla_rt_t *rt,
		int flags,
		const gla_path_t *package,
		apr_pool_t *pool);

int gla_rt_boot_entity(
		gla_rt_t *rt,
		const gla_path_t *path);

int gla_rt_boot_file(
		gla_rt_t *rt,
		const char *filename);

void gla_rt_dump_value_simple(
		gla_rt_t *rt,
		int idx,
		bool full,
		const char *info);

void gla_rt_dump_value(
		gla_rt_t *rt,
		int idx,
		int maxdepth,
		bool full,
		const char *info);

void gla_rt_dump_sources(
		gla_rt_t *rt);

void gla_rt_dump_opstack(
		gla_rt_t *rt,
		const char *info);

void gla_rt_dump_execstack(
		gla_rt_t *rt,
		const char *info);

gla_mount_t *gla_rt_resolve( /* TODO refactor: remove 'shifted' and shift 'path' */
		gla_rt_t *rt,
		gla_path_t *path,
		int flags,
		int *shifted,
		apr_pool_t *pool);

int gla_rt_init_package(
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *pool);

int gla_rt_data_put(
		gla_rt_t *rt,
		const void *key,
		void *data);

void *gla_rt_data_get(
		gla_rt_t *rt,
		const void *key);

/* imports an executable entity and pushes it onto the operand stack */
int gla_rt_import(
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *pool);

void *gla_rt_typetag(
		gla_rt_t *rt,
		gla_path_t path);

/* Acquire a string within the squirrel vm. The string
 * may be one already obtained from the vm.
 * Returns the pointer to the vm string. */
const char *gla_rt_string_acquire(
		gla_rt_t *rt,
		const char *string);

/* Release a string.
 * The given string data must have been previously acquired,
 * but the pointer itself may be one not belonging to the vm. */
void gla_rt_string_release(
		gla_rt_t *rt,
		const char *string);

int gla_rt_mount_object(
		gla_rt_t *rt,
		const gla_path_t *path,
		int (*push)(gla_rt_t *rt, void *user, apr_pool_t *pperm, apr_pool_t *ptemp), /* pperm: permanent storage until mount is deleted; ptemp: temporary storage until 'push' is finished */
		void *user);

int gla_rt_mount_read_buffer(
		gla_rt_t *rt,
		const gla_path_t *path,
		const void *buffer,
		int size);

HSQUIRRELVM gla_rt_vm(
		gla_rt_t *rt);

apr_pool_t *gla_rt_pool_stack(
		gla_rt_t *rt);

gla_rt_t *gla_rt_vmbegin(
		HSQUIRRELVM vm);

int gla_rt_vmthrow_null(
		gla_rt_t *rt);

int gla_rt_vmthrow_top(
		gla_rt_t *rt);

int gla_rt_vmthrow(
		gla_rt_t *rt,
		const char *fmt, ...);

int gla_rt_vmsuccess(
		gla_rt_t *rt,
		bool has_ret);
#endif

