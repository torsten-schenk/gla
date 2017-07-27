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
#include "../_io.h"
#include "../rt.h"

#define BUFFER_SIZE 1024

static void dummy_sigchld(
		int signo)
{}

enum {
	STDMODE_DISCARD,
	STDMODE_PARENT,
	STDMODE_IO,
	STDMODE_STRING
};

static void exec_copy_topipe(
		int *fd,
		char *buffer,
		gla_io_t *io)
{
	int bytes = io->meta->read(io, buffer, BUFFER_SIZE);
	if(bytes <= 0) {
		close(fd[1]);
		fd[1] = -1;
	}
	else {
		write(fd[1], buffer, bytes);
	}
}

static void exec_copy_frompipe(
		char *buffer,
		int *fd,
		gla_io_t *io,
		int mode)
{
	int bytes = read(fd[0], buffer, BUFFER_SIZE);
	if(bytes <= 0) {
		close(fd[0]);
		fd[0] = -1;
	}
	else if(io != NULL) {
		assert(io->meta->write != NULL);
		io->meta->write(io, buffer, bytes);
	}
}

SQInteger _gla_platform_fn_exec(
		HSQUIRRELVM vm)
{
	//TODO security note: access to whole system! maybe introduce flags which prevent using system(), fopen() and other related functions
	int ret;
	int pid;
	const SQChar *command;
	const SQChar *string_stdin;
	int offset_stdin = 0;
	int len_stdin = 0;
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
				mode_stdin = STDMODE_STRING;
				sq_getstring(vm, 3, &string_stdin);
				len_stdin = strlen(string_stdin);
				offset_stdin = 0;
				break;
			default:
				mode_stdin = STDMODE_IO;
				my_stdin = gla_mod_io_io_get(rt, 3);
				if(my_stdin == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 2: expected null, bool or input stream");
				else if(my_stdin->meta->read == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 2: stream for stdin not readable");
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
				mode_stdin = STDMODE_IO;
				my_stdout = gla_mod_io_io_get(rt, 4);
				if(my_stdout == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 3: expected null, bool or output stream");
				else if(my_stdout->meta->write == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 3: stream for stdout not writeable");
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
				mode_stdin = STDMODE_IO;
				my_stderr = gla_mod_io_io_get(rt, 5);
				if(my_stderr == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 4: expected null, bool or input stream");
				else if(my_stderr->meta->write == NULL)
					return gla_rt_vmthrow(rt, "invalid argument 4: stream for stderr not writeable");
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
	ret = pipe(pipe_stdin);
	if(ret < 0)
		goto error;
	ret = pipe(pipe_stdout);
	if(ret < 0)
		goto error;
	ret = pipe(pipe_stderr);
	if(ret < 0)
		goto error;

	pid = fork();
	if(pid == 0) {
		c = 0;
		close(pipe_ctrl[0]);
		if(mode_stdin != STDMODE_PARENT) {
			ret = dup2(pipe_stdin[0], fileno(stdin));
			if(ret == -1) {
				write(pipe_ctrl[1], &c, 1);
				exit(0);
			}
			close(pipe_stdin[0]);
			close(pipe_stdin[1]);
		}

		ret = dup2(pipe_stdout[1], fileno(stdout));
		if(ret == -1) {
			write(pipe_ctrl[1], &c, 1);
			exit(0);
		}
		close(pipe_stdout[0]);
		close(pipe_stdout[1]);

		ret = dup2(pipe_stderr[1], fileno(stderr));
		if(ret == -1) {
			write(pipe_ctrl[1], &c, 1);
			exit(0);
		}
		close(pipe_stderr[0]);
		close(pipe_stderr[1]);

		c = 1;
		write(pipe_ctrl[1], &c, 1);
		execvp(command, (char*const*)args);
		c = 2;
		write(pipe_ctrl[1], &c, 1);
		exit(0);
	}
	else if(pid > 0) {
		close(pipe_ctrl[1]);
		close(pipe_stderr[1]);
		close(pipe_stdout[1]);
		close(pipe_stdin[0]);
		pipe_ctrl[1] = -1;
		pipe_stderr[1] = -1;
		pipe_stdout[1] = -1;
		pipe_stdin[0] = -1;

		/* wait for child process to be setup */
		ret = read(pipe_ctrl[0], &c, 1);
		if(c == 0) {
			err_message = "error starting child process";
			goto error;
		}

		if(mode_stdin == STDMODE_DISCARD) {
			close(pipe_stdin[1]);
			pipe_stdin[1] = -1;
		}
		for(;;) {
			fd_set rfds;
			fd_set wfds;
			int nfds = 0;
			struct timeval timeout;
			timeout.tv_sec = 10;
			timeout.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			if(pipe_ctrl[0] != -1) {
				FD_SET(pipe_ctrl[0], &rfds);
				if(pipe_ctrl[0] > nfds)
					nfds = pipe_ctrl[0];
			}
			if(pipe_stdin[1] != -1) {
				FD_SET(pipe_stdin[1], &wfds);
				if(pipe_stdin[1] > nfds)
					nfds = pipe_stdin[1];
			}
			if(pipe_stdout[0] != -1) {
				FD_SET(pipe_stdout[0], &rfds);
				if(pipe_stdout[0] > nfds)
					nfds = pipe_stdout[0];
			}
			if(pipe_stderr[0] != -1) {
				FD_SET(pipe_stderr[0], &rfds);
				if(pipe_stderr[0] > nfds)
					nfds = pipe_stderr[0];
			}
			if(nfds == 0) { /* child process exited, either with error or success. err_message already has been set in fd_set handler for pipe_ctrl. */
				while((ret = waitpid(pid, &exitcode, 0)) != pid);
				if(err_message == NULL) {
					if(my_stdout != NULL && my_stdout->wstatus != GLA_SUCCESS)
						err_message = "error writing stdout";
					else if(my_stdout != NULL && my_stdout->wstatus != GLA_SUCCESS)
						err_message = "error writing stderr";
				}
				goto error;
			}
			ret = select(nfds + 1, &rfds, &wfds, NULL, &timeout);
			if(ret == 0 || (ret == -1 && errno == EINTR)) { /* timeout or interruption */
			}
			else if(ret == -1) { /* error */
				printf("error in select()"); /* TODO what to do here? possibly count down some retries of select() and throw error if countdown has reached 0 */
				sleep(1);
			}
			else { /* select() selected some fds for us */
				if(pipe_ctrl[0] != -1 && FD_ISSET(pipe_ctrl[0], &rfds)) { /* idea: if this pipe is closed, execvp() was successful. otherwise, the write() after execve() has been executed and execvp() did not succeed. so once we're here, we know if execution failed or not. */
					int bytes = read(pipe_ctrl[0], &c, 1);
					close(pipe_ctrl[0]);
					pipe_ctrl[0] = -1;
					if(bytes == 0) /* pipe has been closed, execvp() successful */
						err_message = NULL;
					else /* write() has been called, execvp() error */
						err_message = "error executing command";
				}
				if(pipe_stdin[1] != -1 && FD_ISSET(pipe_stdin[1], &wfds)) {
					switch(mode_stdin) {
						case STDMODE_STRING:
							if(offset_stdin == len_stdin) {
								close(pipe_stdin[1]);
								pipe_stdin[1] = -1;
							}
							else {
								ret = write(pipe_stdin[1], string_stdin, len_stdin - offset_stdin);
								if(ret <= 0) { /* TODO is this an error in every case/in any case? */
									close(pipe_stdin[1]);
									pipe_stdin[1] = -1;
								}
								else
									offset_stdin += ret;
							}
							break;
						case STDMODE_IO:
							exec_copy_topipe(pipe_stdin, buffer, my_stdin);
							break;
						case STDMODE_DISCARD:
							close(pipe_stdin[1]);
							pipe_stdin[1] = -1;
							break;
						default:
							assert(false);
					}
				}
				if(pipe_stdout[0] != -1 && FD_ISSET(pipe_stdout[0], &rfds))
					exec_copy_frompipe(buffer, pipe_stdout, my_stdout, mode_stdout);
				if(pipe_stderr[0] != -1 && FD_ISSET(pipe_stderr[0], &rfds))
					exec_copy_frompipe(buffer, pipe_stderr, my_stderr, mode_stderr);
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

