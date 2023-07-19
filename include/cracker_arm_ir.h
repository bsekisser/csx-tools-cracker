#pragma once

#define ARM_DPI_OP mlBFEXT(IR, 24, 21)
#define ARM_DPI_I BEXT(IR, 25)
#define ARM_DPI_S BEXT(IR, 20)
#define ARM_DPI_SOP (ARM_DPI_I ? SOP_ROR : mlBFEXT(IR, 6, 5))

#define ARM_IR_CC mlBFEXT(IR, 31, 28)
#define ARM_IR_RD mlBFEXT(IR, 15, 12)
#define ARM_IR_RM mlBFEXT(IR, 3, 0)
#define ARM_IR_RN mlBFEXT(IR, 19, 16)
#define ARM_IR_RS mlBFEXT(IR, 11, 8)

#define ARM_IR_4and7 (BEXT(IR, 4) && BEXT(IR, 7))
#define ARM_IR_6_5 mlBFEXT(IR, 6, 5)
#define ARM_IR_27_25 mlBFEXT(IR, 27, 25)

/* **** */

enum {
	ARM_IR_LDST_BIT_b22 = 22,
	ARM_IR_LDST_BIT_l20 = 20,
	ARM_IR_LDST_BIT_p24 = 24,
	ARM_IR_LDST_BIT_u23 = 23,
	ARM_IR_LDST_BIT_w21 = 21,
};

#define ARM_IR_LDST_BIT(_x) BEXT(IR, ARM_IR_LDST_BIT_ ## _x)

/* **** */

enum {
	ARM_IR_LDSTM_BIT_s22 = 22,
};

#define ARM_IR_LDSTM_BIT(_x) BEXT(IR, ARM_IR_LDSTM_BIT_ ## _x)

/* **** */

enum {
	ARM_IR_LDST_SH_BIT_i22 = 22,
	ARM_IR_LDST_SH_BIT_s6 = 6,
	ARM_IR_LDST_SH_BIT_h5 = 5,
};

#define ARM_IR_LDST_SH_BIT(_x) BEXT(IR, ARM_IR_LDST_SH_BIT_ ## _x)

#define ARM_IR_LDST_SH_FLAG_B (ARM_IR_LDST_SH_FLAG_S && !ARM_IR_LDST_SH_BIT(h5))
#define ARM_IR_LDST_SH_FLAG_H (ARM_IR_LDST_SH_BIT(h5) & !ARM_IR_LDST_SH_FLAG_D)
#define ARM_IR_LDST_SH_FLAG_D (!ARM_IR_LDST_BIT(l20) && ARM_IR_LDST_SH_BIT(s6))
#define ARM_IR_LDST_SH_FLAG_S (ARM_IR_LDST_BIT(l20) && ARM_IR_LDST_SH_BIT(s6))
