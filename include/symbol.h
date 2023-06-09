#pragma once

/* **** */

#include "queue.h"

/* **** */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* **** */

typedef struct symbol_t** symbol_h;
typedef struct symbol_t* symbol_p;
typedef struct symbol_t {
	qelem_t qelem;

	uint32_t pat;
	uint32_t end_pat;

	uint pass;
	uint refs;
	struct {
		uint dst;
		uint src;
	}reg;
	size_t size;
	uint type;
	uint type_subtype;

	union {
		uint _flags;
		struct {
			uint in_bounds:1;
			uint thumb:1;
		};
	};
}symbol_t;

void symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs);
symbol_p symbol_find_pat(symbol_h h2sqh, symbol_h h2lhs, uint32_t pat, uint32_t mask);
symbol_p symbol_new(uint32_t pat, size_t size, uint type);
symbol_p symbol_next(symbol_h h2lhs, symbol_p cjs);
