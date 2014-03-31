/*
 * OpenRISC irq.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/ftrace.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/export.h>

#include <linux/irqflags.h>

unsigned long arch_local_save_flags(void)
{
	return mfspr(SPR_SR) & (SPR_SR_IEE|SPR_SR_TEE);
}
EXPORT_SYMBOL(arch_local_save_flags);

void arch_local_irq_restore(unsigned long flags)
{
	mtspr(SPR_SR, ((mfspr(SPR_SR) & ~(SPR_SR_IEE|SPR_SR_TEE)) | flags));
}
EXPORT_SYMBOL(arch_local_irq_restore);




static void or1k_pic_mask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->irq));
}

static void or1k_pic_unmask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) | (1UL << data->irq));
}

static void or1k_pic_ack(struct irq_data *data)
{

	/* The OpenRISC 1000 spec says to write a 1 to the bit to ack the
	 * interrupt, but the OR1200 does this backwards and requires a 0
	 * to be written...
	 */

#ifdef CONFIG_OR1K_1200

	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->irq));
#else
	WARN(1, "Interrupt handling possibily broken\n");
	mtspr(SPR_PICSR, (1UL << irq));
#endif
}

static void or1k_pic_mask_ack(struct irq_data *data)
{
	

#ifdef CONFIG_OR1K_1200
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->irq));
#else
	WARN(1, "Interrupt handling possibily broken\n");
	mtspr(SPR_PICSR, (1UL << irq));
#endif
}

static int or1k_pic_set_type(struct irq_data *data, unsigned int flow_type)
{

	return irq_setup_alt_chip(data, flow_type);
}

static inline int pic_get_irq(int first)
{
	int irq;

	irq = ffs(mfspr(SPR_PICSR) >> first);

	return irq ? irq + first - 1 : NO_IRQ;
}

static void __init or1k_irq_init(void)
{
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	
	mtspr(SPR_PICMR, (0UL));

	gc = irq_alloc_generic_chip("or1k-PIC", 1, 0, 0, handle_level_irq);
	ct = gc->chip_types;

	ct->chip.irq_unmask = or1k_pic_unmask;
	ct->chip.irq_mask = or1k_pic_mask;
	ct->chip.irq_ack = or1k_pic_ack;
	ct->chip.irq_mask_ack = or1k_pic_mask_ack;
	ct->chip.irq_set_type = or1k_pic_set_type;

#if 0
	
	ct->chip.type = IRQ_TYPE_EDGE_BOTH | IRQ_TYPE_LEVEL_MASK;
#endif

	irq_setup_generic_chip(gc, IRQ_MSK(NR_IRQS), 0,
			       IRQ_NOREQUEST, IRQ_LEVEL | IRQ_NOPROBE);
}

void __init init_IRQ(void)
{
	or1k_irq_init();
}

void __irq_entry do_IRQ(struct pt_regs *regs)
{
	int irq = -1;
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();

	while ((irq = pic_get_irq(irq + 1)) != NO_IRQ)
		generic_handle_irq(irq);

	irq_exit();
	set_irq_regs(old_regs);
}

unsigned int irq_create_of_mapping(struct device_node *controller,
				   const u32 *intspec, unsigned int intsize)
{
	return intspec[0];
}
EXPORT_SYMBOL_GPL(irq_create_of_mapping);
