/*
 * Freescale Hypervisor Management Driver

 * Copyright (C) 2008-2011 Freescale Semiconductor, Inc.
 * Author: Timur Tabi <timur@freescale.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * The Freescale hypervisor management driver provides several services to
 * drivers and applications related to the Freescale hypervisor:
 *
 * 1. An ioctl interface for querying and managing partitions.
 *
 * 2. A file interface to reading incoming doorbells.
 *
 * 3. An interrupt handler for shutting down the partition upon receiving the
 *    shutdown doorbell from a manager partition.
 *
 * 4. A kernel interface for receiving callbacks when a managed partition
 *    shuts down.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>

#include <linux/io.h>
#include <asm/fsl_hcalls.h>

#include <linux/fsl_hypervisor.h>

static BLOCKING_NOTIFIER_HEAD(failover_subscribers);

static long ioctl_restart(struct fsl_hv_ioctl_restart __user *p)
{
	struct fsl_hv_ioctl_restart param;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_restart)))
		return -EFAULT;

	param.ret = fh_partition_restart(param.partition);

	if (copy_to_user(&p->ret, &param.ret, sizeof(__u32)))
		return -EFAULT;

	return 0;
}

static long ioctl_status(struct fsl_hv_ioctl_status __user *p)
{
	struct fsl_hv_ioctl_status param;
	u32 status;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_status)))
		return -EFAULT;

	param.ret = fh_partition_get_status(param.partition, &status);
	if (!param.ret)
		param.status = status;

	if (copy_to_user(p, &param, sizeof(struct fsl_hv_ioctl_status)))
		return -EFAULT;

	return 0;
}

static long ioctl_start(struct fsl_hv_ioctl_start __user *p)
{
	struct fsl_hv_ioctl_start param;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_start)))
		return -EFAULT;

	param.ret = fh_partition_start(param.partition, param.entry_point,
				       param.load);

	if (copy_to_user(&p->ret, &param.ret, sizeof(__u32)))
		return -EFAULT;

	return 0;
}

static long ioctl_stop(struct fsl_hv_ioctl_stop __user *p)
{
	struct fsl_hv_ioctl_stop param;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_stop)))
		return -EFAULT;

	param.ret = fh_partition_stop(param.partition);

	if (copy_to_user(&p->ret, &param.ret, sizeof(__u32)))
		return -EFAULT;

	return 0;
}

static long ioctl_memcpy(struct fsl_hv_ioctl_memcpy __user *p)
{
	struct fsl_hv_ioctl_memcpy param;

	struct page **pages = NULL;
	void *sg_list_unaligned = NULL;
	struct fh_sg_list *sg_list = NULL;

	unsigned int num_pages;
	unsigned long lb_offset; 

	unsigned int i;
	long ret = 0;
	int num_pinned; 
	phys_addr_t remote_paddr; 
	uint32_t count; 

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_memcpy)))
		return -EFAULT;

	if ((param.source == -1) == (param.target == -1))
		return -EINVAL;

	lb_offset = param.local_vaddr & (PAGE_SIZE - 1);
	num_pages = (param.count + lb_offset + PAGE_SIZE - 1) >> PAGE_SHIFT;

	

	pages = kzalloc(num_pages * sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_debug("fsl-hv: could not allocate page list\n");
		return -ENOMEM;
	}

	sg_list_unaligned = kmalloc(num_pages * sizeof(struct fh_sg_list) +
		sizeof(struct fh_sg_list) - 1, GFP_KERNEL);
	if (!sg_list_unaligned) {
		pr_debug("fsl-hv: could not allocate S/G list\n");
		ret = -ENOMEM;
		goto exit;
	}
	sg_list = PTR_ALIGN(sg_list_unaligned, sizeof(struct fh_sg_list));

	
	down_read(&current->mm->mmap_sem);
	num_pinned = get_user_pages(current, current->mm,
		param.local_vaddr - lb_offset, num_pages,
		(param.source == -1) ? READ : WRITE,
		0, pages, NULL);
	up_read(&current->mm->mmap_sem);

	if (num_pinned != num_pages) {
		
		pr_debug("fsl-hv: could not lock source buffer\n");
		ret = (num_pinned < 0) ? num_pinned : -EFAULT;
		goto exit;
	}

	if (param.source == -1) {
		sg_list[0].source = page_to_phys(pages[0]) + lb_offset;
		sg_list[0].target = param.remote_paddr;
	} else {
		sg_list[0].source = param.remote_paddr;
		sg_list[0].target = page_to_phys(pages[0]) + lb_offset;
	}
	sg_list[0].size = min_t(uint64_t, param.count, PAGE_SIZE - lb_offset);

	remote_paddr = param.remote_paddr + sg_list[0].size;
	count = param.count - sg_list[0].size;

	for (i = 1; i < num_pages; i++) {
		if (param.source == -1) {
			
			sg_list[i].source = page_to_phys(pages[i]);
			sg_list[i].target = remote_paddr;
		} else {
			
			sg_list[i].source = remote_paddr;
			sg_list[i].target = page_to_phys(pages[i]);
		}
		sg_list[i].size = min_t(uint64_t, count, PAGE_SIZE);

		remote_paddr += sg_list[i].size;
		count -= sg_list[i].size;
	}

	param.ret = fh_partition_memcpy(param.source, param.target,
		virt_to_phys(sg_list), num_pages);

exit:
	if (pages) {
		for (i = 0; i < num_pages; i++)
			if (pages[i])
				put_page(pages[i]);
	}

	kfree(sg_list_unaligned);
	kfree(pages);

	if (!ret)
		if (copy_to_user(&p->ret, &param.ret, sizeof(__u32)))
			return -EFAULT;

	return ret;
}

static long ioctl_doorbell(struct fsl_hv_ioctl_doorbell __user *p)
{
	struct fsl_hv_ioctl_doorbell param;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_doorbell)))
		return -EFAULT;

	param.ret = ev_doorbell_send(param.doorbell);

	if (copy_to_user(&p->ret, &param.ret, sizeof(__u32)))
		return -EFAULT;

	return 0;
}

static long ioctl_dtprop(struct fsl_hv_ioctl_prop __user *p, int set)
{
	struct fsl_hv_ioctl_prop param;
	char __user *upath, *upropname;
	void __user *upropval;
	char *path = NULL, *propname = NULL;
	void *propval = NULL;
	int ret = 0;

	
	if (copy_from_user(&param, p, sizeof(struct fsl_hv_ioctl_prop)))
		return -EFAULT;

	upath = (char __user *)(uintptr_t)param.path;
	upropname = (char __user *)(uintptr_t)param.propname;
	upropval = (void __user *)(uintptr_t)param.propval;

	path = strndup_user(upath, FH_DTPROP_MAX_PATHLEN);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		goto out;
	}

	propname = strndup_user(upropname, FH_DTPROP_MAX_PATHLEN);
	if (IS_ERR(propname)) {
		ret = PTR_ERR(propname);
		goto out;
	}

	if (param.proplen > FH_DTPROP_MAX_PROPLEN) {
		ret = -EINVAL;
		goto out;
	}

	propval = kmalloc(param.proplen, GFP_KERNEL);
	if (!propval) {
		ret = -ENOMEM;
		goto out;
	}

	if (set) {
		if (copy_from_user(propval, upropval, param.proplen)) {
			ret = -EFAULT;
			goto out;
		}

		param.ret = fh_partition_set_dtprop(param.handle,
						    virt_to_phys(path),
						    virt_to_phys(propname),
						    virt_to_phys(propval),
						    param.proplen);
	} else {
		param.ret = fh_partition_get_dtprop(param.handle,
						    virt_to_phys(path),
						    virt_to_phys(propname),
						    virt_to_phys(propval),
						    &param.proplen);

		if (param.ret == 0) {
			if (copy_to_user(upropval, propval, param.proplen) ||
			    put_user(param.proplen, &p->proplen)) {
				ret = -EFAULT;
				goto out;
			}
		}
	}

	if (put_user(param.ret, &p->ret))
		ret = -EFAULT;

out:
	kfree(path);
	kfree(propval);
	kfree(propname);

	return ret;
}

static long fsl_hv_ioctl(struct file *file, unsigned int cmd,
			 unsigned long argaddr)
{
	void __user *arg = (void __user *)argaddr;
	long ret;

	switch (cmd) {
	case FSL_HV_IOCTL_PARTITION_RESTART:
		ret = ioctl_restart(arg);
		break;
	case FSL_HV_IOCTL_PARTITION_GET_STATUS:
		ret = ioctl_status(arg);
		break;
	case FSL_HV_IOCTL_PARTITION_START:
		ret = ioctl_start(arg);
		break;
	case FSL_HV_IOCTL_PARTITION_STOP:
		ret = ioctl_stop(arg);
		break;
	case FSL_HV_IOCTL_MEMCPY:
		ret = ioctl_memcpy(arg);
		break;
	case FSL_HV_IOCTL_DOORBELL:
		ret = ioctl_doorbell(arg);
		break;
	case FSL_HV_IOCTL_GETPROP:
		ret = ioctl_dtprop(arg, 0);
		break;
	case FSL_HV_IOCTL_SETPROP:
		ret = ioctl_dtprop(arg, 1);
		break;
	default:
		pr_debug("fsl-hv: bad ioctl dir=%u type=%u cmd=%u size=%u\n",
			 _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd),
			 _IOC_SIZE(cmd));
		return -ENOTTY;
	}

	return ret;
}

static struct list_head db_list;

static DEFINE_SPINLOCK(db_list_lock);

#define QSIZE	16

#define nextp(x) (((x) + 1) & (QSIZE - 1))

struct doorbell_queue {
	struct list_head list;
	spinlock_t lock;
	wait_queue_head_t wait;
	unsigned int head;
	unsigned int tail;
	uint32_t q[QSIZE];
};

struct list_head isr_list;

struct doorbell_isr {
	struct list_head list;
	unsigned int irq;
	uint32_t doorbell;	
	uint32_t partition;	
};

static void fsl_hv_queue_doorbell(uint32_t doorbell)
{
	struct doorbell_queue *dbq;
	unsigned long flags;

	
	spin_lock_irqsave(&db_list_lock, flags);

	list_for_each_entry(dbq, &db_list, list) {
		if (dbq->head != nextp(dbq->tail)) {
			dbq->q[dbq->tail] = doorbell;
			smp_wmb();
			dbq->tail = nextp(dbq->tail);
			wake_up_interruptible(&dbq->wait);
		}
	}

	spin_unlock_irqrestore(&db_list_lock, flags);
}

static irqreturn_t fsl_hv_isr(int irq, void *data)
{
	fsl_hv_queue_doorbell((uintptr_t) data);

	return IRQ_HANDLED;
}

static irqreturn_t fsl_hv_state_change_thread(int irq, void *data)
{
	struct doorbell_isr *dbisr = data;

	blocking_notifier_call_chain(&failover_subscribers, dbisr->partition,
				     NULL);

	return IRQ_HANDLED;
}

static irqreturn_t fsl_hv_state_change_isr(int irq, void *data)
{
	unsigned int status;
	struct doorbell_isr *dbisr = data;
	int ret;

	
	fsl_hv_queue_doorbell(dbisr->doorbell);

	
	ret = fh_partition_get_status(dbisr->partition, &status);
	if (!ret && (status == FH_PARTITION_STOPPED))
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static unsigned int fsl_hv_poll(struct file *filp, struct poll_table_struct *p)
{
	struct doorbell_queue *dbq = filp->private_data;
	unsigned long flags;
	unsigned int mask;

	spin_lock_irqsave(&dbq->lock, flags);

	poll_wait(filp, &dbq->wait, p);
	mask = (dbq->head == dbq->tail) ? 0 : (POLLIN | POLLRDNORM);

	spin_unlock_irqrestore(&dbq->lock, flags);

	return mask;
}

static ssize_t fsl_hv_read(struct file *filp, char __user *buf, size_t len,
			   loff_t *off)
{
	struct doorbell_queue *dbq = filp->private_data;
	uint32_t __user *p = (uint32_t __user *) buf; 
	unsigned long flags;
	ssize_t count = 0;

	
	while (len >= sizeof(uint32_t)) {
		uint32_t dbell;	

		spin_lock_irqsave(&dbq->lock, flags);

		if (dbq->head == dbq->tail) {
			spin_unlock_irqrestore(&dbq->lock, flags);
			if (count)
				break;
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			if (wait_event_interruptible(dbq->wait,
						     dbq->head != dbq->tail))
				return -ERESTARTSYS;
			continue;
		}

		smp_rmb();

		dbell = dbq->q[dbq->head];
		dbq->head = nextp(dbq->head);

		spin_unlock_irqrestore(&dbq->lock, flags);

		if (put_user(dbell, p))
			return -EFAULT;
		p++;
		count += sizeof(uint32_t);
		len -= sizeof(uint32_t);
	}

	return count;
}

static int fsl_hv_open(struct inode *inode, struct file *filp)
{
	struct doorbell_queue *dbq;
	unsigned long flags;
	int ret = 0;

	dbq = kzalloc(sizeof(struct doorbell_queue), GFP_KERNEL);
	if (!dbq) {
		pr_err("fsl-hv: out of memory\n");
		return -ENOMEM;
	}

	spin_lock_init(&dbq->lock);
	init_waitqueue_head(&dbq->wait);

	spin_lock_irqsave(&db_list_lock, flags);
	list_add(&dbq->list, &db_list);
	spin_unlock_irqrestore(&db_list_lock, flags);

	filp->private_data = dbq;

	return ret;
}

static int fsl_hv_close(struct inode *inode, struct file *filp)
{
	struct doorbell_queue *dbq = filp->private_data;
	unsigned long flags;

	int ret = 0;

	spin_lock_irqsave(&db_list_lock, flags);
	list_del(&dbq->list);
	spin_unlock_irqrestore(&db_list_lock, flags);

	kfree(dbq);

	return ret;
}

static const struct file_operations fsl_hv_fops = {
	.owner = THIS_MODULE,
	.open = fsl_hv_open,
	.release = fsl_hv_close,
	.poll = fsl_hv_poll,
	.read = fsl_hv_read,
	.unlocked_ioctl = fsl_hv_ioctl,
	.compat_ioctl = fsl_hv_ioctl,
};

static struct miscdevice fsl_hv_misc_dev = {
	MISC_DYNAMIC_MINOR,
	"fsl-hv",
	&fsl_hv_fops
};

static irqreturn_t fsl_hv_shutdown_isr(int irq, void *data)
{
	orderly_poweroff(false);

	return IRQ_HANDLED;
}

static int get_parent_handle(struct device_node *np)
{
	struct device_node *parent;
	const uint32_t *prop;
	uint32_t handle;
	int len;

	parent = of_get_parent(np);
	if (!parent)
		
		return -ENODEV;

	prop = of_get_property(parent, "hv-handle", &len);
	if (!prop)
		prop = of_get_property(parent, "reg", &len);

	if (!prop || (len != sizeof(uint32_t))) {
		
		of_node_put(parent);
		return -ENODEV;
	}

	handle = be32_to_cpup(prop);
	of_node_put(parent);

	return handle;
}

int fsl_hv_failover_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&failover_subscribers, nb);
}
EXPORT_SYMBOL(fsl_hv_failover_register);

int fsl_hv_failover_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&failover_subscribers, nb);
}
EXPORT_SYMBOL(fsl_hv_failover_unregister);

static int has_fsl_hypervisor(void)
{
	struct device_node *node;
	int ret;

	if (!(mfmsr() & MSR_GS))
		return 0;

	node = of_find_node_by_path("/hypervisor");
	if (!node)
		return 0;

	ret = of_find_property(node, "fsl,hv-version", NULL) != NULL;

	of_node_put(node);

	return ret;
}

static int __init fsl_hypervisor_init(void)
{
	struct device_node *np;
	struct doorbell_isr *dbisr, *n;
	int ret;

	pr_info("Freescale hypervisor management driver\n");

	if (!has_fsl_hypervisor()) {
		pr_info("fsl-hv: no hypervisor found\n");
		return -ENODEV;
	}

	ret = misc_register(&fsl_hv_misc_dev);
	if (ret) {
		pr_err("fsl-hv: cannot register device\n");
		return ret;
	}

	INIT_LIST_HEAD(&db_list);
	INIT_LIST_HEAD(&isr_list);

	for_each_compatible_node(np, NULL, "epapr,hv-receive-doorbell") {
		unsigned int irq;
		const uint32_t *handle;

		handle = of_get_property(np, "interrupts", NULL);
		irq = irq_of_parse_and_map(np, 0);
		if (!handle || (irq == NO_IRQ)) {
			pr_err("fsl-hv: no 'interrupts' property in %s node\n",
				np->full_name);
			continue;
		}

		dbisr = kzalloc(sizeof(*dbisr), GFP_KERNEL);
		if (!dbisr)
			goto out_of_memory;

		dbisr->irq = irq;
		dbisr->doorbell = be32_to_cpup(handle);

		if (of_device_is_compatible(np, "fsl,hv-shutdown-doorbell")) {
			
			ret = request_irq(irq, fsl_hv_shutdown_isr, 0,
					  np->name, NULL);
		} else if (of_device_is_compatible(np,
			"fsl,hv-state-change-doorbell")) {
			dbisr->partition = ret = get_parent_handle(np);
			if (ret < 0) {
				pr_err("fsl-hv: node %s has missing or "
				       "malformed parent\n", np->full_name);
				kfree(dbisr);
				continue;
			}
			ret = request_threaded_irq(irq, fsl_hv_state_change_isr,
						   fsl_hv_state_change_thread,
						   0, np->name, dbisr);
		} else
			ret = request_irq(irq, fsl_hv_isr, 0, np->name, dbisr);

		if (ret < 0) {
			pr_err("fsl-hv: could not request irq %u for node %s\n",
			       irq, np->full_name);
			kfree(dbisr);
			continue;
		}

		list_add(&dbisr->list, &isr_list);

		pr_info("fsl-hv: registered handler for doorbell %u\n",
			dbisr->doorbell);
	}

	return 0;

out_of_memory:
	list_for_each_entry_safe(dbisr, n, &isr_list, list) {
		free_irq(dbisr->irq, dbisr);
		list_del(&dbisr->list);
		kfree(dbisr);
	}

	misc_deregister(&fsl_hv_misc_dev);

	return -ENOMEM;
}

static void __exit fsl_hypervisor_exit(void)
{
	struct doorbell_isr *dbisr, *n;

	list_for_each_entry_safe(dbisr, n, &isr_list, list) {
		free_irq(dbisr->irq, dbisr);
		list_del(&dbisr->list);
		kfree(dbisr);
	}

	misc_deregister(&fsl_hv_misc_dev);
}

module_init(fsl_hypervisor_init);
module_exit(fsl_hypervisor_exit);

MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_DESCRIPTION("Freescale hypervisor management driver");
MODULE_LICENSE("GPL v2");
