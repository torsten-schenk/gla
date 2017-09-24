#include <assert.h>
#include <apr_pools.h>

#include "_rtdata.h"
#include "hash.h"
#include "io.h"
#include "buffer.h"
#include "file.h"
#include "packpath.h"

#include "module.h"

static int push_cbridge(
		gla_rt_t *rt,
		void *module_data_,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	rtdata_t *data;
	HSQUIRRELVM vm = gla_rt_vm(rt);

	data = apr_pcalloc(pool, sizeof(rtdata_t));
	ret = gla_rt_data_put(rt, RTDATA_TOKEN, data);
	if(ret != GLA_SUCCESS)
		return ret;
	
	sq_newtable(vm);

	sq_pushstring(vm, "IO", -1);
	sq_newclosure(vm, gla_mod_io_io_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "Buffer", -1);
	sq_newclosure(vm, gla_mod_io_buffer_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "File", -1);
	sq_newclosure(vm, gla_mod_io_file_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "PackagePath", -1);
	sq_newclosure(vm, gla_mod_io_packpath_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "PackagePath_PackageIterator", -1);
	sq_newclosure(vm, gla_mod_io_packpath_packit_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "PackagePath_EntityIterator", -1);
	sq_newclosure(vm, gla_mod_io_packpath_entityit_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "HashFunction", -1);
	sq_newclosure(vm, gla_mod_io_hash_augment, 0);
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

int gla_mod_io_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	gla_path_t path;
	
	path = *root;
	ret = gla_path_parse_append_entity(&path, "<" GLA_ENTITY_EXT_OBJECT ">cbridge", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_mount_object(rt, &path, push_cbridge, NULL);
	if(ret != GLA_SUCCESS)
		return ret;

	return GLA_SUCCESS;
}

int gla_mod_io_path_get(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool)
{
	HSQUIRRELVM vm = gla_rt_vm(rt);
	rtdata_t *rtdata;
	HSQOBJECT object;
	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL) /* here: cbridge not yet initialized, i.e. nothing of module io imported yet. Therefore, it is impossible for the instance at 'idx' to be a PackagePath or any other supported path type. */
		return GLA_INVALID_ARGUMENT;

	idx = sq_toabs(vm, idx);
	if(SQ_FAILED(sq_getclass(vm, idx)))
		return GLA_INVALID_ARGUMENT;
	sq_getstackobj(vm, -1, &object);
	sq_poptop(vm);
	if(memcmp(&object, &rtdata->packpath_class, sizeof(HSQOBJECT)) == 0) {
		gla_mod_io_packpath_get(path, clone, rt, idx, pool);
		return GLA_SUCCESS;
	}
	else
		return GLA_INVALID_ARGUMENT;
}

