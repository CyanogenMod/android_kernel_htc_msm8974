#ifndef _ASM_POWERPC_PROCESSOR_H
#define _ASM_POWERPC_PROCESSOR_H

/*
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/reg.h>

#ifdef CONFIG_VSX
#define TS_FPRWIDTH 2
#else
#define TS_FPRWIDTH 1
#endif

#ifndef __ASSEMBLY__
#include <linux/compiler.h>
#include <linux/cache.h>
#include <asm/ptrace.h>
#include <asm/types.h>


#define _PREP_Motorola	0x01	
#define _PREP_Firm	0x02	
#define _PREP_IBM	0x00	
#define _PREP_Bull	0x03	

#define _CHRP_Motorola	0x04	
#define _CHRP_IBM	0x05	
#define _CHRP_Pegasos	0x06	
#define _CHRP_briq	0x07	

#if defined(__KERNEL__) && defined(CONFIG_PPC32)

extern int _chrp_type;

#ifdef CONFIG_PPC_PREP

extern int _prep_type;

#endif 

#endif 

#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#define HMT_very_low()   asm volatile("or 31,31,31   # very low priority")
#define HMT_low()	 asm volatile("or 1,1,1	     # low priority")
#define HMT_medium_low() asm volatile("or 6,6,6      # medium low priority")
#define HMT_medium()	 asm volatile("or 2,2,2	     # medium priority")
#define HMT_medium_high() asm volatile("or 5,5,5      # medium high priority")
#define HMT_high()	 asm volatile("or 3,3,3	     # high priority")

#ifdef __KERNEL__

struct task_struct;
void start_thread(struct pt_regs *regs, unsigned long fdptr, unsigned long sp);
void release_thread(struct task_struct *);

extern void prepare_to_copy(struct task_struct *tsk);

extern long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

extern struct task_struct *last_task_used_math;
extern struct task_struct *last_task_used_altivec;
extern struct task_struct *last_task_used_vsx;
extern struct task_struct *last_task_used_spe;

#ifdef CONFIG_PPC32

#if CONFIG_TASK_SIZE > CONFIG_KERNEL_START
#error User TASK_SIZE overlaps with KERNEL_START address
#endif
#define TASK_SIZE	(CONFIG_TASK_SIZE)

#define TASK_UNMAPPED_BASE	(TASK_SIZE / 8 * 3)
#endif

#ifdef CONFIG_PPC64
#define TASK_SIZE_USER64 (0x0000100000000000UL)

#define TASK_SIZE_USER32 (0x0000000100000000UL - (1*PAGE_SIZE))

#define TASK_SIZE_OF(tsk) (test_tsk_thread_flag(tsk, TIF_32BIT) ? \
		TASK_SIZE_USER32 : TASK_SIZE_USER64)
#define TASK_SIZE	  TASK_SIZE_OF(current)

#define TASK_UNMAPPED_BASE_USER32 (PAGE_ALIGN(TASK_SIZE_USER32 / 4))
#define TASK_UNMAPPED_BASE_USER64 (PAGE_ALIGN(TASK_SIZE_USER64 / 4))

#define TASK_UNMAPPED_BASE ((is_32bit_task()) ? \
		TASK_UNMAPPED_BASE_USER32 : TASK_UNMAPPED_BASE_USER64 )
#endif

#ifdef __powerpc64__

#define STACK_TOP_USER64 TASK_SIZE_USER64
#define STACK_TOP_USER32 TASK_SIZE_USER32

#define STACK_TOP (is_32bit_task() ? \
		   STACK_TOP_USER32 : STACK_TOP_USER64)

#define STACK_TOP_MAX STACK_TOP_USER64

#else 

#define STACK_TOP TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP

#endif 

typedef struct {
	unsigned long seg;
} mm_segment_t;

#define TS_FPROFFSET 0
#define TS_VSRLOWOFFSET 1
#define TS_FPR(i) fpr[i][TS_FPROFFSET]

struct thread_struct {
	unsigned long	ksp;		
	unsigned long	ksp_limit;	

#ifdef CONFIG_PPC64
	unsigned long	ksp_vsid;
#endif
	struct pt_regs	*regs;		
	mm_segment_t	fs;		
#ifdef CONFIG_BOOKE
	
	unsigned long	normsave[8] ____cacheline_aligned;
#endif
#ifdef CONFIG_PPC32
	void		*pgdir;		
#endif
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	unsigned long	dbcr0;
	unsigned long	dbcr1;
#ifdef CONFIG_BOOKE
	unsigned long	dbcr2;
#endif
	/*
	 * The stored value of the DBSR register will be the value at the
	 * last debug interrupt. This register can only be read from the
	 * user (will never be written to) and has value while helping to
	 * describe the reason for the last debug trap.  Torez
	 */
	unsigned long	dbsr;
	unsigned long	iac1;
	unsigned long	iac2;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
	unsigned long	iac3;
	unsigned long	iac4;
#endif
	unsigned long	dac1;
	unsigned long	dac2;
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
	unsigned long	dvc1;
	unsigned long	dvc2;
#endif
#endif
	
	double		fpr[32][TS_FPRWIDTH];
	struct {

		unsigned int pad;
		unsigned int val;	
	} fpscr;
	int		fpexc_mode;	
	unsigned int	align_ctl;	
#ifdef CONFIG_PPC64
	unsigned long	start_tb;	
	unsigned long	accum_tb;	
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	struct perf_event *ptrace_bps[HBP_NUM];
	struct perf_event *last_hit_ubp;
#endif 
#endif
	unsigned long	dabr;		
#ifdef CONFIG_ALTIVEC
	
	vector128	vr[32] __attribute__((aligned(16)));
	
	vector128	vscr __attribute__((aligned(16)));
	unsigned long	vrsave;
	int		used_vr;	
#endif 
#ifdef CONFIG_VSX
	
	int		used_vsr;	
#endif 
#ifdef CONFIG_SPE
	unsigned long	evr[32];	
	u64		acc;		
	unsigned long	spefscr;	
	int		used_spe;	
#endif 
#ifdef CONFIG_KVM_BOOK3S_32_HANDLER
	void*		kvm_shadow_vcpu; 
#endif 
#ifdef CONFIG_PPC64
	unsigned long	dscr;
	int		dscr_inherit;
#endif
};

#define ARCH_MIN_TASKALIGN 16

#define INIT_SP		(sizeof(init_stack) + (unsigned long) &init_stack)
#define INIT_SP_LIMIT \
	(_ALIGN_UP(sizeof(init_thread_info), 16) + (unsigned long) &init_stack)

#ifdef CONFIG_SPE
#define SPEFSCR_INIT .spefscr = SPEFSCR_FINVE | SPEFSCR_FDBZE | SPEFSCR_FUNFE | SPEFSCR_FOVFE,
#else
#define SPEFSCR_INIT
#endif

#ifdef CONFIG_PPC32
#define INIT_THREAD { \
	.ksp = INIT_SP, \
	.ksp_limit = INIT_SP_LIMIT, \
	.fs = KERNEL_DS, \
	.pgdir = swapper_pg_dir, \
	.fpexc_mode = MSR_FE0 | MSR_FE1, \
	SPEFSCR_INIT \
}
#else
#define INIT_THREAD  { \
	.ksp = INIT_SP, \
	.ksp_limit = INIT_SP_LIMIT, \
	.regs = (struct pt_regs *)INIT_SP - 1,  \
	.fs = KERNEL_DS, \
	.fpr = {{0}}, \
	.fpscr = { .val = 0, }, \
	.fpexc_mode = 0, \
}
#endif

#define thread_saved_pc(tsk)    \
        ((tsk)->thread.regs? (tsk)->thread.regs->nip: 0)

#define task_pt_regs(tsk)	((struct pt_regs *)(tsk)->thread.regs)

unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->nip: 0)
#define KSTK_ESP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->gpr[1]: 0)

#define GET_FPEXC_CTL(tsk, adr) get_fpexc_mode((tsk), (adr))
#define SET_FPEXC_CTL(tsk, val) set_fpexc_mode((tsk), (val))

extern int get_fpexc_mode(struct task_struct *tsk, unsigned long adr);
extern int set_fpexc_mode(struct task_struct *tsk, unsigned int val);

#define GET_ENDIAN(tsk, adr) get_endian((tsk), (adr))
#define SET_ENDIAN(tsk, val) set_endian((tsk), (val))

extern int get_endian(struct task_struct *tsk, unsigned long adr);
extern int set_endian(struct task_struct *tsk, unsigned int val);

#define GET_UNALIGN_CTL(tsk, adr)	get_unalign_ctl((tsk), (adr))
#define SET_UNALIGN_CTL(tsk, val)	set_unalign_ctl((tsk), (val))

extern int get_unalign_ctl(struct task_struct *tsk, unsigned long adr);
extern int set_unalign_ctl(struct task_struct *tsk, unsigned int val);

static inline unsigned int __unpack_fe01(unsigned long msr_bits)
{
	return ((msr_bits & MSR_FE0) >> 10) | ((msr_bits & MSR_FE1) >> 8);
}

static inline unsigned long __pack_fe01(unsigned int fpmode)
{
	return ((fpmode << 10) & MSR_FE0) | ((fpmode << 8) & MSR_FE1);
}

#ifdef CONFIG_PPC64
#define cpu_relax()	do { HMT_low(); HMT_medium(); barrier(); } while (0)
#else
#define cpu_relax()	barrier()
#endif

int validate_sp(unsigned long sp, struct task_struct *p,
                       unsigned long nbytes);

#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

static inline void prefetch(const void *x)
{
	if (unlikely(!x))
		return;

	__asm__ __volatile__ ("dcbt 0,%0" : : "r" (x));
}

static inline void prefetchw(const void *x)
{
	if (unlikely(!x))
		return;

	__asm__ __volatile__ ("dcbtst 0,%0" : : "r" (x));
}

#define spin_lock_prefetch(x)	prefetchw(x)

#ifdef CONFIG_PPC64
#define HAVE_ARCH_PICK_MMAP_LAYOUT
#endif

#ifdef CONFIG_PPC64
static inline unsigned long get_clean_sp(struct pt_regs *regs, int is_32)
{
	unsigned long sp;

	if (is_32)
		sp = regs->gpr[1] & 0x0ffffffffUL;
	else
		sp = regs->gpr[1];

	return sp;
}
#else
static inline unsigned long get_clean_sp(struct pt_regs *regs, int is_32)
{
	return regs->gpr[1];
}
#endif

extern unsigned long cpuidle_disable;
enum idle_boot_override {IDLE_NO_OVERRIDE = 0, IDLE_POWERSAVE_OFF};

extern int powersave_nap;	
void cpu_idle_wait(void);

#ifdef CONFIG_PSERIES_IDLE
extern void update_smt_snooze_delay(int snooze);
extern int pseries_notify_cpuidle_add_cpu(int cpu);
#else
static inline void update_smt_snooze_delay(int snooze) {}
static inline int pseries_notify_cpuidle_add_cpu(int cpu) { return 0; }
#endif

extern void flush_instruction_cache(void);
extern void hard_reset_now(void);
extern void poweroff_now(void);
extern int fix_alignment(struct pt_regs *);
extern void cvt_fd(float *from, double *to);
extern void cvt_df(double *from, float *to);
extern void _nmask_and_or_msr(unsigned long nmask, unsigned long or_val);

#ifdef CONFIG_PPC64
#define NET_IP_ALIGN	0
#endif

#endif 
#endif 
#endif 
