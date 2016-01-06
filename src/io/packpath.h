#pragma once

#include "common.h"

SQInteger gla_mod_io_packpath_augment(
		HSQUIRRELVM vm);

SQInteger gla_mod_io_packpath_packit_augment(
		HSQUIRRELVM vm);

SQInteger gla_mod_io_packpath_entityit_augment(
		HSQUIRRELVM vm);

/* does not check for class, any instance conforming to PackagePath members will succeed here.
 * Use gla_mod_io_path_get() instead */
int gla_mod_io_packpath_get(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool);

