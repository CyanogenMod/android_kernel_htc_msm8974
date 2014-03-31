/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <asm/tlbflush.h>
#include <asm/homecache.h>


void *dma_alloc_coherent(struct device *dev,
			 size_t size,
			 dma_addr_t *dma_handle,
			 gfp_t gfp)
{
	u64 dma_mask = dev->coherent_dma_mask ?: DMA_BIT_MASK(32);
	int node = dev_to_node(dev);
	int order = get_order(size);
	struct page *pg;
	dma_addr_t addr;

	gfp |= __GFP_ZERO;

	if (dma_mask <= DMA_BIT_MASK(32))
		node = 0;

	pg = homecache_alloc_pages_node(node, gfp, order, PAGE_HOME_UNCACHED);
	if (pg == NULL)
		return NULL;

	addr = page_to_phys(pg);
	if (addr + size > dma_mask) {
		homecache_free_pages(addr, order);
		return NULL;
	}

	*dma_handle = addr;
	return page_address(pg);
}
EXPORT_SYMBOL(dma_alloc_coherent);

void dma_free_coherent(struct device *dev, size_t size,
		  void *vaddr, dma_addr_t dma_handle)
{
	homecache_free_pages((unsigned long)vaddr, get_order(size));
}
EXPORT_SYMBOL(dma_free_coherent);


static void __dma_map_pa_range(dma_addr_t dma_addr, size_t size)
{
	struct page *page = pfn_to_page(PFN_DOWN(dma_addr));
	size_t bytesleft = PAGE_SIZE - (dma_addr & (PAGE_SIZE - 1));

	while ((ssize_t)size > 0) {
		
		homecache_flush_cache(page++, 0);

		
		size -= bytesleft;
		bytesleft = PAGE_SIZE;
	}
}

dma_addr_t dma_map_single(struct device *dev, void *ptr, size_t size,
	       enum dma_data_direction direction)
{
	dma_addr_t dma_addr = __pa(ptr);

	BUG_ON(!valid_dma_direction(direction));
	WARN_ON(size == 0);

	__dma_map_pa_range(dma_addr, size);

	return dma_addr;
}
EXPORT_SYMBOL(dma_map_single);

void dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size,
		 enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));
}
EXPORT_SYMBOL(dma_unmap_single);

int dma_map_sg(struct device *dev, struct scatterlist *sglist, int nents,
	   enum dma_data_direction direction)
{
	struct scatterlist *sg;
	int i;

	BUG_ON(!valid_dma_direction(direction));

	WARN_ON(nents == 0 || sglist->length == 0);

	for_each_sg(sglist, sg, nents, i) {
		sg->dma_address = sg_phys(sg);
		__dma_map_pa_range(sg->dma_address, sg->length);
	}

	return nents;
}
EXPORT_SYMBOL(dma_map_sg);

void dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries,
	     enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));
}
EXPORT_SYMBOL(dma_unmap_sg);

dma_addr_t dma_map_page(struct device *dev, struct page *page,
			unsigned long offset, size_t size,
			enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));

	BUG_ON(offset + size > PAGE_SIZE);
	homecache_flush_cache(page, 0);

	return page_to_pa(page) + offset;
}
EXPORT_SYMBOL(dma_map_page);

void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
	       enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));
}
EXPORT_SYMBOL(dma_unmap_page);

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
			     size_t size, enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));
}
EXPORT_SYMBOL(dma_sync_single_for_cpu);

void dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
				size_t size, enum dma_data_direction direction)
{
	unsigned long start = PFN_DOWN(dma_handle);
	unsigned long end = PFN_DOWN(dma_handle + size - 1);
	unsigned long i;

	BUG_ON(!valid_dma_direction(direction));
	for (i = start; i <= end; ++i)
		homecache_flush_cache(pfn_to_page(i), 0);
}
EXPORT_SYMBOL(dma_sync_single_for_device);

void dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems,
		    enum dma_data_direction direction)
{
	BUG_ON(!valid_dma_direction(direction));
	WARN_ON(nelems == 0 || sg[0].length == 0);
}
EXPORT_SYMBOL(dma_sync_sg_for_cpu);

void dma_sync_sg_for_device(struct device *dev, struct scatterlist *sglist,
			    int nelems, enum dma_data_direction direction)
{
	struct scatterlist *sg;
	int i;

	BUG_ON(!valid_dma_direction(direction));
	WARN_ON(nelems == 0 || sglist->length == 0);

	for_each_sg(sglist, sg, nelems, i) {
		dma_sync_single_for_device(dev, sg->dma_address,
					   sg_dma_len(sg), direction);
	}
}
EXPORT_SYMBOL(dma_sync_sg_for_device);

void dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma_handle,
				   unsigned long offset, size_t size,
				   enum dma_data_direction direction)
{
	dma_sync_single_for_cpu(dev, dma_handle + offset, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_range_for_cpu);

void dma_sync_single_range_for_device(struct device *dev,
				      dma_addr_t dma_handle,
				      unsigned long offset, size_t size,
				      enum dma_data_direction direction)
{
	dma_sync_single_for_device(dev, dma_handle + offset, size, direction);
}
EXPORT_SYMBOL(dma_sync_single_range_for_device);

void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
		    enum dma_data_direction direction)
{
}
EXPORT_SYMBOL(dma_cache_sync);
