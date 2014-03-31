/*
 * MTD primitives for XIP support. Architecture specific functions
 *
 * Do not include this file directly. It's included from linux/mtd/xip.h
 * 
 * Author:	Nicolas Pitre
 * Created:	Nov 2, 2004
 * Copyright:	(C) 2004 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_PXA_MTD_XIP_H__
#define __ARCH_PXA_MTD_XIP_H__

#include <mach/regs-ost.h>

#define xip_irqpending()	(ICIP & ICMR)

#define xip_currtime()		(OSCR)
#define xip_elapsed_since(x)	(signed)((OSCR - (x)) / 4)


#define xip_cpu_idle()  asm volatile ("mcr p14, 0, %0, c7, c0, 0" :: "r" (1))

#endif 
