#include <stdio.h>
#include <pcre.h>

#include "common.h"

#include "regex.h"

#define LOCALS_GROUPS(REGEX, NGROUP) \
	const struct regex *regex = REGEXES + REGEX; \
	int vec[NGROUP * 3 + 3]; \
	int matches;

#define EXEC_GROUPS \
	do { \
		if(len < 0) \
			len = strlen(string); \
	\
		matches = pcre_exec(regex->compiled, regex->extra, string, len, 0, PCRE_ANCHORED, vec, sizeof(vec) / sizeof(*vec)); \
		if(matches == PCRE_ERROR_NOMATCH) \
			return 0; \
		else if(matches <= 0) \
			return GLA_INTERNAL; \
	} while(0)

#define EXEC_GROUPS_NOCHECK \
	do { \
		if(len < 0) \
			len = strlen(string); \
	\
		matches = pcre_exec(regex->compiled, regex->extra, string, len, 0, PCRE_ANCHORED, vec, sizeof(vec) / sizeof(*vec)); \
	} while(0)

#define COPY_GROUP(GROUP, TARGET) \
	do { \
		if(TARGET != NULL) { \
			TARGET[TARGET ## _max - 1] = 0; \
			if(vec[GROUP * 2 + 1] - vec[GROUP * 2] > TARGET ## _max) \
				return GLA_OVERFLOW; \
			else if(vec[GROUP * 2 + 1] - vec[GROUP * 2] < TARGET ## _max) \
				TARGET[vec[GROUP * 2 + 1] - vec[GROUP * 2]] = 0; \
			strncpy(TARGET, string + vec[GROUP * 2], vec[GROUP * 2 + 1] - vec[GROUP * 2]); \
		} \
	} while(0)

#define CLONE_GROUP(GROUP, TARGET) \
	do { \
		if(TARGET != NULL) { \
			char *out = apr_palloc(pool, vec[GROUP * 2 + 1] - vec[GROUP * 2] + 1); \
			if(out == NULL) \
				return GLA_ALLOC; \
			strncpy(out, string + vec[GROUP * 2], vec[GROUP * 2 + 1] - vec[GROUP * 2]); \
			out[vec[GROUP * 2 + 1] - vec[GROUP * 2]] = 0; \
			*TARGET = out; \
		} \
	} while(0)

struct regex {
	const char *pattern;
	pcre *compiled;
	pcre_extra *extra;
};

static struct regex REGEXES[] = {
	{ "^([a-zA-Z0-9_-]+)(?:\\.([a-zA-Z0-9_-]+))?$" }, /* filename to entity */
	{ "^([a-zA-Z0-9_-]+)$" }, /* dirname to entity */
	{ "^([a-zA-Z0-9_-]+)\\." }, /* path fragment string */
	{ "^([a-zA-Z0-9_-]+)" }, /* final path fragment string */
	{ "^\\<([a-zA-Z0-9_-]*)\\>" }, /* extension string */
	{ "^([a-zA-Z0-9_]+)$" } /* storage: valid table name */
};

enum {
	FILENAME_TO_ENTITY,
	DIRNAME_TO_ENTITY,
	PATH_FRAGMENT,
	PATH_FINAL_FRAGMENT,
	PATH_EXTENSION,
	STORAGE_TABLE_NAME
};

apr_status_t cleanup(
		void *data)
{
	int i;

	for(i = 0; i < sizeof(REGEXES) / sizeof(*REGEXES); i++) {
		if(REGEXES[i].extra != NULL)
			pcre_free(REGEXES[i].extra);
		pcre_free(REGEXES[i].compiled);
		REGEXES[i].compiled = NULL;
		REGEXES[i].extra = NULL;
	}
	return APR_SUCCESS;
}

int gla_regex_init(
		apr_pool_t *pool)
{
	int i;
	int error_offset;
	const char *error_string;

	apr_pool_cleanup_register(pool, NULL, cleanup, apr_pool_cleanup_null);

	for(i = 0; i < sizeof(REGEXES) / sizeof(*REGEXES); i++) {
		REGEXES[i].compiled = pcre_compile(REGEXES[i].pattern, 0, &error_string, &error_offset, NULL);
		if(REGEXES[i].compiled == NULL) {
			printf("Error in regular expression /%s/, offset %d: %s\n", REGEXES[i].pattern, error_offset, error_string);
			return GLA_INTERNAL;
		}
		REGEXES[i].extra = pcre_study(REGEXES[i].compiled, 0, &error_string);
		if(REGEXES[i].extra != NULL && error_string != NULL) {
			printf("Error studying regular expression /%s/: %s\n", REGEXES[i].pattern, error_string);
			return GLA_INTERNAL;
		}
	}
	return 0;
}

int gla_regex_filename_to_entity(
		const char *string,
		int len,
		char *name,
		int name_max,
		char *extension,
		int extension_max)
{
	LOCALS_GROUPS(FILENAME_TO_ENTITY, 2)

	EXEC_GROUPS;

	COPY_GROUP(1, name);
	COPY_GROUP(2, extension);

	return vec[1] + vec[0];
}

int gla_regex_dirname_to_entity(
		const char *string,
		int len,
		char *name,
		int name_max)
{
	LOCALS_GROUPS(DIRNAME_TO_ENTITY, 1)

	EXEC_GROUPS;

	COPY_GROUP(1, name);

	return vec[1] + vec[0];
}

int gla_regex_path_fragment(
		const char *string,
		int len,
		const char **fragment,
		bool *isfinal,
		apr_pool_t *pool)
{
	{ /* check fragment match */
		LOCALS_GROUPS(PATH_FRAGMENT, 1)

		EXEC_GROUPS_NOCHECK;
		if(matches <= 0 && matches != PCRE_ERROR_NOMATCH)
			return GLA_INTERNAL;
		else if(matches != PCRE_ERROR_NOMATCH) {
			CLONE_GROUP(1, fragment);
			if(isfinal != NULL)
				*isfinal = false;
			return vec[1] + vec[0];
		}
	}
	{ /* check final fragment match */
		LOCALS_GROUPS(PATH_FINAL_FRAGMENT, 1)

		EXEC_GROUPS;
		CLONE_GROUP(1, fragment);
		if(isfinal != NULL)
			*isfinal = true;
		return vec[1] + vec[0];
	}
}

int gla_regex_path_extension(
		const char *string,
		int len,
		const char **extension,
		apr_pool_t *pool)
{
	LOCALS_GROUPS(PATH_EXTENSION, 1)

	EXEC_GROUPS;
	CLONE_GROUP(1, extension);
	return vec[1] + vec[0];
}

int gla_regex_storage_table_name(
		const char *string,
		int len)
{
	LOCALS_GROUPS(STORAGE_TABLE_NAME, 1);
	EXEC_GROUPS;
	return vec[1] + vec[0];
}


