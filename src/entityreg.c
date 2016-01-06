#include <errno.h>
#include <stddef.h>

#include "common.h"

#include "store.h"
#include "entityreg.h"

typedef struct {
	gla_id_t package;
	const char *name;
	const char *extension; /* NULL or non-empty */
} binding_t;

typedef struct {
	binding_t binding; /* MUST be first member (see cmp function) */
	char data[1];
} entry_t;

struct gla_entityreg {
	int entry_size;
	gla_store_t *store;
	apr_pool_t *pool;
	btree_t *entries;
	btree_t *covered;
};

/*static void dump_entry(
		const void *a_)
{
	const entry_t *a = a_;
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

static int cmp_entries_package(
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
		const void *a_,
		const void *b_)
{
	const binding_t *a = a_;
	const binding_t *b = b_;
	int cmp;

	if(a->package < b->package)
		return -1;
	else if(a->package > b->package)
		return 1;
	cmp = strcmp(a->name, b->name);
	if(cmp != 0)
		return cmp;
	if(a->extension == NULL && b->extension == NULL)
		return 0;
	else if(a->extension == NULL)
		return -1;
	else if(b->extension == NULL)
		return 1;
	else
		return strcmp(a->extension, b->extension);
}

static int cb_cmp_entries(
		btree_t *btree,
		const void *a,
		const void *b,
		void *group)
{
	int (*cmp)(const void*, const void*) = group;
	return cmp(a, b);
}

static apr_status_t cleanup(
		void *self_)
{
	gla_entityreg_t *self = self_;

	btree_destroy(self->covered);
	btree_destroy(self->entries);
	return APR_SUCCESS;
}

gla_entityreg_t *gla_entityreg_new(
		gla_store_t *store,
		int entry_size,
		apr_pool_t *pool)
{
	gla_entityreg_t *self;

	self = apr_palloc(pool, sizeof(gla_entityreg_t));
	if(self == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	apr_pool_cleanup_register(pool, self, cleanup, apr_pool_cleanup_null);

	self->entries = btree_new(GLA_BTREE_ORDER, -1, cb_cmp_entries, BTREE_OPT_DEFAULT);
	if(self->entries == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	btree_set_group_default(self->entries, cmp_entries);

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

void *gla_entityreg_get(
		gla_entityreg_t *self,
		const gla_path_t *path)
{
	int ret;

	errno = GLA_SUCCESS;
	gla_path_t package_path;
	binding_t binding;
	entry_t *entry;

	if(gla_path_type(path) != GLA_PATH_ENTITY) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	memset(&binding, 0, sizeof(binding));
	package_path = *path;
	gla_path_package(&package_path);
	binding.package = gla_store_path_get(self->store, &package_path);
	if(binding.package == GLA_ID_INVALID)
		return NULL;
	binding.name = gla_store_string_get(self->store, path->entity);
	if(binding.name == NULL)
		return NULL;
	if(path->extension != NULL && *path->extension != 0) {
		binding.extension = gla_store_string_get(self->store, path->extension);
		if(binding.extension == NULL)
			return NULL;
	}

	entry = btree_get(self->entries, &binding);
	if(entry == NULL) {
		gla_id_t curid = binding.package;
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
		if(ret != 0) {
			errno = GLA_INTERNAL;
			return NULL;
		}
		errno = GLA_NOTFOUND;
/*	btree_dump(self->entries, dump_entry); */
	}
	else
		errno = GLA_SUCCESS;
	return entry->data;
}

void *gla_entityreg_try(
		gla_entityreg_t *self,
		const gla_path_t *path)
{
	gla_path_t package_path;
	binding_t binding;
	entry_t *entry;

	if(gla_path_type(path) != GLA_PATH_ENTITY) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	memset(&binding, 0, sizeof(binding));
	package_path = *path;
	gla_path_package(&package_path);
	binding.package = gla_store_path_try(self->store, &package_path);
	if(binding.package == GLA_ID_INVALID && errno != GLA_SUCCESS)
		return NULL;
	binding.name = gla_store_string_try(self->store, path->entity);
	if(binding.name == NULL)
		return NULL;
	if(path->extension != NULL && *path->extension != 0) {
		binding.extension = gla_store_string_get(self->store, path->extension);
		if(binding.extension == NULL)
			return NULL;
	}
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

int gla_entityreg_path_for(
		gla_entityreg_t *self,
		gla_path_t *path,
		const void *data,
		apr_pool_t *pool)
{
	const entry_t *entry = data - offsetof(entry_t, data);
	int depth = 0;
	gla_id_t cur;
	const char *name;

	cur = entry->binding.package;
	while(cur != GLA_ID_ROOT) {
		depth++;
		cur = gla_store_path_parent(self->store, cur);
		if(cur == GLA_ID_INVALID)
			return GLA_INTERNAL;
	}

	gla_path_root(path);
	path->extension = entry->binding.extension;
	path->entity = entry->binding.name;
	path->size = depth;
	cur = entry->binding.package;
	while(cur != GLA_ID_ROOT) {
		name = gla_store_path_name(self->store, cur);
		if(name == NULL)
			return GLA_INTERNAL;
		path->package[--depth] = name;
		cur = gla_store_path_parent(self->store, cur);
		if(cur == GLA_ID_INVALID)
			return GLA_INTERNAL;
	}
	return GLA_SUCCESS;
}

int gla_entityreg_find_first(
		gla_entityreg_t *self,
		gla_id_t package_id,
		gla_entityreg_it_t *it)
{
	entry_t *entry;

	it->lower = btree_find_lower_group(self->entries, &package_id, cmp_entries_package, &it->cur);
	it->upper = btree_find_upper_group(self->entries, &package_id, cmp_entries_package, NULL);

	if(it->lower == it->upper)
		return GLA_NOTFOUND;
	entry = it->cur.element;
	if(entry->binding.package != package_id)
		return GLA_NOTFOUND;
	it->package = package_id;
	it->name = entry->binding.name;
	it->extension = entry->binding.extension;
	it->data = entry->data;
	return GLA_SUCCESS;
}

int gla_entityreg_find_last(
		gla_entityreg_t *self,
		gla_id_t package_id,
		gla_entityreg_it_t *it)
{
	entry_t *entry;

	it->lower = btree_find_lower_group(self->entries, &package_id, cmp_entries_package, NULL);
	it->upper = btree_find_upper_group(self->entries, &package_id, cmp_entries_package, &it->cur);
	if(it->lower == it->upper)
		return GLA_NOTFOUND;
	btree_iterate_prev(&it->cur);
	entry = it->cur.element;
	if(entry->binding.package != package_id)
		return GLA_NOTFOUND;
	it->package = package_id;
	it->name = entry->binding.name;
	it->extension = entry->binding.extension;
	it->data = entry->data;
	return GLA_SUCCESS;
}

int gla_entityreg_iterate_next(
		gla_entityreg_it_t *it)
{
	entry_t *entry;

	btree_iterate_next(&it->cur);
	if(it->cur.index == it->upper)
		return GLA_END;
	entry = it->cur.element;
	if(entry->binding.package != it->package)
		return GLA_END;

	it->name = entry->binding.name;
	it->extension = entry->binding.extension;
	it->data = entry->data;
	return GLA_SUCCESS;
}

int gla_entityreg_iterate_prev(
		gla_entityreg_it_t *it)
{
	entry_t *entry;

	if(it->cur.index == it->lower)
		return GLA_END;
	btree_iterate_prev(&it->cur);
	entry = it->cur.element;
	if(entry->binding.package != it->package)
		return GLA_END;

	it->name = entry->binding.name;
	it->extension = entry->binding.extension;
	it->data = entry->data;
	return GLA_SUCCESS;
}

int gla_entityreg_covered(
		gla_entityreg_t *self,
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

gla_id_t gla_entityreg_covered_children(
		gla_entityreg_t *self,
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

gla_id_t gla_entityreg_covered_next(
		gla_entityreg_t *self,
		gla_id_t package_id)
{
	gla_id_t cur = package_id;

	cur = gla_store_path_next_sibling(self->store, cur);
	while(cur != GLA_ID_INVALID && !btree_contains(self->covered, &cur))
		cur = gla_store_path_next_sibling(self->store, cur); /* also sets errno whilch will be used for this function if any error occured */
	return cur;
}

gla_store_t *gla_entityreg_store(
		gla_entityreg_t *self)
{
	return self->store;
}

apr_pool_t *gla_entityreg_pool(
		gla_entityreg_t *self)
{
	return self->pool;
}

#ifdef TESTING
#include "test/entityreg.h"
#endif

