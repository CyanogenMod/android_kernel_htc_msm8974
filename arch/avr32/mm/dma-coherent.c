/*
 *  Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/export.h>

#include <asm/addrspace.h>
#include <asm/cacheflush.h>

void dma_cache_sync(struct device *dev, void *vaddr, size_t size, int direction)
{
	if (PXSEG(vaddr) == P2SEG)
		return;

	switch (direction) {
	case DMA_FROM_DEVICE:		
		invalidate_dcache_region(vaddr, size);
		break;
	case DMA_TO_DEVICE:		
		clean_dcache_region(vaddr, size);
		break;
	case DMA_BIDIRECTIONAL:		
		flush_dcache_region(vaddr, size);
		break;
	default:
		BUG();
	}
}
EXPORT_SYMBOL(dma_cache_sync);

static struct page *__dma_alloc(struct device *dev, size_t size,
				dma_addr_t *handle, gfp_t gfp)
{
	struct page *page, *free, *end;
	int order;

	gfp &= ~(__GFP_COMP);

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		return NULL;
	split_page(page, order);

	invalidate_dcache_region(phys_to_virt(page_to_phys(page)), size);

	*handle = page_to_bus(page);
	free = page + (size >> PAGE_SHIFT);
	end = page + (1 << order);

	while (free < end) {
		__free_page(free);
		free++;
	}

	return page;
}

static void __dma_free(struct device *dev, size_t size,
		       struct page *page, dma_addr_t handle)
{
	struct page *end = page + (PAGE_ALIGN(size) >> PAGE_SHIFT);

	while (page < end)
		__free_page(page++);
}

void *dma_alloc_coherent(struct device *dev, size_t size,
			 dma_addr_t *handle, gfp_t gfp)
{
	struct page *page;
	void *ret = NULL;

	page = __dma_alloc(dev, size, handle, gfp);
	if (page)
		ret = phys_to_uncached(page_to_phys(page));

	return ret;
}
EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_coherent(struct device *dev, size_t size,
		       void *cpu_addr, dma_addr_t handle)
{
	void *addr = phys_to_cached(uncached_to_phys(cpu_addr));
	struct page *page;

	pr_debug("dma_free_coherent addr %p (phys %08lx) size %u\n",
		 cpu_addr, (unsigned long)handle, (unsigned)size);
	BUG_ON(!virt_addr_valid(addr));
	page = virt_to_page(addr);
	__dma_free(dev, size, page, handle);
}
EXPORT_SYMBOL(dma_free_coherent);

void *dma_alloc_writecombine(struct device *dev, size_t size,
			     dma_addr_t *handle, gfp_t gfp)
{
	struct page *page;
	dma_addr_t phys;

	page = __dma_alloc(dev, size, handle, gfp);
	if (!page)
		return NULL;

	phys = page_to_phys(page);
	*handle = phys;

	
	return __ioremap(phys, size, _PAGE_BUFFER);
}
EXPORT_SYMBOL(dma_alloc_writecombine);

void dma_free_writecombine(struct device *dev, size_t size,
			   void *cpu_addr, dma_addr_t handle)
{
	struct page *page;

	iounmap(cpu_addr);

	page = phys_to_page(handle);
	__dma_free(dev, size, page, handle);
}
EXPORT_SYMBOL(dma_free_writecombine);
