/*
 *	include/asm-mips/i8259.h
 *
 *	i8259A interrupt definitions.
 *
 *	Copyright (C) 2003  Maciej W. Rozycki
 *	Copyright (C) 2003  Ralf Baechle <ralf@linux-mips.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_I8259_H
#define _ASM_I8259_H

#include <linux/compiler.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <irq.h>

#define PIC_MASTER_CMD		0x20
#define PIC_MASTER_IMR		0x21
#define PIC_MASTER_ISR		PIC_MASTER_CMD
#define PIC_MASTER_POLL		PIC_MASTER_ISR
#define PIC_MASTER_OCW3		PIC_MASTER_ISR
#define PIC_SLAVE_CMD		0xa0
#define PIC_SLAVE_IMR		0xa1

#define PIC_CASCADE_IR		2
#define MASTER_ICW4_DEFAULT	0x01
#define SLAVE_ICW4_DEFAULT	0x01
#define PIC_ICW4_AEOI		2

extern raw_spinlock_t i8259A_lock;

extern int i8259A_irq_pending(unsigned int irq);
extern void make_8259A_irq(unsigned int irq);

extern void init_i8259_irqs(void);

static inline int i8259_irq(void)
{
	int irq;

	raw_spin_lock(&i8259A_lock);

	
	outb(0x0C, PIC_MASTER_CMD);		
	irq = inb(PIC_MASTER_CMD) & 7;
	if (irq == PIC_CASCADE_IR) {
		outb(0x0C, PIC_SLAVE_CMD);		
		irq = (inb(PIC_SLAVE_CMD) & 7) + 8;
	}

	if (unlikely(irq == 7)) {
		outb(0x0B, PIC_MASTER_ISR);		
		if(~inb(PIC_MASTER_ISR) & 0x80)
			irq = -1;
	}

	raw_spin_unlock(&i8259A_lock);

	return likely(irq >= 0) ? irq + I8259A_IRQ_BASE : irq;
}

#endif 
