/*
 *  linux/arch/h8300/mm/kmap.c
 *  
 *  Based on
 *  linux/arch/m68knommu/mm/kmap.c
 *
 *  Copyright (C) 2000 Lineo, <davidm@snapgear.com>
 *  Copyright (C) 2000-2002 David McCullough <davidm@snapgear.com>
 */

#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/io.h>

#undef DEBUG

#define VIRT_OFFSET (0x01000000)

void *__ioremap(unsigned long physaddr, unsigned long size, int cacheflag)
{
	return (void *)(physaddr + VIRT_OFFSET);
}

void iounmap(void *addr)
{
}

void __iounmap(void *addr, unsigned long size)
{
}

void kernel_set_cachemode(void *addr, unsigned long size, int cmode)
{
}
