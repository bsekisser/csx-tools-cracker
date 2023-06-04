#include "cracker.h"
#include "cracker_arm.h"
#include "cracker_arm_ir.h"
#include "cracker_data.h"
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

static int __pat_out_of_bounds(cracker_p cj, uint32_t pat, size_t size)
{
	int oob = pat < cj->content.base;
	oob |= pat > (cj->content.end - size);

	return(oob);
}

static uint8_t* _check_bounds(cracker_p cj, uint32_t pat, size_t size, void **p2ptr)
{
	if(__pat_out_of_bounds(cj, pat, size))
		return(0);

	uint8_t* target = cj->content.data + (pat - cj->content.base);

	if(p2ptr)
		*p2ptr = target;

	return(target);
}

uint32_t _read(cracker_p cj, uint32_t pat, size_t size)
{
	uint32_t data = 0;

	cracker_read_if(cj, pat, size, &data);
	return(data);
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
			cracker_symbol_end(cjs, PC, 0);
			cracker_clear(cj);
			break;
		}
	}

	if(trace)
		printf("\n");
}

void cracker_dump_hex(cracker_p cj, uint32_t start, uint32_t end)
{
	const unsigned stride = 31;
//	const unsigned stride = 15;
//	const unsigned stride = 7;

	if(__pat_out_of_bounds(cj, start, 1)
		|| __pat_out_of_bounds(cj, end, stride))
			return;

	if(end < start)
		LOG_ACTION(return);

	const unsigned stride_mask = ~stride;

	const uint32_t eend = (end + stride) & stride_mask;

	uint32_t pat = start & stride_mask;

	LOG("start = 0x%08x, sstart = 0x%08x, end = 0x%08x, eend = 0x%08x -- stride = 0x%08x, mask = 0x%08x",
		start, pat, end, eend, stride, stride_mask);

	do {
		printf("0x%08x: ", pat);
		char sstring[64], *dst = sstring;
		*dst = 0;

		for(unsigned count = stride + 1; count; count--, pat++) {
			int valid = (pat >= start) && (pat <= end);

			uint8_t data = valid ? _read(cj, pat, sizeof(uint8_t)) : 0;
			uint8_t c = ((data < ' ') || (data > 0x7e))  ? '.' : data;

			*dst++ = valid ? c : ' ';

			if(valid) {
				printf("%02x ", data & 0xff);
			} else {
				printf("   ");
			}
		}

		*dst = 0;
		printf("-- %s\n", sstring);
	}while(pat < eend);
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

	const uint symbol_count = 
		cj->symbol_count.data
		+ cj->symbol_count.string
		+ cj->symbol_count.text;

	LOG("symbol_count (data = 0x%08x, string = 0x%08x, text = 0x%08x) == 0x%08x",
		cj->symbol_count.data, cj->symbol_count.string, cj->symbol_count.text, symbol_count);

	LOG("symbols_added: 0x%08x, pass_symbol_count: 0x%08x",
		cj->symbol_count.added, pass_symbol_count);
}

/* **** */

int cracker_read_if(cracker_p cj, uint32_t pat, size_t size, uint32_t* data)
{
	uint8_t* src = 0;
	if(!cracker_read_src_if(cj, pat, size, &src))
		return(0);
	
	uint8_t* src_limit = cj->content.data + cj->content.size;
	
	for(uint i = 0; i < size; i++) {
		*data |= ((*src++) << (i << 3));
		if(src > src_limit)
			break;
	}

	return(1);
}

int cracker_read_src_if(cracker_p cj, uint32_t pat, size_t size, uint8_t** src)
{
	assert(0 != src);

	if(__pat_out_of_bounds(cj, pat, size))
		return(0);

	uint32_t offset = pat - cj->content.base;
	*src = offset + cj->content.data;

	return(1);
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

		const int thumb = pat & 1;
		if(0) LOG("pat = %c(0x%08x)", thumb ? 'T' : 'A', pat);

		cjs = symbol_new(pat, sizeof(uint32_t), SYMBOL_TEXT);

		cjs->end_pat = ~0U;
		cjs->in_bounds = (0 != _check_bounds(cj, pat, sizeof(uint32_t), 0));
		cjs->thumb = thumb;

		cracker_symbol_enqueue(sqh, lhs, cjs);
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
