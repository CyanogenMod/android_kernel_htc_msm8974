/*
 * arch/arm/mach-ks8695/include/mach/regs-lan.h
 *
 * Copyright (C) 2006 Andrew Victor
 *
 * KS8695 - LAN Registers and bit definitions.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_LAN_H
#define KS8695_LAN_H

#define KS8695_LAN_OFFSET	(0xF0000 + 0x8000)
#define KS8695_LAN_VA		(KS8695_IO_VA + KS8695_LAN_OFFSET)
#define KS8695_LAN_PA		(KS8695_IO_PA + KS8695_LAN_OFFSET)


#define KS8695_LMDTXC		(0x00)		
#define KS8695_LMDRXC		(0x04)		
#define KS8695_LMDTSC		(0x08)		
#define KS8695_LMDRSC		(0x0c)		
#define KS8695_LTDLB		(0x10)		
#define KS8695_LRDLB		(0x14)		
#define KS8695_LMAL		(0x18)		
#define KS8695_LMAH		(0x1c)		
#define KS8695_LMAAL(n)		(0x80 + ((n)*8))	
#define KS8695_LMAAH(n)		(0x84 + ((n)*8))	


#define LMDTXC_LMTRST		(1    << 31)	
#define LMDTXC_LMTBS		(0x3f << 24)	
#define LMDTXC_LMTUCG		(1    << 18)	
#define LMDTXC_LMTTCG		(1    << 17)	
#define LMDTXC_LMTICG		(1    << 16)	
#define LMDTXC_LMTFCE		(1    <<  9)	
#define LMDTXC_LMTLB		(1    <<  8)	
#define LMDTXC_LMTEP		(1    <<  2)	
#define LMDTXC_LMTAC		(1    <<  1)	
#define LMDTXC_LMTE		(1    <<  0)	

#define LMDRXC_LMRBS		(0x3f << 24)	
#define LMDRXC_LMRUCC		(1    << 18)	
#define LMDRXC_LMRTCG		(1    << 17)	
#define LMDRXC_LMRICG		(1    << 16)	
#define LMDRXC_LMRFCE		(1    <<  9)	
#define LMDRXC_LMRB		(1    <<  6)	
#define LMDRXC_LMRM		(1    <<  5)	
#define LMDRXC_LMRU		(1    <<  4)	
#define LMDRXC_LMRERR		(1    <<  3)	
#define LMDRXC_LMRA		(1    <<  2)	
#define LMDRXC_LMRE		(1    <<  1)	

#define LMAAH_E			(1    << 31)	


#endif
