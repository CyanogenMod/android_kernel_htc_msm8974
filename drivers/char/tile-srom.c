/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 * SPI Flash ROM driver
 *
 * This source code is derived from code provided in "Linux Device
 * Drivers, Third Edition", by Jonathan Corbet, Alessandro Rubini, and
 * Greg Kroah-Hartman, published by O'Reilly Media, Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>	
#include <linux/slab.h>		
#include <linux/fs.h>		
#include <linux/errno.h>	
#include <linux/types.h>	
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	
#include <linux/aio.h>
#include <linux/pagemap.h>
#include <linux/hugetlb.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <hv/hypervisor.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <hv/drv_srom_intf.h>

#define SROM_CHUNK_SIZE ((size_t)4096)


#define SROM_WAIT_TRY_INTERVAL 20

#define SROM_MAX_WAIT_TRY_TIMES 1000

struct srom_dev {
	int hv_devhdl;			
	u32 total_size;			
	u32 sector_size;		
	u32 page_size;			
	struct mutex lock;		
};

static int srom_major;			
module_param(srom_major, int, 0);
MODULE_AUTHOR("Tilera Corporation");
MODULE_LICENSE("GPL");

static int srom_devs;			
static struct cdev srom_cdev;
static struct class *srom_class;
static struct srom_dev *srom_devices;


static ssize_t _srom_read(int hv_devhdl, void *buf,
			  loff_t off, size_t count)
{
	int retval, retries = SROM_MAX_WAIT_TRY_TIMES;
	for (;;) {
		retval = hv_dev_pread(hv_devhdl, 0, (HV_VirtAddr)buf,
				      count, off);
		if (retval >= 0)
			return retval;
		if (retval == HV_EAGAIN)
			continue;
		if (retval == HV_EBUSY && --retries > 0) {
			msleep(SROM_WAIT_TRY_INTERVAL);
			continue;
		}
		pr_err("_srom_read: error %d\n", retval);
		return -EIO;
	}
}

static ssize_t _srom_write(int hv_devhdl, const void *buf,
			   loff_t off, size_t count)
{
	int retval, retries = SROM_MAX_WAIT_TRY_TIMES;
	for (;;) {
		retval = hv_dev_pwrite(hv_devhdl, 0, (HV_VirtAddr)buf,
				       count, off);
		if (retval >= 0)
			return retval;
		if (retval == HV_EAGAIN)
			continue;
		if (retval == HV_EBUSY && --retries > 0) {
			msleep(SROM_WAIT_TRY_INTERVAL);
			continue;
		}
		pr_err("_srom_write: error %d\n", retval);
		return -EIO;
	}
}

static int srom_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &srom_devices[iminor(inode)];
	return 0;
}


static int srom_release(struct inode *inode, struct file *filp)
{
	struct srom_dev *srom = filp->private_data;
	char dummy;

	/* Make sure we've flushed anything written to the ROM. */
	mutex_lock(&srom->lock);
	if (srom->hv_devhdl >= 0)
		_srom_write(srom->hv_devhdl, &dummy, SROM_FLUSH_OFF, 1);
	mutex_unlock(&srom->lock);

	filp->private_data = NULL;

	return 0;
}


static ssize_t srom_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *f_pos)
{
	int retval = 0;
	void *kernbuf;
	struct srom_dev *srom = filp->private_data;

	kernbuf = kmalloc(SROM_CHUNK_SIZE, GFP_KERNEL);
	if (!kernbuf)
		return -ENOMEM;

	if (mutex_lock_interruptible(&srom->lock)) {
		retval = -ERESTARTSYS;
		kfree(kernbuf);
		return retval;
	}

	while (count) {
		int hv_retval;
		int bytes_this_pass = min(count, SROM_CHUNK_SIZE);

		hv_retval = _srom_read(srom->hv_devhdl, kernbuf,
				       *f_pos, bytes_this_pass);
		if (hv_retval <= 0) {
			if (retval == 0)
				retval = hv_retval;
			break;
		}

		if (copy_to_user(buf, kernbuf, hv_retval) != 0) {
			retval = -EFAULT;
			break;
		}

		retval += hv_retval;
		*f_pos += hv_retval;
		buf += hv_retval;
		count -= hv_retval;
	}

	mutex_unlock(&srom->lock);
	kfree(kernbuf);

	return retval;
}

/**
 * srom_write() - Write data to the device.
 * @filp: File for this specific open of the device.
 * @buf: User's data buffer.
 * @count: Number of bytes requested.
 * @f_pos: File position.
 *
 * Returns number of bytes written, or an error code.
 */
static ssize_t srom_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	int retval = 0;
	void *kernbuf;
	struct srom_dev *srom = filp->private_data;

	kernbuf = kmalloc(SROM_CHUNK_SIZE, GFP_KERNEL);
	if (!kernbuf)
		return -ENOMEM;

	if (mutex_lock_interruptible(&srom->lock)) {
		retval = -ERESTARTSYS;
		kfree(kernbuf);
		return retval;
	}

	while (count) {
		int hv_retval;
		int bytes_this_pass = min(count, SROM_CHUNK_SIZE);

		if (copy_from_user(kernbuf, buf, bytes_this_pass) != 0) {
			retval = -EFAULT;
			break;
		}

		hv_retval = _srom_write(srom->hv_devhdl, kernbuf,
					*f_pos, bytes_this_pass);
		if (hv_retval <= 0) {
			if (retval == 0)
				retval = hv_retval;
			break;
		}

		retval += hv_retval;
		*f_pos += hv_retval;
		buf += hv_retval;
		count -= hv_retval;
	}

	mutex_unlock(&srom->lock);
	kfree(kernbuf);

	return retval;
}

loff_t srom_llseek(struct file *filp, loff_t offset, int origin)
{
	struct srom_dev *srom = filp->private_data;

	if (mutex_lock_interruptible(&srom->lock))
		return -ERESTARTSYS;

	switch (origin) {
	case SEEK_END:
		offset += srom->total_size;
		break;
	case SEEK_CUR:
		offset += filp->f_pos;
		break;
	}

	if (offset < 0 || offset > srom->total_size) {
		offset = -EINVAL;
	} else {
		filp->f_pos = offset;
		filp->f_version = 0;
	}

	mutex_unlock(&srom->lock);

	return offset;
}

static ssize_t total_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct srom_dev *srom = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", srom->total_size);
}

static ssize_t sector_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct srom_dev *srom = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", srom->sector_size);
}

static ssize_t page_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct srom_dev *srom = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", srom->page_size);
}

static struct device_attribute srom_dev_attrs[] = {
	__ATTR(total_size, S_IRUGO, total_show, NULL),
	__ATTR(sector_size, S_IRUGO, sector_show, NULL),
	__ATTR(page_size, S_IRUGO, page_show, NULL),
	__ATTR_NULL
};

static char *srom_devnode(struct device *dev, umode_t *mode)
{
	*mode = S_IRUGO | S_IWUSR;
	return kasprintf(GFP_KERNEL, "srom/%s", dev_name(dev));
}

static const struct file_operations srom_fops = {
	.owner =     THIS_MODULE,
	.llseek =    srom_llseek,
	.read =	     srom_read,
	.write =     srom_write,
	.open =	     srom_open,
	.release =   srom_release,
};

static int srom_setup_minor(struct srom_dev *srom, int index)
{
	struct device *dev;
	int devhdl = srom->hv_devhdl;

	mutex_init(&srom->lock);

	if (_srom_read(devhdl, &srom->total_size,
		       SROM_TOTAL_SIZE_OFF, sizeof(srom->total_size)) < 0)
		return -EIO;
	if (_srom_read(devhdl, &srom->sector_size,
		       SROM_SECTOR_SIZE_OFF, sizeof(srom->sector_size)) < 0)
		return -EIO;
	if (_srom_read(devhdl, &srom->page_size,
		       SROM_PAGE_SIZE_OFF, sizeof(srom->page_size)) < 0)
		return -EIO;

	dev = device_create(srom_class, &platform_bus,
			    MKDEV(srom_major, index), srom, "%d", index);
	return IS_ERR(dev) ? PTR_ERR(dev) : 0;
}

static int srom_init(void)
{
	int result, i;
	dev_t dev = MKDEV(srom_major, 0);

	srom_devices = kzalloc(4 * sizeof(struct srom_dev), GFP_KERNEL);

	
	for (i = 0; ; i++) {
		int devhdl;
		char buf[20];
		struct srom_dev *new_srom_devices =
			krealloc(srom_devices, (i+1) * sizeof(struct srom_dev),
				 GFP_KERNEL | __GFP_ZERO);
		if (!new_srom_devices) {
			result = -ENOMEM;
			goto fail_mem;
		}
		srom_devices = new_srom_devices;
		sprintf(buf, "srom/0/%d", i);
		devhdl = hv_dev_open((HV_VirtAddr)buf, 0);
		if (devhdl < 0) {
			if (devhdl != HV_ENODEV)
				pr_notice("srom/%d: hv_dev_open failed: %d.\n",
					  i, devhdl);
			break;
		}
		srom_devices[i].hv_devhdl = devhdl;
	}
	srom_devs = i;

	
	if (srom_devs == 0) {
		result = -ENODEV;
		goto fail_mem;
	}

	
	if (srom_major)
		result = register_chrdev_region(dev, srom_devs, "srom");
	else {
		result = alloc_chrdev_region(&dev, 0, srom_devs, "srom");
		srom_major = MAJOR(dev);
	}
	if (result < 0)
		goto fail_mem;

	
	cdev_init(&srom_cdev, &srom_fops);
	srom_cdev.owner = THIS_MODULE;
	srom_cdev.ops = &srom_fops;
	result = cdev_add(&srom_cdev, dev, srom_devs);
	if (result < 0)
		goto fail_chrdev;

	
	srom_class = class_create(THIS_MODULE, "srom");
	if (IS_ERR(srom_class)) {
		result = PTR_ERR(srom_class);
		goto fail_cdev;
	}
	srom_class->dev_attrs = srom_dev_attrs;
	srom_class->devnode = srom_devnode;

	
	for (i = 0; i < srom_devs; i++) {
		result = srom_setup_minor(srom_devices + i, i);
		if (result < 0)
			goto fail_class;
	}

	return 0;

fail_class:
	for (i = 0; i < srom_devs; i++)
		device_destroy(srom_class, MKDEV(srom_major, i));
	class_destroy(srom_class);
fail_cdev:
	cdev_del(&srom_cdev);
fail_chrdev:
	unregister_chrdev_region(dev, srom_devs);
fail_mem:
	kfree(srom_devices);
	return result;
}

static void srom_cleanup(void)
{
	int i;
	for (i = 0; i < srom_devs; i++)
		device_destroy(srom_class, MKDEV(srom_major, i));
	class_destroy(srom_class);
	cdev_del(&srom_cdev);
	unregister_chrdev_region(MKDEV(srom_major, 0), srom_devs);
	kfree(srom_devices);
}

module_init(srom_init);
module_exit(srom_cleanup);
