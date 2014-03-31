/*
 * Co-processor register definitions for PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2012 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __UNICORE_HWDEF_COPRO_H__
#define __UNICORE_HWDEF_COPRO_H__

#define CR_M	(1 << 0)	
#define CR_A	(1 << 1)	
#define CR_D	(1 << 2)	
#define CR_I	(1 << 3)	
#define CR_B	(1 << 4)	
#define CR_T	(1 << 5)	
#define CR_V	(1 << 13)	

#ifndef __ASSEMBLY__

#define vectors_high()		(cr_alignment & CR_V)

extern unsigned long cr_no_alignment;	
extern unsigned long cr_alignment;	

static inline unsigned int get_cr(void)
{
	unsigned int val;
	asm("movc %0, p0.c1, #0" : "=r" (val) : : "cc");
	return val;
}

static inline void set_cr(unsigned int val)
{
	asm volatile("movc p0.c1, %0, #0" : : "r" (val) : "cc");
	isb();
}

extern void adjust_cr(unsigned long mask, unsigned long set);

#endif 

#endif 
