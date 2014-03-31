/*
 *  n_tracerouter.c - Trace data router through tty space
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
 * This trace router uses the Linux line discipline framework to route
 * trace data coming from a HW Modem to a PTI (Parallel Trace Module) port.
 * The solution is not specific to a HW modem and this line disciple can
 * be used to route any stream of data in kernel space.
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
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm-generic/bug.h>
#include "n_tracesink.h"

#define RECEIVE_ROOM	65536
#define DRIVERNAME	"n_tracerouter"

struct tracerouter_data {
	u8 opencalled;
	struct tty_struct *kref_tty;
};
static struct tracerouter_data *tr_data;

static DEFINE_MUTEX(routelock);

static int n_tracerouter_open(struct tty_struct *tty)
{
	int retval = -EEXIST;

	mutex_lock(&routelock);
	if (tr_data->opencalled == 0) {

		tr_data->kref_tty = tty_kref_get(tty);
		if (tr_data->kref_tty == NULL) {
			retval = -EFAULT;
		} else {
			tr_data->opencalled = 1;
			tty->disc_data      = tr_data;
			tty->receive_room   = RECEIVE_ROOM;
			tty_driver_flush_buffer(tty);
			retval = 0;
		}
	}
	mutex_unlock(&routelock);
	return retval;
}

static void n_tracerouter_close(struct tty_struct *tty)
{
	struct tracerouter_data *tptr = tty->disc_data;

	mutex_lock(&routelock);
	WARN_ON(tptr->kref_tty != tr_data->kref_tty);
	tty_driver_flush_buffer(tty);
	tty_kref_put(tr_data->kref_tty);
	tr_data->kref_tty = NULL;
	tr_data->opencalled = 0;
	tty->disc_data = NULL;
	mutex_unlock(&routelock);
}

static ssize_t n_tracerouter_read(struct tty_struct *tty, struct file *file,
				  unsigned char __user *buf, size_t nr) {
	return -EINVAL;
}

static ssize_t n_tracerouter_write(struct tty_struct *tty, struct file *file,
				   const unsigned char *buf, size_t nr) {
	return -EINVAL;
}

static void n_tracerouter_receivebuf(struct tty_struct *tty,
					const unsigned char *cp,
					char *fp, int count)
{
	mutex_lock(&routelock);
	n_tracesink_datadrain((u8 *) cp, count);
	mutex_unlock(&routelock);
}


static struct tty_ldisc_ops tty_ptirouter_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= DRIVERNAME,
	.open		= n_tracerouter_open,
	.close		= n_tracerouter_close,
	.read		= n_tracerouter_read,
	.write		= n_tracerouter_write,
	.receive_buf	= n_tracerouter_receivebuf
};

static int __init n_tracerouter_init(void)
{
	int retval;

	tr_data = kzalloc(sizeof(struct tracerouter_data), GFP_KERNEL);
	if (tr_data == NULL)
		return -ENOMEM;


	
	retval = tty_register_ldisc(N_TRACEROUTER, &tty_ptirouter_ldisc);
	if (retval < 0) {
		pr_err("%s: Registration failed: %d\n", __func__, retval);
		kfree(tr_data);
	}
	return retval;
}

static void __exit n_tracerouter_exit(void)
{
	int retval = tty_unregister_ldisc(N_TRACEROUTER);

	if (retval < 0)
		pr_err("%s: Unregistration failed: %d\n", __func__,  retval);
	else
		kfree(tr_data);
}

module_init(n_tracerouter_init);
module_exit(n_tracerouter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jay Freyensee");
MODULE_ALIAS_LDISC(N_TRACEROUTER);
MODULE_DESCRIPTION("Trace router ldisc driver");
