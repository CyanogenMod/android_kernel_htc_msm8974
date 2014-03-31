/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 * Derived (i.e. mostly copied) from arch/i386/kernel/irq.c:
 *	Copyright (C) 1992, 1998 Linus Torvalds, Ingo Molnar
 */

#include "linux/cpumask.h"
#include "linux/hardirq.h"
#include "linux/interrupt.h"
#include "linux/kernel_stat.h"
#include "linux/module.h"
#include "linux/sched.h"
#include "linux/seq_file.h"
#include "linux/slab.h"
#include "as-layout.h"
#include "kern_util.h"
#include "os.h"

static struct irq_fd *active_fds = NULL;
static struct irq_fd **last_irq_ptr = &active_fds;

extern void free_irqs(void);

void sigio_handler(int sig, struct uml_pt_regs *regs)
{
	struct irq_fd *irq_fd;
	int n;

	if (smp_sigio_handler())
		return;

	while (1) {
		n = os_waiting_for_events(active_fds);
		if (n <= 0) {
			if (n == -EINTR)
				continue;
			else break;
		}

		for (irq_fd = active_fds; irq_fd != NULL;
		     irq_fd = irq_fd->next) {
			if (irq_fd->current_events != 0) {
				irq_fd->current_events = 0;
				do_IRQ(irq_fd->irq, regs);
			}
		}
	}

	free_irqs();
}

static DEFINE_SPINLOCK(irq_lock);

static int activate_fd(int irq, int fd, int type, void *dev_id)
{
	struct pollfd *tmp_pfd;
	struct irq_fd *new_fd, *irq_fd;
	unsigned long flags;
	int events, err, n;

	err = os_set_fd_async(fd);
	if (err < 0)
		goto out;

	err = -ENOMEM;
	new_fd = kmalloc(sizeof(struct irq_fd), GFP_KERNEL);
	if (new_fd == NULL)
		goto out;

	if (type == IRQ_READ)
		events = UM_POLLIN | UM_POLLPRI;
	else events = UM_POLLOUT;
	*new_fd = ((struct irq_fd) { .next  		= NULL,
				     .id 		= dev_id,
				     .fd 		= fd,
				     .type 		= type,
				     .irq 		= irq,
				     .events 		= events,
				     .current_events 	= 0 } );

	err = -EBUSY;
	spin_lock_irqsave(&irq_lock, flags);
	for (irq_fd = active_fds; irq_fd != NULL; irq_fd = irq_fd->next) {
		if ((irq_fd->fd == fd) && (irq_fd->type == type)) {
			printk(KERN_ERR "Registering fd %d twice\n", fd);
			printk(KERN_ERR "Irqs : %d, %d\n", irq_fd->irq, irq);
			printk(KERN_ERR "Ids : 0x%p, 0x%p\n", irq_fd->id,
			       dev_id);
			goto out_unlock;
		}
	}

	if (type == IRQ_WRITE)
		fd = -1;

	tmp_pfd = NULL;
	n = 0;

	while (1) {
		n = os_create_pollfd(fd, events, tmp_pfd, n);
		if (n == 0)
			break;

		spin_unlock_irqrestore(&irq_lock, flags);
		kfree(tmp_pfd);

		tmp_pfd = kmalloc(n, GFP_KERNEL);
		if (tmp_pfd == NULL)
			goto out_kfree;

		spin_lock_irqsave(&irq_lock, flags);
	}

	*last_irq_ptr = new_fd;
	last_irq_ptr = &new_fd->next;

	spin_unlock_irqrestore(&irq_lock, flags);

	maybe_sigio_broken(fd, (type == IRQ_READ));

	return 0;

 out_unlock:
	spin_unlock_irqrestore(&irq_lock, flags);
 out_kfree:
	kfree(new_fd);
 out:
	return err;
}

static void free_irq_by_cb(int (*test)(struct irq_fd *, void *), void *arg)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);
	os_free_irq_by_cb(test, arg, active_fds, &last_irq_ptr);
	spin_unlock_irqrestore(&irq_lock, flags);
}

struct irq_and_dev {
	int irq;
	void *dev;
};

static int same_irq_and_dev(struct irq_fd *irq, void *d)
{
	struct irq_and_dev *data = d;

	return ((irq->irq == data->irq) && (irq->id == data->dev));
}

static void free_irq_by_irq_and_dev(unsigned int irq, void *dev)
{
	struct irq_and_dev data = ((struct irq_and_dev) { .irq  = irq,
							  .dev  = dev });

	free_irq_by_cb(same_irq_and_dev, &data);
}

static int same_fd(struct irq_fd *irq, void *fd)
{
	return (irq->fd == *((int *)fd));
}

void free_irq_by_fd(int fd)
{
	free_irq_by_cb(same_fd, &fd);
}

static struct irq_fd *find_irq_by_fd(int fd, int irqnum, int *index_out)
{
	struct irq_fd *irq;
	int i = 0;
	int fdi;

	for (irq = active_fds; irq != NULL; irq = irq->next) {
		if ((irq->fd == fd) && (irq->irq == irqnum))
			break;
		i++;
	}
	if (irq == NULL) {
		printk(KERN_ERR "find_irq_by_fd doesn't have descriptor %d\n",
		       fd);
		goto out;
	}
	fdi = os_get_pollfd(i);
	if ((fdi != -1) && (fdi != fd)) {
		printk(KERN_ERR "find_irq_by_fd - mismatch between active_fds "
		       "and pollfds, fd %d vs %d, need %d\n", irq->fd,
		       fdi, fd);
		irq = NULL;
		goto out;
	}
	*index_out = i;
 out:
	return irq;
}

void reactivate_fd(int fd, int irqnum)
{
	struct irq_fd *irq;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&irq_lock, flags);
	irq = find_irq_by_fd(fd, irqnum, &i);
	if (irq == NULL) {
		spin_unlock_irqrestore(&irq_lock, flags);
		return;
	}
	os_set_pollfd(i, irq->fd);
	spin_unlock_irqrestore(&irq_lock, flags);

	add_sigio_fd(fd);
}

void deactivate_fd(int fd, int irqnum)
{
	struct irq_fd *irq;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&irq_lock, flags);
	irq = find_irq_by_fd(fd, irqnum, &i);
	if (irq == NULL) {
		spin_unlock_irqrestore(&irq_lock, flags);
		return;
	}

	os_set_pollfd(i, -1);
	spin_unlock_irqrestore(&irq_lock, flags);

	ignore_sigio_fd(fd);
}
EXPORT_SYMBOL(deactivate_fd);

int deactivate_all_fds(void)
{
	struct irq_fd *irq;
	int err;

	for (irq = active_fds; irq != NULL; irq = irq->next) {
		err = os_clear_fd_async(irq->fd);
		if (err)
			return err;
	}
	
	os_set_ioignore();

	return 0;
}

unsigned int do_IRQ(int irq, struct uml_pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs((struct pt_regs *)regs);
	irq_enter();
	generic_handle_irq(irq);
	irq_exit();
	set_irq_regs(old_regs);
	return 1;
}

int um_request_irq(unsigned int irq, int fd, int type,
		   irq_handler_t handler,
		   unsigned long irqflags, const char * devname,
		   void *dev_id)
{
	int err;

	if (fd != -1) {
		err = activate_fd(irq, fd, type, dev_id);
		if (err)
			return err;
	}

	return request_irq(irq, handler, irqflags, devname, dev_id);
}

EXPORT_SYMBOL(um_request_irq);
EXPORT_SYMBOL(reactivate_fd);

static void dummy(struct irq_data *d)
{
}

static struct irq_chip normal_irq_type = {
	.name = "SIGIO",
	.release = free_irq_by_irq_and_dev,
	.irq_disable = dummy,
	.irq_enable = dummy,
	.irq_ack = dummy,
};

static struct irq_chip SIGVTALRM_irq_type = {
	.name = "SIGVTALRM",
	.release = free_irq_by_irq_and_dev,
	.irq_disable = dummy,
	.irq_enable = dummy,
	.irq_ack = dummy,
};

void __init init_IRQ(void)
{
	int i;

	irq_set_chip_and_handler(TIMER_IRQ, &SIGVTALRM_irq_type, handle_edge_irq);

	for (i = 1; i < NR_IRQS; i++)
		irq_set_chip_and_handler(i, &normal_irq_type, handle_edge_irq);
}


static unsigned long pending_mask;

unsigned long to_irq_stack(unsigned long *mask_out)
{
	struct thread_info *ti;
	unsigned long mask, old;
	int nested;

	mask = xchg(&pending_mask, *mask_out);
	if (mask != 0) {
		old = *mask_out;
		do {
			old |= mask;
			mask = xchg(&pending_mask, old);
		} while (mask != old);
		return 1;
	}

	ti = current_thread_info();
	nested = (ti->real_thread != NULL);
	if (!nested) {
		struct task_struct *task;
		struct thread_info *tti;

		task = cpu_tasks[ti->cpu].task;
		tti = task_thread_info(task);

		*ti = *tti;
		ti->real_thread = tti;
		task->stack = ti;
	}

	mask = xchg(&pending_mask, 0);
	*mask_out |= mask | nested;
	return 0;
}

unsigned long from_irq_stack(int nested)
{
	struct thread_info *ti, *to;
	unsigned long mask;

	ti = current_thread_info();

	pending_mask = 1;

	to = ti->real_thread;
	current->stack = to;
	ti->real_thread = NULL;
	*to = *ti;

	mask = xchg(&pending_mask, 0);
	return mask & ~1;
}

