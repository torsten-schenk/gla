#pragma once

#include <apr_pools.h>

#include "common.h"

/* instead of using a separate stdio mount, use mnt_internal and create a new method add_device; maybe register a gla_io_t struct there */
/* sets errno on error */
gla_mount_t *gla_mount_stdio_new(
		apr_pool_t *pool);

