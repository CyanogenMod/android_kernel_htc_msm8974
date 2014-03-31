/*
 * IBM eServer Hypervisor Virtual Console Server Device Driver
 * Copyright (C) 2003, 2004 IBM Corp.
 *  Ryan S. Arnold (rsa@us.ibm.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Author(s) :  Ryan S. Arnold <rsa@us.ibm.com>
 *
 * This is the device driver for the IBM Hypervisor Virtual Console Server,
 * "hvcs".  The IBM hvcs provides a tty driver interface to allow Linux
 * user space applications access to the system consoles of logically
 * partitioned operating systems, e.g. Linux, running on the same partitioned
 * Power5 ppc64 system.  Physical hardware consoles per partition are not
 * practical on this hardware so system consoles are accessed by this driver
 * using inter-partition firmware interfaces to virtual terminal devices.
 *
 * A vty is known to the HMC as a "virtual serial server adapter".  It is a
 * virtual terminal device that is created by firmware upon partition creation
 * to act as a partitioned OS's console device.
 *
 * Firmware dynamically (via hotplug) exposes vty-servers to a running ppc64
 * Linux system upon their creation by the HMC or their exposure during boot.
 * The non-user interactive backend of this driver is implemented as a vio
 * device driver so that it can receive notification of vty-server lifetimes
 * after it registers with the vio bus to handle vty-server probe and remove
 * callbacks.
 *
 * Many vty-servers can be configured to connect to one vty, but a vty can
 * only be actively connected to by a single vty-server, in any manner, at one
 * time.  If the HMC is currently hosting the console for a target Linux
 * partition; attempts to open the tty device to the partition's console using
 * the hvcs on any partition will return -EBUSY with every open attempt until
 * the HMC frees the connection between its vty-server and the desired
 * partition's vty device.  Conversely, a vty-server may only be connected to
 * a single vty at one time even though it may have several configured vty
 * partner possibilities.
 *
 * Firmware does not provide notification of vty partner changes to this
 * driver.  This means that an HMC Super Admin may add or remove partner vtys
 * from a vty-server's partner list but the changes will not be signaled to
 * the vty-server.  Firmware only notifies the driver when a vty-server is
 * added or removed from the system.  To compensate for this deficiency, this
 * driver implements a sysfs update attribute which provides a method for
 * rescanning partner information upon a user's request.
 *
 * Each vty-server, prior to being exposed to this driver is reference counted
 * using the 2.6 Linux kernel kref construct.
 *
 * For direction on installation and usage of this driver please reference
 * Documentation/powerpc/hvcs.txt.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/hvconsole.h>
#include <asm/hvcserver.h>
#include <asm/uaccess.h>
#include <asm/vio.h>


#define HVCS_DRIVER_VERSION "1.3.3"

MODULE_AUTHOR("Ryan S. Arnold <rsa@us.ibm.com>");
MODULE_DESCRIPTION("IBM hvcs (Hypervisor Virtual Console Server) Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(HVCS_DRIVER_VERSION);

#define HVCS_CLOSE_WAIT (HZ/100) 

#define HVCS_DEFAULT_SERVER_ADAPTERS	64

#define HVCS_MAX_SERVER_ADAPTERS	1024

#define HVCS_MINOR_START	0

#define __ALIGNED__	__attribute__((__aligned__(8)))

#define HVCS_BUFF_LEN	16

#define HVCS_MAX_FROM_USER	4096

static struct ktermios hvcs_tty_termios = {
	.c_iflag = IGNBRK | IGNPAR,
	.c_oflag = OPOST,
	.c_cflag = B38400 | CS8 | CREAD | HUPCL,
	.c_cc = INIT_C_CC,
	.c_ispeed = 38400,
	.c_ospeed = 38400
};

static int hvcs_parm_num_devs = -1;
module_param(hvcs_parm_num_devs, int, 0);

static const char hvcs_driver_name[] = "hvcs";
static const char hvcs_device_node[] = "hvcs";
static const char hvcs_driver_string[]
	= "IBM hvcs (Hypervisor Virtual Console Server) Driver";

static int hvcs_rescan_status;

static struct tty_driver *hvcs_tty_driver;

static int *hvcs_index_list;

static int hvcs_index_count;

static int hvcs_kicked;

static struct task_struct *hvcs_task;

static unsigned long *hvcs_pi_buff;

static DEFINE_SPINLOCK(hvcs_pi_lock);

struct hvcs_struct {
	spinlock_t lock;

	unsigned int index;

	struct tty_struct *tty;
	int open_count;

	int todo_mask;

	char buffer[HVCS_BUFF_LEN];
	int chars_in_buffer;

	struct kref kref; 
	int connected; 
	uint32_t p_unit_address; 
	uint32_t p_partition_ID; 
	char p_location_code[HVCS_CLC_LENGTH + 1]; 
	struct list_head next; 
	struct vio_dev *vdev;
};

#define from_kref(k) container_of(k, struct hvcs_struct, kref)

static LIST_HEAD(hvcs_structs);
static DEFINE_SPINLOCK(hvcs_structs_lock);
static DEFINE_MUTEX(hvcs_init_mutex);

static void hvcs_unthrottle(struct tty_struct *tty);
static void hvcs_throttle(struct tty_struct *tty);
static irqreturn_t hvcs_handle_interrupt(int irq, void *dev_instance);

static int hvcs_write(struct tty_struct *tty,
		const unsigned char *buf, int count);
static int hvcs_write_room(struct tty_struct *tty);
static int hvcs_chars_in_buffer(struct tty_struct *tty);

static int hvcs_has_pi(struct hvcs_struct *hvcsd);
static void hvcs_set_pi(struct hvcs_partner_info *pi,
		struct hvcs_struct *hvcsd);
static int hvcs_get_pi(struct hvcs_struct *hvcsd);
static int hvcs_rescan_devices_list(void);

static int hvcs_partner_connect(struct hvcs_struct *hvcsd);
static void hvcs_partner_free(struct hvcs_struct *hvcsd);

static int hvcs_enable_device(struct hvcs_struct *hvcsd,
		uint32_t unit_address, unsigned int irq, struct vio_dev *dev);

static int hvcs_open(struct tty_struct *tty, struct file *filp);
static void hvcs_close(struct tty_struct *tty, struct file *filp);
static void hvcs_hangup(struct tty_struct * tty);

static int __devinit hvcs_probe(struct vio_dev *dev,
		const struct vio_device_id *id);
static int __devexit hvcs_remove(struct vio_dev *dev);
static int __init hvcs_module_init(void);
static void __exit hvcs_module_exit(void);
static int __devinit hvcs_initialize(void);

#define HVCS_SCHED_READ	0x00000001
#define HVCS_QUICK_READ	0x00000002
#define HVCS_TRY_WRITE	0x00000004
#define HVCS_READ_MASK	(HVCS_SCHED_READ | HVCS_QUICK_READ)

static inline struct hvcs_struct *from_vio_dev(struct vio_dev *viod)
{
	return dev_get_drvdata(&viod->dev);
}

static ssize_t hvcs_partner_vtys_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&hvcsd->lock, flags);
	retval = sprintf(buf, "%X\n", hvcsd->p_unit_address);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return retval;
}
static DEVICE_ATTR(partner_vtys, S_IRUGO, hvcs_partner_vtys_show, NULL);

static ssize_t hvcs_partner_clcs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&hvcsd->lock, flags);
	retval = sprintf(buf, "%s\n", &hvcsd->p_location_code[0]);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return retval;
}
static DEVICE_ATTR(partner_clcs, S_IRUGO, hvcs_partner_clcs_show, NULL);

static ssize_t hvcs_current_vty_store(struct device *dev, struct device_attribute *attr, const char * buf,
		size_t count)
{
	printk(KERN_INFO "HVCS: Denied current_vty change: -EPERM.\n");
	return -EPERM;
}

static ssize_t hvcs_current_vty_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&hvcsd->lock, flags);
	retval = sprintf(buf, "%s\n", &hvcsd->p_location_code[0]);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return retval;
}

static DEVICE_ATTR(current_vty,
	S_IRUGO | S_IWUSR, hvcs_current_vty_show, hvcs_current_vty_store);

static ssize_t hvcs_vterm_state_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;

	
	if (simple_strtol(buf, NULL, 0) != 0)
		return -EINVAL;

	spin_lock_irqsave(&hvcsd->lock, flags);

	if (hvcsd->open_count > 0) {
		spin_unlock_irqrestore(&hvcsd->lock, flags);
		printk(KERN_INFO "HVCS: vterm state unchanged.  "
				"The hvcs device node is still in use.\n");
		return -EPERM;
	}

	if (hvcsd->connected == 0) {
		spin_unlock_irqrestore(&hvcsd->lock, flags);
		printk(KERN_INFO "HVCS: vterm state unchanged. The"
				" vty-server is not connected to a vty.\n");
		return -EPERM;
	}

	hvcs_partner_free(hvcsd);
	printk(KERN_INFO "HVCS: Closed vty-server@%X and"
			" partner vty@%X:%d connection.\n",
			hvcsd->vdev->unit_address,
			hvcsd->p_unit_address,
			(uint32_t)hvcsd->p_partition_ID);

	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return count;
}

static ssize_t hvcs_vterm_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&hvcsd->lock, flags);
	retval = sprintf(buf, "%d\n", hvcsd->connected);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return retval;
}
static DEVICE_ATTR(vterm_state, S_IRUGO | S_IWUSR,
		hvcs_vterm_state_show, hvcs_vterm_state_store);

static ssize_t hvcs_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_dev *viod = to_vio_dev(dev);
	struct hvcs_struct *hvcsd = from_vio_dev(viod);
	unsigned long flags;
	int retval;

	spin_lock_irqsave(&hvcsd->lock, flags);
	retval = sprintf(buf, "%d\n", hvcsd->index);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return retval;
}

static DEVICE_ATTR(index, S_IRUGO, hvcs_index_show, NULL);

static struct attribute *hvcs_attrs[] = {
	&dev_attr_partner_vtys.attr,
	&dev_attr_partner_clcs.attr,
	&dev_attr_current_vty.attr,
	&dev_attr_vterm_state.attr,
	&dev_attr_index.attr,
	NULL,
};

static struct attribute_group hvcs_attr_group = {
	.attrs = hvcs_attrs,
};

static ssize_t hvcs_rescan_show(struct device_driver *ddp, char *buf)
{
	
	return snprintf(buf, PAGE_SIZE, "%d\n", hvcs_rescan_status);
}

static ssize_t hvcs_rescan_store(struct device_driver *ddp, const char * buf,
		size_t count)
{
	if ((simple_strtol(buf, NULL, 0) != 1)
		&& (hvcs_rescan_status != 0))
		return -EINVAL;

	hvcs_rescan_status = 1;
	printk(KERN_INFO "HVCS: rescanning partner info for all"
		" vty-servers.\n");
	hvcs_rescan_devices_list();
	hvcs_rescan_status = 0;
	return count;
}

static DRIVER_ATTR(rescan,
	S_IRUGO | S_IWUSR, hvcs_rescan_show, hvcs_rescan_store);

static void hvcs_kick(void)
{
	hvcs_kicked = 1;
	wmb();
	wake_up_process(hvcs_task);
}

static void hvcs_unthrottle(struct tty_struct *tty)
{
	struct hvcs_struct *hvcsd = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&hvcsd->lock, flags);
	hvcsd->todo_mask |= HVCS_SCHED_READ;
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	hvcs_kick();
}

static void hvcs_throttle(struct tty_struct *tty)
{
	struct hvcs_struct *hvcsd = tty->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&hvcsd->lock, flags);
	vio_disable_interrupts(hvcsd->vdev);
	spin_unlock_irqrestore(&hvcsd->lock, flags);
}

static irqreturn_t hvcs_handle_interrupt(int irq, void *dev_instance)
{
	struct hvcs_struct *hvcsd = dev_instance;

	spin_lock(&hvcsd->lock);
	vio_disable_interrupts(hvcsd->vdev);
	hvcsd->todo_mask |= HVCS_SCHED_READ;
	spin_unlock(&hvcsd->lock);
	hvcs_kick();

	return IRQ_HANDLED;
}

static void hvcs_try_write(struct hvcs_struct *hvcsd)
{
	uint32_t unit_address = hvcsd->vdev->unit_address;
	struct tty_struct *tty = hvcsd->tty;
	int sent;

	if (hvcsd->todo_mask & HVCS_TRY_WRITE) {
		
		sent = hvc_put_chars(unit_address,
				&hvcsd->buffer[0],
				hvcsd->chars_in_buffer );
		if (sent > 0) {
			hvcsd->chars_in_buffer = 0;
			
			hvcsd->todo_mask &= ~(HVCS_TRY_WRITE);
			

			if (tty) {
				tty_wakeup(tty);
			}
		}
	}
}

static int hvcs_io(struct hvcs_struct *hvcsd)
{
	uint32_t unit_address;
	struct tty_struct *tty;
	char buf[HVCS_BUFF_LEN] __ALIGNED__;
	unsigned long flags;
	int got = 0;

	spin_lock_irqsave(&hvcsd->lock, flags);

	unit_address = hvcsd->vdev->unit_address;
	tty = hvcsd->tty;

	hvcs_try_write(hvcsd);

	if (!tty || test_bit(TTY_THROTTLED, &tty->flags)) {
		hvcsd->todo_mask &= ~(HVCS_READ_MASK);
		goto bail;
	} else if (!(hvcsd->todo_mask & (HVCS_READ_MASK)))
		goto bail;

	
	hvcsd->todo_mask &= ~(HVCS_READ_MASK);

	if (tty_buffer_request_room(tty, HVCS_BUFF_LEN) >= HVCS_BUFF_LEN) {
		got = hvc_get_chars(unit_address,
				&buf[0],
				HVCS_BUFF_LEN);
		tty_insert_flip_string(tty, buf, got);
	}

	
	if (got)
		hvcsd->todo_mask |= HVCS_QUICK_READ;

	spin_unlock_irqrestore(&hvcsd->lock, flags);
	
	if(got)
		tty_flip_buffer_push(tty);

	if (!got) {
		
		spin_lock_irqsave(&hvcsd->lock, flags);
		vio_enable_interrupts(hvcsd->vdev);
		spin_unlock_irqrestore(&hvcsd->lock, flags);
	}

	return hvcsd->todo_mask;

 bail:
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	return hvcsd->todo_mask;
}

static int khvcsd(void *unused)
{
	struct hvcs_struct *hvcsd;
	int hvcs_todo_mask;

	__set_current_state(TASK_RUNNING);

	do {
		hvcs_todo_mask = 0;
		hvcs_kicked = 0;
		wmb();

		spin_lock(&hvcs_structs_lock);
		list_for_each_entry(hvcsd, &hvcs_structs, next) {
			hvcs_todo_mask |= hvcs_io(hvcsd);
		}
		spin_unlock(&hvcs_structs_lock);

		 if (hvcs_todo_mask & (HVCS_TRY_WRITE | HVCS_QUICK_READ)) {
			yield();
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		if (!hvcs_kicked)
			schedule();
		__set_current_state(TASK_RUNNING);
	} while (!kthread_should_stop());

	return 0;
}

static struct vio_device_id hvcs_driver_table[] __devinitdata= {
	{"serial-server", "hvterm2"},
	{ "", "" }
};
MODULE_DEVICE_TABLE(vio, hvcs_driver_table);

static void hvcs_return_index(int index)
{
	
	if (!hvcs_index_list)
		return;
	if (index < 0 || index >= hvcs_index_count)
		return;
	if (hvcs_index_list[index] == -1)
		return;
	else
		hvcs_index_list[index] = -1;
}

static void destroy_hvcs_struct(struct kref *kref)
{
	struct hvcs_struct *hvcsd = from_kref(kref);
	struct vio_dev *vdev;
	unsigned long flags;

	spin_lock(&hvcs_structs_lock);
	spin_lock_irqsave(&hvcsd->lock, flags);

	
	list_del(&(hvcsd->next));

	if (hvcsd->connected == 1) {
		hvcs_partner_free(hvcsd);
		printk(KERN_INFO "HVCS: Closed vty-server@%X and"
				" partner vty@%X:%d connection.\n",
				hvcsd->vdev->unit_address,
				hvcsd->p_unit_address,
				(uint32_t)hvcsd->p_partition_ID);
	}
	printk(KERN_INFO "HVCS: Destroyed hvcs_struct for vty-server@%X.\n",
			hvcsd->vdev->unit_address);

	vdev = hvcsd->vdev;
	hvcsd->vdev = NULL;

	hvcsd->p_unit_address = 0;
	hvcsd->p_partition_ID = 0;
	hvcs_return_index(hvcsd->index);
	memset(&hvcsd->p_location_code[0], 0x00, HVCS_CLC_LENGTH + 1);

	spin_unlock_irqrestore(&hvcsd->lock, flags);
	spin_unlock(&hvcs_structs_lock);

	sysfs_remove_group(&vdev->dev.kobj, &hvcs_attr_group);

	kfree(hvcsd);
}

static int hvcs_get_index(void)
{
	int i;
	
	if (!hvcs_index_list) {
		printk(KERN_ERR "HVCS: hvcs_index_list NOT valid!.\n");
		return -EFAULT;
	}
	
	for(i = 0; i < hvcs_index_count; i++) {
		if (hvcs_index_list[i] == -1) {
			hvcs_index_list[i] = 0;
			return i;
		}
	}
	return -1;
}

static int __devinit hvcs_probe(
	struct vio_dev *dev,
	const struct vio_device_id *id)
{
	struct hvcs_struct *hvcsd;
	int index, rc;
	int retval;

	if (!dev || !id) {
		printk(KERN_ERR "HVCS: probed with invalid parameter.\n");
		return -EPERM;
	}

	
	rc = hvcs_initialize();
	if (rc) {
		pr_err("HVCS: Failed to initialize core driver.\n");
		return rc;
	}

	
	index = hvcs_get_index();
	if (index < 0) {
		return -EFAULT;
	}

	hvcsd = kzalloc(sizeof(*hvcsd), GFP_KERNEL);
	if (!hvcsd)
		return -ENODEV;


	spin_lock_init(&hvcsd->lock);
	
	kref_init(&hvcsd->kref);

	hvcsd->vdev = dev;
	dev_set_drvdata(&dev->dev, hvcsd);

	hvcsd->index = index;

	
	hvcsd->chars_in_buffer = 0;
	hvcsd->todo_mask = 0;
	hvcsd->connected = 0;

	if (hvcs_get_pi(hvcsd)) {
		printk(KERN_ERR "HVCS: Failed to fetch partner"
			" info for vty-server@%X on device probe.\n",
			hvcsd->vdev->unit_address);
	}

	spin_lock(&hvcs_structs_lock);
	list_add_tail(&(hvcsd->next), &hvcs_structs);
	spin_unlock(&hvcs_structs_lock);

	retval = sysfs_create_group(&dev->dev.kobj, &hvcs_attr_group);
	if (retval) {
		printk(KERN_ERR "HVCS: Can't create sysfs attrs for vty-server@%X\n",
		       hvcsd->vdev->unit_address);
		return retval;
	}

	printk(KERN_INFO "HVCS: vty-server@%X added to the vio bus.\n", dev->unit_address);

	return 0;
}

static int __devexit hvcs_remove(struct vio_dev *dev)
{
	struct hvcs_struct *hvcsd = dev_get_drvdata(&dev->dev);
	unsigned long flags;
	struct tty_struct *tty;

	if (!hvcsd)
		return -ENODEV;

	

	spin_lock_irqsave(&hvcsd->lock, flags);

	tty = hvcsd->tty;

	spin_unlock_irqrestore(&hvcsd->lock, flags);

	kref_put(&hvcsd->kref, destroy_hvcs_struct);

	if (tty)
		tty_hangup(tty);

	printk(KERN_INFO "HVCS: vty-server@%X removed from the"
			" vio bus.\n", dev->unit_address);
	return 0;
};

static struct vio_driver hvcs_vio_driver = {
	.id_table	= hvcs_driver_table,
	.probe		= hvcs_probe,
	.remove		= __devexit_p(hvcs_remove),
	.name		= hvcs_driver_name,
};

static void hvcs_set_pi(struct hvcs_partner_info *pi, struct hvcs_struct *hvcsd)
{
	int clclength;

	hvcsd->p_unit_address = pi->unit_address;
	hvcsd->p_partition_ID  = pi->partition_ID;
	clclength = strlen(&pi->location_code[0]);
	if (clclength > HVCS_CLC_LENGTH)
		clclength = HVCS_CLC_LENGTH;

	
	strncpy(&hvcsd->p_location_code[0],
			&pi->location_code[0], clclength + 1);
}

static int hvcs_get_pi(struct hvcs_struct *hvcsd)
{
	struct hvcs_partner_info *pi;
	uint32_t unit_address = hvcsd->vdev->unit_address;
	struct list_head head;
	int retval;

	spin_lock(&hvcs_pi_lock);
	if (!hvcs_pi_buff) {
		spin_unlock(&hvcs_pi_lock);
		return -EFAULT;
	}
	retval = hvcs_get_partner_info(unit_address, &head, hvcs_pi_buff);
	spin_unlock(&hvcs_pi_lock);
	if (retval) {
		printk(KERN_ERR "HVCS: Failed to fetch partner"
			" info for vty-server@%x.\n", unit_address);
		return retval;
	}

	
	hvcsd->p_unit_address = 0;
	hvcsd->p_partition_ID = 0;

	list_for_each_entry(pi, &head, node)
		hvcs_set_pi(pi, hvcsd);

	hvcs_free_partner_info(&head);
	return 0;
}

static int hvcs_rescan_devices_list(void)
{
	struct hvcs_struct *hvcsd;
	unsigned long flags;

	spin_lock(&hvcs_structs_lock);

	list_for_each_entry(hvcsd, &hvcs_structs, next) {
		spin_lock_irqsave(&hvcsd->lock, flags);
		hvcs_get_pi(hvcsd);
		spin_unlock_irqrestore(&hvcsd->lock, flags);
	}

	spin_unlock(&hvcs_structs_lock);

	return 0;
}

static int hvcs_has_pi(struct hvcs_struct *hvcsd)
{
	if ((!hvcsd->p_unit_address) || (!hvcsd->p_partition_ID))
		return 0;
	return 1;
}

static int hvcs_partner_connect(struct hvcs_struct *hvcsd)
{
	int retval;
	unsigned int unit_address = hvcsd->vdev->unit_address;

	retval = hvcs_register_connection(unit_address,
			hvcsd->p_partition_ID,
			hvcsd->p_unit_address);
	if (!retval) {
		hvcsd->connected = 1;
		return 0;
	} else if (retval != -EINVAL)
		return retval;

	if (hvcs_get_pi(hvcsd))
		return -ENOMEM;

	if (!hvcs_has_pi(hvcsd))
		return -ENODEV;

	retval = hvcs_register_connection(unit_address,
			hvcsd->p_partition_ID,
			hvcsd->p_unit_address);
	if (retval != -EINVAL) {
		hvcsd->connected = 1;
		return retval;
	}

	printk(KERN_INFO "HVCS: vty-server or partner"
			" vty is busy.  Try again later.\n");
	return -EBUSY;
}

static void hvcs_partner_free(struct hvcs_struct *hvcsd)
{
	int retval;
	do {
		retval = hvcs_free_connection(hvcsd->vdev->unit_address);
	} while (retval == -EBUSY);
	hvcsd->connected = 0;
}

static int hvcs_enable_device(struct hvcs_struct *hvcsd, uint32_t unit_address,
		unsigned int irq, struct vio_dev *vdev)
{
	unsigned long flags;
	int rc;

	if (!(rc = request_irq(irq, &hvcs_handle_interrupt,
				0, "ibmhvcs", hvcsd))) {
		if (vio_enable_interrupts(vdev) == H_SUCCESS)
			return 0;
		else {
			printk(KERN_ERR "HVCS: int enable failed for"
					" vty-server@%X.\n", unit_address);
			free_irq(irq, hvcsd);
		}
	} else
		printk(KERN_ERR "HVCS: irq req failed for"
				" vty-server@%X.\n", unit_address);

	spin_lock_irqsave(&hvcsd->lock, flags);
	hvcs_partner_free(hvcsd);
	spin_unlock_irqrestore(&hvcsd->lock, flags);

	return rc;

}

static struct hvcs_struct *hvcs_get_by_index(int index)
{
	struct hvcs_struct *hvcsd;
	unsigned long flags;

	spin_lock(&hvcs_structs_lock);
	list_for_each_entry(hvcsd, &hvcs_structs, next) {
		spin_lock_irqsave(&hvcsd->lock, flags);
		if (hvcsd->index == index) {
			kref_get(&hvcsd->kref);
			spin_unlock_irqrestore(&hvcsd->lock, flags);
			spin_unlock(&hvcs_structs_lock);
			return hvcsd;
		}
		spin_unlock_irqrestore(&hvcsd->lock, flags);
	}
	spin_unlock(&hvcs_structs_lock);

	return NULL;
}

static int hvcs_open(struct tty_struct *tty, struct file *filp)
{
	struct hvcs_struct *hvcsd;
	int rc, retval = 0;
	unsigned long flags;
	unsigned int irq;
	struct vio_dev *vdev;
	unsigned long unit_address;

	if (tty->driver_data)
		goto fast_open;

	if (!(hvcsd = hvcs_get_by_index(tty->index))) {
		printk(KERN_WARNING "HVCS: open failed, no device associated"
				" with tty->index %d.\n", tty->index);
		return -ENODEV;
	}

	spin_lock_irqsave(&hvcsd->lock, flags);

	if (hvcsd->connected == 0)
		if ((retval = hvcs_partner_connect(hvcsd)))
			goto error_release;

	hvcsd->open_count = 1;
	hvcsd->tty = tty;
	tty->driver_data = hvcsd;

	memset(&hvcsd->buffer[0], 0x00, HVCS_BUFF_LEN);

	irq = hvcsd->vdev->irq;
	vdev = hvcsd->vdev;
	unit_address = hvcsd->vdev->unit_address;

	hvcsd->todo_mask |= HVCS_SCHED_READ;
	spin_unlock_irqrestore(&hvcsd->lock, flags);

	if (((rc = hvcs_enable_device(hvcsd, unit_address, irq, vdev)))) {
		kref_put(&hvcsd->kref, destroy_hvcs_struct);
		printk(KERN_WARNING "HVCS: enable device failed.\n");
		return rc;
	}

	goto open_success;

fast_open:
	hvcsd = tty->driver_data;

	spin_lock_irqsave(&hvcsd->lock, flags);
	kref_get(&hvcsd->kref);
	hvcsd->open_count++;
	hvcsd->todo_mask |= HVCS_SCHED_READ;
	spin_unlock_irqrestore(&hvcsd->lock, flags);

open_success:
	hvcs_kick();

	printk(KERN_INFO "HVCS: vty-server@%X connection opened.\n",
		hvcsd->vdev->unit_address );

	return 0;

error_release:
	spin_unlock_irqrestore(&hvcsd->lock, flags);
	kref_put(&hvcsd->kref, destroy_hvcs_struct);

	printk(KERN_WARNING "HVCS: partner connect failed.\n");
	return retval;
}

static void hvcs_close(struct tty_struct *tty, struct file *filp)
{
	struct hvcs_struct *hvcsd;
	unsigned long flags;
	int irq;

	if (tty_hung_up_p(filp))
		return;

	if (!tty->driver_data)
		return;

	hvcsd = tty->driver_data;

	spin_lock_irqsave(&hvcsd->lock, flags);
	if (--hvcsd->open_count == 0) {

		vio_disable_interrupts(hvcsd->vdev);

		hvcsd->tty = NULL;

		irq = hvcsd->vdev->irq;
		spin_unlock_irqrestore(&hvcsd->lock, flags);

		tty_wait_until_sent_from_close(tty, HVCS_CLOSE_WAIT);

		tty->driver_data = NULL;

		free_irq(irq, hvcsd);
		kref_put(&hvcsd->kref, destroy_hvcs_struct);
		return;
	} else if (hvcsd->open_count < 0) {
		printk(KERN_ERR "HVCS: vty-server@%X open_count: %d"
				" is missmanaged.\n",
		hvcsd->vdev->unit_address, hvcsd->open_count);
	}

	spin_unlock_irqrestore(&hvcsd->lock, flags);
	kref_put(&hvcsd->kref, destroy_hvcs_struct);
}

static void hvcs_hangup(struct tty_struct * tty)
{
	struct hvcs_struct *hvcsd = tty->driver_data;
	unsigned long flags;
	int temp_open_count;
	int irq;

	spin_lock_irqsave(&hvcsd->lock, flags);
	
	temp_open_count = hvcsd->open_count;

	vio_disable_interrupts(hvcsd->vdev);

	hvcsd->todo_mask = 0;

	
	hvcsd->tty->driver_data = NULL;
	hvcsd->tty = NULL;

	hvcsd->open_count = 0;

	memset(&hvcsd->buffer[0], 0x00, HVCS_BUFF_LEN);
	hvcsd->chars_in_buffer = 0;

	irq = hvcsd->vdev->irq;

	spin_unlock_irqrestore(&hvcsd->lock, flags);

	free_irq(irq, hvcsd);

	while(temp_open_count) {
		--temp_open_count;
		kref_put(&hvcsd->kref, destroy_hvcs_struct);
	}
}

static int hvcs_write(struct tty_struct *tty,
		const unsigned char *buf, int count)
{
	struct hvcs_struct *hvcsd = tty->driver_data;
	unsigned int unit_address;
	const unsigned char *charbuf;
	unsigned long flags;
	int total_sent = 0;
	int tosend = 0;
	int result = 0;

	if (!hvcsd)
		return -ENODEV;

	
	if (count > HVCS_MAX_FROM_USER) {
		printk(KERN_WARNING "HVCS write: count being truncated to"
				" HVCS_MAX_FROM_USER.\n");
		count = HVCS_MAX_FROM_USER;
	}

	charbuf = buf;

	spin_lock_irqsave(&hvcsd->lock, flags);

	if (hvcsd->open_count <= 0) {
		spin_unlock_irqrestore(&hvcsd->lock, flags);
		return -ENODEV;
	}

	unit_address = hvcsd->vdev->unit_address;

	while (count > 0) {
		tosend = min(count, (HVCS_BUFF_LEN - hvcsd->chars_in_buffer));
		if (!tosend)
			break;

		memcpy(&hvcsd->buffer[hvcsd->chars_in_buffer],
				&charbuf[total_sent],
				tosend);

		hvcsd->chars_in_buffer += tosend;

		result = 0;

		if (!(hvcsd->todo_mask & HVCS_TRY_WRITE))
			
			result = hvc_put_chars(unit_address,
					&hvcsd->buffer[0],
					hvcsd->chars_in_buffer);

		total_sent+=tosend;
		count-=tosend;
		if (result == 0) {
			hvcsd->todo_mask |= HVCS_TRY_WRITE;
			hvcs_kick();
			break;
		}

		hvcsd->chars_in_buffer = 0;
		if (result < 0)
			break;
	}

	spin_unlock_irqrestore(&hvcsd->lock, flags);

	if (result == -1)
		return -EIO;
	else
		return total_sent;
}

static int hvcs_write_room(struct tty_struct *tty)
{
	struct hvcs_struct *hvcsd = tty->driver_data;

	if (!hvcsd || hvcsd->open_count <= 0)
		return 0;

	return HVCS_BUFF_LEN - hvcsd->chars_in_buffer;
}

static int hvcs_chars_in_buffer(struct tty_struct *tty)
{
	struct hvcs_struct *hvcsd = tty->driver_data;

	return hvcsd->chars_in_buffer;
}

static const struct tty_operations hvcs_ops = {
	.open = hvcs_open,
	.close = hvcs_close,
	.hangup = hvcs_hangup,
	.write = hvcs_write,
	.write_room = hvcs_write_room,
	.chars_in_buffer = hvcs_chars_in_buffer,
	.unthrottle = hvcs_unthrottle,
	.throttle = hvcs_throttle,
};

static int hvcs_alloc_index_list(int n)
{
	int i;

	hvcs_index_list = kmalloc(n * sizeof(hvcs_index_count),GFP_KERNEL);
	if (!hvcs_index_list)
		return -ENOMEM;
	hvcs_index_count = n;
	for (i = 0; i < hvcs_index_count; i++)
		hvcs_index_list[i] = -1;
	return 0;
}

static void hvcs_free_index_list(void)
{
	
	kfree(hvcs_index_list);
	hvcs_index_list = NULL;
	hvcs_index_count = 0;
}

static int __devinit hvcs_initialize(void)
{
	int rc, num_ttys_to_alloc;

	mutex_lock(&hvcs_init_mutex);
	if (hvcs_task) {
		mutex_unlock(&hvcs_init_mutex);
		return 0;
	}

	
	if (hvcs_parm_num_devs <= 0 ||
		(hvcs_parm_num_devs > HVCS_MAX_SERVER_ADAPTERS)) {
		num_ttys_to_alloc = HVCS_DEFAULT_SERVER_ADAPTERS;
	} else
		num_ttys_to_alloc = hvcs_parm_num_devs;

	hvcs_tty_driver = alloc_tty_driver(num_ttys_to_alloc);
	if (!hvcs_tty_driver)
		return -ENOMEM;

	if (hvcs_alloc_index_list(num_ttys_to_alloc)) {
		rc = -ENOMEM;
		goto index_fail;
	}

	hvcs_tty_driver->driver_name = hvcs_driver_name;
	hvcs_tty_driver->name = hvcs_device_node;


	hvcs_tty_driver->minor_start = HVCS_MINOR_START;
	hvcs_tty_driver->type = TTY_DRIVER_TYPE_SYSTEM;

	hvcs_tty_driver->init_termios = hvcs_tty_termios;
	hvcs_tty_driver->flags = TTY_DRIVER_REAL_RAW;

	tty_set_operations(hvcs_tty_driver, &hvcs_ops);

	if (tty_register_driver(hvcs_tty_driver)) {
		printk(KERN_ERR "HVCS: registration as a tty driver failed.\n");
		rc = -EIO;
		goto register_fail;
	}

	hvcs_pi_buff = (unsigned long *) __get_free_page(GFP_KERNEL);
	if (!hvcs_pi_buff) {
		rc = -ENOMEM;
		goto buff_alloc_fail;
	}

	hvcs_task = kthread_run(khvcsd, NULL, "khvcsd");
	if (IS_ERR(hvcs_task)) {
		printk(KERN_ERR "HVCS: khvcsd creation failed.\n");
		rc = -EIO;
		goto kthread_fail;
	}
	mutex_unlock(&hvcs_init_mutex);
	return 0;

kthread_fail:
	free_page((unsigned long)hvcs_pi_buff);
buff_alloc_fail:
	tty_unregister_driver(hvcs_tty_driver);
register_fail:
	hvcs_free_index_list();
index_fail:
	put_tty_driver(hvcs_tty_driver);
	hvcs_tty_driver = NULL;
	mutex_unlock(&hvcs_init_mutex);
	return rc;
}

static int __init hvcs_module_init(void)
{
	int rc = vio_register_driver(&hvcs_vio_driver);
	if (rc) {
		printk(KERN_ERR "HVCS: can't register vio driver\n");
		return rc;
	}

	pr_info("HVCS: Driver registered.\n");

	rc = driver_create_file(&(hvcs_vio_driver.driver), &driver_attr_rescan);
	if (rc)
		pr_warning(KERN_ERR "HVCS: Failed to create rescan file (err %d)\n", rc);

	return 0;
}

static void __exit hvcs_module_exit(void)
{
	vio_unregister_driver(&hvcs_vio_driver);
	if (!hvcs_task)
		return;

	kthread_stop(hvcs_task);

	spin_lock(&hvcs_pi_lock);
	free_page((unsigned long)hvcs_pi_buff);
	hvcs_pi_buff = NULL;
	spin_unlock(&hvcs_pi_lock);

	driver_remove_file(&hvcs_vio_driver.driver, &driver_attr_rescan);

	tty_unregister_driver(hvcs_tty_driver);

	hvcs_free_index_list();

	put_tty_driver(hvcs_tty_driver);

	printk(KERN_INFO "HVCS: driver module removed.\n");
}

module_init(hvcs_module_init);
module_exit(hvcs_module_exit);
