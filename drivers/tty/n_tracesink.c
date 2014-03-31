/*
 *  n_tracesink.c - Trace data router and sink path through tty space.
 *
 *  Copyright (C) Intel 2011
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The trace sink uses the Linux line discipline framework to receive
 * trace data coming from the PTI source line discipline driver
 * to a user-desired tty port, like USB.
 * This is to provide a way to extract modem trace data on
 * devices that do not have a PTI HW module, or just need modem
 * trace data to come out of a different HW output port.
 * This is part of a solution for the P1149.7, compact JTAG, standard.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/tty.h>
#include <linux/tty_ldisc.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <asm-generic/bug.h>
#include "n_tracesink.h"

#define RECEIVE_ROOM	65536
#define DRIVERNAME	"n_tracesink"

static struct tty_struct *this_tty;
static DEFINE_MUTEX(writelock);

static int n_tracesink_open(struct tty_struct *tty)
{
	int retval = -EEXIST;

	mutex_lock(&writelock);
	if (this_tty == NULL) {
		this_tty = tty_kref_get(tty);
		if (this_tty == NULL) {
			retval = -EFAULT;
		} else {
			tty->disc_data = this_tty;
			tty_driver_flush_buffer(tty);
			retval = 0;
		}
	}
	mutex_unlock(&writelock);

	return retval;
}

static void n_tracesink_close(struct tty_struct *tty)
{
	mutex_lock(&writelock);
	tty_driver_flush_buffer(tty);
	tty_kref_put(this_tty);
	this_tty = NULL;
	tty->disc_data = NULL;
	mutex_unlock(&writelock);
}

static ssize_t n_tracesink_read(struct tty_struct *tty, struct file *file,
				unsigned char __user *buf, size_t nr) {
	return -EINVAL;
}

static ssize_t n_tracesink_write(struct tty_struct *tty, struct file *file,
				 const unsigned char *buf, size_t nr) {
	return -EINVAL;
}

void n_tracesink_datadrain(u8 *buf, int count)
{
	mutex_lock(&writelock);

	if ((buf != NULL) && (count > 0) && (this_tty != NULL))
		this_tty->ops->write(this_tty, buf, count);

	mutex_unlock(&writelock);
}
EXPORT_SYMBOL_GPL(n_tracesink_datadrain);


static struct tty_ldisc_ops tty_n_tracesink = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= DRIVERNAME,
	.open		= n_tracesink_open,
	.close		= n_tracesink_close,
	.read		= n_tracesink_read,
	.write		= n_tracesink_write
};

static int __init n_tracesink_init(void)
{
	
	int retval = tty_register_ldisc(N_TRACESINK, &tty_n_tracesink);

	if (retval < 0)
		pr_err("%s: Registration failed: %d\n", __func__, retval);

	return retval;
}

static void __exit n_tracesink_exit(void)
{
	int retval = tty_unregister_ldisc(N_TRACESINK);

	if (retval < 0)
		pr_err("%s: Unregistration failed: %d\n", __func__,  retval);
}

module_init(n_tracesink_init);
module_exit(n_tracesink_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jay Freyensee");
MODULE_ALIAS_LDISC(N_TRACESINK);
MODULE_DESCRIPTION("Trace sink ldisc driver");
