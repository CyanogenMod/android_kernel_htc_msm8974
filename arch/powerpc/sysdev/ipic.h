/*
 * IPIC private definitions and structure.
 *
 * Maintainer: Kumar Gala <galak@kernel.crashing.org>
 *
 * Copyright 2005 Freescale Semiconductor, Inc
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __IPIC_H__
#define __IPIC_H__

#include <asm/ipic.h>

#define NR_IPIC_INTS 128

#define IPIC_IRQ_EXT0 48
#define IPIC_IRQ_EXT1 17
#define IPIC_IRQ_EXT7 23

#define IPIC_PRIORITY_DEFAULT 0x05309770

#define	SICFR_IPSA	0x00010000
#define	SICFR_IPSB	0x00020000
#define	SICFR_IPSC	0x00040000
#define	SICFR_IPSD	0x00080000
#define	SICFR_MPSA	0x00200000
#define	SICFR_MPSB	0x00400000

#define	SEMSR_SIRQ0	0x00008000

#define SERCR_MCPR	0x00000001

struct ipic {
	volatile u32 __iomem	*regs;

	
	struct irq_domain		*irqhost;
};

struct ipic_info {
	u8	ack;		
	u8	mask;		
	u8	prio;		
	u8	force;		
	u8	bit;		
	u8	prio_mask;	
};

#endif 
