#include "cracker.h"
#include "cracker_arm.h"
#include "cracker_arm_ir.h"
#include "cracker_strings.h"
#include "cracker_symbol.h"
#include "cracker_thumb.h"
#include "cracker_trace.h"

/* **** */

#include "bitfield.h"
#include "err_test.h"
#include "log.h"
#include "shift_roll.h"
#include "unused.h"

/* **** */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* **** */

//#define RGNDirPath "../../../garmin/rgn_files/"
#define RGNDirPath "../../../garmin/rgn_files/"

//#define RGNFileName "029201000350" /* xxx */

#define RGNFileName "038201000610"
//#define RGNFileName "038201000280"

//#define RGNFileName "048101000610"

//#define RGNFileName "049701000610"

/* **** */

static uint8_t* _check_bounds(cracker_p cj, uint32_t pat, size_t size, void **p2ptr)
{
	if(pat < cj->content.base)
		return(0);
	if(pat > (cj->content.end - size))
		return(0);

	uint8_t* target = cj->content.data + (pat - cj->content.base);

	if(p2ptr)
		*p2ptr = target;

	return(target);
}

uint32_t _read(cracker_p cj, uint32_t pat, size_t size)
{
	if(pat < cj->content.base)
		return(0);
	if(pat > cj->content.end)
		return(0);

	uint32_t res = 0;

	uint8_t* src = cj->content.data;

	for(uint i = 0; i < size; i++) {
		uint32_t offset = pat++ - cj->content.base;
		res |= ((src[offset]) << (i << 3));
		if(pat > cj->content.end)
			break;
	}

	return(res);
}

/* **** */

void cracker_clear(cracker_p cj)
{
	for(int i = 0; i < 16; i++) {
		vGPR(i) = 0;
		GPR(i)->_flags = 0;
	}

	for(int i = 0; i < REG_COUNT; i++) {
		rRx(i) = 0;
		vRx(i) = 0;
		rrCIRx(i)->_flags = 0;
	}
}

symbol_p cracker_data(cracker_p cj, uint32_t pat, size_t size)
{
	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, &lhs, pat, ~0U);

	if(cjs) {
		BSET(cjs->size, size);
		BSET(cjs->type, SYMBOL_DATA);

		if(cj->collect_refs)
			cjs->refs++;
	} else {
		cj->symbol_count.added++;
		cj->symbol_count.data++;
		
		cracker_symbol_end(lhs, pat, "cracker_data -- lhs");
		
		cjs = symbol_new(pat, size, SYMBOL_DATA);
		symbol_enqueue(sqh, lhs, cjs);
	}

	return(cjs);
}

static void cracker_pass_step(cracker_p cj, symbol_p cjs, int trace)
{
	if(0 == cjs->in_bounds)
		return;

	if(cj->symbol_pass && (cjs->pass >= cj->symbol_pass))
		return;

	cjs->pass = cj->symbol_pass;

	if(!BTST(cjs->type, SYMBOL_TEXT))
		return;

	cj->symbol = cjs;
	PC = cjs->pat;
	while(PC <= cjs->end_pat) {
		if(!cracker_symbol_step(cj, cjs)) {
			cracker_clear(cj);
			break;
		}
	}

	if(trace)
		printf("\n");
}

uint32_t cracker_data_ptr_read(cracker_p cj, uint32_t pat, size_t size)
{
	cracker_data(cj, pat, sizeof(uint32_t));
	pat = _read(cj, pat, sizeof(uint32_t));
	cracker_data(cj, pat, size);
	return(_read(cj, pat, size));
}

void cracker_pass(cracker_p cj, int trace)
{
	printf("\n\n/* pass %u **** **** **** **** */\n\n", cj->symbol_pass);

	cracker_clear(cj);
	cj->core.trace = !!trace;
	cj->symbol_count.added = 0;

	uint pass_symbol_count = 0;

	symbol_p cjs = cj->symbol_qhead;

	while(cjs) {
		cracker_pass_step(cj, cjs, trace);
		cjs = symbol_next(0, cjs);
		pass_symbol_count++;
	}

	const uint new_symbol_count = cj->symbol_count.data + cj->symbol_count.text;

	LOG("symbol_count (data = 0x%08x, text = 0x%08x) == 0x%08x",
		cj->symbol_count.data, cj->symbol_count.text, new_symbol_count);

	LOG("symbols_added: 0x%08x, pass_symbol_count: 0x%08x",
		cj->symbol_count.added, pass_symbol_count);
}

/* **** */

int cracker_step(cracker_p cj)
{
	IP = PC;

	if(IP & 1)
		return(thumb_step(cj));

	return(arm_step(cj));
}

/* **** */

void cracker_symbol__log_data(cracker_p cj, symbol_p cjs) {
	LOG_START("0x%08x:", cjs->pat);

	_LOG_(" refs: 0x%04x", cjs->refs);

	uint32_t data = 0;
	size_t size = cjs->size;

	if(BEXT(size, sizeof(uint32_t)))
		data = _read(cj, cjs->pat, sizeof(uint32_t));
	else if(BEXT(size, sizeof(uint16_t)))
		data = _read(cj, cjs->pat, sizeof(uint16_t));
	else if(BEXT(size, sizeof(uint8_t)))
		data = _read(cj, cjs->pat, sizeof(uint8_t));

	_LOG_(", size: 0x%04x", size);

	if(BXCG(&size, sizeof(uint32_t), 0)) {
		_LOG_(", (uint32_t (0x%08x))%s", data, size ? " |" : "");
	}

	if(BXCG(&size, sizeof(uint16_t), 0)) {
		_LOG_(", (uint16_t (0x%04x))%s", (uint16_t)data, size ? " |" : "");
	}

	if(BTST(size, sizeof(uint8_t))) {
		_LOG_(", (uint8_t (0x%02x))", (uint8_t)data);
	}

	LOG_END();
}

void cracker_symbol__log_text(cracker_p cj, symbol_p cjs)
{
	const uint32_t pat_mask = ~3 >> (cjs->pat & 1);

	LOG_START("0x%08x", cjs->pat & pat_mask);

	if(cjs->in_bounds)
			_LOG_(" -- 0x%08x", cjs->end_pat);

	_LOG_(": refs: 0x%04x", cjs->refs);

	uint32_t reg_src = cjs->reg.src;
	if(reg_src) {
		_LOG_(", { ");
		for(int i = 0; reg_src; i++) {
			if(BXCG(&reg_src, i, 0))
				_LOG_("r%u%s", i, reg_src ? ", " : "");
		}
		_LOG_(" }");
	}

	LOG_END(", TEXT ENTRY");

	if(cjs->in_bounds) {
		PC = cjs->pat;
		while(PC <= cjs->end_pat) {
			if(!cracker_symbol_step(cj, cjs)) {
				cracker_clear(cj);
				break;
			}
		}

		printf("\n");
	}

	UNUSED(cj);
}

/* **** */

void cracker_symbol_end(symbol_p cjs, uint32_t pat, const char* name)
{
	if(0 == cjs)
		return;

	if(0 == cjs->in_bounds)
		return;

	const uint32_t pat_mask = (~3 >> cjs->thumb);
	const uint32_t pat_masked = pat & pat_mask;

	if(pat_masked < (cjs->pat & pat_mask))
		return;

	if(pat_masked > (cjs->end_pat & pat_mask))
		return;

//	const uint32_t end_pat_offset = 4 >> cjs->thumb;
//	const uint32_t end_pat = (pat - end_pat_offset) & pat_mask;
	const uint32_t end_pat = pat - (1 << (pat & 1));

	if(0) LOG("%s%s-- pat: 0x%08x, end: 0x%08x, new_end: 0x%08x",
		name ?: "", name ? " " : "",
		cjs->pat, cjs->end_pat, end_pat);

	cjs->end_pat = end_pat;
}

void cracker_symbol_log(cracker_p cj, symbol_p cjs)
{
	if(0 == cj->core.trace)
		return;

	if(BTST(cjs->type, SYMBOL_TEXT))
		cracker_symbol__log_text(cj, cjs);

	if(BTST(cjs->type, SYMBOL_DATA))
		cracker_symbol__log_data(cj, cjs);
}

void cracker_symbol_queue_log(cracker_p cj, symbol_p sqh)
{
	cj->collect_refs = 0;
	cj->core.trace = 1;

	symbol_p cjs = sqh;

	do {
		cracker_symbol_log(cj, cjs);

		cjs = symbol_next(0, cjs);
	}while(cjs);
}

int cracker_symbol_step(cracker_p cj, symbol_p cjs)
{
	IP = PC;
	
	if(cjs->thumb)
		return(thumb_step(cj));
	
	return(arm_step(cj));
}

/* **** */

symbol_p cracker_text(cracker_p cj, uint32_t pat)
{
	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, &lhs, pat, ~1U);

	if(cjs) {
		BSET(cjs->size, sizeof(uint32_t));
		BSET(cjs->type, SYMBOL_TEXT);

		if(cj->collect_refs)
			cjs->refs++;
	} else {
		cj->symbol_count.added++;
		cj->symbol_count.text++;

//		LOG("pat = %c(0x%08x)", thumb ? 'T' : 'A', pat);

		cracker_symbol_end(lhs, pat, "cracker_text -- lhs");

		cjs = symbol_new(pat, sizeof(uint32_t), SYMBOL_TEXT);
		symbol_enqueue(sqh, lhs, cjs);

		cjs->end_pat = ~0U;
		cjs->in_bounds = (0 != _check_bounds(cj, pat, sizeof(uint32_t), 0));
		cjs->thumb = pat & 1;
	}

	return(cjs);
}

int cracker_text_branch_link(cracker_p cj, uint32_t new_lr)
{
	cracker_text(cj, new_lr);

	return(0);
}

symbol_p cracker_text_end(cracker_p cj, uint32_t pat)
{
	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, &lhs, pat, ~1U);

	cracker_symbol_end(lhs, pat, "cracker_text_end -- lhs");
	cracker_symbol_end(cjs, pat, "cracker_text_end -- cjs");

	return(cjs);
}

int cracker_text_end_if(cracker_p cj, uint32_t pat, int end)
{
	if(end)
		cracker_text_end(cj, pat);
	
	return(!end);
}
