/* MN103E010 on-board DMA controller registers
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#ifndef _ASM_PROC_DMACTL_REGS_H
#define _ASM_PROC_DMACTL_REGS_H

#include <asm/cpu-regs.h>

#ifdef __KERNEL__

#define	DMxCTR(N)		__SYSREG(0xd2000000 + ((N) * 0x100), u32)	
#define	DMxCTR_BG		0x0000001f	
#define	DMxCTR_BG_SOFT		0x00000000	
#define	DMxCTR_BG_SC0TX		0x00000002	
#define	DMxCTR_BG_SC0RX		0x00000003	
#define	DMxCTR_BG_SC1TX		0x00000004	
#define	DMxCTR_BG_SC1RX		0x00000005	
#define	DMxCTR_BG_SC2TX		0x00000006	
#define	DMxCTR_BG_SC2RX		0x00000007	
#define	DMxCTR_BG_TM0UFLOW	0x00000008	
#define	DMxCTR_BG_TM1UFLOW	0x00000009	
#define	DMxCTR_BG_TM2UFLOW	0x0000000a	
#define	DMxCTR_BG_TM3UFLOW	0x0000000b	
#define	DMxCTR_BG_TM6ACMPCAP	0x0000000c	
#define	DMxCTR_BG_AFE		0x0000000d	
#define	DMxCTR_BG_ADC		0x0000000e	
#define	DMxCTR_BG_IRDA		0x0000000f	
#define	DMxCTR_BG_RTC		0x00000010	
#define	DMxCTR_BG_XIRQ0		0x00000011	
#define	DMxCTR_BG_XIRQ1		0x00000012	
#define	DMxCTR_BG_XDMR0		0x00000013	
#define	DMxCTR_BG_XDMR1		0x00000014	
#define	DMxCTR_SAM		0x000000e0	
#define	DMxCTR_SAM_INCR		0x00000000	
#define	DMxCTR_SAM_DECR		0x00000020	
#define	DMxCTR_SAM_FIXED	0x00000040	
#define	DMxCTR_DAM		0x00000000	
#define	DMxCTR_DAM_INCR		0x00000000	
#define	DMxCTR_DAM_DECR		0x00000100	
#define	DMxCTR_DAM_FIXED	0x00000200	
#define	DMxCTR_TM		0x00001800	
#define	DMxCTR_TM_BATCH		0x00000000	
#define	DMxCTR_TM_INTERM	0x00001000	
#define	DMxCTR_UT		0x00006000	
#define	DMxCTR_UT_1		0x00000000	
#define	DMxCTR_UT_2		0x00002000	
#define	DMxCTR_UT_4		0x00004000	
#define	DMxCTR_UT_16		0x00006000	
#define	DMxCTR_TEN		0x00010000	
#define	DMxCTR_RQM		0x00060000	
#define	DMxCTR_RQM_FALLEDGE	0x00000000	
#define	DMxCTR_RQM_RISEEDGE	0x00020000	
#define	DMxCTR_RQM_LOLEVEL	0x00040000	
#define	DMxCTR_RQM_HILEVEL	0x00060000	
#define	DMxCTR_RQF		0x01000000	
#define	DMxCTR_XEND		0x80000000	

#define	DMxSRC(N)		__SYSREG(0xd2000004 + ((N) * 0x100), u32)	

#define	DMxDST(N)		__SYSREG(0xd2000008 + ((N) * 0x100), u32)	

#define	DMxSIZ(N)		__SYSREG(0xd200000c + ((N) * 0x100), u32)	
#define DMxSIZ_CT		0x000fffff	

#define	DMxCYC(N)		__SYSREG(0xd2000010 + ((N) * 0x100), u32)	
#define DMxCYC_CYC		0x000000ff	

#define DM0IRQ			16		
#define DM1IRQ			17		
#define DM2IRQ			18		
#define DM3IRQ			19		

#define	DM0ICR			GxICR(DM0IRQ)	
#define	DM1ICR			GxICR(DM0IR1)	
#define	DM2ICR			GxICR(DM0IR2)	
#define	DM3ICR			GxICR(DM0IR3)	

#ifndef __ASSEMBLY__

struct mn10300_dmactl_regs {
	u32		ctr;
	const void	*src;
	void		*dst;
	u32		siz;
	u32		cyc;
} __attribute__((aligned(0x100)));

#endif 

#endif 

#endif 
