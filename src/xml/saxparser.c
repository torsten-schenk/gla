#include <expat.h>

#include "../rt.h"

#include "saxparser.h"

#define BUFFER_SIZE 65536

enum { /* operand stack indices for fn_parse */
	PARSE_OP_THIS = 2,
	PARSE_OP_BEGIN = 4,
	PARSE_OP_END,
	PARSE_OP_ATTRIBUTE,
	PARSE_OP_TEXT
};

typedef struct {
	int status; /* GLA_SUCCESS, GLA_VM (error in cb handler), GLA_END (parsing aborted by handler) */
	HSQUIRRELVM vm;
} context_t;

static apr_status_t cleanup_parser(
		void *parser_)
{
	XML_ParserFree((XML_Parser)(parser_));
	return APR_SUCCESS;
}

static void push_ns_name(
		HSQUIRRELVM vm,
		const char *full)
{
	const char *ns = NULL;
	const char *name;

	name = strchr(full, ':');
	if(name != NULL) {
		name++;
		ns = full;
	}
	else
		name = full;
	if(ns == NULL)
		sq_pushnull(vm);
	else
		sq_pushstring(vm, ns, name - ns - 1);
	sq_pushstring(vm, name, -1);
}

static void sax_begin(
		void *ctx_,
		const char *element,
		const char **attributes)
{
	SQBool resume;
	context_t *ctx = ctx_;

	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_BEGIN);
		sq_push(ctx->vm, PARSE_OP_THIS);
		push_ns_name(ctx->vm, element);
		if(SQ_FAILED(sq_call(ctx->vm, 3, true, true))) {
			sq_pushnull(ctx->vm); /* the missing return value */
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
	while(ctx->status == GLA_SUCCESS && *attributes != NULL) {
		const char *key = attributes[0];
		const char *value = attributes[1];
		sq_push(ctx->vm, PARSE_OP_ATTRIBUTE);
		sq_push(ctx->vm, PARSE_OP_THIS);
		push_ns_name(ctx->vm, key);
		sq_pushstring(ctx->vm, value, -1);
		if(SQ_FAILED(sq_call(ctx->vm, 4, true, true))) {
			sq_pushnull(ctx->vm); /* the missing return value */
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
		attributes += 2;
	}
	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_ATTRIBUTE);
		sq_push(ctx->vm, PARSE_OP_THIS);
		sq_pushnull(ctx->vm);
		sq_pushnull(ctx->vm);
		sq_pushnull(ctx->vm);
		if(SQ_FAILED(sq_call(ctx->vm, 4, true, true))) {
			sq_pushnull(ctx->vm); /* the missing return value */
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
}

static void sax_end(
		void *ctx_,
		const char *element)
{
	SQBool resume;
	context_t *ctx = ctx_;

	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_END);
		sq_push(ctx->vm, PARSE_OP_THIS);
		push_ns_name(ctx->vm, element);
		if(SQ_FAILED(sq_call(ctx->vm, 3, true, true))) {
			sq_pushnull(ctx->vm); /* the missing return value */
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
}

static void sax_text(
		void *ctx_,
		const char *text,
		int len)
{
	SQBool resume;
	context_t *ctx = ctx_;

	if(ctx->status == GLA_SUCCESS) {
		sq_push(ctx->vm, PARSE_OP_TEXT);
		sq_push(ctx->vm, PARSE_OP_THIS);
		sq_pushstring(ctx->vm, text, len);
		if(SQ_FAILED(sq_call(ctx->vm, 2, true, true))) {
			sq_pushnull(ctx->vm); /* the missing return value */
			ctx->status = GLA_VM;
		}
		else if(!SQ_FAILED(sq_getbool(ctx->vm, -1, &resume)) && !resume)
			ctx->status = GLA_END;
		sq_pop(ctx->vm, 2);
	}
}

static SQInteger fn_parse(
		HSQUIRRELVM vm)
{
	int ret;
	gla_path_t path;
	gla_mount_t *mnt;
	gla_io_t *io;
	int shifted;
	int bytes;
	void *buffer;
	XML_Parser parser;
	context_t ctx;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");

	sq_pushstring(vm, "begin", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'end'");
	sq_pushstring(vm, "end", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'attribute'");
	sq_pushstring(vm, "attribute", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'text'");
	sq_pushstring(vm, "text", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'begin'");

	ret = gla_path_get_entity(&path, false, rt, 3, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Invalid entity given");
	if(path.extension == NULL)
		path.extension = "xml";

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_SOURCE, &shifted, rt->mpstack);
	if(mnt == NULL)
		return gla_rt_vmthrow(rt, "No such entity");

 	parser = XML_ParserCreate(NULL);
	if(parser == NULL)
		return gla_rt_vmthrow(rt, "Error creating xml parser");
	apr_pool_cleanup_register(rt->mpstack, parser, cleanup_parser, apr_pool_cleanup_null);
	XML_SetElementHandler(parser, sax_begin, sax_end);
	XML_SetCharacterDataHandler(parser, sax_text);
	XML_SetUserData(parser, &ctx);
	memset(&ctx, 0, sizeof(ctx));
	ctx.status = GLA_SUCCESS;
	ctx.vm = vm;

	io = gla_mount_open(mnt, &path, GLA_MODE_READ, rt->mpstack);
	if(io == NULL)
		return gla_rt_vmthrow(rt, "Error opening entity");
	while(gla_io_rstatus(io) == GLA_SUCCESS) {
		buffer = XML_GetBuffer(parser, BUFFER_SIZE);
		if(buffer == NULL)
			return gla_rt_vmthrow(rt, "Error getting parser buffer");
		bytes = gla_io_read(io, buffer, BUFFER_SIZE);
		ret = XML_ParseBuffer(parser, bytes, false);
		if(ret == 0)
			return gla_rt_vmthrow(rt, "XML Error at line %d, column %d: %s", (int)XML_GetCurrentLineNumber(parser), (int)XML_GetCurrentColumnNumber(parser), XML_ErrorString(XML_GetErrorCode(parser)));
		else if(ctx.status == GLA_VM)
			return gla_rt_vmthrow(rt, "Error parsing xml");
	}
	if(gla_io_rstatus(io) != GLA_END)
		return gla_rt_vmthrow(rt, "Error reading entity");
	ret = XML_ParseBuffer(parser, 0, true);
	if(ret == 0)
		return gla_rt_vmthrow(rt, "Error in xml entity");
	else if(ctx.status == GLA_VM)
		return gla_rt_vmthrow(rt, "Error parsing xml");
	else
		return gla_rt_vmsuccess(rt, false);
}

int gla_mod_xml_saxparser_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "parse", -1);
	sq_newclosure(vm, fn_parse, 0);
	sq_newslot(vm, -3, false);
	
	return GLA_SUCCESS;
}

