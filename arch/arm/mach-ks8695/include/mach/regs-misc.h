/*
 * arch/arm/mach-ks8695/include/mach/regs-misc.h
 *
 * Copyright (C) 2006 Andrew Victor
 *
 * KS8695 - Miscellaneous Registers
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_MISC_H
#define KS8695_MISC_H

#define KS8695_MISC_OFFSET	(0xF0000 + 0xEA00)
#define KS8695_MISC_VA		(KS8695_IO_VA + KS8695_MISC_OFFSET)
#define KS8695_MISC_PA		(KS8695_IO_PA + KS8695_MISC_OFFSET)


#define KS8695_DID		(0x00)		
#define KS8695_RID		(0x04)		
#define KS8695_HMC		(0x08)		
#define KS8695_WMC		(0x0c)		
#define KS8695_WPPM		(0x10)		
#define KS8695_PPS		(0x1c)		

#define DID_ID			(0xffff << 0)	

#define RID_SUBID		(0xf << 4)	
#define RID_REVISION		(0xf << 0)	

#define HMC_HSS			(1 << 1)	
#define HMC_HDS			(1 << 0)	

#define WMC_WANC		(1 << 30)	
#define WMC_WANR		(1 << 29)	
#define WMC_WANAP		(1 << 28)	
#define WMC_WANA100F		(1 << 27)	
#define WMC_WANA100H		(1 << 26)	
#define WMC_WANA10F		(1 << 25)	
#define WMC_WANA10H		(1 << 24)	
#define WMC_WLS			(1 << 23)	
#define WMC_WDS			(1 << 22)	
#define WMC_WSS			(1 << 21)	
#define WMC_WLPP		(1 << 20)	
#define WMC_WLP100F		(1 << 19)	
#define WMC_WLP100H		(1 << 18)	
#define WMC_WLP10F		(1 << 17)	
#define WMC_WLP10H		(1 << 16)	
#define WMC_WAND		(1 << 15)	
#define WMC_WANF100		(1 << 14)	
#define WMC_WANFF		(1 << 13)	
#define WMC_WLED1S		(7 <<  4)	
#define		WLED1S_SPEED		(0 << 4)
#define		WLED1S_LINK		(1 << 4)
#define		WLED1S_DUPLEX		(2 << 4)
#define		WLED1S_COLLISION	(3 << 4)
#define		WLED1S_ACTIVITY		(4 << 4)
#define		WLED1S_FDX_COLLISION	(5 << 4)
#define		WLED1S_LINK_ACTIVITY	(6 << 4)
#define WMC_WLED0S		(7 << 0)	
#define		WLED0S_SPEED		(0 << 0)
#define		WLED0S_LINK		(1 << 0)
#define		WLED0S_DUPLEX		(2 << 0)
#define		WLED0S_COLLISION	(3 << 0)
#define		WLED0S_ACTIVITY		(4 << 0)
#define		WLED0S_FDX_COLLISION	(5 << 0)
#define		WLED0S_LINK_ACTIVITY	(6 << 0)

#define WPPM_WLPBK		(1 << 14)	
#define WPPM_WRLPKB		(1 << 13)	
#define WPPM_WPI		(1 << 12)	
#define WPPM_WFL		(1 << 10)	
#define WPPM_MDIXS		(1 << 9)	
#define WPPM_FEF		(1 << 8)	
#define WPPM_AMDIXP		(1 << 7)	
#define WPPM_TXDIS		(1 << 6)	
#define WPPM_DFEF		(1 << 5)	
#define WPPM_PD			(1 << 4)	
#define WPPM_DMDX		(1 << 3)	
#define WPPM_FMDX		(1 << 2)	
#define WPPM_LPBK		(1 << 1)	

#define PPS_PPSM		(1 << 0)	


#endif
