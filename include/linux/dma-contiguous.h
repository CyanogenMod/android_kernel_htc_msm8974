#ifndef __LINUX_CMA_H
#define __LINUX_CMA_H

/*
 * Contiguous Memory Allocator for DMA mapping framework
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */


#ifdef __KERNEL__

struct cma;
struct page;
struct device;

#ifdef CONFIG_CMA

#define MAX_CMA_AREAS	(1 + CONFIG_CMA_AREAS)


phys_addr_t cma_get_base(struct device *dev);
bool cma_area_exist(struct device *dev);

extern struct cma *dma_contiguous_def_area;

void dma_contiguous_reserve(phys_addr_t addr_limit);

int dma_contiguous_reserve_area(phys_addr_t size, phys_addr_t *res_base,
				  phys_addr_t limit, const char *name);

int dma_contiguous_add_device(struct device *dev, phys_addr_t base);


static inline int dma_declare_contiguous(struct device *dev, phys_addr_t size,
					 phys_addr_t base, phys_addr_t limit)
{
	int ret;
	ret = dma_contiguous_reserve_area(size, &base, limit, NULL);
	if (ret == 0)
		ret = dma_contiguous_add_device(dev, base);
	return ret;
}

struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int order);
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count);

#else

#define MAX_CMA_AREAS	(0)

static inline void dma_contiguous_reserve(phys_addr_t limit) { }

static inline
int dma_declare_contiguous(struct device *dev, phys_addr_t size,
			   phys_addr_t base, phys_addr_t limit)
{
	return -ENOSYS;
}

static inline
struct page *dma_alloc_from_contiguous(struct device *dev, int count,
				       unsigned int order)
{
	return NULL;
}

static inline
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count)
{
	return false;
}


static inline phys_addr_t cma_get_base(struct device *dev)
{
	return 0;
}

static inline bool cma_area_exist(struct device *dev)
{
	return false;
}

#endif

#endif

#endif
