#include <assert.h>
#include <stdlib.h>

#include "../rt.h"
#include "../_io.h"

#include "io.h"
#include "buffer.h"

#define DEFAULT_CLUSTER_SIZE 65536

typedef struct cluster cluster_t;

struct cluster {
	cluster_t *next;
	int64_t offset; /* offset of first byte of 'data' */
	char *data;
};

typedef struct {
	gla_io_t super;
	int cluster_size;
	cluster_t *head;
	cluster_t *tail;
	int tailfill; /* fill size of the last cluster, i.e. the cluster for which 'next' is NULL */

	cluster_t *wcluster;
	cluster_t *rcluster;
	int woff; /* index into wcluster->data */
	int roff; /* index into rcluster->data */
} io_t;

static gla_meta_io_t io_meta = {
	.read = io_read,
	.write = io_write,
	.rseek = io_rseek,
	.wseek = io_wseek,
	.rtell = io_rtell,
	.wtell = io_wtell,
	.size = io_size,
	.close = io_close
};

static cluster_t *newcluster(
		int size)
{
	char *alloc = malloc(sizeof(cluster_t) + size);
	if(alloc == NULL)
		return NULL;
	cluster_t *cluster = (cluster_t*)alloc;
	memset(cluster, 0, sizeof(cluster_t));
	cluster->data = alloc + sizeof(cluster_t);
	return cluster;
}

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int size)
{
	io_t *self = (io_t*)self_;
	char *di = buffer;
	int total = 0;
	int cur;
	int cluster_size;

	if(self->rcluster == NULL || (self->rcluster->next == NULL && self->roff == self->tailfill))
		return 0;

	while(size > 0) {
		if(self->rcluster->next == NULL)
			cluster_size = self->tailfill;
		else
			cluster_size = self->cluster_size;
		cur = MIN(size, cluster_size - self->roff);
		memcpy(di, self->rcluster->data + self->roff, cur);
		size -= cur;
		di += cur;
		total += cur;
		self->roff += cur;
		if(self->roff == cluster_size) {
			if(self->rcluster->next == NULL) {
				self->super.rstatus = GLA_END;
				return total;
			}
			self->rcluster = self->rcluster->next;
			self->roff = 0;
		}
	}
	return total;
}

static int io_write(
		gla_io_t *self_,
		const void *buffer,
		int size)
{
	const char *si = buffer;
	int total = 0;
	int cur;
	io_t *self = (io_t*)self_;

	if(size == 0)
		return 0;
	else if(self->head == NULL) {
		self->head = newcluster(self->cluster_size);
		if(self->head == NULL) {
			self->super.wstatus = GLA_ALLOC;
			return 0;
		}
		self->tail = self->head;
		self->wcluster = self->head;
		self->rcluster = self->head;
	}
	while(size > 0) {
		cur = MIN(size, self->cluster_size - self->woff);
		memcpy(self->wcluster->data + self->woff, si, cur);
		si += cur;
		total += cur;
		size -= cur;
		self->woff += cur;
		if(size > 0) {
			if(self->wcluster->next == NULL) {
				self->wcluster->next = newcluster(self->cluster_size);
				if(self->wcluster->next == NULL) {
					self->super.wstatus = GLA_ALLOC;
					if(total > 0)
						self->super.rstatus = GLA_SUCCESS;
					return total;
				}
				self->tail = self->wcluster->next;
				self->wcluster->next->offset = self->wcluster->offset + self->cluster_size;
			}
			self->wcluster = self->wcluster->next;
			self->woff = 0;
		}
	}
	if(self->wcluster->next == NULL && self->woff > self->tailfill)
		self->tailfill = self->woff;

	self->super.rstatus = GLA_SUCCESS;
	return total;
}

static int io_rseek(
		gla_io_t *self_,
		int64_t off)
{
	int64_t clidx;
	io_t *self = (io_t*)self_;

	assert(off >= 0);
	if(self->head == NULL) {
		assert(off == 0);
		return GLA_SUCCESS;
	}
	assert(off <= self->tail->offset + self->tailfill);
	self->rcluster = self->head;
	for(clidx = off / self->cluster_size; clidx > 0; clidx--)
		self->rcluster = self->rcluster->next;
	self->roff = off % self->cluster_size;
	if(self->rcluster == self->tail && self->roff == self->tailfill)
		self->super.rstatus = GLA_END;
	else
		self->super.rstatus = GLA_SUCCESS;
	return GLA_SUCCESS;
}

static int io_wseek(
		gla_io_t *self_,
		int64_t off)
{
	int64_t clidx;
	io_t *self = (io_t*)self_;

	assert(off >= 0);
	if(self->head == NULL) {
		assert(off == 0);
		return GLA_SUCCESS;
	}
	assert(off <= self->tail->offset + self->tailfill);
	self->wcluster = self->head;
	for(clidx = off / self->cluster_size; clidx > 0; clidx--)
		self->wcluster = self->wcluster->next;
	self->woff = off % self->cluster_size;
	return GLA_SUCCESS;
}

static int64_t io_rtell(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;

	if(self->head == NULL)
		return 0;
	else
		return self->rcluster->offset + self->roff;
}

static int64_t io_wtell(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;

	if(self->head == NULL)
		return 0;
	else
		return self->wcluster->offset + self->woff;
}

static int64_t io_size(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	if(self->tail == NULL)
		return 0;
	else
		return self->tail->offset + self->tailfill;
}

static void io_close(
		gla_io_t *self_)
{
	io_t *self = (io_t*)self_;
	cluster_t *cur = self->head;
	cluster_t *prev;

	while(cur != NULL) {
		prev = cur;
		cur = cur->next;
		free(prev);
	}
}

static SQInteger fn_ctor(
		HSQUIRRELVM vm)
{
	SQInteger cluster_size = DEFAULT_CLUSTER_SIZE;
	apr_pool_t *pool;
	io_t *io;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) > 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(sq_gettop(vm) >= 2 && SQ_FAILED(sq_getinteger(vm, 2, &cluster_size)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	if(apr_pool_create_unmanaged(&pool) != APR_SUCCESS)
		return gla_rt_vmthrow(rt, "Error creating io pool");
	io = apr_pcalloc(pool, sizeof(io_t));
	if(io == NULL)
		return gla_rt_vmthrow(rt, "Error allocating io struct");
	io->super.meta = &io_meta;
	io->super.mode = GLA_MODE_READ | GLA_MODE_WRITE;
	io->super.wstatus = GLA_SUCCESS;
	io->super.rstatus = GLA_END;
	io->cluster_size = cluster_size;

	if(gla_mod_io_io_init(rt, 1, &io->super, 0, 0, pool) != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error initializing IO");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_tostring(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	io_t *io;
	int64_t size = 0;
	cluster_t *cur;
	char *string;
	char *di;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) > 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	io = up;
	if(io->tail == NULL)
		sq_pushstring(vm, "", -1);
	else {
		size = io->tail->offset + io->tailfill;
		string = apr_palloc(rt->mpstack, size);
		if(string == NULL)
			return gla_rt_vmthrow(rt, "Error allocating temporary string");

		di = string;
		for(cur = io->head; cur->next != NULL; cur = cur->next) {
			memcpy(di, cur->data, io->cluster_size);
			di += io->cluster_size;
		}
		memcpy(di, cur->data, io->tailfill);
		sq_pushstring(vm, string, size);
	}
	return gla_rt_vmsuccess(rt, true);
}

SQInteger gla_mod_io_buffer_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "constructor", -1);
	sq_newclosure(vm, fn_ctor, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "_tostring", -1);
	sq_newclosure(vm, fn_tostring, 0);
	sq_newslot(vm, 2, false);

	return gla_rt_vmsuccess(rt, true);
}

