#include <csv.h>

#include "../rt.h"

#include "module.h"

#define BUFFER_SIZE 1024

enum {
	PARSE_OP_THIS = 2,
	PARSE_OP_ENTRY = 4,
	PARSE_OP_END,
	PARSE_OP_BEGIN
};

typedef struct {
	int col;
	int row;
	int status; /* GLA_SUCCESS, GLA_VM (error in cb handler), GLA_END (parsing aborted by handler) */
	HSQUIRRELVM vm;
} context_t;

static void cb_col(void *data, size_t size, void *ctx_) {
	SQBool resume;
	context_t *ctx = ctx_;

	if(ctx->status == GLA_SUCCESS && ctx->col == 0) {
		sq_push(ctx->vm, PARSE_OP_BEGIN);
		sq_push(ctx->vm, PARSE_OP_THIS);
		sq_pushinteger(ctx->vm, ctx->row);
		if(SQ_FAILED(sq_call(ctx->vm, 2, true, true))) {
			sq_pushnull(ctx->vm);
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_ENTRY);
		sq_push(ctx->vm, PARSE_OP_THIS);
		sq_pushinteger(ctx->vm, ctx->col++);
		sq_pushstring(ctx->vm, data, size);
		if(SQ_FAILED(sq_call(ctx->vm, 3, true, true))) {
			sq_pushnull(ctx->vm);
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
}

static void cb_endrow(int unknown, void *ctx_) {

	SQBool resume;
	context_t *ctx = ctx_;

	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_END);
		sq_push(ctx->vm, PARSE_OP_THIS);
		sq_pushinteger(ctx->vm, ctx->row++);
		sq_pushinteger(ctx->vm, ctx->col);
		if(SQ_FAILED(sq_call(ctx->vm, 3, true, true))) {
			sq_pushnull(ctx->vm);
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
	ctx->col = 0;
}

static apr_status_t cleanup_csv(
		void *parser_)
{
	struct csv_parser *parser = parser_;
	csv_free(parser);
	return APR_SUCCESS;
}

static SQInteger fn_parse(
		HSQUIRRELVM vm)
{
	SQInteger delim;
	SQInteger quote;
	int ret;
	gla_path_t path;
	gla_mount_t *mnt;
	gla_io_t *io;
	int shifted;
	int bytes;
	char buffer[BUFFER_SIZE];
	struct csv_parser parser;
	context_t ctx;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 5)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinteger(vm, 4, &delim)))
		return gla_rt_vmthrow(rt, "Invalid argument 3: expected integer");
	else if(SQ_FAILED(sq_getinteger(vm, 5, &quote)))
		return gla_rt_vmthrow(rt, "Invalid argument 4: expected integer");
	else if(csv_init(&parser, CSV_STRICT | CSV_STRICT_FINI) != 0)
		return gla_rt_vmthrow(rt, "Error initializing csv parser");
	sq_pop(vm, 2); /* delim and strict */
	apr_pool_cleanup_register(rt->mpstack, &parser, cleanup_csv, apr_pool_cleanup_null);

	sq_pushstring(vm, "entry", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'entry'");
	sq_pushstring(vm, "end", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'end'");
	sq_pushstring(vm, "begin", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'begin'");


	ret = gla_path_get_entity(&path, false, rt, 3, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Invalid entity given");
	if(path.extension == NULL)
		path.extension = "csv";

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_SOURCE, &shifted, rt->mpstack);
	if(mnt == NULL)
		return gla_rt_vmthrow(rt, "No such entity");
	csv_set_delim(&parser, delim);
	csv_set_quote(&parser, quote);
	io = gla_mount_open(mnt, &path, GLA_MODE_READ, rt->mpstack);
	if(io == NULL)
		return gla_rt_vmthrow(rt, "Error opening entity");
	ctx.vm = rt->vm;
	ctx.status = GLA_SUCCESS;
	ctx.col = 0;
	ctx.row = 0;
	while(gla_io_rstatus(io) == GLA_SUCCESS) {
		bytes = gla_io_read(io, buffer, BUFFER_SIZE);
		ret = csv_parse(&parser, buffer, bytes, cb_col, cb_endrow, &ctx);
		if(ret != bytes)
			return gla_rt_vmthrow(rt, "CSV error: %s", csv_strerror(csv_error(&parser)));
		else if(ctx.status == GLA_VM)
			return gla_rt_vmthrow(rt, "Error from callback");
	}
	if(gla_io_rstatus(io) != GLA_END)
		return gla_rt_vmthrow(rt, "Error reading entity");
	ret = csv_fini(&parser, cb_col, cb_endrow, &ctx);
	if(ret != 0)
		return gla_rt_vmthrow(rt, "CSV error: %s", csv_strerror(csv_error(&parser)));
	else if(ctx.status == GLA_VM)
		return gla_rt_vmthrow(rt, "Error from callback");
	else
		return gla_rt_vmsuccess(rt, false);
}


int gla_mod_csv_cbparser_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "parse", -1);
	sq_newclosure(vm, fn_parse, 0);
	sq_newslot(vm, -3, false);
	
	return GLA_SUCCESS;
}

