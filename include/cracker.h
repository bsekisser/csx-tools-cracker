#pragma once

/* **** forward declarations/definitions */

typedef struct cracker_tag* cracker_ptr;
typedef cracker_ptr const cracker_ref;

/* **** */

#include "cracker_regs.h"

#include "symbol.h"

/* **** */

#include "libbse/include/queue.h"

/* **** */

#include <stdint.h>

/* **** */

enum {
	rrRD,
	rrRM,
	rrRN,
	rrRS,
	rrRSOP,
	rrRSCO,
	REG_COUNT,
//
	rrREA = rrRSOP,
//
	rrPTR = 0x10,
};

enum {
	rSP = 13,
	rLR,
	rPC,
};

/* **** */

typedef struct cracker_core_inst_tag* cracker_core_inst_ptr;
typedef cracker_core_inst_ptr const cracker_core_inst_ref;

typedef struct cracker_core_inst_tag {
	uint32_t ip;
#define IP CORE_INST->ip

	uint32_t ir;
#define IR CORE_INST->ir

	cracker_reg_t reg[REG_COUNT];
#define rrCIRx(_x) (&CORE_INST->reg[_x])
#define rrCIR(_x) rrCIRx(rrR##_x)

	uint8_t cc; /* for thumb */
#define CCv CORE_INST->cc
#define CCx ((IP & 1) ? CCv : ARM_IR_CC)

	uint8_t rr[REG_COUNT];
#define rRx(_x) CORE_INST->rr[_x]
#define rR(_x) rRx(rrR##_x)
}cracker_core_inst_t;

#define vRx(_x) rrCIRx(_x)->v
#define vR(_x) rrCIR(_x)->v

typedef struct cracker_core_tag* cracker_core_ptr;
typedef cracker_core_ptr const cracker_core_ref;

typedef struct cracker_core_tag {
	cracker_core_inst_t inst;
#define CORE_INST (&CORE->inst)

	cracker_reg_t reg[16];
#define GPR(_x) (&CORE->reg[_x])
#define GPR_rRx(_x) GPR(rRx(_x))
#define vGPR(_x) GPR(_x)->v
#define vGPR_rRx(_x) GPR_rRx(_x)->v

	union {
		unsigned _flags;
		struct {
			unsigned enabled:1;
			unsigned comment:1;
			unsigned started:1;
		}trace;
	};
}cracker_core_t;

typedef struct cracker_content_tag* cracker_content_ptr;
typedef cracker_content_ptr const cracker_content_ref;

typedef struct cracker_content_tag {
		void* data;
		void* data_limit;

		uint32_t base;
		uint32_t end;
		size_t size;
}cracker_content_t;

typedef struct cracker_tag {
	cracker_core_t core;
#define CORE (&cj->core)

	cracker_content_t content;

	symbol_ptr symbol;
	unsigned symbol_pass;
	unsigned symbol_text_mod;
	symbol_ptr symbol_qhead;

	struct {
			unsigned added;
			unsigned data;
			unsigned string;
			unsigned text;
	}symbol_count;

	union {
//		unsigned _flags;
		struct {
			unsigned collect_refs:1;
			unsigned trace:1;
		};
	};
}cracker_t;

#define IS_THUMB (IP & 1)

#define rRx_IS_PC(_x) (rPC == rRx(_x))
#define rR_IS_PC(_x) rRx_IS_PC(rrR##_x)

#define rRx_IS_PC_REF(_x) rrCIRx(_x)->is_pc_ref
#define rR_IS_PC_REF(_x) rRx_IS_PC_REF(rrR##_x)

#define rR_NAME(_x) reg_name[rR(_x)]

#define LR vGPR(rLR)
#define PC vGPR(rPC)

/* **** */

void cracker_clear(cracker_ref cj);
void cracker_dump_hex(cracker_ref cj, uint32_t start, uint32_t end);
void cracker_pass(cracker_ref cj, int trace);
int cracker_pat_bounded(cracker_ref cj, uint32_t* p2start, uint32_t* p2end);
int cracker_pat_in_bounds(cracker_ref cj, uint32_t pat, size_t size);
int cracker_pat_out_of_bounds(cracker_ref cj, uint32_t pat, size_t size);
int cracker_pat_range_out_of_bounds(cracker_ref cj, uint32_t start, uint32_t end);
int cracker_pat_src_if(cracker_ref cj, uint32_t pat, size_t size, uint8_t** src);
int cracker_pat_src_limit_if(cracker_ref cj, uint32_t pat, uint8_t** src, uint8_t** src_limit);
uint32_t cracker_read(cracker_ref cj, uint32_t pat, size_t size);
int cracker_read_if(cracker_ref cj, uint32_t pat, size_t size, uint32_t* data);
int cracker_read_src_if(cracker_ref cj, uint32_t pat, size_t size, uint8_t** src);
void cracker_relocation(cracker_ref cj, uint32_t pat);
int cracker_step(cracker_ref cj);
symbol_ptr cracker_text(cracker_ref cj, uint32_t pat);
int cracker_text_branch(cracker_ref cj, unsigned cc, uint32_t new_pc, uint32_t next_pc);
int cracker_text_branch_and_link(cracker_ref cj, unsigned cc, uint32_t new_pc, uint32_t new_lr);
int cracker_text_branch_link(cracker_ref cj, unsigned cc, uint32_t new_lr);
symbol_ptr cracker_text_end(cracker_ref cj, uint32_t pat);
int cracker_text_end_if(cracker_ref cj, uint32_t pat, int end);

/* **** */

#define setup_rR_vR(_rx, _rr, _vr) _setup_rR_vR(cj, rrR##_rx, _rr, _vr)
static inline uint32_t _setup_rR_vR(cracker_ref cj, uint8_t rx, uint8_t rr, uint32_t vr) {
	rRx(rx) = rr;
	return((vRx(rx) = vr));
}
