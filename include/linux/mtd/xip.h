/*
 * MTD primitives for XIP support
 *
 * Author:	Nicolas Pitre
 * Created:	Nov 2, 2004
 * Copyright:	(C) 2004 MontaVista Software, Inc.
 *
 * This XIP support for MTD has been loosely inspired
 * by an earlier patch authored by David Woodhouse.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MTD_XIP_H__
#define __LINUX_MTD_XIP_H__


#ifdef CONFIG_MTD_XIP

#include <linux/compiler.h>

#define __xipram noinline __attribute__ ((__section__ (".data")))


#include <asm/mtd-xip.h>

#ifndef xip_irqpending

#warning "missing IRQ and timer primitives for XIP MTD support"
#warning "some of the XIP MTD support code will be disabled"
#warning "your system will therefore be unresponsive when writing or erasing flash"

#define xip_irqpending()	(0)
#define xip_currtime()		(0)
#define xip_elapsed_since(x)	(0)

#endif

#ifndef xip_iprefetch
#define xip_iprefetch()		do { } while (0)
#endif

#ifndef xip_cpu_idle
#define xip_cpu_idle()  do { } while (0)
#endif

#else

#define __xipram

#endif 

#endif 
