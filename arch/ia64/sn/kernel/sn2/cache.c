/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Copyright (C) 2001-2003, 2006 Silicon Graphics, Inc. All rights reserved.
 *
 */
#include <linux/module.h>
#include <asm/pgalloc.h>
#include <asm/sn/arch.h>

void
sn_flush_all_caches(long flush_addr, long bytes)
{
	unsigned long addr = flush_addr;

	
	if (is_shub1() && (addr & RGN_BITS) == RGN_BASE(RGN_UNCACHED))
		addr = (addr - RGN_BASE(RGN_UNCACHED)) + RGN_BASE(RGN_KERNEL);

	flush_icache_range(addr, addr + bytes);
	flush_icache_range(addr, addr + bytes);
	mb();
}
EXPORT_SYMBOL(sn_flush_all_caches);
