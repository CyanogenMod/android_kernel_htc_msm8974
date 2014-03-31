/*
 * Copyright © 2008 Keith Packard <keithp@keithp.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _LINUX_IO_MAPPING_H
#define _LINUX_IO_MAPPING_H

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <asm/io.h>
#include <asm/page.h>


#ifdef CONFIG_HAVE_ATOMIC_IOMAP

#include <asm/iomap.h>

struct io_mapping {
	resource_size_t base;
	unsigned long size;
	pgprot_t prot;
};


static inline struct io_mapping *
io_mapping_create_wc(resource_size_t base, unsigned long size)
{
	struct io_mapping *iomap;
	pgprot_t prot;

	iomap = kmalloc(sizeof(*iomap), GFP_KERNEL);
	if (!iomap)
		goto out_err;

	if (iomap_create_wc(base, size, &prot))
		goto out_free;

	iomap->base = base;
	iomap->size = size;
	iomap->prot = prot;
	return iomap;

out_free:
	kfree(iomap);
out_err:
	return NULL;
}

static inline void
io_mapping_free(struct io_mapping *mapping)
{
	iomap_free(mapping->base, mapping->size);
	kfree(mapping);
}

static inline void __iomem *
io_mapping_map_atomic_wc(struct io_mapping *mapping,
			 unsigned long offset)
{
	resource_size_t phys_addr;
	unsigned long pfn;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;
	pfn = (unsigned long) (phys_addr >> PAGE_SHIFT);
	return iomap_atomic_prot_pfn(pfn, mapping->prot);
}

static inline void
io_mapping_unmap_atomic(void __iomem *vaddr)
{
	iounmap_atomic(vaddr);
}

static inline void __iomem *
io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset)
{
	resource_size_t phys_addr;

	BUG_ON(offset >= mapping->size);
	phys_addr = mapping->base + offset;

	return ioremap_wc(phys_addr, PAGE_SIZE);
}

static inline void
io_mapping_unmap(void __iomem *vaddr)
{
	iounmap(vaddr);
}

#else

#include <linux/uaccess.h>

struct io_mapping;

static inline struct io_mapping *
io_mapping_create_wc(resource_size_t base, unsigned long size)
{
	return (struct io_mapping __force *) ioremap_wc(base, size);
}

static inline void
io_mapping_free(struct io_mapping *mapping)
{
	iounmap((void __force __iomem *) mapping);
}

static inline void __iomem *
io_mapping_map_atomic_wc(struct io_mapping *mapping,
			 unsigned long offset)
{
	pagefault_disable();
	return ((char __force __iomem *) mapping) + offset;
}

static inline void
io_mapping_unmap_atomic(void __iomem *vaddr)
{
	pagefault_enable();
}

static inline void __iomem *
io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset)
{
	return ((char __force __iomem *) mapping) + offset;
}

static inline void
io_mapping_unmap(void __iomem *vaddr)
{
}

#endif 

#endif 
