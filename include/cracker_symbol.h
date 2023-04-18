#pragma once

/* **** */

#include <stdint.h>

/* **** */

typedef struct symbol_t** symbol_h;
typedef struct symbol_t* symbol_p;

void symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs, symbol_p rhs);
symbol_p symbol_find_pat(symbol_h h2sqh, uint32_t pat, symbol_h lhs, symbol_h rhs);
symbol_p symbol_next(symbol_h lhs, symbol_p cjs, symbol_h rhs);
