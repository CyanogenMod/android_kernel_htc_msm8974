/*
** ccio-dma.c:
**	DMA management routines for first generation cache-coherent machines.
**	Program U2/Uturn in "Virtual Mode" and use the I/O MMU.
**
**	(c) Copyright 2000 Grant Grundler
**	(c) Copyright 2000 Ryan Bradetich
**	(c) Copyright 2000 Hewlett-Packard Company
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
**
**  "Real Mode" operation refers to U2/Uturn chip operation.
**  U2/Uturn were designed to perform coherency checks w/o using
**  the I/O MMU - basically what x86 does.
**
**  Philipp Rumpf has a "Real Mode" driver for PCX-W machines at:
**      CVSROOT=:pserver:anonymous@198.186.203.37:/cvsroot/linux-parisc
**      cvs -z3 co linux/arch/parisc/kernel/dma-rm.c
**
**  I've rewritten his code to work under TPG's tree. See ccio-rm-dma.c.
**
**  Drawbacks of using Real Mode are:
**	o outbound DMA is slower - U2 won't prefetch data (GSC+ XQL signal).
**      o Inbound DMA less efficient - U2 can't use DMA_FAST attribute.
**	o Ability to do scatter/gather in HW is lost.
**	o Doesn't work under PCX-U/U+ machines since they didn't follow
**        the coherency design originally worked out. Only PCX-W does.
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/iommu-helper.h>
#include <linux/export.h>

#include <asm/byteorder.h>
#include <asm/cache.h>		
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/hardware.h>       
#include <asm/parisc-device.h>

#define MODULE_NAME "ccio"

#undef DEBUG_CCIO_RES
#undef DEBUG_CCIO_RUN
#undef DEBUG_CCIO_INIT
#undef DEBUG_CCIO_RUN_SG

#ifdef CONFIG_PROC_FS
#undef CCIO_COLLECT_STATS
#endif

#include <asm/runway.h>		

#ifdef DEBUG_CCIO_INIT
#define DBG_INIT(x...)  printk(x)
#else
#define DBG_INIT(x...)
#endif

#ifdef DEBUG_CCIO_RUN
#define DBG_RUN(x...)   printk(x)
#else
#define DBG_RUN(x...)
#endif

#ifdef DEBUG_CCIO_RES
#define DBG_RES(x...)   printk(x)
#else
#define DBG_RES(x...)
#endif

#ifdef DEBUG_CCIO_RUN_SG
#define DBG_RUN_SG(x...) printk(x)
#else
#define DBG_RUN_SG(x...)
#endif

#define CCIO_INLINE	inline
#define WRITE_U32(value, addr) __raw_writel(value, addr)
#define READ_U32(addr) __raw_readl(addr)

#define U2_IOA_RUNWAY 0x580
#define U2_BC_GSC     0x501
#define UTURN_IOA_RUNWAY 0x581
#define UTURN_BC_GSC     0x502

#define IOA_NORMAL_MODE      0x00020080 
#define CMD_TLB_DIRECT_WRITE 35         
#define CMD_TLB_PURGE        33         

struct ioa_registers {
        
        int32_t    unused1[12];
        uint32_t   io_command;             
        uint32_t   io_status;              
        uint32_t   io_control;             
        int32_t    unused2[1];

        
        uint32_t   io_err_resp;            
        uint32_t   io_err_info;            
        uint32_t   io_err_req;             
        uint32_t   io_err_resp_hi;         
        uint32_t   io_tlb_entry_m;         
        uint32_t   io_tlb_entry_l;         
        uint32_t   unused3[1];
        uint32_t   io_pdir_base;           
        uint32_t   io_io_low_hv;           
        uint32_t   io_io_high_hv;          
        uint32_t   unused4[1];
        uint32_t   io_chain_id_mask;       
        uint32_t   unused5[2];
        uint32_t   io_io_low;              
        uint32_t   io_io_high;             
};


struct ioc {
	struct ioa_registers __iomem *ioc_regs;  
	u8  *res_map;	                
	u64 *pdir_base;	                
	u32 pdir_size; 			
	u32 res_hint;	                
	u32 res_size;		    	
	spinlock_t res_lock;

#ifdef CCIO_COLLECT_STATS
#define CCIO_SEARCH_SAMPLE 0x100
	unsigned long avg_search[CCIO_SEARCH_SAMPLE];
	unsigned long avg_idx;		  
	unsigned long used_pages;
	unsigned long msingle_calls;
	unsigned long msingle_pages;
	unsigned long msg_calls;
	unsigned long msg_pages;
	unsigned long usingle_calls;
	unsigned long usingle_pages;
	unsigned long usg_calls;
	unsigned long usg_pages;
#endif
	unsigned short cujo20_bug;

	
	u32 chainid_shift; 		
	struct ioc *next;		
	const char *name;		
	unsigned int hw_path;           
	struct pci_dev *fake_pci_dev;   
	struct resource mmio_region[2]; 
};

static struct ioc *ioc_list;
static int ioc_count;

#define IOVP_SIZE PAGE_SIZE
#define IOVP_SHIFT PAGE_SHIFT
#define IOVP_MASK PAGE_MASK

#define CCIO_IOVA(iovp,offset) ((iovp) | (offset))
#define CCIO_IOVP(iova) ((iova) & IOVP_MASK)

#define PDIR_INDEX(iovp)    ((iovp)>>IOVP_SHIFT)
#define MKIOVP(pdir_idx)    ((long)(pdir_idx) << IOVP_SHIFT)
#define MKIOVA(iovp,offset) (dma_addr_t)((long)iovp | (long)offset)

#define CCIO_SEARCH_LOOP(ioc, res_idx, mask, size)  \
       for(; res_ptr < res_end; ++res_ptr) { \
		int ret;\
		unsigned int idx;\
		idx = (unsigned int)((unsigned long)res_ptr - (unsigned long)ioc->res_map); \
		ret = iommu_is_span_boundary(idx << 3, pages_needed, 0, boundary_size);\
		if ((0 == (*res_ptr & mask)) && !ret) { \
			*res_ptr |= mask; \
			res_idx = idx;\
			ioc->res_hint = res_idx + (size >> 3); \
			goto resource_found; \
		} \
	}

#define CCIO_FIND_FREE_MAPPING(ioa, res_idx, mask, size) \
       u##size *res_ptr = (u##size *)&((ioc)->res_map[ioa->res_hint & ~((size >> 3) - 1)]); \
       u##size *res_end = (u##size *)&(ioc)->res_map[ioa->res_size]; \
       CCIO_SEARCH_LOOP(ioc, res_idx, mask, size); \
       res_ptr = (u##size *)&(ioc)->res_map[0]; \
       CCIO_SEARCH_LOOP(ioa, res_idx, mask, size);


static int
ccio_alloc_range(struct ioc *ioc, struct device *dev, size_t size)
{
	unsigned int pages_needed = size >> IOVP_SHIFT;
	unsigned int res_idx;
	unsigned long boundary_size;
#ifdef CCIO_COLLECT_STATS
	unsigned long cr_start = mfctl(16);
#endif
	
	BUG_ON(pages_needed == 0);
	BUG_ON((pages_needed * IOVP_SIZE) > DMA_CHUNK_SIZE);
     
	DBG_RES("%s() size: %d pages_needed %d\n", 
		__func__, size, pages_needed);


	boundary_size = ALIGN((unsigned long long)dma_get_seg_boundary(dev) + 1,
			      1ULL << IOVP_SHIFT) >> IOVP_SHIFT;

	if (pages_needed <= 8) {
#if 0
		unsigned long mask = ~(~0UL >> pages_needed);
		CCIO_FIND_FREE_MAPPING(ioc, res_idx, mask, 8);
#else
		CCIO_FIND_FREE_MAPPING(ioc, res_idx, 0xff, 8);
#endif
	} else if (pages_needed <= 16) {
		CCIO_FIND_FREE_MAPPING(ioc, res_idx, 0xffff, 16);
	} else if (pages_needed <= 32) {
		CCIO_FIND_FREE_MAPPING(ioc, res_idx, ~(unsigned int)0, 32);
#ifdef __LP64__
	} else if (pages_needed <= 64) {
		CCIO_FIND_FREE_MAPPING(ioc, res_idx, ~0UL, 64);
#endif
	} else {
		panic("%s: %s() Too many pages to map. pages_needed: %u\n",
		       __FILE__,  __func__, pages_needed);
	}

	panic("%s: %s() I/O MMU is out of mapping resources.\n", __FILE__,
	      __func__);
	
resource_found:
	
	DBG_RES("%s() res_idx %d res_hint: %d\n",
		__func__, res_idx, ioc->res_hint);

#ifdef CCIO_COLLECT_STATS
	{
		unsigned long cr_end = mfctl(16);
		unsigned long tmp = cr_end - cr_start;
		
		cr_start = (cr_end < cr_start) ?  -(tmp) : (tmp);
	}
	ioc->avg_search[ioc->avg_idx++] = cr_start;
	ioc->avg_idx &= CCIO_SEARCH_SAMPLE - 1;
	ioc->used_pages += pages_needed;
#endif
	return res_idx << 3;
}

#define CCIO_FREE_MAPPINGS(ioc, res_idx, mask, size) \
        u##size *res_ptr = (u##size *)&((ioc)->res_map[res_idx]); \
        BUG_ON((*res_ptr & mask) != mask); \
        *res_ptr &= ~(mask);

static void
ccio_free_range(struct ioc *ioc, dma_addr_t iova, unsigned long pages_mapped)
{
	unsigned long iovp = CCIO_IOVP(iova);
	unsigned int res_idx = PDIR_INDEX(iovp) >> 3;

	BUG_ON(pages_mapped == 0);
	BUG_ON((pages_mapped * IOVP_SIZE) > DMA_CHUNK_SIZE);
	BUG_ON(pages_mapped > BITS_PER_LONG);

	DBG_RES("%s():  res_idx: %d pages_mapped %d\n", 
		__func__, res_idx, pages_mapped);

#ifdef CCIO_COLLECT_STATS
	ioc->used_pages -= pages_mapped;
#endif

	if(pages_mapped <= 8) {
#if 0
		
		unsigned long mask = ~(~0UL >> pages_mapped);
		CCIO_FREE_MAPPINGS(ioc, res_idx, mask, 8);
#else
		CCIO_FREE_MAPPINGS(ioc, res_idx, 0xffUL, 8);
#endif
	} else if(pages_mapped <= 16) {
		CCIO_FREE_MAPPINGS(ioc, res_idx, 0xffffUL, 16);
	} else if(pages_mapped <= 32) {
		CCIO_FREE_MAPPINGS(ioc, res_idx, ~(unsigned int)0, 32);
#ifdef __LP64__
	} else if(pages_mapped <= 64) {
		CCIO_FREE_MAPPINGS(ioc, res_idx, ~0UL, 64);
#endif
	} else {
		panic("%s:%s() Too many pages to unmap.\n", __FILE__,
		      __func__);
	}
}


typedef unsigned long space_t;
#define KERNEL_SPACE 0

#define IOPDIR_VALID    0x01UL
#define HINT_SAFE_DMA   0x02UL	
#ifdef CONFIG_EISA
#define HINT_STOP_MOST  0x04UL	
#else
#define HINT_STOP_MOST  0x00UL	
#endif
#define HINT_UDPATE_ENB 0x08UL  
#define HINT_PREFETCH   0x10UL	


static u32 hint_lookup[] = {
	[PCI_DMA_BIDIRECTIONAL]	= HINT_STOP_MOST | HINT_SAFE_DMA | IOPDIR_VALID,
	[PCI_DMA_TODEVICE]	= HINT_STOP_MOST | HINT_PREFETCH | IOPDIR_VALID,
	[PCI_DMA_FROMDEVICE]	= HINT_STOP_MOST | IOPDIR_VALID,
};

 
static void CCIO_INLINE
ccio_io_pdir_entry(u64 *pdir_ptr, space_t sid, unsigned long vba,
		   unsigned long hints)
{
	register unsigned long pa;
	register unsigned long ci; 

	
	BUG_ON(sid != KERNEL_SPACE);

	mtsp(sid,1);

	pa = virt_to_phys(vba);
	asm volatile("depw  %1,31,12,%0" : "+r" (pa) : "r" (hints));
	((u32 *)pdir_ptr)[1] = (u32) pa;


#ifdef __LP64__
	asm volatile ("extrd,u %1,15,4,%0" : "=r" (ci) : "r" (pa));
	asm volatile ("extrd,u %1,31,16,%0" : "+r" (pa) : "r" (pa));
	asm volatile ("depd  %1,35,4,%0" : "+r" (pa) : "r" (ci));
#else
	pa = 0;
#endif
	asm volatile ("lci %%r0(%%sr1, %1), %0" : "=r" (ci) : "r" (vba));
	asm volatile ("extru %1,19,12,%0" : "+r" (ci) : "r" (ci));
	asm volatile ("depw  %1,15,12,%0" : "+r" (pa) : "r" (ci));

	((u32 *)pdir_ptr)[0] = (u32) pa;


	asm volatile("fdc %%r0(%0)" : : "r" (pdir_ptr));
	asm volatile("sync");
}

static CCIO_INLINE void
ccio_clear_io_tlb(struct ioc *ioc, dma_addr_t iovp, size_t byte_cnt)
{
	u32 chain_size = 1 << ioc->chainid_shift;

	iovp &= IOVP_MASK;	
	byte_cnt += chain_size;

	while(byte_cnt > chain_size) {
		WRITE_U32(CMD_TLB_PURGE | iovp, &ioc->ioc_regs->io_command);
		iovp += chain_size;
		byte_cnt -= chain_size;
	}
}

 
static CCIO_INLINE void
ccio_mark_invalid(struct ioc *ioc, dma_addr_t iova, size_t byte_cnt)
{
	u32 iovp = (u32)CCIO_IOVP(iova);
	size_t saved_byte_cnt;

	
	saved_byte_cnt = byte_cnt = ALIGN(byte_cnt, IOVP_SIZE);

	while(byte_cnt > 0) {
		
		unsigned int idx = PDIR_INDEX(iovp);
		char *pdir_ptr = (char *) &(ioc->pdir_base[idx]);

		BUG_ON(idx >= (ioc->pdir_size / sizeof(u64)));
		pdir_ptr[7] = 0;	 
		asm volatile("fdc %%r0(%0)" : : "r" (pdir_ptr[7]));

		iovp     += IOVP_SIZE;
		byte_cnt -= IOVP_SIZE;
	}

	asm volatile("sync");
	ccio_clear_io_tlb(ioc, CCIO_IOVP(iova), saved_byte_cnt);
}


static int 
ccio_dma_supported(struct device *dev, u64 mask)
{
	if(dev == NULL) {
		printk(KERN_ERR MODULE_NAME ": EISA/ISA/et al not supported\n");
		BUG();
		return 0;
	}

	
	return (int)(mask == 0xffffffffUL);
}

static dma_addr_t 
ccio_map_single(struct device *dev, void *addr, size_t size,
		enum dma_data_direction direction)
{
	int idx;
	struct ioc *ioc;
	unsigned long flags;
	dma_addr_t iovp;
	dma_addr_t offset;
	u64 *pdir_start;
	unsigned long hint = hint_lookup[(int)direction];

	BUG_ON(!dev);
	ioc = GET_IOC(dev);

	BUG_ON(size <= 0);

	
	offset = ((unsigned long) addr) & ~IOVP_MASK;

	
	size = ALIGN(size + offset, IOVP_SIZE);
	spin_lock_irqsave(&ioc->res_lock, flags);

#ifdef CCIO_COLLECT_STATS
	ioc->msingle_calls++;
	ioc->msingle_pages += size >> IOVP_SHIFT;
#endif

	idx = ccio_alloc_range(ioc, dev, size);
	iovp = (dma_addr_t)MKIOVP(idx);

	pdir_start = &(ioc->pdir_base[idx]);

	DBG_RUN("%s() 0x%p -> 0x%lx size: %0x%x\n",
		__func__, addr, (long)iovp | offset, size);

	
	if((size % L1_CACHE_BYTES) || ((unsigned long)addr % L1_CACHE_BYTES))
		hint |= HINT_SAFE_DMA;

	while(size > 0) {
		ccio_io_pdir_entry(pdir_start, KERNEL_SPACE, (unsigned long)addr, hint);

		DBG_RUN(" pdir %p %08x%08x\n",
			pdir_start,
			(u32) (((u32 *) pdir_start)[0]),
			(u32) (((u32 *) pdir_start)[1]));
		++pdir_start;
		addr += IOVP_SIZE;
		size -= IOVP_SIZE;
	}

	spin_unlock_irqrestore(&ioc->res_lock, flags);

	
	return CCIO_IOVA(iovp, offset);
}

static void 
ccio_unmap_single(struct device *dev, dma_addr_t iova, size_t size, 
		  enum dma_data_direction direction)
{
	struct ioc *ioc;
	unsigned long flags; 
	dma_addr_t offset = iova & ~IOVP_MASK;
	
	BUG_ON(!dev);
	ioc = GET_IOC(dev);

	DBG_RUN("%s() iovp 0x%lx/%x\n",
		__func__, (long)iova, size);

	iova ^= offset;        
	size += offset;
	size = ALIGN(size, IOVP_SIZE);

	spin_lock_irqsave(&ioc->res_lock, flags);

#ifdef CCIO_COLLECT_STATS
	ioc->usingle_calls++;
	ioc->usingle_pages += size >> IOVP_SHIFT;
#endif

	ccio_mark_invalid(ioc, iova, size);
	ccio_free_range(ioc, iova, (size >> IOVP_SHIFT));
	spin_unlock_irqrestore(&ioc->res_lock, flags);
}

static void * 
ccio_alloc_consistent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
      void *ret;
#if 0
	if(!hwdev) {
		
		*dma_handle = 0;
		return 0;
	}
#endif
        ret = (void *) __get_free_pages(flag, get_order(size));

	if (ret) {
		memset(ret, 0, size);
		*dma_handle = ccio_map_single(dev, ret, size, PCI_DMA_BIDIRECTIONAL);
	}

	return ret;
}

static void 
ccio_free_consistent(struct device *dev, size_t size, void *cpu_addr, 
		     dma_addr_t dma_handle)
{
	ccio_unmap_single(dev, dma_handle, size, 0);
	free_pages((unsigned long)cpu_addr, get_order(size));
}

#define PIDE_FLAG 0x80000000UL

#ifdef CCIO_COLLECT_STATS
#define IOMMU_MAP_STATS
#endif
#include "iommu-helpers.h"

static int
ccio_map_sg(struct device *dev, struct scatterlist *sglist, int nents, 
	    enum dma_data_direction direction)
{
	struct ioc *ioc;
	int coalesced, filled = 0;
	unsigned long flags;
	unsigned long hint = hint_lookup[(int)direction];
	unsigned long prev_len = 0, current_len = 0;
	int i;
	
	BUG_ON(!dev);
	ioc = GET_IOC(dev);
	
	DBG_RUN_SG("%s() START %d entries\n", __func__, nents);

	
	if (nents == 1) {
		sg_dma_address(sglist) = ccio_map_single(dev,
				(void *)sg_virt_addr(sglist), sglist->length,
				direction);
		sg_dma_len(sglist) = sglist->length;
		return 1;
	}

	for(i = 0; i < nents; i++)
		prev_len += sglist[i].length;
	
	spin_lock_irqsave(&ioc->res_lock, flags);

#ifdef CCIO_COLLECT_STATS
	ioc->msg_calls++;
#endif

	coalesced = iommu_coalesce_chunks(ioc, dev, sglist, nents, ccio_alloc_range);

	filled = iommu_fill_pdir(ioc, sglist, nents, hint, ccio_io_pdir_entry);

	spin_unlock_irqrestore(&ioc->res_lock, flags);

	BUG_ON(coalesced != filled);

	DBG_RUN_SG("%s() DONE %d mappings\n", __func__, filled);

	for (i = 0; i < filled; i++)
		current_len += sg_dma_len(sglist + i);

	BUG_ON(current_len != prev_len);

	return filled;
}

static void 
ccio_unmap_sg(struct device *dev, struct scatterlist *sglist, int nents, 
	      enum dma_data_direction direction)
{
	struct ioc *ioc;

	BUG_ON(!dev);
	ioc = GET_IOC(dev);

	DBG_RUN_SG("%s() START %d entries,  %08lx,%x\n",
		__func__, nents, sg_virt_addr(sglist), sglist->length);

#ifdef CCIO_COLLECT_STATS
	ioc->usg_calls++;
#endif

	while(sg_dma_len(sglist) && nents--) {

#ifdef CCIO_COLLECT_STATS
		ioc->usg_pages += sg_dma_len(sglist) >> PAGE_SHIFT;
#endif
		ccio_unmap_single(dev, sg_dma_address(sglist),
				  sg_dma_len(sglist), direction);
		++sglist;
	}

	DBG_RUN_SG("%s() DONE (nents %d)\n", __func__, nents);
}

static struct hppa_dma_ops ccio_ops = {
	.dma_supported =	ccio_dma_supported,
	.alloc_consistent =	ccio_alloc_consistent,
	.alloc_noncoherent =	ccio_alloc_consistent,
	.free_consistent =	ccio_free_consistent,
	.map_single =		ccio_map_single,
	.unmap_single =		ccio_unmap_single,
	.map_sg = 		ccio_map_sg,
	.unmap_sg = 		ccio_unmap_sg,
	.dma_sync_single_for_cpu =	NULL,	
	.dma_sync_single_for_device =	NULL,	
	.dma_sync_sg_for_cpu =		NULL,	
	.dma_sync_sg_for_device =		NULL,	
};

#ifdef CONFIG_PROC_FS
static int ccio_proc_info(struct seq_file *m, void *p)
{
	int len = 0;
	struct ioc *ioc = ioc_list;

	while (ioc != NULL) {
		unsigned int total_pages = ioc->res_size << 3;
#ifdef CCIO_COLLECT_STATS
		unsigned long avg = 0, min, max;
		int j;
#endif

		len += seq_printf(m, "%s\n", ioc->name);
		
		len += seq_printf(m, "Cujo 2.0 bug    : %s\n",
				  (ioc->cujo20_bug ? "yes" : "no"));
		
		len += seq_printf(m, "IO PDIR size    : %d bytes (%d entries)\n",
			       total_pages * 8, total_pages);

#ifdef CCIO_COLLECT_STATS
		len += seq_printf(m, "IO PDIR entries : %ld free  %ld used (%d%%)\n",
				  total_pages - ioc->used_pages, ioc->used_pages,
				  (int)(ioc->used_pages * 100 / total_pages));
#endif

		len += seq_printf(m, "Resource bitmap : %d bytes (%d pages)\n", 
				  ioc->res_size, total_pages);

#ifdef CCIO_COLLECT_STATS
		min = max = ioc->avg_search[0];
		for(j = 0; j < CCIO_SEARCH_SAMPLE; ++j) {
			avg += ioc->avg_search[j];
			if(ioc->avg_search[j] > max) 
				max = ioc->avg_search[j];
			if(ioc->avg_search[j] < min) 
				min = ioc->avg_search[j];
		}
		avg /= CCIO_SEARCH_SAMPLE;
		len += seq_printf(m, "  Bitmap search : %ld/%ld/%ld (min/avg/max CPU Cycles)\n",
				  min, avg, max);

		len += seq_printf(m, "pci_map_single(): %8ld calls  %8ld pages (avg %d/1000)\n",
				  ioc->msingle_calls, ioc->msingle_pages,
				  (int)((ioc->msingle_pages * 1000)/ioc->msingle_calls));

		
		min = ioc->usingle_calls - ioc->usg_calls;
		max = ioc->usingle_pages - ioc->usg_pages;
		len += seq_printf(m, "pci_unmap_single: %8ld calls  %8ld pages (avg %d/1000)\n",
				  min, max, (int)((max * 1000)/min));
 
		len += seq_printf(m, "pci_map_sg()    : %8ld calls  %8ld pages (avg %d/1000)\n",
				  ioc->msg_calls, ioc->msg_pages,
				  (int)((ioc->msg_pages * 1000)/ioc->msg_calls));

		len += seq_printf(m, "pci_unmap_sg()  : %8ld calls  %8ld pages (avg %d/1000)\n\n\n",
				  ioc->usg_calls, ioc->usg_pages,
				  (int)((ioc->usg_pages * 1000)/ioc->usg_calls));
#endif	

		ioc = ioc->next;
	}

	return 0;
}

static int ccio_proc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, &ccio_proc_info, NULL);
}

static const struct file_operations ccio_proc_info_fops = {
	.owner = THIS_MODULE,
	.open = ccio_proc_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ccio_proc_bitmap_info(struct seq_file *m, void *p)
{
	int len = 0;
	struct ioc *ioc = ioc_list;

	while (ioc != NULL) {
		u32 *res_ptr = (u32 *)ioc->res_map;
		int j;

		for (j = 0; j < (ioc->res_size / sizeof(u32)); j++) {
			if ((j & 7) == 0)
				len += seq_puts(m, "\n   ");
			len += seq_printf(m, "%08x", *res_ptr);
			res_ptr++;
		}
		len += seq_puts(m, "\n\n");
		ioc = ioc->next;
		break; 
	}

	return 0;
}

static int ccio_proc_bitmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, &ccio_proc_bitmap_info, NULL);
}

static const struct file_operations ccio_proc_bitmap_fops = {
	.owner = THIS_MODULE,
	.open = ccio_proc_bitmap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif 

static struct ioc * ccio_find_ioc(int hw_path)
{
	int i;
	struct ioc *ioc;

	ioc = ioc_list;
	for (i = 0; i < ioc_count; i++) {
		if (ioc->hw_path == hw_path)
			return ioc;

		ioc = ioc->next;
	}

	return NULL;
}

void * ccio_get_iommu(const struct parisc_device *dev)
{
	dev = find_pa_parent_type(dev, HPHW_IOA);
	if (!dev)
		return NULL;

	return ccio_find_ioc(dev->hw_path);
}

#define CUJO_20_STEP       0x10000000	

void ccio_cujo20_fixup(struct parisc_device *cujo, u32 iovp)
{
	unsigned int idx;
	struct parisc_device *dev = parisc_parent(cujo);
	struct ioc *ioc = ccio_get_iommu(dev);
	u8 *res_ptr;

	ioc->cujo20_bug = 1;
	res_ptr = ioc->res_map;
	idx = PDIR_INDEX(iovp) >> 3;

	while (idx < ioc->res_size) {
 		res_ptr[idx] |= 0xff;
		idx += PDIR_INDEX(CUJO_20_STEP) >> 3;
	}
}

#if 0

static int
ccio_get_iotlb_size(struct parisc_device *dev)
{
	if (dev->spa_shift == 0) {
		panic("%s() : Can't determine I/O TLB size.\n", __func__);
	}
	return (1 << dev->spa_shift);
}
#else

#define CCIO_CHAINID_SHIFT	8
#define CCIO_CHAINID_MASK	0xff
#endif 

static const struct parisc_device_id ccio_tbl[] = {
	{ HPHW_IOA, HVERSION_REV_ANY_ID, U2_IOA_RUNWAY, 0xb }, 
	{ HPHW_IOA, HVERSION_REV_ANY_ID, UTURN_IOA_RUNWAY, 0xb }, 
	{ 0, }
};

static int ccio_probe(struct parisc_device *dev);

static struct parisc_driver ccio_driver = {
	.name =		"ccio",
	.id_table =	ccio_tbl,
	.probe =	ccio_probe,
};

static void
ccio_ioc_init(struct ioc *ioc)
{
	int i;
	unsigned int iov_order;
	u32 iova_space_size;


	iova_space_size = (u32) (totalram_pages / count_parisc_driver(&ccio_driver));

	

	if (iova_space_size < (1 << (20 - PAGE_SHIFT))) {
		iova_space_size =  1 << (20 - PAGE_SHIFT);
#ifdef __LP64__
	} else if (iova_space_size > (1 << (30 - PAGE_SHIFT))) {
		iova_space_size =  1 << (30 - PAGE_SHIFT);
#endif
	}



	iov_order = get_order(iova_space_size << PAGE_SHIFT);

	
	iova_space_size = 1 << (iov_order + PAGE_SHIFT);

	ioc->pdir_size = (iova_space_size / IOVP_SIZE) * sizeof(u64);

	BUG_ON(ioc->pdir_size > 8 * 1024 * 1024);   

	
	BUG_ON((1 << get_order(ioc->pdir_size)) != (ioc->pdir_size >> PAGE_SHIFT));

	DBG_INIT("%s() hpa 0x%p mem %luMB IOV %dMB (%d bits)\n",
			__func__, ioc->ioc_regs,
			(unsigned long) totalram_pages >> (20 - PAGE_SHIFT),
			iova_space_size>>20,
			iov_order + PAGE_SHIFT);

	ioc->pdir_base = (u64 *)__get_free_pages(GFP_KERNEL, 
						 get_order(ioc->pdir_size));
	if(NULL == ioc->pdir_base) {
		panic("%s() could not allocate I/O Page Table\n", __func__);
	}
	memset(ioc->pdir_base, 0, ioc->pdir_size);

	BUG_ON((((unsigned long)ioc->pdir_base) & PAGE_MASK) != (unsigned long)ioc->pdir_base);
	DBG_INIT(" base %p\n", ioc->pdir_base);

	
 	ioc->res_size = (ioc->pdir_size / sizeof(u64)) >> 3;
	DBG_INIT("%s() res_size 0x%x\n", __func__, ioc->res_size);
	
	ioc->res_map = (u8 *)__get_free_pages(GFP_KERNEL, 
					      get_order(ioc->res_size));
	if(NULL == ioc->res_map) {
		panic("%s() could not allocate resource map\n", __func__);
	}
	memset(ioc->res_map, 0, ioc->res_size);

	
	ioc->res_hint = 16;

	
	spin_lock_init(&ioc->res_lock);

	ioc->chainid_shift = get_order(iova_space_size) + PAGE_SHIFT - CCIO_CHAINID_SHIFT;
	DBG_INIT(" chainid_shift 0x%x\n", ioc->chainid_shift);

	WRITE_U32(CCIO_CHAINID_MASK << ioc->chainid_shift, 
		  &ioc->ioc_regs->io_chain_id_mask);

	WRITE_U32(virt_to_phys(ioc->pdir_base), 
		  &ioc->ioc_regs->io_pdir_base);

	WRITE_U32(IOA_NORMAL_MODE, &ioc->ioc_regs->io_control);

	WRITE_U32(0, &ioc->ioc_regs->io_tlb_entry_m);
	WRITE_U32(0, &ioc->ioc_regs->io_tlb_entry_l);

	for(i = 1 << CCIO_CHAINID_SHIFT; i ; i--) {
		WRITE_U32((CMD_TLB_DIRECT_WRITE | (i << ioc->chainid_shift)),
			  &ioc->ioc_regs->io_command);
	}
}

static void __init
ccio_init_resource(struct resource *res, char *name, void __iomem *ioaddr)
{
	int result;

	res->parent = NULL;
	res->flags = IORESOURCE_MEM;
	res->start = (unsigned long)((signed) READ_U32(ioaddr) << 16);
	res->end = (unsigned long)((signed) (READ_U32(ioaddr + 4) << 16) - 1);
	res->name = name;
	if (res->end + 1 == res->start)
		return;

	result = insert_resource(&iomem_resource, res);
	if (result < 0) {
		printk(KERN_ERR "%s() failed to claim CCIO bus address space (%08lx,%08lx)\n", 
			__func__, (unsigned long)res->start, (unsigned long)res->end);
	}
}

static void __init ccio_init_resources(struct ioc *ioc)
{
	struct resource *res = ioc->mmio_region;
	char *name = kmalloc(14, GFP_KERNEL);

	snprintf(name, 14, "GSC Bus [%d/]", ioc->hw_path);

	ccio_init_resource(res, name, &ioc->ioc_regs->io_io_low);
	ccio_init_resource(res + 1, name, &ioc->ioc_regs->io_io_low_hv);
}

static int new_ioc_area(struct resource *res, unsigned long size,
		unsigned long min, unsigned long max, unsigned long align)
{
	if (max <= min)
		return -EBUSY;

	res->start = (max - size + 1) &~ (align - 1);
	res->end = res->start + size;
	
	if (!insert_resource(&iomem_resource, res))
		return 0;

	return new_ioc_area(res, size, min, max - size, align);
}

static int expand_ioc_area(struct resource *res, unsigned long size,
		unsigned long min, unsigned long max, unsigned long align)
{
	unsigned long start, len;

	if (!res->parent)
		return new_ioc_area(res, size, min, max, align);

	start = (res->start - size) &~ (align - 1);
	len = res->end - start + 1;
	if (start >= min) {
		if (!adjust_resource(res, start, len))
			return 0;
	}

	start = res->start;
	len = ((size + res->end + align) &~ (align - 1)) - start;
	if (start + len <= max) {
		if (!adjust_resource(res, start, len))
			return 0;
	}

	return -EBUSY;
}

int ccio_allocate_resource(const struct parisc_device *dev,
		struct resource *res, unsigned long size,
		unsigned long min, unsigned long max, unsigned long align)
{
	struct resource *parent = &iomem_resource;
	struct ioc *ioc = ccio_get_iommu(dev);
	if (!ioc)
		goto out;

	parent = ioc->mmio_region;
	if (parent->parent &&
	    !allocate_resource(parent, res, size, min, max, align, NULL, NULL))
		return 0;

	if ((parent + 1)->parent &&
	    !allocate_resource(parent + 1, res, size, min, max, align,
				NULL, NULL))
		return 0;

	if (!expand_ioc_area(parent, size, min, max, align)) {
		__raw_writel(((parent->start)>>16) | 0xffff0000,
			     &ioc->ioc_regs->io_io_low);
		__raw_writel(((parent->end)>>16) | 0xffff0000,
			     &ioc->ioc_regs->io_io_high);
	} else if (!expand_ioc_area(parent + 1, size, min, max, align)) {
		parent++;
		__raw_writel(((parent->start)>>16) | 0xffff0000,
			     &ioc->ioc_regs->io_io_low_hv);
		__raw_writel(((parent->end)>>16) | 0xffff0000,
			     &ioc->ioc_regs->io_io_high_hv);
	} else {
		return -EBUSY;
	}

 out:
	return allocate_resource(parent, res, size, min, max, align, NULL,NULL);
}

int ccio_request_resource(const struct parisc_device *dev,
		struct resource *res)
{
	struct resource *parent;
	struct ioc *ioc = ccio_get_iommu(dev);

	if (!ioc) {
		parent = &iomem_resource;
	} else if ((ioc->mmio_region->start <= res->start) &&
			(res->end <= ioc->mmio_region->end)) {
		parent = ioc->mmio_region;
	} else if (((ioc->mmio_region + 1)->start <= res->start) &&
			(res->end <= (ioc->mmio_region + 1)->end)) {
		parent = ioc->mmio_region + 1;
	} else {
		return -EBUSY;
	}

	return insert_resource(parent, res);
}

static int __init ccio_probe(struct parisc_device *dev)
{
	int i;
	struct ioc *ioc, **ioc_p = &ioc_list;

	ioc = kzalloc(sizeof(struct ioc), GFP_KERNEL);
	if (ioc == NULL) {
		printk(KERN_ERR MODULE_NAME ": memory allocation failure\n");
		return 1;
	}

	ioc->name = dev->id.hversion == U2_IOA_RUNWAY ? "U2" : "UTurn";

	printk(KERN_INFO "Found %s at 0x%lx\n", ioc->name,
		(unsigned long)dev->hpa.start);

	for (i = 0; i < ioc_count; i++) {
		ioc_p = &(*ioc_p)->next;
	}
	*ioc_p = ioc;

	ioc->hw_path = dev->hw_path;
	ioc->ioc_regs = ioremap_nocache(dev->hpa.start, 4096);
	ccio_ioc_init(ioc);
	ccio_init_resources(ioc);
	hppa_dma_ops = &ccio_ops;
	dev->dev.platform_data = kzalloc(sizeof(struct pci_hba_data), GFP_KERNEL);

	
	BUG_ON(dev->dev.platform_data == NULL);
	HBA_DATA(dev->dev.platform_data)->iommu = ioc;

#ifdef CONFIG_PROC_FS
	if (ioc_count == 0) {
		proc_create(MODULE_NAME, 0, proc_runway_root,
			    &ccio_proc_info_fops);
		proc_create(MODULE_NAME"-bitmap", 0, proc_runway_root,
			    &ccio_proc_bitmap_fops);
	}
#endif
	ioc_count++;

	parisc_has_iommu();
	return 0;
}

void __init ccio_init(void)
{
	register_parisc_driver(&ccio_driver);
}

