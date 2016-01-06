#include "common.h"

static apr_pool_t *test_pool;
static gla_store_t *test_self;

/********************************** TEST 1 ************************************/

static int test1_init()
{
	int ret;

	ret = apr_pool_create(&test_pool, NULL);
	if(ret != APR_SUCCESS)
		return ret;

	test_self = gla_store_new(test_pool);
	if(test_self == NULL)
		return errno;
	return 0;
}

static int test1_cleanup()
{
	apr_pool_destroy(test_pool);
	return 0;
}

static void test1_get_string()
{
	const char *origin = "A String";
	const char *string;

	string = gla_store_string_get(test_self, origin);
	CU_ASSERT_PTR_NOT_NULL_FATAL(string);
	CU_ASSERT_EQUAL(strcmp(string, origin), 0);
	CU_ASSERT_PTR_NOT_EQUAL(string, origin);
}

static void test1_try_string()
{
	const char *string;

	errno = GLA_TEST;
	string = gla_store_string_try(test_self, "Not inserted");
	CU_ASSERT_PTR_NULL(string);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);
}

static void test1_get_null_string()
{
	const char *string;

	errno = GLA_TEST;
	string = gla_store_string_get(test_self, NULL);
	CU_ASSERT_PTR_NULL(string);
	CU_ASSERT_EQUAL(errno, GLA_SUCCESS);
}

static void test1_try_null_string()
{
	const char *string;

	errno = GLA_TEST;
	string = gla_store_string_try(test_self, NULL);
	CU_ASSERT_PTR_NULL(string);
	CU_ASSERT_EQUAL(errno, GLA_SUCCESS);
}

/********************************** TEST 2 ************************************/

#define TEST2_ADD_PACKAGE(PATH, VAR) \
	do { \
		ret = gla_path_parse_package(&path, PATH, test_pool); \
		if(ret != GLA_SUCCESS) {\
			printf("ERROR PARSING %s: %d\n", PATH, ret); \
			return ret; \
		} \
		VAR = gla_store_path_get(test_self, &path); \
		if(VAR == GLA_ID_INVALID) \
		  return errno; \
	} while(false)

static const char *test2_existing;
static gla_id_t test2_a;
static gla_id_t test2_aa;
static gla_id_t test2_ab;
static gla_id_t test2_b;
static gla_id_t test2_ba;
static gla_id_t test2_bb;

#include "src/regex.h"

static int test2_init()
{
	int ret;
	gla_path_t path;

	ret = apr_pool_create(&test_pool, NULL);
	if(ret != APR_SUCCESS)
		return ret;

	ret = gla_regex_init(test_pool);
	if(ret != GLA_SUCCESS)
		return ret;

	test_self = gla_store_new(test_pool);
	if(test_self == NULL)
		return errno;

	test2_existing = gla_store_string_get(test_self, "An existing string");
	if(test2_existing == NULL)
		return errno;

	TEST2_ADD_PACKAGE("a", test2_a);
	TEST2_ADD_PACKAGE("a.aa", test2_aa);
	TEST2_ADD_PACKAGE("a.ab", test2_ab);
	TEST2_ADD_PACKAGE("b", test2_b);
	TEST2_ADD_PACKAGE("b.ba", test2_ba);
	TEST2_ADD_PACKAGE("b.bb", test2_bb);

	return 0;
}

static int test2_cleanup()
{
	apr_pool_destroy(test_pool);
	return 0;
}

static void test2_get_string()
{
	char *buffer;
	const char *string;

	buffer = apr_palloc(test_pool, strlen(test2_existing) + 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buffer);
	strcpy(buffer, test2_existing);
	string = gla_store_string_get(test_self, buffer);
	CU_ASSERT_PTR_EQUAL(string, test2_existing);
}

static void test2_try_string()
{
	char *buffer;
	const char *string;

	buffer = apr_palloc(test_pool, strlen(test2_existing) + 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buffer);
	strcpy(buffer, test2_existing);
	string = gla_store_string_try(test_self, buffer);
	CU_ASSERT_PTR_EQUAL(string, test2_existing);
}

static void test2_root_children()
{
	gla_id_t id;

	id = gla_store_path_first_child(test_self, GLA_ID_ROOT);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_b); /* current child sequence: FILO */
	id = gla_store_path_next_sibling(test_self, id);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_a); /* current child sequence: FILO */
	errno = GLA_TEST;
	id = gla_store_path_next_sibling(test_self, id);
	CU_ASSERT_EQUAL(id, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(errno, GLA_END);
}

static void test2_parents()
{
	gla_id_t id;

	/* root as parent */
	id = gla_store_path_parent(test_self, test2_a);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, GLA_ID_ROOT);
	id = gla_store_path_parent(test_self, test2_b);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, GLA_ID_ROOT);

	/* 'a' as parent */
	id = gla_store_path_parent(test_self, test2_aa);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_a);
	id = gla_store_path_parent(test_self, test2_ab);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_a);

	/* 'b' as parent */
	id = gla_store_path_parent(test_self, test2_ba);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_b);
	id = gla_store_path_parent(test_self, test2_bb);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID); /* capture errors here */
	CU_ASSERT_EQUAL(id, test2_b);

	/* parent of root */
	errno = GLA_TEST;
	id = gla_store_path_parent(test_self, GLA_ID_ROOT);
	CU_ASSERT_EQUAL(id, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(errno, GLA_END);

	/* parent of non-existing */
	errno = GLA_TEST;
	id = gla_store_path_parent(test_self, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(id, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);
}

/******************************** INITIALIZE **********************************/

int glatest_store()
{
	CU_pSuite suite;
	CU_pTest test;

	BEGIN_SUITE("Store API 1 - initial operations", test1_init, test1_cleanup);
		ADD_TEST("get string", test1_get_string);
		ADD_TEST("try string", test1_try_string);
		ADD_TEST("get null string", test1_get_null_string);
		ADD_TEST("try null string", test1_try_null_string);
	END_SUITE;

	BEGIN_SUITE("Store API 2 - consecutive operations", test2_init, test2_cleanup);
		ADD_TEST("get existing string", test2_get_string);
		ADD_TEST("try existing string", test2_try_string);
		ADD_TEST("root path children", test2_root_children);
		ADD_TEST("parents", test2_parents);
	END_SUITE;

	return GLA_SUCCESS;
}

