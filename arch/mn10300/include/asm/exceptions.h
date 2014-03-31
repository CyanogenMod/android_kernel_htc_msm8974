/* MN10300 Microcontroller core exceptions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_EXCEPTIONS_H
#define _ASM_EXCEPTIONS_H

#include <linux/linkage.h>

#define GDBSTUB_BKPT		0xFF

#ifndef __ASSEMBLY__

enum exception_code {
	EXCEP_RESET		= 0x000000,	

	
	EXCEP_ITLBMISS		= 0x000100,	
	EXCEP_DTLBMISS		= 0x000108,	
	EXCEP_IAERROR		= 0x000110,	
	EXCEP_DAERROR		= 0x000118,	

	
	EXCEP_TRAP		= 0x000128,	
	EXCEP_ISTEP		= 0x000130,	
	EXCEP_IBREAK		= 0x000150,	
	EXCEP_OBREAK		= 0x000158,	
	EXCEP_PRIVINS		= 0x000160,	
	EXCEP_UNIMPINS		= 0x000168,	
	EXCEP_UNIMPEXINS	= 0x000170,	
	EXCEP_MEMERR		= 0x000178,	
	EXCEP_MISALIGN		= 0x000180,	
	EXCEP_BUSERROR		= 0x000188,	
	EXCEP_ILLINSACC		= 0x000190,	
	EXCEP_ILLDATACC		= 0x000198,	
	EXCEP_IOINSACC		= 0x0001a0,	
	EXCEP_PRIVINSACC	= 0x0001a8,	
	EXCEP_PRIVDATACC	= 0x0001b0,	
	EXCEP_DATINSACC		= 0x0001b8,	
	EXCEP_DOUBLE_FAULT	= 0x000200,	

	
	EXCEP_FPU_DISABLED	= 0x0001c0,	
	EXCEP_FPU_UNIMPINS	= 0x0001c8,	
	EXCEP_FPU_OPERATION	= 0x0001d0,	

	
	EXCEP_WDT		= 0x000240,	
	EXCEP_NMI		= 0x000248,	
	EXCEP_IRQ_LEVEL0	= 0x000280,	
	EXCEP_IRQ_LEVEL1	= 0x000288,	
	EXCEP_IRQ_LEVEL2	= 0x000290,	
	EXCEP_IRQ_LEVEL3	= 0x000298,	
	EXCEP_IRQ_LEVEL4	= 0x0002a0,	
	EXCEP_IRQ_LEVEL5	= 0x0002a8,	
	EXCEP_IRQ_LEVEL6	= 0x0002b0,	

	
	EXCEP_SYSCALL0		= 0x000300,	
	EXCEP_SYSCALL1		= 0x000308,	
	EXCEP_SYSCALL2		= 0x000310,	
	EXCEP_SYSCALL3		= 0x000318,	
	EXCEP_SYSCALL4		= 0x000320,	
	EXCEP_SYSCALL5		= 0x000328,	
	EXCEP_SYSCALL6		= 0x000330,	
	EXCEP_SYSCALL7		= 0x000338,	
	EXCEP_SYSCALL8		= 0x000340,	
	EXCEP_SYSCALL9		= 0x000348,	
	EXCEP_SYSCALL10		= 0x000350,	
	EXCEP_SYSCALL11		= 0x000358,	
	EXCEP_SYSCALL12		= 0x000360,	
	EXCEP_SYSCALL13		= 0x000368,	
	EXCEP_SYSCALL14		= 0x000370,	
	EXCEP_SYSCALL15		= 0x000378,	
};

extern void __set_intr_stub(enum exception_code code, void *handler);
extern void set_intr_stub(enum exception_code code, void *handler);

struct pt_regs;

extern asmlinkage void __common_exception(void);
extern asmlinkage void itlb_miss(void);
extern asmlinkage void dtlb_miss(void);
extern asmlinkage void itlb_aerror(void);
extern asmlinkage void dtlb_aerror(void);
extern asmlinkage void raw_bus_error(void);
extern asmlinkage void double_fault(void);
extern asmlinkage int  system_call(struct pt_regs *);
extern asmlinkage void nmi(struct pt_regs *, enum exception_code);
extern asmlinkage void uninitialised_exception(struct pt_regs *,
					       enum exception_code);
extern asmlinkage void irq_handler(void);
extern asmlinkage void profile_handler(void);
extern asmlinkage void nmi_handler(void);
extern asmlinkage void misalignment(struct pt_regs *, enum exception_code);

extern void die(const char *, struct pt_regs *, enum exception_code)
	__noreturn;

extern int die_if_no_fixup(const char *, struct pt_regs *, enum exception_code);

#define NUM2EXCEP_IRQ_LEVEL(num)	(EXCEP_IRQ_LEVEL0 + (num) * 8)

#endif 

#endif 
