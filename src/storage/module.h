#pragma once

#include <apr_pools.h>

#include "common.h"

int gla_mod_storage_init(
		apr_pool_t *pool);

int gla_mod_storage_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp);

