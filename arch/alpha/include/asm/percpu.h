#ifndef __ALPHA_PERCPU_H
#define __ALPHA_PERCPU_H

#if defined(MODULE) && defined(CONFIG_SMP)
#define ARCH_NEEDS_WEAK_PER_CPU
#endif

#include <asm-generic/percpu.h>

#endif 
