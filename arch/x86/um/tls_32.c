/*
 * Copyright (C) 2005 Paolo 'Blaisorblade' Giarrusso <blaisorblade@yahoo.it>
 * Licensed under the GPL
 */

#include "linux/percpu.h"
#include "linux/sched.h"
#include "asm/uaccess.h"
#include "os.h"
#include "skas.h"
#include "sysdep/tls.h"

static int host_supports_tls = -1;
int host_gdt_entry_tls_min;

int do_set_thread_area(struct user_desc *info)
{
	int ret;
	u32 cpu;

	cpu = get_cpu();
	ret = os_set_thread_area(info, userspace_pid[cpu]);
	put_cpu();

	if (ret)
		printk(KERN_ERR "PTRACE_SET_THREAD_AREA failed, err = %d, "
		       "index = %d\n", ret, info->entry_number);

	return ret;
}

int do_get_thread_area(struct user_desc *info)
{
	int ret;
	u32 cpu;

	cpu = get_cpu();
	ret = os_get_thread_area(info, userspace_pid[cpu]);
	put_cpu();

	if (ret)
		printk(KERN_ERR "PTRACE_GET_THREAD_AREA failed, err = %d, "
		       "index = %d\n", ret, info->entry_number);

	return ret;
}

static int get_free_idx(struct task_struct* task)
{
	struct thread_struct *t = &task->thread;
	int idx;

	if (!t->arch.tls_array)
		return GDT_ENTRY_TLS_MIN;

	for (idx = 0; idx < GDT_ENTRY_TLS_ENTRIES; idx++)
		if (!t->arch.tls_array[idx].present)
			return idx + GDT_ENTRY_TLS_MIN;
	return -ESRCH;
}

static inline void clear_user_desc(struct user_desc* info)
{
	
	memset(info, 0, sizeof(*info));

	info->read_exec_only = 1;
	info->seg_not_present = 1;
}

#define O_FORCE 1

static int load_TLS(int flags, struct task_struct *to)
{
	int ret = 0;
	int idx;

	for (idx = GDT_ENTRY_TLS_MIN; idx < GDT_ENTRY_TLS_MAX; idx++) {
		struct uml_tls_struct* curr =
			&to->thread.arch.tls_array[idx - GDT_ENTRY_TLS_MIN];

		if (!curr->present) {
			if (!curr->flushed) {
				clear_user_desc(&curr->tls);
				curr->tls.entry_number = idx;
			} else {
				WARN_ON(!LDT_empty(&curr->tls));
				continue;
			}
		}

		if (!(flags & O_FORCE) && curr->flushed)
			continue;

		ret = do_set_thread_area(&curr->tls);
		if (ret)
			goto out;

		curr->flushed = 1;
	}
out:
	return ret;
}

static inline int needs_TLS_update(struct task_struct *task)
{
	int i;
	int ret = 0;

	for (i = GDT_ENTRY_TLS_MIN; i < GDT_ENTRY_TLS_MAX; i++) {
		struct uml_tls_struct* curr =
			&task->thread.arch.tls_array[i - GDT_ENTRY_TLS_MIN];

		if (curr->flushed)
			continue;
		ret = 1;
		break;
	}
	return ret;
}

void clear_flushed_tls(struct task_struct *task)
{
	int i;

	for (i = GDT_ENTRY_TLS_MIN; i < GDT_ENTRY_TLS_MAX; i++) {
		struct uml_tls_struct* curr =
			&task->thread.arch.tls_array[i - GDT_ENTRY_TLS_MIN];

		if (!curr->present)
			continue;

		curr->flushed = 0;
	}
}


int arch_switch_tls(struct task_struct *to)
{
	if (!host_supports_tls)
		return 0;

	if (likely(to->mm))
		return load_TLS(O_FORCE, to);

	return 0;
}

static int set_tls_entry(struct task_struct* task, struct user_desc *info,
			 int idx, int flushed)
{
	struct thread_struct *t = &task->thread;

	if (idx < GDT_ENTRY_TLS_MIN || idx > GDT_ENTRY_TLS_MAX)
		return -EINVAL;

	t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].tls = *info;
	t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].present = 1;
	t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].flushed = flushed;

	return 0;
}

int arch_copy_tls(struct task_struct *new)
{
	struct user_desc info;
	int idx, ret = -EFAULT;

	if (copy_from_user(&info,
			   (void __user *) UPT_ESI(&new->thread.regs.regs),
			   sizeof(info)))
		goto out;

	ret = -EINVAL;
	if (LDT_empty(&info))
		goto out;

	idx = info.entry_number;

	ret = set_tls_entry(new, &info, idx, 0);
out:
	return ret;
}

static int get_tls_entry(struct task_struct *task, struct user_desc *info,
			 int idx)
{
	struct thread_struct *t = &task->thread;

	if (!t->arch.tls_array)
		goto clear;

	if (idx < GDT_ENTRY_TLS_MIN || idx > GDT_ENTRY_TLS_MAX)
		return -EINVAL;

	if (!t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].present)
		goto clear;

	*info = t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].tls;

out:
	if (unlikely(task == current &&
		     !t->arch.tls_array[idx - GDT_ENTRY_TLS_MIN].flushed)) {
		printk(KERN_ERR "get_tls_entry: task with pid %d got here "
				"without flushed TLS.", current->pid);
	}

	return 0;
clear:
	clear_user_desc(info);
	info->entry_number = idx;
	goto out;
}

int sys_set_thread_area(struct user_desc __user *user_desc)
{
	struct user_desc info;
	int idx, ret;

	if (!host_supports_tls)
		return -ENOSYS;

	if (copy_from_user(&info, user_desc, sizeof(info)))
		return -EFAULT;

	idx = info.entry_number;

	if (idx == -1) {
		idx = get_free_idx(current);
		if (idx < 0)
			return idx;
		info.entry_number = idx;
		
		if (put_user(idx, &user_desc->entry_number))
			return -EFAULT;
	}

	ret = do_set_thread_area(&info);
	if (ret)
		return ret;
	return set_tls_entry(current, &info, idx, 1);
}

int ptrace_set_thread_area(struct task_struct *child, int idx,
			   struct user_desc __user *user_desc)
{
	struct user_desc info;

	if (!host_supports_tls)
		return -EIO;

	if (copy_from_user(&info, user_desc, sizeof(info)))
		return -EFAULT;

	return set_tls_entry(child, &info, idx, 0);
}

int sys_get_thread_area(struct user_desc __user *user_desc)
{
	struct user_desc info;
	int idx, ret;

	if (!host_supports_tls)
		return -ENOSYS;

	if (get_user(idx, &user_desc->entry_number))
		return -EFAULT;

	ret = get_tls_entry(current, &info, idx);
	if (ret < 0)
		goto out;

	if (copy_to_user(user_desc, &info, sizeof(info)))
		ret = -EFAULT;

out:
	return ret;
}

int ptrace_get_thread_area(struct task_struct *child, int idx,
		struct user_desc __user *user_desc)
{
	struct user_desc info;
	int ret;

	if (!host_supports_tls)
		return -EIO;

	ret = get_tls_entry(child, &info, idx);
	if (ret < 0)
		goto out;

	if (copy_to_user(user_desc, &info, sizeof(info)))
		ret = -EFAULT;
out:
	return ret;
}

static int __init __setup_host_supports_tls(void)
{
	check_host_supports_tls(&host_supports_tls, &host_gdt_entry_tls_min);
	if (host_supports_tls) {
		printk(KERN_INFO "Host TLS support detected\n");
		printk(KERN_INFO "Detected host type: ");
		switch (host_gdt_entry_tls_min) {
		case GDT_ENTRY_TLS_MIN_I386:
			printk(KERN_CONT "i386");
			break;
		case GDT_ENTRY_TLS_MIN_X86_64:
			printk(KERN_CONT "x86_64");
			break;
		}
		printk(KERN_CONT " (GDT indexes %d to %d)\n",
		       host_gdt_entry_tls_min,
		       host_gdt_entry_tls_min + GDT_ENTRY_TLS_ENTRIES);
	} else
		printk(KERN_ERR "  Host TLS support NOT detected! "
				"TLS support inside UML will not work\n");
	return 0;
}

__initcall(__setup_host_supports_tls);
