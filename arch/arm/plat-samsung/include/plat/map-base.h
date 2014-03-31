/* linux/include/asm-arm/plat-s3c/map.h
 *
 * Copyright 2003, 2007 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C - Memory map definitions (virtual addresses)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_PLAT_MAP_H
#define __ASM_PLAT_MAP_H __FILE__


#define S3C_ADDR_BASE	0xF6000000

#ifndef __ASSEMBLY__
#define S3C_ADDR(x)	((void __iomem __force *)S3C_ADDR_BASE + (x))
#else
#define S3C_ADDR(x)	(S3C_ADDR_BASE + (x))
#endif

#define S3C_VA_IRQ	S3C_ADDR(0x00000000)	
#define S3C_VA_SYS	S3C_ADDR(0x00100000)	
#define S3C_VA_MEM	S3C_ADDR(0x00200000)	
#define S3C_VA_TIMER	S3C_ADDR(0x00300000)	
#define S3C_VA_WATCHDOG	S3C_ADDR(0x00400000)	
#define S3C_VA_UART	S3C_ADDR(0x01000000)	

#define S3C_ADDR_CPU(x)	S3C_ADDR(0x00500000 + (x))

#endif 
