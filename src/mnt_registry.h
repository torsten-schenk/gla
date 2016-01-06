#pragma once

#include <apr_pools.h>

#include "common.h"

gla_mount_t *gla_mount_registry_new(
		gla_rt_t *rt);

int gla_mount_registry_put(
		gla_mount_t *self,
		const gla_path_t *path,
		int idx,
		bool allow_override);

