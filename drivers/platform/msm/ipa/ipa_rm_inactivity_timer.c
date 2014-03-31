/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#include <mach/ipa.h>
#include "ipa_i.h"

struct ipa_rm_it_private {
	bool initied;
	enum ipa_rm_resource_name resource_name;
	spinlock_t lock;
	struct delayed_work work;
	bool release_in_prog;
	unsigned long jiffies;
};

static struct ipa_rm_it_private ipa_rm_it_handles[IPA_RM_RESOURCE_MAX];

static void ipa_rm_inactivity_timer_func(struct work_struct *work)
{

	struct ipa_rm_it_private *me = container_of(to_delayed_work(work),
						    struct ipa_rm_it_private,
						    work);
	unsigned long flags;

	IPADBG("%s: timer expired for resource %d!\n", __func__,
	    me->resource_name);

	
	spin_lock_irqsave(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
	if (ipa_rm_it_handles[me->resource_name].release_in_prog) {
		IPADBG("%s: calling release_resource on resource %d!\n",
		     __func__, me->resource_name);
		ipa_rm_release_resource(me->resource_name);
		ipa_rm_it_handles[me->resource_name].release_in_prog = false;
	}
	spin_unlock_irqrestore(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
}

int ipa_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
				 unsigned long msecs)
{
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: resource %d already inited\n",
		    __func__, resource_name);
		return -EINVAL;
	}

	spin_lock_init(&ipa_rm_it_handles[resource_name].lock);
	ipa_rm_it_handles[resource_name].resource_name = resource_name;
	ipa_rm_it_handles[resource_name].jiffies = msecs_to_jiffies(msecs);
	ipa_rm_it_handles[resource_name].release_in_prog = false;

	INIT_DELAYED_WORK(&ipa_rm_it_handles[resource_name].work,
			  ipa_rm_inactivity_timer_func);
	ipa_rm_it_handles[resource_name].initied = 1;

	return 0;
}

int ipa_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name)
{
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: resource %d already inited\n",
		    __func__, resource_name);
		return -EINVAL;
	}

	memset(&ipa_rm_it_handles[resource_name], 0,
	       sizeof(struct ipa_rm_it_private));

	return 0;
}

int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name)
{
	int ret;
	unsigned long flags;
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: Not initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	cancel_delayed_work(&ipa_rm_it_handles[resource_name].work);
	ipa_rm_it_handles[resource_name].release_in_prog = false;
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);
	ret = ipa_rm_request_resource(resource_name);
	IPADBG("%s: resource %d: returning %d\n", __func__, resource_name, ret);
	return ret;
}

int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name)
{
	unsigned long flags;
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: Not initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	if (ipa_rm_it_handles[resource_name].release_in_prog) {
		IPADBG("%s: Timer already set, not scheduling again %d\n",
		    __func__, resource_name);
		spin_unlock_irqrestore(
			&ipa_rm_it_handles[resource_name].lock, flags);
		return 0;
	}
	ipa_rm_it_handles[resource_name].release_in_prog = true;
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);

	IPADBG("%s: setting delayed work\n", __func__);
	schedule_delayed_work(&ipa_rm_it_handles[resource_name].work,
			      ipa_rm_it_handles[resource_name].jiffies);

	return 0;
}

