#include "common.h"

static apr_pool_t *test_pool;
static gla_store_t *test_store;
static gla_entityreg_t *test_self;

/********************************** TEST 1 ************************************/

#include "src/regex.h"

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
	test_self = gla_entityreg_new(test_store, sizeof(int), test_pool);
	if(test_self == NULL)
		return errno;
	return 0;
}

static int test1_cleanup()
{
	apr_pool_destroy(test_pool);
	return 0;
}

#define INSERT_ENTITY(NAME) \
	do { \
		ret = gla_path_parse_entity(&path, NAME, test_pool); \
		CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS); \
		data = gla_entityreg_get(test_self, &path); \
		CU_ASSERT_PTR_NOT_NULL_FATAL(data); \
	} while(false)

static void test1_access()
{
	int ret;
	int *data;
	int *orgdata;
	gla_path_t path;
	gla_path_t path_rev;
	gla_id_t id;
	gla_id_t orgid;

	CU_ASSERT_PTR_NOT_NULL_FATAL(test_store);

	/* test root existence check */
	ret = gla_entityreg_covered(test_self, NULL);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* insert new entity into registry */
	ret = gla_path_parse_entity(&path, "<ext>a.aa.EntityName", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	data = gla_entityreg_get(test_self, &path);
	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT_EQUAL(errno, GLA_NOTFOUND);
	orgdata = data;

	/* retrieve entity path from data */
	ret = gla_entityreg_path_for(test_self, &path_rev, data, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_TRUE(gla_path_equal(&path, &path_rev));

	/* retrieve same entity using try-method */
	data = gla_entityreg_try(test_self, &path);
	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT_PTR_EQUAL(data, orgdata);

	/* retrieve same entitiy using get-method */
	data = gla_entityreg_get(test_self, &path);
	CU_ASSERT_EQUAL(errno, GLA_SUCCESS);
	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT_PTR_EQUAL(data, orgdata);

	/* insert another new entity into registry */
	ret = gla_path_parse_entity(&path, "<ext>a.ab.EntityName", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	data = gla_entityreg_get(test_self, &path);
	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT_PTR_NOT_EQUAL(data, orgdata);

	/* insert new entity into root package */
	ret = gla_path_parse_entity(&path, "<ext>EntityName", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	data = gla_entityreg_get(test_self, &path);
	CU_ASSERT_PTR_NOT_NULL(data);
	CU_ASSERT_PTR_NOT_EQUAL(data, orgdata);

	/* retrieve entity path from data */
	ret = gla_entityreg_path_for(test_self, &path_rev, data, test_pool);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);
	CU_ASSERT_TRUE(gla_path_equal(&path, &path_rev));

	/* add some other packages to store which are not used in registry */
	ret = gla_path_parse_package(&path, "b", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	id = gla_store_path_get(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID);
	ret = gla_path_parse_package(&path, "a.aa.aaa", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	id = gla_store_path_get(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID);

	/* get root package first child */
	id = gla_entityreg_covered_children(test_self, GLA_ID_ROOT);
	ret = gla_path_parse_package(&path, "a", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	orgid = gla_store_path_try(test_store, &path);
	CU_ASSERT_NOT_EQUAL(orgid, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(orgid, id);

	/* get next sibling */
	errno = GLA_TEST;
	id = gla_entityreg_covered_next(test_self, orgid);
	CU_ASSERT_EQUAL(id, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(errno, GLA_END);

	/* get leaf package first child */
	ret = gla_path_parse_package(&path, "a.aa", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	id = gla_store_path_try(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(id, GLA_ID_INVALID);
	errno = GLA_TEST;
	id = gla_entityreg_covered_children(test_self, id);
	CU_ASSERT_EQUAL(id, GLA_ID_INVALID);
	CU_ASSERT_EQUAL(errno, GLA_END);

	/* test root existence check again */
	ret = gla_entityreg_covered(test_self, NULL);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* test positive existence check */
	ret = gla_path_parse_package(&path, "a", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_entityreg_covered(test_self, &path);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	ret = gla_path_parse_package(&path, "a.aa", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_entityreg_covered(test_self, &path);
	CU_ASSERT_EQUAL(ret, GLA_SUCCESS);

	/* test negative existence check, contained within store */
	ret = gla_path_parse_package(&path, "b", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_entityreg_covered(test_self, &path);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);

	/* test negative existence check, not contained within store */
	ret = gla_path_parse_package(&path, "c", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	ret = gla_entityreg_covered(test_self, &path);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);

	/* insert several other entities (for later usage) */
	INSERT_ENTITY("<exta>z.za.EntityA");
	INSERT_ENTITY("<extc>z.za.EntityC");
	INSERT_ENTITY("<extb>z.za.EntityB");
	INSERT_ENTITY("<extd>z.za.EntityD");
	INSERT_ENTITY("<exte>z.zb.EntityE");
}

#define EXPECT_ENTRY(NAME, EXTENSION) \
	do {\
		CU_ASSERT_PTR_NOT_NULL_FATAL(it.name); \
		CU_ASSERT_PTR_NOT_NULL_FATAL(it.extension); \
		CU_ASSERT_EQUAL(it.package, package); \
		CU_ASSERT_EQUAL(strcmp(it.name, NAME), 0); \
		CU_ASSERT_EQUAL(strcmp(it.extension, EXTENSION), 0); \
	} while(false)

static void test1_forward_iteration()
{
	int ret;
	gla_path_t path;
	gla_id_t package;
	gla_entityreg_it_t it;

	/* package: z.za */
	ret = gla_path_parse_package(&path, "z.za", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	package = gla_store_path_try(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(package, GLA_ID_INVALID);

	/* forward iteration */
	ret = gla_entityreg_find_first(test_self, package, &it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityA", "exta");

	ret = gla_entityreg_iterate_next(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityB", "extb");

	ret = gla_entityreg_iterate_next(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityC", "extc");

	ret = gla_entityreg_iterate_next(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityD", "extd");

	ret = gla_entityreg_iterate_next(&it);
	CU_ASSERT_EQUAL(ret, GLA_END);
	EXPECT_ENTRY("EntityD", "extd");
}

static void test1_reverse_iteration()
{
	int ret;
	gla_path_t path;
	gla_id_t package;
	gla_entityreg_it_t it;

	/* set current package: z.za */
	ret = gla_path_parse_package(&path, "z.za", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	package = gla_store_path_try(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(package, GLA_ID_INVALID);

	/* forward iteration */
	ret = gla_entityreg_find_last(test_self, package, &it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityD", "extd");

	ret = gla_entityreg_iterate_prev(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityC", "extc");

	ret = gla_entityreg_iterate_prev(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityB", "extb");

	ret = gla_entityreg_iterate_prev(&it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityA", "exta");

	ret = gla_entityreg_iterate_prev(&it);
	CU_ASSERT_EQUAL(ret, GLA_END);
	EXPECT_ENTRY("EntityA", "exta");
}

void test1_empty_iteration()
{
	int ret;
	gla_id_t package;
	gla_entityreg_it_t it;
	gla_path_t path;

	/* set current package: a */
	ret = gla_path_parse_package(&path, "a", test_pool);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	package = gla_store_path_try(test_store, &path);
	CU_ASSERT_NOT_EQUAL_FATAL(package, GLA_ID_INVALID);

	/* test for NOTFOUND result */
	ret = gla_entityreg_find_first(test_self, package, &it);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
	ret = gla_entityreg_find_last(test_self, package, &it);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);

	/* test invalid package for NOTFOUND result */
	ret = gla_entityreg_find_first(test_self, GLA_ID_INVALID, &it);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
	ret = gla_entityreg_find_last(test_self, GLA_ID_INVALID, &it);
	CU_ASSERT_EQUAL(ret, GLA_NOTFOUND);
}

void test1_root_iteration()
{
	int ret;
	gla_entityreg_it_t it;
	gla_id_t package = GLA_ID_ROOT;

	ret = gla_entityreg_find_first(test_self, GLA_ID_ROOT, &it);
	CU_ASSERT_EQUAL_FATAL(ret, GLA_SUCCESS);
	EXPECT_ENTRY("EntityName", "ext");

	ret = gla_entityreg_iterate_next(&it);
	CU_ASSERT_EQUAL(ret, GLA_END);
}

/******************************** INITIALIZE **********************************/

int glatest_entityreg()
{
	CU_pSuite suite;
	CU_pTest test;

	BEGIN_SUITE("Entity Registry API", test1_init, test1_cleanup);
		ADD_TEST("access", test1_access);
		ADD_TEST("forward iteration", test1_forward_iteration);
		ADD_TEST("reverse iteration", test1_reverse_iteration);
		ADD_TEST("empty iteration", test1_empty_iteration);
		ADD_TEST("root iteration", test1_root_iteration);
	END_SUITE;

	return GLA_SUCCESS;
}

