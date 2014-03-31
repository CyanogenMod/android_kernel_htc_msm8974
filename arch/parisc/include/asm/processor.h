/*
 * include/asm-parisc/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 * Copyright (C) 2001 Grant Grundler
 */

#ifndef __ASM_PARISC_PROCESSOR_H
#define __ASM_PARISC_PROCESSOR_H

#ifndef __ASSEMBLY__
#include <linux/threads.h>

#include <asm/prefetch.h>
#include <asm/hardware.h>
#include <asm/pdc.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/percpu.h>

#endif 

#define KERNEL_STACK_SIZE 	(4*PAGE_SIZE)

#ifdef CONFIG_PA20
#define current_ia(x)	__asm__("mfia %0" : "=r"(x))
#else 
#define current_ia(x)	__asm__("blr 0,%0\n\tnop" : "=r"(x))
#endif
#define current_text_addr() ({ void *pc; current_ia(pc); pc; })

#define TASK_SIZE_OF(tsk)       ((tsk)->thread.task_size)
#define TASK_SIZE	        TASK_SIZE_OF(current)
#define TASK_UNMAPPED_BASE      (current->thread.map_base)

#define DEFAULT_TASK_SIZE32	(0xFFF00000UL)
#define DEFAULT_MAP_BASE32	(0x40000000UL)

#ifdef CONFIG_64BIT
#define DEFAULT_TASK_SIZE       (MAX_ADDRESS-0xf000000)
#define DEFAULT_MAP_BASE        (0x200000000UL)
#else
#define DEFAULT_TASK_SIZE	DEFAULT_TASK_SIZE32
#define DEFAULT_MAP_BASE	DEFAULT_MAP_BASE32
#endif

#ifdef __KERNEL__


#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	DEFAULT_TASK_SIZE

#endif

#ifndef __ASSEMBLY__

struct system_cpuinfo_parisc {
	unsigned int	cpu_count;
	unsigned int	cpu_hz;
	unsigned int	hversion;
	unsigned int	sversion;
	enum cpu_type	cpu_type;

	struct {
		struct pdc_model model;
		unsigned long versions;
		unsigned long cpuid;
		unsigned long capabilities;
		char   sys_model_name[81]; 
	} pdc;

	const char	*cpu_name;	
	const char	*family_name;	
};


struct cpuinfo_parisc {
	unsigned long it_value;     
	unsigned long it_delta;     
	unsigned long irq_count;    
	unsigned long irq_max_cr16; 
	unsigned long cpuid;        
	unsigned long hpa;          
	unsigned long txn_addr;     
#ifdef CONFIG_SMP
	unsigned long pending_ipi;  
	unsigned long ipi_count;    
#endif
	unsigned long bh_count;     
	unsigned long prof_counter; 
	unsigned long prof_multiplier;	
	unsigned long fp_rev;
	unsigned long fp_model;
	unsigned int state;
	struct parisc_device *dev;
	unsigned long loops_per_jiffy;
};

extern struct system_cpuinfo_parisc boot_cpu_data;
DECLARE_PER_CPU(struct cpuinfo_parisc, cpu_data);

#define CPU_HVERSION ((boot_cpu_data.hversion >> 4) & 0x0FFF)

typedef struct {
	int seg;  
} mm_segment_t;

#define ARCH_MIN_TASKALIGN	8

struct thread_struct {
	struct pt_regs regs;
	unsigned long  task_size;
	unsigned long  map_base;
	unsigned long  flags;
}; 

#define task_pt_regs(tsk) ((struct pt_regs *)&((tsk)->thread.regs))

#define PARISC_UAC_NOPRINT	(1UL << 0)	
#define PARISC_UAC_SIGBUS	(1UL << 1)
#define PARISC_KERNEL_DEATH	(1UL << 31)	

#define PARISC_UAC_SHIFT	0
#define PARISC_UAC_MASK		(PARISC_UAC_NOPRINT|PARISC_UAC_SIGBUS)

#define SET_UNALIGN_CTL(task,value)                                       \
        ({                                                                \
        (task)->thread.flags = (((task)->thread.flags & ~PARISC_UAC_MASK) \
                                | (((value) << PARISC_UAC_SHIFT) &        \
                                   PARISC_UAC_MASK));                     \
        0;                                                                \
        })

#define GET_UNALIGN_CTL(task,addr)                                        \
        ({                                                                \
        put_user(((task)->thread.flags & PARISC_UAC_MASK)                 \
                 >> PARISC_UAC_SHIFT, (int __user *) (addr));             \
        })

#define INIT_THREAD { \
	.regs = {	.gr	= { 0, }, \
			.fr	= { 0, }, \
			.sr	= { 0, }, \
			.iasq	= { 0, }, \
			.iaoq	= { 0, }, \
			.cr27	= 0, \
		}, \
	.task_size	= DEFAULT_TASK_SIZE, \
	.map_base	= DEFAULT_MAP_BASE, \
	.flags		= 0 \
	}


struct task_struct;
unsigned long thread_saved_pc(struct task_struct *t);
void show_trace(struct task_struct *task, unsigned long *stack);

typedef unsigned int elf_caddr_t;

#define start_thread_som(regs, new_pc, new_sp) do {	\
	unsigned long *sp = (unsigned long *)new_sp;	\
	__u32 spaceid = (__u32)current->mm->context;	\
	unsigned long pc = (unsigned long)new_pc;	\
				\
	pc |= 3;					\
							\
	regs->iasq[0] = spaceid;			\
	regs->iasq[1] = spaceid;			\
	regs->iaoq[0] = pc;				\
	regs->iaoq[1] = pc + 4;                         \
	regs->sr[2] = LINUX_GATEWAY_SPACE;              \
	regs->sr[3] = 0xffff;				\
	regs->sr[4] = spaceid;				\
	regs->sr[5] = spaceid;				\
	regs->sr[6] = spaceid;				\
	regs->sr[7] = spaceid;				\
	regs->gr[ 0] = USER_PSW;                        \
	regs->gr[30] = ((new_sp)+63)&~63;		\
	regs->gr[31] = pc;				\
							\
	get_user(regs->gr[26],&sp[0]);			\
	get_user(regs->gr[25],&sp[-1]); 		\
	get_user(regs->gr[24],&sp[-2]); 		\
	get_user(regs->gr[23],&sp[-3]); 		\
} while(0)


#ifdef CONFIG_64BIT
#define USER_WIDE_MODE	(!test_thread_flag(TIF_32BIT))
#else
#define USER_WIDE_MODE	0
#endif

#define start_thread(regs, new_pc, new_sp) do {		\
	elf_addr_t *sp = (elf_addr_t *)new_sp;		\
	__u32 spaceid = (__u32)current->mm->context;	\
	elf_addr_t pc = (elf_addr_t)new_pc | 3;		\
	elf_caddr_t *argv = (elf_caddr_t *)bprm->exec + 1;	\
							\
	regs->iasq[0] = spaceid;			\
	regs->iasq[1] = spaceid;			\
	regs->iaoq[0] = pc;				\
	regs->iaoq[1] = pc + 4;                         \
	regs->sr[2] = LINUX_GATEWAY_SPACE;              \
	regs->sr[3] = 0xffff;				\
	regs->sr[4] = spaceid;				\
	regs->sr[5] = spaceid;				\
	regs->sr[6] = spaceid;				\
	regs->sr[7] = spaceid;				\
	regs->gr[ 0] = USER_PSW | (USER_WIDE_MODE ? PSW_W : 0); \
	regs->fr[ 0] = 0LL;                            	\
	regs->fr[ 1] = 0LL;                            	\
	regs->fr[ 2] = 0LL;                            	\
	regs->fr[ 3] = 0LL;                            	\
	regs->gr[30] = (((unsigned long)sp + 63) &~ 63) | (USER_WIDE_MODE ? 1 : 0); \
	regs->gr[31] = pc;				\
							\
	get_user(regs->gr[25], (argv - 1));		\
	regs->gr[24] = (long) argv;			\
	regs->gr[23] = 0;				\
} while(0)

struct task_struct;
struct mm_struct;

extern void release_thread(struct task_struct *);
extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

#define prepare_to_copy(tsk)	do { } while (0)

extern void map_hpux_gateway_page(struct task_struct *tsk, struct mm_struct *mm);

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)	((tsk)->thread.regs.iaoq[0])
#define KSTK_ESP(tsk)	((tsk)->thread.regs.gr[30])

#define cpu_relax()	barrier()

static inline int parisc_requires_coherency(void)
{
#ifdef CONFIG_PA8X00
	return (boot_cpu_data.cpu_type == mako) ||
		(boot_cpu_data.cpu_type == mako2);
#else
	return 0;
#endif
}

#endif 

#endif 
