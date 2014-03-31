#ifndef _ASM_IA64_PERCPU_H
#define _ASM_IA64_PERCPU_H

/*
 * Copyright (C) 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#define PERCPU_ENOUGH_ROOM PERCPU_PAGE_SIZE

#ifdef __ASSEMBLY__
# define THIS_CPU(var)	(var)  
#else 


#include <linux/threads.h>

#ifdef CONFIG_SMP

#ifdef HAVE_MODEL_SMALL_ATTRIBUTE
# define PER_CPU_ATTRIBUTES	__attribute__((__model__ (__small__)))
#endif

#define __my_cpu_offset	__ia64_per_cpu_var(local_per_cpu_offset)

extern void *per_cpu_init(void);

#else 

#define per_cpu_init()				(__phys_per_cpu_start)

#endif	

#define PER_CPU_BASE_SECTION ".data..percpu"

#define __ia64_per_cpu_var(var) (*({					\
	__verify_pcpu_ptr(&(var));					\
	((typeof(var) __kernel __force *)&(var));			\
}))

#include <asm-generic/percpu.h>

DECLARE_PER_CPU(unsigned long, local_per_cpu_offset);

#endif 

#endif 
