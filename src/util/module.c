#include <assert.h>
#include <apr_pools.h>

#include "stackexec.h"
#include "exec.h"

#include "module.h"

static int push_cbridge(
		gla_rt_t *rt,
		void *module_data_,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	HSQUIRRELVM vm = gla_rt_vm(rt);

	sq_newtable(vm);

	sq_pushstring(vm, "StackExecuter", -1);
	sq_newclosure(vm, gla_mod_util_stackexec_augment, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "Executer", -1);
	sq_newclosure(vm, gla_mod_util_exec_augment, 0);
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

int gla_mod_util_register(
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

