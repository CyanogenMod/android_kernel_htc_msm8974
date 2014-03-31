/*
 * Kernel-based Virtual Machine driver for Linux
 *
 * This module enables machines with Intel VT-x extensions to run virtual
 * machines without emulation or binary translation.
 *
 * timer support
 *
 * Copyright 2010 Red Hat, Inc. and/or its affiliates.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/kvm_host.h>
#include <linux/kvm.h>
#include <linux/hrtimer.h>
#include <linux/atomic.h>
#include "kvm_timer.h"

enum hrtimer_restart kvm_timer_fn(struct hrtimer *data)
{
	struct kvm_timer *ktimer = container_of(data, struct kvm_timer, timer);
	struct kvm_vcpu *vcpu = ktimer->vcpu;
	wait_queue_head_t *q = &vcpu->wq;

	if (ktimer->reinject || !atomic_read(&ktimer->pending)) {
		atomic_inc(&ktimer->pending);
		
		kvm_make_request(KVM_REQ_PENDING_TIMER, vcpu);
	}

	if (waitqueue_active(q))
		wake_up_interruptible(q);

	if (ktimer->t_ops->is_periodic(ktimer)) {
		hrtimer_add_expires_ns(&ktimer->timer, ktimer->period);
		return HRTIMER_RESTART;
	} else
		return HRTIMER_NORESTART;
}
