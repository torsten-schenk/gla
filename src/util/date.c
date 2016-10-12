#include <apr_time.h>
#include <date_interface.h>

#include "../rt.h"

#include "date.h"

#define RTDATA_TOKEN gla_mod_util_date_augment

typedef struct {
	HSQMEMBERHANDLE year_member;
	HSQMEMBERHANDLE month_member;
	HSQMEMBERHANDLE day_member;
} rtdata_t;

#define M_GET(I, N) \
	do { \
		SQInteger dummy; \
		if(SQ_FAILED(sq_getbyhandle(vm, I, &rtdata->N##_member))) \
			return gla_rt_vmthrow(rt, "Error getting value '"  #N  "'."); \
		else if(SQ_FAILED(sq_getinteger(vm, -1, &dummy))) { \
			sq_poptop(vm); \
			return gla_rt_vmthrow(rt, "Invalid value for '" #N "'."); \
		} \
		sq_poptop(vm); \
		dt.N = dummy; \
	} while(false)

#define M_SET(I, N) \
	do { \
		sq_pushinteger(vm, dt.N); \
		if(SQ_FAILED(sq_setbyhandle(vm, I, &rtdata->N##_member))) \
			return gla_rt_vmthrow(rt, "Error setting value '" #N "'."); \
	} while(false)

static SQInteger fn_toepoch(
		HSQUIRRELVM vm)
{
	date_time dt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count.");

	M_GET(1, year);
	M_GET(1, month);
	M_GET(1, day);

	sq_pushinteger(vm, date_to_epoch(&dt));
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_fromepoch(
		HSQUIRRELVM vm)
{
	date_time dt;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	SQInteger offset;
	rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count.");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &offset)))
		return gla_rt_vmthrow(rt, "Invalid argument: expected integer.");
	date_from_epoch(offset, &dt);

	M_SET(1, year);
	M_SET(1, month);
	M_SET(1, day);

	return gla_rt_vmsuccess(rt, false);
}

SQInteger gla_mod_util_date_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	rtdata_t *rtdata = apr_pcalloc(rt->mpool, sizeof(rtdata_t));
	if(rtdata == NULL)
		return gla_rt_vmthrow(rt, "Not enough memory.");

	sq_pushstring(vm, "toepoch", -1);
	sq_newclosure(vm, fn_toepoch, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "fromepoch", -1);
	sq_newclosure(vm, fn_fromepoch, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "year", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->year_member)))
		return gla_rt_vmthrow(rt, "Error getting 'year' member.");
	sq_pushstring(vm, "month", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->month_member)))
		return gla_rt_vmthrow(rt, "Error getting 'month' member.");
	sq_pushstring(vm, "day", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->day_member)))
		return gla_rt_vmthrow(rt, "Error getting 'day' member.");
	gla_rt_data_put(rt, RTDATA_TOKEN, rtdata);

	return gla_rt_vmsuccess(rt, false);
}

