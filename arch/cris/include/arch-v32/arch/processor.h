#ifndef _ASM_CRIS_ARCH_PROCESSOR_H
#define _ASM_CRIS_ARCH_PROCESSOR_H


#define current_text_addr() \
	({void *pc; __asm__ __volatile__ ("lapcq .,%0" : "=rm" (pc)); pc;})

struct thread_struct {
	unsigned long ksp;	
	unsigned long usp;	
	unsigned long ccs;	
};

#ifndef CONFIG_ETRAX_VCS_SIM
#define TASK_SIZE	(0xB0000000UL)
#else
#define TASK_SIZE	(0xA0000000UL)
#endif

#define INIT_THREAD { 0, 0, (1 << I_CCS_BITNR) }

#define KSTK_EIP(tsk)		\
({				\
	unsigned long eip = 0;	\
	unsigned long regs = (unsigned long)task_pt_regs(tsk); \
	if (regs > PAGE_SIZE && virt_addr_valid(regs))	    \
		eip = ((struct pt_regs *)regs)->erp;	    \
	eip; \
})

#define start_thread(regs, ip, usp) \
do { \
	regs->erp = ip; \
	regs->ccs |= 1 << (U_CCS_BITNR + CCS_SHIFT); \
	wrusp(usp); \
} while(0)

#define arch_fixup(regs) {};

#endif 
