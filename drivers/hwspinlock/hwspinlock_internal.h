/*
 * Hardware spinlocks internal header
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com
 *
 * Contact: Ohad Ben-Cohen <ohad@wizery.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __HWSPINLOCK_HWSPINLOCK_H
#define __HWSPINLOCK_HWSPINLOCK_H

#include <linux/spinlock.h>
#include <linux/device.h>

struct hwspinlock_device;

struct hwspinlock_ops {
	int (*trylock)(struct hwspinlock *lock);
	void (*unlock)(struct hwspinlock *lock);
	void (*relax)(struct hwspinlock *lock);
};

struct hwspinlock {
	struct hwspinlock_device *bank;
	spinlock_t lock;
	void *priv;
};

struct hwspinlock_device {
	struct device *dev;
	const struct hwspinlock_ops *ops;
	int base_id;
	int num_locks;
	struct hwspinlock lock[0];
};

static inline int hwlock_to_id(struct hwspinlock *hwlock)
{
	int local_id = hwlock - &hwlock->bank->lock[0];

	return hwlock->bank->base_id + local_id;
}

#endif 
