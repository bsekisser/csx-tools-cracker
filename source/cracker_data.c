#include "cracker_data.h"
#include "cracker_symbol.h"
#include "cracker.h"

#include "symbol.h"

/* **** */

#include "libbse/include/bitfield.h"

/* **** */

#include <ctype.h>
#include <stdint.h>

/* **** */

static int __is_only_space(int c) {
	return(' ' == (char)c);
}

static uint8_t* _skip(uint8_t* src_start, uint8_t* src_limit, int (*test)(int))
{
	if(!src_start || !src_limit)
		return(0);

	uint8_t* src = (uint8_t*)src_start;
	for(; src < src_limit; src++) {
		uint8_t c = *src;

		if(0 == c)
			break;

		if(!test(c))
			break;
	}

	return(src);
}

symbol_ptr cracker_data(cracker_ref cj, uint32_t pat, size_t size, size_t len)
{
	if(!pat) return(0);

	symbol_href sqh = &cj->symbol_qhead;

	symbol_ptr lhs = 0;
	symbol_ptr cjs = symbol_find_pat(sqh, &lhs, pat, ~0U);

	if(cjs) {
		BSET(cjs->size, size);
		BSET(cjs->type, SYMBOL_DATA);

		if(cj->collect_refs)
			cjs->refs++;
	} else {
		cj->symbol_count.added++;
		cj->symbol_count.data++;

		cjs = symbol_new(pat, size, SYMBOL_DATA);
		cjs->end_pat = -1 + (pat + (len ? len : size));

		cracker_symbol_enqueue(sqh, lhs, cjs);
	}

	return(cjs);
}


uint32_t cracker_data_ptr_read(cracker_ref cj, uint32_t pat, size_t size)
{
	uint32_t data_pat = 0;
	if(cracker_data_read_if(cj, pat, sizeof(uint32_t), &data_pat))
		return(cracker_data_read(cj, data_pat, size));

	return(0);
}

uint32_t cracker_data_read(cracker_ref cj, uint32_t pat, size_t size)
{
	uint32_t data = 0;

	cracker_data_read_if(cj, pat, size, &data);
	return(data);
}

int cracker_data_read_if(cracker_ref cj, uint32_t pat, size_t size, uint32_t* data)
{
	cracker_data(cj, pat, size, 0);
	return(cracker_read_if(cj, pat, size, data));
}

symbol_ptr cracker_data_string(cracker_ref cj, uint32_t start_pat)
{
	int subtype = SYMBOL_STRING_CSTRING;

	uint8_t* src_limit = 0;
	uint8_t* src_start = 0;

	if(!cracker_pat_src_limit_if(cj, start_pat, &src_start, &src_limit))
		return(0);

	uint8_t* string_start = _skip(src_start, src_limit, __is_only_space);
	if(!string_start || (0 == *string_start))
		return(0);

	uint8_t* string_end = _skip(string_start, src_limit, isprint);
	if(!string_end)
		return(0);

	if(0 != *string_end)
		subtype = SYMBOL_STRING_NSTRING;

	const size_t len = 1 + string_end - string_start;

	if(5 > len)
		return(0);

	const uint32_t pat = start_pat + (string_start - src_start);

	symbol_ptr cjs = cracker_symbol_find(cj, 0, pat, ~0U);
	if(cjs)
		return(0);

	cjs = cracker_data(cj, pat, sizeof(uint8_t), len);

	if(cjs) {
		BSET(cjs->type, SYMBOL_STRING);
		cjs->type_subtype = subtype;

		cj->symbol_count.data--;
		cj->symbol_count.string++;
	}

	return(cjs);
}

symbol_ptr cracker_data_string_rel(cracker_ref cj, uint32_t pat)
{
	pat += cj->content.base;
	return(cracker_data_string(cj, pat));
}

