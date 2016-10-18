#include "rt.h"
#include "preproc.h"

struct gla_preproc {
	SQInteger (*lexfeed)(SQUserPointer);
	SQUserPointer user;
};

int gla_preproc_init(
		gla_preproc_t *self,
		SQInteger (*lexfeed)(SQUserPointer),
		SQUserPointer user,
		apr_pool_t *pool)
{
	return GLA_SUCCESS;
}

SQInteger gla_preproc_lexfeed(
		SQUserPointer self_)
{
	gla_preproc_t *self = self_;
}

