#include "cracker.h"

/* **** */

#include "libbse/include/bitfield.h"
#include "libbse/include/log.h"
#include "libbse/include/unused.h"

/* **** */

#include <assert.h>
#include <stdint.h>

/* **** */

static void _cracker_reg_dst(cracker_ref cj, uint8_t rrd)
{
	assert(0 != cj->symbol);

	BSET(cj->symbol->reg.dst, rrd);
}

static void _cracker_reg_src(cracker_ref cj, uint8_t rrs)
{
	assert(0 != cj->symbol);

	if(BTST(cj->symbol->reg.dst, rrs))
		return;

	BSET(cj->symbol->reg.src, rrs);
}

/* **** */

static uint32_t _setup_rRx_vRx_src(cracker_ref cj, uint8_t rxs, uint8_t rrs)
{
	_setup_rR_vR(cj, rxs, rrs, vGPR(rrs));

	if(rPC == rrs) {
		vRx(rxs) += 4 >> IS_THUMB;
		vRx(rxs) &= ~(3 >> IS_THUMB);
	}

	return(vRx(rxs));
}

/* **** */

void cracker_reg_dst(cracker_ref cj, uint8_t rxd, uint8_t rrd)
{
	_cracker_reg_dst(cj, rrd);

	_setup_rR_vR(cj, rxd, rrd, 0);
}

uint32_t cracker_reg_dst_src(cracker_ref cj, uint8_t rxd, uint8_t rrd, uint8_t rxs)
{
	_cracker_reg_dst(cj, rrd);

	const uint32_t v = _setup_rR_vR(cj, rxd, rrd, vRx(rxs));

	rrCIRx(rxd)->_flags = GPR_rRx(rxs)->_flags;
	rrCIRx(rxd)->is_pc_ref = rRx_IS_PC(rxs);

	return(v);
}

void cracker_reg_dst_wb(cracker_ref cj, uint8_t rxd)
{
	vGPR_rRx(rxd) = vRx(rxd);
	GPR_rRx(rxd)->_flags = rrCIRx(rxd)->_flags;

	if((rPC == rRx(rxd)) && vRx(rxd))
		cracker_text(cj, vRx(rxd));
}

uint32_t cracker_reg_src(cracker_ref cj, uint8_t rxs, uint8_t rrs, int load)
{
	_cracker_reg_src(cj, rrs);

	const uint32_t v = load
		? _setup_rRx_vRx_src(cj, rxs, rrs)
		: _setup_rR_vR(cj, rxs, rrs, 0);

	rrCIRx(rxs)->_flags = GPR(rrs)->_flags;

	return(v);
}
