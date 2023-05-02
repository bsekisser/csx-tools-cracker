#pragma once

/* **** */

#include "cracker.h"

/* **** */

#include <stdint.h>
#include <sys/types.h>

/* **** */

typedef struct cracker_reg_t* cracker_reg_p;
typedef struct cracker_reg_t {
	uint32_t v;

	union {
		uint _flags;
		struct {
			uint is_pc_ref:1;
		};
	};
}cracker_reg_t;

void cracker_reg_dst(cracker_p cj, uint8_t rxd, uint8_t rrd);
void cracker_reg_dst_wb(cracker_p cj, uint8_t rxd);
void cracker_reg_dst_src(cracker_p cj, uint8_t rxd, uint8_t rrd, uint8_t rxs);
void cracker_reg_src(cracker_p cj, uint8_t rxs, uint8_t rrs, int load);

#define setup_rR_dst(_rxd, _rrd) cracker_reg_dst(cj, rrR##_rxd, _rrd)
#define setup_rR_dst_rR_src(_rxd, _rrd, _rxs) cracker_reg_dst_src(cj, rrR##_rxd, _rrd, rrR##_rxs)
#define setup_rR_src(_rxs, _rrs) cracker_reg_src(cj, rrR##_rxs, _rrs, 0)
#define setup_rR_vR_src(_rxs, _rrs) cracker_reg_src(cj, rrR##_rxs, _rrs, 1)
