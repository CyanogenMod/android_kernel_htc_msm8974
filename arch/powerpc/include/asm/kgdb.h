/*
 * The PowerPC (32/64) specific defines / externs for KGDB.  Based on
 * the previous 32bit and 64bit specific files, which had the following
 * copyrights:
 *
 * PPC64 Mods (C) 2005 Frank Rowand (frowand@mvista.com)
 * PPC Mods (C) 2004 Tom Rini (trini@mvista.com)
 * PPC Mods (C) 2003 John Whitney (john.whitney@timesys.com)
 * PPC Mods (C) 1998 Michael Tesch (tesch@cs.wisc.edu)
 *
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Author: Tom Rini <trini@kernel.crashing.org>
 *
 * 2006 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#ifdef __KERNEL__
#ifndef __POWERPC_KGDB_H__
#define __POWERPC_KGDB_H__

#ifndef __ASSEMBLY__

#define BREAK_INSTR_SIZE	4
#define BUFMAX			((NUMREGBYTES * 2) + 512)
#define OUTBUFMAX		((NUMREGBYTES * 2) + 512)
static inline void arch_kgdb_breakpoint(void)
{
	asm(".long 0x7d821008"); 
}
#define CACHE_FLUSH_IS_SAFE	1
#define DBG_MAX_REG_NUM     70

#ifdef CONFIG_PPC64
#define NUMREGBYTES		((68 * 8) + (3 * 4))
#define NUMCRITREGBYTES		184
#else 
#ifndef CONFIG_E500
#define MAXREG			(PT_FPSCR+1)
#else
#define MAXREG                 ((32*2)+6+2+1)
#endif
#define NUMREGBYTES		(MAXREG * sizeof(int))
#define NUMCRITREGBYTES		(23 * sizeof(int))
#endif 
#endif 
#endif 
#endif 
