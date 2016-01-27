#include <assert.h>

#include "common.h"

#define GLA_MOUNT_NOPROTO
#include "_mount.h"

uint32_t gla_mount_features(
		gla_mount_t *self)
{
	return self->meta->features;
}

int gla_mount_depth(
		gla_mount_t *self)
{
	return self->depth;
}

uint32_t gla_mount_flags(
		gla_mount_t *self)
{
	return self->flags;
}

int gla_mount_rename(
		gla_mount_t *self,
		const gla_path_t *path,
		const char *renamed,
		apr_pool_t *ptemp)
{
	if(self->meta->rename == NULL)
		return GLA_NOTSUPPORTED;
	else
		return self->meta->rename(self, path, renamed, ptemp);
}

gla_mountit_t *gla_mount_entities(
		gla_mount_t *self,
		const gla_path_t *path,
		apr_pool_t *psession)
{
	if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	else if(self->meta->entities == NULL) {
		errno = 0;
		return NULL;
	}
	else
		return self->meta->entities(self, path, psession);
}

gla_mountit_t *gla_mount_packages(
		gla_mount_t *self,
		const gla_path_t *path,
		apr_pool_t *psession)
{
	if(gla_path_type(path) != GLA_PATH_PACKAGE) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	else if(self->meta->packages == NULL) {
		errno = 0;
		return NULL;
	}
	else
		return self->meta->packages(self, path, psession);
}

int gla_mount_touch(
		gla_mount_t *self,
		const gla_path_t *path,
		bool mkpackage,
		apr_pool_t *petmp)
{
	if(self->meta->touch == NULL)
		return GLA_NOTSUPPORTED;
	return self->meta->touch(self, path, mkpackage, petmp);
}

int gla_mount_erase(
		gla_mount_t *self,
		const gla_path_t *path,
		apr_pool_t *petmp)
{
	if(gla_path_type(path) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	else if(self->meta->erase == NULL)
		return GLA_NOTSUPPORTED;
	return self->meta->erase(self, path, petmp);
}

gla_io_t *gla_mount_open(
		gla_mount_t *self,
		const gla_path_t *path,
		int mode,
		apr_pool_t *psession)
{
	if(gla_path_type(path) != GLA_PATH_ENTITY) {
		errno = GLA_INVALID_PATH_TYPE;
		return NULL;
	}
	assert((mode & GLA_MODE_READ) != 0 || (mode & GLA_MODE_WRITE) != 0);
	assert((mode & GLA_MODE_APPEND) == 0 || (mode & GLA_MODE_WRITE) != 0);
	return self->meta->open(self, path, mode, psession);
}

int gla_mount_push(
		gla_mount_t *self,
		gla_rt_t *rt,
		const gla_path_t *path,
		apr_pool_t *petmp)
{
	if(self->meta->push == NULL)
		return GLA_NOTSUPPORTED;
	if(gla_path_type(path) != GLA_PATH_ENTITY)
		return GLA_INVALID_PATH_TYPE;
	return self->meta->push(self, rt, path, petmp);
}

int gla_mount_info(
		gla_mount_t *self,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *psession)
{
	return self->meta->info(self, info, path, psession);
}

const char *gla_mount_tofilepath(
		gla_mount_t *self,
		const gla_path_t *path,
		bool dirname,
		apr_pool_t *psession)
{
	if(self->meta->tofilepath == NULL) {
		errno = GLA_NOTSUPPORTED;
		return NULL;
	}
	return self->meta->tofilepath(self, path, dirname, psession);
}

#ifdef DEBUG
void gla_mount_dump(
		gla_mount_t *self)
{
	self->meta->dump(self);
}
#endif

int gla_mountit_next(
		gla_mountit_t *self,
		const char **name,
		const char **extension)
{
	int ret;

	if(self->status != GLA_SUCCESS)
		return self->status;
	ret = self->next(self, name, extension);
	assert(extension == NULL || *extension != NULL); /* NEVER return NULL as an extension */
	self->status = ret;
	return ret;
}

void gla_mountit_close(
		gla_mountit_t *self)
{
	if(self->close != NULL && self->status != GLA_CLOSED)
		self->close(self);
	self->status = GLA_CLOSED;
}

