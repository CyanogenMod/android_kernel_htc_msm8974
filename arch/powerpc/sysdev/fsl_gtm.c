/*
 * Freescale General-purpose Timers Module
 *
 * Copyright (c) Freescale Semicondutor, Inc. 2006.
 *               Shlomi Gridish <gridish@freescale.com>
 *               Jerry Huang <Chang-Ming.Huang@freescale.com>
 * Copyright (c) MontaVista Software, Inc. 2008.
 *               Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <asm/fsl_gtm.h>

#define GTCFR_STP(x)		((x) & 1 ? 1 << 5 : 1 << 1)
#define GTCFR_RST(x)		((x) & 1 ? 1 << 4 : 1 << 0)

#define GTMDR_ICLK_MASK		(3 << 1)
#define GTMDR_ICLK_ICAS		(0 << 1)
#define GTMDR_ICLK_ICLK		(1 << 1)
#define GTMDR_ICLK_SLGO		(2 << 1)
#define GTMDR_FRR		(1 << 3)
#define GTMDR_ORI		(1 << 4)
#define GTMDR_SPS(x)		((x) << 8)

struct gtm_timers_regs {
	u8	gtcfr1;		
	u8	res0[0x3];
	u8	gtcfr2;		
	u8	res1[0xB];
	__be16	gtmdr1;		
	__be16	gtmdr2;		
	__be16	gtrfr1;		
	__be16	gtrfr2;		
	__be16	gtcpr1;		
	__be16	gtcpr2;		
	__be16	gtcnr1;		
	__be16	gtcnr2;		
	__be16	gtmdr3;		
	__be16	gtmdr4;		
	__be16	gtrfr3;		
	__be16	gtrfr4;		
	__be16	gtcpr3;		
	__be16	gtcpr4;		
	__be16	gtcnr3;		
	__be16	gtcnr4;		
	__be16	gtevr1;		
	__be16	gtevr2;		
	__be16	gtevr3;		
	__be16	gtevr4;		
	__be16	gtpsr1;		
	__be16	gtpsr2;		
	__be16	gtpsr3;		
	__be16	gtpsr4;		
	u8 res2[0x40];
} __attribute__ ((packed));

struct gtm {
	unsigned int clock;
	struct gtm_timers_regs __iomem *regs;
	struct gtm_timer timers[4];
	spinlock_t lock;
	struct list_head list_node;
};

static LIST_HEAD(gtms);

struct gtm_timer *gtm_get_timer16(void)
{
	struct gtm *gtm = NULL;
	int i;

	list_for_each_entry(gtm, &gtms, list_node) {
		spin_lock_irq(&gtm->lock);

		for (i = 0; i < ARRAY_SIZE(gtm->timers); i++) {
			if (!gtm->timers[i].requested) {
				gtm->timers[i].requested = true;
				spin_unlock_irq(&gtm->lock);
				return &gtm->timers[i];
			}
		}

		spin_unlock_irq(&gtm->lock);
	}

	if (gtm)
		return ERR_PTR(-EBUSY);
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL(gtm_get_timer16);

struct gtm_timer *gtm_get_specific_timer16(struct gtm *gtm,
					   unsigned int timer)
{
	struct gtm_timer *ret = ERR_PTR(-EBUSY);

	if (timer > 3)
		return ERR_PTR(-EINVAL);

	spin_lock_irq(&gtm->lock);

	if (gtm->timers[timer].requested)
		goto out;

	ret = &gtm->timers[timer];
	ret->requested = true;

out:
	spin_unlock_irq(&gtm->lock);
	return ret;
}
EXPORT_SYMBOL(gtm_get_specific_timer16);

void gtm_put_timer16(struct gtm_timer *tmr)
{
	gtm_stop_timer16(tmr);

	spin_lock_irq(&tmr->gtm->lock);
	tmr->requested = false;
	spin_unlock_irq(&tmr->gtm->lock);
}
EXPORT_SYMBOL(gtm_put_timer16);

static int gtm_set_ref_timer16(struct gtm_timer *tmr, int frequency,
			       int reference_value, bool free_run)
{
	struct gtm *gtm = tmr->gtm;
	int num = tmr - &gtm->timers[0];
	unsigned int prescaler;
	u8 iclk = GTMDR_ICLK_ICLK;
	u8 psr;
	u8 sps;
	unsigned long flags;
	int max_prescaler = 256 * 256 * 16;

	
	if (!tmr->gtpsr)
		max_prescaler /= 256;

	prescaler = gtm->clock / frequency;
	if (prescaler > max_prescaler)
		return -EINVAL;

	if (prescaler > max_prescaler / 16) {
		iclk = GTMDR_ICLK_SLGO;
		prescaler /= 16;
	}

	if (prescaler <= 256) {
		psr = 0;
		sps = prescaler - 1;
	} else {
		psr = 256 - 1;
		sps = prescaler / 256 - 1;
	}

	spin_lock_irqsave(&gtm->lock, flags);

	clrsetbits_8(tmr->gtcfr, ~(GTCFR_STP(num) | GTCFR_RST(num)),
				 GTCFR_STP(num) | GTCFR_RST(num));

	setbits8(tmr->gtcfr, GTCFR_STP(num));

	if (tmr->gtpsr)
		out_be16(tmr->gtpsr, psr);
	clrsetbits_be16(tmr->gtmdr, 0xFFFF, iclk | GTMDR_SPS(sps) |
			GTMDR_ORI | (free_run ? GTMDR_FRR : 0));
	out_be16(tmr->gtcnr, 0);
	out_be16(tmr->gtrfr, reference_value);
	out_be16(tmr->gtevr, 0xFFFF);

	
	clrbits8(tmr->gtcfr, GTCFR_STP(num));

	spin_unlock_irqrestore(&gtm->lock, flags);

	return 0;
}

int gtm_set_timer16(struct gtm_timer *tmr, unsigned long usec, bool reload)
{
	
	int freq = 1000000;
	unsigned int bit;

	bit = fls_long(usec);
	if (bit > 15) {
		freq >>= bit - 15;
		usec >>= bit - 15;
	}

	if (!freq)
		return -EINVAL;

	return gtm_set_ref_timer16(tmr, freq, usec, reload);
}
EXPORT_SYMBOL(gtm_set_timer16);

int gtm_set_exact_timer16(struct gtm_timer *tmr, u16 usec, bool reload)
{
	
	const int freq = 1000000;


	return gtm_set_ref_timer16(tmr, freq, usec, reload);
}
EXPORT_SYMBOL(gtm_set_exact_timer16);

void gtm_stop_timer16(struct gtm_timer *tmr)
{
	struct gtm *gtm = tmr->gtm;
	int num = tmr - &gtm->timers[0];
	unsigned long flags;

	spin_lock_irqsave(&gtm->lock, flags);

	setbits8(tmr->gtcfr, GTCFR_STP(num));
	out_be16(tmr->gtevr, 0xFFFF);

	spin_unlock_irqrestore(&gtm->lock, flags);
}
EXPORT_SYMBOL(gtm_stop_timer16);

void gtm_ack_timer16(struct gtm_timer *tmr, u16 events)
{
	out_be16(tmr->gtevr, events);
}
EXPORT_SYMBOL(gtm_ack_timer16);

static void __init gtm_set_shortcuts(struct device_node *np,
				     struct gtm_timer *timers,
				     struct gtm_timers_regs __iomem *regs)
{
	timers[0].gtcfr = &regs->gtcfr1;
	timers[0].gtmdr = &regs->gtmdr1;
	timers[0].gtcnr = &regs->gtcnr1;
	timers[0].gtrfr = &regs->gtrfr1;
	timers[0].gtevr = &regs->gtevr1;

	timers[1].gtcfr = &regs->gtcfr1;
	timers[1].gtmdr = &regs->gtmdr2;
	timers[1].gtcnr = &regs->gtcnr2;
	timers[1].gtrfr = &regs->gtrfr2;
	timers[1].gtevr = &regs->gtevr2;

	timers[2].gtcfr = &regs->gtcfr2;
	timers[2].gtmdr = &regs->gtmdr3;
	timers[2].gtcnr = &regs->gtcnr3;
	timers[2].gtrfr = &regs->gtrfr3;
	timers[2].gtevr = &regs->gtevr3;

	timers[3].gtcfr = &regs->gtcfr2;
	timers[3].gtmdr = &regs->gtmdr4;
	timers[3].gtcnr = &regs->gtcnr4;
	timers[3].gtrfr = &regs->gtrfr4;
	timers[3].gtevr = &regs->gtevr4;

	
	if (!of_device_is_compatible(np, "fsl,cpm2-gtm")) {
		timers[0].gtpsr = &regs->gtpsr1;
		timers[1].gtpsr = &regs->gtpsr2;
		timers[2].gtpsr = &regs->gtpsr3;
		timers[3].gtpsr = &regs->gtpsr4;
	}
}

static int __init fsl_gtm_init(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "fsl,gtm") {
		int i;
		struct gtm *gtm;
		const u32 *clock;
		int size;

		gtm = kzalloc(sizeof(*gtm), GFP_KERNEL);
		if (!gtm) {
			pr_err("%s: unable to allocate memory\n",
				np->full_name);
			continue;
		}

		spin_lock_init(&gtm->lock);

		clock = of_get_property(np, "clock-frequency", &size);
		if (!clock || size != sizeof(*clock)) {
			pr_err("%s: no clock-frequency\n", np->full_name);
			goto err;
		}
		gtm->clock = *clock;

		for (i = 0; i < ARRAY_SIZE(gtm->timers); i++) {
			int ret;
			struct resource irq;

			ret = of_irq_to_resource(np, i, &irq);
			if (ret == NO_IRQ) {
				pr_err("%s: not enough interrupts specified\n",
				       np->full_name);
				goto err;
			}
			gtm->timers[i].irq = irq.start;
			gtm->timers[i].gtm = gtm;
		}

		gtm->regs = of_iomap(np, 0);
		if (!gtm->regs) {
			pr_err("%s: unable to iomap registers\n",
			       np->full_name);
			goto err;
		}

		gtm_set_shortcuts(np, gtm->timers, gtm->regs);
		list_add(&gtm->list_node, &gtms);

		
		np->data = gtm;
		of_node_get(np);

		continue;
err:
		kfree(gtm);
	}
	return 0;
}
arch_initcall(fsl_gtm_init);
