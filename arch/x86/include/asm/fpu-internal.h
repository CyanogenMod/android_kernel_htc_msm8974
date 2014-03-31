/*
 * Copyright (C) 1994 Linus Torvalds
 *
 * Pentium III FXSR, SSE support
 * General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * x86-64 work by Andi Kleen 2002
 */

#ifndef _FPU_INTERNAL_H
#define _FPU_INTERNAL_H

#include <linux/kernel_stat.h>
#include <linux/regset.h>
#include <linux/slab.h>
#include <asm/asm.h>
#include <asm/cpufeature.h>
#include <asm/processor.h>
#include <asm/sigcontext.h>
#include <asm/user.h>
#include <asm/uaccess.h>
#include <asm/xsave.h>

extern unsigned int sig_xstate_size;
extern void fpu_init(void);

DECLARE_PER_CPU(struct task_struct *, fpu_owner_task);

extern user_regset_active_fn fpregs_active, xfpregs_active;
extern user_regset_get_fn fpregs_get, xfpregs_get, fpregs_soft_get,
				xstateregs_get;
extern user_regset_set_fn fpregs_set, xfpregs_set, fpregs_soft_set,
				 xstateregs_set;


#define xstateregs_active	fpregs_active

extern struct _fpx_sw_bytes fx_sw_reserved;
#ifdef CONFIG_IA32_EMULATION
extern unsigned int sig_xstate_ia32_size;
extern struct _fpx_sw_bytes fx_sw_reserved_ia32;
struct _fpstate_ia32;
struct _xstate_ia32;
extern int save_i387_xstate_ia32(void __user *buf);
extern int restore_i387_xstate_ia32(void __user *buf);
#endif

#ifdef CONFIG_MATH_EMULATION
extern void finit_soft_fpu(struct i387_soft_struct *soft);
#else
static inline void finit_soft_fpu(struct i387_soft_struct *soft) {}
#endif

#define X87_FSW_ES (1 << 7)	

static __always_inline __pure bool use_xsaveopt(void)
{
	return static_cpu_has(X86_FEATURE_XSAVEOPT);
}

static __always_inline __pure bool use_xsave(void)
{
	return static_cpu_has(X86_FEATURE_XSAVE);
}

static __always_inline __pure bool use_fxsr(void)
{
        return static_cpu_has(X86_FEATURE_FXSR);
}

extern void __sanitize_i387_state(struct task_struct *);

static inline void sanitize_i387_state(struct task_struct *tsk)
{
	if (!use_xsaveopt())
		return;
	__sanitize_i387_state(tsk);
}

#ifdef CONFIG_X86_64
static inline int fxrstor_checking(struct i387_fxsave_struct *fx)
{
	int err;

	
#ifdef CONFIG_AS_FXSAVEQ
	asm volatile("1:  fxrstorq %[fx]\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     _ASM_EXTABLE(1b, 3b)
		     : [err] "=r" (err)
		     : [fx] "m" (*fx), "0" (0));
#else
	asm volatile("1:  rex64/fxrstor (%[fx])\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     _ASM_EXTABLE(1b, 3b)
		     : [err] "=r" (err)
		     : [fx] "R" (fx), "m" (*fx), "0" (0));
#endif
	return err;
}

static inline int fxsave_user(struct i387_fxsave_struct __user *fx)
{
	int err;

	err = __clear_user(&fx->sw_reserved,
			   sizeof(struct _fpx_sw_bytes));
	if (unlikely(err))
		return -EFAULT;

	
#ifdef CONFIG_AS_FXSAVEQ
	asm volatile("1:  fxsaveq %[fx]\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     _ASM_EXTABLE(1b, 3b)
		     : [err] "=r" (err), [fx] "=m" (*fx)
		     : "0" (0));
#else
	asm volatile("1:  rex64/fxsave (%[fx])\n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3:  movl $-1,%[err]\n"
		     "    jmp  2b\n"
		     ".previous\n"
		     _ASM_EXTABLE(1b, 3b)
		     : [err] "=r" (err), "=m" (*fx)
		     : [fx] "R" (fx), "0" (0));
#endif
	if (unlikely(err) &&
	    __clear_user(fx, sizeof(struct i387_fxsave_struct)))
		err = -EFAULT;
	
	return err;
}

static inline void fpu_fxsave(struct fpu *fpu)
{

#ifdef CONFIG_AS_FXSAVEQ
	__asm__ __volatile__("fxsaveq %0"
			     : "=m" (fpu->state->fxsave));
#else
	asm volatile("rex64/fxsave (%[fx])"
		     : "=m" (fpu->state->fxsave)
		     : [fx] "R" (&fpu->state->fxsave));
#endif
}

#else  

static inline int fxrstor_checking(struct i387_fxsave_struct *fx)
{
	alternative_input(
		"nop ; frstor %1",
		"fxrstor %1",
		X86_FEATURE_FXSR,
		"m" (*fx));

	return 0;
}

static inline void fpu_fxsave(struct fpu *fpu)
{
	asm volatile("fxsave %[fx]"
		     : [fx] "=m" (fpu->state->fxsave));
}

#endif	

static inline int fpu_save_init(struct fpu *fpu)
{
	if (use_xsave()) {
		fpu_xsave(fpu);

		if (!(fpu->state->xsave.xsave_hdr.xstate_bv & XSTATE_FP))
			return 1;
	} else if (use_fxsr()) {
		fpu_fxsave(fpu);
	} else {
		asm volatile("fnsave %[fx]; fwait"
			     : [fx] "=m" (fpu->state->fsave));
		return 0;
	}

	if (unlikely(fpu->state->fxsave.swd & X87_FSW_ES)) {
		asm volatile("fnclex");
		return 0;
	}
	return 1;
}

static inline int __save_init_fpu(struct task_struct *tsk)
{
	return fpu_save_init(&tsk->thread.fpu);
}

static inline int fpu_fxrstor_checking(struct fpu *fpu)
{
	return fxrstor_checking(&fpu->state->fxsave);
}

static inline int fpu_restore_checking(struct fpu *fpu)
{
	if (use_xsave())
		return fpu_xrstor_checking(fpu);
	else
		return fpu_fxrstor_checking(fpu);
}

static inline int restore_fpu_checking(struct task_struct *tsk)
{
	alternative_input(
		ASM_NOP8 ASM_NOP2,
		"emms\n\t"		
		"fildl %P[addr]",	
		X86_FEATURE_FXSAVE_LEAK,
		[addr] "m" (tsk->thread.fpu.has_fpu));

	return fpu_restore_checking(&tsk->thread.fpu);
}

static inline int __thread_has_fpu(struct task_struct *tsk)
{
	return tsk->thread.fpu.has_fpu;
}

static inline void __thread_clear_has_fpu(struct task_struct *tsk)
{
	tsk->thread.fpu.has_fpu = 0;
	percpu_write(fpu_owner_task, NULL);
}

static inline void __thread_set_has_fpu(struct task_struct *tsk)
{
	tsk->thread.fpu.has_fpu = 1;
	percpu_write(fpu_owner_task, tsk);
}

static inline void __thread_fpu_end(struct task_struct *tsk)
{
	__thread_clear_has_fpu(tsk);
	stts();
}

static inline void __thread_fpu_begin(struct task_struct *tsk)
{
	clts();
	__thread_set_has_fpu(tsk);
}

typedef struct { int preload; } fpu_switch_t;

static inline int fpu_lazy_restore(struct task_struct *new, unsigned int cpu)
{
	return new == percpu_read_stable(fpu_owner_task) &&
		cpu == new->thread.fpu.last_cpu;
}

static inline fpu_switch_t switch_fpu_prepare(struct task_struct *old, struct task_struct *new, int cpu)
{
	fpu_switch_t fpu;

	fpu.preload = tsk_used_math(new) && new->fpu_counter > 5;
	if (__thread_has_fpu(old)) {
		if (!__save_init_fpu(old))
			cpu = ~0;
		old->thread.fpu.last_cpu = cpu;
		old->thread.fpu.has_fpu = 0;	

		
		if (fpu.preload) {
			new->fpu_counter++;
			__thread_set_has_fpu(new);
			prefetch(new->thread.fpu.state);
		} else
			stts();
	} else {
		old->fpu_counter = 0;
		old->thread.fpu.last_cpu = ~0;
		if (fpu.preload) {
			new->fpu_counter++;
			if (fpu_lazy_restore(new, cpu))
				fpu.preload = 0;
			else
				prefetch(new->thread.fpu.state);
			__thread_fpu_begin(new);
		}
	}
	return fpu;
}

static inline void switch_fpu_finish(struct task_struct *new, fpu_switch_t fpu)
{
	if (fpu.preload) {
		if (unlikely(restore_fpu_checking(new)))
			__thread_fpu_end(new);
	}
}

extern int save_i387_xstate(void __user *buf);
extern int restore_i387_xstate(void __user *buf);

static inline void __clear_fpu(struct task_struct *tsk)
{
	if (__thread_has_fpu(tsk)) {
		
		asm volatile("1: fwait\n"
			     "2:\n"
			     _ASM_EXTABLE(1b, 2b));
		__thread_fpu_end(tsk);
	}
}

static inline void user_fpu_end(void)
{
	preempt_disable();
	__thread_fpu_end(current);
	preempt_enable();
}

static inline void user_fpu_begin(void)
{
	preempt_disable();
	if (!user_has_fpu())
		__thread_fpu_begin(current);
	preempt_enable();
}

static inline void save_init_fpu(struct task_struct *tsk)
{
	WARN_ON_ONCE(!__thread_has_fpu(tsk));
	preempt_disable();
	__save_init_fpu(tsk);
	__thread_fpu_end(tsk);
	preempt_enable();
}

static inline void clear_fpu(struct task_struct *tsk)
{
	preempt_disable();
	__clear_fpu(tsk);
	preempt_enable();
}

static inline unsigned short get_fpu_cwd(struct task_struct *tsk)
{
	if (cpu_has_fxsr) {
		return tsk->thread.fpu.state->fxsave.cwd;
	} else {
		return (unsigned short)tsk->thread.fpu.state->fsave.cwd;
	}
}

static inline unsigned short get_fpu_swd(struct task_struct *tsk)
{
	if (cpu_has_fxsr) {
		return tsk->thread.fpu.state->fxsave.swd;
	} else {
		return (unsigned short)tsk->thread.fpu.state->fsave.swd;
	}
}

static inline unsigned short get_fpu_mxcsr(struct task_struct *tsk)
{
	if (cpu_has_xmm) {
		return tsk->thread.fpu.state->fxsave.mxcsr;
	} else {
		return MXCSR_DEFAULT;
	}
}

static bool fpu_allocated(struct fpu *fpu)
{
	return fpu->state != NULL;
}

static inline int fpu_alloc(struct fpu *fpu)
{
	if (fpu_allocated(fpu))
		return 0;
	fpu->state = kmem_cache_alloc(task_xstate_cachep, GFP_KERNEL);
	if (!fpu->state)
		return -ENOMEM;
	WARN_ON((unsigned long)fpu->state & 15);
	return 0;
}

static inline void fpu_free(struct fpu *fpu)
{
	if (fpu->state) {
		kmem_cache_free(task_xstate_cachep, fpu->state);
		fpu->state = NULL;
	}
}

static inline void fpu_copy(struct fpu *dst, struct fpu *src)
{
	memcpy(dst->state, src->state, xstate_size);
}

extern void fpu_finit(struct fpu *fpu);

#endif
