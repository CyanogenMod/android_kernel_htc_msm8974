/*
 * Copyright (C) 2007 Lemote Inc.
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/irq_cpu.h>
#include <asm/i8259.h>
#include <asm/mipsregs.h>

#include <loongson.h>
#include <machine.h>

#define LOONGSON_TIMER_IRQ	(MIPS_CPU_IRQ_BASE + 7)	
#define LOONGSON_NORTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 6)	
#define LOONGSON_UART_IRQ	(MIPS_CPU_IRQ_BASE + 3)	
#define LOONGSON_SOUTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 2)	

#define LOONGSON_INT_BIT_INT0		(1 << 11)
#define LOONGSON_INT_BIT_INT1		(1 << 12)

int mach_i8259_irq(void)
{
	int irq, isr;

	irq = -1;

	if ((LOONGSON_INTISR & LOONGSON_INTEN) & LOONGSON_INT_BIT_INT0) {
		raw_spin_lock(&i8259A_lock);
		isr = inb(PIC_MASTER_CMD) &
			~inb(PIC_MASTER_IMR) & ~(1 << PIC_CASCADE_IR);
		if (!isr)
			isr = (inb(PIC_SLAVE_CMD) & ~inb(PIC_SLAVE_IMR)) << 8;
		irq = ffs(isr) - 1;
		if (unlikely(irq == 7)) {
			outb(0x0B, PIC_MASTER_ISR);	
			if (~inb(PIC_MASTER_ISR) & 0x80)
				irq = -1;
		}
		raw_spin_unlock(&i8259A_lock);
	}

	return irq;
}
EXPORT_SYMBOL(mach_i8259_irq);

static void i8259_irqdispatch(void)
{
	int irq;

	irq = mach_i8259_irq();
	if (irq >= 0)
		do_IRQ(irq);
	else
		spurious_interrupt();
}

void mach_irq_dispatch(unsigned int pending)
{
	if (pending & CAUSEF_IP7)
		do_IRQ(LOONGSON_TIMER_IRQ);
	else if (pending & CAUSEF_IP6) {	
		do_perfcnt_IRQ();
		bonito_irqdispatch();
	} else if (pending & CAUSEF_IP3)	
		do_IRQ(LOONGSON_UART_IRQ);
	else if (pending & CAUSEF_IP2)	
		i8259_irqdispatch();
	else
		spurious_interrupt();
}

static irqreturn_t ip6_action(int cpl, void *dev_id)
{
	return IRQ_HANDLED;
}

struct irqaction ip6_irqaction = {
	.handler = ip6_action,
	.name = "cascade",
	.flags = IRQF_SHARED | IRQF_NO_THREAD,
};

struct irqaction cascade_irqaction = {
	.handler = no_action,
	.name = "cascade",
	.flags = IRQF_NO_THREAD,
};

void __init mach_init_irq(void)
{

	
	LOONGSON_INTPOL = LOONGSON_INT_BIT_INT0 | LOONGSON_INT_BIT_INT1;
	LOONGSON_INTEDGE &= ~(LOONGSON_INT_BIT_INT0 | LOONGSON_INT_BIT_INT1);

	
	mips_cpu_irq_init();
	init_i8259_irqs();
	bonito_irq_init();

	
	setup_irq(LOONGSON_NORTH_BRIDGE_IRQ, &ip6_irqaction);
	
	setup_irq(LOONGSON_SOUTH_BRIDGE_IRQ, &cascade_irqaction);
}
