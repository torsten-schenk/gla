#include <assert.h>
#include <apr_pools.h>

#include "stackexec.h"
#include "exec.h"
#include "time.h"
#include "date.h"

#include "module.h"

static SQInteger fn_strcoll(
		HSQUIRRELVM vm)
{
	const SQChar *arg_a;
	const SQChar *arg_b;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &arg_a)))
		return gla_rt_vmthrow(rt, "invalid argument: expected string");
	else if(SQ_FAILED(sq_getstring(vm, 3, &arg_b)))
		return gla_rt_vmthrow(rt, "invalid argument: expected string");
	sq_pushinteger(vm, strcoll(arg_a, arg_b));
	return gla_rt_vmsuccess(rt, true);
}

static int push_strlib(
		gla_rt_t *rt,
		void *module_data_,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	HSQUIRRELVM vm = gla_rt_vm(rt);

	sq_newtable(vm);

	sq_pushstring(vm, "strcoll", -1);
	sq_newclosure(vm, fn_strcoll, 0);
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

int gla_mod_locale_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp)
{
	int ret;
	gla_path_t path;
	
	path = *root;
	ret = gla_path_parse_append_entity(&path, "<" GLA_ENTITY_EXT_OBJECT ">strlib", tmp);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_mount_object(rt, &path, push_strlib, NULL);
	if(ret != GLA_SUCCESS)
		return ret;

	return GLA_SUCCESS;
}

