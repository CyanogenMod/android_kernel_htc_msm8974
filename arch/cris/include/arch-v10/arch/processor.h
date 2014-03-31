#ifndef __ASM_CRIS_ARCH_PROCESSOR_H
#define __ASM_CRIS_ARCH_PROCESSOR_H

#define current_text_addr() ({void *pc; __asm__ ("move.d $pc,%0" : "=rm" (pc)); pc; })

#define wp_works_ok 1


struct thread_struct {
	unsigned long ksp;     
	unsigned long usp;     
	unsigned long dccr;    
};


#ifdef CONFIG_CRIS_LOW_MAP
#define TASK_SIZE       (0x50000000UL)   
#else
#define TASK_SIZE       (0xA0000000UL)   
#endif

#define INIT_THREAD  { \
   0, 0, 0x20 }  

#define KSTK_EIP(tsk)	\
({			\
	unsigned long eip = 0;   \
	unsigned long regs = (unsigned long)task_pt_regs(tsk); \
	if (regs > PAGE_SIZE && \
		virt_addr_valid(regs)) \
	eip = ((struct pt_regs *)regs)->irp; \
	eip; \
})


#define start_thread(regs, ip, usp) do { \
	regs->irp = ip;       \
	regs->dccr |= 1 << U_DCCR_BITNR; \
	wrusp(usp);           \
} while(0)

#define arch_fixup(regs) \
   regs->frametype = CRIS_FRAME_NORMAL;

#endif
