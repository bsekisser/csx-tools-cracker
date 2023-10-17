#pragma once

/* ***** */

#include "queue.h"

/* ***** */

#include <stdint.h>

/* ***** */

typedef struct symbol_t** symbol_h;
typedef struct symbol_t* symbol_p;
typedef struct symbol_t {
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
