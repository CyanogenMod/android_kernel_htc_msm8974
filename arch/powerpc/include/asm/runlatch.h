/*
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 */
#ifndef _ASM_POWERPC_RUNLATCH_H
#define _ASM_POWERPC_RUNLATCH_H

#ifdef CONFIG_PPC64

extern void __ppc64_runlatch_on(void);
extern void __ppc64_runlatch_off(void);

#define ppc64_runlatch_off()					\
	do {							\
		if (cpu_has_feature(CPU_FTR_CTRL) &&		\
		    test_thread_local_flags(_TLF_RUNLATCH)) {	\
			unsigned long msr = mfmsr();		\
			__hard_irq_disable();			\
			__ppc64_runlatch_off();			\
			if (msr & MSR_EE)			\
				__hard_irq_enable();		\
		}      						\
	} while (0)

#define ppc64_runlatch_on()					\
	do {							\
		if (cpu_has_feature(CPU_FTR_CTRL) &&		\
		    !test_thread_local_flags(_TLF_RUNLATCH)) {	\
			unsigned long msr = mfmsr();		\
			__hard_irq_disable();			\
			__ppc64_runlatch_on();			\
			if (msr & MSR_EE)			\
				__hard_irq_enable();		\
		}      						\
	} while (0)
#else
#define ppc64_runlatch_on()
#define ppc64_runlatch_off()
#endif 

#endif 
