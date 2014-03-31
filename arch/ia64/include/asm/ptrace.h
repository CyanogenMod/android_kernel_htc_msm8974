#ifndef _ASM_IA64_PTRACE_H
#define _ASM_IA64_PTRACE_H

/*
 * Copyright (C) 1998-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 * Copyright (C) 2003 Intel Co
 *	Suresh Siddha <suresh.b.siddha@intel.com>
 *	Fenghua Yu <fenghua.yu@intel.com>
 *	Arun Sharma <arun.sharma@intel.com>
 *
 * 12/07/98	S. Eranian	added pt_regs & switch_stack
 * 12/21/98	D. Mosberger	updated to match latest code
 *  6/17/99	D. Mosberger	added second unat member to "struct switch_stack"
 *
 */


#include <asm/fpu.h>

#ifdef __KERNEL__
#ifndef ASM_OFFSETS_C
#include <asm/asm-offsets.h>
#endif

#if defined(CONFIG_IA64_PAGE_SIZE_4KB)
# define KERNEL_STACK_SIZE_ORDER		3
#elif defined(CONFIG_IA64_PAGE_SIZE_8KB)
# define KERNEL_STACK_SIZE_ORDER		2
#elif defined(CONFIG_IA64_PAGE_SIZE_16KB)
# define KERNEL_STACK_SIZE_ORDER		1
#else
# define KERNEL_STACK_SIZE_ORDER		0
#endif

#define IA64_RBS_OFFSET			((IA64_TASK_SIZE + IA64_THREAD_INFO_SIZE + 31) & ~31)
#define IA64_STK_OFFSET			((1 << KERNEL_STACK_SIZE_ORDER)*PAGE_SIZE)

#define KERNEL_STACK_SIZE		IA64_STK_OFFSET

#endif 

#ifndef __ASSEMBLY__

struct pt_regs {
	
	unsigned long b6;		
	unsigned long b7;		

	unsigned long ar_csd;           
	unsigned long ar_ssd;           

	unsigned long r8;		
	unsigned long r9;		
	unsigned long r10;		
	unsigned long r11;		

	unsigned long cr_ipsr;		
	unsigned long cr_iip;		
	unsigned long cr_ifs;

	unsigned long ar_unat;		
	unsigned long ar_pfs;		
	unsigned long ar_rsc;		
	
	unsigned long ar_rnat;		
	unsigned long ar_bspstore;	

	unsigned long pr;		
	unsigned long b0;		
	unsigned long loadrs;		

	unsigned long r1;		
	unsigned long r12;		
	unsigned long r13;		

	unsigned long ar_fpsr;		
	unsigned long r15;		

	

	unsigned long r14;		
	unsigned long r2;		
	unsigned long r3;		

	
	unsigned long r16;		
	unsigned long r17;		
	unsigned long r18;		
	unsigned long r19;		
	unsigned long r20;		
	unsigned long r21;		
	unsigned long r22;		
	unsigned long r23;		
	unsigned long r24;		
	unsigned long r25;		
	unsigned long r26;		
	unsigned long r27;		
	unsigned long r28;		
	unsigned long r29;		
	unsigned long r30;		
	unsigned long r31;		

	unsigned long ar_ccv;		

	struct ia64_fpreg f6;		
	struct ia64_fpreg f7;		
	struct ia64_fpreg f8;		
	struct ia64_fpreg f9;		
	struct ia64_fpreg f10;		
	struct ia64_fpreg f11;		
};

struct switch_stack {
	unsigned long caller_unat;	
	unsigned long ar_fpsr;		

	struct ia64_fpreg f2;		
	struct ia64_fpreg f3;		
	struct ia64_fpreg f4;		
	struct ia64_fpreg f5;		

	struct ia64_fpreg f12;		
	struct ia64_fpreg f13;		
	struct ia64_fpreg f14;		
	struct ia64_fpreg f15;		
	struct ia64_fpreg f16;		
	struct ia64_fpreg f17;		
	struct ia64_fpreg f18;		
	struct ia64_fpreg f19;		
	struct ia64_fpreg f20;		
	struct ia64_fpreg f21;		
	struct ia64_fpreg f22;		
	struct ia64_fpreg f23;		
	struct ia64_fpreg f24;		
	struct ia64_fpreg f25;		
	struct ia64_fpreg f26;		
	struct ia64_fpreg f27;		
	struct ia64_fpreg f28;		
	struct ia64_fpreg f29;		
	struct ia64_fpreg f30;		
	struct ia64_fpreg f31;		

	unsigned long r4;		
	unsigned long r5;		
	unsigned long r6;		
	unsigned long r7;		

	unsigned long b0;		
	unsigned long b1;
	unsigned long b2;
	unsigned long b3;
	unsigned long b4;
	unsigned long b5;

	unsigned long ar_pfs;		
	unsigned long ar_lc;		
	unsigned long ar_unat;		
	unsigned long ar_rnat;		
	unsigned long ar_bspstore;	
	unsigned long pr;		
};

#ifdef __KERNEL__

#include <asm/current.h>
#include <asm/page.h>

# define instruction_pointer(regs) ((regs)->cr_iip + ia64_psr(regs)->ri)

static inline unsigned long user_stack_pointer(struct pt_regs *regs)
{
	
	return regs->ar_bspstore;
}

static inline int is_syscall_success(struct pt_regs *regs)
{
	return regs->r10 != -1;
}

static inline long regs_return_value(struct pt_regs *regs)
{
	if (is_syscall_success(regs))
		return regs->r8;
	else
		return -regs->r8;
}

#define profile_pc(regs)						\
({									\
	unsigned long __ip = instruction_pointer(regs);			\
	(__ip & ~3UL) + ((__ip & 3UL) << 2);				\
})

  
# define task_pt_regs(t)		(((struct pt_regs *) ((char *) (t) + IA64_STK_OFFSET)) - 1)
# define ia64_psr(regs)			((struct ia64_psr *) &(regs)->cr_ipsr)
# define user_mode(regs)		(((struct ia64_psr *) &(regs)->cr_ipsr)->cpl != 0)
# define user_stack(task,regs)	((long) regs - (long) task == IA64_STK_OFFSET - sizeof(*regs))
# define fsys_mode(task,regs)					\
  ({								\
	  struct task_struct *_task = (task);			\
	  struct pt_regs *_regs = (regs);			\
	  !user_mode(_regs) && user_stack(_task, _regs);	\
  })

# define force_successful_syscall_return()	(task_pt_regs(current)->r8 = 0)

  struct task_struct;			
  struct unw_frame_info;		

  extern void ia64_do_show_stack (struct unw_frame_info *, void *);
  extern unsigned long ia64_get_user_rbs_end (struct task_struct *, struct pt_regs *,
					      unsigned long *);
  extern long ia64_peek (struct task_struct *, struct switch_stack *, unsigned long,
			 unsigned long, long *);
  extern long ia64_poke (struct task_struct *, struct switch_stack *, unsigned long,
			 unsigned long, long);
  extern void ia64_flush_fph (struct task_struct *);
  extern void ia64_sync_fph (struct task_struct *);
  extern void ia64_sync_krbs(void);
  extern long ia64_sync_user_rbs (struct task_struct *, struct switch_stack *,
				  unsigned long, unsigned long);

  
  extern unsigned long ia64_get_scratch_nat_bits (struct pt_regs *pt, unsigned long scratch_unat);
  
  extern unsigned long ia64_put_scratch_nat_bits (struct pt_regs *pt, unsigned long nat);

  extern void ia64_increment_ip (struct pt_regs *pt);
  extern void ia64_decrement_ip (struct pt_regs *pt);

  extern void ia64_ptrace_stop(void);
  #define arch_ptrace_stop(code, info) \
	ia64_ptrace_stop()
  #define arch_ptrace_stop_needed(code, info) \
	(!test_thread_flag(TIF_RESTORE_RSE))

  extern void ptrace_attach_sync_user_rbs (struct task_struct *);
  #define arch_ptrace_attach(child) \
	ptrace_attach_sync_user_rbs(child)

  #define arch_has_single_step()  (1)
  #define arch_has_block_step()   (1)

#endif 

struct pt_all_user_regs {
	unsigned long nat;
	unsigned long cr_iip;
	unsigned long cfm;
	unsigned long cr_ipsr;
	unsigned long pr;

	unsigned long gr[32];
	unsigned long br[8];
	unsigned long ar[128];
	struct ia64_fpreg fr[128];
};

#endif 

#define PT_AUR_RSC	16
#define PT_AUR_BSP	17
#define PT_AUR_BSPSTORE	18
#define PT_AUR_RNAT	19
#define PT_AUR_CCV	32
#define PT_AUR_UNAT	36
#define PT_AUR_FPSR	40
#define PT_AUR_PFS	64
#define PT_AUR_LC	65
#define PT_AUR_EC	66

#define PTRACE_SINGLEBLOCK	12	
#define PTRACE_OLD_GETSIGINFO	13	
#define PTRACE_OLD_SETSIGINFO	14	
#define PTRACE_GETREGS		18	
#define PTRACE_SETREGS		19	

#define PTRACE_OLDSETOPTIONS	21

#endif 
