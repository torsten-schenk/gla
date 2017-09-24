#include <assert.h>
#include <stdlib.h>

#include "../io/module.h"
#include "../rt.h"
#include "../_io.h"

#include "io.h"
#include "buffer.h"

#define BUFFER_SIZE 1024

typedef struct {
	gla_io_t super;
	FILE *file;
	int64_t size;
} io_t;

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int size)
{
	io_t *self = (io_t*)self_;
	size_t bytes;
	bytes = fread(buffer, 1, size, self->file);
	if(feof(self->file))
		self->super.rstatus = GLA_END;
	else if(ferror(self->file))
		self->super.rstatus = GLA_IO;
	return bytes;
}

static int io_write(
		gla_io_t *self_,
		const void *buffer,
		int size)
{
	io_t *self = (io_t*)self_;
	return fwrite(buffer, 1, size, self->file);
}

static int64_t io_size(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	return self->size;
}

static void io_close(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	fclose(self->file);
	self->file = NULL;
}

static int io_seek(
		gla_io_t *self_,
		int64_t off)
{
	int ret;
	io_t *self = (io_t*)self_;

	ret = fseek(self->file, off, SEEK_SET);
	if(ret)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int64_t io_tell(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	int64_t ret;

	ret = ftell(self->file);
	if(ret < 0)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}


static gla_meta_io_t io_meta = {
	.read = io_read,
	.write = io_write,
	.rseek = io_seek,
	.wseek = io_seek,
	.rtell = io_tell,
	.wtell = io_tell,
	.size = io_size,
	.close = io_close
};

static SQInteger fn_ctor(
		HSQUIRRELVM vm)
{
	const SQChar *filename;
	const SQChar *mode;
	apr_pool_t *pool;
	io_t *io;
	fpos_t pos;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &filename)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	else if(SQ_FAILED(sq_getstring(vm, 3, &mode)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected string");
	if(apr_pool_create_unmanaged(&pool) != APR_SUCCESS)
		return gla_rt_vmthrow(rt, "Error creating io pool");
	io = apr_pcalloc(pool, sizeof(io_t));
	if(io == NULL)
		return gla_rt_vmthrow(rt, "Error allocating io struct");
	io->super.meta = &io_meta;
	if(!strcmp(mode, "r")) {
		io->super.mode = GLA_MODE_READ;
		mode = "rb";
	}
	else if(!strcmp(mode, "w")) {
		io->super.mode = GLA_MODE_WRITE;
		mode = "wb";
	}
	else if(!strcmp(mode, "a")) {
		io->super.mode = GLA_MODE_WRITE;
		mode = "ab";
	}
	else {
		apr_pool_destroy(pool);
		return gla_rt_vmthrow(rt, "given file mode currently not supported");
	}

	io->super.wstatus = GLA_SUCCESS;
	io->super.rstatus = GLA_SUCCESS;
	io->file = fopen(filename, mode);
	if(!io->file) {
		apr_pool_destroy(pool);
		return gla_rt_vmthrow(rt, "Error opening file");
	}
	fgetpos(io->file, &pos);
	fseek(io->file, 0, SEEK_END);
	io->size = ftell(io->file);
	fseek(io->file, 0, SEEK_SET);
	fsetpos(io->file, &pos);

	if(gla_mod_io_io_init(rt, 1, &io->super, 0, 0, pool) != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error initializing IO");

	return gla_rt_vmsuccess(rt, false);
}

SQInteger gla_mod_io_file_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "constructor", -1);
	sq_newclosure(vm, fn_ctor, 0);
	sq_newslot(vm, 2, false);

/*	sq_pushstring(vm, "hexdump", -1);
	sq_newclosure(vm, fn_hexdump, 0);
	sq_newslot(vm, 2, false);*/

/*	sq_pushstring(vm, "_tostring", -1);
	sq_newclosure(vm, fn_tostring, 0);
	sq_newslot(vm, 2, false);*/

	return gla_rt_vmsuccess(rt, true);
}

gla_io_t *gla_mod_io_file_new(
		gla_rt_t *rt)
{
	gla_path_t path;
	int ret;

	gla_path_parse_entity(&path, "gla.io.File", rt->mpstack);
	ret = gla_rt_import(rt, &path, rt->mpstack);
	if(ret != GLA_SUCCESS) {
		errno = ret;
		return NULL;
	}
	sq_pushroottable(rt->vm);
	if(SQ_FAILED(sq_call(rt->vm, 1, true, false))) {
		GLA_LOG_ERROR(rt, "Error creating file instance");
	}
	sq_remove(rt->vm, -2);
	return gla_mod_io_io_get(rt, -1);
}

