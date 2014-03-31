#ifndef _ASM_POWERPC_PERCPU_H_
#define _ASM_POWERPC_PERCPU_H_
#ifdef __powerpc64__


#ifdef CONFIG_SMP

#include <asm/paca.h>

#define __my_cpu_offset local_paca->data_offset

#endif 
#endif 

#include <asm-generic/percpu.h>

#endif 
