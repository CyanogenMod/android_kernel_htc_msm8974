/*
 *	Executive OSM
 *
 * 	Copyright (C) 1999-2002	Red Hat Software
 *
 *	Written by Alan Cox, Building Number Three Ltd
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; either version 2 of the License, or (at your
 *	option) any later version.
 *
 *	A lot of the I2O message side code from this is taken from the Red
 *	Creek RCPCI45 adapter driver by Red Creek Communications
 *
 *	Fixes/additions:
 *		Philipp Rumpf
 *		Juha Sievänen <Juha.Sievanen@cs.Helsinki.FI>
 *		Auvo Häkkinen <Auvo.Hakkinen@cs.Helsinki.FI>
 *		Deepak Saxena <deepak@plexity.net>
 *		Boji T Kannanthanam <boji.t.kannanthanam@intel.com>
 *		Alan Cox <alan@lxorguk.ukuu.org.uk>:
 *			Ported to Linux 2.5.
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *			Minor fixes for 2.6.
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *			Support for sysfs included.
 */

#include <linux/module.h>
#include <linux/i2o.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>	
#include <asm/param.h>		
#include "core.h"

#define OSM_NAME "exec-osm"

struct i2o_driver i2o_exec_driver;

static LIST_HEAD(i2o_exec_wait_list);

struct i2o_exec_wait {
	wait_queue_head_t *wq;	
	struct i2o_dma dma;	
	u32 tcntxt;		
	int complete;		
	u32 m;			
	struct i2o_message *msg;	
	struct list_head list;	
	spinlock_t lock;	
};

struct i2o_exec_lct_notify_work {
	struct work_struct work;	
	struct i2o_controller *c;	
};

static struct i2o_class_id i2o_exec_class_id[] = {
	{I2O_CLASS_EXECUTIVE},
	{I2O_CLASS_END}
};

static struct i2o_exec_wait *i2o_exec_wait_alloc(void)
{
	struct i2o_exec_wait *wait;

	wait = kzalloc(sizeof(*wait), GFP_KERNEL);
	if (!wait)
		return NULL;

	INIT_LIST_HEAD(&wait->list);
	spin_lock_init(&wait->lock);

	return wait;
};

static void i2o_exec_wait_free(struct i2o_exec_wait *wait)
{
	kfree(wait);
};

int i2o_msg_post_wait_mem(struct i2o_controller *c, struct i2o_message *msg,
			  unsigned long timeout, struct i2o_dma *dma)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	struct i2o_exec_wait *wait;
	static u32 tcntxt = 0x80000000;
	unsigned long flags;
	int rc = 0;

	wait = i2o_exec_wait_alloc();
	if (!wait) {
		i2o_msg_nop(c, msg);
		return -ENOMEM;
	}

	if (tcntxt == 0xffffffff)
		tcntxt = 0x80000000;

	if (dma)
		wait->dma = *dma;

	msg->u.s.icntxt = cpu_to_le32(i2o_exec_driver.context);
	wait->tcntxt = tcntxt++;
	msg->u.s.tcntxt = cpu_to_le32(wait->tcntxt);

	wait->wq = &wq;
	list_add(&wait->list, &i2o_exec_wait_list);

	i2o_msg_post(c, msg);

	wait_event_interruptible_timeout(wq, wait->complete, timeout * HZ);

	spin_lock_irqsave(&wait->lock, flags);

	wait->wq = NULL;

	if (wait->complete)
		rc = le32_to_cpu(wait->msg->body[0]) >> 24;
	else {
		if (dma)
			dma->virt = NULL;

		rc = -ETIMEDOUT;
	}

	spin_unlock_irqrestore(&wait->lock, flags);

	if (rc != -ETIMEDOUT) {
		i2o_flush_reply(c, wait->m);
		i2o_exec_wait_free(wait);
	}

	return rc;
};

static int i2o_msg_post_wait_complete(struct i2o_controller *c, u32 m,
				      struct i2o_message *msg, u32 context)
{
	struct i2o_exec_wait *wait, *tmp;
	unsigned long flags;
	int rc = 1;

	list_for_each_entry_safe(wait, tmp, &i2o_exec_wait_list, list) {
		if (wait->tcntxt == context) {
			spin_lock_irqsave(&wait->lock, flags);

			list_del(&wait->list);

			wait->m = m;
			wait->msg = msg;
			wait->complete = 1;

			if (wait->wq)
				rc = 0;
			else
				rc = -1;

			spin_unlock_irqrestore(&wait->lock, flags);

			if (rc) {
				struct device *dev;

				dev = &c->pdev->dev;

				pr_debug("%s: timedout reply received!\n",
					 c->name);
				i2o_dma_free(dev, &wait->dma);
				i2o_exec_wait_free(wait);
			} else
				wake_up_interruptible(wait->wq);

			return rc;
		}
	}

	osm_warn("%s: Bogus reply in POST WAIT (tr-context: %08x)!\n", c->name,
		 context);

	return -1;
};

static ssize_t i2o_exec_show_vendor_id(struct device *d,
				       struct device_attribute *attr, char *buf)
{
	struct i2o_device *dev = to_i2o_device(d);
	u16 id;

	if (!i2o_parm_field_get(dev, 0x0000, 0, &id, 2)) {
		sprintf(buf, "0x%04x", le16_to_cpu(id));
		return strlen(buf) + 1;
	}

	return 0;
};

static ssize_t i2o_exec_show_product_id(struct device *d,
					struct device_attribute *attr,
					char *buf)
{
	struct i2o_device *dev = to_i2o_device(d);
	u16 id;

	if (!i2o_parm_field_get(dev, 0x0000, 1, &id, 2)) {
		sprintf(buf, "0x%04x", le16_to_cpu(id));
		return strlen(buf) + 1;
	}

	return 0;
};

static DEVICE_ATTR(vendor_id, S_IRUGO, i2o_exec_show_vendor_id, NULL);
static DEVICE_ATTR(product_id, S_IRUGO, i2o_exec_show_product_id, NULL);

static int i2o_exec_probe(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);
	int rc;

	rc = i2o_event_register(i2o_dev, &i2o_exec_driver, 0, 0xffffffff);
	if (rc) goto err_out;

	rc = device_create_file(dev, &dev_attr_vendor_id);
	if (rc) goto err_evtreg;
	rc = device_create_file(dev, &dev_attr_product_id);
	if (rc) goto err_vid;

	i2o_dev->iop->exec = i2o_dev;

	return 0;

err_vid:
	device_remove_file(dev, &dev_attr_vendor_id);
err_evtreg:
	i2o_event_register(to_i2o_device(dev), &i2o_exec_driver, 0, 0);
err_out:
	return rc;
};

static int i2o_exec_remove(struct device *dev)
{
	device_remove_file(dev, &dev_attr_product_id);
	device_remove_file(dev, &dev_attr_vendor_id);

	i2o_event_register(to_i2o_device(dev), &i2o_exec_driver, 0, 0);

	return 0;
};

#ifdef CONFIG_I2O_LCT_NOTIFY_ON_CHANGES
static int i2o_exec_lct_notify(struct i2o_controller *c, u32 change_ind)
{
	i2o_status_block *sb = c->status_block.virt;
	struct device *dev;
	struct i2o_message *msg;

	mutex_lock(&c->lct_lock);

	dev = &c->pdev->dev;

	if (i2o_dma_realloc(dev, &c->dlct,
					le32_to_cpu(sb->expected_lct_size))) {
		mutex_unlock(&c->lct_lock);
		return -ENOMEM;
	}

	msg = i2o_msg_get_wait(c, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg)) {
		mutex_unlock(&c->lct_lock);
		return PTR_ERR(msg);
	}

	msg->u.head[0] = cpu_to_le32(EIGHT_WORD_MSG_SIZE | SGL_OFFSET_6);
	msg->u.head[1] = cpu_to_le32(I2O_CMD_LCT_NOTIFY << 24 | HOST_TID << 12 |
				     ADAPTER_TID);
	msg->u.s.icntxt = cpu_to_le32(i2o_exec_driver.context);
	msg->u.s.tcntxt = cpu_to_le32(0x00000000);
	msg->body[0] = cpu_to_le32(0xffffffff);
	msg->body[1] = cpu_to_le32(change_ind);
	msg->body[2] = cpu_to_le32(0xd0000000 | c->dlct.len);
	msg->body[3] = cpu_to_le32(c->dlct.phys);

	i2o_msg_post(c, msg);

	mutex_unlock(&c->lct_lock);

	return 0;
}
#endif

static void i2o_exec_lct_modified(struct work_struct *_work)
{
	struct i2o_exec_lct_notify_work *work =
		container_of(_work, struct i2o_exec_lct_notify_work, work);
	u32 change_ind = 0;
	struct i2o_controller *c = work->c;

	kfree(work);

	if (i2o_device_parse_lct(c) != -EAGAIN)
		change_ind = c->lct->change_ind + 1;

#ifdef CONFIG_I2O_LCT_NOTIFY_ON_CHANGES
	i2o_exec_lct_notify(c, change_ind);
#endif
};

static int i2o_exec_reply(struct i2o_controller *c, u32 m,
			  struct i2o_message *msg)
{
	u32 context;

	if (le32_to_cpu(msg->u.head[0]) & MSG_FAIL) {
		struct i2o_message __iomem *pmsg;
		u32 pm;


		pm = le32_to_cpu(msg->body[3]);
		pmsg = i2o_msg_in_to_virt(c, pm);
		context = readl(&pmsg->u.s.tcntxt);

		i2o_report_status(KERN_INFO, "i2o_core", msg);

		
		i2o_msg_nop_mfa(c, pm);
	} else
		context = le32_to_cpu(msg->u.s.tcntxt);

	if (context & 0x80000000)
		return i2o_msg_post_wait_complete(c, m, msg, context);

	if ((le32_to_cpu(msg->u.head[1]) >> 24) == I2O_CMD_LCT_NOTIFY) {
		struct i2o_exec_lct_notify_work *work;

		pr_debug("%s: LCT notify received\n", c->name);

		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (!work)
			return -ENOMEM;

		work->c = c;

		INIT_WORK(&work->work, i2o_exec_lct_modified);
		queue_work(i2o_exec_driver.event_queue, &work->work);
		return 1;
	}

	printk(KERN_WARNING "%s: Unsolicited message reply sent to core!"
	       "Message dumped to syslog\n", c->name);
	i2o_dump_message(msg);

	return -EFAULT;
}

static void i2o_exec_event(struct work_struct *work)
{
	struct i2o_event *evt = container_of(work, struct i2o_event, work);

	if (likely(evt->i2o_dev))
		osm_debug("Event received from device: %d\n",
			  evt->i2o_dev->lct_data.tid);
	kfree(evt);
};

int i2o_exec_lct_get(struct i2o_controller *c)
{
	struct i2o_message *msg;
	int i = 0;
	int rc = -EAGAIN;

	for (i = 1; i <= I2O_LCT_GET_TRIES; i++) {
		msg = i2o_msg_get_wait(c, I2O_TIMEOUT_MESSAGE_GET);
		if (IS_ERR(msg))
			return PTR_ERR(msg);

		msg->u.head[0] =
		    cpu_to_le32(EIGHT_WORD_MSG_SIZE | SGL_OFFSET_6);
		msg->u.head[1] =
		    cpu_to_le32(I2O_CMD_LCT_NOTIFY << 24 | HOST_TID << 12 |
				ADAPTER_TID);
		msg->body[0] = cpu_to_le32(0xffffffff);
		msg->body[1] = cpu_to_le32(0x00000000);
		msg->body[2] = cpu_to_le32(0xd0000000 | c->dlct.len);
		msg->body[3] = cpu_to_le32(c->dlct.phys);

		rc = i2o_msg_post_wait(c, msg, I2O_TIMEOUT_LCT_GET);
		if (rc < 0)
			break;

		rc = i2o_device_parse_lct(c);
		if (rc != -EAGAIN)
			break;
	}

	return rc;
}

struct i2o_driver i2o_exec_driver = {
	.name = OSM_NAME,
	.reply = i2o_exec_reply,
	.event = i2o_exec_event,
	.classes = i2o_exec_class_id,
	.driver = {
		   .probe = i2o_exec_probe,
		   .remove = i2o_exec_remove,
		   },
};

int __init i2o_exec_init(void)
{
	return i2o_driver_register(&i2o_exec_driver);
};

void i2o_exec_exit(void)
{
	i2o_driver_unregister(&i2o_exec_driver);
};

EXPORT_SYMBOL(i2o_msg_post_wait_mem);
EXPORT_SYMBOL(i2o_exec_lct_get);
