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


#ifndef __CVMX_FPA_H__
#define __CVMX_FPA_H__

#include "cvmx-address.h"
#include "cvmx-fpa-defs.h"

#define CVMX_FPA_NUM_POOLS      8
#define CVMX_FPA_MIN_BLOCK_SIZE 128
#define CVMX_FPA_ALIGNMENT      128

typedef union {
	uint64_t u64;
	struct {
		uint64_t scraddr:8;
		
		uint64_t len:8;
		
		uint64_t did:8;
		uint64_t addr:40;
	} s;
} cvmx_fpa_iobdma_data_t;

typedef struct {
	
	const char *name;
	
	uint64_t size;
	
	void *base;
	
	uint64_t starting_element_count;
} cvmx_fpa_pool_info_t;

extern cvmx_fpa_pool_info_t cvmx_fpa_pool_info[CVMX_FPA_NUM_POOLS];


static inline const char *cvmx_fpa_get_name(uint64_t pool)
{
	return cvmx_fpa_pool_info[pool].name;
}

static inline void *cvmx_fpa_get_base(uint64_t pool)
{
	return cvmx_fpa_pool_info[pool].base;
}

static inline int cvmx_fpa_is_member(uint64_t pool, void *ptr)
{
	return ((ptr >= cvmx_fpa_pool_info[pool].base) &&
		((char *)ptr <
		 ((char *)(cvmx_fpa_pool_info[pool].base)) +
		 cvmx_fpa_pool_info[pool].size *
		 cvmx_fpa_pool_info[pool].starting_element_count));
}

static inline void cvmx_fpa_enable(void)
{
	union cvmx_fpa_ctl_status status;

	status.u64 = cvmx_read_csr(CVMX_FPA_CTL_STATUS);
	if (status.s.enb) {
		cvmx_dprintf
		    ("Warning: Enabling FPA when FPA already enabled.\n");
	}

	if (cvmx_octeon_is_pass1()) {
		union cvmx_fpa_fpfx_marks marks;
		int i;
		for (i = 1; i < 8; i++) {
			marks.u64 =
			    cvmx_read_csr(CVMX_FPA_FPF1_MARKS + (i - 1) * 8ull);
			marks.s.fpf_wr = 0xe0;
			cvmx_write_csr(CVMX_FPA_FPF1_MARKS + (i - 1) * 8ull,
				       marks.u64);
		}

		
		cvmx_wait(10);
	}

	
	status.u64 = 0;
	status.s.enb = 1;
	cvmx_write_csr(CVMX_FPA_CTL_STATUS, status.u64);
}

static inline void *cvmx_fpa_alloc(uint64_t pool)
{
	uint64_t address =
	    cvmx_read_csr(CVMX_ADDR_DID(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool)));
	if (address)
		return cvmx_phys_to_ptr(address);
	else
		return NULL;
}

static inline void cvmx_fpa_async_alloc(uint64_t scr_addr, uint64_t pool)
{
	cvmx_fpa_iobdma_data_t data;

	data.s.scraddr = scr_addr >> 3;
	data.s.len = 1;
	data.s.did = CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool);
	data.s.addr = 0;
	cvmx_send_single(data.u64);
}

static inline void cvmx_fpa_free_nosync(void *ptr, uint64_t pool,
					uint64_t num_cache_lines)
{
	cvmx_addr_t newptr;
	newptr.u64 = cvmx_ptr_to_phys(ptr);
	newptr.sfilldidspace.didspace =
	    CVMX_ADDR_DIDSPACE(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool));
	
	barrier();
	/* value written is number of cache lines not written back */
	cvmx_write_io(newptr.u64, num_cache_lines);
}

static inline void cvmx_fpa_free(void *ptr, uint64_t pool,
				 uint64_t num_cache_lines)
{
	cvmx_addr_t newptr;
	newptr.u64 = cvmx_ptr_to_phys(ptr);
	newptr.sfilldidspace.didspace =
	    CVMX_ADDR_DIDSPACE(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool));
	CVMX_SYNCWS;
	/* value written is number of cache lines not written back */
	cvmx_write_io(newptr.u64, num_cache_lines);
}

extern int cvmx_fpa_setup_pool(uint64_t pool, const char *name, void *buffer,
			       uint64_t block_size, uint64_t num_blocks);

extern uint64_t cvmx_fpa_shutdown_pool(uint64_t pool);

uint64_t cvmx_fpa_get_block_size(uint64_t pool);

#endif 
