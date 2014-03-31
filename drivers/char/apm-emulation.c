#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/apm_bios.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/apm-emulation.h>
#include <linux/freezer.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>


#define APM_MINOR_DEV	134


#define APM_MAX_EVENTS		16

struct apm_queue {
	unsigned int		event_head;
	unsigned int		event_tail;
	apm_event_t		events[APM_MAX_EVENTS];
};

enum apm_suspend_state {
	SUSPEND_NONE,
	SUSPEND_PENDING,
	SUSPEND_READ,
	SUSPEND_ACKED,
	SUSPEND_ACKTO,
	SUSPEND_WAIT,
	SUSPEND_DONE,
};

struct apm_user {
	struct list_head	list;

	unsigned int		suser: 1;
	unsigned int		writer: 1;
	unsigned int		reader: 1;

	int			suspend_result;
	enum apm_suspend_state	suspend_state;

	struct apm_queue	queue;
};

static atomic_t suspend_acks_pending = ATOMIC_INIT(0);
static atomic_t userspace_notification_inhibit = ATOMIC_INIT(0);
static int apm_disabled;
static struct task_struct *kapmd_tsk;

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);

static DECLARE_RWSEM(user_list_lock);
static LIST_HEAD(apm_user_list);

static DECLARE_WAIT_QUEUE_HEAD(kapmd_wait);
static DEFINE_SPINLOCK(kapmd_queue_lock);
static struct apm_queue kapmd_queue;

static DEFINE_MUTEX(state_lock);

static const char driver_version[] = "1.13";	



static void __apm_get_power_status(struct apm_power_info *info)
{
}

void (*apm_get_power_status)(struct apm_power_info *) = __apm_get_power_status;
EXPORT_SYMBOL(apm_get_power_status);


static inline int queue_empty(struct apm_queue *q)
{
	return q->event_head == q->event_tail;
}

static inline apm_event_t queue_get_event(struct apm_queue *q)
{
	q->event_tail = (q->event_tail + 1) % APM_MAX_EVENTS;
	return q->events[q->event_tail];
}

static void queue_add_event(struct apm_queue *q, apm_event_t event)
{
	q->event_head = (q->event_head + 1) % APM_MAX_EVENTS;
	if (q->event_head == q->event_tail) {
		static int notified;

		if (notified++ == 0)
		    printk(KERN_ERR "apm: an event queue overflowed\n");
		q->event_tail = (q->event_tail + 1) % APM_MAX_EVENTS;
	}
	q->events[q->event_head] = event;
}

static void queue_event(apm_event_t event)
{
	struct apm_user *as;

	down_read(&user_list_lock);
	list_for_each_entry(as, &apm_user_list, list) {
		if (as->reader)
			queue_add_event(&as->queue, event);
	}
	up_read(&user_list_lock);
	wake_up_interruptible(&apm_waitqueue);
}

static ssize_t apm_read(struct file *fp, char __user *buf, size_t count, loff_t *ppos)
{
	struct apm_user *as = fp->private_data;
	apm_event_t event;
	int i = count, ret = 0;

	if (count < sizeof(apm_event_t))
		return -EINVAL;

	if (queue_empty(&as->queue) && fp->f_flags & O_NONBLOCK)
		return -EAGAIN;

	wait_event_interruptible(apm_waitqueue, !queue_empty(&as->queue));

	while ((i >= sizeof(event)) && !queue_empty(&as->queue)) {
		event = queue_get_event(&as->queue);

		ret = -EFAULT;
		if (copy_to_user(buf, &event, sizeof(event)))
			break;

		mutex_lock(&state_lock);
		if (as->suspend_state == SUSPEND_PENDING &&
		    (event == APM_SYS_SUSPEND || event == APM_USER_SUSPEND))
			as->suspend_state = SUSPEND_READ;
		mutex_unlock(&state_lock);

		buf += sizeof(event);
		i -= sizeof(event);
	}

	if (i < count)
		ret = count - i;

	return ret;
}

static unsigned int apm_poll(struct file *fp, poll_table * wait)
{
	struct apm_user *as = fp->private_data;

	poll_wait(fp, &apm_waitqueue, wait);
	return queue_empty(&as->queue) ? 0 : POLLIN | POLLRDNORM;
}

static long
apm_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	struct apm_user *as = filp->private_data;
	int err = -EINVAL;

	if (!as->suser || !as->writer)
		return -EPERM;

	switch (cmd) {
	case APM_IOC_SUSPEND:
		mutex_lock(&state_lock);

		as->suspend_result = -EINTR;

		switch (as->suspend_state) {
		case SUSPEND_READ:
			as->suspend_state = SUSPEND_ACKED;
			atomic_dec(&suspend_acks_pending);
			mutex_unlock(&state_lock);

			wake_up(&apm_suspend_waitqueue);

			while (wait_event_freezable(apm_suspend_waitqueue,
					as->suspend_state != SUSPEND_ACKED))
				msleep(10);
			break;
		case SUSPEND_ACKTO:
			as->suspend_result = -ETIMEDOUT;
			mutex_unlock(&state_lock);
			break;
		default:
			as->suspend_state = SUSPEND_WAIT;
			mutex_unlock(&state_lock);

			as->suspend_result = pm_suspend(PM_SUSPEND_MEM);
		}

		mutex_lock(&state_lock);
		err = as->suspend_result;
		as->suspend_state = SUSPEND_NONE;
		mutex_unlock(&state_lock);
		break;
	}

	return err;
}

static int apm_release(struct inode * inode, struct file * filp)
{
	struct apm_user *as = filp->private_data;

	filp->private_data = NULL;

	down_write(&user_list_lock);
	list_del(&as->list);
	up_write(&user_list_lock);

	mutex_lock(&state_lock);
	if (as->suspend_state == SUSPEND_PENDING ||
	    as->suspend_state == SUSPEND_READ)
		atomic_dec(&suspend_acks_pending);
	mutex_unlock(&state_lock);

	wake_up(&apm_suspend_waitqueue);

	kfree(as);
	return 0;
}

static int apm_open(struct inode * inode, struct file * filp)
{
	struct apm_user *as;

	as = kzalloc(sizeof(*as), GFP_KERNEL);
	if (as) {
		as->suser = capable(CAP_SYS_ADMIN);
		as->writer = (filp->f_mode & FMODE_WRITE) == FMODE_WRITE;
		as->reader = (filp->f_mode & FMODE_READ) == FMODE_READ;

		down_write(&user_list_lock);
		list_add(&as->list, &apm_user_list);
		up_write(&user_list_lock);

		filp->private_data = as;
	}

	return as ? 0 : -ENOMEM;
}

static const struct file_operations apm_bios_fops = {
	.owner		= THIS_MODULE,
	.read		= apm_read,
	.poll		= apm_poll,
	.unlocked_ioctl	= apm_ioctl,
	.open		= apm_open,
	.release	= apm_release,
	.llseek		= noop_llseek,
};

static struct miscdevice apm_device = {
	.minor		= APM_MINOR_DEV,
	.name		= "apm_bios",
	.fops		= &apm_bios_fops
};


#ifdef CONFIG_PROC_FS
static int proc_apm_show(struct seq_file *m, void *v)
{
	struct apm_power_info info;
	char *units;

	info.ac_line_status = 0xff;
	info.battery_status = 0xff;
	info.battery_flag   = 0xff;
	info.battery_life   = -1;
	info.time	    = -1;
	info.units	    = -1;

	if (apm_get_power_status)
		apm_get_power_status(&info);

	switch (info.units) {
	default:	units = "?";	break;
	case 0: 	units = "min";	break;
	case 1: 	units = "sec";	break;
	}

	seq_printf(m, "%s 1.2 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		     driver_version, APM_32_BIT_SUPPORT,
		     info.ac_line_status, info.battery_status,
		     info.battery_flag, info.battery_life,
		     info.time, units);

	return 0;
}

static int proc_apm_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_apm_show, NULL);
}

static const struct file_operations apm_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= proc_apm_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int kapmd(void *arg)
{
	do {
		apm_event_t event;

		wait_event_interruptible(kapmd_wait,
				!queue_empty(&kapmd_queue) || kthread_should_stop());

		if (kthread_should_stop())
			break;

		spin_lock_irq(&kapmd_queue_lock);
		event = 0;
		if (!queue_empty(&kapmd_queue))
			event = queue_get_event(&kapmd_queue);
		spin_unlock_irq(&kapmd_queue_lock);

		switch (event) {
		case 0:
			break;

		case APM_LOW_BATTERY:
		case APM_POWER_STATUS_CHANGE:
			queue_event(event);
			break;

		case APM_USER_SUSPEND:
		case APM_SYS_SUSPEND:
			pm_suspend(PM_SUSPEND_MEM);
			break;

		case APM_CRITICAL_SUSPEND:
			atomic_inc(&userspace_notification_inhibit);
			pm_suspend(PM_SUSPEND_MEM);
			atomic_dec(&userspace_notification_inhibit);
			break;
		}
	} while (1);

	return 0;
}

static int apm_suspend_notifier(struct notifier_block *nb,
				unsigned long event,
				void *dummy)
{
	struct apm_user *as;
	int err;

	
	if (atomic_read(&userspace_notification_inhibit))
		return NOTIFY_DONE;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&state_lock);
		down_read(&user_list_lock);

		list_for_each_entry(as, &apm_user_list, list) {
			if (as->suspend_state != SUSPEND_WAIT && as->reader &&
			    as->writer && as->suser) {
				as->suspend_state = SUSPEND_PENDING;
				atomic_inc(&suspend_acks_pending);
				queue_add_event(&as->queue, APM_USER_SUSPEND);
			}
		}

		up_read(&user_list_lock);
		mutex_unlock(&state_lock);
		wake_up_interruptible(&apm_waitqueue);

		err = wait_event_interruptible_timeout(
			apm_suspend_waitqueue,
			atomic_read(&suspend_acks_pending) == 0,
			5*HZ);

		
		if (err == 0) {
			mutex_lock(&state_lock);
			down_read(&user_list_lock);
			list_for_each_entry(as, &apm_user_list, list) {
				if (as->suspend_state == SUSPEND_PENDING ||
				    as->suspend_state == SUSPEND_READ) {
					as->suspend_state = SUSPEND_ACKTO;
					atomic_dec(&suspend_acks_pending);
				}
			}
			up_read(&user_list_lock);
			mutex_unlock(&state_lock);
		}

		
		if (err >= 0)
			return NOTIFY_OK;

		
		return notifier_from_errno(err);

	case PM_POST_SUSPEND:
		queue_event(APM_NORMAL_RESUME);

		mutex_lock(&state_lock);
		down_read(&user_list_lock);
		list_for_each_entry(as, &apm_user_list, list) {
			if (as->suspend_state == SUSPEND_ACKED) {
				as->suspend_result = 0;
				as->suspend_state = SUSPEND_DONE;
			}
		}
		up_read(&user_list_lock);
		mutex_unlock(&state_lock);

		wake_up(&apm_suspend_waitqueue);
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block apm_notif_block = {
	.notifier_call = apm_suspend_notifier,
};

static int __init apm_init(void)
{
	int ret;

	if (apm_disabled) {
		printk(KERN_NOTICE "apm: disabled on user request.\n");
		return -ENODEV;
	}

	kapmd_tsk = kthread_create(kapmd, NULL, "kapmd");
	if (IS_ERR(kapmd_tsk)) {
		ret = PTR_ERR(kapmd_tsk);
		kapmd_tsk = NULL;
		goto out;
	}
	wake_up_process(kapmd_tsk);

#ifdef CONFIG_PROC_FS
	proc_create("apm", 0, NULL, &apm_proc_fops);
#endif

	ret = misc_register(&apm_device);
	if (ret)
		goto out_stop;

	ret = register_pm_notifier(&apm_notif_block);
	if (ret)
		goto out_unregister;

	return 0;

 out_unregister:
	misc_deregister(&apm_device);
 out_stop:
	remove_proc_entry("apm", NULL);
	kthread_stop(kapmd_tsk);
 out:
	return ret;
}

static void __exit apm_exit(void)
{
	unregister_pm_notifier(&apm_notif_block);
	misc_deregister(&apm_device);
	remove_proc_entry("apm", NULL);

	kthread_stop(kapmd_tsk);
}

module_init(apm_init);
module_exit(apm_exit);

MODULE_AUTHOR("Stephen Rothwell");
MODULE_DESCRIPTION("Advanced Power Management");
MODULE_LICENSE("GPL");

#ifndef MODULE
static int __init apm_setup(char *str)
{
	while ((str != NULL) && (*str != '\0')) {
		if (strncmp(str, "off", 3) == 0)
			apm_disabled = 1;
		if (strncmp(str, "on", 2) == 0)
			apm_disabled = 0;
		str = strchr(str, ',');
		if (str != NULL)
			str += strspn(str, ", \t");
	}
	return 1;
}

__setup("apm=", apm_setup);
#endif

void apm_queue_event(apm_event_t event)
{
	unsigned long flags;

	spin_lock_irqsave(&kapmd_queue_lock, flags);
	queue_add_event(&kapmd_queue, event);
	spin_unlock_irqrestore(&kapmd_queue_lock, flags);

	wake_up_interruptible(&kapmd_wait);
}
EXPORT_SYMBOL(apm_queue_event);
