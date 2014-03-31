/*
 * Copyright 2007-2009 Analog Devices Inc.
 *                         Philippe Gerum <rpm@xenomai.org>
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _ASM_BLACKFIN_PDA_H
#define _ASM_BLACKFIN_PDA_H

#include <mach/anomaly.h>

#ifndef __ASSEMBLY__

struct blackfin_pda {			
	struct blackfin_pda *next;

	unsigned long syscfg;
#ifdef CONFIG_SMP
	unsigned long imask;		
#endif

	unsigned long *ipdt;		
	unsigned long *ipdt_swapcount;	
	unsigned long *dpdt;		
	unsigned long *dpdt_swapcount;	

	unsigned long ex_iptr;
	unsigned long ex_optr;
	unsigned long ex_buf[4];
	unsigned long ex_imask;		
	unsigned long ex_ipend;		
	unsigned long *ex_stack;	

#ifdef ANOMALY_05000261
	unsigned long last_cplb_fault_retx;
#endif
	unsigned long dcplb_fault_addr;
	unsigned long icplb_fault_addr;
	unsigned long retx;
	unsigned long seqstat;
	unsigned int __nmi_count;	
#ifdef CONFIG_DEBUG_DOUBLEFAULT
	unsigned long dcplb_doublefault_addr;
	unsigned long icplb_doublefault_addr;
	unsigned long retx_doublefault;
	unsigned long seqstat_doublefault;
#endif
};

struct blackfin_initial_pda {
	void *retx;
#ifdef CONFIG_DEBUG_DOUBLEFAULT
	void *dcplb_doublefault_addr;
	void *icplb_doublefault_addr;
	void *retx_doublefault;
	unsigned seqstat_doublefault;
#endif
};

extern struct blackfin_pda cpu_pda[];

#endif	

#endif 
