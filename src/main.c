#include <apr_strings.h>
#include <apr_general.h>
#include <apr_env.h>
#include <apr_file_io.h>
#include <pthread.h>

#include "common.h"

#include "rt.h"
#include "mnt_dir.h"

#ifdef TESTING
#include <CUnit/Basic.h>

int glatest_store();
int glatest_entityreg();
int glatest_mnt_internal();

int main(
		int argn,
		const char *const *argv)
{
	int ret;
	
	apr_app_initialize(&argn, &argv, NULL);
	apr_pool_initialize();

	ret = CU_initialize_registry();
	if(ret != CUE_SUCCESS) {
		printf("Error initializing CUnit.\n");
		goto error;
	}

	ret = glatest_store();
	if(ret != GLA_SUCCESS)
		goto error;
	ret = glatest_entityreg();
	if(ret != GLA_SUCCESS)
		goto error;
	ret = glatest_mnt_internal();
	if(ret != GLA_SUCCESS)
		goto error;

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	ret = CU_get_error();

error:
	CU_cleanup_registry();
	apr_pool_terminate();
	apr_terminate();
	return ret;
}
#else
#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>

#include "regex.h"

static const char *getcwd_pool(
		apr_pool_t *pool)
{
	long path_max;
	size_t size;
	char *buf = NULL;
	char *ptr = NULL;
	char *pbuf;

	path_max = pathconf(".", _PC_PATH_MAX);
	if(path_max == -1)
		size = 1024;
	else if(path_max > 10240)
		size = 10240;
	else
		size = path_max;

	for(; ptr == NULL; size *= 2) {
		buf = realloc(buf, size);
		if(buf == NULL)
			return NULL;
		ptr = getcwd(buf, size);
		if(ptr == NULL && errno != ERANGE)
			return NULL;
	}
	pbuf = apr_palloc(pool, strlen(ptr) + 1);
	strcpy(pbuf, ptr);
	free(buf);
	return pbuf;
}

static int add_path(
		gla_rt_t *rt,
		const char *pathname,
		int flags)
{
	int ret;
	gla_mount_t *mnt;

	mnt = gla_mount_dir_new(pathname, 0, rt->mpool);
	if(mnt == NULL) {
		fprintf(stderr, "error creating mount\n");
		return errno;
	}
	ret = gla_rt_mount(rt, mnt, flags);
	if(ret != GLA_SUCCESS) {
		fprintf(stderr, "error mounting package path\n");
		return ret;
	}
	return GLA_SUCCESS;
}

static int add_path_at(
		gla_rt_t *rt,
		const char *pathname,
		const char *package,
		int flags,
		int devflags,
		apr_pool_t *pool)
{
	int ret;
	gla_mount_t *mnt;
	gla_path_t package_path;

	ret = gla_path_parse_package(&package_path, package, pool);

	mnt = gla_mount_dir_new(pathname, devflags, rt->mpool);
	if(mnt == NULL) {
		fprintf(stderr, "error creating mount\n");
		return errno;
	}
	ret = gla_rt_mount_at(rt, mnt, flags, &package_path);
	if(ret != GLA_SUCCESS) {
		fprintf(stderr, "error mounting package path\n");
		return ret;
	}
	return GLA_SUCCESS;
}

int main(
		int argn,
		const char *const *argv)
{
	int ret;
	int i;
	apr_pool_t *global_pool;
	apr_pool_t *temp_pool;
	gla_rt_t *rt;
	gla_mount_t *mnt;
	char *envval;
	const char *tmpdir = NULL;
	const char *arg_entity = NULL;
	const char *arg_file = NULL;
	const char *cwd;
	bool arg_notmp = false; /* TODO parse argument */
	gla_path_t path;
	
	apr_app_initialize(&argn, &argv, NULL);
	apr_pool_initialize();

	ret = apr_pool_create(&global_pool, NULL);
	if(ret != APR_SUCCESS) {
		fprintf(stderr, "error creating global memory pool\n");
		goto error;
	}

	ret = apr_pool_create(&temp_pool, NULL);
	if(ret != APR_SUCCESS) {
		fprintf(stderr, "error creating temporary memory pool\n");
		goto error;
	}

	ret = gla_regex_init(global_pool);
	if(ret != GLA_SUCCESS) {
		fprintf(stderr, "error initializing regular expressions\n");
		goto error;
	}

	for(i = 1; i < argn; i++)
		if(strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}

	rt = gla_rt_new(argv + i, argn - i, global_pool);
	if(rt == NULL) {
		fprintf(stderr, "error creating new runtime\n");
		ret = errno;
		goto error;
	}

	if(tmpdir == NULL && !arg_notmp) {
		ret = gla_path_parse_package(&path, "tmp", rt->mpool);
		if(ret != 0) {
			fprintf(stderr, "error parsing path\n");
			goto error;
		}
		ret = apr_temp_dir_get(&tmpdir, temp_pool);
		if(ret != APR_SUCCESS) {
			fprintf(stderr, "error retrieving temp directory");
			goto error;
		}
		mnt = gla_mount_dir_new(tmpdir, 0, rt->mpool);
		if(mnt == NULL) {
			fprintf(stderr, "error creating mount\n");
			ret = errno;
			goto error;
		}
		ret = gla_rt_mount_at(rt, mnt, GLA_MOUNT_SOURCE | GLA_MOUNT_TARGET, &path);
		if(ret != GLA_SUCCESS) {
			fprintf(stderr, "error mounting initial path\n");
			goto error;
		}
	}

	ret = apr_env_get(&envval, "GLA_SQLIB_PATH", temp_pool);
	if(ret == APR_SUCCESS) {
		char *start = envval;
		char *cur = start;
		bool finished = false;
		while(!finished) {
			finished = *cur == 0;
			if(*cur == ':' || *cur == 0) {
				*cur = 0;
				if(start < cur)
					add_path(rt, start, GLA_MOUNT_SOURCE);
				start = cur + 1;
			}
			cur++;
		}
	}

	/* TODO add command-line switch to enable/disable modules; where to get name from? directory? */
	ret = apr_env_get(&envval, "GLA_SQMOD_PATH", temp_pool);
	if(ret == APR_SUCCESS) {
		char *start = envval;
		char *cur = start;
		bool finished = false;
		apr_dir_t *dir;
		apr_finfo_t info;
		const char *path;
		while(!finished) {
			finished = *cur == 0;
			if(*cur == ':' || *cur == 0) {
				*cur = 0;
				if(start < cur) {
					ret = apr_dir_open(&dir, start, temp_pool);
					if(ret == APR_SUCCESS) {
						for(;;) {
							ret = apr_dir_read(&info, APR_FINFO_DIRENT, dir);
							if(ret == APR_SUCCESS && strcmp(info.name, ".") != 0 && strcmp(info.name, "..") != 0) {
								path = apr_psprintf(temp_pool, "%s/%s", start, info.name);
								add_path(rt, path, GLA_MOUNT_SOURCE); /* TODO (also see liens above and below): report errors or just skip directories, which cause errors? */
							}
							else if(ret == APR_ENOENT)
								break;
						}
						apr_dir_close(dir);
					}
				}
				start = cur + 1;
			}
			cur++;
		}
	}

	for(i = 1; i < argn; i++) {
		if(argv[i][0] != '-') {
			if(arg_entity != NULL) {
				fprintf(stderr, "invalid argument %d: script entity to execute specified multiple times\n", i);
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			else if(arg_file != NULL) {
				fprintf(stderr, "invalid parameter '-f': must not be given if script entity to execute is set\n");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			arg_entity = argv[i];
		}
		else if(strcmp(argv[i], "-f") == 0) { /* file to execute */
			i++;
			if(i == argn || argv[i][0] == '-') {
				fprintf(stderr, "missing value for parameter '-f'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			else if(arg_file != NULL) {
				fprintf(stderr, "invalid parameter '-f': must not be specified multiple times\n");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			else if(arg_entity != NULL) {
				fprintf(stderr, "invalid parameter '-f': must not be given if script entity to execute is set\n");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			arg_file = argv[i];
		}
		else if(strcmp(argv[i], "-s") == 0) { /* source */
			i++;
			if(i == argn || argv[i][0] == '-') {
				fprintf(stderr, "missing value for parameter '-s'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			if(add_path(rt, argv[i], GLA_MOUNT_SOURCE) != GLA_SUCCESS)
				goto error;
		}
		else if(strcmp(argv[i], "-t") == 0) { /* target */
			i++;
			if(i == argn || argv[i][0] == '-') {
				fprintf(stderr, "missing value for parameter '-t'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			if(add_path(rt, argv[i], GLA_MOUNT_TARGET) != GLA_SUCCESS)
				goto error;
		}
		else if(strcmp(argv[i], "-p") == 0) { /* source and target */
			i++;
			if(i == argn || argv[i][0] == '-') {
				fprintf(stderr, "missing value for parameter '-p'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			if(add_path(rt, argv[i], GLA_MOUNT_TARGET | GLA_MOUNT_SOURCE) != GLA_SUCCESS)
				goto error;
		}
		else if(strcmp(argv[i], "-m") == 0) { /* mountpoint */
			const char *si;
			char *package;
			i++;
			if(i == argn || argv[i][0] == '-') {
				fprintf(stderr, "missing value for parameter '-m'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			for(si = argv[i]; *si != 0 && *si != '='; si++);
			if(*si == 0) {
				fprintf(stderr, "invalid value for parameter '-m': expected 'package'='filesystem path'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			package = apr_pcalloc(temp_pool, si - argv[i] + 1);
			if(package == NULL) {
				fprintf(stderr, "error allocating temporary package string");
				ret = GLA_ALLOC;
				goto error;
			}
			memcpy(package, argv[i], si - argv[i]);
			si++;
			if(*si == 0) {
				fprintf(stderr, "invalid value for parameter '-m': expected 'package'='filesystem path'");
				ret = GLA_INVALID_ARGUMENT;
				goto error;
			}
			ret = add_path_at(rt, si, package, GLA_MOUNT_SOURCE | GLA_MOUNT_TARGET, 0, temp_pool);
		}
		else if(strcmp(argv[i], "--") == 0)
			break;
	}

	cwd = getcwd_pool(temp_pool);
	if(cwd == NULL) {
		fprintf(stderr, "error getting current working directory\n");
		ret = GLA_ALLOC;
		goto error;
	}
	ret = add_path_at(rt, cwd, "sys.cwd", GLA_MOUNT_SOURCE | GLA_MOUNT_TARGET, GLA_MNT_DIR_FILENAMES, temp_pool);
	if(arg_entity != NULL) {
		ret = gla_path_parse_entity(&path, arg_entity, global_pool);
		if(ret != GLA_SUCCESS) {
			fprintf(stderr, "error parsing entity path\n");
			goto error;
		}

		apr_pool_destroy(temp_pool);
		temp_pool = NULL;

		ret = gla_rt_boot_entity(rt, &path);
	}
	else if(arg_file != NULL) {
		char *full;
		char *dir;

		full = apr_palloc(temp_pool, strlen(arg_file) + 1);
		if(full == NULL) {
			fprintf(stderr, "error allocating temporary string\n");
			ret = GLA_ALLOC;
			goto error;
		}
		strcpy(full, arg_file);
		dir = dirname(full);

		ret = add_path_at(rt, dir, "sys.root", GLA_MOUNT_SOURCE | GLA_MOUNT_TARGET, 0, temp_pool);

		apr_pool_destroy(temp_pool);
		temp_pool = NULL;
		ret = gla_rt_boot_file(rt, arg_file);
	}
	else {
/*		apr_file_t *file;
		ret = apr_file_open_stdin(&file, temp_pool);
		if(ret != APR_SUCCESS) {
			fprintf(stderr, "error opening stdin\n");
			ret = GLA_IO;
			goto error;
		}
		while(true) {
			apr_file_gets()
		}*/
		fprintf(stderr, "missing argument: parameter '-f' or script entity to execute must be given\n");
		ret = GLA_INVALID_ARGUMENT;
		goto error;
	}

error:
	apr_pool_terminate();
	apr_terminate();
//	pthread_exit(NULL); /* suppress valgrind leak report from dlopen() */
	if(ret == 0)
		return 0;
	else
		return 1;
}
#endif

