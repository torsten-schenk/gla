#pragma once

#include "common.h"

struct gla_meta_io {
	int (*read)(gla_io_t *self, void *buffer, int bytes); /* NULL: opened in write mode; default: ALWAYS read fully (TODO add an option which allows partial reading in case the operation would block), except when EOF is encountered. Always returns number of bytes read, never an error value. Call status() to check for error. */
	int (*write)(gla_io_t *self, const void *buffer, int bytes); /* NULL: opened in read mode */
	int (*rseek)(gla_io_t *self, int64_t off);
	int (*wseek)(gla_io_t *self, int64_t off);
	int64_t (*rtell)(gla_io_t *self);
	int64_t (*wtell)(gla_io_t *self);
	int64_t (*size)(gla_io_t *self);
//	int (*truncate)(gla_io_t *self); /* truncate 'self' at current write position; if read position is > end, update read position and set it to end */
	int (*flush)(gla_io_t *self); /* write only */
	void (*close)(gla_io_t *self);
};

struct gla_io {
	const gla_meta_io_t *meta;
	int mode;
	int rstatus;
	int wstatus;
};

#ifndef GLA_IO_NOPROTO
static int io_read(
		gla_io_t *self,
		void *buffer,
		int size);

static int io_write(
		gla_io_t *self,
		const void *buffer,
		int size);

/* whence: SEEK_X, see fseek() */
static int io_rseek(
		gla_io_t *self,
		int64_t off);

static int io_wseek(
		gla_io_t *self,
		int64_t off);

static int64_t io_rtell(
		gla_io_t *self);

static int64_t io_wtell(
		gla_io_t *self);

static int64_t io_size(
		gla_io_t *self);

static int io_truncate(
		gla_io_t *self);

static int io_flush(
		gla_io_t *self);

static void io_close(
		gla_io_t *self);
#endif

