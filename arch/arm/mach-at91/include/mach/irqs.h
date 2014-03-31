/*
 * arch/arm/mach-at91/include/mach/irqs.h
 *
 *  Copyright (C) 2004 SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#include <linux/io.h>
#include <mach/at91_aic.h>

#define NR_AIC_IRQS 32


#define irq_finish(irq) do { at91_aic_write(AT91_AIC_EOICR, 0); } while (0)


#define	NR_IRQS		(NR_AIC_IRQS + (5 * 32))

#define FIQ_START AT91_ID_FIQ

#endif
