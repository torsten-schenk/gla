#pragma once

typedef struct gla_preproc gla_preproc_t;

int gla_preproc_init(
		gla_preproc_t *self,
		SQInteger (*lexfeed)(SQUserPointer),
		SQUserPointer user,
		apr_pool_t *pool);

SQInteger gla_preproc_lexfeed(
		SQUserPointer self_);

