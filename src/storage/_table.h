#pragma once

#include "_colspec.h"

#define MAX_TABLE_NAME_LEN 128
bool gla_mod_storage_table_valid_name(
		const char *name);

typedef struct methods_database methods_database_t;
typedef struct abstract_table abstract_table_t;
typedef struct methods_table methods_table_t;
typedef struct methods_iterator methods_iterator_t;
typedef struct abstract_iterator abstract_iterator_t;
typedef struct abstract_builder abstract_builder_t;
typedef struct methods_builder methods_builder_t;
typedef struct abstract_meta abstract_meta_t;
typedef struct abstract_database abstract_database_t;

struct abstract_iterator {
	const abstract_meta_t *meta;
	abstract_table_t *table;

	int index;
};

enum {
	OPEN_CREATE = 0x01,
	OPEN_TRUNCATE = 0x02,
	OPEN_RDONLY = 0x04
};

/* TODO current problem with find_lower and find_upper: if an allocated (i.e. non-sortable) value is given, the returned position would not
 * neccessarily be the position of a potential insert of the same row. See also the todo-comment in file bdb.c in function m_table_ldrl */
/* Note: Caller is responsible that all prerequisites mentioned here are fulfilled. */
/* Note: Loading rows is allowed if either no row is loaded or the currently loaded row is read-only. */
/* Note on sorting: currently no sorting performed (TODO sort integer, bool, float; string?; what about sq_cmp()? MAYBE introduce column flag; ALSO: use custom compare function per column) */
struct methods_table {
	/* Destroy a handler instance. */
//	void (*destroy)(abstract_table_t *self);
	/* Load row using 'lower' method with previously set key. In case no corresponding row has been found, loading takes place but GLA_NOTFOUND is returned. */
	int (*ldrl)(abstract_table_t *self, int kcols, apr_pool_t *tmp);
	/* Load row using 'upper' method with previously set key. In case no corresponding row has been found, loading takes place but GLA_NOTFOUND is returned. */
	int (*ldru)(abstract_table_t *self, int kcols, apr_pool_t *tmp);
	/* Load row using an index */
	int (*ldri)(abstract_table_t *self, int row, apr_pool_t *tmp);
	/* Load first row */
	int (*ldrb)(abstract_table_t *self, apr_pool_t *tmp);
	/* Load virtual row after last; just the iterator is set, no actual loading takes place */
	int (*ldre)(abstract_table_t *self, apr_pool_t *tmp);
	/* Return index of currently loaded row. If no index could be determined by previous load-operation, -ENOENT is returned. */
	int (*idx)(abstract_table_t *it, apr_pool_t *tmp);
	/* Replace previously loaded edit row. Do not clear edit row. */
	int (*str)(abstract_table_t *self, apr_pool_t *tmp);
	/* Create new row by key using the current edit row. Do not clear edit row. */
	int (*mkrk)(abstract_table_t *self, apr_pool_t *tmp);
	/* Create new row by index using the current edit row. Do not clear edit row. */
	int (*mkri)(abstract_table_t *self, int row, apr_pool_t *tmp);
	/* Remove all rows. Returns number of rows removed. */
	int (*rma)(abstract_table_t *self, apr_pool_t *tmp);
	/* Remove rows. 'row': first row to remove; 'n': number of rows to remove. Returns number of rows removed. */
	int (*rmr)(abstract_table_t *self, int row, int n, apr_pool_t *tmp);
	/* Clear edit row. Changes are not committed. */
	int (*clr)(abstract_table_t *self, apr_pool_t *tmp);
	/* Put a cell into current edit row. */
	int (*ptc)(abstract_table_t *self, int cell, int idx, apr_pool_t *tmp);
	/* Get a cell from current edit row. */
	int (*gtc)(abstract_table_t *self, int cell, apr_pool_t *tmp);
	/* return number of rows */
	int (*sz)(abstract_table_t *self, apr_pool_t *tmp);
};

struct methods_iterator {
	/* Initialize given iterator with iterator for currently loaded row. */
	int (*init)(abstract_iterator_t *it, apr_pool_t *tmp);
	/* Iterator destructor. May be NULL. NOTE: corresponding table may already be destroyed/closed. */
	void (*destroy)(abstract_iterator_t *it);
	/* Move iterator by given amount. May be positive or negative. */
	int (*mv)(abstract_iterator_t *it, int amount, apr_pool_t *tmp);
	/* Update iterator to point to current row. */
	int (*upd)(abstract_iterator_t *it, apr_pool_t *tmp);
	/* Load a row using an iterator */
	int (*ldr)(abstract_iterator_t *it, apr_pool_t *tmp);
	/* Duplicate iterator. No need to call init() on dst  */
	int (*dup)(abstract_iterator_t *dst, abstract_iterator_t *src, apr_pool_t *tmp);
};

struct methods_builder {
	int (*init)(abstract_builder_t *self);
	void (*destroy)(abstract_builder_t *self);
	int (*set)(abstract_builder_t *self, const char *key, int validx);
	int (*get)(abstract_builder_t *self, const char *key);
};

struct methods_database {
	int (*open)(abstract_database_t *db, int paramidx);
	int (*flush)(abstract_database_t *db);
	int (*close)(abstract_database_t *db);
	int (*open_table)(abstract_database_t *db, const char *name, uint64_t magic, int flags, abstract_builder_t *builder, abstract_table_t *table, colspec_t **colspec); /* magic = 0: ignore */
	int (*flush_table)(abstract_database_t *db, abstract_table_t *table);
	int (*close_table)(abstract_database_t *db, abstract_table_t *table); /* return: in case closing did not complete successfully, an error is returned; however, the table will be closed even in an error case */
	int (*table_exists)(abstract_database_t *db, const char *name); /* return: 0, 1 (false, true) or < 0 in case of error */
};

struct abstract_database {
	const abstract_meta_t *meta; /* NULL: closed */
	abstract_table_t *first_open;
	abstract_table_t *last_open;
};

struct abstract_table {
	const abstract_meta_t *meta;
	abstract_database_t *db; /* NULL: closed */
	abstract_table_t *prev_open;
	abstract_table_t *next_open;
};

struct abstract_builder {
	const abstract_meta_t *meta; /* NULL: closed */
	colspec_t *colspec;
	bool multikey;
};

struct abstract_meta {
	const methods_database_t *database_methods;
	const methods_builder_t *builder_methods;
	const methods_table_t *table_methods;
	const methods_iterator_t *iterator_methods;
	gla_rt_t *rt;
	HSQOBJECT database_class;
	HSQOBJECT builder_class;
	HSQOBJECT table_class;
	HSQOBJECT handler_class;
	HSQOBJECT null_object;
	int iterator_size;
};

int gla_mod_storage_builder_augment(
		gla_rt_t *rt,
		int idx);

/* opstack: cbridge, database class, builder class, table class */
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
		int iterator_size);
		
/* note: operand stack must contain table handler instance at position 1 */
/*int abstract_table_init(
		abstract_table_t *self,
		const methods_table_t *methods,
		gla_rt_t *rt,
		int kcols,
		int vcols);

void abstract_table_destroy(
		abstract_table_t *self);
*/

