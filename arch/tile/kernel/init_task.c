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

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init_task.h>
#include <linux/mqueue.h>
#include <linux/module.h>
#include <linux/start_kernel.h>
#include <linux/uaccess.h>

static struct signal_struct init_signals = INIT_SIGNALS(init_signals);
static struct sighand_struct init_sighand = INIT_SIGHAND(init_sighand);

union thread_union init_thread_union __init_task_data = {
	INIT_THREAD_INFO(init_task)
};

struct task_struct init_task = INIT_TASK(init_task);
EXPORT_SYMBOL(init_task);

DEFINE_PER_CPU(unsigned long, boot_sp) =
	(unsigned long)init_stack + THREAD_SIZE;

#ifdef CONFIG_SMP
DEFINE_PER_CPU(unsigned long, boot_pc) = (unsigned long)start_kernel;
#else
unsigned long __initdata boot_pc = (unsigned long)start_kernel;
#endif
