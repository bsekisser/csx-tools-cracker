#include "cracker_thumb.h"

#include "cracker_arm_ir.h"
#include "cracker_enum.h"
#include "cracker_strings.h"
#include "cracker_trace.h"
#include "cracker.h"

/* **** */

#include "bitfield.h"
#include "log.h"

/* **** */

#include <stdint.h>

/* **** */

#include "cracker_alubox.h"

/* **** */

static int _fetch(cracker_p cj)
{
	PC += sizeof(uint16_t);
	
	return(_read(cj, IP & ~1, sizeof(uint16_t)));
}

/* **** */

static int thumb_inst_add_rd_pcsp_i(cracker_p cj)
{
	const uint16_t imm8 = mlBFMOV(IR, 7, 0, 2);
	const int is_sp = BEXT(IR, 11);
	
	setup_rR_vR(N, is_sp ? rSP : rPC, is_sp ? 0 : (THUMB_PC & ~3U));
	
	setup_rR_dst_src(D, mlBFEXT(IR, 10, 8), rR(N));

	CORE_TRACE_START();

	_CORE_TRACE_("%s(%s, %s", imm8 ? "ADD" : "MOV", rR_NAME(D), rR_NAME(N));

	if(imm8) {
		_CORE_TRACE_(", 0x%03x", imm8);
	}

	_CORE_TRACE_(")");

	if(rPC == rR(N)) {
		vGPR_rR(D) = vR(N) + imm8;

		_CORE_TRACE_("; /* 0x%08x */", vGPR_rR(D));
	}

	CORE_TRACE_END();

	return(1);
}

static int thumb_inst_add_sub_rn_rd(cracker_p cj)
{
	const int bit_i = BEXT(IR, 10);
	const uint8_t op2 = BEXT(IR, 9);
	const char* ops[2] = { "ADDS", "SUBS" };

	if(bit_i)
		setup_rR_vR(M, ~0, mlBFEXT(IR, 8, 6));
	else
		setup_rR_src(M, mlBFEXT(IR, 8, 6));

	setup_rR_vR_src(N, mlBFEXT(IR, 5, 3));
	setup_rR_dst_src(D, mlBFEXT(IR, 2, 0), rR(N));

	CORE_TRACE_START("%s(%s, %s, ", ops[op2], reg_name[rR(D)],
		reg_name[rR(N)]);

	if(bit_i) {
		_CORE_TRACE_("0x%01x", vR(M));
	} else {
		_CORE_TRACE_("%s", reg_name[rR(M)]);
	}

	_CORE_TRACE_(")");

	if((rPC == rR_SRC(N)) && bit_i && vR(M)) {
		uint8_t aluop[2] = { ARM_ADD, ARM_SUB };
		
		vR(D) = alubox(aluop[op2], vR(N), vR(M));
		vGPR_rR(D) = vR(D);
		
		const char aluopc[2] = { '+', '-' };
		
		_CORE_TRACE_("; /* 0x%08x %c 0x%08x = 0x%08x */ XXX",
			vR(N), aluopc[op2], vR(M), vR(D));
	}

	CORE_TRACE_END();
	
	return(1);
}

static int thumb_inst_add_sub_sp_i(cracker_p cj)
{
	const uint16_t imm7 = mlBFMOV(IR, 6, 0, 2);
	const int is_sub = BEXT(IR, 7);
	
	CORE_TRACE("%s(SP, 0x%03x)", is_sub ? "SUB" : "ADD", imm7);

	return(1);
}

enum {
	THUMB_ASCM_MOV,
	THUMB_ASCM_CMP,
	THUMB_ASCM_ADD,
	THUMB_ASCM_SUB,
};

static int thumb_inst_ascm_i(cracker_p cj)
{
	const uint imm8 = mlBFEXT(IR, 7, 0);
	const uint op = mlBFEXT(IR, 12, 11);
	const char* opname[4] = { "MOVS", "CMPS", "ADDS", "SUBS" };
	const uint aluop[4] = { ARM_MOV, ARM_CMP, ARM_ADD, ARM_SUB };
	
	setup_rR_vR_src(N, mlBFEXT(IR, 10, 8));

	if(rPC == rR_SRC(N))
		setup_rR_vR_dst_src(D, rR(N), rR(N));
	else
		setup_rR_dst_src(D, rR(N), rR(N));

	switch(op) {
		case THUMB_ASCM_ADD:
		case THUMB_ASCM_CMP:
		case THUMB_ASCM_MOV:
		case THUMB_ASCM_SUB: {
			CORE_TRACE_START("%s(%s, 0x%02x)",
				opname[op], reg_name[rR(D)], imm8);

			if(rPC == rR_SRC(N)) {
				vR(D) = alubox(aluop[op], vR(N), imm8);
				vGPR_rR(D) = vR(D);

				const char opc[4] = { '=', '-', '+', '-' };

				_CORE_TRACE_("; /* 0x%08x %c 0x%02x = 0x%08x */ XXX",
					vR(N), opc[op], imm8, vR(D));
			}
			
			CORE_TRACE_END();
		}break;
		default:
			CORE_TRACE("ASCM_OP = 0x%02x", op);
			return(0);
			LOG_ACTION(exit(-1));
			break;
	}

	return(1);
}

static int thumb_inst_b(cracker_p cj)
{
	const int32_t eao = mlBFMOVs(IR, 10, 0, 1);
	const uint32_t new_pc = PC + eao;

	CORE_TRACE("B(0x%08x); /* 0x%08x + 0x%03x*/", new_pc & ~1, PC, eao);

	cracker_text(cj, new_pc | 1);
	
	return(0);
}

static int thumb_inst_bcc(cracker_p cj)
{
	CCv = mlBFEXT(IR, 11, 8);
	const int32_t imm8 = mlBFMOVs(IR, 7, 0, 1);

	const uint32_t new_pc = (THUMB_PC + imm8) | 1;

	CORE_TRACE("B(0x%08x); /* 0x%08x + 0x%03x */", new_pc & ~1, THUMB_PC, imm8);

	cracker_text(cj, new_pc);

	const uint32_t new_lr = PC | 1;
	return(cracker_text_branch_link(cj, new_lr));
}

static int thumb_inst_bx_blx(cracker_p cj)
{
	const int link = BEXT(IR, 7);
	
	setup_rR_vR(M, mlBFEXT(IR, 6, 3), vR_GPR(M));
	
	CORE_TRACE_START();
	
	_CORE_TRACE_("B%sX(%s)", link ? "L" : "", rR_NAME(M));

	if(rPC == rR_SRC(M)) {
		_CORE_TRACE_("; /* 0x%08x */", vR(M));

		cracker_text(cj, vR(M));
	}

	CORE_TRACE_END();

	if(link) {
		const uint32_t new_lr = PC | 1;
		return(cracker_text_branch_link(cj, new_lr));
	}

	return(0);
}

static int thumb_inst_bxx__bl_blx(cracker_p cj, uint32_t eao, int blx)
{
	const uint32_t new_pc = ((LR + eao) & (blx ? ~3 : ~1)) | (blx ? 0 : 1);

	if(0) LOG("LR = 0x%08x, PC = 0x%08x", LR, new_pc);

	const uint32_t new_lr = PC | 1;

	int splat = (new_pc == PC);
	CORE_TRACE("BL%s(0x%08x); /* 0x%08x + %s0x%08x, LR = 0x%08x */",
		blx ? "X" : "", new_pc & ~1, PC, splat ? "x" : "", eao, new_lr & ~1);

	if(0) LOG("LR = 0x%08x, PC = 0x%08x", new_lr, new_pc);

	cracker_text(cj, new_pc);

	return(cracker_text_branch_link(cj, new_lr));
}

static int thumb_inst_bxx_bl_blx(cracker_p cj)
{
	const uint32_t eao = mlBFMOV(IR, 10, 0, 1);

	return(thumb_inst_bxx__bl_blx(cj, eao, 1 ^ BEXT(IR, 12)));
}

static int thumb_inst_bxx_prefix(cracker_p cj)
{
	const int32_t eao_prefix = mlBFMOVs(IR, 10, 0, 12);
	const uint8_t h_prefix = mlBFEXT(IR, 12, 11);

	if(2 != h_prefix) {
		LOG_ACTION(exit(-1));
	}

	LR = 2 + PC + eao_prefix;

	const uint32_t ir_suffix = _read(cj, PC & ~1, sizeof(uint16_t));
	if(0xe800 == (ir_suffix & 0xe800)) {
		const int blx = 1 ^ BEXT(ir_suffix, 12);
		
		if(blx && (ir_suffix & 1))
			goto return_not_prefix_suffix;
		
		IR = (IR << 16) | ir_suffix;
		PC += 2;
		
		return(thumb_inst_bxx_bl_blx(cj));
	}

return_not_prefix_suffix:
	CORE_TRACE("BL/BLX(0x%08x) /* LR = 0x%08x */", eao_prefix, LR);
	return(0);
}

static int thumb_inst_dpr_rms_rdn(cracker_p cj)
{
	const char* _dpr_ops[16] = {
		"AND", "EOR", "LSL", "LSR", "ASR", "ADC", "SBC", "ROR",
		"TST", "RSB", "CMP", "CMN", "ORR", "MUL", "BIC", "MVN" };

	const uint operation = mlBFEXT(IR, 9, 6);
	const char* dpr_ops = _dpr_ops[operation];

	setup_rR_vR(D, mlBFEXT(IR, 2, 0), 0);
	setup_rR_vR(M, mlBFEXT(IR, 5, 3), 0);

	rR_SRC(D) = 0;
	
	CORE_TRACE("%s(%s, %s)", dpr_ops, rR_NAME(D), rR_NAME(M));

	return(1);
}

static int thumb_inst_ldst_bwh_o_rn_rd(cracker_p cj)
{
	const int bit_h = BEXT(IR, 15);
	const int bit_b = BEXT(IR, 12);
	const int bit_l = BEXT(IR, 11);

	const uint8_t imm5 = mlBFEXT(IR, 10, 6);
	
	setup_rR_vR_src(N, mlBFEXT(IR, 5, 3));
	
	if(bit_l)
		setup_rR_vR_dst_src(D, mlBFEXT(IR, 2, 0), rR(N));
	else
		setup_rR_src(D, mlBFEXT(IR, 2, 0));
	
	const char* bwh = (bit_h ? "H" : (bit_b ? "B" : ""));

	CORE_TRACE_START();

	_CORE_TRACE_("%sR%s(%s, %s", bit_l ? "LD" : "ST", bwh,
		rR_NAME(D), rR_NAME(N));

	const uint16_t offset = imm5 << (bit_h ? 1 : (bit_b ? 0 : 2));
	const size_t size = (bit_h ? 2: (bit_b ? 1 : 4));

	if(imm5) {
		_CORE_TRACE_(", 0x%03x", offset);
	}

	_CORE_TRACE_(")");

	if(rPC == rR_SRC(N)) {
		const uint32_t pat = vR(N) + offset;

		cracker_data(cj, pat, size);

		_CORE_TRACE_("; /* [0x%08x] */", pat);
	}

	CORE_TRACE_END();
	
	return(1);
}

static int thumb_inst_ldst_op_rm_rn_rd(cracker_p cj)
{
//	const int opcode mlBFEXT(IR, 11, 9);
	const int flag_s = (3 == mlBFEXT(IR, 10, 9));
	const int bit_b = !flag_s ? BEXT(IR, 10) : !BEXT(IR, 11);
	const int bit_h = !flag_s ? BEXT(IR, 9) : BEXT(IR, 11);
	const int bit_l = !flag_s ?  BEXT(IR, 11) : 0;

	setup_rR_vR_src(M, mlBFEXT(IR, 8, 6));
	setup_rR_vR_src(N, mlBFEXT(IR, 5, 3));
	
	if(bit_l)
		setup_rR_vR_dst_src(D, mlBFEXT(IR, 2, 0), rR(N));
	else
		setup_rR_vR_src(D, mlBFEXT(IR, 2, 0));
	
	const char* bwh = (bit_h ? "H" : (bit_b ? "B" : ""));

	CORE_TRACE_START();

	_CORE_TRACE_("%sR%s%s(%s, %s, %s)", bit_l ? "LD" : "ST", bwh,
		flag_s ? "S" : "",
		rR_NAME(D), rR_NAME(N), rR_NAME(M));

	CORE_TRACE_END();
	
	return(1);
}

static int thumb_inst_ldst_rd_i(cracker_p cj)
{
	const int bit_l = BEXT(IR, 11);
	const uint16_t offset = mlBFMOV(IR, 7, 0, 2);

	switch(mlBFTST(IR, 15, 12)) {
		case 0x4000: /* pc */
			setup_rR_vR(N, rPC, THUMB_PC & ~3);
			break;
		case 0x9000: /* sp */
			setup_rR_vR(N, rSP, 0);
			break;
		default:
			CORE_TRACE();
			LOG_ACTION(exit(-1));
			break;
	}

	if(bit_l)
		setup_rR_vR_dst_src(D, mlBFEXT(IR, 10, 8), rR(N));
	else
		setup_rR_src(D, mlBFEXT(IR, 10, 8));

	CORE_TRACE_START("%sR(%s, %s, 0x%04x", bit_l ? "LD" : "ST",
		reg_name[rR(D)], reg_name[rR(N)], offset);

	if(rPC == rR(N)) {
		const uint ea = vR(N) + offset;
		
		cracker_data(cj, ea, sizeof(uint32_t));

		const uint32_t data = _read(cj, ea, sizeof(uint32_t));
		vGPR_rR(D) = data;

		_CORE_TRACE_("); /* [0x%08x]:0x%08x */", ea, data);
	}

	CORE_TRACE_END();

	return(1);
}

static int thumb_inst_ldstm(cracker_p cj)
{
	const int bit_l = BEXT(IR, 11);

	setup_rR_src(N, mlBFEXT(IR, 10, 8));

	char reglist[9], *dst = reglist;

	for(int i = 0; i <= 7; i++) {
		uint8_t rr = BEXT(IR, i);
		*dst++ = rr ? ('0' + i) : '.';
		
		if(bit_l)
			cracker_reg_dst(cj, rr);
		else
			cracker_reg_src(cj, rr);
	}
	*dst = 0;

	CORE_TRACE("%sMIA(%s.WB, {%s})", bit_l ? "LD" : "ST",
		reg_name[rR(N)], reglist);

	return(1);
}

static int thumb_inst_pop_push(cracker_p cj)
{
	const int bit_l = BEXT(IR, 11);
	const int bit_r = BEXT(IR, 8);

	char reglist[9], *dst = reglist;

	for(int i = 0; i <= 7; i++) {
		uint8_t rr = BEXT(IR, i);
		*dst++ = rr ? ('0' + i) : '.';
		
		if(bit_l)
			cracker_reg_dst(cj, rr);
		else
			cracker_reg_src(cj, rr);
	}
	*dst = 0;

	const char *pclrs = bit_r ? (bit_l ? ", PC" : ", LR") : "";
	CORE_TRACE("%s(rSP, r{%s%s});", bit_l ? "POP" : "PUSH", reglist, pclrs);

	return(!(bit_r && bit_l));
}

static int thumb_inst_sdp_rms_rdn(cracker_p cj)
{
	const uint8_t operation = mlBFEXT(IR, 9, 8);
	if(0x03 == operation) { /* invalid */
		return(0);
	}

	const char* _sos[] = { "ADD", "CMP", "MOV", 0 };
	const char* sos = _sos[operation];

	setup_rR_src(M, mlBFEXT(IR, 6, 3));
	setup_rR_dst_src(D, mlBFEXT(IR, 2, 0) | BMOV(IR, 7, 3), rR(M));

	CORE_TRACE("%s(%s, %s)", sos, reg_name[rR(D)],
		reg_name[rR(M)]);

	return(rPC != rR(D));
}

static int thumb_inst_shift_i(cracker_p cj)
{
	const uint sop = mlBFEXT(IR, 12, 11);

	setup_rR_vR(S, ~0, mlBFEXT(IR, 10, 6));
	setup_rR_src(M, mlBFEXT(IR, 5, 3));
	setup_rR_dst_src(D, mlBFEXT(IR, 2, 0), rR(M));
	
	switch(sop) {
		case SOP_ASR:
		case SOP_LSR:
			if(!vR(S))
				vR(S) = 32;
		case SOP_LSL:
		case SOP_ROR:
			break;
		default:
			CORE_TRACE("SHIFT_I_OP = 0x%02x", sop);
			LOG_ACTION(exit(-1));
			break;
	}

	CORE_TRACE("%s(%s, %s, 0x%02x)",
		shift_op_string[sop], reg_name[rR(D)],
		reg_name[rR(M)], vR(S));

	return(1);
}

/* **** */

#define _return(_x) _x

static int thumb_step__fail_decode(cracker_p cj, int crap)
{
	for(uint8_t lsb = 13; lsb > 8; lsb--) {
		uint32_t opcode = mlBFTST(IR, 15, lsb);
		CORE_TRACE("IR[[15:%01u]:0] = 0x%04x", lsb, opcode);

		switch(opcode) {
			case 0xe000:
				lsb = 11;
				break;
			case 0xf800:
				CORE_TRACE("BL/BLX: H[IR[12:11]] = %01u", mlBFEXT(IR, 12, 11));
				break;
			default:
				break;
		}
	}

	if(crap)
		LOG_ACTION(exit(-1));

	cracker_text_end(cj, IP);
	return(0);
}

static int thumb_step_group0_0000_1fff(cracker_p cj)
{
	switch(mlBFTST(IR, 15, 10)) {
		case 0x1800: /* 0001 10xx xxxx xxxx */
			return(thumb_inst_add_sub_rn_rd(cj));
		case 0x1c00: /* 0001 11xx xxxx xxxx */
			return(thumb_inst_add_sub_rn_rd(cj));
		default:
			return(thumb_inst_shift_i(cj));
	}
	
	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}

static int thumb_step_group2_4000_5fff(cracker_p cj)
{
	if(0x5000 == mlBFTST(IR, 15, 12)) /* 0101 xxxx xxxx xxxx */
		return(thumb_inst_ldst_op_rm_rn_rd(cj));
	else if(0x4800 == mlBFTST(IR, 15, 11)) /* 0100 1xxx xxxx xxxx */
		return(thumb_inst_ldst_rd_i(cj));
	else {
		switch(mlBFTST(IR, 15, 10)) {
			case 0x4000: /* 0100 00xx xxxx xxxx */
				return(thumb_inst_dpr_rms_rdn(cj));
			case 0x4400: /* 0100 01xx xxxx xxxx */
				switch(mlBFTST(IR, 15, 8)) {
					case 0x4700: /* 0100 0111 xxxx xxxx */
						return(thumb_inst_bx_blx(cj));
					default: /* 0100 01xx xxxx xxxx */
						return(thumb_inst_sdp_rms_rdn(cj));
				}
				break;
		}
	}

	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}

static int thumb_step_group5_b000_bfff(cracker_p cj)
{
	switch(mlBFTST(IR, 15, 8)) {
		case 0xb000: /* 1011 0000 xxxx xxxx */
			return(thumb_inst_add_sub_sp_i(cj));
		case 0xb400: /* 1011 0100 xxxx xxxx */
		case 0xb500: /* 1011 0101 xxxx xxxx */
		case 0xbc00: /* 1011 1100 xxxx xxxx */
		case 0xbd00: /* 1011 1101 xxxx xxxx */
			return(thumb_inst_pop_push(cj));
	}

	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}

static int thumb_step_group6_c000_dfff(cracker_p cj)
{
	if(BTST(IR, 12)) {
		switch(mlBFTST(IR, 15, 8)) {
			case 0xde00: /* undefined */
			case 0xdf00: /* swi */
				break;
			default:
				return(thumb_inst_bcc(cj));
		}
	} else
		return(thumb_inst_ldstm(cj));

	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}

static int thumb_step_group7_e000_ffff(cracker_p cj)
{
	switch(mlBFTST(IR, 15, 11)) {
		case 0xe000:
			return(thumb_inst_b(cj));
		case 0xe800:
			if(IR & 1)
				break;
			__attribute__((fallthrough));
		case 0xf800:
			return(thumb_inst_bxx_bl_blx(cj));
		case 0xf000:
			return(thumb_inst_bxx_prefix(cj));
	}

	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}

int thumb_step(cracker_p cj)
{
	IR = _fetch(cj);
	CCv = CC_AL;

	uint32_t group = mlBFTST(IR, 15, 13);
	switch(group) {
		case 0x0000: /* 000x xxxx xxxx xxxx */
			return(thumb_step_group0_0000_1fff(cj));
		case 0x2000: /* 001x xxxx xxxx xxxx */
			return(thumb_inst_ascm_i(cj));
		case 0x4000: /* 010x xxxx xxxx xxxx */
			return(thumb_step_group2_4000_5fff(cj));
		case 0x6000: /* 011x xxxx xxxx xxxx */
			return(thumb_inst_ldst_bwh_o_rn_rd(cj));
		case 0x8000: /* 100x xxxx xxxx xxxx */
			if(BTST(IR, 12)) /* 1001 xxxx xxxx xxxx */
				return(thumb_inst_ldst_rd_i(cj));
			else /* 1000 xxxx xxxx xxxx */
				return(thumb_inst_ldst_bwh_o_rn_rd(cj));
			break;
		case 0xa000: /* 101x xxxx xxxx xxxx */
			if(BTST(IR, 12)) /* 1011 xxxx xxxx xxxx */
				return(thumb_step_group5_b000_bfff(cj));
			else /* 1010 xxxx xxxx xxxx */
				return(thumb_inst_add_rd_pcsp_i(cj));
			break;
		case 0xc000: /* 110x xxxx xxxx xxxx */
			return(thumb_step_group6_c000_dfff(cj));
		case 0xe000: /* 111x xxxx xxxx xxxx */
			return(thumb_step_group7_e000_ffff(cj));
	}

	LOG_ACTION(return(thumb_step__fail_decode(cj, 1)));
}
