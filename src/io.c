#include "common.h"

#include "io/io.h"

#define GLA_IO_NOPROTO
#include "_io.h"

int gla_io_read(
		gla_io_t *self,
		void *buffer,
		int size)
{
	if(self->meta->read == NULL)
		return GLA_NOTSUPPORTED;
	else if(self->rstatus != GLA_SUCCESS)
		return 0;
	return self->meta->read(self, buffer, size);
}

int gla_io_write(
		gla_io_t *self,
		const void *buffer,
		int size)
{
	if(self->meta->write == NULL)
		return GLA_NOTSUPPORTED;
	else if(self->wstatus != GLA_SUCCESS)
		return 0;
	return self->meta->write(self, buffer, size);
}

int gla_io_rseek(
		gla_io_t *self,
		int64_t off,
		int whence)
{
	int64_t size;
	int64_t cur;

	if(self->meta->rseek == NULL || self->meta->rtell == NULL || self->meta->size == NULL)
		return GLA_NOTSUPPORTED;
	else if(self->rstatus != GLA_SUCCESS && self->rstatus != GLA_END)
		return GLA_IO;
	size = self->meta->size(self);
	if(size < 0)
		return size;
	switch(whence) {
		case SEEK_SET:
			break;
		case SEEK_CUR:
			cur = self->meta->rtell(self);
			if(cur < 0)
				return cur;
			off += cur;
			break;
		case SEEK_END:
			off = size - off;
			break;
		default:
			return GLA_INVALID_ARGUMENT;
	}
	if(off < 0)
		return GLA_UNDERFLOW;
	else if(off > size)
		return GLA_OVERFLOW;
	else
		return self->meta->rseek(self, off);
}

int gla_io_wseek(
		gla_io_t *self,
		int64_t off,
		int whence)
{
	int64_t size;
	int64_t cur;

	if(self->meta->wseek == NULL || self->meta->wtell == NULL || self->meta->size == NULL)
		return GLA_NOTSUPPORTED;
	else if(self->wstatus != GLA_SUCCESS && self->wstatus != GLA_END)
		return GLA_IO;
	size = self->meta->size(self);
	if(size < 0)
		return size;
	switch(whence) {
		case SEEK_SET:
			break;
		case SEEK_CUR:
			cur = self->meta->wtell(self);
			if(cur < 0)
				return cur;
			off += cur;
			break;
		case SEEK_END:
			off = size - off;
			break;
		default:
			return GLA_INVALID_ARGUMENT;
	}
	if(off < 0)
		return GLA_UNDERFLOW;
	else if(off > size)
		return GLA_OVERFLOW;
	else
		return self->meta->wseek(self, off);
}

int64_t gla_io_rtell(
		gla_io_t *self)
{
	if(self->meta->rtell == NULL)
		return GLA_NOTSUPPORTED;
	return self->meta->rtell(self);
}

int64_t gla_io_wtell(
		gla_io_t *self)
{
	if(self->meta->wtell == NULL)
		return GLA_NOTSUPPORTED;
	return self->meta->wtell(self);
}

int64_t gla_io_size(
		gla_io_t *self)
{
	if(self->meta->size == NULL)
		return GLA_NOTSUPPORTED;
	return self->meta->size(self);
}

int gla_io_flush(
		gla_io_t *self)
{
	if(self->meta->flush == NULL)
		return GLA_SUCCESS;
	else if((self->mode & GLA_MODE_WRITE) == 0)
		return GLA_IO;
	return self->meta->flush(self);
}

int gla_io_wstatus(
		gla_io_t *self)
{
	return self->wstatus;
}

int gla_io_rstatus(
		gla_io_t *self)
{
	return self->rstatus;
}

gla_io_t *gla_io_get(
		gla_rt_t *rt,
		int idx)
{
	return gla_mod_io_io_get(rt, idx);
}

void gla_io_close(
		gla_io_t *self)
{
	if(self->meta->close != NULL && (self->wstatus != GLA_CLOSED || self->rstatus != GLA_CLOSED))
		self->meta->close(self);
	self->rstatus = GLA_CLOSED;
	self->wstatus = GLA_CLOSED;
}

