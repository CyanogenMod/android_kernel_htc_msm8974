/*
 * i8259 interrupt controller driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#undef DEBUG

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/prom.h>

static volatile void __iomem *pci_intack; 

static unsigned char cached_8259[2] = { 0xff, 0xff };
#define cached_A1 (cached_8259[0])
#define cached_21 (cached_8259[1])

static DEFINE_RAW_SPINLOCK(i8259_lock);

static struct irq_domain *i8259_host;

unsigned int i8259_irq(void)
{
	int irq;
	int lock = 0;

	
	if (pci_intack)
		irq = readb(pci_intack);
	else {
		raw_spin_lock(&i8259_lock);
		lock = 1;

		
		outb(0x0C, 0x20);		
		irq = inb(0x20) & 7;
		if (irq == 2 ) {
			outb(0x0C, 0xA0);	
			irq = (inb(0xA0) & 7) + 8;
		}
	}

	if (irq == 7) {
		if (!pci_intack)
			outb(0x0B, 0x20);	
		if(~inb(0x20) & 0x80)
			irq = NO_IRQ;
	} else if (irq == 0xff)
		irq = NO_IRQ;

	if (lock)
		raw_spin_unlock(&i8259_lock);
	return irq;
}

static void i8259_mask_and_ack_irq(struct irq_data *d)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&i8259_lock, flags);
	if (d->irq > 7) {
		cached_A1 |= 1 << (d->irq-8);
		inb(0xA1); 	
		outb(cached_A1, 0xA1);
		outb(0x20, 0xA0);	
		outb(0x20, 0x20);	
	} else {
		cached_21 |= 1 << d->irq;
		inb(0x21); 	
		outb(cached_21, 0x21);
		outb(0x20, 0x20);	
	}
	raw_spin_unlock_irqrestore(&i8259_lock, flags);
}

static void i8259_set_irq_mask(int irq_nr)
{
	outb(cached_A1,0xA1);
	outb(cached_21,0x21);
}

static void i8259_mask_irq(struct irq_data *d)
{
	unsigned long flags;

	pr_debug("i8259_mask_irq(%d)\n", d->irq);

	raw_spin_lock_irqsave(&i8259_lock, flags);
	if (d->irq < 8)
		cached_21 |= 1 << d->irq;
	else
		cached_A1 |= 1 << (d->irq-8);
	i8259_set_irq_mask(d->irq);
	raw_spin_unlock_irqrestore(&i8259_lock, flags);
}

static void i8259_unmask_irq(struct irq_data *d)
{
	unsigned long flags;

	pr_debug("i8259_unmask_irq(%d)\n", d->irq);

	raw_spin_lock_irqsave(&i8259_lock, flags);
	if (d->irq < 8)
		cached_21 &= ~(1 << d->irq);
	else
		cached_A1 &= ~(1 << (d->irq-8));
	i8259_set_irq_mask(d->irq);
	raw_spin_unlock_irqrestore(&i8259_lock, flags);
}

static struct irq_chip i8259_pic = {
	.name		= "i8259",
	.irq_mask	= i8259_mask_irq,
	.irq_disable	= i8259_mask_irq,
	.irq_unmask	= i8259_unmask_irq,
	.irq_mask_ack	= i8259_mask_and_ack_irq,
};

static struct resource pic1_iores = {
	.name = "8259 (master)",
	.start = 0x20,
	.end = 0x21,
	.flags = IORESOURCE_BUSY,
};

static struct resource pic2_iores = {
	.name = "8259 (slave)",
	.start = 0xa0,
	.end = 0xa1,
	.flags = IORESOURCE_BUSY,
};

static struct resource pic_edgectrl_iores = {
	.name = "8259 edge control",
	.start = 0x4d0,
	.end = 0x4d1,
	.flags = IORESOURCE_BUSY,
};

static int i8259_host_match(struct irq_domain *h, struct device_node *node)
{
	return h->of_node == NULL || h->of_node == node;
}

static int i8259_host_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	pr_debug("i8259_host_map(%d, 0x%lx)\n", virq, hw);

	
	if (hw == 2)
		irq_set_status_flags(virq, IRQ_NOREQUEST);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &i8259_pic, handle_level_irq);
	return 0;
}

static int i8259_host_xlate(struct irq_domain *h, struct device_node *ct,
			    const u32 *intspec, unsigned int intsize,
			    irq_hw_number_t *out_hwirq, unsigned int *out_flags)
{
	static unsigned char map_isa_senses[4] = {
		IRQ_TYPE_LEVEL_LOW,
		IRQ_TYPE_LEVEL_HIGH,
		IRQ_TYPE_EDGE_FALLING,
		IRQ_TYPE_EDGE_RISING,
	};

	*out_hwirq = intspec[0];
	if (intsize > 1 && intspec[1] < 4)
		*out_flags = map_isa_senses[intspec[1]];
	else
		*out_flags = IRQ_TYPE_NONE;

	return 0;
}

static struct irq_domain_ops i8259_host_ops = {
	.match = i8259_host_match,
	.map = i8259_host_map,
	.xlate = i8259_host_xlate,
};

struct irq_domain *i8259_get_host(void)
{
	return i8259_host;
}

void i8259_init(struct device_node *node, unsigned long intack_addr)
{
	unsigned long flags;

	
	raw_spin_lock_irqsave(&i8259_lock, flags);

	
	outb(0xff, 0xA1);
	outb(0xff, 0x21);

	
	outb(0x11, 0x20); 
	outb(0x00, 0x21); 
	outb(0x04, 0x21); 
	outb(0x01, 0x21); 

	
	outb(0x11, 0xA0); 
	outb(0x08, 0xA1); 
	outb(0x02, 0xA1); 
	outb(0x01, 0xA1); 

	
	udelay(100);

	
	outb(0x0B, 0x20);
	outb(0x0B, 0xA0);

	
	cached_21 &= ~(1 << 2);

	
	outb(cached_A1, 0xA1);
	outb(cached_21, 0x21);

	raw_spin_unlock_irqrestore(&i8259_lock, flags);

	
	i8259_host = irq_domain_add_legacy_isa(node, &i8259_host_ops, NULL);
	if (i8259_host == NULL) {
		printk(KERN_ERR "i8259: failed to allocate irq host !\n");
		return;
	}

	
	request_resource(&ioport_resource, &pic1_iores);
	request_resource(&ioport_resource, &pic2_iores);
	request_resource(&ioport_resource, &pic_edgectrl_iores);

	if (intack_addr != 0)
		pci_intack = ioremap(intack_addr, 1);

	printk(KERN_INFO "i8259 legacy interrupt controller initialized\n");
}
