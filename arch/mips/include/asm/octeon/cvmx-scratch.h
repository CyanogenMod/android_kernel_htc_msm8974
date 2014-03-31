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


#ifndef __CVMX_SCRATCH_H__
#define __CVMX_SCRATCH_H__

#define CVMX_SCRATCH_BASE       (-32768l)	

static inline uint8_t cvmx_scratch_read8(uint64_t address)
{
	return *CASTPTR(volatile uint8_t, CVMX_SCRATCH_BASE + address);
}

static inline uint16_t cvmx_scratch_read16(uint64_t address)
{
	return *CASTPTR(volatile uint16_t, CVMX_SCRATCH_BASE + address);
}

static inline uint32_t cvmx_scratch_read32(uint64_t address)
{
	return *CASTPTR(volatile uint32_t, CVMX_SCRATCH_BASE + address);
}

static inline uint64_t cvmx_scratch_read64(uint64_t address)
{
	return *CASTPTR(volatile uint64_t, CVMX_SCRATCH_BASE + address);
}

static inline void cvmx_scratch_write8(uint64_t address, uint64_t value)
{
	*CASTPTR(volatile uint8_t, CVMX_SCRATCH_BASE + address) =
	    (uint8_t) value;
}

static inline void cvmx_scratch_write16(uint64_t address, uint64_t value)
{
	*CASTPTR(volatile uint16_t, CVMX_SCRATCH_BASE + address) =
	    (uint16_t) value;
}

static inline void cvmx_scratch_write32(uint64_t address, uint64_t value)
{
	*CASTPTR(volatile uint32_t, CVMX_SCRATCH_BASE + address) =
	    (uint32_t) value;
}

static inline void cvmx_scratch_write64(uint64_t address, uint64_t value)
{
	*CASTPTR(volatile uint64_t, CVMX_SCRATCH_BASE + address) = value;
}

#endif 
