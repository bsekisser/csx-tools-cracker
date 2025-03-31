#include "cracker_data.h"
#include "cracker_strings.h"
#include "cracker_symbol.h"
#include "cracker_trace.h"
#include "cracker.h"
#include "garmin.h"

/* **** */

#include "libarm/include/arm_ir.h"

#include "libbse/include/bitfield.h"
#include "libbse/include/err_test.h"
#include "libbse/include/log.h"
#include "libbse/include/unused.h"

/* **** */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* **** */

static int __scrounge_text_xxx(cracker_ref cj) {
	while(cracker_step(cj))
		;

	return(cj->symbol_count.added);
}

static int _scrounge_block__arm(cracker_ref cj, int safe) {
	if(0 != (PC & 3))
		return(0);

	switch(IR & 0xff000000) {
		case 0xea000000: /* b */
			return(__scrounge_text_xxx(cj));
	}

	switch(IR & 0xfffff000) {
		case 0xe28ff000: /* add pc, pc, #000 */
		case 0xe59ff000: /* ldr pc, pc, #000 */
			return(__scrounge_text_xxx(cj));
	}

	if(!safe) switch(IR & 0xff000000) {
		case 0xe5000000: /* ldr rd, rt, #000 */
			return(__scrounge_text_xxx(cj));
	}

	return(0);
	UNUSED(safe);
}

static symbol_ptr _scrounge_block__strings(cracker_ref cj, uint32_t start, uint32_t end) {
	const size_t byte_count = 1 + end - start;
	LOG("start: 0x%08x, end: 0x%08x, byte_count = 0x%08zx", start, end, byte_count);

	if(!cracker_pat_bounded(cj, &start, &end))
		return(0);

	symbol_ptr cjs = 0;
	for(uint32_t pat = start; pat < end; pat++) {
		cjs = cracker_data_string(cj, pat);
		if(cjs)
			break;
	}

	return(cjs);
}

static int _scrounge_block__thumb(cracker_ref cj, int safe) {
	PC |= 1;

	if(!safe && (0xf000f800 == (0xf000f800 & IR)))
		cracker_relocation(cj, PC);
	else do {
		if(0xb500 == (IR & 0xff00)) { /* push ...,lr */
				return(__scrounge_text_xxx(cj));
		} else if(0xb400 == (IR & 0xfe00)) { /* push */
				if(!safe)
					return(__scrounge_text_xxx(cj));
		} else switch(IR & 0xf800) {
			case 0x4800: /* ldr rr, pc, $000 */
				return(__scrounge_text_xxx(cj));
			case 0xf000: /* b 7ff */
				if(!safe)
					return(__scrounge_text_xxx(cj));
				break;
		}

		PC += sizeof(uint16_t);
	}while(IR >>= 16);

	return(0);
}

static int _scrounge_block(cracker_ref cj, uint32_t start, uint32_t end, int safe) {
	const size_t byte_count = 1 + end - start;
	LOG("start: 0x%08x, end: 0x%08x, byte_count = 0x%08zx", start, end, byte_count);

	if(sizeof(uint16_t) > byte_count)
		return(0);

	if(!cracker_pat_bounded(cj, &start, &end))
		return(0);

	const uint32_t eend = end - sizeof(uint16_t);

	for(uint32_t xpc = start & ~1; xpc < eend; xpc += sizeof(uint32_t)) {
		PC = xpc;

		if(xpc & 3)
			xpc &= ~3;

		if(!cracker_read_if(cj, PC, sizeof(uint32_t), &IR))
			break;

		if(_scrounge_block__arm(cj, safe))
			continue;
		else if(_scrounge_block__thumb(cj, safe))
			continue;
	}

	return(0);
}

static void _scrounge_pass_strings(cracker_ref cj) {
	/* scrounge pass */
	symbol_ptr rhs = cj->symbol_qhead;

	do {
		symbol_ref lhs = rhs;
		rhs = symbol_next(0, rhs);

		assert(lhs != rhs);

		if(rhs) {
			if(0 < cracker_symbol_intergap(cj, lhs, rhs)) {
				const uint32_t lhs_end_pat = 1 + lhs->end_pat;
				const uint32_t rhs_pat = -1 + rhs->pat;

				symbol_ptr cjs = _scrounge_block__strings(cj, lhs_end_pat, rhs_pat);
				if(cjs) {
					LOG("lhs = 0x%08" PRIxPTR ", cjs = 0x%08" PRIxPTR ", rhs = 0x%08" PRIxPTR,
						(uintptr_t)lhs, (uintptr_t)cjs, (uintptr_t)rhs);

					LOG("cjs.start = 0x%08x, cjs.end = 0x%08x", cjs->pat, cjs->end_pat);
					rhs = cjs;
				}
			}
		}
	}while(rhs);
}

static symbol_ptr _scrounge_pass(cracker_ref cj, symbol_ref cjs, int safe) {
	/* scrounge pass */
	symbol_ptr rhs = cjs ?: cj->symbol_qhead;

	do {
		symbol_ref lhs = rhs;
		rhs = symbol_next(0, rhs);

		assert(lhs != rhs);

		if(rhs) {
			if(0 < cracker_symbol_intergap(cj, lhs, rhs)) {
				const uint32_t lhs_end_pat = 1 + lhs->end_pat;
				const uint32_t rhs_pat = -1 + rhs->pat;

				if(_scrounge_block(cj, lhs_end_pat, rhs_pat, safe))
					continue;
			}
		}
	}while(rhs);

	LOG("symbol_count.added = 0x%08x", cj->symbol_count.added);
	return(0);
}

static void load_content(cracker_content_ptr content, const char* file_name)
{
	char out[256];
	snprintf(out, 254, "%s%s%s", LOCAL_RGNDIR, RGNFileName, file_name);

	LOG("opening %s", out);

	const int fd = open(out, O_RDONLY);
	ERR(fd);

	struct stat sb;
	ERR(fstat(fd, &sb));

	void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	ERR_NULL(data);

	close(fd);

	content->data = data;
	content->data_limit = data + sb.st_size;

	content->base = 0x10020000;
	content->size = sb.st_size;
	content->end = content->base + content->size;

	LOG("Start: 0x%08x, End: 0x%08x",
		content->base, content->end);
}

int main(void)
{
	cracker_ref cj = calloc(1, sizeof(cracker_t));

	const int loader = 1;

	if(loader)
		load_content(&cj->content, LOADER_FileName);
	else
		load_content(&cj->content, FIRMWARE_FileName);

	cracker_data(cj, cj->content.end, sizeof(uint32_t), 0);
	cracker_text(cj, cj->content.base);

	if(0) if(loader) { // loader
		unsigned string_table[] = {
			0x00000920, 0x0000418c, 0x0000419c, 0x000041a4,
			0x00004254, 0x00004570, 0x000046af, 0x000046c8,
			0x000048fc, 0x000050ac, 0x00006190, 0x00006c54,
			0x000077dc, 0x00007d9c, 0x00008ea4, 0x0000ab34,
			0x0000b114, 0x0000c0c0, 0x0000f780, 0x0000f794,
			0x00010a14, 0x00010a2c, 0x00010a48, 0x00010eb4,
			0x00012190, 0x000121a4, 0x000121a8, 0x000121bc,

			0x0001597b, 0x00015982,

			0x000159d8, 0x000159f0,	0x00015a08, 0x00015a1c,
			0x00015a2d,

			0x00015ae4,

			0x00015ccc, 0x00015ce0, 0x00015cf4, 0x00015d08,
			0x00015d1c, 0x00015d30, 0x00015d44,	0x00015d58,
			0x00015d6c, 0x00015d80, 0x00015d94,

			0x00015dbc,	0x00015dd0,

			0x00015e38, 0x00015e48,
			0x00015e5c, 0x00015e70, 0x00015e88, 0x00015f04,
			0x00015f1c, 0x00015f68, 0x00015f78, 0x00015f7c,
			0x00015fcc, 0x00015fdf, 0x00015ff2, 0x000166e6,
			0x00016e48, 0x00016e58, 0x00016e64, 0x00016e7c,
			0x000175bc, 0x0001ac04, 0x0001afa8, 0x0001afbc,
			0x0001cef8, 0x0001cf0c, 0x0001df38, 0x0001df50,
			0x0001df6c, 0x0001e3d8,

			0x0001f40c, 0x0001f420,
			0x0001f424, 0x0001f438,

			0x00022cc4, 0x00022cdc, 0x00022cf4, 0x00022d08,
			0x00022d19,

			0x00022d25, 0x00022d39,

			0x00022f4c, 0x00022f60, 0x00022f74,	0x00022f88,
			0x00022f9c, 0x00022fb0, 0x00022fc4,	0x00022fd8,
			0x00022fec, 0x00023000, 0x00023014,	0x00023028,
			0x0002303c, 0x00023084, 0x00023094,	0x000230a8,
			0x000230bc, 0x0002310c, 0x0002315a,

			0x00023280, 0x00023380, 0x00023480, 0x00023580,
			0x00023680, 0x00023780, 0x00023880, 0x00023980,
			0x00023a80, 0x00023b80, 0x00023c80, 0x00023d80,
			0x00023e80,

			0x00024250, 0x00024350, 0x00024450, 0x00024550,
			0x00024650, 0x00024750, 0x00024850, 0x00024950,
			0x00024a50, 0x00024b50, 0x00024c50, 0x00024d50,
			0x00024e50,

			0,
		};

		for(unsigned i = 0; string_table[i]; i++)
			cracker_data_string_rel(cj, string_table[i]);
	}

	uint32_t src = cj->content.end;
	do {
		IR = cracker_read(cj, src, sizeof(uint32_t));
		switch(IR) {
			case 0xffffa55a:
				cracker_data(cj, src, sizeof(uint32_t), 0);
				break;
			default:
				switch(IR & 0xffff) {
					case 0x5aa5:
					case 0xa55a:
						cracker_data(cj, src, sizeof(uint16_t), 0);
						break;
				}
				break;
		}

		src--;
	} while(src > cj->content.base);

	PC = cj->content.base;
	IR = cracker_read(cj, PC, sizeof(uint32_t));
	PC += sizeof(uint32_t);
	switch(IR) {
		case 0xea000002:
			cracker_data_ptr_read(cj, PC, sizeof(uint32_t)); /* end */
			PC += sizeof(uint32_t);
			cracker_data_ptr_read(cj, PC, sizeof(uint16_t)); /* hwid */
			PC += sizeof(uint32_t);
			cracker_data_ptr_read(cj, PC, sizeof(uint16_t)); /* sw_version */
			break;
		default:
			exit(-1);
	}

	int safe = 1;
	symbol_ptr cjs = 0;

	for(cj->symbol_pass = 1; cj->symbol_count.added; cj->symbol_pass++)
//	for(cj->symbol_pass = 1; cj->symbol_pass <= 3; cj->symbol_pass++)
	{
		cracker_pass(cj, 0);

		if(0 == cj->symbol_count.added) {
//			cjs = _scrounge_pass(cj, cjs, safe);
			safe = 0;
		} else
			cjs = 0;
	}

	_scrounge_pass_strings(cj);

	cj->collect_refs = 1;
	cj->symbol_pass = 0;

	cracker_pass(cj, 0);

	CORE_TRACE("/* **** **** **** **** */");

	printf("\n\n/* **** **** **** **** */\n\n");

	cracker_clear(cj);
	cracker_symbol_queue_log(cj, cj->symbol_qhead);

	free(cj);
	return(0);
}
