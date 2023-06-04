#include "cracker_data.h"
#include "cracker_symbol.h"
#include "cracker.h"

#include "symbol.h"

/* **** */

#include "bitfield.h"

/* **** */

#include <stdint.h>

/* **** */

symbol_p cracker_data(cracker_p cj, uint32_t pat, size_t size, size_t len)
{
	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, &lhs, pat, ~0U);

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


uint32_t cracker_data_ptr_read(cracker_p cj, uint32_t pat, size_t size)
{
	cracker_data(cj, pat, sizeof(uint32_t), 0);
	pat = _read(cj, pat, sizeof(uint32_t));
	return(cracker_data_read(cj, pat, size));
}

uint32_t cracker_data_read(cracker_p cj, uint32_t pat, size_t size)
{
	cracker_data(cj, pat, size, 0);
	return(_read(cj, pat, size));
}

int cracker_data_read_if(cracker_p cj, uint32_t pat, size_t size, uint32_t* data)
{
	cracker_data(cj, pat, size, 0);
	return(cracker_read_if(cj, pat, size, data));
}

symbol_p cracker_data_string(cracker_p cj, uint32_t start)
{
	int subtype = SYMBOL_STRING_CSTRING;

	uint32_t data = 0;
	do {
		if(!cracker_data_read_if(cj, start, sizeof(uint8_t), &data))
			return(0);

		if(' ' != data)
			break;

		start++;
	}while(' ' == data);

	uint32_t pat = start;
	do {
		if(!cracker_data_read_if(cj, pat, sizeof(uint8_t), &data))
			return(0);

		if((data < ' ') || (data > 0x7e)) {
			subtype = SYMBOL_STRING_NSTRING;
			break;
		}

		pat++;
	}while(0 != data);

	size_t len = 1 + pat - start;

	if(5 > len)
		return(0);

	symbol_p cjs = cracker_data(cj, start, sizeof(uint8_t), len);

	if(cjs) {
		BSET(cjs->type, SYMBOL_STRING);
		cjs->type_subtype = subtype;

		cj->symbol_count.data--;
		cj->symbol_count.string++;
	}

	return(cjs);
}

symbol_p cracker_data_string_rel(cracker_p cj, uint32_t pat)
{
	pat += cj->content.base;
	return(cracker_data_string(cj, pat));
}

