#pragma once

#include "common.h"

SQInteger gla_mod_io_hash_augment(
		HSQUIRRELVM vm);

int gla_mod_io_hash_calc(
		gla_rt_t *rt,
		void *self, /* userdata pointer of hash function instance */
		gla_io_t *src, /* data to process */
		int64_t bytes, /* number of bytes to process; -1: until end; if end reached but bytes remaining, return GLA_UNDERFLOW */
		const char *push_type, /* value type to push onto squirrel stack (NULL: don't push) */
		gla_io_t *dest); /* io to copy result to (NULL: don't copy) */

