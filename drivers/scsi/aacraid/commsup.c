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
 *  commsup.c
 *
 * Abstract: Contain all routines that are required for FSA host/adapter
 *    communication.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>

#include "aacraid.h"


static int fib_map_alloc(struct aac_dev *dev)
{
	dprintk((KERN_INFO
	  "allocate hardware fibs pci_alloc_consistent(%p, %d * (%d + %d), %p)\n",
	  dev->pdev, dev->max_fib_size, dev->scsi_host_ptr->can_queue,
	  AAC_NUM_MGT_FIB, &dev->hw_fib_pa));
	dev->hw_fib_va = pci_alloc_consistent(dev->pdev,
		(dev->max_fib_size + sizeof(struct aac_fib_xporthdr))
		* (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) + (ALIGN32 - 1),
		&dev->hw_fib_pa);
	if (dev->hw_fib_va == NULL)
		return -ENOMEM;
	return 0;
}


void aac_fib_map_free(struct aac_dev *dev)
{
	pci_free_consistent(dev->pdev,
	  dev->max_fib_size * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB),
	  dev->hw_fib_va, dev->hw_fib_pa);
	dev->hw_fib_va = NULL;
	dev->hw_fib_pa = 0;
}


int aac_fib_setup(struct aac_dev * dev)
{
	struct fib *fibptr;
	struct hw_fib *hw_fib;
	dma_addr_t hw_fib_pa;
	int i;

	while (((i = fib_map_alloc(dev)) == -ENOMEM)
	 && (dev->scsi_host_ptr->can_queue > (64 - AAC_NUM_MGT_FIB))) {
		dev->init->MaxIoCommands = cpu_to_le32((dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) >> 1);
		dev->scsi_host_ptr->can_queue = le32_to_cpu(dev->init->MaxIoCommands) - AAC_NUM_MGT_FIB;
	}
	if (i<0)
		return -ENOMEM;

	
	hw_fib_pa = (dev->hw_fib_pa + (ALIGN32 - 1)) & ~(ALIGN32 - 1);
	dev->hw_fib_va = (struct hw_fib *)((unsigned char *)dev->hw_fib_va +
		(hw_fib_pa - dev->hw_fib_pa));
	dev->hw_fib_pa = hw_fib_pa;
	memset(dev->hw_fib_va, 0,
		(dev->max_fib_size + sizeof(struct aac_fib_xporthdr)) *
		(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB));

	
	dev->hw_fib_va = (struct hw_fib *)((unsigned char *)dev->hw_fib_va +
		sizeof(struct aac_fib_xporthdr));
	dev->hw_fib_pa += sizeof(struct aac_fib_xporthdr);

	hw_fib = dev->hw_fib_va;
	hw_fib_pa = dev->hw_fib_pa;
	for (i = 0, fibptr = &dev->fibs[i];
		i < (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
		i++, fibptr++)
	{
		fibptr->dev = dev;
		fibptr->hw_fib_va = hw_fib;
		fibptr->data = (void *) fibptr->hw_fib_va->data;
		fibptr->next = fibptr+1;	
		sema_init(&fibptr->event_wait, 0);
		spin_lock_init(&fibptr->event_lock);
		hw_fib->header.XferState = cpu_to_le32(0xffffffff);
		hw_fib->header.SenderSize = cpu_to_le16(dev->max_fib_size);
		fibptr->hw_fib_pa = hw_fib_pa;
		hw_fib = (struct hw_fib *)((unsigned char *)hw_fib +
			dev->max_fib_size + sizeof(struct aac_fib_xporthdr));
		hw_fib_pa = hw_fib_pa +
			dev->max_fib_size + sizeof(struct aac_fib_xporthdr);
	}
	dev->fibs[dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB - 1].next = NULL;
	dev->free_fib = &dev->fibs[0];
	return 0;
}


struct fib *aac_fib_alloc(struct aac_dev *dev)
{
	struct fib * fibptr;
	unsigned long flags;
	spin_lock_irqsave(&dev->fib_lock, flags);
	fibptr = dev->free_fib;
	if(!fibptr){
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		return fibptr;
	}
	dev->free_fib = fibptr->next;
	spin_unlock_irqrestore(&dev->fib_lock, flags);
	fibptr->type = FSAFS_NTC_FIB_CONTEXT;
	fibptr->size = sizeof(struct fib);
	fibptr->hw_fib_va->header.XferState = 0;
	fibptr->flags = 0;
	fibptr->callback = NULL;
	fibptr->callback_data = NULL;

	return fibptr;
}


void aac_fib_free(struct fib *fibptr)
{
	unsigned long flags, flagsv;

	spin_lock_irqsave(&fibptr->event_lock, flagsv);
	if (fibptr->done == 2) {
		spin_unlock_irqrestore(&fibptr->event_lock, flagsv);
		return;
	}
	spin_unlock_irqrestore(&fibptr->event_lock, flagsv);

	spin_lock_irqsave(&fibptr->dev->fib_lock, flags);
	if (unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
		aac_config.fib_timeouts++;
	if (fibptr->hw_fib_va->header.XferState != 0) {
		printk(KERN_WARNING "aac_fib_free, XferState != 0, fibptr = 0x%p, XferState = 0x%x\n",
			 (void*)fibptr,
			 le32_to_cpu(fibptr->hw_fib_va->header.XferState));
	}
	fibptr->next = fibptr->dev->free_fib;
	fibptr->dev->free_fib = fibptr;
	spin_unlock_irqrestore(&fibptr->dev->fib_lock, flags);
}


void aac_fib_init(struct fib *fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;

	hw_fib->header.StructType = FIB_MAGIC;
	hw_fib->header.Size = cpu_to_le16(fibptr->dev->max_fib_size);
	hw_fib->header.XferState = cpu_to_le32(HostOwned | FibInitialized | FibEmpty | FastResponseCapable);
	hw_fib->header.SenderFibAddress = 0; 
	hw_fib->header.ReceiverFibAddress = cpu_to_le32(fibptr->hw_fib_pa);
	hw_fib->header.SenderSize = cpu_to_le16(fibptr->dev->max_fib_size);
}


static void fib_dealloc(struct fib * fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;
	BUG_ON(hw_fib->header.StructType != FIB_MAGIC);
	hw_fib->header.XferState = 0;
}



static int aac_get_entry (struct aac_dev * dev, u32 qid, struct aac_entry **entry, u32 * index, unsigned long *nonotify)
{
	struct aac_queue * q;
	unsigned long idx;


	q = &dev->queues->queue[qid];

	idx = *index = le32_to_cpu(*(q->headers.producer));
	
	if (idx != le32_to_cpu(*(q->headers.consumer))) {
		if (--idx == 0) {
			if (qid == AdapNormCmdQueue)
				idx = ADAP_NORM_CMD_ENTRIES;
			else
				idx = ADAP_NORM_RESP_ENTRIES;
		}
		if (idx != le32_to_cpu(*(q->headers.consumer)))
			*nonotify = 1;
	}

	if (qid == AdapNormCmdQueue) {
		if (*index >= ADAP_NORM_CMD_ENTRIES)
			*index = 0; 
	} else {
		if (*index >= ADAP_NORM_RESP_ENTRIES)
			*index = 0; 
	}

	
	if ((*index + 1) == le32_to_cpu(*(q->headers.consumer))) {
		printk(KERN_WARNING "Queue %d full, %u outstanding.\n",
				qid, q->numpending);
		return 0;
	} else {
		*entry = q->base + *index;
		return 1;
	}
}


int aac_queue_get(struct aac_dev * dev, u32 * index, u32 qid, struct hw_fib * hw_fib, int wait, struct fib * fibptr, unsigned long *nonotify)
{
	struct aac_entry * entry = NULL;
	int map = 0;

	if (qid == AdapNormCmdQueue) {
		
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			printk(KERN_ERR "GetEntries failed\n");
		}
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		map = 1;
	} else {
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			
		}
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		entry->addr = hw_fib->header.SenderFibAddress;
			
		hw_fib->header.ReceiverFibAddress = hw_fib->header.SenderFibAddress;	
		map = 0;
	}
	if (map)
		entry->addr = cpu_to_le32(fibptr->hw_fib_pa);
	return 0;
}



int aac_fib_send(u16 command, struct fib *fibptr, unsigned long size,
		int priority, int wait, int reply, fib_callback callback,
		void *callback_data)
{
	struct aac_dev * dev = fibptr->dev;
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	unsigned long flags = 0;
	unsigned long qflags;
	unsigned long mflags = 0;
	unsigned long sflags = 0;


	if (!(hw_fib->header.XferState & cpu_to_le32(HostOwned)))
		return -EBUSY;
	fibptr->flags = 0;
	if (wait && !reply) {
		return -EINVAL;
	} else if (!wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(Async | ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.AsyncSent);
	} else if (!wait && !reply) {
		hw_fib->header.XferState |= cpu_to_le32(NoResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NoResponseSent);
	} else if (wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NormalSent);
	}

	hw_fib->header.SenderFibAddress = cpu_to_le32(((u32)(fibptr - dev->fibs)) << 2);
	hw_fib->header.SenderData = (u32)(fibptr - dev->fibs);
	hw_fib->header.Command = cpu_to_le16(command);
	hw_fib->header.XferState |= cpu_to_le32(SentFromHost);
	fibptr->hw_fib_va->header.Flags = 0;	
	hw_fib->header.Size = cpu_to_le16(sizeof(struct aac_fibhdr) + size);
	if (le16_to_cpu(hw_fib->header.Size) > le16_to_cpu(hw_fib->header.SenderSize)) {
		return -EMSGSIZE;
	}
	hw_fib->header.XferState |= cpu_to_le32(NormalPriority);

	if (!wait) {
		fibptr->callback = callback;
		fibptr->callback_data = callback_data;
		fibptr->flags = FIB_CONTEXT_FLAG;
	}

	fibptr->done = 0;

	FIB_COUNTER_INCREMENT(aac_config.FibsSent);

	dprintk((KERN_DEBUG "Fib contents:.\n"));
	dprintk((KERN_DEBUG "  Command =               %d.\n", le32_to_cpu(hw_fib->header.Command)));
	dprintk((KERN_DEBUG "  SubCommand =            %d.\n", le32_to_cpu(((struct aac_query_mount *)fib_data(fibptr))->command)));
	dprintk((KERN_DEBUG "  XferState  =            %x.\n", le32_to_cpu(hw_fib->header.XferState)));
	dprintk((KERN_DEBUG "  hw_fib va being sent=%p\n",fibptr->hw_fib_va));
	dprintk((KERN_DEBUG "  hw_fib pa being sent=%lx\n",(ulong)fibptr->hw_fib_pa));
	dprintk((KERN_DEBUG "  fib being sent=%p\n",fibptr));

	if (!dev->queues)
		return -EBUSY;

	if (wait) {

		spin_lock_irqsave(&dev->manage_lock, mflags);
		if (dev->management_fib_count >= AAC_NUM_MGT_FIB) {
			printk(KERN_INFO "No management Fibs Available:%d\n",
						dev->management_fib_count);
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
			return -EBUSY;
		}
		dev->management_fib_count++;
		spin_unlock_irqrestore(&dev->manage_lock, mflags);
		spin_lock_irqsave(&fibptr->event_lock, flags);
	}

	if (dev->sync_mode) {
		if (wait)
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
		spin_lock_irqsave(&dev->sync_lock, sflags);
		if (dev->sync_fib) {
			list_add_tail(&fibptr->fiblink, &dev->sync_fib_list);
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
		} else {
			dev->sync_fib = fibptr;
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
			aac_adapter_sync_cmd(dev, SEND_SYNCHRONOUS_FIB,
				(u32)fibptr->hw_fib_pa, 0, 0, 0, 0, 0,
				NULL, NULL, NULL, NULL, NULL);
		}
		if (wait) {
			fibptr->flags |= FIB_CONTEXT_FLAG_WAIT;
			if (down_interruptible(&fibptr->event_wait)) {
				fibptr->flags &= ~FIB_CONTEXT_FLAG_WAIT;
				return -EFAULT;
			}
			return 0;
		}
		return -EINPROGRESS;
	}

	if (aac_adapter_deliver(fibptr) != 0) {
		printk(KERN_ERR "aac_fib_send: returned -EBUSY\n");
		if (wait) {
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
		}
		return -EBUSY;
	}



	if (wait) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		
		if (wait < 0) {
			unsigned long count = 36000000L; 
			while (down_trylock(&fibptr->event_wait)) {
				int blink;
				if (--count == 0) {
					struct aac_queue * q = &dev->queues->queue[AdapNormCmdQueue];
					spin_lock_irqsave(q->lock, qflags);
					q->numpending--;
					spin_unlock_irqrestore(q->lock, qflags);
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: first asynchronous command timed out.\n"
						  "Usually a result of a PCI interrupt routing problem;\n"
						  "update mother board BIOS or consider utilizing one of\n"
						  "the SAFE mode kernel options (acpi, apic etc)\n");
					}
					return -ETIMEDOUT;
				}
				if ((blink = aac_adapter_check_health(dev)) > 0) {
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: adapter blinkLED 0x%x.\n"
						  "Usually a result of a serious unrecoverable hardware problem\n",
						  blink);
					}
					return -EFAULT;
				}
				udelay(5);
			}
		} else if (down_interruptible(&fibptr->event_wait)) {
		}

		spin_lock_irqsave(&fibptr->event_lock, flags);
		if (fibptr->done == 0) {
			fibptr->done = 2; 
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			return -ERESTARTSYS;
		}
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		BUG_ON(fibptr->done == 0);

		if(unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
			return -ETIMEDOUT;
		return 0;
	}
	if (reply)
		return -EINPROGRESS;
	else
		return 0;
}


int aac_consumer_get(struct aac_dev * dev, struct aac_queue * q, struct aac_entry **entry)
{
	u32 index;
	int status;
	if (le32_to_cpu(*q->headers.producer) == le32_to_cpu(*q->headers.consumer)) {
		status = 0;
	} else {
		if (le32_to_cpu(*q->headers.consumer) >= q->entries)
			index = 0;
		else
			index = le32_to_cpu(*q->headers.consumer);
		*entry = q->base + index;
		status = 1;
	}
	return(status);
}


void aac_consumer_free(struct aac_dev * dev, struct aac_queue *q, u32 qid)
{
	int wasfull = 0;
	u32 notify;

	if ((le32_to_cpu(*q->headers.producer)+1) == le32_to_cpu(*q->headers.consumer))
		wasfull = 1;

	if (le32_to_cpu(*q->headers.consumer) >= q->entries)
		*q->headers.consumer = cpu_to_le32(1);
	else
		le32_add_cpu(q->headers.consumer, 1);

	if (wasfull) {
		switch (qid) {

		case HostNormCmdQueue:
			notify = HostNormCmdNotFull;
			break;
		case HostNormRespQueue:
			notify = HostNormRespNotFull;
			break;
		default:
			BUG();
			return;
		}
		aac_adapter_notify(dev, notify);
	}
}


int aac_fib_adapter_complete(struct fib *fibptr, unsigned short size)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_dev * dev = fibptr->dev;
	struct aac_queue * q;
	unsigned long nointr = 0;
	unsigned long qflags;

	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE1) {
		kfree(hw_fib);
		return 0;
	}

	if (hw_fib->header.XferState == 0) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree(hw_fib);
		return 0;
	}
	if (hw_fib->header.StructType != FIB_MAGIC) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree(hw_fib);
		return -EINVAL;
	}
	if (hw_fib->header.XferState & cpu_to_le32(SentFromAdapter)) {
		if (dev->comm_interface == AAC_COMM_MESSAGE) {
			kfree (hw_fib);
		} else {
			u32 index;
			hw_fib->header.XferState |= cpu_to_le32(HostProcessed);
			if (size) {
				size += sizeof(struct aac_fibhdr);
				if (size > le16_to_cpu(hw_fib->header.SenderSize))
					return -EMSGSIZE;
				hw_fib->header.Size = cpu_to_le16(size);
			}
			q = &dev->queues->queue[AdapNormRespQueue];
			spin_lock_irqsave(q->lock, qflags);
			aac_queue_get(dev, &index, AdapNormRespQueue, hw_fib, 1, NULL, &nointr);
			*(q->headers.producer) = cpu_to_le32(index + 1);
			spin_unlock_irqrestore(q->lock, qflags);
			if (!(nointr & (int)aac_config.irq_mod))
				aac_adapter_notify(dev, AdapNormRespQueue);
		}
	} else {
		printk(KERN_WARNING "aac_fib_adapter_complete: "
			"Unknown xferstate detected.\n");
		BUG();
	}
	return 0;
}


int aac_fib_complete(struct fib *fibptr)
{
	unsigned long flags;
	struct hw_fib * hw_fib = fibptr->hw_fib_va;


	if (hw_fib->header.XferState == 0)
		return 0;

	if (hw_fib->header.StructType != FIB_MAGIC)
		return -EINVAL;
	spin_lock_irqsave(&fibptr->event_lock, flags);
	if (fibptr->done == 2) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&fibptr->event_lock, flags);

	if((hw_fib->header.XferState & cpu_to_le32(SentFromHost)) &&
		(hw_fib->header.XferState & cpu_to_le32(AdapterProcessed)))
	{
		fib_dealloc(fibptr);
	}
	else if(hw_fib->header.XferState & cpu_to_le32(SentFromHost))
	{
		fib_dealloc(fibptr);
	} else if(hw_fib->header.XferState & cpu_to_le32(HostOwned)) {
		fib_dealloc(fibptr);
	} else {
		BUG();
	}
	return 0;
}


void aac_printf(struct aac_dev *dev, u32 val)
{
	char *cp = dev->printfbuf;
	if (dev->printf_enabled)
	{
		int length = val & 0xffff;
		int level = (val >> 16) & 0xffff;

		if (length > 255)
			length = 255;
		if (cp[length] != 0)
			cp[length] = 0;
		if (level == LOG_AAC_HIGH_ERROR)
			printk(KERN_WARNING "%s:%s", dev->name, cp);
		else
			printk(KERN_INFO "%s:%s", dev->name, cp);
	}
	memset(cp, 0, 256);
}



#define AIF_SNIFF_TIMEOUT	(30*HZ)
static void aac_handle_aif(struct aac_dev * dev, struct fib * fibptr)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_aifcmd * aifcmd = (struct aac_aifcmd *)hw_fib->data;
	u32 channel, id, lun, container;
	struct scsi_device *device;
	enum {
		NOTHING,
		DELETE,
		ADD,
		CHANGE
	} device_config_needed = NOTHING;

	

	if (!dev || !dev->fsa_dev)
		return;
	container = channel = id = lun = (u32)-1;

	switch (le32_to_cpu(aifcmd->command)) {
	case AifCmdDriverNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifDenMorphComplete:
		case AifDenVolumeExtendComplete:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;


			if ((dev != NULL) && (dev->scsi_host_ptr != NULL)) {
				device = scsi_device_lookup(dev->scsi_host_ptr,
					CONTAINER_TO_CHANNEL(container),
					CONTAINER_TO_ID(container),
					CONTAINER_TO_LUN(container));
				if (device) {
					dev->fsa_dev[container].config_needed = CHANGE;
					dev->fsa_dev[container].config_waiting_on = AifEnConfigChange;
					dev->fsa_dev[container].config_waiting_stamp = jiffies;
					scsi_device_put(device);
				}
			}
		}

		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdEventNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifEnBatteryEvent:
			dev->cache_protected =
				(((__le32 *)aifcmd->data)[1] == cpu_to_le32(3));
			break;
		case AifEnAddContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = ADD;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		case AifEnDeleteContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = DELETE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		case AifEnContainerChange:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			if (dev->fsa_dev[container].config_waiting_on &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				break;
			dev->fsa_dev[container].config_needed = CHANGE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		case AifEnConfigChange:
			break;

		case AifEnAddJBOD:
		case AifEnDeleteJBOD:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if ((container >> 28)) {
				container = (u32)-1;
				break;
			}
			channel = (container >> 24) & 0xF;
			if (channel >= dev->maximum_num_channels) {
				container = (u32)-1;
				break;
			}
			id = container & 0xFFFF;
			if (id >= dev->maximum_num_physicals) {
				container = (u32)-1;
				break;
			}
			lun = (container >> 16) & 0xFF;
			container = (u32)-1;
			channel = aac_phys_to_logical(channel);
			device_config_needed =
			  (((__le32 *)aifcmd->data)[0] ==
			    cpu_to_le32(AifEnAddJBOD)) ? ADD : DELETE;
			if (device_config_needed == ADD) {
				device = scsi_device_lookup(dev->scsi_host_ptr,
					channel,
					id,
					lun);
				if (device) {
					scsi_remove_device(device);
					scsi_device_put(device);
				}
			}
			break;

		case AifEnEnclosureManagement:
			if (dev->jbod)
				break;
			switch (le32_to_cpu(((__le32 *)aifcmd->data)[3])) {
			case EM_DRIVE_INSERTION:
			case EM_DRIVE_REMOVAL:
				container = le32_to_cpu(
					((__le32 *)aifcmd->data)[2]);
				if ((container >> 28)) {
					container = (u32)-1;
					break;
				}
				channel = (container >> 24) & 0xF;
				if (channel >= dev->maximum_num_channels) {
					container = (u32)-1;
					break;
				}
				id = container & 0xFFFF;
				lun = (container >> 16) & 0xFF;
				container = (u32)-1;
				if (id >= dev->maximum_num_physicals) {
					
					if ((0x2000 <= id) || lun || channel ||
					  ((channel = (id >> 7) & 0x3F) >=
					  dev->maximum_num_channels))
						break;
					lun = (id >> 4) & 7;
					id &= 0xF;
				}
				channel = aac_phys_to_logical(channel);
				device_config_needed =
				  (((__le32 *)aifcmd->data)[3]
				    == cpu_to_le32(EM_DRIVE_INSERTION)) ?
				  ADD : DELETE;
				break;
			}
			break;
		}

		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdJobProgress:

		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    (((__le32 *)aifcmd->data)[6] == ((__le32 *)aifcmd->data)[5] ||
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsSuccess))) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = ADD;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
		}
		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    ((__le32 *)aifcmd->data)[6] == 0 &&
		    ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsRunning)) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = DELETE;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
		}
		break;
	}

	container = 0;
retry_next:
	if (device_config_needed == NOTHING)
	for (; container < dev->maximum_num_containers; ++container) {
		if ((dev->fsa_dev[container].config_waiting_on == 0) &&
			(dev->fsa_dev[container].config_needed != NOTHING) &&
			time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT)) {
			device_config_needed =
				dev->fsa_dev[container].config_needed;
			dev->fsa_dev[container].config_needed = NOTHING;
			channel = CONTAINER_TO_CHANNEL(container);
			id = CONTAINER_TO_ID(container);
			lun = CONTAINER_TO_LUN(container);
			break;
		}
	}
	if (device_config_needed == NOTHING)
		return;



	if (!dev || !dev->scsi_host_ptr)
		return;
	if ((channel == CONTAINER_CHANNEL) &&
	  (device_config_needed != NOTHING)) {
		if (dev->fsa_dev[container].valid == 1)
			dev->fsa_dev[container].valid = 2;
		aac_probe_container(dev, container);
	}
	device = scsi_device_lookup(dev->scsi_host_ptr, channel, id, lun);
	if (device) {
		switch (device_config_needed) {
		case DELETE:
#if (defined(AAC_DEBUG_INSTRUMENT_AIF_DELETE))
			scsi_remove_device(device);
#else
			if (scsi_device_online(device)) {
				scsi_device_set_state(device, SDEV_OFFLINE);
				sdev_printk(KERN_INFO, device,
					"Device offlined - %s\n",
					(channel == CONTAINER_CHANNEL) ?
						"array deleted" :
						"enclosure services event");
			}
#endif
			break;
		case ADD:
			if (!scsi_device_online(device)) {
				sdev_printk(KERN_INFO, device,
					"Device online - %s\n",
					(channel == CONTAINER_CHANNEL) ?
						"array created" :
						"enclosure services event");
				scsi_device_set_state(device, SDEV_RUNNING);
			}
			
		case CHANGE:
			if ((channel == CONTAINER_CHANNEL)
			 && (!dev->fsa_dev[container].valid)) {
#if (defined(AAC_DEBUG_INSTRUMENT_AIF_DELETE))
				scsi_remove_device(device);
#else
				if (!scsi_device_online(device))
					break;
				scsi_device_set_state(device, SDEV_OFFLINE);
				sdev_printk(KERN_INFO, device,
					"Device offlined - %s\n",
					"array failed");
#endif
				break;
			}
			scsi_rescan_device(&device->sdev_gendev);

		default:
			break;
		}
		scsi_device_put(device);
		device_config_needed = NOTHING;
	}
	if (device_config_needed == ADD)
		scsi_add_device(dev->scsi_host_ptr, channel, id, lun);
	if (channel == CONTAINER_CHANNEL) {
		container++;
		device_config_needed = NOTHING;
		goto retry_next;
	}
}

static int _aac_reset_adapter(struct aac_dev *aac, int forced)
{
	int index, quirks;
	int retval;
	struct Scsi_Host *host;
	struct scsi_device *dev;
	struct scsi_cmnd *command;
	struct scsi_cmnd *command_list;
	int jafo = 0;

	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	aac_adapter_disable_int(aac);
	if (aac->thread->pid != current->pid) {
		spin_unlock_irq(host->host_lock);
		kthread_stop(aac->thread);
		jafo = 1;
	}

	retval = aac_adapter_restart(aac, forced ? 0 : aac_adapter_check_health(aac));

	if (retval)
		goto out;

	for (retval = 1, index = 0; index < (aac->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB); index++) {
		struct fib *fib = &aac->fibs[index];
		if (!(fib->hw_fib_va->header.XferState & cpu_to_le32(NoResponseExpected | Async)) &&
		  (fib->hw_fib_va->header.XferState & cpu_to_le32(ResponseExpected))) {
			unsigned long flagv;
			spin_lock_irqsave(&fib->event_lock, flagv);
			up(&fib->event_wait);
			spin_unlock_irqrestore(&fib->event_lock, flagv);
			schedule();
			retval = 0;
		}
	}
	
	if (retval == 0)
		ssleep(2);
	index = aac->cardtype;

	aac_fib_map_free(aac);
	pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr, aac->comm_phys);
	aac->comm_addr = NULL;
	aac->comm_phys = 0;
	kfree(aac->queues);
	aac->queues = NULL;
	free_irq(aac->pdev->irq, aac);
	if (aac->msi)
		pci_disable_msi(aac->pdev);
	kfree(aac->fsa_dev);
	aac->fsa_dev = NULL;
	quirks = aac_get_driver_ident(index)->quirks;
	if (quirks & AAC_QUIRK_31BIT) {
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(31)))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(31)))))
			goto out;
	} else {
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32)))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(32)))))
			goto out;
	}
	if ((retval = (*(aac_get_driver_ident(index)->init))(aac)))
		goto out;
	if (quirks & AAC_QUIRK_31BIT)
		if ((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32))))
			goto out;
	if (jafo) {
		aac->thread = kthread_run(aac_command_thread, aac, aac->name);
		if (IS_ERR(aac->thread)) {
			retval = PTR_ERR(aac->thread);
			goto out;
		}
	}
	(void)aac_get_adapter_info(aac);
	if ((quirks & AAC_QUIRK_34SG) && (host->sg_tablesize > 34)) {
		host->sg_tablesize = 34;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
	if ((quirks & AAC_QUIRK_17SG) && (host->sg_tablesize > 17)) {
		host->sg_tablesize = 17;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
	aac_get_config_status(aac, 1);
	aac_get_containers(aac);
	command_list = NULL;
	__shost_for_each_device(dev, host) {
		unsigned long flags;
		spin_lock_irqsave(&dev->list_lock, flags);
		list_for_each_entry(command, &dev->cmd_list, list)
			if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
				command->SCp.buffer = (struct scatterlist *)command_list;
				command_list = command;
			}
		spin_unlock_irqrestore(&dev->list_lock, flags);
	}
	while ((command = command_list)) {
		command_list = (struct scsi_cmnd *)command->SCp.buffer;
		command->SCp.buffer = NULL;
		command->result = DID_OK << 16
		  | COMMAND_COMPLETE << 8
		  | SAM_STAT_TASK_SET_FULL;
		command->SCp.phase = AAC_OWNER_ERROR_HANDLER;
		command->scsi_done(command);
	}
	retval = 0;

out:
	aac->in_reset = 0;
	scsi_unblock_requests(host);
	if (jafo) {
		spin_lock_irq(host->host_lock);
	}
	return retval;
}

int aac_reset_adapter(struct aac_dev * aac, int forced)
{
	unsigned long flagv = 0;
	int retval;
	struct Scsi_Host * host;

	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return -EBUSY;

	if (aac->in_reset) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return -EBUSY;
	}
	aac->in_reset = 1;
	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	if (forced < 2) for (retval = 60; retval; --retval) {
		struct scsi_device * dev;
		struct scsi_cmnd * command;
		int active = 0;

		__shost_for_each_device(dev, host) {
			spin_lock_irqsave(&dev->list_lock, flagv);
			list_for_each_entry(command, &dev->cmd_list, list) {
				if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
					active++;
					break;
				}
			}
			spin_unlock_irqrestore(&dev->list_lock, flagv);
			if (active)
				break;

		}
		if (active == 0)
			break;
		ssleep(1);
	}

	
	if (forced < 2)
		aac_send_shutdown(aac);
	spin_lock_irqsave(host->host_lock, flagv);
	retval = _aac_reset_adapter(aac, forced ? forced : ((aac_check_reset != 0) && (aac_check_reset != 1)));
	spin_unlock_irqrestore(host->host_lock, flagv);

	if ((forced < 2) && (retval == -ENODEV)) {
		
		struct fib * fibctx = aac_fib_alloc(aac);
		if (fibctx) {
			struct aac_pause *cmd;
			int status;

			aac_fib_init(fibctx);

			cmd = (struct aac_pause *) fib_data(fibctx);

			cmd->command = cpu_to_le32(VM_ContainerConfig);
			cmd->type = cpu_to_le32(CT_PAUSE_IO);
			cmd->timeout = cpu_to_le32(1);
			cmd->min = cpu_to_le32(1);
			cmd->noRescan = cpu_to_le32(1);
			cmd->count = cpu_to_le32(0);

			status = aac_fib_send(ContainerCommand,
			  fibctx,
			  sizeof(struct aac_pause),
			  FsaNormal,
			  -2 , 1,
			  NULL, NULL);

			if (status >= 0)
				aac_fib_complete(fibctx);
			if (status != -ERESTARTSYS)
				aac_fib_free(fibctx);
		}
	}

	return retval;
}

int aac_check_health(struct aac_dev * aac)
{
	int BlinkLED;
	unsigned long time_now, flagv = 0;
	struct list_head * entry;
	struct Scsi_Host * host;

	
	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return 0;

	if (aac->in_reset || !(BlinkLED = aac_adapter_check_health(aac))) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return 0; 
	}

	aac->in_reset = 1;


	time_now = jiffies/HZ;
	entry = aac->fib_list.next;

	while (entry != &aac->fib_list) {
		struct aac_fib_context *fibctx = list_entry(entry, struct aac_fib_context, next);
		struct hw_fib * hw_fib;
		struct fib * fib;
		if (fibctx->count > 20) {
			u32 time_last = fibctx->jiffies;
			if ((time_now - time_last) > aif_timeout) {
				entry = entry->next;
				aac_close_fib_context(aac, fibctx);
				continue;
			}
		}
		hw_fib = kzalloc(sizeof(struct hw_fib), GFP_ATOMIC);
		fib = kzalloc(sizeof(struct fib), GFP_ATOMIC);
		if (fib && hw_fib) {
			struct aac_aifcmd * aif;

			fib->hw_fib_va = hw_fib;
			fib->dev = aac;
			aac_fib_init(fib);
			fib->type = FSAFS_NTC_FIB_CONTEXT;
			fib->size = sizeof (struct fib);
			fib->data = hw_fib->data;
			aif = (struct aac_aifcmd *)hw_fib->data;
			aif->command = cpu_to_le32(AifCmdEventNotify);
			aif->seqnum = cpu_to_le32(0xFFFFFFFF);
			((__le32 *)aif->data)[0] = cpu_to_le32(AifEnExpEvent);
			((__le32 *)aif->data)[1] = cpu_to_le32(AifExeFirmwarePanic);
			((__le32 *)aif->data)[2] = cpu_to_le32(AifHighPriority);
			((__le32 *)aif->data)[3] = cpu_to_le32(BlinkLED);

			list_add_tail(&fib->fiblink, &fibctx->fib_list);
			fibctx->count++;
			up(&fibctx->wait_sem);
		} else {
			printk(KERN_WARNING "aifd: didn't allocate NewFib.\n");
			kfree(fib);
			kfree(hw_fib);
		}
		entry = entry->next;
	}

	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	if (BlinkLED < 0) {
		printk(KERN_ERR "%s: Host adapter dead %d\n", aac->name, BlinkLED);
		goto out;
	}

	printk(KERN_ERR "%s: Host adapter BLINK LED 0x%x\n", aac->name, BlinkLED);

	if (!aac_check_reset || ((aac_check_reset == 1) &&
		(aac->supplement_adapter_info.SupportedOptions2 &
			AAC_OPTION_IGNORE_RESET)))
		goto out;
	host = aac->scsi_host_ptr;
	if (aac->thread->pid != current->pid)
		spin_lock_irqsave(host->host_lock, flagv);
	BlinkLED = _aac_reset_adapter(aac, aac_check_reset != 1);
	if (aac->thread->pid != current->pid)
		spin_unlock_irqrestore(host->host_lock, flagv);
	return BlinkLED;

out:
	aac->in_reset = 0;
	return BlinkLED;
}



int aac_command_thread(void *data)
{
	struct aac_dev *dev = data;
	struct hw_fib *hw_fib, *hw_newfib;
	struct fib *fib, *newfib;
	struct aac_fib_context *fibctx;
	unsigned long flags;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long next_jiffies = jiffies + HZ;
	unsigned long next_check_jiffies = next_jiffies;
	long difference = HZ;

	if (dev->aif_thread)
		return -EINVAL;

	dev->aif_thread = 1;
	add_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	dprintk ((KERN_INFO "aac_command_thread start\n"));
	while (1) {
		spin_lock_irqsave(dev->queues->queue[HostNormCmdQueue].lock, flags);
		while(!list_empty(&(dev->queues->queue[HostNormCmdQueue].cmdq))) {
			struct list_head *entry;
			struct aac_aifcmd * aifcmd;

			set_current_state(TASK_RUNNING);

			entry = dev->queues->queue[HostNormCmdQueue].cmdq.next;
			list_del(entry);

			spin_unlock_irqrestore(dev->queues->queue[HostNormCmdQueue].lock, flags);
			fib = list_entry(entry, struct fib, fiblink);
			hw_fib = fib->hw_fib_va;
			memset(fib, 0, sizeof(struct fib));
			fib->type = FSAFS_NTC_FIB_CONTEXT;
			fib->size = sizeof(struct fib);
			fib->hw_fib_va = hw_fib;
			fib->data = hw_fib->data;
			fib->dev = dev;
			aifcmd = (struct aac_aifcmd *) hw_fib->data;
			if (aifcmd->command == cpu_to_le32(AifCmdDriverNotify)) {
				
				aac_handle_aif(dev, fib);
				*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
				aac_fib_adapter_complete(fib, (u16)sizeof(u32));
			} else {

				u32 time_now, time_last;
				unsigned long flagv;
				unsigned num;
				struct hw_fib ** hw_fib_pool, ** hw_fib_p;
				struct fib ** fib_pool, ** fib_p;

				
				if ((aifcmd->command ==
				     cpu_to_le32(AifCmdEventNotify)) ||
				    (aifcmd->command ==
				     cpu_to_le32(AifCmdJobProgress))) {
					aac_handle_aif(dev, fib);
				}

				time_now = jiffies/HZ;

				num = le32_to_cpu(dev->init->AdapterFibsSize)
				    / sizeof(struct hw_fib); 
				spin_lock_irqsave(&dev->fib_lock, flagv);
				entry = dev->fib_list.next;
				while (entry != &dev->fib_list) {
					entry = entry->next;
					++num;
				}
				spin_unlock_irqrestore(&dev->fib_lock, flagv);
				hw_fib_pool = NULL;
				fib_pool = NULL;
				if (num
				 && ((hw_fib_pool = kmalloc(sizeof(struct hw_fib *) * num, GFP_KERNEL)))
				 && ((fib_pool = kmalloc(sizeof(struct fib *) * num, GFP_KERNEL)))) {
					hw_fib_p = hw_fib_pool;
					fib_p = fib_pool;
					while (hw_fib_p < &hw_fib_pool[num]) {
						if (!(*(hw_fib_p++) = kmalloc(sizeof(struct hw_fib), GFP_KERNEL))) {
							--hw_fib_p;
							break;
						}
						if (!(*(fib_p++) = kmalloc(sizeof(struct fib), GFP_KERNEL))) {
							kfree(*(--hw_fib_p));
							break;
						}
					}
					if ((num = hw_fib_p - hw_fib_pool) == 0) {
						kfree(fib_pool);
						fib_pool = NULL;
						kfree(hw_fib_pool);
						hw_fib_pool = NULL;
					}
				} else {
					kfree(hw_fib_pool);
					hw_fib_pool = NULL;
				}
				spin_lock_irqsave(&dev->fib_lock, flagv);
				entry = dev->fib_list.next;
				hw_fib_p = hw_fib_pool;
				fib_p = fib_pool;
				while (entry != &dev->fib_list) {
					fibctx = list_entry(entry, struct aac_fib_context, next);
					if (fibctx->count > 20)
					{
						time_last = fibctx->jiffies;
						if ((time_now - time_last) > aif_timeout) {
							entry = entry->next;
							aac_close_fib_context(dev, fibctx);
							continue;
						}
					}
					if (hw_fib_p < &hw_fib_pool[num]) {
						hw_newfib = *hw_fib_p;
						*(hw_fib_p++) = NULL;
						newfib = *fib_p;
						*(fib_p++) = NULL;
						memcpy(hw_newfib, hw_fib, sizeof(struct hw_fib));
						memcpy(newfib, fib, sizeof(struct fib));
						newfib->hw_fib_va = hw_newfib;
						list_add_tail(&newfib->fiblink, &fibctx->fib_list);
						fibctx->count++;
						up(&fibctx->wait_sem);
					} else {
						printk(KERN_WARNING "aifd: didn't allocate NewFib.\n");
					}
					entry = entry->next;
				}
				*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
				aac_fib_adapter_complete(fib, sizeof(u32));
				spin_unlock_irqrestore(&dev->fib_lock, flagv);
				
				hw_fib_p = hw_fib_pool;
				fib_p = fib_pool;
				while (hw_fib_p < &hw_fib_pool[num]) {
					kfree(*hw_fib_p);
					kfree(*fib_p);
					++fib_p;
					++hw_fib_p;
				}
				kfree(hw_fib_pool);
				kfree(fib_pool);
			}
			kfree(fib);
			spin_lock_irqsave(dev->queues->queue[HostNormCmdQueue].lock, flags);
		}
		spin_unlock_irqrestore(dev->queues->queue[HostNormCmdQueue].lock, flags);

		if ((time_before(next_check_jiffies,next_jiffies))
		 && ((difference = next_check_jiffies - jiffies) <= 0)) {
			next_check_jiffies = next_jiffies;
			if (aac_check_health(dev) == 0) {
				difference = ((long)(unsigned)check_interval)
					   * HZ;
				next_check_jiffies = jiffies + difference;
			} else if (!dev->queues)
				break;
		}
		if (!time_before(next_check_jiffies,next_jiffies)
		 && ((difference = next_jiffies - jiffies) <= 0)) {
			struct timeval now;
			int ret;

			
			ret = aac_check_health(dev);
			if (!ret && !dev->queues)
				break;
			next_check_jiffies = jiffies
					   + ((long)(unsigned)check_interval)
					   * HZ;
			do_gettimeofday(&now);

			
			if (((1000000 - (1000000 / HZ)) > now.tv_usec)
			 && (now.tv_usec > (1000000 / HZ)))
				difference = (((1000000 - now.tv_usec) * HZ)
				  + 500000) / 1000000;
			else if (ret == 0) {
				struct fib *fibptr;

				if ((fibptr = aac_fib_alloc(dev))) {
					int status;
					__le32 *info;

					aac_fib_init(fibptr);

					info = (__le32 *) fib_data(fibptr);
					if (now.tv_usec > 500000)
						++now.tv_sec;

					*info = cpu_to_le32(now.tv_sec);

					status = aac_fib_send(SendHostTime,
						fibptr,
						sizeof(*info),
						FsaNormal,
						1, 1,
						NULL,
						NULL);
					if (status >= 0)
						aac_fib_complete(fibptr);
					if (status != -ERESTARTSYS)
						aac_fib_free(fibptr);
				}
				difference = (long)(unsigned)update_interval*HZ;
			} else {
				
				difference = 10 * HZ;
			}
			next_jiffies = jiffies + difference;
			if (time_before(next_check_jiffies,next_jiffies))
				difference = next_check_jiffies - jiffies;
		}
		if (difference <= 0)
			difference = 1;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(difference);

		if (kthread_should_stop())
			break;
	}
	if (dev->queues)
		remove_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
	dev->aif_thread = 0;
	return 0;
}
