#pragma once

#include <apr_pools.h>

#include "common.h"

/* sets errno on failure */
gla_store_t *gla_store_new(
		apr_pool_t *pool);

/* sets errno in case GLA_ID_INVALID is returned (GLA_SUCCESS: root path has been given) */
gla_id_t gla_store_path_get(
		gla_store_t *self,
		const gla_path_t *path);

/* sets errno in case GLA_ID_INVALID is returned (GLA_SUCCESS: root path has been given) */
gla_id_t gla_store_path_try(
		gla_store_t *self,
		const gla_path_t *path);

/* sets errno in case GLA_ID_INVALID is returned (GLA_SUCCESS: root path has been given) */
gla_id_t gla_store_path_deepest(
		gla_store_t *self,
		const gla_path_t *path,
		int *depth);

/* sets errno in case GLA_ID_INVALID is returned */
gla_id_t gla_store_path_parent(
		gla_store_t *self,
		gla_id_t package);

/* sets errno in case GLA_ID_INVALID is returned */
gla_id_t gla_store_path_first_child(
		gla_store_t *self,
		gla_id_t package);

/* sets errno in case GLA_ID_INVALID is returned */
gla_id_t gla_store_path_next_sibling(
		gla_store_t *self,
		gla_id_t package);

/* returns name of given package; NULL for root/invalid/not in store;
 * NOT whole path, just the last fragment;
 * does not set errno in case NULL is returned */
const char *gla_store_path_name(
		gla_store_t *self,
		gla_id_t package);

/* add a static string to the store. 'string' must be valid until
 * store is destroyed. */
int gla_store_string_static(
		gla_store_t *self,
		const char *string);

/* sets errno in case NULL has been returned (GLA_SUCCESS: NULL string has been given) */
const char *gla_store_string_get(
		gla_store_t *self,
		const char *string);

const char *gla_store_string_try(
		gla_store_t *self,
		const char *string);

void gla_store_gc(
		gla_store_t *self);

