/*
 * IBM Onboard Peripheral Bus Interrupt Controller
 *
 * Copyright 2010 Jack Miller, IBM Corporation.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/time.h>

#include <asm/reg_a2.h>
#include <asm/irq.h>

#define OPB_NR_IRQS 32

#define OPB_MLSASIER	0x04    
#define OPB_MLSIR	0x50	
#define OPB_MLSIER	0x54	
#define OPB_MLSIPR	0x58	
#define OPB_MLSIIR	0x5c	

static int opb_index = 0;

struct opb_pic {
	struct irq_domain *host;
	void *regs;
	int index;
	spinlock_t lock;
};

static u32 opb_in(struct opb_pic *opb, int offset)
{
	return in_be32(opb->regs + offset);
}

static void opb_out(struct opb_pic *opb, int offset, u32 val)
{
	out_be32(opb->regs + offset, val);
}

static void opb_unmask_irq(struct irq_data *d)
{
	struct opb_pic *opb;
	unsigned long flags;
	u32 ier, bitset;

	opb = d->chip_data;
	bitset = (1 << (31 - irqd_to_hwirq(d)));

	spin_lock_irqsave(&opb->lock, flags);

	ier = opb_in(opb, OPB_MLSIER);
	opb_out(opb, OPB_MLSIER, ier | bitset);
	ier = opb_in(opb, OPB_MLSIER);

	spin_unlock_irqrestore(&opb->lock, flags);
}

static void opb_mask_irq(struct irq_data *d)
{
	struct opb_pic *opb;
	unsigned long flags;
	u32 ier, mask;

	opb = d->chip_data;
	mask = ~(1 << (31 - irqd_to_hwirq(d)));

	spin_lock_irqsave(&opb->lock, flags);

	ier = opb_in(opb, OPB_MLSIER);
	opb_out(opb, OPB_MLSIER, ier & mask);
	ier = opb_in(opb, OPB_MLSIER); 

	spin_unlock_irqrestore(&opb->lock, flags);
}

static void opb_ack_irq(struct irq_data *d)
{
	struct opb_pic *opb;
	unsigned long flags;
	u32 bitset;

	opb = d->chip_data;
	bitset = (1 << (31 - irqd_to_hwirq(d)));

	spin_lock_irqsave(&opb->lock, flags);

	opb_out(opb, OPB_MLSIR, bitset);
	opb_in(opb, OPB_MLSIR); 

	spin_unlock_irqrestore(&opb->lock, flags);
}

static void opb_mask_ack_irq(struct irq_data *d)
{
	struct opb_pic *opb;
	unsigned long flags;
	u32 bitset;
	u32 ier, ir;

	opb = d->chip_data;
	bitset = (1 << (31 - irqd_to_hwirq(d)));

	spin_lock_irqsave(&opb->lock, flags);

	ier = opb_in(opb, OPB_MLSIER);
	opb_out(opb, OPB_MLSIER, ier & ~bitset);
	ier = opb_in(opb, OPB_MLSIER); 

	opb_out(opb, OPB_MLSIR, bitset);
	ir = opb_in(opb, OPB_MLSIR); 

	spin_unlock_irqrestore(&opb->lock, flags);
}

static int opb_set_irq_type(struct irq_data *d, unsigned int flow)
{
	struct opb_pic *opb;
	unsigned long flags;
	int invert, ipr, mask, bit;

	opb = d->chip_data;


	switch (flow) {
		case IRQ_TYPE_NONE:
			opb_mask_irq(d);
			return 0;

		case IRQ_TYPE_LEVEL_HIGH:
			invert = 0;
			break;

		case IRQ_TYPE_LEVEL_LOW:
			invert = 1;
			break;

		default:
			return -EINVAL;
	}

	bit = (1 << (31 - irqd_to_hwirq(d)));
	mask = ~bit;

	spin_lock_irqsave(&opb->lock, flags);

	ipr = opb_in(opb, OPB_MLSIPR);
	ipr = (ipr & mask) | (invert ? bit : 0);
	opb_out(opb, OPB_MLSIPR, ipr);
	ipr = opb_in(opb, OPB_MLSIPR);  

	spin_unlock_irqrestore(&opb->lock, flags);

	
	irqd_set_trigger_type(d, flow);

	return 0;
}

static struct irq_chip opb_irq_chip = {
	.name		= "OPB",
	.irq_mask	= opb_mask_irq,
	.irq_unmask	= opb_unmask_irq,
	.irq_mask_ack	= opb_mask_ack_irq,
	.irq_ack	= opb_ack_irq,
	.irq_set_type	= opb_set_irq_type
};

static int opb_host_map(struct irq_domain *host, unsigned int virq,
		irq_hw_number_t hwirq)
{
	struct opb_pic *opb;

	opb = host->host_data;


	irq_set_chip_data(virq, opb);
	irq_set_chip_and_handler(virq, &opb_irq_chip, handle_level_irq);
	irq_set_irq_type(virq, IRQ_TYPE_NONE);

	return 0;
}

static const struct irq_domain_ops opb_host_ops = {
	.map = opb_host_map,
	.xlate = irq_domain_xlate_twocell,
};

irqreturn_t opb_irq_handler(int irq, void *private)
{
	struct opb_pic *opb;
	u32 ir, src, subvirq;

	opb = (struct opb_pic *) private;

	ir = opb_in(opb, OPB_MLSIR);
	if (!ir)
		return IRQ_NONE;

	do {
		
		src = 32 - ffs(ir);


		subvirq = irq_linear_revmap(opb->host, src);

		generic_handle_irq(subvirq);
	} while ((ir = opb_in(opb, OPB_MLSIR)));

	return IRQ_HANDLED;
}

struct opb_pic *opb_pic_init_one(struct device_node *dn)
{
	struct opb_pic *opb;
	struct resource res;

	if (of_address_to_resource(dn, 0, &res)) {
		printk(KERN_ERR "opb: Couldn't translate resource\n");
		return  NULL;
	}

	opb = kzalloc(sizeof(struct opb_pic), GFP_KERNEL);
	if (!opb) {
		printk(KERN_ERR "opb: Failed to allocate opb struct!\n");
		return NULL;
	}

	
	opb->regs = ioremap(res.start + 0x10000, 0x1000);
	if (!opb->regs) {
		printk(KERN_ERR "opb: Failed to allocate register space!\n");
		goto free_opb;
	}


	opb->host = irq_domain_add_linear(dn, OPB_NR_IRQS, &opb_host_ops, opb);
	if (!opb->host) {
		printk(KERN_ERR "opb: Failed to allocate IRQ host!\n");
		goto free_regs;
	}

	opb->index = opb_index++;
	spin_lock_init(&opb->lock);

	
	opb_out(opb, OPB_MLSASIER, 0);
	opb_out(opb, OPB_MLSIER, 0);

	
	opb_out(opb, OPB_MLSIR, 0xFFFFFFFF);

	return opb;

free_regs:
	iounmap(opb->regs);
free_opb:
	kfree(opb);
	return NULL;
}

void __init opb_pic_init(void)
{
	struct device_node *dn;
	struct opb_pic *opb;
	int virq;
	int rc;

	
	for_each_compatible_node(dn, NULL, "ibm,opb") {

		
		opb = opb_pic_init_one(dn);
		if (!opb) {
			printk(KERN_WARNING "opb: Failed to init node, skipped!\n");
			continue;
		}

		
		virq = irq_of_parse_and_map(dn, 0);
		if (virq <= 0) {
			printk("opb: irq_op_parse_and_map failed!\n");
			continue;
		}

		
		rc = request_irq(virq, opb_irq_handler, IRQF_NO_THREAD,
				 "OPB LS Cascade", opb);
		if (rc) {
			printk("opb: request_irq failed: %d\n", rc);
			continue;
		}

		printk("OPB%d init with %d IRQs at %p\n", opb->index,
				OPB_NR_IRQS, opb->regs);
	}
}
