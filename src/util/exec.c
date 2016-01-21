#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "../io/io.h"

#include "../log.h"
#include "../rt.h"
#include "../_io.h"

#include "stackexec.h"

static gla_meta_io_t io_meta = {
	.read = io_read,
	.write = io_write
};

typedef struct {
	gla_io_t super;
	int fd;
	HSQOBJECT object;
	apr_pool_t *pool;
} io_t;

typedef struct {
	gla_rt_t *rt;
	char **args;
	pid_t pid;
	/* if io_X.super.meta != NULL, use redirection */
	io_t io_stdin;
	io_t io_stdout;
	io_t io_stderr;
} exec_t;

static int io_read(
		gla_io_t *self_,
		void *buffer,
		int size)
{
	io_t *self = (io_t*)self_;
	char *di = buffer;
	int bytes;
	int remaining = size;

	while(remaining != 0) {
		bytes = read(self->fd, di, remaining);
		if(bytes == 0) {
			self->super.rstatus = GLA_END;
			return size - remaining;
		}
		else if(bytes < 0) {
			self->super.rstatus = GLA_IO;
			return size - remaining;
		}
		remaining -= bytes;
		di += bytes;
	}
	return size;
}

static int io_write(
		gla_io_t *self_,
		const void *buffer,
		int size)
{
	io_t *self = (io_t*)self_;
	int bytes;
	int remaining = 0;
	const char *si = buffer;

	while(remaining != 0) {
		bytes = write(self->fd, si, remaining);
		if(bytes == 0) {
			self->super.wstatus = GLA_END;
			return size - remaining;
		}
		else if(bytes < 0) {
			self->super.wstatus = GLA_IO;
			return size - remaining;
		}
		remaining -= bytes;
		si += bytes;
	}
	return size;
}

static SQInteger fn_dtor(
		SQUserPointer up,
		SQInteger size)
{
	exec_t *self = up;
	gla_rt_t *rt = self->rt;
	int i = 0;

	while(self->args[i] != NULL) {
		free(self->args[i]);
		i++;
	}
	free(self->args);
	if(!self->rt->shutdown) {
		if(self->io_stdin.super.meta != NULL)
			sq_release(rt->vm, &self->io_stdin.object);
		if(self->io_stdout.super.meta != NULL)
			sq_release(rt->vm, &self->io_stdout.object);
		if(self->io_stderr.super.meta != NULL)
			sq_release(rt->vm, &self->io_stderr.object);
	}
	return SQ_OK;
}

static SQInteger fn_ctor(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int i;
	const SQChar *string;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) < 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;
	memset(self, 0, sizeof(exec_t));
	self->rt = rt;
	self->args = calloc(sq_gettop(vm), sizeof(char*));
	if(self->args == NULL)
		return gla_rt_vmthrow(rt, "Not enough memory");
	for(i = 0; i < sq_gettop(vm) - 1; i++) {
		sq_getstring(vm, i + 2, &string);
		self->args[i] = strdup(string);
		if(self->args[i] == NULL)
			return gla_rt_vmthrow(rt, "Not enough memory");
	}

	sq_setreleasehook(vm, 1, fn_dtor);
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_singleshot(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	SQBool throw_status = false;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(sq_gettop(vm) >= 2 && SQ_FAILED(sq_getbool(vm, 2, &throw_status)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected bool");
	else if(sq_gettop(vm) > 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	self = up;

	if(self->pid != 0)
		return gla_rt_vmthrow(rt, "Process already executing");
	ret = execvp(self->args[0], self->args);
	if(throw_status && ret != 0)
		return gla_rt_vmthrow(rt, "Program returned error code %d", status);
	else {
		sq_pushinteger(vm, status);
		return gla_rt_vmsuccess(rt, true);
	}
	if(ret != 0 && throw_status)
		return 
	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_execute(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	int stdin_pipe[2];
	int stdout_pipe[2];
	int stderr_pipe[2];
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io_stdin.super.meta != NULL) {
		ret = pipe(stdin_pipe);
		if(ret != 0)
			return gla_rt_vmthrow(rt, "Error creating stdin pipe");
	}
	if(self->io_stdout.super.meta != NULL) {
		ret = pipe(stdout_pipe);
		if(ret != 0)
			return gla_rt_vmthrow(rt, "Error creating stdin pipe");
	}
	if(self->io_stderr.super.meta != NULL) {
		ret = pipe(stderr_pipe);
		if(ret != 0)
			return gla_rt_vmthrow(rt, "Error creating stdin pipe");
	}
	self->pid = fork();
	if(self->pid < 0)
		return gla_rt_vmthrow(rt, "Error forking");
	else if(self->pid == 0) { /* child process */
		if(self->io_stdin.super.meta != NULL) {
			close(stdin_pipe[1]);
			self->io_stdin.fd = stdin_pipe[0];
			while((dup2(stdin_pipe[0], STDIN_FILENO) == -1) && (errno == EINTR));
		}
		if(self->io_stdout.super.meta != NULL) {
			close(stdout_pipe[0]);
			self->io_stdout.fd = stdout_pipe[1];
			while((dup2(stdout_pipe[1], STDOUT_FILENO) == -1) && (errno == EINTR));
		}
		if(self->io_stderr.super.meta != NULL) {
			close(stderr_pipe[0]);
			self->io_stderr.fd = stderr_pipe[1];
			while((dup2(stderr_pipe[1], STDERR_FILENO) == -1) && (errno == EINTR));
		}
		ret = execvp(self->args[0], self->args);
		exit(ret);
		assert(false);
		return gla_rt_vmthrow(rt, "Dead end reached");
	}
	else { /* parent process */
		if(self->io_stdin.super.meta != NULL) {
			close(stdin_pipe[0]);
			self->io_stdin.fd = stdin_pipe[1];
		}
		if(self->io_stdout.super.meta != NULL) {
			close(stdout_pipe[1]);
			self->io_stdout.fd = stdout_pipe[0];
		}
		if(self->io_stderr.super.meta != NULL) {
			close(stderr_pipe[1]);
			self->io_stderr.fd = stderr_pipe[0];
		}
		return gla_rt_vmsuccess(rt, false);
	}
}

static SQInteger fn_wait(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	int status;
	SQBool throw_status = false;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	else if(sq_gettop(vm) >= 2 && SQ_FAILED(sq_getbool(vm, 2, &throw_status)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected bool");
	else if(sq_gettop(vm) > 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	self = up;

	if(self->pid == 0) {
		sq_pushnull(vm);
		return gla_rt_vmsuccess(rt, true);
	}

	ret = waitpid(self->pid, &status, 0);
	self->pid = 0;
	if(throw_status && status != 0)
		return gla_rt_vmthrow(rt, "Program returned error code %d", status);
	else {
		sq_pushinteger(vm, status);
		return gla_rt_vmsuccess(rt, true);
	}
}

static SQInteger fn_stdin(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io_stdin.super.meta == NULL) {
		if(apr_pool_create_unmanaged(&self->io_stdin.pool) != APR_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating memory pool");
		self->io_stdin.super.meta = &io_meta;
		self->io_stdin.super.mode = GLA_MODE_WRITE;
		self->io_stdin.super.wstatus = GLA_SUCCESS;
		self->io_stdin.super.rstatus = GLA_SUCCESS;
		ret = gla_mod_io_io_new(rt, &self->io_stdin.super, 0, 0, self->io_stdin.pool);
		if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating io object");
		sq_getstackobj(vm, -1, &self->io_stdin.object);
		sq_addref(vm, &self->io_stdin.object);
	}
	sq_pushobject(vm, self->io_stdin.object);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_stdout(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io_stdout.super.meta == NULL) {
		if(apr_pool_create_unmanaged(&self->io_stdout.pool) != APR_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating memory pool");
		self->io_stdout.super.meta = &io_meta;
		self->io_stdout.super.mode = GLA_MODE_READ;
		self->io_stdout.super.wstatus = GLA_SUCCESS;
		self->io_stdout.super.rstatus = GLA_SUCCESS;
		ret = gla_mod_io_io_new(rt, &self->io_stdout.super, 0, 0, self->io_stdout.pool);
		if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating io object");
		sq_getstackobj(vm, -1, &self->io_stdout.object);
		sq_addref(vm, &self->io_stdout.object);
	}
	sq_pushobject(vm, self->io_stdout.object);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_stderr(
		HSQUIRRELVM vm)
{
	SQUserPointer up;
	exec_t *self;
	int ret;
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getinstanceup(vm, 1, &up, NULL)))
		return gla_rt_vmthrow(rt, "Error getting instance userpointer");
	self = up;

	if(self->io_stderr.super.meta == NULL) {
		if(apr_pool_create_unmanaged(&self->io_stderr.pool) != APR_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating memory pool");
		self->io_stderr.super.meta = &io_meta;
		self->io_stderr.super.mode = GLA_MODE_READ;
		self->io_stderr.super.wstatus = GLA_SUCCESS;
		self->io_stderr.super.rstatus = GLA_SUCCESS;
		ret = gla_mod_io_io_new(rt, &self->io_stderr.super, 0, 0, self->io_stderr.pool);
		if(ret != GLA_SUCCESS)
			return gla_rt_vmthrow(rt, "Error creating io object");
		sq_getstackobj(vm, -1, &self->io_stderr.object);
		sq_addref(vm, &self->io_stderr.object);
	}
	sq_pushobject(vm, self->io_stderr.object);
	return gla_rt_vmsuccess(rt, true);
}

SQInteger gla_mod_util_exec_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);

	sq_pushstring(vm, "constructor", -1);
	sq_newclosure(vm, fn_ctor, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "execute", -1);
	sq_newclosure(vm, fn_execute, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "wait", -1);
	sq_newclosure(vm, fn_wait, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "stdin", -1);
	sq_newclosure(vm, fn_stdin, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "stdout", -1);
	sq_newclosure(vm, fn_stdout, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "stderr", -1);
	sq_newclosure(vm, fn_stderr, 0);
	sq_newslot(vm, -3, false);

	sq_setclassudsize(vm, -1, sizeof(exec_t));

	return gla_rt_vmsuccess(rt, false);
}

