#include "common.h"

#include "src/io.h"
#include "src/regex.h"
#include "src/path.h"
#include "src/mount.h"

static apr_pool_t *test_pool;
static gla_mount_t *test_self;
static gla_store_t *test_store;

static int test1_init()
{
	int ret;

	ret = apr_pool_create(&test_pool, NULL);
	if(ret != APR_SUCCESS)
		return ret;

	ret = gla_regex_init(test_pool);
	if(ret != GLA_SUCCESS)
		return ret;

	test_store = gla_store_new(test_pool);
	if(test_store == NULL)
		return errno;

	test_self = gla_mount_internal_new(test_store, test_pool);
	if(test_self == NULL)
		return errno;
	return 0;
}

static int test1_cleanup()
{
	apr_pool_destroy(test_pool);
	return GLA_SUCCESS;
}

static void test1_members()
{
	CU_ASSERT_PTR_NOT_NULL_FATAL(mnt_meta.entities);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mnt_meta.packages);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mnt_meta.open);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mnt_meta.push);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mnt_meta.info);
	CU_ASSERT_PTR_NULL(mnt_meta.touch);
	CU_ASSERT_PTR_NULL(mnt_meta.tofilepath);
}

static void test1_unsupported()
{
	int ret;
	const char *filename = NULL;

	ret = gla_mount_touch(test_self, NULL, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTSUPPORTED);

	filename = gla_mount_tofilepath(test_self, NULL, test_pool);
	CU_ASSERT_EQUAL(errno, GLA_NOTSUPPORTED);
	CU_ASSERT_PTR_NULL(filename);
}

static int test1_access_user = 50;

static int test1_access_cb_push(
		gla_rt_t *rt,
		void *user,
		apr_pool_t *pperm,
		apr_pool_t *ptemp)
{
	int *v = user;

	CU_ASSERT_PTR_EQUAL_FATAL(v, &test1_access_user);
	*v = 40;
	return GLA_SUCCESS;
}

#define INSERT_ENTITY(NAME) \
	do { \
		ret = gla_path_parse_entity(&path, NAME, test_pool); \
		CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS); \
		ret = gla_mount_internal_add_object(test_self, &path, test1_access_cb_push, &test1_access_user); \
		CU_ASSERT_EQUAL(ret, GLA_SUCCESS); \
	} while(false)

static const char test_buffer[] = "This is a test buffer.";

static void test1_access()
{
	int ret;
	gla_path_t path;

	/* add test read buffer entity */
	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_internal_add_read_buffer(test_self, &path, test_buffer, sizeof(test_buffer) - 1);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* add test object entity */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_internal_add_object(test_self, &path, test1_access_cb_push, &test1_access_user);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* same entity twice */
	ret = gla_mount_internal_add_object(test_self, &path, test1_access_cb_push, &test1_access_user);
	CU_ASSERT_EQUAL(ret, GLA_ALREADY);
	ret = gla_mount_internal_add_read_buffer(test_self, &path, test_buffer, sizeof(test_buffer) - 1);
	CU_ASSERT_EQUAL(ret, GLA_ALREADY);

	/* add test object entity in root package */
	ret = gla_path_parse_entity(&path, "RootEntity", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_internal_add_object(test_self, &path, test1_access_cb_push, &test1_access_user);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* register additional entities for later usage */
	INSERT_ENTITY("test.sub.package.AnotherEntity");
	INSERT_ENTITY("test.sub2.package.AnotherEntity");
	INSERT_ENTITY("test2.Entity2");
	INSERT_ENTITY("test.Entity");
	INSERT_ENTITY("RootEntity2");
	INSERT_ENTITY("<ext>RootEntityWithExtension");
}

static void test1_push()
{
	int ret;
	gla_path_t path;

	/* perform 'push' on previously registered object */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	test1_access_user = 60;
	ret = gla_mount_push(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(test1_access_user, 40);

	/* perform 'push' on previously registered buffer */
	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_push(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_INVALID_ACCESS);

	/* perform 'push' on package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_push(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_INVALID_PATH_TYPE);

	/* perform 'push' on root */
	ret = gla_mount_push(test_self, NULL, NULL, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_INVALID_PATH_TYPE);

	/* perform 'push' on unregistered entity */
	ret = gla_path_parse_entity(&path, "test.NoEntity", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_push(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
}

static void test1_open()
{
	int ret;
	gla_path_t path;
	gla_io_t *io = NULL;

	/* perform 'open' on previously registered buffer */
	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	io = gla_mount_open(test_self, &path, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NOT_NULL(io);

	/* try opening in truncate mode */
	io = gla_mount_open(test_self, &path, GLA_MODE_TRUNCATE, test_pool);
	CU_ASSERT_PTR_NULL(io);

	/* try opening in append mode */
	io = gla_mount_open(test_self, &path, GLA_MODE_APPEND, test_pool);
	CU_ASSERT_PTR_NULL(io);

	/* perform 'open' on previously registered object */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	io = gla_mount_open(test_self, &path, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NULL(io);

	/* perform 'open' on package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	io = gla_mount_open(test_self, &path, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NULL(io);
	CU_ASSERT_EQUAL(errno, GLA_INVALID_PATH_TYPE);

	/* perform 'open' on root */
	io = gla_mount_open(test_self, NULL, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NULL(io);
	CU_ASSERT_EQUAL(errno, GLA_INVALID_PATH_TYPE);

	/* perform 'open' on unregistered entity */
	ret = gla_path_parse_entity(&path, "test.NoEntity", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	io = gla_mount_open(test_self, &path, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NULL(io);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);
}

static void test1_buffer_io()
{
	int ret;
	gla_path_t path;
	gla_io_t *io = NULL;
	char buffer[sizeof(test_buffer)];
	int offset = 0;

	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	io = gla_mount_open(test_self, &path, GLA_MODE_READ, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(io);

	/* check initial status */
	ret = gla_io_status(io);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* read some bytes */
	ret = gla_io_read(io, buffer + offset, 2);
	CU_ASSERT_EQUAL_FATAL(ret, 2);
	offset += ret;
	ret = gla_io_status(io);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* read remaining (overshoot)  */
	ret = gla_io_read(io, buffer + offset, 100000);
	CU_ASSERT_EQUAL_FATAL(ret, sizeof(test_buffer) - offset - 1);
	offset += ret;
	ret = gla_io_status(io);
	CU_ASSERT_EQUAL(ret, GLA_END);
	buffer[offset] = 0;
	CU_ASSERT_EQUAL(strcmp(buffer, test_buffer), 0);

	/* read after end */
	ret = gla_io_read(io, buffer, 10);
	CU_ASSERT_EQUAL(ret, 0);

	/* close io */
	gla_io_close(io);
	ret = gla_io_status(io);
	CU_ASSERT_EQUAL(ret, GLA_CLOSED);

	/* read from closed */
	ret = gla_io_read(io, buffer, 10);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test1_info()
{
	int ret;
	gla_path_t path;
	gla_pathinfo_t info;

	/* request info of previously registered object */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_PUSH);

	/* request info of previously registered buffer */
	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of previously used package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of root package */
	ret = gla_mount_info(test_self, &info, NULL, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of unregistered entity */
	ret = gla_path_parse_entity(&path, "test.NoEntity", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);

	/* request info of unused package */
	ret = gla_path_parse_entity(&path, "nopackage", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
}

static void test1_info_exists()
{
	int ret;
	gla_path_t path;
	gla_pathinfo_t info;

	/* request info of previously registered object */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_PUSH);

	/* request info of previously registered buffer */
	ret = gla_path_parse_entity(&path, "test.Buffer", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of previously used package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, &info, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of root package */
	ret = gla_mount_info(test_self, &info, NULL, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_EQUAL(info.access, GLA_ACCESS_READ);

	/* request info of unregistered entity */
	ret = gla_path_parse_entity(&path, "test.NoEntity", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);

	/* request info of unused package */
	ret = gla_path_parse_entity(&path, "nopackage", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_mount_info(test_self, NULL, &path, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
}

void test1_entities()
{
	int ret;
	gla_path_t path;
	gla_mountit_t *it = NULL;
	const char *name;
	const char *extension;

	/* iterate root */
	it = gla_mount_entities(test_self, NULL, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NULL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "RootEntity"), 0);

	/* next */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NULL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "RootEntity2"), 0);

	/* next */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NOT_NULL_FATAL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "RootEntityWithExtension"), 0);
	CU_ASSERT_EQUAL(strcmp(extension, "ext"), 0);

	/* next (end of iteration) */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL(ret, GLA_END);


	/* iterate package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_entities(test_self, &path, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NULL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "Buffer"), 0);

	/* next */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NULL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "Entity"), 0);

	/* next */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_PTR_NULL(extension);
	CU_ASSERT_EQUAL(strcmp(name, "Object"), 0);

	/* next (end of iteration) */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL(ret, GLA_END);


	/* iterate entity path */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_entities(test_self, &path, test_pool);
	CU_ASSERT_PTR_NULL(it);
	CU_ASSERT_EQUAL(errno, GLA_INVALID_PATH_TYPE);

	/* iterate non-existent package */
	ret = gla_path_parse_package(&path, "test.nopackage", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_entities(test_self, &path, test_pool);
	CU_ASSERT_PTR_NULL(it);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);

	/* iterate empty package */
	ret = gla_path_parse_package(&path, "test.sub", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_entities(test_self, &path, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, &extension);
	CU_ASSERT_EQUAL(ret, GLA_END);
}

void test1_packages()
{
	int ret;
	gla_path_t path;
	gla_mountit_t *it = NULL;
	const char *name;

	/* iterate root */
	it = gla_mount_packages(test_self, NULL, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_EQUAL(strcmp(name, "test2"), 0);
	/* NOTE: if the strcmp-assertions fail, check the order of the iteration.
	 * currently: FILO */

	/* next */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_EQUAL(strcmp(name, "test"), 0);

	/* next (end of iteration) */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_END);


	/* iterate package */
	ret = gla_path_parse_package(&path, "test", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_packages(test_self, &path, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_EQUAL(strcmp(name, "sub2"), 0);

	/* next */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL_FATAL(name);
	CU_ASSERT_EQUAL(strcmp(name, "sub"), 0);

	/* next (end of iteration) */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_END);


	/* iterate entity path */
	ret = gla_path_parse_entity(&path, "test.Object", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_packages(test_self, &path, test_pool);
	CU_ASSERT_PTR_NULL(it);
	CU_ASSERT_EQUAL(errno, GLA_INVALID_PATH_TYPE);

	/* iterate non-existent package */
	ret = gla_path_parse_package(&path, "test.nopackage", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_packages(test_self, &path, test_pool);
	CU_ASSERT_PTR_NULL(it);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);

	/* iterate empty package */
	ret = gla_path_parse_package(&path, "test2", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	it = gla_mount_packages(test_self, &path, test_pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(it);

	/* first */
	ret = gla_mountit_next(it, &name, NULL);
	CU_ASSERT_EQUAL(ret, GLA_END);
}

int glatest_mnt_internal()
{
	CU_pSuite suite;
	CU_pTest test;

	BEGIN_SUITE("Mount handler 'Internal' API", test1_init, test1_cleanup);
		ADD_TEST("all member functions set", test1_members);
		ADD_TEST("unsupported operations", test1_unsupported);
		ADD_TEST("access", test1_access);
		ADD_TEST("push operation", test1_push);
		ADD_TEST("open operation", test1_open);
		ADD_TEST("buffer io", test1_buffer_io);
		ADD_TEST("info operation", test1_info);
		ADD_TEST("info operation: existence check only", test1_info_exists);
		ADD_TEST("entities operation", test1_entities);
		ADD_TEST("packages operation", test1_packages);
	END_SUITE;

	return GLA_SUCCESS;
}

