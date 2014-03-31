/*
 *      linux/drivers/video/maxinefb.h
 *
 *      DECstation 5000/xx onboard framebuffer support, Copyright (C) 1999 by
 *      Michael Engel <engel@unix-ag.org> and Karsten Merker <merker@guug.de>
 *      This file is subject to the terms and conditions of the GNU General
 *      Public License.  See the file COPYING in the main directory of this
 *      archive for more details.
 */

#include <asm/addrspace.h>

#define MAXINEFB_IMS332_ADDRESS		KSEG1ADDR(0x1c140000)

#define DS5000_xx_ONBOARD_FBMEM_START	KSEG1ADDR(0x0a000000)


#define IMS332_REG_CURSOR_RAM           0x200	

#define IMS332_REG_COLOR_PALETTE        0x100	
#define IMS332_REG_CURSOR_COLOR_PALETTE	0x0a1	
						
