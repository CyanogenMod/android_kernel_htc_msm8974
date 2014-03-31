/*
 *	mcfmmu.h -- definitions for the ColdFire v4e MMU
 *
 *	(C) Copyright 2011,  Greg Ungerer <gerg@uclinux.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#ifndef	MCFMMU_H
#define	MCFMMU_H

#define	MMUBASE		0xfe000000

#define	MMUCR		(MMUBASE + 0x00)	
#define	MMUOR		(MMUBASE + 0x04)	
#define	MMUSR		(MMUBASE + 0x08)	
#define	MMUAR		(MMUBASE + 0x10)	
#define	MMUTR		(MMUBASE + 0x14)	
#define	MMUDR		(MMUBASE + 0x18)	

#define	MMUCR_EN	0x00000001		
#define	MMUCR_ASM	0x00000002		

#define	MMUOR_UAA	0x00000001		
#define	MMUOR_ACC	0x00000002		
#define	MMUOR_RD	0x00000004		
#define	MMUOR_WR	0x00000000		
#define	MMUOR_ADR	0x00000008		
#define	MMUOR_ITLB	0x00000010		
#define	MMUOR_CAS	0x00000020		
#define	MMUOR_CNL	0x00000040		
#define	MMUOR_CA	0x00000080		
#define	MMUOR_STLB	0x00000100		
#define	MMUOR_AAN	16			
#define	MMUOR_AAMASK	0xffff0000		

#define	MMUSR_HIT	0x00000002		
#define	MMUSR_WF	0x00000008		
#define	MMUSR_RF	0x00000010		
#define	MMUSR_SPF	0x00000020		

#define	MMUTR_V		0x00000001		
#define	MMUTR_SG	0x00000002		
#define	MMUTR_IDN	2			
#define	MMUTR_IDMASK	0x000003fc		
#define	MMUTR_VAN	10			
#define	MMUTR_VAMASK	0xfffffc00		

#define	MMUDR_LK	0x00000002		
#define	MMUDR_X		0x00000004		
#define	MMUDR_W		0x00000008		
#define	MMUDR_R		0x00000010		
#define	MMUDR_SP	0x00000020		
#define	MMUDR_CM_CWT	0x00000000		
#define	MMUDR_CM_CCB	0x00000040		
#define	MMUDR_CM_NCP	0x00000080		
#define	MMUDR_CM_NCI	0x000000c0		
#define	MMUDR_SZ_1MB	0x00000000		
#define	MMUDR_SZ_4KB	0x00000100		
#define	MMUDR_SZ_8KB	0x00000200		
#define	MMUDR_SZ_1KB	0x00000300		
#define	MMUDR_PAN	10			
#define	MMUDR_PAMASK	0xfffffc00		

#ifndef __ASSEMBLY__

static inline u32 mmu_read(u32 a)
{
	return *((volatile u32 *) a);
}

static inline void mmu_write(u32 a, u32 v)
{
	*((volatile u32 *) a) = v;
	__asm__ __volatile__ ("nop");
}

int cf_tlb_miss(struct pt_regs *regs, int write, int dtlb, int extension_word);

#endif

#endif	
