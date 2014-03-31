/* MN10300 Hardware IRQ statistics and management
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Modified by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_HARDIRQ_H
#define _ASM_HARDIRQ_H

#include <linux/threads.h>
#include <linux/irq.h>
#include <asm/exceptions.h>

typedef struct {
	unsigned int	__softirq_pending;
#ifdef CONFIG_MN10300_WD_TIMER
	unsigned int	__nmi_count;	
	unsigned int	__irq_count;	
#endif
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	

extern void ack_bad_irq(int irq);

typedef void (*intr_stub_fnx)(struct pt_regs *regs,
			      enum exception_code intcode);

extern asmlinkage void set_excp_vector(enum exception_code code,
				       intr_stub_fnx handler);

#endif 
