#include <apr_time.h>
#include <date_interface.h>

#include "../rt.h"

#include "time.h"

#define RTDATA_TOKEN gla_mod_util_time_augment

typedef struct {
	HSQMEMBERHANDLE usec_member;
	HSQMEMBERHANDLE sec_member;
	HSQMEMBERHANDLE min_member;
	HSQMEMBERHANDLE hour_member;
	HSQMEMBERHANDLE mday_member;
	HSQMEMBERHANDLE mon_member;
	HSQMEMBERHANDLE year_member;
	HSQMEMBERHANDLE wday_member;
	HSQMEMBERHANDLE yday_member;
	HSQMEMBERHANDLE isdst_member;
	HSQMEMBERHANDLE gmtoff_member;
	HSQMEMBERHANDLE shift_member;
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
		exp.tm_##N = dummy; \
	} while(false)

#define M_SET(I, N) \
	do { \
		sq_pushinteger(vm, exp.tm_##N); \
		if(SQ_FAILED(sq_setbyhandle(vm, I, &rtdata->N##_member))) \
			return gla_rt_vmthrow(rt, "Error setting value '" #N "'."); \
	} while(false)


static SQInteger fn_togmt(
		HSQUIRRELVM vm)
{
	apr_time_exp_t exp;
	apr_time_t time;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	SQInteger shift;

	if(sq_gettop(vm) != 1)
		return gla_rt_vmthrow(rt, "Invalid argument count.");
	else if(SQ_FAILED(sq_getbyhandle(vm, 1, &rtdata->shift_member)))
		return gla_rt_vmthrow(rt, "Error getting value 'shift'.");
	else if(SQ_FAILED(sq_getinteger(vm, -1, &shift))) {
		sq_poptop(vm);
		return gla_rt_vmthrow(rt, "Invalid value for 'shift'.");
	}
	sq_poptop(vm);

	M_GET(1, usec);
	M_GET(1, sec);
	M_GET(1, min);
	M_GET(1, hour);
	M_GET(1, mday);
	M_GET(1, mon);
	M_GET(1, year);
	M_GET(1, wday);
	M_GET(1, yday);
	M_GET(1, isdst);
	M_GET(1, gmtoff);

//	time = time_to_epoch(&exp) - shift;
	time = 0; //TODO placeholder
	sq_pushinteger(vm, time);
	return gla_rt_vmsuccess(rt, true);
}

static SQInteger fn_fromgmt(
		HSQUIRRELVM vm)
{
	apr_time_exp_t exp;
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	SQInteger time;
	rtdata_t *rtdata = gla_rt_data_get(rt, RTDATA_TOKEN);
	SQInteger shift;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count.");
	else if(SQ_FAILED(sq_getinteger(vm, 2, &time)))
		return gla_rt_vmthrow(rt, "Invalid argument: expected integer.");
	else if(SQ_FAILED(sq_getbyhandle(vm, 1, &rtdata->shift_member)))
		return gla_rt_vmthrow(rt, "Error getting value 'shift'.");
	else if(SQ_FAILED(sq_getinteger(vm, -1, &shift))) {
		sq_poptop(vm);
		return gla_rt_vmthrow(rt, "Invalid value for 'shift'.");
	}
	sq_poptop(vm);
	time += shift;
//	time_from_epoch(time, &exp);

	M_SET(1, usec);
	M_SET(1, sec);
	M_SET(1, min);
	M_SET(1, hour);
	M_SET(1, mday);
	M_SET(1, mon);
	M_SET(1, year);
	M_SET(1, wday);
	M_SET(1, yday);
	M_SET(1, isdst);
	M_SET(1, gmtoff);

	return gla_rt_vmsuccess(rt, false);
}

SQInteger gla_mod_util_time_augment(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	rtdata_t *rtdata = apr_pcalloc(rt->mpool, sizeof(rtdata_t));
	if(rtdata == NULL)
		return gla_rt_vmthrow(rt, "Not enough memory.");

	sq_pushstring(vm, "togmt", -1);
	sq_newclosure(vm, fn_togmt, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "fromgmt", -1);
	sq_newclosure(vm, fn_fromgmt, 0);
	sq_newslot(vm, -3, false);

	sq_pushstring(vm, "usec", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->usec_member)))
		return gla_rt_vmthrow(rt, "Error getting 'usec' member.");
	sq_pushstring(vm, "sec", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->sec_member)))
		return gla_rt_vmthrow(rt, "Error getting 'sec' member.");
	sq_pushstring(vm, "min", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->min_member)))
		return gla_rt_vmthrow(rt, "Error getting 'min' member.");
	sq_pushstring(vm, "hour", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->hour_member)))
		return gla_rt_vmthrow(rt, "Error getting 'hour' member.");
	sq_pushstring(vm, "mday", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->mday_member)))
		return gla_rt_vmthrow(rt, "Error getting 'mday' member.");
	sq_pushstring(vm, "mon", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->mon_member)))
		return gla_rt_vmthrow(rt, "Error getting 'mon' member.");
	sq_pushstring(vm, "year", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->year_member)))
		return gla_rt_vmthrow(rt, "Error getting 'year' member.");
	sq_pushstring(vm, "wday", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->wday_member)))
		return gla_rt_vmthrow(rt, "Error getting 'wday' member.");
	sq_pushstring(vm, "yday", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->yday_member)))
		return gla_rt_vmthrow(rt, "Error getting 'yday' member.");
	sq_pushstring(vm, "isdst", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->isdst_member)))
		return gla_rt_vmthrow(rt, "Error getting 'isdst' member.");
	sq_pushstring(vm, "gmtoff", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->gmtoff_member)))
		return gla_rt_vmthrow(rt, "Error getting 'gmtoff' member.");
	sq_pushstring(vm, "shift", -1);
	if(SQ_FAILED(sq_getmemberhandle(vm, -2, &rtdata->shift_member)))
		return gla_rt_vmthrow(rt, "Error getting 'shift' member.");
	gla_rt_data_put(rt, RTDATA_TOKEN, rtdata);

	return gla_rt_vmsuccess(rt, false);
}

