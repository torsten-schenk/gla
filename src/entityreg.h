#pragma once

#include <apr_pools.h>

#include "common.h"

/* sets errno in case NULL is returned */
gla_entityreg_t *gla_entityreg_new(
		gla_store_t *store,
		int entry_size,
		apr_pool_t *pool);

/* sets errno; if not NULL is returned, errno can either be GLA_SUCCESS (if entry existed before) or GLA_NOTFOUND (if entry didn't exist before)
 * NOTE: returned address is constant and will not change after insert/remove operations */
void *gla_entityreg_get(
		gla_entityreg_t *self,
		const gla_path_t *path);

void *gla_entityreg_try(
		gla_entityreg_t *self,
		const gla_path_t *path);

/* get the path for an enty. 
 * CAUTION: only pass 'data' that has been retrieved by _try or _get! */
int gla_entityreg_path_for(
		gla_entityreg_t *self,
		gla_path_t *path,
		const void *data,
		apr_pool_t *pool);

/*gla_entityreg_entity_first_child(
		gla_entityreg_t *self,
		gla_id_t package);*/

/* returns whether the given package is used within the registry.
 * This includes all parent packages of all registered entities/packages,
 * i.e. if pack1.pack2.Test is registered, package pack1 and pack1.pack2
 * will return success here. */
int gla_entityreg_covered(
		gla_entityreg_t *self,
		const gla_path_t *path);


/* begin iteration of package. iterates all registered entities
 * whithin the given package.
 * returned error codes:
 * - GLA_NOTFOUND: nothing registered in given package */
int gla_entityreg_find_first(
		gla_entityreg_t *self,
		gla_id_t package_id,
		gla_entityreg_it_t *it);

int gla_entityreg_find_last(
		gla_entityreg_t *self,
		gla_id_t package_id,
		gla_entityreg_it_t *it);

int gla_entityreg_iterate_next(
		gla_entityreg_it_t *it);

int gla_entityreg_iterate_prev(
		gla_entityreg_it_t *it);

/* sets errno in case GLA_ID_INVALID is returned:
 *   - GLA_END: no children
 *   - GLA_NOTFOUND: package not covered by registry */
gla_id_t gla_entityreg_covered_children(
		gla_entityreg_t *self,
		gla_id_t package);

/* sets errno in case GLA_ID_INVALID is returned:
 *   - GLA_END: no children
 *   - GLA_NOTFOUND: package not covered by registry */
gla_id_t gla_entityreg_covered_next(
		gla_entityreg_t *self,
		gla_id_t package);

gla_store_t *gla_entityreg_store(
		gla_entityreg_t *self);

apr_pool_t *gla_entityreg_pool(
		gla_entityreg_t *self);

