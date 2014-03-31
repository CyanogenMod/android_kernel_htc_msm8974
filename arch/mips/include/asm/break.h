/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 2003 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef __ASM_BREAK_H
#define __ASM_BREAK_H

#define BRK_USERBP	0	
#define BRK_KERNELBP	1	
#define BRK_ABORT	2	
#define BRK_BD_TAKEN	3	
#define BRK_BD_NOTTAKEN	4	
#define BRK_SSTEPBP	5	
#define BRK_OVERFLOW	6	
#define BRK_DIVZERO	7	
#define BRK_RANGE	8	
#define BRK_STACKOVERFLOW 9	
#define BRK_NORLD	10	
#define _BRK_THREADBP	11	
#define BRK_BUG		512	
#define BRK_KDB		513	
#define BRK_MEMU	514	
#define BRK_KPROBE_BP	515	
#define BRK_KPROBE_SSTEPBP 516	
#define BRK_MULOVF	1023	

#endif 
