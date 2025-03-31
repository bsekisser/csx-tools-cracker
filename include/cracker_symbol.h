#pragma once

/* **** */

enum {
	SYMBOL_DATA,
//	SYMBOL_RELOCATION,
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

void cracker_symbol_end(symbol_ref cjs, uint32_t pat, const char* name);
void cracker_symbol_enqueue(symbol_href h2sqh, symbol_ref lhs, symbol_ref cjs);
symbol_ptr cracker_symbol_find(cracker_ref cj, symbol_href h2lhs, uint32_t pat, uint32_t mask);
int32_t cracker_symbol_intergap(cracker_ref cj, symbol_ref lhs, symbol_ref rhs);
void cracker_symbol_queue_log(cracker_ref cj, symbol_ref sqh);
int cracker_symbol_step(cracker_ref cj, symbol_ref cjs);
int cracker_symbol_step_block(cracker_ref cj, symbol_ref cjs);
