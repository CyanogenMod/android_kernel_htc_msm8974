/*
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/init_task.h>
#include <linux/mqueue.h>
#include <linux/fs.h>

static struct signal_struct init_signals = INIT_SIGNALS(init_signals);
static struct sighand_struct init_sighand = INIT_SIGHAND(init_sighand);
struct task_struct init_task = INIT_TASK(init_task);
EXPORT_SYMBOL(init_task);

union thread_union init_thread_union
    __init_task_data = {
INIT_THREAD_INFO(init_task)};
