#include "cracker_symbol.h"
#include "cracker.h"

/* **** */

#include "bitfield.h"

/* **** */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* **** */ 

void symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs)
{
	const symbol_p sqh = *h2sqh;
	const symbol_p rhs = lhs ? ((symbol_p)lhs->qelem.next) : sqh;

	if(rhs)
		assert(rhs->pat > cjs->pat);

	cjs->qelem.next = (void*)rhs;

	if(lhs) {
		assert(lhs->pat < cjs->pat);
		lhs->qelem.next = (void*)cjs;
	}

	if((0 == sqh) || (0 == lhs))
		*h2sqh = cjs;
}

symbol_p symbol_find_pat(symbol_h h2sqh, symbol_h h2lhs, uint32_t pat, uint32_t mask)
{
	assert(0 != h2sqh);
	
	symbol_p rhs = *h2sqh;

	const uint32_t masked_pat = pat & mask;

	while(rhs) {
		if(masked_pat == (rhs->pat & mask))
			return(rhs);
		if(masked_pat < (rhs->pat & mask))
			return(0);

		if(h2lhs)
			*h2lhs = rhs;

		rhs = (symbol_p)rhs->qelem.next;
	}

	return(0);
}

symbol_p symbol_new(uint32_t pat, size_t size, uint type)
{
	const symbol_p cjs = calloc(1, sizeof(symbol_t));
	
	cjs->pat = pat;
	BSET(cjs->size, size);
	BSET(cjs->type, type);

	return(cjs);
}

symbol_p symbol_next(symbol_h h2lhs, symbol_p cjs)
{
	assert(0 != cjs);

	if(h2lhs)
		*h2lhs = cjs;

	cjs = (symbol_p)cjs->qelem.next;

	return(cjs);
}
