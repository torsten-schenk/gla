#include "../rt.h"

#include "unicode.h"

/* BEGIN from source https://gist.github.com/antonijn/9009746 */
static int is_valid_char(uint32_t ch)
{
	return ch < 0xd800 || ch > 0xdfff;
}

static int is_combo_char(uint32_t ch)
{
	return (ch >= 0x0300 && ch <= 0x036f)
	    || (ch >= 0x20d0 && ch <= 0x20ff)
	    || (ch >= 0xfe20 && ch <= 0xfe2f);
}

/*static int utf16_getch(uint16_t buf[], unsigned long *idx, size_t strlen,
                 uint32_t *cp)
{
	if (*idx >= strlen) {
		return -1;
	}
	uint16_t ch = buf[(*idx)++];
	if ((ch & 0xfc00) != 0xd800) {
		*cp = (uint32_t)ch;
		return 0;
	}
	if (*idx > strlen) {
		return -1;
	}
	uint16_t nxt = buf[(*idx)++];
	if ((nxt & 0xfc00) != 0xdc00) {
		return -1;
	}
	*cp = ((ch & 0x03ff) << 10) | (nxt & 0x03ff);
	return 0;
}

static int utf16_codepoint_count(uint16_t chars[], size_t strlen, size_t *out_size)
{
	unsigned long idx = 0;
	for (*out_size = 0; *out_size < strlen; ++*out_size) {
		uint32_t cp;
		utf16_getch(chars, &idx, strlen, &cp);
		if (!is_valid_char(cp)) {
			return -1;
		}
	}
	return 0;
}

static int utf16_to_utf32(uint16_t input[], uint32_t output[],
                   size_t count, size_t *out_size)
{
	unsigned long i;
	unsigned long idx = 0;
	for (*out_size = 0; *out_size < count; ++*out_size) {
		utf16_getch(input, &idx, count, output + i);
		if (!is_valid_char(output[i])) {
			return -1;
		}
	}
	return 0;
}*/

static int utf8_getch(
		const uint8_t **si,
		const uint8_t *end,
		uint32_t *di)
{
	int remunits;
	uint8_t nxt, msk;
	uint32_t cp;
	nxt = **si;
	(*si)++;
	if(nxt & 0x80) {
		msk = 0xe0;
		for(remunits = 1; (nxt & msk) != ((msk << 1) & 0xff); ++remunits)
			msk = (msk >> 1) | 0x80;
	}
	else {
		remunits = 0;
		msk = 0;
	}
	cp = nxt ^ msk;
	while(remunits-- > 0) {
		cp <<= 6;
		if(*si >= end)
			return -1;
		cp |= **si & 0x3f;
		(*si)++;
	}
	*di = cp;
	return 0;
}

static int utf8_codepoint_count(
		const uint8_t *chars,
		size_t count,
		size_t *out_size)
{
	const uint8_t *si = chars;
	const uint8_t *end = si + count;
	size_t n = 0;
	uint32_t cp;

	while(si < end) {
		if(utf8_getch(&si, end, &cp) != 0)
			return -1;
		else if(!is_valid_char(cp))
			return -1;
		n++;
	}
	*out_size = n;
	return 0;
}

static int utf8_to_utf32(
		const uint8_t *input,
		uint32_t *output,
		size_t count)
{
	const uint8_t *si = input;
	const uint8_t *end = si + count;
	uint32_t *di = output;

	while(si < end) {
		if(utf8_getch(&si, end, di) != 0)
			return -1;
		else if(!is_valid_char(*di))
			return -1;
		di++;
	}
	return 0;
}
/* END from source https://gist.github.com/antonijn/9009746 */

static SQInteger fn_utf8_to_utf32(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	const SQChar *utf8;
	size_t n;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &utf8)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	else if(utf8_codepoint_count((const uint8_t*)utf8, strlen(utf8), &n) != 0)
		return gla_rt_vmthrow(rt, "Invalid argument 1: not a valid utf-8 string");



	return gla_rt_vmsuccess(rt, false);
}

static SQInteger fn_utf8_strlen(
		HSQUIRRELVM vm)
{
	gla_rt_t *rt = gla_rt_vmbegin(vm);
	const SQChar *utf8;
	size_t n;

	if(sq_gettop(vm) != 2)
		return gla_rt_vmthrow(rt, "Invalid argument count");
	else if(SQ_FAILED(sq_getstring(vm, 2, &utf8)))
		return gla_rt_vmthrow(rt, "Invalid argument 1: expected string");
	else if(utf8_codepoint_count((const uint8_t*)utf8, strlen(utf8), &n) != 0)
		return gla_rt_vmthrow(rt, "Invalid argument 1: not a valid utf-8 string");

	sq_pushinteger(vm, n);
	return gla_rt_vmsuccess(rt, true);
}


int gla_mod_string_unicode_cbridge(
		gla_rt_t *rt)
{
	HSQUIRRELVM vm = rt->vm;

	sq_pushstring(vm, "utf8toutf32", -1);
	sq_newclosure(vm, fn_utf8_to_utf32, 0);
	sq_newslot(vm, -3, false);
	
	sq_pushstring(vm, "utf8strlen", -1);
	sq_newclosure(vm, fn_utf8_strlen, 0);
	sq_newslot(vm, -3, false);
	
	return GLA_SUCCESS;
}

