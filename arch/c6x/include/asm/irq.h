/*
 *  Port on Texas Instruments TMS320C6x architecture
 *
 *  Copyright (C) 2004, 2006, 2009, 2010, 2011 Texas Instruments Incorporated
 *  Author: Aurelien Jacquiot (aurelien.jacquiot@jaluna.com)
 *
 *  Large parts taken directly from powerpc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef _ASM_C6X_IRQ_H
#define _ASM_C6X_IRQ_H

#include <linux/irqdomain.h>
#include <linux/threads.h>
#include <linux/list.h>
#include <linux/radix-tree.h>
#include <asm/percpu.h>

#define irq_canonicalize(irq)  (irq)

#define NR_PRIORITY_IRQS 16

#define NR_IRQS_LEGACY	NR_PRIORITY_IRQS

#define NR_IRQS		256

#define NO_IRQ		0

extern void __init init_pic_c64xplus(void);

extern void init_IRQ(void);

struct pt_regs;

extern asmlinkage void c6x_do_IRQ(unsigned int prio, struct pt_regs *regs);

extern unsigned long irq_err_count;

#endif 
