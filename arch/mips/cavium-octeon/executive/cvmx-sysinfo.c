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

#include <linux/module.h>

#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-spinlock.h>
#include <asm/octeon/cvmx-sysinfo.h>

static struct {
	struct cvmx_sysinfo sysinfo;	   
	cvmx_spinlock_t lock;	   

} state = {
	.lock = CVMX_SPINLOCK_UNLOCKED_INITIALIZER
};


uint64_t linux_mem32_min;
uint64_t linux_mem32_max;
uint64_t linux_mem32_wired;
uint64_t linux_mem32_offset;

struct cvmx_sysinfo *cvmx_sysinfo_get(void)
{
	return &(state.sysinfo);
}
EXPORT_SYMBOL(cvmx_sysinfo_get);

int cvmx_sysinfo_minimal_initialize(void *phy_mem_desc_ptr,
				    uint16_t board_type,
				    uint8_t board_rev_major,
				    uint8_t board_rev_minor,
				    uint32_t cpu_clock_hz)
{

	
	if (state.sysinfo.board_type)
		return 0;

	memset(&(state.sysinfo), 0x0, sizeof(state.sysinfo));
	state.sysinfo.phy_mem_desc_ptr = phy_mem_desc_ptr;
	state.sysinfo.board_type = board_type;
	state.sysinfo.board_rev_major = board_rev_major;
	state.sysinfo.board_rev_minor = board_rev_minor;
	state.sysinfo.cpu_clock_hz = cpu_clock_hz;

	return 1;
}
