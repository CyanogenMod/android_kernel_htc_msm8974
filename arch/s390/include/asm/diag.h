/*
 * s390 diagnose functions
 *
 * Copyright IBM Corp. 2007
 * Author(s): Michael Holzheu <holzheu@de.ibm.com>
 */

#ifndef _ASM_S390_DIAG_H
#define _ASM_S390_DIAG_H

static inline void diag10_range(unsigned long start_pfn, unsigned long num_pfn)
{
	unsigned long start_addr, end_addr;

	start_addr = start_pfn << PAGE_SHIFT;
	end_addr = (start_pfn + num_pfn - 1) << PAGE_SHIFT;

	asm volatile(
		"0:	diag	%0,%1,0x10\n"
		"1:\n"
		EX_TABLE(0b, 1b)
		EX_TABLE(1b, 1b)
		: : "a" (start_addr), "a" (end_addr));
}

extern int diag14(unsigned long rx, unsigned long ry1, unsigned long subcode);

struct diag210 {
	u16 vrdcdvno;	
	u16 vrdclen;	
	u8 vrdcvcla;	
	u8 vrdcvtyp;	
	u8 vrdcvsta;	
	u8 vrdcvfla;	
	u8 vrdcrccl;	
	u8 vrdccrty;	
	u8 vrdccrmd;	
	u8 vrdccrft;	
} __attribute__((packed, aligned(4)));

extern int diag210(struct diag210 *addr);

#endif 
