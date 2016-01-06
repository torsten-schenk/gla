#include <assert.h>
#include <apr_pools.h>
#include <gla/squirrel.h>
#include <sqstdstring.h>

#include "module.h"

static int push_stringlib(
		gla_rt_t *rt,
		void *module_data_,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	HSQUIRRELVM vm = gla_rt_vm(rt);

	sq_newtable(vm);
	sqstd_register_stringlib(vm);

	return GLA_SUCCESS;
}

int gla_mod_sqstdlib_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	gla_path_t path;
	
	path = *root;
	ret = gla_path_parse_append_entity(&path, "<" GLA_ENTITY_EXT_OBJECT ">stringlib", pool);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_mount_object(rt, &path, push_stringlib, NULL);
	if(ret != GLA_SUCCESS)
		return ret;

	return GLA_SUCCESS;
}

