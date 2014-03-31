/*
 *  drivers/s390/cio/device.c
 *  bus driver for ccw devices
 *
 *    Copyright IBM Corp. 2002,2008
 *    Author(s): Arnd Bergmann (arndb@de.ibm.com)
 *		 Cornelia Huck (cornelia.huck@de.ibm.com)
 *		 Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#define KMSG_COMPONENT "cio"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/kernel_stat.h>

#include <asm/ccwdev.h>
#include <asm/cio.h>
#include <asm/param.h>		
#include <asm/cmb.h>
#include <asm/isc.h>

#include "chp.h"
#include "cio.h"
#include "cio_debug.h"
#include "css.h"
#include "device.h"
#include "ioasm.h"
#include "io_sch.h"
#include "blacklist.h"
#include "chsc.h"

static struct timer_list recovery_timer;
static DEFINE_SPINLOCK(recovery_lock);
static int recovery_phase;
static const unsigned long recovery_delay[] = { 3, 30, 300 };


static int
ccw_bus_match (struct device * dev, struct device_driver * drv)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_driver *cdrv = to_ccwdrv(drv);
	const struct ccw_device_id *ids = cdrv->ids, *found;

	if (!ids)
		return 0;

	found = ccw_device_id_match(ids, &cdev->id);
	if (!found)
		return 0;

	cdev->id.driver_info = found->driver_info;

	return 1;
}

static int snprint_alias(char *buf, size_t size,
			 struct ccw_device_id *id, const char *suffix)
{
	int len;

	len = snprintf(buf, size, "ccw:t%04Xm%02X", id->cu_type, id->cu_model);
	if (len > size)
		return len;
	buf += len;
	size -= len;

	if (id->dev_type != 0)
		len += snprintf(buf, size, "dt%04Xdm%02X%s", id->dev_type,
				id->dev_model, suffix);
	else
		len += snprintf(buf, size, "dtdm%s", suffix);

	return len;
}

static int ccw_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_device_id *id = &(cdev->id);
	int ret;
	char modalias_buf[30];

	
	ret = add_uevent_var(env, "CU_TYPE=%04X", id->cu_type);
	if (ret)
		return ret;

	
	ret = add_uevent_var(env, "CU_MODEL=%02X", id->cu_model);
	if (ret)
		return ret;

	
	
	ret = add_uevent_var(env, "DEV_TYPE=%04X", id->dev_type);
	if (ret)
		return ret;

	
	ret = add_uevent_var(env, "DEV_MODEL=%02X", id->dev_model);
	if (ret)
		return ret;

	
	snprint_alias(modalias_buf, sizeof(modalias_buf), id, "");
	ret = add_uevent_var(env, "MODALIAS=%s", modalias_buf);
	return ret;
}

static struct bus_type ccw_bus_type;

static void io_subchannel_irq(struct subchannel *);
static int io_subchannel_probe(struct subchannel *);
static int io_subchannel_remove(struct subchannel *);
static void io_subchannel_shutdown(struct subchannel *);
static int io_subchannel_sch_event(struct subchannel *, int);
static int io_subchannel_chp_event(struct subchannel *, struct chp_link *,
				   int);
static void recovery_func(unsigned long data);
wait_queue_head_t ccw_device_init_wq;
atomic_t ccw_device_init_count;

static struct css_device_id io_subchannel_ids[] = {
	{ .match_flags = 0x1, .type = SUBCHANNEL_TYPE_IO, },
	{  },
};
MODULE_DEVICE_TABLE(css, io_subchannel_ids);

static int io_subchannel_prepare(struct subchannel *sch)
{
	struct ccw_device *cdev;
	cdev = sch_get_cdev(sch);
	if (cdev && !device_is_registered(&cdev->dev))
		return -EAGAIN;
	return 0;
}

static int io_subchannel_settle(void)
{
	int ret;

	ret = wait_event_interruptible(ccw_device_init_wq,
				atomic_read(&ccw_device_init_count) == 0);
	if (ret)
		return -EINTR;
	flush_workqueue(cio_work_q);
	return 0;
}

static struct css_driver io_subchannel_driver = {
	.drv = {
		.owner = THIS_MODULE,
		.name = "io_subchannel",
	},
	.subchannel_type = io_subchannel_ids,
	.irq = io_subchannel_irq,
	.sch_event = io_subchannel_sch_event,
	.chp_event = io_subchannel_chp_event,
	.probe = io_subchannel_probe,
	.remove = io_subchannel_remove,
	.shutdown = io_subchannel_shutdown,
	.prepare = io_subchannel_prepare,
	.settle = io_subchannel_settle,
};

int __init io_subchannel_init(void)
{
	int ret;

	init_waitqueue_head(&ccw_device_init_wq);
	atomic_set(&ccw_device_init_count, 0);
	setup_timer(&recovery_timer, recovery_func, 0);

	ret = bus_register(&ccw_bus_type);
	if (ret)
		return ret;
	ret = css_driver_register(&io_subchannel_driver);
	if (ret)
		bus_unregister(&ccw_bus_type);

	return ret;
}



static ssize_t
chpids_show (struct device * dev, struct device_attribute *attr, char * buf)
{
	struct subchannel *sch = to_subchannel(dev);
	struct chsc_ssd_info *ssd = &sch->ssd_info;
	ssize_t ret = 0;
	int chp;
	int mask;

	for (chp = 0; chp < 8; chp++) {
		mask = 0x80 >> chp;
		if (ssd->path_mask & mask)
			ret += sprintf(buf + ret, "%02x ", ssd->chpid[chp].id);
		else
			ret += sprintf(buf + ret, "00 ");
	}
	ret += sprintf (buf+ret, "\n");
	return min((ssize_t)PAGE_SIZE, ret);
}

static ssize_t
pimpampom_show (struct device * dev, struct device_attribute *attr, char * buf)
{
	struct subchannel *sch = to_subchannel(dev);
	struct pmcw *pmcw = &sch->schib.pmcw;

	return sprintf (buf, "%02x %02x %02x\n",
			pmcw->pim, pmcw->pam, pmcw->pom);
}

static ssize_t
devtype_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_device_id *id = &(cdev->id);

	if (id->dev_type != 0)
		return sprintf(buf, "%04x/%02x\n",
				id->dev_type, id->dev_model);
	else
		return sprintf(buf, "n/a\n");
}

static ssize_t
cutype_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_device_id *id = &(cdev->id);

	return sprintf(buf, "%04x/%02x\n",
		       id->cu_type, id->cu_model);
}

static ssize_t
modalias_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_device_id *id = &(cdev->id);
	int len;

	len = snprint_alias(buf, PAGE_SIZE, id, "\n");

	return len > PAGE_SIZE ? PAGE_SIZE : len;
}

static ssize_t
online_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);

	return sprintf(buf, cdev->online ? "1\n" : "0\n");
}

int ccw_device_is_orphan(struct ccw_device *cdev)
{
	return sch_is_pseudo_sch(to_subchannel(cdev->dev.parent));
}

static void ccw_device_unregister(struct ccw_device *cdev)
{
	if (device_is_registered(&cdev->dev)) {
		
		device_del(&cdev->dev);
	}
	if (cdev->private->flags.initialized) {
		cdev->private->flags.initialized = 0;
		
		put_device(&cdev->dev);
	}
}

static void io_subchannel_quiesce(struct subchannel *);

int ccw_device_set_offline(struct ccw_device *cdev)
{
	struct subchannel *sch;
	int ret, state;

	if (!cdev)
		return -ENODEV;
	if (!cdev->online || !cdev->drv)
		return -EINVAL;

	if (cdev->drv->set_offline) {
		ret = cdev->drv->set_offline(cdev);
		if (ret != 0)
			return ret;
	}
	cdev->online = 0;
	spin_lock_irq(cdev->ccwlock);
	sch = to_subchannel(cdev->dev.parent);
	
	while (!dev_fsm_final_state(cdev) &&
	       cdev->private->state != DEV_STATE_DISCONNECTED) {
		spin_unlock_irq(cdev->ccwlock);
		wait_event(cdev->private->wait_q, (dev_fsm_final_state(cdev) ||
			   cdev->private->state == DEV_STATE_DISCONNECTED));
		spin_lock_irq(cdev->ccwlock);
	}
	do {
		ret = ccw_device_offline(cdev);
		if (!ret)
			break;
		CIO_MSG_EVENT(0, "ccw_device_offline returned %d, device "
			      "0.%x.%04x\n", ret, cdev->private->dev_id.ssid,
			      cdev->private->dev_id.devno);
		if (ret != -EBUSY)
			goto error;
		state = cdev->private->state;
		spin_unlock_irq(cdev->ccwlock);
		io_subchannel_quiesce(sch);
		spin_lock_irq(cdev->ccwlock);
		cdev->private->state = state;
	} while (ret == -EBUSY);
	spin_unlock_irq(cdev->ccwlock);
	wait_event(cdev->private->wait_q, (dev_fsm_final_state(cdev) ||
		   cdev->private->state == DEV_STATE_DISCONNECTED));
	
	if (cdev->private->state == DEV_STATE_BOXED) {
		pr_warning("%s: The device entered boxed state while "
			   "being set offline\n", dev_name(&cdev->dev));
	} else if (cdev->private->state == DEV_STATE_NOT_OPER) {
		pr_warning("%s: The device stopped operating while "
			   "being set offline\n", dev_name(&cdev->dev));
	}
	
	put_device(&cdev->dev);
	return 0;

error:
	cdev->private->state = DEV_STATE_OFFLINE;
	dev_fsm_event(cdev, DEV_EVENT_NOTOPER);
	spin_unlock_irq(cdev->ccwlock);
	
	put_device(&cdev->dev);
	return -ENODEV;
}

int ccw_device_set_online(struct ccw_device *cdev)
{
	int ret;
	int ret2;

	if (!cdev)
		return -ENODEV;
	if (cdev->online || !cdev->drv)
		return -EINVAL;
	
	if (!get_device(&cdev->dev))
		return -ENODEV;

	spin_lock_irq(cdev->ccwlock);
	ret = ccw_device_online(cdev);
	spin_unlock_irq(cdev->ccwlock);
	if (ret == 0)
		wait_event(cdev->private->wait_q, dev_fsm_final_state(cdev));
	else {
		CIO_MSG_EVENT(0, "ccw_device_online returned %d, "
			      "device 0.%x.%04x\n",
			      ret, cdev->private->dev_id.ssid,
			      cdev->private->dev_id.devno);
		
		put_device(&cdev->dev);
		return ret;
	}
	spin_lock_irq(cdev->ccwlock);
	
	if ((cdev->private->state != DEV_STATE_ONLINE) &&
	    (cdev->private->state != DEV_STATE_W4SENSE)) {
		spin_unlock_irq(cdev->ccwlock);
		
		if (cdev->private->state == DEV_STATE_BOXED) {
			pr_warning("%s: Setting the device online failed "
				   "because it is boxed\n",
				   dev_name(&cdev->dev));
		} else if (cdev->private->state == DEV_STATE_NOT_OPER) {
			pr_warning("%s: Setting the device online failed "
				   "because it is not operational\n",
				   dev_name(&cdev->dev));
		}
		
		put_device(&cdev->dev);
		return -ENODEV;
	}
	spin_unlock_irq(cdev->ccwlock);
	if (cdev->drv->set_online)
		ret = cdev->drv->set_online(cdev);
	if (ret)
		goto rollback;
	cdev->online = 1;
	return 0;

rollback:
	spin_lock_irq(cdev->ccwlock);
	
	while (!dev_fsm_final_state(cdev) &&
	       cdev->private->state != DEV_STATE_DISCONNECTED) {
		spin_unlock_irq(cdev->ccwlock);
		wait_event(cdev->private->wait_q, (dev_fsm_final_state(cdev) ||
			   cdev->private->state == DEV_STATE_DISCONNECTED));
		spin_lock_irq(cdev->ccwlock);
	}
	ret2 = ccw_device_offline(cdev);
	if (ret2)
		goto error;
	spin_unlock_irq(cdev->ccwlock);
	wait_event(cdev->private->wait_q, (dev_fsm_final_state(cdev) ||
		   cdev->private->state == DEV_STATE_DISCONNECTED));
	
	put_device(&cdev->dev);
	return ret;

error:
	CIO_MSG_EVENT(0, "rollback ccw_device_offline returned %d, "
		      "device 0.%x.%04x\n",
		      ret2, cdev->private->dev_id.ssid,
		      cdev->private->dev_id.devno);
	cdev->private->state = DEV_STATE_OFFLINE;
	spin_unlock_irq(cdev->ccwlock);
	
	put_device(&cdev->dev);
	return ret;
}

static int online_store_handle_offline(struct ccw_device *cdev)
{
	if (cdev->private->state == DEV_STATE_DISCONNECTED) {
		spin_lock_irq(cdev->ccwlock);
		ccw_device_sched_todo(cdev, CDEV_TODO_UNREG_EVAL);
		spin_unlock_irq(cdev->ccwlock);
		return 0;
	}
	if (cdev->drv && cdev->drv->set_offline)
		return ccw_device_set_offline(cdev);
	return -EINVAL;
}

static int online_store_recog_and_online(struct ccw_device *cdev)
{
	
	if (cdev->private->state == DEV_STATE_BOXED) {
		spin_lock_irq(cdev->ccwlock);
		ccw_device_recognition(cdev);
		spin_unlock_irq(cdev->ccwlock);
		wait_event(cdev->private->wait_q,
			   cdev->private->flags.recog_done);
		if (cdev->private->state != DEV_STATE_OFFLINE)
			
			return -EAGAIN;
	}
	if (cdev->drv && cdev->drv->set_online)
		return ccw_device_set_online(cdev);
	return -EINVAL;
}

static int online_store_handle_online(struct ccw_device *cdev, int force)
{
	int ret;

	ret = online_store_recog_and_online(cdev);
	if (ret && !force)
		return ret;
	if (force && cdev->private->state == DEV_STATE_BOXED) {
		ret = ccw_device_stlck(cdev);
		if (ret)
			return ret;
		if (cdev->id.cu_type == 0)
			cdev->private->state = DEV_STATE_NOT_OPER;
		ret = online_store_recog_and_online(cdev);
		if (ret)
			return ret;
	}
	return 0;
}

static ssize_t online_store (struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	int force, ret;
	unsigned long i;

	
	if (atomic_cmpxchg(&cdev->private->onoff, 0, 1) != 0)
		return -EAGAIN;
	
	if (!dev_fsm_final_state(cdev) &&
	    cdev->private->state != DEV_STATE_DISCONNECTED) {
		ret = -EAGAIN;
		goto out_onoff;
	}
	
	if (work_pending(&cdev->private->todo_work)) {
		ret = -EAGAIN;
		goto out_onoff;
	}

	if (cdev->drv && !try_module_get(cdev->drv->driver.owner)) {
		ret = -EINVAL;
		goto out_onoff;
	}
	if (!strncmp(buf, "force\n", count)) {
		force = 1;
		i = 1;
		ret = 0;
	} else {
		force = 0;
		ret = strict_strtoul(buf, 16, &i);
	}
	if (ret)
		goto out;
	switch (i) {
	case 0:
		ret = online_store_handle_offline(cdev);
		break;
	case 1:
		ret = online_store_handle_online(cdev, force);
		break;
	default:
		ret = -EINVAL;
	}
out:
	if (cdev->drv)
		module_put(cdev->drv->driver.owner);
out_onoff:
	atomic_set(&cdev->private->onoff, 0);
	return (ret < 0) ? ret : count;
}

static ssize_t
available_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct subchannel *sch;

	if (ccw_device_is_orphan(cdev))
		return sprintf(buf, "no device\n");
	switch (cdev->private->state) {
	case DEV_STATE_BOXED:
		return sprintf(buf, "boxed\n");
	case DEV_STATE_DISCONNECTED:
	case DEV_STATE_DISCONNECTED_SENSE_ID:
	case DEV_STATE_NOT_OPER:
		sch = to_subchannel(dev->parent);
		if (!sch->lpm)
			return sprintf(buf, "no path\n");
		else
			return sprintf(buf, "no device\n");
	default:
		
		return sprintf(buf, "good\n");
	}
}

static ssize_t
initiate_logging(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	struct subchannel *sch = to_subchannel(dev);
	int rc;

	rc = chsc_siosl(sch->schid);
	if (rc < 0) {
		pr_warning("Logging for subchannel 0.%x.%04x failed with "
			   "errno=%d\n",
			   sch->schid.ssid, sch->schid.sch_no, rc);
		return rc;
	}
	pr_notice("Logging for subchannel 0.%x.%04x was triggered\n",
		  sch->schid.ssid, sch->schid.sch_no);
	return count;
}

static DEVICE_ATTR(chpids, 0444, chpids_show, NULL);
static DEVICE_ATTR(pimpampom, 0444, pimpampom_show, NULL);
static DEVICE_ATTR(devtype, 0444, devtype_show, NULL);
static DEVICE_ATTR(cutype, 0444, cutype_show, NULL);
static DEVICE_ATTR(modalias, 0444, modalias_show, NULL);
static DEVICE_ATTR(online, 0644, online_show, online_store);
static DEVICE_ATTR(availability, 0444, available_show, NULL);
static DEVICE_ATTR(logging, 0200, NULL, initiate_logging);

static struct attribute *io_subchannel_attrs[] = {
	&dev_attr_chpids.attr,
	&dev_attr_pimpampom.attr,
	&dev_attr_logging.attr,
	NULL,
};

static struct attribute_group io_subchannel_attr_group = {
	.attrs = io_subchannel_attrs,
};

static struct attribute * ccwdev_attrs[] = {
	&dev_attr_devtype.attr,
	&dev_attr_cutype.attr,
	&dev_attr_modalias.attr,
	&dev_attr_online.attr,
	&dev_attr_cmb_enable.attr,
	&dev_attr_availability.attr,
	NULL,
};

static struct attribute_group ccwdev_attr_group = {
	.attrs = ccwdev_attrs,
};

static const struct attribute_group *ccwdev_attr_groups[] = {
	&ccwdev_attr_group,
	NULL,
};

static int ccw_device_register(struct ccw_device *cdev)
{
	struct device *dev = &cdev->dev;
	int ret;

	dev->bus = &ccw_bus_type;
	ret = dev_set_name(&cdev->dev, "0.%x.%04x", cdev->private->dev_id.ssid,
			   cdev->private->dev_id.devno);
	if (ret)
		return ret;
	return device_add(dev);
}

static int match_dev_id(struct device *dev, void *data)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_dev_id *dev_id = data;

	return ccw_dev_id_is_equal(&cdev->private->dev_id, dev_id);
}

static struct ccw_device *get_ccwdev_by_dev_id(struct ccw_dev_id *dev_id)
{
	struct device *dev;

	dev = bus_find_device(&ccw_bus_type, NULL, dev_id, match_dev_id);

	return dev ? to_ccwdev(dev) : NULL;
}

static void ccw_device_do_unbind_bind(struct ccw_device *cdev)
{
	int ret;

	if (device_is_registered(&cdev->dev)) {
		device_release_driver(&cdev->dev);
		ret = device_attach(&cdev->dev);
		WARN_ON(ret == -ENODEV);
	}
}

static void
ccw_device_release(struct device *dev)
{
	struct ccw_device *cdev;

	cdev = to_ccwdev(dev);
	
	put_device(cdev->dev.parent);
	kfree(cdev->private);
	kfree(cdev);
}

static struct ccw_device * io_subchannel_allocate_dev(struct subchannel *sch)
{
	struct ccw_device *cdev;

	cdev  = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (cdev) {
		cdev->private = kzalloc(sizeof(struct ccw_device_private),
					GFP_KERNEL | GFP_DMA);
		if (cdev->private)
			return cdev;
	}
	kfree(cdev);
	return ERR_PTR(-ENOMEM);
}

static void ccw_device_todo(struct work_struct *work);

static int io_subchannel_initialize_dev(struct subchannel *sch,
					struct ccw_device *cdev)
{
	cdev->private->cdev = cdev;
	cdev->private->int_class = IOINT_CIO;
	atomic_set(&cdev->private->onoff, 0);
	cdev->dev.parent = &sch->dev;
	cdev->dev.release = ccw_device_release;
	INIT_WORK(&cdev->private->todo_work, ccw_device_todo);
	cdev->dev.groups = ccwdev_attr_groups;
	
	device_initialize(&cdev->dev);
	if (!get_device(&sch->dev)) {
		
		put_device(&cdev->dev);
		return -ENODEV;
	}
	cdev->private->flags.initialized = 1;
	return 0;
}

static struct ccw_device * io_subchannel_create_ccwdev(struct subchannel *sch)
{
	struct ccw_device *cdev;
	int ret;

	cdev = io_subchannel_allocate_dev(sch);
	if (!IS_ERR(cdev)) {
		ret = io_subchannel_initialize_dev(sch, cdev);
		if (ret)
			cdev = ERR_PTR(ret);
	}
	return cdev;
}

static void io_subchannel_recog(struct ccw_device *, struct subchannel *);

static void sch_create_and_recog_new_device(struct subchannel *sch)
{
	struct ccw_device *cdev;

	
	cdev = io_subchannel_create_ccwdev(sch);
	if (IS_ERR(cdev)) {
		
		css_sch_device_unregister(sch);
		return;
	}
	
	io_subchannel_recog(cdev, sch);
}

static void io_subchannel_register(struct ccw_device *cdev)
{
	struct subchannel *sch;
	int ret, adjust_init_count = 1;
	unsigned long flags;

	sch = to_subchannel(cdev->dev.parent);
	if (!device_is_registered(&sch->dev))
		goto out_err;
	css_update_ssd_info(sch);
	if (device_is_registered(&cdev->dev)) {
		if (!cdev->drv) {
			ret = device_reprobe(&cdev->dev);
			if (ret)
				
				CIO_MSG_EVENT(0, "device_reprobe() returned"
					      " %d for 0.%x.%04x\n", ret,
					      cdev->private->dev_id.ssid,
					      cdev->private->dev_id.devno);
		}
		adjust_init_count = 0;
		goto out;
	}
	dev_set_uevent_suppress(&sch->dev, 0);
	kobject_uevent(&sch->dev.kobj, KOBJ_ADD);
	
	ret = ccw_device_register(cdev);
	if (ret) {
		CIO_MSG_EVENT(0, "Could not register ccw dev 0.%x.%04x: %d\n",
			      cdev->private->dev_id.ssid,
			      cdev->private->dev_id.devno, ret);
		spin_lock_irqsave(sch->lock, flags);
		sch_set_cdev(sch, NULL);
		spin_unlock_irqrestore(sch->lock, flags);
		
		put_device(&cdev->dev);
		goto out_err;
	}
out:
	cdev->private->flags.recog_done = 1;
	wake_up(&cdev->private->wait_q);
out_err:
	if (adjust_init_count && atomic_dec_and_test(&ccw_device_init_count))
		wake_up(&ccw_device_init_wq);
}

static void ccw_device_call_sch_unregister(struct ccw_device *cdev)
{
	struct subchannel *sch;

	
	if (!get_device(cdev->dev.parent))
		return;
	sch = to_subchannel(cdev->dev.parent);
	css_sch_device_unregister(sch);
	
	put_device(&sch->dev);
}

void
io_subchannel_recog_done(struct ccw_device *cdev)
{
	if (css_init_done == 0) {
		cdev->private->flags.recog_done = 1;
		return;
	}
	switch (cdev->private->state) {
	case DEV_STATE_BOXED:
		
	case DEV_STATE_NOT_OPER:
		cdev->private->flags.recog_done = 1;
		
		ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		if (atomic_dec_and_test(&ccw_device_init_count))
			wake_up(&ccw_device_init_wq);
		break;
	case DEV_STATE_OFFLINE:
		ccw_device_sched_todo(cdev, CDEV_TODO_REGISTER);
		break;
	}
}

static void io_subchannel_recog(struct ccw_device *cdev, struct subchannel *sch)
{
	struct ccw_device_private *priv;

	cdev->ccwlock = sch->lock;

	
	priv = cdev->private;
	priv->dev_id.devno = sch->schib.pmcw.dev;
	priv->dev_id.ssid = sch->schid.ssid;
	priv->schid = sch->schid;
	priv->state = DEV_STATE_NOT_OPER;
	INIT_LIST_HEAD(&priv->cmb_list);
	init_waitqueue_head(&priv->wait_q);
	init_timer(&priv->timer);

	
	atomic_inc(&ccw_device_init_count);

	
	spin_lock_irq(sch->lock);
	sch_set_cdev(sch, cdev);
	ccw_device_recognition(cdev);
	spin_unlock_irq(sch->lock);
}

static int ccw_device_move_to_sch(struct ccw_device *cdev,
				  struct subchannel *sch)
{
	struct subchannel *old_sch;
	int rc, old_enabled = 0;

	old_sch = to_subchannel(cdev->dev.parent);
	
	if (!get_device(&sch->dev))
		return -ENODEV;

	if (!sch_is_pseudo_sch(old_sch)) {
		spin_lock_irq(old_sch->lock);
		old_enabled = old_sch->schib.pmcw.ena;
		rc = 0;
		if (old_enabled)
			rc = cio_disable_subchannel(old_sch);
		spin_unlock_irq(old_sch->lock);
		if (rc == -EBUSY) {
			
			put_device(&sch->dev);
			return rc;
		}
	}

	mutex_lock(&sch->reg_mutex);
	rc = device_move(&cdev->dev, &sch->dev, DPM_ORDER_PARENT_BEFORE_DEV);
	mutex_unlock(&sch->reg_mutex);
	if (rc) {
		CIO_MSG_EVENT(0, "device_move(0.%x.%04x,0.%x.%04x)=%d\n",
			      cdev->private->dev_id.ssid,
			      cdev->private->dev_id.devno, sch->schid.ssid,
			      sch->schib.pmcw.dev, rc);
		if (old_enabled) {
			
			spin_lock_irq(old_sch->lock);
			cio_enable_subchannel(old_sch, (u32)(addr_t)old_sch);
			spin_unlock_irq(old_sch->lock);
		}
		
		put_device(&sch->dev);
		return rc;
	}
	
	if (!sch_is_pseudo_sch(old_sch)) {
		spin_lock_irq(old_sch->lock);
		sch_set_cdev(old_sch, NULL);
		spin_unlock_irq(old_sch->lock);
		css_schedule_eval(old_sch->schid);
	}
	
	put_device(&old_sch->dev);
	
	spin_lock_irq(sch->lock);
	cdev->private->schid = sch->schid;
	cdev->ccwlock = sch->lock;
	if (!sch_is_pseudo_sch(sch))
		sch_set_cdev(sch, cdev);
	spin_unlock_irq(sch->lock);
	if (!sch_is_pseudo_sch(sch))
		css_update_ssd_info(sch);
	return 0;
}

static int ccw_device_move_to_orph(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	struct channel_subsystem *css = to_css(sch->dev.parent);

	return ccw_device_move_to_sch(cdev, css->pseudo_subchannel);
}

static void io_subchannel_irq(struct subchannel *sch)
{
	struct ccw_device *cdev;

	cdev = sch_get_cdev(sch);

	CIO_TRACE_EVENT(6, "IRQ");
	CIO_TRACE_EVENT(6, dev_name(&sch->dev));
	if (cdev)
		dev_fsm_event(cdev, DEV_EVENT_INTERRUPT);
	else
		kstat_cpu(smp_processor_id()).irqs[IOINT_CIO]++;
}

void io_subchannel_init_config(struct subchannel *sch)
{
	memset(&sch->config, 0, sizeof(sch->config));
	sch->config.csense = 1;
}

static void io_subchannel_init_fields(struct subchannel *sch)
{
	if (cio_is_console(sch->schid))
		sch->opm = 0xff;
	else
		sch->opm = chp_get_sch_opm(sch);
	sch->lpm = sch->schib.pmcw.pam & sch->opm;
	sch->isc = cio_is_console(sch->schid) ? CONSOLE_ISC : IO_SCH_ISC;

	CIO_MSG_EVENT(6, "Detected device %04x on subchannel 0.%x.%04X"
		      " - PIM = %02X, PAM = %02X, POM = %02X\n",
		      sch->schib.pmcw.dev, sch->schid.ssid,
		      sch->schid.sch_no, sch->schib.pmcw.pim,
		      sch->schib.pmcw.pam, sch->schib.pmcw.pom);

	io_subchannel_init_config(sch);
}

static int io_subchannel_probe(struct subchannel *sch)
{
	struct io_subchannel_private *io_priv;
	struct ccw_device *cdev;
	int rc;

	if (cio_is_console(sch->schid)) {
		rc = sysfs_create_group(&sch->dev.kobj,
					&io_subchannel_attr_group);
		if (rc)
			CIO_MSG_EVENT(0, "Failed to create io subchannel "
				      "attributes for subchannel "
				      "0.%x.%04x (rc=%d)\n",
				      sch->schid.ssid, sch->schid.sch_no, rc);
		dev_set_uevent_suppress(&sch->dev, 0);
		kobject_uevent(&sch->dev.kobj, KOBJ_ADD);
		cdev = sch_get_cdev(sch);
		cdev->dev.groups = ccwdev_attr_groups;
		device_initialize(&cdev->dev);
		cdev->private->flags.initialized = 1;
		ccw_device_register(cdev);
		if (cdev->private->state != DEV_STATE_NOT_OPER &&
		    cdev->private->state != DEV_STATE_OFFLINE &&
		    cdev->private->state != DEV_STATE_BOXED)
			get_device(&cdev->dev);
		return 0;
	}
	io_subchannel_init_fields(sch);
	rc = cio_commit_config(sch);
	if (rc)
		goto out_schedule;
	rc = sysfs_create_group(&sch->dev.kobj,
				&io_subchannel_attr_group);
	if (rc)
		goto out_schedule;
	
	io_priv = kzalloc(sizeof(*io_priv), GFP_KERNEL | GFP_DMA);
	if (!io_priv)
		goto out_schedule;

	set_io_private(sch, io_priv);
	css_schedule_eval(sch->schid);
	return 0;

out_schedule:
	spin_lock_irq(sch->lock);
	css_sched_sch_todo(sch, SCH_TODO_UNREG);
	spin_unlock_irq(sch->lock);
	return 0;
}

static int
io_subchannel_remove (struct subchannel *sch)
{
	struct io_subchannel_private *io_priv = to_io_private(sch);
	struct ccw_device *cdev;

	cdev = sch_get_cdev(sch);
	if (!cdev)
		goto out_free;
	io_subchannel_quiesce(sch);
	
	spin_lock_irq(cdev->ccwlock);
	sch_set_cdev(sch, NULL);
	set_io_private(sch, NULL);
	cdev->private->state = DEV_STATE_NOT_OPER;
	spin_unlock_irq(cdev->ccwlock);
	ccw_device_unregister(cdev);
out_free:
	kfree(io_priv);
	sysfs_remove_group(&sch->dev.kobj, &io_subchannel_attr_group);
	return 0;
}

static void io_subchannel_verify(struct subchannel *sch)
{
	struct ccw_device *cdev;

	cdev = sch_get_cdev(sch);
	if (cdev)
		dev_fsm_event(cdev, DEV_EVENT_VERIFY);
}

static void io_subchannel_terminate_path(struct subchannel *sch, u8 mask)
{
	struct ccw_device *cdev;

	cdev = sch_get_cdev(sch);
	if (!cdev)
		return;
	if (cio_update_schib(sch))
		goto err;
	
	if (scsw_actl(&sch->schib.scsw) == 0 || sch->schib.pmcw.lpum != mask)
		goto out;
	if (cdev->private->state == DEV_STATE_ONLINE) {
		ccw_device_kill_io(cdev);
		goto out;
	}
	if (cio_clear(sch))
		goto err;
out:
	
	dev_fsm_event(cdev, DEV_EVENT_VERIFY);
	return;

err:
	dev_fsm_event(cdev, DEV_EVENT_NOTOPER);
}

static int io_subchannel_chp_event(struct subchannel *sch,
				   struct chp_link *link, int event)
{
	struct ccw_device *cdev = sch_get_cdev(sch);
	int mask;

	mask = chp_ssd_get_mask(&sch->ssd_info, link);
	if (!mask)
		return 0;
	switch (event) {
	case CHP_VARY_OFF:
		sch->opm &= ~mask;
		sch->lpm &= ~mask;
		if (cdev)
			cdev->private->path_gone_mask |= mask;
		io_subchannel_terminate_path(sch, mask);
		break;
	case CHP_VARY_ON:
		sch->opm |= mask;
		sch->lpm |= mask;
		if (cdev)
			cdev->private->path_new_mask |= mask;
		io_subchannel_verify(sch);
		break;
	case CHP_OFFLINE:
		if (cio_update_schib(sch))
			return -ENODEV;
		if (cdev)
			cdev->private->path_gone_mask |= mask;
		io_subchannel_terminate_path(sch, mask);
		break;
	case CHP_ONLINE:
		if (cio_update_schib(sch))
			return -ENODEV;
		sch->lpm |= mask & sch->opm;
		if (cdev)
			cdev->private->path_new_mask |= mask;
		io_subchannel_verify(sch);
		break;
	}
	return 0;
}

static void io_subchannel_quiesce(struct subchannel *sch)
{
	struct ccw_device *cdev;
	int ret;

	spin_lock_irq(sch->lock);
	cdev = sch_get_cdev(sch);
	if (cio_is_console(sch->schid))
		goto out_unlock;
	if (!sch->schib.pmcw.ena)
		goto out_unlock;
	ret = cio_disable_subchannel(sch);
	if (ret != -EBUSY)
		goto out_unlock;
	if (cdev->handler)
		cdev->handler(cdev, cdev->private->intparm, ERR_PTR(-EIO));
	while (ret == -EBUSY) {
		cdev->private->state = DEV_STATE_QUIESCE;
		cdev->private->iretry = 255;
		ret = ccw_device_cancel_halt_clear(cdev);
		if (ret == -EBUSY) {
			ccw_device_set_timeout(cdev, HZ/10);
			spin_unlock_irq(sch->lock);
			wait_event(cdev->private->wait_q,
				   cdev->private->state != DEV_STATE_QUIESCE);
			spin_lock_irq(sch->lock);
		}
		ret = cio_disable_subchannel(sch);
	}
out_unlock:
	spin_unlock_irq(sch->lock);
}

static void io_subchannel_shutdown(struct subchannel *sch)
{
	io_subchannel_quiesce(sch);
}

static int device_is_disconnected(struct ccw_device *cdev)
{
	if (!cdev)
		return 0;
	return (cdev->private->state == DEV_STATE_DISCONNECTED ||
		cdev->private->state == DEV_STATE_DISCONNECTED_SENSE_ID);
}

static int recovery_check(struct device *dev, void *data)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	int *redo = data;

	spin_lock_irq(cdev->ccwlock);
	switch (cdev->private->state) {
	case DEV_STATE_DISCONNECTED:
		CIO_MSG_EVENT(3, "recovery: trigger 0.%x.%04x\n",
			      cdev->private->dev_id.ssid,
			      cdev->private->dev_id.devno);
		dev_fsm_event(cdev, DEV_EVENT_VERIFY);
		*redo = 1;
		break;
	case DEV_STATE_DISCONNECTED_SENSE_ID:
		*redo = 1;
		break;
	}
	spin_unlock_irq(cdev->ccwlock);

	return 0;
}

static void recovery_work_func(struct work_struct *unused)
{
	int redo = 0;

	bus_for_each_dev(&ccw_bus_type, NULL, &redo, recovery_check);
	if (redo) {
		spin_lock_irq(&recovery_lock);
		if (!timer_pending(&recovery_timer)) {
			if (recovery_phase < ARRAY_SIZE(recovery_delay) - 1)
				recovery_phase++;
			mod_timer(&recovery_timer, jiffies +
				  recovery_delay[recovery_phase] * HZ);
		}
		spin_unlock_irq(&recovery_lock);
	} else
		CIO_MSG_EVENT(4, "recovery: end\n");
}

static DECLARE_WORK(recovery_work, recovery_work_func);

static void recovery_func(unsigned long data)
{
	schedule_work(&recovery_work);
}

static void ccw_device_schedule_recovery(void)
{
	unsigned long flags;

	CIO_MSG_EVENT(4, "recovery: schedule\n");
	spin_lock_irqsave(&recovery_lock, flags);
	if (!timer_pending(&recovery_timer) || (recovery_phase != 0)) {
		recovery_phase = 0;
		mod_timer(&recovery_timer, jiffies + recovery_delay[0] * HZ);
	}
	spin_unlock_irqrestore(&recovery_lock, flags);
}

static int purge_fn(struct device *dev, void *data)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_dev_id *id = &cdev->private->dev_id;

	spin_lock_irq(cdev->ccwlock);
	if (is_blacklisted(id->ssid, id->devno) &&
	    (cdev->private->state == DEV_STATE_OFFLINE) &&
	    (atomic_cmpxchg(&cdev->private->onoff, 0, 1) == 0)) {
		CIO_MSG_EVENT(3, "ccw: purging 0.%x.%04x\n", id->ssid,
			      id->devno);
		ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		atomic_set(&cdev->private->onoff, 0);
	}
	spin_unlock_irq(cdev->ccwlock);
	
	if (signal_pending(current))
		return -EINTR;

	return 0;
}

int ccw_purge_blacklisted(void)
{
	CIO_MSG_EVENT(2, "ccw: purging blacklisted devices\n");
	bus_for_each_dev(&ccw_bus_type, NULL, NULL, purge_fn);
	return 0;
}

void ccw_device_set_disconnected(struct ccw_device *cdev)
{
	if (!cdev)
		return;
	ccw_device_set_timeout(cdev, 0);
	cdev->private->flags.fake_irb = 0;
	cdev->private->state = DEV_STATE_DISCONNECTED;
	if (cdev->online)
		ccw_device_schedule_recovery();
}

void ccw_device_set_notoper(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	CIO_TRACE_EVENT(2, "notoper");
	CIO_TRACE_EVENT(2, dev_name(&sch->dev));
	ccw_device_set_timeout(cdev, 0);
	cio_disable_subchannel(sch);
	cdev->private->state = DEV_STATE_NOT_OPER;
}

enum io_sch_action {
	IO_SCH_UNREG,
	IO_SCH_ORPH_UNREG,
	IO_SCH_ATTACH,
	IO_SCH_UNREG_ATTACH,
	IO_SCH_ORPH_ATTACH,
	IO_SCH_REPROBE,
	IO_SCH_VERIFY,
	IO_SCH_DISC,
	IO_SCH_NOP,
};

static enum io_sch_action sch_get_action(struct subchannel *sch)
{
	struct ccw_device *cdev;

	cdev = sch_get_cdev(sch);
	if (cio_update_schib(sch)) {
		
		if (!cdev)
			return IO_SCH_UNREG;
		if (ccw_device_notify(cdev, CIO_GONE) != NOTIFY_OK)
			return IO_SCH_UNREG;
		return IO_SCH_ORPH_UNREG;
	}
	
	if (!cdev)
		return IO_SCH_ATTACH;
	if (sch->schib.pmcw.dev != cdev->private->dev_id.devno) {
		if (ccw_device_notify(cdev, CIO_GONE) != NOTIFY_OK)
			return IO_SCH_UNREG_ATTACH;
		return IO_SCH_ORPH_ATTACH;
	}
	if ((sch->schib.pmcw.pam & sch->opm) == 0) {
		if (ccw_device_notify(cdev, CIO_NO_PATH) != NOTIFY_OK)
			return IO_SCH_UNREG;
		return IO_SCH_DISC;
	}
	if (device_is_disconnected(cdev))
		return IO_SCH_REPROBE;
	if (cdev->online)
		return IO_SCH_VERIFY;
	return IO_SCH_NOP;
}

static int io_subchannel_sch_event(struct subchannel *sch, int process)
{
	unsigned long flags;
	struct ccw_device *cdev;
	struct ccw_dev_id dev_id;
	enum io_sch_action action;
	int rc = -EAGAIN;

	spin_lock_irqsave(sch->lock, flags);
	if (!device_is_registered(&sch->dev))
		goto out_unlock;
	if (work_pending(&sch->todo_work))
		goto out_unlock;
	cdev = sch_get_cdev(sch);
	if (cdev && work_pending(&cdev->private->todo_work))
		goto out_unlock;
	action = sch_get_action(sch);
	CIO_MSG_EVENT(2, "event: sch 0.%x.%04x, process=%d, action=%d\n",
		      sch->schid.ssid, sch->schid.sch_no, process,
		      action);
	
	switch (action) {
	case IO_SCH_REPROBE:
		
		ccw_device_trigger_reprobe(cdev);
		rc = 0;
		goto out_unlock;
	case IO_SCH_VERIFY:
		if (cdev->private->flags.resuming == 1) {
			if (cio_enable_subchannel(sch, (u32)(addr_t)sch)) {
				ccw_device_set_notoper(cdev);
				break;
			}
		}
		
		io_subchannel_verify(sch);
		rc = 0;
		goto out_unlock;
	case IO_SCH_DISC:
		ccw_device_set_disconnected(cdev);
		rc = 0;
		goto out_unlock;
	case IO_SCH_ORPH_UNREG:
	case IO_SCH_ORPH_ATTACH:
		ccw_device_set_disconnected(cdev);
		break;
	case IO_SCH_UNREG_ATTACH:
	case IO_SCH_UNREG:
		if (!cdev)
			break;
		if (cdev->private->state == DEV_STATE_SENSE_ID) {
			dev_fsm_event(cdev, DEV_EVENT_NOTOPER);
		} else
			ccw_device_set_notoper(cdev);
		break;
	case IO_SCH_NOP:
		rc = 0;
		goto out_unlock;
	default:
		break;
	}
	spin_unlock_irqrestore(sch->lock, flags);
	
	if (!process)
		goto out;
	
	switch (action) {
	case IO_SCH_ORPH_UNREG:
	case IO_SCH_ORPH_ATTACH:
		
		rc = ccw_device_move_to_orph(cdev);
		if (rc)
			goto out;
		break;
	case IO_SCH_UNREG_ATTACH:
		if (cdev->private->flags.resuming) {
			
			rc = 0;
			goto out;
		}
		
		ccw_device_unregister(cdev);
		break;
	default:
		break;
	}
	
	switch (action) {
	case IO_SCH_ORPH_UNREG:
	case IO_SCH_UNREG:
		if (!cdev || !cdev->private->flags.resuming)
			css_sch_device_unregister(sch);
		break;
	case IO_SCH_ORPH_ATTACH:
	case IO_SCH_UNREG_ATTACH:
	case IO_SCH_ATTACH:
		dev_id.ssid = sch->schid.ssid;
		dev_id.devno = sch->schib.pmcw.dev;
		cdev = get_ccwdev_by_dev_id(&dev_id);
		if (!cdev) {
			sch_create_and_recog_new_device(sch);
			break;
		}
		rc = ccw_device_move_to_sch(cdev, sch);
		if (rc) {
			
			put_device(&cdev->dev);
			goto out;
		}
		spin_lock_irqsave(sch->lock, flags);
		ccw_device_trigger_reprobe(cdev);
		spin_unlock_irqrestore(sch->lock, flags);
		
		put_device(&cdev->dev);
		break;
	default:
		break;
	}
	return 0;

out_unlock:
	spin_unlock_irqrestore(sch->lock, flags);
out:
	return rc;
}

#ifdef CONFIG_CCW_CONSOLE
static struct ccw_device console_cdev;
static struct ccw_device_private console_private;
static int console_cdev_in_use;

static DEFINE_SPINLOCK(ccw_console_lock);

spinlock_t * cio_get_console_lock(void)
{
	return &ccw_console_lock;
}

static int ccw_device_console_enable(struct ccw_device *cdev,
				     struct subchannel *sch)
{
	struct io_subchannel_private *io_priv = cio_get_console_priv();
	int rc;

	
	memset(io_priv, 0, sizeof(*io_priv));
	set_io_private(sch, io_priv);
	io_subchannel_init_fields(sch);
	rc = cio_commit_config(sch);
	if (rc)
		return rc;
	sch->driver = &io_subchannel_driver;
	
	cdev->dev.parent= &sch->dev;
	sch_set_cdev(sch, cdev);
	io_subchannel_recog(cdev, sch);
	
	spin_lock_irq(cdev->ccwlock);
	while (!dev_fsm_final_state(cdev))
		wait_cons_dev();
	rc = -EIO;
	if (cdev->private->state != DEV_STATE_OFFLINE)
		goto out_unlock;
	ccw_device_online(cdev);
	while (!dev_fsm_final_state(cdev))
		wait_cons_dev();
	if (cdev->private->state != DEV_STATE_ONLINE)
		goto out_unlock;
	rc = 0;
out_unlock:
	spin_unlock_irq(cdev->ccwlock);
	return rc;
}

struct ccw_device *
ccw_device_probe_console(void)
{
	struct subchannel *sch;
	int ret;

	if (xchg(&console_cdev_in_use, 1) != 0)
		return ERR_PTR(-EBUSY);
	sch = cio_probe_console();
	if (IS_ERR(sch)) {
		console_cdev_in_use = 0;
		return (void *) sch;
	}
	memset(&console_cdev, 0, sizeof(struct ccw_device));
	memset(&console_private, 0, sizeof(struct ccw_device_private));
	console_cdev.private = &console_private;
	console_private.cdev = &console_cdev;
	console_private.int_class = IOINT_CIO;
	ret = ccw_device_console_enable(&console_cdev, sch);
	if (ret) {
		cio_release_console();
		console_cdev_in_use = 0;
		return ERR_PTR(ret);
	}
	console_cdev.online = 1;
	return &console_cdev;
}

static int ccw_device_pm_restore(struct device *dev);

int ccw_device_force_console(void)
{
	if (!console_cdev_in_use)
		return -ENODEV;
	return ccw_device_pm_restore(&console_cdev.dev);
}
EXPORT_SYMBOL_GPL(ccw_device_force_console);
#endif

static int
__ccwdev_check_busid(struct device *dev, void *id)
{
	char *bus_id;

	bus_id = id;

	return (strcmp(bus_id, dev_name(dev)) == 0);
}


struct ccw_device *get_ccwdev_by_busid(struct ccw_driver *cdrv,
				       const char *bus_id)
{
	struct device *dev;

	dev = driver_find_device(&cdrv->driver, NULL, (void *)bus_id,
				 __ccwdev_check_busid);

	return dev ? to_ccwdev(dev) : NULL;
}


static int
ccw_device_probe (struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_driver *cdrv = to_ccwdrv(dev->driver);
	int ret;

	cdev->drv = cdrv; 
	if (cdrv->int_class != 0)
		cdev->private->int_class = cdrv->int_class;
	else
		cdev->private->int_class = IOINT_CIO;

	ret = cdrv->probe ? cdrv->probe(cdev) : -ENODEV;

	if (ret) {
		cdev->drv = NULL;
		cdev->private->int_class = IOINT_CIO;
		return ret;
	}

	return 0;
}

static int
ccw_device_remove (struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct ccw_driver *cdrv = cdev->drv;
	int ret;

	if (cdrv->remove)
		cdrv->remove(cdev);
	if (cdev->online) {
		cdev->online = 0;
		spin_lock_irq(cdev->ccwlock);
		ret = ccw_device_offline(cdev);
		spin_unlock_irq(cdev->ccwlock);
		if (ret == 0)
			wait_event(cdev->private->wait_q,
				   dev_fsm_final_state(cdev));
		else
			CIO_MSG_EVENT(0, "ccw_device_offline returned %d, "
				      "device 0.%x.%04x\n",
				      ret, cdev->private->dev_id.ssid,
				      cdev->private->dev_id.devno);
		
		put_device(&cdev->dev);
	}
	ccw_device_set_timeout(cdev, 0);
	cdev->drv = NULL;
	cdev->private->int_class = IOINT_CIO;
	return 0;
}

static void ccw_device_shutdown(struct device *dev)
{
	struct ccw_device *cdev;

	cdev = to_ccwdev(dev);
	if (cdev->drv && cdev->drv->shutdown)
		cdev->drv->shutdown(cdev);
	disable_cmf(cdev);
}

static int ccw_device_pm_prepare(struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);

	if (work_pending(&cdev->private->todo_work))
		return -EAGAIN;
	
	if (atomic_read(&cdev->private->onoff))
		return -EAGAIN;

	if (cdev->online && cdev->drv && cdev->drv->prepare)
		return cdev->drv->prepare(cdev);

	return 0;
}

static void ccw_device_pm_complete(struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);

	if (cdev->online && cdev->drv && cdev->drv->complete)
		cdev->drv->complete(cdev);
}

static int ccw_device_pm_freeze(struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	int ret, cm_enabled;

	
	if (!dev_fsm_final_state(cdev))
		return -EAGAIN;
	if (!cdev->online)
		return 0;
	if (cdev->drv && cdev->drv->freeze) {
		ret = cdev->drv->freeze(cdev);
		if (ret)
			return ret;
	}

	spin_lock_irq(sch->lock);
	cm_enabled = cdev->private->cmb != NULL;
	spin_unlock_irq(sch->lock);
	if (cm_enabled) {
		
		ret = ccw_set_cmf(cdev, 0);
		if (ret)
			return ret;
	}
	
	spin_lock_irq(sch->lock);
	ret = cio_disable_subchannel(sch);
	spin_unlock_irq(sch->lock);

	return ret;
}

static int ccw_device_pm_thaw(struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	int ret, cm_enabled;

	if (!cdev->online)
		return 0;

	spin_lock_irq(sch->lock);
	
	ret = cio_enable_subchannel(sch, (u32)(addr_t)sch);
	cm_enabled = cdev->private->cmb != NULL;
	spin_unlock_irq(sch->lock);
	if (ret)
		return ret;

	if (cm_enabled) {
		ret = ccw_set_cmf(cdev, 1);
		if (ret)
			return ret;
	}

	if (cdev->drv && cdev->drv->thaw)
		ret = cdev->drv->thaw(cdev);

	return ret;
}

static void __ccw_device_pm_restore(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	spin_lock_irq(sch->lock);
	if (cio_is_console(sch->schid)) {
		cio_enable_subchannel(sch, (u32)(addr_t)sch);
		goto out_unlock;
	}
	cdev->private->flags.resuming = 1;
	cdev->private->path_new_mask = LPM_ANYPATH;
	css_sched_sch_todo(sch, SCH_TODO_EVAL);
	spin_unlock_irq(sch->lock);
	css_wait_for_slow_path();

	
	sch = to_subchannel(cdev->dev.parent);
	spin_lock_irq(sch->lock);
	if (cdev->private->state != DEV_STATE_ONLINE &&
	    cdev->private->state != DEV_STATE_OFFLINE)
		goto out_unlock;

	ccw_device_recognition(cdev);
	spin_unlock_irq(sch->lock);
	wait_event(cdev->private->wait_q, dev_fsm_final_state(cdev) ||
		   cdev->private->state == DEV_STATE_DISCONNECTED);
	spin_lock_irq(sch->lock);

out_unlock:
	cdev->private->flags.resuming = 0;
	spin_unlock_irq(sch->lock);
}

static int resume_handle_boxed(struct ccw_device *cdev)
{
	cdev->private->state = DEV_STATE_BOXED;
	if (ccw_device_notify(cdev, CIO_BOXED) == NOTIFY_OK)
		return 0;
	ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
	return -ENODEV;
}

static int resume_handle_disc(struct ccw_device *cdev)
{
	cdev->private->state = DEV_STATE_DISCONNECTED;
	if (ccw_device_notify(cdev, CIO_GONE) == NOTIFY_OK)
		return 0;
	ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
	return -ENODEV;
}

static int ccw_device_pm_restore(struct device *dev)
{
	struct ccw_device *cdev = to_ccwdev(dev);
	struct subchannel *sch;
	int ret = 0;

	__ccw_device_pm_restore(cdev);
	sch = to_subchannel(cdev->dev.parent);
	spin_lock_irq(sch->lock);
	if (cio_is_console(sch->schid))
		goto out_restore;

	
	switch (cdev->private->state) {
	case DEV_STATE_OFFLINE:
	case DEV_STATE_ONLINE:
		cdev->private->flags.donotify = 0;
		break;
	case DEV_STATE_BOXED:
		ret = resume_handle_boxed(cdev);
		if (ret)
			goto out_unlock;
		goto out_restore;
	default:
		ret = resume_handle_disc(cdev);
		if (ret)
			goto out_unlock;
		goto out_restore;
	}
	
	if (!ccw_device_test_sense_data(cdev)) {
		ccw_device_update_sense_data(cdev);
		ccw_device_sched_todo(cdev, CDEV_TODO_REBIND);
		ret = -ENODEV;
		goto out_unlock;
	}
	if (!cdev->online)
		goto out_unlock;

	if (ccw_device_online(cdev)) {
		ret = resume_handle_disc(cdev);
		if (ret)
			goto out_unlock;
		goto out_restore;
	}
	spin_unlock_irq(sch->lock);
	wait_event(cdev->private->wait_q, dev_fsm_final_state(cdev));
	spin_lock_irq(sch->lock);

	if (ccw_device_notify(cdev, CIO_OPER) == NOTIFY_BAD) {
		ccw_device_sched_todo(cdev, CDEV_TODO_UNREG);
		ret = -ENODEV;
		goto out_unlock;
	}

	
	if (cdev->private->cmb) {
		spin_unlock_irq(sch->lock);
		ret = ccw_set_cmf(cdev, 1);
		spin_lock_irq(sch->lock);
		if (ret) {
			CIO_MSG_EVENT(2, "resume: cdev 0.%x.%04x: cmf failed "
				      "(rc=%d)\n", cdev->private->dev_id.ssid,
				      cdev->private->dev_id.devno, ret);
			ret = 0;
		}
	}

out_restore:
	spin_unlock_irq(sch->lock);
	if (cdev->online && cdev->drv && cdev->drv->restore)
		ret = cdev->drv->restore(cdev);
	return ret;

out_unlock:
	spin_unlock_irq(sch->lock);
	return ret;
}

static const struct dev_pm_ops ccw_pm_ops = {
	.prepare = ccw_device_pm_prepare,
	.complete = ccw_device_pm_complete,
	.freeze = ccw_device_pm_freeze,
	.thaw = ccw_device_pm_thaw,
	.restore = ccw_device_pm_restore,
};

static struct bus_type ccw_bus_type = {
	.name   = "ccw",
	.match  = ccw_bus_match,
	.uevent = ccw_uevent,
	.probe  = ccw_device_probe,
	.remove = ccw_device_remove,
	.shutdown = ccw_device_shutdown,
	.pm = &ccw_pm_ops,
};

int ccw_driver_register(struct ccw_driver *cdriver)
{
	struct device_driver *drv = &cdriver->driver;

	drv->bus = &ccw_bus_type;

	return driver_register(drv);
}

void ccw_driver_unregister(struct ccw_driver *cdriver)
{
	driver_unregister(&cdriver->driver);
}

struct subchannel_id
ccw_device_get_subchannel_id(struct ccw_device *cdev)
{
	struct subchannel *sch;

	sch = to_subchannel(cdev->dev.parent);
	return sch->schid;
}

static void ccw_device_todo(struct work_struct *work)
{
	struct ccw_device_private *priv;
	struct ccw_device *cdev;
	struct subchannel *sch;
	enum cdev_todo todo;

	priv = container_of(work, struct ccw_device_private, todo_work);
	cdev = priv->cdev;
	sch = to_subchannel(cdev->dev.parent);
	
	spin_lock_irq(cdev->ccwlock);
	todo = priv->todo;
	priv->todo = CDEV_TODO_NOTHING;
	CIO_MSG_EVENT(4, "cdev_todo: cdev=0.%x.%04x todo=%d\n",
		      priv->dev_id.ssid, priv->dev_id.devno, todo);
	spin_unlock_irq(cdev->ccwlock);
	
	switch (todo) {
	case CDEV_TODO_ENABLE_CMF:
		cmf_reenable(cdev);
		break;
	case CDEV_TODO_REBIND:
		ccw_device_do_unbind_bind(cdev);
		break;
	case CDEV_TODO_REGISTER:
		io_subchannel_register(cdev);
		break;
	case CDEV_TODO_UNREG_EVAL:
		if (!sch_is_pseudo_sch(sch))
			css_schedule_eval(sch->schid);
		
	case CDEV_TODO_UNREG:
		if (sch_is_pseudo_sch(sch))
			ccw_device_unregister(cdev);
		else
			ccw_device_call_sch_unregister(cdev);
		break;
	default:
		break;
	}
	
	put_device(&cdev->dev);
}

void ccw_device_sched_todo(struct ccw_device *cdev, enum cdev_todo todo)
{
	CIO_MSG_EVENT(4, "cdev_todo: sched cdev=0.%x.%04x todo=%d\n",
		      cdev->private->dev_id.ssid, cdev->private->dev_id.devno,
		      todo);
	if (cdev->private->todo >= todo)
		return;
	cdev->private->todo = todo;
	
	if (!get_device(&cdev->dev))
		return;
	if (!queue_work(cio_work_q, &cdev->private->todo_work)) {
		
		put_device(&cdev->dev);
	}
}

int ccw_device_siosl(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	return chsc_siosl(sch->schid);
}
EXPORT_SYMBOL_GPL(ccw_device_siosl);

MODULE_LICENSE("GPL");
EXPORT_SYMBOL(ccw_device_set_online);
EXPORT_SYMBOL(ccw_device_set_offline);
EXPORT_SYMBOL(ccw_driver_register);
EXPORT_SYMBOL(ccw_driver_unregister);
EXPORT_SYMBOL(get_ccwdev_by_busid);
EXPORT_SYMBOL_GPL(ccw_device_get_subchannel_id);
