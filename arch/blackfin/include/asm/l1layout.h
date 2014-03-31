/*
 * Defines a layout of L1 scratchpad memory that userspace can rely on.
 *
 * Copyright 2006-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _L1LAYOUT_H_
#define _L1LAYOUT_H_

#include <asm/blackfin.h>

#ifndef CONFIG_SMP
#ifndef __ASSEMBLY__

struct l1_scratch_task_info
{
	
	void *stack_start;
	void *lowest_sp;
};

#define L1_SCRATCH_TASK_INFO ((struct l1_scratch_task_info *)\
						get_l1_scratch_start())

#endif
#endif

#endif
