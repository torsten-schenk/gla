#pragma once

#include <stdbool.h>
#include <apr_pools.h>

int gla_regex_init(
		apr_pool_t *pool);

int gla_regex_filename_to_entity(
		const char *string,
		int len,
		char *name,
		int name_max,
		char *extension,
		int extension_max);

int gla_regex_dirname_to_entity(
		const char *string,
		int len,
		char *name,
		int name_max);

int gla_regex_path_fragment(
		const char *string,
		int len,
		const char **fragment,
		bool *isfinal,
		apr_pool_t *pool);

int gla_regex_path_extension(
		const char *string,
		int len,
		const char **extension,
		apr_pool_t *pool);

int gla_regex_storage_table_name(
		const char *string,
		int len);

