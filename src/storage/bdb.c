#include <stdlib.h>
#include <btree/bdb.h>
#include <assert.h>

#include "../log.h"
#include "../rt.h"

#include "_table.h"

#include "bdb.h"

#define METATABLE_DB_NAME "metatable.db"
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

enum {
	SIZE_VAR = 8,/* CAUTION: must be >= other sizes */
	SIZE_INT = 8,
	SIZE_FLOAT = 8,
	SIZE_BOOL = 1,
	SIZE_REC = 4,
	SIZE_FLAGS = 4,
	SIZE_TYPE = 1
};

enum {
	OFF_COLSPEC_MAGIC = 0,
	OFF_COLSPEC_NKEY = OFF_COLSPEC_MAGIC + SIZE_INT,
	OFF_COLSPEC_NTOTAL = OFF_COLSPEC_NKEY + SIZE_INT,
	SIZE_COLSPEC = OFF_COLSPEC_NTOTAL + SIZE_INT
};

enum {
	OFF_COLSPEC_COL_TYPE = 0,
	OFF_COLSPEC_COL_FLAGS = OFF_COLSPEC_COL_TYPE + SIZE_TYPE,
	SIZE_COLSPEC_COL = OFF_COLSPEC_COL_FLAGS + SIZE_FLAGS
};

enum {
	REC_NULL = 0
};

enum {
	CELL_EMPTY = 0,
	CELL_INTERNAL,
	CELL_EXTERNAL
};

#define OFF_COLSPEC_COL(I) (SIZE_COLSPEC + (I) * SIZE_COLSPEC_COL)

typedef struct table table_t;

typedef struct {
	int offset;
	int state;
	HSQOBJECT object;
} coldata_t;

typedef struct {
	abstract_meta_t super;
} meta_t;

typedef struct {
	abstract_database_t super;
	DB_ENV *env;
	DB *metatable;
	apr_pool_t *pool;
	bdb_store_t *store;
	apr_hash_t *store_cache_pri; /* store record (db_recno_t) -> squirrel string (HSQOBJECT) */
	apr_hash_t *store_cache_sec; /* squirrel string (HSQOBJECT) -> store record (db_recno_t) */
} database_t;

typedef struct {
	abstract_builder_t super;
	int order;
} builder_t;

struct table {
	abstract_table_t super;
	colspec_t *colspec;
	bdb_btree_t *btree;
	coldata_t *coldata; /* array size: colspec->n_total + 1; last element contains value 'row_size' */

	int row_size;
	char *editable_row;
	bdb_btree_it_t editable_it;
};

typedef struct {
	abstract_iterator_t super;

	bdb_btree_it_t it;
} it_t;

static void buffer_set_bool(
		char *buffer,
		int offset,
		bool value)
{
	memcpy(buffer + offset, &value, SIZE_BOOL);
}

static bool buffer_get_bool(
		const char *buffer,
		int offset)
{
	bool value;
	memcpy(&value, buffer + offset, SIZE_BOOL);
	return value;
}

static void buffer_set_int(
		char *buffer,
		int offset,
		int64_t value)
{
	value = htobe64(value);
	memcpy(buffer + offset, &value, SIZE_INT);
}

static int64_t buffer_get_int(
		const char *buffer,
		int offset)
{
	int64_t value;
	memcpy(&value, buffer + offset, SIZE_INT);
	return be64toh(value);
}

static void buffer_set_rec(
		char *buffer,
		int offset,
		db_recno_t value)
{
	assert(sizeof(db_recno_t) == SIZE_REC); /* TODO db_recno_t requires endian conversion? See also in get() */
	memcpy(buffer + offset, &value, SIZE_REC);
}

static db_recno_t buffer_get_rec(
		const char *buffer,
		int offset)
{
	db_recno_t value;
	assert(sizeof(db_recno_t) == SIZE_REC);
	memcpy(&value, buffer + offset, SIZE_REC);
	return value;
}

static void buffer_set_type(
		char *buffer,
		int offset,
		uint8_t type)
{
	buffer[offset] = type;
}

static uint8_t buffer_get_type(
		const char *buffer,
		int offset)
{
	return buffer[offset];
}

static void buffer_set_flags(
		char *buffer,
		int offset,
		uint32_t value)
{
	value = htobe32(value);
	memcpy(buffer + offset, &value, SIZE_FLAGS);
}

static uint32_t buffer_get_flags(
		const char *buffer,
		int offset)
{
	uint32_t value;
	memcpy(&value, buffer + offset, SIZE_FLAGS);
	return be32toh(value);
}

static inline bool is_store_type(
		int type)
{
	return type == COL_STRING;
}

static int store_addref(
		bdb_store_t *store,
		char *data,
		int coltype,
		int amount)
{
	int ret = GLA_SUCCESS;
	db_recno_t recno = REC_NULL;

	if(is_store_type(coltype))
		recno = buffer_get_rec(data, 0);
	else if(coltype == COL_VARIANT && is_store_type(buffer_get_type(data, 0)))
		recno = buffer_get_rec(data, SIZE_TYPE);
	if(recno != REC_NULL) {
		if(amount < 0)
			ret = bdb_store_release(store, NULL, recno, -amount);
		else if(amount > 0)
			ret = bdb_store_acquire(store, NULL, recno, amount);
	}
	return ret;
}

static int cb_cmp(
		const void *a_,
		const void *b_,
		void *table_)
{
	table_t *table = table_;

	return memcmp(a_, b_, table->coldata[table->colspec->n_key].offset);
}

static int cb_cmp_set(
		const void *a_,
		const void *b_,
		void *table_,
		void *kcols_)
{
	table_t *table = table_;
	int *kcols = kcols_;

	return memcmp(a_, b_, table->coldata[*kcols].offset);
}

static int cb_acquire(
		void *a_,
		void *table_)
{
	int i;
	int ret;
	table_t *table = table_;
	database_t *db = (database_t*)table->super.db;
	colspec_t *colspec = table->colspec;

	for(i = 0; i < colspec->n_total; i++) {
		ret = store_addref(db->store, table->editable_row + table->coldata[i].offset, colspec->column[i].type, 1);
		if(ret != 0)
			return ret;
	}
	return 0;
}

static void cb_release(
		void *a_,
		void *table_)
{
	int i;
	table_t *table = table_;
	database_t *db = (database_t*)table->super.db;
	colspec_t *colspec = table->colspec;

	for(i = 0; i < colspec->n_total; i++)
		store_addref(db->store, table->editable_row + table->coldata[i].offset, colspec->column[i].type, -1);
}

static int cell_deserialize(
		table_t *table,
		HSQOBJECT *target,
		const char *source,
		int type)
{
	int ret;
	SQInteger vint;
	SQBool vbool;
	db_recno_t rec;
	int size;
	char *tmp;
	database_t *db = (database_t*)table->super.db;
	gla_rt_t *rt = table->super.meta->rt;
	HSQUIRRELVM vm = table->super.meta->rt->vm;

	if(type == COL_VARIANT) {
		type = buffer_get_type(source, 0);
		source += SIZE_TYPE;
	}
	switch(type) {
		case COL_INTEGER:
			vint = buffer_get_int(source, 0);
			sq_pushinteger(vm, vint);
			sq_getstackobj(vm, -1, target);
			sq_poptop(vm);
			break;
		case COL_STRING:
			/* TODO empty vs. null string; empty data cannot be stored */
			rec = buffer_get_rec(source, 0);
			if(rec == REC_NULL) {
				sq_pushstring(vm, "", -1);
				sq_getstackobj(vm, -1, target);
				sq_addref(vm, target);
				sq_poptop(vm);
			}
			else {
				size = bdb_store_size(db->store, NULL, rec);
				if(size == 0) {
					LOG_ERROR("Error getting entry size from store");
					return errno;
				}
				tmp = apr_palloc(rt->mpstack, size);
				assert(tmp != NULL);
				ret = bdb_store_data(db->store, NULL, rec, tmp, 0, size);
				if(ret != 0) {
					LOG_ERROR("Error getting entry data from store");
					return ret;
				}
				sq_pushstring(vm, tmp, size);
				sq_getstackobj(vm, -1, target);
				sq_addref(vm, target);
				sq_poptop(vm);
			}
			break;
		case COL_BOOL:
			vbool = buffer_get_bool(source, 0);
			sq_pushbool(vm, vbool);
			sq_getstackobj(vm, -1, target);
			sq_poptop(vm);
			break;
			break;
		case COL_NULL:
			sq_pushnull(vm);
			sq_getstackobj(vm, -1, target);
			sq_poptop(vm);
			break;
		default:
			LOG_ERROR("Unsupported or invalid column type in colspec");
			return GLA_INTERNAL;
	}
	return GLA_SUCCESS;
}

static int cell_serialize(
		table_t *table,
		char *target,
		const HSQOBJECT *source,
		int type,
		bool try)
{
	database_t *db = (database_t*)table->super.db;
	SQInteger vint;
	SQBool vbool;
	const SQChar *vstr;
	db_recno_t rec;
	HSQUIRRELVM vm = table->super.meta->rt->vm;
	/* TODO empty vs. null string; empty data cannot be stored */

	sq_pushobject(vm, *source);
	if(type == COL_VARIANT) {
		memset(target + SIZE_TYPE, 0, SIZE_VAR);
		switch(sq_gettype(vm, -1)) {
			case OT_INTEGER:
				type = COL_INTEGER;
				break;
			case OT_STRING:
				type = COL_STRING;
				break;
			case OT_NULL:
				type = COL_NULL;
				break;
			case OT_BOOL:
				type = COL_BOOL;
				break;
			default:
				sq_poptop(vm);
				LOG_ERROR("Unsupported or invalid cell value type for variant");
				return GLA_NOTSUPPORTED;
		}
		buffer_set_type(target, 0, type);
		target += SIZE_TYPE;
	}
	switch(type) {
		case COL_INTEGER:
			assert(sq_gettype(vm, -1) == OT_INTEGER);
			sq_getinteger(vm, -1, &vint);
			buffer_set_int(target, 0, vint);
			break;
		case COL_BOOL:
			assert(sq_gettype(vm, -1) == OT_BOOL);
			sq_getbool(vm, -1, &vbool);
			buffer_set_bool(target, 0, vbool);
			break;
		case COL_STRING:
			assert(sq_gettype(vm, -1) == OT_STRING || sq_gettype(vm, -1) == OT_NULL); /* TODO null vs. empty string */
			if(sq_gettype(vm, -1) == OT_NULL)
				buffer_set_rec(target, 0, REC_NULL);
			else {
				sq_getstring(vm, -1, &vstr);
				if(*vstr == 0)
					rec = REC_NULL;
				else {
					if(try)
						rec = bdb_store_try(db->store, NULL, vstr, strlen(vstr));
					else
						rec = bdb_store_get(db->store, NULL, vstr, strlen(vstr), false);
					if(rec == REC_NULL) {
						sq_poptop(vm);
						if(errno == -ENOENT)
							return GLA_NOTFOUND;
						else {
							LOG_ERROR("Error getting string record from store");
							return GLA_IO;
						}
					}
				}
				buffer_set_rec(target, 0, rec);
			}
			break;
		case COL_NULL:
			break;
		default:
			sq_poptop(vm);
			LOG_ERROR("Unsupported or invalid column type in colspec");
			return GLA_INTERNAL;
	}
	sq_poptop(vm);
	return GLA_SUCCESS;
}

static void row_clear(
		table_t *table)
{
	int i;
	colspec_t *colspec = table->colspec;
	HSQUIRRELVM vm = table->super.meta->rt->vm;

	memset(table->editable_row, 0, table->row_size);
	memset(&table->editable_it, 0, sizeof(bdb_btree_it_t));
	for(i = 0; i < colspec->n_total; i++) {
		sq_release(vm, &table->coldata[i].object);
		memset(&table->coldata[i].object, 0, sizeof(HSQOBJECT));
		table->coldata[i].state = CELL_EMPTY;
	}
}

static void cells_clear(
		table_t *table,
		int loff, /* lower offset (inclusive) */
		int uoff) /* upper offset (exclusive) */
{
	int i;
	HSQUIRRELVM vm = table->super.meta->rt->vm;

	memset(table->editable_row, loff, uoff);
	for(i = loff; i < uoff; i++) {
		sq_release(vm, &table->coldata[i].object);
		memset(&table->coldata[i].object, 0, sizeof(HSQOBJECT));
		table->coldata[i].state = CELL_EMPTY;
	}
}

static int colspec_data_size(
		const colspec_t *colspec,
		coldata_t *coldata)
{
	int size = 0;
	int i;
	for(i = 0; i < colspec->n_total; i++) {
		if(coldata != NULL)
			coldata[i].offset = size;
		switch(colspec->column[i].type) {
			case COL_VARIANT:
				size += SIZE_TYPE + SIZE_VAR;
				break;
			case COL_INTEGER:
				size += SIZE_INT;
				break;
			case COL_FLOAT:
				size += SIZE_FLOAT;
				break;
			case COL_BOOL:
				size += SIZE_BOOL;
				break;
			case COL_STRING:
				size += SIZE_REC; /* TODO optimization: add another parameter 'short_string_size' for string that are stored immediately without using the global store */
				break;
			default:
				LOG_ERROR("Internal error: unsupported column type in column specification: %d", colspec->column[i].type);
				return GLA_INTERNAL;
		}
	}
	return size;
}

static void todbt_key_colspec(
		const char *table_name,
		DBT *dbt,
		apr_pool_t *pool)
{
	int len = strlen(table_name);
	char *data = apr_palloc(pool, len + 1);

	assert(data != NULL);
	memset(dbt, 0, sizeof(*dbt));
	dbt->data = data;
	dbt->size = len + 1;
	data[0] = 0x01;
	memcpy(data + 1, table_name, len);
}

/* COLSPEC SERIALIZATION */
static void todbt_colspec(
		uint64_t magic,
		const colspec_t *colspec,
		DBT *dbt,
		apr_pool_t *pool)
{
	int i;
	const column_t *col;
	int size = SIZE_COLSPEC + colspec->n_total * SIZE_COLSPEC_COL; 
	char *data = apr_palloc(pool, size);
	assert(data != NULL);

	memset(dbt, 0, sizeof(*dbt));
	dbt->data = data;
	dbt->size = size;
	dbt->ulen = size;
	dbt->flags = DB_DBT_USERMEM;

	buffer_set_int(data, OFF_COLSPEC_MAGIC, magic);
	buffer_set_int(data, OFF_COLSPEC_NKEY, colspec->n_key);
	buffer_set_int(data, OFF_COLSPEC_NTOTAL, colspec->n_total);
	for(i = 0; i < colspec->n_total; i++) {
		col = colspec->column + i;
		buffer_set_type(data, OFF_COLSPEC_COL(i) + OFF_COLSPEC_COL_TYPE, col->type);
		buffer_set_flags(data, OFF_COLSPEC_COL(i) + OFF_COLSPEC_COL_FLAGS, col->flags);
	}
}

static int fromdbt_colspec(
		gla_rt_t *rt,
		const DBT *dbt,
		uint64_t *magic,
		colspec_t **colspec)
{
	int ret;
	int i;
	int nkey;
	int ntotal;

	*magic = buffer_get_int(dbt->data, OFF_COLSPEC_MAGIC);
	nkey = buffer_get_int(dbt->data, OFF_COLSPEC_NKEY);
	ntotal = buffer_get_int(dbt->data, OFF_COLSPEC_NTOTAL);
	*colspec = gla_mod_storage_new_colspec(rt, nkey, ntotal);
	if(*colspec == NULL)
		return errno;
	for(i = 0; i < ntotal; i++) {
		ret = gla_mod_storage_colspec_set_type(rt, *colspec, i, buffer_get_type(dbt->data, OFF_COLSPEC_COL(i) + OFF_COLSPEC_COL_TYPE));
		if(ret != GLA_SUCCESS) {
			sq_release(rt->vm, &(*colspec)->thisobj);
			return ret;
		}
		ret = gla_mod_storage_colspec_set_flags(rt, *colspec, i, buffer_get_flags(dbt->data, OFF_COLSPEC_COL(i) + OFF_COLSPEC_COL_FLAGS));
		if(ret != GLA_SUCCESS) {
			sq_release(rt->vm, &(*colspec)->thisobj);
			return ret;
		}
	}
/*	sq_pushobject(rt->vm, (*colspec)->thisobj);
	sq_pushstring(rt->vm, "columns", -1);
	sq_get(rt->vm, -2);
	gla_rt_dump_value(rt, -1, -1, true, NULL);
	sq_poptop(rt->vm);*/
	return GLA_SUCCESS;
}

/* TODO use column data and just store _unVal of HSQOBJECT in btree if type for column is set */

static int m_database_open(
		abstract_database_t *db_,
		int paramidx)
{
	int ret;
	const SQChar *string;
	bool create = false;
	bool rdonly = false;
	bool truncate = false;
	int flags;
	const char *filepath;
	gla_path_t path;
	database_t *db = (database_t*)db_;
	gla_rt_t *rt = db->super.meta->rt;
	gla_mount_t *mount;
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "mode", -1);
	if(!SQ_FAILED(sq_get(vm, paramidx))) {
		if(SQ_FAILED(sq_getstring(vm, -1, &string))) {
			LOG_ERROR("Invalid parameter 'mode': expected string");
			return GLA_VM;
		}
		sq_poptop(vm);
		while(*string != 0) {
			if(*string == 'r')
				rdonly = true;
			else if(*string == 'c')
				create = true;
			else if(*string == 't')
				truncate = true;
			else {
				LOG_ERROR("Invalid parameter 'mode': no such mode flag '%c'", *string);
				return GLA_VM;
			}
			string++;
		}
		if(create && rdonly) {
			LOG_ERROR("Invalid parameter 'mode': cannot specify both 'r' and 'c'");
			return GLA_VM;
		}
		else if(truncate && rdonly) {
			LOG_ERROR("Invalid parameter 'mode': cannot specify both 'r' and 't'");
			return GLA_VM;
		}
	}

	sq_pushstring(vm, "package", -1);
	if(!SQ_FAILED(sq_get(vm, paramidx))) {
		ret = gla_path_get_package(&path, false, rt, -1, rt->mpstack);
		if(ret != GLA_SUCCESS) {
			sq_poptop(vm);
			LOG_ERROR("Invalid parameter 'package': not a valid package string");
			return GLA_VM;
		}
		flags = GLA_MOUNT_FILESYSTEM;
		flags |= GLA_MOUNT_TARGET;
		if(rdonly)
			flags |= GLA_MOUNT_SOURCE;
		if(create)
			flags |= GLA_MOUNT_CREATE;
		mount = gla_rt_resolve(rt, &path, flags, NULL, rt->mpstack);
		if(mount == NULL) {
			sq_poptop(vm);
			LOG_ERROR("Invalid parameter 'package': package not target-mounted on filesystem");
			return GLA_VM;
		}
		filepath = gla_mount_tofilepath(mount, &path, false, rt->mpstack);
		assert(filepath != NULL);
		sq_poptop(vm);
	}
	else {
		LOG_ERROR("Missing parameter 'package'");
		return GLA_VM;
	}

	ret = db_env_create(&db->env, 0);
	if(ret != 0) {
		LOG_ERROR("Error creating dabatase environment structure");
		return GLA_ALLOC;
	}
	flags = DB_INIT_MPOOL;
	if(create)
		flags |= DB_CREATE;
	if(rdonly)
		flags |= DB_RDONLY;
	ret = db->env->open(db->env, filepath, flags, 0);
	if(ret != 0) {
		LOG_ERROR("Error opening database environment");
		ret = GLA_IO;
		goto error_1;
	}
	
	ret = db_create(&db->metatable, db->env, 0);
	if(ret != 0) {
		LOG_ERROR("Error creating database structure");
		ret = GLA_ALLOC;
		goto error_1;
	}
	flags = 0;
	if(create)
		flags |= DB_CREATE;
	if(rdonly)
		flags |= DB_RDONLY;
	if(truncate)
		flags |= DB_TRUNCATE;
	ret = db->metatable->open(db->metatable, NULL, METATABLE_DB_NAME, NULL, DB_BTREE, flags, 0);
	if(ret != 0) {
		LOG_ERROR("Error opening metatable database");
		ret = GLA_IO;
		goto error_2;
	}

	if(truncate) {
		db->store = bdb_store_create(db->env, NULL, "store");
		if(db->store == NULL) {
			LOG_ERROR("Error creating store database");
			ret = errno;
			goto error_2;
		}
	}
	else {
		db->store = bdb_store_open(db->env, NULL, "store", rdonly ? BTREE_RDONLY : 0);
		if(create && db->store == NULL && errno == -ENOENT) {
			db->store = bdb_store_create(db->env, NULL, "store");
			if(db->store == NULL) {
				LOG_ERROR("Error creating store database");
				ret = errno;
				goto error_2;
			}
		}
		else if(db->store == NULL) {
			LOG_ERROR("Error opening store database");
			ret = errno;
			goto error_2;
		}
	}

	if(apr_pool_create_unmanaged(&db->pool) != APR_SUCCESS) {
		LOG_ERROR("Error creating database memory pool");
		goto error_3;
	}
	db->store_cache_pri = apr_hash_make(db->pool);
	if(db->store_cache_pri == NULL) {
		LOG_ERROR("Error creating database primary store cache");
		goto error_4;
	}
	db->store_cache_sec = apr_hash_make(db->pool);
	if(db->store_cache_sec == NULL) {
		LOG_ERROR("Error creating database secondary store cache");
		goto error_4;
	}

	return GLA_SUCCESS;

error_4:
	apr_pool_destroy(db->pool);
error_3:
	bdb_store_destroy(db->store);
error_2:
	db->metatable->close(db->metatable, 0);
	db->metatable = NULL;
error_1:
	db->env->close(db->env, 0);
	db->env = NULL;
	return ret;
}

static int m_database_flush(
		abstract_database_t *db_)
{
	int ret;
	database_t *db = (database_t*)db_;

	ret = db->metatable->sync(db->metatable, 0);
	if(ret != 0) {
		LOG_ERROR("Error flushing metatable database");
		return GLA_IO;
	}
	return GLA_SUCCESS;
}

static int m_database_close(
		abstract_database_t *db_)
{
	database_t *db = (database_t*)db_;
	gla_rt_t *rt = db->super.meta->rt;
	apr_hash_index_t *hi;
	void *val;
	HSQOBJECT *object;

	if(!rt->shutdown) {
		for(hi = apr_hash_first(db->pool, db->store_cache_sec); hi != NULL; hi = apr_hash_next(hi)) {
			apr_hash_this(hi, NULL, NULL, &val);
			object = val;
			sq_release(rt->vm, object);
		}
	}
	apr_pool_destroy(db->pool);
	bdb_store_destroy(db->store);
	db->metatable->close(db->metatable, 0);
	db->env->close(db->env, 0);
	db->metatable = NULL;
	db->env = NULL;

	return GLA_SUCCESS;
}

/* TODO store referencing: when a table is opened with TRUNCATE flag and the table already exists, unreference all store entries used by the currently existing table */
static int m_database_open_table(
		abstract_database_t *db_,
		const char *name,
		uint64_t magic,
		int flags,
		abstract_builder_t *builder_,
		abstract_table_t *table_,
		colspec_t **colspec_)
{
	int ret;
	database_t *db = (database_t*)db_;
	builder_t *builder = (builder_t*)builder_;
	table_t *table = (table_t*)table_;
	int options = 0;
	gla_rt_t *rt = db->super.meta->rt;
	colspec_t *colspec = NULL;
	bool doref;
	DBT dbt_key;
	DBT dbt_data;
	int row_size = -1;

	if(builder != NULL) {
		assert(builder->order >= 3);
		if(builder->super.multikey)
			options |= BTREE_OPT_MULTI_KEY;
		colspec = builder->super.colspec;
		table->coldata = calloc(colspec->n_total + 1, sizeof(coldata_t));
		if(table->coldata == NULL)
			return GLA_ALLOC;
		row_size = colspec_data_size(colspec, table->coldata);
		if(row_size < 0) {
			free(table->coldata);
			return row_size;
		}
		table->coldata[colspec->n_total].offset = row_size;
	}

	memset(&dbt_data, 0, sizeof(dbt_data));
	todbt_key_colspec(name, &dbt_key, rt->mpstack);
	ret = db->metatable->get(db->metatable, NULL, &dbt_key, &dbt_data, 0);
	if(ret == DB_NOTFOUND && (flags & OPEN_CREATE) == 0) {
		free(table->coldata);
		return GLA_NOTFOUND;
	}
	else if(ret == DB_NOTFOUND || (flags & OPEN_TRUNCATE) != 0) {
		todbt_colspec(magic, colspec, &dbt_data, rt->mpstack);
		ret = db->metatable->put(db->metatable, NULL, &dbt_key, &dbt_data, 0);
		if(ret != 0) {
			LOG_ERROR("Error creating metatable entry");
			free(table->coldata);
			return GLA_IO;
		}
		table->btree = bdb_btree_create_user(db->env, NULL, name, -1, builder->order, row_size, colspec->n_key == 0 ? NULL : cb_cmp, cb_acquire, cb_release, options, table);
		if(table->btree == NULL) {
			free(table->coldata);
			return errno;
		}
		doref = true;
	}
	else if(ret == 0 && colspec != NULL) {
		DBT dbt_tmp;
		todbt_colspec(magic, colspec, &dbt_tmp, rt->mpstack);
		if(magic == 0) /* TODO this ignores magic bytes on request. this variant may be a bit hacky; generally: improve colspec comparison */
			buffer_set_int(dbt_data.data, OFF_COLSPEC_MAGIC, 0);
		if(dbt_tmp.size != dbt_data.size || memcmp(dbt_tmp.data, dbt_data.data, dbt_tmp.size) != 0) {
			LOG_ERROR("Given column specification does not match stored table");
			free(table->coldata);
			return GLA_IO;
		}
		table->btree = bdb_btree_create_user(db->env, NULL, name, -1, builder->order, row_size, colspec->n_key == 0 ? NULL : cb_cmp, cb_acquire, cb_release, options, table);
		if(table->btree == NULL) {
			free(table->coldata);
			return errno;
		}
		doref = true;
	}
	else if(ret == 0) {
		ret = fromdbt_colspec(rt, &dbt_data, &magic, &colspec);
		if(ret != GLA_SUCCESS) {
			free(table->coldata);
			return ret;
		}
		table->btree = bdb_btree_open_user(db->env, NULL, name, -1, colspec->n_key == 0 ? NULL : cb_cmp, cb_acquire, cb_release, (flags & OPEN_RDONLY) != 0 ? BTREE_RDONLY : 0, table);
		if(table->btree == NULL) {
			free(table->coldata);
			return errno;
		}
		table->coldata = calloc(colspec->n_total + 1, sizeof(coldata_t));
		if(table->coldata == NULL) {
			bdb_btree_destroy(table->btree);
			free(table->coldata);
			return GLA_ALLOC;
		}
		row_size = colspec_data_size(colspec, table->coldata);
		if(row_size < 0) {
			bdb_btree_destroy(table->btree);
			free(table->coldata);
			return row_size;
		}
		table->coldata[colspec->n_total].offset = row_size;
		doref = false;
	}
	else {
		LOG_ERROR("Error getting entry from metatable");
		free(table->coldata);
		return GLA_VM;
	}
	table->editable_row = calloc(1, row_size);
	if(table->editable_row == NULL) {
		bdb_btree_destroy(table->btree);
		free(table->coldata);
		return GLA_ALLOC;
	}

	table->row_size = row_size;
	table->colspec = colspec;
	if(doref)
		sq_addref(rt->vm, &colspec->thisobj);
	*colspec_ = colspec;

	return GLA_SUCCESS;
}

static int m_database_flush_table(
		abstract_database_t *db_,
		abstract_table_t *table_)
{
	return GLA_SUCCESS;
}

static int m_database_close_table(
		abstract_database_t *db_,
		abstract_table_t *table_)
{
	int i;
	table_t *table = (table_t*)table_;
	colspec_t *colspec = table->colspec;
	gla_rt_t *rt = table->super.meta->rt;

	if(!rt->shutdown) { /* unref only if squirrel is not shutting down */
		sq_release(rt->vm, &colspec->thisobj);
		for(i = 0; i < colspec->n_total; i++)
			sq_release(rt->vm, &table->coldata[i].object);
	}
	bdb_btree_destroy(table->btree);
	free(table->coldata);
	free(table->editable_row);
	return GLA_SUCCESS;
}

static int m_database_table_exists(
		abstract_database_t *db_,
		const char *name)
{
	database_t *db = (database_t*)db_;
	gla_rt_t *rt = db->super.meta->rt;
	int ret;
	DBT dbt_key;

	todbt_key_colspec(name, &dbt_key, rt->mpstack);
	ret = db->metatable->exists(db->metatable, NULL, &dbt_key, 0);
	if(ret == DB_NOTFOUND)
		return 0;
	else if(ret < 0)
		return GLA_IO;
	else
		return 1;
}

static methods_database_t database_methods = {
	.open = m_database_open,
	.flush = m_database_flush,
	.close = m_database_close,
	.open_table = m_database_open_table,
	.flush_table = m_database_flush_table,
	.close_table = m_database_close_table,
	.table_exists = m_database_table_exists
};

/* TODO (also ldru) use store_try method to retrieve store data. Set editable_it.index to -ENOENT if this fails. See fnm_table_idx in interface.c for more details. */
static int m_table_ldrl(
		abstract_table_t *self_,
		int kcols,
		apr_pool_t *tmp)
{
	int i;
	int ret;
	bool match;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;
	HSQOBJECT obj;
	void *element;

	assert(kcols > 0);
	assert(kcols <= colspec->n_key);

	for(i = 0; i < kcols; i++)
		if(self->coldata[i].state == CELL_EXTERNAL) { /* TODO CELL_EMPTY? */
			ret = cell_serialize(self, self->editable_row + self->coldata[i].offset, &self->coldata[i].object, colspec->column[i].type, false); /* TODO current problem: if a string does not exist in store, what to do? create string in store (mind: read-only database)? return an error (mind: find lower should always work, maybe make an exception for store data)? set string record to 0 (mind: returned position would not neccessarily be the position where the row would be inserted, which violates one principle of find lower)? introduce a proper comparison for store items (mind: in many cases unneccessary overhead in cb_cmp; possibly introduce a cache)? */
			if(ret == GLA_NOTFOUND) {
				self->editable_it.index = bdb_btree_size(self->btree, NULL);
				return GLA_NOTFOUND;
			}
			else if(ret != GLA_SUCCESS)
				return GLA_IO;
		}
	if(kcols == colspec->n_key)
		ret = bdb_btree_find_lower(self->btree, NULL, self->editable_row, &self->editable_it);
	else
		ret = bdb_btree_find_lower_set_user(self->btree, NULL, self->editable_row, cb_cmp_set, &kcols, &self->editable_it);
	if(ret < 0)
		return ret;
	element = self->editable_it.element;
	if(element != NULL) {
		match = true;
		for(i = 0; i < colspec->n_key; i++) {
			cell_deserialize(self, &obj, element + self->coldata[i].offset, colspec->column[i].type);
			if(memcmp(&self->coldata[i].object, &obj, sizeof(HSQOBJECT)) != 0) {
				match = false;
				break;
			}
			self->coldata[i].state = CELL_INTERNAL; /* convert value into CELL_INTERNAL as it equals the cell data at editable_row */
		}
		cells_clear(self, i, colspec->n_total); /* clear all non-matching cells */
		memcpy(self->editable_row, element, self->row_size);
	}
	else {
		match = false;
		cells_clear(self, 0, colspec->n_total);
	}
	return match ? GLA_SUCCESS : GLA_NOTFOUND;
}

static int m_table_ldru(
		abstract_table_t *self_,
		int kcols,
		apr_pool_t *tmp)
{
	int i;
	int ret;
	bool match;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;
	HSQOBJECT obj;
	void *element;

	assert(kcols > 0);
	assert(kcols <= colspec->n_key);

	for(i = 0; i < kcols; i++)
		if(self->coldata[i].state == CELL_EXTERNAL) { /* TODO CELL_EMPTY? */
			ret = cell_serialize(self, self->editable_row + self->coldata[i].offset, &self->coldata[i].object, colspec->column[i].type, false); /* TODO current problem: if a string does not exist in store, what to do? create string in store (mind: read-only database)? return an error (mind: find lower should always work, maybe make an exception for store data)? set string record to 0 (mind: returned position would not neccessarily be the position where the row would be inserted, which violates one principle of find lower)? introduce a proper comparison for store items (mind: in many cases unneccessary overhead in cb_cmp; possibly introduce a cache)? */
			if(ret == GLA_NOTFOUND) {
				self->editable_it.index = bdb_btree_size(self->btree, NULL);
				return GLA_NOTFOUND;
			}
			else if(ret != GLA_SUCCESS)
				return GLA_IO;
		}
	if(kcols == colspec->n_key)
		ret = bdb_btree_find_upper(self->btree, NULL, self->editable_row, &self->editable_it);
	else
		ret = bdb_btree_find_upper_set_user(self->btree, NULL, self->editable_row, cb_cmp_set, &kcols, &self->editable_it);
	if(ret < 0)
		return ret;
	element = self->editable_it.element;
	if(element != NULL) {
		match = true;
		for(i = 0; i < colspec->n_key; i++) {
			cell_deserialize(self, &obj, element + self->coldata[i].offset, colspec->column[i].type);
			if(memcmp(&self->coldata[i].object, &obj, sizeof(HSQOBJECT)) != 0) {
				match = false;
				break;
			}
			self->coldata[i].state = CELL_INTERNAL; /* convert value into CELL_INTERNAL as it equals the cell data at editable_row */
		}
		cells_clear(self, i, colspec->n_total); /* clear all non-matching cells */
		memcpy(self->editable_row, element, self->row_size);
	}
	else {
		match = false;
		cells_clear(self, 0, colspec->n_total);
	}
	return match ? GLA_SUCCESS : GLA_NOTFOUND;
}

static int m_table_ldri(
		abstract_table_t *self_,
		int row,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;

	row_clear(self);
	if(row >= bdb_btree_size(self->btree, NULL)) /* TODO where to check index for size? Also check other methods */
		return GLA_NOTFOUND;
	ret = bdb_btree_find_at(self->btree, NULL, row, &self->editable_it);
	if(ret == -ENOENT)
		return GLA_NOTFOUND;
	else if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	assert(self->editable_it.element != NULL);
	memcpy(self->editable_row, self->editable_it.element, self->row_size);
	return GLA_SUCCESS;
}

static int m_table_ldrb(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;

	row_clear(self);
	ret = bdb_btree_find_begin(self->btree, NULL, &self->editable_it);
	if(ret < 0)
		return GLA_IO;
	if(self->editable_it.element != NULL)
		memcpy(self->editable_row, self->editable_it.element, self->row_size);

	return GLA_SUCCESS;
}

static int m_table_ldre(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;

	row_clear(self);
	ret = bdb_btree_find_end(self->btree, NULL, &self->editable_it);
	if(ret < 0)
		return GLA_IO;
	return GLA_SUCCESS;
}

static int m_table_idx(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	return self->editable_it.index;
}

static int m_it_init(
		abstract_iterator_t *it_,
		apr_pool_t *tmp)
{
	it_t *it = (it_t*)it_;
	table_t *self = (table_t*)it->super.table;

	memcpy(&it->it, &self->editable_it, sizeof(bdb_btree_it_t));
	it->super.index = it->it.index;
	return GLA_SUCCESS;
}

static int m_it_mv(
		abstract_iterator_t *it_,
		int amount,
		apr_pool_t *tmp)
{
	int ret;
	it_t *it = (it_t*)it_;
	table_t *table = (table_t*)it->super.table;

	if(amount == 0)
		ret = 0;
/*	else if(amount == 1) TODO uncomment after fixing bug in bdb_btree_iterate_next (possibly also _prev)
		ret = bdb_btree_iterate_next(&it->it, NULL);
	else if(amount == -1)
		ret = bdb_btree_iterate_prev(&it->it, NULL);*/
	else if(it->it.index + amount <= 0)
		ret = bdb_btree_find_begin(table->btree, NULL, &it->it);
	else if(it->it.index + amount >= bdb_btree_size(table->btree, NULL))
		ret = bdb_btree_find_end(table->btree, NULL, &it->it);
	else
		ret = bdb_btree_find_at(table->btree, NULL, it->it.index + amount, &it->it);
	if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	it->super.index = it->it.index;
	return GLA_SUCCESS;
}

static int m_it_upd(
		abstract_iterator_t *it_,
		apr_pool_t *tmp)
{
	it_t *it = (it_t*)it_;
	table_t *table = (table_t*)it->super.table;

	memcpy(&it->it, &table->editable_it, sizeof(btree_it_t));
	it->super.index = it->it.index;
	return GLA_SUCCESS;
}

static int m_it_ldr(
		abstract_iterator_t *it_,
		apr_pool_t *tmp)
{
	int ret;
	it_t *it = (it_t*)it_;
	table_t *table = (table_t*)it->super.table;

	row_clear(table);
	if(it->it.element == NULL)
		return GLA_NOTFOUND;
	memcpy(&table->editable_it, &it->it, sizeof(bdb_btree_it_t));
	ret = bdb_btree_iterate_refresh(&table->editable_it, NULL);
	if(ret != 0) {
		LOG_ERROR("Error refreshing iterator");
		return GLA_IO;
	}
	memcpy(table->editable_row, table->editable_it.element, table->row_size);
	return GLA_SUCCESS;
}

static int m_it_dup(
		abstract_iterator_t *dst_,
		abstract_iterator_t *src_,
		apr_pool_t *tmp)
{
	it_t *dst = (it_t*)dst_;
	it_t *src = (it_t*)src_;

	memcpy(&dst->it, &src->it, sizeof(bdb_btree_it_t));
	return GLA_SUCCESS;
}

static int m_table_str(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int i;
	int ret;
	table_t *self = (table_t*)self_;
	colspec_t *colspec = self->colspec;

	for(i = 0; i < colspec->n_total; i++) {
		if(self->coldata[i].state == CELL_EXTERNAL) {
			ret = cell_serialize(self, self->editable_row + self->coldata[i].offset, &self->coldata[i].object, colspec->column[i].type, false);
			if(ret != 0)
				return GLA_IO;
		}
		else if(self->coldata[i].state == CELL_EMPTY) {
			assert(false); /* TODO what to do here? */
		}
	}
	ret = bdb_btree_update(self->btree, NULL, self->editable_it.index, self->editable_row);
	if(ret != 0) {
		LOG_ERROR("Error storing row using key");
		return ret;
	}
	return GLA_SUCCESS;
}

static int m_table_mkrk(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int i;
	int ret;
	table_t *self = (table_t*)self_;
	colspec_t *colspec = self->colspec;

	for(i = 0; i < colspec->n_total; i++) {
		if(self->coldata[i].state == CELL_EXTERNAL) {
			ret = cell_serialize(self, self->editable_row + self->coldata[i].offset, &self->coldata[i].object, colspec->column[i].type, false);
			if(ret != 0)
				return GLA_IO;
		}
		else if(self->coldata[i].state == CELL_EMPTY) {
			assert(false); /* TODO what to do here? */
		}
	}
	ret = bdb_btree_insert(self->btree, NULL, self->editable_row);
	if(ret != 0) {
		LOG_ERROR("Error storing row using key");
		return ret;
	}
	return GLA_SUCCESS;
}

static int m_table_mkri(
		abstract_table_t *self_,
		int row,
		apr_pool_t *tmp)
{
	int i;
	int ret;
	table_t *self = (table_t*)self_;
	colspec_t *colspec = self->colspec;

	for(i = 0; i < colspec->n_total; i++) {
		if(self->coldata[i].state == CELL_EXTERNAL) {
			ret = cell_serialize(self, self->editable_row + self->coldata[i].offset, &self->coldata[i].object, colspec->column[i].type, false);
			if(ret != 0)
				return GLA_IO;
		}
		else if(self->coldata[i].state == CELL_EMPTY) {
			assert(false); /* TODO what to do here? */
		}
	}
	ret = bdb_btree_insert_at(self->btree, NULL, row, self->editable_row);
	if(ret != 0) {
		LOG_ERROR("Error storing row using key");
		return ret;
	}
	return GLA_SUCCESS;
}

static int m_table_rmr(
		abstract_table_t *self_,
		int row,
		int n,
		apr_pool_t *tmp)
{
	int ret;
	int size;
	int i;
	table_t *self = (table_t*)self_;

	assert(self->editable_it.element == NULL);

	size = bdb_btree_size(self->btree, NULL);
	if(size < 0) {
		LOG_ERROR("Error getting btree size");
		return GLA_INTERNAL; /* TODO error handling */
	}
	assert(row >= 0);
	assert(n >= 0);
	if(row >= size)
		return 0;
	if(n + row > size)
		n = size - row;
	for(i = 0; i < n; i++) {
		ret = bdb_btree_remove_at(self->btree, NULL, row);
		if(ret != 0) {
			LOG_ERROR("Error removing row");
			return GLA_IO; /* TODO error handling */
		}
	}
	return n;
}

static int m_table_clr(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	row_clear(self);
	return GLA_SUCCESS;
}

static int m_table_ptc(
		abstract_table_t *self_,
		int cell,
		int idx,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	colspec_t *colspec = self->colspec;
	HSQUIRRELVM vm = self->super.meta->rt->vm;

	assert(cell >= 0);
	assert(cell < colspec->n_total);

	sq_release(vm, &self->coldata[cell].object);
	sq_getstackobj(vm, idx, &self->coldata[cell].object);
	sq_addref(vm, &self->coldata[cell].object);
	self->coldata[cell].state = CELL_EXTERNAL;
	return GLA_SUCCESS;
}

static int m_table_gtc(
		abstract_table_t *self_,
		int cell,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	const colspec_t *colspec = self->colspec;

	assert(cell >= 0);
	assert(cell < colspec->n_total);

	if(self->coldata[cell].state == CELL_EMPTY) {
		ret = cell_deserialize(self, &self->coldata[cell].object, self->editable_row + self->coldata[cell].offset, colspec->column[cell].type);
		if(ret != GLA_SUCCESS)
			return ret;
		self->coldata[cell].state = CELL_INTERNAL;
	}
	sq_pushobject(vm, self->coldata[cell].object);
	return GLA_SUCCESS;
}

static int m_table_sz(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	return bdb_btree_size(self->btree, NULL);
}

static methods_table_t table_methods = {
	.ldrl = m_table_ldrl,
	.ldru = m_table_ldru,
	.ldri = m_table_ldri,
	.ldrb = m_table_ldrb,
	.ldre = m_table_ldre,
	.idx = m_table_idx,
	.str = m_table_str,
	.mkrk = m_table_mkrk,
	.mkri = m_table_mkri,
	.rmr = m_table_rmr,
	.clr = m_table_clr,
	.ptc = m_table_ptc,
	.gtc = m_table_gtc,
	.sz = m_table_sz
};

static methods_iterator_t iterator_methods = {
	.init = m_it_init,
	.mv = m_it_mv,
	.upd = m_it_upd,
	.ldr = m_it_ldr,
	.dup = m_it_dup
};

static int m_builder_init(
		abstract_builder_t *self_)
{
	builder_t *self = (builder_t*)self_;
	self->order = 10;
	return GLA_SUCCESS;
}

static int m_builder_set(
		abstract_builder_t *self_,
		const char *key,
		int validx)
{
	builder_t *self = (builder_t*)self_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;

	if(strcmp(key, "order") == 0) {
		SQInteger value;
		if(SQ_FAILED(sq_getinteger(vm, validx, &value))) {
			LOG_ERROR("Invalid value for 'order': expected integer");
			return GLA_VM;
		}
		else if(value < 3) {
			LOG_ERROR("Invalid value for 'order': must be >= 3");
			return GLA_VM;
		}
		self->order = value;
	}
	else
		return GLA_NOTFOUND;
	return GLA_SUCCESS;
}

static int m_builder_get(
		abstract_builder_t *self_,
		const char *key)
{
	builder_t *self = (builder_t*)self_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;

	if(strcmp(key, "order") == 0)
		sq_pushinteger(vm, self->order);
	else
		return GLA_NOTFOUND;
	return GLA_SUCCESS;
}

static methods_builder_t builder_methods = {
	.init = m_builder_init,
	.set = m_builder_set,
	.get = m_builder_get
};

static SQInteger fn_cbridge(
		HSQUIRRELVM vm)
{
	meta_t *meta;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	meta = apr_pcalloc(rt->mpool, sizeof(meta_t));
	if(meta == NULL)
		return gla_rt_vmthrow(rt, "Error allocating meta data");

	ret = gla_mod_storage_meta_init(rt, &meta->super, &database_methods, &builder_methods, &table_methods, &iterator_methods, sizeof(database_t), sizeof(builder_t), sizeof(table_t), sizeof(it_t));
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Error initializing bdb storage cbridge");

	return gla_rt_vmsuccess(rt, false);
}

int gla_mod_storage_bdb_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "bdb", -1);
	sq_newclosure(vm, fn_cbridge, 0);
	sq_newslot(vm, -3, false);

	return 0;
}

