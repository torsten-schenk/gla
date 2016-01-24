#pragma once

#include "common.h"

/* TODO put this into rt.c and use functions to access members */
struct gla_rt {
	gla_store_t *store;

	HSQUIRRELVM vm;
	bool gather_descriptions;
	bool shutdown;
	gla_packreg_t *reg_initialized;
	gla_entityreg_t *reg_imported;
	gla_packreg_t *reg_mounted;
	gla_mount_t *mnt_internal;
	gla_mount_t *mnt_registry;
	gla_mount_t *mnt_stdio;
	gla_bridge_t *bridge;
	btree_t *data; /* used by gla_rt_data_X methods */
	apr_pool_t *msuper; /* the memory pool that will persist until the program exits */
	apr_pool_t *mpool; /* the memory pool that persists until the runtime is destroyed */
	apr_pool_t *mpstack;
	int refcnt;
};

