#include "cracker_disasm.h"

/* **** */

#include "bitfield.h"
#include "capstone_assert_test.h"
#include "log.h"
#include "unused.h"

/* **** */

#include <capstone/capstone.h>

/* **** */

UNUSED_FN static void __cracker_disasm(cracker_p cj, uint32_t address, uint32_t opcode, int thumb)
{
	csh handle = 0;
	cs_insn *insn;

	const size_t size = thumb ? sizeof(uint16_t) : sizeof(uint32_t);
	const int mode = thumb ? CS_MODE_THUMB : CS_MODE_ARM;

	cs_assert_success(cs_open(CS_ARCH_ARM, mode, &handle));

	const uint8_t* insn_data = (uint8_t*)&opcode;
	uint64_t insn_addr = address;

	size_t count = cs_disasm(handle, insn_data, size, insn_addr, 0, &insn);

	if (count > 0) {
		for(uint j = 0; j < count; j++) {
			const cs_insn* insn_j = &insn[j];

			const uint64_t insn_address = insn_j->address;
			printf("0x%08" PRIx64 ":\t", insn_address);
			for(unsigned int k = 0; k < size; k++)
				printf(" 0x%02x", insn_data[(j << 2) + k]);
			printf("\t\t%s\t\t%s\n", insn_j->mnemonic,
					insn_j->op_str);
		}
	} else
		printf("0x%02zx:[0x%08x] == 0x%08x: Failed to disassemble given code!\n",
			size, address, opcode);

	cs_close(&handle);

	UNUSED(cj);
}

UNUSED_FN static void __cracker_disasm_iter(cracker_p cj, uint32_t address, uint32_t opcode, int thumb)
{
	csh handle = 0;
	cs_insn insn;

	size_t size = thumb ? sizeof(uint16_t) : sizeof(uint32_t);
	const int mode = thumb ? CS_MODE_THUMB : CS_MODE_ARM;

	cs_assert_success(cs_open(CS_ARCH_ARM, mode, &handle));

	const uint8_t* insn_data = (uint8_t*)&opcode;
	uint64_t insn_addr = address;

	size_t count = cs_disasm_iter(handle, &insn_data, &size, &insn_addr, &insn);

	if (count > 0) {
		const uint64_t insn_address = insn.address;
		printf("0x%08" PRIx64 ":\t", insn_address);
		for(unsigned int k = 0; k < size; k++)
			printf(" 0x%02x", insn_data[k]);
		printf("\t\t%s\t\t%s\n", insn.mnemonic,
				insn.op_str);
	} else
		printf("0x%08x:(0x%02zx): Failed to disassemble given code!\n", address, size);

	cs_close(&handle);

	UNUSED(cj);
}

#define _cracker_disasm __cracker_disasm

void cracker_disasm(cracker_p cj, uint32_t address, uint32_t opcode)
{
	if(address & 1)
		cracker_disasm_thumb(cj, address, opcode);
	else
		cracker_disasm_arm(cj, address, opcode);
}

void cracker_disasm_arm(cracker_p cj, uint32_t address, uint32_t opcode)
{
	_cracker_disasm(cj, address & ~3, opcode, 0);
}

void cracker_disasm_thumb(cracker_p cj, uint32_t address, uint32_t opcode)
{
	_cracker_disasm(cj, address & ~1, opcode, 1);
}
