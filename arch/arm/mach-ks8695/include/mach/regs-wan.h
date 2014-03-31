/*
 * arch/arm/mach-ks8695/include/mach/regs-wan.h
 *
 * Copyright (C) 2006 Andrew Victor
 *
 * KS8695 - WAN Registers and bit definitions.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_WAN_H
#define KS8695_WAN_H

#define KS8695_WAN_OFFSET	(0xF0000 + 0x6000)
#define KS8695_WAN_VA		(KS8695_IO_VA + KS8695_WAN_OFFSET)
#define KS8695_WAN_PA		(KS8695_IO_PA + KS8695_WAN_OFFSET)


#define KS8695_WMDTXC		(0x00)		
#define KS8695_WMDRXC		(0x04)		
#define KS8695_WMDTSC		(0x08)		
#define KS8695_WMDRSC		(0x0c)		
#define KS8695_WTDLB		(0x10)		
#define KS8695_WRDLB		(0x14)		
#define KS8695_WMAL		(0x18)		
#define KS8695_WMAH		(0x1c)		
#define KS8695_WMAAL(n)		(0x80 + ((n)*8))	
#define KS8695_WMAAH(n)		(0x84 + ((n)*8))	


#define WMDTXC_WMTRST		(1    << 31)	
#define WMDTXC_WMTBS		(0x3f << 24)	
#define WMDTXC_WMTUCG		(1    << 18)	
#define WMDTXC_WMTTCG		(1    << 17)	
#define WMDTXC_WMTICG		(1    << 16)	
#define WMDTXC_WMTFCE		(1    <<  9)	
#define WMDTXC_WMTLB		(1    <<  8)	
#define WMDTXC_WMTEP		(1    <<  2)	
#define WMDTXC_WMTAC		(1    <<  1)	
#define WMDTXC_WMTE		(1    <<  0)	

#define WMDRXC_WMRBS		(0x3f << 24)	
#define WMDRXC_WMRUCC		(1    << 18)	
#define WMDRXC_WMRTCG		(1    << 17)	
#define WMDRXC_WMRICG		(1    << 16)	
#define WMDRXC_WMRFCE		(1    <<  9)	
#define WMDRXC_WMRB		(1    <<  6)	
#define WMDRXC_WMRM		(1    <<  5)	
#define WMDRXC_WMRU		(1    <<  4)	
#define WMDRXC_WMRERR		(1    <<  3)	
#define WMDRXC_WMRA		(1    <<  2)	
#define WMDRXC_WMRE		(1    <<  0)	

#define WMAAH_E			(1    << 31)	


#endif
