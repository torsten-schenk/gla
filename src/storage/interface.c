#include <stdlib.h>
#include <assert.h>

#include "../rt.h"
#include "../log.h"

#include "_table.h"

#include "interface.h"

#define RTDATA_TOKEN gla_mod_storage_table_cbridge

typedef struct {
	HSQOBJECT handler_class;
	HSQOBJECT iterator_class;
	HSQMEMBERHANDLE iterator_index_member;
} rtdata_t;

bool gla_mod_storage_table_valid_name(
		const char *name)
{
	if(name == NULL)
		LOG_ERROR("Missing table name");
	else if(*name == 0)
		LOG_ERROR("Empty table name not allowed");
	else if(strlen(name) > MAX_TABLE_NAME_LEN)
		LOG_ERROR("Table name too long");
	else {
		while(*name != 0) {
			name++;
		}
		return true;
	}
	return false;
}

static SQInteger fnm_db_dtor(
		SQUserPointer up,
		SQInteger size)
{
	abstract_database_t *db = up;
	abstract_table_t *table;
	abstract_table_t *next;

	for(table = db->first_open; table != NULL; table = next) {
		db->meta->database_methods->close_table(db, table);
		next = table->next_open;
		table->db = NULL;
		table->prev_open = NULL;
		table->next_open = NULL;
	}
	db->first_open = NULL;
	db->last_open = NULL;
	db->meta->database_methods->close(db);

	return SQ_OK;
}

static SQInteger fnbr_db_ctor(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	SQInteger udsize;
	abstract_database_t *db;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getclass(vm, 2)))
		return gla_rt_vmthrow(rt, "Error getting database class from instance");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting database instance userpointer");
	else if(sq_gettype(vm, 3) != OT_TABLE)
		return gla_rt_vmthrow(rt, "Error getting database parameters: expected table or null");
	db = up;
	udsize = sq_getsize(vm, 2);
	if(udsize < 0)
		return gla_rt_vmthrow(rt, "Error getting database userdata size");
	memset(db, 0, udsize);

	sq_pushstring(vm, "_meta", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Error getting database class '_meta' member");
	if(SQ_FAILED(sq_getuserpointer(vm, -1, &up)))
		return gla_rt_vmthrow(rt, "Error getting database class '_meta' from stack");
	sq_poptop(vm);
	db->meta = up;

	ret = db->meta->database_methods->open(db, 3);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error opening database");
	sq_setreleasehook(vm, 2, fnm_db_dtor);

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_dtor(
		SQUserPointer up,
		SQInteger size)
{
	abstract_table_t *table = up;
	abstract_database_t *db = table->db;

	if(db != NULL) {
		table->meta->database_methods->close_table(table->db, table);
		if(table->prev_open == NULL)
			db->first_open = table->next_open;
		else
			table->prev_open->next_open = table->next_open;
		if(table->next_open == NULL)
			db->last_open = table->prev_open;
		else
			table->next_open->prev_open = table->prev_open;
		table->db = NULL;
	}
	return SQ_OK;
}

static SQInteger fnbr_db_close(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	SQUserPointer up;
	abstract_database_t *db;
	abstract_table_t *table;
	abstract_table_t *next;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	db = up;

	for(table = db->first_open; table != NULL; table = next) {
		db->meta->database_methods->close_table(db, table);
		next = table->next_open;
		table->db = NULL;
		table->prev_open = NULL;
		table->next_open = NULL;
	}
	db->first_open = NULL;
	db->last_open = NULL;
//	db->meta->database_methods->close(db); TODO also close database here; in all ohter methods, add a check, whether database is currently open

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnbr_db_open_table(
		HSQUIRRELVM vm)
{
	int ret;
	int flags;
	SQInteger magic;
	const SQChar *mode;
	const SQChar *name;
	SQUserPointer up;
	SQInteger udsize;
	abstract_database_t *db;
	abstract_builder_t *builder;
	abstract_table_t *table;
	colspec_t *colspec = NULL;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 6)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 3, &name)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected string");
	else if(SQ_FAILED(sq_getinteger(vm, 4, &magic)))
		return gla_rt_vmthrow(rt, "Invalid argument 3: expected integer");
	if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected database instance");
	db = up;
	if(SQ_FAILED(sq_getstring(vm, 5, &mode)))
		return gla_rt_vmthrow(rt, "Invalid argument 4: expected string");
	if(sq_gettype(vm, 6) != OT_NULL) {
		if(SQ_FAILED(sq_getinstanceup(vm, 6, &up, NULL)))
			return gla_rt_vmthrow(rt, "Invalid argument 5: expected builder instance");
		builder = up;
	}
	else
		builder = NULL;
	
	flags = 0;
	while(*mode != 0) {
		if(*mode == 'c')
			flags |= OPEN_CREATE;
		else if(*mode == 't')
			flags |= OPEN_TRUNCATE;
		else if(*mode == 'r')
			flags |= OPEN_RDONLY;
		mode++;
	}
	if((flags & OPEN_RDONLY) != 0 && (flags & OPEN_CREATE) != 0)
		return gla_rt_vmthrow(rt, "Invalid open mode: cannot specifx both 'r' and 'c'");
	else if((flags & OPEN_RDONLY) != 0 && (flags & OPEN_TRUNCATE) != 0)
		return gla_rt_vmthrow(rt, "Invalid open mode: cannot specifx both 'r' and 't'");

	if((flags & OPEN_CREATE) != 0 || (flags & OPEN_TRUNCATE) != 0) {
		if(builder == NULL)
			return gla_rt_vmthrow(rt, "Invalid arguments: open mode 'c' and 't' both require a builder but no builder given");
		else if(db->meta != builder->meta)
			return gla_rt_vmthrow(rt, "Invalid arguments: database instance and builder instance don't belong to same storage subsystem");
		assert(builder->colspec != NULL);
	}

	sq_pushobject(vm, db->meta->handler_class);
	if(SQ_FAILED(sq_createinstance(vm, -1)))
		return gla_rt_vmthrow(rt, "Error creating table handler instance");
	else if(SQ_FAILED(sq_getinstanceup(vm, -1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting handler instance userpointer");
	table = up;
	udsize = sq_getsize(vm, -2);
	if(udsize < 0)
		return gla_rt_vmthrow(rt, "Error getting handler userdata size");
	memset(table, 0, udsize);
	table->meta = db->meta;
	table->db = db;

	ret = db->meta->database_methods->open_table(db, name, magic, flags, builder, table, &colspec);
	if(ret == GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "Error opening table: no such table");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error opening table");
	assert(colspec != NULL);

	sq_pushobject(vm, db->meta->table_class);
	if(SQ_FAILED(sq_createinstance(vm, -1)))
		return gla_rt_vmthrow(rt, "Error creating table instance");
	sq_pushstring(vm, "_c", -1);
	sq_push(vm, -4);
	sq_set(vm, -3);
	sq_pushstring(vm, "_colspec", -1);
	sq_pushobject(vm, colspec->thisobj);
	sq_set(vm, -3);
	sq_pushstring(vm, "_db", -1);
	sq_push(vm, 2);
	sq_set(vm, -3);
	sq_setreleasehook(vm, -3, fnm_table_dtor);

	table->prev_open = db->last_open;
	if(db->last_open == NULL)
		db->first_open = table;
	else
		db->last_open->next_open = table;
	db->last_open = table;
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnbr_db_close_table(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	abstract_database_t *db;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting database instance userpointer");
	db = up;
	if(SQ_FAILED(sq_getinstanceup(vm, 3, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting table instance userpointer");
	table = up;
	assert(db->meta == table->meta);
	assert(table->meta->rt != NULL);
	if(table->db == NULL) /* already closed */
		return gla_rt_vmsuccess(rt, false);

	db->meta->database_methods->close_table(db, table);
	if(table->prev_open == NULL)
		db->first_open = table->next_open;
	else
		table->prev_open->next_open = table->next_open;
	if(table->next_open == NULL)
		db->last_open = table->prev_open;
	else
		table->next_open->prev_open = table->prev_open;
	table->db = NULL;

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnbr_db_table_exists(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	const SQChar *name;
	abstract_database_t *db;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting database instance userpointer");
	else if(SQ_FAILED(sq_getstring(vm, 3, &name)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected string");
	db = up;
	assert(db->meta->rt != NULL);

	ret = db->meta->database_methods->table_exists(db, name);
	if(ret < 0)
		return gla_rt_vmthrow(rt, "Error checking table existence");
	sq_pushbool(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnbr_db_builder_class(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	abstract_database_t *db;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting database instance userpointer");
	db = up;
	sq_pushobject(vm, db->meta->builder_class);

	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_ldrl(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_kcols;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_kcols)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ldrl(table, p_kcols, rt->mpstack);
	if(ret != GLA_SUCCESS && ret != GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "Error in 'ldrl'");

	sq_pushbool(vm, ret == GLA_SUCCESS);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_ldru(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_kcols;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_kcols)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ldru(table, p_kcols, rt->mpstack);
	if(ret != GLA_SUCCESS && ret != GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "Error in 'ldru'");

	sq_pushbool(vm, ret == GLA_SUCCESS);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_ldri(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_row;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_row)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ldri(table, p_row, rt->mpstack);
	if(ret == GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "No such row");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'ldri'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_ldrb(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ldrb(table, rt->mpstack);
	if(ret == GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "No such row");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'ldrb'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_ldre(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ldre(table, rt->mpstack);
	if(ret == GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "No such row");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'ldre'");

	return gla_rt_vmsuccess(rt, false);
}

/* Returns null if there is no index, i.e. the preceding load-operation
 * was unable to compute an index (example: when using a string store
 * and trying load a row using ldrl; column sorting for the corresponding key column is disabled
 * and the store does not contain the requested string) */
static SQInteger fnm_table_idx(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->idx(table, rt->mpstack);
	if(ret == -ENOENT)
		sq_pushnull(vm);
	else if(ret < 0)
		return gla_rt_vmthrow(rt, "Error in 'idx'");
	else
		sq_pushinteger(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_str(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->str(table, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'str'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_mkrk(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->mkrk(table, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'mkrk'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_mkri(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_row;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_row)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->mkri(table, p_row, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'mkri'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_rmr(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_row;
	SQInteger p_n;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_row)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	else if(SQ_FAILED(sq_getinteger(vm, 3, &p_n)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->rmr(table, p_row, p_n, rt->mpstack);
	if(ret < 0)
		return gla_rt_vmthrow(rt, "Error in 'rmr'");
	sq_pushinteger(vm, ret);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_clr(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->clr(table, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'clr'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_itdt(
		SQUserPointer up,
		SQInteger size)
{
	abstract_iterator_t *it = up;
	if(it->meta->iterator_methods->destroy != NULL)
		it->meta->iterator_methods->destroy(it);
	free(it);
	return SQ_OK;
}

static SQInteger fnm_table_it(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	abstract_iterator_t *it;
	const rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	assert(rtdata != NULL);
	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	it = calloc(1, table->meta->iterator_size);
	if(it == NULL)
		return gla_rt_vmthrow(rt, "Error allocating iterator data");
	it->meta = table->meta;
	it->table = table;
	ret = it->meta->iterator_methods->init(it, rt->mpstack);
	if(ret < 0) {
		free(it);
		return gla_rt_vmthrow(rt, "Error in 'it'");
	}
	sq_pushobject(vm, rtdata->iterator_class);
	if(SQ_FAILED(sq_createinstance(vm, -1))) {
		free(it);
		return gla_rt_vmthrow(rt, "Error creating iterator instance");
	}
	if(SQ_FAILED(sq_setinstanceup(vm, -1, it))) {
		free(it);
		return gla_rt_vmthrow(rt, "Error setting iterator userpointer");
	}
	sq_setreleasehook(vm, -1, fnm_table_itdt);
	sq_pushinteger(vm, it->index);
	sq_setbyhandle(vm, -2, &rtdata->iterator_index_member);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_itmv(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_amount;
	SQUserPointer up;
	abstract_iterator_t *it;
	const rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	assert(rtdata != NULL);
	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting iterator instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 3, &p_amount)))
		return gla_rt_vmthrow(rt, "Invalid argument 2: expected integer");
	it = up;
	assert(it->table->meta->rt != NULL);
	if(it->table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = it->meta->iterator_methods->mv(it, p_amount, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'itmv'");
	sq_pushinteger(vm, it->index);
	sq_setbyhandle(vm, 2, &rtdata->iterator_index_member);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_itupd(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_iterator_t *it;
	const rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	assert(rtdata != NULL);
	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting iterator instance userpointer");
	it = up;
	assert(it->table->meta->rt != NULL);
	if(it->table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = it->meta->iterator_methods->upd(it, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'itupd'");
	sq_pushinteger(vm, it->index);
	sq_setbyhandle(vm, 2, &rtdata->iterator_index_member);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_itldr(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_iterator_t *it;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting iterator instance userpointer");
	it = up;
	assert(it->table->meta->rt != NULL);
	if(it->table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = it->meta->iterator_methods->ldr(it, rt->mpstack);
	if(ret == GLA_NOTFOUND)
		return gla_rt_vmthrow(rt, "No such row");
	else if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'itldr'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_itdup(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_iterator_t *org;
	abstract_iterator_t *it;
	const rtdata_t *rtdata;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	assert(rtdata != NULL);
	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting iterator instance userpointer");
	org = up;
	assert(org->table->meta->rt != NULL);
	if(org->table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	it = calloc(1, org->table->meta->iterator_size);
	if(it == NULL)
		return gla_rt_vmthrow(rt, "Error allocating iterator data");
	it->meta = org->meta;
	it->table = org->table;
	it->index = org->index;
	ret = org->meta->iterator_methods->dup(it, org, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'itdup'");

	sq_pushobject(vm, rtdata->iterator_class);
	if(SQ_FAILED(sq_createinstance(vm, -1))) {
		free(it);
		return gla_rt_vmthrow(rt, "Error creating iterator instance");
	}
	if(SQ_FAILED(sq_setinstanceup(vm, -1, it))) {
		free(it);
		return gla_rt_vmthrow(rt, "Error setting iterator userpointer");
	}
	sq_setreleasehook(vm, -1, fnm_table_itdt);
	sq_pushinteger(vm, it->index);
	sq_setbyhandle(vm, -2, &rtdata->iterator_index_member);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_ptc(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_cell;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_cell)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->ptc(table, p_cell, 3, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'ptc'");

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_table_gtc(
		HSQUIRRELVM vm)
{
	int ret;
	SQInteger p_cell;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &p_cell)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected integer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->gtc(table, p_cell, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error in 'gtc'");

	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_table_sz(
		HSQUIRRELVM vm)
{
	int ret;
	SQUserPointer up;
	abstract_table_t *table;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	table = up;
	assert(table->meta->rt != NULL);
	if(table->db == NULL)
		return gla_rt_vmthrow(rt, "Table already closed");

	ret = table->meta->table_methods->sz(table, rt->mpstack);
	if(ret < 0)
		return gla_rt_vmthrow(rt, "Error in 'sz'");
	sq_pushinteger(vm, ret);

	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fnm_builder_dtor(
		SQUserPointer up,
		SQInteger size)
{
	abstract_builder_t *builder = up;
	gla_rt_t *rt = builder->meta->rt;
	if(rt != NULL) {
		if(builder->meta->builder_methods->destroy != NULL)
			builder->meta->builder_methods->destroy(builder);
		if(!rt->shutdown)
			sq_release(rt->vm, &builder->colspec->thisobj);
	}
	return SQ_OK;
}

static SQInteger fnbr_builder_ctor(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	abstract_builder_t *builder;
	int size;
	int ret;
	colspec_t *colspec;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 2, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	builder = up;
	if(SQ_FAILED(sq_getinstanceup(vm, 3, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting colspec instance userpointer");
	colspec = up;

	if(SQ_FAILED(sq_getclass(vm, 2)))
		return gla_rt_vmthrow(rt, "Error getting instance class");
	size = sq_getsize(vm, -1);
	assert(size > 0);
	memset(builder, 0, size);
	sq_pushstring(vm, "_meta", -1);
	if(SQ_FAILED(sq_get(vm, -2)))
		return gla_rt_vmthrow(rt, "Error getting '_meta' member from builder class");
	else if(SQ_FAILED(sq_getuserpointer(vm, -1, &up)))
		return gla_rt_vmthrow(rt, "Invalid builder class member '_meta': not a userpointer");
	builder->meta = up;
	builder->colspec = colspec;
	builder->multikey = false;

	if(builder->meta->builder_methods->init != NULL) {
		ret = builder->meta->builder_methods->init(builder);
		if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error initializing builder");
	}
	sq_addref(vm, &colspec->thisobj);
	sq_setreleasehook(vm, 1, fnm_builder_dtor);

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_builder_set(
		HSQUIRRELVM vm)
{
	const SQChar *key;
	int ret;
	SQUserPointer up;
	abstract_builder_t *builder;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getstring(vm, 2, &key)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	builder = up;

	if(strcmp(key, "multikey") == 0) {
		SQBool value;
		if(SQ_FAILED(sq_getbool(vm, 3, &value)))
			return gla_rt_vmthrow(rt, "Invalid value for 'multikey': expected boolean");
		builder->multikey = value;
	}
	else {
		ret = builder->meta->builder_methods->set(builder, key, 3);
		if(ret == GLA_NOTFOUND)
			return gla_rt_vmthrow_null(rt);
		else if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error setting option");
	}
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fnm_builder_get(
		HSQUIRRELVM vm)
{
	const SQChar *key;
	int ret;
	SQUserPointer up;
	abstract_builder_t *builder;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(SQ_FAILED(sq_getstring(vm, 2, &key)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	builder = up;

	if(strcmp(key, "multikey") == 0)
		sq_pushbool(vm, builder->multikey);
	else {
		ret = builder->meta->builder_methods->get(builder, key);
		if(ret == GLA_NOTFOUND)
			return gla_rt_vmthrow_null(rt);
		else if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error getting option");
	}
	return gla_rt_vmsuccess(rt, true);
}

int gla_mod_storage_meta_init(
		gla_rt_t *rt,
		abstract_meta_t *meta,
		const methods_database_t *database_methods,
		const methods_builder_t *builder_methods,
		const methods_table_t *table_methods,
		const methods_iterator_t *iterator_methods,
		int database_size,
		int builder_size,
		int table_size,
		int iterator_size)
{
	const rtdata_t *rtdata;
	HSQUIRRELVM vm = rt->vm;

	rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	assert(rtdata != NULL);
	if(sq_gettop(vm) != 4) {
		LOG_ERROR("Invalid argument count");
		return GLA_VM;
	}

	sq_pushobject(vm, rtdata->handler_class);
	if(SQ_FAILED(sq_newclass(vm, true))) {
		LOG_ERROR("Error creating handler class");
		return GLA_VM;
	}
	sq_pushstring(vm, "_meta", -1);
	sq_pushuserpointer(vm, meta);
	sq_newslot(vm, -3, true);
	sq_setclassudsize(vm, -1, table_size);
	sq_getstackobj(vm, -1, &meta->handler_class);
	sq_addref(vm, &meta->handler_class);
	sq_poptop(vm);

	sq_pushnull(vm);
	sq_getstackobj(vm, -1, &meta->null_object);
	sq_poptop(vm);

	meta->rt = rt;
	meta->database_methods = database_methods;
	meta->builder_methods = builder_methods;
	meta->table_methods = table_methods;
	meta->iterator_methods = iterator_methods;
	meta->iterator_size = iterator_size;

	sq_pushstring(vm, "_meta", -1);
	sq_pushuserpointer(vm, meta);
	sq_newslot(vm, 2, true);
	sq_getstackobj(vm, 2, &meta->database_class);
	sq_addref(vm, &meta->database_class);
	sq_setclassudsize(vm, 2, database_size);

	sq_pushstring(vm, "_meta", -1);
	sq_pushuserpointer(vm, meta);
	sq_newslot(vm, 3, true);
	sq_getstackobj(vm, 3, &meta->builder_class);
	sq_addref(vm, &meta->builder_class);
	sq_setclassudsize(vm, 3, builder_size);

	sq_pushstring(vm, "Builder", -1);
	sq_push(vm, 3);
	sq_newslot(vm, 2, true);

	sq_pushstring(rt->vm, "set", -1);
	sq_newclosure(rt->vm, fnm_builder_set, 0);
	sq_newslot(rt->vm, 3, false);

	sq_pushstring(rt->vm, "get", -1);
	sq_newclosure(rt->vm, fnm_builder_get, 0);
	sq_newslot(rt->vm, 3, false);

	sq_pushstring(vm, "_meta", -1);
	sq_pushuserpointer(vm, meta);
	sq_newslot(vm, 4, true);
	sq_getstackobj(vm, 4, &meta->table_class);
	sq_addref(vm, &meta->table_class);

	return GLA_SUCCESS;
}

int gla_mod_storage_table_cbridge(
		gla_rt_t *rt)
{
	int ret;
	gla_path_t path;
	rtdata_t *rtdata;
	HSQUIRRELVM vm = rt->vm;

	rtdata = apr_pcalloc(rt->mpool, sizeof(rtdata_t));
	if(rtdata == NULL)
		return GLA_ALLOC;
	ret = gla_path_parse_entity(&path, "gla.storage.Iterator", rt->mpstack);
	if(ret != GLA_SUCCESS)
		return ret;
	ret = gla_rt_import(rt, &path, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return ret;
	sq_getstackobj(vm, -1, &rtdata->iterator_class);
	sq_pushstring(vm, "index", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->iterator_index_member)))
		return GLA_VM;
	sq_poptop(vm);
	ret = gla_rt_data_put(rt, RTDATA_TOKEN, rtdata);
	if(ret != GLA_SUCCESS)
		return ret;

	sq_newclass(vm, false);
	sq_getstackobj(vm, -1, &rtdata->handler_class);
	sq_addref(vm, &rtdata->handler_class);

	sq_pushstring(vm, "ldrl", -1);
	sq_newclosure(vm, fnm_table_ldrl, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "ldru", -1);
	sq_newclosure(vm, fnm_table_ldru, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "ldri", -1);
	sq_newclosure(vm, fnm_table_ldri, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "ldrb", -1);
	sq_newclosure(vm, fnm_table_ldrb, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "ldre", -1);
	sq_newclosure(vm, fnm_table_ldre, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "idx", -1);
	sq_newclosure(vm, fnm_table_idx, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "str", -1);
	sq_newclosure(vm, fnm_table_str, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "mkrk", -1);
	sq_newclosure(vm, fnm_table_mkrk, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "mkri", -1);
	sq_newclosure(vm, fnm_table_mkri, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "rmr", -1);
	sq_newclosure(vm, fnm_table_rmr, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "clr", -1);
	sq_newclosure(vm, fnm_table_clr, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "it", -1);
	sq_newclosure(vm, fnm_table_it, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "itmv", -1);
	sq_newclosure(vm, fnm_table_itmv, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "itupd", -1);
	sq_newclosure(vm, fnm_table_itupd, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "itldr", -1);
	sq_newclosure(vm, fnm_table_itldr, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "itdup", -1);
	sq_newclosure(vm, fnm_table_itdup, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "ptc", -1);
	sq_newclosure(vm, fnm_table_ptc, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "gtc", -1);
	sq_newclosure(vm, fnm_table_gtc, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "sz", -1);
	sq_newclosure(vm, fnm_table_sz, 0);
	sq_newslot(vm, -3, false);

	sq_poptop(vm);

	/* builder functions */
	sq_pushstring(vm, "builder", -1);
	sq_newtable(vm);

	sq_pushstring(vm, "ctor", -1);
	sq_newclosure(vm, fnbr_builder_ctor, 0);
	sq_newslot(vm, -3, false);

	sq_newslot(vm, -3, false);

	/* database functions */
	sq_pushstring(vm, "database", -1);
	sq_newtable(vm);

	sq_pushstring(vm, "ctor", -1);
	sq_newclosure(vm, fnbr_db_ctor, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "close", -1);
	sq_newclosure(vm, fnbr_db_close, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "openTable", -1);
	sq_newclosure(vm, fnbr_db_open_table, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "closeTable", -1);
	sq_newclosure(vm, fnbr_db_close_table, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "tableExists", -1);
	sq_newclosure(vm, fnbr_db_table_exists, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "builderClass", -1);
	sq_newclosure(vm, fnbr_db_builder_class, 0);
	sq_newslot(vm, -3, false);

	sq_newslot(vm, -3, false);


	return GLA_SUCCESS;
}

