#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>

#include "../io/module.h"
#include "../common.h"
#include "../rt.h"

#define BUFFER_SIZE 1024

static void dummy_sigchld(
		int signo)
{}

enum {
	STDMODE_DISCARD,
	STDMODE_PARENT,
	STDMODE_IO
};

SQInteger _gla_platform_fn_exec(
		HSQUIRRELVM vm)
{
	//TODO security note: access to whole system! maybe introduce flags which prevent using system(), fopen() and other related functions
	int ret;
	int pid;
	const SQChar *command;
	SQBool use_parentstd;
	const char **args;
	gla_io_t *my_stdin = NULL;
	gla_io_t *my_stdout = NULL;
	gla_io_t *my_stderr = NULL;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	int mode_stdin;
	int mode_stdout;
	int mode_stderr;
	int exitcode = -1;
	int pipe_ctrl[2];
	int pipe_stdin[2];
	int pipe_stdout[2];
	int pipe_stderr[2];
	char *buffer;
	const char *err_message = "unknown error";
	char c;

	pipe_ctrl[0] = -1;
	pipe_ctrl[1] = -1;
	pipe_stdin[0] = -1;
	pipe_stdin[1] = -1;
	pipe_stdout[0] = -1;
	pipe_stdout[1] = -1;
	pipe_stderr[0] = -1;
	pipe_stderr[1] = -1;

	if(sq_gettop(vm) < 2 || sq_gettop(vm) > 5)
		return gla_rt_vmthrow(rt, "invalid argument count");
	if(sq_gettype(vm, 2) == OT_STRING) {
		sq_getstring(vm, 2, &command);
		args = apr_palloc(rt->mpstack, sizeof(char*) * 2);
		args[0] = command;
		args[1] = NULL;
	}
	else if(sq_gettype(vm, 2) == OT_ARRAY) {
		const SQChar *arg;
		int argn = sq_getsize(vm, 2);
		int i;
		if(argn == 0)
			return gla_rt_vmthrow(rt, "invalid argument 1: array must not be empty");
		args = apr_palloc(rt->mpstack, sizeof(char*) * (argn + 1));
		sq_pushnull(vm);
		for(i = 0; i < argn; i++) {
			sq_pushinteger(vm, i);
			if(SQ_FAILED(sq_get(vm, 2)))
				return gla_rt_vmthrow(rt, "error reading argument 1: getting element failed");
			else if(SQ_FAILED(sq_getstring(vm, -1, &arg)))
				return gla_rt_vmthrow(rt, "invalid argument 1: array must consist of string only");
			args[i] = arg;
			sq_poptop(vm);
		}
		args[argn] = NULL;
		command = args[0];
	}
	else
		return gla_rt_vmthrow(rt, "invalid argument 1: expected command string or arguments array");

	if(sq_gettop(vm) >= 3)
		switch(sq_gettype(vm, 3)) {
			case OT_NULL: /* no stdin means we close stdin after fork() */
				mode_stdin = STDMODE_DISCARD;
				break;
			case OT_BOOL: /* true: use stdin of parent process; false: same as null */
				sq_getbool(vm, 3, &use_parentstd);
				if(use_parentstd)
					mode_stdin = STDMODE_PARENT;
				else
					mode_stdin = STDMODE_DISCARD;
				break;
			case OT_STRING:
				return gla_rt_vmthrow(rt, "stdin string not yet implemented");
			default:
				my_stdin = gla_mod_io_io_get(rt, 3);
				if(my_stdin == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 2: expected null, bool or input stream");
				break;
		}
	else
		mode_stdin = STDMODE_PARENT;
	if(sq_gettop(vm) >= 4)
		switch(sq_gettype(vm, 4)) {
			case OT_NULL: /* discard all output */
				mode_stdout = STDMODE_DISCARD;
				break;
			case OT_BOOL: /* true: use stdout of parent process; false: same as null */
				sq_getbool(vm, 4, &use_parentstd);
				if(use_parentstd)
					mode_stdout = STDMODE_PARENT;
				else
					mode_stdout = STDMODE_DISCARD;
				break;
			default:
				my_stdout = gla_mod_io_io_get(rt, 4);
				if(my_stdout == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 4: expected null, bool or input stream");
				break;
		}
	else
		mode_stdout = STDMODE_PARENT;
	if(sq_gettop(vm) >= 5)
		switch(sq_gettype(vm, 5)) {
			case OT_NULL: /* discard all output */
				mode_stderr = STDMODE_DISCARD;
				break;
			case OT_BOOL: /* true: use stderr of parent process; false: same as null */
				sq_getbool(vm, 5, &use_parentstd);
				if(use_parentstd)
					mode_stderr = STDMODE_PARENT;
				else
					mode_stderr = STDMODE_DISCARD;
				break;
			default:
				my_stderr = gla_mod_io_io_get(rt, 5);
				if(my_stderr == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 5: expected null, bool or input stream");
				break;
		}
	else
		mode_stderr = STDMODE_PARENT;

	buffer = apr_palloc(rt->mpstack, BUFFER_SIZE);

	err_message = "error creating pipe";
	ret = pipe(pipe_ctrl);
	if(ret < 0)
		goto error;
	if(fcntl(pipe_ctrl[1], F_SETFD, fcntl(pipe_ctrl[1], F_GETFD) | FD_CLOEXEC) == -1)
		goto error;
	if(mode_stdin != STDMODE_PARENT) {
		ret = pipe(pipe_stdin);
		if(ret < 0)
			goto error;
	}
	if(mode_stdout != STDMODE_PARENT) {
		ret = pipe(pipe_stdout);
		if(ret < 0)
			goto error;
	}
	if(mode_stderr != STDMODE_PARENT) {
		ret = pipe(pipe_stderr);
		if(ret < 0)
			goto error;
	}

	pid = fork();
	if(pid == 0) {
		c = 0;
		close(pipe_ctrl[0]);
		if(mode_stdin != STDMODE_PARENT) {
			ret = dup2(pipe_stdin[0], STDIN_FILENO);
			if(ret == -1) {
				write(pipe_ctrl[1], &c, 1);
				exit(0);
			}
			close(pipe_stdin[0]);
			close(pipe_stdin[1]);
		}

		if(mode_stdout != STDMODE_PARENT) {
			ret = dup2(pipe_stdout[1], STDOUT_FILENO);
			if(ret == -1) {
				write(pipe_ctrl[1], &c, 1);
				exit(0);
			}
			close(pipe_stdout[0]);
			close(pipe_stdout[1]);
		}

		if(mode_stderr != STDMODE_PARENT) {
			ret = dup2(pipe_stderr[1], STDERR_FILENO);
			if(ret == -1) {
				write(pipe_ctrl[1], &c, 1);
				exit(0);
			}
			close(pipe_stderr[0]);
			close(pipe_stderr[1]);
		}

		c = 1;
		write(pipe_ctrl[1], &c, 1);
		execvp(command, (char*const*)args);
		c = 2;
		write(pipe_ctrl[1], &c, 1);
		exit(0);
	}
	else if(pid > 0) {
		struct sigaction sa;
		struct sigaction sa_old;
		close(pipe_ctrl[1]);

		if(pipe_stderr[1] != -1)
			close(pipe_stderr[1]);
		if(pipe_stdout[1] != -1)
			close(pipe_stdout[1]);
		if(pipe_stdin[0] != -1)
			close(pipe_stdin[0]);
		pipe_ctrl[1] = -1;
		pipe_stderr[1] = -1;
		pipe_stdout[1] = -1;
		pipe_stdin[0] = -1;

		ret = read(pipe_ctrl[0], &c, 1);
		if(c == 0) {
			err_message = "error starting child process";
			goto error;
		}

		if(mode_stdin == STDMODE_DISCARD) {
			close(pipe_stdin[1]);
			pipe_stdin[1] = -1;
		}
		if(mode_stdout == STDMODE_DISCARD) {
			close(pipe_stdout[0]);
			pipe_stdout[0] = -1;
		}
		if(mode_stderr == STDMODE_DISCARD) {
			close(pipe_stderr[0]);
			pipe_stderr[0] = -1;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = dummy_sigchld;
		sigaction(SIGCHLD, &sa, &sa_old);
		for(;;) {
			fd_set fds;
			int nfds;
			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			FD_ZERO(&fds);
			if(pipe_stdout[0] != -1) {
				FD_SET(pipe_stdout[0], &fds);
				if(pipe_stdout[0] > nfds)
					nfds = pipe_stdout[0];
			}
			if(pipe_stderr[0] != -1) {
				FD_SET(pipe_stderr[0], &fds);
				if(pipe_stderr[0] > nfds)
					nfds = pipe_stderr[0];
			}
			ret = select(nfds, &fds, NULL, NULL, &timeout);
			if(ret == 0) { /* timeout */
			}
			else if(ret == -1) { /* error, possibly interrupted */
				if(errno == EINTR) {
					ret = waitpid(pid, &exitcode, WNOHANG);
					if(ret == pid) { /* finished */
						ret = read(pipe_ctrl[0], &c, 1);
						if(ret == 1)
							err_message = "error executing command";
						else
							err_message = NULL;
						sigaction(SIGCHLD, &sa_old, NULL);
						goto error;
					}
				}
				else { /* other error */
					printf("error while executing"); /* TODO what to do here? possibly count down some retries of select() and throw error if countdown has reached 0 */
					sleep(1);
				}
			}
			else {
				if(pipe_stdout[0] != -1 && FD_ISSET(pipe_stdout[0], &fds)) {
					int bytes = read(pipe_stdout[0], buffer, BUFFER_SIZE - 1);
					if(bytes > 0)
						buffer[bytes] = 0;
					else
						buffer[0] = 0;
					printf("STDOUT READ: %d '%s'\n", bytes, buffer);
					assert(my_stdout != NULL);
				}
				if(pipe_stderr[0] != -1 && FD_ISSET(pipe_stderr[0], &fds)) {
					int bytes = read(pipe_stderr[0], buffer, BUFFER_SIZE - 1);
					if(bytes > 0)
						buffer[bytes] = 0;
					else
						buffer[0] = 0;
					printf("STDERR READ: %d '%s'\n", bytes, buffer);
					assert(my_stderr != NULL);
				}
			}
		}
	}
	else {
		err_message = "error forking";
		goto error;
	}

error:
	if(pipe_stderr[1] != -1)
		close(pipe_stderr[1]);
	if(pipe_stderr[0] != -1)
		close(pipe_stderr[0]);
	if(pipe_stdout[1] != -1)
		close(pipe_stdout[1]);
	if(pipe_stdout[0] != -1)
		close(pipe_stdout[0]);
	if(pipe_stdin[1] != -1)
		close(pipe_stdin[1]);
	if(pipe_stdin[0] != -1)
		close(pipe_stdin[0]);
	if(pipe_ctrl[1] != -1)
		close(pipe_ctrl[1]);
	if(pipe_ctrl[0] != -1)
		close(pipe_ctrl[0]);
	if(err_message == NULL) {
		sq_pushinteger(vm, exitcode);
		return gla_rt_vmsuccess(rt, true);
	}
	else
		return gla_rt_vmthrow(rt, err_message);
}

