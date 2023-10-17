#include "cracker.h"
#include "cracker_arm.h"
#include "cracker_arm_ir.h"
#include "cracker_data.h"
#include "cracker_enum.h"
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
	if(cracker_symbol_step_block(cj, cjs)) {
		const uint32_t pat_bump = 4 >> cjs->thumb;
		const uint32_t pat_mask = pat_bump - 1;
//		cracker_symbol_end(cjs, (IP + pat_bump) & ~pat_mask, 0);
		cracker_symbol_end(cjs, (PC + pat_bump) & ~pat_mask, 0);

		cracker_clear(cj);
	}


	if(trace)
		printf("\n");
}

void cracker_dump_hex(cracker_p cj, uint32_t start, uint32_t end)
{
	const unsigned stride = 31;
//	const unsigned stride = 15;
//	const unsigned stride = 7;

	if(cracker_pat_range_out_of_bounds(cj, start + 1, end - stride))
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
			const int valid_range = (pat >= start) && (pat <= end);

			uint8_t c = '.';

			if(valid_range) {
				c = cracker_read(cj, pat, sizeof(uint8_t)) & 0xff;

				printf("%02x ", c);
				c = ((c < ' ') || (c > 0x7e)) ? '.' : c;
			} else
				printf("-- ");

			*dst++ = c;
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

	unsigned pass_symbol_count = 0;

	symbol_p cjs = cj->symbol_qhead;

	while(cjs) {
		cracker_pass_step(cj, cjs, trace);
		cjs = symbol_next(0, cjs);
		pass_symbol_count++;
	}

	const unsigned symbol_count = 
		cj->symbol_count.data
		+ cj->symbol_count.string
		+ cj->symbol_count.text;

	LOG("symbol_count (data = 0x%08x, string = 0x%08x, text = 0x%08x) == 0x%08x",
		cj->symbol_count.data, cj->symbol_count.string, cj->symbol_count.text, symbol_count);

	LOG("symbols_added: 0x%08x, pass_symbol_count: 0x%08x",
		cj->symbol_count.added, pass_symbol_count);
}

/* **** */

int cracker_pat_bounded(cracker_p cj, uint32_t* p2start, uint32_t* p2end)
{
	if(*p2end <= cj->content.base)
		return(0);

	if(*p2start >= cj->content.end)
		return(0);

	if(*p2end > cj->content.end)
		*p2end = cj->content.end;

	if(*p2start < cj->content.base)
		*p2start = cj->content.base;

	return(1);
}

int cracker_pat_in_bounds(cracker_p cj, uint32_t pat, size_t size)
{
	return(0 == cracker_pat_out_of_bounds(cj, pat, size));
}

int cracker_pat_out_of_bounds(cracker_p cj, uint32_t pat, size_t size)
{
	if(pat < cj->content.base)
		return(1);
	
	if(pat > (cj->content.end - size))
		return(1);

	return(0);
}

int cracker_pat_range_out_of_bounds(cracker_p cj, uint32_t start, uint32_t end)
{
	if(cracker_pat_out_of_bounds(cj, start, 0))
		return(1);

	if(cracker_pat_out_of_bounds(cj, end, 0))
		return(1);

	return(0);
}

int cracker_pat_src_if(cracker_p cj, uint32_t pat, size_t size, uint8_t** src)
{
	assert(0 != src);

	if(cracker_pat_out_of_bounds(cj, pat, size))
		return(0);

	const uint32_t offset = pat - cj->content.base;
	*src = offset + cj->content.data;

	return(1);
}

int cracker_pat_src_limit_if(cracker_p cj, uint32_t pat, uint8_t** src_start, uint8_t** src_limit)
{
	assert(0 != src_limit);
	assert(0 != src_start);

	if(!cracker_pat_src_if(cj, pat, sizeof(uint8_t), src_start))
		return(0);

	*src_limit = cj->content.data_limit;
	return(1);
}

/* **** */

uint32_t cracker_read(cracker_p cj, uint32_t pat, size_t size)
{
	uint32_t data = 0;
	
	cracker_read_if(cj, pat, size, &data);
	return(data);
}

int cracker_read_if(cracker_p cj, uint32_t pat, size_t size, uint32_t* data)
{
	*data = 0;

	uint8_t* src = 0;
	if(!cracker_pat_src_if(cj, pat, size, &src))
		return(0);

	for(unsigned i = 0; i < size; i++)
		*data |= (*src++ << (i << 3));

	return(1);
}

/* **** */

void cracker_relocation_step(cracker_p cj, uint32_t pat)
{
	const uint32_t savedPC = PC;
	const uint32_t saved_symbol_text_mod = cj->symbol_text_mod;

	BSET(cj->symbol_text_mod, SYMBOL_TEXT_XXX);
	
	PC = pat;
	cracker_step(cj);
	
	PC = savedPC;
	cj->symbol_text_mod = saved_symbol_text_mod;
}

void cracker_relocation(cracker_p cj, uint32_t pat)
{
	const void* src_start = cj->content.data;
	const void* src_limit = cj->content.data_limit;
	
	uint8_t* src = (void*)src_start;

	uint32_t pat_test = pat;
	
	if(0x05000000 == (IR & 0x0f000001)) {
		pat_test = mlBFEXT(pat, 23, 0);
	} else if(0xf000e800 == (IR & 0xf800e800)) {
		pat_test >>= 2;
		const uint32_t pat_low = pat_test & 0x7ff;
		const uint32_t pat_high = (pat_test >> 12) & 0x7ff;

		pat_test = pat_low | (pat_high << 16);
	}

	if(cj->collect_refs) {
		LOG("IP: 0x%08x, 0x%08x", pat, pat_test);
		fprintf(stderr, "IP: 0x%08x, 0x%08x", pat, pat_test);
	}

	while((void*)src < src_limit) {
		uint32_t test = le32toh(*(uint32_t*)src);
		if(test == pat_test) {
			const uint32_t offset = (void*)src - src_start;
			const uint32_t pa = cj->content.base + offset;

			if(cj->collect_refs) {
				uint32_t data = cracker_read(cj, pa, sizeof(uint32_t));
				cracker_relocation_step(cj, pa - (1 << 4));
				
				LOG("0x%08x(0x%08x)", pa, data);
				fprintf(stderr, ", 0x%08x(0x%08x)", pa, data);
			}
		}
		src++;
	}

	if(cj->collect_refs)
		fprintf(stderr, "\n");
}

/* **** */

int cracker_step(cracker_p cj)
{
	if(PC & 1)
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
		cjs->in_bounds = cracker_pat_in_bounds(cj, pat, sizeof(uint32_t));
		cjs->type |= cj->symbol_text_mod;
		cjs->thumb = thumb;

		cracker_symbol_enqueue(sqh, lhs, cjs);
	}

	return(cjs);
}

int cracker_text_branch(cracker_p cj, unsigned bcc, uint32_t new_pc)
{
	PC = new_pc;
	cracker_text(cj, new_pc);
	
	return(0);
	UNUSED(bcc);
}

int cracker_text_branch_link(cracker_p cj, unsigned bcc, uint32_t new_lr)
{
	LR = new_lr;
	cracker_text(cj, new_lr);

	return(0);
	UNUSED(bcc);
}

int cracker_text_branch_and_link(cracker_p cj, unsigned bcc, uint32_t new_pc, uint32_t new_lr)
{
	cracker_text_branch_link(cj, bcc, new_lr);
	return(cracker_text_branch(cj, bcc, new_pc));
}

int cracker_text_branch_cc(cracker_p cj, unsigned bcc, uint32_t new_pc, uint32_t next_pc)
{
	if(CC_AL != bcc)
		cracker_text(cj, next_pc);

	if(cj->collect_refs)
		return(cracker_text_branch(cj, bcc, new_pc));
	else
		return(0);

	UNUSED(bcc);
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
