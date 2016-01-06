#pragma once

#include <apr_pools.h>

#include "common.h"

enum {
	GLA_MNT_DIR_FILENAMES = 0x01, /* use filenames instead of entity/type name, this also implies handling files with invalid entity name */
	GLA_MNT_DIR_DIRNAMES = 0x02 /* use dirnames instead of package names, i.e. also handle directories whith an invalid package name */
};

/* sets errno on NULL return */
gla_mount_t *gla_mount_dir_new(
		const char *dir,
		int options,
		apr_pool_t *pool);

