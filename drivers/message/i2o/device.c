/*
 *	Functions to handle I2O devices
 *
 *	Copyright (C) 2004	Markus Lidel <Markus.Lidel@shadowconnect.com>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; either version 2 of the License, or (at your
 *	option) any later version.
 *
 *	Fixes/additions:
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>
 *			initial version.
 */

#include <linux/module.h>
#include <linux/i2o.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "core.h"

static inline int i2o_device_issue_claim(struct i2o_device *dev, u32 cmd,
					 u32 type)
{
	struct i2o_message *msg;

	msg = i2o_msg_get_wait(dev->iop, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(cmd << 24 | HOST_TID << 12 | dev->lct_data.tid);
	msg->body[0] = cpu_to_le32(type);

	return i2o_msg_post_wait(dev->iop, msg, 60);
}

int i2o_device_claim(struct i2o_device *dev)
{
	int rc = 0;

	mutex_lock(&dev->lock);

	rc = i2o_device_issue_claim(dev, I2O_CMD_UTIL_CLAIM, I2O_CLAIM_PRIMARY);
	if (!rc)
		pr_debug("i2o: claim of device %d succeeded\n",
			 dev->lct_data.tid);
	else
		pr_debug("i2o: claim of device %d failed %d\n",
			 dev->lct_data.tid, rc);

	mutex_unlock(&dev->lock);

	return rc;
}

int i2o_device_claim_release(struct i2o_device *dev)
{
	int tries;
	int rc = 0;

	mutex_lock(&dev->lock);

	for (tries = 0; tries < 10; tries++) {
		rc = i2o_device_issue_claim(dev, I2O_CMD_UTIL_RELEASE,
					    I2O_CLAIM_PRIMARY);
		if (!rc)
			break;

		ssleep(1);
	}

	if (!rc)
		pr_debug("i2o: claim release of device %d succeeded\n",
			 dev->lct_data.tid);
	else
		pr_debug("i2o: claim release of device %d failed %d\n",
			 dev->lct_data.tid, rc);

	mutex_unlock(&dev->lock);

	return rc;
}

static void i2o_device_release(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);

	pr_debug("i2o: device %s released\n", dev_name(dev));

	kfree(i2o_dev);
}

static ssize_t i2o_device_show_class_id(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);

	sprintf(buf, "0x%03x\n", i2o_dev->lct_data.class_id);
	return strlen(buf) + 1;
}

static ssize_t i2o_device_show_tid(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);

	sprintf(buf, "0x%03x\n", i2o_dev->lct_data.tid);
	return strlen(buf) + 1;
}

struct device_attribute i2o_device_attrs[] = {
	__ATTR(class_id, S_IRUGO, i2o_device_show_class_id, NULL),
	__ATTR(tid, S_IRUGO, i2o_device_show_tid, NULL),
	__ATTR_NULL
};

static struct i2o_device *i2o_device_alloc(void)
{
	struct i2o_device *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&dev->list);
	mutex_init(&dev->lock);

	dev->device.bus = &i2o_bus_type;
	dev->device.release = &i2o_device_release;

	return dev;
}

static int i2o_device_add(struct i2o_controller *c, i2o_lct_entry *entry)
{
	struct i2o_device *i2o_dev, *tmp;
	int rc;

	i2o_dev = i2o_device_alloc();
	if (IS_ERR(i2o_dev)) {
		printk(KERN_ERR "i2o: unable to allocate i2o device\n");
		return PTR_ERR(i2o_dev);
	}

	i2o_dev->lct_data = *entry;

	dev_set_name(&i2o_dev->device, "%d:%03x", c->unit,
		     i2o_dev->lct_data.tid);

	i2o_dev->iop = c;
	i2o_dev->device.parent = &c->device;

	rc = device_register(&i2o_dev->device);
	if (rc)
		goto err;

	list_add_tail(&i2o_dev->list, &c->devices);

	
	tmp = i2o_iop_find_device(i2o_dev->iop, i2o_dev->lct_data.user_tid);
	if (tmp && (tmp != i2o_dev)) {
		rc = sysfs_create_link(&i2o_dev->device.kobj,
				       &tmp->device.kobj, "user");
		if (rc)
			goto unreg_dev;
	}

	
	list_for_each_entry(tmp, &c->devices, list)
	    if ((tmp->lct_data.user_tid == i2o_dev->lct_data.tid)
		&& (tmp != i2o_dev)) {
		rc = sysfs_create_link(&tmp->device.kobj,
				       &i2o_dev->device.kobj, "user");
		if (rc)
			goto rmlink1;
	}

	
	tmp = i2o_iop_find_device(i2o_dev->iop, i2o_dev->lct_data.parent_tid);
	if (tmp && (tmp != i2o_dev)) {
		rc = sysfs_create_link(&i2o_dev->device.kobj,
				       &tmp->device.kobj, "parent");
		if (rc)
			goto rmlink1;
	}

	
	list_for_each_entry(tmp, &c->devices, list)
	    if ((tmp->lct_data.parent_tid == i2o_dev->lct_data.tid)
		&& (tmp != i2o_dev)) {
		rc = sysfs_create_link(&tmp->device.kobj,
				       &i2o_dev->device.kobj, "parent");
		if (rc)
			goto rmlink2;
	}

	i2o_driver_notify_device_add_all(i2o_dev);

	pr_debug("i2o: device %s added\n", dev_name(&i2o_dev->device));

	return 0;

rmlink2:
	list_for_each_entry(tmp, &c->devices, list) {
		if (tmp->lct_data.parent_tid == i2o_dev->lct_data.tid)
			sysfs_remove_link(&tmp->device.kobj, "parent");
	}
	sysfs_remove_link(&i2o_dev->device.kobj, "parent");
rmlink1:
	list_for_each_entry(tmp, &c->devices, list)
		if (tmp->lct_data.user_tid == i2o_dev->lct_data.tid)
			sysfs_remove_link(&tmp->device.kobj, "user");
	sysfs_remove_link(&i2o_dev->device.kobj, "user");
unreg_dev:
	list_del(&i2o_dev->list);
	device_unregister(&i2o_dev->device);
err:
	kfree(i2o_dev);
	return rc;
}

void i2o_device_remove(struct i2o_device *i2o_dev)
{
	struct i2o_device *tmp;
	struct i2o_controller *c = i2o_dev->iop;

	i2o_driver_notify_device_remove_all(i2o_dev);

	sysfs_remove_link(&i2o_dev->device.kobj, "parent");
	sysfs_remove_link(&i2o_dev->device.kobj, "user");

	list_for_each_entry(tmp, &c->devices, list) {
		if (tmp->lct_data.parent_tid == i2o_dev->lct_data.tid)
			sysfs_remove_link(&tmp->device.kobj, "parent");
		if (tmp->lct_data.user_tid == i2o_dev->lct_data.tid)
			sysfs_remove_link(&tmp->device.kobj, "user");
	}
	list_del(&i2o_dev->list);

	device_unregister(&i2o_dev->device);
}

int i2o_device_parse_lct(struct i2o_controller *c)
{
	struct i2o_device *dev, *tmp;
	i2o_lct *lct;
	u32 *dlct = c->dlct.virt;
	int max = 0, i = 0;
	u16 table_size;
	u32 buf;

	mutex_lock(&c->lct_lock);

	kfree(c->lct);

	buf = le32_to_cpu(*dlct++);
	table_size = buf & 0xffff;

	lct = c->lct = kmalloc(table_size * 4, GFP_KERNEL);
	if (!lct) {
		mutex_unlock(&c->lct_lock);
		return -ENOMEM;
	}

	lct->lct_ver = buf >> 28;
	lct->boot_tid = buf >> 16 & 0xfff;
	lct->table_size = table_size;
	lct->change_ind = le32_to_cpu(*dlct++);
	lct->iop_flags = le32_to_cpu(*dlct++);

	table_size -= 3;

	pr_debug("%s: LCT has %d entries (LCT size: %d)\n", c->name, max,
		 lct->table_size);

	while (table_size > 0) {
		i2o_lct_entry *entry = &lct->lct_entry[max];
		int found = 0;

		buf = le32_to_cpu(*dlct++);
		entry->entry_size = buf & 0xffff;
		entry->tid = buf >> 16 & 0xfff;

		entry->change_ind = le32_to_cpu(*dlct++);
		entry->device_flags = le32_to_cpu(*dlct++);

		buf = le32_to_cpu(*dlct++);
		entry->class_id = buf & 0xfff;
		entry->version = buf >> 12 & 0xf;
		entry->vendor_id = buf >> 16;

		entry->sub_class = le32_to_cpu(*dlct++);

		buf = le32_to_cpu(*dlct++);
		entry->user_tid = buf & 0xfff;
		entry->parent_tid = buf >> 12 & 0xfff;
		entry->bios_info = buf >> 24;

		memcpy(&entry->identity_tag, dlct, 8);
		dlct += 2;

		entry->event_capabilities = le32_to_cpu(*dlct++);

		
		list_for_each_entry_safe(dev, tmp, &c->devices, list) {
			if (entry->tid == dev->lct_data.tid) {
				found = 1;
				break;
			}
		}

		if (!found)
			i2o_device_add(c, entry);

		table_size -= 9;
		max++;
	}

	
	list_for_each_entry_safe(dev, tmp, &c->devices, list) {
		int found = 0;

		for (i = 0; i < max; i++) {
			if (lct->lct_entry[i].tid == dev->lct_data.tid) {
				found = 1;
				break;
			}
		}

		if (!found)
			i2o_device_remove(dev);
	}

	mutex_unlock(&c->lct_lock);

	return 0;
}


int i2o_parm_issue(struct i2o_device *i2o_dev, int cmd, void *oplist,
		   int oplen, void *reslist, int reslen)
{
	struct i2o_message *msg;
	int i = 0;
	int rc;
	struct i2o_dma res;
	struct i2o_controller *c = i2o_dev->iop;
	struct device *dev = &c->pdev->dev;

	res.virt = NULL;

	if (i2o_dma_alloc(dev, &res, reslen))
		return -ENOMEM;

	msg = i2o_msg_get_wait(c, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg)) {
		i2o_dma_free(dev, &res);
		return PTR_ERR(msg);
	}

	i = 0;
	msg->u.head[1] =
	    cpu_to_le32(cmd << 24 | HOST_TID << 12 | i2o_dev->lct_data.tid);
	msg->body[i++] = cpu_to_le32(0x00000000);
	msg->body[i++] = cpu_to_le32(0x4C000000 | oplen);	
	memcpy(&msg->body[i], oplist, oplen);
	i += (oplen / 4 + (oplen % 4 ? 1 : 0));
	msg->body[i++] = cpu_to_le32(0xD0000000 | res.len);	
	msg->body[i++] = cpu_to_le32(res.phys);

	msg->u.head[0] =
	    cpu_to_le32(I2O_MESSAGE_SIZE(i + sizeof(struct i2o_message) / 4) |
			SGL_OFFSET_5);

	rc = i2o_msg_post_wait_mem(c, msg, 10, &res);

	
	if (rc == -ETIMEDOUT)
		return rc;

	memcpy(reslist, res.virt, res.len);
	i2o_dma_free(dev, &res);

	return rc;
}

int i2o_parm_field_get(struct i2o_device *i2o_dev, int group, int field,
		       void *buf, int buflen)
{
	u32 opblk[] = { cpu_to_le32(0x00000001),
		cpu_to_le32((u16) group << 16 | I2O_PARAMS_FIELD_GET),
		cpu_to_le32((s16) field << 16 | 0x00000001)
	};
	u8 *resblk;		
	int rc;

	resblk = kmalloc(buflen + 8, GFP_KERNEL);
	if (!resblk)
		return -ENOMEM;

	rc = i2o_parm_issue(i2o_dev, I2O_CMD_UTIL_PARAMS_GET, opblk,
			    sizeof(opblk), resblk, buflen + 8);

	memcpy(buf, resblk + 8, buflen);	

	kfree(resblk);

	return rc;
}

int i2o_parm_table_get(struct i2o_device *dev, int oper, int group,
		       int fieldcount, void *ibuf, int ibuflen, void *resblk,
		       int reslen)
{
	u16 *opblk;
	int size;

	size = 10 + ibuflen;
	if (size % 4)
		size += 4 - size % 4;

	opblk = kmalloc(size, GFP_KERNEL);
	if (opblk == NULL) {
		printk(KERN_ERR "i2o: no memory for query buffer.\n");
		return -ENOMEM;
	}

	opblk[0] = 1;		
	opblk[1] = 0;		
	opblk[2] = oper;
	opblk[3] = group;
	opblk[4] = fieldcount;
	memcpy(opblk + 5, ibuf, ibuflen);	

	size = i2o_parm_issue(dev, I2O_CMD_UTIL_PARAMS_GET, opblk,
			      size, resblk, reslen);

	kfree(opblk);
	if (size > reslen)
		return reslen;

	return size;
}

EXPORT_SYMBOL(i2o_device_claim);
EXPORT_SYMBOL(i2o_device_claim_release);
EXPORT_SYMBOL(i2o_parm_field_get);
EXPORT_SYMBOL(i2o_parm_table_get);
EXPORT_SYMBOL(i2o_parm_issue);
