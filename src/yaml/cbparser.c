#include <yaml.h>

#include "../rt.h"

#include "cbparser.h"

enum {
	TOKEN_EMPTY,
	TOKEN_STREAM_START,
	TOKEN_STREAM_END,
	TOKEN_VERSION_DIRECTIVE,
	TOKEN_TAG_DIRECTIVE,
	TOKEN_DOC_START,
	TOKEN_DOC_END,
	TOKEN_BLOCKSEQ_START,
	TOKEN_BLOCKMAP_START,
	TOKEN_BLOCK_END,
	TOKEN_FLOWSEQ_START,
	TOKEN_FLOWSEQ_END,
	TOKEN_FLOWMAP_START,
	TOKEN_FLOWMAP_END,
	TOKEN_BLOCK_ENTRY,
	TOKEN_FLOW_ENTRY,
	TOKEN_KEY,
	TOKEN_VALUE,
	TOKEN_ALIAS,
	TOKEN_ANCHOR,
	TOKEN_TAG,
	TOKEN_SCALAR
};

static int handle_noargs(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	if(SQ_FAILED(sq_call(vm, 1, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_alias(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushstring(vm, (SQChar*)token->data.alias.value, -1);
	if(SQ_FAILED(sq_call(vm, 2, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_anchor(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushstring(vm, (SQChar*)token->data.anchor.value, -1);
	if(SQ_FAILED(sq_call(vm, 2, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_tag(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushstring(vm, (SQChar*)token->data.tag.handle, -1);
	sq_pushstring(vm, (SQChar*)token->data.tag.suffix, -1);
	if(SQ_FAILED(sq_call(vm, 3, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_scalar(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushstring(vm, (SQChar*)token->data.scalar.value, -1);
	sq_pushinteger(vm, token->data.scalar.length);
	switch(token->data.scalar.style) { /* TODO use 'context' argument instead of all current arguments and push all style strings beforehand */
		case YAML_ANY_SCALAR_STYLE:
			sq_pushnull(vm);
			break;
		case YAML_PLAIN_SCALAR_STYLE:
			sq_pushstring(vm, "plain", -1);
			break;
		case YAML_SINGLE_QUOTED_SCALAR_STYLE:
			sq_pushstring(vm, "singlequoted", -1);
			break;
		case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
			sq_pushstring(vm, "doublequoted", -1);
			break;
		case YAML_LITERAL_SCALAR_STYLE:
			sq_pushstring(vm, "literal", -1);
			break;
		case YAML_FOLDED_SCALAR_STYLE:
			sq_pushstring(vm, "folded", -1);
			break;
		default:
			sq_pushnull(vm);
	}
	if(SQ_FAILED(sq_call(vm, 4, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_version_directive(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushinteger(vm, token->data.version_directive.major);
	sq_pushinteger(vm, token->data.version_directive.minor);
	if(SQ_FAILED(sq_call(vm, 3, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

static int handle_tag_directive(
		gla_rt_t *rt,
		HSQOBJECT *fn,
		yaml_token_t *token)
{
	HSQUIRRELVM vm = rt->vm;
	SQBool resume;

	sq_pushobject(vm, *fn);
	sq_push(vm, 2);
	sq_pushstring(vm, (SQChar*)token->data.tag_directive.handle, -1);
	sq_pushstring(vm, (SQChar*)token->data.tag_directive.prefix, -1);
	if(SQ_FAILED(sq_call(vm, 3, true, true))) {
		sq_pushnull(vm);
		return GLA_VM;
	}
	else if(!SQ_FAILED(sq_getbool(vm, -1, &resume)) && !resume)
		return GLA_END;
	else
		return GLA_SUCCESS;
}

struct {
	int id;
	const char *member;
	int index;
	int (*handler)(gla_rt_t*, HSQOBJECT*, yaml_token_t*);
} tokendef[] = {
	{ YAML_NO_TOKEN, "empty", TOKEN_EMPTY, handle_noargs },
	{ YAML_STREAM_START_TOKEN, "streamStart", TOKEN_STREAM_START, handle_noargs },
	{ YAML_STREAM_END_TOKEN, "streamEnd", TOKEN_STREAM_END, handle_noargs },
	{ YAML_VERSION_DIRECTIVE_TOKEN, "versionDirective", TOKEN_VERSION_DIRECTIVE, handle_version_directive },
	{ YAML_TAG_DIRECTIVE_TOKEN, "tagDirective", TOKEN_TAG_DIRECTIVE, handle_tag_directive },
	{ YAML_DOCUMENT_START_TOKEN, "documentStart", TOKEN_DOC_START, handle_noargs },
	{ YAML_DOCUMENT_END_TOKEN, "documentEnd", TOKEN_DOC_END, handle_noargs },
	{ YAML_BLOCK_SEQUENCE_START_TOKEN, "blockSequenceStart", TOKEN_BLOCKSEQ_START, handle_noargs },
	{ YAML_BLOCK_MAPPING_START_TOKEN, "blockMappingStart", TOKEN_BLOCKMAP_START, handle_noargs },
	{ YAML_BLOCK_END_TOKEN, "blockEnd", TOKEN_BLOCK_END, handle_noargs },
	{ YAML_FLOW_SEQUENCE_START_TOKEN, "flowSequenceStart", TOKEN_FLOWSEQ_START, handle_noargs },
	{ YAML_FLOW_SEQUENCE_END_TOKEN, "flowSequenceEnd", TOKEN_FLOWSEQ_END, handle_noargs },
	{ YAML_FLOW_MAPPING_START_TOKEN, "flowMappingStart", TOKEN_FLOWMAP_START, handle_noargs },
	{ YAML_FLOW_MAPPING_END_TOKEN, "flowMappingEnd", TOKEN_FLOWMAP_END, handle_noargs },
	{ YAML_BLOCK_ENTRY_TOKEN, "blockEntry", TOKEN_BLOCK_ENTRY, handle_noargs },
	{ YAML_FLOW_ENTRY_TOKEN, "flowEntry", TOKEN_FLOW_ENTRY, handle_noargs },
	{ YAML_KEY_TOKEN, "key", TOKEN_KEY, handle_noargs },
	{ YAML_VALUE_TOKEN, "value", TOKEN_VALUE, handle_noargs },
	{ YAML_ALIAS_TOKEN, "alias", TOKEN_ALIAS, handle_alias },
	{ YAML_ANCHOR_TOKEN, "anchor", TOKEN_ANCHOR, handle_anchor },
	{ YAML_TAG_TOKEN, "tag", TOKEN_TAG, handle_tag },
	{ YAML_SCALAR_TOKEN, "scalar", TOKEN_SCALAR, handle_scalar }
};

static int read_yaml(
		void *io_,
		unsigned char *buffer,
		size_t size,
		size_t *n)
{
	gla_io_t *io = io_;

	*n = 0;
	if(gla_io_rstatus(io) == GLA_END)
		return 1;
	*n = gla_io_read(io, buffer, size);
	if(gla_io_rstatus(io) != GLA_SUCCESS && gla_io_rstatus(io) != GLA_END)
		return 0;
	else
		return 1;
}

static apr_status_t cleanup_yaml(
		void *parser_)
{
	yaml_parser_t *parser = parser_;
	yaml_parser_delete(parser);
	return APR_SUCCESS;
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
	int i;
	yaml_parser_t parser;
	yaml_token_t token;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	HSQOBJECT callback[ARRAY_SIZE(tokendef)];
	bool hascb[ARRAY_SIZE(tokendef)];

	if(sq_gettop(vm) != 3)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(yaml_parser_initialize(&parser) == 0)
		return gla_rt_vmthrow(rt, "Error initializing yaml parser");
	apr_pool_cleanup_register(rt->mpstack, &parser, cleanup_yaml, apr_pool_cleanup_null);

	memset(&token, 0, sizeof(token));
	memset(callback, 0, sizeof(callback));
	memset(hascb, 0, sizeof(hascb));
	for(i = 0; i < ARRAY_SIZE(tokendef); i++) {
		sq_pushstring(vm, tokendef[i].member, -1);
		if(!SQ_FAILED(sq_get(vm, 2))) {
			sq_getstackobj(vm, -1, callback + tokendef[i].index);
			sq_poptop(vm);
			hascb[tokendef[i].index] = true;
		}
	}
/*	sq_pushstring(vm, "streamStart", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'entry'");
	sq_pushstring(vm, "streamEnd", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'end'");
	sq_pushstring(vm, "versionDirective", -1);
	if(SQ_FAILED(sq_get(vm, 2)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: missing member 'begin'");*/


	ret = gla_path_get_entity(&path, false, rt, 3, rt->mpstack);
	if(ret != GLA_SUCCESS)
		return gla_rt_vmthrow(rt, "Invalid entity given");
	if(path.extension == NULL)
		path.extension = "yaml";

	mnt = gla_rt_resolve(rt, &path, GLA_MOUNT_SOURCE, &shifted, rt->mpstack);
	if(mnt == NULL)
		return gla_rt_vmthrow(rt, "No such entity");
	io = gla_mount_open(mnt, &path, GLA_MODE_READ, rt->mpstack);
	if(io == NULL)
		return gla_rt_vmthrow(rt, "Error opening entity");
	yaml_parser_set_input(&parser, read_yaml, io);

	do {
		if(yaml_parser_scan(&parser, &token) == 0)
			return gla_rt_vmthrow(rt, "Error reading yaml token");
		for(i = 0; i < ARRAY_SIZE(tokendef); i++)
			if(tokendef[i].id == token.type) {
				if(!hascb[tokendef[i].index])
					break;
				ret = tokendef[i].handler(rt, callback + tokendef[i].index, &token);
				sq_pop(vm, 2);
				if(ret == GLA_VM)
					return gla_rt_vmthrow(rt, "Error from callback");
				else if(ret == GLA_END)
					break;
				break;
			}
	} while(token.type != YAML_STREAM_END_TOKEN);
	yaml_token_delete(&token);
	return gla_rt_vmsuccess(rt, false);
}

int gla_mod_yaml_cbparser_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "parse", -1);
	sq_newclosure(vm, fn_parse, 0);
	sq_newslot(vm, -3, false);

	return GLA_SUCCESS;
}

