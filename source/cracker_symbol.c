#include "symbol.h"

#include "cracker_arm.h"
#include "cracker_symbol.h"
#include "cracker_thumb.h"
#include "cracker.h"

/* **** */

#include "libbse/include/bitfield.h"
#include "libbse/include/log.h"
#include "libbse/include/unused.h"

/* **** */

#include <assert.h>
#include <stdint.h>

/* **** */

static void cracker_symbol__log__data_log(cracker_ref cj, symbol_ref cjs, size_t size) {
	if(0 == BTST(cjs->size, size))
		return;

	_LOG_(", (uint%zu_t", size << 3);

	uint32_t data = 0;
	if(cracker_read_if(cj, cjs->pat, size, &data)) {
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

static void cracker_symbol__log_data(cracker_ref cj, symbol_ref cjs) {
	LOG_START("0x%08x", cjs->pat);
		_LOG_(" -- 0x%08x", cjs->end_pat);

	_LOG_(": refs: 0x%04x", cjs->refs);

	_LOG_(", size: 0x%04zx", cjs->size);

	cracker_symbol__log__data_log(cj, cjs, sizeof(uint32_t));
	cracker_symbol__log__data_log(cj, cjs, sizeof(uint16_t));
	cracker_symbol__log__data_log(cj, cjs, sizeof(uint8_t));

 	LOG_END();
}

static void cracker_symbol__log_string(cracker_ref cj, symbol_ref cjs) {
	const size_t len = 1 + (cjs->end_pat - cjs->pat);

	LOG_START("0x%08x -- 0x%08x:", cjs->pat, cjs->end_pat);

	_LOG_(", refs: 0x%04x", cjs->refs);

	_LOG_(", len: 0x%04zx", len);

	uint8_t* src = 0;
	if(cracker_pat_src_if(cj, cjs->pat, sizeof(uint8_t), &src)) {
		switch(cjs->type_subtype) {
			case SYMBOL_STRING_CSTRING:
				_LOG_(":: %s", src);
				break;
			case SYMBOL_STRING_NSTRING:
				_LOG_(":: %.*s", (int)(len - 1), src);
				break;
		}
	}

	LOG_END();
}

static void cracker_symbol__log_text(cracker_ref cj, symbol_ref cjs)
{
	const uint32_t pat_mask = ~(3 >> cjs->thumb);

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
		if(cracker_symbol_step_block(cj, cjs))
			cracker_clear(cj);

		printf("\n");
	}

	UNUSED(cj);
}

/* **** */

void cracker_symbol_end(symbol_ref cjs, uint32_t pat, const char* name)
{
	if(0 == cjs)
		return;

	const uint32_t pat_mask = ~(3 >> cjs->thumb);
	const uint32_t pat_masked = pat & pat_mask;

	if(pat_masked <= cjs->pat)
		return;

	if(pat_masked >= cjs->end_pat)
		return;

	const uint32_t end_pat = pat_masked - 1;

	if(0) LOG("%s%s-- pat: 0x%08x, end: 0x%08x, new_end: 0x%08x",
		name ?: "", name ? " " : "",
		cjs->pat, cjs->end_pat, end_pat);

	cjs->end_pat = end_pat;
}

void cracker_symbol_enqueue(symbol_href h2sqh, symbol_ref lhs, symbol_ref cjs)
{
	if(0 == cjs)
		return;

	cracker_symbol_end(lhs, cjs->pat, "cracker_symbol_enqueue -- lhs");

	symbol_ref rhs = lhs ? (symbol_ptr)lhs->qelem.next : 0;

	if(rhs)
		cracker_symbol_end(cjs, rhs->pat, "cracker_symbol_enqueue -- rhs");

	symbol_enqueue(h2sqh, lhs, cjs);
}

symbol_ptr cracker_symbol_find(cracker_ref cj, symbol_href h2lhs, uint32_t pat, uint32_t mask)
{
	symbol_href sqh = &cj->symbol_qhead;
	return(symbol_find_pat(sqh, h2lhs, pat, mask));
}

int32_t cracker_symbol_intergap(cracker_ref cj, symbol_ref lhs, symbol_ref rhs)
{
	if(!lhs || !rhs) return(0);

	const uint32_t pat_bump = 3 >> lhs->thumb;
	const uint32_t pat_mask = ~pat_bump;

//	const uint32_t lhs_end_pat = lhs->end_pat & ~(3 >> lhs->thumb);
	const uint32_t lhs_end_pat = lhs->end_pat;
	const uint32_t lhs_end_pat_bumped = (lhs_end_pat + pat_bump) & pat_mask;

	const uint32_t rhs_pat = rhs->pat & ~(3 >> rhs->thumb);

	const int32_t byte_count = rhs_pat - lhs_end_pat_bumped;

	return(byte_count);
	UNUSED(cj);
}

void cracker_symbol_log(cracker_ref cj, symbol_ref cjs)
{
	if(0 == cj->core.trace.enabled)
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

void cracker_symbol_queue_log(cracker_ref cj, symbol_ref sqh)
{
	cj->collect_refs = 0;
	cj->core.trace.enabled = 1;

	symbol_ptr cjs = 0, lhs = 0;

	while(symbol_next(sqh, &lhs, &cjs, 0)) {
		cracker_symbol_log(cj, cjs);

		const size_t byte_count = cracker_symbol_intergap(cj, lhs, cjs);
		if(byte_count) {
			const uint32_t cjs_pat = cjs->pat & ~(3 >> cjs->thumb);

			LOG("0x%08x -- 0x%08x === 0x%08zx", lhs->end_pat, cjs_pat, byte_count);
			cracker_dump_hex(cj, 1 + lhs->end_pat, -1 + cjs_pat);
			printf("\n");
		}
	}
}

int cracker_symbol_step(cracker_ref cj, symbol_ref cjs)
{
	if(cjs->thumb)
		return(thumb_step(cj));

	return(arm_step(cj));
}

int cracker_symbol_step_block(cracker_ref cj, symbol_ref cjs)
{
	PC = cjs->pat;
	while(PC <= cjs->end_pat)
		if(0 == cracker_symbol_step(cj, cjs))
			return(-1);

	return(0);
}
