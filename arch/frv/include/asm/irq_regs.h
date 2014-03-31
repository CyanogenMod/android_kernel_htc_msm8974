/* FRV per-CPU frame pointer holder
 *
 * Copyright (C) 2006 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_IRQ_REGS_H
#define _ASM_IRQ_REGS_H

#define ARCH_HAS_OWN_IRQ_REGS

#ifndef __ASSEMBLY__
#define get_irq_regs() (__frame)
#endif

#endif 
