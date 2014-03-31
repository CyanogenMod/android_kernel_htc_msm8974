/*
 * reloc_table.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _RELOC_TABLE_H_
#define _RELOC_TABLE_H_
#include <linux/types.h>

#define ROP_N	0		
#define ROP_R	1		
#define ROP_W	2		
#define ROP_RW	3		

#define ROP_ANY	0		
#define ROP_SGN	1		
#define ROP_UNS	2		
#define ROP_MAX 3	

#define ROP_IGN	0		
#define ROP_LIT 0		
#define ROP_SYM	1		
#define ROP_SYMD 2		

#define RSTK_N 0		
#define RSTK_POP 1		
#define RSTK_UOP 2		
#define RSTK_PSH 3		

enum dload_actions {
	
	RACT_VAL,
	
	RACT_ASGN,
	RACT_ADD,		
	RACT_PCR,		
	RACT_ADDISP,		
	RACT_ASGPC,		

	RACT_PLUS,		
	RACT_SUB,		
	RACT_NEG,		

	RACT_MPY,		
	RACT_DIV,		
	RACT_MOD,		

	RACT_SR,		
	RACT_ASR,		
	RACT_SL,		
	RACT_AND,		
	RACT_OR,		
	RACT_XOR,		
	RACT_NOT,		
	RACT_C6SECT,		
	RACT_C6BASE,		
	RACT_C6DSPL,		
	RACT_PCR23T		
};

#define RFV_POSN(aaa) ((aaa) & 0xF)
#define RFV_WIDTH(aaa) (((aaa) >> 4) & 0x3F)
#define RFV_ACTION(aaa) ((aaa) >> 10)

#define RFV_SIGN(iii) (((iii) >> 2) & 0x3)
#define RFV_SYM(iii) (((iii) >> 4) & 0x3)
#define RFV_STK(iii) (((iii) >> 6) & 0x3)
#define RFV_ACCS(iii) ((iii) & 0x3)

#if (TMS32060)
#define RFV_SCALE(iii) ((iii) >> 11)
#define RFV_BIGOFF(iii) (((iii) >> 8) & 0x7)
#else
#define RFV_BIGOFF(iii) ((iii) >> 8)
#endif

#endif 
