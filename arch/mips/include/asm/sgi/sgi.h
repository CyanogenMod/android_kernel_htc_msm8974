/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * sgi.h: Definitions specific to SGI machines.
 *
 * Copyright (C) 1996 David S. Miller (dm@sgi.com)
 */
#ifndef _ASM_SGI_SGI_H
#define _ASM_SGI_SGI_H

enum sgi_mach {
	ip4,	
	ip5,	
	ip6,	
	ip7,	
	ip9,	
	ip12,	
	ip15,	
	ip17,	
	ip19,	
	ip20,	
	ip21,	
	ip22,	
	ip25,	
	ip26,	
	ip27,	
	ip28,	
	ip30,	
	ip32,	
};

extern enum sgi_mach sgimach;
extern void sgi_sysinit(void);

#ifdef __MIPSEL__
#define SGI_MSB(regaddr)   (regaddr)
#else
#define SGI_MSB(regaddr)   ((regaddr) | 0x3)
#endif

#endif 
