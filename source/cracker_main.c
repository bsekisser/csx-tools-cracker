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
	uint32_t res = 0;
	uint8_t* src = _check_bounds(cj, pat, size, 0);

	if(0 == src)
	{
//		return(0xdeadbeef);
		return(-1);
		LOG_ACTION(exit(-1));
	}

	for(uint i = 0; i < size; i++)
		res |= ((*src++) << (i << 3));

	return(res);
}

/* **** */

void cracker_symbol_log_data(cracker_p cj, symbol_p cjs) {
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

void cracker_symbol_log_text(cracker_p cj, symbol_p cjs)
{
	if(0 == cj->core.trace)
		return;

	LOG_START("0x%08x -- 0x%08x:", cjs->pat, cjs->end_pat);

	_LOG_(" refs: 0x%04x", cjs->refs);

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
	
	UNUSED(cj);
}

void cracker_symbol_log(cracker_p cj, symbol_p cjs)
{
	if(0 == cj->core.trace)
		return;

	if(BTST(cjs->type, SYMBOL_TEXT))
		cracker_symbol_log_text(cj, cjs);

	if(BTST(cjs->type, SYMBOL_DATA))
		cracker_symbol_log_data(cj, cjs);
}

void cracker_symbol_queue_log(cracker_p cj, symbol_p sqh)
{
	symbol_p cjs = sqh;

	do {
		cracker_symbol_log(cj, cjs);

		cjs = symbol_next(0, cjs, 0);
	}while(cjs);
}

/* **** */

void cracker_clear(cracker_p cj)
{
	for(int i = 0; i < 16; i++) {
		vGPR(i) = 0;
		GPR(i).src = ~0;
		GPR(i).isPtr = 0;
	}

	for(int i = 0; i < REG_COUNT; i++) {
		rRx(i) = 0;
		vRx(i) = 0;
	}
}

symbol_p cracker_data(cracker_p cj, uint32_t pat, size_t size)
{
	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0, rhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, pat, &lhs, &rhs);

	if(cjs) {
//		assert(BTST(cjs->size, size));
//		assert(BTST(cjs->type, SYMBOL_DATA));

		BSET(cjs->size, size);
		BSET(cjs->type, SYMBOL_DATA);

		cjs->refs++;
	} else {
		cjs = symbol_new(pat, size, SYMBOL_DATA);
		symbol_enqueue(sqh, lhs, cjs, rhs);
	}

	return(cjs);
}

void cracker_reg_dst(cracker_p cj, uint8_t r)
{
	if(!cj->symbol)
		return;

	BSET(cj->symbol->reg.dst, r);
}

void cracker_reg_src(cracker_p cj, uint8_t r)
{
	if(!cj->symbol)
		return;

	if(BTST(cj->symbol->reg.dst, r))
		return;
	
	BSET(cj->symbol->reg.src, r);
}

int cracker_step(cracker_p cj)
{
	IP = PC;

	if(IP & 1)
		return(thumb_step(cj));

	return(arm_step(cj));
}

symbol_p cracker_text(cracker_p cj, uint32_t pat)
{
//	const uint32_t pat_thumb = pat & ~1U;
	const int thumb = pat & 1;

	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0, rhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, pat, &lhs, &rhs);

	if(cjs) {
//		assert(BTST(cjs->size, sizeof(uint32_t)));
//		assert(BTST(cjs->type, SYMBOL_TEXT));

		BSET(cjs->size, sizeof(uint32_t));
		BSET(cjs->type, SYMBOL_TEXT);

		cjs->refs++;
	} else {
		cjs = symbol_new(pat, sizeof(uint32_t), SYMBOL_TEXT);
		symbol_enqueue(sqh, lhs, cjs, rhs);

		cjs->end_pat = ~0U;
		cjs->in_bounds = !!_check_bounds(cj, pat, sizeof(uint32_t), 0);
		cjs->thumb = thumb;

		if(lhs || rhs) {
			cracker_text_end(cj, pat);
		}
	}

	return(cjs);
}

void cracker_symbol_end(symbol_p cjs, uint32_t pat, const char* name)
{
	if(0 == cjs)
		return;

	if((cjs->pat < pat) && (cjs->end_pat >= pat)) {
		uint32_t end_pat = pat - (4 >> cjs->thumb);

		if(0) LOG("%s%s-- pat: 0x%08x, end: 0x%08x, new_end: 0x%08x",
			name ?: "", name ? " " : "",
			cjs->pat, cjs->end_pat, end_pat);

		cjs->end_pat = end_pat;
	}

}

symbol_p cracker_text_end(cracker_p cj, uint32_t pat)
{
	const int thumb = pat & 1;
	pat &= ~1U;

	symbol_h sqh = &cj->symbol_qhead;

	symbol_p lhs = 0, rhs = 0;
	symbol_p cjs = symbol_find_pat(sqh, pat, &lhs, &rhs);

	if(lhs && lhs->in_bounds) {
		cracker_symbol_end(lhs, pat, "lhs");
	}

	if(cjs && cjs->in_bounds) {
		cracker_symbol_end(cjs, pat, "cjs");
	}

	return(cjs);
}

int cracker_text_end_if(cracker_p cj, uint32_t pat, int end)
{
	if(end)
		cracker_text_end(cj, pat);
	
	return(0 == end);
}

/* **** */

void cracker_pass(cracker_p cj, uint pass, int trace)
{
	printf("\n\n/* pass %u **** **** **** **** */\n\n", pass);

	cj->core.trace = !!trace;

	symbol_p cjs = cj->symbol_qhead;

	while(cjs) {
		if(cjs->pass < pass) {
			cjs->pass++;
			
			cracker_symbol_log(cj, cjs);
			
			if(BTST(cjs->type, SYMBOL_TEXT)) {
					cracker_clear(cj);

					cj->symbol = cjs;
					PC = cjs->pat;
					while(PC <= cjs->end_pat) {
						if(!cracker_step(cj))
							break;
					}

				if(trace)
					printf("\n");
			}

		}

		cjs = symbol_next(0, cjs, 0);
	}
}

int main(void)
{
	int fd;
//	ERR(fd = open(RGNDirPath RGNFileName "_loader.bin", O_RDONLY));
	ERR(fd = open(RGNDirPath RGNFileName "_firmware.bin", O_RDONLY));

	struct stat sb;

	ERR(fstat(fd, &sb));

	void *data;
	ERR_NULL(data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));

	close(fd);

	cracker_t cjt, *cj = &cjt;

	cjt.content.data = data;
	cjt.content.base = 0x10020000;
	cjt.content.size = sb.st_size;
	cjt.content.end = cjt.content.base + cjt.content.size;

	LOG("Loaded: " RGNFileName "_loader.bin... Start: 0x%08x, End: 0x%08x",
		cjt.content.base, cjt.content.end);

	symbol_p cjs = cracker_text(cj, cj->content.base);
	cj->symbol = cjs;

	cracker_pass(cj, 1, 0);
	cracker_pass(cj, 2, 1);

	CORE_TRACE("/* **** **** **** **** */");

	printf("\n\n/* **** **** **** **** */\n\n");

	cracker_symbol_queue_log(cj, cj->symbol_qhead);

	munmap(data, sb.st_size);
}
