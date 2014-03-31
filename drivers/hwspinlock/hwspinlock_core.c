/*
 * Hardware spinlock framework
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

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/radix-tree.h>
#include <linux/hwspinlock.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>

#include "hwspinlock_internal.h"

#define HWSPINLOCK_UNUSED	(0) 

static RADIX_TREE(hwspinlock_tree, GFP_KERNEL);

static DEFINE_MUTEX(hwspinlock_tree_lock);


int __hwspin_trylock(struct hwspinlock *hwlock, int mode, unsigned long *flags)
{
	int ret;

	BUG_ON(!hwlock);
	BUG_ON(!flags && mode == HWLOCK_IRQSTATE);

	if (mode == HWLOCK_IRQSTATE)
		ret = spin_trylock_irqsave(&hwlock->lock, *flags);
	else if (mode == HWLOCK_IRQ)
		ret = spin_trylock_irq(&hwlock->lock);
	else
		ret = spin_trylock(&hwlock->lock);

	
	if (!ret)
		return -EBUSY;

	
	ret = hwlock->bank->ops->trylock(hwlock);

	
	if (!ret) {
		if (mode == HWLOCK_IRQSTATE)
			spin_unlock_irqrestore(&hwlock->lock, *flags);
		else if (mode == HWLOCK_IRQ)
			spin_unlock_irq(&hwlock->lock);
		else
			spin_unlock(&hwlock->lock);

		return -EBUSY;
	}

	mb();

	return 0;
}
EXPORT_SYMBOL_GPL(__hwspin_trylock);

int __hwspin_lock_timeout(struct hwspinlock *hwlock, unsigned int to,
					int mode, unsigned long *flags)
{
	int ret;
	unsigned long expire;

	expire = msecs_to_jiffies(to) + jiffies;

	for (;;) {
		
		ret = __hwspin_trylock(hwlock, mode, flags);
		if (ret != -EBUSY)
			break;

		if (time_is_before_eq_jiffies(expire))
			return -ETIMEDOUT;

		if (hwlock->bank->ops->relax)
			hwlock->bank->ops->relax(hwlock);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(__hwspin_lock_timeout);

void __hwspin_unlock(struct hwspinlock *hwlock, int mode, unsigned long *flags)
{
	BUG_ON(!hwlock);
	BUG_ON(!flags && mode == HWLOCK_IRQSTATE);

	mb();

	hwlock->bank->ops->unlock(hwlock);

	
	if (mode == HWLOCK_IRQSTATE)
		spin_unlock_irqrestore(&hwlock->lock, *flags);
	else if (mode == HWLOCK_IRQ)
		spin_unlock_irq(&hwlock->lock);
	else
		spin_unlock(&hwlock->lock);
}
EXPORT_SYMBOL_GPL(__hwspin_unlock);

static int hwspin_lock_register_single(struct hwspinlock *hwlock, int id)
{
	struct hwspinlock *tmp;
	int ret;

	mutex_lock(&hwspinlock_tree_lock);

	ret = radix_tree_insert(&hwspinlock_tree, id, hwlock);
	if (ret) {
		if (ret == -EEXIST)
			pr_err("hwspinlock id %d already exists!\n", id);
		goto out;
	}

	
	tmp = radix_tree_tag_set(&hwspinlock_tree, id, HWSPINLOCK_UNUSED);

	
	WARN_ON(tmp != hwlock);

out:
	mutex_unlock(&hwspinlock_tree_lock);
	return 0;
}

static struct hwspinlock *hwspin_lock_unregister_single(unsigned int id)
{
	struct hwspinlock *hwlock = NULL;
	int ret;

	mutex_lock(&hwspinlock_tree_lock);

	
	ret = radix_tree_tag_get(&hwspinlock_tree, id, HWSPINLOCK_UNUSED);
	if (ret == 0) {
		pr_err("hwspinlock %d still in use (or not present)\n", id);
		goto out;
	}

	hwlock = radix_tree_delete(&hwspinlock_tree, id);
	if (!hwlock) {
		pr_err("failed to delete hwspinlock %d\n", id);
		goto out;
	}

out:
	mutex_unlock(&hwspinlock_tree_lock);
	return hwlock;
}

int hwspin_lock_register(struct hwspinlock_device *bank, struct device *dev,
		const struct hwspinlock_ops *ops, int base_id, int num_locks)
{
	struct hwspinlock *hwlock;
	int ret = 0, i;

	if (!bank || !ops || !dev || !num_locks || !ops->trylock ||
							!ops->unlock) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	bank->dev = dev;
	bank->ops = ops;
	bank->base_id = base_id;
	bank->num_locks = num_locks;

	for (i = 0; i < num_locks; i++) {
		hwlock = &bank->lock[i];

		spin_lock_init(&hwlock->lock);
		hwlock->bank = bank;

		ret = hwspin_lock_register_single(hwlock, i);
		if (ret)
			goto reg_failed;
	}

	return 0;

reg_failed:
	while (--i >= 0)
		hwspin_lock_unregister_single(i);
	return ret;
}
EXPORT_SYMBOL_GPL(hwspin_lock_register);

int hwspin_lock_unregister(struct hwspinlock_device *bank)
{
	struct hwspinlock *hwlock, *tmp;
	int i;

	for (i = 0; i < bank->num_locks; i++) {
		hwlock = &bank->lock[i];

		tmp = hwspin_lock_unregister_single(bank->base_id + i);
		if (!tmp)
			return -EBUSY;

		
		WARN_ON(tmp != hwlock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hwspin_lock_unregister);

static int __hwspin_lock_request(struct hwspinlock *hwlock)
{
	struct device *dev = hwlock->bank->dev;
	struct hwspinlock *tmp;
	int ret;

	
	if (!try_module_get(dev->driver->owner)) {
		dev_err(dev, "%s: can't get owner\n", __func__);
		return -EINVAL;
	}

	
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "%s: can't power on device\n", __func__);
		return ret;
	}

	
	tmp = radix_tree_tag_clear(&hwspinlock_tree, hwlock_to_id(hwlock),
							HWSPINLOCK_UNUSED);

	
	WARN_ON(tmp != hwlock);

	return ret;
}

int hwspin_lock_get_id(struct hwspinlock *hwlock)
{
	if (!hwlock) {
		pr_err("invalid hwlock\n");
		return -EINVAL;
	}

	return hwlock_to_id(hwlock);
}
EXPORT_SYMBOL_GPL(hwspin_lock_get_id);

struct hwspinlock *hwspin_lock_request(void)
{
	struct hwspinlock *hwlock;
	int ret;

	mutex_lock(&hwspinlock_tree_lock);

	
	ret = radix_tree_gang_lookup_tag(&hwspinlock_tree, (void **)&hwlock,
						0, 1, HWSPINLOCK_UNUSED);
	if (ret == 0) {
		pr_warn("a free hwspinlock is not available\n");
		hwlock = NULL;
		goto out;
	}

	
	WARN_ON(ret > 1);

	
	ret = __hwspin_lock_request(hwlock);
	if (ret < 0)
		hwlock = NULL;

out:
	mutex_unlock(&hwspinlock_tree_lock);
	return hwlock;
}
EXPORT_SYMBOL_GPL(hwspin_lock_request);

struct hwspinlock *hwspin_lock_request_specific(unsigned int id)
{
	struct hwspinlock *hwlock;
	int ret;

	mutex_lock(&hwspinlock_tree_lock);

	
	hwlock = radix_tree_lookup(&hwspinlock_tree, id);
	if (!hwlock) {
		pr_warn("hwspinlock %u does not exist\n", id);
		goto out;
	}

	
	WARN_ON(hwlock_to_id(hwlock) != id);

	
	ret = radix_tree_tag_get(&hwspinlock_tree, id, HWSPINLOCK_UNUSED);
	if (ret == 0) {
		pr_warn("hwspinlock %u is already in use\n", id);
		hwlock = NULL;
		goto out;
	}

	
	ret = __hwspin_lock_request(hwlock);
	if (ret < 0)
		hwlock = NULL;

out:
	mutex_unlock(&hwspinlock_tree_lock);
	return hwlock;
}
EXPORT_SYMBOL_GPL(hwspin_lock_request_specific);

int hwspin_lock_free(struct hwspinlock *hwlock)
{
	struct device *dev = hwlock->bank->dev;
	struct hwspinlock *tmp;
	int ret;

	if (!hwlock) {
		pr_err("invalid hwlock\n");
		return -EINVAL;
	}

	mutex_lock(&hwspinlock_tree_lock);

	
	ret = radix_tree_tag_get(&hwspinlock_tree, hwlock_to_id(hwlock),
							HWSPINLOCK_UNUSED);
	if (ret == 1) {
		dev_err(dev, "%s: hwlock is already free\n", __func__);
		dump_stack();
		ret = -EINVAL;
		goto out;
	}

	
	ret = pm_runtime_put(dev);
	if (ret < 0)
		goto out;

	
	tmp = radix_tree_tag_set(&hwspinlock_tree, hwlock_to_id(hwlock),
							HWSPINLOCK_UNUSED);

	
	WARN_ON(tmp != hwlock);

	module_put(dev->driver->owner);

out:
	mutex_unlock(&hwspinlock_tree_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(hwspin_lock_free);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock interface");
MODULE_AUTHOR("Ohad Ben-Cohen <ohad@wizery.com>");
