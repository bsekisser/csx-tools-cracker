#include "symbol.h"

/* **** */

#include "libbse/include/bitfield.h"
#include "libbse/include/queue.h"

/* **** */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* **** */

void symbol_enqueue(symbol_href h2sqh, symbol_ref lhs, symbol_ref cjs)
{
	symbol_ref sqh = *h2sqh;
	symbol_ref rhs = lhs ? ((symbol_ptr)lhs->qelem.next) : sqh;

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

symbol_ptr symbol_find_pat(symbol_href h2sqh, symbol_href h2lhs, uint32_t pat, uint32_t mask)
{
	assert(0 != h2sqh);

	symbol_ptr rhs = *h2sqh;

	const uint32_t masked_pat = pat & mask;

	while(rhs) {
		if(masked_pat == (rhs->pat & mask))
			return(rhs);
		if(masked_pat < (rhs->pat & mask))
			return(0);

		if(h2lhs)
			*h2lhs = rhs;

		rhs = (symbol_ptr)rhs->qelem.next;
	}

	return(0);
}

symbol_ptr symbol_new(uint32_t pat, size_t size, unsigned type)
{
	symbol_ref cjs = calloc(1, sizeof(symbol_t));

	cjs->pat = pat;
	BSET(cjs->size, size);
	BSET(cjs->type, type);

	return(cjs);
}

symbol_ptr symbol_next(symbol_href h2lhs, symbol_ref cjs)
{
	assert(0 != cjs);

	if(h2lhs)
		*h2lhs = cjs;

	return((symbol_ptr)cjs->qelem.next);
}
