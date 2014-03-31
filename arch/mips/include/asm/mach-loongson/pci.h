/*
 * Copyright (c) 2008 Zhang Le <r0bertz@gentoo.org>
 * Copyright (c) 2009 Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __ASM_MACH_LOONGSON_PCI_H_
#define __ASM_MACH_LOONGSON_PCI_H_

extern struct pci_ops loongson_pci_ops;

#define LOONGSON_PCI_IO_START	0x00004000UL

#ifdef CONFIG_CPU_SUPPORTS_ADDRWINCFG


#define LOONGSON_CPU_MEM_SRC	0x40000000ul		
#define LOONGSON_PCI_MEM_DST	LOONGSON_CPU_MEM_SRC

#define LOONGSON_PCI_MEM_START	LOONGSON_PCI_MEM_DST
#define LOONGSON_PCI_MEM_END	(0x80000000ul-1)	

#define MMAP_CPUTOPCI_SIZE	(LOONGSON_PCI_MEM_END - \
					LOONGSON_PCI_MEM_START + 1)

#else	

#define LOONGSON_PCI_MEM_START	LOONGSON_PCILO1_BASE
#define LOONGSON_PCI_MEM_END	(LOONGSON_PCILO1_BASE + 0x04000000 * 2)
#define LOONGSON_PCI_IO_START	0x00004000UL

#endif	

#endif 
