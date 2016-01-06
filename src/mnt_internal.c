#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "common.h"

#include "_io.h"
#include "_mount.h"
#include "store.h"
#include "entityreg.h"
#include "mnt_internal.h"

enum {
	TYPE_OBJECT, TYPE_READ_BUFFER
};

typedef struct {
	gla_mount_t super;

	apr_pool_t *pool;
	gla_store_t *store;
	gla_entityreg_t *registry;
} mount_t;

typedef struct {
	int type;
	union {
		struct {
			int (*push)(gla_rt_t *rt, void *user, apr_pool_t *pperm, apr_pool_t *ptemp);
			void *user;
		} object;
		struct {
			const void *data;
			int size;
		} buffer;
	} u;
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

typedef struct {
	gla_io_t super;
	const char *data;
	int offset; /* negative: error code (i.e. GLA_CLOSED) */
	int size;
} io_t;

static gla_meta_mount_t mnt_meta = {
#ifdef DEBUG
	.dump = dump,
#endif
	.entities = mnt_entities,
	.packages = mnt_packages,
	.info = mnt_info,
	.open = mnt_open,
	.push = mnt_push
};

static gla_meta_io_t io_meta = {
	.read = io_read
};

#ifdef DEBUG
static void mnt_dump(
		gla_mount_t *self_)
{
	printf("internal");
}
#endif

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

	id = gla_store_path_try(self->store, path);
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

	id = gla_store_path_try(self->store, path);
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

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int bytes)
{
	io_t *self = (io_t*)self_;

	if(self->offset < 0)
		return self->offset;
	bytes = MIN(bytes, self->size - self->offset);
	memcpy(buffer, self->data + self->offset, bytes);
	self->offset += bytes;

	if(self->offset == self->size)
		self->super.rstatus = GLA_END;
	return bytes;
}

/*static int io_seek(
		gla_io_t *self)
{
	return GLA_NOTSUPPORTED;
}*/

/*static int io_status(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;

	if(self->offset < 0)
		return self->offset;
	else if(self->offset == self->size)
		return GLA_END;
	else
		return GLA_SUCCESS;
}*/

static gla_io_t *mnt_open(
		gla_mount_t *self_,
		const gla_path_t *path,
		int mode,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;
	io_t *io;

	if(mode != GLA_MODE_READ) {
		errno = GLA_NOTSUPPORTED;
		return NULL;
	}

	entry = gla_entityreg_try(self->registry, path);
	if(entry == NULL)
		return NULL;
	else if(entry->type != TYPE_READ_BUFFER) {
		errno = GLA_INVALID_ACCESS;
		return NULL;
	}

	io = apr_pcalloc(pool, sizeof(*io));
	if(io == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	io->super.rstatus = GLA_SUCCESS;
	io->super.wstatus = GLA_CLOSED;
	io->super.mode = mode;
	io->super.meta = &io_meta;

	io->data = entry->u.buffer.data;
	io->size = entry->u.buffer.size;

	return &io->super;
}

static int mnt_push(
		gla_mount_t *self_,
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;

	entry = gla_entityreg_try(self->registry, path);
	if(entry == NULL)
		return errno;
	else if(entry->type != TYPE_OBJECT)
		return GLA_INVALID_ACCESS;
	return entry->u.object.push(rt, entry->u.object.user, self->pool, pool);
}

int mnt_info(
		gla_mount_t *self_,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *pool)
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
		
		if(info != NULL) {
			if(entry->type == TYPE_OBJECT)
				info->access = GLA_ACCESS_PUSH;
			else if(entry->type == TYPE_READ_BUFFER)
				info->access = GLA_ACCESS_READ;
		}
		return GLA_SUCCESS;
	}
}

gla_mount_t *gla_mount_internal_new(
		gla_store_t *store,
		apr_pool_t *pool)
{
	mount_t *mount;

	mount = apr_pcalloc(pool, sizeof(*mount));
	if(mount == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	mount->registry = gla_entityreg_new(store, sizeof(entry_t), pool);
	if(mount->registry == NULL)
		return NULL;

	mount->super.depth = -1;
	mount->super.meta = &mnt_meta;
	mount->store = store;
	mount->pool = pool;

	return &mount->super;
}

int gla_mount_internal_add_object(
		gla_mount_t *self_,
		const gla_path_t *path,
		int (*push)(gla_rt_t *rt, void *user, apr_pool_t *pperm, apr_pool_t *ptemp),
		void *user)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;

	if(self->super.meta != &mnt_meta)
		return GLA_NOTSUPPORTED;
	if(gla_path_type(path) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	entry = gla_entityreg_get(self->registry, path);
	if(errno != GLA_NOTFOUND)
		return GLA_ALREADY;
	entry->type = TYPE_OBJECT;
	entry->u.object.user = user;
	entry->u.object.push = push;

	return GLA_SUCCESS;
}

int gla_mount_internal_add_read_buffer(
		gla_mount_t *self_,
		const gla_path_t *path,
		const void *buffer,
		int size)
{
	mount_t *self = (mount_t*)self_;
	entry_t *entry;

	if(self->super.meta != &mnt_meta)
		return GLA_NOTSUPPORTED;
	if(gla_path_type(path) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	entry = gla_entityreg_get(self->registry, path);
	if(errno != GLA_NOTFOUND)
		return GLA_ALREADY;
	entry->type = TYPE_READ_BUFFER;
	entry->u.buffer.data = buffer;
	entry->u.buffer.size = size;

	return GLA_SUCCESS;
}

#ifdef TESTING
#include "test/mnt_internal.h"
#endif

