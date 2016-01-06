#pragma once

#include <apr_pools.h>

#include "common.h"

/* sets errno in case NULL is returned */
gla_packreg_t *gla_packreg_new(
		gla_store_t *store,
		int entry_size,
		apr_pool_t *pool);

/* sets errno; if not NULL is returned, errno can either be GLA_SUCCESS (if entry existed before) or GLA_NOTFOUND (if entry didn't exist before) */
void *gla_packreg_get(
		gla_packreg_t *self,
		const gla_path_t *path);

void *gla_packreg_try(
		gla_packreg_t *self,
		const gla_path_t *path);

/*gla_packreg_entity_first_child(
		gla_packreg_t *self,
		gla_id_t package);*/

/* returns whether the given package is used within the registry.
 * This includes all parent packages of all registered entities/packages,
 * i.e. if pack1.pack2.Test is registered, package pack1 and pack1.pack2
 * will return success here. */
int gla_packreg_covered(
		gla_packreg_t *self,
		const gla_path_t *path);


/* only works if registry stores packages; sets errno (see also gla_packreg_get) */
void *gla_packreg_get_id(
		gla_packreg_t *self,
		gla_id_t package_id);

void *gla_packreg_try_id(
		gla_packreg_t *self,
		gla_id_t package_id);


/* begin iteration of package. iterates all registered things (i.e. entities/packages)
 * whithin the given package. NOTE: in case of package registry,
 * packages which are not registered (i.e. those which are only implicitly covered by
 * the registry due to a deeper registered package) will NOT be iterated.
 * returned error codes:
 * - GLA_NOTFOUND: nothing registered in given package */
/*int gla_packreg_find_first(
		gla_packreg_t *self,
		gla_id_t package_id,
		gla_packreg_it_t *it);

int gla_packreg_iterate_next(
		gla_packreg_it_t *it);

int gla_packreg_iterate_prev(
		gla_packreg_it_t *it);*/

/* sets errno in case GLA_ID_INVALID is returned:
 *   - GLA_END: no children
 *   - GLA_NOTFOUND: package not covered by registry */
gla_id_t gla_packreg_covered_children(
		gla_packreg_t *self,
		gla_id_t package);

/* sets errno in case GLA_ID_INVALID is returned:
 *   - GLA_END: no children
 *   - GLA_NOTFOUND: package not covered by registry */
gla_id_t gla_packreg_covered_next(
		gla_packreg_t *self,
		gla_id_t package);

gla_store_t *gla_packreg_store(
		gla_packreg_t *self);

apr_pool_t *gla_packreg_pool(
		gla_packreg_t *self);

