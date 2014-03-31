/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2010 Cavium Networks
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


#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-l2c.h>
#include <asm/octeon/cvmx-spinlock.h>

cvmx_spinlock_t cvmx_l2c_spinlock;

int cvmx_l2c_get_core_way_partition(uint32_t core)
{
	uint32_t field;

	
	if (core >= cvmx_octeon_num_cores())
		return -1;

	if (OCTEON_IS_MODEL(OCTEON_CN63XX))
		return cvmx_read_csr(CVMX_L2C_WPAR_PPX(core)) & 0xffff;

	field = (core & 0x3) * 8;


	switch (core & 0xC) {
	case 0x0:
		return (cvmx_read_csr(CVMX_L2C_SPAR0) & (0xFF << field)) >> field;
	case 0x4:
		return (cvmx_read_csr(CVMX_L2C_SPAR1) & (0xFF << field)) >> field;
	case 0x8:
		return (cvmx_read_csr(CVMX_L2C_SPAR2) & (0xFF << field)) >> field;
	case 0xC:
		return (cvmx_read_csr(CVMX_L2C_SPAR3) & (0xFF << field)) >> field;
	}
	return 0;
}

int cvmx_l2c_set_core_way_partition(uint32_t core, uint32_t mask)
{
	uint32_t field;
	uint32_t valid_mask;

	valid_mask = (0x1 << cvmx_l2c_get_num_assoc()) - 1;

	mask &= valid_mask;

	
	if (mask == valid_mask && !OCTEON_IS_MODEL(OCTEON_CN63XX))
		return -1;

	
	if (core >= cvmx_octeon_num_cores())
		return -1;

	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		cvmx_write_csr(CVMX_L2C_WPAR_PPX(core), mask);
		return 0;
	}

	field = (core & 0x3) * 8;

	switch (core & 0xC) {
	case 0x0:
		cvmx_write_csr(CVMX_L2C_SPAR0,
			       (cvmx_read_csr(CVMX_L2C_SPAR0) & ~(0xFF << field)) |
			       mask << field);
		break;
	case 0x4:
		cvmx_write_csr(CVMX_L2C_SPAR1,
			       (cvmx_read_csr(CVMX_L2C_SPAR1) & ~(0xFF << field)) |
			       mask << field);
		break;
	case 0x8:
		cvmx_write_csr(CVMX_L2C_SPAR2,
			       (cvmx_read_csr(CVMX_L2C_SPAR2) & ~(0xFF << field)) |
			       mask << field);
		break;
	case 0xC:
		cvmx_write_csr(CVMX_L2C_SPAR3,
			       (cvmx_read_csr(CVMX_L2C_SPAR3) & ~(0xFF << field)) |
			       mask << field);
		break;
	}
	return 0;
}

int cvmx_l2c_set_hw_way_partition(uint32_t mask)
{
	uint32_t valid_mask;

	valid_mask = (0x1 << cvmx_l2c_get_num_assoc()) - 1;
	mask &= valid_mask;

	
	if (mask == valid_mask  && !OCTEON_IS_MODEL(OCTEON_CN63XX))
		return -1;

	if (OCTEON_IS_MODEL(OCTEON_CN63XX))
		cvmx_write_csr(CVMX_L2C_WPAR_IOBX(0), mask);
	else
		cvmx_write_csr(CVMX_L2C_SPAR4,
			       (cvmx_read_csr(CVMX_L2C_SPAR4) & ~0xFF) | mask);
	return 0;
}

int cvmx_l2c_get_hw_way_partition(void)
{
	if (OCTEON_IS_MODEL(OCTEON_CN63XX))
		return cvmx_read_csr(CVMX_L2C_WPAR_IOBX(0)) & 0xffff;
	else
		return cvmx_read_csr(CVMX_L2C_SPAR4) & (0xFF);
}

void cvmx_l2c_config_perf(uint32_t counter, enum cvmx_l2c_event event,
			  uint32_t clear_on_read)
{
	if (OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN3XXX)) {
		union cvmx_l2c_pfctl pfctl;

		pfctl.u64 = cvmx_read_csr(CVMX_L2C_PFCTL);

		switch (counter) {
		case 0:
			pfctl.s.cnt0sel = event;
			pfctl.s.cnt0ena = 1;
			pfctl.s.cnt0rdclr = clear_on_read;
			break;
		case 1:
			pfctl.s.cnt1sel = event;
			pfctl.s.cnt1ena = 1;
			pfctl.s.cnt1rdclr = clear_on_read;
			break;
		case 2:
			pfctl.s.cnt2sel = event;
			pfctl.s.cnt2ena = 1;
			pfctl.s.cnt2rdclr = clear_on_read;
			break;
		case 3:
		default:
			pfctl.s.cnt3sel = event;
			pfctl.s.cnt3ena = 1;
			pfctl.s.cnt3rdclr = clear_on_read;
			break;
		}

		cvmx_write_csr(CVMX_L2C_PFCTL, pfctl.u64);
	} else {
		union cvmx_l2c_tadx_prf l2c_tadx_prf;
		int tad;

		cvmx_dprintf("L2C performance counter events are different for this chip, mapping 'event' to cvmx_l2c_tad_event_t\n");
		if (clear_on_read)
			cvmx_dprintf("L2C counters don't support clear on read for this chip\n");

		l2c_tadx_prf.u64 = cvmx_read_csr(CVMX_L2C_TADX_PRF(0));

		switch (counter) {
		case 0:
			l2c_tadx_prf.s.cnt0sel = event;
			break;
		case 1:
			l2c_tadx_prf.s.cnt1sel = event;
			break;
		case 2:
			l2c_tadx_prf.s.cnt2sel = event;
			break;
		default:
		case 3:
			l2c_tadx_prf.s.cnt3sel = event;
			break;
		}
		for (tad = 0; tad < CVMX_L2C_TADS; tad++)
			cvmx_write_csr(CVMX_L2C_TADX_PRF(tad),
				       l2c_tadx_prf.u64);
	}
}

uint64_t cvmx_l2c_read_perf(uint32_t counter)
{
	switch (counter) {
	case 0:
		if (OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN3XXX))
			return cvmx_read_csr(CVMX_L2C_PFC0);
		else {
			uint64_t counter = 0;
			int tad;
			for (tad = 0; tad < CVMX_L2C_TADS; tad++)
				counter += cvmx_read_csr(CVMX_L2C_TADX_PFC0(tad));
			return counter;
		}
	case 1:
		if (OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN3XXX))
			return cvmx_read_csr(CVMX_L2C_PFC1);
		else {
			uint64_t counter = 0;
			int tad;
			for (tad = 0; tad < CVMX_L2C_TADS; tad++)
				counter += cvmx_read_csr(CVMX_L2C_TADX_PFC1(tad));
			return counter;
		}
	case 2:
		if (OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN3XXX))
			return cvmx_read_csr(CVMX_L2C_PFC2);
		else {
			uint64_t counter = 0;
			int tad;
			for (tad = 0; tad < CVMX_L2C_TADS; tad++)
				counter += cvmx_read_csr(CVMX_L2C_TADX_PFC2(tad));
			return counter;
		}
	case 3:
	default:
		if (OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN3XXX))
			return cvmx_read_csr(CVMX_L2C_PFC3);
		else {
			uint64_t counter = 0;
			int tad;
			for (tad = 0; tad < CVMX_L2C_TADS; tad++)
				counter += cvmx_read_csr(CVMX_L2C_TADX_PFC3(tad));
			return counter;
		}
	}
}

static void fault_in(uint64_t addr, int len)
{
	volatile char *ptr;
	volatile char dummy;
	len += addr & CVMX_CACHE_LINE_MASK;
	addr &= ~CVMX_CACHE_LINE_MASK;
	ptr = (volatile char *)cvmx_phys_to_ptr(addr);
	CVMX_DCACHE_INVALIDATE;
	while (len > 0) {
		dummy += *ptr;
		len -= CVMX_CACHE_LINE_SIZE;
		ptr += CVMX_CACHE_LINE_SIZE;
	}
}

int cvmx_l2c_lock_line(uint64_t addr)
{
	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		int shift = CVMX_L2C_TAG_ADDR_ALIAS_SHIFT;
		uint64_t assoc = cvmx_l2c_get_num_assoc();
		uint64_t tag = addr >> shift;
		uint64_t index = CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, cvmx_l2c_address_to_index(addr) << CVMX_L2C_IDX_ADDR_SHIFT);
		uint64_t way;
		union cvmx_l2c_tadx_tag l2c_tadx_tag;

		CVMX_CACHE_LCKL2(CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, addr), 0);

		
		for (way = 0; way < assoc; way++) {
			CVMX_CACHE_LTGL2I(index | (way << shift), 0);
			
			CVMX_SYNC;
			l2c_tadx_tag.u64 = cvmx_read_csr(CVMX_L2C_TADX_TAG(0));
			if (l2c_tadx_tag.s.valid && l2c_tadx_tag.s.tag == tag)
				break;
		}

		
		if (way >= assoc) {
			
			return -1;
		}

		
		if (!l2c_tadx_tag.s.lock) {
			
			return -1;
		}
		return way;
	} else {
		int retval = 0;
		union cvmx_l2c_dbg l2cdbg;
		union cvmx_l2c_lckbase lckbase;
		union cvmx_l2c_lckoff lckoff;
		union cvmx_l2t_err l2t_err;

		cvmx_spinlock_lock(&cvmx_l2c_spinlock);

		l2cdbg.u64 = 0;
		lckbase.u64 = 0;
		lckoff.u64 = 0;

		
		l2t_err.u64 = cvmx_read_csr(CVMX_L2T_ERR);
		l2t_err.s.lckerr = 1;
		l2t_err.s.lckerr2 = 1;
		cvmx_write_csr(CVMX_L2T_ERR, l2t_err.u64);

		addr &= ~CVMX_CACHE_LINE_MASK;

		
		l2cdbg.s.ppnum = cvmx_get_core_num();
		CVMX_SYNC;
		cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64);
		cvmx_read_csr(CVMX_L2C_DBG);

		lckoff.s.lck_offset = 0; 
		cvmx_write_csr(CVMX_L2C_LCKOFF, lckoff.u64);
		cvmx_read_csr(CVMX_L2C_LCKOFF);

		if (((union cvmx_l2c_cfg)(cvmx_read_csr(CVMX_L2C_CFG))).s.idxalias) {
			int alias_shift = CVMX_L2C_IDX_ADDR_SHIFT + 2 * CVMX_L2_SET_BITS - 1;
			uint64_t addr_tmp = addr ^ (addr & ((1 << alias_shift) - 1)) >> CVMX_L2_SET_BITS;
			lckbase.s.lck_base = addr_tmp >> 7;
		} else {
			lckbase.s.lck_base = addr >> 7;
		}

		lckbase.s.lck_ena = 1;
		cvmx_write_csr(CVMX_L2C_LCKBASE, lckbase.u64);
		
		cvmx_read_csr(CVMX_L2C_LCKBASE);

		fault_in(addr, CVMX_CACHE_LINE_SIZE);

		lckbase.s.lck_ena = 0;
		cvmx_write_csr(CVMX_L2C_LCKBASE, lckbase.u64);
		
		cvmx_read_csr(CVMX_L2C_LCKBASE);

		
		cvmx_write_csr(CVMX_L2C_DBG, 0);
		cvmx_read_csr(CVMX_L2C_DBG);

		l2t_err.u64 = cvmx_read_csr(CVMX_L2T_ERR);
		if (l2t_err.s.lckerr || l2t_err.s.lckerr2)
			retval = 1;  

		cvmx_spinlock_unlock(&cvmx_l2c_spinlock);
		return retval;
	}
}

int cvmx_l2c_lock_mem_region(uint64_t start, uint64_t len)
{
	int retval = 0;

	
	len += start & CVMX_CACHE_LINE_MASK;
	start &= ~CVMX_CACHE_LINE_MASK;
	len = (len + CVMX_CACHE_LINE_MASK) & ~CVMX_CACHE_LINE_MASK;

	while (len) {
		retval += cvmx_l2c_lock_line(start);
		start += CVMX_CACHE_LINE_SIZE;
		len -= CVMX_CACHE_LINE_SIZE;
	}
	return retval;
}

void cvmx_l2c_flush(void)
{
	uint64_t assoc, set;
	uint64_t n_assoc, n_set;

	n_set = cvmx_l2c_get_num_sets();
	n_assoc = cvmx_l2c_get_num_assoc();

	if (OCTEON_IS_MODEL(OCTEON_CN6XXX)) {
		uint64_t address;
		
		int assoc_shift = CVMX_L2C_TAG_ADDR_ALIAS_SHIFT;
		int set_shift = CVMX_L2C_IDX_ADDR_SHIFT;
		for (set = 0; set < n_set; set++) {
			for (assoc = 0; assoc < n_assoc; assoc++) {
				address = CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
						       (assoc << assoc_shift) |	(set << set_shift));
				CVMX_CACHE_WBIL2I(address, 0);
			}
		}
	} else {
		for (set = 0; set < n_set; set++)
			for (assoc = 0; assoc < n_assoc; assoc++)
				cvmx_l2c_flush_line(assoc, set);
	}
}


int cvmx_l2c_unlock_line(uint64_t address)
{

	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		int assoc;
		union cvmx_l2c_tag tag;
		uint32_t tag_addr;
		uint32_t index = cvmx_l2c_address_to_index(address);

		tag_addr = ((address >> CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) & ((1 << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) - 1));

		for (assoc = 0; assoc < CVMX_L2_ASSOC; assoc++) {
			tag = cvmx_l2c_get_tag(assoc, index);

			if (tag.s.V && (tag.s.addr == tag_addr)) {
				CVMX_CACHE_WBIL2(CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, address), 0);
				return tag.s.L;
			}
		}
	} else {
		int assoc;
		union cvmx_l2c_tag tag;
		uint32_t tag_addr;

		uint32_t index = cvmx_l2c_address_to_index(address);

		
		tag_addr = ((address >> CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) & ((1 << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) - 1));
		for (assoc = 0; assoc < CVMX_L2_ASSOC; assoc++) {
			tag = cvmx_l2c_get_tag(assoc, index);

			if (tag.s.V && (tag.s.addr == tag_addr)) {
				cvmx_l2c_flush_line(assoc, index);
				return tag.s.L;
			}
		}
	}
	return 0;
}

int cvmx_l2c_unlock_mem_region(uint64_t start, uint64_t len)
{
	int num_unlocked = 0;
	
	len += start & CVMX_CACHE_LINE_MASK;
	start &= ~CVMX_CACHE_LINE_MASK;
	len = (len + CVMX_CACHE_LINE_MASK) & ~CVMX_CACHE_LINE_MASK;
	while (len > 0) {
		num_unlocked += cvmx_l2c_unlock_line(start);
		start += CVMX_CACHE_LINE_SIZE;
		len -= CVMX_CACHE_LINE_SIZE;
	}

	return num_unlocked;
}

union __cvmx_l2c_tag {
	uint64_t u64;
	struct cvmx_l2c_tag_cn50xx {
		uint64_t reserved:40;
		uint64_t V:1;		
		uint64_t D:1;		
		uint64_t L:1;		
		uint64_t U:1;		
		uint64_t addr:20;	
	} cn50xx;
	struct cvmx_l2c_tag_cn30xx {
		uint64_t reserved:41;
		uint64_t V:1;		
		uint64_t D:1;		
		uint64_t L:1;		
		uint64_t U:1;		
		uint64_t addr:19;	
	} cn30xx;
	struct cvmx_l2c_tag_cn31xx {
		uint64_t reserved:42;
		uint64_t V:1;		
		uint64_t D:1;		
		uint64_t L:1;		
		uint64_t U:1;		
		uint64_t addr:18;	
	} cn31xx;
	struct cvmx_l2c_tag_cn38xx {
		uint64_t reserved:43;
		uint64_t V:1;		
		uint64_t D:1;		
		uint64_t L:1;		
		uint64_t U:1;		
		uint64_t addr:17;	
	} cn38xx;
	struct cvmx_l2c_tag_cn58xx {
		uint64_t reserved:44;
		uint64_t V:1;		
		uint64_t D:1;		
		uint64_t L:1;		
		uint64_t U:1;		
		uint64_t addr:16;	
	} cn58xx;
	struct cvmx_l2c_tag_cn58xx cn56xx;	
	struct cvmx_l2c_tag_cn31xx cn52xx;	
};


static union __cvmx_l2c_tag __read_l2_tag(uint64_t assoc, uint64_t index)
{

	uint64_t debug_tag_addr = CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, (index << 7) + 96);
	uint64_t core = cvmx_get_core_num();
	union __cvmx_l2c_tag tag_val;
	uint64_t dbg_addr = CVMX_L2C_DBG;
	unsigned long flags;

	union cvmx_l2c_dbg debug_val;
	debug_val.u64 = 0;
	debug_val.s.ppnum = core;
	debug_val.s.l2t = 1;
	debug_val.s.set = assoc;

	local_irq_save(flags);
	CVMX_SYNC;
	
	CVMX_DCACHE_INVALIDATE;


	asm volatile (
		".set push\n\t"
		".set mips64\n\t"
		".set noreorder\n\t"
		"sd    %[dbg_val], 0(%[dbg_addr])\n\t"   
		"ld    $0, 0(%[dbg_addr])\n\t"
		"ld    %[tag_val], 0(%[tag_addr])\n\t"   
		"sd    $0, 0(%[dbg_addr])\n\t"          
		"ld    $0, 0(%[dbg_addr])\n\t"
		"cache 9, 0($0)\n\t"             
		".set pop"
		: [tag_val] "=r" (tag_val)
		: [dbg_addr] "r" (dbg_addr), [dbg_val] "r" (debug_val), [tag_addr] "r" (debug_tag_addr)
		: "memory");

	local_irq_restore(flags);

	return tag_val;
}


union cvmx_l2c_tag cvmx_l2c_get_tag(uint32_t association, uint32_t index)
{
	union cvmx_l2c_tag tag;
	tag.u64 = 0;

	if ((int)association >= cvmx_l2c_get_num_assoc()) {
		cvmx_dprintf("ERROR: cvmx_l2c_get_tag association out of range\n");
		return tag;
	}
	if ((int)index >= cvmx_l2c_get_num_sets()) {
		cvmx_dprintf("ERROR: cvmx_l2c_get_tag index out of range (arg: %d, max: %d)\n",
			     (int)index, cvmx_l2c_get_num_sets());
		return tag;
	}
	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		union cvmx_l2c_tadx_tag l2c_tadx_tag;
		uint64_t address = CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
						(association << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) |
						(index << CVMX_L2C_IDX_ADDR_SHIFT));
		CVMX_CACHE_LTGL2I(address, 0);
		CVMX_SYNC;   
		l2c_tadx_tag.u64 = cvmx_read_csr(CVMX_L2C_TADX_TAG(0));

		tag.s.V     = l2c_tadx_tag.s.valid;
		tag.s.D     = l2c_tadx_tag.s.dirty;
		tag.s.L     = l2c_tadx_tag.s.lock;
		tag.s.U     = l2c_tadx_tag.s.use;
		tag.s.addr  = l2c_tadx_tag.s.tag;
	} else {
		union __cvmx_l2c_tag tmp_tag;
		
		tmp_tag = __read_l2_tag(association, index);

		if (OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)) {
			tag.s.V    = tmp_tag.cn58xx.V;
			tag.s.D    = tmp_tag.cn58xx.D;
			tag.s.L    = tmp_tag.cn58xx.L;
			tag.s.U    = tmp_tag.cn58xx.U;
			tag.s.addr = tmp_tag.cn58xx.addr;
		} else if (OCTEON_IS_MODEL(OCTEON_CN38XX)) {
			tag.s.V    = tmp_tag.cn38xx.V;
			tag.s.D    = tmp_tag.cn38xx.D;
			tag.s.L    = tmp_tag.cn38xx.L;
			tag.s.U    = tmp_tag.cn38xx.U;
			tag.s.addr = tmp_tag.cn38xx.addr;
		} else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN52XX)) {
			tag.s.V    = tmp_tag.cn31xx.V;
			tag.s.D    = tmp_tag.cn31xx.D;
			tag.s.L    = tmp_tag.cn31xx.L;
			tag.s.U    = tmp_tag.cn31xx.U;
			tag.s.addr = tmp_tag.cn31xx.addr;
		} else if (OCTEON_IS_MODEL(OCTEON_CN30XX)) {
			tag.s.V    = tmp_tag.cn30xx.V;
			tag.s.D    = tmp_tag.cn30xx.D;
			tag.s.L    = tmp_tag.cn30xx.L;
			tag.s.U    = tmp_tag.cn30xx.U;
			tag.s.addr = tmp_tag.cn30xx.addr;
		} else if (OCTEON_IS_MODEL(OCTEON_CN50XX)) {
			tag.s.V    = tmp_tag.cn50xx.V;
			tag.s.D    = tmp_tag.cn50xx.D;
			tag.s.L    = tmp_tag.cn50xx.L;
			tag.s.U    = tmp_tag.cn50xx.U;
			tag.s.addr = tmp_tag.cn50xx.addr;
		} else {
			cvmx_dprintf("Unsupported OCTEON Model in %s\n", __func__);
		}
	}
	return tag;
}

uint32_t cvmx_l2c_address_to_index(uint64_t addr)
{
	uint64_t idx = addr >> CVMX_L2C_IDX_ADDR_SHIFT;
	int indxalias = 0;

	if (OCTEON_IS_MODEL(OCTEON_CN6XXX)) {
		union cvmx_l2c_ctl l2c_ctl;
		l2c_ctl.u64 = cvmx_read_csr(CVMX_L2C_CTL);
		indxalias = !l2c_ctl.s.disidxalias;
	} else {
		union cvmx_l2c_cfg l2c_cfg;
		l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
		indxalias = l2c_cfg.s.idxalias;
	}

	if (indxalias) {
		if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
			uint32_t a_14_12 = (idx / (CVMX_L2C_MEMBANK_SELECT_SIZE/(1<<CVMX_L2C_IDX_ADDR_SHIFT))) & 0x7;
			idx ^= idx / cvmx_l2c_get_num_sets();
			idx ^= a_14_12;
		} else {
			idx ^= ((addr & CVMX_L2C_ALIAS_MASK) >> CVMX_L2C_TAG_ADDR_ALIAS_SHIFT);
		}
	}
	idx &= CVMX_L2C_IDX_MASK;
	return idx;
}

int cvmx_l2c_get_cache_size_bytes(void)
{
	return cvmx_l2c_get_num_sets() * cvmx_l2c_get_num_assoc() *
		CVMX_CACHE_LINE_SIZE;
}

int cvmx_l2c_get_set_bits(void)
{
	int l2_set_bits;
	if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
		l2_set_bits = 11;	
	else if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN63XX))
		l2_set_bits = 10;	
	else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
		l2_set_bits = 9;	
	else if (OCTEON_IS_MODEL(OCTEON_CN30XX))
		l2_set_bits = 8;	
	else if (OCTEON_IS_MODEL(OCTEON_CN50XX))
		l2_set_bits = 7;	
	else {
		cvmx_dprintf("Unsupported OCTEON Model in %s\n", __func__);
		l2_set_bits = 11;	
	}
	return l2_set_bits;
}

int cvmx_l2c_get_num_sets(void)
{
	return 1 << cvmx_l2c_get_set_bits();
}

int cvmx_l2c_get_num_assoc(void)
{
	int l2_assoc;
	if (OCTEON_IS_MODEL(OCTEON_CN56XX) ||
	    OCTEON_IS_MODEL(OCTEON_CN52XX) ||
	    OCTEON_IS_MODEL(OCTEON_CN58XX) ||
	    OCTEON_IS_MODEL(OCTEON_CN50XX) ||
	    OCTEON_IS_MODEL(OCTEON_CN38XX))
		l2_assoc = 8;
	else if (OCTEON_IS_MODEL(OCTEON_CN63XX))
		l2_assoc = 16;
	else if (OCTEON_IS_MODEL(OCTEON_CN31XX) ||
		 OCTEON_IS_MODEL(OCTEON_CN30XX))
		l2_assoc = 4;
	else {
		cvmx_dprintf("Unsupported OCTEON Model in %s\n", __func__);
		l2_assoc = 8;
	}

	
	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		union cvmx_mio_fus_dat3 mio_fus_dat3;

		mio_fus_dat3.u64 = cvmx_read_csr(CVMX_MIO_FUS_DAT3);

		if (mio_fus_dat3.s.l2c_crip == 3)
			l2_assoc = 4;
		else if (mio_fus_dat3.s.l2c_crip == 2)
			l2_assoc = 8;
		else if (mio_fus_dat3.s.l2c_crip == 1)
			l2_assoc = 12;
	} else {
		union cvmx_l2d_fus3 val;
		val.u64 = cvmx_read_csr(CVMX_L2D_FUS3);
		if ((val.u64 >> 35) & 0x1)
			l2_assoc = l2_assoc >> 2;
		else if ((val.u64 >> 34) & 0x1)
			l2_assoc = l2_assoc >> 1;
	}
	return l2_assoc;
}

void cvmx_l2c_flush_line(uint32_t assoc, uint32_t index)
{
	
	if (index > (uint32_t)cvmx_l2c_get_num_sets()) {
		cvmx_dprintf("ERROR: cvmx_l2c_flush_line index out of range.\n");
		return;
	}

	
	if (assoc > (uint32_t)cvmx_l2c_get_num_assoc()) {
		cvmx_dprintf("ERROR: cvmx_l2c_flush_line association out of range.\n");
		return;
	}

	if (OCTEON_IS_MODEL(OCTEON_CN63XX)) {
		uint64_t address;
		address = CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
				(assoc << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) |
				(index << CVMX_L2C_IDX_ADDR_SHIFT));
		CVMX_CACHE_WBIL2I(address, 0);
	} else {
		union cvmx_l2c_dbg l2cdbg;

		l2cdbg.u64 = 0;
		if (!OCTEON_IS_MODEL(OCTEON_CN30XX))
			l2cdbg.s.ppnum = cvmx_get_core_num();
		l2cdbg.s.finv = 1;

		l2cdbg.s.set = assoc;
		cvmx_spinlock_lock(&cvmx_l2c_spinlock);
		CVMX_SYNC;
		cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64);
		cvmx_read_csr(CVMX_L2C_DBG);

		CVMX_PREPARE_FOR_STORE(CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
						    index * CVMX_CACHE_LINE_SIZE),
				       0);
		
		CVMX_SYNC;
		cvmx_write_csr(CVMX_L2C_DBG, 0);
		cvmx_read_csr(CVMX_L2C_DBG);
		cvmx_spinlock_unlock(&cvmx_l2c_spinlock);
	}
}
