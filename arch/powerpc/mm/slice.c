/*
 * address space "slices" (meta-segments) support
 *
 * Copyright (C) 2007 Benjamin Herrenschmidt, IBM Corporation.
 *
 * Based on hugetlb implementation
 *
 * Copyright (C) 2003 David Gibson, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#undef DEBUG

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <asm/mman.h>
#include <asm/mmu.h>
#include <asm/spu.h>

static DEFINE_SPINLOCK(slice_convert_lock);


#ifdef DEBUG
int _slice_debug = 1;

static void slice_print_mask(const char *label, struct slice_mask mask)
{
	char	*p, buf[16 + 3 + 16 + 1];
	int	i;

	if (!_slice_debug)
		return;
	p = buf;
	for (i = 0; i < SLICE_NUM_LOW; i++)
		*(p++) = (mask.low_slices & (1 << i)) ? '1' : '0';
	*(p++) = ' ';
	*(p++) = '-';
	*(p++) = ' ';
	for (i = 0; i < SLICE_NUM_HIGH; i++)
		*(p++) = (mask.high_slices & (1 << i)) ? '1' : '0';
	*(p++) = 0;

	printk(KERN_DEBUG "%s:%s\n", label, buf);
}

#define slice_dbg(fmt...) do { if (_slice_debug) pr_debug(fmt); } while(0)

#else

static void slice_print_mask(const char *label, struct slice_mask mask) {}
#define slice_dbg(fmt...)

#endif

static struct slice_mask slice_range_to_mask(unsigned long start,
					     unsigned long len)
{
	unsigned long end = start + len - 1;
	struct slice_mask ret = { 0, 0 };

	if (start < SLICE_LOW_TOP) {
		unsigned long mend = min(end, SLICE_LOW_TOP);
		unsigned long mstart = min(start, SLICE_LOW_TOP);

		ret.low_slices = (1u << (GET_LOW_SLICE_INDEX(mend) + 1))
			- (1u << GET_LOW_SLICE_INDEX(mstart));
	}

	if ((start + len) > SLICE_LOW_TOP)
		ret.high_slices = (1u << (GET_HIGH_SLICE_INDEX(end) + 1))
			- (1u << GET_HIGH_SLICE_INDEX(start));

	return ret;
}

static int slice_area_is_free(struct mm_struct *mm, unsigned long addr,
			      unsigned long len)
{
	struct vm_area_struct *vma;

	if ((mm->task_size - len) < addr)
		return 0;
	vma = find_vma(mm, addr);
	return (!vma || (addr + len) <= vma->vm_start);
}

static int slice_low_has_vma(struct mm_struct *mm, unsigned long slice)
{
	return !slice_area_is_free(mm, slice << SLICE_LOW_SHIFT,
				   1ul << SLICE_LOW_SHIFT);
}

static int slice_high_has_vma(struct mm_struct *mm, unsigned long slice)
{
	unsigned long start = slice << SLICE_HIGH_SHIFT;
	unsigned long end = start + (1ul << SLICE_HIGH_SHIFT);

	if (start == 0)
		start = SLICE_LOW_TOP;

	return !slice_area_is_free(mm, start, end - start);
}

static struct slice_mask slice_mask_for_free(struct mm_struct *mm)
{
	struct slice_mask ret = { 0, 0 };
	unsigned long i;

	for (i = 0; i < SLICE_NUM_LOW; i++)
		if (!slice_low_has_vma(mm, i))
			ret.low_slices |= 1u << i;

	if (mm->task_size <= SLICE_LOW_TOP)
		return ret;

	for (i = 0; i < SLICE_NUM_HIGH; i++)
		if (!slice_high_has_vma(mm, i))
			ret.high_slices |= 1u << i;

	return ret;
}

static struct slice_mask slice_mask_for_size(struct mm_struct *mm, int psize)
{
	struct slice_mask ret = { 0, 0 };
	unsigned long i;
	u64 psizes;

	psizes = mm->context.low_slices_psize;
	for (i = 0; i < SLICE_NUM_LOW; i++)
		if (((psizes >> (i * 4)) & 0xf) == psize)
			ret.low_slices |= 1u << i;

	psizes = mm->context.high_slices_psize;
	for (i = 0; i < SLICE_NUM_HIGH; i++)
		if (((psizes >> (i * 4)) & 0xf) == psize)
			ret.high_slices |= 1u << i;

	return ret;
}

static int slice_check_fit(struct slice_mask mask, struct slice_mask available)
{
	return (mask.low_slices & available.low_slices) == mask.low_slices &&
		(mask.high_slices & available.high_slices) == mask.high_slices;
}

static void slice_flush_segments(void *parm)
{
	struct mm_struct *mm = parm;
	unsigned long flags;

	if (mm != current->active_mm)
		return;

	
	get_paca()->context = current->active_mm->context;

	local_irq_save(flags);
	slb_flush_and_rebolt();
	local_irq_restore(flags);
}

static void slice_convert(struct mm_struct *mm, struct slice_mask mask, int psize)
{
	
	u64 lpsizes, hpsizes;
	unsigned long i, flags;

	slice_dbg("slice_convert(mm=%p, psize=%d)\n", mm, psize);
	slice_print_mask(" mask", mask);

	spin_lock_irqsave(&slice_convert_lock, flags);

	lpsizes = mm->context.low_slices_psize;
	for (i = 0; i < SLICE_NUM_LOW; i++)
		if (mask.low_slices & (1u << i))
			lpsizes = (lpsizes & ~(0xful << (i * 4))) |
				(((unsigned long)psize) << (i * 4));

	hpsizes = mm->context.high_slices_psize;
	for (i = 0; i < SLICE_NUM_HIGH; i++)
		if (mask.high_slices & (1u << i))
			hpsizes = (hpsizes & ~(0xful << (i * 4))) |
				(((unsigned long)psize) << (i * 4));

	mm->context.low_slices_psize = lpsizes;
	mm->context.high_slices_psize = hpsizes;

	slice_dbg(" lsps=%lx, hsps=%lx\n",
		  mm->context.low_slices_psize,
		  mm->context.high_slices_psize);

	spin_unlock_irqrestore(&slice_convert_lock, flags);

#ifdef CONFIG_SPU_BASE
	spu_flush_all_slbs(mm);
#endif
}

static unsigned long slice_find_area_bottomup(struct mm_struct *mm,
					      unsigned long len,
					      struct slice_mask available,
					      int psize, int use_cache)
{
	struct vm_area_struct *vma;
	unsigned long start_addr, addr;
	struct slice_mask mask;
	int pshift = max_t(int, mmu_psize_defs[psize].shift, PAGE_SHIFT);

	if (use_cache) {
		if (len <= mm->cached_hole_size) {
			start_addr = addr = TASK_UNMAPPED_BASE;
			mm->cached_hole_size = 0;
		} else
			start_addr = addr = mm->free_area_cache;
	} else
		start_addr = addr = TASK_UNMAPPED_BASE;

full_search:
	for (;;) {
		addr = _ALIGN_UP(addr, 1ul << pshift);
		if ((TASK_SIZE - len) < addr)
			break;
		vma = find_vma(mm, addr);
		BUG_ON(vma && (addr >= vma->vm_end));

		mask = slice_range_to_mask(addr, len);
		if (!slice_check_fit(mask, available)) {
			if (addr < SLICE_LOW_TOP)
				addr = _ALIGN_UP(addr + 1,  1ul << SLICE_LOW_SHIFT);
			else
				addr = _ALIGN_UP(addr + 1,  1ul << SLICE_HIGH_SHIFT);
			continue;
		}
		if (!vma || addr + len <= vma->vm_start) {
			if (use_cache)
				mm->free_area_cache = addr + len;
			return addr;
		}
		if (use_cache && (addr + mm->cached_hole_size) < vma->vm_start)
		        mm->cached_hole_size = vma->vm_start - addr;
		addr = vma->vm_end;
	}

	
	if (use_cache && start_addr != TASK_UNMAPPED_BASE) {
		start_addr = addr = TASK_UNMAPPED_BASE;
		mm->cached_hole_size = 0;
		goto full_search;
	}
	return -ENOMEM;
}

static unsigned long slice_find_area_topdown(struct mm_struct *mm,
					     unsigned long len,
					     struct slice_mask available,
					     int psize, int use_cache)
{
	struct vm_area_struct *vma;
	unsigned long addr;
	struct slice_mask mask;
	int pshift = max_t(int, mmu_psize_defs[psize].shift, PAGE_SHIFT);

	
	if (use_cache) {
		if (len <= mm->cached_hole_size) {
			mm->cached_hole_size = 0;
			mm->free_area_cache = mm->mmap_base;
		}

		addr = mm->free_area_cache;

		
		if (addr > len) {
			addr = _ALIGN_DOWN(addr - len, 1ul << pshift);
			mask = slice_range_to_mask(addr, len);
			if (slice_check_fit(mask, available) &&
			    slice_area_is_free(mm, addr, len))
					return (mm->free_area_cache = addr);
		}
	}

	addr = mm->mmap_base;
	while (addr > len) {
		
		addr = _ALIGN_DOWN(addr - len, 1ul << pshift);

		
		mask = slice_range_to_mask(addr, len);
		if (!slice_check_fit(mask, available)) {
			if (addr < SLICE_LOW_TOP)
				addr = _ALIGN_DOWN(addr, 1ul << SLICE_LOW_SHIFT);
			else if (addr < (1ul << SLICE_HIGH_SHIFT))
				addr = SLICE_LOW_TOP;
			else
				addr = _ALIGN_DOWN(addr, 1ul << SLICE_HIGH_SHIFT);
			continue;
		}

		vma = find_vma(mm, addr);
		if (!vma || (addr + len) <= vma->vm_start) {
			
			if (use_cache)
				mm->free_area_cache = addr;
			return addr;
		}

		
		if (use_cache && (addr + mm->cached_hole_size) < vma->vm_start)
		        mm->cached_hole_size = vma->vm_start - addr;

		
		addr = vma->vm_start;
	}

	addr = slice_find_area_bottomup(mm, len, available, psize, 0);

	if (use_cache) {
		mm->free_area_cache = mm->mmap_base;
		mm->cached_hole_size = ~0UL;
	}

	return addr;
}


static unsigned long slice_find_area(struct mm_struct *mm, unsigned long len,
				     struct slice_mask mask, int psize,
				     int topdown, int use_cache)
{
	if (topdown)
		return slice_find_area_topdown(mm, len, mask, psize, use_cache);
	else
		return slice_find_area_bottomup(mm, len, mask, psize, use_cache);
}

#define or_mask(dst, src)	do {			\
	(dst).low_slices |= (src).low_slices;		\
	(dst).high_slices |= (src).high_slices;		\
} while (0)

#define andnot_mask(dst, src)	do {			\
	(dst).low_slices &= ~(src).low_slices;		\
	(dst).high_slices &= ~(src).high_slices;	\
} while (0)

#ifdef CONFIG_PPC_64K_PAGES
#define MMU_PAGE_BASE	MMU_PAGE_64K
#else
#define MMU_PAGE_BASE	MMU_PAGE_4K
#endif

unsigned long slice_get_unmapped_area(unsigned long addr, unsigned long len,
				      unsigned long flags, unsigned int psize,
				      int topdown, int use_cache)
{
	struct slice_mask mask = {0, 0};
	struct slice_mask good_mask;
	struct slice_mask potential_mask = {0,0} ;
	struct slice_mask compat_mask = {0, 0};
	int fixed = (flags & MAP_FIXED);
	int pshift = max_t(int, mmu_psize_defs[psize].shift, PAGE_SHIFT);
	struct mm_struct *mm = current->mm;
	unsigned long newaddr;

	
	BUG_ON(mm->task_size == 0);

	slice_dbg("slice_get_unmapped_area(mm=%p, psize=%d...\n", mm, psize);
	slice_dbg(" addr=%lx, len=%lx, flags=%lx, topdown=%d, use_cache=%d\n",
		  addr, len, flags, topdown, use_cache);

	if (len > mm->task_size)
		return -ENOMEM;
	if (len & ((1ul << pshift) - 1))
		return -EINVAL;
	if (fixed && (addr & ((1ul << pshift) - 1)))
		return -EINVAL;
	if (fixed && addr > (mm->task_size - len))
		return -EINVAL;

	
	if (!fixed && addr) {
		addr = _ALIGN_UP(addr, 1ul << pshift);
		slice_dbg(" aligned addr=%lx\n", addr);
		
		if (addr > mm->task_size - len ||
		    !slice_area_is_free(mm, addr, len))
			addr = 0;
	}

	good_mask = slice_mask_for_size(mm, psize);
	slice_print_mask(" good_mask", good_mask);


#ifdef CONFIG_PPC_64K_PAGES
	
	if (psize == MMU_PAGE_64K) {
		compat_mask = slice_mask_for_size(mm, MMU_PAGE_4K);
		if (fixed)
			or_mask(good_mask, compat_mask);
	}
#endif

	
	if (addr != 0 || fixed) {
		
		mask = slice_range_to_mask(addr, len);
		slice_print_mask(" mask", mask);

		if (slice_check_fit(mask, good_mask)) {
			slice_dbg(" fits good !\n");
			return addr;
		}
	} else {
		newaddr = slice_find_area(mm, len, good_mask, psize, topdown,
					  use_cache);
		if (newaddr != -ENOMEM) {
			slice_dbg(" found area at 0x%lx\n", newaddr);
			return newaddr;
		}
	}

	potential_mask = slice_mask_for_free(mm);
	or_mask(potential_mask, good_mask);
	slice_print_mask(" potential", potential_mask);

	if ((addr != 0 || fixed) && slice_check_fit(mask, potential_mask)) {
		slice_dbg(" fits potential !\n");
		goto convert;
	}

	
	if (fixed)
		return -EBUSY;

	slice_dbg(" search...\n");

	if (addr) {
		addr = slice_find_area(mm, len, good_mask, psize, topdown,
				       use_cache);
		if (addr != -ENOMEM) {
			slice_dbg(" found area at 0x%lx\n", addr);
			return addr;
		}
	}

	addr = slice_find_area(mm, len, potential_mask, psize, topdown,
			       use_cache);

#ifdef CONFIG_PPC_64K_PAGES
	if (addr == -ENOMEM && psize == MMU_PAGE_64K) {
		
		or_mask(potential_mask, compat_mask);
		addr = slice_find_area(mm, len, potential_mask, psize,
				       topdown, use_cache);
	}
#endif

	if (addr == -ENOMEM)
		return -ENOMEM;

	mask = slice_range_to_mask(addr, len);
	slice_dbg(" found potential area at 0x%lx\n", addr);
	slice_print_mask(" mask", mask);

 convert:
	andnot_mask(mask, good_mask);
	andnot_mask(mask, compat_mask);
	if (mask.low_slices || mask.high_slices) {
		slice_convert(mm, mask, psize);
		if (psize > MMU_PAGE_BASE)
			on_each_cpu(slice_flush_segments, mm, 1);
	}
	return addr;

}
EXPORT_SYMBOL_GPL(slice_get_unmapped_area);

unsigned long arch_get_unmapped_area(struct file *filp,
				     unsigned long addr,
				     unsigned long len,
				     unsigned long pgoff,
				     unsigned long flags)
{
	return slice_get_unmapped_area(addr, len, flags,
				       current->mm->context.user_psize,
				       0, 1);
}

unsigned long arch_get_unmapped_area_topdown(struct file *filp,
					     const unsigned long addr0,
					     const unsigned long len,
					     const unsigned long pgoff,
					     const unsigned long flags)
{
	return slice_get_unmapped_area(addr0, len, flags,
				       current->mm->context.user_psize,
				       1, 1);
}

unsigned int get_slice_psize(struct mm_struct *mm, unsigned long addr)
{
	u64 psizes;
	int index;

	if (addr < SLICE_LOW_TOP) {
		psizes = mm->context.low_slices_psize;
		index = GET_LOW_SLICE_INDEX(addr);
	} else {
		psizes = mm->context.high_slices_psize;
		index = GET_HIGH_SLICE_INDEX(addr);
	}

	return (psizes >> (index * 4)) & 0xf;
}
EXPORT_SYMBOL_GPL(get_slice_psize);

void slice_set_user_psize(struct mm_struct *mm, unsigned int psize)
{
	unsigned long flags, lpsizes, hpsizes;
	unsigned int old_psize;
	int i;

	slice_dbg("slice_set_user_psize(mm=%p, psize=%d)\n", mm, psize);

	spin_lock_irqsave(&slice_convert_lock, flags);

	old_psize = mm->context.user_psize;
	slice_dbg(" old_psize=%d\n", old_psize);
	if (old_psize == psize)
		goto bail;

	mm->context.user_psize = psize;
	wmb();

	lpsizes = mm->context.low_slices_psize;
	for (i = 0; i < SLICE_NUM_LOW; i++)
		if (((lpsizes >> (i * 4)) & 0xf) == old_psize)
			lpsizes = (lpsizes & ~(0xful << (i * 4))) |
				(((unsigned long)psize) << (i * 4));

	hpsizes = mm->context.high_slices_psize;
	for (i = 0; i < SLICE_NUM_HIGH; i++)
		if (((hpsizes >> (i * 4)) & 0xf) == old_psize)
			hpsizes = (hpsizes & ~(0xful << (i * 4))) |
				(((unsigned long)psize) << (i * 4));

	mm->context.low_slices_psize = lpsizes;
	mm->context.high_slices_psize = hpsizes;

	slice_dbg(" lsps=%lx, hsps=%lx\n",
		  mm->context.low_slices_psize,
		  mm->context.high_slices_psize);

 bail:
	spin_unlock_irqrestore(&slice_convert_lock, flags);
}

void slice_set_psize(struct mm_struct *mm, unsigned long address,
		     unsigned int psize)
{
	unsigned long i, flags;
	u64 *p;

	spin_lock_irqsave(&slice_convert_lock, flags);
	if (address < SLICE_LOW_TOP) {
		i = GET_LOW_SLICE_INDEX(address);
		p = &mm->context.low_slices_psize;
	} else {
		i = GET_HIGH_SLICE_INDEX(address);
		p = &mm->context.high_slices_psize;
	}
	*p = (*p & ~(0xful << (i * 4))) | ((unsigned long) psize << (i * 4));
	spin_unlock_irqrestore(&slice_convert_lock, flags);

#ifdef CONFIG_SPU_BASE
	spu_flush_all_slbs(mm);
#endif
}

void slice_set_range_psize(struct mm_struct *mm, unsigned long start,
			   unsigned long len, unsigned int psize)
{
	struct slice_mask mask = slice_range_to_mask(start, len);

	slice_convert(mm, mask, psize);
}

int is_hugepage_only_range(struct mm_struct *mm, unsigned long addr,
			   unsigned long len)
{
	struct slice_mask mask, available;
	unsigned int psize = mm->context.user_psize;

	mask = slice_range_to_mask(addr, len);
	available = slice_mask_for_size(mm, psize);
#ifdef CONFIG_PPC_64K_PAGES
	
	if (psize == MMU_PAGE_64K) {
		struct slice_mask compat_mask;
		compat_mask = slice_mask_for_size(mm, MMU_PAGE_4K);
		or_mask(available, compat_mask);
	}
#endif

#if 0 
	slice_dbg("is_hugepage_only_range(mm=%p, addr=%lx, len=%lx)\n",
		 mm, addr, len);
	slice_print_mask(" mask", mask);
	slice_print_mask(" available", available);
#endif
	return !slice_check_fit(mask, available);
}

