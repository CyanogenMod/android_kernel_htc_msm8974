/*
 * arch/arm/mach-at91/include/mach/at91rm9200_emac.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Ethernet MAC registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91RM9200_EMAC_H
#define AT91RM9200_EMAC_H

#define	AT91_EMAC_CTL		0x00	
#define		AT91_EMAC_LB		(1 <<  0)	
#define		AT91_EMAC_LBL		(1 <<  1)	
#define		AT91_EMAC_RE		(1 <<  2)	
#define		AT91_EMAC_TE		(1 <<  3)	
#define		AT91_EMAC_MPE		(1 <<  4)	
#define		AT91_EMAC_CSR		(1 <<  5)	
#define		AT91_EMAC_INCSTAT	(1 <<  6)	
#define		AT91_EMAC_WES		(1 <<  7)	
#define		AT91_EMAC_BP		(1 <<  8)	

#define	AT91_EMAC_CFG		0x04	
#define		AT91_EMAC_SPD		(1 <<  0)	
#define		AT91_EMAC_FD		(1 <<  1)	
#define		AT91_EMAC_BR		(1 <<  2)	
#define		AT91_EMAC_CAF		(1 <<  4)	
#define		AT91_EMAC_NBC		(1 <<  5)	
#define		AT91_EMAC_MTI		(1 <<  6)	
#define		AT91_EMAC_UNI		(1 <<  7)	
#define		AT91_EMAC_BIG		(1 <<  8)	
#define		AT91_EMAC_EAE		(1 <<  9)	
#define		AT91_EMAC_CLK		(3 << 10)	
#define		AT91_EMAC_CLK_DIV8		(0 << 10)
#define		AT91_EMAC_CLK_DIV16		(1 << 10)
#define		AT91_EMAC_CLK_DIV32		(2 << 10)
#define		AT91_EMAC_CLK_DIV64		(3 << 10)
#define		AT91_EMAC_RTY		(1 << 12)	
#define		AT91_EMAC_RMII		(1 << 13)	

#define	AT91_EMAC_SR		0x08	
#define		AT91_EMAC_SR_LINK	(1 <<  0)	
#define		AT91_EMAC_SR_MDIO	(1 <<  1)	
#define		AT91_EMAC_SR_IDLE	(1 <<  2)	

#define	AT91_EMAC_TAR		0x0c	

#define	AT91_EMAC_TCR		0x10	
#define		AT91_EMAC_LEN		(0x7ff << 0)	
#define		AT91_EMAC_NCRC		(1     << 15)	

#define	AT91_EMAC_TSR		0x14	
#define		AT91_EMAC_TSR_OVR	(1 <<  0)	
#define		AT91_EMAC_TSR_COL	(1 <<  1)	
#define		AT91_EMAC_TSR_RLE	(1 <<  2)	
#define		AT91_EMAC_TSR_IDLE	(1 <<  3)	
#define		AT91_EMAC_TSR_BNQ	(1 <<  4)	
#define		AT91_EMAC_TSR_COMP	(1 <<  5)	
#define		AT91_EMAC_TSR_UND	(1 <<  6)	

#define	AT91_EMAC_RBQP		0x18	

#define	AT91_EMAC_RSR		0x20	
#define		AT91_EMAC_RSR_BNA	(1 <<  0)	
#define		AT91_EMAC_RSR_REC	(1 <<  1)	
#define		AT91_EMAC_RSR_OVR	(1 <<  2)	

#define	AT91_EMAC_ISR		0x24	
#define		AT91_EMAC_DONE		(1 <<  0)	
#define		AT91_EMAC_RCOM		(1 <<  1)	
#define		AT91_EMAC_RBNA		(1 <<  2)	
#define		AT91_EMAC_TOVR		(1 <<  3)	
#define		AT91_EMAC_TUND		(1 <<  4)	
#define		AT91_EMAC_RTRY		(1 <<  5)	
#define		AT91_EMAC_TBRE		(1 <<  6)	
#define		AT91_EMAC_TCOM		(1 <<  7)	
#define		AT91_EMAC_TIDLE		(1 <<  8)	
#define		AT91_EMAC_LINK		(1 <<  9)	
#define		AT91_EMAC_ROVR		(1 << 10)	
#define		AT91_EMAC_ABT		(1 << 11)	

#define	AT91_EMAC_IER		0x28	
#define	AT91_EMAC_IDR		0x2c	
#define	AT91_EMAC_IMR		0x30	

#define	AT91_EMAC_MAN		0x34	
#define		AT91_EMAC_DATA		(0xffff << 0)	
#define		AT91_EMAC_REGA		(0x1f	<< 18)	
#define		AT91_EMAC_PHYA		(0x1f	<< 23)	
#define		AT91_EMAC_RW		(3	<< 28)	
#define			AT91_EMAC_RW_W		(1 << 28)
#define			AT91_EMAC_RW_R		(2 << 28)
#define		AT91_EMAC_MAN_802_3	0x40020000	

#define AT91_EMAC_FRA		0x40	
#define AT91_EMAC_SCOL		0x44	
#define AT91_EMAC_MCOL		0x48	
#define AT91_EMAC_OK		0x4c	
#define AT91_EMAC_SEQE		0x50	
#define AT91_EMAC_ALE		0x54	
#define AT91_EMAC_DTE		0x58	
#define AT91_EMAC_LCOL		0x5c	
#define AT91_EMAC_ECOL		0x60	
#define AT91_EMAC_TUE		0x64	
#define AT91_EMAC_CSE		0x68	
#define AT91_EMAC_DRFC		0x6c	
#define AT91_EMAC_ROV		0x70	
#define AT91_EMAC_CDE		0x74	
#define AT91_EMAC_ELR		0x78	
#define AT91_EMAC_RJB		0x7c	
#define AT91_EMAC_USF		0x80	
#define AT91_EMAC_SQEE		0x84	

#define AT91_EMAC_HSL		0x90	
#define AT91_EMAC_HSH		0x94	
#define AT91_EMAC_SA1L		0x98	
#define AT91_EMAC_SA1H		0x9c	
#define AT91_EMAC_SA2L		0xa0	
#define AT91_EMAC_SA2H		0xa4	
#define AT91_EMAC_SA3L		0xa8	
#define AT91_EMAC_SA3H		0xac	
#define AT91_EMAC_SA4L		0xb0	
#define AT91_EMAC_SA4H		0xb4	

#endif
