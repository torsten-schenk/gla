#pragma once

#include <apr_pools.h>

#include "common.h"

int gla_mod_io_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp);

/* check for PackagePath, FilePath, ... instance at 'idx' and convert it to a path */
int gla_mod_io_path_get(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool);

