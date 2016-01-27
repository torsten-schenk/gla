#include <assert.h>

#include "../log.h"
#include "../rt.h"

#include "_rtdata.h"
#include "io.h"
#include "packpath.h"

typedef struct {
	apr_pool_t *pool;
	btree_t *encountered;
	gla_path_t path;
	gla_mount_t **mounts;
	gla_mountit_t *mntit;
} pathit_t;

static int cb_cmp1(
		btree_t *btree,
		const void *a,
		const void *b,
		void *group)
{
	return memcmp(a, b, sizeof(HSQOBJECT));
}

static int cb_acquire1(
		btree_t *btree,
		void *a_)
{
	HSQOBJECT *a = a_;
	gla_rt_t *rt = btree_data(btree);
	sq_addref(rt->vm, a);
	return SQ_OK;
}

static void cb_release1(
		btree_t *btree,
		void *a_)
{
	HSQOBJECT *a = a_;
	gla_rt_t *rt = btree_data(btree);
	sq_release(rt->vm, a);
}

static int cb_cmp2(
		btree_t *btree,
		const void *a,
		const void *b,
		void *group)
{
	return memcmp(a, b, 2 * sizeof(HSQOBJECT));
}

static int cb_acquire2(
		btree_t *btree,
		void *a_)
{
	HSQOBJECT *a = a_;
	gla_rt_t *rt = btree_data(btree);
	sq_addref(rt->vm, a + 0);
	sq_addref(rt->vm, a + 1);
	return SQ_OK;
}

static void cb_release2(
		btree_t *btree,
		void *a_)
{
	HSQOBJECT *a = a_;
	gla_rt_t *rt = btree_data(btree);
	sq_release(rt->vm, a + 0);
	sq_release(rt->vm, a + 1);
}

static apr_status_t cleanup_btree(
		void *data)
{
	btree_t *btree = data;
	btree_destroy(btree);
	return APR_SUCCESS;
}

static int get_path(
		gla_rt_t *rt,
		gla_path_t *path,
		int idx)
{
	const SQChar *fragment;
	int i;
	SQInteger size;
	HSQUIRRELVM vm = rt->vm;

	idx = sq_toabs(vm, idx);
	gla_path_root(path);
	sq_pushstring(vm, "_size", -1);
	if(SQ_FAILED(sq_get(vm, idx))) {
		LOG_ERROR("Error getting _size member");
		return GLA_VM;
	}
	else if(SQ_FAILED(sq_getinteger(vm, -1, &size))) {
		LOG_ERROR("Invalid _size value");
		return GLA_VM;
	}
	sq_poptop(vm);
	assert(size >= 0 && size <= GLA_PATH_MAX_DEPTH);

	path->size = size;
	if(size > 0) {
		sq_pushstring(vm, "_package", -1);
		if(SQ_FAILED(sq_get(vm, idx))) {
			sq_poptop(vm);
			LOG_ERROR("Error getting _package member");
			return GLA_VM;
		}
		for(i = 0; i < size; i++) {
			sq_pushinteger(vm, i);
			if(SQ_FAILED(sq_get(vm, -2))) {
				sq_poptop(vm);
				LOG_ERROR("Error getting package fragment %d", i);
				return GLA_VM;
			}
			else if(SQ_FAILED(sq_getstring(vm, -1, &fragment))) {
				sq_poptop(vm);
				LOG_ERROR("Invalid package fragment %d\n", i);
				return GLA_VM;
			}
			sq_poptop(vm);
			path->package[i] = fragment;
		}
		sq_poptop(vm);
	}
	else
		gla_path_root(path);

	sq_pushstring(vm, "_entity", -1);
	if(SQ_FAILED(sq_get(vm, idx))) {
		LOG_ERROR("Error getting _entity member");
		return GLA_VM;
	}
	if(sq_gettype(vm, -1) == OT_NULL) {
		sq_poptop(vm);
		return GLA_SUCCESS;
	}
	if(SQ_FAILED(sq_getstring(vm, -1, &fragment))) {
		LOG_ERROR("Invalid entity value\n");
		return GLA_VM;
	}
	sq_poptop(vm);
	path->entity = fragment;

	sq_pushstring(vm, "_extension", -1);
	if(SQ_FAILED(sq_get(vm, idx))) {
		LOG_ERROR("Error getting _extension member");
		return GLA_VM;
	}
	if(sq_gettype(vm, -1) == OT_NULL) {
		sq_poptop(vm);
		return GLA_SUCCESS;
	}
	if(SQ_FAILED(sq_getstring(vm, -1, &fragment))) {
		LOG_ERROR("Invalid extension value\n");
		return GLA_VM;
	}
	sq_poptop(vm);
	path->extension = fragment;
	return GLA_SUCCESS;
}

static int set_path(
		gla_rt_t *rt,
		const gla_path_t *path,
		int idx)
{
	int i;
	HSQUIRRELVM vm = rt->vm;

	idx = sq_toabs(vm, idx);
	sq_pushstring(vm, "_size", -1);
	sq_pushinteger(vm, path->size - path->shifted);
	if(SQ_FAILED(sq_set(vm, idx))) {
		sq_pop(vm, 2);
		LOG_ERROR("Error setting _size member");
		return GLA_VM;
	}

	sq_pushstring(vm, "_package", -1);
	if(SQ_FAILED(sq_get(vm, idx))) {
		sq_poptop(vm);
		LOG_ERROR("Error getting _package member");
		return GLA_VM;
	}
	for(i = path->shifted; i < path->size; i++) {
		sq_pushinteger(vm, i - path->shifted);
		sq_pushstring(vm, path->package[i], -1);
		if(SQ_FAILED(sq_set(vm, -3))) {
			sq_pop(vm, 3);
			LOG_ERROR("Error setting package fragment %d", i);
			return GLA_VM;
		}
	}
	for(i = path->size - path->shifted; i < GLA_PATH_MAX_DEPTH; i++) {
		sq_pushinteger(vm, i - path->shifted);
		sq_pushnull(vm);
		if(SQ_FAILED(sq_set(vm, -3))) {
			sq_pop(vm, 3);
			LOG_ERROR("Error setting package fragment %d", i);
			return GLA_VM;
		}
	}
	sq_poptop(vm);

	sq_pushstring(vm, "_entity", -1);
	if(path->entity != NULL)
		sq_pushstring(vm, path->entity, -1);
	else
		sq_pushnull(vm);
	if(SQ_FAILED(sq_set(vm, idx))) {
		sq_pop(vm, 2);
		LOG_ERROR("Error setting _entity member");
		return GLA_VM;
	}

	sq_pushstring(vm, "_extension", -1);
	if(path->extension != NULL)
		sq_pushstring(vm, path->extension, -1);
	else
		sq_pushnull(vm);
	if(SQ_FAILED(sq_set(vm, idx))) {
		sq_pop(vm, 2);
		LOG_ERROR("Error setting _extension member");
		return GLA_VM;
	}
	return GLA_SUCCESS;

}

static SQInteger fn_open(
		HSQUIRRELVM vm)
{
	const SQChar *modestr;
	int mode;
	gla_mount_t *mnt;
	gla_io_t *io;
	int flags = 0;
	gla_path_t path;
	apr_pool_t *pool;
	bool setend = false;
	uint64_t roff;
	uint64_t woff;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &modestr)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	if(strcasecmp(modestr, "r") == 0)
		mode = GLA_MODE_READ;
	else if(strcasecmp(modestr, "w") == 0)
		mode = GLA_MODE_WRITE;
	else if(strcasecmp(modestr, "a") == 0) {
		mode = GLA_MODE_WRITE | GLA_MODE_APPEND;
		return gla_rt_vmthrow(rt, "APPEND mode currently not supported"); /* TODO */
	}
	else if(strcasecmp(modestr, "r+") == 0)
		mode = GLA_MODE_READ | GLA_MODE_WRITE | GLA_MODE_APPEND;
	else if(strcasecmp(modestr, "w+") == 0)
		mode = GLA_MODE_READ | GLA_MODE_WRITE;
	else if(strcasecmp(modestr, "a+") == 0) {
		mode = GLA_MODE_READ | GLA_MODE_WRITE | GLA_MODE_APPEND;
		setend = true;
		return gla_rt_vmthrow(rt, "APPEND mode currently not supported"); /* TODO */
	}
	else
		gla_rt_vmthrow(rt, "Invalid file open mode specified");
	if((mode & GLA_MODE_WRITE) != 0)
		flags |= GLA_MOUNT_TARGET | GLA_MOUNT_CREATE;
	else
		flags |= GLA_MOUNT_SOURCE;
	mnt = gla_rt_resolve(rt, &path, flags, NULL, rt->mpstack);

	if(mnt == NULL) {
		if(errno == GLA_NOTFOUND)
			return gla_rt_vmthrow(rt, "No such entity");
		else
			return gla_rt_vmthrow(rt, "Error resolving entity");
	}

	if(apr_pool_create_unmanaged(&pool) != APR_SUCCESS)
		return gla_rt_vmthrow(rt, "Error creating io memory pool");
	io = gla_mount_open(mnt, &path, mode, pool);
	if(io == NULL) {
		apr_pool_destroy(pool);
		return gla_rt_vmthrow(rt, "Error opening entity");
	}
	roff = 0;
	if(setend) {
		woff = 0; /* TODO set to file size and enable append (see above) */
	}
	else
		woff = 0;
	GLA_RT_SUBFN(gla_mod_io_io_new(rt, io, roff, woff, pool), 1, "Error creating new IO instance");

	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_tofilepath(
		HSQUIRRELVM vm)
{
	gla_path_t path;
	gla_mount_t *mnt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	const char *filepath;

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	mnt = gla_rt_resolve(rt, &path, 0, NULL, rt->mpstack);
	if(mnt == NULL) {
		if(errno == GLA_NOTFOUND)
			return gla_rt_vmthrow(rt, "Error converting entity to file system path: path does not exist");
		else
			return gla_rt_vmthrow(rt, "Error resolving path");
	}

	filepath = gla_mount_tofilepath(mnt, &path, false, rt->mpstack);
	if(filepath == NULL) {
		if(errno == GLA_NOTSUPPORTED)
			return gla_rt_vmthrow(rt, "Error converting entity to file system path: operation not supported");
		else
			return gla_rt_vmthrow(rt, "Error converting entity to file system path: unknown error");
	}
	sq_pushstring(rt->vm, filepath, -1);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_exists(
		HSQUIRRELVM vm)
{
	gla_path_t path;
	int ret;
	gla_pathinfo_t info;
	gla_mount_t *mnt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	mnt = gla_rt_resolve(rt, &path, 0, NULL, rt->mpstack);
	if(mnt == NULL) {
		if(errno == GLA_NOTFOUND) {
			sq_pushbool(vm, false);
			return gla_rt_vmsuccess(rt, true);
		}
		else
			return gla_rt_vmthrow(rt, "Error resolving path");
	}
	ret = gla_mount_info(mnt, &info, &path, rt->mpstack);
	if(ret == GLA_NOTFOUND) {
		sq_pushbool(vm, false);
		return gla_rt_vmsuccess(rt, true);
	}
	else if(ret == GLA_SUCCESS) {
		sq_pushbool(vm, true);
		return gla_rt_vmsuccess(rt, true);
	}
	else
		return gla_rt_vmthrow(rt, "Error getting path info from mount device.");
}

static SQInteger fn_touch(
		HSQUIRRELVM vm)
{
	gla_path_t path;
	int ret;
	gla_mount_t *mnt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_TARGET | GLA_MOUNT_CREATE, NULL, rt->mpstack);
	if(mnt == NULL) {
		if(errno == GLA_NOTFOUND)
			return gla_rt_vmthrow(rt, "No such path");
		else
			return gla_rt_vmthrow(rt, "Error resolving path");
	}
	ret = gla_mount_touch(mnt, &path, false, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error touching path");
	
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_rename(
		HSQUIRRELVM vm)
{
	gla_path_t path;
	int ret;
	gla_mount_t *mnt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	const SQChar *renamed;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &renamed)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_TARGET | GLA_MOUNT_CREATE, NULL, rt->mpstack);
	if(mnt == NULL) {
		if(errno == GLA_NOTFOUND)
			return gla_rt_vmthrow(rt, "No such path or path not renameable");
		else
			return gla_rt_vmthrow(rt, "Error resolving path");
	}
	ret = gla_mount_rename(mnt, &path, renamed, rt->mpstack);
	switch(ret) {
		case GLA_SUCCESS:
			return gla_rt_vmsuccess(rt, false);
		case GLA_NOTSUPPORTED:
			return gla_rt_vmthrow(rt, "Error renaming path: operation not supported");
		default:
			return gla_rt_vmthrow(rt, "Error renaming path");
	}
}

static SQInteger fn_erase(
		HSQUIRRELVM vm)
{
	gla_path_t path;
	int ret;
	gla_mount_t *mnt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_TARGET, NULL, rt->mpstack);
	if(mnt == NULL) {
		sq_pushbool(vm, false);
		return gla_rt_vmsuccess(rt, true);
	}
	ret = gla_mount_erase(mnt, &path, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error erasing path");
	
	sq_pushbool(vm, true);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_parse(
		HSQUIRRELVM vm)
{
	int ret;
	const SQChar *string;
	SQBool isleaf = false;
	gla_path_t path;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) < 2 || sq_gettop(vm) > 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &string)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	else if(sq_gettop(vm) >= 3 && SQ_FAILED(sq_getbool(vm, 3, &isleaf)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected bool");
	if(isleaf)
		ret = gla_path_parse_entity(&path, string, rt->mpstack);
	else
		ret = gla_path_parse_package(&path, string, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Invalid path given");
	GLA_RT_SUBFN(set_path(rt, &path, 1), 0, "Error setting path to instance");
	
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_packages(
		HSQUIRRELVM vm)
{
	const SQChar *filter = NULL;
	gla_path_t path;
	int ret;
	btree_t *encountered;
	gla_mount_t **mounts;
	gla_mountit_t *it;
	const char *name;
	HSQOBJECT object;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) > 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(sq_gettop(vm) >= 2 && SQ_FAILED(sq_getstring(vm, 2, &filter)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	if(filter != NULL)
		return gla_rt_vmthrow(rt, "filtering not yet implemented"); /* TODO */

	encountered = btree_new(GLA_BTREE_ORDER, sizeof(HSQOBJECT), cb_cmp1, BTREE_OPT_DEFAULT);
	if(encountered == NULL)
		return gla_rt_vmthrow(rt, "Error allocating temporary btree");
	btree_set_data(encountered, rt);
	apr_pool_cleanup_register(rt->mpstack, encountered, cleanup_btree, apr_pool_cleanup_null);
	GLA_RT_SUBFN(get_path(rt, &path, 1), 0, "Error getting path from instance");
	if(gla_path_type(&path) != GLA_PATH_PACKAGE)
		return gla_rt_vmthrow(rt, "Invalid path type: package expected, entity found");
	mounts = gla_rt_mounts_for(rt, 0, &path, rt->mpstack);
	if(mounts == NULL)
		return gla_rt_vmthrow(rt, "Error getting mountpoints for path");
	sq_newarray(rt->vm, 0);
	while(*mounts) {
		gla_path_shift_many(&path, gla_mount_depth(*mounts));
		it = gla_mount_packages(*mounts, &path, rt->mpstack);
		if(it == NULL) {
			if(errno == GLA_NOTFOUND) {
				mounts++;
				continue;
			}
			else
				return gla_rt_vmthrow(rt, "Error creating package iterator for mountpoint");
		}
		while(true) {
			ret = gla_mountit_next(it, &name, NULL);
			if(ret == GLA_END)
				break;
			else if(ret != GLA_SUCCESS)
				return gla_rt_vmthrow(rt, "IO error iterating packages");
			sq_pushstring(vm, name, -1);
			sq_getstackobj(vm, -1, &object);
			if(btree_get(encountered, &object) == NULL) {
				btree_put(encountered, &object);
				sq_arrayappend(vm, -2);
			}
			else
				sq_poptop(vm);
		}

		gla_path_unshift_many(&path, gla_mount_depth(*mounts));
		mounts++;
	}

	return gla_rt_vmsuccess(rt, true);
}

SQInteger fn_pathit_dtor(
		SQUserPointer up,
		SQInteger size)
{
	pathit_t *it = up;
	btree_destroy(it->encountered);
	apr_pool_destroy(it->pool);
	return SQ_OK;
}

static int fn_packit_set_next(
		gla_rt_t *rt,
		int idx,
		pathit_t *it)
{
	int ret;
	HSQOBJECT object;
	const char *name = NULL;

	idx = sq_toabs(rt->vm, idx);
	sq_pushstring(rt->vm, "name", -1);
	while(true) {
		if(*(it->mounts) == NULL) {
			sq_pushnull(rt->vm);
			break;
		}
		if(it->mntit == NULL) {
			gla_path_shift_many(&it->path, gla_mount_depth(*(it->mounts)));
			it->mntit = gla_mount_packages(*(it->mounts), &it->path, it->pool);
			if(it->mntit == NULL) {
				if(errno == GLA_NOTFOUND || errno == GLA_NOTSUPPORTED) {
					gla_path_unshift_many(&it->path, gla_mount_depth(*(it->mounts)));
					it->mounts++;
					continue;
				}
				else {
					sq_poptop(rt->vm);
					LOG_ERROR("Error creating package iterator for mountpoint");
					return errno;
				}
			}
		}
		ret = gla_mountit_next(it->mntit, &name, NULL);
		if(ret == GLA_END) {
			name = NULL;
			gla_path_unshift_many(&it->path, gla_mount_depth(*(it->mounts)));
			it->mounts++;
			it->mntit = NULL;
			continue;
		}
		else if(ret != GLA_SUCCESS) {
			sq_poptop(rt->vm);
			LOG_ERROR("IO error iterating packages");
			return ret;
		}
		else {
			sq_pushstring(rt->vm, name, -1);
			sq_getstackobj(rt->vm, -1, &object);
			if(btree_get(it->encountered, &object) == NULL) {
				btree_put(it->encountered, &object);
				break;
			}
			sq_poptop(rt->vm);
		}
	}
	if(SQ_FAILED(sq_set(rt->vm, idx)))
		return GLA_VM;
	return GLA_SUCCESS;
}

static SQInteger fn_packit_init(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	pathit_t *it;
	gla_path_t path;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	it = up;
	memset(it, 0, sizeof(pathit_t));
	GLA_RT_SUBFN(get_path(rt, &path, 2), 0, "Error getting path from instance");
	if(gla_path_type(&path) != GLA_PATH_PACKAGE)
		return gla_rt_vmthrow(rt, "Invalid path type: package expected, entity found");
	if(apr_pool_create_unmanaged(&it->pool) != APR_SUCCESS)
		return gla_rt_vmthrow(rt, "Error initializing memory pool");
	it->encountered = btree_new(GLA_BTREE_ORDER, sizeof(HSQOBJECT), cb_cmp1, BTREE_OPT_DEFAULT);
	if(it->encountered == NULL) {
		return gla_rt_vmthrow(rt, "Error creating btree");
		apr_pool_destroy(it->pool);
	}
	btree_set_data(it->encountered, rt);
	btree_sethook_refcount(it->encountered, cb_acquire1, cb_release1);
	sq_setreleasehook(rt->vm, 1, fn_pathit_dtor);

	gla_path_clone(&it->path, &path, it->pool);
	it->mounts = gla_rt_mounts_for(rt, 0, &path, it->pool);
	if(it->mounts == NULL)
		return gla_rt_vmthrow(rt, "Error getting mountpoints for path");
	GLA_RT_SUBFN(fn_packit_set_next(rt, 1, it), 0, "Error setting first entry");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_packit_next(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	pathit_t *it;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	it = up;
	GLA_RT_SUBFN(fn_packit_set_next(rt, 1, it), 0, "Error iterating next entry");
	return gla_rt_vmsuccess(rt, false);
}

static int fn_entityit_set_next(
		gla_rt_t *rt,
		int idx,
		pathit_t *it)
{
	int ret;
	HSQOBJECT object[2];
	const char *name = NULL;
	const char *extension = NULL;

	idx = sq_toabs(rt->vm, idx);
	while(true) {
		if(*(it->mounts) == NULL) {
			sq_pushnull(rt->vm);
			sq_pushnull(rt->vm);
			break;
		}
		if(it->mntit == NULL) {
			gla_path_shift_many(&it->path, gla_mount_depth(*(it->mounts)));
			it->mntit = gla_mount_entities(*(it->mounts), &it->path, it->pool);
			if(it->mntit == NULL) {
				if(errno == GLA_NOTFOUND || errno == GLA_NOTSUPPORTED) {
					gla_path_unshift_many(&it->path, gla_mount_depth(*(it->mounts)));
					it->mounts++;
					continue;
				}
				else {
					LOG_ERROR("Error creating package iterator for mountpoint");
					return errno;
				}
			}
		}
		ret = gla_mountit_next(it->mntit, &name, &extension);
		if(ret == GLA_END) {
			name = NULL;
			gla_path_unshift_many(&it->path, gla_mount_depth(*(it->mounts)));
			it->mounts++;
			it->mntit = NULL;
			continue;
		}
		else if(ret != GLA_SUCCESS) {
			LOG_ERROR("IO error iterating packages");
			return ret;
		}
		else {
			sq_pushstring(rt->vm, name, -1);
			sq_pushstring(rt->vm, extension, -1);
			sq_getstackobj(rt->vm, -2, object + 0);
			sq_getstackobj(rt->vm, -1, object + 1);
			if(btree_get(it->encountered, object) == NULL) {
				btree_put(it->encountered, object);
				break;
			}
			sq_pop(rt->vm, 2);
		}
	}
	sq_pushstring(rt->vm, "name", -1);
	sq_push(rt->vm, -3);
	if(SQ_FAILED(sq_set(rt->vm, idx)))
		return GLA_VM;
	sq_pushstring(rt->vm, "extension", -1);
	sq_push(rt->vm, -2);
	if(SQ_FAILED(sq_set(rt->vm, idx)))
		return GLA_VM;
	sq_pop(rt->vm, 2);
	return GLA_SUCCESS;
}

static SQInteger fn_entityit_init(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	pathit_t *it;
	gla_path_t path;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	it = up;
	memset(it, 0, sizeof(pathit_t));
	GLA_RT_SUBFN(get_path(rt, &path, 2), 0, "Error getting path from instance");
	if(gla_path_type(&path) != GLA_PATH_PACKAGE)
		return gla_rt_vmthrow(rt, "Invalid path type: package expected, entity found");
	if(apr_pool_create_unmanaged(&it->pool) != APR_SUCCESS)
		return gla_rt_vmthrow(rt, "Error initializing memory pool");
	it->encountered = btree_new(GLA_BTREE_ORDER, 2 * sizeof(HSQOBJECT), cb_cmp2, BTREE_OPT_DEFAULT);
	if(it->encountered == NULL) {
		return gla_rt_vmthrow(rt, "Error creating btree");
		apr_pool_destroy(it->pool);
	}
	btree_set_data(it->encountered, rt);
	btree_sethook_refcount(it->encountered, cb_acquire2, cb_release2);
	sq_setreleasehook(rt->vm, 1, fn_pathit_dtor);

	gla_path_clone(&it->path, &path, it->pool);
	it->mounts = gla_rt_mounts_for(rt, 0, &path, it->pool);
	if(it->mounts == NULL)
		return gla_rt_vmthrow(rt, "Error getting mountpoints for path");
	GLA_RT_SUBFN(fn_entityit_set_next(rt, 1, it), 0, "Error setting first entry");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_entityit_next(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	pathit_t *it;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	it = up;
	GLA_RT_SUBFN(fn_entityit_set_next(rt, 1, it), 0, "Error iterating next entry");
	return gla_rt_vmsuccess(rt, false);
}

SQInteger gla_mod_io_packpath_augment(
		HSQUIRRELVM vm)
{
	rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL)
		return gla_rt_vmthrow(rt, "Error getting rt data");
	sq_getstackobj(vm, 2, &rtdata->packpath_class);

	sq_pushstring(vm, "PACKAGE", -1);
	sq_pushinteger(vm, GLA_PATH_PACKAGE);
	sq_newslot(vm, 2, true);

	sq_pushstring(vm, "ENTITY", -1);
	sq_pushinteger(vm, GLA_PATH_ENTITY);
	sq_newslot(vm, 2, true);

	sq_pushstring(vm, "MAX_DEPTH", -1);
	sq_pushinteger(vm, GLA_PATH_MAX_DEPTH);
	sq_newslot(vm, 2, true);

	sq_pushstring(vm, "tofilepath", -1);
	sq_newclosure(vm, fn_tofilepath, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "exists", -1);
	sq_newclosure(vm, fn_exists, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "touch", -1);
	sq_newclosure(vm, fn_touch, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "parse", -1);
	sq_newclosure(vm, fn_parse, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "rename", -1);
	sq_newclosure(vm, fn_rename, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "erase", -1);
	sq_newclosure(vm, fn_erase, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "_open", -1); /* TODO move this function to rt.c; mind comment in path.h */
	sq_newclosure(vm, fn_open, 0);
	sq_newslot(vm, 2, false);

/*	sq_pushstring(vm, "packages", -1);
	sq_newclosure(vm, fn_packages, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "entities", -1);
	sq_newclosure(vm, fn_entities, 0);
	sq_newslot(vm, 2, false);*/

	return gla_rt_vmsuccess(rt, true);
}

SQInteger gla_mod_io_packpath_packit_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "_init", -1);
	sq_newclosure(vm, fn_packit_init, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "next", -1);
	sq_newclosure(vm, fn_packit_next, 0);
	sq_newslot(vm, 2, false);

	sq_setclassudsize(vm, 2, sizeof(pathit_t));

	return gla_rt_vmsuccess(rt, true);
}

SQInteger gla_mod_io_packpath_entityit_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "_init", -1);
	sq_newclosure(vm, fn_entityit_init, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "next", -1);
	sq_newclosure(vm, fn_entityit_next, 0);
	sq_newslot(vm, 2, false);

	sq_setclassudsize(vm, 2, sizeof(pathit_t));

	return gla_rt_vmsuccess(rt, true);
}

int gla_mod_io_packpath_get(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool)
{
	gla_path_t org;
	if(clone) {
		GLA_RT_SUBSUBFN(get_path(rt, &org, idx), 0, "Error getting path from instance");
		return gla_path_clone(path, &org, pool);
	}
	else {
		GLA_RT_SUBSUBFN(get_path(rt, path, idx), 0, "Error getting path from instance");
		return GLA_SUCCESS;
	}
}

