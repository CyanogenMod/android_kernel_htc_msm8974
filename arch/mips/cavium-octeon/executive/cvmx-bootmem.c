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


#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-spinlock.h>
#include <asm/octeon/cvmx-bootmem.h>



static struct cvmx_bootmem_desc *cvmx_bootmem_desc;


#define NEXT_OFFSET 0
#define SIZE_OFFSET 8

static void cvmx_bootmem_phy_set_size(uint64_t addr, uint64_t size)
{
	cvmx_write64_uint64((addr + SIZE_OFFSET) | (1ull << 63), size);
}

static void cvmx_bootmem_phy_set_next(uint64_t addr, uint64_t next)
{
	cvmx_write64_uint64((addr + NEXT_OFFSET) | (1ull << 63), next);
}

static uint64_t cvmx_bootmem_phy_get_size(uint64_t addr)
{
	return cvmx_read64_uint64((addr + SIZE_OFFSET) | (1ull << 63));
}

static uint64_t cvmx_bootmem_phy_get_next(uint64_t addr)
{
	return cvmx_read64_uint64((addr + NEXT_OFFSET) | (1ull << 63));
}

void *cvmx_bootmem_alloc_range(uint64_t size, uint64_t alignment,
			       uint64_t min_addr, uint64_t max_addr)
{
	int64_t address;
	address =
	    cvmx_bootmem_phy_alloc(size, min_addr, max_addr, alignment, 0);

	if (address > 0)
		return cvmx_phys_to_ptr(address);
	else
		return NULL;
}

void *cvmx_bootmem_alloc_address(uint64_t size, uint64_t address,
				 uint64_t alignment)
{
	return cvmx_bootmem_alloc_range(size, alignment, address,
					address + size);
}

void *cvmx_bootmem_alloc(uint64_t size, uint64_t alignment)
{
	return cvmx_bootmem_alloc_range(size, alignment, 0, 0);
}

void *cvmx_bootmem_alloc_named_range(uint64_t size, uint64_t min_addr,
				     uint64_t max_addr, uint64_t align,
				     char *name)
{
	int64_t addr;

	addr = cvmx_bootmem_phy_named_block_alloc(size, min_addr, max_addr,
						  align, name, 0);
	if (addr >= 0)
		return cvmx_phys_to_ptr(addr);
	else
		return NULL;
}

void *cvmx_bootmem_alloc_named_address(uint64_t size, uint64_t address,
				       char *name)
{
    return cvmx_bootmem_alloc_named_range(size, address, address + size,
					  0, name);
}

void *cvmx_bootmem_alloc_named(uint64_t size, uint64_t alignment, char *name)
{
    return cvmx_bootmem_alloc_named_range(size, 0, 0, alignment, name);
}
EXPORT_SYMBOL(cvmx_bootmem_alloc_named);

int cvmx_bootmem_free_named(char *name)
{
	return cvmx_bootmem_phy_named_block_free(name, 0);
}

struct cvmx_bootmem_named_block_desc *cvmx_bootmem_find_named_block(char *name)
{
	return cvmx_bootmem_phy_named_block_find(name, 0);
}
EXPORT_SYMBOL(cvmx_bootmem_find_named_block);

void cvmx_bootmem_lock(void)
{
	cvmx_spinlock_lock((cvmx_spinlock_t *) &(cvmx_bootmem_desc->lock));
}

void cvmx_bootmem_unlock(void)
{
	cvmx_spinlock_unlock((cvmx_spinlock_t *) &(cvmx_bootmem_desc->lock));
}

int cvmx_bootmem_init(void *mem_desc_ptr)
{
	if (!cvmx_bootmem_desc) {
#if   defined(CVMX_ABI_64)
		
		cvmx_bootmem_desc = cvmx_phys_to_ptr(CAST64(mem_desc_ptr));
#else
		cvmx_bootmem_desc = (struct cvmx_bootmem_desc *) mem_desc_ptr;
#endif
	}

	return 0;
}


int64_t cvmx_bootmem_phy_alloc(uint64_t req_size, uint64_t address_min,
			       uint64_t address_max, uint64_t alignment,
			       uint32_t flags)
{

	uint64_t head_addr;
	uint64_t ent_addr;
	
	uint64_t prev_addr = 0;
	uint64_t new_ent_addr = 0;
	uint64_t desired_min_addr;

#ifdef DEBUG
	cvmx_dprintf("cvmx_bootmem_phy_alloc: req_size: 0x%llx, "
		     "min_addr: 0x%llx, max_addr: 0x%llx, align: 0x%llx\n",
		     (unsigned long long)req_size,
		     (unsigned long long)address_min,
		     (unsigned long long)address_max,
		     (unsigned long long)alignment);
#endif

	if (cvmx_bootmem_desc->major_version > 3) {
		cvmx_dprintf("ERROR: Incompatible bootmem descriptor "
			     "version: %d.%d at addr: %p\n",
			     (int)cvmx_bootmem_desc->major_version,
			     (int)cvmx_bootmem_desc->minor_version,
			     cvmx_bootmem_desc);
		goto error_out;
	}


	
	if (!req_size)
		goto error_out;

	
	req_size = (req_size + (CVMX_BOOTMEM_ALIGNMENT_SIZE - 1)) &
		~(CVMX_BOOTMEM_ALIGNMENT_SIZE - 1);

	if (address_min && !address_max)
		address_max = address_min + req_size;
	else if (!address_min && !address_max)
		address_max = ~0ull;  


	if (alignment < CVMX_BOOTMEM_ALIGNMENT_SIZE)
		alignment = CVMX_BOOTMEM_ALIGNMENT_SIZE;

	if (alignment)
		address_min = ALIGN(address_min, alignment);

	if (req_size > address_max - address_min)
		goto error_out;

	

	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_lock();
	head_addr = cvmx_bootmem_desc->head_addr;
	ent_addr = head_addr;
	for (; ent_addr;
	     prev_addr = ent_addr,
	     ent_addr = cvmx_bootmem_phy_get_next(ent_addr)) {
		uint64_t usable_base, usable_max;
		uint64_t ent_size = cvmx_bootmem_phy_get_size(ent_addr);

		if (cvmx_bootmem_phy_get_next(ent_addr)
		    && ent_addr > cvmx_bootmem_phy_get_next(ent_addr)) {
			cvmx_dprintf("Internal bootmem_alloc() error: ent: "
				"0x%llx, next: 0x%llx\n",
				(unsigned long long)ent_addr,
				(unsigned long long)
				cvmx_bootmem_phy_get_next(ent_addr));
			goto error_out;
		}

		usable_base =
		    ALIGN(max(address_min, ent_addr), alignment);
		usable_max = min(address_max, ent_addr + ent_size);

		desired_min_addr = usable_base;
		if (!((ent_addr + ent_size) > usable_base
				&& ent_addr < address_max
				&& req_size <= usable_max - usable_base))
			continue;
		if (flags & CVMX_BOOTMEM_FLAG_END_ALLOC) {
			desired_min_addr = usable_max - req_size;
			desired_min_addr &= ~(alignment - 1);
		}

		
		if (desired_min_addr == ent_addr) {
			if (req_size < ent_size) {
				new_ent_addr = ent_addr + req_size;
				cvmx_bootmem_phy_set_next(new_ent_addr,
					cvmx_bootmem_phy_get_next(ent_addr));
				cvmx_bootmem_phy_set_size(new_ent_addr,
							ent_size -
							req_size);

				cvmx_bootmem_phy_set_next(ent_addr,
							new_ent_addr);
			}

			if (prev_addr)
				cvmx_bootmem_phy_set_next(prev_addr,
					cvmx_bootmem_phy_get_next(ent_addr));
			else
				cvmx_bootmem_desc->head_addr =
					cvmx_bootmem_phy_get_next(ent_addr);

			if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
				cvmx_bootmem_unlock();
			return desired_min_addr;
		}
		new_ent_addr = desired_min_addr;
		cvmx_bootmem_phy_set_next(new_ent_addr,
					cvmx_bootmem_phy_get_next
					(ent_addr));
		cvmx_bootmem_phy_set_size(new_ent_addr,
					cvmx_bootmem_phy_get_size
					(ent_addr) -
					(desired_min_addr -
						ent_addr));
		cvmx_bootmem_phy_set_size(ent_addr,
					desired_min_addr - ent_addr);
		cvmx_bootmem_phy_set_next(ent_addr, new_ent_addr);
		
	}
error_out:
	
	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_unlock();
	return -1;
}

int __cvmx_bootmem_phy_free(uint64_t phy_addr, uint64_t size, uint32_t flags)
{
	uint64_t cur_addr;
	uint64_t prev_addr = 0;	
	int retval = 0;

#ifdef DEBUG
	cvmx_dprintf("__cvmx_bootmem_phy_free addr: 0x%llx, size: 0x%llx\n",
		     (unsigned long long)phy_addr, (unsigned long long)size);
#endif
	if (cvmx_bootmem_desc->major_version > 3) {
		cvmx_dprintf("ERROR: Incompatible bootmem descriptor "
			     "version: %d.%d at addr: %p\n",
			     (int)cvmx_bootmem_desc->major_version,
			     (int)cvmx_bootmem_desc->minor_version,
			     cvmx_bootmem_desc);
		return 0;
	}

	
	if (!size)
		return 0;

	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_lock();
	cur_addr = cvmx_bootmem_desc->head_addr;
	if (cur_addr == 0 || phy_addr < cur_addr) {
		
		if (cur_addr && phy_addr + size > cur_addr)
			goto bootmem_free_done;	
		else if (phy_addr + size == cur_addr) {
			
			cvmx_bootmem_phy_set_next(phy_addr,
						  cvmx_bootmem_phy_get_next
						  (cur_addr));
			cvmx_bootmem_phy_set_size(phy_addr,
						  cvmx_bootmem_phy_get_size
						  (cur_addr) + size);
			cvmx_bootmem_desc->head_addr = phy_addr;

		} else {
			
			cvmx_bootmem_phy_set_next(phy_addr, cur_addr);
			cvmx_bootmem_phy_set_size(phy_addr, size);
			cvmx_bootmem_desc->head_addr = phy_addr;
		}
		retval = 1;
		goto bootmem_free_done;
	}

	
	while (cur_addr && phy_addr > cur_addr) {
		prev_addr = cur_addr;
		cur_addr = cvmx_bootmem_phy_get_next(cur_addr);
	}

	if (!cur_addr) {
		if (prev_addr + cvmx_bootmem_phy_get_size(prev_addr) ==
		    phy_addr) {
			cvmx_bootmem_phy_set_size(prev_addr,
						  cvmx_bootmem_phy_get_size
						  (prev_addr) + size);
		} else {
			cvmx_bootmem_phy_set_next(prev_addr, phy_addr);
			cvmx_bootmem_phy_set_size(phy_addr, size);
			cvmx_bootmem_phy_set_next(phy_addr, 0);
		}
		retval = 1;
		goto bootmem_free_done;
	} else {
		if (prev_addr + cvmx_bootmem_phy_get_size(prev_addr) ==
		    phy_addr) {
			
			cvmx_bootmem_phy_set_size(prev_addr,
						  cvmx_bootmem_phy_get_size
						  (prev_addr) + size);
			if (phy_addr + size == cur_addr) {
				
				cvmx_bootmem_phy_set_size(prev_addr,
					cvmx_bootmem_phy_get_size(cur_addr) +
					cvmx_bootmem_phy_get_size(prev_addr));
				cvmx_bootmem_phy_set_next(prev_addr,
					cvmx_bootmem_phy_get_next(cur_addr));
			}
			retval = 1;
			goto bootmem_free_done;
		} else if (phy_addr + size == cur_addr) {
			
			cvmx_bootmem_phy_set_size(phy_addr,
						  cvmx_bootmem_phy_get_size
						  (cur_addr) + size);
			cvmx_bootmem_phy_set_next(phy_addr,
						  cvmx_bootmem_phy_get_next
						  (cur_addr));
			cvmx_bootmem_phy_set_next(prev_addr, phy_addr);
			retval = 1;
			goto bootmem_free_done;
		}

		
		cvmx_bootmem_phy_set_size(phy_addr, size);
		cvmx_bootmem_phy_set_next(phy_addr, cur_addr);
		cvmx_bootmem_phy_set_next(prev_addr, phy_addr);

	}
	retval = 1;

bootmem_free_done:
	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_unlock();
	return retval;

}

struct cvmx_bootmem_named_block_desc *
	cvmx_bootmem_phy_named_block_find(char *name, uint32_t flags)
{
	unsigned int i;
	struct cvmx_bootmem_named_block_desc *named_block_array_ptr;

#ifdef DEBUG
	cvmx_dprintf("cvmx_bootmem_phy_named_block_find: %s\n", name);
#endif
	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_lock();

	
	named_block_array_ptr = (struct cvmx_bootmem_named_block_desc *)
	    cvmx_phys_to_ptr(cvmx_bootmem_desc->named_block_array_addr);

#ifdef DEBUG
	cvmx_dprintf
	    ("cvmx_bootmem_phy_named_block_find: named_block_array_ptr: %p\n",
	     named_block_array_ptr);
#endif
	if (cvmx_bootmem_desc->major_version == 3) {
		for (i = 0;
		     i < cvmx_bootmem_desc->named_block_num_blocks; i++) {
			if ((name && named_block_array_ptr[i].size
			     && !strncmp(name, named_block_array_ptr[i].name,
					 cvmx_bootmem_desc->named_block_name_len
					 - 1))
			    || (!name && !named_block_array_ptr[i].size)) {
				if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
					cvmx_bootmem_unlock();

				return &(named_block_array_ptr[i]);
			}
		}
	} else {
		cvmx_dprintf("ERROR: Incompatible bootmem descriptor "
			     "version: %d.%d at addr: %p\n",
			     (int)cvmx_bootmem_desc->major_version,
			     (int)cvmx_bootmem_desc->minor_version,
			     cvmx_bootmem_desc);
	}
	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_bootmem_unlock();

	return NULL;
}

int cvmx_bootmem_phy_named_block_free(char *name, uint32_t flags)
{
	struct cvmx_bootmem_named_block_desc *named_block_ptr;

	if (cvmx_bootmem_desc->major_version != 3) {
		cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: "
			     "%d.%d at addr: %p\n",
			     (int)cvmx_bootmem_desc->major_version,
			     (int)cvmx_bootmem_desc->minor_version,
			     cvmx_bootmem_desc);
		return 0;
	}
#ifdef DEBUG
	cvmx_dprintf("cvmx_bootmem_phy_named_block_free: %s\n", name);
#endif

	cvmx_bootmem_lock();

	named_block_ptr =
	    cvmx_bootmem_phy_named_block_find(name,
					      CVMX_BOOTMEM_FLAG_NO_LOCKING);
	if (named_block_ptr) {
#ifdef DEBUG
		cvmx_dprintf("cvmx_bootmem_phy_named_block_free: "
			     "%s, base: 0x%llx, size: 0x%llx\n",
			     name,
			     (unsigned long long)named_block_ptr->base_addr,
			     (unsigned long long)named_block_ptr->size);
#endif
		__cvmx_bootmem_phy_free(named_block_ptr->base_addr,
					named_block_ptr->size,
					CVMX_BOOTMEM_FLAG_NO_LOCKING);
		named_block_ptr->size = 0;
		
	}

	cvmx_bootmem_unlock();
	return named_block_ptr != NULL;	
}

int64_t cvmx_bootmem_phy_named_block_alloc(uint64_t size, uint64_t min_addr,
					   uint64_t max_addr,
					   uint64_t alignment,
					   char *name,
					   uint32_t flags)
{
	int64_t addr_allocated;
	struct cvmx_bootmem_named_block_desc *named_block_desc_ptr;

#ifdef DEBUG
	cvmx_dprintf("cvmx_bootmem_phy_named_block_alloc: size: 0x%llx, min: "
		     "0x%llx, max: 0x%llx, align: 0x%llx, name: %s\n",
		     (unsigned long long)size,
		     (unsigned long long)min_addr,
		     (unsigned long long)max_addr,
		     (unsigned long long)alignment,
		     name);
#endif
	if (cvmx_bootmem_desc->major_version != 3) {
		cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: "
			     "%d.%d at addr: %p\n",
			     (int)cvmx_bootmem_desc->major_version,
			     (int)cvmx_bootmem_desc->minor_version,
			     cvmx_bootmem_desc);
		return -1;
	}

	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

	
	named_block_desc_ptr =
		cvmx_bootmem_phy_named_block_find(NULL,
						  flags | CVMX_BOOTMEM_FLAG_NO_LOCKING);

	if (cvmx_bootmem_phy_named_block_find(name,
					      flags | CVMX_BOOTMEM_FLAG_NO_LOCKING) || !named_block_desc_ptr) {
		if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
			cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
		return -1;
	}


	size = ALIGN(size, CVMX_BOOTMEM_ALIGNMENT_SIZE);

	addr_allocated = cvmx_bootmem_phy_alloc(size, min_addr, max_addr,
						alignment,
						flags | CVMX_BOOTMEM_FLAG_NO_LOCKING);
	if (addr_allocated >= 0) {
		named_block_desc_ptr->base_addr = addr_allocated;
		named_block_desc_ptr->size = size;
		strncpy(named_block_desc_ptr->name, name,
			cvmx_bootmem_desc->named_block_name_len);
		named_block_desc_ptr->name[cvmx_bootmem_desc->named_block_name_len - 1] = 0;
	}

	if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
		cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
	return addr_allocated;
}
