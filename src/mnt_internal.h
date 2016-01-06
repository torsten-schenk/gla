#pragma once

#include <apr_pools.h>

#include "common.h"

/* sets errno on error */
gla_mount_t *gla_mount_internal_new(
		gla_store_t *store,
		apr_pool_t *pool);

int gla_mount_internal_add_object(
		gla_mount_t *self,
		const gla_path_t *path, /* registered entity: [root].[path]*/
		int (*push)(gla_rt_t *rt, void *user, apr_pool_t *pperm, apr_pool_t *ptemp), /* pperm: permanent storage until mount is deleted; ptemp: temporary storage until 'push' is finished */
		void *user);


int gla_mount_internal_add_read_buffer(
		gla_mount_t *self,
		const gla_path_t *path,
		const void *buffer,
		int size);

