#ifndef _M68K_CURRENT_H
#define _M68K_CURRENT_H

#ifdef CONFIG_MMU

register struct task_struct *current __asm__("%a2");

#else

#include <linux/thread_info.h>

struct task_struct;

static inline struct task_struct *get_current(void)
{
	return(current_thread_info()->task);
}

#define	current	get_current()

#endif 

#endif 
