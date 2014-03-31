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


#ifndef __CVMX_BOOTMEM_H__
#define __CVMX_BOOTMEM_H__
#define CVMX_BOOTMEM_NAME_LEN 128

#define CVMX_BOOTMEM_NUM_NAMED_BLOCKS 64

#define CVMX_BOOTMEM_ALIGNMENT_SIZE     (16ull)

#define CVMX_BOOTMEM_FLAG_END_ALLOC    (1 << 0)

#define CVMX_BOOTMEM_FLAG_NO_LOCKING   (1 << 1)

struct cvmx_bootmem_block_header {
	uint64_t next_block_addr;
	uint64_t size;

};

struct cvmx_bootmem_named_block_desc {
	
	uint64_t base_addr;
	uint64_t size;
	
	char name[CVMX_BOOTMEM_NAME_LEN];
};

#define CVMX_BOOTMEM_DESC_MAJ_VER   3

#define CVMX_BOOTMEM_DESC_MIN_VER   0

struct cvmx_bootmem_desc {
	
	uint32_t lock;
	
	uint32_t flags;
	uint64_t head_addr;

	
	uint32_t major_version;

	uint32_t minor_version;

	uint64_t app_data_addr;
	uint64_t app_data_size;

	
	uint32_t named_block_num_blocks;

	
	uint32_t named_block_name_len;
	
	uint64_t named_block_array_addr;

};

extern int cvmx_bootmem_init(void *mem_desc_ptr);

extern void *cvmx_bootmem_alloc(uint64_t size, uint64_t alignment);

extern void *cvmx_bootmem_alloc_address(uint64_t size, uint64_t address,
					uint64_t alignment);

extern void *cvmx_bootmem_alloc_range(uint64_t size, uint64_t alignment,
				      uint64_t min_addr, uint64_t max_addr);



extern void *cvmx_bootmem_alloc_named(uint64_t size, uint64_t alignment,
				      char *name);



extern void *cvmx_bootmem_alloc_named_address(uint64_t size, uint64_t address,
					      char *name);



extern void *cvmx_bootmem_alloc_named_range(uint64_t size, uint64_t min_addr,
					    uint64_t max_addr, uint64_t align,
					    char *name);

extern int cvmx_bootmem_free_named(char *name);

struct cvmx_bootmem_named_block_desc *cvmx_bootmem_find_named_block(char *name);

int64_t cvmx_bootmem_phy_alloc(uint64_t req_size, uint64_t address_min,
			       uint64_t address_max, uint64_t alignment,
			       uint32_t flags);

int64_t cvmx_bootmem_phy_named_block_alloc(uint64_t size, uint64_t min_addr,
					   uint64_t max_addr,
					   uint64_t alignment,
					   char *name, uint32_t flags);

struct cvmx_bootmem_named_block_desc *
cvmx_bootmem_phy_named_block_find(char *name, uint32_t flags);

int cvmx_bootmem_phy_named_block_free(char *name, uint32_t flags);

int __cvmx_bootmem_phy_free(uint64_t phy_addr, uint64_t size, uint32_t flags);

void cvmx_bootmem_lock(void);

void cvmx_bootmem_unlock(void);

#endif 
