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

static void _cracker_symbol_enqueue(symbol_h h2sqh, symbol_p lhs, symbol_p cjs)
{
	if(0 == cjs)
		return;

	cracker_symbol_end(lhs, cjs->pat, "cracker_symbol_enqueue -- lhs");

	symbol_p rhs = lhs ? (symbol_p)lhs->qelem.next : 0;

	if(rhs)
		cracker_symbol_end(cjs, rhs->pat, "cracker_symbol_enqueue -- rhs");

	symbol_enqueue(h2sqh, lhs, cjs);
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

symbol_p cracker_data_rel_string(cracker_p cj, uint32_t pat)
{
	size_t len = 0;
	uint8_t* src = 0;
	int subtype = SYMBOL_STRING_CSTRING;

	pat += cj->content.base;
	if(_check_bounds(cj, pat, sizeof(uint8_t), (void**)&src)) {
		do {
			uint8_t c = *src++;
			if((c < ' ') || (c > 0x7f)) {
				len--;
				subtype = SYMBOL_STRING_NSTRING;
				break;
			} else if(len > 0x7f) {
				break;
			}
			len++;
		}while(*src);
	}

	if(0 == len) {
		LOG_ACTION(return(0));
	}

	symbol_p cjs = cracker_data(cj, pat, sizeof(uint8_t));

	BSET(cjs->type, SYMBOL_STRING);
	cjs->type_subtype = subtype;
	cjs->end_pat = pat + len;

	return(cjs);
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

		cjs = symbol_new(pat, size, SYMBOL_DATA);
		cjs->end_pat = pat + size - 1;

		_cracker_symbol_enqueue(sqh, lhs, cjs);
	}

	return(cjs);
}

static void cracker_dump_hex(cracker_p cj, uint32_t start, uint32_t end)
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

uint32_t cracker_data_ptr_read(cracker_p cj, uint32_t pat, size_t size)
{
	cracker_data(cj, pat, sizeof(uint32_t));
	pat = _read(cj, pat, sizeof(uint32_t));
	return(cracker_data_read(cj, pat, size));
}

uint32_t cracker_data_read(cracker_p cj, uint32_t pat, size_t size)
{
	cracker_data(cj, pat, size);
	return(_read(cj, pat, size));
}

int cracker_data_read_if(cracker_p cj, uint32_t pat, size_t size, uint32_t* data)
{
	cracker_data(cj, pat, size);
	return(cracker_read_if(cj, pat, size, data));
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

int cracker_read_if(cracker_p cj, uint32_t pat, size_t size, uint32_t* data)
{
	*data = 0;
	if(__pat_out_of_bounds(cj, pat, size))
		return(0);

	uint32_t offset = pat - cj->content.base;
	uint8_t* src = offset + cj->content.data;
	uint8_t* src_limit = cj->content.data + cj->content.size;
	
	for(uint i = 0; i < size; i++) {
		*data |= ((*src++) << (i << 3));
		if(src > src_limit)
			break;
	}

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

void cracker_symbol__log_data_log(cracker_p cj, symbol_p cjs, size_t size) {
	if(0 == BTST(cjs->size, size))
		return;

	_LOG_(", (uint%u_t", size << 3);

	if(_check_bounds(cj, cjs->pat, size, 0)) {
		uint32_t data = _read(cj, cjs->pat, size);

		switch(size) {
			case sizeof(uint32_t):
				_LOG_(" (0x%08x)", data);
				break;
			case sizeof(uint16_t):
				_LOG_(" (0x%04x)", data);
				break;
			case sizeof(uint8_t):
				_LOG_(" (0x%02x)", data);
				break;
		}
	}

	_LOG_(")");
}

void cracker_symbol__log_string(cracker_p cj, symbol_p cjs) {
	size_t len = cjs->end_pat - cjs->pat;

	LOG_START("0x%08x -- 0x%08x:", cjs->pat, cjs->end_pat);

	_LOG_(", refs: 0x%04x", cjs->refs);

	_LOG_(", len: 0x%04x", len);

	uint8_t* src = _check_bounds(cj, cjs->pat, sizeof(uint8_t), 0);
	if(src) {
		switch(cjs->type_subtype) {
			case SYMBOL_STRING_CSTRING:
				_LOG_(":: %s", src);
				break;
			case SYMBOL_STRING_NSTRING:
				_LOG_(":: %.*s", len - 1, src);
				break;
		}
	}

	LOG_END();
}

void cracker_symbol__log_data(cracker_p cj, symbol_p cjs) {
	LOG_START("0x%08x", cjs->pat);
		_LOG_(" -- 0x%08x", cjs->end_pat);

	_LOG_(": refs: 0x%04x", cjs->refs);

	_LOG_(", size: 0x%04x", cjs->size);

	cracker_symbol__log_data_log(cj, cjs, sizeof(uint32_t));
	cracker_symbol__log_data_log(cj, cjs, sizeof(uint16_t));
	cracker_symbol__log_data_log(cj, cjs, sizeof(uint8_t));

 	LOG_END();
}

void cracker_symbol__log_text(cracker_p cj, symbol_p cjs)
{
	const uint32_t pat_mask = ~3 >> cjs->thumb;

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

	LOG_END(", TEXT ENTRY%s", BEXT(cjs->type, SYMBOL_TEXT_XXX) ? " XXX" : "");

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

	const uint32_t end_pat = pat_masked - 1;

	if(0) LOG("%s%s-- pat: 0x%08x, end: 0x%08x, new_end: 0x%08x",
		name ?: "", name ? " " : "",
		cjs->pat, cjs->end_pat, end_pat);

	cjs->end_pat = end_pat;
}

size_t cracker_symbol_intergap(cracker_p cj, symbol_p lhs, symbol_p rhs)
{
	const uint32_t pat_bump = 3 >> lhs->thumb;
	const uint32_t pat_mask = ~pat_bump;

//	const uint32_t lhs_end_pat = lhs->end_pat & (~3 >> lhs->thumb);
	const uint32_t lhs_end_pat = lhs->end_pat;
	const uint32_t lhs_end_pat_bumped = (lhs_end_pat + pat_bump) & pat_mask;

	const uint32_t rhs_pat = rhs->pat & (~3 >> rhs->thumb);

	const size_t byte_count = rhs_pat - lhs_end_pat_bumped;
	return(byte_count);
}

void cracker_symbol_log(cracker_p cj, symbol_p cjs)
{
	if(0 == cj->core.trace)
		return;

	assert(0 != cjs);

	if(BTST(cjs->type, SYMBOL_TEXT))
		cracker_symbol__log_text(cj, cjs);

	if(BTST(cjs->type, SYMBOL_DATA)) {
		if(BTST(cjs->type, SYMBOL_STRING))
			cracker_symbol__log_string(cj, cjs);
		else
			cracker_symbol__log_data(cj, cjs);
	}
}

void cracker_symbol_queue_log(cracker_p cj, symbol_p sqh)
{
	cj->collect_refs = 0;
	cj->core.trace = 1;

	symbol_p cjs = sqh;

	do {
		cracker_symbol_log(cj, cjs);

		symbol_p lhs = cjs;
		cjs = symbol_next(0, cjs);

		if(cjs) {
			const size_t byte_count = cracker_symbol_intergap(cj, lhs, cjs);
			if(byte_count) {
				const uint32_t cjs_pat = cjs->pat & (~3 >> cjs->thumb);

				LOG("0x%08x -- 0x%08x === 0x%08x", lhs->end_pat, cjs_pat, byte_count);
				cracker_dump_hex(cj, 1 + lhs->end_pat, -1 + cjs_pat);
				printf("\n");
			}
		}
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

		const int thumb = pat & 1;
		if(0) LOG("pat = %c(0x%08x)", thumb ? 'T' : 'A', pat);

		cjs = symbol_new(pat, sizeof(uint32_t), SYMBOL_TEXT);

		cjs->end_pat = ~0U;
		cjs->in_bounds = (0 != _check_bounds(cj, pat, sizeof(uint32_t), 0));
		cjs->thumb = thumb;

		_cracker_symbol_enqueue(sqh, lhs, cjs);
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
