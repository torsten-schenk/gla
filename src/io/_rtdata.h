#pragma once

#include "common.h"

#include "module.h"

#define RTDATA_TOKEN gla_mod_io_register

typedef struct {
	HSQOBJECT io_class;
	HSQOBJECT hash_class;
	HSQOBJECT serialize_string;
	HSQOBJECT deserialize_string;
	HSQOBJECT packpath_class;
} rtdata_t;

