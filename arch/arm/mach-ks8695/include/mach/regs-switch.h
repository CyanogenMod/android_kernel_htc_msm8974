/*
 * arch/arm/mach-ks8695/include/mach/regs-switch.h
 *
 * Copyright (C) 2006 Andrew Victor
 *
 * KS8695 - Switch Registers and bit definitions.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_SWITCH_H
#define KS8695_SWITCH_H

#define KS8695_SWITCH_OFFSET	(0xF0000 + 0xe800)
#define KS8695_SWITCH_VA	(KS8695_IO_VA + KS8695_SWITCH_OFFSET)
#define KS8695_SWITCH_PA	(KS8695_IO_PA + KS8695_SWITCH_OFFSET)


#define KS8695_SEC0		(0x00)		
#define KS8695_SEC1		(0x04)		
#define KS8695_SEC2		(0x08)		

#define KS8695_SEPXCZ(x,z)	(0x0c + (((x)-1)*3 + ((z)-1))*4)	

#define KS8695_SEP12AN		(0x48)		
#define KS8695_SEP34AN		(0x4c)		
#define KS8695_SEIAC		(0x50)		
#define KS8695_SEIADH2		(0x54)		
#define KS8695_SEIADH1		(0x58)		
#define KS8695_SEIADL		(0x5c)		
#define KS8695_SEAFC		(0x60)		
#define KS8695_SEDSCPH		(0x64)		
#define KS8695_SEDSCPL		(0x68)		
#define KS8695_SEMAH		(0x6c)		
#define KS8695_SEMAL		(0x70)		
#define KS8695_LPPM12		(0x74)		
#define KS8695_LPPM34		(0x78)		


#define SEC0_LLED1S		(7 << 25)	
#define		LLED1S_SPEED		(0 << 25)
#define		LLED1S_LINK		(1 << 25)
#define		LLED1S_DUPLEX		(2 << 25)
#define		LLED1S_COLLISION	(3 << 25)
#define		LLED1S_ACTIVITY		(4 << 25)
#define		LLED1S_FDX_COLLISION	(5 << 25)
#define		LLED1S_LINK_ACTIVITY	(6 << 25)
#define SEC0_LLED0S		(7 << 22)	
#define		LLED0S_SPEED		(0 << 22)
#define		LLED0S_LINK		(1 << 22)
#define		LLED0S_DUPLEX		(2 << 22)
#define		LLED0S_COLLISION	(3 << 22)
#define		LLED0S_ACTIVITY		(4 << 22)
#define		LLED0S_FDX_COLLISION	(5 << 22)
#define		LLED0S_LINK_ACTIVITY	(6 << 22)
#define SEC0_ENABLE		(1 << 0)	



#endif
