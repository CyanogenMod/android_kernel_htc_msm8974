/*
**  IA64 System Bus Adapter (SBA) I/O MMU manager
**
**	(c) Copyright 2002-2005 Alex Williamson
**	(c) Copyright 2002-2003 Grant Grundler
**	(c) Copyright 2002-2005 Hewlett-Packard Company
**
**	Portions (c) 2000 Grant Grundler (from parisc I/O MMU code)
**	Portions (c) 1999 Dave S. Miller (from sparc64 I/O MMU code)
**
**	This program is free software; you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
**      (at your option) any later version.
**
**
** This module initializes the IOC (I/O Controller) found on HP
** McKinley machines and their successors.
**
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <linux/nodemask.h>
#include <linux/bitops.h>         
#include <linux/crash_dump.h>
#include <linux/iommu-helper.h>
#include <linux/dma-mapping.h>
#include <linux/prefetch.h>

#include <asm/delay.h>		
#include <asm/io.h>
#include <asm/page.h>		
#include <asm/dma.h>

#include <asm/acpi-ext.h>

extern int swiotlb_late_init_with_default_size (size_t size);

#define PFX "IOC: "

#undef PDIR_SEARCH_TIMING

#define ALLOW_IOV_BYPASS

#undef ALLOW_IOV_BYPASS_SG

#undef FULL_VALID_PDIR

#define ENABLE_MARK_CLEAN

#undef DEBUG_SBA_INIT
#undef DEBUG_SBA_RUN
#undef DEBUG_SBA_RUN_SG
#undef DEBUG_SBA_RESOURCE
#undef ASSERT_PDIR_SANITY
#undef DEBUG_LARGE_SG_ENTRIES
#undef DEBUG_BYPASS

#if defined(FULL_VALID_PDIR) && defined(ASSERT_PDIR_SANITY)
#error FULL_VALID_PDIR and ASSERT_PDIR_SANITY are mutually exclusive
#endif

#define SBA_INLINE	__inline__

#ifdef DEBUG_SBA_INIT
#define DBG_INIT(x...)	printk(x)
#else
#define DBG_INIT(x...)
#endif

#ifdef DEBUG_SBA_RUN
#define DBG_RUN(x...)	printk(x)
#else
#define DBG_RUN(x...)
#endif

#ifdef DEBUG_SBA_RUN_SG
#define DBG_RUN_SG(x...)	printk(x)
#else
#define DBG_RUN_SG(x...)
#endif


#ifdef DEBUG_SBA_RESOURCE
#define DBG_RES(x...)	printk(x)
#else
#define DBG_RES(x...)
#endif

#ifdef DEBUG_BYPASS
#define DBG_BYPASS(x...)	printk(x)
#else
#define DBG_BYPASS(x...)
#endif

#ifdef ASSERT_PDIR_SANITY
#define ASSERT(expr) \
        if(!(expr)) { \
                printk( "\n" __FILE__ ":%d: Assertion " #expr " failed!\n",__LINE__); \
                panic(#expr); \
        }
#else
#define ASSERT(expr)
#endif

#define DELAYED_RESOURCE_CNT	64

#define PCI_DEVICE_ID_HP_SX2000_IOC	0x12ec

#define ZX1_IOC_ID	((PCI_DEVICE_ID_HP_ZX1_IOC << 16) | PCI_VENDOR_ID_HP)
#define ZX2_IOC_ID	((PCI_DEVICE_ID_HP_ZX2_IOC << 16) | PCI_VENDOR_ID_HP)
#define REO_IOC_ID	((PCI_DEVICE_ID_HP_REO_IOC << 16) | PCI_VENDOR_ID_HP)
#define SX1000_IOC_ID	((PCI_DEVICE_ID_HP_SX1000_IOC << 16) | PCI_VENDOR_ID_HP)
#define SX2000_IOC_ID	((PCI_DEVICE_ID_HP_SX2000_IOC << 16) | PCI_VENDOR_ID_HP)

#define ZX1_IOC_OFFSET	0x1000	

#define IOC_FUNC_ID	0x000
#define IOC_FCLASS	0x008	
#define IOC_IBASE	0x300	
#define IOC_IMASK	0x308
#define IOC_PCOM	0x310
#define IOC_TCNFG	0x318
#define IOC_PDIR_BASE	0x320

#define IOC_ROPE0_CFG	0x500
#define   IOC_ROPE_AO	  0x10	


#define ZX1_SBA_IOMMU_COOKIE	0x0000badbadc0ffeeUL

static unsigned long iovp_size;
static unsigned long iovp_shift;
static unsigned long iovp_mask;

struct ioc {
	void __iomem	*ioc_hpa;	
	char		*res_map;	
	u64		*pdir_base;	
	unsigned long	ibase;		
	unsigned long	imask;		

	unsigned long	*res_hint;	
	unsigned long	dma_mask;
	spinlock_t	res_lock;	
					
	unsigned int	res_bitshift;	
	unsigned int	res_size;	
#ifdef CONFIG_NUMA
	unsigned int	node;		
#endif
#if DELAYED_RESOURCE_CNT > 0
	spinlock_t	saved_lock;	
					
	int		saved_cnt;
	struct sba_dma_pair {
		dma_addr_t	iova;
		size_t		size;
	} saved[DELAYED_RESOURCE_CNT];
#endif

#ifdef PDIR_SEARCH_TIMING
#define SBA_SEARCH_SAMPLE	0x100
	unsigned long avg_search[SBA_SEARCH_SAMPLE];
	unsigned long avg_idx;	
#endif

	
	struct ioc	*next;		
	acpi_handle	handle;		
	const char 	*name;
	unsigned int	func_id;
	unsigned int	rev;		
	u32		iov_size;
	unsigned int	pdir_size;	
	struct pci_dev	*sac_only_dev;
};

static struct ioc *ioc_list;
static int reserve_sba_gart = 1;

static SBA_INLINE void sba_mark_invalid(struct ioc *, dma_addr_t, size_t);
static SBA_INLINE void sba_free_range(struct ioc *, dma_addr_t, size_t);

#define sba_sg_address(sg)	sg_virt((sg))

#ifdef FULL_VALID_PDIR
static u64 prefetch_spill_page;
#endif

#ifdef CONFIG_PCI
# define GET_IOC(dev)	(((dev)->bus == &pci_bus_type)						\
			 ? ((struct ioc *) PCI_CONTROLLER(to_pci_dev(dev))->iommu) : NULL)
#else
# define GET_IOC(dev)	NULL
#endif

#define DMA_CHUNK_SIZE  (BITS_PER_LONG*iovp_size)

#define ROUNDUP(x,y) ((x + ((y)-1)) & ~((y)-1))

#define READ_REG(addr)       __raw_readq(addr)
#define WRITE_REG(val, addr) __raw_writeq(val, addr)

#ifdef DEBUG_SBA_INIT

static void
sba_dump_tlb(char *hpa)
{
	DBG_INIT("IO TLB at 0x%p\n", (void *)hpa);
	DBG_INIT("IOC_IBASE    : %016lx\n", READ_REG(hpa+IOC_IBASE));
	DBG_INIT("IOC_IMASK    : %016lx\n", READ_REG(hpa+IOC_IMASK));
	DBG_INIT("IOC_TCNFG    : %016lx\n", READ_REG(hpa+IOC_TCNFG));
	DBG_INIT("IOC_PDIR_BASE: %016lx\n", READ_REG(hpa+IOC_PDIR_BASE));
	DBG_INIT("\n");
}
#endif


#ifdef ASSERT_PDIR_SANITY

static void
sba_dump_pdir_entry(struct ioc *ioc, char *msg, uint pide)
{
	
	u64 *ptr = &ioc->pdir_base[pide  & ~(BITS_PER_LONG - 1)];
	unsigned long *rptr = (unsigned long *) &ioc->res_map[(pide >>3) & -sizeof(unsigned long)];
	uint rcnt;

	printk(KERN_DEBUG "SBA: %s rp %p bit %d rval 0x%lx\n",
		 msg, rptr, pide & (BITS_PER_LONG - 1), *rptr);

	rcnt = 0;
	while (rcnt < BITS_PER_LONG) {
		printk(KERN_DEBUG "%s %2d %p %016Lx\n",
		       (rcnt == (pide & (BITS_PER_LONG - 1)))
		       ? "    -->" : "       ",
		       rcnt, ptr, (unsigned long long) *ptr );
		rcnt++;
		ptr++;
	}
	printk(KERN_DEBUG "%s", msg);
}


static int
sba_check_pdir(struct ioc *ioc, char *msg)
{
	u64 *rptr_end = (u64 *) &(ioc->res_map[ioc->res_size]);
	u64 *rptr = (u64 *) ioc->res_map;	
	u64 *pptr = ioc->pdir_base;	
	uint pide = 0;

	while (rptr < rptr_end) {
		u64 rval;
		int rcnt; 

		rval = *rptr;
		rcnt = 64;

		while (rcnt) {
			
			u32 pde = ((u32)((*pptr >> (63)) & 0x1));
			if ((rval & 0x1) ^ pde)
			{
				sba_dump_pdir_entry(ioc, msg, pide);
				return(1);
			}
			rcnt--;
			rval >>= 1;	
			pptr++;
			pide++;
		}
		rptr++;	
	}
	
	return 0;
}


static void
sba_dump_sg( struct ioc *ioc, struct scatterlist *startsg, int nents)
{
	while (nents-- > 0) {
		printk(KERN_DEBUG " %d : DMA %08lx/%05x CPU %p\n", nents,
		       startsg->dma_address, startsg->dma_length,
		       sba_sg_address(startsg));
		startsg = sg_next(startsg);
	}
}

static void
sba_check_sg( struct ioc *ioc, struct scatterlist *startsg, int nents)
{
	struct scatterlist *the_sg = startsg;
	int the_nents = nents;

	while (the_nents-- > 0) {
		if (sba_sg_address(the_sg) == 0x0UL)
			sba_dump_sg(NULL, startsg, nents);
		the_sg = sg_next(the_sg);
	}
}

#endif 




#define PAGES_PER_RANGE 1	

#define SBA_IOVA(ioc,iovp,offset) ((ioc->ibase) | (iovp) | (offset))
#define SBA_IOVP(ioc,iova) ((iova) & ~(ioc->ibase))

#define PDIR_ENTRY_SIZE	sizeof(u64)

#define PDIR_INDEX(iovp)   ((iovp)>>iovp_shift)

#define RESMAP_MASK(n)    ~(~0UL << (n))
#define RESMAP_IDX_MASK   (sizeof(unsigned long) - 1)


static SBA_INLINE int
get_iovp_order (unsigned long size)
{
	long double d = size - 1;
	long order;

	order = ia64_getf_exp(d);
	order = order - iovp_shift - 0xffff + 1;
	if (order < 0)
		order = 0;
	return order;
}

static unsigned long ptr_to_pide(struct ioc *ioc, unsigned long *res_ptr,
				 unsigned int bitshiftcnt)
{
	return (((unsigned long)res_ptr - (unsigned long)ioc->res_map) << 3)
		+ bitshiftcnt;
}

static SBA_INLINE unsigned long
sba_search_bitmap(struct ioc *ioc, struct device *dev,
		  unsigned long bits_wanted, int use_hint)
{
	unsigned long *res_ptr;
	unsigned long *res_end = (unsigned long *) &(ioc->res_map[ioc->res_size]);
	unsigned long flags, pide = ~0UL, tpide;
	unsigned long boundary_size;
	unsigned long shift;
	int ret;

	ASSERT(((unsigned long) ioc->res_hint & (sizeof(unsigned long) - 1UL)) == 0);
	ASSERT(res_ptr < res_end);

	boundary_size = (unsigned long long)dma_get_seg_boundary(dev) + 1;
	boundary_size = ALIGN(boundary_size, 1ULL << iovp_shift) >> iovp_shift;

	BUG_ON(ioc->ibase & ~iovp_mask);
	shift = ioc->ibase >> iovp_shift;

	spin_lock_irqsave(&ioc->res_lock, flags);

	
	if (likely(use_hint)) {
		res_ptr = ioc->res_hint;
	} else {
		res_ptr = (ulong *)ioc->res_map;
		ioc->res_bitshift = 0;
	}

	bits_wanted = 1UL << get_iovp_order(bits_wanted << iovp_shift);

	if (likely(bits_wanted == 1)) {
		unsigned int bitshiftcnt;
		for(; res_ptr < res_end ; res_ptr++) {
			if (likely(*res_ptr != ~0UL)) {
				bitshiftcnt = ffz(*res_ptr);
				*res_ptr |= (1UL << bitshiftcnt);
				pide = ptr_to_pide(ioc, res_ptr, bitshiftcnt);
				ioc->res_bitshift = bitshiftcnt + bits_wanted;
				goto found_it;
			}
		}
		goto not_found;

	}
	
	if (likely(bits_wanted <= BITS_PER_LONG/2)) {
		unsigned long o = 1 << get_iovp_order(bits_wanted << iovp_shift);
		uint bitshiftcnt = ROUNDUP(ioc->res_bitshift, o);
		unsigned long mask, base_mask;

		base_mask = RESMAP_MASK(bits_wanted);
		mask = base_mask << bitshiftcnt;

		DBG_RES("%s() o %ld %p", __func__, o, res_ptr);
		for(; res_ptr < res_end ; res_ptr++)
		{ 
			DBG_RES("    %p %lx %lx\n", res_ptr, mask, *res_ptr);
			ASSERT(0 != mask);
			for (; mask ; mask <<= o, bitshiftcnt += o) {
				tpide = ptr_to_pide(ioc, res_ptr, bitshiftcnt);
				ret = iommu_is_span_boundary(tpide, bits_wanted,
							     shift,
							     boundary_size);
				if ((0 == ((*res_ptr) & mask)) && !ret) {
					*res_ptr |= mask;     
					pide = tpide;
					ioc->res_bitshift = bitshiftcnt + bits_wanted;
					goto found_it;
				}
			}

			bitshiftcnt = 0;
			mask = base_mask;

		}

	} else {
		int qwords, bits, i;
		unsigned long *end;

		qwords = bits_wanted >> 6; 
		bits = bits_wanted - (qwords * BITS_PER_LONG);

		end = res_end - qwords;

		for (; res_ptr < end; res_ptr++) {
			tpide = ptr_to_pide(ioc, res_ptr, 0);
			ret = iommu_is_span_boundary(tpide, bits_wanted,
						     shift, boundary_size);
			if (ret)
				goto next_ptr;
			for (i = 0 ; i < qwords ; i++) {
				if (res_ptr[i] != 0)
					goto next_ptr;
			}
			if (bits && res_ptr[i] && (__ffs(res_ptr[i]) < bits))
				continue;

			
			for (i = 0 ; i < qwords ; i++)
				res_ptr[i] = ~0UL;
			res_ptr[i] |= RESMAP_MASK(bits);

			pide = tpide;
			res_ptr += qwords;
			ioc->res_bitshift = bits;
			goto found_it;
next_ptr:
			;
		}
	}

not_found:
	prefetch(ioc->res_map);
	ioc->res_hint = (unsigned long *) ioc->res_map;
	ioc->res_bitshift = 0;
	spin_unlock_irqrestore(&ioc->res_lock, flags);
	return (pide);

found_it:
	ioc->res_hint = res_ptr;
	spin_unlock_irqrestore(&ioc->res_lock, flags);
	return (pide);
}


static int
sba_alloc_range(struct ioc *ioc, struct device *dev, size_t size)
{
	unsigned int pages_needed = size >> iovp_shift;
#ifdef PDIR_SEARCH_TIMING
	unsigned long itc_start;
#endif
	unsigned long pide;

	ASSERT(pages_needed);
	ASSERT(0 == (size & ~iovp_mask));

#ifdef PDIR_SEARCH_TIMING
	itc_start = ia64_get_itc();
#endif
	pide = sba_search_bitmap(ioc, dev, pages_needed, 1);
	if (unlikely(pide >= (ioc->res_size << 3))) {
		pide = sba_search_bitmap(ioc, dev, pages_needed, 0);
		if (unlikely(pide >= (ioc->res_size << 3))) {
#if DELAYED_RESOURCE_CNT > 0
			unsigned long flags;

			spin_lock_irqsave(&ioc->saved_lock, flags);
			if (ioc->saved_cnt > 0) {
				struct sba_dma_pair *d;
				int cnt = ioc->saved_cnt;

				d = &(ioc->saved[ioc->saved_cnt - 1]);

				spin_lock(&ioc->res_lock);
				while (cnt--) {
					sba_mark_invalid(ioc, d->iova, d->size);
					sba_free_range(ioc, d->iova, d->size);
					d--;
				}
				ioc->saved_cnt = 0;
				READ_REG(ioc->ioc_hpa+IOC_PCOM);	
				spin_unlock(&ioc->res_lock);
			}
			spin_unlock_irqrestore(&ioc->saved_lock, flags);

			pide = sba_search_bitmap(ioc, dev, pages_needed, 0);
			if (unlikely(pide >= (ioc->res_size << 3))) {
				printk(KERN_WARNING "%s: I/O MMU @ %p is"
				       "out of mapping resources, %u %u %lx\n",
				       __func__, ioc->ioc_hpa, ioc->res_size,
				       pages_needed, dma_get_seg_boundary(dev));
				return -1;
			}
#else
			printk(KERN_WARNING "%s: I/O MMU @ %p is"
			       "out of mapping resources, %u %u %lx\n",
			       __func__, ioc->ioc_hpa, ioc->res_size,
			       pages_needed, dma_get_seg_boundary(dev));
			return -1;
#endif
		}
	}

#ifdef PDIR_SEARCH_TIMING
	ioc->avg_search[ioc->avg_idx++] = (ia64_get_itc() - itc_start) / pages_needed;
	ioc->avg_idx &= SBA_SEARCH_SAMPLE - 1;
#endif

	prefetchw(&(ioc->pdir_base[pide]));

#ifdef ASSERT_PDIR_SANITY
	
	if(0x00 != ((u8 *) ioc->pdir_base)[pide*PDIR_ENTRY_SIZE + 7]) {
		sba_dump_pdir_entry(ioc, "sba_search_bitmap() botched it?", pide);
	}
#endif

	DBG_RES("%s(%x) %d -> %lx hint %x/%x\n",
		__func__, size, pages_needed, pide,
		(uint) ((unsigned long) ioc->res_hint - (unsigned long) ioc->res_map),
		ioc->res_bitshift );

	return (pide);
}


static SBA_INLINE void
sba_free_range(struct ioc *ioc, dma_addr_t iova, size_t size)
{
	unsigned long iovp = SBA_IOVP(ioc, iova);
	unsigned int pide = PDIR_INDEX(iovp);
	unsigned int ridx = pide >> 3;	
	unsigned long *res_ptr = (unsigned long *) &((ioc)->res_map[ridx & ~RESMAP_IDX_MASK]);
	int bits_not_wanted = size >> iovp_shift;
	unsigned long m;

	
	bits_not_wanted = 1UL << get_iovp_order(bits_not_wanted << iovp_shift);
	for (; bits_not_wanted > 0 ; res_ptr++) {
		
		if (unlikely(bits_not_wanted > BITS_PER_LONG)) {

			
			*res_ptr = 0UL;
			bits_not_wanted -= BITS_PER_LONG;
			pide += BITS_PER_LONG;

		} else {

			
			m = RESMAP_MASK(bits_not_wanted) << (pide & (BITS_PER_LONG - 1));
			bits_not_wanted = 0;

			DBG_RES("%s( ,%x,%x) %x/%lx %x %p %lx\n", __func__, (uint) iova, size,
			        bits_not_wanted, m, pide, res_ptr, *res_ptr);

			ASSERT(m != 0);
			ASSERT(bits_not_wanted);
			ASSERT((*res_ptr & m) == m); 
			*res_ptr &= ~m;
		}
	}
}




#if 1
#define sba_io_pdir_entry(pdir_ptr, vba) *pdir_ptr = ((vba & ~0xE000000000000FFFULL)	\
						      | 0x8000000000000000ULL)
#else
void SBA_INLINE
sba_io_pdir_entry(u64 *pdir_ptr, unsigned long vba)
{
	*pdir_ptr = ((vba & ~0xE000000000000FFFULL) | 0x80000000000000FFULL);
}
#endif

#ifdef ENABLE_MARK_CLEAN
/**
 * Since DMA is i-cache coherent, any (complete) pages that were written via
 * DMA can be marked as "clean" so that lazy_mmu_prot_update() doesn't have to
 * flush them when they get mapped into an executable vm-area.
 */
static void
mark_clean (void *addr, size_t size)
{
	unsigned long pg_addr, end;

	pg_addr = PAGE_ALIGN((unsigned long) addr);
	end = (unsigned long) addr + size;
	while (pg_addr + PAGE_SIZE <= end) {
		struct page *page = virt_to_page((void *)pg_addr);
		set_bit(PG_arch_1, &page->flags);
		pg_addr += PAGE_SIZE;
	}
}
#endif

static SBA_INLINE void
sba_mark_invalid(struct ioc *ioc, dma_addr_t iova, size_t byte_cnt)
{
	u32 iovp = (u32) SBA_IOVP(ioc,iova);

	int off = PDIR_INDEX(iovp);

	
	ASSERT(byte_cnt > 0);
	ASSERT(0 == (byte_cnt & ~iovp_mask));

#ifdef ASSERT_PDIR_SANITY
	
	if (!(ioc->pdir_base[off] >> 60)) {
		sba_dump_pdir_entry(ioc,"sba_mark_invalid()", PDIR_INDEX(iovp));
	}
#endif

	if (byte_cnt <= iovp_size)
	{
		ASSERT(off < ioc->pdir_size);

		iovp |= iovp_shift;     

#ifndef FULL_VALID_PDIR
		ioc->pdir_base[off] &= ~(0x80000000000000FFULL);
#else
		ioc->pdir_base[off] = (0x80000000000000FFULL | prefetch_spill_page);
#endif
	} else {
		u32 t = get_iovp_order(byte_cnt) + iovp_shift;

		iovp |= t;
		ASSERT(t <= 31);   

		do {
			
			ASSERT(ioc->pdir_base[off]  >> 63);
#ifndef FULL_VALID_PDIR
			
			ioc->pdir_base[off] &= ~(0x80000000000000FFULL);
#else
			ioc->pdir_base[off] = (0x80000000000000FFULL | prefetch_spill_page);
#endif
			off++;
			byte_cnt -= iovp_size;
		} while (byte_cnt > 0);
	}

	WRITE_REG(iovp | ioc->ibase, ioc->ioc_hpa+IOC_PCOM);
}

static dma_addr_t sba_map_page(struct device *dev, struct page *page,
			       unsigned long poff, size_t size,
			       enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
	struct ioc *ioc;
	void *addr = page_address(page) + poff;
	dma_addr_t iovp;
	dma_addr_t offset;
	u64 *pdir_start;
	int pide;
#ifdef ASSERT_PDIR_SANITY
	unsigned long flags;
#endif
#ifdef ALLOW_IOV_BYPASS
	unsigned long pci_addr = virt_to_phys(addr);
#endif

#ifdef ALLOW_IOV_BYPASS
	ASSERT(to_pci_dev(dev)->dma_mask);
	if (likely((pci_addr & ~to_pci_dev(dev)->dma_mask) == 0)) {
		DBG_BYPASS("sba_map_single_attrs() bypass mask/addr: "
			   "0x%lx/0x%lx\n",
		           to_pci_dev(dev)->dma_mask, pci_addr);
		return pci_addr;
	}
#endif
	ioc = GET_IOC(dev);
	ASSERT(ioc);

	prefetch(ioc->res_hint);

	ASSERT(size > 0);
	ASSERT(size <= DMA_CHUNK_SIZE);

	
	offset = ((dma_addr_t) (long) addr) & ~iovp_mask;

	
	size = (size + offset + ~iovp_mask) & iovp_mask;

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	if (sba_check_pdir(ioc,"Check before sba_map_single_attrs()"))
		panic("Sanity check failed");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

	pide = sba_alloc_range(ioc, dev, size);
	if (pide < 0)
		return 0;

	iovp = (dma_addr_t) pide << iovp_shift;

	DBG_RUN("%s() 0x%p -> 0x%lx\n", __func__, addr, (long) iovp | offset);

	pdir_start = &(ioc->pdir_base[pide]);

	while (size > 0) {
		ASSERT(((u8 *)pdir_start)[7] == 0); 
		sba_io_pdir_entry(pdir_start, (unsigned long) addr);

		DBG_RUN("     pdir 0x%p %lx\n", pdir_start, *pdir_start);

		addr += iovp_size;
		size -= iovp_size;
		pdir_start++;
	}
	
	wmb();

	
#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_check_pdir(ioc,"Check after sba_map_single_attrs()");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif
	return SBA_IOVA(ioc, iovp, offset);
}

static dma_addr_t sba_map_single_attrs(struct device *dev, void *addr,
				       size_t size, enum dma_data_direction dir,
				       struct dma_attrs *attrs)
{
	return sba_map_page(dev, virt_to_page(addr),
			    (unsigned long)addr & ~PAGE_MASK, size, dir, attrs);
}

#ifdef ENABLE_MARK_CLEAN
static SBA_INLINE void
sba_mark_clean(struct ioc *ioc, dma_addr_t iova, size_t size)
{
	u32	iovp = (u32) SBA_IOVP(ioc,iova);
	int	off = PDIR_INDEX(iovp);
	void	*addr;

	if (size <= iovp_size) {
		addr = phys_to_virt(ioc->pdir_base[off] &
		                    ~0xE000000000000FFFULL);
		mark_clean(addr, size);
	} else {
		do {
			addr = phys_to_virt(ioc->pdir_base[off] &
			                    ~0xE000000000000FFFULL);
			mark_clean(addr, min(size, iovp_size));
			off++;
			size -= iovp_size;
		} while (size > 0);
	}
}
#endif

static void sba_unmap_page(struct device *dev, dma_addr_t iova, size_t size,
			   enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct ioc *ioc;
#if DELAYED_RESOURCE_CNT > 0
	struct sba_dma_pair *d;
#endif
	unsigned long flags;
	dma_addr_t offset;

	ioc = GET_IOC(dev);
	ASSERT(ioc);

#ifdef ALLOW_IOV_BYPASS
	if (likely((iova & ioc->imask) != ioc->ibase)) {
		DBG_BYPASS("sba_unmap_single_attrs() bypass addr: 0x%lx\n",
			   iova);

#ifdef ENABLE_MARK_CLEAN
		if (dir == DMA_FROM_DEVICE) {
			mark_clean(phys_to_virt(iova), size);
		}
#endif
		return;
	}
#endif
	offset = iova & ~iovp_mask;

	DBG_RUN("%s() iovp 0x%lx/%x\n", __func__, (long) iova, size);

	iova ^= offset;        
	size += offset;
	size = ROUNDUP(size, iovp_size);

#ifdef ENABLE_MARK_CLEAN
	if (dir == DMA_FROM_DEVICE)
		sba_mark_clean(ioc, iova, size);
#endif

#if DELAYED_RESOURCE_CNT > 0
	spin_lock_irqsave(&ioc->saved_lock, flags);
	d = &(ioc->saved[ioc->saved_cnt]);
	d->iova = iova;
	d->size = size;
	if (unlikely(++(ioc->saved_cnt) >= DELAYED_RESOURCE_CNT)) {
		int cnt = ioc->saved_cnt;
		spin_lock(&ioc->res_lock);
		while (cnt--) {
			sba_mark_invalid(ioc, d->iova, d->size);
			sba_free_range(ioc, d->iova, d->size);
			d--;
		}
		ioc->saved_cnt = 0;
		READ_REG(ioc->ioc_hpa+IOC_PCOM);	
		spin_unlock(&ioc->res_lock);
	}
	spin_unlock_irqrestore(&ioc->saved_lock, flags);
#else 
	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_mark_invalid(ioc, iova, size);
	sba_free_range(ioc, iova, size);
	READ_REG(ioc->ioc_hpa+IOC_PCOM);	
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif 
}

void sba_unmap_single_attrs(struct device *dev, dma_addr_t iova, size_t size,
			    enum dma_data_direction dir, struct dma_attrs *attrs)
{
	sba_unmap_page(dev, iova, size, dir, attrs);
}

static void *
sba_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
		   gfp_t flags, struct dma_attrs *attrs)
{
	struct ioc *ioc;
	void *addr;

	ioc = GET_IOC(dev);
	ASSERT(ioc);

#ifdef CONFIG_NUMA
	{
		struct page *page;
		page = alloc_pages_exact_node(ioc->node == MAX_NUMNODES ?
		                        numa_node_id() : ioc->node, flags,
		                        get_order(size));

		if (unlikely(!page))
			return NULL;

		addr = page_address(page);
	}
#else
	addr = (void *) __get_free_pages(flags, get_order(size));
#endif
	if (unlikely(!addr))
		return NULL;

	memset(addr, 0, size);
	*dma_handle = virt_to_phys(addr);

#ifdef ALLOW_IOV_BYPASS
	ASSERT(dev->coherent_dma_mask);
	if (likely((*dma_handle & ~dev->coherent_dma_mask) == 0)) {
		DBG_BYPASS("sba_alloc_coherent() bypass mask/addr: 0x%lx/0x%lx\n",
		           dev->coherent_dma_mask, *dma_handle);

		return addr;
	}
#endif

	*dma_handle = sba_map_single_attrs(&ioc->sac_only_dev->dev, addr,
					   size, 0, NULL);

	return addr;
}


static void sba_free_coherent(struct device *dev, size_t size, void *vaddr,
			      dma_addr_t dma_handle, struct dma_attrs *attrs)
{
	sba_unmap_single_attrs(dev, dma_handle, size, 0, NULL);
	free_pages((unsigned long) vaddr, get_order(size));
}


#define PIDE_FLAG 0x1UL

#ifdef DEBUG_LARGE_SG_ENTRIES
int dump_run_sg = 0;
#endif



static SBA_INLINE int
sba_fill_pdir(
	struct ioc *ioc,
	struct scatterlist *startsg,
	int nents)
{
	struct scatterlist *dma_sg = startsg;	
	int n_mappings = 0;
	u64 *pdirp = NULL;
	unsigned long dma_offset = 0;

	while (nents-- > 0) {
		int     cnt = startsg->dma_length;
		startsg->dma_length = 0;

#ifdef DEBUG_LARGE_SG_ENTRIES
		if (dump_run_sg)
			printk(" %2d : %08lx/%05x %p\n",
				nents, startsg->dma_address, cnt,
				sba_sg_address(startsg));
#else
		DBG_RUN_SG(" %d : %08lx/%05x %p\n",
				nents, startsg->dma_address, cnt,
				sba_sg_address(startsg));
#endif
		if (startsg->dma_address & PIDE_FLAG) {
			u32 pide = startsg->dma_address & ~PIDE_FLAG;
			dma_offset = (unsigned long) pide & ~iovp_mask;
			startsg->dma_address = 0;
			if (n_mappings)
				dma_sg = sg_next(dma_sg);
			dma_sg->dma_address = pide | ioc->ibase;
			pdirp = &(ioc->pdir_base[pide >> iovp_shift]);
			n_mappings++;
		}

		if (cnt) {
			unsigned long vaddr = (unsigned long) sba_sg_address(startsg);
			ASSERT(pdirp);

			dma_sg->dma_length += cnt;
			cnt += dma_offset;
			dma_offset=0;	
			cnt = ROUNDUP(cnt, iovp_size);
			do {
				sba_io_pdir_entry(pdirp, vaddr);
				vaddr += iovp_size;
				cnt -= iovp_size;
				pdirp++;
			} while (cnt > 0);
		}
		startsg = sg_next(startsg);
	}
	
	wmb();

#ifdef DEBUG_LARGE_SG_ENTRIES
	dump_run_sg = 0;
#endif
	return(n_mappings);
}


#define DMA_CONTIG(__X, __Y) \
	(((((unsigned long) __X) | ((unsigned long) __Y)) << (BITS_PER_LONG - iovp_shift)) == 0UL)


static SBA_INLINE int
sba_coalesce_chunks(struct ioc *ioc, struct device *dev,
	struct scatterlist *startsg,
	int nents)
{
	struct scatterlist *vcontig_sg;    
	unsigned long vcontig_len;         
	unsigned long vcontig_end;
	struct scatterlist *dma_sg;        
	unsigned long dma_offset, dma_len; 
	int n_mappings = 0;
	unsigned int max_seg_size = dma_get_max_seg_size(dev);
	int idx;

	while (nents > 0) {
		unsigned long vaddr = (unsigned long) sba_sg_address(startsg);

		dma_sg = vcontig_sg = startsg;
		dma_len = vcontig_len = vcontig_end = startsg->length;
		vcontig_end +=  vaddr;
		dma_offset = vaddr & ~iovp_mask;

		
		startsg->dma_address = startsg->dma_length = 0;

		while (--nents > 0) {
			unsigned long vaddr;	

			startsg = sg_next(startsg);

			
			startsg->dma_address = startsg->dma_length = 0;

			
			ASSERT(startsg->length <= DMA_CHUNK_SIZE);

			if (((dma_len + dma_offset + startsg->length + ~iovp_mask) & iovp_mask)
			    > DMA_CHUNK_SIZE)
				break;

			if (dma_len + startsg->length > max_seg_size)
				break;

			vaddr = (unsigned long) sba_sg_address(startsg);
			if  (vcontig_end == vaddr)
			{
				vcontig_len += startsg->length;
				vcontig_end += startsg->length;
				dma_len     += startsg->length;
				continue;
			}

#ifdef DEBUG_LARGE_SG_ENTRIES
			dump_run_sg = (vcontig_len > iovp_size);
#endif

			vcontig_sg->dma_length = vcontig_len;

			vcontig_sg = startsg;
			vcontig_len = startsg->length;

			if (DMA_CONTIG(vcontig_end, vaddr))
			{
				vcontig_end = vcontig_len + vaddr;
				dma_len += vcontig_len;
				continue;
			} else {
				break;
			}
		}

		vcontig_sg->dma_length = vcontig_len;
		dma_len = (dma_len + dma_offset + ~iovp_mask) & iovp_mask;
		ASSERT(dma_len <= DMA_CHUNK_SIZE);
		idx = sba_alloc_range(ioc, dev, dma_len);
		if (idx < 0) {
			dma_sg->dma_length = 0;
			return -1;
		}
		dma_sg->dma_address = (dma_addr_t)(PIDE_FLAG | (idx << iovp_shift)
						   | dma_offset);
		n_mappings++;
	}

	return n_mappings;
}

static void sba_unmap_sg_attrs(struct device *dev, struct scatterlist *sglist,
			       int nents, enum dma_data_direction dir,
			       struct dma_attrs *attrs);
static int sba_map_sg_attrs(struct device *dev, struct scatterlist *sglist,
			    int nents, enum dma_data_direction dir,
			    struct dma_attrs *attrs)
{
	struct ioc *ioc;
	int coalesced, filled = 0;
#ifdef ASSERT_PDIR_SANITY
	unsigned long flags;
#endif
#ifdef ALLOW_IOV_BYPASS_SG
	struct scatterlist *sg;
#endif

	DBG_RUN_SG("%s() START %d entries\n", __func__, nents);
	ioc = GET_IOC(dev);
	ASSERT(ioc);

#ifdef ALLOW_IOV_BYPASS_SG
	ASSERT(to_pci_dev(dev)->dma_mask);
	if (likely((ioc->dma_mask & ~to_pci_dev(dev)->dma_mask) == 0)) {
		for_each_sg(sglist, sg, nents, filled) {
			sg->dma_length = sg->length;
			sg->dma_address = virt_to_phys(sba_sg_address(sg));
		}
		return filled;
	}
#endif
	
	if (nents == 1) {
		sglist->dma_length = sglist->length;
		sglist->dma_address = sba_map_single_attrs(dev, sba_sg_address(sglist), sglist->length, dir, attrs);
		return 1;
	}

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	if (sba_check_pdir(ioc,"Check before sba_map_sg_attrs()"))
	{
		sba_dump_sg(ioc, sglist, nents);
		panic("Check before sba_map_sg_attrs()");
	}
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

	prefetch(ioc->res_hint);

	coalesced = sba_coalesce_chunks(ioc, dev, sglist, nents);
	if (coalesced < 0) {
		sba_unmap_sg_attrs(dev, sglist, nents, dir, attrs);
		return 0;
	}

	filled = sba_fill_pdir(ioc, sglist, nents);

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	if (sba_check_pdir(ioc,"Check after sba_map_sg_attrs()"))
	{
		sba_dump_sg(ioc, sglist, nents);
		panic("Check after sba_map_sg_attrs()\n");
	}
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

	ASSERT(coalesced == filled);
	DBG_RUN_SG("%s() DONE %d mappings\n", __func__, filled);

	return filled;
}

static void sba_unmap_sg_attrs(struct device *dev, struct scatterlist *sglist,
			       int nents, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
#ifdef ASSERT_PDIR_SANITY
	struct ioc *ioc;
	unsigned long flags;
#endif

	DBG_RUN_SG("%s() START %d entries,  %p,%x\n",
		   __func__, nents, sba_sg_address(sglist), sglist->length);

#ifdef ASSERT_PDIR_SANITY
	ioc = GET_IOC(dev);
	ASSERT(ioc);

	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_check_pdir(ioc,"Check before sba_unmap_sg_attrs()");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

	while (nents && sglist->dma_length) {

		sba_unmap_single_attrs(dev, sglist->dma_address,
				       sglist->dma_length, dir, attrs);
		sglist = sg_next(sglist);
		nents--;
	}

	DBG_RUN_SG("%s() DONE (nents %d)\n", __func__,  nents);

#ifdef ASSERT_PDIR_SANITY
	spin_lock_irqsave(&ioc->res_lock, flags);
	sba_check_pdir(ioc,"Check after sba_unmap_sg_attrs()");
	spin_unlock_irqrestore(&ioc->res_lock, flags);
#endif

}


static void __init
ioc_iova_init(struct ioc *ioc)
{
	int tcnfg;
	int agp_found = 0;
	struct pci_dev *device = NULL;
#ifdef FULL_VALID_PDIR
	unsigned long index;
#endif

	ioc->ibase = READ_REG(ioc->ioc_hpa + IOC_IBASE) & ~0x1UL;
	ioc->imask = READ_REG(ioc->ioc_hpa + IOC_IMASK) | 0xFFFFFFFF00000000UL;

	ioc->iov_size = ~ioc->imask + 1;

	DBG_INIT("%s() hpa %p IOV base 0x%lx mask 0x%lx (%dMB)\n",
		__func__, ioc->ioc_hpa, ioc->ibase, ioc->imask,
		ioc->iov_size >> 20);

	switch (iovp_size) {
		case  4*1024: tcnfg = 0; break;
		case  8*1024: tcnfg = 1; break;
		case 16*1024: tcnfg = 2; break;
		case 64*1024: tcnfg = 3; break;
		default:
			panic(PFX "Unsupported IOTLB page size %ldK",
				iovp_size >> 10);
			break;
	}
	WRITE_REG(tcnfg, ioc->ioc_hpa + IOC_TCNFG);

	ioc->pdir_size = (ioc->iov_size / iovp_size) * PDIR_ENTRY_SIZE;
	ioc->pdir_base = (void *) __get_free_pages(GFP_KERNEL,
						   get_order(ioc->pdir_size));
	if (!ioc->pdir_base)
		panic(PFX "Couldn't allocate I/O Page Table\n");

	memset(ioc->pdir_base, 0, ioc->pdir_size);

	DBG_INIT("%s() IOV page size %ldK pdir %p size %x\n", __func__,
		iovp_size >> 10, ioc->pdir_base, ioc->pdir_size);

	ASSERT(ALIGN((unsigned long) ioc->pdir_base, 4*1024) == (unsigned long) ioc->pdir_base);
	WRITE_REG(virt_to_phys(ioc->pdir_base), ioc->ioc_hpa + IOC_PDIR_BASE);

	for_each_pci_dev(device)	
		agp_found |= pci_find_capability(device, PCI_CAP_ID_AGP);

	if (agp_found && reserve_sba_gart) {
		printk(KERN_INFO PFX "reserving %dMb of IOVA space at 0x%lx for agpgart\n",
		      ioc->iov_size/2 >> 20, ioc->ibase + ioc->iov_size/2);
		ioc->pdir_size /= 2;
		((u64 *)ioc->pdir_base)[PDIR_INDEX(ioc->iov_size/2)] = ZX1_SBA_IOMMU_COOKIE;
	}
#ifdef FULL_VALID_PDIR
	if (!prefetch_spill_page) {
		char *spill_poison = "SBAIOMMU POISON";
		int poison_size = 16;
		void *poison_addr, *addr;

		addr = (void *)__get_free_pages(GFP_KERNEL, get_order(iovp_size));
		if (!addr)
			panic(PFX "Couldn't allocate PDIR spill page\n");

		poison_addr = addr;
		for ( ; (u64) poison_addr < addr + iovp_size; poison_addr += poison_size)
			memcpy(poison_addr, spill_poison, poison_size);

		prefetch_spill_page = virt_to_phys(addr);

		DBG_INIT("%s() prefetch spill addr: 0x%lx\n", __func__, prefetch_spill_page);
	}
	for (index = 0 ; index < (ioc->pdir_size / PDIR_ENTRY_SIZE) ; index++)
		((u64 *)ioc->pdir_base)[index] = (0x80000000000000FF | prefetch_spill_page);
#endif

	
	WRITE_REG(ioc->ibase | (get_iovp_order(ioc->iov_size) + iovp_shift), ioc->ioc_hpa + IOC_PCOM);
	READ_REG(ioc->ioc_hpa + IOC_PCOM);

	
	WRITE_REG(ioc->ibase | 1, ioc->ioc_hpa + IOC_IBASE);
	READ_REG(ioc->ioc_hpa + IOC_IBASE);
}

static void __init
ioc_resource_init(struct ioc *ioc)
{
	spin_lock_init(&ioc->res_lock);
#if DELAYED_RESOURCE_CNT > 0
	spin_lock_init(&ioc->saved_lock);
#endif

	
	ioc->res_size = ioc->pdir_size / PDIR_ENTRY_SIZE; 
	ioc->res_size >>= 3;  
	DBG_INIT("%s() res_size 0x%x\n", __func__, ioc->res_size);

	ioc->res_map = (char *) __get_free_pages(GFP_KERNEL,
						 get_order(ioc->res_size));
	if (!ioc->res_map)
		panic(PFX "Couldn't allocate resource map\n");

	memset(ioc->res_map, 0, ioc->res_size);
	
	ioc->res_hint = (unsigned long *) ioc->res_map;

#ifdef ASSERT_PDIR_SANITY
	
	ioc->res_map[0] = 0x1;
	ioc->pdir_base[0] = 0x8000000000000000ULL | ZX1_SBA_IOMMU_COOKIE;
#endif
#ifdef FULL_VALID_PDIR
	
	ioc->res_map[ioc->res_size - 1] |= 0x80UL; 
	ioc->pdir_base[(ioc->pdir_size / PDIR_ENTRY_SIZE) - 1] = (0x80000000000000FF
							      | prefetch_spill_page);
#endif

	DBG_INIT("%s() res_map %x %p\n", __func__,
		 ioc->res_size, (void *) ioc->res_map);
}

static void __init
ioc_sac_init(struct ioc *ioc)
{
	struct pci_dev *sac = NULL;
	struct pci_controller *controller = NULL;

	sac = kzalloc(sizeof(*sac), GFP_KERNEL);
	if (!sac)
		panic(PFX "Couldn't allocate struct pci_dev");

	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller)
		panic(PFX "Couldn't allocate struct pci_controller");

	controller->iommu = ioc;
	sac->sysdata = controller;
	sac->dma_mask = 0xFFFFFFFFUL;
#ifdef CONFIG_PCI
	sac->dev.bus = &pci_bus_type;
#endif
	ioc->sac_only_dev = sac;
}

static void __init
ioc_zx1_init(struct ioc *ioc)
{
	unsigned long rope_config;
	unsigned int i;

	if (ioc->rev < 0x20)
		panic(PFX "IOC 2.0 or later required for IOMMU support\n");

	
	ioc->dma_mask = (0x1UL << 39) - 1;

	for (i=0; i<(8*8); i+=8) {
		rope_config = READ_REG(ioc->ioc_hpa + IOC_ROPE0_CFG + i);
		rope_config &= ~IOC_ROPE_AO;
		WRITE_REG(rope_config, ioc->ioc_hpa + IOC_ROPE0_CFG + i);
	}
}

typedef void (initfunc)(struct ioc *);

struct ioc_iommu {
	u32 func_id;
	char *name;
	initfunc *init;
};

static struct ioc_iommu ioc_iommu_info[] __initdata = {
	{ ZX1_IOC_ID, "zx1", ioc_zx1_init },
	{ ZX2_IOC_ID, "zx2", NULL },
	{ SX1000_IOC_ID, "sx1000", NULL },
	{ SX2000_IOC_ID, "sx2000", NULL },
};

static struct ioc * __init
ioc_init(unsigned long hpa, void *handle)
{
	struct ioc *ioc;
	struct ioc_iommu *info;

	ioc = kzalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return NULL;

	ioc->next = ioc_list;
	ioc_list = ioc;

	ioc->handle = handle;
	ioc->ioc_hpa = ioremap(hpa, 0x1000);

	ioc->func_id = READ_REG(ioc->ioc_hpa + IOC_FUNC_ID);
	ioc->rev = READ_REG(ioc->ioc_hpa + IOC_FCLASS) & 0xFFUL;
	ioc->dma_mask = 0xFFFFFFFFFFFFFFFFUL;	

	for (info = ioc_iommu_info; info < ioc_iommu_info + ARRAY_SIZE(ioc_iommu_info); info++) {
		if (ioc->func_id == info->func_id) {
			ioc->name = info->name;
			if (info->init)
				(info->init)(ioc);
		}
	}

	iovp_size = (1 << iovp_shift);
	iovp_mask = ~(iovp_size - 1);

	DBG_INIT("%s: PAGE_SIZE %ldK, iovp_size %ldK\n", __func__,
		PAGE_SIZE >> 10, iovp_size >> 10);

	if (!ioc->name) {
		ioc->name = kmalloc(24, GFP_KERNEL);
		if (ioc->name)
			sprintf((char *) ioc->name, "Unknown (%04x:%04x)",
				ioc->func_id & 0xFFFF, (ioc->func_id >> 16) & 0xFFFF);
		else
			ioc->name = "Unknown";
	}

	ioc_iova_init(ioc);
	ioc_resource_init(ioc);
	ioc_sac_init(ioc);

	if ((long) ~iovp_mask > (long) ia64_max_iommu_merge_mask)
		ia64_max_iommu_merge_mask = ~iovp_mask;

	printk(KERN_INFO PFX
		"%s %d.%d HPA 0x%lx IOVA space %dMb at 0x%lx\n",
		ioc->name, (ioc->rev >> 4) & 0xF, ioc->rev & 0xF,
		hpa, ioc->iov_size >> 20, ioc->ibase);

	return ioc;
}




#ifdef CONFIG_PROC_FS
static void *
ioc_start(struct seq_file *s, loff_t *pos)
{
	struct ioc *ioc;
	loff_t n = *pos;

	for (ioc = ioc_list; ioc; ioc = ioc->next)
		if (!n--)
			return ioc;

	return NULL;
}

static void *
ioc_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct ioc *ioc = v;

	++*pos;
	return ioc->next;
}

static void
ioc_stop(struct seq_file *s, void *v)
{
}

static int
ioc_show(struct seq_file *s, void *v)
{
	struct ioc *ioc = v;
	unsigned long *res_ptr = (unsigned long *)ioc->res_map;
	int i, used = 0;

	seq_printf(s, "Hewlett Packard %s IOC rev %d.%d\n",
		ioc->name, ((ioc->rev >> 4) & 0xF), (ioc->rev & 0xF));
#ifdef CONFIG_NUMA
	if (ioc->node != MAX_NUMNODES)
		seq_printf(s, "NUMA node       : %d\n", ioc->node);
#endif
	seq_printf(s, "IOVA size       : %ld MB\n", ((ioc->pdir_size >> 3) * iovp_size)/(1024*1024));
	seq_printf(s, "IOVA page size  : %ld kb\n", iovp_size/1024);

	for (i = 0; i < (ioc->res_size / sizeof(unsigned long)); ++i, ++res_ptr)
		used += hweight64(*res_ptr);

	seq_printf(s, "PDIR size       : %d entries\n", ioc->pdir_size >> 3);
	seq_printf(s, "PDIR used       : %d entries\n", used);

#ifdef PDIR_SEARCH_TIMING
	{
		unsigned long i = 0, avg = 0, min, max;
		min = max = ioc->avg_search[0];
		for (i = 0; i < SBA_SEARCH_SAMPLE; i++) {
			avg += ioc->avg_search[i];
			if (ioc->avg_search[i] > max) max = ioc->avg_search[i];
			if (ioc->avg_search[i] < min) min = ioc->avg_search[i];
		}
		avg /= SBA_SEARCH_SAMPLE;
		seq_printf(s, "Bitmap search   : %ld/%ld/%ld (min/avg/max CPU Cycles/IOVA page)\n",
		           min, avg, max);
	}
#endif
#ifndef ALLOW_IOV_BYPASS
	 seq_printf(s, "IOVA bypass disabled\n");
#endif
	return 0;
}

static const struct seq_operations ioc_seq_ops = {
	.start = ioc_start,
	.next  = ioc_next,
	.stop  = ioc_stop,
	.show  = ioc_show
};

static int
ioc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ioc_seq_ops);
}

static const struct file_operations ioc_fops = {
	.open    = ioc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};

static void __init
ioc_proc_init(void)
{
	struct proc_dir_entry *dir;

	dir = proc_mkdir("bus/mckinley", NULL);
	if (!dir)
		return;

	proc_create(ioc_list->name, 0, dir, &ioc_fops);
}
#endif

static void
sba_connect_bus(struct pci_bus *bus)
{
	acpi_handle handle, parent;
	acpi_status status;
	struct ioc *ioc;

	if (!PCI_CONTROLLER(bus))
		panic(PFX "no sysdata on bus %d!\n", bus->number);

	if (PCI_CONTROLLER(bus)->iommu)
		return;

	handle = PCI_CONTROLLER(bus)->acpi_handle;
	if (!handle)
		return;

	do {
		for (ioc = ioc_list; ioc; ioc = ioc->next)
			if (ioc->handle == handle) {
				PCI_CONTROLLER(bus)->iommu = ioc;
				return;
			}

		status = acpi_get_parent(handle, &parent);
		handle = parent;
	} while (ACPI_SUCCESS(status));

	printk(KERN_WARNING "No IOC for PCI Bus %04x:%02x in ACPI\n", pci_domain_nr(bus), bus->number);
}

#ifdef CONFIG_NUMA
static void __init
sba_map_ioc_to_node(struct ioc *ioc, acpi_handle handle)
{
	unsigned int node;
	int pxm;

	ioc->node = MAX_NUMNODES;

	pxm = acpi_get_pxm(handle);

	if (pxm < 0)
		return;

	node = pxm_to_node(pxm);

	if (node >= MAX_NUMNODES || !node_online(node))
		return;

	ioc->node = node;
	return;
}
#else
#define sba_map_ioc_to_node(ioc, handle)
#endif

static int __init
acpi_sba_ioc_add(struct acpi_device *device)
{
	struct ioc *ioc;
	acpi_status status;
	u64 hpa, length;
	struct acpi_device_info *adi;

	status = hp_acpi_csr_space(device->handle, &hpa, &length);
	if (ACPI_FAILURE(status))
		return 1;

	status = acpi_get_object_info(device->handle, &adi);
	if (ACPI_FAILURE(status))
		return 1;

	if (strncmp("HWP0001", adi->hardware_id.string, 7) == 0) {
		hpa += ZX1_IOC_OFFSET;
		
		if (!iovp_shift)
			iovp_shift = min(PAGE_SHIFT, 16);
	}
	kfree(adi);

	if (!iovp_shift)
		iovp_shift = 12;

	ioc = ioc_init(hpa, device->handle);
	if (!ioc)
		return 1;

	
	sba_map_ioc_to_node(ioc, device->handle);
	return 0;
}

static const struct acpi_device_id hp_ioc_iommu_device_ids[] = {
	{"HWP0001", 0},
	{"HWP0004", 0},
	{"", 0},
};
static struct acpi_driver acpi_sba_ioc_driver = {
	.name		= "IOC IOMMU Driver",
	.ids		= hp_ioc_iommu_device_ids,
	.ops		= {
		.add	= acpi_sba_ioc_add,
	},
};

extern struct dma_map_ops swiotlb_dma_ops;

static int __init
sba_init(void)
{
	if (!ia64_platform_is("hpzx1") && !ia64_platform_is("hpzx1_swiotlb"))
		return 0;

#if defined(CONFIG_IA64_GENERIC)
	if (is_kdump_kernel()) {
		dma_ops = &swiotlb_dma_ops;
		if (swiotlb_late_init_with_default_size(64 * (1<<20)) != 0)
			panic("Unable to initialize software I/O TLB:"
				  " Try machvec=dig boot option");
		machvec_init("dig");
		return 0;
	}
#endif

	acpi_bus_register_driver(&acpi_sba_ioc_driver);
	if (!ioc_list) {
#ifdef CONFIG_IA64_GENERIC
		dma_ops = &swiotlb_dma_ops;
		if (swiotlb_late_init_with_default_size(64 * (1<<20)) != 0)
			panic("Unable to find SBA IOMMU or initialize "
			      "software I/O TLB: Try machvec=dig boot option");
		machvec_init("dig");
#else
		panic("Unable to find SBA IOMMU: Try a generic or DIG kernel");
#endif
		return 0;
	}

#if defined(CONFIG_IA64_GENERIC) || defined(CONFIG_IA64_HP_ZX1_SWIOTLB)
	if (ia64_platform_is("hpzx1_swiotlb")) {
		extern void hwsw_init(void);

		hwsw_init();
	}
#endif

#ifdef CONFIG_PCI
	{
		struct pci_bus *b = NULL;
		while ((b = pci_find_next_bus(b)) != NULL)
			sba_connect_bus(b);
	}
#endif

#ifdef CONFIG_PROC_FS
	ioc_proc_init();
#endif
	return 0;
}

subsys_initcall(sba_init); 

static int __init
nosbagart(char *str)
{
	reserve_sba_gart = 0;
	return 1;
}

static int sba_dma_supported (struct device *dev, u64 mask)
{
	
	return ((mask & 0xFFFFFFFFUL) == 0xFFFFFFFFUL);
}

static int sba_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return 0;
}

__setup("nosbagart", nosbagart);

static int __init
sba_page_override(char *str)
{
	unsigned long page_size;

	page_size = memparse(str, &str);
	switch (page_size) {
		case 4096:
		case 8192:
		case 16384:
		case 65536:
			iovp_shift = ffs(page_size) - 1;
			break;
		default:
			printk("%s: unknown/unsupported iommu page size %ld\n",
			       __func__, page_size);
	}

	return 1;
}

__setup("sbapagesize=",sba_page_override);

struct dma_map_ops sba_dma_ops = {
	.alloc			= sba_alloc_coherent,
	.free			= sba_free_coherent,
	.map_page		= sba_map_page,
	.unmap_page		= sba_unmap_page,
	.map_sg			= sba_map_sg_attrs,
	.unmap_sg		= sba_unmap_sg_attrs,
	.sync_single_for_cpu	= machvec_dma_sync_single,
	.sync_sg_for_cpu	= machvec_dma_sync_sg,
	.sync_single_for_device	= machvec_dma_sync_single,
	.sync_sg_for_device	= machvec_dma_sync_sg,
	.dma_supported		= sba_dma_supported,
	.mapping_error		= sba_dma_mapping_error,
};

void sba_dma_init(void)
{
	dma_ops = &sba_dma_ops;
}
