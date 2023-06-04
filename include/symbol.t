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
