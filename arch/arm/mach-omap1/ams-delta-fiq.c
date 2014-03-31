/*
 *  Amstrad E3 FIQ handling
 *
 *  Copyright (C) 2009 Janusz Krzysztofik
 *  Copyright (c) 2006 Matt Callow
 *  Copyright (c) 2004 Amstrad Plc
 *  Copyright (C) 2001 RidgeRun, Inc.
 *
 * Parts of this code are taken from linux/arch/arm/mach-omap/irq.c
 * in the MontaVista 2.4 kernel (and the Amstrad changes therein)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/io.h>

#include <plat/board-ams-delta.h>

#include <asm/fiq.h>

#include <mach/ams-delta-fiq.h>

static struct fiq_handler fh = {
	.name	= "ams-delta-fiq"
};

unsigned int fiq_buffer[1024];
EXPORT_SYMBOL(fiq_buffer);

static unsigned int irq_counter[16];

static irqreturn_t deferred_fiq(int irq, void *dev_id)
{
	struct irq_desc *irq_desc;
	struct irq_chip *irq_chip = NULL;
	int gpio, irq_num, fiq_count;

	irq_desc = irq_to_desc(gpio_to_irq(AMS_DELTA_GPIO_PIN_KEYBRD_CLK));
	if (irq_desc)
		irq_chip = irq_desc->irq_data.chip;

	for (gpio = AMS_DELTA_GPIO_PIN_KEYBRD_CLK;
			gpio <= AMS_DELTA_GPIO_PIN_HOOK_SWITCH; gpio++) {
		irq_num = gpio_to_irq(gpio);
		fiq_count = fiq_buffer[FIQ_CNT_INT_00 + gpio];

		while (irq_counter[gpio] < fiq_count) {
			if (gpio != AMS_DELTA_GPIO_PIN_KEYBRD_CLK) {
				struct irq_data *d = irq_get_irq_data(irq_num);

				if (irq_chip && irq_chip->irq_unmask)
					irq_chip->irq_unmask(d);
			}
			generic_handle_irq(irq_num);

			irq_counter[gpio]++;
		}
	}
	return IRQ_HANDLED;
}

void __init ams_delta_init_fiq(void)
{
	void *fiqhandler_start;
	unsigned int fiqhandler_length;
	struct pt_regs FIQ_regs;
	unsigned long val, offset;
	int i, retval;

	fiqhandler_start = &qwerty_fiqin_start;
	fiqhandler_length = &qwerty_fiqin_end - &qwerty_fiqin_start;
	pr_info("Installing fiq handler from %p, length 0x%x\n",
			fiqhandler_start, fiqhandler_length);

	retval = claim_fiq(&fh);
	if (retval) {
		pr_err("ams_delta_init_fiq(): couldn't claim FIQ, ret=%d\n",
				retval);
		return;
	}

	retval = request_irq(INT_DEFERRED_FIQ, deferred_fiq,
			IRQ_TYPE_EDGE_RISING, "deferred_fiq", 0);
	if (retval < 0) {
		pr_err("Failed to get deferred_fiq IRQ, ret=%d\n", retval);
		release_fiq(&fh);
		return;
	}
	offset = IRQ_ILR0_REG_OFFSET + INT_DEFERRED_FIQ * 0x4;
	val = omap_readl(DEFERRED_FIQ_IH_BASE + offset) & ~(1 << 1);
	omap_writel(val, DEFERRED_FIQ_IH_BASE + offset);

	set_fiq_handler(fiqhandler_start, fiqhandler_length);

	fiq_buffer[FIQ_GPIO_INT_MASK]	= 0;
	fiq_buffer[FIQ_MASK]		= 0;
	fiq_buffer[FIQ_STATE]		= 0;
	fiq_buffer[FIQ_KEY]		= 0;
	fiq_buffer[FIQ_KEYS_CNT]	= 0;
	fiq_buffer[FIQ_KEYS_HICNT]	= 0;
	fiq_buffer[FIQ_TAIL_OFFSET]	= 0;
	fiq_buffer[FIQ_HEAD_OFFSET]	= 0;
	fiq_buffer[FIQ_BUF_LEN]		= 256;
	fiq_buffer[FIQ_MISSED_KEYS]	= 0;
	fiq_buffer[FIQ_BUFFER_START]	=
			(unsigned int) &fiq_buffer[FIQ_CIRC_BUFF];

	for (i = FIQ_CNT_INT_00; i <= FIQ_CNT_INT_15; i++)
		fiq_buffer[i] = 0;

	FIQ_regs.ARM_r9 = (unsigned int)fiq_buffer;
	set_fiq_regs(&FIQ_regs);

	pr_info("request_fiq(): fiq_buffer = %p\n", fiq_buffer);

	offset = IRQ_ILR0_REG_OFFSET + INT_GPIO_BANK1 * 0x4;
	val = omap_readl(OMAP_IH1_BASE + offset) | 1;
	omap_writel(val, OMAP_IH1_BASE + offset);
}
