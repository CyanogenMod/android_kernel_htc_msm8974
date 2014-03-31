#ifndef _ASM_IA64_PROCESSOR_H
#define _ASM_IA64_PROCESSOR_H

/*
 * Copyright (C) 1998-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 1999 Don Dugger <don.dugger@intel.com>
 *
 * 11/24/98	S.Eranian	added ia64_set_iva()
 * 12/03/99	D. Mosberger	implement thread_saved_pc() via kernel unwind API
 * 06/16/00	A. Mallick	added csd/ssd/tssd for ia32 support
 */


#include <asm/intrinsics.h>
#include <asm/kregs.h>
#include <asm/ptrace.h>
#include <asm/ustack.h>

#define __ARCH_WANT_UNLOCKED_CTXSW
#define ARCH_HAS_PREFETCH_SWITCH_STACK

#define IA64_NUM_PHYS_STACK_REG	96
#define IA64_NUM_DBG_REGS	8

#define DEFAULT_MAP_BASE	__IA64_UL_CONST(0x2000000000000000)
#define DEFAULT_TASK_SIZE	__IA64_UL_CONST(0xa000000000000000)

#define TASK_SIZE_OF(tsk)	((tsk)->thread.task_size)
#define TASK_SIZE       	TASK_SIZE_OF(current)

#define TASK_UNMAPPED_BASE	(current->thread.map_base)

#define IA64_THREAD_FPH_VALID	(__IA64_UL(1) << 0)	
#define IA64_THREAD_DBG_VALID	(__IA64_UL(1) << 1)	
#define IA64_THREAD_PM_VALID	(__IA64_UL(1) << 2)	
#define IA64_THREAD_UAC_NOPRINT	(__IA64_UL(1) << 3)	
#define IA64_THREAD_UAC_SIGBUS	(__IA64_UL(1) << 4)	
#define IA64_THREAD_MIGRATION	(__IA64_UL(1) << 5)	
#define IA64_THREAD_FPEMU_NOPRINT (__IA64_UL(1) << 6)	
#define IA64_THREAD_FPEMU_SIGFPE  (__IA64_UL(1) << 7)	

#define IA64_THREAD_UAC_SHIFT	3
#define IA64_THREAD_UAC_MASK	(IA64_THREAD_UAC_NOPRINT | IA64_THREAD_UAC_SIGBUS)
#define IA64_THREAD_FPEMU_SHIFT	6
#define IA64_THREAD_FPEMU_MASK	(IA64_THREAD_FPEMU_NOPRINT | IA64_THREAD_FPEMU_SIGFPE)


#define IA64_NSEC_PER_CYC_SHIFT	30

#ifndef __ASSEMBLY__

#include <linux/cache.h>
#include <linux/compiler.h>
#include <linux/threads.h>
#include <linux/types.h>

#include <asm/fpu.h>
#include <asm/page.h>
#include <asm/percpu.h>
#include <asm/rse.h>
#include <asm/unwind.h>
#include <linux/atomic.h>
#ifdef CONFIG_NUMA
#include <asm/nodedata.h>
#endif

struct ia64_psr {
	__u64 reserved0 : 1;
	__u64 be : 1;
	__u64 up : 1;
	__u64 ac : 1;
	__u64 mfl : 1;
	__u64 mfh : 1;
	__u64 reserved1 : 7;
	__u64 ic : 1;
	__u64 i : 1;
	__u64 pk : 1;
	__u64 reserved2 : 1;
	__u64 dt : 1;
	__u64 dfl : 1;
	__u64 dfh : 1;
	__u64 sp : 1;
	__u64 pp : 1;
	__u64 di : 1;
	__u64 si : 1;
	__u64 db : 1;
	__u64 lp : 1;
	__u64 tb : 1;
	__u64 rt : 1;
	__u64 reserved3 : 4;
	__u64 cpl : 2;
	__u64 is : 1;
	__u64 mc : 1;
	__u64 it : 1;
	__u64 id : 1;
	__u64 da : 1;
	__u64 dd : 1;
	__u64 ss : 1;
	__u64 ri : 2;
	__u64 ed : 1;
	__u64 bn : 1;
	__u64 reserved4 : 19;
};

union ia64_isr {
	__u64  val;
	struct {
		__u64 code : 16;
		__u64 vector : 8;
		__u64 reserved1 : 8;
		__u64 x : 1;
		__u64 w : 1;
		__u64 r : 1;
		__u64 na : 1;
		__u64 sp : 1;
		__u64 rs : 1;
		__u64 ir : 1;
		__u64 ni : 1;
		__u64 so : 1;
		__u64 ei : 2;
		__u64 ed : 1;
		__u64 reserved2 : 20;
	};
};

union ia64_lid {
	__u64 val;
	struct {
		__u64  rv  : 16;
		__u64  eid : 8;
		__u64  id  : 8;
		__u64  ig  : 32;
	};
};

union ia64_tpr {
	__u64 val;
	struct {
		__u64 ig0 : 4;
		__u64 mic : 4;
		__u64 rsv : 8;
		__u64 mmi : 1;
		__u64 ig1 : 47;
	};
};

union ia64_itir {
	__u64 val;
	struct {
		__u64 rv3  :  2; 
		__u64 ps   :  6; 
		__u64 key  : 24; 
		__u64 rv4  : 32; 
	};
};

union  ia64_rr {
	__u64 val;
	struct {
		__u64  ve	:  1;  
		__u64  reserved0:  1;  
		__u64  ps	:  6;  
		__u64  rid	: 24;  
		__u64  reserved1: 32;  
	};
};

struct cpuinfo_ia64 {
	unsigned int softirq_pending;
	unsigned long itm_delta;	
	unsigned long itm_next;		
	unsigned long nsec_per_cyc;	
	unsigned long unimpl_va_mask;	
	unsigned long unimpl_pa_mask;	
	unsigned long itc_freq;		
	unsigned long proc_freq;	
	unsigned long cyc_per_usec;	
	unsigned long ptce_base;
	unsigned int ptce_count[2];
	unsigned int ptce_stride[2];
	struct task_struct *ksoftirqd;	

#ifdef CONFIG_SMP
	unsigned long loops_per_jiffy;
	int cpu;
	unsigned int socket_id;	
	unsigned short core_id;	
	unsigned short thread_id; 
	unsigned short num_log;	
	unsigned char cores_per_socket;	
	unsigned char threads_per_core;	
#endif

	
	unsigned long ppn;
	unsigned long features;
	unsigned char number;
	unsigned char revision;
	unsigned char model;
	unsigned char family;
	unsigned char archrev;
	char vendor[16];
	char *model_name;

#ifdef CONFIG_NUMA
	struct ia64_node_data *node_data;
#endif
};

DECLARE_PER_CPU(struct cpuinfo_ia64, ia64_cpu_info);

#define local_cpu_data		(&__ia64_per_cpu_var(ia64_cpu_info))
#define cpu_data(cpu)		(&per_cpu(ia64_cpu_info, cpu))

extern void print_cpu_info (struct cpuinfo_ia64 *);

typedef struct {
	unsigned long seg;
} mm_segment_t;

#define SET_UNALIGN_CTL(task,value)								\
({												\
	(task)->thread.flags = (((task)->thread.flags & ~IA64_THREAD_UAC_MASK)			\
				| (((value) << IA64_THREAD_UAC_SHIFT) & IA64_THREAD_UAC_MASK));	\
	0;											\
})
#define GET_UNALIGN_CTL(task,addr)								\
({												\
	put_user(((task)->thread.flags & IA64_THREAD_UAC_MASK) >> IA64_THREAD_UAC_SHIFT,	\
		 (int __user *) (addr));							\
})

#define SET_FPEMU_CTL(task,value)								\
({												\
	(task)->thread.flags = (((task)->thread.flags & ~IA64_THREAD_FPEMU_MASK)		\
			  | (((value) << IA64_THREAD_FPEMU_SHIFT) & IA64_THREAD_FPEMU_MASK));	\
	0;											\
})
#define GET_FPEMU_CTL(task,addr)								\
({												\
	put_user(((task)->thread.flags & IA64_THREAD_FPEMU_MASK) >> IA64_THREAD_FPEMU_SHIFT,	\
		 (int __user *) (addr));							\
})

struct thread_struct {
	__u32 flags;			
	
	__u8 on_ustack;			
	__u8 pad[3];
	__u64 ksp;			
	__u64 map_base;			
	__u64 task_size;		
	__u64 rbs_bot;			
	int last_fph_cpu;		

#ifdef CONFIG_PERFMON
	void *pfm_context;		     
	unsigned long pfm_needs_checking;    
# define INIT_THREAD_PM		.pfm_context =		NULL,     \
				.pfm_needs_checking =	0UL,
#else
# define INIT_THREAD_PM
#endif
	unsigned long dbr[IA64_NUM_DBG_REGS];
	unsigned long ibr[IA64_NUM_DBG_REGS];
	struct ia64_fpreg fph[96];	
};

#define INIT_THREAD {						\
	.flags =	0,					\
	.on_ustack =	0,					\
	.ksp =		0,					\
	.map_base =	DEFAULT_MAP_BASE,			\
	.rbs_bot =	STACK_TOP - DEFAULT_USER_STACK_SIZE,	\
	.task_size =	DEFAULT_TASK_SIZE,			\
	.last_fph_cpu =  -1,					\
	INIT_THREAD_PM						\
	.dbr =		{0, },					\
	.ibr =		{0, },					\
	.fph =		{{{{0}}}, }				\
}

#define start_thread(regs,new_ip,new_sp) do {							\
	regs->cr_ipsr = ((regs->cr_ipsr | (IA64_PSR_BITS_TO_SET | IA64_PSR_CPL))		\
			 & ~(IA64_PSR_BITS_TO_CLEAR | IA64_PSR_RI | IA64_PSR_IS));		\
	regs->cr_iip = new_ip;									\
	regs->ar_rsc = 0xf;					\
	regs->ar_rnat = 0;									\
	regs->ar_bspstore = current->thread.rbs_bot;						\
	regs->ar_fpsr = FPSR_DEFAULT;								\
	regs->loadrs = 0;									\
	regs->r8 = get_dumpable(current->mm);			\
	regs->r12 = new_sp - 16;				\
	if (unlikely(!get_dumpable(current->mm))) {							\
										\
		regs->ar_pfs = 0; regs->b0 = 0; regs->pr = 0;					\
		regs->r1 = 0; regs->r9  = 0; regs->r11 = 0; regs->r13 = 0; regs->r15 = 0;	\
	}											\
} while (0)

struct mm_struct;
struct task_struct;

#define release_thread(dead_task)

#define prepare_to_copy(tsk)	do { } while (0)

extern pid_t kernel_thread (int (*fn)(void *), void *arg, unsigned long flags);

extern unsigned long get_wchan (struct task_struct *p);

#define KSTK_EIP(tsk)					\
  ({							\
	struct pt_regs *_regs = task_pt_regs(tsk);	\
	_regs->cr_iip + ia64_psr(_regs)->ri;		\
  })

#define KSTK_ESP(tsk)  ((tsk)->thread.ksp)

extern void ia64_getreg_unknown_kr (void);
extern void ia64_setreg_unknown_kr (void);

#define ia64_get_kr(regnum)					\
({								\
	unsigned long r = 0;					\
								\
	switch (regnum) {					\
	    case 0: r = ia64_getreg(_IA64_REG_AR_KR0); break;	\
	    case 1: r = ia64_getreg(_IA64_REG_AR_KR1); break;	\
	    case 2: r = ia64_getreg(_IA64_REG_AR_KR2); break;	\
	    case 3: r = ia64_getreg(_IA64_REG_AR_KR3); break;	\
	    case 4: r = ia64_getreg(_IA64_REG_AR_KR4); break;	\
	    case 5: r = ia64_getreg(_IA64_REG_AR_KR5); break;	\
	    case 6: r = ia64_getreg(_IA64_REG_AR_KR6); break;	\
	    case 7: r = ia64_getreg(_IA64_REG_AR_KR7); break;	\
	    default: ia64_getreg_unknown_kr(); break;		\
	}							\
	r;							\
})

#define ia64_set_kr(regnum, r) 					\
({								\
	switch (regnum) {					\
	    case 0: ia64_setreg(_IA64_REG_AR_KR0, r); break;	\
	    case 1: ia64_setreg(_IA64_REG_AR_KR1, r); break;	\
	    case 2: ia64_setreg(_IA64_REG_AR_KR2, r); break;	\
	    case 3: ia64_setreg(_IA64_REG_AR_KR3, r); break;	\
	    case 4: ia64_setreg(_IA64_REG_AR_KR4, r); break;	\
	    case 5: ia64_setreg(_IA64_REG_AR_KR5, r); break;	\
	    case 6: ia64_setreg(_IA64_REG_AR_KR6, r); break;	\
	    case 7: ia64_setreg(_IA64_REG_AR_KR7, r); break;	\
	    default: ia64_setreg_unknown_kr(); break;		\
	}							\
})


#define ia64_is_local_fpu_owner(t)								\
({												\
	struct task_struct *__ia64_islfo_task = (t);						\
	(__ia64_islfo_task->thread.last_fph_cpu == smp_processor_id()				\
	 && __ia64_islfo_task == (struct task_struct *) ia64_get_kr(IA64_KR_FPU_OWNER));	\
})

#define ia64_set_local_fpu_owner(t) do {						\
	struct task_struct *__ia64_slfo_task = (t);					\
	__ia64_slfo_task->thread.last_fph_cpu = smp_processor_id();			\
	ia64_set_kr(IA64_KR_FPU_OWNER, (unsigned long) __ia64_slfo_task);		\
} while (0)

#define ia64_drop_fpu(t)	((t)->thread.last_fph_cpu = -1)

extern void __ia64_init_fpu (void);
extern void __ia64_save_fpu (struct ia64_fpreg *fph);
extern void __ia64_load_fpu (struct ia64_fpreg *fph);
extern void ia64_save_debug_regs (unsigned long *save_area);
extern void ia64_load_debug_regs (unsigned long *save_area);

#define ia64_fph_enable()	do { ia64_rsm(IA64_PSR_DFH); ia64_srlz_d(); } while (0)
#define ia64_fph_disable()	do { ia64_ssm(IA64_PSR_DFH); ia64_srlz_d(); } while (0)

static inline void
ia64_init_fpu (void) {
	ia64_fph_enable();
	__ia64_init_fpu();
	ia64_fph_disable();
}

static inline void
ia64_save_fpu (struct ia64_fpreg *fph) {
	ia64_fph_enable();
	__ia64_save_fpu(fph);
	ia64_fph_disable();
}

static inline void
ia64_load_fpu (struct ia64_fpreg *fph) {
	ia64_fph_enable();
	__ia64_load_fpu(fph);
	ia64_fph_disable();
}

static inline __u64
ia64_clear_ic (void)
{
	__u64 psr;
	psr = ia64_getreg(_IA64_REG_PSR);
	ia64_stop();
	ia64_rsm(IA64_PSR_I | IA64_PSR_IC);
	ia64_srlz_i();
	return psr;
}

static inline void
ia64_set_psr (__u64 psr)
{
	ia64_stop();
	ia64_setreg(_IA64_REG_PSR_L, psr);
	ia64_srlz_i();
}

static inline void
ia64_itr (__u64 target_mask, __u64 tr_num,
	  __u64 vmaddr, __u64 pte,
	  __u64 log_page_size)
{
	ia64_setreg(_IA64_REG_CR_ITIR, (log_page_size << 2));
	ia64_setreg(_IA64_REG_CR_IFA, vmaddr);
	ia64_stop();
	if (target_mask & 0x1)
		ia64_itri(tr_num, pte);
	if (target_mask & 0x2)
		ia64_itrd(tr_num, pte);
}

static inline void
ia64_itc (__u64 target_mask, __u64 vmaddr, __u64 pte,
	  __u64 log_page_size)
{
	ia64_setreg(_IA64_REG_CR_ITIR, (log_page_size << 2));
	ia64_setreg(_IA64_REG_CR_IFA, vmaddr);
	ia64_stop();
	
	if (target_mask & 0x1)
		ia64_itci(pte);
	if (target_mask & 0x2)
		ia64_itcd(pte);
}

static inline void
ia64_ptr (__u64 target_mask, __u64 vmaddr, __u64 log_size)
{
	if (target_mask & 0x1)
		ia64_ptri(vmaddr, (log_size << 2));
	if (target_mask & 0x2)
		ia64_ptrd(vmaddr, (log_size << 2));
}

static inline void
ia64_set_iva (void *ivt_addr)
{
	ia64_setreg(_IA64_REG_CR_IVA, (__u64) ivt_addr);
	ia64_srlz_i();
}

static inline void
ia64_set_pta (__u64 pta)
{
	
	ia64_setreg(_IA64_REG_CR_PTA, pta);
	ia64_srlz_i();
}

static inline void
ia64_eoi (void)
{
	ia64_setreg(_IA64_REG_CR_EOI, 0);
	ia64_srlz_d();
}

#define cpu_relax()	ia64_hint(ia64_hint_pause)

static inline int
ia64_get_irr(unsigned int vector)
{
	unsigned int reg = vector / 64;
	unsigned int bit = vector % 64;
	u64 irr;

	switch (reg) {
	case 0: irr = ia64_getreg(_IA64_REG_CR_IRR0); break;
	case 1: irr = ia64_getreg(_IA64_REG_CR_IRR1); break;
	case 2: irr = ia64_getreg(_IA64_REG_CR_IRR2); break;
	case 3: irr = ia64_getreg(_IA64_REG_CR_IRR3); break;
	}

	return test_bit(bit, &irr);
}

static inline void
ia64_set_lrr0 (unsigned long val)
{
	ia64_setreg(_IA64_REG_CR_LRR0, val);
	ia64_srlz_d();
}

static inline void
ia64_set_lrr1 (unsigned long val)
{
	ia64_setreg(_IA64_REG_CR_LRR1, val);
	ia64_srlz_d();
}


static inline __u64
ia64_unat_pos (void *spill_addr)
{
	return ((__u64) spill_addr >> 3) & 0x3f;
}

static inline void
ia64_set_unat (__u64 *unat, void *spill_addr, unsigned long nat)
{
	__u64 bit = ia64_unat_pos(spill_addr);
	__u64 mask = 1UL << bit;

	*unat = (*unat & ~mask) | (nat << bit);
}

static inline unsigned long
thread_saved_pc (struct task_struct *t)
{
	struct unw_frame_info info;
	unsigned long ip;

	unw_init_from_blocked_task(&info, t);
	if (unw_unwind(&info) < 0)
		return 0;
	unw_get_ip(&info, &ip);
	return ip;
}

#define current_text_addr() \
	({ void *_pc; _pc = (void *)ia64_getreg(_IA64_REG_IP); _pc; })

static inline __u64
ia64_get_ivr (void)
{
	__u64 r;
	ia64_srlz_d();
	r = ia64_getreg(_IA64_REG_CR_IVR);
	ia64_srlz_d();
	return r;
}

static inline void
ia64_set_dbr (__u64 regnum, __u64 value)
{
	__ia64_set_dbr(regnum, value);
#ifdef CONFIG_ITANIUM
	ia64_srlz_d();
#endif
}

static inline __u64
ia64_get_dbr (__u64 regnum)
{
	__u64 retval;

	retval = __ia64_get_dbr(regnum);
#ifdef CONFIG_ITANIUM
	ia64_srlz_d();
#endif
	return retval;
}

static inline __u64
ia64_rotr (__u64 w, __u64 n)
{
	return (w >> n) | (w << (64 - n));
}

#define ia64_rotl(w,n)	ia64_rotr((w), (64) - (n))

static inline void *
ia64_imva (void *addr)
{
	void *result;
	result = (void *) ia64_tpa(addr);
	return __va(result);
}

#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH
#define PREFETCH_STRIDE			L1_CACHE_BYTES

static inline void
prefetch (const void *x)
{
	 ia64_lfetch(ia64_lfhint_none, x);
}

static inline void
prefetchw (const void *x)
{
	ia64_lfetch_excl(ia64_lfhint_none, x);
}

#define spin_lock_prefetch(x)	prefetchw(x)

extern unsigned long boot_option_idle_override;

enum idle_boot_override {IDLE_NO_OVERRIDE=0, IDLE_HALT, IDLE_FORCE_MWAIT,
			 IDLE_NOMWAIT, IDLE_POLL};

void cpu_idle_wait(void);
void default_idle(void);

#define ia64_platform_is(x) (strcmp(x, platform_name) == 0)

#endif 

#endif 
