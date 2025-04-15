#pragma once

/* **** */

#include "cracker.h"

/* **** */

#include <stdint.h>
#include <sys/types.h>

/* **** */

typedef struct cracker_reg_tag* cracker_reg_ptr;
typedef cracker_reg_ptr const cracker_reg_ref;

typedef struct cracker_reg_tag {
	uint32_t v;

	union {
		unsigned _flags;
		struct {
			unsigned is_pc_ref:1;
		};
	};
}cracker_reg_t;

void cracker_reg_dst(cracker_ref cj, uint8_t rxd, uint8_t rrd);
void cracker_reg_dst_wb(cracker_ref cj, uint8_t rxd);
uint32_t cracker_reg_dst_src(cracker_ref cj, uint8_t rxd, uint8_t rrd, uint8_t rxs);
uint32_t cracker_reg_src(cracker_ref cj, uint8_t rxs, uint8_t rrs, int load);

#define setup_rR_dst(_rxd, _rrd) cracker_reg_dst(cj, rrR##_rxd, _rrd)
#define setup_rR_dst_rR_src(_rxd, _rrd, _rxs) cracker_reg_dst_src(cj, rrR##_rxd, _rrd, rrR##_rxs)
#define setup_rR_src(_rxs, _rrs) cracker_reg_src(cj, rrR##_rxs, _rrs, 0)
#define setup_rR_vR_src(_rxs, _rrs) cracker_reg_src(cj, rrR##_rxs, _rrs, 1)
