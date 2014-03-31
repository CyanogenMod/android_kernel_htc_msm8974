/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_SWITCH_TO_H
#define __ASM_AVR32_SWITCH_TO_H

#ifdef CONFIG_OWNERSHIP_TRACE
#include <asm/ocd.h>
#define finish_arch_switch(prev)			\
	do {						\
		ocd_write(PID, prev->pid);		\
		ocd_write(PID, current->pid);		\
	} while(0)
#endif

struct cpu_context;
struct task_struct;
extern struct task_struct *__switch_to(struct task_struct *,
				       struct cpu_context *,
				       struct cpu_context *);
#define switch_to(prev, next, last)					\
	do {								\
		last = __switch_to(prev, &prev->thread.cpu_context + 1,	\
				   &next->thread.cpu_context);		\
	} while (0)


#endif 
