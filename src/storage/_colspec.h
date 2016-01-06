#pragma once

#include "common.h"

/* when performing changes in this file, search for COLSPEC SERIALIZATION
 * in sources and modify code there */
enum {
	COL_NULL = 0, /* if column type is COL_VARIANT, a single cell can use COL_NULL to indicate that the NULL value is stored within this cell */
	COL_VARIANT = 0,
	COL_INTEGER,
	COL_FLOAT,
	COL_BOOL,
	COL_STRING
};

enum {
	COL_OPT_SORT = 0x00000001
};

typedef struct {
	int type;
	int flags;
} column_t;

typedef struct {
	HSQOBJECT thisobj; /* initially not referenced; ref this is pointer is acquired */
	int n_key;
	int n_total;
	column_t *column;
} colspec_t;

/* CAUTION: must call sq_release(vm, colspec->thisobj) after created column spec is no longer required/used within squirrel environment */
colspec_t *gla_mod_storage_new_colspec(
		gla_rt_t *rt,
		int nkey,
		int ntotal);

int gla_mod_storage_colspec_set_type(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		int type);

int gla_mod_storage_colspec_set_flags(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		int flags);

int gla_mod_storage_colspec_set_name(
		gla_rt_t *rt,
		colspec_t *colspec,
		int col,
		const char *name);

