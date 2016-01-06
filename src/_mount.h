#pragma once

#include "common.h"

struct gla_meta_mount {
#ifdef DEBUG
	void (*dump)(gla_mount_t *self);
#endif
	uint32_t features; /* flags using GLA_MOUNT_FEATURES mask */
	int (*rename)(gla_mount_t *self, const gla_path_t *path, const char *renamed, apr_pool_t *pool);
	gla_mountit_t *(*entities)(gla_mount_t *self, const gla_path_t *path, apr_pool_t *pool);
	gla_mountit_t *(*packages)(gla_mount_t *self, const gla_path_t *path, apr_pool_t *pool);
	int (*touch)(gla_mount_t *self, const gla_path_t *path, bool mkpackage, apr_pool_t *pool);
	gla_io_t *(*open)(gla_mount_t *self, const gla_path_t *path, int mode, apr_pool_t *pool);
	int (*push)(gla_mount_t *self, gla_rt_t *rt, const gla_path_t *path, apr_pool_t *pool);
	int (*info)(gla_mount_t *self, gla_pathinfo_t *info, const gla_path_t *path, apr_pool_t *pool); /* return: GLA_NOTFOUND, GLA_INVALID_PATH_TYPE (package/entity confusion), GLA_SUCCESS */
	const char *(*tofilepath)(gla_mount_t *self, const gla_path_t *path, bool dirname, apr_pool_t *pool);
};

struct gla_mount {
	const gla_meta_mount_t *meta;
	int depth; /* mount depth (TODO maybe replace by gla_path_t?) */
	uint32_t flags; /* mount flags */
};

struct gla_mountit {
	int status;

	int (*next)(gla_mountit_t *self, const char **name, const char **extension);
	void (*close)(gla_mountit_t *self);
};

#ifndef GLA_MOUNT_NOPROTO
#ifdef DEBUG
static void mnt_dump(
		gla_mount_t *self);
#endif

static gla_mountit_t *mnt_entities(
		gla_mount_t *self,
		const gla_path_t *path, /* path is ensured to denote a package */
		apr_pool_t *psession);

static gla_mountit_t *mnt_packages(
		gla_mount_t *self,
		const gla_path_t *path, /* path is ensured to denote a package */
		apr_pool_t *psession);

static int mnt_touch(
		gla_mount_t *self,
		const gla_path_t *path, /* path can either denote an entity or a package */
		bool mkpackage,
		apr_pool_t *ptmp);

static int mnt_rename(
		gla_mount_t *self_,
		const gla_path_t *path,
		const char *renamed,
		apr_pool_t *pool);

static gla_io_t *mnt_open(
		gla_mount_t *self,
	const gla_path_t *path, /* path is ensured to denote an entity */
		int mode, /* mode is ensured to be valid */
		apr_pool_t *psession);

static int mnt_push(
		gla_mount_t *self,
		gla_rt_t *rt,
		const gla_path_t *path, /* path is ensured to denote an entity */
		apr_pool_t *ptmp);

static int mnt_info(
		gla_mount_t *self,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *psession);

static const char *mnt_tofilepath(
		gla_mount_t *self,
		const gla_path_t *path,
		bool dirname,
		apr_pool_t *psession);

/*static int mntit_next(
		gla_mountit_t *self,
		const char **name,
		const char **extension);

static void mntit_close(
		gla_mountit_t *self);*/
#endif

