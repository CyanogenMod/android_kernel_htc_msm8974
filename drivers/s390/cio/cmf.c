/*
 * linux/drivers/s390/cio/cmf.c
 *
 * Linux on zSeries Channel Measurement Facility support
 *
 * Copyright 2000,2006 IBM Corporation
 *
 * Authors: Arnd Bergmann <arndb@de.ibm.com>
 *	    Cornelia Huck <cornelia.huck@de.ibm.com>
 *
 * original idea from Natarajan Krishnaswami <nkrishna@us.ibm.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define KMSG_COMPONENT "cio"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/bootmem.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/timex.h>	

#include <asm/ccwdev.h>
#include <asm/cio.h>
#include <asm/cmb.h>
#include <asm/div64.h>

#include "cio.h"
#include "css.h"
#include "device.h"
#include "ioasm.h"
#include "chsc.h"

#define ARGSTRING "s390cmf"

enum cmb_index {
 
	cmb_ssch_rsch_count,
	cmb_sample_count,
	cmb_device_connect_time,
	cmb_function_pending_time,
	cmb_device_disconnect_time,
	cmb_control_unit_queuing_time,
	cmb_device_active_only_time,
 
	cmb_device_busy_time,
	cmb_initial_command_response_time,
};

enum cmb_format {
	CMF_BASIC,
	CMF_EXTENDED,
	CMF_AUTODETECT = -1,
};

static int format = CMF_AUTODETECT;
module_param(format, bint, 0444);

struct cmb_operations {
	int  (*alloc)  (struct ccw_device *);
	void (*free)   (struct ccw_device *);
	int  (*set)    (struct ccw_device *, u32);
	u64  (*read)   (struct ccw_device *, int);
	int  (*readall)(struct ccw_device *, struct cmbdata *);
	void (*reset)  (struct ccw_device *);
	void *(*align) (void *);
	struct attribute_group *attr_group;
};
static struct cmb_operations *cmbops;

struct cmb_data {
	void *hw_block;   
	void *last_block; 
	int size;	  
	unsigned long long last_update;  
};

static inline u64 time_to_nsec(u32 value)
{
	return ((u64)value) * 128000ull;
}

static inline u64 time_to_avg_nsec(u32 value, u32 count)
{
	u64 ret;

	
	if (count == 0)
		return 0;

	
	ret = time_to_nsec(value);
	do_div(ret, count);

	return ret;
}

static inline void cmf_activate(void *area, unsigned int onoff)
{
	register void * __gpr2 asm("2");
	register long __gpr1 asm("1");

	__gpr2 = area;
	__gpr1 = onoff ? 2 : 0;
	
	asm("schm" : : "d" (__gpr2), "d" (__gpr1) );
}

static int set_schib(struct ccw_device *cdev, u32 mme, int mbfc,
		     unsigned long address)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);

	sch->config.mme = mme;
	sch->config.mbfc = mbfc;
	
	if (mbfc)
		sch->config.mba = address;
	else
		sch->config.mbi = address;

	return cio_commit_config(sch);
}

struct set_schib_struct {
	u32 mme;
	int mbfc;
	unsigned long address;
	wait_queue_head_t wait;
	int ret;
	struct kref kref;
};

static void cmf_set_schib_release(struct kref *kref)
{
	struct set_schib_struct *set_data;

	set_data = container_of(kref, struct set_schib_struct, kref);
	kfree(set_data);
}

#define CMF_PENDING 1

static int set_schib_wait(struct ccw_device *cdev, u32 mme,
				int mbfc, unsigned long address)
{
	struct set_schib_struct *set_data;
	int ret;

	spin_lock_irq(cdev->ccwlock);
	if (!cdev->private->cmb) {
		ret = -ENODEV;
		goto out;
	}
	set_data = kzalloc(sizeof(struct set_schib_struct), GFP_ATOMIC);
	if (!set_data) {
		ret = -ENOMEM;
		goto out;
	}
	init_waitqueue_head(&set_data->wait);
	kref_init(&set_data->kref);
	set_data->mme = mme;
	set_data->mbfc = mbfc;
	set_data->address = address;

	ret = set_schib(cdev, mme, mbfc, address);
	if (ret != -EBUSY)
		goto out_put;

	if (cdev->private->state != DEV_STATE_ONLINE) {
		
		ret = -EBUSY;
		goto out_put;
	}

	cdev->private->state = DEV_STATE_CMFCHANGE;
	set_data->ret = CMF_PENDING;
	cdev->private->cmb_wait = set_data;

	spin_unlock_irq(cdev->ccwlock);
	if (wait_event_interruptible(set_data->wait,
				     set_data->ret != CMF_PENDING)) {
		spin_lock_irq(cdev->ccwlock);
		if (set_data->ret == CMF_PENDING) {
			set_data->ret = -ERESTARTSYS;
			if (cdev->private->state == DEV_STATE_CMFCHANGE)
				cdev->private->state = DEV_STATE_ONLINE;
		}
		spin_unlock_irq(cdev->ccwlock);
	}
	spin_lock_irq(cdev->ccwlock);
	cdev->private->cmb_wait = NULL;
	ret = set_data->ret;
out_put:
	kref_put(&set_data->kref, cmf_set_schib_release);
out:
	spin_unlock_irq(cdev->ccwlock);
	return ret;
}

void retry_set_schib(struct ccw_device *cdev)
{
	struct set_schib_struct *set_data;

	set_data = cdev->private->cmb_wait;
	if (!set_data) {
		WARN_ON(1);
		return;
	}
	kref_get(&set_data->kref);
	set_data->ret = set_schib(cdev, set_data->mme, set_data->mbfc,
				  set_data->address);
	wake_up(&set_data->wait);
	kref_put(&set_data->kref, cmf_set_schib_release);
}

static int cmf_copy_block(struct ccw_device *cdev)
{
	struct subchannel *sch;
	void *reference_buf;
	void *hw_block;
	struct cmb_data *cmb_data;

	sch = to_subchannel(cdev->dev.parent);

	if (cio_update_schib(sch))
		return -ENODEV;

	if (scsw_fctl(&sch->schib.scsw) & SCSW_FCTL_START_FUNC) {
		
		if ((!(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_SUSPENDED)) &&
		    (scsw_actl(&sch->schib.scsw) &
		     (SCSW_ACTL_DEVACT | SCSW_ACTL_SCHACT)) &&
		    (!(scsw_stctl(&sch->schib.scsw) & SCSW_STCTL_SEC_STATUS)))
			return -EBUSY;
	}
	cmb_data = cdev->private->cmb;
	hw_block = cmbops->align(cmb_data->hw_block);
	if (!memcmp(cmb_data->last_block, hw_block, cmb_data->size))
		
		return 0;
	reference_buf = kzalloc(cmb_data->size, GFP_ATOMIC);
	if (!reference_buf)
		return -ENOMEM;
	
	do {
		memcpy(cmb_data->last_block, hw_block, cmb_data->size);
		memcpy(reference_buf, hw_block, cmb_data->size);
	} while (memcmp(cmb_data->last_block, reference_buf, cmb_data->size));
	cmb_data->last_update = get_clock();
	kfree(reference_buf);
	return 0;
}

struct copy_block_struct {
	wait_queue_head_t wait;
	int ret;
	struct kref kref;
};

static void cmf_copy_block_release(struct kref *kref)
{
	struct copy_block_struct *copy_block;

	copy_block = container_of(kref, struct copy_block_struct, kref);
	kfree(copy_block);
}

static int cmf_cmb_copy_wait(struct ccw_device *cdev)
{
	struct copy_block_struct *copy_block;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		ret = -ENODEV;
		goto out;
	}
	copy_block = kzalloc(sizeof(struct copy_block_struct), GFP_ATOMIC);
	if (!copy_block) {
		ret = -ENOMEM;
		goto out;
	}
	init_waitqueue_head(&copy_block->wait);
	kref_init(&copy_block->kref);

	ret = cmf_copy_block(cdev);
	if (ret != -EBUSY)
		goto out_put;

	if (cdev->private->state != DEV_STATE_ONLINE) {
		ret = -EBUSY;
		goto out_put;
	}

	cdev->private->state = DEV_STATE_CMFUPDATE;
	copy_block->ret = CMF_PENDING;
	cdev->private->cmb_wait = copy_block;

	spin_unlock_irqrestore(cdev->ccwlock, flags);
	if (wait_event_interruptible(copy_block->wait,
				     copy_block->ret != CMF_PENDING)) {
		spin_lock_irqsave(cdev->ccwlock, flags);
		if (copy_block->ret == CMF_PENDING) {
			copy_block->ret = -ERESTARTSYS;
			if (cdev->private->state == DEV_STATE_CMFUPDATE)
				cdev->private->state = DEV_STATE_ONLINE;
		}
		spin_unlock_irqrestore(cdev->ccwlock, flags);
	}
	spin_lock_irqsave(cdev->ccwlock, flags);
	cdev->private->cmb_wait = NULL;
	ret = copy_block->ret;
out_put:
	kref_put(&copy_block->kref, cmf_copy_block_release);
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

void cmf_retry_copy_block(struct ccw_device *cdev)
{
	struct copy_block_struct *copy_block;

	copy_block = cdev->private->cmb_wait;
	if (!copy_block) {
		WARN_ON(1);
		return;
	}
	kref_get(&copy_block->kref);
	copy_block->ret = cmf_copy_block(cdev);
	wake_up(&copy_block->wait);
	kref_put(&copy_block->kref, cmf_copy_block_release);
}

static void cmf_generic_reset(struct ccw_device *cdev)
{
	struct cmb_data *cmb_data;

	spin_lock_irq(cdev->ccwlock);
	cmb_data = cdev->private->cmb;
	if (cmb_data) {
		memset(cmb_data->last_block, 0, cmb_data->size);
		memset(cmbops->align(cmb_data->hw_block), 0, cmb_data->size);
		cmb_data->last_update = 0;
	}
	cdev->private->cmb_start_time = get_clock();
	spin_unlock_irq(cdev->ccwlock);
}

struct cmb_area {
	struct cmb *mem;
	struct list_head list;
	int num_channels;
	spinlock_t lock;
};

static struct cmb_area cmb_area = {
	.lock = __SPIN_LOCK_UNLOCKED(cmb_area.lock),
	.list = LIST_HEAD_INIT(cmb_area.list),
	.num_channels  = 1024,
};



module_param_named(maxchannels, cmb_area.num_channels, uint, 0444);

struct cmb {
	u16 ssch_rsch_count;
	u16 sample_count;
	u32 device_connect_time;
	u32 function_pending_time;
	u32 device_disconnect_time;
	u32 control_unit_queuing_time;
	u32 device_active_only_time;
	u32 reserved[2];
};

static int alloc_cmb_single(struct ccw_device *cdev,
			    struct cmb_data *cmb_data)
{
	struct cmb *cmb;
	struct ccw_device_private *node;
	int ret;

	spin_lock_irq(cdev->ccwlock);
	if (!list_empty(&cdev->private->cmb_list)) {
		ret = -EBUSY;
		goto out;
	}

	cmb = cmb_area.mem;
	list_for_each_entry(node, &cmb_area.list, cmb_list) {
		struct cmb_data *data;
		data = node->cmb;
		if ((struct cmb*)data->hw_block > cmb)
			break;
		cmb++;
	}
	if (cmb - cmb_area.mem >= cmb_area.num_channels) {
		ret = -ENOMEM;
		goto out;
	}

	
	list_add_tail(&cdev->private->cmb_list, &node->cmb_list);
	cmb_data->hw_block = cmb;
	cdev->private->cmb = cmb_data;
	ret = 0;
out:
	spin_unlock_irq(cdev->ccwlock);
	return ret;
}

static int alloc_cmb(struct ccw_device *cdev)
{
	int ret;
	struct cmb *mem;
	ssize_t size;
	struct cmb_data *cmb_data;

	
	cmb_data = kzalloc(sizeof(struct cmb_data), GFP_KERNEL);
	if (!cmb_data)
		return -ENOMEM;

	cmb_data->last_block = kzalloc(sizeof(struct cmb), GFP_KERNEL);
	if (!cmb_data->last_block) {
		kfree(cmb_data);
		return -ENOMEM;
	}
	cmb_data->size = sizeof(struct cmb);
	spin_lock(&cmb_area.lock);

	if (!cmb_area.mem) {
		
		size = sizeof(struct cmb) * cmb_area.num_channels;
		WARN_ON(!list_empty(&cmb_area.list));

		spin_unlock(&cmb_area.lock);
		mem = (void*)__get_free_pages(GFP_KERNEL | GFP_DMA,
				 get_order(size));
		spin_lock(&cmb_area.lock);

		if (cmb_area.mem) {
			
			free_pages((unsigned long)mem, get_order(size));
		} else if (!mem) {
			
			ret = -ENOMEM;
			goto out;
		} else {
			
			memset(mem, 0, size);
			cmb_area.mem = mem;
			cmf_activate(cmb_area.mem, 1);
		}
	}

	
	ret = alloc_cmb_single(cdev, cmb_data);
out:
	spin_unlock(&cmb_area.lock);
	if (ret) {
		kfree(cmb_data->last_block);
		kfree(cmb_data);
	}
	return ret;
}

static void free_cmb(struct ccw_device *cdev)
{
	struct ccw_device_private *priv;
	struct cmb_data *cmb_data;

	spin_lock(&cmb_area.lock);
	spin_lock_irq(cdev->ccwlock);

	priv = cdev->private;

	if (list_empty(&priv->cmb_list)) {
		
		goto out;
	}

	cmb_data = priv->cmb;
	priv->cmb = NULL;
	if (cmb_data)
		kfree(cmb_data->last_block);
	kfree(cmb_data);
	list_del_init(&priv->cmb_list);

	if (list_empty(&cmb_area.list)) {
		ssize_t size;
		size = sizeof(struct cmb) * cmb_area.num_channels;
		cmf_activate(NULL, 0);
		free_pages((unsigned long)cmb_area.mem, get_order(size));
		cmb_area.mem = NULL;
	}
out:
	spin_unlock_irq(cdev->ccwlock);
	spin_unlock(&cmb_area.lock);
}

static int set_cmb(struct ccw_device *cdev, u32 mme)
{
	u16 offset;
	struct cmb_data *cmb_data;
	unsigned long flags;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		spin_unlock_irqrestore(cdev->ccwlock, flags);
		return -EINVAL;
	}
	cmb_data = cdev->private->cmb;
	offset = mme ? (struct cmb *)cmb_data->hw_block - cmb_area.mem : 0;
	spin_unlock_irqrestore(cdev->ccwlock, flags);

	return set_schib_wait(cdev, mme, 0, offset);
}

static u64 read_cmb(struct ccw_device *cdev, int index)
{
	struct cmb *cmb;
	u32 val;
	int ret;
	unsigned long flags;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return 0;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		ret = 0;
		goto out;
	}
	cmb = ((struct cmb_data *)cdev->private->cmb)->last_block;

	switch (index) {
	case cmb_ssch_rsch_count:
		ret = cmb->ssch_rsch_count;
		goto out;
	case cmb_sample_count:
		ret = cmb->sample_count;
		goto out;
	case cmb_device_connect_time:
		val = cmb->device_connect_time;
		break;
	case cmb_function_pending_time:
		val = cmb->function_pending_time;
		break;
	case cmb_device_disconnect_time:
		val = cmb->device_disconnect_time;
		break;
	case cmb_control_unit_queuing_time:
		val = cmb->control_unit_queuing_time;
		break;
	case cmb_device_active_only_time:
		val = cmb->device_active_only_time;
		break;
	default:
		ret = 0;
		goto out;
	}
	ret = time_to_avg_nsec(val, cmb->sample_count);
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static int readall_cmb(struct ccw_device *cdev, struct cmbdata *data)
{
	struct cmb *cmb;
	struct cmb_data *cmb_data;
	u64 time;
	unsigned long flags;
	int ret;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return ret;
	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data) {
		ret = -ENODEV;
		goto out;
	}
	if (cmb_data->last_update == 0) {
		ret = -EAGAIN;
		goto out;
	}
	cmb = cmb_data->last_block;
	time = cmb_data->last_update - cdev->private->cmb_start_time;

	memset(data, 0, sizeof(struct cmbdata));

	
	data->size = offsetof(struct cmbdata, device_busy_time);

	
	data->elapsed_time = (time * 1000) >> 12;

	
	data->ssch_rsch_count = cmb->ssch_rsch_count;
	data->sample_count = cmb->sample_count;

	
	data->device_connect_time = time_to_nsec(cmb->device_connect_time);
	data->function_pending_time = time_to_nsec(cmb->function_pending_time);
	data->device_disconnect_time =
		time_to_nsec(cmb->device_disconnect_time);
	data->control_unit_queuing_time
		= time_to_nsec(cmb->control_unit_queuing_time);
	data->device_active_only_time
		= time_to_nsec(cmb->device_active_only_time);
	ret = 0;
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static void reset_cmb(struct ccw_device *cdev)
{
	cmf_generic_reset(cdev);
}

static void * align_cmb(void *area)
{
	return area;
}

static struct attribute_group cmf_attr_group;

static struct cmb_operations cmbops_basic = {
	.alloc	= alloc_cmb,
	.free	= free_cmb,
	.set	= set_cmb,
	.read	= read_cmb,
	.readall    = readall_cmb,
	.reset	    = reset_cmb,
	.align	    = align_cmb,
	.attr_group = &cmf_attr_group,
};


struct cmbe {
	u32 ssch_rsch_count;
	u32 sample_count;
	u32 device_connect_time;
	u32 function_pending_time;
	u32 device_disconnect_time;
	u32 control_unit_queuing_time;
	u32 device_active_only_time;
	u32 device_busy_time;
	u32 initial_command_response_time;
	u32 reserved[7];
};

static inline struct cmbe *cmbe_align(struct cmbe *c)
{
	unsigned long addr;
	addr = ((unsigned long)c + sizeof (struct cmbe) - sizeof(long)) &
				 ~(sizeof (struct cmbe) - sizeof(long));
	return (struct cmbe*)addr;
}

static int alloc_cmbe(struct ccw_device *cdev)
{
	struct cmbe *cmbe;
	struct cmb_data *cmb_data;
	int ret;

	cmbe = kzalloc (sizeof (*cmbe) * 2, GFP_KERNEL);
	if (!cmbe)
		return -ENOMEM;
	cmb_data = kzalloc(sizeof(struct cmb_data), GFP_KERNEL);
	if (!cmb_data) {
		ret = -ENOMEM;
		goto out_free;
	}
	cmb_data->last_block = kzalloc(sizeof(struct cmbe), GFP_KERNEL);
	if (!cmb_data->last_block) {
		ret = -ENOMEM;
		goto out_free;
	}
	cmb_data->size = sizeof(struct cmbe);
	spin_lock_irq(cdev->ccwlock);
	if (cdev->private->cmb) {
		spin_unlock_irq(cdev->ccwlock);
		ret = -EBUSY;
		goto out_free;
	}
	cmb_data->hw_block = cmbe;
	cdev->private->cmb = cmb_data;
	spin_unlock_irq(cdev->ccwlock);

	
	spin_lock(&cmb_area.lock);
	if (list_empty(&cmb_area.list))
		cmf_activate(NULL, 1);
	list_add_tail(&cdev->private->cmb_list, &cmb_area.list);
	spin_unlock(&cmb_area.lock);

	return 0;
out_free:
	if (cmb_data)
		kfree(cmb_data->last_block);
	kfree(cmb_data);
	kfree(cmbe);
	return ret;
}

static void free_cmbe(struct ccw_device *cdev)
{
	struct cmb_data *cmb_data;

	spin_lock_irq(cdev->ccwlock);
	cmb_data = cdev->private->cmb;
	cdev->private->cmb = NULL;
	if (cmb_data)
		kfree(cmb_data->last_block);
	kfree(cmb_data);
	spin_unlock_irq(cdev->ccwlock);

	
	spin_lock(&cmb_area.lock);
	list_del_init(&cdev->private->cmb_list);
	if (list_empty(&cmb_area.list))
		cmf_activate(NULL, 0);
	spin_unlock(&cmb_area.lock);
}

static int set_cmbe(struct ccw_device *cdev, u32 mme)
{
	unsigned long mba;
	struct cmb_data *cmb_data;
	unsigned long flags;

	spin_lock_irqsave(cdev->ccwlock, flags);
	if (!cdev->private->cmb) {
		spin_unlock_irqrestore(cdev->ccwlock, flags);
		return -EINVAL;
	}
	cmb_data = cdev->private->cmb;
	mba = mme ? (unsigned long) cmbe_align(cmb_data->hw_block) : 0;
	spin_unlock_irqrestore(cdev->ccwlock, flags);

	return set_schib_wait(cdev, mme, 1, mba);
}


static u64 read_cmbe(struct ccw_device *cdev, int index)
{
	struct cmbe *cmb;
	struct cmb_data *cmb_data;
	u32 val;
	int ret;
	unsigned long flags;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return 0;

	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data) {
		ret = 0;
		goto out;
	}
	cmb = cmb_data->last_block;

	switch (index) {
	case cmb_ssch_rsch_count:
		ret = cmb->ssch_rsch_count;
		goto out;
	case cmb_sample_count:
		ret = cmb->sample_count;
		goto out;
	case cmb_device_connect_time:
		val = cmb->device_connect_time;
		break;
	case cmb_function_pending_time:
		val = cmb->function_pending_time;
		break;
	case cmb_device_disconnect_time:
		val = cmb->device_disconnect_time;
		break;
	case cmb_control_unit_queuing_time:
		val = cmb->control_unit_queuing_time;
		break;
	case cmb_device_active_only_time:
		val = cmb->device_active_only_time;
		break;
	case cmb_device_busy_time:
		val = cmb->device_busy_time;
		break;
	case cmb_initial_command_response_time:
		val = cmb->initial_command_response_time;
		break;
	default:
		ret = 0;
		goto out;
	}
	ret = time_to_avg_nsec(val, cmb->sample_count);
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static int readall_cmbe(struct ccw_device *cdev, struct cmbdata *data)
{
	struct cmbe *cmb;
	struct cmb_data *cmb_data;
	u64 time;
	unsigned long flags;
	int ret;

	ret = cmf_cmb_copy_wait(cdev);
	if (ret < 0)
		return ret;
	spin_lock_irqsave(cdev->ccwlock, flags);
	cmb_data = cdev->private->cmb;
	if (!cmb_data) {
		ret = -ENODEV;
		goto out;
	}
	if (cmb_data->last_update == 0) {
		ret = -EAGAIN;
		goto out;
	}
	time = cmb_data->last_update - cdev->private->cmb_start_time;

	memset (data, 0, sizeof(struct cmbdata));

	
	data->size = offsetof(struct cmbdata, device_busy_time);

	
	data->elapsed_time = (time * 1000) >> 12;

	cmb = cmb_data->last_block;
	
	data->ssch_rsch_count = cmb->ssch_rsch_count;
	data->sample_count = cmb->sample_count;

	
	data->device_connect_time = time_to_nsec(cmb->device_connect_time);
	data->function_pending_time = time_to_nsec(cmb->function_pending_time);
	data->device_disconnect_time =
		time_to_nsec(cmb->device_disconnect_time);
	data->control_unit_queuing_time
		= time_to_nsec(cmb->control_unit_queuing_time);
	data->device_active_only_time
		= time_to_nsec(cmb->device_active_only_time);
	data->device_busy_time = time_to_nsec(cmb->device_busy_time);
	data->initial_command_response_time
		= time_to_nsec(cmb->initial_command_response_time);

	ret = 0;
out:
	spin_unlock_irqrestore(cdev->ccwlock, flags);
	return ret;
}

static void reset_cmbe(struct ccw_device *cdev)
{
	cmf_generic_reset(cdev);
}

static void * align_cmbe(void *area)
{
	return cmbe_align(area);
}

static struct attribute_group cmf_attr_group_ext;

static struct cmb_operations cmbops_extended = {
	.alloc	    = alloc_cmbe,
	.free	    = free_cmbe,
	.set	    = set_cmbe,
	.read	    = read_cmbe,
	.readall    = readall_cmbe,
	.reset	    = reset_cmbe,
	.align	    = align_cmbe,
	.attr_group = &cmf_attr_group_ext,
};

static ssize_t cmb_show_attr(struct device *dev, char *buf, enum cmb_index idx)
{
	return sprintf(buf, "%lld\n",
		(unsigned long long) cmf_read(to_ccwdev(dev), idx));
}

static ssize_t cmb_show_avg_sample_interval(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct ccw_device *cdev;
	long interval;
	unsigned long count;
	struct cmb_data *cmb_data;

	cdev = to_ccwdev(dev);
	count = cmf_read(cdev, cmb_sample_count);
	spin_lock_irq(cdev->ccwlock);
	cmb_data = cdev->private->cmb;
	if (count) {
		interval = cmb_data->last_update -
			cdev->private->cmb_start_time;
		interval = (interval * 1000) >> 12;
		interval /= count;
	} else
		interval = -1;
	spin_unlock_irq(cdev->ccwlock);
	return sprintf(buf, "%ld\n", interval);
}

static ssize_t cmb_show_avg_utilization(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct cmbdata data;
	u64 utilization;
	unsigned long t, u;
	int ret;

	ret = cmf_readall(to_ccwdev(dev), &data);
	if (ret == -EAGAIN || ret == -ENODEV)
		
		return sprintf(buf, "n/a\n");
	else if (ret)
		return ret;

	utilization = data.device_connect_time +
		      data.function_pending_time +
		      data.device_disconnect_time;

	
	while (-1ul < (data.elapsed_time | utilization)) {
		utilization >>= 8;
		data.elapsed_time >>= 8;
	}

	
	t = (unsigned long) data.elapsed_time / 1000;
	u = (unsigned long) utilization / t;

	return sprintf(buf, "%02ld.%01ld%%\n", u/ 10, u - (u/ 10) * 10);
}

#define cmf_attr(name) \
static ssize_t show_##name(struct device *dev, \
			   struct device_attribute *attr, char *buf)	\
{ return cmb_show_attr((dev), buf, cmb_##name); } \
static DEVICE_ATTR(name, 0444, show_##name, NULL);

#define cmf_attr_avg(name) \
static ssize_t show_avg_##name(struct device *dev, \
			       struct device_attribute *attr, char *buf) \
{ return cmb_show_attr((dev), buf, cmb_##name); } \
static DEVICE_ATTR(avg_##name, 0444, show_avg_##name, NULL);

cmf_attr(ssch_rsch_count);
cmf_attr(sample_count);
cmf_attr_avg(device_connect_time);
cmf_attr_avg(function_pending_time);
cmf_attr_avg(device_disconnect_time);
cmf_attr_avg(control_unit_queuing_time);
cmf_attr_avg(device_active_only_time);
cmf_attr_avg(device_busy_time);
cmf_attr_avg(initial_command_response_time);

static DEVICE_ATTR(avg_sample_interval, 0444, cmb_show_avg_sample_interval,
		   NULL);
static DEVICE_ATTR(avg_utilization, 0444, cmb_show_avg_utilization, NULL);

static struct attribute *cmf_attributes[] = {
	&dev_attr_avg_sample_interval.attr,
	&dev_attr_avg_utilization.attr,
	&dev_attr_ssch_rsch_count.attr,
	&dev_attr_sample_count.attr,
	&dev_attr_avg_device_connect_time.attr,
	&dev_attr_avg_function_pending_time.attr,
	&dev_attr_avg_device_disconnect_time.attr,
	&dev_attr_avg_control_unit_queuing_time.attr,
	&dev_attr_avg_device_active_only_time.attr,
	NULL,
};

static struct attribute_group cmf_attr_group = {
	.name  = "cmf",
	.attrs = cmf_attributes,
};

static struct attribute *cmf_attributes_ext[] = {
	&dev_attr_avg_sample_interval.attr,
	&dev_attr_avg_utilization.attr,
	&dev_attr_ssch_rsch_count.attr,
	&dev_attr_sample_count.attr,
	&dev_attr_avg_device_connect_time.attr,
	&dev_attr_avg_function_pending_time.attr,
	&dev_attr_avg_device_disconnect_time.attr,
	&dev_attr_avg_control_unit_queuing_time.attr,
	&dev_attr_avg_device_active_only_time.attr,
	&dev_attr_avg_device_busy_time.attr,
	&dev_attr_avg_initial_command_response_time.attr,
	NULL,
};

static struct attribute_group cmf_attr_group_ext = {
	.name  = "cmf",
	.attrs = cmf_attributes_ext,
};

static ssize_t cmb_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%d\n", to_ccwdev(dev)->private->cmb ? 1 : 0);
}

static ssize_t cmb_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t c)
{
	struct ccw_device *cdev;
	int ret;
	unsigned long val;

	ret = strict_strtoul(buf, 16, &val);
	if (ret)
		return ret;

	cdev = to_ccwdev(dev);

	switch (val) {
	case 0:
		ret = disable_cmf(cdev);
		break;
	case 1:
		ret = enable_cmf(cdev);
		break;
	}

	return c;
}

DEVICE_ATTR(cmb_enable, 0644, cmb_enable_show, cmb_enable_store);

int ccw_set_cmf(struct ccw_device *cdev, int enable)
{
	return cmbops->set(cdev, enable ? 2 : 0);
}

int enable_cmf(struct ccw_device *cdev)
{
	int ret;

	ret = cmbops->alloc(cdev);
	cmbops->reset(cdev);
	if (ret)
		return ret;
	ret = cmbops->set(cdev, 2);
	if (ret) {
		cmbops->free(cdev);
		return ret;
	}
	ret = sysfs_create_group(&cdev->dev.kobj, cmbops->attr_group);
	if (!ret)
		return 0;
	cmbops->set(cdev, 0);  
	cmbops->free(cdev);
	return ret;
}

int disable_cmf(struct ccw_device *cdev)
{
	int ret;

	ret = cmbops->set(cdev, 0);
	if (ret)
		return ret;
	cmbops->free(cdev);
	sysfs_remove_group(&cdev->dev.kobj, cmbops->attr_group);
	return ret;
}

u64 cmf_read(struct ccw_device *cdev, int index)
{
	return cmbops->read(cdev, index);
}

int cmf_readall(struct ccw_device *cdev, struct cmbdata *data)
{
	return cmbops->readall(cdev, data);
}

int cmf_reenable(struct ccw_device *cdev)
{
	cmbops->reset(cdev);
	return cmbops->set(cdev, 2);
}

static int __init init_cmf(void)
{
	char *format_string;
	char *detect_string = "parameter";

	if (format == CMF_AUTODETECT) {
		if (!css_general_characteristics.ext_mb) {
			format = CMF_BASIC;
		} else {
			format = CMF_EXTENDED;
		}
		detect_string = "autodetected";
	} else {
		detect_string = "parameter";
	}

	switch (format) {
	case CMF_BASIC:
		format_string = "basic";
		cmbops = &cmbops_basic;
		break;
	case CMF_EXTENDED:
		format_string = "extended";
		cmbops = &cmbops_extended;
		break;
	default:
		return 1;
	}
	pr_info("Channel measurement facility initialized using format "
		"%s (mode %s)\n", format_string, detect_string);
	return 0;
}

module_init(init_cmf);


MODULE_AUTHOR("Arnd Bergmann <arndb@de.ibm.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("channel measurement facility base driver\n"
		   "Copyright 2003 IBM Corporation\n");

EXPORT_SYMBOL_GPL(enable_cmf);
EXPORT_SYMBOL_GPL(disable_cmf);
EXPORT_SYMBOL_GPL(cmf_read);
EXPORT_SYMBOL_GPL(cmf_readall);
