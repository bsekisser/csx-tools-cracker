#pragma once

/* **** */

//#include "symbol.t"

enum {
	SYMBOL_DATA,
	SYMBOL_STRING,
	SYMBOL_TEXT,
	SYMBOL_TEXT_XXX,
};

enum {
	SYMBOL_STRING_CSTRING,
	SYMBOL_STRING_NSTRING,
};

/* **** */

#include "cracker.h"

/* **** */

#include <stdint.h>

/* **** */

void cracker_symbol_end(symbol_p cjs, uint32_t pat, const char* name);
void cracker_symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs);
size_t cracker_symbol_intergap(cracker_p cj, symbol_p lhs, symbol_p rhs);
void cracker_symbol_queue_log(cracker_p cj, symbol_p sqh);
int cracker_symbol_step(cracker_p cj, symbol_p cjs);
