/*
 * File:	htirq.c
 * Purpose:	Hypertransport Interrupt Capability
 *
 * Copyright (C) 2006 Linux Networx
 * Copyright (C) Eric Biederman <ebiederman@lnxi.com>
 */

#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/htirq.h>

static DEFINE_SPINLOCK(ht_irq_lock);

struct ht_irq_cfg {
	struct pci_dev *dev;
	 
	ht_irq_update_t *update;
	unsigned pos;
	unsigned idx;
	struct ht_irq_msg msg;
};


void write_ht_irq_msg(unsigned int irq, struct ht_irq_msg *msg)
{
	struct ht_irq_cfg *cfg = irq_get_handler_data(irq);
	unsigned long flags;
	spin_lock_irqsave(&ht_irq_lock, flags);
	if (cfg->msg.address_lo != msg->address_lo) {
		pci_write_config_byte(cfg->dev, cfg->pos + 2, cfg->idx);
		pci_write_config_dword(cfg->dev, cfg->pos + 4, msg->address_lo);
	}
	if (cfg->msg.address_hi != msg->address_hi) {
		pci_write_config_byte(cfg->dev, cfg->pos + 2, cfg->idx + 1);
		pci_write_config_dword(cfg->dev, cfg->pos + 4, msg->address_hi);
	}
	if (cfg->update)
		cfg->update(cfg->dev, irq, msg);
	spin_unlock_irqrestore(&ht_irq_lock, flags);
	cfg->msg = *msg;
}

void fetch_ht_irq_msg(unsigned int irq, struct ht_irq_msg *msg)
{
	struct ht_irq_cfg *cfg = irq_get_handler_data(irq);
	*msg = cfg->msg;
}

void mask_ht_irq(struct irq_data *data)
{
	struct ht_irq_cfg *cfg = irq_data_get_irq_handler_data(data);
	struct ht_irq_msg msg = cfg->msg;

	msg.address_lo |= 1;
	write_ht_irq_msg(data->irq, &msg);
}

void unmask_ht_irq(struct irq_data *data)
{
	struct ht_irq_cfg *cfg = irq_data_get_irq_handler_data(data);
	struct ht_irq_msg msg = cfg->msg;

	msg.address_lo &= ~1;
	write_ht_irq_msg(data->irq, &msg);
}

int __ht_create_irq(struct pci_dev *dev, int idx, ht_irq_update_t *update)
{
	struct ht_irq_cfg *cfg;
	unsigned long flags;
	u32 data;
	int max_irq;
	int pos;
	int irq;
	int node;

	pos = pci_find_ht_capability(dev, HT_CAPTYPE_IRQ);
	if (!pos)
		return -EINVAL;

	
	spin_lock_irqsave(&ht_irq_lock, flags);
	pci_write_config_byte(dev, pos + 2, 1);
	pci_read_config_dword(dev, pos + 4, &data);
	spin_unlock_irqrestore(&ht_irq_lock, flags);

	max_irq = (data >> 16) & 0xff;
	if ( idx > max_irq)
		return -EINVAL;

	cfg = kmalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->dev = dev;
	cfg->update = update;
	cfg->pos = pos;
	cfg->idx = 0x10 + (idx * 2);
	
	cfg->msg.address_lo = 0xffffffff;
	cfg->msg.address_hi = 0xffffffff;

	node = dev_to_node(&dev->dev);
	irq = create_irq_nr(0, node);

	if (irq <= 0) {
		kfree(cfg);
		return -EBUSY;
	}
	irq_set_handler_data(irq, cfg);

	if (arch_setup_ht_irq(irq, dev) < 0) {
		ht_destroy_irq(irq);
		return -EBUSY;
	}

	return irq;
}

int ht_create_irq(struct pci_dev *dev, int idx)
{
	return __ht_create_irq(dev, idx, NULL);
}

void ht_destroy_irq(unsigned int irq)
{
	struct ht_irq_cfg *cfg;

	cfg = irq_get_handler_data(irq);
	irq_set_chip(irq, NULL);
	irq_set_handler_data(irq, NULL);
	destroy_irq(irq);

	kfree(cfg);
}

EXPORT_SYMBOL(__ht_create_irq);
EXPORT_SYMBOL(ht_create_irq);
EXPORT_SYMBOL(ht_destroy_irq);
