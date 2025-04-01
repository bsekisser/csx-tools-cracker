#pragma once

/* **** */

#include "libbse/include/queue.h"

/* **** */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* **** */

typedef struct symbol_tag** symbol_hptr;
typedef symbol_hptr const symbol_href;

typedef struct symbol_tag* symbol_ptr;
typedef symbol_ptr const symbol_ref;

typedef struct symbol_tag {
	qelem_t qelem;

	uint32_t pat;
	uint32_t end_pat;

	unsigned pass;
	unsigned refs;
	struct {
		unsigned dst;
		unsigned src;
	}reg;
	size_t size;
	unsigned type;
	unsigned type_subtype;

	union {
		unsigned _flags;
		struct {
			unsigned in_bounds:1;
			unsigned thumb:1;
		};
	};
}symbol_t;

void symbol_enqueue(symbol_href h2sqh, symbol_ref lhs, symbol_ref cjs);
symbol_ptr symbol_find_pat(symbol_href h2sqh, symbol_href h2lhs, uint32_t pat, uint32_t mask);
symbol_ptr symbol_new(uint32_t pat, size_t size, unsigned type);
symbol_ptr symbol_next(symbol_ref sqh, symbol_href h2lhs, symbol_href h2cjs, symbol_href h2rhs);
