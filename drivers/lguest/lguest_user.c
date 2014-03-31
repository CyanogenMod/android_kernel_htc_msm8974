#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/export.h>
#include "lg.h"

bool send_notify_to_eventfd(struct lg_cpu *cpu)
{
	unsigned int i;
	struct lg_eventfd_map *map;

	rcu_read_lock();
	map = rcu_dereference(cpu->lg->eventfds);
	for (i = 0; i < map->num; i++) {
		if (map->map[i].addr == cpu->pending_notify) {
			eventfd_signal(map->map[i].event, 1);
			cpu->pending_notify = 0;
			break;
		}
	}
	
	rcu_read_unlock();

	
	return cpu->pending_notify == 0;
}

static int add_eventfd(struct lguest *lg, unsigned long addr, int fd)
{
	struct lg_eventfd_map *new, *old = lg->eventfds;

	if (!addr)
		return -EINVAL;

	new = kmalloc(sizeof(*new) + sizeof(new->map[0]) * (old->num + 1),
		      GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	
	memcpy(new->map, old->map, sizeof(old->map[0]) * old->num);
	new->num = old->num;

	
	new->map[new->num].addr = addr;
	new->map[new->num].event = eventfd_ctx_fdget(fd);
	if (IS_ERR(new->map[new->num].event)) {
		int err =  PTR_ERR(new->map[new->num].event);
		kfree(new);
		return err;
	}
	new->num++;

	/*
	 * Now put new one in place: rcu_assign_pointer() is a fancy way of
	 * doing "lg->eventfds = new", but it uses memory barriers to make
	 * absolutely sure that the contents of "new" written above is nailed
	 * down before we actually do the assignment.
	 *
	 * We have to think about these kinds of things when we're operating on
	 * live data without locks.
	 */
	rcu_assign_pointer(lg->eventfds, new);

	synchronize_rcu();
	kfree(old);

	return 0;
}

static int attach_eventfd(struct lguest *lg, const unsigned long __user *input)
{
	unsigned long addr, fd;
	int err;

	if (get_user(addr, input) != 0)
		return -EFAULT;
	input++;
	if (get_user(fd, input) != 0)
		return -EFAULT;

	mutex_lock(&lguest_lock);
	err = add_eventfd(lg, addr, fd);
	mutex_unlock(&lguest_lock);

	return err;
}

static int user_send_irq(struct lg_cpu *cpu, const unsigned long __user *input)
{
	unsigned long irq;

	if (get_user(irq, input) != 0)
		return -EFAULT;
	if (irq >= LGUEST_IRQS)
		return -EINVAL;

	set_interrupt(cpu, irq);
	return 0;
}

static ssize_t read(struct file *file, char __user *user, size_t size,loff_t*o)
{
	struct lguest *lg = file->private_data;
	struct lg_cpu *cpu;
	unsigned int cpu_id = *o;

	
	if (!lg)
		return -EINVAL;

	
	if (cpu_id >= lg->nr_cpus)
		return -EINVAL;

	cpu = &lg->cpus[cpu_id];

	
	if (current != cpu->tsk)
		return -EPERM;

	
	if (lg->dead) {
		size_t len;

		
		if (IS_ERR(lg->dead))
			return PTR_ERR(lg->dead);

		
		len = min(size, strlen(lg->dead)+1);
		if (copy_to_user(user, lg->dead, len) != 0)
			return -EFAULT;
		return len;
	}

	if (cpu->pending_notify)
		cpu->pending_notify = 0;

	
	return run_guest(cpu, (unsigned long __user *)user);
}

static int lg_cpu_start(struct lg_cpu *cpu, unsigned id, unsigned long start_ip)
{
	
	if (id >= ARRAY_SIZE(cpu->lg->cpus))
		return -EINVAL;

	
	cpu->id = id;
	cpu->lg = container_of((cpu - id), struct lguest, cpus[0]);
	cpu->lg->nr_cpus++;

	
	init_clockdev(cpu);

	cpu->regs_page = get_zeroed_page(GFP_KERNEL);
	if (!cpu->regs_page)
		return -ENOMEM;

	
	cpu->regs = (void *)cpu->regs_page + PAGE_SIZE - sizeof(*cpu->regs);

	lguest_arch_setup_regs(cpu, start_ip);

	cpu->tsk = current;

	cpu->mm = get_task_mm(cpu->tsk);

	cpu->last_pages = NULL;

	
	return 0;
}

static int initialize(struct file *file, const unsigned long __user *input)
{
	
	struct lguest *lg;
	int err;
	unsigned long args[3];

	mutex_lock(&lguest_lock);
	
	if (file->private_data) {
		err = -EBUSY;
		goto unlock;
	}

	if (copy_from_user(args, input, sizeof(args)) != 0) {
		err = -EFAULT;
		goto unlock;
	}

	lg = kzalloc(sizeof(*lg), GFP_KERNEL);
	if (!lg) {
		err = -ENOMEM;
		goto unlock;
	}

	lg->eventfds = kmalloc(sizeof(*lg->eventfds), GFP_KERNEL);
	if (!lg->eventfds) {
		err = -ENOMEM;
		goto free_lg;
	}
	lg->eventfds->num = 0;

	
	lg->mem_base = (void __user *)args[0];
	lg->pfn_limit = args[1];

	
	err = lg_cpu_start(&lg->cpus[0], 0, args[2]);
	if (err)
		goto free_eventfds;

	err = init_guest_pagetable(lg);
	if (err)
		goto free_regs;

	
	file->private_data = lg;

	mutex_unlock(&lguest_lock);

	
	return sizeof(args);

free_regs:
	
	free_page(lg->cpus[0].regs_page);
free_eventfds:
	kfree(lg->eventfds);
free_lg:
	kfree(lg);
unlock:
	mutex_unlock(&lguest_lock);
	return err;
}

static ssize_t write(struct file *file, const char __user *in,
		     size_t size, loff_t *off)
{
	struct lguest *lg = file->private_data;
	const unsigned long __user *input = (const unsigned long __user *)in;
	unsigned long req;
	struct lg_cpu *uninitialized_var(cpu);
	unsigned int cpu_id = *off;

	
	if (get_user(req, input) != 0)
		return -EFAULT;
	input++;

	
	if (req != LHREQ_INITIALIZE) {
		if (!lg || (cpu_id >= lg->nr_cpus))
			return -EINVAL;
		cpu = &lg->cpus[cpu_id];

		
		if (lg->dead)
			return -ENOENT;
	}

	switch (req) {
	case LHREQ_INITIALIZE:
		return initialize(file, input);
	case LHREQ_IRQ:
		return user_send_irq(cpu, input);
	case LHREQ_EVENTFD:
		return attach_eventfd(lg, input);
	default:
		return -EINVAL;
	}
}

static int close(struct inode *inode, struct file *file)
{
	struct lguest *lg = file->private_data;
	unsigned int i;

	
	if (!lg)
		return 0;

	mutex_lock(&lguest_lock);

	
	free_guest_pagetable(lg);

	for (i = 0; i < lg->nr_cpus; i++) {
		
		hrtimer_cancel(&lg->cpus[i].hrt);
		
		free_page(lg->cpus[i].regs_page);
		mmput(lg->cpus[i].mm);
	}

	
	for (i = 0; i < lg->eventfds->num; i++)
		eventfd_ctx_put(lg->eventfds->map[i].event);
	kfree(lg->eventfds);

	if (!IS_ERR(lg->dead))
		kfree(lg->dead);
	
	kfree(lg);
	
	mutex_unlock(&lguest_lock);

	return 0;
}

static const struct file_operations lguest_fops = {
	.owner	 = THIS_MODULE,
	.release = close,
	.write	 = write,
	.read	 = read,
	.llseek  = default_llseek,
};

static struct miscdevice lguest_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "lguest",
	.fops	= &lguest_fops,
};

int __init lguest_device_init(void)
{
	return misc_register(&lguest_dev);
}

void __exit lguest_device_remove(void)
{
	misc_deregister(&lguest_dev);
}
