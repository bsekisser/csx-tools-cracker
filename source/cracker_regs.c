#include "cracker.h"

/* **** */

#include "bitfield.h"
#include "log.h"
#include "unused.h"

/* **** */

#include <assert.h>
#include <stdint.h>

/* **** */

static void _cracker_reg_dst(cracker_p cj, uint8_t rrd)
{
	assert(0 != cj->symbol);

	BSET(cj->symbol->reg.dst, rrd);
}

static void _cracker_reg_src(cracker_p cj, uint8_t rrs)
{
	assert(0 != cj->symbol);

	if(BTST(cj->symbol->reg.dst, rrs))
		return;
	
	BSET(cj->symbol->reg.src, rrs);
}

/* **** */

static void _setup_rRx_vRx_src(cracker_p cj, uint8_t rxs, uint8_t rrs)
{
	_setup_rR_vR(cj, rxs, rrs, vGPR(rrs));

	if(rPC == rrs) {
		vRx(rxs) += 4 >> IS_THUMB;
		vRx(rxs) &= ~3U >> IS_THUMB;
	}
}

/* **** */

void cracker_reg_dst(cracker_p cj, uint8_t rxd, uint8_t rrd)
{
	_cracker_reg_dst(cj, rrd);

	_setup_rR_vR(cj, rxd, rrd, 0);
}

void cracker_reg_dst_src(cracker_p cj, uint8_t rxd, uint8_t rrd, uint8_t rxs)
{
	_cracker_reg_dst(cj, rrd);
	
	_setup_rR_vR(cj, rxd, rrd, vRx(rxs));

	rrCIRx(rxd)->_flags = GPR_rRx(rxs)->_flags;
	rrCIRx(rxd)->is_pc_ref = rRx_IS_PC(rxs);
}

void cracker_reg_dst_wb(cracker_p cj, uint8_t rxd)
{
	vGPR_rRx(rxd) = vRx(rxd);
	GPR_rRx(rxd)->_flags = rrCIRx(rxd)->_flags;

	if((rPC == rxd) && vRx(rxd))
		cracker_text(cj, vRx(rxd));
}

void cracker_reg_src(cracker_p cj, uint8_t rxs, uint8_t rrs, int load)
{
	_cracker_reg_src(cj, rrs);

	if(load)
		_setup_rRx_vRx_src(cj, rxs, rrs);
	else
		_setup_rR_vR(cj, rxs, rrs, 0);

	rrCIRx(rxs)->_flags = GPR(rrs)->_flags;
}
