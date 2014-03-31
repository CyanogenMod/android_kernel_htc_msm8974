
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/gfp.h>
#include <linux/bootmem.h>
#include <linux/export.h>
#include <linux/scatterlist.h>
#include <linux/log2.h>
#include <linux/dma-mapping.h>
#include <linux/iommu-helper.h>

#include <asm/io.h>
#include <asm/hwrpb.h>

#include "proto.h"
#include "pci_impl.h"


#define DEBUG_ALLOC 0
#if DEBUG_ALLOC > 0
# define DBGA(args...)		printk(KERN_DEBUG args)
#else
# define DBGA(args...)
#endif
#if DEBUG_ALLOC > 1
# define DBGA2(args...)		printk(KERN_DEBUG args)
#else
# define DBGA2(args...)
#endif

#define DEBUG_NODIRECT 0

#define ISA_DMA_MASK		0x00ffffff

static inline unsigned long
mk_iommu_pte(unsigned long paddr)
{
	return (paddr >> (PAGE_SHIFT-1)) | 1;
}


unsigned long
size_for_memory(unsigned long max)
{
	unsigned long mem = max_low_pfn << PAGE_SHIFT;
	if (mem < max)
		max = roundup_pow_of_two(mem);
	return max;
}

struct pci_iommu_arena * __init
iommu_arena_new_node(int nid, struct pci_controller *hose, dma_addr_t base,
		     unsigned long window_size, unsigned long align)
{
	unsigned long mem_size;
	struct pci_iommu_arena *arena;

	mem_size = window_size / (PAGE_SIZE / sizeof(unsigned long));

	if (align < mem_size)
		align = mem_size;


#ifdef CONFIG_DISCONTIGMEM

	arena = alloc_bootmem_node(NODE_DATA(nid), sizeof(*arena));
	if (!NODE_DATA(nid) || !arena) {
		printk("%s: couldn't allocate arena from node %d\n"
		       "    falling back to system-wide allocation\n",
		       __func__, nid);
		arena = alloc_bootmem(sizeof(*arena));
	}

	arena->ptes = __alloc_bootmem_node(NODE_DATA(nid), mem_size, align, 0);
	if (!NODE_DATA(nid) || !arena->ptes) {
		printk("%s: couldn't allocate arena ptes from node %d\n"
		       "    falling back to system-wide allocation\n",
		       __func__, nid);
		arena->ptes = __alloc_bootmem(mem_size, align, 0);
	}

#else 

	arena = alloc_bootmem(sizeof(*arena));
	arena->ptes = __alloc_bootmem(mem_size, align, 0);

#endif 

	spin_lock_init(&arena->lock);
	arena->hose = hose;
	arena->dma_base = base;
	arena->size = window_size;
	arena->next_entry = 0;

	arena->align_entry = 1;

	return arena;
}

struct pci_iommu_arena * __init
iommu_arena_new(struct pci_controller *hose, dma_addr_t base,
		unsigned long window_size, unsigned long align)
{
	return iommu_arena_new_node(0, hose, base, window_size, align);
}

static long
iommu_arena_find_pages(struct device *dev, struct pci_iommu_arena *arena,
		       long n, long mask)
{
	unsigned long *ptes;
	long i, p, nent;
	int pass = 0;
	unsigned long base;
	unsigned long boundary_size;

	base = arena->dma_base >> PAGE_SHIFT;
	if (dev) {
		boundary_size = dma_get_seg_boundary(dev) + 1;
		boundary_size >>= PAGE_SHIFT;
	} else {
		boundary_size = 1UL << (32 - PAGE_SHIFT);
	}

	
	ptes = arena->ptes;
	nent = arena->size >> PAGE_SHIFT;
	p = ALIGN(arena->next_entry, mask + 1);
	i = 0;

again:
	while (i < n && p+i < nent) {
		if (!i && iommu_is_span_boundary(p, n, base, boundary_size)) {
			p = ALIGN(p + 1, mask + 1);
			goto again;
		}

		if (ptes[p+i])
			p = ALIGN(p + i + 1, mask + 1), i = 0;
		else
			i = i + 1;
	}

	if (i < n) {
		if (pass < 1) {
			alpha_mv.mv_pci_tbi(arena->hose, 0, -1);

			pass++;
			p = 0;
			i = 0;
			goto again;
		} else
			return -1;
	}

	return p;
}

static long
iommu_arena_alloc(struct device *dev, struct pci_iommu_arena *arena, long n,
		  unsigned int align)
{
	unsigned long flags;
	unsigned long *ptes;
	long i, p, mask;

	spin_lock_irqsave(&arena->lock, flags);

	
	ptes = arena->ptes;
	mask = max(align, arena->align_entry) - 1;
	p = iommu_arena_find_pages(dev, arena, n, mask);
	if (p < 0) {
		spin_unlock_irqrestore(&arena->lock, flags);
		return -1;
	}

	for (i = 0; i < n; ++i)
		ptes[p+i] = IOMMU_INVALID_PTE;

	arena->next_entry = p + n;
	spin_unlock_irqrestore(&arena->lock, flags);

	return p;
}

static void
iommu_arena_free(struct pci_iommu_arena *arena, long ofs, long n)
{
	unsigned long *p;
	long i;

	p = arena->ptes + ofs;
	for (i = 0; i < n; ++i)
		p[i] = 0;
}

static int pci_dac_dma_supported(struct pci_dev *dev, u64 mask)
{
	dma_addr_t dac_offset = alpha_mv.pci_dac_offset;
	int ok = 1;

	
	if (dac_offset == 0)
		ok = 0;

	
	if ((dac_offset & dev->dma_mask) != dac_offset)
		ok = 0;

	
	DBGA("pci_dac_dma_supported %s from %p\n",
	     ok ? "yes" : "no", __builtin_return_address(0));

	return ok;
}


static dma_addr_t
pci_map_single_1(struct pci_dev *pdev, void *cpu_addr, size_t size,
		 int dac_allowed)
{
	struct pci_controller *hose = pdev ? pdev->sysdata : pci_isa_hose;
	dma_addr_t max_dma = pdev ? pdev->dma_mask : ISA_DMA_MASK;
	struct pci_iommu_arena *arena;
	long npages, dma_ofs, i;
	unsigned long paddr;
	dma_addr_t ret;
	unsigned int align = 0;
	struct device *dev = pdev ? &pdev->dev : NULL;

	paddr = __pa(cpu_addr);

#if !DEBUG_NODIRECT
	
	if (paddr + size + __direct_map_base - 1 <= max_dma
	    && paddr + size <= __direct_map_size) {
		ret = paddr + __direct_map_base;

		DBGA2("pci_map_single: [%p,%zx] -> direct %llx from %p\n",
		      cpu_addr, size, ret, __builtin_return_address(0));

		return ret;
	}
#endif

	
	if (dac_allowed) {
		ret = paddr + alpha_mv.pci_dac_offset;

		DBGA2("pci_map_single: [%p,%zx] -> DAC %llx from %p\n",
		      cpu_addr, size, ret, __builtin_return_address(0));

		return ret;
	}

	if (! alpha_mv.mv_pci_tbi) {
		printk_once(KERN_WARNING "pci_map_single: no HW sg\n");
		return 0;
	}

	arena = hose->sg_pci;
	if (!arena || arena->dma_base + arena->size - 1 > max_dma)
		arena = hose->sg_isa;

	npages = iommu_num_pages(paddr, size, PAGE_SIZE);

	
	if (pdev && pdev == isa_bridge)
		align = 8;
	dma_ofs = iommu_arena_alloc(dev, arena, npages, align);
	if (dma_ofs < 0) {
		printk(KERN_WARNING "pci_map_single failed: "
		       "could not allocate dma page tables\n");
		return 0;
	}

	paddr &= PAGE_MASK;
	for (i = 0; i < npages; ++i, paddr += PAGE_SIZE)
		arena->ptes[i + dma_ofs] = mk_iommu_pte(paddr);

	ret = arena->dma_base + dma_ofs * PAGE_SIZE;
	ret += (unsigned long)cpu_addr & ~PAGE_MASK;

	DBGA2("pci_map_single: [%p,%zx] np %ld -> sg %llx from %p\n",
	      cpu_addr, size, npages, ret, __builtin_return_address(0));

	return ret;
}

static struct pci_dev *alpha_gendev_to_pci(struct device *dev)
{
	if (dev && dev->bus == &pci_bus_type)
		return to_pci_dev(dev);

	BUG_ON(!isa_bridge);

	if (!dev || !dev->dma_mask || !*dev->dma_mask)
		return isa_bridge;

	if (*dev->dma_mask >= isa_bridge->dma_mask)
		return isa_bridge;

	
	return NULL;
}

static dma_addr_t alpha_pci_map_page(struct device *dev, struct page *page,
				     unsigned long offset, size_t size,
				     enum dma_data_direction dir,
				     struct dma_attrs *attrs)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	int dac_allowed;

	if (dir == PCI_DMA_NONE)
		BUG();

	dac_allowed = pdev ? pci_dac_dma_supported(pdev, pdev->dma_mask) : 0; 
	return pci_map_single_1(pdev, (char *)page_address(page) + offset, 
				size, dac_allowed);
}


static void alpha_pci_unmap_page(struct device *dev, dma_addr_t dma_addr,
				 size_t size, enum dma_data_direction dir,
				 struct dma_attrs *attrs)
{
	unsigned long flags;
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	struct pci_controller *hose = pdev ? pdev->sysdata : pci_isa_hose;
	struct pci_iommu_arena *arena;
	long dma_ofs, npages;

	if (dir == PCI_DMA_NONE)
		BUG();

	if (dma_addr >= __direct_map_base
	    && dma_addr < __direct_map_base + __direct_map_size) {
		

		DBGA2("pci_unmap_single: direct [%llx,%zx] from %p\n",
		      dma_addr, size, __builtin_return_address(0));

		return;
	}

	if (dma_addr > 0xffffffff) {
		DBGA2("pci64_unmap_single: DAC [%llx,%zx] from %p\n",
		      dma_addr, size, __builtin_return_address(0));
		return;
	}

	arena = hose->sg_pci;
	if (!arena || dma_addr < arena->dma_base)
		arena = hose->sg_isa;

	dma_ofs = (dma_addr - arena->dma_base) >> PAGE_SHIFT;
	if (dma_ofs * PAGE_SIZE >= arena->size) {
		printk(KERN_ERR "Bogus pci_unmap_single: dma_addr %llx "
		       " base %llx size %x\n",
		       dma_addr, arena->dma_base, arena->size);
		return;
		BUG();
	}

	npages = iommu_num_pages(dma_addr, size, PAGE_SIZE);

	spin_lock_irqsave(&arena->lock, flags);

	iommu_arena_free(arena, dma_ofs, npages);

	if (dma_ofs >= arena->next_entry)
		alpha_mv.mv_pci_tbi(hose, dma_addr, dma_addr + size - 1);

	spin_unlock_irqrestore(&arena->lock, flags);

	DBGA2("pci_unmap_single: sg [%llx,%zx] np %ld from %p\n",
	      dma_addr, size, npages, __builtin_return_address(0));
}


static void *alpha_pci_alloc_coherent(struct device *dev, size_t size,
				      dma_addr_t *dma_addrp, gfp_t gfp,
				      struct dma_attrs *attrs)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	void *cpu_addr;
	long order = get_order(size);

	gfp &= ~GFP_DMA;

try_again:
	cpu_addr = (void *)__get_free_pages(gfp, order);
	if (! cpu_addr) {
		printk(KERN_INFO "pci_alloc_consistent: "
		       "get_free_pages failed from %p\n",
			__builtin_return_address(0));
		return NULL;
	}
	memset(cpu_addr, 0, size);

	*dma_addrp = pci_map_single_1(pdev, cpu_addr, size, 0);
	if (*dma_addrp == 0) {
		free_pages((unsigned long)cpu_addr, order);
		if (alpha_mv.mv_pci_tbi || (gfp & GFP_DMA))
			return NULL;
		gfp |= GFP_DMA;
		goto try_again;
	}

	DBGA2("pci_alloc_consistent: %zx -> [%p,%llx] from %p\n",
	      size, cpu_addr, *dma_addrp, __builtin_return_address(0));

	return cpu_addr;
}


static void alpha_pci_free_coherent(struct device *dev, size_t size,
				    void *cpu_addr, dma_addr_t dma_addr,
				    struct dma_attrs *attrs)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	pci_unmap_single(pdev, dma_addr, size, PCI_DMA_BIDIRECTIONAL);
	free_pages((unsigned long)cpu_addr, get_order(size));

	DBGA2("pci_free_consistent: [%llx,%zx] from %p\n",
	      dma_addr, size, __builtin_return_address(0));
}


#define SG_ENT_VIRT_ADDRESS(SG) (sg_virt((SG)))
#define SG_ENT_PHYS_ADDRESS(SG) __pa(SG_ENT_VIRT_ADDRESS(SG))

static void
sg_classify(struct device *dev, struct scatterlist *sg, struct scatterlist *end,
	    int virt_ok)
{
	unsigned long next_paddr;
	struct scatterlist *leader;
	long leader_flag, leader_length;
	unsigned int max_seg_size;

	leader = sg;
	leader_flag = 0;
	leader_length = leader->length;
	next_paddr = SG_ENT_PHYS_ADDRESS(leader) + leader_length;

	
	max_seg_size = dev ? dma_get_max_seg_size(dev) : 0;
	for (++sg; sg < end; ++sg) {
		unsigned long addr, len;
		addr = SG_ENT_PHYS_ADDRESS(sg);
		len = sg->length;

		if (leader_length + len > max_seg_size)
			goto new_segment;

		if (next_paddr == addr) {
			sg->dma_address = -1;
			leader_length += len;
		} else if (((next_paddr | addr) & ~PAGE_MASK) == 0 && virt_ok) {
			sg->dma_address = -2;
			leader_flag = 1;
			leader_length += len;
		} else {
new_segment:
			leader->dma_address = leader_flag;
			leader->dma_length = leader_length;
			leader = sg;
			leader_flag = 0;
			leader_length = len;
		}

		next_paddr = addr + len;
	}

	leader->dma_address = leader_flag;
	leader->dma_length = leader_length;
}


static int
sg_fill(struct device *dev, struct scatterlist *leader, struct scatterlist *end,
	struct scatterlist *out, struct pci_iommu_arena *arena,
	dma_addr_t max_dma, int dac_allowed)
{
	unsigned long paddr = SG_ENT_PHYS_ADDRESS(leader);
	long size = leader->dma_length;
	struct scatterlist *sg;
	unsigned long *ptes;
	long npages, dma_ofs, i;

#if !DEBUG_NODIRECT
	if (leader->dma_address == 0
	    && paddr + size + __direct_map_base - 1 <= max_dma
	    && paddr + size <= __direct_map_size) {
		out->dma_address = paddr + __direct_map_base;
		out->dma_length = size;

		DBGA("    sg_fill: [%p,%lx] -> direct %llx\n",
		     __va(paddr), size, out->dma_address);

		return 0;
	}
#endif

	
	if (leader->dma_address == 0 && dac_allowed) {
		out->dma_address = paddr + alpha_mv.pci_dac_offset;
		out->dma_length = size;

		DBGA("    sg_fill: [%p,%lx] -> DAC %llx\n",
		     __va(paddr), size, out->dma_address);

		return 0;
	}


	paddr &= ~PAGE_MASK;
	npages = iommu_num_pages(paddr, size, PAGE_SIZE);
	dma_ofs = iommu_arena_alloc(dev, arena, npages, 0);
	if (dma_ofs < 0) {
		
		if (leader->dma_address == 0)
			return -1;

		sg_classify(dev, leader, end, 0);
		return sg_fill(dev, leader, end, out, arena, max_dma, dac_allowed);
	}

	out->dma_address = arena->dma_base + dma_ofs*PAGE_SIZE + paddr;
	out->dma_length = size;

	DBGA("    sg_fill: [%p,%lx] -> sg %llx np %ld\n",
	     __va(paddr), size, out->dma_address, npages);

	ptes = &arena->ptes[dma_ofs];
	sg = leader;
	do {
#if DEBUG_ALLOC > 0
		struct scatterlist *last_sg = sg;
#endif

		size = sg->length;
		paddr = SG_ENT_PHYS_ADDRESS(sg);

		while (sg+1 < end && (int) sg[1].dma_address == -1) {
			size += sg[1].length;
			sg++;
		}

		npages = iommu_num_pages(paddr, size, PAGE_SIZE);

		paddr &= PAGE_MASK;
		for (i = 0; i < npages; ++i, paddr += PAGE_SIZE)
			*ptes++ = mk_iommu_pte(paddr);

#if DEBUG_ALLOC > 0
		DBGA("    (%ld) [%p,%x] np %ld\n",
		     last_sg - leader, SG_ENT_VIRT_ADDRESS(last_sg),
		     last_sg->length, npages);
		while (++last_sg <= sg) {
			DBGA("        (%ld) [%p,%x] cont\n",
			     last_sg - leader, SG_ENT_VIRT_ADDRESS(last_sg),
			     last_sg->length);
		}
#endif
	} while (++sg < end && (int) sg->dma_address < 0);

	return 1;
}

static int alpha_pci_map_sg(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir,
			    struct dma_attrs *attrs)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	struct scatterlist *start, *end, *out;
	struct pci_controller *hose;
	struct pci_iommu_arena *arena;
	dma_addr_t max_dma;
	int dac_allowed;

	if (dir == PCI_DMA_NONE)
		BUG();

	dac_allowed = dev ? pci_dac_dma_supported(pdev, pdev->dma_mask) : 0;

	
	if (nents == 1) {
		sg->dma_length = sg->length;
		sg->dma_address
		  = pci_map_single_1(pdev, SG_ENT_VIRT_ADDRESS(sg),
				     sg->length, dac_allowed);
		return sg->dma_address != 0;
	}

	start = sg;
	end = sg + nents;

	
	sg_classify(dev, sg, end, alpha_mv.mv_pci_tbi != 0);

	
	if (alpha_mv.mv_pci_tbi) {
		hose = pdev ? pdev->sysdata : pci_isa_hose;
		max_dma = pdev ? pdev->dma_mask : ISA_DMA_MASK;
		arena = hose->sg_pci;
		if (!arena || arena->dma_base + arena->size - 1 > max_dma)
			arena = hose->sg_isa;
	} else {
		max_dma = -1;
		arena = NULL;
		hose = NULL;
	}

	for (out = sg; sg < end; ++sg) {
		if ((int) sg->dma_address < 0)
			continue;
		if (sg_fill(dev, sg, end, out, arena, max_dma, dac_allowed) < 0)
			goto error;
		out++;
	}

	
	if (out < end)
		out->dma_length = 0;

	if (out - start == 0)
		printk(KERN_WARNING "pci_map_sg failed: no entries?\n");
	DBGA("pci_map_sg: %ld entries\n", out - start);

	return out - start;

 error:
	printk(KERN_WARNING "pci_map_sg failed: "
	       "could not allocate dma page tables\n");

	if (out > start)
		pci_unmap_sg(pdev, start, out - start, dir);
	return 0;
}


static void alpha_pci_unmap_sg(struct device *dev, struct scatterlist *sg,
			       int nents, enum dma_data_direction dir,
			       struct dma_attrs *attrs)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	unsigned long flags;
	struct pci_controller *hose;
	struct pci_iommu_arena *arena;
	struct scatterlist *end;
	dma_addr_t max_dma;
	dma_addr_t fbeg, fend;

	if (dir == PCI_DMA_NONE)
		BUG();

	if (! alpha_mv.mv_pci_tbi)
		return;

	hose = pdev ? pdev->sysdata : pci_isa_hose;
	max_dma = pdev ? pdev->dma_mask : ISA_DMA_MASK;
	arena = hose->sg_pci;
	if (!arena || arena->dma_base + arena->size - 1 > max_dma)
		arena = hose->sg_isa;

	fbeg = -1, fend = 0;

	spin_lock_irqsave(&arena->lock, flags);

	for (end = sg + nents; sg < end; ++sg) {
		dma_addr_t addr;
		size_t size;
		long npages, ofs;
		dma_addr_t tend;

		addr = sg->dma_address;
		size = sg->dma_length;
		if (!size)
			break;

		if (addr > 0xffffffff) {
			
			DBGA("    (%ld) DAC [%llx,%zx]\n",
			      sg - end + nents, addr, size);
			continue;
		}

		if (addr >= __direct_map_base
		    && addr < __direct_map_base + __direct_map_size) {
			
			DBGA("    (%ld) direct [%llx,%zx]\n",
			      sg - end + nents, addr, size);
			continue;
		}

		DBGA("    (%ld) sg [%llx,%zx]\n",
		     sg - end + nents, addr, size);

		npages = iommu_num_pages(addr, size, PAGE_SIZE);
		ofs = (addr - arena->dma_base) >> PAGE_SHIFT;
		iommu_arena_free(arena, ofs, npages);

		tend = addr + size - 1;
		if (fbeg > addr) fbeg = addr;
		if (fend < tend) fend = tend;
	}

	if ((fend - arena->dma_base) >> PAGE_SHIFT >= arena->next_entry)
		alpha_mv.mv_pci_tbi(hose, fbeg, fend);

	spin_unlock_irqrestore(&arena->lock, flags);

	DBGA("pci_unmap_sg: %ld entries\n", nents - (end - sg));
}


static int alpha_pci_supported(struct device *dev, u64 mask)
{
	struct pci_dev *pdev = alpha_gendev_to_pci(dev);
	struct pci_controller *hose;
	struct pci_iommu_arena *arena;

	if (__direct_map_size != 0
	    && (__direct_map_base + __direct_map_size - 1 <= mask ||
		__direct_map_base + (max_low_pfn << PAGE_SHIFT) - 1 <= mask))
		return 1;

	
	hose = pdev ? pdev->sysdata : pci_isa_hose;
	arena = hose->sg_isa;
	if (arena && arena->dma_base + arena->size - 1 <= mask)
		return 1;
	arena = hose->sg_pci;
	if (arena && arena->dma_base + arena->size - 1 <= mask)
		return 1;

	
	if (!__direct_map_base && MAX_DMA_ADDRESS - IDENT_ADDR - 1 <= mask)
		return 1;

	return 0;
}


int
iommu_reserve(struct pci_iommu_arena *arena, long pg_count, long align_mask) 
{
	unsigned long flags;
	unsigned long *ptes;
	long i, p;

	if (!arena) return -EINVAL;

	spin_lock_irqsave(&arena->lock, flags);

	
	ptes = arena->ptes;
	p = iommu_arena_find_pages(NULL, arena, pg_count, align_mask);
	if (p < 0) {
		spin_unlock_irqrestore(&arena->lock, flags);
		return -1;
	}

	for (i = 0; i < pg_count; ++i)
		ptes[p+i] = IOMMU_RESERVED_PTE;

	arena->next_entry = p + pg_count;
	spin_unlock_irqrestore(&arena->lock, flags);

	return p;
}

int 
iommu_release(struct pci_iommu_arena *arena, long pg_start, long pg_count)
{
	unsigned long *ptes;
	long i;

	if (!arena) return -EINVAL;

	ptes = arena->ptes;

	
	for(i = pg_start; i < pg_start + pg_count; i++)
		if (ptes[i] != IOMMU_RESERVED_PTE)
			return -EBUSY;

	iommu_arena_free(arena, pg_start, pg_count);
	return 0;
}

int
iommu_bind(struct pci_iommu_arena *arena, long pg_start, long pg_count, 
	   struct page **pages)
{
	unsigned long flags;
	unsigned long *ptes;
	long i, j;

	if (!arena) return -EINVAL;
	
	spin_lock_irqsave(&arena->lock, flags);

	ptes = arena->ptes;

	for(j = pg_start; j < pg_start + pg_count; j++) {
		if (ptes[j] != IOMMU_RESERVED_PTE) {
			spin_unlock_irqrestore(&arena->lock, flags);
			return -EBUSY;
		}
	}
		
	for(i = 0, j = pg_start; i < pg_count; i++, j++)
		ptes[j] = mk_iommu_pte(page_to_phys(pages[i]));

	spin_unlock_irqrestore(&arena->lock, flags);

	return 0;
}

int
iommu_unbind(struct pci_iommu_arena *arena, long pg_start, long pg_count)
{
	unsigned long *p;
	long i;

	if (!arena) return -EINVAL;

	p = arena->ptes + pg_start;
	for(i = 0; i < pg_count; i++)
		p[i] = IOMMU_RESERVED_PTE;

	return 0;
}

static int alpha_pci_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return dma_addr == 0;
}

static int alpha_pci_set_mask(struct device *dev, u64 mask)
{
	if (!dev->dma_mask ||
	    !pci_dma_supported(alpha_gendev_to_pci(dev), mask))
		return -EIO;

	*dev->dma_mask = mask;
	return 0;
}

struct dma_map_ops alpha_pci_ops = {
	.alloc			= alpha_pci_alloc_coherent,
	.free			= alpha_pci_free_coherent,
	.map_page		= alpha_pci_map_page,
	.unmap_page		= alpha_pci_unmap_page,
	.map_sg			= alpha_pci_map_sg,
	.unmap_sg		= alpha_pci_unmap_sg,
	.mapping_error		= alpha_pci_mapping_error,
	.dma_supported		= alpha_pci_supported,
	.set_dma_mask		= alpha_pci_set_mask,
};

struct dma_map_ops *dma_ops = &alpha_pci_ops;
EXPORT_SYMBOL(dma_ops);
