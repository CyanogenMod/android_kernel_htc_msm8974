/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2010 Adaptec, Inc.
 *               2010 PMC-Sierra, Inc. (aacraid@pmc-sierra.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *  sa.c
 *
 * Abstract: Drawbridge specific support functions
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/time.h>
#include <linux/interrupt.h>

#include <scsi/scsi_host.h>

#include "aacraid.h"

static irqreturn_t aac_sa_intr(int irq, void *dev_id)
{
	struct aac_dev *dev = dev_id;
	unsigned short intstat, mask;

	intstat = sa_readw(dev, DoorbellReg_p);
	mask = ~(sa_readw(dev, SaDbCSR.PRISETIRQMASK));

	

	if (intstat & mask) {
		if (intstat & PrintfReady) {
			aac_printf(dev, sa_readl(dev, Mailbox5));
			sa_writew(dev, DoorbellClrReg_p, PrintfReady); 
			sa_writew(dev, DoorbellReg_s, PrintfDone);
		} else if (intstat & DOORBELL_1) {	
			sa_writew(dev, DoorbellClrReg_p, DOORBELL_1);
			aac_command_normal(&dev->queues->queue[HostNormCmdQueue]);
		} else if (intstat & DOORBELL_2) {	
			sa_writew(dev, DoorbellClrReg_p, DOORBELL_2);
			aac_response_normal(&dev->queues->queue[HostNormRespQueue]);
		} else if (intstat & DOORBELL_3) {	
			sa_writew(dev, DoorbellClrReg_p, DOORBELL_3);
		} else if (intstat & DOORBELL_4) {	
			sa_writew(dev, DoorbellClrReg_p, DOORBELL_4);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}


static void aac_sa_disable_interrupt (struct aac_dev *dev)
{
	sa_writew(dev, SaDbCSR.PRISETIRQMASK, 0xffff);
}


static void aac_sa_enable_interrupt (struct aac_dev *dev)
{
	sa_writew(dev, SaDbCSR.PRICLEARIRQMASK, (PrintfReady | DOORBELL_1 |
				DOORBELL_2 | DOORBELL_3 | DOORBELL_4));
}

 
static void aac_sa_notify_adapter(struct aac_dev *dev, u32 event)
{
	switch (event) {

	case AdapNormCmdQue:
		sa_writew(dev, DoorbellReg_s,DOORBELL_1);
		break;
	case HostNormRespNotFull:
		sa_writew(dev, DoorbellReg_s,DOORBELL_4);
		break;
	case AdapNormRespQue:
		sa_writew(dev, DoorbellReg_s,DOORBELL_2);
		break;
	case HostNormCmdNotFull:
		sa_writew(dev, DoorbellReg_s,DOORBELL_3);
		break;
	case HostShutdown:
		break;
	case FastIo:
		sa_writew(dev, DoorbellReg_s,DOORBELL_6);
		break;
	case AdapPrintfDone:
		sa_writew(dev, DoorbellReg_s,DOORBELL_5);
		break;
	default:
		BUG();
		break;
	}
}



static int sa_sync_cmd(struct aac_dev *dev, u32 command, 
		u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6,
		u32 *ret, u32 *r1, u32 *r2, u32 *r3, u32 *r4)
{
	unsigned long start;
 	int ok;
	sa_writel(dev, Mailbox0, command);
	sa_writel(dev, Mailbox1, p1);
	sa_writel(dev, Mailbox2, p2);
	sa_writel(dev, Mailbox3, p3);
	sa_writel(dev, Mailbox4, p4);

	sa_writew(dev, DoorbellClrReg_p, DOORBELL_0);
	sa_writew(dev, DoorbellReg_s, DOORBELL_0);

	ok = 0;
	start = jiffies;

	while(time_before(jiffies, start+30*HZ))
	{
		udelay(5);
		if(sa_readw(dev, DoorbellReg_p) & DOORBELL_0)  {
			ok = 1;
			break;
		}
		msleep(1);
	}

	if (ok != 1)
		return -ETIMEDOUT;
	sa_writew(dev, DoorbellClrReg_p, DOORBELL_0);
	if (ret)
		*ret = sa_readl(dev, Mailbox0);
	if (r1)
		*r1 = sa_readl(dev, Mailbox1);
	if (r2)
		*r2 = sa_readl(dev, Mailbox2);
	if (r3)
		*r3 = sa_readl(dev, Mailbox3);
	if (r4)
		*r4 = sa_readl(dev, Mailbox4);
	return 0;
}

 
static void aac_sa_interrupt_adapter (struct aac_dev *dev)
{
	sa_sync_cmd(dev, BREAKPOINT_REQUEST, 0, 0, 0, 0, 0, 0,
			NULL, NULL, NULL, NULL, NULL);
}


static void aac_sa_start_adapter(struct aac_dev *dev)
{
	struct aac_init *init;
	init = dev->init;
	init->HostElapsedSeconds = cpu_to_le32(get_seconds());
	
	sa_sync_cmd(dev, INIT_STRUCT_BASE_ADDRESS, 
			(u32)(ulong)dev->init_pa, 0, 0, 0, 0, 0,
			NULL, NULL, NULL, NULL, NULL);
}

static int aac_sa_restart_adapter(struct aac_dev *dev, int bled)
{
	return -EINVAL;
}

static int aac_sa_check_health(struct aac_dev *dev)
{
	long status = sa_readl(dev, Mailbox7);

	if (status & SELF_TEST_FAILED)
		return -1;
	if (status & KERNEL_PANIC)
		return -2;
	if (!(status & KERNEL_UP_AND_RUNNING))
		return -3;
	return 0;
}

static int aac_sa_ioremap(struct aac_dev * dev, u32 size)
{
	if (!size) {
		iounmap(dev->regs.sa);
		return 0;
	}
	dev->base = dev->regs.sa = ioremap(dev->scsi_host_ptr->base, size);
	return (dev->base == NULL) ? -1 : 0;
}


int aac_sa_init(struct aac_dev *dev)
{
	unsigned long start;
	unsigned long status;
	int instance;
	const char *name;

	instance = dev->id;
	name     = dev->name;

	if (aac_sa_ioremap(dev, dev->base_size)) {
		printk(KERN_WARNING "%s: unable to map adapter.\n", name);
		goto error_iounmap;
	}

	if (sa_readl(dev, Mailbox7) & SELF_TEST_FAILED) {
		printk(KERN_WARNING "%s%d: adapter self-test failed.\n", name, instance);
		goto error_iounmap;
	}
	if (sa_readl(dev, Mailbox7) & KERNEL_PANIC) {
		printk(KERN_WARNING "%s%d: adapter kernel panic'd.\n", name, instance);
		goto error_iounmap;
	}
	start = jiffies;
	while (!(sa_readl(dev, Mailbox7) & KERNEL_UP_AND_RUNNING)) {
		if (time_after(jiffies, start+startup_timeout*HZ)) {
			status = sa_readl(dev, Mailbox7);
			printk(KERN_WARNING "%s%d: adapter kernel failed to start, init status = %lx.\n", 
					name, instance, status);
			goto error_iounmap;
		}
		msleep(1);
	}


	dev->a_ops.adapter_interrupt = aac_sa_interrupt_adapter;
	dev->a_ops.adapter_disable_int = aac_sa_disable_interrupt;
	dev->a_ops.adapter_enable_int = aac_sa_enable_interrupt;
	dev->a_ops.adapter_notify = aac_sa_notify_adapter;
	dev->a_ops.adapter_sync_cmd = sa_sync_cmd;
	dev->a_ops.adapter_check_health = aac_sa_check_health;
	dev->a_ops.adapter_restart = aac_sa_restart_adapter;
	dev->a_ops.adapter_intr = aac_sa_intr;
	dev->a_ops.adapter_deliver = aac_rx_deliver_producer;
	dev->a_ops.adapter_ioremap = aac_sa_ioremap;

	aac_adapter_disable_int(dev);
	aac_adapter_enable_int(dev);

	if(aac_init_adapter(dev) == NULL)
		goto error_irq;
	dev->sync_mode = 0;	
	if (request_irq(dev->pdev->irq, dev->a_ops.adapter_intr,
			IRQF_SHARED|IRQF_DISABLED,
			"aacraid", (void *)dev ) < 0) {
		printk(KERN_WARNING "%s%d: Interrupt unavailable.\n",
			name, instance);
		goto error_iounmap;
	}
	dev->dbg_base = dev->scsi_host_ptr->base;
	dev->dbg_base_mapped = dev->base;
	dev->dbg_size = dev->base_size;

	aac_adapter_enable_int(dev);

	aac_sa_start_adapter(dev);
	return 0;

error_irq:
	aac_sa_disable_interrupt(dev);
	free_irq(dev->pdev->irq, (void *)dev);

error_iounmap:

	return -1;
}

