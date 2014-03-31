/*
 * arch/arm/mach-ks8695/include/mach/regs-pci.h
 *
 * Copyright (C) 2006 Ben Dooks <ben@simtec.co.uk>
 * Copyright (C) 2006 Simtec Electronics
 *
 * KS8695 - PCI bridge registers and bit definitions.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#define KS8695_PCI_OFFSET	(0xF0000 + 0x2000)
#define KS8695_PCI_VA		(KS8695_IO_VA + KS8695_PCI_OFFSET)
#define KS8695_PCI_PA		(KS8695_IO_PA + KS8695_PCI_OFFSET)


#define KS8695_CRCFID		(0x000)		
#define KS8695_CRCFCS		(0x004)		
#define KS8695_CRCFRV		(0x008)		
#define KS8695_CRCFLT		(0x00C)		
#define KS8695_CRCBMA		(0x010)		
#define KS8695_CRCSID		(0x02C)		
#define KS8695_CRCFIT		(0x03C)		
#define KS8695_PBCA		(0x100)		
#define KS8695_PBCD		(0x104)		
#define KS8695_PBM		(0x200)		
#define KS8695_PBCS		(0x204)		
#define KS8695_PMBA		(0x208)		
#define KS8695_PMBAC		(0x20C)		
#define KS8695_PMBAM		(0x210)		
#define KS8695_PMBAT		(0x214)		
#define KS8695_PIOBA		(0x218)		
#define KS8695_PIOBAC		(0x21C)		
#define KS8695_PIOBAM		(0x220)		
#define KS8695_PIOBAT		(0x224)		







#define CFRV_GUEST		(1 << 23)

#define PBCA_TYPE1		(1)
#define PBCA_ENABLE		(1 << 31)


