#include <stdlib.h>
#include <btree/memory.h>
#include <assert.h>

#include "../log.h"
#include "../rt.h"

#include "_table.h"

#include "memory.h"

typedef struct {
	abstract_meta_t super;
} meta_t;

typedef struct {
	abstract_database_t super;
} database_t;

typedef struct {
	abstract_builder_t super;
	int order;
} builder_t;

typedef struct {
	abstract_table_t super;
	colspec_t *colspec;
	btree_t *btree;

	HSQOBJECT *editable_row;
	bool *editable_isset; /* TODO replace with editable_state; state = CELL_EMPTY, CELL_INTERNAL (copied from btree), CELL_EXTERNAL (set using ptc) */
	btree_it_t editable_it;
} table_t;

typedef struct {
	abstract_iterator_t super;

	btree_it_t it;
} it_t;

static int cb_cmp(
		btree_t *btree,
		const void *a_,
		const void *b_,
		void *group)
{
	int n_key = *(int*)(group);
	return memcmp(a_, b_, n_key * sizeof(HSQOBJECT));
}

static int cb_acquire(
		btree_t *btree,
		void *row_)
{
	int i;
	table_t *self = btree_data(btree);
	HSQOBJECT *row = row_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	colspec_t *colspec = self->colspec;

	for(i = 0; i < colspec->n_total; i++)
		sq_addref(vm, row + i);
	return 0;
}

static void cb_release(
		btree_t *btree,
		void *row_)
{
	int i;
	table_t *self = btree_data(btree);
	HSQOBJECT *row = row_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	gla_rt_t *rt = self->super.meta->rt;
	colspec_t *colspec = self->colspec;

	if(!rt->shutdown)
		for(i = 0; i < colspec->n_total; i++)
			sq_release(vm, row + i);
}

/* TODO use this to add another array to table_t which is of size NUM_REFCOUNTED_COLS and contains the indices of all refcounted cols */
/*static inline bool is_refcounted(
		int coltype)
{
	return coltype == COL_STRING || coltype == COL_VARIANT;
}*/
/* Following code create the refcounted columns array

	int i;
	int ncols = 0;
	for(i = 0; i < colspec->n_total; i++)
		if(is_refcounted(colspec->column[i].type))
			ncols++;
	if(ncols > 0) {
		int *col = apr_palloc(table->super.meta->rt->mpstack, ncols * sizeof(int));
		int k;
		btree_it_t it;
		btree_it_t end;

		if(col == NULL) {
			LOG_WARN("Error destroying memory table: cannot allocate column index array");
			goto error;
		}
		k = 0;
		for(i = 0; i < colspec->n_total; i++)
			if(is_refcounted(colspec->column[i].type))
				col[k++] = i;
		assert(k == ncols);
		btree_find_end(table->btree, &end);
		for(btree_find_begin(table->btree, &it); it.index != end.index; btree_iterate_next(&it))
			for(k = 0; k < ncols; k++)
				sq_release(table->super.meta->rt->vm, (HSQOBJECT*)it.element + k);
	}*/

/* TODO use column data and just store _unVal of HSQOBJECT in btree if type for column is set */

static void row_clear(
		table_t *table)
{
	int i;
	const colspec_t *colspec = table->colspec;
	HSQUIRRELVM vm = table->super.meta->rt->vm;

	for(i = 0; i < colspec->n_total; i++)
		if(table->editable_isset[i])
			sq_release(vm, table->editable_row + i);
	memset(table->editable_row, 0, colspec->n_total * sizeof(HSQOBJECT));
	memset(table->editable_isset, 0, colspec->n_total * sizeof(bool));
	memset(&table->editable_it, 0, sizeof(btree_it_t));
}

static void cells_clear(
		table_t *table,
		int lower,
		int upper)
{
	int i;
	HSQUIRRELVM vm = table->super.meta->rt->vm;

	for(i = lower; i < upper; i++)
		if(table->editable_isset[i])
			sq_release(vm, table->editable_row + i);
	memset(table->editable_row + lower, 0, (upper - lower) * sizeof(HSQOBJECT));
	memset(table->editable_isset + lower, 0, (upper - lower) * sizeof(bool));
}

static int m_database_open(
		abstract_database_t *db_,
		int paramidx)
{
	return GLA_SUCCESS;
}

static int m_database_flush(
		abstract_database_t *db_)
{
	return GLA_SUCCESS;
}

static int m_database_close(
		abstract_database_t *db_)
{
	return GLA_SUCCESS;
}

static int m_database_open_table(
		abstract_database_t *db_,
		const char *name,
		uint64_t magic,
		int flags,
		abstract_builder_t *builder_,
		abstract_table_t *table_,
		colspec_t **colspec_)
{
	database_t *db = (database_t*)db_;
	builder_t *builder = (builder_t*)builder_;
	table_t *table = (table_t*)table_;
	int options = 0;
	gla_rt_t *rt = db->super.meta->rt;
	colspec_t *colspec;

	if((flags & OPEN_CREATE) == 0)
		return GLA_NOTFOUND;
	assert(builder->order >= 3);
	colspec = builder->super.colspec;
	if(builder->super.multikey)
		options |= BTREE_OPT_MULTI_KEY;

	table->btree = btree_new(builder->order, sizeof(HSQOBJECT) * colspec->n_total, colspec->n_key == 0 ? NULL : cb_cmp, options);
	if(table->btree == NULL)
		return GLA_ALLOC;
	btree_set_group_default(table->btree, &colspec->n_key);
	btree_set_data(table->btree, table);
	btree_sethook_refcount(table->btree, cb_acquire, cb_release);

	table->editable_row = calloc(colspec->n_total, sizeof(HSQOBJECT));
	if(table->editable_row == NULL) {
		btree_destroy(table->btree);
		return GLA_ALLOC;
	}
	table->editable_isset = calloc(colspec->n_total, sizeof(bool));
	if(table->editable_isset == NULL) {
		free(table->editable_row);
		btree_destroy(table->btree);
		return GLA_ALLOC;
	}

	table->colspec = colspec;
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
	table_t *table = (table_t*)table_;
	colspec_t *colspec = table->colspec;
	gla_rt_t *rt = table->super.meta->rt;

	if(!rt->shutdown)
		sq_release(rt->vm, &colspec->thisobj);

	btree_destroy(table->btree);
	free(table->editable_row);
	free(table->editable_isset);
	return GLA_SUCCESS;
}

static int m_database_table_exists(
		abstract_database_t *db_,
		const char *name)
{
	return 0;
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

static int m_table_ldrl(
		abstract_table_t *self_,
		int kcols,
		apr_pool_t *tmp)
{
	int ret;
	bool match;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;
	void *element;

	assert(kcols > 0);
	assert(kcols <= colspec->n_key);
	ret = btree_find_lower_group(self->btree, self->editable_row, &kcols, &self->editable_it);
	if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	element = self->editable_it.element;
	if(element != NULL) {
		match = cb_cmp(self->btree, element, self->editable_row, &kcols) == 0;
		cells_clear(self, 0, colspec->n_total);
		memcpy(self->editable_row, element, colspec->n_total * sizeof(HSQOBJECT));
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
	int ret;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;

	assert(kcols >= 0);
	assert(kcols <= colspec->n_key);
	ret = btree_find_upper_group(self->btree, self->editable_row, &kcols, &self->editable_it);
	if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	cells_clear(self, 0, colspec->n_total);
	return GLA_SUCCESS;
}

static int m_table_ldri(
		abstract_table_t *self_,
		int row,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;

	assert(row >= 0);

	row_clear(self);
	if(row >= btree_size(self->btree)) /* TODO where to check index for size? Also check other methods */
		return GLA_NOTFOUND;
	ret = btree_find_at(self->btree, row, &self->editable_it);
	if(ret == -ENOENT)
		return GLA_NOTFOUND;
	else if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	assert(self->editable_it.element != NULL);
	memcpy(self->editable_row, self->editable_it.element, colspec->n_total * sizeof(HSQOBJECT));
	return GLA_SUCCESS;
}

static int m_table_ldrb(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;

	row_clear(self);
	ret = btree_find_begin(self->btree, &self->editable_it);
	if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
	if(self->editable_it.element != NULL)
		memcpy(self->editable_row, self->editable_it.element, colspec->n_total * sizeof(HSQOBJECT));
	return GLA_SUCCESS;
}

static int m_table_ldre(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int ret;
	table_t *self = (table_t*)self_;

	row_clear(self);
	ret = btree_find_end(self->btree, &self->editable_it);
	if(ret < 0)
		return GLA_INTERNAL; /* TODO error handling */
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

	memcpy(&it->it, &self->editable_it, sizeof(btree_it_t));
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
	else if(amount == 1)
		ret = btree_iterate_next(&it->it);
	else if(amount == -1)
		ret = btree_iterate_prev(&it->it);
	else if(it->it.index + amount <= 0)
		ret = btree_find_begin(table->btree, &it->it);
	else if(it->it.index + amount >= btree_size(table->btree))
		ret = btree_find_end(table->btree, &it->it);
	else
		ret = btree_find_at(table->btree, it->it.index + amount, &it->it);
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
	it_t *it = (it_t*)it_;
	table_t *table = (table_t*)it->super.table;
	const colspec_t *colspec = table->colspec;

	row_clear(table);
	if(it->it.element == NULL)
		return GLA_NOTFOUND;
	memcpy(&table->editable_it, &it->it, sizeof(btree_it_t));
	memcpy(table->editable_row, table->editable_it.element, colspec->n_total * sizeof(HSQOBJECT));
	return GLA_SUCCESS;
}

static int m_it_dup(
		abstract_iterator_t *dst_,
		abstract_iterator_t *src_,
		apr_pool_t *tmp)
{
	it_t *dst = (it_t*)dst_;
	it_t *src = (it_t*)src_;

	memcpy(&dst->it, &src->it, sizeof(btree_it_t));
	return GLA_SUCCESS;
}

static int m_table_str(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int i;
	HSQOBJECT *cells;
	table_t *self = (table_t*)self_;
	const colspec_t *colspec = self->colspec;
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	
	assert(self->editable_it.element != NULL);
	assert(memcmp(self->editable_it.element, self->editable_row, colspec->n_key * sizeof(HSQOBJECT)) == 0); /* ensure that key columns haven't changed */
	cells = self->editable_it.element;
	for(i = 0; i < colspec->n_key; i++)
		if(self->editable_isset[i]) {
			LOG_ERROR("Error replacing row: key coumn cells have been changed since the row has been loaded");
			return GLA_NOTSUPPORTED;
		}
	for(i = colspec->n_key; i < colspec->n_total; i++)
		if(self->editable_isset[i]) {
			sq_release(vm, cells + i);
			sq_addref(vm, self->editable_row + i);
			memcpy(cells + i, self->editable_row + i, sizeof(HSQOBJECT));
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
	const colspec_t *colspec = self->colspec;
	
	for(i = colspec->n_key; i < colspec->n_total; i++)
		if(!self->editable_isset[i])
			memcpy(self->editable_row + i, &self->super.meta->null_object, sizeof(HSQOBJECT));
	ret = btree_insert(self->btree, self->editable_row);
	if(ret == -EALREADY) {
		LOG_ERROR("Error storing row using key: entry with given key already exists");
		return GLA_INTERNAL; /* TODO error handling */
	}
	else if(ret != 0) {
		LOG_ERROR("Error storing row using key");
		return GLA_INTERNAL; /* TODO error handling */
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
	const colspec_t *colspec = self->colspec;

	for(i = colspec->n_key; i < colspec->n_total; i++)
		if(!self->editable_isset[i])
			memcpy(self->editable_row + i, &self->super.meta->null_object, sizeof(HSQOBJECT));
	ret = btree_insert_at(self->btree, row, self->editable_row);
	if(ret != 0) {
		LOG_ERROR("Error storing row using index");
		return GLA_INTERNAL; /* TODO error handling */
	}
	return GLA_SUCCESS;
}

static int m_table_rma(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	int size;
	table_t *self = (table_t*)self_;

	assert(self->editable_it.element == NULL);
	size = btree_size(self->btree);
	btree_clear(self->btree);

	return size;
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

	size = btree_size(self->btree);
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
		ret = btree_remove_at(self->btree, row);
		if(ret != 0) {
			LOG_ERROR("Error removing row");
			return GLA_INTERNAL; /* TODO error handling */
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
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	const colspec_t *colspec = self->colspec;

	assert(cell >= 0);
	assert(cell < colspec->n_total);
	if(self->editable_isset[cell]) {
		sq_release(vm, self->editable_row + cell);
		self->editable_isset[cell] = false;
	}
	if(SQ_FAILED(sq_getstackobj(vm, idx, self->editable_row + cell))) {
		LOG_ERROR("Error getting cell value from stack");
		return GLA_VM;
	}
	self->editable_isset[cell] = true;
	sq_addref(vm, self->editable_row + cell);
	return GLA_SUCCESS;
}

static int m_table_gtc(
		abstract_table_t *self_,
		int cell,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	HSQUIRRELVM vm = self->super.meta->rt->vm;
	const colspec_t *colspec = self->colspec;

	assert(cell >= 0);
	assert(cell < colspec->n_total);
	sq_pushobject(vm, self->editable_row[cell]);
	return GLA_SUCCESS;
}

static int m_table_sz(
		abstract_table_t *self_,
		apr_pool_t *tmp)
{
	table_t *self = (table_t*)self_;
	return btree_size(self->btree);
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
	.rma = m_table_rma,
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
		return gla_rt_vmthrow(rt, "Error initializing memory storage cbridge");

	return gla_rt_vmsuccess(rt, false);
}

int gla_mod_storage_memory_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "memory", -1);
	sq_newclosure(vm, fn_cbridge, 0);
	sq_newslot(vm, -3, false);

	return 0;
}

