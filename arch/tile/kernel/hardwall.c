/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
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
 */

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/rwsem.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/smp.h>
#include <linux/cdev.h>
#include <linux/compat.h>
#include <asm/hardwall.h>
#include <asm/traps.h>
#include <asm/siginfo.h>
#include <asm/irq_regs.h>

#include <arch/interrupts.h>
#include <arch/spr_def.h>


struct hardwall_info {
	struct list_head list;             
	struct list_head task_head;        
	struct cpumask cpumask;            
	int ulhc_x;                        
	int ulhc_y;                        
	int width;                         
	int height;                        
	int id;                            
	int teardown_in_progress;          
};

static LIST_HEAD(rectangles);

static struct proc_dir_entry *hardwall_proc_dir;

static void hardwall_add_proc(struct hardwall_info *rect);
static void hardwall_remove_proc(struct hardwall_info *rect);

static DEFINE_SPINLOCK(hardwall_lock);

static int udn_disabled;
static int __init noudn(char *str)
{
	pr_info("User-space UDN access is disabled\n");
	udn_disabled = 1;
	return 0;
}
early_param("noudn", noudn);



#define cpu_online_set(cpu, dst) do { \
	if (cpu_online(cpu))          \
		cpumask_set_cpu(cpu, dst);    \
} while (0)


static int contains(struct hardwall_info *r, int x, int y)
{
	return (x >= r->ulhc_x && x < r->ulhc_x + r->width) &&
		(y >= r->ulhc_y && y < r->ulhc_y + r->height);
}

static int setup_rectangle(struct hardwall_info *r, struct cpumask *mask)
{
	int x, y, cpu, ulhc, lrhc;

	
	ulhc = find_first_bit(cpumask_bits(mask), nr_cpumask_bits);
	lrhc = find_last_bit(cpumask_bits(mask), nr_cpumask_bits);

	
	r->ulhc_x = cpu_x(ulhc);
	r->ulhc_y = cpu_y(ulhc);
	r->width = cpu_x(lrhc) - r->ulhc_x + 1;
	r->height = cpu_y(lrhc) - r->ulhc_y + 1;
	cpumask_copy(&r->cpumask, mask);
	r->id = ulhc;   

	
	if (r->width <= 0 || r->height <= 0)
		return -EINVAL;

	
	for (y = 0, cpu = 0; y < smp_height; ++y)
		for (x = 0; x < smp_width; ++x, ++cpu)
			if (cpumask_test_cpu(cpu, mask) != contains(r, x, y))
				return -EINVAL;

	return 0;
}

static int overlaps(struct hardwall_info *a, struct hardwall_info *b)
{
	return a->ulhc_x + a->width > b->ulhc_x &&    
		b->ulhc_x + b->width > a->ulhc_x &&   
		a->ulhc_y + a->height > b->ulhc_y &&  
		b->ulhc_y + b->height > a->ulhc_y;    
}



enum direction_protect {
	N_PROTECT = (1 << 0),
	E_PROTECT = (1 << 1),
	S_PROTECT = (1 << 2),
	W_PROTECT = (1 << 3)
};

static void enable_firewall_interrupts(void)
{
	arch_local_irq_unmask_now(INT_UDN_FIREWALL);
}

static void disable_firewall_interrupts(void)
{
	arch_local_irq_mask_now(INT_UDN_FIREWALL);
}

static void hardwall_setup_ipi_func(void *info)
{
	struct hardwall_info *r = info;
	int cpu = smp_processor_id();
	int x = cpu % smp_width;
	int y = cpu / smp_width;
	int bits = 0;
	if (x == r->ulhc_x)
		bits |= W_PROTECT;
	if (x == r->ulhc_x + r->width - 1)
		bits |= E_PROTECT;
	if (y == r->ulhc_y)
		bits |= N_PROTECT;
	if (y == r->ulhc_y + r->height - 1)
		bits |= S_PROTECT;
	BUG_ON(bits == 0);
	__insn_mtspr(SPR_UDN_DIRECTION_PROTECT, bits);
	enable_firewall_interrupts();

}

static void hardwall_setup(struct hardwall_info *r)
{
	int x, y, cpu, delta;
	struct cpumask rect_cpus;

	cpumask_clear(&rect_cpus);

	
	cpu = r->ulhc_y * smp_width + r->ulhc_x;
	delta = (r->height - 1) * smp_width;
	for (x = 0; x < r->width; ++x, ++cpu) {
		cpu_online_set(cpu, &rect_cpus);
		cpu_online_set(cpu + delta, &rect_cpus);
	}

	
	cpu -= r->width;
	delta = r->width - 1;
	for (y = 0; y < r->height; ++y, cpu += smp_width) {
		cpu_online_set(cpu, &rect_cpus);
		cpu_online_set(cpu + delta, &rect_cpus);
	}

	
	on_each_cpu_mask(&rect_cpus, hardwall_setup_ipi_func, r, 1);
}

void __kprobes do_hardwall_trap(struct pt_regs* regs, int fault_num)
{
	struct hardwall_info *rect;
	struct task_struct *p;
	struct siginfo info;
	int x, y;
	int cpu = smp_processor_id();
	int found_processes;
	unsigned long flags;

	struct pt_regs *old_regs = set_irq_regs(regs);
	irq_enter();

	
	x = cpu % smp_width;
	y = cpu / smp_width;
	spin_lock_irqsave(&hardwall_lock, flags);
	list_for_each_entry(rect, &rectangles, list) {
		if (contains(rect, x, y))
			break;
	}

	BUG_ON(&rect->list == &rectangles);

	if (rect->teardown_in_progress) {
		pr_notice("cpu %d: detected hardwall violation %#lx"
		       " while teardown already in progress\n",
		       cpu, (long) __insn_mfspr(SPR_UDN_DIRECTION_PROTECT));
		goto done;
	}

	rect->teardown_in_progress = 1;
	wmb(); 
	pr_notice("cpu %d: detected hardwall violation %#lx...\n",
	       cpu, (long) __insn_mfspr(SPR_UDN_DIRECTION_PROTECT));
	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code = ILL_HARDWALL;
	found_processes = 0;
	list_for_each_entry(p, &rect->task_head, thread.hardwall_list) {
		BUG_ON(p->thread.hardwall != rect);
		if (!(p->flags & PF_EXITING)) {
			found_processes = 1;
			pr_notice("hardwall: killing %d\n", p->pid);
			do_send_sig_info(info.si_signo, &info, p, false);
		}
	}
	if (!found_processes)
		pr_notice("hardwall: no associated processes!\n");

 done:
	spin_unlock_irqrestore(&hardwall_lock, flags);

	disable_firewall_interrupts();

	irq_exit();
	set_irq_regs(old_regs);
}

void grant_network_mpls(void)
{
	__insn_mtspr(SPR_MPL_UDN_ACCESS_SET_0, 1);
	__insn_mtspr(SPR_MPL_UDN_AVAIL_SET_0, 1);
	__insn_mtspr(SPR_MPL_UDN_COMPLETE_SET_0, 1);
	__insn_mtspr(SPR_MPL_UDN_TIMER_SET_0, 1);
#if !CHIP_HAS_REV1_XDN()
	__insn_mtspr(SPR_MPL_UDN_REFILL_SET_0, 1);
	__insn_mtspr(SPR_MPL_UDN_CA_SET_0, 1);
#endif
}

void restrict_network_mpls(void)
{
	__insn_mtspr(SPR_MPL_UDN_ACCESS_SET_1, 1);
	__insn_mtspr(SPR_MPL_UDN_AVAIL_SET_1, 1);
	__insn_mtspr(SPR_MPL_UDN_COMPLETE_SET_1, 1);
	__insn_mtspr(SPR_MPL_UDN_TIMER_SET_1, 1);
#if !CHIP_HAS_REV1_XDN()
	__insn_mtspr(SPR_MPL_UDN_REFILL_SET_1, 1);
	__insn_mtspr(SPR_MPL_UDN_CA_SET_1, 1);
#endif
}



static struct hardwall_info *hardwall_create(
	size_t size, const unsigned char __user *bits)
{
	struct hardwall_info *iter, *rect;
	struct cpumask mask;
	unsigned long flags;
	int rc;

	
	if (size > PAGE_SIZE)
		return ERR_PTR(-EINVAL);

	
	if (copy_from_user(&mask, bits, min(sizeof(struct cpumask), size)))
		return ERR_PTR(-EFAULT);

	if (size < sizeof(struct cpumask)) {
		memset((char *)&mask + size, 0, sizeof(struct cpumask) - size);
	} else if (size > sizeof(struct cpumask)) {
		size_t i;
		for (i = sizeof(struct cpumask); i < size; ++i) {
			char c;
			if (get_user(c, &bits[i]))
				return ERR_PTR(-EFAULT);
			if (c)
				return ERR_PTR(-EINVAL);
		}
	}

	
	rect = kmalloc(sizeof(struct hardwall_info),
			GFP_KERNEL | __GFP_ZERO);
	if (rect == NULL)
		return ERR_PTR(-ENOMEM);
	INIT_LIST_HEAD(&rect->task_head);

	
	rc = setup_rectangle(rect, &mask);
	if (rc != 0) {
		kfree(rect);
		return ERR_PTR(rc);
	}

	
	spin_lock_irqsave(&hardwall_lock, flags);
	list_for_each_entry(iter, &rectangles, list) {
		if (overlaps(iter, rect)) {
			spin_unlock_irqrestore(&hardwall_lock, flags);
			kfree(rect);
			return ERR_PTR(-EBUSY);
		}
	}
	list_add_tail(&rect->list, &rectangles);
	spin_unlock_irqrestore(&hardwall_lock, flags);

	
	hardwall_setup(rect);

	
	hardwall_add_proc(rect);

	return rect;
}

static int hardwall_activate(struct hardwall_info *rect)
{
	int cpu, x, y;
	unsigned long flags;
	struct task_struct *p = current;
	struct thread_struct *ts = &p->thread;

	
	if (rect == NULL)
		return -ENODATA;

	
	if (rect->teardown_in_progress)
		return -EINVAL;

	if (cpumask_weight(&p->cpus_allowed) != 1)
		return -EPERM;

	
	cpu = smp_processor_id();
	BUG_ON(cpumask_first(&p->cpus_allowed) != cpu);
	x = cpu_x(cpu);
	y = cpu_y(cpu);
	if (!contains(rect, x, y))
		return -EINVAL;

	
	if (ts->hardwall) {
		BUG_ON(ts->hardwall != rect);
		return 0;
	}

	
	ts->hardwall = rect;
	spin_lock_irqsave(&hardwall_lock, flags);
	list_add(&ts->hardwall_list, &rect->task_head);
	spin_unlock_irqrestore(&hardwall_lock, flags);
	grant_network_mpls();
	printk(KERN_DEBUG "Pid %d (%s) activated for hardwall: cpu %d\n",
	       p->pid, p->comm, cpu);
	return 0;
}

static void _hardwall_deactivate(struct task_struct *task)
{
	struct thread_struct *ts = &task->thread;

	if (cpumask_weight(&task->cpus_allowed) != 1) {
		pr_err("pid %d (%s) releasing networks with"
		       " an affinity mask containing %d cpus!\n",
		       task->pid, task->comm,
		       cpumask_weight(&task->cpus_allowed));
		BUG();
	}

	BUG_ON(ts->hardwall == NULL);
	ts->hardwall = NULL;
	list_del(&ts->hardwall_list);
	if (task == current)
		restrict_network_mpls();
}

int hardwall_deactivate(struct task_struct *task)
{
	unsigned long flags;
	int activated;

	spin_lock_irqsave(&hardwall_lock, flags);
	activated = (task->thread.hardwall != NULL);
	if (activated)
		_hardwall_deactivate(task);
	spin_unlock_irqrestore(&hardwall_lock, flags);

	if (!activated)
		return -EINVAL;

	printk(KERN_DEBUG "Pid %d (%s) deactivated for hardwall: cpu %d\n",
	       task->pid, task->comm, smp_processor_id());
	return 0;
}

static void stop_udn_switch(void *ignored)
{
#if !CHIP_HAS_REV1_XDN()
	
	__insn_mtspr(SPR_UDN_SP_FREEZE,
		     SPR_UDN_SP_FREEZE__SP_FRZ_MASK |
		     SPR_UDN_SP_FREEZE__DEMUX_FRZ_MASK |
		     SPR_UDN_SP_FREEZE__NON_DEST_EXT_MASK);
#endif
}

static void drain_udn_switch(void *ignored)
{
#if !CHIP_HAS_REV1_XDN()
	int i;
	int from_tile_words, ca_count;

	
	for (i = 0; i < 5; i++) {
		int words, j;
		__insn_mtspr(SPR_UDN_SP_FIFO_SEL, i);
		words = __insn_mfspr(SPR_UDN_SP_STATE) & 0xF;
		for (j = 0; j < words; j++)
			(void) __insn_mfspr(SPR_UDN_SP_FIFO_DATA);
		BUG_ON((__insn_mfspr(SPR_UDN_SP_STATE) & 0xF) != 0);
	}

	
	from_tile_words = (__insn_mfspr(SPR_UDN_DEMUX_STATUS) >> 10) & 0x3;
	for (i = 0; i < from_tile_words; i++)
		(void) __insn_mfspr(SPR_UDN_DEMUX_WRITE_FIFO);

	
	while (__insn_mfspr(SPR_UDN_DATA_AVAIL) & (1 << 0))
		(void) __tile_udn0_receive();
	while (__insn_mfspr(SPR_UDN_DATA_AVAIL) & (1 << 1))
		(void) __tile_udn1_receive();
	while (__insn_mfspr(SPR_UDN_DATA_AVAIL) & (1 << 2))
		(void) __tile_udn2_receive();
	while (__insn_mfspr(SPR_UDN_DATA_AVAIL) & (1 << 3))
		(void) __tile_udn3_receive();
	BUG_ON((__insn_mfspr(SPR_UDN_DATA_AVAIL) & 0xF) != 0);

	
	ca_count = __insn_mfspr(SPR_UDN_DEMUX_CA_COUNT);
	for (i = 0; i < ca_count; i++)
		(void) __insn_mfspr(SPR_UDN_CA_DATA);
	BUG_ON(__insn_mfspr(SPR_UDN_DEMUX_CA_COUNT) != 0);

	
	__insn_mtspr(SPR_UDN_DEMUX_CTL, 1);

	for (i = 0; i < 5; i++) {
		__insn_mtspr(SPR_UDN_SP_FIFO_SEL, i);
		__insn_mtspr(SPR_UDN_SP_STATE, 0xc3000);
	}
#endif
}

void reset_network_state(void)
{
#if !CHIP_HAS_REV1_XDN()
	
	unsigned int cpu = smp_processor_id();
	unsigned int x = cpu % smp_width;
	unsigned int y = cpu / smp_width;
#endif

	if (udn_disabled)
		return;

#if !CHIP_HAS_REV1_XDN()
	__insn_mtspr(SPR_UDN_TILE_COORD, (x << 18) | (y << 7));

	
	__insn_mtspr(SPR_UDN_TAG_VALID, 0xf);
	__insn_mtspr(SPR_UDN_TAG_0, (1 << 0));
	__insn_mtspr(SPR_UDN_TAG_1, (1 << 1));
	__insn_mtspr(SPR_UDN_TAG_2, (1 << 2));
	__insn_mtspr(SPR_UDN_TAG_3, (1 << 3));
#endif

	
	__insn_mtspr(SPR_UDN_AVAIL_EN, 0);
	__insn_mtspr(SPR_UDN_DEADLOCK_TIMEOUT, 0);
#if !CHIP_HAS_REV1_XDN()
	__insn_mtspr(SPR_UDN_REFILL_EN, 0);
	__insn_mtspr(SPR_UDN_DEMUX_QUEUE_SEL, 0);
	__insn_mtspr(SPR_UDN_SP_FIFO_SEL, 0);
#endif

	
#if !CHIP_HAS_REV1_XDN()
	__insn_mtspr(SPR_UDN_SP_FREEZE, 0);
#endif
}

static void restart_udn_switch(void *ignored)
{
	reset_network_state();

	
	__insn_mtspr(SPR_UDN_DIRECTION_PROTECT, 0);
	disable_firewall_interrupts();
}

static void fill_mask(struct hardwall_info *r, struct cpumask *result)
{
	int x, y, cpu;

	cpumask_clear(result);

	cpu = r->ulhc_y * smp_width + r->ulhc_x;
	for (y = 0; y < r->height; ++y, cpu += smp_width - r->width) {
		for (x = 0; x < r->width; ++x, ++cpu)
			cpu_online_set(cpu, result);
	}
}

static void hardwall_destroy(struct hardwall_info *rect)
{
	struct task_struct *task;
	unsigned long flags;
	struct cpumask mask;

	
	if (rect == NULL)
		return;

	spin_lock_irqsave(&hardwall_lock, flags);
	list_for_each_entry(task, &rect->task_head, thread.hardwall_list)
		_hardwall_deactivate(task);
	spin_unlock_irqrestore(&hardwall_lock, flags);

	
	printk(KERN_DEBUG "Clearing hardwall rectangle %dx%d %d,%d\n",
	       rect->width, rect->height, rect->ulhc_x, rect->ulhc_y);
	fill_mask(rect, &mask);
	on_each_cpu_mask(&mask, stop_udn_switch, NULL, 1);
	on_each_cpu_mask(&mask, drain_udn_switch, NULL, 1);

	
	on_each_cpu_mask(&mask, restart_udn_switch, NULL, 1);

	
	hardwall_remove_proc(rect);

	
	spin_lock_irqsave(&hardwall_lock, flags);
	BUG_ON(!list_empty(&rect->task_head));
	list_del(&rect->list);
	spin_unlock_irqrestore(&hardwall_lock, flags);
	kfree(rect);
}


static int hardwall_proc_show(struct seq_file *sf, void *v)
{
	struct hardwall_info *rect = sf->private;
	char buf[256];

	int rc = cpulist_scnprintf(buf, sizeof(buf), &rect->cpumask);
	buf[rc++] = '\n';
	seq_write(sf, buf, rc);
	return 0;
}

static int hardwall_proc_open(struct inode *inode,
			      struct file *file)
{
	return single_open(file, hardwall_proc_show, PDE(inode)->data);
}

static const struct file_operations hardwall_proc_fops = {
	.open		= hardwall_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void hardwall_add_proc(struct hardwall_info *rect)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", rect->id);
	proc_create_data(buf, 0444, hardwall_proc_dir,
			 &hardwall_proc_fops, rect);
}

static void hardwall_remove_proc(struct hardwall_info *rect)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", rect->id);
	remove_proc_entry(buf, hardwall_proc_dir);
}

int proc_pid_hardwall(struct task_struct *task, char *buffer)
{
	struct hardwall_info *rect = task->thread.hardwall;
	return rect ? sprintf(buffer, "%d\n", rect->id) : 0;
}

void proc_tile_hardwall_init(struct proc_dir_entry *root)
{
	if (!udn_disabled)
		hardwall_proc_dir = proc_mkdir("hardwall", root);
}



static long hardwall_ioctl(struct file *file, unsigned int a, unsigned long b)
{
	struct hardwall_info *rect = file->private_data;

	if (_IOC_TYPE(a) != HARDWALL_IOCTL_BASE)
		return -EINVAL;

	switch (_IOC_NR(a)) {
	case _HARDWALL_CREATE:
		if (udn_disabled)
			return -ENOSYS;
		if (rect != NULL)
			return -EALREADY;
		rect = hardwall_create(_IOC_SIZE(a),
					(const unsigned char __user *)b);
		if (IS_ERR(rect))
			return PTR_ERR(rect);
		file->private_data = rect;
		return 0;

	case _HARDWALL_ACTIVATE:
		return hardwall_activate(rect);

	case _HARDWALL_DEACTIVATE:
		if (current->thread.hardwall != rect)
			return -EINVAL;
		return hardwall_deactivate(current);

	case _HARDWALL_GET_ID:
		return rect ? rect->id : -EINVAL;

	default:
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT
static long hardwall_compat_ioctl(struct file *file,
				  unsigned int a, unsigned long b)
{
	
	return hardwall_ioctl(file, a, (unsigned long)compat_ptr(b));
}
#endif

static int hardwall_flush(struct file *file, fl_owner_t owner)
{
	struct hardwall_info *rect = file->private_data;
	struct task_struct *task, *tmp;
	unsigned long flags;

	if (rect) {
		spin_lock_irqsave(&hardwall_lock, flags);
		list_for_each_entry_safe(task, tmp, &rect->task_head,
					 thread.hardwall_list) {
			if (task->files == owner || task->files == NULL)
				_hardwall_deactivate(task);
		}
		spin_unlock_irqrestore(&hardwall_lock, flags);
	}

	return 0;
}

static int hardwall_release(struct inode *inode, struct file *file)
{
	hardwall_destroy(file->private_data);
	return 0;
}

static const struct file_operations dev_hardwall_fops = {
	.open           = nonseekable_open,
	.unlocked_ioctl = hardwall_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hardwall_compat_ioctl,
#endif
	.flush          = hardwall_flush,
	.release        = hardwall_release,
};

static struct cdev hardwall_dev;

static int __init dev_hardwall_init(void)
{
	int rc;
	dev_t dev;

	rc = alloc_chrdev_region(&dev, 0, 1, "hardwall");
	if (rc < 0)
		return rc;
	cdev_init(&hardwall_dev, &dev_hardwall_fops);
	rc = cdev_add(&hardwall_dev, dev, 1);
	if (rc < 0)
		return rc;

	return 0;
}
late_initcall(dev_hardwall_init);
