#pragma once

SQInteger gla_mod_io_file_augment(
		HSQUIRRELVM vm);

/* push a new buffer instance to opstack and return its io handle */
gla_io_t *gla_mod_io_file_new(
		gla_rt_t *rt);

