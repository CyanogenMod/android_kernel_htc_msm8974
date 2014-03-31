/*
 * Init task definition
 *
 * Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init_task.h>
#include <linux/fs.h>
#include <linux/mqueue.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>

static struct signal_struct init_signals = INIT_SIGNALS(init_signals);
static struct sighand_struct init_sighand = INIT_SIGHAND(init_sighand);

union thread_union init_thread_union
	__attribute__((__section__(".data.init_task"),
		__aligned__(THREAD_SIZE))) = {
			INIT_THREAD_INFO(init_task)
		};

struct task_struct init_task = INIT_TASK(init_task);
EXPORT_SYMBOL(init_task);
