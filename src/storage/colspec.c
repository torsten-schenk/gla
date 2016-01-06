#include <assert.h>
#include <stdlib.h>

#include "../rt.h"
#include "../log.h"

#include "_colspec.h"

#include "colspec.h"

#define RTDATA_TOKEN gla_mod_storage_colspec_cbridge

typedef struct {
	HSQOBJECT colspec_class;
} rtdata_t;

static SQInteger fn_colspec_class(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);

	assert(rtdata != NULL);
	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	sq_getstackobj(vm, 2, &rtdata->colspec_class);
	sq_addref(vm, &rtdata->colspec_class);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_dtor(
		SQUserPointer up,
		SQInteger size)
{
	free(up);
	return SQ_OK;
}

static SQInteger fn_init(
		HSQUIRRELVM vm)
{
	SQInteger kcols;
	SQInteger ncols;
	const SQChar *type;
	SQInteger i;
	int size;
	char *alloc;
	colspec_t *colspec;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");

	sq_pushstring(vm, "kcols", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Missing field 'kcols'");
	if(SQ_FAILED(sq_getinteger(vm, -1, &kcols)))
		return gla_rt_vmthrow(rt, "Invalid value in field 'kcols': expected integer");
	sq_pop(vm, 1);

	sq_pushstring(vm, "columns", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Missing field 'columns'");
	ncols = sq_getsize(vm, -1);
	if(ncols < 0)
		return gla_rt_vmthrow(rt, "Invalid value in field 'columns': expected array");

	size = sizeof(colspec_t) + ncols * sizeof(column_t);
	for(i = 0; i < ncols; i++) {
		sq_pushinteger(vm, i);
		if(SQ_FAILED(sq_get(vm, -2)))
			return gla_rt_vmthrow(rt, "Error retrieving entry from 'columns'");
		sq_pushstring(vm, "type", -1);
		if(SQ_FAILED(sq_get(vm, -2)))
			return gla_rt_vmthrow(rt, "Error retrieving 'type' from column");
		if(sq_gettype(vm, -1) != OT_NULL && SQ_FAILED(sq_getstring(vm, -1, &type)))
			return gla_rt_vmthrow(rt, "Error retrieving 'type' string from stack");
		sq_pop(vm, 2);
	}
	
	alloc = calloc(1, size);
	if(alloc == NULL)
		return gla_rt_vmthrow(rt, "");
	colspec = (colspec_t*)alloc;
	colspec->column = (column_t*)(alloc + sizeof(colspec_t));
	if(SQ_FAILED(sq_setinstanceup(vm, 2, colspec))) {
		free(colspec);
		return gla_rt_vmthrow(rt, "Error setting instance userpointer");
	}
	colspec->n_key = kcols;
	colspec->n_total = ncols;
	sq_setreleasehook(vm, 2, fn_dtor);
	sq_getstackobj(vm, 2, &colspec->thisobj);

	for(i = 0; i < ncols; i++) {
		sq_pushinteger(vm, i);
		sq_get(vm, -2);
		sq_pushstring(vm, "type", -1);
		sq_get(vm, -2);
		if(sq_gettype(vm, -1) == OT_NULL)
			colspec->column[i].type = COL_VARIANT;
		else {
			sq_getstring(vm, -1, &type);

			if(strcmp(type, "integer") == 0 || strcmp(type, "i") == 0)
				colspec->column[i].type = COL_INTEGER;
			else if(strcmp(type, "bool") == 0 || strcmp(type, "b") == 0)
				colspec->column[i].type = COL_BOOL;
			else if(strcmp(type, "string") == 0 || strcmp(type, "s") == 0)
				colspec->column[i].type = COL_STRING;
			else if(strcmp(type, "float") == 0 || strcmp(type, "f") == 0)
				colspec->column[i].type = COL_FLOAT;
			else
				return gla_rt_vmthrow(rt, "Invalid column type");
		}
		sq_pop(vm, 2);
	}

	sq_pop(vm, 1);

	return gla_rt_vmsuccess(rt, false);
}

int gla_mod_storage_colspec_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;
	rtdata_t *rtdata = apr_pcalloc(rt->mpool, sizeof(rtdata_t));

	gla_rt_data_put(rt, gla_mod_storage_colspec_cbridge, rtdata);

	sq_pushstring(vm, "colspecInit", -1);
	sq_newclosure(vm, fn_init, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "colspec", -1);
	sq_newclosure(vm, fn_colspec_class, 0);
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

colspec_t *gla_mod_storage_new_colspec(
		gla_rt_t *rt,
		int nkey,
		int ncols)
{
	int i;
	const rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	colspec_t *colspec;
	int size;
	char *alloc;

	if(nkey > ncols) {
		LOG_ERROR("Cannot create column spec: number of key columns > total number of columns");
		errno = GLA_INVALID_ARGUMENT;
		return NULL;
	}
	assert(rtdata != NULL);
	sq_pushobject(rt->vm, rtdata->colspec_class);
	sq_createinstance(rt->vm, -1);

	size = sizeof(colspec_t) + ncols * sizeof(column_t);
	alloc = calloc(1, size);
	if(alloc == NULL) {
		errno = GLA_ALLOC;
		return NULL;
	}
	colspec = (colspec_t*)alloc;
	colspec->column = (column_t*)(alloc + sizeof(colspec_t));

	colspec->n_key = nkey;
	colspec->n_total = ncols;
	sq_setinstanceup(rt->vm, -1, colspec);
	sq_setreleasehook(rt->vm, -1, fn_dtor);
	sq_getstackobj(rt->vm, -1, &colspec->thisobj);
	sq_addref(rt->vm, &colspec->thisobj);

	sq_pushstring(rt->vm, "columns", -1);
	sq_newarray(rt->vm, ncols);
	for(i = 0; i < ncols; i++) {
		sq_pushinteger(rt->vm, i);
		sq_newtable(rt->vm);

		sq_pushstring(rt->vm, "type", -1);
		sq_pushnull(rt->vm);
		sq_newslot(rt->vm, -3, false);

		sq_set(rt->vm, -3);
	}
	sq_set(rt->vm, -3);

	sq_pushstring(rt->vm, "kcols", -1);
	sq_pushinteger(rt->vm, nkey);
	sq_set(rt->vm, -3);

	sq_pushstring(rt->vm, "vcols", -1);
	sq_pushinteger(rt->vm, ncols - nkey);
	sq_set(rt->vm, -3);

	sq_pop(rt->vm, 2);
	return colspec;
}

int gla_mod_storage_colspec_set_type(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		int type)
{
	if(col >= colspec->n_total || col < 0) {
		LOG_ERROR("Error setting column spec column type: invalid column number %d", col);
		return GLA_INVALID_ARGUMENT;
	}
	sq_pushobject(rt->vm, colspec->thisobj);
	sq_pushstring(rt->vm, "columns", -1);
	sq_get(rt->vm, -2);
	sq_pushinteger(rt->vm, col);
	sq_get(rt->vm, -2);
	
	sq_pushstring(rt->vm, "type", -1);
	switch(type) {
		case COL_VARIANT:
			sq_pushnull(rt->vm);
			break;
		case COL_INTEGER:
			sq_pushstring(rt->vm, "integer", -1);
			break;
		case COL_STRING:
			sq_pushstring(rt->vm, "string", -1);
			break;
		case COL_BOOL:
			sq_pushstring(rt->vm, "bool", -1);
			break;
		case COL_FLOAT:
			sq_pushstring(rt->vm, "float", -1);
			break;
		default:
			LOG_ERROR("Error setting column spec column type: unknown type %d", type);

	}
	sq_set(rt->vm, -3);
	sq_pop(rt->vm, 3);

	colspec->column[col].type = type;
	return GLA_SUCCESS;
}

int gla_mod_storage_colspec_set_flags(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		int flags)
{
	if(col >= colspec->n_total || col < 0) {
		LOG_ERROR("Error setting column spec column type: invalid column number %d", col);
		return GLA_INVALID_ARGUMENT;
	}
	colspec->column[col].flags = flags;
	return GLA_SUCCESS;
}

int gla_mod_storage_colspec_set_name(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		const char *name)
{

	return GLA_SUCCESS;
}

