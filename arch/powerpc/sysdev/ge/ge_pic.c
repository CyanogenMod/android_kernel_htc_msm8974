/*
 * Interrupt handling for GE FPGA based PIC
 *
 * Author: Martyn Welch <martyn.welch@ge.com>
 *
 * 2008 (c) GE Intelligent Platforms Embedded Systems, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/irq.h>

#include "ge_pic.h"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define DBG(fmt...) do { printk(KERN_DEBUG "gef_pic: " fmt); } while (0)
#else
#define DBG(fmt...) do { } while (0)
#endif

#define GEF_PIC_NUM_IRQS	32

#define GEF_PIC_INTR_STATUS	0x0000

#define GEF_PIC_INTR_MASK(cpu)	(0x0010 + (0x4 * cpu))
#define GEF_PIC_CPU0_INTR_MASK	GEF_PIC_INTR_MASK(0)
#define GEF_PIC_CPU1_INTR_MASK	GEF_PIC_INTR_MASK(1)

#define GEF_PIC_MCP_MASK(cpu)	(0x0018 + (0x4 * cpu))
#define GEF_PIC_CPU0_MCP_MASK	GEF_PIC_MCP_MASK(0)
#define GEF_PIC_CPU1_MCP_MASK	GEF_PIC_MCP_MASK(1)


static DEFINE_RAW_SPINLOCK(gef_pic_lock);

static void __iomem *gef_pic_irq_reg_base;
static struct irq_domain *gef_pic_irq_host;
static int gef_pic_cascade_irq;


void gef_pic_cascade(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int cascade_irq;

	cascade_irq = gef_pic_get_irq();

	if (cascade_irq != NO_IRQ)
		generic_handle_irq(cascade_irq);

	chip->irq_eoi(&desc->irq_data);
}

static void gef_pic_mask(struct irq_data *d)
{
	unsigned long flags;
	unsigned int hwirq = irqd_to_hwirq(d);
	u32 mask;

	raw_spin_lock_irqsave(&gef_pic_lock, flags);
	mask = in_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_MASK(0));
	mask &= ~(1 << hwirq);
	out_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_MASK(0), mask);
	raw_spin_unlock_irqrestore(&gef_pic_lock, flags);
}

static void gef_pic_mask_ack(struct irq_data *d)
{
	gef_pic_mask(d);
}

static void gef_pic_unmask(struct irq_data *d)
{
	unsigned long flags;
	unsigned int hwirq = irqd_to_hwirq(d);
	u32 mask;

	raw_spin_lock_irqsave(&gef_pic_lock, flags);
	mask = in_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_MASK(0));
	mask |= (1 << hwirq);
	out_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_MASK(0), mask);
	raw_spin_unlock_irqrestore(&gef_pic_lock, flags);
}

static struct irq_chip gef_pic_chip = {
	.name		= "gefp",
	.irq_mask	= gef_pic_mask,
	.irq_mask_ack	= gef_pic_mask_ack,
	.irq_unmask	= gef_pic_unmask,
};


static int gef_pic_host_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hwirq)
{
	
	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &gef_pic_chip, handle_level_irq);

	return 0;
}

static int gef_pic_host_xlate(struct irq_domain *h, struct device_node *ct,
			    const u32 *intspec, unsigned int intsize,
			    irq_hw_number_t *out_hwirq, unsigned int *out_flags)
{

	*out_hwirq = intspec[0];
	if (intsize > 1)
		*out_flags = intspec[1];
	else
		*out_flags = IRQ_TYPE_LEVEL_HIGH;

	return 0;
}

static const struct irq_domain_ops gef_pic_host_ops = {
	.map	= gef_pic_host_map,
	.xlate	= gef_pic_host_xlate,
};


void __init gef_pic_init(struct device_node *np)
{
	unsigned long flags;

	
	gef_pic_irq_reg_base = of_iomap(np, 0);

	raw_spin_lock_irqsave(&gef_pic_lock, flags);

	
	out_be32(gef_pic_irq_reg_base + GEF_PIC_CPU0_INTR_MASK, 0);
	out_be32(gef_pic_irq_reg_base + GEF_PIC_CPU1_INTR_MASK, 0);

	out_be32(gef_pic_irq_reg_base + GEF_PIC_CPU0_MCP_MASK, 0);
	out_be32(gef_pic_irq_reg_base + GEF_PIC_CPU1_MCP_MASK, 0);

	raw_spin_unlock_irqrestore(&gef_pic_lock, flags);

	
	gef_pic_cascade_irq = irq_of_parse_and_map(np, 0);
	if (gef_pic_cascade_irq == NO_IRQ) {
		printk(KERN_ERR "SBC610: failed to map cascade interrupt");
		return;
	}

	
	gef_pic_irq_host = irq_domain_add_linear(np, GEF_PIC_NUM_IRQS,
					  &gef_pic_host_ops, NULL);
	if (gef_pic_irq_host == NULL)
		return;

	
	irq_set_chained_handler(gef_pic_cascade_irq, gef_pic_cascade);
}

unsigned int gef_pic_get_irq(void)
{
	u32 cause, mask, active;
	unsigned int virq = NO_IRQ;
	int hwirq;

	cause = in_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_STATUS);

	mask = in_be32(gef_pic_irq_reg_base + GEF_PIC_INTR_MASK(0));

	active = cause & mask;

	if (active) {
		for (hwirq = GEF_PIC_NUM_IRQS - 1; hwirq > -1; hwirq--) {
			if (active & (0x1 << hwirq))
				break;
		}
		virq = irq_linear_revmap(gef_pic_irq_host,
			(irq_hw_number_t)hwirq);
	}

	return virq;
}

