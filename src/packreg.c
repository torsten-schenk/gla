#include <errno.h>

#include "common.h"

#include "store.h"
#include "packreg.h"

typedef struct {
	gla_id_t id;
} binding_t;

typedef struct {
	binding_t binding; /* MUST be first member (see cmp function) */
	char data[1];
} entry_t;

struct gla_packreg {
	int entry_size;
	int type;
	gla_store_t *store;
	apr_pool_t *pool;
	btree_t *entries;
	btree_t *covered;
};

/*static void dump_entity(
		const void *a_)
{
	const entity_entry_t *a = a_;
	printf("%s (%d)", a->binding.name, a->binding.package);
}*/

static int cmp_covered(
		btree_t *btree,
		const void *a_,
		const void *b_,
		void *group)
{
	const gla_id_t *a = a_;
	const gla_id_t *b = b_;
	if(*a < *b)
		return -1;
	else if(*a > *b)
		return 1;
	else
		return 0;
}

static int cmp_entries(
		btree_t *btree,
		const void *a_,
		const void *b_,
		void *group)
{
	const binding_t *a = a_;
	const binding_t *b = b_;

	if(a->id < b->id)
		return -1;
	else if(a->id > b->id)
		return 1;
	else
		return 0;
}

static apr_status_t cleanup(
		void *self_)
{
	gla_packreg_t *self = self_;

	btree_destroy(self->covered);
	btree_destroy(self->entries);
	return APR_SUCCESS;
}

gla_packreg_t *gla_packreg_new(
		gla_store_t *store,
		int entry_size,
		apr_pool_t *pool)
{
	gla_packreg_t *self;

	self = apr_palloc(pool, sizeof(gla_packreg_t));
	if(self == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	apr_pool_cleanup_register(pool, self, cleanup, apr_pool_cleanup_null);

	self->entries = btree_new(GLA_BTREE_ORDER, -1, cmp_entries, BTREE_OPT_DEFAULT);
	if(self->entries == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	self->covered = btree_new(GLA_BTREE_ORDER, sizeof(gla_id_t), cmp_covered, BTREE_OPT_DEFAULT);
	if(self->covered == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	self->store = store;
	self->pool = pool;
	self->entry_size = entry_size;

	return self;
}

void *gla_packreg_get(
		gla_packreg_t *self,
		const gla_path_t *path)
{
	int ret;

	errno = GLA_SUCCESS;
	binding_t binding;
	entry_t  *entry;

	if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	memset(&binding, 0, sizeof(binding));
	binding.id = gla_store_path_get(self->store, path);
	if(binding.id == GLA_ID_INVALID && errno != GLA_SUCCESS)
		return NULL;
	entry = btree_get(self->entries, &binding);
	if(entry == NULL) {
		gla_id_t curid = binding.id;
		while(curid != GLA_ID_INVALID && !btree_contains(self->covered, &curid)) {
			ret = btree_insert(self->covered, &curid);
			if(ret == -ENOMEM) {
				errno = GLA_ALLOC;
				return NULL;
			}
			else if(ret != 0) {
				errno = GLA_INTERNAL;
				return NULL;
			}
			curid = gla_store_path_parent(self->store, curid);
		}
		entry = apr_pcalloc(self->pool, sizeof(*entry) + self->entry_size - 1);
		if(entry == NULL) {
			errno = GLA_ALLOC;
			return NULL;
		}
		entry->binding = binding;
		ret = btree_insert(self->entries, entry);
		if(entry == NULL) {
			errno = GLA_ALLOC;
			return NULL;
		}
		errno = GLA_NOTFOUND;
	}
	else
		errno = GLA_SUCCESS;
	return entry->data;
}

void *gla_packreg_try(
		gla_packreg_t *self,
		const gla_path_t *path)
{
	binding_t binding;
	entry_t  *entry;

	if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	memset(&binding, 0, sizeof(binding));
	binding.id = gla_store_path_get(self->store, path);
	if(binding.id == GLA_ID_INVALID && errno != GLA_SUCCESS)
		return NULL;
	entry = btree_get(self->entries, &binding);
	if(entry == NULL) {
		errno = GLA_NOTFOUND;
		return NULL;
	}
	else {
		errno = GLA_SUCCESS;
		return entry->data;
	}
}

int gla_packreg_covered(
		gla_packreg_t *self,
		const gla_path_t *path)
{
	if(gla_path_is_root(path))
		return GLA_SUCCESS;
	else if(gla_path_type(path) != GLA_PATH_PACKAGE)
		return GLA_INVALID_PATH_TYPE;
	else {
		gla_id_t package_id = gla_store_path_try(self->store, path);
		if(package_id == GLA_ID_INVALID)
			return GLA_NOTFOUND;
		else if(btree_contains(self->covered, &package_id))
			return GLA_SUCCESS;
		else
			return GLA_NOTFOUND;
	}
}

void *gla_packreg_get_id(
		gla_packreg_t *self,
		gla_id_t package_id)
{
	int ret;
	binding_t binding;
	entry_t *entry;

	errno = GLA_SUCCESS;
	binding.id = package_id;
	entry = btree_get(self->entries, &binding);
	if(entry == NULL) {
		entry = apr_pcalloc(self->pool, sizeof(*entry) + self->entry_size - 1);
		if(entry == NULL) {
			errno = GLA_ALLOC;
			return NULL;
		}
		entry->binding = binding;
		ret = btree_insert(self->entries, entry);
		if(ret != 0) {
			errno = GLA_INTERNAL;
			return NULL;
		}
		errno = GLA_NOTFOUND;
	}
	return entry->data;
}

void *gla_packreg_try_id(
		gla_packreg_t *self,
		gla_id_t package_id)
{
	binding_t binding;
	entry_t *entry;

	binding.id = package_id;
	entry = btree_get(self->entries, &binding);
	if(entry == NULL) {
		errno = GLA_NOTFOUND;
		return NULL;
	}
	else
		return entry->data;
}

gla_id_t gla_packreg_covered_children(
		gla_packreg_t *self,
		gla_id_t package_id)
{
	gla_id_t cur;

	if(!btree_contains(self->covered, &package_id)) {
		errno = GLA_NOTFOUND;
		return GLA_ID_INVALID;
	}
	cur = gla_store_path_first_child(self->store, package_id);
	while(cur != GLA_ID_INVALID && !btree_contains(self->covered, &cur))
		cur = gla_store_path_next_sibling(self->store, cur); /* also sets errno whilch will be used for this function if any error occured */
	return cur;
}

gla_id_t gla_packreg_covered_next(
		gla_packreg_t *self,
		gla_id_t package_id)
{
	gla_id_t cur = package_id;

	cur = gla_store_path_next_sibling(self->store, cur);
	while(cur != GLA_ID_INVALID && !btree_contains(self->covered, &cur))
		cur = gla_store_path_next_sibling(self->store, cur); /* also sets errno whilch will be used for this function if any error occured */
	return cur;
}

gla_store_t *gla_packreg_store(
		gla_packreg_t *self)
{
	return self->store;
}

apr_pool_t *gla_packreg_pool(
		gla_packreg_t *self)
{
	return self->pool;
}

/*#ifdef TESTING
#include "test/registry.h"
#endif*/

