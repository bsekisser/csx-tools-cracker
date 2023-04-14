#pragma once

/* **** */

#include "cracker.h"

/* **** */

void cracker_disasm(cracker_p cj, uint32_t address, uint32_t opcode);
void cracker_disasm_arm(cracker_p cj, uint32_t address, uint32_t opcode);
void cracker_disasm_thumb(cracker_p cj, uint32_t address, uint32_t opcode);
