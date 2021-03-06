#include <assert.h>
#include <apr_pools.h>

#include "unicode.h"

#include "module.h"

static int push_cbridge(
		gla_rt_t *rt,
		void *module_data_,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	HSQUIRRELVM vm = gla_rt_vm(rt);

	sq_newtable(vm);

	sq_pushstring(vm, "unicode", -1);
	sq_newtable(vm);
	ret = gla_mod_string_unicode_cbridge(rt);
	if(ret != GLA_SUCCESS)
		return ret;
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

int gla_mod_string_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	gla_path_t path;
	
	path = *root;
	ret = gla_path_parse_append_entity(&path, "<" GLA_ENTITY_EXT_OBJECT ">_cbridge", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_mount_object(rt, &path, push_cbridge, NULL);
	if(ret != GLA_SUCCESS)
		return ret;

	return GLA_SUCCESS;
}

