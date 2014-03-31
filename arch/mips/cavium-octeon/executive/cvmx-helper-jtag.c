
/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/


#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-helper-jtag.h>


void cvmx_helper_qlm_jtag_init(void)
{
	union cvmx_ciu_qlm_jtgc jtgc;
	uint32_t clock_div = 0;
	uint32_t divisor = cvmx_sysinfo_get()->cpu_clock_hz / (25 * 1000000);
	divisor = (divisor - 1) >> 2;
	
	while (divisor) {
		clock_div++;
		divisor = divisor >> 1;
	}

	jtgc.u64 = 0;
	jtgc.s.clk_div = clock_div;
	jtgc.s.mux_sel = 0;
	if (OCTEON_IS_MODEL(OCTEON_CN52XX))
		jtgc.s.bypass = 0x3;
	else
		jtgc.s.bypass = 0xf;
	cvmx_write_csr(CVMX_CIU_QLM_JTGC, jtgc.u64);
	cvmx_read_csr(CVMX_CIU_QLM_JTGC);
}

uint32_t cvmx_helper_qlm_jtag_shift(int qlm, int bits, uint32_t data)
{
	union cvmx_ciu_qlm_jtgd jtgd;
	jtgd.u64 = 0;
	jtgd.s.shift = 1;
	jtgd.s.shft_cnt = bits - 1;
	jtgd.s.shft_reg = data;
	if (!OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X))
		jtgd.s.select = 1 << qlm;
	cvmx_write_csr(CVMX_CIU_QLM_JTGD, jtgd.u64);
	do {
		jtgd.u64 = cvmx_read_csr(CVMX_CIU_QLM_JTGD);
	} while (jtgd.s.shift);
	return jtgd.s.shft_reg >> (32 - bits);
}

void cvmx_helper_qlm_jtag_shift_zeros(int qlm, int bits)
{
	while (bits > 0) {
		int n = bits;
		if (n > 32)
			n = 32;
		cvmx_helper_qlm_jtag_shift(qlm, n, 0);
		bits -= n;
	}
}

void cvmx_helper_qlm_jtag_update(int qlm)
{
	union cvmx_ciu_qlm_jtgd jtgd;

	
	jtgd.u64 = 0;
	jtgd.s.update = 1;
	if (!OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X))
		jtgd.s.select = 1 << qlm;
	cvmx_write_csr(CVMX_CIU_QLM_JTGD, jtgd.u64);
	do {
		jtgd.u64 = cvmx_read_csr(CVMX_CIU_QLM_JTGD);
	} while (jtgd.s.update);
}
