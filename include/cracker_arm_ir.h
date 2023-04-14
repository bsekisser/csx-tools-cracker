#pragma once

#define ARM_DPI_OP mlBFEXT(IR, 24, 21)
#define ARM_DPI_I BEXT(IR, 25)
#define ARM_DPI_S BEXT(IR, 20)
#define ARM_DPI_SOP mlBFEXT(IR, 6, 5)

#define ARM_IR_CC mlBFEXT(IR, 31, 28)
#define ARM_IR_RD mlBFEXT(IR, 15, 12)
#define ARM_IR_RM mlBFEXT(IR, 3, 0)
#define ARM_IR_RN mlBFEXT(IR, 19, 16)
#define ARM_IR_RS mlBFEXT(IR, 11, 8)

#define ARM_IR_4and7 (BEXT(IR, 4) && BEXT(IR, 7))
#define ARM_IR_6_5 mlBFEXT(IR, 6, 5)
#define ARM_IR_27_25 mlBFEXT(IR, 27, 25)

enum {
	ARM_IR_LDST_BIT_l20 = 20,
};

#define ARM_IR_LDST_BIT(_x) BEXT(IR, ARM_IR_LDST_BIT_ ## _x)

#define ARM_LDST_B BEXT(IR, 22)
#define ARM_LDST_H BEXT(IR, 5)
#define ARM_LDST_I22 BEXT(IR, 22)
#define ARM_LDST_L BEXT(IR, 20)
#define ARM_LDST_P BEXT(IR, 24)
#define ARM_LDST_U BEXT(IR, 23)
#define ARM_LDST_S BEXT(IR, 6)
#define ARM_LDST_W BEXT(IR, 21)
#define ARM_LDSTM_S BEXT(IR, 22)

#define ARM_LDST_FLAG_B (ARM_LDST_FLAG_S && !ARM_LDST_H)
#define ARM_LDST_FLAG_H (ARM_LDST_H & !ARM_LDST_FLAG_D)
#define ARM_LDST_FLAG_D (!ARM_LDST_L && ARM_LDST_S)
#define ARM_LDST_FLAG_S (ARM_LDST_L && ARM_LDST_S)

/* **** */

enum {
	ARM_IR_LDSTM_BIT_s22 = 22,
};

#define ARM_IR_LDSTM_BIT(_x) BEXT(IR, ARM_IR_LDSTM_BIT_ ## _x)

/* **** */

enum {
	ARM_IR_LDST_SH_BIT_i22 = 22,
};

#define ARM_IR_LDST_SH_BIT(_x) BEXT(IR, ARM_IR_LDST_SH_BIT_ ## _x)
