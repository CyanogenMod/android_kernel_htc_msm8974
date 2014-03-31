/*
 *  include/asm-ppc/gg2.h -- VLSI VAS96011/12 `Golden Gate 2' register definitions
 *
 *  Copyright (C) 1997 Geert Uytterhoeven
 *
 *  This file is based on the following documentation:
 *
 *	The VAS96011/12 Chipset, Data Book, Edition 1.0
 *	VLSI Technology, Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#ifndef _ASMPPC_GG2_H
#define _ASMPPC_GG2_H


#define GG2_PCI_MEM_BASE	0xc0000000	
#define GG2_ISA_MEM_BASE	0xf7000000	
#define GG2_ISA_IO_BASE		0xf8000000	
#define GG2_PCI_CONFIG_BASE	0xfec00000	
#define GG2_INT_ACK_SPECIAL	0xfec80000	
						
#define GG2_ROM_BASE0		0xff000000	
#define GG2_ROM_BASE1		0xff800000	



extern void __iomem *gg2_pci_config_base;	

#define GG2_PCI_BUSNO		0x40	
#define GG2_PCI_SUBBUSNO	0x41	
#define GG2_PCI_DISCCTR		0x42	
#define GG2_PCI_PPC_CTRL	0x50	
#define GG2_PCI_ADDR_MAP	0x5c	
#define GG2_PCI_PCI_CTRL	0x60	
#define GG2_PCI_ROM_CTRL	0x70	
#define GG2_PCI_ROM_TIME	0x74	
#define GG2_PCI_CC_CTRL		0x80	
#define GG2_PCI_DRAM_BANK0	0x90	
#define GG2_PCI_DRAM_BANK1	0x94	
#define GG2_PCI_DRAM_BANK2	0x98	
#define GG2_PCI_DRAM_BANK3	0x9c	
#define GG2_PCI_DRAM_BANK4	0xa0	
#define GG2_PCI_DRAM_BANK5	0xa4	
#define GG2_PCI_DRAM_TIME0	0xb0	
#define GG2_PCI_DRAM_TIME1	0xb4	
#define GG2_PCI_DRAM_CTRL	0xc0	
#define GG2_PCI_ERR_CTRL	0xd0	
#define GG2_PCI_ERR_STATUS	0xd4	
					

#endif 
