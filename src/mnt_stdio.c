#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "common.h"

#include "_io.h"
#include "_mount.h"
#include "store.h"
#include "entityreg.h"
#include "mnt_stdio.h"

enum {
	STREAM_STDIN,
	STREAM_STDOUT,
	STREAM_STDERR,
	N_STREAMS = 3
};

static const struct {
	const char *name;
	int mode;
} stream_info[N_STREAMS] = {
	{ .name = "stdin", .mode = GLA_MODE_READ },
	{ .name = "stdout", .mode = GLA_MODE_WRITE },
	{ .name = "stderr", .mode = GLA_MODE_WRITE }
};

typedef struct {
	gla_mount_t super;

} mount_t;

typedef struct {
	gla_mountit_t super;
	int id;
} entity_it_t;

typedef struct {
	gla_io_t super;

	FILE *stream;
} io_t;

static gla_meta_mount_t mnt_meta = {
#ifdef DEBUG
	.dump = dump,
#endif
	.entities = mnt_entities,
	.info = mnt_info,
	.open = mnt_open,
};

static gla_meta_io_t io_meta = {
	.read = io_read,
	.write = io_write
};

static int id_for(
		const gla_path_t *path)
{
	int i;
	if(gla_path_type(path) == GLA_PATH_PACKAGE)
		return GLA_NOTFOUND;
	else if(path->size != 1)
		return GLA_NOTFOUND;
	else if(path->extension == NULL || strcmp(path->extension, GLA_ENTITY_EXT_IO) != 0)
		return GLA_NOTFOUND;

	for(i = 0; i < N_STREAMS; i++)
		if(strcmp(stream_info[i].name, path->entity) == 0)
			return i;
	return GLA_NOTFOUND;
}

#ifdef DEBUG
static void mnt_dump(
		gla_mount_t *self_)
{
	printf("stdio");
}
#endif

static int mntit_entities_next(
		gla_mountit_t *it_,
		const char **name,
		const char **extension)
{
	entity_it_t *it = (entity_it_t*)it_;
	
	it->id++;
	if(it->id == N_STREAMS)
		return GLA_END;
	if(name != NULL)
		*name = stream_info[it->id].name;
	if(extension != NULL)
		*extension = NULL;
	return GLA_SUCCESS;
}

static gla_mountit_t *mnt_entities(
		gla_mount_t *self_,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	entity_it_t *it;
	
	it = apr_pcalloc(pool, sizeof(*it));
	if(it == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	it->id = -1;
	it->super.next = mntit_entities_next;

	return &it->super;
}

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int bytes)
{
	io_t *self = (io_t*)self_;
	int ret;
	int total = 0;
	char *di;

	for(di = buffer; bytes > 0; bytes--, di++) {
		ret = fread(di, 1, 1, self->stream);
		if(ret == 0) {
			self->super.rstatus = GLA_END;
			return total;
		}
		else if(ret < 0) {
			self->super.rstatus = GLA_IO;
			return total;
		}
		total++;
		if(*di == '\n') /* TODO is this the correct method? Note: fread will block after this character */
			return total;
	}
	return total;
}

static int io_write(
		gla_io_t *self_,
		const void *buffer,
		int bytes)
{
	io_t *self = (io_t*)self_;
	int ret;

	ret = fwrite(buffer, 1, bytes, self->stream);
	if(ret < 0)
		return GLA_IO;
	return ret;
}

static gla_io_t *mnt_open(
		gla_mount_t *self_,
		const gla_path_t *path,
		int mode,
		apr_pool_t *pool)
{
	io_t *io;
	FILE *stream;
	int id;

	id = id_for(path);
	if(id < 0) {
		errno = id;
		return NULL;
	}
	switch(id) {
		case STREAM_STDIN:
			stream = stdin;
			break;
		case STREAM_STDOUT:
			stream = stdout;
			break;
		case STREAM_STDERR:
			stream = stderr;
			break;
		default:
			assert(false);
			errno = GLA_INTERNAL;
			return NULL;
	}
	if(mode != stream_info[id].mode) {
		errno = GLA_NOTSUPPORTED;
		return NULL;
	}

	io = apr_pcalloc(pool, sizeof(*io));
	if(io == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	io->super.rstatus = GLA_SUCCESS; /* TODO all IOs: set r/wstatus to GLA_CLOSED corresponding to the open mode (i.e. all read-only have wstatus set to closed) */
	io->super.wstatus = GLA_SUCCESS;
	io->super.mode = mode;
	io->super.meta = &io_meta;
	io->stream = stream;

	return &io->super;
}

int mnt_info(
		gla_mount_t *self_,
		gla_pathinfo_t *info,
		const gla_path_t *path,
		apr_pool_t *pool)
{
	int id;

	if(info != NULL) {
		memset(info, 0, sizeof(*info));
		info->can_create = false;
	}
	if(path == NULL)
		return GLA_SUCCESS;

	id = id_for(path);
	if(id < 0)
		return id;
	if(info != NULL) {
		if(stream_info[id].mode == GLA_MODE_READ)
			info->access = GLA_ACCESS_READ;
		else if(stream_info[id].mode == GLA_MODE_WRITE)
			info->access = GLA_ACCESS_WRITE;
	}
	return GLA_SUCCESS;
}

gla_mount_t *gla_mount_stdio_new(
		apr_pool_t *pool)
{
	mount_t *mount;

	mount = apr_pcalloc(pool, sizeof(*mount));
	if(mount == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}

	mount->super.depth = -1;
	mount->super.meta = &mnt_meta;

	return &mount->super;
}

