#define ARM_IP_NEXT ((IP + 4) & ~3U)
#define ARM_PC ((IP + 8) & ~3U)

/* **** */

#include "cracker_arm.h"
#include "cracker_arm_enum.h"
#include "cracker_arm_ir.h"
#include "cracker_data.h"
#include "cracker_disasm.h"
#include "cracker_enum.h"
#include "cracker_regs.h"
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

/* **** */

static int _fetch(cracker_p cj, uint32_t* p2ir)
{
	IP = PC;
	PC += sizeof(uint32_t);

	return(cracker_read_if(cj, IP & ~3U, sizeof(uint32_t), p2ir));
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
			if(!ARM_DPI_I && !BEXT(IR, 4) && (0 == vR(S)))
				vR(SOP) = _rrx_v(vR(M), 0);
			else
				vR(SOP) = _ror(vR(M), vR(S));
			break;
	}
}

/* **** */

static int arm_inst_b_bl(cracker_p cj)
{
	const int blx = (CC_NV == ARM_IR_CC);
	const int hl = BEXT(IR, 24);
	const int32_t offset = mlBFMOVs(IR, 23, 0, 2)
		+ (blx ? ((hl << 1) | 1) : 0);

	const int link = blx || (!blx && hl);
	const uint32_t new_pc = (ARM_PC + offset);

	// TODO: BLX CC_NV -->> CC_AL

	int splat = (new_pc == ARM_IP_NEXT);
	CORE_TRACE("B%s%s(0x%08x) /* %c(%s0x%08x) */",
		link ? "L" : "", blx ? "X" : "", new_pc & ~(3 >> blx),
		blx ? 'T' : 'A', splat ? "x" : "", offset);

	if(splat)
		cracker_relocation(cj, IP);

	if(link) {
		unsigned bcc = blx ? CC_AL : ARM_IR_CC;

		return(cracker_text_branch_and_link(cj, bcc, new_pc, ARM_IP_NEXT));
	}

	return(cracker_text_branch_cc(cj, ARM_IR_CC, new_pc, ARM_IP_NEXT));
}

static int arm_inst_bx(cracker_p cj)
{
	if(!(0xf == mlBFEXT(IR, 19, 16))
		|| !(0xf == mlBFEXT(IR, 15, 12))
		|| !(0xf == mlBFEXT(IR, 11, 8)))
			return(0);

	const int link = BEXT(IR, 5);

	setup_rR_vR_src(M, ARM_IR_RM);

	CORE_TRACE_START();

	_CORE_TRACE_("BX(%s)", rR_NAME(M));

	if(rR_IS_PC_REF(M)) {
		const int thumb = vR(M) & 1;
		_CORE_TRACE_("; /* %c(0x%08x) */", thumb ? 'T' : 'A', vR(M));
	}

	CORE_TRACE_END();

	if(link)
		cracker_text_branch_link(cj, ARM_IR_CC, ARM_IP_NEXT);

	if(rR_IS_PC_REF(M))
		return(cracker_text_branch(cj, ARM_IR_CC, vR(M)));

	return(0);
}

#define mcrc(_cp, _op1, _cn, _cm, _op2) \
	(((_cp) & 15) << 8) | (((_op1) & 7) << 21) \
	| (((_cn) & 15) << 16) | ((_cm) & 15) | (((_op2) & 7) << 5)

static int arm_inst_cdp_mcr_mrc(cracker_p cj)
{
	const unsigned is_cdp = (0 == BEXT(IR, 4));
	const unsigned is_mrc = BEXT(IR, 20);

	if(is_cdp || is_mrc) {
		setup_rR_dst(D, ARM_IR_RD);
		cracker_reg_dst_wb(cj, rrRD);
	} else
		setup_rR_src(D, ARM_IR_RD);

	CORE_TRACE_START();

	const unsigned cp = mlBFEXT(IR, 11, 8);
	const unsigned op[2] = { mlBFEXT(IR, 23, 21), mlBFEXT(IR, 7, 5) };
	setup_rR_vR(N, mlBFEXT(IR, 19, 16), 0);
	setup_rR_vR(M, mlBFEXT(IR, 3, 0), 0);

	_CORE_TRACE_("%s(CP%u, %u, %s, CRn(%u), CRm(%u)",
		(is_cdp ? "CDP" : (is_mrc ? "MRC" : "MCR")),
		cp, op[0], rR_NAME(D), rR(N), rR(M));

	if(op[1]) {
		_CORE_TRACE_(", %u", op[1]);
	}

	_CORE_TRACE_(")");

	const char* ss = "";

	switch(mcrc(cp, op[0], rR(N), rR(M), op[1])) {
		case mcrc(15, 0, 1, 0, 0):
			ss = "Control Register";
			break;
		case mcrc(15, 0, 2, 0, 0):
			ss = "Translation Table Base Register 0";
			break;
		case mcrc(15, 0, 5, 0, 0):
			ss = "Data Fault Status Register";
			break;
		case mcrc(15, 0, 5, 0, 1):
			ss = "Instruction Fault Status Register";
			break;
		case mcrc(15, 0, 7, 0, 4):
			ss = "Wait For Interrupt";
			break;
		case mcrc(15, 0, 7, 7, 0):
			ss = "Invalidate ICache and DCache";
			break;
		case mcrc(15, 0, 7, 10, 4):
			ss = "Drain Write Buffer";
			break;
		case mcrc(15, 0, 8, 7, 0):
			ss = "Invalidate TLB";
			break;
		default:
			LOG_ACTION(exit(-1));
			break;
	}

	CORE_TRACE_END("; /* %s */", ss);

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static void arm_inst_dp(cracker_p cj)
{
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
			setup_rR_vR_src(N, ARM_IR_RN);
			setup_rR_dst_rR_src(D, ARM_IR_RD, N);

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

	if(CC_AL == ARM_IR_CC) {
		if(alubox(&vR(D), ARM_DPI_OP, vR(N), vR(SOP)))
			cracker_reg_dst_wb(cj, rrRD);

		if(0) if(rR_IS_PC(N)) {
			_CORE_TRACE_("/* 0x%08x ??? 0x%08x --- 0x%08x ???ARM_ALU??? */",
				vR(N), vR(SOP), vR(D));
		}
	}
}

static void arm_inst_dp__final(cracker_p cj)
{
	const char* dpi_ss[16] = {
		"& ", "^ ", "- ", "- ", "+ ", "+ ", "- ", "- ",
		"& ", "^ ", "- ", "+ ", "| ", "XX", "& ~", "XX",
	};

	switch(ARM_DPI_OP) {
		case ARM_MOV:
		case ARM_MVN:
			_CORE_TRACE_("; /* 0x%08x */", vR(D));
			break;
		default:
			_CORE_TRACE_("; /* 0x%08x %s0x%08x == 0x%08x */",
				vR(N), dpi_ss[ARM_DPI_OP], vR(SOP), vR(D));
			break;
	}

	CORE_TRACE_END();
}

static int arm_inst_dp_immediate(cracker_p cj)
{
	setup_rR_vR(S, ~0, mlBFMOV(IR, 11, 8, 1));
	setup_rR_vR(M, ~0, mlBFEXT(IR, 7, 0));

	setup_rR_vR(SOP, ~0, _ror(vR(M), vR(S)));

	arm_inst_dp(cj);

	if(vR(S)) {
		_CORE_TRACE_("ROR(%u, %u))", vR(M), vR(S));
//		_CORE_TRACE_("ROR(%u, %u)); /* 0x%08x */", vR(M), vR(S), vR(SOP));
	} else {
		_CORE_TRACE_("%u)", vR(SOP));
//		if(rPC == rR(D)) {
//			_CORE_TRACE_("; /* 0x%08x */", vR(D));
//		}
	}

	arm_inst_dp__final(cj);

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
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
		_CORE_TRACE_("%s(%s, 0x%02x))",
			shift_op_string[ARM_DPI_SOP],
			rR_NAME(M), vR(S));
	} else {
		if(SOP_ROR == ARM_DPI_SOP) {
			_CORE_TRACE_("RRX(%s))", rR_NAME(M));
		} else {
			_CORE_TRACE_("%s)", rR_NAME(M));
		}
	}

	arm_inst_dp__final(cj);

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_dp_register_shift(cracker_p cj)
{
	setup_rR_src(M, ARM_IR_RM);
	setup_rR_src(S, ARM_IR_RS);

	_shifter_operand(cj);

	arm_inst_dp(cj);

	CORE_TRACE("%s(%s, %s))",
		shift_op_string[ARM_DPI_SOP],
		rR_NAME(M), rR_NAME(S));

	arm_inst_dp__final(cj);

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_ldc_stc(cracker_p cj)
{
//	const uint32_t op0 = mlBFTST(IR, 27, 24) | BTST(IR, 4);
	setup_rR_src(N, mlBFEXT(IR, 19, 16));
	setup_rR_dst(D, mlBFEXT(IR, 15, 12));
	if(BEXT(IR, 20))
		cracker_reg_dst_wb(cj, rrRD);
	
	const uint8_t cp = mlBFEXT(IR, 11, 8);

	CORE_TRACE_START();

	_CORE_TRACE_("%sC%s(CP%u, CRd(%u)",
		BEXT(IR, 20) ? "LD" : "ST", BEXT(IR, 22) ? "L" : "",
		cp, rR(D));

	const uint8_t offset = mlBFMOV(IR, 7, 0, 2);

	_CORE_TRACE_(", CRn(%u)%s, %s0x%03x",
		rR(N), BEXT(IR, 21) ? ".WB " : "", BEXT(IR, 23) ? "" : "-", offset);

	CORE_TRACE_END(")");

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_ldst(cracker_p cj, unsigned wb_dst)
{
	setup_rR_vR_src(N, ARM_IR_RN);

	if(0) LOG("0x%02x -- 0x%08x -- IS_PC:%01u, IS_PC_REF:%01u",
		rR(N), vR(N), rR_IS_PC(N), rR_IS_PC_REF(N));

	if(ARM_IR_LDST_BIT(l20)) {
		if(wb_dst) {
			setup_rR_dst(D, ARM_IR_RD);
			cracker_reg_dst_wb(cj, rrRD);
		} else
			setup_rR_dst_rR_src(D, ARM_IR_RD, N);

		if(0) LOG("0x%02x -- 0x%08x -- IS_PC:%01u, IS_PC_REF:%01u",
			rR(D), vR(D), rR_IS_PC(D), rR_IS_PC_REF(D));
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
	if(arm_inst_ldst(cj, 0))
		return(0);

	int32_t offset = mlBFEXT(IR, 11, 0);

	_CORE_TRACE_(", %s0x%04x", ARM_IR_LDST_BIT(u23) ? "+" : "-", offset);

	_CORE_TRACE_(")");

	if(!ARM_IR_LDST_BIT(u23))
		offset = ~offset;

	const uint32_t pat = vR(N) + offset;
	const size_t size = ARM_IR_LDST_BIT(b22) ? sizeof(uint8_t) : sizeof(uint32_t);

	if(rR_IS_PC(N) || rR_IS_PC_REF(N)) {
		_CORE_TRACE_("; /* [0x%08x]", pat);

		if(cracker_data_read_if(cj, pat, size, &vR(D))) {
			if(ARM_IR_LDST_BIT(b22)) {
				_CORE_TRACE_(":0x%02x", vR(D));
			} else {
				_CORE_TRACE_(":0x%08x", vR(D));
			}
		}

		_CORE_TRACE_(" */");
	}

	if(ARM_IR_LDST_BIT(l20))
		cracker_reg_dst_wb(cj, rrRD);

	CORE_TRACE_END();

	const int ldpc = ARM_IR_LDST_BIT(l20) && rR_IS_PC(D) && (CC_AL == ARM_IR_CC);
	return(cracker_text_end_if(cj, ARM_IP_NEXT, ldpc));
}

static int arm_inst_ldst_scaled_register_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj, 1))
		return(0);

	setup_rR_src(M, ARM_IR_RM);
	setup_rR_vR(S, ~0, mlBFEXT(IR, 11, 7));

	if(0 == mlBFEXT(IR, 11, 4)) {
		_CORE_TRACE_(", %s%s", ARM_IR_LDST_BIT(u23) ? "+" : "-", rR_NAME(M));
	} else {
		unsigned shift = ARM_IR_6_5;

		switch(rR(S)) {
			case SOP_ASR:
			case SOP_LSR:
				if(0 == rR(S))
					rR(S) = 32;
				break;
		}

		_CORE_TRACE_(", %s(%s, %u)", shift_op_string[shift],
			rR_NAME(M), vR(S));
	}

	CORE_TRACE_END(")");

	const int ldpc = ARM_IR_LDST_BIT(l20) && rR_IS_PC(D) && (CC_AL == ARM_IR_CC);
	return(cracker_text_end_if(cj, ARM_IP_NEXT, ldpc));
}

static int arm_inst_ldst_sh_immediate_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj, 1))
		return(0);

	int16_t offset = mlBFMOV(IR, 11, 8, 4) | mlBFEXT(IR, 3, 0);

	_CORE_TRACE_(", %s0x%04x", ARM_IR_LDST_BIT(u23) ? "+" : "-", offset);

	if(!ARM_IR_LDST_BIT(u23))
		offset = ~offset;

	if(rR_IS_PC_REF(N)) {
		const uint32_t pat = vR(N) + offset;
		const size_t size = ARM_IR_LDST_SH_FLAG_D ? 8 : (ARM_IR_LDST_SH_FLAG_H ? 2 : 1);

		cracker_data(cj, pat, size, 0);

		_CORE_TRACE_("); /* [0x%08x] */", pat);
	}

	CORE_TRACE_END();

	const int ldpc = ARM_IR_LDST_BIT(l20) && rR_IS_PC(D) && (CC_AL == ARM_IR_CC);
	return(cracker_text_end_if(cj, ARM_IP_NEXT, ldpc));
}

static int arm_inst_ldst_sh_register_offset(cracker_p cj)
{
	if(arm_inst_ldst(cj, 1))
		return(0);

	if(0 != mlBFEXT(IR, 11, 8)) {
		CORE_TRACE_END("XXX");
		return(0);
	}

	setup_rR_src(M, ARM_IR_RM);

	CORE_TRACE_END(", %s%s", ARM_IR_LDST_BIT(u23) ? "" : "-", rR_NAME(M));

	const int ldpc = ARM_IR_LDST_BIT(l20) && rR_IS_PC(D) && (CC_AL == ARM_IR_CC);
	return(cracker_text_end_if(cj, ARM_IP_NEXT, ldpc));
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

	if(!(0 == ARM_IR_LDSTM_BIT(s22))) {
		CORE_TRACE_END("XXX");
		return(0);
	}

	char reglist[17], *dst = reglist;

	for(int i = 0; i <= 15; i++) {
		if(BEXT(IR, i)) {
			*dst++ = (i <= 9  ? '0' : 'a' - 10) + i;
			if(ARM_IR_LDST_BIT(l20))
				cracker_reg_dst(cj, rrRD, i);
			else
				cracker_reg_src(cj, rrRD, i, 0);
		} else
			*dst++ = '.';
	}

	*dst = 0;

	CORE_TRACE_END("{%s}%s)", reglist, ARM_IR_LDSTM_BIT(s22) ? ".S" : "");

	const int ldpc = ARM_IR_LDST_BIT(l20) && BEXT(IR, rPC);
	return(cracker_text_end_if(cj, ARM_IP_NEXT, ldpc && (CC_AL == ARM_IR_CC)));
}

static int arm_inst_mla(cracker_p cj)
{
	setup_rR_vR_src(M, ARM_IR_RM);
	setup_rR_vR_src(N, ARM_IR_RN);
	setup_rR_vR_src(S, ARM_IR_RS);

	setup_rR_dst(D, ARM_IR_RD);

	vR(D) = (vR(M) * vR(S)) + vR(N);
	cracker_reg_dst_wb(cj, rrRD);

	CORE_TRACE_START("MLA%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));
	_CORE_TRACE_(", %s", rR_NAME(N));

	_CORE_TRACE_("); /* (0x%08x * 0x%08x) + 0x%08x = 0x%08x */",
		vR(M), vR(S), vR(N), vR(D));

	CORE_TRACE_END("XXX )");

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_mrs(cracker_p cj)
{
	if(!(0xf == mlBFEXT(IR, 19, 16))
		|| !(0 == mlBFEXT(IR, 11, 0)))
			return(0);

	setup_rR_dst(D, ARM_IR_RD);
	cracker_reg_dst_wb(cj, rrRD);

	int bit_r = BEXT(IR, 22);

	CORE_TRACE("MRS(%s, %cPSR)",
		rR_NAME(D), bit_r ? 'S' : 'C');

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_msr(cracker_p cj)
{
	if(!(0x0f == mlBFEXT(IR, 15, 12)))
		return(0);

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

		setup_rR_vR(M, ~0, mlBFEXT(IR, 7, 0));
		setup_rR_vR(S, ~0, mlBFMOV(IR, 11, 8, 1));

		_shifter_operand(cj);

		if(vR(S)) {
			_CORE_TRACE_("ROR(%03u, %02u)", vR(M), vR(S));
		} else {
			_CORE_TRACE_("RRX(%03u)", vR(M));
		}

		_CORE_TRACE_("; /* 0x%08x */", vR(SOP));
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

static int arm_inst_mul(cracker_p cj)
{
	if(0) LOG("0x%08x, RN = 0x%08x, RD = 0x%08x, RS = 0x%08x, RM = 0x%08x",
		0x0ff00090 & IR, ARM_IR_RN, ARM_IR_RD, ARM_IR_RS, ARM_IR_RM);

//	assert(0 == ARM_IR_RD);

	setup_rR_vR_src(M, ARM_IR_RM);
	setup_rR_vR_src(S, ARM_IR_RS);

	setup_rR_dst(D, ARM_IR_RN); /* D -- !! RN !! */

	vR(D) = vR(M) * vR(S);
	cracker_reg_dst_wb(cj, rrRD);

	CORE_TRACE_START("MUL%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));

	_CORE_TRACE_("); /* 0x%08x * 0x%08x = 0x%08x */",
		vR(M), vR(S), vR(D));

	CORE_TRACE_END(" XXX )");

	return(cracker_text_end_if(cj, ARM_IP_NEXT, rR_IS_PC(D)));
}

static int arm_inst_smul_xy(cracker_p cj)
{
//	LOG("0x%08x, 0x%08x", 0x0ff00090 & IR, ARM_IR_RD);
	assert(0 == ARM_IR_RD);
	
	// 0x01600080 --- cccc 0001 | 0110 dddd | zzzz ssss | 1yx0 mmmm

	const unsigned bit_y = BMOV(IR, 6, 4);
	const unsigned bit_x = BMOV(IR, 5, 4);

	setup_rR_src(S, ARM_IR_RS);
	setup_rR_src(M, ARM_IR_RM);
	
	setup_rR_dst(D, ARM_IR_RN); /* D -- !! RN !! */

	int16_t vx = (vR(M) >> bit_x) & 0xffff;
	int16_t vy = (vR(S) >> bit_y) & 0xffff;

	vR(D) = (signed)(vx * vy);
	cracker_reg_dst_wb(cj, rrRD);
	
	CORE_TRACE_START("SMUL%c%c(", bit_x ? 'T' : 'B', bit_y ? 'T' : 'B');
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));
	_CORE_TRACE_(")");

	_CORE_TRACE_("; /* 0x%08x * 0x%08x = 0x%08x */", vx, vy, vR(D));

	CORE_TRACE_END();

	return(cracker_text_end_if(cj, ARM_IP_NEXT, (rR_IS_PC(D)) || (rR_IS_PC(N))));
}

static int arm_inst_smull(cracker_p cj)
{
	setup_rR_vR_src(S, ARM_IR_RS);
	setup_rR_vR_src(M, ARM_IR_RM);

	setup_rR_dst(D, ARM_IR_RD);
	setup_rR_dst(N, ARM_IR_RN);

	int64_t result = (int32_t)vR(M) * (int32_t)vR(S);
	
	vR(D) = result & 0xffffffff;
	cracker_reg_dst_wb(cj, rrRD);

	vR(N) = (result >> 32) & 0xffffffff;
	cracker_reg_dst_wb(cj, rrRN);

	CORE_TRACE_START("SMULL%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(":%s", rR_NAME(N));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));
	
	_CORE_TRACE_("); /* 0x%08x * 0x%08x = 0x%016llx */",
		vR(M), vR(S), result);

	CORE_TRACE_END(" XXX )");

	return(cracker_text_end_if(cj, ARM_IP_NEXT, (rR_IS_PC(D)) || (rR_IS_PC(N))));
}

static int arm_inst_umull(cracker_p cj)
{
	setup_rR_vR_src(S, ARM_IR_RS);
	setup_rR_vR_src(M, ARM_IR_RM);

	setup_rR_dst(D, ARM_IR_RD);
	setup_rR_dst(N, ARM_IR_RN);

	uint64_t result = (uint32_t)vR(M) * (uint32_t)vR(S);
	
	vR(D) = result & 0xffffffff;
	cracker_reg_dst_wb(cj, rrRD);

	vR(N) = (result >> 32) & 0xffffffff;
	cracker_reg_dst_wb(cj, rrRN);

	CORE_TRACE_START("UMULL%s(", ARM_DPI_S ? "S" : "");
	_CORE_TRACE_("%s", rR_NAME(D));
	_CORE_TRACE_(":%s", rR_NAME(N));
	_CORE_TRACE_(", %s", rR_NAME(M));
	_CORE_TRACE_(", %s", rR_NAME(S));
	
	_CORE_TRACE_("); /* 0x%08x * 0x%08x = 0x%016llx */",
		vR(M), vR(S), result);

	CORE_TRACE_END(" XXX )");

	return(cracker_text_end_if(cj, ARM_IP_NEXT, (rR_IS_PC(D)) || (rR_IS_PC(N))));
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
			if(ARM_IR_4and7) {
				_CORE_TRACE_(", 0x0fe00090 & IR = 0x%08x", IR & 0x0fe00090);
			} else {
				_CORE_TRACE_(", DPI_OP = 0x%02x (%s), IR[7] = %0u, IR[4] = %0u",
					ARM_DPI_OP, dpi_op_string[ARM_DPI_OP], BEXT(IR, 7), BEXT(IR, 4));
			}break;
		case 0x07:
			_CORE_TRACE_(", IR[27:24] = 0x%02x, IR[4] = %u", mlBFEXT(IR, 27, 24), BEXT(IR, 4));
			break;
		default:
			break;
	}

	CORE_TRACE_END(" /* XXX */");

	cracker_disasm_arm(cj, IP, IR);
	cracker_text_end(cj, IP);
	LOG_ACTION(return(0));
//	LOG_ACTION(exit(-1));
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
		switch(mlBFTST(IR, 27, 21)) {
			case 0x00000000:
				return(arm_inst_mul(cj));
			case 0x00200000:
				return(arm_inst_mla(cj));
			case 0x00800000:
				return(arm_inst_umull(cj));
			case 0x00c00000:
				return(arm_inst_smull(cj));
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
	else {
		switch(mlBFTST(IR, 27, 20) | BTST(IR, 7) | BTST(IR, 4)) {
			case 0x01600080:
				return(arm_inst_smul_xy(cj));
		}
	}

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
					return(arm_inst_dp_register_shift(cj));
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

	LOG("IR[24, 23] = %u, IR[20] = %u, IR[7] = %u, IR[4] = %u",
		mlBFEXT(IR, 24, 23), BEXT(IR, 20), BEXT(IR, 7), BEXT(IR, 4));

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
	if(!_fetch(cj, &IR))
		return(0);

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
