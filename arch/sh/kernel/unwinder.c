/*
 * Copyright (C) 2009  Matt Fleming
 *
 * Based, in part, on kernel/time/clocksource.c.
 *
 * This file provides arbitration code for stack unwinders.
 *
 * Multiple stack unwinders can be available on a system, usually with
 * the most accurate unwinder being the currently active one.
 */
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/unwinder.h>
#include <linux/atomic.h>

static struct list_head unwinder_list;
static struct unwinder stack_reader = {
	.name = "stack-reader",
	.dump = stack_reader_dump,
	.rating = 50,
	.list = {
		.next = &unwinder_list,
		.prev = &unwinder_list,
	},
};

static struct unwinder *curr_unwinder = &stack_reader;

static struct list_head unwinder_list = {
	.next = &stack_reader.list,
	.prev = &stack_reader.list,
};

static DEFINE_SPINLOCK(unwinder_lock);

static struct unwinder *select_unwinder(void)
{
	struct unwinder *best;

	if (list_empty(&unwinder_list))
		return NULL;

	best = list_entry(unwinder_list.next, struct unwinder, list);
	if (best == curr_unwinder)
		return NULL;

	return best;
}

static int unwinder_enqueue(struct unwinder *ops)
{
	struct list_head *tmp, *entry = &unwinder_list;

	list_for_each(tmp, &unwinder_list) {
		struct unwinder *o;

		o = list_entry(tmp, struct unwinder, list);
		if (o == ops)
			return -EBUSY;
		
		if (o->rating >= ops->rating)
			entry = tmp;
	}
	list_add(&ops->list, entry);

	return 0;
}

int unwinder_register(struct unwinder *u)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&unwinder_lock, flags);
	ret = unwinder_enqueue(u);
	if (!ret)
		curr_unwinder = select_unwinder();
	spin_unlock_irqrestore(&unwinder_lock, flags);

	return ret;
}

int unwinder_faulted = 0;

void unwind_stack(struct task_struct *task, struct pt_regs *regs,
		  unsigned long *sp, const struct stacktrace_ops *ops,
		  void *data)
{
	unsigned long flags;

	if (unwinder_faulted) {
		spin_lock_irqsave(&unwinder_lock, flags);

		
		if (unwinder_faulted && !list_is_singular(&unwinder_list)) {
			list_del(&curr_unwinder->list);
			curr_unwinder = select_unwinder();

			unwinder_faulted = 0;
		}

		spin_unlock_irqrestore(&unwinder_lock, flags);
	}

	curr_unwinder->dump(task, regs, sp, ops, data);
}
EXPORT_SYMBOL_GPL(unwind_stack);
