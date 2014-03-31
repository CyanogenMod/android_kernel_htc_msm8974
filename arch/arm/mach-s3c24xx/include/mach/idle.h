/* arch/arm/mach-s3c2410/include/mach/idle.h
 *
 * Copyright (c) 2004 Simtec Electronics <linux@simtec.co.uk>
 *		http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410 CPU Idle controls
*/

#ifndef __ASM_ARCH_IDLE_H
#define __ASM_ARCH_IDLE_H __FILE__


extern void (*s3c24xx_idle)(void);

extern void s3c24xx_default_idle(void);

#endif 
