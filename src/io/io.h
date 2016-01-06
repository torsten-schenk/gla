#pragma once

#include "common.h"

SQInteger gla_mod_io_io_augment(
		HSQUIRRELVM vm);

/* push a new IO instance to stack that will handle 'io';
 * 'pool' denotes a pool that will be used while 'io' is opened */
int gla_mod_io_io_new(
		gla_rt_t *rt,
		gla_io_t *io,
		uint64_t roff,
		uint64_t woff,
		apr_pool_t *pool);

int gla_mod_io_io_init(
		gla_rt_t *rt,
		int idx,
		gla_io_t *io,
		uint64_t roff,
		uint64_t woff,
		apr_pool_t *pool);

gla_io_t *gla_mod_io_io_get(
		gla_rt_t *rt,
		int idx);

