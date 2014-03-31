/*
 * Linux driver for System z and s390 unit record devices
 * (z/VM virtual punch, reader, printer)
 *
 * Copyright IBM Corp. 2001, 2009
 * Authors: Malcolm Beattie <beattiem@uk.ibm.com>
 *	    Michael Holzheu <holzheu@de.ibm.com>
 *	    Frank Munzert <munzert@de.ibm.com>
 */

#define KMSG_COMPONENT "vmur"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/cio.h>
#include <asm/ccwdev.h>
#include <asm/debug.h>
#include <asm/diag.h>

#include "vmur.h"


static char ur_banner[] = "z/VM virtual unit record device driver";

MODULE_AUTHOR("IBM Corporation");
MODULE_DESCRIPTION("s390 z/VM virtual unit record device driver");
MODULE_LICENSE("GPL");

static dev_t ur_first_dev_maj_min;
static struct class *vmur_class;
static struct debug_info *vmur_dbf;

static struct ccw_device_id ur_ids[] = {
	{ CCWDEV_CU_DI(READER_PUNCH_DEVTYPE, 80) },
	{ CCWDEV_CU_DI(PRINTER_DEVTYPE, 132) },
	{  }
};

MODULE_DEVICE_TABLE(ccw, ur_ids);

static int ur_probe(struct ccw_device *cdev);
static void ur_remove(struct ccw_device *cdev);
static int ur_set_online(struct ccw_device *cdev);
static int ur_set_offline(struct ccw_device *cdev);
static int ur_pm_suspend(struct ccw_device *cdev);

static struct ccw_driver ur_driver = {
	.driver = {
		.name	= "vmur",
		.owner	= THIS_MODULE,
	},
	.ids		= ur_ids,
	.probe		= ur_probe,
	.remove		= ur_remove,
	.set_online	= ur_set_online,
	.set_offline	= ur_set_offline,
	.freeze		= ur_pm_suspend,
	.int_class	= IOINT_VMR,
};

static DEFINE_MUTEX(vmur_mutex);

static struct urdev *urdev_alloc(struct ccw_device *cdev)
{
	struct urdev *urd;

	urd = kzalloc(sizeof(struct urdev), GFP_KERNEL);
	if (!urd)
		return NULL;
	urd->reclen = cdev->id.driver_info;
	ccw_device_get_id(cdev, &urd->dev_id);
	mutex_init(&urd->io_mutex);
	init_waitqueue_head(&urd->wait);
	spin_lock_init(&urd->open_lock);
	atomic_set(&urd->ref_count,  1);
	urd->cdev = cdev;
	get_device(&cdev->dev);
	return urd;
}

static void urdev_free(struct urdev *urd)
{
	TRACE("urdev_free: %p\n", urd);
	if (urd->cdev)
		put_device(&urd->cdev->dev);
	kfree(urd);
}

static void urdev_get(struct urdev *urd)
{
	atomic_inc(&urd->ref_count);
}

static struct urdev *urdev_get_from_cdev(struct ccw_device *cdev)
{
	struct urdev *urd;
	unsigned long flags;

	spin_lock_irqsave(get_ccwdev_lock(cdev), flags);
	urd = dev_get_drvdata(&cdev->dev);
	if (urd)
		urdev_get(urd);
	spin_unlock_irqrestore(get_ccwdev_lock(cdev), flags);
	return urd;
}

static struct urdev *urdev_get_from_devno(u16 devno)
{
	char bus_id[16];
	struct ccw_device *cdev;
	struct urdev *urd;

	sprintf(bus_id, "0.0.%04x", devno);
	cdev = get_ccwdev_by_busid(&ur_driver, bus_id);
	if (!cdev)
		return NULL;
	urd = urdev_get_from_cdev(cdev);
	put_device(&cdev->dev);
	return urd;
}

static void urdev_put(struct urdev *urd)
{
	if (atomic_dec_and_test(&urd->ref_count))
		urdev_free(urd);
}

static int ur_pm_suspend(struct ccw_device *cdev)
{
	struct urdev *urd = dev_get_drvdata(&cdev->dev);

	TRACE("ur_pm_suspend: cdev=%p\n", cdev);
	if (urd->open_flag) {
		pr_err("Unit record device %s is busy, %s refusing to "
		       "suspend.\n", dev_name(&cdev->dev), ur_banner);
		return -EBUSY;
	}
	return 0;
}


static void free_chan_prog(struct ccw1 *cpa)
{
	struct ccw1 *ptr = cpa;

	while (ptr->cda) {
		kfree((void *)(addr_t) ptr->cda);
		ptr++;
	}
	kfree(cpa);
}

static struct ccw1 *alloc_chan_prog(const char __user *ubuf, int rec_count,
				    int reclen)
{
	struct ccw1 *cpa;
	void *kbuf;
	int i;

	TRACE("alloc_chan_prog(%p, %i, %i)\n", ubuf, rec_count, reclen);

	cpa = kzalloc((rec_count + 1) * sizeof(struct ccw1),
		      GFP_KERNEL | GFP_DMA);
	if (!cpa)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < rec_count; i++) {
		cpa[i].cmd_code = WRITE_CCW_CMD;
		cpa[i].flags = CCW_FLAG_CC | CCW_FLAG_SLI;
		cpa[i].count = reclen;
		kbuf = kmalloc(reclen, GFP_KERNEL | GFP_DMA);
		if (!kbuf) {
			free_chan_prog(cpa);
			return ERR_PTR(-ENOMEM);
		}
		cpa[i].cda = (u32)(addr_t) kbuf;
		if (copy_from_user(kbuf, ubuf, reclen)) {
			free_chan_prog(cpa);
			return ERR_PTR(-EFAULT);
		}
		ubuf += reclen;
	}
	
	cpa[i].cmd_code = CCW_CMD_NOOP;
	return cpa;
}

static int do_ur_io(struct urdev *urd, struct ccw1 *cpa)
{
	int rc;
	struct ccw_device *cdev = urd->cdev;
	DECLARE_COMPLETION_ONSTACK(event);

	TRACE("do_ur_io: cpa=%p\n", cpa);

	rc = mutex_lock_interruptible(&urd->io_mutex);
	if (rc)
		return rc;

	urd->io_done = &event;

	spin_lock_irq(get_ccwdev_lock(cdev));
	rc = ccw_device_start(cdev, cpa, 1, 0, 0);
	spin_unlock_irq(get_ccwdev_lock(cdev));

	TRACE("do_ur_io: ccw_device_start returned %d\n", rc);
	if (rc)
		goto out;

	wait_for_completion(&event);
	TRACE("do_ur_io: I/O complete\n");
	rc = 0;

out:
	mutex_unlock(&urd->io_mutex);
	return rc;
}

static void ur_int_handler(struct ccw_device *cdev, unsigned long intparm,
			   struct irb *irb)
{
	struct urdev *urd;

	TRACE("ur_int_handler: intparm=0x%lx cstat=%02x dstat=%02x res=%u\n",
	      intparm, irb->scsw.cmd.cstat, irb->scsw.cmd.dstat,
	      irb->scsw.cmd.count);

	if (!intparm) {
		TRACE("ur_int_handler: unsolicited interrupt\n");
		return;
	}
	urd = dev_get_drvdata(&cdev->dev);
	BUG_ON(!urd);
	
	if (IS_ERR(irb))
		urd->io_request_rc = PTR_ERR(irb);
	else if (irb->scsw.cmd.dstat == (DEV_STAT_CHN_END | DEV_STAT_DEV_END))
		urd->io_request_rc = 0;
	else
		urd->io_request_rc = -EIO;

	complete(urd->io_done);
}

static ssize_t ur_attr_reclen_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct urdev *urd;
	int rc;

	urd = urdev_get_from_cdev(to_ccwdev(dev));
	if (!urd)
		return -ENODEV;
	rc = sprintf(buf, "%zu\n", urd->reclen);
	urdev_put(urd);
	return rc;
}

static DEVICE_ATTR(reclen, 0444, ur_attr_reclen_show, NULL);

static int ur_create_attributes(struct device *dev)
{
	return device_create_file(dev, &dev_attr_reclen);
}

static void ur_remove_attributes(struct device *dev)
{
	device_remove_file(dev, &dev_attr_reclen);
}

static int get_urd_class(struct urdev *urd)
{
	static struct diag210 ur_diag210;
	int cc;

	ur_diag210.vrdcdvno = urd->dev_id.devno;
	ur_diag210.vrdclen = sizeof(struct diag210);

	cc = diag210(&ur_diag210);
	switch (cc) {
	case 0:
		return -EOPNOTSUPP;
	case 2:
		return ur_diag210.vrdcvcla; 
	case 3:
		return -ENODEV;
	default:
		return -EIO;
	}
}

static struct urfile *urfile_alloc(struct urdev *urd)
{
	struct urfile *urf;

	urf = kzalloc(sizeof(struct urfile), GFP_KERNEL);
	if (!urf)
		return NULL;
	urf->urd = urd;

	TRACE("urfile_alloc: urd=%p urf=%p rl=%zu\n", urd, urf,
	      urf->dev_reclen);

	return urf;
}

static void urfile_free(struct urfile *urf)
{
	TRACE("urfile_free: urf=%p urd=%p\n", urf, urf->urd);
	kfree(urf);
}

static ssize_t do_write(struct urdev *urd, const char __user *udata,
			size_t count, size_t reclen, loff_t *ppos)
{
	struct ccw1 *cpa;
	int rc;

	cpa = alloc_chan_prog(udata, count / reclen, reclen);
	if (IS_ERR(cpa))
		return PTR_ERR(cpa);

	rc = do_ur_io(urd, cpa);
	if (rc)
		goto fail_kfree_cpa;

	if (urd->io_request_rc) {
		rc = urd->io_request_rc;
		goto fail_kfree_cpa;
	}
	*ppos += count;
	rc = count;

fail_kfree_cpa:
	free_chan_prog(cpa);
	return rc;
}

static ssize_t ur_write(struct file *file, const char __user *udata,
			size_t count, loff_t *ppos)
{
	struct urfile *urf = file->private_data;

	TRACE("ur_write: count=%zu\n", count);

	if (count == 0)
		return 0;

	if (count % urf->dev_reclen)
		return -EINVAL;	

	if (count > urf->dev_reclen * MAX_RECS_PER_IO)
		count = urf->dev_reclen * MAX_RECS_PER_IO;

	return do_write(urf->urd, udata, count, urf->dev_reclen, ppos);
}

static int diag_position_to_record(int devno, int record)
{
	int cc;

	cc = diag14(record, devno, 0x28);
	switch (cc) {
	case 0:
		return 0;
	case 2:
		return -ENOMEDIUM;
	case 3:
		return -ENODATA; 
	default:
		return -EIO;
	}
}

static int diag_read_file(int devno, char *buf)
{
	int cc;

	cc = diag14((unsigned long) buf, devno, 0x00);
	switch (cc) {
	case 0:
		return 0;
	case 1:
		return -ENODATA;
	case 2:
		return -ENOMEDIUM;
	default:
		return -EIO;
	}
}

static ssize_t diag14_read(struct file *file, char __user *ubuf, size_t count,
			   loff_t *offs)
{
	size_t len, copied, res;
	char *buf;
	int rc;
	u16 reclen;
	struct urdev *urd;

	urd = ((struct urfile *) file->private_data)->urd;
	reclen = ((struct urfile *) file->private_data)->file_reclen;

	rc = diag_position_to_record(urd->dev_id.devno, *offs / PAGE_SIZE + 1);
	if (rc == -ENODATA)
		return 0;
	if (rc)
		return rc;

	len = min((size_t) PAGE_SIZE, count);
	buf = (char *) __get_free_page(GFP_KERNEL | GFP_DMA);
	if (!buf)
		return -ENOMEM;

	copied = 0;
	res = (size_t) (*offs % PAGE_SIZE);
	do {
		rc = diag_read_file(urd->dev_id.devno, buf);
		if (rc == -ENODATA) {
			break;
		}
		if (rc)
			goto fail;
		if (reclen && (copied == 0) && (*offs < PAGE_SIZE))
			*((u16 *) &buf[FILE_RECLEN_OFFSET]) = reclen;
		len = min(count - copied, PAGE_SIZE - res);
		if (copy_to_user(ubuf + copied, buf + res, len)) {
			rc = -EFAULT;
			goto fail;
		}
		res = 0;
		copied += len;
	} while (copied != count);

	*offs += copied;
	rc = copied;
fail:
	free_page((unsigned long) buf);
	return rc;
}

static ssize_t ur_read(struct file *file, char __user *ubuf, size_t count,
		       loff_t *offs)
{
	struct urdev *urd;
	int rc;

	TRACE("ur_read: count=%zu ppos=%li\n", count, (unsigned long) *offs);

	if (count == 0)
		return 0;

	urd = ((struct urfile *) file->private_data)->urd;
	rc = mutex_lock_interruptible(&urd->io_mutex);
	if (rc)
		return rc;
	rc = diag14_read(file, ubuf, count, offs);
	mutex_unlock(&urd->io_mutex);
	return rc;
}

static int diag_read_next_file_info(struct file_control_block *buf, int spid)
{
	int cc;

	cc = diag14((unsigned long) buf, spid, 0xfff);
	switch (cc) {
	case 0:
		return 0;
	default:
		return -ENODATA;
	}
}

static int verify_uri_device(struct urdev *urd)
{
	struct file_control_block *fcb;
	char *buf;
	int rc;

	fcb = kmalloc(sizeof(*fcb), GFP_KERNEL | GFP_DMA);
	if (!fcb)
		return -ENOMEM;

	
	rc = diag_read_next_file_info(fcb, 0);
	if (rc)
		goto fail_free_fcb;

	
	if (fcb->file_stat & (FLG_SYSTEM_HOLD | FLG_USER_HOLD)) {
		rc = -EPERM;
		goto fail_free_fcb;
	}

	
	buf = (char *) __get_free_page(GFP_KERNEL | GFP_DMA);
	if (!buf) {
		rc = -ENOMEM;
		goto fail_free_fcb;
	}
	rc = diag_read_file(urd->dev_id.devno, buf);
	if ((rc != 0) && (rc != -ENODATA)) 
		goto fail_free_buf;

	
	rc = diag_read_next_file_info(fcb, 0);
	if (rc)
		goto fail_free_buf;
	if (!(fcb->file_stat & FLG_IN_USE)) {
		rc = -EMFILE;
		goto fail_free_buf;
	}
	rc = 0;

fail_free_buf:
	free_page((unsigned long) buf);
fail_free_fcb:
	kfree(fcb);
	return rc;
}

static int verify_device(struct urdev *urd)
{
	switch (urd->class) {
	case DEV_CLASS_UR_O:
		return 0; 
	case DEV_CLASS_UR_I:
		return verify_uri_device(urd);
	default:
		return -EOPNOTSUPP;
	}
}

static int get_uri_file_reclen(struct urdev *urd)
{
	struct file_control_block *fcb;
	int rc;

	fcb = kmalloc(sizeof(*fcb), GFP_KERNEL | GFP_DMA);
	if (!fcb)
		return -ENOMEM;
	rc = diag_read_next_file_info(fcb, 0);
	if (rc)
		goto fail_free;
	if (fcb->file_stat & FLG_CP_DUMP)
		rc = 0;
	else
		rc = fcb->rec_len;

fail_free:
	kfree(fcb);
	return rc;
}

static int get_file_reclen(struct urdev *urd)
{
	switch (urd->class) {
	case DEV_CLASS_UR_O:
		return 0;
	case DEV_CLASS_UR_I:
		return get_uri_file_reclen(urd);
	default:
		return -EOPNOTSUPP;
	}
}

static int ur_open(struct inode *inode, struct file *file)
{
	u16 devno;
	struct urdev *urd;
	struct urfile *urf;
	unsigned short accmode;
	int rc;

	accmode = file->f_flags & O_ACCMODE;

	if (accmode == O_RDWR)
		return -EACCES;
	devno = MINOR(file->f_dentry->d_inode->i_rdev);

	urd = urdev_get_from_devno(devno);
	if (!urd) {
		rc = -ENXIO;
		goto out;
	}

	spin_lock(&urd->open_lock);
	while (urd->open_flag) {
		spin_unlock(&urd->open_lock);
		if (file->f_flags & O_NONBLOCK) {
			rc = -EBUSY;
			goto fail_put;
		}
		if (wait_event_interruptible(urd->wait, urd->open_flag == 0)) {
			rc = -ERESTARTSYS;
			goto fail_put;
		}
		spin_lock(&urd->open_lock);
	}
	urd->open_flag++;
	spin_unlock(&urd->open_lock);

	TRACE("ur_open\n");

	if (((accmode == O_RDONLY) && (urd->class != DEV_CLASS_UR_I)) ||
	    ((accmode == O_WRONLY) && (urd->class != DEV_CLASS_UR_O))) {
		TRACE("ur_open: unsupported dev class (%d)\n", urd->class);
		rc = -EACCES;
		goto fail_unlock;
	}

	rc = verify_device(urd);
	if (rc)
		goto fail_unlock;

	urf = urfile_alloc(urd);
	if (!urf) {
		rc = -ENOMEM;
		goto fail_unlock;
	}

	urf->dev_reclen = urd->reclen;
	rc = get_file_reclen(urd);
	if (rc < 0)
		goto fail_urfile_free;
	urf->file_reclen = rc;
	file->private_data = urf;
	return 0;

fail_urfile_free:
	urfile_free(urf);
fail_unlock:
	spin_lock(&urd->open_lock);
	urd->open_flag--;
	spin_unlock(&urd->open_lock);
fail_put:
	urdev_put(urd);
out:
	return rc;
}

static int ur_release(struct inode *inode, struct file *file)
{
	struct urfile *urf = file->private_data;

	TRACE("ur_release\n");
	spin_lock(&urf->urd->open_lock);
	urf->urd->open_flag--;
	spin_unlock(&urf->urd->open_lock);
	wake_up_interruptible(&urf->urd->wait);
	urdev_put(urf->urd);
	urfile_free(urf);
	return 0;
}

static loff_t ur_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t newpos;

	if ((file->f_flags & O_ACCMODE) != O_RDONLY)
		return -ESPIPE; 
	if (offset % PAGE_SIZE)
		return -ESPIPE; 
	switch (whence) {
	case 0: 
		newpos = offset;
		break;
	case 1: 
		newpos = file->f_pos + offset;
		break;
	default:
		return -EINVAL;
	}
	file->f_pos = newpos;
	return newpos;
}

static const struct file_operations ur_fops = {
	.owner	 = THIS_MODULE,
	.open	 = ur_open,
	.release = ur_release,
	.read	 = ur_read,
	.write	 = ur_write,
	.llseek  = ur_llseek,
};

static int ur_probe(struct ccw_device *cdev)
{
	struct urdev *urd;
	int rc;

	TRACE("ur_probe: cdev=%p\n", cdev);

	mutex_lock(&vmur_mutex);
	urd = urdev_alloc(cdev);
	if (!urd) {
		rc = -ENOMEM;
		goto fail_unlock;
	}

	rc = ur_create_attributes(&cdev->dev);
	if (rc) {
		rc = -ENOMEM;
		goto fail_urdev_put;
	}
	cdev->handler = ur_int_handler;

	
	urd->class = get_urd_class(urd);
	if (urd->class < 0) {
		rc = urd->class;
		goto fail_remove_attr;
	}
	if ((urd->class != DEV_CLASS_UR_I) && (urd->class != DEV_CLASS_UR_O)) {
		rc = -EOPNOTSUPP;
		goto fail_remove_attr;
	}
	spin_lock_irq(get_ccwdev_lock(cdev));
	dev_set_drvdata(&cdev->dev, urd);
	spin_unlock_irq(get_ccwdev_lock(cdev));

	mutex_unlock(&vmur_mutex);
	return 0;

fail_remove_attr:
	ur_remove_attributes(&cdev->dev);
fail_urdev_put:
	urdev_put(urd);
fail_unlock:
	mutex_unlock(&vmur_mutex);
	return rc;
}

static int ur_set_online(struct ccw_device *cdev)
{
	struct urdev *urd;
	int minor, major, rc;
	char node_id[16];

	TRACE("ur_set_online: cdev=%p\n", cdev);

	mutex_lock(&vmur_mutex);
	urd = urdev_get_from_cdev(cdev);
	if (!urd) {
		
		rc = -ENODEV;
		goto fail_unlock;
	}

	if (urd->char_device) {
		
		rc = -EBUSY;
		goto fail_urdev_put;
	}

	minor = urd->dev_id.devno;
	major = MAJOR(ur_first_dev_maj_min);

	urd->char_device = cdev_alloc();
	if (!urd->char_device) {
		rc = -ENOMEM;
		goto fail_urdev_put;
	}

	urd->char_device->ops = &ur_fops;
	urd->char_device->dev = MKDEV(major, minor);
	urd->char_device->owner = ur_fops.owner;

	rc = cdev_add(urd->char_device, urd->char_device->dev, 1);
	if (rc)
		goto fail_free_cdev;
	if (urd->cdev->id.cu_type == READER_PUNCH_DEVTYPE) {
		if (urd->class == DEV_CLASS_UR_I)
			sprintf(node_id, "vmrdr-%s", dev_name(&cdev->dev));
		if (urd->class == DEV_CLASS_UR_O)
			sprintf(node_id, "vmpun-%s", dev_name(&cdev->dev));
	} else if (urd->cdev->id.cu_type == PRINTER_DEVTYPE) {
		sprintf(node_id, "vmprt-%s", dev_name(&cdev->dev));
	} else {
		rc = -EOPNOTSUPP;
		goto fail_free_cdev;
	}

	urd->device = device_create(vmur_class, NULL, urd->char_device->dev,
				    NULL, "%s", node_id);
	if (IS_ERR(urd->device)) {
		rc = PTR_ERR(urd->device);
		TRACE("ur_set_online: device_create rc=%d\n", rc);
		goto fail_free_cdev;
	}
	urdev_put(urd);
	mutex_unlock(&vmur_mutex);
	return 0;

fail_free_cdev:
	cdev_del(urd->char_device);
	urd->char_device = NULL;
fail_urdev_put:
	urdev_put(urd);
fail_unlock:
	mutex_unlock(&vmur_mutex);
	return rc;
}

static int ur_set_offline_force(struct ccw_device *cdev, int force)
{
	struct urdev *urd;
	int rc;

	TRACE("ur_set_offline: cdev=%p\n", cdev);
	urd = urdev_get_from_cdev(cdev);
	if (!urd)
		
		return -ENODEV;
	if (!urd->char_device) {
		
		rc = -EBUSY;
		goto fail_urdev_put;
	}
	if (!force && (atomic_read(&urd->ref_count) > 2)) {
		
		TRACE("ur_set_offline: BUSY\n");
		rc = -EBUSY;
		goto fail_urdev_put;
	}
	device_destroy(vmur_class, urd->char_device->dev);
	cdev_del(urd->char_device);
	urd->char_device = NULL;
	rc = 0;

fail_urdev_put:
	urdev_put(urd);
	return rc;
}

static int ur_set_offline(struct ccw_device *cdev)
{
	int rc;

	mutex_lock(&vmur_mutex);
	rc = ur_set_offline_force(cdev, 0);
	mutex_unlock(&vmur_mutex);
	return rc;
}

static void ur_remove(struct ccw_device *cdev)
{
	unsigned long flags;

	TRACE("ur_remove\n");

	mutex_lock(&vmur_mutex);

	if (cdev->online)
		ur_set_offline_force(cdev, 1);
	ur_remove_attributes(&cdev->dev);

	spin_lock_irqsave(get_ccwdev_lock(cdev), flags);
	urdev_put(dev_get_drvdata(&cdev->dev));
	dev_set_drvdata(&cdev->dev, NULL);
	spin_unlock_irqrestore(get_ccwdev_lock(cdev), flags);

	mutex_unlock(&vmur_mutex);
}

static int __init ur_init(void)
{
	int rc;
	dev_t dev;

	if (!MACHINE_IS_VM) {
		pr_err("The %s cannot be loaded without z/VM\n",
		       ur_banner);
		return -ENODEV;
	}

	vmur_dbf = debug_register("vmur", 4, 1, 4 * sizeof(long));
	if (!vmur_dbf)
		return -ENOMEM;
	rc = debug_register_view(vmur_dbf, &debug_sprintf_view);
	if (rc)
		goto fail_free_dbf;

	debug_set_level(vmur_dbf, 6);

	vmur_class = class_create(THIS_MODULE, "vmur");
	if (IS_ERR(vmur_class)) {
		rc = PTR_ERR(vmur_class);
		goto fail_free_dbf;
	}

	rc = ccw_driver_register(&ur_driver);
	if (rc)
		goto fail_class_destroy;

	rc = alloc_chrdev_region(&dev, 0, NUM_MINORS, "vmur");
	if (rc) {
		pr_err("Kernel function alloc_chrdev_region failed with "
		       "error code %d\n", rc);
		goto fail_unregister_driver;
	}
	ur_first_dev_maj_min = MKDEV(MAJOR(dev), 0);

	pr_info("%s loaded.\n", ur_banner);
	return 0;

fail_unregister_driver:
	ccw_driver_unregister(&ur_driver);
fail_class_destroy:
	class_destroy(vmur_class);
fail_free_dbf:
	debug_unregister(vmur_dbf);
	return rc;
}

static void __exit ur_exit(void)
{
	unregister_chrdev_region(ur_first_dev_maj_min, NUM_MINORS);
	ccw_driver_unregister(&ur_driver);
	class_destroy(vmur_class);
	debug_unregister(vmur_dbf);
	pr_info("%s unloaded.\n", ur_banner);
}

module_init(ur_init);
module_exit(ur_exit);
