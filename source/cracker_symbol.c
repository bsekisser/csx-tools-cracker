#include "cracker_symbol.h"
#include "cracker.h"

/* **** */

#include <assert.h>
#include <stdint.h>

/* **** */ 

void symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs, symbol_p rhs)
{
	if(lhs) {
		assert(lhs->pat < cjs->pat);
		lhs->qelem.next = (void*)cjs;
	}

	if(!(*h2sqh))
		*h2sqh = cjs;

	if(rhs)
		assert(rhs->pat > cjs->pat);

	cjs->qelem.next = (void*)rhs;
}

symbol_p symbol_find_pat(symbol_h h2sqh, uint32_t pat, symbol_h lhs, symbol_h rhs)
{
	symbol_p _lhs = 0, _rhs = 0, cjs = *h2sqh;

	if(!lhs)
		lhs = &_lhs;

	if(!rhs)
		rhs = &_rhs;

	while(cjs) {
		*rhs = (symbol_p)cjs->qelem.next;

		if(cjs->pat == pat)
			return(cjs);

		if(cjs->pat < pat) {
			*lhs = cjs;

			if(*rhs && ((*rhs)->pat > pat))
				return(0);

		}

		cjs = *rhs;
	}

	return(0);
}

symbol_p symbol_next(symbol_h lhs, symbol_p cjs, symbol_h rhs)
{
	if(lhs)
		*lhs = cjs;

	cjs = (symbol_p)cjs->qelem.next;

	if(rhs)
		*rhs = (symbol_p)(cjs ? cjs->qelem.next : 0);

	return(cjs);
}
