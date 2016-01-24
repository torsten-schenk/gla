#include <stdio.h>

#include "common.h"

#include "io/module.h"

#include "rt.h"
#include "regex.h"
#include "log.h"

int gla_path_parse_package(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool)
{
	bool isfinal;
	const char *si;
	int ret;
	int i;

	memset(path, 0, sizeof(*path));
	if(string == NULL || *string == 0) /* root package */
		return GLA_SUCCESS;
	si = string;
	isfinal = false;
	while(!isfinal) {
		ret = gla_regex_path_fragment(si, -1, NULL, &isfinal, NULL);
		if(ret < 0)
			return ret;
		else if(ret == 0)
			return GLA_INVALID_PATH;
		si += ret;
		if(isfinal && *si != 0)
			return GLA_INVALID_PATH;
		else if(*si == 0 && !isfinal)
			return GLA_INVALID_PATH;
		path->size++;
		if(path->size > GLA_PATH_MAX_DEPTH)
			return GLA_OVERFLOW;
	}
	si = string;
	for(i = 0; i < path->size; i++) {
		ret = gla_regex_path_fragment(si, -1, path->package + i, NULL, pool);
		if(ret < 0)
			return ret;
		si += ret;
	}

	return GLA_SUCCESS;
}

int gla_path_parse_entity(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool)
{
	bool isfinal;
	const char *si;
	int ret;
	int i;

	si = string;
	memset(path, 0, sizeof(*path));
	ret = gla_regex_path_extension(si, -1, NULL, NULL);
	if(ret < 0)
		return ret;
	si += ret;
	while(true) {
		ret = gla_regex_path_fragment(si, -1, NULL, &isfinal, NULL);
		if(ret < 0)
			return ret;
		else if(ret == 0)
			return GLA_INVALID_PATH;
		si += ret;
		if(isfinal && *si != 0)
			return GLA_INVALID_PATH;
		else if(*si == 0 && !isfinal)
			return GLA_INVALID_PATH;
		if(isfinal) /* this is the entity fragment */
			break;
		path->size++;
		if(path->size > GLA_PATH_MAX_DEPTH)
			return GLA_OVERFLOW;
	}
	si = string;
	ret = gla_regex_path_extension(si, -1, &path->extension, pool);
	if(ret < 0)
		return ret;
	si += ret; /* if no extension has been found, 'ret' equals 0 and this line is also correct */
	for(i = 0; i < path->size; i++) {
		ret = gla_regex_path_fragment(si, -1, path->package + i, NULL, pool);
		if(ret < 0)
			return ret;
		si += ret;
	}
	ret = gla_regex_path_fragment(si, -1, &path->entity, NULL, pool);
	if(ret < 0)
		return ret;

	return GLA_SUCCESS;
}

int gla_path_get_package(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool)
{
	const SQChar *string;
	int ret;

	idx = sq_toabs(rt->vm, idx);
	if(sq_gettype(rt->vm, idx) == OT_STRING) {
		if(SQ_FAILED(sq_getstring(rt->vm, idx, &string))) {
			LOG_ERROR("Error retrieving string from operand stack");
			return GLA_VM;
		}
		ret = gla_path_parse_package(path, string, pool);
		if(ret != GLA_SUCCESS) {
			LOG_ERROR("Error parsing path");
			return ret;
		}
	}
	else {
		ret = gla_mod_io_path_get(path, clone, rt, idx, pool);
		if(ret != GLA_SUCCESS)
			return ret;
		else if(gla_path_type(path) != GLA_PATH_PACKAGE)
			return GLA_INVALID_PATH_TYPE;
	}
	return GLA_SUCCESS;
}

int gla_path_get_entity(
		gla_path_t *path,
		bool clone,
		gla_rt_t *rt,
		int idx,
		apr_pool_t *pool)
{
	const SQChar *string;
	int ret;

	idx = sq_toabs(rt->vm, idx);
	if(sq_gettype(rt->vm, idx) == OT_STRING) {
		if(SQ_FAILED(sq_getstring(rt->vm, idx, &string))) {
			LOG_ERROR("Error retrieving string from operand stack");
			return GLA_VM;
		}
		ret = gla_path_parse_entity(path, string, pool);
		if(ret != GLA_SUCCESS) {
			LOG_ERROR("Error parsing path");
			return ret;
		}
	}
	else {
		ret = gla_mod_io_path_get(path, clone, rt, idx, pool);
		if(ret != GLA_SUCCESS)
			return ret;
		else if(gla_path_type(path) != GLA_PATH_ENTITY)
			return GLA_INVALID_PATH_TYPE;
	}
	return GLA_SUCCESS;
}

int gla_path_type(
		const gla_path_t *path)
{
	if(path == NULL || path->entity == NULL)
		return GLA_PATH_PACKAGE;
	else
		return GLA_PATH_ENTITY;
}

int gla_path_parse_append_entity(
		gla_path_t *path,
		const char *string,
		apr_pool_t *pool)
{
	bool isfinal;
	const char *si;
	int ret;
	int i;
	int root_size;

	if(path == NULL)
		return GLA_INVALID_ARGUMENT;
	else if(path->entity != NULL)
		return GLA_INVALID_PATH_TYPE;

	root_size = path->size;
	si = string;
	ret = gla_regex_path_extension(si, -1, NULL, NULL);
	if(ret < 0)
		return ret;
	si += ret;
	isfinal = false;
	while(true) {
		ret = gla_regex_path_fragment(si, -1, NULL, &isfinal, NULL);
		if(ret < 0)
			return ret;
		else if(ret == 0)
			return GLA_INVALID_PATH;
		si += ret;
		if(isfinal && *si != 0)
			return GLA_INVALID_PATH;
		else if(*si == 0 && !isfinal)
			return GLA_INVALID_PATH;
		if(isfinal)
			break;
		path->size++;
		if(path->size > GLA_PATH_MAX_DEPTH)
			return GLA_OVERFLOW;
	}
	si = string;
	ret = gla_regex_path_extension(si, -1, &path->extension, pool);
	if(ret < 0)
		return ret;
	si += ret; /* if no extension has been found, 'ret' equals 0 and this line is also correct */
	for(i = root_size; i < path->size; i++) {
		ret = gla_regex_path_fragment(si, -1, path->package + i, NULL, pool);
		if(ret < 0)
			return ret;
		si += ret;
	}
	ret = gla_regex_path_fragment(si, -1, &path->entity, NULL, pool);
	if(ret < 0)
		return ret;

	return GLA_SUCCESS;
}

bool gla_path_equal(
		const gla_path_t *a,
		const gla_path_t *b)
{
	int i;

	if(a->size != b->size)
		return false;
	else if((a->entity == NULL || b->entity == NULL) && (a->entity != b->entity))
		return false;
	else if((a->extension == NULL || b->extension == NULL) && (a->extension != b->extension))
		return false;

	for(i = 0; i < a->size; i++)
		if(a->package[i] != b->package[i] && strcmp(a->package[i], b->package[i]) != 0)
			return false;
	if(a->entity != b->entity && strcmp(a->entity, b->entity) != 0)
		return false;
	return true;
}

const char *gla_path_shift(
		gla_path_t *path)
{
	if(path == NULL || (path->size - path->shifted) == 0)
		return NULL;
	path->shifted++;
	return path->package[path->shifted - 1];
}

int gla_path_shift_many(
		gla_path_t *path,
		int amount)
{
	if(path == NULL)
		return 0;
	amount = MIN(amount, path->size - path->shifted);
	path->shifted += amount;
	return amount;
}

int gla_path_shift_relative(
		gla_path_t *path,
		const gla_path_t *root)
{
	int i;

	if(root->entity != NULL)
		return GLA_INVALID_PATH_TYPE;
	else if(root->shifted != 0 || path->shifted != 0)
		return GLA_INVALID_PATH;
	else if(path->size < root->size)
		return GLA_INVALID_PATH;
	for(i = 0; i < root->size; i++)
		if(strcmp(root->package[i], path->package[i]) != 0)
			return GLA_INVALID_PATH;
	path->shifted = root->size;
	return GLA_SUCCESS;
}

int gla_path_unshift(
		gla_path_t *path)
{
	if(path->shifted > 0) {
		path->shifted--;
		return 1;
	}
	else
		return 0;
}

int gla_path_unshift_many(
		gla_path_t *path,
		int amount)
{
	amount = MIN(amount, path->shifted);
	path->shifted -= amount;
	return amount;
}

bool gla_path_is_root(
		const gla_path_t *path)
{
	return path == NULL || (path->entity == NULL && path->size - path->shifted == 0);
}

void gla_path_package(
		gla_path_t *path)
{
	if(path != NULL) {
		path->entity = NULL;
		path->extension = NULL;
	}
}

void gla_path_root(
		gla_path_t *path)
{
	if(path != NULL)
		memset(path, 0, sizeof(*path));
}

const char *gla_path_tostring(
		const gla_path_t *path,
		apr_pool_t *pool)
{
	int len = 0;
	int i;
	char *string;
	char *di;

	if(path == NULL)
		return "::";

	for(i = path->shifted; i < path->size; i++)
		len += strlen(path->package[i]) + 1;
	if(path->entity != NULL)
		len += strlen(path->entity) + 1;
	if(path->extension != NULL)
		len += strlen(path->extension) + 2;
	string = apr_palloc(pool, len);
	if(string == NULL)
		return NULL;
	di = string;
	if(path->extension != NULL) {
		strcpy(di, "<");
		di += strlen(di);
		strcpy(di, path->extension);
		di += strlen(di);
		strcpy(di, ">");
		di += strlen(di);
	}
	for(i = path->shifted; i < path->size; i++) {
		strcpy(di, path->package[i]);
		di += strlen(di);
		if(i < path->size - 1) {
			strcpy(di, ".");
			di += strlen(di);
		}
	}
	if(path->entity != NULL) {
		if(path->size - path->shifted > 0) {
			strcpy(di, ".");
			di += strlen(di);
		}
		strcpy(di, path->entity);
	}

	return string;
}

int gla_path_clone(
		gla_path_t *dest,
		const gla_path_t *src,
		apr_pool_t *pool)
{
	int i;
	char *copy;
	memcpy(dest, src, sizeof(gla_path_t));
	for(i = 0; i < src->size; i++) {
		copy = apr_palloc(pool, strlen(src->package[i]) + 1);
		if(copy == NULL)
			return GLA_ALLOC;
		strcpy(copy, src->package[i]);
		dest->package[i] = copy;
	}
	if(src->entity != NULL) {
		copy = apr_palloc(pool, strlen(src->entity) + 1);
		if(copy == NULL)
			return GLA_ALLOC;
		strcpy(copy, src->entity);
		dest->entity = copy;
	}
	if(src->extension != NULL) {
		copy = apr_palloc(pool, strlen(src->extension) + 1);
		if(copy == NULL)
			return GLA_ALLOC;
		strcpy(copy, src->extension);
		dest->extension = copy;
	}
	return GLA_SUCCESS;
}

void gla_path_dump(
		const gla_path_t *path)
{
	int i;

	printf("------------------------------------------- PATH DUMP -----------------------------------\n");
	if(path->entity == NULL)
		printf("type=package ");
	else
		printf("type=entity ");
	printf("size=%d ", path->size);
	printf("shifted=%d ", path->shifted);
	if(path->entity != NULL)
		printf("extension=%s ", path->extension);
	else if(path->extension != NULL)
		printf("UNEXPECTED EXTENSION ");
	printf("\n");
	for(i = 0; i < path->shifted; i++)
		printf("* %s\n", path->package[i]);
	for(i = path->shifted; i < path->size; i++)
		printf("  %s\n", path->package[i]);
	if(path->entity != NULL)
		printf("  %s\n", path->entity);
	printf("-----------------------------------------------------------------------------------------\n");
}

