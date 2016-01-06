#include "common.h"

#include "rt.h"
#include "_mount.h"
#include "entityreg.h"
#include "store.h"
#include "mnt_registry.h"

typedef struct {
	gla_mount_t super;

	gla_rt_t *rt;
	gla_entityreg_t *registry;
} mount_t;

typedef struct {
	HSQOBJECT object;
} entry_t;

typedef struct {
	gla_mountit_t super;
	bool first;
	gla_id_t parent;
	gla_entityreg_t *registry;
	gla_entityreg_it_t regit;
} entity_it_t;

typedef struct {
	gla_mountit_t super;
	bool first;
	gla_id_t parent;
	gla_entityreg_t *registry;
	gla_id_t cur;
} package_it_t;

static gla_meta_mount_t mnt_meta = {
#ifdef DEBUG
	.dump = dump,
#endif
	.entities = mnt_entities,
	.packages = mnt_packages,
	.info = mnt_info,
	.push = mnt_push
};

static int mntit_entities_next(
		gla_mountit_t *it_,
		const char **name,
		const char **extension)
{
	entity_it_t *it = (entity_it_t*)it_;
	int ret;

	if(it->first) {
		ret = gla_entityreg_find_first(it->registry, it->parent, &it->regit);
		it->first = false;
	}
	else
		ret = gla_entityreg_iterate_next(&it->regit);
	if(ret != GLA_SUCCESS) {
		if(ret == GLA_NOTFOUND)
			return GLA_END;
		else
			return ret;
	}
	if(name != NULL)
		*name = it->regit.name;
	if(extension != NULL)
		*extension = it->regit.extension;
	return GLA_SUCCESS;
}

static gla_mountit_t *mnt_entities(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	gla_id_t id;
	entity_it_t *it;
	gla_entityreg_it_t regit;

	id = gla_store_path_try(self->rt->store, path);
	if(id == GLA_ID_INVALID) {
		errno = GLA_NOTFOUND;
		return NULL;
	}
	
	it = apr_pcalloc(pool, sizeof(*it));
	if(it == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	it->registry = self->registry;
	it->parent = id;
	it->regit = regit;
	it->first = true;
	it->super.next = mntit_entities_next;

	return &it->super;
}

static int mntit_packages_next(
		gla_mountit_t *it_,
		const char **name,
		const char **extension) /* unused */
{
	package_it_t *it = (package_it_t*)it_;

	if(it->first) {
		it->cur = gla_entityreg_covered_children(it->registry, it->parent);
		it->first = false;
	}
	else
		it->cur = gla_entityreg_covered_next(it->registry, it->cur);
	if(it->cur == GLA_ID_INVALID) {
		if(errno == GLA_NOTFOUND)
			return GLA_END;
		else
			return errno;
	}
	if(name != NULL)
		*name = gla_store_path_name(gla_entityreg_store(it->registry), it->cur);
	if(extension != NULL)
		*extension = NULL;
	return GLA_SUCCESS;
}

static gla_mountit_t *mnt_packages(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	package_it_t *it;
	gla_id_t id;

	id = gla_store_path_try(self->rt->store, path);
	if(id == GLA_ID_INVALID)
		return NULL;
	it = apr_pcalloc(pool, sizeof(*it));
	if(it == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	it->registry = self->registry;
	it->first = true;
	it->parent = id;
	it->super.next = mntit_packages_next;

	return &it->super;
}

static int mnt_push(
		gla_mount_t *self_,
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *ptmp)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;

	entry = gla_entityreg_try(self->registry, path);
	if(entry == NULL)
		return errno;
	sq_pushobject(self->rt->vm, entry->object);
	return GLA_SUCCESS;
}

static int mnt_info(
		gla_mount_t *self_,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *psession)
{
	mount_t *self = (mount_t*)self_;

	if(info != NULL) {
		memset(info, 0, sizeof(*info));
		info->can_create = false;
	}
	/* TODO implement setting of 'info' argument */
	if(path == NULL)
		return GLA_SUCCESS;

	if(gla_path_type(path) == GLA_PATH_PACKAGE) {
		if(info != NULL)
			info->access = GLA_ACCESS_READ;
		return gla_entityreg_covered(self->registry, path);
	}
	else {
		entry_t *entry;

		entry = gla_entityreg_try(self->registry, path);
		if(entry == NULL)
			return errno;
		
		if(info != NULL)
			info->access = GLA_ACCESS_PUSH;
		return GLA_SUCCESS;
	}
}

gla_mount_t *gla_mount_registry_new(
		gla_rt_t *rt)
{
	mount_t *mount;

	mount = apr_pcalloc(rt->mpool, sizeof(*mount));
	if(mount == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	mount->registry = gla_entityreg_new(rt->store, sizeof(entry_t), rt->mpool);
	if(mount->registry == NULL)
		return NULL;

	mount->super.depth = -1;
	mount->super.meta = &mnt_meta;
	mount->rt = rt;

	return &mount->super;
}

int gla_mount_registry_put(
		gla_mount_t *self_,
		const gla_path_t *path,
		int idx,
		bool allow_override)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;

	if(self->super.meta != &mnt_meta)
		return GLA_NOTSUPPORTED;
	if(gla_path_type(path) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	entry = gla_entityreg_get(self->registry, path);
	if(errno != GLA_NOTFOUND && !allow_override)
		return GLA_ALREADY;
	sq_release(self->rt->vm, &entry->object);
	sq_getstackobj(self->rt->vm, idx, &entry->object);
	sq_addref(self->rt->vm, &entry->object);
	return GLA_SUCCESS;
}

