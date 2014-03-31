/*
 * Copyright (C) 2001 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include "sysrq.h"

#if !defined(CONFIG_UML_X86) || defined(CONFIG_64BIT)
void show_trace(struct task_struct *task, unsigned long * stack)
{
	unsigned long addr;

	if (!stack) {
		stack = (unsigned long*) &stack;
		WARN_ON(1);
	}

	printk(KERN_INFO "Call Trace: \n");
	while (((long) stack & (THREAD_SIZE-1)) != 0) {
		addr = *stack;
		if (__kernel_text_address(addr)) {
			printk(KERN_INFO "%08lx:  [<%08lx>]",
			       (unsigned long) stack, addr);
			print_symbol(KERN_CONT " %s", addr);
			printk(KERN_CONT "\n");
		}
		stack++;
	}
	printk(KERN_INFO "\n");
}
#endif

void dump_stack(void)
{
	unsigned long stack;

	show_trace(current, &stack);
}
EXPORT_SYMBOL(dump_stack);

static const int kstack_depth_to_print = 24;

void show_stack(struct task_struct *task, unsigned long *esp)
{
	unsigned long *stack;
	int i;

	if (esp == NULL) {
		if (task != current && task != NULL) {
			esp = (unsigned long *) KSTK_ESP(task);
		} else {
			esp = (unsigned long *) &esp;
		}
	}

	stack = esp;
	for (i = 0; i < kstack_depth_to_print; i++) {
		if (kstack_end(stack))
			break;
		if (i && ((i % 8) == 0))
			printk(KERN_INFO "       ");
		printk(KERN_CONT "%08lx ", *stack++);
	}

	show_trace(task, esp);
}
