/*
 * Support for the Tundra TSI148 VME-PCI Bridge Chip
 *
 * Author: Martyn Welch <martyn.welch@ge.com>
 * Copyright 2008 GE Intelligent Platforms Embedded Systems, Inc.
 *
 * Based on work by Tom Armistead and Ajit Prem
 * Copyright 2004 Motorola Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include "../vme.h"
#include "../vme_bridge.h"
#include "vme_tsi148.h"

static int __init tsi148_init(void);
static int tsi148_probe(struct pci_dev *, const struct pci_device_id *);
static void tsi148_remove(struct pci_dev *);
static void __exit tsi148_exit(void);


static bool err_chk;
static int geoid;

static const char driver_name[] = "vme_tsi148";

static DEFINE_PCI_DEVICE_TABLE(tsi148_ids) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_TUNDRA, PCI_DEVICE_ID_TUNDRA_TSI148) },
	{ },
};

static struct pci_driver tsi148_driver = {
	.name = driver_name,
	.id_table = tsi148_ids,
	.probe = tsi148_probe,
	.remove = tsi148_remove,
};

static void reg_join(unsigned int high, unsigned int low,
	unsigned long long *variable)
{
	*variable = (unsigned long long)high << 32;
	*variable |= (unsigned long long)low;
}

static void reg_split(unsigned long long variable, unsigned int *high,
	unsigned int *low)
{
	*low = (unsigned int)variable & 0xFFFFFFFF;
	*high = (unsigned int)(variable >> 32);
}

static u32 tsi148_DMA_irqhandler(struct tsi148_driver *bridge,
	int channel_mask)
{
	u32 serviced = 0;

	if (channel_mask & TSI148_LCSR_INTS_DMA0S) {
		wake_up(&bridge->dma_queue[0]);
		serviced |= TSI148_LCSR_INTC_DMA0C;
	}
	if (channel_mask & TSI148_LCSR_INTS_DMA1S) {
		wake_up(&bridge->dma_queue[1]);
		serviced |= TSI148_LCSR_INTC_DMA1C;
	}

	return serviced;
}

static u32 tsi148_LM_irqhandler(struct tsi148_driver *bridge, u32 stat)
{
	int i;
	u32 serviced = 0;

	for (i = 0; i < 4; i++) {
		if (stat & TSI148_LCSR_INTS_LMS[i]) {
			
			bridge->lm_callback[i](i);
			serviced |= TSI148_LCSR_INTC_LMC[i];
		}
	}

	return serviced;
}

static u32 tsi148_MB_irqhandler(struct vme_bridge *tsi148_bridge, u32 stat)
{
	int i;
	u32 val;
	u32 serviced = 0;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	for (i = 0; i < 4; i++) {
		if (stat & TSI148_LCSR_INTS_MBS[i]) {
			val = ioread32be(bridge->base +	TSI148_GCSR_MBOX[i]);
			dev_err(tsi148_bridge->parent, "VME Mailbox %d received"
				": 0x%x\n", i, val);
			serviced |= TSI148_LCSR_INTC_MBC[i];
		}
	}

	return serviced;
}

static u32 tsi148_PERR_irqhandler(struct vme_bridge *tsi148_bridge)
{
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	dev_err(tsi148_bridge->parent, "PCI Exception at address: 0x%08x:%08x, "
		"attributes: %08x\n",
		ioread32be(bridge->base + TSI148_LCSR_EDPAU),
		ioread32be(bridge->base + TSI148_LCSR_EDPAL),
		ioread32be(bridge->base + TSI148_LCSR_EDPAT));

	dev_err(tsi148_bridge->parent, "PCI-X attribute reg: %08x, PCI-X split "
		"completion reg: %08x\n",
		ioread32be(bridge->base + TSI148_LCSR_EDPXA),
		ioread32be(bridge->base + TSI148_LCSR_EDPXS));

	iowrite32be(TSI148_LCSR_EDPAT_EDPCL, bridge->base + TSI148_LCSR_EDPAT);

	return TSI148_LCSR_INTC_PERRC;
}

static u32 tsi148_VERR_irqhandler(struct vme_bridge *tsi148_bridge)
{
	unsigned int error_addr_high, error_addr_low;
	unsigned long long error_addr;
	u32 error_attrib;
	struct vme_bus_error *error;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	error_addr_high = ioread32be(bridge->base + TSI148_LCSR_VEAU);
	error_addr_low = ioread32be(bridge->base + TSI148_LCSR_VEAL);
	error_attrib = ioread32be(bridge->base + TSI148_LCSR_VEAT);

	reg_join(error_addr_high, error_addr_low, &error_addr);

	
	if (error_attrib & TSI148_LCSR_VEAT_VEOF) {
		dev_err(tsi148_bridge->parent, "VME Bus Exception Overflow "
			"Occurred\n");
	}

	error = kmalloc(sizeof(struct vme_bus_error), GFP_ATOMIC);
	if (error) {
		error->address = error_addr;
		error->attributes = error_attrib;
		list_add_tail(&error->list, &tsi148_bridge->vme_errors);
	} else {
		dev_err(tsi148_bridge->parent, "Unable to alloc memory for "
			"VMEbus Error reporting\n");
		dev_err(tsi148_bridge->parent, "VME Bus Error at address: "
			"0x%llx, attributes: %08x\n", error_addr, error_attrib);
	}

	
	iowrite32be(TSI148_LCSR_VEAT_VESCL, bridge->base + TSI148_LCSR_VEAT);

	return TSI148_LCSR_INTC_VERRC;
}

static u32 tsi148_IACK_irqhandler(struct tsi148_driver *bridge)
{
	wake_up(&bridge->iack_queue);

	return TSI148_LCSR_INTC_IACKC;
}

static u32 tsi148_VIRQ_irqhandler(struct vme_bridge *tsi148_bridge,
	u32 stat)
{
	int vec, i, serviced = 0;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	for (i = 7; i > 0; i--) {
		if (stat & (1 << i)) {
			vec = ioread8(bridge->base + TSI148_LCSR_VIACK[i] + 3);

			vme_irq_handler(tsi148_bridge, i, vec);

			serviced |= (1 << i);
		}
	}

	return serviced;
}

static irqreturn_t tsi148_irqhandler(int irq, void *ptr)
{
	u32 stat, enable, serviced = 0;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = ptr;

	bridge = tsi148_bridge->driver_priv;

	
	enable = ioread32be(bridge->base + TSI148_LCSR_INTEO);
	stat = ioread32be(bridge->base + TSI148_LCSR_INTS);

	
	stat &= enable;

	if (unlikely(!stat))
		return IRQ_NONE;

	
	
	if (stat & (TSI148_LCSR_INTS_DMA1S | TSI148_LCSR_INTS_DMA0S))
		serviced |= tsi148_DMA_irqhandler(bridge, stat);

	
	if (stat & (TSI148_LCSR_INTS_LM3S | TSI148_LCSR_INTS_LM2S |
			TSI148_LCSR_INTS_LM1S | TSI148_LCSR_INTS_LM0S))
		serviced |= tsi148_LM_irqhandler(bridge, stat);

	
	if (stat & (TSI148_LCSR_INTS_MB3S | TSI148_LCSR_INTS_MB2S |
			TSI148_LCSR_INTS_MB1S | TSI148_LCSR_INTS_MB0S))
		serviced |= tsi148_MB_irqhandler(tsi148_bridge, stat);

	
	if (stat & TSI148_LCSR_INTS_PERRS)
		serviced |= tsi148_PERR_irqhandler(tsi148_bridge);

	
	if (stat & TSI148_LCSR_INTS_VERRS)
		serviced |= tsi148_VERR_irqhandler(tsi148_bridge);

	
	if (stat & TSI148_LCSR_INTS_IACKS)
		serviced |= tsi148_IACK_irqhandler(bridge);

	
	if (stat & (TSI148_LCSR_INTS_IRQ7S | TSI148_LCSR_INTS_IRQ6S |
			TSI148_LCSR_INTS_IRQ5S | TSI148_LCSR_INTS_IRQ4S |
			TSI148_LCSR_INTS_IRQ3S | TSI148_LCSR_INTS_IRQ2S |
			TSI148_LCSR_INTS_IRQ1S))
		serviced |= tsi148_VIRQ_irqhandler(tsi148_bridge, stat);

	
	iowrite32be(serviced, bridge->base + TSI148_LCSR_INTC);

	return IRQ_HANDLED;
}

static int tsi148_irq_init(struct vme_bridge *tsi148_bridge)
{
	int result;
	unsigned int tmp;
	struct pci_dev *pdev;
	struct tsi148_driver *bridge;

	pdev = container_of(tsi148_bridge->parent, struct pci_dev, dev);

	bridge = tsi148_bridge->driver_priv;

	
	INIT_LIST_HEAD(&tsi148_bridge->vme_errors);

	mutex_init(&tsi148_bridge->irq_mtx);

	result = request_irq(pdev->irq,
			     tsi148_irqhandler,
			     IRQF_SHARED,
			     driver_name, tsi148_bridge);
	if (result) {
		dev_err(tsi148_bridge->parent, "Can't get assigned pci irq "
			"vector %02X\n", pdev->irq);
		return result;
	}

	
	tmp = TSI148_LCSR_INTEO_DMA1EO | TSI148_LCSR_INTEO_DMA0EO |
		TSI148_LCSR_INTEO_MB3EO | TSI148_LCSR_INTEO_MB2EO |
		TSI148_LCSR_INTEO_MB1EO | TSI148_LCSR_INTEO_MB0EO |
		TSI148_LCSR_INTEO_PERREO | TSI148_LCSR_INTEO_VERREO |
		TSI148_LCSR_INTEO_IACKEO;




	iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEO);
	iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEN);

	return 0;
}

static void tsi148_irq_exit(struct vme_bridge *tsi148_bridge,
	struct pci_dev *pdev)
{
	struct tsi148_driver *bridge = tsi148_bridge->driver_priv;

	
	iowrite32be(0x0, bridge->base + TSI148_LCSR_INTEO);
	iowrite32be(0x0, bridge->base + TSI148_LCSR_INTEN);

	
	iowrite32be(0xFFFFFFFF, bridge->base + TSI148_LCSR_INTC);

	
	free_irq(pdev->irq, tsi148_bridge);
}

static int tsi148_iack_received(struct tsi148_driver *bridge)
{
	u32 tmp;

	tmp = ioread32be(bridge->base + TSI148_LCSR_VICR);

	if (tmp & TSI148_LCSR_VICR_IRQS)
		return 0;
	else
		return 1;
}

static void tsi148_irq_set(struct vme_bridge *tsi148_bridge, int level,
	int state, int sync)
{
	struct pci_dev *pdev;
	u32 tmp;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	
	if (state == 0) {
		tmp = ioread32be(bridge->base + TSI148_LCSR_INTEN);
		tmp &= ~TSI148_LCSR_INTEN_IRQEN[level - 1];
		iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEN);

		tmp = ioread32be(bridge->base + TSI148_LCSR_INTEO);
		tmp &= ~TSI148_LCSR_INTEO_IRQEO[level - 1];
		iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEO);

		if (sync != 0) {
			pdev = container_of(tsi148_bridge->parent,
				struct pci_dev, dev);

			synchronize_irq(pdev->irq);
		}
	} else {
		tmp = ioread32be(bridge->base + TSI148_LCSR_INTEO);
		tmp |= TSI148_LCSR_INTEO_IRQEO[level - 1];
		iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEO);

		tmp = ioread32be(bridge->base + TSI148_LCSR_INTEN);
		tmp |= TSI148_LCSR_INTEN_IRQEN[level - 1];
		iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEN);
	}
}

static int tsi148_irq_generate(struct vme_bridge *tsi148_bridge, int level,
	int statid)
{
	u32 tmp;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	mutex_lock(&bridge->vme_int);

	
	tmp = ioread32be(bridge->base + TSI148_LCSR_VICR);

	
	tmp = (tmp & ~TSI148_LCSR_VICR_STID_M) |
		(statid & TSI148_LCSR_VICR_STID_M);
	iowrite32be(tmp, bridge->base + TSI148_LCSR_VICR);

	
	tmp = tmp | TSI148_LCSR_VICR_IRQL[level];
	iowrite32be(tmp, bridge->base + TSI148_LCSR_VICR);

	
	wait_event_interruptible(bridge->iack_queue,
		tsi148_iack_received(bridge));

	mutex_unlock(&bridge->vme_int);

	return 0;
}

static struct vme_bus_error *tsi148_find_error(struct vme_bridge *tsi148_bridge,
	u32 aspace, unsigned long long address, size_t count)
{
	struct list_head *err_pos;
	struct vme_bus_error *vme_err, *valid = NULL;
	unsigned long long bound;

	bound = address + count;

	err_pos = NULL;
	
	list_for_each(err_pos, &tsi148_bridge->vme_errors) {
		vme_err = list_entry(err_pos, struct vme_bus_error, list);
		if ((vme_err->address >= address) &&
			(vme_err->address < bound)) {

			valid = vme_err;
			break;
		}
	}

	return valid;
}

static void tsi148_clear_errors(struct vme_bridge *tsi148_bridge,
	u32 aspace, unsigned long long address, size_t count)
{
	struct list_head *err_pos, *temp;
	struct vme_bus_error *vme_err;
	unsigned long long bound;

	bound = address + count;

	err_pos = NULL;
	
	list_for_each_safe(err_pos, temp, &tsi148_bridge->vme_errors) {
		vme_err = list_entry(err_pos, struct vme_bus_error, list);

		if ((vme_err->address >= address) &&
			(vme_err->address < bound)) {

			list_del(err_pos);
			kfree(vme_err);
		}
	}
}

static int tsi148_slave_set(struct vme_slave_resource *image, int enabled,
	unsigned long long vme_base, unsigned long long size,
	dma_addr_t pci_base, u32 aspace, u32 cycle)
{
	unsigned int i, addr = 0, granularity = 0;
	unsigned int temp_ctl = 0;
	unsigned int vme_base_low, vme_base_high;
	unsigned int vme_bound_low, vme_bound_high;
	unsigned int pci_offset_low, pci_offset_high;
	unsigned long long vme_bound, pci_offset;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = image->parent;
	bridge = tsi148_bridge->driver_priv;

	i = image->number;

	switch (aspace) {
	case VME_A16:
		granularity = 0x10;
		addr |= TSI148_LCSR_ITAT_AS_A16;
		break;
	case VME_A24:
		granularity = 0x1000;
		addr |= TSI148_LCSR_ITAT_AS_A24;
		break;
	case VME_A32:
		granularity = 0x10000;
		addr |= TSI148_LCSR_ITAT_AS_A32;
		break;
	case VME_A64:
		granularity = 0x10000;
		addr |= TSI148_LCSR_ITAT_AS_A64;
		break;
	case VME_CRCSR:
	case VME_USER1:
	case VME_USER2:
	case VME_USER3:
	case VME_USER4:
	default:
		dev_err(tsi148_bridge->parent, "Invalid address space\n");
		return -EINVAL;
		break;
	}

	
	reg_split(vme_base, &vme_base_high, &vme_base_low);

	vme_bound = vme_base + size - granularity;
	reg_split(vme_bound, &vme_bound_high, &vme_bound_low);
	pci_offset = (unsigned long long)pci_base - vme_base;
	reg_split(pci_offset, &pci_offset_high, &pci_offset_low);

	if (vme_base_low & (granularity - 1)) {
		dev_err(tsi148_bridge->parent, "Invalid VME base alignment\n");
		return -EINVAL;
	}
	if (vme_bound_low & (granularity - 1)) {
		dev_err(tsi148_bridge->parent, "Invalid VME bound alignment\n");
		return -EINVAL;
	}
	if (pci_offset_low & (granularity - 1)) {
		dev_err(tsi148_bridge->parent, "Invalid PCI Offset "
			"alignment\n");
		return -EINVAL;
	}

	
	temp_ctl = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITAT);
	temp_ctl &= ~TSI148_LCSR_ITAT_EN;
	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITAT);

	
	iowrite32be(vme_base_high, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITSAU);
	iowrite32be(vme_base_low, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITSAL);
	iowrite32be(vme_bound_high, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITEAU);
	iowrite32be(vme_bound_low, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITEAL);
	iowrite32be(pci_offset_high, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITOFU);
	iowrite32be(pci_offset_low, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITOFL);

	
	temp_ctl &= ~TSI148_LCSR_ITAT_2eSSTM_M;
	switch (cycle & (VME_2eSST160 | VME_2eSST267 | VME_2eSST320)) {
	case VME_2eSST160:
		temp_ctl |= TSI148_LCSR_ITAT_2eSSTM_160;
		break;
	case VME_2eSST267:
		temp_ctl |= TSI148_LCSR_ITAT_2eSSTM_267;
		break;
	case VME_2eSST320:
		temp_ctl |= TSI148_LCSR_ITAT_2eSSTM_320;
		break;
	}

	
	temp_ctl &= ~(0x1F << 7);
	if (cycle & VME_BLT)
		temp_ctl |= TSI148_LCSR_ITAT_BLT;
	if (cycle & VME_MBLT)
		temp_ctl |= TSI148_LCSR_ITAT_MBLT;
	if (cycle & VME_2eVME)
		temp_ctl |= TSI148_LCSR_ITAT_2eVME;
	if (cycle & VME_2eSST)
		temp_ctl |= TSI148_LCSR_ITAT_2eSST;
	if (cycle & VME_2eSSTB)
		temp_ctl |= TSI148_LCSR_ITAT_2eSSTB;

	
	temp_ctl &= ~TSI148_LCSR_ITAT_AS_M;
	temp_ctl |= addr;

	temp_ctl &= ~0xF;
	if (cycle & VME_SUPER)
		temp_ctl |= TSI148_LCSR_ITAT_SUPR ;
	if (cycle & VME_USER)
		temp_ctl |= TSI148_LCSR_ITAT_NPRIV;
	if (cycle & VME_PROG)
		temp_ctl |= TSI148_LCSR_ITAT_PGM;
	if (cycle & VME_DATA)
		temp_ctl |= TSI148_LCSR_ITAT_DATA;

	
	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITAT);

	if (enabled)
		temp_ctl |= TSI148_LCSR_ITAT_EN;

	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITAT);

	return 0;
}

static int tsi148_slave_get(struct vme_slave_resource *image, int *enabled,
	unsigned long long *vme_base, unsigned long long *size,
	dma_addr_t *pci_base, u32 *aspace, u32 *cycle)
{
	unsigned int i, granularity = 0, ctl = 0;
	unsigned int vme_base_low, vme_base_high;
	unsigned int vme_bound_low, vme_bound_high;
	unsigned int pci_offset_low, pci_offset_high;
	unsigned long long vme_bound, pci_offset;
	struct tsi148_driver *bridge;

	bridge = image->parent->driver_priv;

	i = image->number;

	
	ctl = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITAT);

	vme_base_high = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITSAU);
	vme_base_low = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITSAL);
	vme_bound_high = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITEAU);
	vme_bound_low = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITEAL);
	pci_offset_high = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITOFU);
	pci_offset_low = ioread32be(bridge->base + TSI148_LCSR_IT[i] +
		TSI148_LCSR_OFFSET_ITOFL);

	
	reg_join(vme_base_high, vme_base_low, vme_base);
	reg_join(vme_bound_high, vme_bound_low, &vme_bound);
	reg_join(pci_offset_high, pci_offset_low, &pci_offset);

	*pci_base = (dma_addr_t)vme_base + pci_offset;

	*enabled = 0;
	*aspace = 0;
	*cycle = 0;

	if (ctl & TSI148_LCSR_ITAT_EN)
		*enabled = 1;

	if ((ctl & TSI148_LCSR_ITAT_AS_M) == TSI148_LCSR_ITAT_AS_A16) {
		granularity = 0x10;
		*aspace |= VME_A16;
	}
	if ((ctl & TSI148_LCSR_ITAT_AS_M) == TSI148_LCSR_ITAT_AS_A24) {
		granularity = 0x1000;
		*aspace |= VME_A24;
	}
	if ((ctl & TSI148_LCSR_ITAT_AS_M) == TSI148_LCSR_ITAT_AS_A32) {
		granularity = 0x10000;
		*aspace |= VME_A32;
	}
	if ((ctl & TSI148_LCSR_ITAT_AS_M) == TSI148_LCSR_ITAT_AS_A64) {
		granularity = 0x10000;
		*aspace |= VME_A64;
	}

	
	*size = (unsigned long long)((vme_bound - *vme_base) + granularity);


	if ((ctl & TSI148_LCSR_ITAT_2eSSTM_M) == TSI148_LCSR_ITAT_2eSSTM_160)
		*cycle |= VME_2eSST160;
	if ((ctl & TSI148_LCSR_ITAT_2eSSTM_M) == TSI148_LCSR_ITAT_2eSSTM_267)
		*cycle |= VME_2eSST267;
	if ((ctl & TSI148_LCSR_ITAT_2eSSTM_M) == TSI148_LCSR_ITAT_2eSSTM_320)
		*cycle |= VME_2eSST320;

	if (ctl & TSI148_LCSR_ITAT_BLT)
		*cycle |= VME_BLT;
	if (ctl & TSI148_LCSR_ITAT_MBLT)
		*cycle |= VME_MBLT;
	if (ctl & TSI148_LCSR_ITAT_2eVME)
		*cycle |= VME_2eVME;
	if (ctl & TSI148_LCSR_ITAT_2eSST)
		*cycle |= VME_2eSST;
	if (ctl & TSI148_LCSR_ITAT_2eSSTB)
		*cycle |= VME_2eSSTB;

	if (ctl & TSI148_LCSR_ITAT_SUPR)
		*cycle |= VME_SUPER;
	if (ctl & TSI148_LCSR_ITAT_NPRIV)
		*cycle |= VME_USER;
	if (ctl & TSI148_LCSR_ITAT_PGM)
		*cycle |= VME_PROG;
	if (ctl & TSI148_LCSR_ITAT_DATA)
		*cycle |= VME_DATA;

	return 0;
}

static int tsi148_alloc_resource(struct vme_master_resource *image,
	unsigned long long size)
{
	unsigned long long existing_size;
	int retval = 0;
	struct pci_dev *pdev;
	struct vme_bridge *tsi148_bridge;

	tsi148_bridge = image->parent;

	pdev = container_of(tsi148_bridge->parent, struct pci_dev, dev);

	existing_size = (unsigned long long)(image->bus_resource.end -
		image->bus_resource.start);

	
	if ((size != 0) && (existing_size == (size - 1)))
		return 0;

	if (existing_size != 0) {
		iounmap(image->kern_base);
		image->kern_base = NULL;
		kfree(image->bus_resource.name);
		release_resource(&image->bus_resource);
		memset(&image->bus_resource, 0, sizeof(struct resource));
	}

	
	if (size == 0)
		return 0;

	if (image->bus_resource.name == NULL) {
		image->bus_resource.name = kmalloc(VMENAMSIZ+3, GFP_ATOMIC);
		if (image->bus_resource.name == NULL) {
			dev_err(tsi148_bridge->parent, "Unable to allocate "
				"memory for resource name\n");
			retval = -ENOMEM;
			goto err_name;
		}
	}

	sprintf((char *)image->bus_resource.name, "%s.%d", tsi148_bridge->name,
		image->number);

	image->bus_resource.start = 0;
	image->bus_resource.end = (unsigned long)size;
	image->bus_resource.flags = IORESOURCE_MEM;

	retval = pci_bus_alloc_resource(pdev->bus,
		&image->bus_resource, size, size, PCIBIOS_MIN_MEM,
		0, NULL, NULL);
	if (retval) {
		dev_err(tsi148_bridge->parent, "Failed to allocate mem "
			"resource for window %d size 0x%lx start 0x%lx\n",
			image->number, (unsigned long)size,
			(unsigned long)image->bus_resource.start);
		goto err_resource;
	}

	image->kern_base = ioremap_nocache(
		image->bus_resource.start, size);
	if (image->kern_base == NULL) {
		dev_err(tsi148_bridge->parent, "Failed to remap resource\n");
		retval = -ENOMEM;
		goto err_remap;
	}

	return 0;

err_remap:
	release_resource(&image->bus_resource);
err_resource:
	kfree(image->bus_resource.name);
	memset(&image->bus_resource, 0, sizeof(struct resource));
err_name:
	return retval;
}

static void tsi148_free_resource(struct vme_master_resource *image)
{
	iounmap(image->kern_base);
	image->kern_base = NULL;
	release_resource(&image->bus_resource);
	kfree(image->bus_resource.name);
	memset(&image->bus_resource, 0, sizeof(struct resource));
}

static int tsi148_master_set(struct vme_master_resource *image, int enabled,
	unsigned long long vme_base, unsigned long long size, u32 aspace,
	u32 cycle, u32 dwidth)
{
	int retval = 0;
	unsigned int i;
	unsigned int temp_ctl = 0;
	unsigned int pci_base_low, pci_base_high;
	unsigned int pci_bound_low, pci_bound_high;
	unsigned int vme_offset_low, vme_offset_high;
	unsigned long long pci_bound, vme_offset, pci_base;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = image->parent;

	bridge = tsi148_bridge->driver_priv;

	
	if (vme_base & 0xFFFF) {
		dev_err(tsi148_bridge->parent, "Invalid VME Window "
			"alignment\n");
		retval = -EINVAL;
		goto err_window;
	}

	if ((size == 0) && (enabled != 0)) {
		dev_err(tsi148_bridge->parent, "Size must be non-zero for "
			"enabled windows\n");
		retval = -EINVAL;
		goto err_window;
	}

	spin_lock(&image->lock);

	retval = tsi148_alloc_resource(image, size);
	if (retval) {
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Unable to allocate memory for "
			"resource\n");
		goto err_res;
	}

	if (size == 0) {
		pci_base = 0;
		pci_bound = 0;
		vme_offset = 0;
	} else {
		pci_base = (unsigned long long)image->bus_resource.start;

		pci_bound = pci_base + (size - 0x10000);
		vme_offset = vme_base - pci_base;
	}

	
	reg_split(pci_base, &pci_base_high, &pci_base_low);
	reg_split(pci_bound, &pci_bound_high, &pci_bound_low);
	reg_split(vme_offset, &vme_offset_high, &vme_offset_low);

	if (pci_base_low & 0xFFFF) {
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Invalid PCI base alignment\n");
		retval = -EINVAL;
		goto err_gran;
	}
	if (pci_bound_low & 0xFFFF) {
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Invalid PCI bound alignment\n");
		retval = -EINVAL;
		goto err_gran;
	}
	if (vme_offset_low & 0xFFFF) {
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Invalid VME Offset "
			"alignment\n");
		retval = -EINVAL;
		goto err_gran;
	}

	i = image->number;

	
	temp_ctl = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTAT);
	temp_ctl &= ~TSI148_LCSR_OTAT_EN;
	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTAT);

	
	temp_ctl &= ~TSI148_LCSR_OTAT_2eSSTM_M;
	switch (cycle & (VME_2eSST160 | VME_2eSST267 | VME_2eSST320)) {
	case VME_2eSST160:
		temp_ctl |= TSI148_LCSR_OTAT_2eSSTM_160;
		break;
	case VME_2eSST267:
		temp_ctl |= TSI148_LCSR_OTAT_2eSSTM_267;
		break;
	case VME_2eSST320:
		temp_ctl |= TSI148_LCSR_OTAT_2eSSTM_320;
		break;
	}

	
	if (cycle & VME_BLT) {
		temp_ctl &= ~TSI148_LCSR_OTAT_TM_M;
		temp_ctl |= TSI148_LCSR_OTAT_TM_BLT;
	}
	if (cycle & VME_MBLT) {
		temp_ctl &= ~TSI148_LCSR_OTAT_TM_M;
		temp_ctl |= TSI148_LCSR_OTAT_TM_MBLT;
	}
	if (cycle & VME_2eVME) {
		temp_ctl &= ~TSI148_LCSR_OTAT_TM_M;
		temp_ctl |= TSI148_LCSR_OTAT_TM_2eVME;
	}
	if (cycle & VME_2eSST) {
		temp_ctl &= ~TSI148_LCSR_OTAT_TM_M;
		temp_ctl |= TSI148_LCSR_OTAT_TM_2eSST;
	}
	if (cycle & VME_2eSSTB) {
		dev_warn(tsi148_bridge->parent, "Currently not setting "
			"Broadcast Select Registers\n");
		temp_ctl &= ~TSI148_LCSR_OTAT_TM_M;
		temp_ctl |= TSI148_LCSR_OTAT_TM_2eSSTB;
	}

	
	temp_ctl &= ~TSI148_LCSR_OTAT_DBW_M;
	switch (dwidth) {
	case VME_D16:
		temp_ctl |= TSI148_LCSR_OTAT_DBW_16;
		break;
	case VME_D32:
		temp_ctl |= TSI148_LCSR_OTAT_DBW_32;
		break;
	default:
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Invalid data width\n");
		retval = -EINVAL;
		goto err_dwidth;
	}

	
	temp_ctl &= ~TSI148_LCSR_OTAT_AMODE_M;
	switch (aspace) {
	case VME_A16:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_A16;
		break;
	case VME_A24:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_A24;
		break;
	case VME_A32:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_A32;
		break;
	case VME_A64:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_A64;
		break;
	case VME_CRCSR:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_CRCSR;
		break;
	case VME_USER1:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_USER1;
		break;
	case VME_USER2:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_USER2;
		break;
	case VME_USER3:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_USER3;
		break;
	case VME_USER4:
		temp_ctl |= TSI148_LCSR_OTAT_AMODE_USER4;
		break;
	default:
		spin_unlock(&image->lock);
		dev_err(tsi148_bridge->parent, "Invalid address space\n");
		retval = -EINVAL;
		goto err_aspace;
		break;
	}

	temp_ctl &= ~(3<<4);
	if (cycle & VME_SUPER)
		temp_ctl |= TSI148_LCSR_OTAT_SUP;
	if (cycle & VME_PROG)
		temp_ctl |= TSI148_LCSR_OTAT_PGM;

	
	iowrite32be(pci_base_high, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAU);
	iowrite32be(pci_base_low, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAL);
	iowrite32be(pci_bound_high, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTEAU);
	iowrite32be(pci_bound_low, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTEAL);
	iowrite32be(vme_offset_high, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTOFU);
	iowrite32be(vme_offset_low, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTOFL);

	
	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTAT);

	if (enabled)
		temp_ctl |= TSI148_LCSR_OTAT_EN;

	iowrite32be(temp_ctl, bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTAT);

	spin_unlock(&image->lock);
	return 0;

err_aspace:
err_dwidth:
err_gran:
	tsi148_free_resource(image);
err_res:
err_window:
	return retval;

}

static int __tsi148_master_get(struct vme_master_resource *image, int *enabled,
	unsigned long long *vme_base, unsigned long long *size, u32 *aspace,
	u32 *cycle, u32 *dwidth)
{
	unsigned int i, ctl;
	unsigned int pci_base_low, pci_base_high;
	unsigned int pci_bound_low, pci_bound_high;
	unsigned int vme_offset_low, vme_offset_high;

	unsigned long long pci_base, pci_bound, vme_offset;
	struct tsi148_driver *bridge;

	bridge = image->parent->driver_priv;

	i = image->number;

	ctl = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTAT);

	pci_base_high = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAU);
	pci_base_low = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAL);
	pci_bound_high = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTEAU);
	pci_bound_low = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTEAL);
	vme_offset_high = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTOFU);
	vme_offset_low = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTOFL);

	
	reg_join(pci_base_high, pci_base_low, &pci_base);
	reg_join(pci_bound_high, pci_bound_low, &pci_bound);
	reg_join(vme_offset_high, vme_offset_low, &vme_offset);

	*vme_base = pci_base + vme_offset;
	*size = (unsigned long long)(pci_bound - pci_base) + 0x10000;

	*enabled = 0;
	*aspace = 0;
	*cycle = 0;
	*dwidth = 0;

	if (ctl & TSI148_LCSR_OTAT_EN)
		*enabled = 1;

	
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_A16)
		*aspace |= VME_A16;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_A24)
		*aspace |= VME_A24;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_A32)
		*aspace |= VME_A32;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_A64)
		*aspace |= VME_A64;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_CRCSR)
		*aspace |= VME_CRCSR;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_USER1)
		*aspace |= VME_USER1;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_USER2)
		*aspace |= VME_USER2;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_USER3)
		*aspace |= VME_USER3;
	if ((ctl & TSI148_LCSR_OTAT_AMODE_M) == TSI148_LCSR_OTAT_AMODE_USER4)
		*aspace |= VME_USER4;

	
	if ((ctl & TSI148_LCSR_OTAT_2eSSTM_M) == TSI148_LCSR_OTAT_2eSSTM_160)
		*cycle |= VME_2eSST160;
	if ((ctl & TSI148_LCSR_OTAT_2eSSTM_M) == TSI148_LCSR_OTAT_2eSSTM_267)
		*cycle |= VME_2eSST267;
	if ((ctl & TSI148_LCSR_OTAT_2eSSTM_M) == TSI148_LCSR_OTAT_2eSSTM_320)
		*cycle |= VME_2eSST320;

	
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_SCT)
		*cycle |= VME_SCT;
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_BLT)
		*cycle |= VME_BLT;
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_MBLT)
		*cycle |= VME_MBLT;
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_2eVME)
		*cycle |= VME_2eVME;
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_2eSST)
		*cycle |= VME_2eSST;
	if ((ctl & TSI148_LCSR_OTAT_TM_M) == TSI148_LCSR_OTAT_TM_2eSSTB)
		*cycle |= VME_2eSSTB;

	if (ctl & TSI148_LCSR_OTAT_SUP)
		*cycle |= VME_SUPER;
	else
		*cycle |= VME_USER;

	if (ctl & TSI148_LCSR_OTAT_PGM)
		*cycle |= VME_PROG;
	else
		*cycle |= VME_DATA;

	
	if ((ctl & TSI148_LCSR_OTAT_DBW_M) == TSI148_LCSR_OTAT_DBW_16)
		*dwidth = VME_D16;
	if ((ctl & TSI148_LCSR_OTAT_DBW_M) == TSI148_LCSR_OTAT_DBW_32)
		*dwidth = VME_D32;

	return 0;
}


static int tsi148_master_get(struct vme_master_resource *image, int *enabled,
	unsigned long long *vme_base, unsigned long long *size, u32 *aspace,
	u32 *cycle, u32 *dwidth)
{
	int retval;

	spin_lock(&image->lock);

	retval = __tsi148_master_get(image, enabled, vme_base, size, aspace,
		cycle, dwidth);

	spin_unlock(&image->lock);

	return retval;
}

static ssize_t tsi148_master_read(struct vme_master_resource *image, void *buf,
	size_t count, loff_t offset)
{
	int retval, enabled;
	unsigned long long vme_base, size;
	u32 aspace, cycle, dwidth;
	struct vme_bus_error *vme_err = NULL;
	struct vme_bridge *tsi148_bridge;

	tsi148_bridge = image->parent;

	spin_lock(&image->lock);

	memcpy_fromio(buf, image->kern_base + offset, (unsigned int)count);
	retval = count;

	if (!err_chk)
		goto skip_chk;

	__tsi148_master_get(image, &enabled, &vme_base, &size, &aspace, &cycle,
		&dwidth);

	vme_err = tsi148_find_error(tsi148_bridge, aspace, vme_base + offset,
		count);
	if (vme_err != NULL) {
		dev_err(image->parent->parent, "First VME read error detected "
			"an at address 0x%llx\n", vme_err->address);
		retval = vme_err->address - (vme_base + offset);
		
		tsi148_clear_errors(tsi148_bridge, aspace, vme_base + offset,
			count);
	}

skip_chk:
	spin_unlock(&image->lock);

	return retval;
}


static ssize_t tsi148_master_write(struct vme_master_resource *image, void *buf,
	size_t count, loff_t offset)
{
	int retval = 0, enabled;
	unsigned long long vme_base, size;
	u32 aspace, cycle, dwidth;

	struct vme_bus_error *vme_err = NULL;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = image->parent;

	bridge = tsi148_bridge->driver_priv;

	spin_lock(&image->lock);

	memcpy_toio(image->kern_base + offset, buf, (unsigned int)count);
	retval = count;

	/*
	 * Writes are posted. We need to do a read on the VME bus to flush out
	 * all of the writes before we check for errors. We can't guarantee
	 * that reading the data we have just written is safe. It is believed
	 * that there isn't any read, write re-ordering, so we can read any
	 * location in VME space, so lets read the Device ID from the tsi148's
	 * own registers as mapped into CR/CSR space.
	 *
	 * We check for saved errors in the written address range/space.
	 */

	if (!err_chk)
		goto skip_chk;

	__tsi148_master_get(image, &enabled, &vme_base, &size, &aspace, &cycle,
		&dwidth);

	ioread16(bridge->flush_image->kern_base + 0x7F000);

	vme_err = tsi148_find_error(tsi148_bridge, aspace, vme_base + offset,
		count);
	if (vme_err != NULL) {
		dev_warn(tsi148_bridge->parent, "First VME write error detected"
			" an at address 0x%llx\n", vme_err->address);
		retval = vme_err->address - (vme_base + offset);
		
		tsi148_clear_errors(tsi148_bridge, aspace, vme_base + offset,
			count);
	}

skip_chk:
	spin_unlock(&image->lock);

	return retval;
}

static unsigned int tsi148_master_rmw(struct vme_master_resource *image,
	unsigned int mask, unsigned int compare, unsigned int swap,
	loff_t offset)
{
	unsigned long long pci_addr;
	unsigned int pci_addr_high, pci_addr_low;
	u32 tmp, result;
	int i;
	struct tsi148_driver *bridge;

	bridge = image->parent->driver_priv;

	
	i = image->number;

	
	mutex_lock(&bridge->vme_rmw);

	
	spin_lock(&image->lock);

	pci_addr_high = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAU);
	pci_addr_low = ioread32be(bridge->base + TSI148_LCSR_OT[i] +
		TSI148_LCSR_OFFSET_OTSAL);

	reg_join(pci_addr_high, pci_addr_low, &pci_addr);
	reg_split(pci_addr + offset, &pci_addr_high, &pci_addr_low);

	
	iowrite32be(mask, bridge->base + TSI148_LCSR_RMWEN);
	iowrite32be(compare, bridge->base + TSI148_LCSR_RMWC);
	iowrite32be(swap, bridge->base + TSI148_LCSR_RMWS);
	iowrite32be(pci_addr_high, bridge->base + TSI148_LCSR_RMWAU);
	iowrite32be(pci_addr_low, bridge->base + TSI148_LCSR_RMWAL);

	
	tmp = ioread32be(bridge->base + TSI148_LCSR_VMCTRL);
	tmp |= TSI148_LCSR_VMCTRL_RMWEN;
	iowrite32be(tmp, bridge->base + TSI148_LCSR_VMCTRL);

	
	result = ioread32be(image->kern_base + offset);

	
	tmp = ioread32be(bridge->base + TSI148_LCSR_VMCTRL);
	tmp &= ~TSI148_LCSR_VMCTRL_RMWEN;
	iowrite32be(tmp, bridge->base + TSI148_LCSR_VMCTRL);

	spin_unlock(&image->lock);

	mutex_unlock(&bridge->vme_rmw);

	return result;
}

static int tsi148_dma_set_vme_src_attributes(struct device *dev, u32 *attr,
	u32 aspace, u32 cycle, u32 dwidth)
{
	
	switch (cycle & (VME_2eSST160 | VME_2eSST267 | VME_2eSST320)) {
	case VME_2eSST160:
		*attr |= TSI148_LCSR_DSAT_2eSSTM_160;
		break;
	case VME_2eSST267:
		*attr |= TSI148_LCSR_DSAT_2eSSTM_267;
		break;
	case VME_2eSST320:
		*attr |= TSI148_LCSR_DSAT_2eSSTM_320;
		break;
	}

	
	if (cycle & VME_SCT)
		*attr |= TSI148_LCSR_DSAT_TM_SCT;

	if (cycle & VME_BLT)
		*attr |= TSI148_LCSR_DSAT_TM_BLT;

	if (cycle & VME_MBLT)
		*attr |= TSI148_LCSR_DSAT_TM_MBLT;

	if (cycle & VME_2eVME)
		*attr |= TSI148_LCSR_DSAT_TM_2eVME;

	if (cycle & VME_2eSST)
		*attr |= TSI148_LCSR_DSAT_TM_2eSST;

	if (cycle & VME_2eSSTB) {
		dev_err(dev, "Currently not setting Broadcast Select "
			"Registers\n");
		*attr |= TSI148_LCSR_DSAT_TM_2eSSTB;
	}

	
	switch (dwidth) {
	case VME_D16:
		*attr |= TSI148_LCSR_DSAT_DBW_16;
		break;
	case VME_D32:
		*attr |= TSI148_LCSR_DSAT_DBW_32;
		break;
	default:
		dev_err(dev, "Invalid data width\n");
		return -EINVAL;
	}

	
	switch (aspace) {
	case VME_A16:
		*attr |= TSI148_LCSR_DSAT_AMODE_A16;
		break;
	case VME_A24:
		*attr |= TSI148_LCSR_DSAT_AMODE_A24;
		break;
	case VME_A32:
		*attr |= TSI148_LCSR_DSAT_AMODE_A32;
		break;
	case VME_A64:
		*attr |= TSI148_LCSR_DSAT_AMODE_A64;
		break;
	case VME_CRCSR:
		*attr |= TSI148_LCSR_DSAT_AMODE_CRCSR;
		break;
	case VME_USER1:
		*attr |= TSI148_LCSR_DSAT_AMODE_USER1;
		break;
	case VME_USER2:
		*attr |= TSI148_LCSR_DSAT_AMODE_USER2;
		break;
	case VME_USER3:
		*attr |= TSI148_LCSR_DSAT_AMODE_USER3;
		break;
	case VME_USER4:
		*attr |= TSI148_LCSR_DSAT_AMODE_USER4;
		break;
	default:
		dev_err(dev, "Invalid address space\n");
		return -EINVAL;
		break;
	}

	if (cycle & VME_SUPER)
		*attr |= TSI148_LCSR_DSAT_SUP;
	if (cycle & VME_PROG)
		*attr |= TSI148_LCSR_DSAT_PGM;

	return 0;
}

static int tsi148_dma_set_vme_dest_attributes(struct device *dev, u32 *attr,
	u32 aspace, u32 cycle, u32 dwidth)
{
	
	switch (cycle & (VME_2eSST160 | VME_2eSST267 | VME_2eSST320)) {
	case VME_2eSST160:
		*attr |= TSI148_LCSR_DDAT_2eSSTM_160;
		break;
	case VME_2eSST267:
		*attr |= TSI148_LCSR_DDAT_2eSSTM_267;
		break;
	case VME_2eSST320:
		*attr |= TSI148_LCSR_DDAT_2eSSTM_320;
		break;
	}

	
	if (cycle & VME_SCT)
		*attr |= TSI148_LCSR_DDAT_TM_SCT;

	if (cycle & VME_BLT)
		*attr |= TSI148_LCSR_DDAT_TM_BLT;

	if (cycle & VME_MBLT)
		*attr |= TSI148_LCSR_DDAT_TM_MBLT;

	if (cycle & VME_2eVME)
		*attr |= TSI148_LCSR_DDAT_TM_2eVME;

	if (cycle & VME_2eSST)
		*attr |= TSI148_LCSR_DDAT_TM_2eSST;

	if (cycle & VME_2eSSTB) {
		dev_err(dev, "Currently not setting Broadcast Select "
			"Registers\n");
		*attr |= TSI148_LCSR_DDAT_TM_2eSSTB;
	}

	
	switch (dwidth) {
	case VME_D16:
		*attr |= TSI148_LCSR_DDAT_DBW_16;
		break;
	case VME_D32:
		*attr |= TSI148_LCSR_DDAT_DBW_32;
		break;
	default:
		dev_err(dev, "Invalid data width\n");
		return -EINVAL;
	}

	
	switch (aspace) {
	case VME_A16:
		*attr |= TSI148_LCSR_DDAT_AMODE_A16;
		break;
	case VME_A24:
		*attr |= TSI148_LCSR_DDAT_AMODE_A24;
		break;
	case VME_A32:
		*attr |= TSI148_LCSR_DDAT_AMODE_A32;
		break;
	case VME_A64:
		*attr |= TSI148_LCSR_DDAT_AMODE_A64;
		break;
	case VME_CRCSR:
		*attr |= TSI148_LCSR_DDAT_AMODE_CRCSR;
		break;
	case VME_USER1:
		*attr |= TSI148_LCSR_DDAT_AMODE_USER1;
		break;
	case VME_USER2:
		*attr |= TSI148_LCSR_DDAT_AMODE_USER2;
		break;
	case VME_USER3:
		*attr |= TSI148_LCSR_DDAT_AMODE_USER3;
		break;
	case VME_USER4:
		*attr |= TSI148_LCSR_DDAT_AMODE_USER4;
		break;
	default:
		dev_err(dev, "Invalid address space\n");
		return -EINVAL;
		break;
	}

	if (cycle & VME_SUPER)
		*attr |= TSI148_LCSR_DDAT_SUP;
	if (cycle & VME_PROG)
		*attr |= TSI148_LCSR_DDAT_PGM;

	return 0;
}

static int tsi148_dma_list_add(struct vme_dma_list *list,
	struct vme_dma_attr *src, struct vme_dma_attr *dest, size_t count)
{
	struct tsi148_dma_entry *entry, *prev;
	u32 address_high, address_low;
	struct vme_dma_pattern *pattern_attr;
	struct vme_dma_pci *pci_attr;
	struct vme_dma_vme *vme_attr;
	dma_addr_t desc_ptr;
	int retval = 0;
	struct vme_bridge *tsi148_bridge;

	tsi148_bridge = list->parent->parent;

	
	entry = kmalloc(sizeof(struct tsi148_dma_entry), GFP_KERNEL);
	if (entry == NULL) {
		dev_err(tsi148_bridge->parent, "Failed to allocate memory for "
			"dma resource structure\n");
		retval = -ENOMEM;
		goto err_mem;
	}

	
	if ((unsigned long)&entry->descriptor & 0x7) {
		dev_err(tsi148_bridge->parent, "Descriptor not aligned to 8 "
			"byte boundary as required: %p\n",
			&entry->descriptor);
		retval = -EINVAL;
		goto err_align;
	}

	memset(&entry->descriptor, 0, sizeof(struct tsi148_dma_descriptor));

	
	switch (src->type) {
	case VME_DMA_PATTERN:
		pattern_attr = src->private;

		entry->descriptor.dsal = pattern_attr->pattern;
		entry->descriptor.dsat = TSI148_LCSR_DSAT_TYP_PAT;
		
		if (pattern_attr->type & VME_DMA_PATTERN_BYTE)
			entry->descriptor.dsat |= TSI148_LCSR_DSAT_PSZ;

		
		if ((pattern_attr->type & VME_DMA_PATTERN_INCREMENT) == 0)
			entry->descriptor.dsat |= TSI148_LCSR_DSAT_NIN;

		break;
	case VME_DMA_PCI:
		pci_attr = src->private;

		reg_split((unsigned long long)pci_attr->address, &address_high,
			&address_low);
		entry->descriptor.dsau = address_high;
		entry->descriptor.dsal = address_low;
		entry->descriptor.dsat = TSI148_LCSR_DSAT_TYP_PCI;
		break;
	case VME_DMA_VME:
		vme_attr = src->private;

		reg_split((unsigned long long)vme_attr->address, &address_high,
			&address_low);
		entry->descriptor.dsau = address_high;
		entry->descriptor.dsal = address_low;
		entry->descriptor.dsat = TSI148_LCSR_DSAT_TYP_VME;

		retval = tsi148_dma_set_vme_src_attributes(
			tsi148_bridge->parent, &entry->descriptor.dsat,
			vme_attr->aspace, vme_attr->cycle, vme_attr->dwidth);
		if (retval < 0)
			goto err_source;
		break;
	default:
		dev_err(tsi148_bridge->parent, "Invalid source type\n");
		retval = -EINVAL;
		goto err_source;
		break;
	}

	/* Assume last link - this will be over-written by adding another */
	entry->descriptor.dnlau = 0;
	entry->descriptor.dnlal = TSI148_LCSR_DNLAL_LLA;


	
	switch (dest->type) {
	case VME_DMA_PCI:
		pci_attr = dest->private;

		reg_split((unsigned long long)pci_attr->address, &address_high,
			&address_low);
		entry->descriptor.ddau = address_high;
		entry->descriptor.ddal = address_low;
		entry->descriptor.ddat = TSI148_LCSR_DDAT_TYP_PCI;
		break;
	case VME_DMA_VME:
		vme_attr = dest->private;

		reg_split((unsigned long long)vme_attr->address, &address_high,
			&address_low);
		entry->descriptor.ddau = address_high;
		entry->descriptor.ddal = address_low;
		entry->descriptor.ddat = TSI148_LCSR_DDAT_TYP_VME;

		retval = tsi148_dma_set_vme_dest_attributes(
			tsi148_bridge->parent, &entry->descriptor.ddat,
			vme_attr->aspace, vme_attr->cycle, vme_attr->dwidth);
		if (retval < 0)
			goto err_dest;
		break;
	default:
		dev_err(tsi148_bridge->parent, "Invalid destination type\n");
		retval = -EINVAL;
		goto err_dest;
		break;
	}

	
	entry->descriptor.dcnt = (u32)count;

	
	list_add_tail(&entry->list, &list->entries);

	
	if (entry->list.prev != &list->entries) {
		prev = list_entry(entry->list.prev, struct tsi148_dma_entry,
			list);
		
		desc_ptr = virt_to_bus(&entry->descriptor);
		reg_split(desc_ptr, &prev->descriptor.dnlau,
			&prev->descriptor.dnlal);
	}

	return 0;

err_dest:
err_source:
err_align:
		kfree(entry);
err_mem:
	return retval;
}

static int tsi148_dma_busy(struct vme_bridge *tsi148_bridge, int channel)
{
	u32 tmp;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	tmp = ioread32be(bridge->base + TSI148_LCSR_DMA[channel] +
		TSI148_LCSR_OFFSET_DSTA);

	if (tmp & TSI148_LCSR_DSTA_BSY)
		return 0;
	else
		return 1;

}

static int tsi148_dma_list_exec(struct vme_dma_list *list)
{
	struct vme_dma_resource *ctrlr;
	int channel, retval = 0;
	struct tsi148_dma_entry *entry;
	dma_addr_t bus_addr;
	u32 bus_addr_high, bus_addr_low;
	u32 val, dctlreg = 0;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	ctrlr = list->parent;

	tsi148_bridge = ctrlr->parent;

	bridge = tsi148_bridge->driver_priv;

	mutex_lock(&ctrlr->mtx);

	channel = ctrlr->number;

	if (!list_empty(&ctrlr->running)) {
		
		mutex_unlock(&ctrlr->mtx);
		return -EBUSY;
	} else {
		list_add(&list->list, &ctrlr->running);
	}

	
	entry = list_first_entry(&list->entries, struct tsi148_dma_entry,
		list);

	bus_addr = virt_to_bus(&entry->descriptor);

	mutex_unlock(&ctrlr->mtx);

	reg_split(bus_addr, &bus_addr_high, &bus_addr_low);

	iowrite32be(bus_addr_high, bridge->base +
		TSI148_LCSR_DMA[channel] + TSI148_LCSR_OFFSET_DNLAU);
	iowrite32be(bus_addr_low, bridge->base +
		TSI148_LCSR_DMA[channel] + TSI148_LCSR_OFFSET_DNLAL);

	
	iowrite32be(dctlreg | TSI148_LCSR_DCTL_DGO, bridge->base +
		TSI148_LCSR_DMA[channel] + TSI148_LCSR_OFFSET_DCTL);

	wait_event_interruptible(bridge->dma_queue[channel],
		tsi148_dma_busy(ctrlr->parent, channel));
	val = ioread32be(bridge->base + TSI148_LCSR_DMA[channel] +
		TSI148_LCSR_OFFSET_DSTA);

	if (val & TSI148_LCSR_DSTA_VBE) {
		dev_err(tsi148_bridge->parent, "DMA Error. DSTA=%08X\n", val);
		retval = -EIO;
	}

	
	mutex_lock(&ctrlr->mtx);
	list_del(&list->list);
	mutex_unlock(&ctrlr->mtx);

	return retval;
}

static int tsi148_dma_list_empty(struct vme_dma_list *list)
{
	struct list_head *pos, *temp;
	struct tsi148_dma_entry *entry;

	
	list_for_each_safe(pos, temp, &list->entries) {
		list_del(pos);
		entry = list_entry(pos, struct tsi148_dma_entry, list);
		kfree(entry);
	}

	return 0;
}

static int tsi148_lm_set(struct vme_lm_resource *lm, unsigned long long lm_base,
	u32 aspace, u32 cycle)
{
	u32 lm_base_high, lm_base_low, lm_ctl = 0;
	int i;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = lm->parent;

	bridge = tsi148_bridge->driver_priv;

	mutex_lock(&lm->mtx);

	
	for (i = 0; i < lm->monitors; i++) {
		if (bridge->lm_callback[i] != NULL) {
			mutex_unlock(&lm->mtx);
			dev_err(tsi148_bridge->parent, "Location monitor "
				"callback attached, can't reset\n");
			return -EBUSY;
		}
	}

	switch (aspace) {
	case VME_A16:
		lm_ctl |= TSI148_LCSR_LMAT_AS_A16;
		break;
	case VME_A24:
		lm_ctl |= TSI148_LCSR_LMAT_AS_A24;
		break;
	case VME_A32:
		lm_ctl |= TSI148_LCSR_LMAT_AS_A32;
		break;
	case VME_A64:
		lm_ctl |= TSI148_LCSR_LMAT_AS_A64;
		break;
	default:
		mutex_unlock(&lm->mtx);
		dev_err(tsi148_bridge->parent, "Invalid address space\n");
		return -EINVAL;
		break;
	}

	if (cycle & VME_SUPER)
		lm_ctl |= TSI148_LCSR_LMAT_SUPR ;
	if (cycle & VME_USER)
		lm_ctl |= TSI148_LCSR_LMAT_NPRIV;
	if (cycle & VME_PROG)
		lm_ctl |= TSI148_LCSR_LMAT_PGM;
	if (cycle & VME_DATA)
		lm_ctl |= TSI148_LCSR_LMAT_DATA;

	reg_split(lm_base, &lm_base_high, &lm_base_low);

	iowrite32be(lm_base_high, bridge->base + TSI148_LCSR_LMBAU);
	iowrite32be(lm_base_low, bridge->base + TSI148_LCSR_LMBAL);
	iowrite32be(lm_ctl, bridge->base + TSI148_LCSR_LMAT);

	mutex_unlock(&lm->mtx);

	return 0;
}

static int tsi148_lm_get(struct vme_lm_resource *lm,
	unsigned long long *lm_base, u32 *aspace, u32 *cycle)
{
	u32 lm_base_high, lm_base_low, lm_ctl, enabled = 0;
	struct tsi148_driver *bridge;

	bridge = lm->parent->driver_priv;

	mutex_lock(&lm->mtx);

	lm_base_high = ioread32be(bridge->base + TSI148_LCSR_LMBAU);
	lm_base_low = ioread32be(bridge->base + TSI148_LCSR_LMBAL);
	lm_ctl = ioread32be(bridge->base + TSI148_LCSR_LMAT);

	reg_join(lm_base_high, lm_base_low, lm_base);

	if (lm_ctl & TSI148_LCSR_LMAT_EN)
		enabled = 1;

	if ((lm_ctl & TSI148_LCSR_LMAT_AS_M) == TSI148_LCSR_LMAT_AS_A16)
		*aspace |= VME_A16;

	if ((lm_ctl & TSI148_LCSR_LMAT_AS_M) == TSI148_LCSR_LMAT_AS_A24)
		*aspace |= VME_A24;

	if ((lm_ctl & TSI148_LCSR_LMAT_AS_M) == TSI148_LCSR_LMAT_AS_A32)
		*aspace |= VME_A32;

	if ((lm_ctl & TSI148_LCSR_LMAT_AS_M) == TSI148_LCSR_LMAT_AS_A64)
		*aspace |= VME_A64;


	if (lm_ctl & TSI148_LCSR_LMAT_SUPR)
		*cycle |= VME_SUPER;
	if (lm_ctl & TSI148_LCSR_LMAT_NPRIV)
		*cycle |= VME_USER;
	if (lm_ctl & TSI148_LCSR_LMAT_PGM)
		*cycle |= VME_PROG;
	if (lm_ctl & TSI148_LCSR_LMAT_DATA)
		*cycle |= VME_DATA;

	mutex_unlock(&lm->mtx);

	return enabled;
}

static int tsi148_lm_attach(struct vme_lm_resource *lm, int monitor,
	void (*callback)(int))
{
	u32 lm_ctl, tmp;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *bridge;

	tsi148_bridge = lm->parent;

	bridge = tsi148_bridge->driver_priv;

	mutex_lock(&lm->mtx);

	
	lm_ctl = ioread32be(bridge->base + TSI148_LCSR_LMAT);
	if ((lm_ctl & (TSI148_LCSR_LMAT_PGM | TSI148_LCSR_LMAT_DATA)) == 0) {
		mutex_unlock(&lm->mtx);
		dev_err(tsi148_bridge->parent, "Location monitor not properly "
			"configured\n");
		return -EINVAL;
	}

	
	if (bridge->lm_callback[monitor] != NULL) {
		mutex_unlock(&lm->mtx);
		dev_err(tsi148_bridge->parent, "Existing callback attached\n");
		return -EBUSY;
	}

	
	bridge->lm_callback[monitor] = callback;

	
	tmp = ioread32be(bridge->base + TSI148_LCSR_INTEN);
	tmp |= TSI148_LCSR_INTEN_LMEN[monitor];
	iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEN);

	tmp = ioread32be(bridge->base + TSI148_LCSR_INTEO);
	tmp |= TSI148_LCSR_INTEO_LMEO[monitor];
	iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEO);

	
	if ((lm_ctl & TSI148_LCSR_LMAT_EN) == 0) {
		lm_ctl |= TSI148_LCSR_LMAT_EN;
		iowrite32be(lm_ctl, bridge->base + TSI148_LCSR_LMAT);
	}

	mutex_unlock(&lm->mtx);

	return 0;
}

static int tsi148_lm_detach(struct vme_lm_resource *lm, int monitor)
{
	u32 lm_en, tmp;
	struct tsi148_driver *bridge;

	bridge = lm->parent->driver_priv;

	mutex_lock(&lm->mtx);

	
	lm_en = ioread32be(bridge->base + TSI148_LCSR_INTEN);
	lm_en &= ~TSI148_LCSR_INTEN_LMEN[monitor];
	iowrite32be(lm_en, bridge->base + TSI148_LCSR_INTEN);

	tmp = ioread32be(bridge->base + TSI148_LCSR_INTEO);
	tmp &= ~TSI148_LCSR_INTEO_LMEO[monitor];
	iowrite32be(tmp, bridge->base + TSI148_LCSR_INTEO);

	iowrite32be(TSI148_LCSR_INTC_LMC[monitor],
		 bridge->base + TSI148_LCSR_INTC);

	
	bridge->lm_callback[monitor] = NULL;

	
	if ((lm_en & (TSI148_LCSR_INTS_LM0S | TSI148_LCSR_INTS_LM1S |
			TSI148_LCSR_INTS_LM2S | TSI148_LCSR_INTS_LM3S)) == 0) {
		tmp = ioread32be(bridge->base + TSI148_LCSR_LMAT);
		tmp &= ~TSI148_LCSR_LMAT_EN;
		iowrite32be(tmp, bridge->base + TSI148_LCSR_LMAT);
	}

	mutex_unlock(&lm->mtx);

	return 0;
}

static int tsi148_slot_get(struct vme_bridge *tsi148_bridge)
{
	u32 slot = 0;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	if (!geoid) {
		slot = ioread32be(bridge->base + TSI148_LCSR_VSTAT);
		slot = slot & TSI148_LCSR_VSTAT_GA_M;
	} else
		slot = geoid;

	return (int)slot;
}

void *tsi148_alloc_consistent(struct device *parent, size_t size,
	dma_addr_t *dma)
{
	struct pci_dev *pdev;

	
	pdev = container_of(parent, struct pci_dev, dev);

	return pci_alloc_consistent(pdev, size, dma);
}

void tsi148_free_consistent(struct device *parent, size_t size, void *vaddr,
	dma_addr_t dma)
{
	struct pci_dev *pdev;

	
	pdev = container_of(parent, struct pci_dev, dev);

	pci_free_consistent(pdev, size, vaddr, dma);
}

static int __init tsi148_init(void)
{
	return pci_register_driver(&tsi148_driver);
}

static int tsi148_crcsr_init(struct vme_bridge *tsi148_bridge,
	struct pci_dev *pdev)
{
	u32 cbar, crat, vstat;
	u32 crcsr_bus_high, crcsr_bus_low;
	int retval;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	
	bridge->crcsr_kernel = pci_alloc_consistent(pdev, VME_CRCSR_BUF_SIZE,
		&bridge->crcsr_bus);
	if (bridge->crcsr_kernel == NULL) {
		dev_err(tsi148_bridge->parent, "Failed to allocate memory for "
			"CR/CSR image\n");
		return -ENOMEM;
	}

	memset(bridge->crcsr_kernel, 0, VME_CRCSR_BUF_SIZE);

	reg_split(bridge->crcsr_bus, &crcsr_bus_high, &crcsr_bus_low);

	iowrite32be(crcsr_bus_high, bridge->base + TSI148_LCSR_CROU);
	iowrite32be(crcsr_bus_low, bridge->base + TSI148_LCSR_CROL);

	
	cbar = ioread32be(bridge->base + TSI148_CBAR);
	cbar = (cbar & TSI148_CRCSR_CBAR_M)>>3;

	vstat = tsi148_slot_get(tsi148_bridge);

	if (cbar != vstat) {
		cbar = vstat;
		dev_info(tsi148_bridge->parent, "Setting CR/CSR offset\n");
		iowrite32be(cbar<<3, bridge->base + TSI148_CBAR);
	}
	dev_info(tsi148_bridge->parent, "CR/CSR Offset: %d\n", cbar);

	crat = ioread32be(bridge->base + TSI148_LCSR_CRAT);
	if (crat & TSI148_LCSR_CRAT_EN) {
		dev_info(tsi148_bridge->parent, "Enabling CR/CSR space\n");
		iowrite32be(crat | TSI148_LCSR_CRAT_EN,
			bridge->base + TSI148_LCSR_CRAT);
	} else
		dev_info(tsi148_bridge->parent, "CR/CSR already enabled\n");

	if (err_chk) {
		retval = tsi148_master_set(bridge->flush_image, 1,
			(vstat * 0x80000), 0x80000, VME_CRCSR, VME_SCT,
			VME_D16);
		if (retval)
			dev_err(tsi148_bridge->parent, "Configuring flush image"
				" failed\n");
	}

	return 0;

}

static void tsi148_crcsr_exit(struct vme_bridge *tsi148_bridge,
	struct pci_dev *pdev)
{
	u32 crat;
	struct tsi148_driver *bridge;

	bridge = tsi148_bridge->driver_priv;

	
	crat = ioread32be(bridge->base + TSI148_LCSR_CRAT);
	iowrite32be(crat & ~TSI148_LCSR_CRAT_EN,
		bridge->base + TSI148_LCSR_CRAT);

	
	iowrite32be(0, bridge->base + TSI148_LCSR_CROU);
	iowrite32be(0, bridge->base + TSI148_LCSR_CROL);

	pci_free_consistent(pdev, VME_CRCSR_BUF_SIZE, bridge->crcsr_kernel,
		bridge->crcsr_bus);
}

static int tsi148_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int retval, i, master_num;
	u32 data;
	struct list_head *pos = NULL;
	struct vme_bridge *tsi148_bridge;
	struct tsi148_driver *tsi148_device;
	struct vme_master_resource *master_image;
	struct vme_slave_resource *slave_image;
	struct vme_dma_resource *dma_ctrlr;
	struct vme_lm_resource *lm;

	tsi148_bridge = kzalloc(sizeof(struct vme_bridge), GFP_KERNEL);
	if (tsi148_bridge == NULL) {
		dev_err(&pdev->dev, "Failed to allocate memory for device "
			"structure\n");
		retval = -ENOMEM;
		goto err_struct;
	}

	tsi148_device = kzalloc(sizeof(struct tsi148_driver), GFP_KERNEL);
	if (tsi148_device == NULL) {
		dev_err(&pdev->dev, "Failed to allocate memory for device "
			"structure\n");
		retval = -ENOMEM;
		goto err_driver;
	}

	tsi148_bridge->driver_priv = tsi148_device;

	
	retval = pci_enable_device(pdev);
	if (retval) {
		dev_err(&pdev->dev, "Unable to enable device\n");
		goto err_enable;
	}

	
	retval = pci_request_regions(pdev, driver_name);
	if (retval) {
		dev_err(&pdev->dev, "Unable to reserve resources\n");
		goto err_resource;
	}

	
	tsi148_device->base = ioremap_nocache(pci_resource_start(pdev, 0),
		4096);
	if (!tsi148_device->base) {
		dev_err(&pdev->dev, "Unable to remap CRG region\n");
		retval = -EIO;
		goto err_remap;
	}

	
	data = ioread32(tsi148_device->base + TSI148_PCFS_ID) & 0x0000FFFF;
	if (data != PCI_VENDOR_ID_TUNDRA) {
		dev_err(&pdev->dev, "CRG region check failed\n");
		retval = -EIO;
		goto err_test;
	}

	
	init_waitqueue_head(&tsi148_device->dma_queue[0]);
	init_waitqueue_head(&tsi148_device->dma_queue[1]);
	init_waitqueue_head(&tsi148_device->iack_queue);
	mutex_init(&tsi148_device->vme_int);
	mutex_init(&tsi148_device->vme_rmw);

	tsi148_bridge->parent = &pdev->dev;
	strcpy(tsi148_bridge->name, driver_name);

	
	retval = tsi148_irq_init(tsi148_bridge);
	if (retval != 0) {
		dev_err(&pdev->dev, "Chip Initialization failed.\n");
		goto err_irq;
	}

	master_num = TSI148_MAX_MASTER;
	if (err_chk) {
		master_num--;

		tsi148_device->flush_image =
			kmalloc(sizeof(struct vme_master_resource), GFP_KERNEL);
		if (tsi148_device->flush_image == NULL) {
			dev_err(&pdev->dev, "Failed to allocate memory for "
			"flush resource structure\n");
			retval = -ENOMEM;
			goto err_master;
		}
		tsi148_device->flush_image->parent = tsi148_bridge;
		spin_lock_init(&tsi148_device->flush_image->lock);
		tsi148_device->flush_image->locked = 1;
		tsi148_device->flush_image->number = master_num;
		tsi148_device->flush_image->address_attr = VME_A16 | VME_A24 |
			VME_A32 | VME_A64;
		tsi148_device->flush_image->cycle_attr = VME_SCT | VME_BLT |
			VME_MBLT | VME_2eVME | VME_2eSST | VME_2eSSTB |
			VME_2eSST160 | VME_2eSST267 | VME_2eSST320 | VME_SUPER |
			VME_USER | VME_PROG | VME_DATA;
		tsi148_device->flush_image->width_attr = VME_D16 | VME_D32;
		memset(&tsi148_device->flush_image->bus_resource, 0,
			sizeof(struct resource));
		tsi148_device->flush_image->kern_base  = NULL;
	}

	
	INIT_LIST_HEAD(&tsi148_bridge->master_resources);
	for (i = 0; i < master_num; i++) {
		master_image = kmalloc(sizeof(struct vme_master_resource),
			GFP_KERNEL);
		if (master_image == NULL) {
			dev_err(&pdev->dev, "Failed to allocate memory for "
			"master resource structure\n");
			retval = -ENOMEM;
			goto err_master;
		}
		master_image->parent = tsi148_bridge;
		spin_lock_init(&master_image->lock);
		master_image->locked = 0;
		master_image->number = i;
		master_image->address_attr = VME_A16 | VME_A24 | VME_A32 |
			VME_A64;
		master_image->cycle_attr = VME_SCT | VME_BLT | VME_MBLT |
			VME_2eVME | VME_2eSST | VME_2eSSTB | VME_2eSST160 |
			VME_2eSST267 | VME_2eSST320 | VME_SUPER | VME_USER |
			VME_PROG | VME_DATA;
		master_image->width_attr = VME_D16 | VME_D32;
		memset(&master_image->bus_resource, 0,
			sizeof(struct resource));
		master_image->kern_base  = NULL;
		list_add_tail(&master_image->list,
			&tsi148_bridge->master_resources);
	}

	
	INIT_LIST_HEAD(&tsi148_bridge->slave_resources);
	for (i = 0; i < TSI148_MAX_SLAVE; i++) {
		slave_image = kmalloc(sizeof(struct vme_slave_resource),
			GFP_KERNEL);
		if (slave_image == NULL) {
			dev_err(&pdev->dev, "Failed to allocate memory for "
			"slave resource structure\n");
			retval = -ENOMEM;
			goto err_slave;
		}
		slave_image->parent = tsi148_bridge;
		mutex_init(&slave_image->mtx);
		slave_image->locked = 0;
		slave_image->number = i;
		slave_image->address_attr = VME_A16 | VME_A24 | VME_A32 |
			VME_A64 | VME_CRCSR | VME_USER1 | VME_USER2 |
			VME_USER3 | VME_USER4;
		slave_image->cycle_attr = VME_SCT | VME_BLT | VME_MBLT |
			VME_2eVME | VME_2eSST | VME_2eSSTB | VME_2eSST160 |
			VME_2eSST267 | VME_2eSST320 | VME_SUPER | VME_USER |
			VME_PROG | VME_DATA;
		list_add_tail(&slave_image->list,
			&tsi148_bridge->slave_resources);
	}

	
	INIT_LIST_HEAD(&tsi148_bridge->dma_resources);
	for (i = 0; i < TSI148_MAX_DMA; i++) {
		dma_ctrlr = kmalloc(sizeof(struct vme_dma_resource),
			GFP_KERNEL);
		if (dma_ctrlr == NULL) {
			dev_err(&pdev->dev, "Failed to allocate memory for "
			"dma resource structure\n");
			retval = -ENOMEM;
			goto err_dma;
		}
		dma_ctrlr->parent = tsi148_bridge;
		mutex_init(&dma_ctrlr->mtx);
		dma_ctrlr->locked = 0;
		dma_ctrlr->number = i;
		dma_ctrlr->route_attr = VME_DMA_VME_TO_MEM |
			VME_DMA_MEM_TO_VME | VME_DMA_VME_TO_VME |
			VME_DMA_MEM_TO_MEM | VME_DMA_PATTERN_TO_VME |
			VME_DMA_PATTERN_TO_MEM;
		INIT_LIST_HEAD(&dma_ctrlr->pending);
		INIT_LIST_HEAD(&dma_ctrlr->running);
		list_add_tail(&dma_ctrlr->list,
			&tsi148_bridge->dma_resources);
	}

	
	INIT_LIST_HEAD(&tsi148_bridge->lm_resources);
	lm = kmalloc(sizeof(struct vme_lm_resource), GFP_KERNEL);
	if (lm == NULL) {
		dev_err(&pdev->dev, "Failed to allocate memory for "
		"location monitor resource structure\n");
		retval = -ENOMEM;
		goto err_lm;
	}
	lm->parent = tsi148_bridge;
	mutex_init(&lm->mtx);
	lm->locked = 0;
	lm->number = 1;
	lm->monitors = 4;
	list_add_tail(&lm->list, &tsi148_bridge->lm_resources);

	tsi148_bridge->slave_get = tsi148_slave_get;
	tsi148_bridge->slave_set = tsi148_slave_set;
	tsi148_bridge->master_get = tsi148_master_get;
	tsi148_bridge->master_set = tsi148_master_set;
	tsi148_bridge->master_read = tsi148_master_read;
	tsi148_bridge->master_write = tsi148_master_write;
	tsi148_bridge->master_rmw = tsi148_master_rmw;
	tsi148_bridge->dma_list_add = tsi148_dma_list_add;
	tsi148_bridge->dma_list_exec = tsi148_dma_list_exec;
	tsi148_bridge->dma_list_empty = tsi148_dma_list_empty;
	tsi148_bridge->irq_set = tsi148_irq_set;
	tsi148_bridge->irq_generate = tsi148_irq_generate;
	tsi148_bridge->lm_set = tsi148_lm_set;
	tsi148_bridge->lm_get = tsi148_lm_get;
	tsi148_bridge->lm_attach = tsi148_lm_attach;
	tsi148_bridge->lm_detach = tsi148_lm_detach;
	tsi148_bridge->slot_get = tsi148_slot_get;
	tsi148_bridge->alloc_consistent = tsi148_alloc_consistent;
	tsi148_bridge->free_consistent = tsi148_free_consistent;

	data = ioread32be(tsi148_device->base + TSI148_LCSR_VSTAT);
	dev_info(&pdev->dev, "Board is%s the VME system controller\n",
		(data & TSI148_LCSR_VSTAT_SCONS) ? "" : " not");
	if (!geoid)
		dev_info(&pdev->dev, "VME geographical address is %d\n",
			data & TSI148_LCSR_VSTAT_GA_M);
	else
		dev_info(&pdev->dev, "VME geographical address is set to %d\n",
			geoid);

	dev_info(&pdev->dev, "VME Write and flush and error check is %s\n",
		err_chk ? "enabled" : "disabled");

	if (tsi148_crcsr_init(tsi148_bridge, pdev)) {
		dev_err(&pdev->dev, "CR/CSR configuration failed.\n");
		goto err_crcsr;
	}

	retval = vme_register_bridge(tsi148_bridge);
	if (retval != 0) {
		dev_err(&pdev->dev, "Chip Registration failed.\n");
		goto err_reg;
	}

	pci_set_drvdata(pdev, tsi148_bridge);

	
	data = ioread32be(tsi148_device->base + TSI148_LCSR_VSTAT);
	data &= ~TSI148_LCSR_VSTAT_BRDFL;
	data |= TSI148_LCSR_VSTAT_CPURST;
	iowrite32be(data, tsi148_device->base + TSI148_LCSR_VSTAT);

	return 0;

err_reg:
	tsi148_crcsr_exit(tsi148_bridge, pdev);
err_crcsr:
err_lm:
	
	list_for_each(pos, &tsi148_bridge->lm_resources) {
		lm = list_entry(pos, struct vme_lm_resource, list);
		list_del(pos);
		kfree(lm);
	}
err_dma:
	
	list_for_each(pos, &tsi148_bridge->dma_resources) {
		dma_ctrlr = list_entry(pos, struct vme_dma_resource, list);
		list_del(pos);
		kfree(dma_ctrlr);
	}
err_slave:
	
	list_for_each(pos, &tsi148_bridge->slave_resources) {
		slave_image = list_entry(pos, struct vme_slave_resource, list);
		list_del(pos);
		kfree(slave_image);
	}
err_master:
	
	list_for_each(pos, &tsi148_bridge->master_resources) {
		master_image = list_entry(pos, struct vme_master_resource,
			list);
		list_del(pos);
		kfree(master_image);
	}

	tsi148_irq_exit(tsi148_bridge, pdev);
err_irq:
err_test:
	iounmap(tsi148_device->base);
err_remap:
	pci_release_regions(pdev);
err_resource:
	pci_disable_device(pdev);
err_enable:
	kfree(tsi148_device);
err_driver:
	kfree(tsi148_bridge);
err_struct:
	return retval;

}

static void tsi148_remove(struct pci_dev *pdev)
{
	struct list_head *pos = NULL;
	struct list_head *tmplist;
	struct vme_master_resource *master_image;
	struct vme_slave_resource *slave_image;
	struct vme_dma_resource *dma_ctrlr;
	int i;
	struct tsi148_driver *bridge;
	struct vme_bridge *tsi148_bridge = pci_get_drvdata(pdev);

	bridge = tsi148_bridge->driver_priv;


	dev_dbg(&pdev->dev, "Driver is being unloaded.\n");

	for (i = 0; i < 8; i++) {
		iowrite32be(0, bridge->base + TSI148_LCSR_IT[i] +
			TSI148_LCSR_OFFSET_ITAT);
		iowrite32be(0, bridge->base + TSI148_LCSR_OT[i] +
			TSI148_LCSR_OFFSET_OTAT);
	}

	iowrite32be(0, bridge->base + TSI148_LCSR_LMAT);

	iowrite32be(0, bridge->base + TSI148_LCSR_CSRAT);

	iowrite32be(0xFFFFFFFF, bridge->base + TSI148_LCSR_EDPAT);
	iowrite32be(0xFFFFFFFF, bridge->base + TSI148_LCSR_VEAT);
	iowrite32be(0x07000700, bridge->base + TSI148_LCSR_PSTAT);

	if (ioread32be(bridge->base + TSI148_LCSR_VICR) & 0x800)
		iowrite32be(0x8000, bridge->base + TSI148_LCSR_VICR);

	iowrite32be(0x0, bridge->base + TSI148_LCSR_INTM1);
	iowrite32be(0x0, bridge->base + TSI148_LCSR_INTM2);

	tsi148_irq_exit(tsi148_bridge, pdev);

	vme_unregister_bridge(tsi148_bridge);

	tsi148_crcsr_exit(tsi148_bridge, pdev);

	
	list_for_each_safe(pos, tmplist, &tsi148_bridge->dma_resources) {
		dma_ctrlr = list_entry(pos, struct vme_dma_resource, list);
		list_del(pos);
		kfree(dma_ctrlr);
	}

	
	list_for_each_safe(pos, tmplist, &tsi148_bridge->slave_resources) {
		slave_image = list_entry(pos, struct vme_slave_resource, list);
		list_del(pos);
		kfree(slave_image);
	}

	
	list_for_each_safe(pos, tmplist, &tsi148_bridge->master_resources) {
		master_image = list_entry(pos, struct vme_master_resource,
			list);
		list_del(pos);
		kfree(master_image);
	}

	iounmap(bridge->base);

	pci_release_regions(pdev);

	pci_disable_device(pdev);

	kfree(tsi148_bridge->driver_priv);

	kfree(tsi148_bridge);
}

static void __exit tsi148_exit(void)
{
	pci_unregister_driver(&tsi148_driver);
}

MODULE_PARM_DESC(err_chk, "Check for VME errors on reads and writes");
module_param(err_chk, bool, 0);

MODULE_PARM_DESC(geoid, "Override geographical addressing");
module_param(geoid, int, 0);

MODULE_DESCRIPTION("VME driver for the Tundra Tempe VME bridge");
MODULE_LICENSE("GPL");

module_init(tsi148_init);
module_exit(tsi148_exit);
