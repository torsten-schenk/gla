#include <assert.h>
#include <endian.h>
#include <errno.h>

#include "../log.h"
#include "../rt.h"

#include "_rtdata.h"
#include "hash.h"
#include "io.h"

#define TMP_BUFFER_SIZE 1024

/* TODO another method: use a custom Factory instance for unknown type strings or type could not be inferred */

typedef struct {
	gla_rt_t *rt;
	rtdata_t *rtdata;
	apr_pool_t *pool;
	gla_io_t *io; /* NULL, if closed */
	int endian;

	uint64_t roff;
	uint64_t woff;
} io_t;

enum {
	TYPE_U8,
	TYPE_S8,
	TYPE_U16,
	TYPE_S16,
	TYPE_U32,
	TYPE_S32,
	TYPE_U64,
	TYPE_S64,
	TYPE_F32,
	TYPE_F64,
	TYPE_STRING,
	TYPE_SERIALIZABLE,
	TYPE_IO,
	N_TYPES
};

enum {
	ENDIAN_NATIVE,
	ENDIAN_LITTLE,
	ENDIAN_BIG
};

static int write_u8(io_t *self, int idx, int64_t bytes);
static int write_s8(io_t *self, int idx, int64_t bytes);
static int write_u16(io_t *self, int idx, int64_t bytes);
static int write_s16(io_t *self, int idx, int64_t bytes);
static int write_u32(io_t *self, int idx, int64_t bytes);
static int write_s32(io_t *self, int idx, int64_t bytes);
static int write_u64(io_t *self, int idx, int64_t bytes);
static int write_s64(io_t *self, int idx, int64_t bytes);
static int write_f32(io_t *self, int idx, int64_t bytes);
static int write_f64(io_t *self, int idx, int64_t bytes);
static int write_string(io_t *self, int idx, int64_t bytes);
static int write_serializable(io_t *self, int idx, int64_t bytes);
static int write_io(io_t *self, int idx, int64_t bytes);
static int read_u8(io_t *self, int idx, int64_t bytes);
static int read_s8(io_t *self, int idx, int64_t bytes);
static int read_u16(io_t *self, int idx, int64_t bytes);
static int read_s16(io_t *self, int idx, int64_t bytes);
static int read_u32(io_t *self, int idx, int64_t bytes);
static int read_s32(io_t *self, int idx, int64_t bytes);
static int read_u64(io_t *self, int idx, int64_t bytes);
static int read_s64(io_t *self, int idx, int64_t bytes);
static int read_f32(io_t *self, int idx, int64_t bytes);
static int read_f64(io_t *self, int idx, int64_t bytes);
static int read_string(io_t *self, int idx, int64_t bytes);
static int read_serializable(io_t *self, int idx, int64_t bytes);
static int read_io(io_t *self, int idx, int64_t bytes);

static const struct {
	const char *name;
	int (*write)(io_t *self, int idx, int64_t bytes);
	int (*read)(io_t *self, int idx, int64_t bytes);
} type_info[N_TYPES] = {
	{ "u8", write_u8, read_u8 },
	{ "s8", write_s8, read_s8 },
	{ "u16", write_u16, read_u16 },
	{ "s16", write_s16, read_s16 },
	{ "u32", write_u32, read_u32 },
	{ "s32", write_s32, read_s32 },
	{ "u64", write_u64, read_u64 },
	{ "s64", write_s64, read_s64 },
	{ "f32", write_f32, read_f32 },
	{ "f64", write_f64, read_f64 },
	{ "string", write_string, read_string },
	{ "serializable", write_serializable, read_serializable },
	{ "io", write_io, read_io }
};

static int copy_io(
		io_t *dest,
		io_t *src,
		int64_t bytes)
{
	int wcur;
	int rcur;
	char buffer[TMP_BUFFER_SIZE];

	if(bytes == -1)
		while(gla_io_wstatus(dest->io) == GLA_SUCCESS && gla_io_rstatus(src->io) == GLA_SUCCESS) {
			rcur = gla_io_read(src->io, buffer, TMP_BUFFER_SIZE);
			wcur = gla_io_write(dest->io, buffer, rcur);
			if(wcur != rcur)
				return GLA_IO;
		}
	else if(bytes >= 0) {
		while(gla_io_wstatus(dest->io) == GLA_SUCCESS && gla_io_rstatus(src->io) == GLA_SUCCESS && bytes > 0) {
			rcur = gla_io_read(src->io, buffer, MIN(TMP_BUFFER_SIZE, bytes));
			wcur = gla_io_write(dest->io, buffer, rcur);
			if(wcur != rcur)
				return GLA_IO;
			bytes -= wcur;
		}

	}
	else if(bytes < 0)
		return GLA_INVALID_ARGUMENT;
	if(bytes > 0 && gla_io_rstatus(src->io) == GLA_END && gla_io_wstatus(dest->io) == GLA_SUCCESS)
		return GLA_UNDERFLOW;
	else if(gla_io_wstatus(dest->io) != GLA_SUCCESS)
		return GLA_IO;
	else if(gla_io_rstatus(src->io) != GLA_SUCCESS && gla_io_rstatus(src->io) != GLA_END)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_u8(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	uint8_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_s8(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	int8_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_u16(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	uint16_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole16(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe16(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_s16(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	int16_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
			cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole16(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe16(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_u32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	uint32_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole32(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe32(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_s32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	int32_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole32(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe32(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_u64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	uint64_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole64(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe64(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_s64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQInteger value;
	int64_t cv;

	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getinteger(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	switch(self->endian) {
		case ENDIAN_LITTLE:
			cv = htole64(cv);
			break;
		case ENDIAN_BIG:
			cv = htobe64(cv);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_f32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQFloat value;
	float cv;
	uint32_t data;

	assert(sizeof(cv) == sizeof(data)); /* TODO improve float handling */
	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getfloat(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	memcpy(&data, &cv, sizeof(data));
	switch(self->endian) {
		case ENDIAN_LITTLE:
			data = htole32(data);
			break;
		case ENDIAN_BIG:
			data = htobe32(data);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_f64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQFloat value;
	double cv;
	uint64_t data;

	assert(sizeof(cv) == sizeof(data)); /* TODO improve float handling */
	if(bytes != -1 && bytes != sizeof(cv))
		return GLA_INVALID_ARGUMENT;
	else if(SQ_FAILED(sq_getfloat(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	cv = value;
	memcpy(&data, &cv, sizeof(data));
	switch(self->endian) {
		case ENDIAN_LITTLE:
			data = htole64(data);
			break;
		case ENDIAN_BIG:
			data = htobe64(data);
			break;
	}
	if(gla_io_write(self->io, &cv, sizeof(cv)) != sizeof(cv))
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_string(
		io_t *self,
		int idx,
		int64_t bytes)
{
	const SQChar *value;

	if(SQ_FAILED(sq_getstring(self->rt->vm, idx, &value)))
		return GLA_NOTSUPPORTED;
	if(bytes == -1)
		bytes = strlen(value);
	else if(bytes > strlen(value))
		return GLA_UNDERFLOW;
	else if(bytes < 0)
		return GLA_INVALID_ARGUMENT;
	if(gla_io_write(self->io, value, bytes) != bytes)
		return GLA_IO;
	else
		return GLA_SUCCESS;
}

static int write_serializable(
		io_t *self,
		int idx,
		int64_t bytes)
{
	HSQUIRRELVM vm = self->rt->vm;

	if(bytes != -1)
		return GLA_INVALID_ARGUMENT;
	idx = sq_toabs(vm, idx);
	sq_pushobject(vm, self->rtdata->serialize_string);
	if(SQ_FAILED(sq_get(vm, idx)))
		return GLA_NOTSUPPORTED;
	sq_push(vm, idx);
	sq_push(vm, 1);
	if(SQ_FAILED(sq_call(vm, 2, false, true))) {
		sq_poptop(vm);
		return GLA_VM;
	}
	return GLA_SUCCESS;
}

static int write_io(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQUserPointer up;
	HSQUIRRELVM vm = self->rt->vm;
	bool isio;

	if(sq_gettype(vm, idx) != OT_INSTANCE)
		return GLA_NOTSUPPORTED;
	sq_pushobject(vm, self->rtdata->io_class);
	sq_push(vm, idx);
	isio = sq_instanceof(vm);
	sq_pop(vm, 2);
	if(!isio)
		return GLA_NOTSUPPORTED;
	if(SQ_FAILED(sq_getinstanceup(vm, idx, &up, NULL))) {
		LOG_ERROR("Error getting instance userpointer");
		return GLA_INTERNAL;
	}
	return copy_io(self, up, bytes);
}

static int read_u8(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint8_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_s8(
		io_t *self,
		int idx,
		int64_t bytes)
{
	int8_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_u16(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint16_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le16toh(value);
				break;
			case ENDIAN_BIG:
				value = be16toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_s16(
		io_t *self,
		int idx,
		int64_t bytes)
{
	int16_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le16toh(value);
				break;
			case ENDIAN_BIG:
				value = be16toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_u32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint32_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le32toh(value);
				break;
			case ENDIAN_BIG:
				value = be32toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_s32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	int32_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le32toh(value);
				break;
			case ENDIAN_BIG:
				value = be32toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_u64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint64_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le64toh(value);
				break;
			case ENDIAN_BIG:
				value = be64toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_s64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	int64_t value;
	int ret;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(value))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &value, sizeof(value));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le64toh(value);
				break;
			case ENDIAN_BIG:
				value = be64toh(value);
				break;
		}
		sq_pushinteger(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_f32(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint32_t data;
	float value;
	int ret;

	assert(sizeof(data) == sizeof(value)); /* TODO improved float handling */
	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(data))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &data, sizeof(data));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le32toh(value);
				break;
			case ENDIAN_BIG:
				value = be32toh(value);
				break;
		}
		memcpy(&value, &data, sizeof(value));
		sq_pushfloat(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_f64(
		io_t *self,
		int idx,
		int64_t bytes)
{
	uint64_t data;
	double value;
	int ret;

	assert(sizeof(data) == sizeof(value)); /* TODO improved float handling */
	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes != -1 && bytes != sizeof(data))
		return GLA_INVALID_ARGUMENT;
	ret = gla_io_read(self->io, &data, sizeof(data));
	if(ret == sizeof(value)) {
		switch(self->endian) {
			case ENDIAN_LITTLE:
				value = le64toh(value);
				break;
			case ENDIAN_BIG:
				value = be64toh(value);
				break;
		}
		memcpy(&value, &data, sizeof(value));
		sq_pushfloat(self->rt->vm, value);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_string(
		io_t *self,
		int idx,
		int64_t bytes)
{
	int ret;
	char *string;

	if(idx != 0)
		return GLA_INVALID_ARGUMENT;
	else if(bytes < 0)
		return GLA_INVALID_ARGUMENT;
	string = apr_palloc(self->rt->mpstack, bytes);
	if(string == NULL)
		return GLA_ALLOC;
	ret = gla_io_read(self->io, string, bytes);
	if(ret == bytes) {
		sq_pushstring(self->rt->vm, string, bytes);
		return GLA_SUCCESS;
	}
	else
		return GLA_IO;
}

static int read_serializable(
		io_t *self,
		int idx,
		int64_t bytes)
{
	HSQUIRRELVM vm = self->rt->vm;

	if(bytes != -1)
		return GLA_INVALID_ARGUMENT;
	else if(idx == 0)
		return GLA_INVALID_ARGUMENT;
	if(SQ_FAILED(sq_createinstance(vm, idx)))
		return GLA_NOTSUPPORTED;
	sq_pushobject(vm, self->rtdata->deserialize_string);
	if(SQ_FAILED(sq_get(vm, -2))) {
		sq_poptop(vm);
		return GLA_NOTSUPPORTED;
	}
	sq_push(vm, -2);
	sq_push(vm, 1);
	if(SQ_FAILED(sq_call(vm, 2, false, true))) {
		sq_poptop(vm);
		gla_rt_dump_opstack(self->rt, "NO CALL");
		return GLA_VM;
	}
	sq_poptop(vm);
	return GLA_SUCCESS;
}

static int read_io(
		io_t *self,
		int idx,
		int64_t bytes)
{
	SQUserPointer up;
	HSQUIRRELVM vm = self->rt->vm;
	bool isio;
	int ret;

	if(idx == 0)
		return GLA_INVALID_ARGUMENT;
	else if(sq_gettype(vm, idx) != OT_INSTANCE)
		return GLA_NOTSUPPORTED;
	idx = sq_toabs(vm, idx);
	sq_push(vm, idx);
	sq_pushobject(vm, self->rtdata->io_class);
	isio = sq_instanceof(vm);
	sq_pop(vm, 2);
	if(!isio)
		return GLA_NOTSUPPORTED;
	if(SQ_FAILED(sq_getinstanceup(vm, idx, &up, NULL))) {
		LOG_ERROR("Error getting instance userpointer");
		return GLA_INTERNAL;
	}
	ret = copy_io(up, self, bytes);
	if(ret == GLA_SUCCESS)
		sq_push(vm, idx);
	return ret;
}

static SQInteger fn_dtor(
		SQUserPointer up,
		SQInteger size)
{
	io_t *self = up;

	if(self->pool != NULL)
		apr_pool_destroy(self->pool);

	return SQ_OK;
}

static SQInteger fn_close(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmsuccess(rt, false);
	gla_io_close(self->io);
	apr_pool_destroy(self->pool);
	self->io = NULL;
	self->pool = NULL;

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_rseek(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	SQInteger off;
	int ret;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &off)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	ret = gla_io_rseek(self->io, off, SEEK_SET);
	if(ret == GLA_NOTSUPPORTED)
		return gla_rt_vmthrow(rt, "Not supported");
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error seeking");
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_wseek(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	SQInteger off;
	int ret;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &off)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	ret = gla_io_wseek(self->io, off, SEEK_SET);
	if(ret == GLA_NOTSUPPORTED)
		return gla_rt_vmthrow(rt, "Not supported");
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error seeking");
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_rtell(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	int64_t ret;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	ret = gla_io_rtell(self->io);
	if(ret == GLA_NOTSUPPORTED)
		return gla_rt_vmthrow(rt, "Not supported");
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error getting current read position");
	sq_pushinteger(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_wtell(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	int64_t ret;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	ret = gla_io_wtell(self->io);
	if(ret == GLA_NOTSUPPORTED)
		return gla_rt_vmthrow(rt, "Not supported");
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error getting current write position");
	sq_pushinteger(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_size(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	int64_t ret;
	io_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	ret = gla_io_size(self->io);
	if(ret == GLA_NOTSUPPORTED)
		return gla_rt_vmthrow(rt, "Not supported");
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error getting size");
	sq_pushinteger(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_hash(
		HSQUIRRELVM vm)
{
	const SQChar *result_type = NULL;
	gla_io_t *result_io = NULL;
	int result_io_idx = 0;
	SQInteger bytes = -1;
	SQUserPointer up;
	bool ishash;
	io_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	/* allowed args: <hashfn>, <hashfn, bytes>, <hashfn, string, bytes>, <hashfn, string>, <hashfn, io>, <hashfn, io, bytes> */
	if(sq_gettop(vm) == 3) {
		if(sq_gettype(vm, 3) == OT_INTEGER)
			sq_getinteger(vm, 3, &bytes);
		else if(sq_gettype(vm, 3) == OT_STRING)
			sq_getstring(vm, 3, &result_type);
		else {
			result_io_idx = 3;
			result_io = gla_mod_io_io_get(rt, 3);
			if(result_io == NULL)
				return gla_rt_vmthrow(rt, "Invalid arguments");
		}
	}
	else if(sq_gettop(vm) == 4) {
		if(sq_gettype(vm, 3) == OT_STRING)
			sq_getstring(vm, 3, &result_type);
		else {
			result_io_idx = 3;
			result_io = gla_mod_io_io_get(rt, 3);
			if(result_io == NULL)
				return gla_rt_vmthrow(rt, "Invalid arguments");
		}
		if(SQ_FAILED(sq_getinteger(vm, 4, &bytes)))
			return gla_rt_vmthrow(rt, "Invalid arguments");
	}
	else if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Already closed");
	sq_pushobject(vm, self->rtdata->hash_class);
	sq_push(vm, 2);
	ishash = sq_instanceof(vm);
	sq_pop(vm, 2);
	if(!ishash)
		return gla_rt_vmthrow(rt, "Invalid arguments");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting hash function instance userpointer");
	ret = gla_mod_io_hash_calc(rt, up, self->io, bytes, result_type, result_io);
	if(ret == GLA_UNDERFLOW)
		return gla_rt_vmthrow(rt, "Not enough data");
	
	if(result_io_idx != 0)
		sq_push(vm, result_io_idx);
	else if(result_type == NULL)
		sq_pushnull(vm);
	/* other case: already pushed by hash_calc() */
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_write(
		HSQUIRRELVM vm)
{
	const SQChar *type = NULL;
	SQUserPointer up;
	io_t *self;
	int idx = 2;
	int typeid = -1;
	int i;
	int ret;
	SQInteger bytes = -1;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) < 2 || sq_gettop(vm) > 4)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(sq_gettop(vm) >= 3 && SQ_FAILED(sq_getstring(vm, 3, &type)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected string");
	else if(sq_gettop(vm) >= 4 && SQ_FAILED(sq_getinteger(vm, 4, &bytes)))
		return gla_rt_vmthrow(rt, "Invalid argument 3: expected integer");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Entity currently closed");
	if(type == NULL) {
		if(sq_gettype(vm, 2) == OT_INTEGER)
			typeid = TYPE_S64;
		else if(sq_gettype(vm, 2) == OT_FLOAT)
			typeid = TYPE_F64;
		else if(sq_gettype(vm, 2) == OT_STRING)
			typeid = TYPE_STRING;
		else if(sq_gettype(vm, 2) == OT_BOOL)
			typeid = TYPE_U8;
	}
	else if(strcasecmp(type, "string") == 0) {
		if(SQ_FAILED(sq_tostring(vm, 2)))
			return gla_rt_vmthrow(rt, "Error converting object to string");
		typeid = TYPE_STRING;
		idx = sq_toabs(vm, -1);
	}
	else
		for(i = 0; i < N_TYPES; i++)
			if(strcasecmp(type, type_info[i].name) == 0) {
				typeid = i;
				break;
			}
	if(typeid < 0) {
		ret = write_io(self, idx, bytes);
		if(ret != GLA_NOTSUPPORTED)
			goto done;
		ret = write_serializable(self, idx, bytes);
		if(ret != GLA_NOTSUPPORTED)
			goto done;
		return gla_rt_vmthrow(rt, "Invalid value type given");
	}
	else
		ret = type_info[typeid].write(self, idx, bytes);

done:
	switch(ret) {
		case GLA_SUCCESS:
			return gla_rt_vmsuccess(rt, false);
		case GLA_INVALID_ARGUMENT:
			return gla_rt_vmthrow(rt, "Invalid size argument");
		case GLA_NOTSUPPORTED:
			return gla_rt_vmthrow(rt, "Invalid value for given type");
		case GLA_UNDERFLOW:
			return gla_rt_vmthrow(rt, "Not enough data available for given size");
		default:
			return gla_rt_vmthrow(rt, "IO Error writing");
	}
}

static SQInteger fn_read(
		HSQUIRRELVM vm)
{
	const SQChar *type = NULL;
	SQUserPointer up;
	io_t *self;
	int typeid = -1;
	int i;
	int ret;
	int idx = 0;
	SQInteger bytes = -1;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	/* valid args: <var, string, integer>; <class, string>; <var, integer>; <string>; <string, integer>; <var> */

	if(sq_gettop(vm) == 2) {
		if(sq_gettype(vm, 2) == OT_STRING)
			sq_getstring(vm, 2, &type);
		else
			idx = 2;
	}
	else if(sq_gettop(vm) == 3) {
		if(sq_gettype(vm, 2) == OT_STRING) {
			sq_getstring(vm, 2, &type);
			if(SQ_FAILED(sq_getinteger(vm, 3, &bytes)))
				return gla_rt_vmthrow(rt, "Invalid arguments");
		}
		else if(sq_gettype(vm, 3) == OT_STRING) {
			sq_getstring(vm, 3, &type);
			idx = 2;
		}
		else {
			if(SQ_FAILED(sq_getinteger(vm, 3, &bytes)))
				return gla_rt_vmthrow(rt, "Invalid arguments");
			idx = 2;
		}
	}
	else if(sq_gettop(vm) == 4) {
		if(SQ_FAILED(sq_getstring(vm, 3, &type)))
			return gla_rt_vmthrow(rt, "Invalid arguments");
		else if(SQ_FAILED(sq_getinteger(vm, 4, &bytes)))
			return gla_rt_vmthrow(rt, "Invalid arguments");
		idx = 2;
	}
	else
		return gla_rt_vmthrow(rt, "Invalid argument count");

	if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io == NULL)
		return gla_rt_vmthrow(rt, "Entity currently closed");

	if(type == NULL) {
		if(idx == 0)
			return gla_rt_vmthrow(rt, "Invalid arguments");
		ret = read_serializable(self, idx, bytes);
		if(ret != GLA_NOTSUPPORTED)
			goto done;
		ret = read_io(self, idx, bytes);
		if(ret != GLA_NOTSUPPORTED)
			goto done;
		return gla_rt_vmthrow(rt, "Invalid arguments");
	}
	else {
		for(i = 0; i < N_TYPES; i++)
			if(strcasecmp(type, type_info[i].name) == 0) {
				typeid = i;
				break;
			}
		if(typeid < 0)
			return gla_rt_vmthrow(rt, "Invalid type given");
		ret = type_info[i].read(self, idx, bytes);
		goto done;
	}
done:
	switch(ret) {
		case GLA_SUCCESS:
			return gla_rt_vmsuccess(rt, true);
		case GLA_INVALID_ARGUMENT:
			return gla_rt_vmthrow(rt, "Invalid arguments");
		case GLA_NOTSUPPORTED:
			return gla_rt_vmthrow(rt, "Invalid instance type");
		case GLA_UNDERFLOW:
			return gla_rt_vmthrow(rt, "Not enough data available for given size");
		default:
			return gla_rt_vmthrow(rt, "IO Error reading");
	}
}

int gla_mod_io_io_init(
		gla_rt_t *rt,
		int idx,
		gla_io_t *io,
		uint64_t roff,
		uint64_t woff,
		apr_pool_t *pool)
{
	rtdata_t *rtdata;
	SQUserPointer up;
	io_t *self;
	HSQUIRRELVM vm = rt->vm;

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL) {
		LOG_ERROR("Error getting rt data for class IO");
		return GLA_INTERNAL;
	}
	if(SQ_FAILED(sq_getinstanceup(vm, idx, &up, NULL))) {
		LOG_ERROR("Error getting instance userpointer");
		return GLA_VM;
	}
	self = up;
	memset(self, 0, sizeof(io_t));
	self->rt = rt;
	self->rtdata = rtdata;
	self->endian = ENDIAN_NATIVE;
	self->pool = pool;
	self->roff = roff;
	self->woff = woff;
	self->io = io;

	sq_setreleasehook(vm, idx, fn_dtor);
	return GLA_SUCCESS;
}

int gla_mod_io_io_new(
		gla_rt_t *rt,
		gla_io_t *io,
		uint64_t roff,
		uint64_t woff,
		apr_pool_t *pool)
{
	int ret;
	rtdata_t *rtdata;
	HSQUIRRELVM vm = rt->vm;

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL) {
		LOG_ERROR("Error getting rt data for class IO (note: import gla.io.IO first)");
		return GLA_INTERNAL;
	}

	sq_pushobject(vm, rtdata->io_class);
	if(SQ_FAILED(sq_createinstance(vm, -1))) {
		sq_poptop(vm);
		LOG_ERROR("Error instantiating IO class");
		return GLA_VM;
	}

	ret = gla_mod_io_io_init(rt, -1, io, roff, woff, pool);
	if(ret != GLA_SUCCESS)
		sq_pop(vm, 2);
	else
		sq_remove(vm, -2);
	return ret;
}

gla_io_t *gla_mod_io_io_get(
		gla_rt_t *rt,
		int idx)
{
	rtdata_t *rtdata;
	SQUserPointer up;
	io_t *self;
	bool isio;
	HSQUIRRELVM vm = rt->vm;

	idx = sq_toabs(vm, idx);
	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL) {
		LOG_ERROR("Error getting rt data for class IO");
		errno = GLA_INTERNAL;
		return NULL;
	}
	sq_pushobject(vm, rtdata->io_class);
	sq_push(vm, idx);
	isio = sq_instanceof(vm);
	sq_pop(vm, 2);
	if(!isio) {
		errno = GLA_INVALID_ARGUMENT;
		return NULL;
	}

	if(SQ_FAILED(sq_getinstanceup(vm, idx, &up, NULL))) {
		LOG_ERROR("Error getting instance userpointer");
		errno = GLA_VM;
		return NULL;
	}
	self = up;
	return self->io;
}

SQInteger gla_mod_io_io_augment(
		HSQUIRRELVM vm)
{
	rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	if(rtdata == NULL)
		return gla_rt_vmthrow(rt, "Error getting rt data");
	sq_getstackobj(vm, 2, &rtdata->io_class);

	sq_pushstring(vm, "_serialize", -1);
	sq_getstackobj(vm, -1, &rtdata->serialize_string);
	sq_addref(vm, &rtdata->serialize_string);
	sq_poptop(vm);
	sq_pushstring(vm, "_deserialize", -1);
	sq_getstackobj(vm, -1, &rtdata->deserialize_string);
	sq_addref(vm, &rtdata->deserialize_string);
	sq_poptop(vm);

	sq_pushstring(vm, "close", -1);
	sq_newclosure(vm, fn_close, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "write", -1);
	sq_newclosure(vm, fn_write, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "read", -1);
	sq_newclosure(vm, fn_read, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "rseek", -1);
	sq_newclosure(vm, fn_rseek, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "wseek", -1);
	sq_newclosure(vm, fn_wseek, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "rtell", -1);
	sq_newclosure(vm, fn_rtell, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "wtell", -1);
	sq_newclosure(vm, fn_wtell, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "size", -1);
	sq_newclosure(vm, fn_size, 0);
	sq_newslot(vm, 2, false);

	sq_pushstring(vm, "hash", -1);
	sq_newclosure(vm, fn_hash, 0);
	sq_newslot(vm, 2, false);

	if(SQ_FAILED(sq_setclassudsize(vm, 2, sizeof(io_t))))
		return GLA_INTERNAL;

	return gla_rt_vmsuccess(rt, true);
}

