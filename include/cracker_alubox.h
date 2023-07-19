#pragma once

/* **** */

#include "cracker_arm_enum.h"

/* **** */

#include <stdint.h>

/* **** */

static inline unsigned alubox(uint32_t* rd, uint8_t op, uint32_t s1, uint32_t s2)
{
	unsigned wb = (2 != (op >> 2));

	uint32_t result = 0;

	switch(op) {
	case ARM_ADC:
	case ARM_ADD:
	case ARM_CMN:
		result = s1 + s2;
		break;
	case ARM_AND:
	case ARM_TST:
		result = s1 & s2;
		break;
	case ARM_ASR:
		result = _asr(s1, s2 & 0xff);
		break;
	case ARM_BIC:
		result = s1 & ~s2;
		break;
	case ARM_CMP:
	case ARM_SBC:
	case ARM_SUB:
		result = s1 - s2;
		break;
	case ARM_EOR:
	case ARM_TEQ:
		result = s1 ^ s2;
		break;
	case ARM_LSL:
		result = _lsl(s1, s2 & 0xff);
		break;
	case ARM_LSR:
		result = _lsr(s1, s2 & 0xff);
		break;
	case ARM_MOV:
		result = s2;
		break;
	case ARM_MUL:
		result = s1 * s2;
		break;
	case ARM_MVN:
		result = ~s2;
		break;
	case ARM_ORR:
		result = s1 | s2;
		break;
	case ARM_RSB:
	case ARM_RSC:
		result = s2 - s1;
		break;
	case ARM_ROR:
		result = _ror(s1, s2);
		break;
	default:
		LOG("op = 0x%02x", op);
		LOG_ACTION(exit(-1));
	}

	if(rd)
		*rd = result;

	return(wb);
}
