#include "cracker_arm.h"
#include "cracker_arm_enum.h"
#include "cracker_arm_ir.h"
#include "cracker_disasm.h"
#include "cracker_enum.h"
#include "cracker_strings.h"
#include "cracker_trace.h"
#include "cracker.h"

#include "soc_core_arm_inst.h"

/* **** */

#include "bitfield.h"
#include "shift_roll.h"
#include "log.h"

/* **** */

#include <assert.h>
#include <stdio.h>

/* **** */

#include "cracker_alubox.h"

static uint32_t _fetch(cracker_p cj)
{
	PC += sizeof(uint32_t);

	return(_read(cj, IP, sizeof(uint32_t)));
}

static void _shifter_operand(cracker_p cj)
{
	switch(ARM_DPI_SOP) {
		case SOP_ASR:
			vR(SOP) = _asr(vR(M), vR(S));
			break;
		case SOP_LSL:
			vR(SOP) = _lsl(vR(M), vR(S));
			break;
		case SOP_LSR:
			vR(SOP) = _lsr(vR(M), vR(S));
			break;
		case SOP_ROR:
			vR(SOP) = _ror(vR(M), vR(S));
			break;
	}
}

/* **** */

static int arm_inst_b_bl(cracker_p cj)
{
	const int blx = (CC_NV == ARM_IR_CC);
	const int hl = BEXT(IR, 24);
	const int32_t offset = mlBFMOVs(IR, 23, 0, 2);
	
	const int link = blx || (!blx && hl);
	const uint32_t new_pc = (ARM_PC + offset) + (blx ? ((hl << 1) | 1): 0);

	int splat = (new_pc == PC);
	CORE_TRACE("B%s%s(0x%08x) /* %c(%s0x%08x) */",
		link ? "L" : "", blx ? "X" : "", new_pc & 1,
		blx ? 'T' : 'A', splat ? "x" : "", offset);

	cracker_text(cj, new_pc);

	if(link || (CC_AL != ARM_IR_CC)) {
		cracker_text(cj, PC);
	}

	return(0);
}

static int arm_inst_bx(cracker_p cj)
{
	assert(0xf == mlBFEXT(IR, 19, 16));
	assert(0xf == mlBFEXT(IR, 15, 12));
	assert(0xf == mlBFEXT(IR, 11, 8));

	const int link = BEXT(IR, 5);

	setup_rR_vR_src(M, ARM_IR_RM);


	CORE_TRACE_START();

	_CORE_TRACE_("BX(%s)", rR_NAME(M));

	if(rPC == rR_SRC(M)) {
		_CORE_TRACE_("; /* %c(0x%08x) */", vR(M) & 1 ? 'T' : 'A', vR(M));

		cracker_text(cj, vR(M));
	}

	if(link || (CC_AL != ARM_IR_CC))
		cracker_text(cj, PC);


	CORE_TRACE_END();
		
	return(cracker_text_end_if(cj, IP, (CC_AL == ARM_IR_CC)));
}

static int arm_inst_cdp_mcr_mrc(cracker_p cj)
{
	int is_cdp = (0 == BEXT(IR, 4));
	int is_mrc = BEXT(IR, 20);

	if(is_cdp || is_mrc) {
		setup_rR_dst(D, ARM_IR_RD);
	} else
		setup_rR_src(D, ARM_IR_RD);

	CORE_TRACE_START();
	
	_CORE_TRACE_("%s(CP%u, %u, %s, CRn(%u), CRm(%u)",
		(is_cdp ? "CDP" : (is_mrc ? "MRC" : "MCR")),
		mlBFEXT(IR, 11, 8), mlBFEXT(IR, 23, 21), rR_NAME(D),
		mlBFEXT(IR, 19, 16), mlBFEXT(IR, 3, 0));
	
	uint op2 = mlBFEXT(IR, 7, 5);

	if(op2) {
		_CORE_TRACE_(", %u)", op2);
	}
	
	CORE_TRACE_END();
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static void arm_inst_dp(cracker_p cj)
{
	int wb = 0;

	CORE_TRACE_START();

	switch(ARM_DPI_OP) {
		case ARM_ADC:
		case ARM_ADD:
		case ARM_AND:
		case ARM_BIC:
		case ARM_EOR:
		case ARM_ORR:
		case ARM_RSB:
		case ARM_RSC:
		case ARM_SBC:
		case ARM_SUB:
			wb = 1;
			setup_rR_vR_src(N, ARM_IR_RN);
			setup_rR_vR_dst_src(D, ARM_IR_RD, rR(N));

			_CORE_TRACE_("%s%s(%s, %s, ",
				dpi_op_string[ARM_DPI_OP], ARM_DPI_S ? "S" : "",
				rR_NAME(D), rR_NAME(N));
			break;
		case ARM_CMP:
		case ARM_CMN:
		case ARM_TEQ:
		case ARM_TST:
			setup_rR_src(N, ARM_IR_RN);

			assert(0 == mlBFEXT(IR, 15, 12));

			_CORE_TRACE_("%s(%s, ", dpi_op_string[ARM_DPI_OP], rR_NAME(N));
			break;
		case ARM_MOV:
		case ARM_MVN:
			setup_rR_dst(D, ARM_IR_RD);

			assert(0 == mlBFEXT(IR, 19, 16));

			_CORE_TRACE_("%s%s(%s, ",
				dpi_op_string[ARM_DPI_OP], ARM_DPI_S ? "S" : "",
				rR_NAME(D));
			break;
		default:
			CORE_TRACE_END("0x%02x, %s", ARM_DPI_OP, dpi_op_string[ARM_DPI_OP]);
			LOG_ACTION(exit(-1));
			break;
	}

	if((CC_AL == ARM_IR_CC) && (rPC == rR_SRC(D)))
	{
		if(wb) {
			vR(D) = alubox(ARM_DPI_OP, vR(D), vR(SOP));

			vGPR_rR(D) = vR(D);
			if(rPC == rR(D))
				cracker_text(cj, vR(D));
		}
	}
}

static int arm_inst_dp_immediate(cracker_p cj)
{
	const uint shift = mlBFMOV(IR, 11, 8, 1);
	const uint imm8 = mlBFEXT(IR, 7, 0);

	setup_rR_vR(SOP, ~0, _ror(imm8, shift));

	arm_inst_dp(cj);

	if(shift) {
		_CORE_TRACE_("ROR(%u, %u)); /* 0x%08x */", imm8, shift, vR(SOP));
	} else {
		_CORE_TRACE_("%u)", vR(SOP));
		if(rPC == rR(D)) {
			_CORE_TRACE_("; /* 0x%08x */", vR(D));
		}
	}
	
	CORE_TRACE_END();

	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_dp_immediate_shift(cracker_p cj) {
	setup_rR_src(M, ARM_IR_RM);
	setup_rR_vR(S, ~0, mlBFEXT(IR, 11, 7));
	
	switch(ARM_DPI_SOP) {
		case SOP_ASR:
		case SOP_LSR:
			if(0 == vR(S))
				vR(S) = 32;
			break;
	}

	_shifter_operand(cj);

	arm_inst_dp(cj);
	
	if(vR(S)) {
		CORE_TRACE_END("%s(%s, 0x%02x))",
			shift_op_string[ARM_DPI_SOP],
			rR_NAME(M), vR(S));
	} else {
		if(SOP_ROR == ARM_DPI_SOP) {
			CORE_TRACE_END("RRX(%s))", rR_NAME(M));
		} else {
			CORE_TRACE_END("%s)", rR_NAME(M));
		}
	}
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_dp_register_shift(cracker_p cj)
{
	setup_rR_src(M, ARM_IR_RM);
	setup_rR_src(S, ARM_IR_RS);

	_shifter_operand(cj);

	arm_inst_dp(cj);

	CORE_TRACE_END("%s(%s, %s))",
		shift_op_string[ARM_DPI_SOP],
		rR_NAME(M), rR_NAME(S));
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldc_stc(cracker_p cj)
{
//	const uint32_t op0 = mlBFTST(IR, 27, 24) | BTST(IR, 4);
	setup_rR_src(N, mlBFEXT(IR, 19, 16));
	setup_rR_dst(D, mlBFEXT(IR, 15, 12));
	const uint8_t cp = mlBFEXT(IR, 11, 8);
	
	CORE_TRACE_START();
	
	_CORE_TRACE_("%sC%s(CP%u, CRd(%u)",
		BEXT(IR, 20) ? "LD" : "ST", BEXT(IR, 22) ? "L" : "",
		cp, rR(D));

	const uint8_t offset = mlBFMOV(IR, 7, 0, 2);

	_CORE_TRACE_(", CRn(%u)%s, %s0x%03x",
		rR(N), BEXT(IR, 21) ? ".WB " : "", BEXT(IR, 23) ? "" : "-", offset);

	CORE_TRACE_END(")");

	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldst(cracker_p cj)
{
	setup_rR_vR_src(N, ARM_IR_RN);

	if(ARM_IR_LDST_BIT(l20)) {
		setup_rR_dst_src(D, ARM_IR_RD, rR(N));
	} else {
		setup_rR_src(D, ARM_IR_RD);
	}

	CORE_TRACE_START();
	
	/* ldr|str{cond}{b}{t} <rd>, <addressing_mode> */
	/* ldr|str{cond}{h|sh|sb|d} <rd>, <addressing_mode> */

	switch(mlBFEXT(IR, 27, 25) & ~1) {
		case 0x00: { /* miscellaneous load/store */
			const int ldst_l = ARM_IR_LDST_BIT(l20) || (!ARM_IR_LDST_BIT(l20) && !ARM_IR_LDST_SH_BIT(h5));
			
			_CORE_TRACE_("%sR", ldst_l ? "LD" : "ST");
			_CORE_TRACE_("%s", ARM_IR_LDST_SH_FLAG_S ? "S" : "");
			_CORE_TRACE_("%s", ARM_IR_LDST_SH_FLAG_H ? "H" : "");
			_CORE_TRACE_("%s", ARM_IR_LDST_SH_FLAG_B ? "B" : "");
			_CORE_TRACE_("%s", ARM_IR_LDST_SH_FLAG_D ? "D" : "");
		}break;
		case 0x02: { /* default load store */
			const int ldst_t = !ARM_IR_LDST_BIT(p24) && ARM_IR_LDST_BIT(w21);

			_CORE_TRACE_("%sR", ARM_IR_LDST_BIT(l20) ? "LD" : "ST");
			_CORE_TRACE_("%s%s", ARM_IR_LDST_BIT(b22) ? "B" : "", ldst_t ? "T" : "");
		}break;
		default:
			CORE_TRACE("/* XXX */");
			cracker_text_end(cj, IP);
			return(-1);
	}

	_CORE_TRACE_("(%s, ", rR_NAME(D));

	const int wb = !ARM_IR_LDST_BIT(p24) || (ARM_IR_LDST_BIT(p24) && ARM_IR_LDST_BIT(w21));
	_CORE_TRACE_("%s%s", rR_NAME(N), wb ? " | rWB" : "");

	return(0);
}

static int arm_inst_ldst_immediate(cracker_p cj)
{
	if(arm_inst_ldst(cj))
		return(0);
	
	int32_t offset = mlBFEXT(IR, 11, 0);
	
	_CORE_TRACE_(", %s0x%04x", ARM_IR_LDST_BIT(u23) ? "+" : "-", offset);
	
	_CORE_TRACE_(")");

	if(!ARM_IR_LDST_BIT(u23))
		offset = ~offset;

	const uint32_t pat = vR(N) + offset;
	const size_t size = ARM_IR_LDST_BIT(b22) ? sizeof(uint8_t) : sizeof(uint32_t);

	int is_rPC = (rPC == rR(N));
	int is_rPC_SRC = (rPC == rR_SRC(N));

	if(is_rPC || is_rPC_SRC) {
		cracker_data(cj, pat, size);

		_CORE_TRACE_("; /* [0x%08x]", pat);

		if(is_rPC) {
			const uint32_t data = _read(cj, pat, size);

			if(ARM_IR_LDST_BIT(b22)) {
				_CORE_TRACE_(":0x%02x", data);
			} else {
				vGPR_rR(D) = data;
				_CORE_TRACE_(":0x%08x", data);
			}
		}

		_CORE_TRACE_(" */");
	}

	CORE_TRACE_END();
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldst_scaled_register_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj))
		return(0);

	setup_rR_src(M, ARM_IR_RM);
	setup_rR_vR(S, ~0, mlBFEXT(IR, 11, 7));

	if(0 == mlBFEXT(IR, 11, 4)) {
		_CORE_TRACE_(", %s%s", ARM_IR_LDST_BIT(u23) ? "+" : "-", rR_NAME(M));
	} else {
		uint shift = ARM_IR_6_5;

		switch(rR(S)) {
			case SOP_ASR:
			case SOP_LSR:
				if(0 == rR(S))
					rR(S) = 32;
				break;
		}
		
		_CORE_TRACE_("%s(%s, %u)", shift_op_string[shift],
			rR_NAME(M), vR(S));
	}

	CORE_TRACE_END(")");

	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldst_sh_immediate_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj))
		return(0);
	
	int16_t offset = mlBFMOV(IR, 11, 8, 4) | mlBFEXT(IR, 3, 0);

	_CORE_TRACE_(", %s0x%04x", ARM_IR_LDST_BIT(u23) ? "+" : "-", offset);

	if(!ARM_IR_LDST_BIT(u23))
		offset = ~offset;

	if(rPC == rR_SRC(N)) {
		const uint32_t pat = vR(N) + offset;
		const size_t size = 1 << ARM_IR_LDST_SH_FLAG_H;

		cracker_data(cj, pat, size);

		_CORE_TRACE_("); /* [0x%08x] */", pat);
	}

	CORE_TRACE_END();
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldst_sh_register_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj))
		return(0);
	
	if(0 != mlBFEXT(IR, 11, 8)) {
		CORE_TRACE_END("XXX");
		return(0);
	}

	setup_rR_src(M, ARM_IR_RM);
	
	CORE_TRACE_END(", %s%s", ARM_IR_LDST_BIT(u23) ? "" : "-", rR_NAME(M));
	
	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_ldstm(cracker_p cj)
{
	setup_rR_src(N, ARM_IR_RN);

	if(rSP == rR(N)) {
		if(0 == ARM_IR_LDST_BIT(l20))
			CORE_TRACE_START("PUSH(");
		else
			CORE_TRACE_START("POP(");
	} else {
		CORE_TRACE_START("%sM", ARM_IR_LDST_BIT(l20) ? "LD" : "ST");
		_CORE_TRACE_("%c%c", ARM_IR_LDST_BIT(u23) ? 'I' : 'D', ARM_IR_LDST_BIT(p24) ? 'B' : 'A');
		_CORE_TRACE_("(%s%s, ", rR_NAME(N), ARM_IR_LDST_BIT(w21) ? ".WB " : "");
	}

	assert(0 == ARM_IR_LDSTM_BIT(s22));

	char reglist[17], *dst = reglist;
	
	for(int i = 0; i <= 15; i++) {
		if(BEXT(IR, i)) {
			*dst++ = (i <= 9  ? '0' : 'a' - 10) + i;
			if(ARM_IR_LDST_BIT(l20))
				cracker_reg_dst(cj, i);
			else
				cracker_reg_src(cj, i);
		} else
			*dst++ = '.';
	}

	*dst = 0;

	CORE_TRACE_END("{%s}%s)", reglist, ARM_IR_LDSTM_BIT(s22) ? ".S" : "");
	
	const int ldpc = ARM_IR_LDST_BIT(l20) && BEXT(IR, PC);
	
	return(cracker_text_end_if(cj, IP, ldpc && (CC_AL == ARM_IR_CC)));
}

static int arm_inst_mla(cracker_p cj)
{
	setup_rR_src(M, ARM_IR_RM);
	setup_rR_src(N, ARM_IR_RN);
	setup_rR_src(S, ARM_IR_RS);

	setup_rR_dst(D, ARM_IR_RD);

	CORE_TRACE_START("MLA%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));
	_CORE_TRACE_(", %s", rR_NAME(N));
	CORE_TRACE_END("); /* %s = (%s * %s) + %s */",
		rR_NAME(D), rR_NAME(M), rR_NAME(S), rR_NAME(N));

	return(rPC != rR(D));
}

static int arm_inst_mrs(cracker_p cj)
{
	assert(0xf == mlBFEXT(IR, 19, 16));
	assert(0 == mlBFEXT(IR, 11, 0));

	setup_rR_dst(D, ARM_IR_RD);

	int bit_r = BEXT(IR, 22);

	CORE_TRACE("MRS(%s, %cPSR)",
		rR_NAME(D), bit_r ? 'S' : 'C');

	return(cracker_text_end_if(cj, IP, rPC == ARM_IR_RD));
}

static int arm_inst_msr(cracker_p cj)
{
	assert(0x0f == mlBFEXT(IR, 15, 12));

	CORE_TRACE_START();

	int bit_r = BEXT(IR, 22);

	const char psr_fields[5] = {
		BEXT(IR, 16) ? 'C' : 'c',
		BEXT(IR, 17) ? 'X' : 'x',
		BEXT(IR, 18) ? 'S' : 's',
		BEXT(IR, 19) ? 'F' : 'f',
		0,
	};

	if(BEXT(IR, 25)) {
		_CORE_TRACE_("MSR(%cPSR_%s, ",
			bit_r ? 'S' : 'C',
			psr_fields);

		uint imm8 = mlBFEXT(IR, 7, 0);
		uint shift = mlBFEXT(IR, 11, 8) << 1;
		
		if(shift) {
			_CORE_TRACE_("ROR(%02u, %03u)", shift, imm8);
		} else {
			_CORE_TRACE_("%03u", imm8);
		}
		
	} else {
		setup_rR_src(M, ARM_IR_RM);

		if(0 != mlBFEXT(IR, 11, 8)) {
			CORE_TRACE_END("/* XXX */");
		}

		_CORE_TRACE_("MSR(%cPSR_%s, %s",
			bit_r ? 'S' : 'C',
			psr_fields,
			rR_NAME(M));
	}

	CORE_TRACE_END(")");

	return(1);
}

static int arm_inst_umull(cracker_p cj)
{
	setup_rR_src(S, ARM_IR_RS);
	setup_rR_src(M, ARM_IR_RM);

	setup_rR_dst(D, ARM_IR_RD);
	setup_rR_dst(N, ARM_IR_RN);

	CORE_TRACE_START("UMULL%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(":%s", rR_NAME(N));
	_CORE_TRACE_(", %s", rR_NAME(M));
	CORE_TRACE_END(", %s", rR_NAME(S));

	return(cracker_text_end_if(cj, IP, (rPC == rR(D)) || (rPC == rR(N))));
}

/* **** */

#define _return(_x) _x

static int arm_step__dp_pre_validate(cracker_p cj)
{
	int fail = 0;

	switch(ARM_DPI_OP & 0x0c) {
		case 0x08:
			fail = (0 == ARM_DPI_S);
			fail |= (0 != ARM_IR_RD);
			break;
		case 0x0c:
			switch(ARM_DPI_OP) {
				case ARM_MOV:
				case ARM_MVN:
					fail = (0 != ARM_IR_RN);
					break;
			}
			break;
	}

	return(0 == fail);
}

static int arm_step__fail_decode(cracker_p cj)
{
	CORE_TRACE_START("IR[27, 25] = 0x%02x", ARM_IR_27_25);

	switch(ARM_IR_27_25) {
		case 0x00:
		case 0x01:
			_CORE_TRACE_(", DPI_OP = 0x%02x (%s), IR[7] = %0u, IR[4] = %0u",
				ARM_DPI_OP, dpi_op_string[ARM_DPI_OP], BEXT(IR, 7), BEXT(IR, 4));
			break;
		case 0x07:
			_CORE_TRACE_(", IR[27:24] = 0x%02x, IR[4] = %u", mlBFEXT(IR, 27, 24), BEXT(IR, 4));
			break;
		default:
			break;
	}

	CORE_TRACE_END("/* XXX */");

	cracker_disasm_arm(cj, IP, IR);
	cracker_text_end(cj, IP);
	LOG_ACTION(return(0));
}

static int arm_step_group0_ldst(cracker_p cj)
{
	assert(0 == ARM_IR_27_25);
	assert(1 == ARM_IR_4and7);
	
	if(0 != ARM_IR_6_5) {
		if(ARM_IR_LDST_SH_BIT(i22))
			return(arm_inst_ldst_sh_immediate_offset(cj));
		else
			return(arm_inst_ldst_sh_register_offset(cj));
	} else {
		switch(mlBFEXT(IR, 27, 21)) {
			case 0x01:
				return(arm_inst_mla(cj));
			case 0x04:
				return(arm_inst_umull(cj));
		}
	}

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}

static int arm_step_group0_misc(cracker_p cj)
{
	if(ARM_INST_BX == (IR & ARM_INST_BX_MASK))
		return(arm_inst_bx(cj));
	if(ARM_INST_MRS == (IR & ARM_INST_MRS_MASK))
		return(arm_inst_mrs(cj));
	if((ARM_INST_MSR_I == (IR & ARM_INST_MSR_I_MASK))
		|| (ARM_INST_MSR_R == (IR & ARM_INST_MSR_R_MASK)))
			return(arm_inst_msr(cj));

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}

static int arm_step_group0(cracker_p cj)
{
	if(BTST(IR, 4)) {
		if(BTST(IR, 7))
			return(arm_step_group0_ldst(cj));
		else {
			if((2 == mlBFEXT(IR, 24, 23)) && !BTST(IR, 20))
				return(arm_step_group0_misc(cj));
			else {
				if(arm_step__dp_pre_validate(cj))
					_return(arm_inst_dp_register_shift(cj));
			}
		}
	} else {
		if((2 == mlBFEXT(IR, 24, 23)) && !BTST(IR, 20))
			return(arm_step_group0_misc(cj));
		else {
			if(arm_step__dp_pre_validate(cj))
				return(arm_inst_dp_immediate_shift(cj));
		}
	}

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}

static int arm_step_group1(cracker_p cj)
{
	if(2 == mlBFEXT(IR, 24, 23)) {
		if(0 == mlBFEXT(IR, 21, 20)) {
//			cracker_inst_undefined(cj);
			return(arm_step__fail_decode(cj));
		} else if(2 == mlBFEXT(IR, 21, 20))
//			cracker_inst_unimplimented(cj); /* move immediate to status register */
			return(arm_step__fail_decode(cj));
	}
	
	if(arm_step__dp_pre_validate(cj))
		return(arm_inst_dp_immediate(cj));

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}

static int arm_step_group7(cracker_p cj)
{
	switch(mlBFEXT(IR, 27, 24)) {
		case 0x0e:
			if(ARM_INST_MCR_MRC == (IR & ARM_INST_MCR_MRC_MASK))
				return(arm_inst_cdp_mcr_mrc(cj));
//		case 0x0f: /* swi */
	}

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}

int arm_step(cracker_p cj)
{
	IR = _fetch(cj);

	if(CC_NV != ARM_IR_CC) {
		switch(ARM_IR_27_25) {
			case 0x00: /* xxxx 000x xxxx xxxx */
				return(arm_step_group0(cj));
			case 0x01: /* xxxx 001x xxxx xxxx */
				return(arm_step_group1(cj));
			case 0x02: /* xxxx 010x xxxx xxxx */
				return(arm_inst_ldst_immediate(cj));
			case 0x03:
				if(0 == BTST(IR, 4))
					return(arm_inst_ldst_scaled_register_offset(cj));
				break;
			case 0x04: /* xxxx 100x xxxx xxxx */
				return(arm_inst_ldstm(cj));
			case 0x05: /* xxxx 101x xxxx xxxx */
				return(arm_inst_b_bl(cj));
			case 0x06: /* xxxx 110x xxxx xxxx */
				_return(arm_inst_ldc_stc(cj));
				break;
			case 0x07: /* xxxx 111x xxxx xxxx */
				return(arm_step_group7(cj));
		}
	} else if(CC_NV == ARM_IR_CC) { /* CC_NV */
		switch(ARM_IR_27_25) {
			case 0x05:
				return(arm_inst_b_bl(cj));
				break;
		}
	}

	LOG_ACTION(return(arm_step__fail_decode(cj)));
}
