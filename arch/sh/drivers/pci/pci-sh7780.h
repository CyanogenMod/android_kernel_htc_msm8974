/*
 *	Low-Level PCI Support for SH7780 targets
 *
 *  Dustin McIntire (dustin@sensoria.com) (c) 2001
 *  Paul Mundt (lethal@linux-sh.org) (c) 2003
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 */

#ifndef _PCI_SH7780_H_
#define _PCI_SH7780_H_

#define	PCIECR			0xFE000008
#define PCIECR_ENBL		0x01

#define SH7780_PCI_CONFIG_BASE	0xFD000000	
#define SH7780_PCI_CONFIG_SIZE	0x01000000	

#define SH7780_PCIREG_BASE	0xFE040000	

#define SH7780_PCIIR		0x114		
#define SH7780_PCIIMR		0x118		
#define SH7780_PCIAIR		0x11C		
#define SH7780_PCICIR		0x120		
#define SH7780_PCIAINT		0x130		
#define SH7780_PCIAINTM		0x134		
#define SH7780_PCIBMIR		0x138		
#define SH7780_PCIPAR		0x1C0		
#define SH7780_PCIPINT		0x1CC		
#define SH7780_PCIPINTM		0x1D0		

#define SH7780_PCIMBR(x)	(0x1E0 + ((x) * 8))
#define SH7780_PCIMBMR(x)	(0x1E4 + ((x) * 8))
#define SH7780_PCIIOBR		0x1F8
#define SH7780_PCIIOBMR		0x1FC
#define SH7780_PCICSCR0		0x210		
#define SH7780_PCICSCR1		0x214		
#define SH7780_PCICSAR0		0x218	
#define SH7780_PCICSAR1		0x21C	

#endif 
