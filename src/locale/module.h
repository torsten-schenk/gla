#pragma once

#include <apr_pools.h>

int gla_mod_locale_register(
		gla_rt_t *rt,
		const gla_path_t *root,
		apr_pool_t *pool,
		apr_pool_t *tmp);

