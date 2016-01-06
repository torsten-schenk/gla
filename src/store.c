#include <errno.h>
#include <apr_hash.h>

#include "common.h"

#include "store.h"

typedef struct {
	gla_id_t parent;
	const char *name;
} package_binding_t;

typedef struct package_entry {
	package_binding_t binding;
	gla_id_t id;
	struct package_entry *parent;
	struct package_entry *children;
	struct package_entry *next;
} package_entry_t;

struct gla_store {
	apr_pool_t *pool;
	apr_hash_t *packages_by_binding;
	apr_hash_t *packages_by_id;
	package_entry_t *packages_root;
	apr_hash_t *strings;
};

gla_store_t *gla_store_new(
		apr_pool_t *pool)
{
	gla_store_t *self;
	package_entry_t *entry;

	self = apr_pcalloc(pool, sizeof(gla_store_t));
	if(self == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	self->pool = pool;
	self->packages_by_binding = apr_hash_make(pool);
	if(self->packages_by_binding == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	self->packages_by_id = apr_hash_make(pool);
	if(self->packages_by_id == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	self->strings = apr_hash_make(pool);
	if(self->strings == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	/* create root package entry */
	entry = apr_pcalloc(self->pool, sizeof(package_entry_t));
	if(entry == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	entry->id = GLA_ID_ROOT;
	entry->binding.parent = GLA_ID_INVALID;
	apr_hash_set(self->packages_by_binding, &entry->binding, sizeof(entry->binding), entry);
	apr_hash_set(self->packages_by_id, &entry->id, sizeof(entry->id), entry);
	self->packages_root = entry;

	return self;
}

gla_id_t gla_store_path_get(
		gla_store_t *self,
		const gla_path_t *path)
{
	int i;
	package_binding_t binding;
	package_entry_t *entry;
	package_entry_t *parent = NULL;

	memset(&binding, 0, sizeof(binding));

	if(gla_path_is_root(path))
		return GLA_ID_ROOT;
	else if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return GLA_ID_INVALID;
	}

	parent = self->packages_root;
	binding.parent = GLA_ID_ROOT;
	for(i = path->shifted; i < path->size; i++) {
		binding.name = gla_store_string_get(self, path->package[i]);
		if(binding.name == NULL) /* errno already set from gla_store_string_get */
			return GLA_ID_INVALID;
		entry = apr_hash_get(self->packages_by_binding, &binding, sizeof(binding));
		if(entry == NULL) {
			entry = apr_pcalloc(self->pool, sizeof(package_entry_t));
			if(entry == NULL) {
				errno = GLA_ALLOC;
				return GLA_ID_INVALID;
			}
			entry->binding = binding;
			entry->parent = parent;
			entry->id = apr_hash_count(self->packages_by_binding);
			entry->next = parent->children;
			parent->children = entry;
			apr_hash_set(self->packages_by_binding, &entry->binding, sizeof(entry->binding), entry);
			apr_hash_set(self->packages_by_id, &entry->id, sizeof(entry->id), entry);
		}
		binding.parent = entry->id;
		parent = entry;
	}
	return binding.parent;
}

gla_id_t gla_store_path_try(
		gla_store_t *self,
		const gla_path_t *path)
{
	int i;
	package_binding_t binding;
	package_entry_t *entry;

	memset(&binding, 0, sizeof(binding));

	if(gla_path_is_root(path))
		return GLA_ID_ROOT;
	else if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return GLA_ID_INVALID;
	}

	binding.parent = GLA_ID_ROOT;
	for(i = path->shifted; i < path->size; i++) {
		binding.name = gla_store_string_try(self, path->package[i]);
		if(binding.name == NULL)
			break;
		entry = apr_hash_get(self->packages_by_binding, &binding, sizeof(binding));
		if(entry == NULL)
			break;
		binding.parent = entry->id;
	}
	if(i == path->size)
		return binding.parent;
	else {
		errno = GLA_NOTFOUND;
		return GLA_ID_INVALID;
	}
}

gla_id_t gla_store_path_deepest(
		gla_store_t *self,
		const gla_path_t *path,
		int *depth)
{
	int i;
	package_binding_t binding;
	package_entry_t *entry;

	memset(&binding, 0, sizeof(binding));

	if(gla_path_is_root(path)) {
		*depth = 0;
		return GLA_ID_ROOT;
	}
	else if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return GLA_ID_INVALID;
	}

	binding.parent = GLA_ID_ROOT;
	for(i = path->shifted; i < path->size; i++) {
		binding.name = gla_store_string_try(self, path->package[i]);
		if(binding.name == NULL)
			break;
		entry = apr_hash_get(self->packages_by_binding, &binding, sizeof(binding));
		if(entry == NULL)
			break;
		binding.parent = entry->id;
	}
	if(depth != NULL)
		*depth = i;
	return binding.parent;
}

gla_id_t gla_store_path_parent(
		gla_store_t *self,
		gla_id_t package_id)
{
	if(package_id == GLA_ID_ROOT) {
		errno = GLA_END;
		return GLA_ID_INVALID;
	}
	else {
		package_entry_t *entry;

		entry = apr_hash_get(self->packages_by_id, &package_id, sizeof(package_id));
		if(entry == NULL) {
			errno = GLA_NOTFOUND;
			return GLA_ID_INVALID;
		}
		return entry->binding.parent;
	}
}

gla_id_t gla_store_path_first_child(
		gla_store_t *self,
		gla_id_t package_id)
{
	package_entry_t *entry;

	entry = apr_hash_get(self->packages_by_id, &package_id, sizeof(package_id));
	if(entry == NULL) {
		errno = GLA_NOTFOUND;
		return GLA_ID_INVALID;
	}
	else if(entry->children == NULL) {
		errno = GLA_END;
		return GLA_ID_INVALID;
	}
	else
		return entry->children->id;
}

/* TODO EFFICIENCY maybe redefine gla_id_t as pointer integer so that
 * it stores a pointer to the corresponding package_entry_t structure */
gla_id_t gla_store_path_next_sibling(
		gla_store_t *self,
		gla_id_t package_id)
{
	package_entry_t *entry;

	entry = apr_hash_get(self->packages_by_id, &package_id, sizeof(package_id));
	if(entry == NULL) {
		errno = GLA_NOTFOUND;
		return GLA_ID_INVALID;
	}
	else if(entry->next == NULL) {
		errno = GLA_END;
		return GLA_ID_INVALID;
	}
	else
		return entry->next->id;
}

const char *gla_store_path_name(
		gla_store_t *self,
		gla_id_t package_id)
{
	package_entry_t *entry;

	entry = apr_hash_get(self->packages_by_id, &package_id, sizeof(package_id));
	if(entry == NULL)
		return NULL;
	return entry->binding.name;
}

const char *gla_store_string_get(
		gla_store_t *self,
		const char *string)
{
	char *value;

	if(string == NULL) {
		errno = GLA_SUCCESS;
		return NULL;
	}

	value = apr_hash_get(self->strings, string, APR_HASH_KEY_STRING);
	if(value != NULL)
		return value;

	value = apr_palloc(self->pool, strlen(string) + 1);
	if(value == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	strcpy(value, string);
	apr_hash_set(self->strings, value, APR_HASH_KEY_STRING, value);
	return value;
}

const char *gla_store_string_try(
		gla_store_t *self,
		const char *string)
{
	const char *internal;

	if(string == NULL) {
		errno = GLA_SUCCESS;
		return NULL;
	}
	internal = apr_hash_get(self->strings, string, APR_HASH_KEY_STRING);
	if(internal == NULL) {
		errno = GLA_NOTFOUND;
		return NULL;
	}
	return internal;
}

void gla_store_gc(
		gla_store_t *self)
{}

#ifdef TESTING
#include "test/store.h"
#endif

