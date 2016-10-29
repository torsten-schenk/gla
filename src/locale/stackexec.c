#include <assert.h>
#include <apr_tables.h>
#include <gravm/runstack.h>

#include "../log.h"
#include "../rt.h"

#include "stackexec.h"

#define CALL_METHOD(ARGS) \
	do { \
		if(sq_call(vm, 1, false, false) != SQ_OK) { \
			sq_getlasterror(vm); \
			return gla_rt_vmthrow_top(rt); \
		} \
	} while(false)

#define MAX_STACK_SIZE 65536

#define CB_BEGIN(METHOD, DEFAULT) \
	stackexec_t *self = self_; \
	HSQUIRRELVM vm = self->rt->vm; \
	int ret = DEFAULT; \
	if(!self->method_isnull[METHOD]) { \
		sq_pushobject(vm, self->methods[METHOD]); \
		sq_push(vm, 2);

#define CB_CALL(NARG) \
		if(sq_call(vm, NARG + 1, true, true) != SQ_OK) { \
			sq_getlasterror(vm); \
			sq_release(vm, &self->exception); \
			sq_getstackobj(vm, -1, &self->exception); \
			sq_addref(vm, &self->exception); \
			errno = 0; \
			return GRAVM_RS_THROW; \
		} \
		switch(sq_gettype(vm, -1)) {

#define CBRET_NULL(CODE) \
		case OT_NULL: { \
			CODE \
		} \
		break;

#define CBRET_NULL_NOP \
		case OT_NULL: \
		break;

#define CBRET_BOOL(CODE) \
		case OT_BOOL: { \
			SQBool value; \
			sq_getbool(vm, -1, &value); \
			CODE \
		} \
		break;

#define CBRET_BOOL_DEFAULT \
		case OT_BOOL: { \
			SQBool value; \
			sq_getbool(vm, -1, &value); \
			ret = value ? GRAVM_RS_TRUE : GRAVM_RS_FALSE; \
		} \
		break;


#define CBRET_DONE \
		default: \
			LOG_WARN("invalid return type"); \
			break; \
	}

/*		if(type == OT_NULL) {
			if(iteration == 0)
				ret = GRAVM_RS_TRUE;
		}
		else if(type == OT_BOOL) {
			sq_getbool(vm, -1, &vb);
			ret = vb ? GRAVM_RS_TRUE : GRAVM_RS_FALSE;
		}
		else
			LOG_WARN("invalid return type from edgeNext");*/

#define CB_CALL_NORET(NARG) \
		if(sq_call(vm, NARG + 1, false, true) != SQ_OK) { \
			sq_getlasterror(vm); \
			sq_release(vm, &self->exception); \
			sq_getstackobj(vm, -1, &self->exception); \
			sq_addref(vm, &self->exception); \
			errno = 0; \
			return GRAVM_RS_THROW; \
		}

#define CB_NULL_METHOD \
	} \
	else {

#define CB_RETURN \
	} \
	return ret;
	
enum {
	METHOD_BEGIN,
	METHOD_END,
	METHOD_DESCEND,
	METHOD_ASCEND,
	METHOD_EDGE_BEGIN,
	METHOD_EDGE_NEXT,
	METHOD_EDGE_END,
	METHOD_EDGE_PREPARE,
	METHOD_EDGE_UNPREPARE,
	METHOD_EDGE_ABORT,
	METHOD_EDGE_CATCH,
	METHOD_NODE_ENTER,
	METHOD_NODE_LEAVE,
	METHOD_NODE_RUN,
	METHOD_NODE_CATCH,
	METHODS
};

static const struct {
	const char *name;
	bool is_mandatory;
} method_defs[] = {
	{ .name = "begin" },
	{ .name = "end" },
	{ .name = "descend" },
	{ .name = "ascend" },
	{ .name = "edgeBegin" },
	{ .name = "edgeNext" },
	{ .name = "edgeEnd" },
	{ .name = "edgePrepare" },
	{ .name = "edgeUnprepare" },
	{ .name = "edgeAbort" },
	{ .name = "edgeCatch" },
	{ .name = "nodeEnter" },
	{ .name = "nodeLeave" },
	{ .name = "nodeRun" },
	{ .name = "nodeCatch" }
};

typedef struct {
} frame_t;

typedef struct {
	bool method_isnull[METHODS];
	HSQOBJECT methods[METHODS];
	int n_edges;
	bool suspended;
	gla_rt_t *rt;
	gravm_runstack_t *rs;
	SQInteger error;
	HSQOBJECT exception;
} stackexec_t;

static int rscb_init(
		void *self);
static int rscb_structure(
		void *self,
		int edge,
		gravm_runstack_edgedef_t *def);
static int rscb_begin(
		void *self);
static int rscb_end(
		void *self);
static int rscb_descend(
		void *self,
		void *parent_frame,
		void *child_frame);
static int rscb_ascend(
		void *self,
		bool throwing,
		int err,
		void *parent_frame,
		void *child_frame);
static int rscb_edge_prepare(
		void *self,
		int id,
		void *framedata);
static int rscb_edge_unprepare(
		void *self,
		int id,
		void *framedata);
static int rscb_edge_begin(
		void *self,
		int id,
		void *framedata);
static int rscb_edge_next(
		void *self,
		int iteration,
		int id,
		void *framedata);
static int rscb_edge_end(
		void *self,
		int id,
		void *framedata);
static int rscb_edge_abort(
		void *self,
		int err,
		int id,
		void *framedata);
static int rscb_edge_catch(
		void *self,
		int err,
		int id,
		void *framedata);
static int rscb_node_enter(
		void *self,
		int id,
		void *framedata);
static int rscb_node_leave(
		void *self,
		int id,
		void *framedata);
static int rscb_node_run(
		void *self,
		int id,
		void *framedata);
static int rscb_node_catch(
		void *self,
		int err,
		int id,
		void *framedata);

static const gravm_runstack_callback_t rs_callbacks = {
	.init = rscb_init,
	.structure = rscb_structure,
	.begin = rscb_begin,
	.end = rscb_end,
	.descend = rscb_descend,
	.ascend = rscb_ascend,

	.edge_prepare = rscb_edge_prepare,
	.edge_unprepare = rscb_edge_unprepare,
	.edge_begin = rscb_edge_begin,
	.edge_next = rscb_edge_next,
	.edge_end = rscb_edge_end,
	.edge_abort = rscb_edge_abort,
	.edge_catch = rscb_edge_catch,

	.node_enter = rscb_node_enter,
	.node_leave = rscb_node_leave,
	.node_run = rscb_node_run,
	.node_catch = rscb_node_catch
};

static int poolcb_rs_destroy(
		void *rs_)
{
	gravm_runstack_t *rs = rs_;
	gravm_runstack_destroy(rs);
	return APR_SUCCESS;
}

static int rscb_init(
		void *self_)
{
	stackexec_t *self = self_;
	return self->n_edges;
}

/* operand stack: exeuter instance, task instance, task class, task class attributes, edges */
static int rscb_structure(
		void *self_,
		int edge,
		gravm_runstack_edgedef_t *def)
{
	stackexec_t *self = self_;
	HSQUIRRELVM vm = self->rt->vm;
	int ret;
	SQObjectType objtype;
	SQInteger integer;

	sq_pushinteger(vm, edge);
	ret = sq_get(vm, 5);
	if(ret != SQ_OK) {
		self->error = gla_rt_vmthrow(self->rt, "error getting edge %d", edge);
		return -1;
	}

	/* edge source */
	sq_pushstring(vm, "source", -1);
	ret = sq_get(vm, -2);
	if(ret == SQ_OK) {
		objtype = sq_gettype(vm, -1);
		switch(objtype) {
			case OT_NULL:
				def->source = GRAVM_RS_ROOT;
				break;
			case OT_INTEGER:
				ret = sq_getinteger(vm, -1, &integer);
				if(ret != SQ_OK) {
					self->error = gla_rt_vmthrow(self->rt, "error getting source of edge %d from stack", edge);
					return -1;
				}
				else if(integer < 0) {
					self->error = gla_rt_vmthrow(self->rt, "error getting source of edge %d: invalid node id %d", edge, (int)integer);
					return -1;
				}
				def->source = integer;
				break;
			case OT_STRING:
				self->error = gla_rt_vmthrow(self->rt, "named edge sources/targets not yet supported"); /* TODO */
				return -1;
			default:
				self->error = gla_rt_vmthrow(self->rt, "invalid source type for edge %d", edge);
				return -1;
		}
		sq_pop(vm, 1);
	}
	else {
		sq_reseterror(vm);
		def->source = GRAVM_RS_ROOT;
	}

	/* edge target */
	sq_pushstring(vm, "target", -1);
	ret = sq_get(vm, -2);
	if(ret != SQ_OK) {
		self->error = gla_rt_vmthrow(self->rt, "error getting target of edge %d", edge);
		return -1;
	}
	objtype = sq_gettype(vm, -1);
	switch(objtype) {
		case OT_NULL:
			self->error = gla_rt_vmthrow(self->rt, "invalid target for edge %d: target must not be root node", edge);
			return -1;
		case OT_INTEGER:
			ret = sq_getinteger(vm, -1, &integer);
			if(ret != SQ_OK) {
				self->error = gla_rt_vmthrow(self->rt, "error getting target of edge %d from stack", edge);
				return -1;
			}
			else if(integer < 0) {
				self->error = gla_rt_vmthrow(self->rt, "error getting target of edge %d: invalid node id %d", edge, (int)integer);
				return -1;
			}
			def->target = integer;
			break;
		case OT_STRING:
			self->error = gla_rt_vmthrow(self->rt, "named edge sources/targets not yet supported"); /* TODO */
			return -1;
		default:
			self->error = gla_rt_vmthrow(self->rt, "invalid target type for edge %d", edge);
			return -1;
	}
	sq_pop(vm, 1);

	/* edge priority */
	sq_pushstring(vm, "priority", -1);
	ret = sq_get(vm, -2);
	if(ret == SQ_OK) {
		objtype = sq_gettype(vm, -1);
		switch(objtype) {
			case OT_NULL:
				def->priority = 0;
				break;
			case OT_INTEGER:
				ret = sq_getinteger(vm, -1, &integer);
				if(ret != SQ_OK) {
					self->error = gla_rt_vmthrow(self->rt, "error getting priority of edge %d from stack", edge);
					return -1;
				}
				def->priority = integer;
				break;
			default:
				self->error = gla_rt_vmthrow(self->rt, "invalid priority type for edge %d", edge);
				return -1;
		}
		sq_pop(vm, 1);
	}
	else
		def->priority = 0;

	sq_pop(vm, 1);
	return 0;
}

static int rscb_begin(
		void *self_)
{
	CB_BEGIN(METHOD_BEGIN, GRAVM_RS_TRUE)
		CB_CALL(0)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_end(
		void *self_)
{
	CB_BEGIN(METHOD_END, GRAVM_RS_SUCCESS)
		CB_CALL_NORET(0)
	CB_RETURN
}

static int rscb_descend(
		void *self_,
		void *parent_frame_,
		void *child_frame_)
{
	CB_BEGIN(METHOD_DESCEND, GRAVM_RS_TRUE)
		CB_CALL(0)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_ascend(
		void *self_,
		bool throwing,
		int err,
		void *parent_frame_,
		void *child_frame_)
{
	CB_BEGIN(METHOD_ASCEND, GRAVM_RS_SUCCESS)
		CB_CALL_NORET(0)
	CB_RETURN
}

static int rscb_edge_prepare(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_PREPARE, GRAVM_RS_SUCCESS)
		sq_pushinteger(vm, id);
		CB_CALL_NORET(1)
	CB_RETURN
}

static int rscb_edge_unprepare(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_UNPREPARE, GRAVM_RS_SUCCESS)
		sq_pushinteger(vm, id);
		CB_CALL_NORET(1)
	CB_RETURN
}

static int rscb_edge_begin(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_BEGIN, GRAVM_RS_TRUE)
		sq_pushinteger(vm, id);
		CB_CALL(1)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_edge_next(
		void *self_,
		int iteration,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_NEXT, GRAVM_RS_FALSE)
		sq_pushinteger(vm, id);
		sq_pushinteger(vm, iteration);
		CB_CALL(2)
		CBRET_NULL(
			ret = false;
		)
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_NULL_METHOD
		if(iteration == 0)
			ret = GRAVM_RS_TRUE;
	CB_RETURN
}

static int rscb_edge_end(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_END, GRAVM_RS_SUCCESS)
		sq_pushinteger(vm, id);
		CB_CALL_NORET(1)
	CB_RETURN
}

static int rscb_edge_abort(
		void *self_,
		int err,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_ABORT, GRAVM_RS_SUCCESS)
		sq_pushinteger(vm, id);
		sq_pushobject(vm, self->exception);
		CB_CALL_NORET(2)
	CB_RETURN
}

static int rscb_edge_catch(
		void *self_,
		int err,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_EDGE_CATCH, GRAVM_RS_FALSE)
		sq_pushinteger(vm, id);
		sq_pushobject(vm, self->exception);
		CB_CALL(2)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_node_enter(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_NODE_ENTER, GRAVM_RS_TRUE)
		sq_pushinteger(vm, id);
		CB_CALL(1)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_node_leave(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_NODE_LEAVE, GRAVM_RS_SUCCESS)
		sq_pushinteger(vm, id);
		CB_CALL_NORET(1)
	CB_RETURN
}

static int rscb_node_run(
		void *self_,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_NODE_RUN, GRAVM_RS_TRUE)
		sq_pushinteger(vm, id);
		CB_CALL(1)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

static int rscb_node_catch(
		void *self_,
		int err,
		int id,
		void *framedata)
{
	CB_BEGIN(METHOD_NODE_CATCH, GRAVM_RS_FALSE)
		sq_pushinteger(vm, id);
		sq_pushobject(vm, self->exception);
		CB_CALL(2)
		CBRET_NULL_NOP
		CBRET_BOOL_DEFAULT
		CBRET_DONE
	CB_RETURN
}

/* operand stack: exeuter instance, task instance */
static SQInteger retrieve_methods(
		gla_rt_t *rt,
		stackexec_t *self)
{
	int ret;
	int i;
	SQObjectType objtype;

	for(i = 0; i < METHODS; i++) {
		sq_pushstring(rt->vm, method_defs[i].name, -1);
		ret = sq_get(rt->vm, 2);
		if(ret != SQ_OK)
			return gla_rt_vmthrow(rt, "error getting methods from task");
		objtype = sq_gettype(rt->vm, -1);
		switch(objtype) {
			case OT_NULL:
				if(method_defs[i].is_mandatory)
					return gla_rt_vmthrow(rt, "master task is missing mandatory method %s", method_defs[i].name);
				else
					self->method_isnull[i] = true;
				break;
			case OT_CLOSURE:
			case OT_NATIVECLOSURE:
				self->method_isnull[i] = false;
				break;
			default:
				return gla_rt_vmthrow(rt, "invalid method type");
		}
		if(self->method_isnull[i]) {
			sq_pop(rt->vm, 1);
			sq_pushnull(rt->vm);
		}
		ret = sq_getstackobj(rt->vm, -1, self->methods + i);
		if(ret != SQ_OK)
			return gla_rt_vmthrow(rt, "error getting methods object from stack");
		sq_pop(rt->vm, 1);
	}

	return SQ_OK;
}

static SQInteger fn_dtor(
		SQUserPointer up,
		SQInteger size)
{
	stackexec_t *self = up;

	gravm_runstack_destroy(self->rs);

	return SQ_OK;
}

static SQInteger fn_ctor(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	stackexec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;
	memset(self, 0, sizeof(stackexec_t));

	sq_pushstring(vm, "task", -1);
	sq_push(vm, 2);
	sq_set(vm, 1);

	self->rt = rt;
	self->rs = gravm_runstack_new(&rs_callbacks, MAX_STACK_SIZE, sizeof(frame_t));
	if(self->rs == NULL)
		return gla_rt_vmthrow(rt, "Error creating runstack vm");
	sq_setreleasehook(vm, 1, fn_dtor);

	/* stack here: exeuter instance, task instance, */

	ret = retrieve_methods(rt, self);
	if(ret != SQ_OK)
		return ret;

	ret = sq_getclass(rt->vm, 2);
	if(ret != SQ_OK)
		return gla_rt_vmthrow(rt, "error getting task class");
	sq_pushnull(rt->vm);
	ret = sq_getattributes(rt->vm, 3);
	if(ret != SQ_OK)
		return gla_rt_vmthrow(rt, "error getting task class attributes");
	sq_pushstring(rt->vm, "edges", -1);
	ret = sq_get(rt->vm, 4);
	if(ret != SQ_OK)
		return gla_rt_vmthrow(rt, "error getting attribute 'edges' for task class");

	/* stack here: exeuter instance, task instance, task class, task class attributes, nodes, edges */

	self->n_edges = sq_getsize(rt->vm, 5);
	if(self->n_edges < 0)
		return gla_rt_vmthrow(rt, "error getting size of task attribute 'edges'");

	self->error = SQ_OK;
	ret = gravm_runstack_prepare(self->rs, self);
	if(ret != 0) {
		if(self->error != SQ_OK)
			return self->error;
		else
			return gla_rt_vmthrow(rt, "error preparing runstack vm");
	}

	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_execute(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	stackexec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	sq_pushstring(vm, "task", -1);
	sq_get(vm, 1);

	self->suspended = false;
	for(;;) {
		ret = gravm_runstack_step(self->rs);
		switch(ret) {
			case GRAVM_RS_THROW:
				sq_pushobject(rt->vm, self->exception);
				sq_release(rt->vm, &self->exception);
				return gla_rt_vmthrow_top(rt);
			case GRAVM_RS_FATAL:
				gravm_runstack_dump(self->rs, NULL, NULL);
				return gla_rt_vmthrow(rt, "fatal error during runstack execution");
			case GRAVM_RS_FALSE:
				return gla_rt_vmsuccess(rt, false);
			case GRAVM_RS_TRUE:
				if(self->suspended)
					return gla_rt_vmsuccess(rt, false);
				break;
			default:
				assert(false);
				return gla_rt_vmthrow(rt, "unexpected return value of runstack execution");
		}
	}
}

static SQInteger fn_step(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	stackexec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	sq_pushstring(vm, "task", -1);
	sq_get(vm, 1);

	ret = gravm_runstack_step(self->rs);
	switch(ret) {
		case GRAVM_RS_THROW:
			sq_pushobject(rt->vm, self->exception);
			sq_release(rt->vm, &self->exception);
			return gla_rt_vmthrow_top(rt);
		case GRAVM_RS_FATAL:
			gravm_runstack_dump(self->rs, NULL, NULL);
			return gla_rt_vmthrow(rt, "fatal error during runstack execution");
		case GRAVM_RS_FALSE:
			return gla_rt_vmsuccess(rt, false);
		case GRAVM_RS_TRUE:
			return gla_rt_vmsuccess(rt, false);
			break;
		default:
			assert(false);
			return gla_rt_vmthrow(rt, "unexpected return value of runstack execution");
	}
}

static SQInteger fn_suspend(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	stackexec_t *self;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;
	self->suspended = true;
	return gla_rt_vmsuccess(rt, false);
}

SQInteger gla_mod_util_stackexec_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "constructor", -1);
	sq_newclosure(vm, fn_ctor, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "execute", -1);
	sq_newclosure(vm, fn_execute, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "step", -1);
	sq_newclosure(vm, fn_step, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "suspend", -1);
	sq_newclosure(vm, fn_suspend, 0);
	sq_newslot(vm, -3, false);

	sq_setclassudsize(vm, -1, sizeof(stackexec_t));

	return gla_rt_vmsuccess(rt, false);
}

