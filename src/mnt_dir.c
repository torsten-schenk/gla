#include <errno.h>
#include <string.h>
#include <apr_file_info.h>
#include <apr_file_io.h>
#include <stdbool.h>
#include <stdio.h>

#include "common.h"

#include "_mount.h"
#include "_io.h"

#include "regex.h"
#include "mnt_dir.h"

#define GLA_FILENAME_MAX_LEN 255

typedef struct {
	gla_mount_t super;

	char *dir;
	int options;
} mount_t;

typedef struct {
	gla_io_t super;

	apr_file_t *file;
} io_t;

typedef struct {
	gla_mountit_t super;

	bool use_rawnames; /* see GLA_MNT_DIR_FILENAMES */
	apr_dir_t *dir;

	char extension[GLA_FILENAME_MAX_LEN + 1];
	char name[GLA_FILENAME_MAX_LEN + 1];
} entity_iterator_t;

static gla_meta_mount_t mnt_meta = {
#ifdef DEBUG
	.dump = dump,
#endif
	.features = GLA_MOUNT_FILESYSTEM,
	.rename = mnt_rename,
	.entities = mnt_entities,
	.packages = mnt_packages,
	.touch = mnt_touch,
	.erase = mnt_erase,
	.info = mnt_info,
	.tofilepath = mnt_tofilepath,
	.open = mnt_open,
};

static gla_meta_io_t io_meta = {
	.read = io_read,
	.write = io_write,
	.flush = io_flush,
	.close = io_close
};

#ifdef DEBUG
static void dump(
		gla_mount_t *self_)
{
	mount_t *self = (mount_t*)self_;
	printf("directory: %s", self->dir);
}
#endif

static const char *mnt_tofilepath(
		gla_mount_t *self_,
		const gla_path_t *path,
		bool dirname, /* if true and 'path' denotes an entity, return the directory where entity 'path' resides in */
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	int len = 0;
	int i;
	char *di;
	char *filepath;

	len = strlen(self->dir) + 1;
	if(path != NULL) {
		for(i = path->shifted; i < path->size; i++)
			len += strlen(path->package[i]) + 1;
		if(gla_path_type(path) == GLA_PATH_ENTITY) {
			len += strlen(path->entity) + 1;
			if(!dirname && path->extension != NULL)
				len += strlen(path->extension) + 1;
		}
	}
	filepath = apr_palloc(pool, len);
	if(filepath == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	di = filepath;
	strcpy(di, self->dir);
	di += strlen(di);
	if(path != NULL) {
		for(i = path->shifted; i < path->size; i++) {
			strcpy(di, "/");
			di += strlen(di);
			strcpy(di, path->package[i]);
			di += strlen(di);
		}
		if(gla_path_type(path) == GLA_PATH_ENTITY && !dirname) {
			strcpy(di, "/");
			di += strlen(di);
			strcpy(di, path->entity);
			di += strlen(di);
			if(!dirname && path->extension != NULL && path->extension[0] != 0) {
				strcpy(di, ".");
				di += strlen(di);
				strcpy(di, path->extension);
				di += strlen(di);
			}
		}
	}
	return filepath;
}

static void mntit_close(
		gla_mountit_t *self_)
{
	entity_iterator_t *self = (entity_iterator_t*)self_;
	if(self->dir != NULL) {
		apr_dir_close(self->dir);
		self->dir = NULL;
	}
}

static int mntit_entities_next(
		gla_mountit_t *self_,
		const char **name,
		const char **extension)
{
	entity_iterator_t *self = (entity_iterator_t*)self_;
	apr_finfo_t info;
	int ret;

	if(name != NULL)
		*name = self->name;
	if(extension != NULL)
		*extension = self->extension;

	for(;;) {
		ret = apr_dir_read(&info, APR_FINFO_MIN, self->dir);
		if(ret == APR_ENOENT) {
			apr_dir_close(self->dir);
			self->dir = NULL;
			return GLA_END;
		}
		else if(ret != 0) {
			apr_dir_close(self->dir);
			self->dir = NULL;
			return GLA_IO;
		}

		if(info.filetype == APR_REG) { /* TODO link treatment */
			if(strlen(info.name) > GLA_FILENAME_MAX_LEN)
				continue;
			if(self->use_rawnames)
				strcpy(self->name, info.name);
			else {
				ret = gla_regex_filename_to_entity(info.name, -1, self->name, GLA_FILENAME_MAX_LEN, self->extension, GLA_FILENAME_MAX_LEN);
				if(ret < 0) /* error */
					return ret;
				else if(ret == 0) /* no match, use filename as name; no extension */
					continue;
			}
			return 0;
		}
	}
}

static gla_mountit_t *mnt_entities(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	const char *filepath;
	entity_iterator_t *it;
	int ret;

	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return NULL;
	it = apr_pcalloc(pool, sizeof(entity_iterator_t));
	if(it == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	ret = apr_dir_open(&it->dir, filepath, pool);
	if(ret != 0) {
		errno = GLA_NOTFOUND;
		return NULL;
	}

	it->use_rawnames = (self->options & GLA_MNT_DIR_FILENAMES) == GLA_MNT_DIR_FILENAMES;
	it->super.next = mntit_entities_next;
	it->super.close = mntit_close;

	return &it->super;
}

static int mntit_packages_next(
		gla_mountit_t *self_,
		const char **name,
		const char **extension) /* extension: unused here */
{
	entity_iterator_t *self = (entity_iterator_t*)self_;
	apr_finfo_t info;
	int ret;

	if(name != NULL)
		*name = self->name;
	if(extension != NULL)
		*extension = NULL;

	for(;;) {
		ret = apr_dir_read(&info, APR_FINFO_MIN, self->dir);
		if(ret == APR_ENOENT) {
			apr_dir_close(self->dir);
			self->dir = NULL;
			return GLA_END;
		}
		else if(ret != 0) {
			apr_dir_close(self->dir);
			self->dir = NULL;
			return GLA_IO;
		}

		if(info.filetype == APR_DIR) {
			if(strlen(info.name) > GLA_FILENAME_MAX_LEN)
				continue;
			else if(strcmp(info.name, "..") == 0)
				continue;
			else if(strcmp(info.name, ".") == 0)
				continue;
			if(self->use_rawnames)
				strcpy(self->name, info.name);
			else {
				ret = gla_regex_dirname_to_entity(info.name, -1, self->name, GLA_FILENAME_MAX_LEN);
				if(ret < 0) /* error */
					return ret;
				else if(ret == 0) /* no match */
					continue;
			}
			return GLA_SUCCESS;
		}
	}
}

static int mnt_rename(
		gla_mount_t *self_,
		const gla_path_t *path,
		const char *renamed,
		apr_pool_t *pool)
{
	gla_path_t newpath;
	int ret;
	const char *newfp;
	const char *oldfp;

	if(gla_path_is_root(path))
		return GLA_INVALID_PATH;
	newpath = *path;
	if(gla_path_type(path) == GLA_PATH_PACKAGE) {
		if(newpath.size - newpath.shifted == 0) /* this prevents mountpoints to be renamed; TODO how to handle this case? */
			return GLA_INVALID_PATH;
		newpath.package[newpath.size - 1] = renamed;
	}
	else
		newpath.entity = renamed;

	newfp = mnt_tofilepath(self_, &newpath, false, pool);
	if(newfp == NULL)
		return errno;
	oldfp = mnt_tofilepath(self_, path, false, pool);
	if(newfp == NULL)
		return errno;
	ret = apr_file_rename(oldfp, newfp, pool);
	if(ret != APR_SUCCESS)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static gla_mountit_t *mnt_packages(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	mount_t *self = (mount_t*)self_;
	const char *filepath;
	entity_iterator_t *it;
	int ret;

	it = apr_pcalloc(pool, sizeof(entity_iterator_t));
	if(it == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return NULL;
	ret = apr_dir_open(&it->dir, filepath, pool);
	if(ret != 0) {
		errno = GLA_NOTFOUND;
		return NULL;
	}

	it->use_rawnames = (self->options & GLA_MNT_DIR_DIRNAMES) == GLA_MNT_DIR_DIRNAMES;
	it->super.next = mntit_packages_next;
	it->super.close = mntit_close;

	return &it->super;
}

int mnt_info(
		gla_mount_t *self_,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	int ret;
	const char *filepath;
	apr_finfo_t statinfo;

	if(info != NULL) {
		memset(info, 0, sizeof(*info));
		info->can_create = true;
	}
	if(path == NULL)
		return GLA_SUCCESS;

	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return errno;
	ret = apr_stat(&statinfo, filepath, APR_FINFO_MIN | APR_FINFO_MTIME | APR_FINFO_CTIME, pool);
	if(ret == APR_ENOENT)
		return GLA_NOTFOUND;
	else if(ret < 0)
		return GLA_IO;
	if(info != NULL) {
		info->created = statinfo.ctime;
		info->modified = statinfo.mtime;
	}
	if(gla_path_type(path) == GLA_PATH_PACKAGE) {
		if(statinfo.filetype == APR_DIR)
			return GLA_SUCCESS;
		else if(statinfo.filetype == APR_REG)
			return GLA_INVALID_PATH_TYPE;
		else
			return GLA_NOTFOUND;
	}
	else if(gla_path_type(path) == GLA_PATH_ENTITY) {
		if(statinfo.filetype == APR_REG)
			return GLA_SUCCESS;
		else if(statinfo.filetype == APR_DIR)
			return GLA_INVALID_PATH_TYPE;
		else
			return GLA_NOTFOUND;
	}
	else
		return GLA_INTERNAL;
}

int mnt_touch(
		gla_mount_t *self_,
		const gla_path_t *path,
		bool mkpackage,
		apr_pool_t *pool)
{
	const char *filepath;
	int ret;
	apr_finfo_t statinfo;

	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return errno;
	if(gla_path_type(path) == GLA_PATH_PACKAGE) {
		ret = apr_stat(&statinfo, filepath, APR_FINFO_MIN, pool);
		if(ret == APR_ENOENT) {
			ret = apr_dir_make_recursive(filepath, APR_FPROT_OS_DEFAULT, pool);
			if(ret != APR_SUCCESS)
				return GLA_IO;
		}
		else if(statinfo.filetype != APR_DIR)
			return GLA_INVALID_PATH_TYPE;
	}
	else {
		ret = apr_stat(&statinfo, filepath, APR_FINFO_MIN, pool);
		if(ret == APR_ENOENT) {
			apr_file_t *file;
			const char *dirpath = mnt_tofilepath(self_, path, true, pool);
			ret = apr_stat(&statinfo, dirpath, APR_FINFO_MIN, pool);
			if(ret == APR_ENOENT)
				if(mkpackage) {
					ret = apr_dir_make_recursive(dirpath, APR_FPROT_OS_DEFAULT, pool);
					if(ret != APR_SUCCESS)
						return GLA_IO;
				}
				else
					return GLA_NOTFOUND;
			else if(statinfo.filetype != APR_DIR)
				return GLA_INVALID_PATH_TYPE;
			ret = apr_file_open(&file, filepath, APR_FOPEN_WRITE | APR_FOPEN_CREATE, APR_FPROT_OS_DEFAULT, pool);
			if(ret != APR_SUCCESS)
				return GLA_IO;
			apr_file_close(file);
		}
		else if(statinfo.filetype != APR_REG)
			return GLA_INVALID_PATH_TYPE;
	}
	return GLA_SUCCESS;
}

int mnt_erase(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	const char *filepath;
	int ret;

	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return errno;
	ret = apr_file_remove(filepath, pool);
	if(ret != APR_SUCCESS)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static void io_close(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	if(self->file != NULL) {
		apr_file_close(self->file);
		self->file = NULL;
	}
}

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int bytes)
{
	io_t *self = (io_t*)self_;
	apr_size_t br = 0;
	apr_status_t ret;

	ret = apr_file_read_full(self->file, buffer, bytes, &br);
	if(ret == APR_EOF)
		self->super.rstatus = GLA_END;
	else if(ret != APR_SUCCESS)
		self->super.rstatus = GLA_IO;
	return br;
}

static int io_write(
		gla_io_t *self_,
		const void *buffer,
		int bytes)
{
	io_t *self = (io_t*)self_;
	apr_size_t bw = 0;
	apr_status_t ret;

	ret = apr_file_write_full(self->file, buffer, bytes, &bw);
	if(ret == APR_EOF)
		self->super.wstatus = GLA_END;
	else if(ret != APR_SUCCESS)
		self->super.wstatus = GLA_IO;
	return bw;
}

static int io_flush(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	int ret;

	ret = apr_file_flush(self->file);
	if(ret != APR_SUCCESS)
		return GLA_IO;
	return GLA_SUCCESS;
}

static gla_io_t *mnt_open(
		gla_mount_t *self_,
		const gla_path_t *path,
		int mode,
		apr_pool_t *pool)
{
	int ret;
	const char *filepath;
	io_t *io;
	apr_int32_t flags;

	io = apr_pcalloc(pool, sizeof(io_t));
	if(io == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	filepath = mnt_tofilepath(self_, path, false, pool);
	if(filepath == NULL)
		return NULL;
	flags = APR_FOPEN_BINARY | APR_BUFFERED;
	if((mode & GLA_MODE_READ) != 0)
		flags = APR_FOPEN_READ;
	if((mode & GLA_MODE_WRITE) != 0) {
		flags = APR_FOPEN_WRITE | APR_FOPEN_CREATE;
		if((mode & GLA_MODE_APPEND) != 0)
			flags |= APR_FOPEN_APPEND;
		else
			flags |= APR_FOPEN_TRUNCATE;
	}
	ret = apr_file_open(&io->file, filepath, flags, APR_FPROT_OS_DEFAULT, pool);
	if(ret != APR_SUCCESS) {
		errno = GLA_IO;
		return NULL;
	}

	io->super.meta = &io_meta;
	io->super.mode = mode;
	io->super.wstatus = GLA_SUCCESS;
	io->super.rstatus = GLA_SUCCESS;

	return &io->super;
}

gla_mount_t *gla_mount_dir_new(
		const char *dir,
		int options,
		apr_pool_t *pool)
{
	mount_t *mount;

	mount = apr_pcalloc(pool, sizeof(mount_t));
	if(mount == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	mount->dir = apr_pcalloc(pool, strlen(dir) + 1);
	if(mount->dir == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	mount->super.depth = -1;
	mount->super.meta = &mnt_meta;
	mount->options = options;
	strcpy(mount->dir, dir);

	return &mount->super;
}

