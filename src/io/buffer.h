#pragma once

#include "common.h"

SQInteger gla_mod_io_buffer_augment(
		HSQUIRRELVM vm);

/* push a new buffer instance to opstack and return its io handle */
gla_io_t *gla_mod_io_buffer_new(
		gla_rt_t *rt);

