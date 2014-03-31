#ifndef _ASM_POWERPC_OHARE_H
#define _ASM_POWERPC_OHARE_H
#ifdef __KERNEL__
/*
 * ohare.h: definitions for using the "O'Hare" I/O controller chip.
 *
 * Copyright (C) 1997 Paul Mackerras.
 *
 * BenH: Changed to match those of heathrow (but not all of them). Please
 *       check if I didn't break anything (especially the media bay).
 */

#define OHARE_MBCR	0x34
#define OHARE_FCR	0x38

#define OH_SCC_RESET		1
#define OH_BAY_POWER_N		2	
#define OH_BAY_PCI_ENABLE	4	
#define OH_BAY_IDE_ENABLE	8
#define OH_BAY_FLOPPY_ENABLE	0x10
#define OH_IDE0_ENABLE		0x20
#define OH_IDE0_RESET_N		0x40	
#define OH_BAY_DEV_MASK		0x1c
#define OH_BAY_RESET_N		0x80
#define OH_IOBUS_ENABLE		0x100	
#define OH_SCC_ENABLE		0x200
#define OH_MESH_ENABLE		0x400
#define OH_FLOPPY_ENABLE	0x800
#define OH_SCCA_IO		0x4000
#define OH_SCCB_IO		0x8000
#define OH_VIA_ENABLE		0x10000	
#define OH_IDE1_RESET_N		0x800000

#define PBOOK_FEATURES		(OH_IDE_ENABLE | OH_SCC_ENABLE | \
				 OH_MESH_ENABLE | OH_SCCA_IO | OH_SCCB_IO)

#define STARMAX_FEATURES	0xbeff7a

#endif 
#endif 
