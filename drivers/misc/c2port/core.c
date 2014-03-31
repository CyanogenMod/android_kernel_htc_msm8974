/*
 *  Silicon Labs C2 port core Linux support
 *
 *  Copyright (c) 2007 Rodolfo Giometti <giometti@linux.it>
 *  Copyright (c) 2007 Eurotech S.p.A. <info@eurotech.it>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kmemcheck.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/c2port.h>

#define DRIVER_NAME             "c2port"
#define DRIVER_VERSION          "0.51.0"

static DEFINE_SPINLOCK(c2port_idr_lock);
static DEFINE_IDR(c2port_idr);


static struct class *c2port_class;


#define C2PORT_DEVICEID		0x00
#define C2PORT_REVID		0x01
#define C2PORT_FPCTL		0x02
#define C2PORT_FPDAT		0xB4

#define C2PORT_GET_VERSION	0x01
#define C2PORT_DEVICE_ERASE	0x03
#define C2PORT_BLOCK_READ	0x06
#define C2PORT_BLOCK_WRITE	0x07
#define C2PORT_PAGE_ERASE	0x08

#define C2PORT_INVALID_COMMAND	0x00
#define C2PORT_COMMAND_FAILED	0x02
#define C2PORT_COMMAND_OK	0x0d


static void c2port_reset(struct c2port_device *dev)
{
	struct c2port_ops *ops = dev->ops;

	local_irq_disable();
	ops->c2ck_set(dev, 0);
	udelay(25);
	ops->c2ck_set(dev, 1);
	local_irq_enable();

	udelay(1);
}

static void c2port_strobe_ck(struct c2port_device *dev)
{
	struct c2port_ops *ops = dev->ops;

	local_irq_disable();
	ops->c2ck_set(dev, 0);
	udelay(1);
	ops->c2ck_set(dev, 1);
	local_irq_enable();

	udelay(1);
}


static void c2port_write_ar(struct c2port_device *dev, u8 addr)
{
	struct c2port_ops *ops = dev->ops;
	int i;

	
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);

	
	for (i = 0; i < 8; i++) {
		ops->c2d_set(dev, addr & 0x01);
		c2port_strobe_ck(dev);

		addr >>= 1;
	}

	
	ops->c2d_dir(dev, 1);
	c2port_strobe_ck(dev);
}

static int c2port_read_ar(struct c2port_device *dev, u8 *addr)
{
	struct c2port_ops *ops = dev->ops;
	int i;

	
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 1);
	*addr = 0;
	for (i = 0; i < 8; i++) {
		*addr >>= 1;	

		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			*addr |= 0x80;
	}

	
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_write_dr(struct c2port_device *dev, u8 data)
{
	struct c2port_ops *ops = dev->ops;
	int timeout, i;

	
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	
	for (i = 0; i < 8; i++) {
		ops->c2d_set(dev, data & 0x01);
		c2port_strobe_ck(dev);

		data >>= 1;
	}

	
	ops->c2d_dir(dev, 1);
	timeout = 20;
	do {
		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_read_dr(struct c2port_device *dev, u8 *data)
{
	struct c2port_ops *ops = dev->ops;
	int timeout, i;

	
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	
	ops->c2d_dir(dev, 1);
	timeout = 20;
	do {
		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	
	*data = 0;
	for (i = 0; i < 8; i++) {
		*data >>= 1;	

		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			*data |= 0x80;
	}

	
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_poll_in_busy(struct c2port_device *dev)
{
	u8 addr;
	int ret, timeout = 20;

	do {
		ret = (c2port_read_ar(dev, &addr));
		if (ret < 0)
			return -EIO;

		if (!(addr & 0x02))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	return 0;
}

static int c2port_poll_out_ready(struct c2port_device *dev)
{
	u8 addr;
	int ret, timeout = 10000; 

	do {
		ret = (c2port_read_ar(dev, &addr));
		if (ret < 0)
			return -EIO;

		if (addr & 0x01)
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	return 0;
}


static ssize_t c2port_show_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", c2dev->name);
}

static ssize_t c2port_show_flash_blocks_num(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->blocks_num);
}

static ssize_t c2port_show_flash_block_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->block_size);
}

static ssize_t c2port_show_flash_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->blocks_num * ops->block_size);
}

static ssize_t c2port_show_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", c2dev->access);
}

static ssize_t c2port_store_access(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;
	int status, ret;

	ret = sscanf(buf, "%d", &status);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&c2dev->mutex);

	c2dev->access = !!status;

	if (c2dev->access)
		ops->c2ck_set(c2dev, 1);
	ops->access(c2dev, c2dev->access);
	if (c2dev->access)
		ops->c2d_dir(c2dev, 1);

	mutex_unlock(&c2dev->mutex);

	return count;
}

static ssize_t c2port_store_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);

	c2port_reset(c2dev);
	c2dev->flash_access = 0;

	mutex_unlock(&c2dev->mutex);

	return count;
}

static ssize_t __c2port_show_dev_id(struct c2port_device *dev, char *buf)
{
	u8 data;
	int ret;

	
	c2port_write_ar(dev, C2PORT_DEVICEID);

	
	ret = c2port_read_dr(dev, &data);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", data);
}

static ssize_t c2port_show_dev_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	ssize_t ret;

	
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_show_dev_id(c2dev, buf);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(dev, "cannot read from %s\n", c2dev->name);

	return ret;
}

static ssize_t __c2port_show_rev_id(struct c2port_device *dev, char *buf)
{
	u8 data;
	int ret;

	
	c2port_write_ar(dev, C2PORT_REVID);

	
	ret = c2port_read_dr(dev, &data);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", data);
}

static ssize_t c2port_show_rev_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	ssize_t ret;

	
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_show_rev_id(c2dev, buf);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot read from %s\n", c2dev->name);

	return ret;
}

static ssize_t c2port_show_flash_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", c2dev->flash_access);
}

static ssize_t __c2port_store_flash_access(struct c2port_device *dev,
						int status)
{
	int ret;

	
	if (!dev->access)
		return -EBUSY;

	dev->flash_access = !!status;

	
	if (dev->flash_access == 0)
		return 0;

	c2port_write_ar(dev, C2PORT_FPCTL);

	
	ret = c2port_write_dr(dev, 0x02);
	if (ret < 0)
		return ret;

	
	ret = c2port_write_dr(dev, 0x01);
	if (ret < 0)
		return ret;

	mdelay(25);

	return 0;
}

static ssize_t c2port_store_flash_access(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	int status;
	ssize_t ret;

	ret = sscanf(buf, "%d", &status);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_store_flash_access(c2dev, status);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0) {
		dev_err(c2dev->dev, "cannot enable %s flash programming\n",
			c2dev->name);
		return ret;
	}

	return count;
}

static ssize_t __c2port_write_flash_erase(struct c2port_device *dev)
{
	u8 status;
	int ret;

	c2port_write_ar(dev, C2PORT_FPDAT);

	
	c2port_write_dr(dev, C2PORT_DEVICE_ERASE);

	
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	c2port_write_dr(dev, 0xde);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;
	c2port_write_dr(dev, 0xad);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;
	c2port_write_dr(dev, 0xa5);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	return 0;
}

static ssize_t c2port_store_flash_erase(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	int ret;

	
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_write_flash_erase(c2dev);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0) {
		dev_err(c2dev->dev, "cannot erase %s flash\n", c2dev->name);
		return ret;
	}

	return count;
}

static ssize_t __c2port_read_flash_data(struct c2port_device *dev,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_ops *ops = dev->ops;
	u8 status, nread = 128;
	int i, ret;

	
	if (offset >= ops->block_size * ops->blocks_num)
		return 0;

	if (ops->block_size * ops->blocks_num - offset < nread)
		nread = ops->block_size * ops->blocks_num - offset;
	if (count < nread)
		nread = count;
	if (nread == 0)
		return nread;

	c2port_write_ar(dev, C2PORT_FPDAT);

	
	c2port_write_dr(dev, C2PORT_BLOCK_READ);

	
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	
	c2port_write_dr(dev, offset >> 8);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	
	c2port_write_dr(dev, offset & 0x00ff);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	
	c2port_write_dr(dev, nread);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	
	for (i = 0; i < nread; i++) {
		ret = c2port_poll_out_ready(dev);
		if (ret < 0)
			return ret;

		ret = c2port_read_dr(dev, buffer+i);
		if (ret < 0)
			return ret;
	}

	return nread;
}

static ssize_t c2port_read_flash_data(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_device *c2dev =
			dev_get_drvdata(container_of(kobj,
						struct device, kobj));
	ssize_t ret;

	
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_read_flash_data(c2dev, buffer, offset, count);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot read %s flash\n", c2dev->name);

	return ret;
}

static ssize_t __c2port_write_flash_data(struct c2port_device *dev,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_ops *ops = dev->ops;
	u8 status, nwrite = 128;
	int i, ret;

	if (nwrite > count)
		nwrite = count;
	if (ops->block_size * ops->blocks_num - offset < nwrite)
		nwrite = ops->block_size * ops->blocks_num - offset;

	
	if (offset >= ops->block_size * ops->blocks_num)
		return -EINVAL;

	c2port_write_ar(dev, C2PORT_FPDAT);

	
	c2port_write_dr(dev, C2PORT_BLOCK_WRITE);

	
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	
	c2port_write_dr(dev, offset >> 8);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	
	c2port_write_dr(dev, offset & 0x00ff);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	
	c2port_write_dr(dev, nwrite);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	
	for (i = 0; i < nwrite; i++) {
		ret = c2port_write_dr(dev, *(buffer+i));
		if (ret < 0)
			return ret;

		ret = c2port_poll_in_busy(dev);
		if (ret < 0)
			return ret;

	}

	
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	return nwrite;
}

static ssize_t c2port_write_flash_data(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_device *c2dev =
			dev_get_drvdata(container_of(kobj,
						struct device, kobj));
	int ret;

	
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_write_flash_data(c2dev, buffer, offset, count);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot write %s flash\n", c2dev->name);

	return ret;
}


static struct device_attribute c2port_attrs[] = {
	__ATTR(name, 0444, c2port_show_name, NULL),
	__ATTR(flash_blocks_num, 0444, c2port_show_flash_blocks_num, NULL),
	__ATTR(flash_block_size, 0444, c2port_show_flash_block_size, NULL),
	__ATTR(flash_size, 0444, c2port_show_flash_size, NULL),
	__ATTR(access, 0644, c2port_show_access, c2port_store_access),
	__ATTR(reset, 0200, NULL, c2port_store_reset),
	__ATTR(dev_id, 0444, c2port_show_dev_id, NULL),
	__ATTR(rev_id, 0444, c2port_show_rev_id, NULL),

	__ATTR(flash_access, 0644, c2port_show_flash_access,
					c2port_store_flash_access),
	__ATTR(flash_erase, 0200, NULL, c2port_store_flash_erase),
	__ATTR_NULL,
};

static struct bin_attribute c2port_bin_attrs = {
	.attr	= {
		.name	= "flash_data",
		.mode	= 0644
	},
	.read	= c2port_read_flash_data,
	.write	= c2port_write_flash_data,
	
};


struct c2port_device *c2port_device_register(char *name,
					struct c2port_ops *ops, void *devdata)
{
	struct c2port_device *c2dev;
	int id, ret;

	if (unlikely(!ops) || unlikely(!ops->access) || \
		unlikely(!ops->c2d_dir) || unlikely(!ops->c2ck_set) || \
		unlikely(!ops->c2d_get) || unlikely(!ops->c2d_set))
		return ERR_PTR(-EINVAL);

	c2dev = kmalloc(sizeof(struct c2port_device), GFP_KERNEL);
	kmemcheck_annotate_bitfield(c2dev, flags);
	if (unlikely(!c2dev))
		return ERR_PTR(-ENOMEM);

	ret = idr_pre_get(&c2port_idr, GFP_KERNEL);
	if (!ret) {
		ret = -ENOMEM;
		goto error_idr_get_new;
	}

	spin_lock_irq(&c2port_idr_lock);
	ret = idr_get_new(&c2port_idr, c2dev, &id);
	spin_unlock_irq(&c2port_idr_lock);

	if (ret < 0)
		goto error_idr_get_new;
	c2dev->id = id;

	c2dev->dev = device_create(c2port_class, NULL, 0, c2dev,
					"c2port%d", id);
	if (unlikely(IS_ERR(c2dev->dev))) {
		ret = PTR_ERR(c2dev->dev);
		goto error_device_create;
	}
	dev_set_drvdata(c2dev->dev, c2dev);

	strncpy(c2dev->name, name, C2PORT_NAME_LEN);
	c2dev->ops = ops;
	mutex_init(&c2dev->mutex);

	
	c2port_bin_attrs.size = ops->blocks_num * ops->block_size;
	ret = device_create_bin_file(c2dev->dev, &c2port_bin_attrs);
	if (unlikely(ret))
		goto error_device_create_bin_file;

	
	c2dev->access = c2dev->flash_access = 0;
	ops->access(c2dev, 0);

	dev_info(c2dev->dev, "C2 port %s added\n", name);
	dev_info(c2dev->dev, "%s flash has %d blocks x %d bytes "
				"(%d bytes total)\n",
				name, ops->blocks_num, ops->block_size,
				ops->blocks_num * ops->block_size);

	return c2dev;

error_device_create_bin_file:
	device_destroy(c2port_class, 0);

error_device_create:
	spin_lock_irq(&c2port_idr_lock);
	idr_remove(&c2port_idr, id);
	spin_unlock_irq(&c2port_idr_lock);

error_idr_get_new:
	kfree(c2dev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(c2port_device_register);

void c2port_device_unregister(struct c2port_device *c2dev)
{
	if (!c2dev)
		return;

	dev_info(c2dev->dev, "C2 port %s removed\n", c2dev->name);

	device_remove_bin_file(c2dev->dev, &c2port_bin_attrs);
	spin_lock_irq(&c2port_idr_lock);
	idr_remove(&c2port_idr, c2dev->id);
	spin_unlock_irq(&c2port_idr_lock);

	device_destroy(c2port_class, c2dev->id);

	kfree(c2dev);
}
EXPORT_SYMBOL(c2port_device_unregister);


static int __init c2port_init(void)
{
	printk(KERN_INFO "Silicon Labs C2 port support v. " DRIVER_VERSION
		" - (C) 2007 Rodolfo Giometti\n");

	c2port_class = class_create(THIS_MODULE, "c2port");
	if (IS_ERR(c2port_class)) {
		printk(KERN_ERR "c2port: failed to allocate class\n");
		return PTR_ERR(c2port_class);
	}
	c2port_class->dev_attrs = c2port_attrs;

	return 0;
}

static void __exit c2port_exit(void)
{
	class_destroy(c2port_class);
}

module_init(c2port_init);
module_exit(c2port_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("Silicon Labs C2 port support v. " DRIVER_VERSION);
MODULE_LICENSE("GPL");
