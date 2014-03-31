/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/m68k/kernel/ptrace.c"
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * Modified by Cort Dougan (cort@hq.fsmlabs.com)
 * and Paul Mackerras (paulus@samba.org).
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file README.legal in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/regset.h>
#include <linux/tracehook.h>
#include <linux/elf.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/signal.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <trace/syscall.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/switch_to.h>

#define CREATE_TRACE_POINTS
#include <trace/events/syscalls.h>

#ifdef CONFIG_PPC32
#define PARAMETER_SAVE_AREA_OFFSET	24  
#else 
#define PARAMETER_SAVE_AREA_OFFSET	48  
#endif

struct pt_regs_offset {
	const char *name;
	int offset;
};

#define STR(s)	#s			
#define REG_OFFSET_NAME(r) {.name = #r, .offset = offsetof(struct pt_regs, r)}
#define GPR_OFFSET_NAME(num)	\
	{.name = STR(gpr##num), .offset = offsetof(struct pt_regs, gpr[num])}
#define REG_OFFSET_END {.name = NULL, .offset = 0}

static const struct pt_regs_offset regoffset_table[] = {
	GPR_OFFSET_NAME(0),
	GPR_OFFSET_NAME(1),
	GPR_OFFSET_NAME(2),
	GPR_OFFSET_NAME(3),
	GPR_OFFSET_NAME(4),
	GPR_OFFSET_NAME(5),
	GPR_OFFSET_NAME(6),
	GPR_OFFSET_NAME(7),
	GPR_OFFSET_NAME(8),
	GPR_OFFSET_NAME(9),
	GPR_OFFSET_NAME(10),
	GPR_OFFSET_NAME(11),
	GPR_OFFSET_NAME(12),
	GPR_OFFSET_NAME(13),
	GPR_OFFSET_NAME(14),
	GPR_OFFSET_NAME(15),
	GPR_OFFSET_NAME(16),
	GPR_OFFSET_NAME(17),
	GPR_OFFSET_NAME(18),
	GPR_OFFSET_NAME(19),
	GPR_OFFSET_NAME(20),
	GPR_OFFSET_NAME(21),
	GPR_OFFSET_NAME(22),
	GPR_OFFSET_NAME(23),
	GPR_OFFSET_NAME(24),
	GPR_OFFSET_NAME(25),
	GPR_OFFSET_NAME(26),
	GPR_OFFSET_NAME(27),
	GPR_OFFSET_NAME(28),
	GPR_OFFSET_NAME(29),
	GPR_OFFSET_NAME(30),
	GPR_OFFSET_NAME(31),
	REG_OFFSET_NAME(nip),
	REG_OFFSET_NAME(msr),
	REG_OFFSET_NAME(ctr),
	REG_OFFSET_NAME(link),
	REG_OFFSET_NAME(xer),
	REG_OFFSET_NAME(ccr),
#ifdef CONFIG_PPC64
	REG_OFFSET_NAME(softe),
#else
	REG_OFFSET_NAME(mq),
#endif
	REG_OFFSET_NAME(trap),
	REG_OFFSET_NAME(dar),
	REG_OFFSET_NAME(dsisr),
	REG_OFFSET_END,
};

int regs_query_register_offset(const char *name)
{
	const struct pt_regs_offset *roff;
	for (roff = regoffset_table; roff->name != NULL; roff++)
		if (!strcmp(roff->name, name))
			return roff->offset;
	return -EINVAL;
}

const char *regs_query_register_name(unsigned int offset)
{
	const struct pt_regs_offset *roff;
	for (roff = regoffset_table; roff->name != NULL; roff++)
		if (roff->offset == offset)
			return roff->name;
	return NULL;
}


#ifdef CONFIG_PPC_ADV_DEBUG_REGS
#define MSR_DEBUGCHANGE	0
#else
#define MSR_DEBUGCHANGE	(MSR_SE | MSR_BE)
#endif

#ifdef CONFIG_PPC32
#define PT_MAX_PUT_REG	PT_MQ
#else
#define PT_MAX_PUT_REG	PT_CCR
#endif

static unsigned long get_user_msr(struct task_struct *task)
{
	return task->thread.regs->msr | task->thread.fpexc_mode;
}

static int set_user_msr(struct task_struct *task, unsigned long msr)
{
	task->thread.regs->msr &= ~MSR_DEBUGCHANGE;
	task->thread.regs->msr |= msr & MSR_DEBUGCHANGE;
	return 0;
}

static int set_user_trap(struct task_struct *task, unsigned long trap)
{
	task->thread.regs->trap = trap & 0xfff0;
	return 0;
}

unsigned long ptrace_get_reg(struct task_struct *task, int regno)
{
	if (task->thread.regs == NULL)
		return -EIO;

	if (regno == PT_MSR)
		return get_user_msr(task);

	if (regno < (sizeof(struct pt_regs) / sizeof(unsigned long)))
		return ((unsigned long *)task->thread.regs)[regno];

	return -EIO;
}

int ptrace_put_reg(struct task_struct *task, int regno, unsigned long data)
{
	if (task->thread.regs == NULL)
		return -EIO;

	if (regno == PT_MSR)
		return set_user_msr(task, data);
	if (regno == PT_TRAP)
		return set_user_trap(task, data);

	if (regno <= PT_MAX_PUT_REG) {
		((unsigned long *)task->thread.regs)[regno] = data;
		return 0;
	}
	return -EIO;
}

static int gpr_get(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   void *kbuf, void __user *ubuf)
{
	int i, ret;

	if (target->thread.regs == NULL)
		return -EIO;

	if (!FULL_REGS(target->thread.regs)) {
		
		for (i = 14; i < 32; i++)
			target->thread.regs->gpr[i] = NV_REG_POISON;
	}

	ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				  target->thread.regs,
				  0, offsetof(struct pt_regs, msr));
	if (!ret) {
		unsigned long msr = get_user_msr(target);
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, &msr,
					  offsetof(struct pt_regs, msr),
					  offsetof(struct pt_regs, msr) +
					  sizeof(msr));
	}

	BUILD_BUG_ON(offsetof(struct pt_regs, orig_gpr3) !=
		     offsetof(struct pt_regs, msr) + sizeof(long));

	if (!ret)
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
					  &target->thread.regs->orig_gpr3,
					  offsetof(struct pt_regs, orig_gpr3),
					  sizeof(struct pt_regs));
	if (!ret)
		ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
					       sizeof(struct pt_regs), -1);

	return ret;
}

static int gpr_set(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   const void *kbuf, const void __user *ubuf)
{
	unsigned long reg;
	int ret;

	if (target->thread.regs == NULL)
		return -EIO;

	CHECK_FULL_REGS(target->thread.regs);

	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 target->thread.regs,
				 0, PT_MSR * sizeof(reg));

	if (!ret && count > 0) {
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, &reg,
					 PT_MSR * sizeof(reg),
					 (PT_MSR + 1) * sizeof(reg));
		if (!ret)
			ret = set_user_msr(target, reg);
	}

	BUILD_BUG_ON(offsetof(struct pt_regs, orig_gpr3) !=
		     offsetof(struct pt_regs, msr) + sizeof(long));

	if (!ret)
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
					 &target->thread.regs->orig_gpr3,
					 PT_ORIG_R3 * sizeof(reg),
					 (PT_MAX_PUT_REG + 1) * sizeof(reg));

	if (PT_MAX_PUT_REG + 1 < PT_TRAP && !ret)
		ret = user_regset_copyin_ignore(
			&pos, &count, &kbuf, &ubuf,
			(PT_MAX_PUT_REG + 1) * sizeof(reg),
			PT_TRAP * sizeof(reg));

	if (!ret && count > 0) {
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, &reg,
					 PT_TRAP * sizeof(reg),
					 (PT_TRAP + 1) * sizeof(reg));
		if (!ret)
			ret = set_user_trap(target, reg);
	}

	if (!ret)
		ret = user_regset_copyin_ignore(
			&pos, &count, &kbuf, &ubuf,
			(PT_TRAP + 1) * sizeof(reg), -1);

	return ret;
}

static int fpr_get(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   void *kbuf, void __user *ubuf)
{
#ifdef CONFIG_VSX
	double buf[33];
	int i;
#endif
	flush_fp_to_thread(target);

#ifdef CONFIG_VSX
	
	for (i = 0; i < 32 ; i++)
		buf[i] = target->thread.TS_FPR(i);
	memcpy(&buf[32], &target->thread.fpscr, sizeof(double));
	return user_regset_copyout(&pos, &count, &kbuf, &ubuf, buf, 0, -1);

#else
	BUILD_BUG_ON(offsetof(struct thread_struct, fpscr) !=
		     offsetof(struct thread_struct, TS_FPR(32)));

	return user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				   &target->thread.fpr, 0, -1);
#endif
}

static int fpr_set(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   const void *kbuf, const void __user *ubuf)
{
#ifdef CONFIG_VSX
	double buf[33];
	int i;
#endif
	flush_fp_to_thread(target);

#ifdef CONFIG_VSX
	
	i = user_regset_copyin(&pos, &count, &kbuf, &ubuf, buf, 0, -1);
	if (i)
		return i;
	for (i = 0; i < 32 ; i++)
		target->thread.TS_FPR(i) = buf[i];
	memcpy(&target->thread.fpscr, &buf[32], sizeof(double));
	return 0;
#else
	BUILD_BUG_ON(offsetof(struct thread_struct, fpscr) !=
		     offsetof(struct thread_struct, TS_FPR(32)));

	return user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				  &target->thread.fpr, 0, -1);
#endif
}

#ifdef CONFIG_ALTIVEC

static int vr_active(struct task_struct *target,
		     const struct user_regset *regset)
{
	flush_altivec_to_thread(target);
	return target->thread.used_vr ? regset->n : 0;
}

static int vr_get(struct task_struct *target, const struct user_regset *regset,
		  unsigned int pos, unsigned int count,
		  void *kbuf, void __user *ubuf)
{
	int ret;

	flush_altivec_to_thread(target);

	BUILD_BUG_ON(offsetof(struct thread_struct, vscr) !=
		     offsetof(struct thread_struct, vr[32]));

	ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				  &target->thread.vr, 0,
				  33 * sizeof(vector128));
	if (!ret) {
		union {
			elf_vrreg_t reg;
			u32 word;
		} vrsave;
		memset(&vrsave, 0, sizeof(vrsave));
		vrsave.word = target->thread.vrsave;
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, &vrsave,
					  33 * sizeof(vector128), -1);
	}

	return ret;
}

static int vr_set(struct task_struct *target, const struct user_regset *regset,
		  unsigned int pos, unsigned int count,
		  const void *kbuf, const void __user *ubuf)
{
	int ret;

	flush_altivec_to_thread(target);

	BUILD_BUG_ON(offsetof(struct thread_struct, vscr) !=
		     offsetof(struct thread_struct, vr[32]));

	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 &target->thread.vr, 0, 33 * sizeof(vector128));
	if (!ret && count > 0) {
		union {
			elf_vrreg_t reg;
			u32 word;
		} vrsave;
		memset(&vrsave, 0, sizeof(vrsave));
		vrsave.word = target->thread.vrsave;
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, &vrsave,
					 33 * sizeof(vector128), -1);
		if (!ret)
			target->thread.vrsave = vrsave.word;
	}

	return ret;
}
#endif 

#ifdef CONFIG_VSX

static int vsr_active(struct task_struct *target,
		      const struct user_regset *regset)
{
	flush_vsx_to_thread(target);
	return target->thread.used_vsr ? regset->n : 0;
}

static int vsr_get(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   void *kbuf, void __user *ubuf)
{
	double buf[32];
	int ret, i;

	flush_vsx_to_thread(target);

	for (i = 0; i < 32 ; i++)
		buf[i] = target->thread.fpr[i][TS_VSRLOWOFFSET];
	ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				  buf, 0, 32 * sizeof(double));

	return ret;
}

static int vsr_set(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   const void *kbuf, const void __user *ubuf)
{
	double buf[32];
	int ret,i;

	flush_vsx_to_thread(target);

	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 buf, 0, 32 * sizeof(double));
	for (i = 0; i < 32 ; i++)
		target->thread.fpr[i][TS_VSRLOWOFFSET] = buf[i];


	return ret;
}
#endif 

#ifdef CONFIG_SPE


static int evr_active(struct task_struct *target,
		      const struct user_regset *regset)
{
	flush_spe_to_thread(target);
	return target->thread.used_spe ? regset->n : 0;
}

static int evr_get(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   void *kbuf, void __user *ubuf)
{
	int ret;

	flush_spe_to_thread(target);

	ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				  &target->thread.evr,
				  0, sizeof(target->thread.evr));

	BUILD_BUG_ON(offsetof(struct thread_struct, acc) + sizeof(u64) !=
		     offsetof(struct thread_struct, spefscr));

	if (!ret)
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
					  &target->thread.acc,
					  sizeof(target->thread.evr), -1);

	return ret;
}

static int evr_set(struct task_struct *target, const struct user_regset *regset,
		   unsigned int pos, unsigned int count,
		   const void *kbuf, const void __user *ubuf)
{
	int ret;

	flush_spe_to_thread(target);

	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 &target->thread.evr,
				 0, sizeof(target->thread.evr));

	BUILD_BUG_ON(offsetof(struct thread_struct, acc) + sizeof(u64) !=
		     offsetof(struct thread_struct, spefscr));

	if (!ret)
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
					 &target->thread.acc,
					 sizeof(target->thread.evr), -1);

	return ret;
}
#endif 


enum powerpc_regset {
	REGSET_GPR,
	REGSET_FPR,
#ifdef CONFIG_ALTIVEC
	REGSET_VMX,
#endif
#ifdef CONFIG_VSX
	REGSET_VSX,
#endif
#ifdef CONFIG_SPE
	REGSET_SPE,
#endif
};

static const struct user_regset native_regsets[] = {
	[REGSET_GPR] = {
		.core_note_type = NT_PRSTATUS, .n = ELF_NGREG,
		.size = sizeof(long), .align = sizeof(long),
		.get = gpr_get, .set = gpr_set
	},
	[REGSET_FPR] = {
		.core_note_type = NT_PRFPREG, .n = ELF_NFPREG,
		.size = sizeof(double), .align = sizeof(double),
		.get = fpr_get, .set = fpr_set
	},
#ifdef CONFIG_ALTIVEC
	[REGSET_VMX] = {
		.core_note_type = NT_PPC_VMX, .n = 34,
		.size = sizeof(vector128), .align = sizeof(vector128),
		.active = vr_active, .get = vr_get, .set = vr_set
	},
#endif
#ifdef CONFIG_VSX
	[REGSET_VSX] = {
		.core_note_type = NT_PPC_VSX, .n = 32,
		.size = sizeof(double), .align = sizeof(double),
		.active = vsr_active, .get = vsr_get, .set = vsr_set
	},
#endif
#ifdef CONFIG_SPE
	[REGSET_SPE] = {
		.n = 35,
		.size = sizeof(u32), .align = sizeof(u32),
		.active = evr_active, .get = evr_get, .set = evr_set
	},
#endif
};

static const struct user_regset_view user_ppc_native_view = {
	.name = UTS_MACHINE, .e_machine = ELF_ARCH, .ei_osabi = ELF_OSABI,
	.regsets = native_regsets, .n = ARRAY_SIZE(native_regsets)
};

#ifdef CONFIG_PPC64
#include <linux/compat.h>

static int gpr32_get(struct task_struct *target,
		     const struct user_regset *regset,
		     unsigned int pos, unsigned int count,
		     void *kbuf, void __user *ubuf)
{
	const unsigned long *regs = &target->thread.regs->gpr[0];
	compat_ulong_t *k = kbuf;
	compat_ulong_t __user *u = ubuf;
	compat_ulong_t reg;
	int i;

	if (target->thread.regs == NULL)
		return -EIO;

	if (!FULL_REGS(target->thread.regs)) {
		
		for (i = 14; i < 32; i++)
			target->thread.regs->gpr[i] = NV_REG_POISON; 
	}

	pos /= sizeof(reg);
	count /= sizeof(reg);

	if (kbuf)
		for (; count > 0 && pos < PT_MSR; --count)
			*k++ = regs[pos++];
	else
		for (; count > 0 && pos < PT_MSR; --count)
			if (__put_user((compat_ulong_t) regs[pos++], u++))
				return -EFAULT;

	if (count > 0 && pos == PT_MSR) {
		reg = get_user_msr(target);
		if (kbuf)
			*k++ = reg;
		else if (__put_user(reg, u++))
			return -EFAULT;
		++pos;
		--count;
	}

	if (kbuf)
		for (; count > 0 && pos < PT_REGS_COUNT; --count)
			*k++ = regs[pos++];
	else
		for (; count > 0 && pos < PT_REGS_COUNT; --count)
			if (__put_user((compat_ulong_t) regs[pos++], u++))
				return -EFAULT;

	kbuf = k;
	ubuf = u;
	pos *= sizeof(reg);
	count *= sizeof(reg);
	return user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
					PT_REGS_COUNT * sizeof(reg), -1);
}

static int gpr32_set(struct task_struct *target,
		     const struct user_regset *regset,
		     unsigned int pos, unsigned int count,
		     const void *kbuf, const void __user *ubuf)
{
	unsigned long *regs = &target->thread.regs->gpr[0];
	const compat_ulong_t *k = kbuf;
	const compat_ulong_t __user *u = ubuf;
	compat_ulong_t reg;

	if (target->thread.regs == NULL)
		return -EIO;

	CHECK_FULL_REGS(target->thread.regs);

	pos /= sizeof(reg);
	count /= sizeof(reg);

	if (kbuf)
		for (; count > 0 && pos < PT_MSR; --count)
			regs[pos++] = *k++;
	else
		for (; count > 0 && pos < PT_MSR; --count) {
			if (__get_user(reg, u++))
				return -EFAULT;
			regs[pos++] = reg;
		}


	if (count > 0 && pos == PT_MSR) {
		if (kbuf)
			reg = *k++;
		else if (__get_user(reg, u++))
			return -EFAULT;
		set_user_msr(target, reg);
		++pos;
		--count;
	}

	if (kbuf) {
		for (; count > 0 && pos <= PT_MAX_PUT_REG; --count)
			regs[pos++] = *k++;
		for (; count > 0 && pos < PT_TRAP; --count, ++pos)
			++k;
	} else {
		for (; count > 0 && pos <= PT_MAX_PUT_REG; --count) {
			if (__get_user(reg, u++))
				return -EFAULT;
			regs[pos++] = reg;
		}
		for (; count > 0 && pos < PT_TRAP; --count, ++pos)
			if (__get_user(reg, u++))
				return -EFAULT;
	}

	if (count > 0 && pos == PT_TRAP) {
		if (kbuf)
			reg = *k++;
		else if (__get_user(reg, u++))
			return -EFAULT;
		set_user_trap(target, reg);
		++pos;
		--count;
	}

	kbuf = k;
	ubuf = u;
	pos *= sizeof(reg);
	count *= sizeof(reg);
	return user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf,
					 (PT_TRAP + 1) * sizeof(reg), -1);
}

static const struct user_regset compat_regsets[] = {
	[REGSET_GPR] = {
		.core_note_type = NT_PRSTATUS, .n = ELF_NGREG,
		.size = sizeof(compat_long_t), .align = sizeof(compat_long_t),
		.get = gpr32_get, .set = gpr32_set
	},
	[REGSET_FPR] = {
		.core_note_type = NT_PRFPREG, .n = ELF_NFPREG,
		.size = sizeof(double), .align = sizeof(double),
		.get = fpr_get, .set = fpr_set
	},
#ifdef CONFIG_ALTIVEC
	[REGSET_VMX] = {
		.core_note_type = NT_PPC_VMX, .n = 34,
		.size = sizeof(vector128), .align = sizeof(vector128),
		.active = vr_active, .get = vr_get, .set = vr_set
	},
#endif
#ifdef CONFIG_SPE
	[REGSET_SPE] = {
		.core_note_type = NT_PPC_SPE, .n = 35,
		.size = sizeof(u32), .align = sizeof(u32),
		.active = evr_active, .get = evr_get, .set = evr_set
	},
#endif
};

static const struct user_regset_view user_ppc_compat_view = {
	.name = "ppc", .e_machine = EM_PPC, .ei_osabi = ELF_OSABI,
	.regsets = compat_regsets, .n = ARRAY_SIZE(compat_regsets)
};
#endif	

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
#ifdef CONFIG_PPC64
	if (test_tsk_thread_flag(task, TIF_32BIT))
		return &user_ppc_compat_view;
#endif
	return &user_ppc_native_view;
}


void user_enable_single_step(struct task_struct *task)
{
	struct pt_regs *regs = task->thread.regs;

	if (regs != NULL) {
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		task->thread.dbcr0 &= ~DBCR0_BT;
		task->thread.dbcr0 |= DBCR0_IDM | DBCR0_IC;
		regs->msr |= MSR_DE;
#else
		regs->msr &= ~MSR_BE;
		regs->msr |= MSR_SE;
#endif
	}
	set_tsk_thread_flag(task, TIF_SINGLESTEP);
}

void user_enable_block_step(struct task_struct *task)
{
	struct pt_regs *regs = task->thread.regs;

	if (regs != NULL) {
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		task->thread.dbcr0 &= ~DBCR0_IC;
		task->thread.dbcr0 = DBCR0_IDM | DBCR0_BT;
		regs->msr |= MSR_DE;
#else
		regs->msr &= ~MSR_SE;
		regs->msr |= MSR_BE;
#endif
	}
	set_tsk_thread_flag(task, TIF_SINGLESTEP);
}

void user_disable_single_step(struct task_struct *task)
{
	struct pt_regs *regs = task->thread.regs;

	if (regs != NULL) {
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		task->thread.dbcr0 &= ~DBCR0_IC;
		if (!DBCR_ACTIVE_EVENTS(task->thread.dbcr0,
					task->thread.dbcr1)) {
			task->thread.dbcr0 &= ~DBCR0_IDM;
			regs->msr &= ~MSR_DE;
		}
#else
		regs->msr &= ~(MSR_SE | MSR_BE);
#endif
	}
	clear_tsk_thread_flag(task, TIF_SINGLESTEP);
}

#ifdef CONFIG_HAVE_HW_BREAKPOINT
void ptrace_triggered(struct perf_event *bp,
		      struct perf_sample_data *data, struct pt_regs *regs)
{
	struct perf_event_attr attr;

	attr = bp->attr;
	attr.disabled = true;
	modify_user_hw_breakpoint(bp, &attr);
}
#endif 

int ptrace_set_debugreg(struct task_struct *task, unsigned long addr,
			       unsigned long data)
{
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	int ret;
	struct thread_struct *thread = &(task->thread);
	struct perf_event *bp;
	struct perf_event_attr attr;
#endif 

	if (addr > 0)
		return -EINVAL;

	
	if ((data & ~0x7UL) >= TASK_SIZE)
		return -EIO;

#ifndef CONFIG_PPC_ADV_DEBUG_REGS

	
	if (data && !(data & DABR_TRANSLATION))
		return -EIO;
#ifdef CONFIG_HAVE_HW_BREAKPOINT
	if (ptrace_get_breakpoints(task) < 0)
		return -ESRCH;

	bp = thread->ptrace_bps[0];
	if ((!data) || !(data & (DABR_DATA_WRITE | DABR_DATA_READ))) {
		if (bp) {
			unregister_hw_breakpoint(bp);
			thread->ptrace_bps[0] = NULL;
		}
		ptrace_put_breakpoints(task);
		return 0;
	}
	if (bp) {
		attr = bp->attr;
		attr.bp_addr = data & ~HW_BREAKPOINT_ALIGN;
		arch_bp_generic_fields(data &
					(DABR_DATA_WRITE | DABR_DATA_READ),
							&attr.bp_type);
		ret =  modify_user_hw_breakpoint(bp, &attr);
		if (ret) {
			ptrace_put_breakpoints(task);
			return ret;
		}
		thread->ptrace_bps[0] = bp;
		ptrace_put_breakpoints(task);
		thread->dabr = data;
		return 0;
	}

	
	hw_breakpoint_init(&attr);
	attr.bp_addr = data & ~HW_BREAKPOINT_ALIGN;
	arch_bp_generic_fields(data & (DABR_DATA_WRITE | DABR_DATA_READ),
								&attr.bp_type);

	thread->ptrace_bps[0] = bp = register_user_hw_breakpoint(&attr,
					       ptrace_triggered, NULL, task);
	if (IS_ERR(bp)) {
		thread->ptrace_bps[0] = NULL;
		ptrace_put_breakpoints(task);
		return PTR_ERR(bp);
	}

	ptrace_put_breakpoints(task);

#endif 

	
	task->thread.dabr = data;
#else 

	
	task->thread.dac1 = data & ~0x3UL;

	if (task->thread.dac1 == 0) {
		dbcr_dac(task) &= ~(DBCR_DAC1R | DBCR_DAC1W);
		if (!DBCR_ACTIVE_EVENTS(task->thread.dbcr0,
					task->thread.dbcr1)) {
			task->thread.regs->msr &= ~MSR_DE;
			task->thread.dbcr0 &= ~DBCR0_IDM;
		}
		return 0;
	}

	

	if (!(data & 0x3UL))
		return -EINVAL;

	task->thread.dbcr0 |= DBCR0_IDM;

	dbcr_dac(task) &= ~(DBCR_DAC1R|DBCR_DAC1W);
	if (data & 0x1UL)
		dbcr_dac(task) |= DBCR_DAC1R;
	if (data & 0x2UL)
		dbcr_dac(task) |= DBCR_DAC1W;
	task->thread.regs->msr |= MSR_DE;
#endif 
	return 0;
}

void ptrace_disable(struct task_struct *child)
{
	
	user_disable_single_step(child);
}

#ifdef CONFIG_PPC_ADV_DEBUG_REGS
static long set_intruction_bp(struct task_struct *child,
			      struct ppc_hw_breakpoint *bp_info)
{
	int slot;
	int slot1_in_use = ((child->thread.dbcr0 & DBCR0_IAC1) != 0);
	int slot2_in_use = ((child->thread.dbcr0 & DBCR0_IAC2) != 0);
	int slot3_in_use = ((child->thread.dbcr0 & DBCR0_IAC3) != 0);
	int slot4_in_use = ((child->thread.dbcr0 & DBCR0_IAC4) != 0);

	if (dbcr_iac_range(child) & DBCR_IAC12MODE)
		slot2_in_use = 1;
	if (dbcr_iac_range(child) & DBCR_IAC34MODE)
		slot4_in_use = 1;

	if (bp_info->addr >= TASK_SIZE)
		return -EIO;

	if (bp_info->addr_mode != PPC_BREAKPOINT_MODE_EXACT) {

		
		if (bp_info->addr2 >= TASK_SIZE)
			return -EIO;

		
		if ((!slot1_in_use) && (!slot2_in_use)) {
			slot = 1;
			child->thread.iac1 = bp_info->addr;
			child->thread.iac2 = bp_info->addr2;
			child->thread.dbcr0 |= DBCR0_IAC1;
			if (bp_info->addr_mode ==
					PPC_BREAKPOINT_MODE_RANGE_EXCLUSIVE)
				dbcr_iac_range(child) |= DBCR_IAC12X;
			else
				dbcr_iac_range(child) |= DBCR_IAC12I;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
		} else if ((!slot3_in_use) && (!slot4_in_use)) {
			slot = 3;
			child->thread.iac3 = bp_info->addr;
			child->thread.iac4 = bp_info->addr2;
			child->thread.dbcr0 |= DBCR0_IAC3;
			if (bp_info->addr_mode ==
					PPC_BREAKPOINT_MODE_RANGE_EXCLUSIVE)
				dbcr_iac_range(child) |= DBCR_IAC34X;
			else
				dbcr_iac_range(child) |= DBCR_IAC34I;
#endif
		} else
			return -ENOSPC;
	} else {
		if (!slot1_in_use) {
			if (slot2_in_use || (slot3_in_use == slot4_in_use)) {
				slot = 1;
				child->thread.iac1 = bp_info->addr;
				child->thread.dbcr0 |= DBCR0_IAC1;
				goto out;
			}
		}
		if (!slot2_in_use) {
			slot = 2;
			child->thread.iac2 = bp_info->addr;
			child->thread.dbcr0 |= DBCR0_IAC2;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
		} else if (!slot3_in_use) {
			slot = 3;
			child->thread.iac3 = bp_info->addr;
			child->thread.dbcr0 |= DBCR0_IAC3;
		} else if (!slot4_in_use) {
			slot = 4;
			child->thread.iac4 = bp_info->addr;
			child->thread.dbcr0 |= DBCR0_IAC4;
#endif
		} else
			return -ENOSPC;
	}
out:
	child->thread.dbcr0 |= DBCR0_IDM;
	child->thread.regs->msr |= MSR_DE;

	return slot;
}

static int del_instruction_bp(struct task_struct *child, int slot)
{
	switch (slot) {
	case 1:
		if ((child->thread.dbcr0 & DBCR0_IAC1) == 0)
			return -ENOENT;

		if (dbcr_iac_range(child) & DBCR_IAC12MODE) {
			
			child->thread.iac2 = 0;
			dbcr_iac_range(child) &= ~DBCR_IAC12MODE;
		}
		child->thread.iac1 = 0;
		child->thread.dbcr0 &= ~DBCR0_IAC1;
		break;
	case 2:
		if ((child->thread.dbcr0 & DBCR0_IAC2) == 0)
			return -ENOENT;

		if (dbcr_iac_range(child) & DBCR_IAC12MODE)
			
			return -EINVAL;
		child->thread.iac2 = 0;
		child->thread.dbcr0 &= ~DBCR0_IAC2;
		break;
#if CONFIG_PPC_ADV_DEBUG_IACS > 2
	case 3:
		if ((child->thread.dbcr0 & DBCR0_IAC3) == 0)
			return -ENOENT;

		if (dbcr_iac_range(child) & DBCR_IAC34MODE) {
			
			child->thread.iac4 = 0;
			dbcr_iac_range(child) &= ~DBCR_IAC34MODE;
		}
		child->thread.iac3 = 0;
		child->thread.dbcr0 &= ~DBCR0_IAC3;
		break;
	case 4:
		if ((child->thread.dbcr0 & DBCR0_IAC4) == 0)
			return -ENOENT;

		if (dbcr_iac_range(child) & DBCR_IAC34MODE)
			
			return -EINVAL;
		child->thread.iac4 = 0;
		child->thread.dbcr0 &= ~DBCR0_IAC4;
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static int set_dac(struct task_struct *child, struct ppc_hw_breakpoint *bp_info)
{
	int byte_enable =
		(bp_info->condition_mode >> PPC_BREAKPOINT_CONDITION_BE_SHIFT)
		& 0xf;
	int condition_mode =
		bp_info->condition_mode & PPC_BREAKPOINT_CONDITION_MODE;
	int slot;

	if (byte_enable && (condition_mode == 0))
		return -EINVAL;

	if (bp_info->addr >= TASK_SIZE)
		return -EIO;

	if ((dbcr_dac(child) & (DBCR_DAC1R | DBCR_DAC1W)) == 0) {
		slot = 1;
		if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_READ)
			dbcr_dac(child) |= DBCR_DAC1R;
		if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_WRITE)
			dbcr_dac(child) |= DBCR_DAC1W;
		child->thread.dac1 = (unsigned long)bp_info->addr;
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
		if (byte_enable) {
			child->thread.dvc1 =
				(unsigned long)bp_info->condition_value;
			child->thread.dbcr2 |=
				((byte_enable << DBCR2_DVC1BE_SHIFT) |
				 (condition_mode << DBCR2_DVC1M_SHIFT));
		}
#endif
#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
	} else if (child->thread.dbcr2 & DBCR2_DAC12MODE) {
		
		return -ENOSPC;
#endif
	} else if ((dbcr_dac(child) & (DBCR_DAC2R | DBCR_DAC2W)) == 0) {
		slot = 2;
		if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_READ)
			dbcr_dac(child) |= DBCR_DAC2R;
		if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_WRITE)
			dbcr_dac(child) |= DBCR_DAC2W;
		child->thread.dac2 = (unsigned long)bp_info->addr;
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
		if (byte_enable) {
			child->thread.dvc2 =
				(unsigned long)bp_info->condition_value;
			child->thread.dbcr2 |=
				((byte_enable << DBCR2_DVC2BE_SHIFT) |
				 (condition_mode << DBCR2_DVC2M_SHIFT));
		}
#endif
	} else
		return -ENOSPC;
	child->thread.dbcr0 |= DBCR0_IDM;
	child->thread.regs->msr |= MSR_DE;

	return slot + 4;
}

static int del_dac(struct task_struct *child, int slot)
{
	if (slot == 1) {
		if ((dbcr_dac(child) & (DBCR_DAC1R | DBCR_DAC1W)) == 0)
			return -ENOENT;

		child->thread.dac1 = 0;
		dbcr_dac(child) &= ~(DBCR_DAC1R | DBCR_DAC1W);
#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
		if (child->thread.dbcr2 & DBCR2_DAC12MODE) {
			child->thread.dac2 = 0;
			child->thread.dbcr2 &= ~DBCR2_DAC12MODE;
		}
		child->thread.dbcr2 &= ~(DBCR2_DVC1M | DBCR2_DVC1BE);
#endif
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
		child->thread.dvc1 = 0;
#endif
	} else if (slot == 2) {
		if ((dbcr_dac(child) & (DBCR_DAC2R | DBCR_DAC2W)) == 0)
			return -ENOENT;

#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
		if (child->thread.dbcr2 & DBCR2_DAC12MODE)
			
			return -EINVAL;
		child->thread.dbcr2 &= ~(DBCR2_DVC2M | DBCR2_DVC2BE);
#endif
#if CONFIG_PPC_ADV_DEBUG_DVCS > 0
		child->thread.dvc2 = 0;
#endif
		child->thread.dac2 = 0;
		dbcr_dac(child) &= ~(DBCR_DAC2R | DBCR_DAC2W);
	} else
		return -EINVAL;

	return 0;
}
#endif 

#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
static int set_dac_range(struct task_struct *child,
			 struct ppc_hw_breakpoint *bp_info)
{
	int mode = bp_info->addr_mode & PPC_BREAKPOINT_MODE_MASK;

	
	if (bp_info->condition_mode)
		return -EINVAL;

	if (bp_info->addr >= TASK_SIZE)
		return -EIO;
	if (mode == PPC_BREAKPOINT_MODE_MASK) {
		if (~((unsigned long)bp_info->addr2) >= TASK_SIZE)
			return -EIO;
	} else {
		if (bp_info->addr2 >= TASK_SIZE)
			return -EIO;
	}

	if (child->thread.dbcr0 &
	    (DBCR0_DAC1R | DBCR0_DAC1W | DBCR0_DAC2R | DBCR0_DAC2W))
		return -ENOSPC;

	if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_READ)
		child->thread.dbcr0 |= (DBCR0_DAC1R | DBCR0_IDM);
	if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_WRITE)
		child->thread.dbcr0 |= (DBCR0_DAC1W | DBCR0_IDM);
	child->thread.dac1 = bp_info->addr;
	child->thread.dac2 = bp_info->addr2;
	if (mode == PPC_BREAKPOINT_MODE_RANGE_INCLUSIVE)
		child->thread.dbcr2  |= DBCR2_DAC12M;
	else if (mode == PPC_BREAKPOINT_MODE_RANGE_EXCLUSIVE)
		child->thread.dbcr2  |= DBCR2_DAC12MX;
	else	
		child->thread.dbcr2  |= DBCR2_DAC12MM;
	child->thread.regs->msr |= MSR_DE;

	return 5;
}
#endif 

static long ppc_set_hwdebug(struct task_struct *child,
		     struct ppc_hw_breakpoint *bp_info)
{
#ifndef CONFIG_PPC_ADV_DEBUG_REGS
	unsigned long dabr;
#endif

	if (bp_info->version != 1)
		return -ENOTSUPP;
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	if ((bp_info->trigger_type == 0) ||
	    (bp_info->trigger_type & ~(PPC_BREAKPOINT_TRIGGER_EXECUTE |
				       PPC_BREAKPOINT_TRIGGER_RW)) ||
	    (bp_info->addr_mode & ~PPC_BREAKPOINT_MODE_MASK) ||
	    (bp_info->condition_mode &
	     ~(PPC_BREAKPOINT_CONDITION_MODE |
	       PPC_BREAKPOINT_CONDITION_BE_ALL)))
		return -EINVAL;
#if CONFIG_PPC_ADV_DEBUG_DVCS == 0
	if (bp_info->condition_mode != PPC_BREAKPOINT_CONDITION_NONE)
		return -EINVAL;
#endif

	if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_EXECUTE) {
		if ((bp_info->trigger_type != PPC_BREAKPOINT_TRIGGER_EXECUTE) ||
		    (bp_info->condition_mode != PPC_BREAKPOINT_CONDITION_NONE))
			return -EINVAL;
		return set_intruction_bp(child, bp_info);
	}
	if (bp_info->addr_mode == PPC_BREAKPOINT_MODE_EXACT)
		return set_dac(child, bp_info);

#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
	return set_dac_range(child, bp_info);
#else
	return -EINVAL;
#endif
#else 
	if ((bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_RW) == 0 ||
	    (bp_info->trigger_type & ~PPC_BREAKPOINT_TRIGGER_RW) != 0 ||
	    bp_info->addr_mode != PPC_BREAKPOINT_MODE_EXACT ||
	    bp_info->condition_mode != PPC_BREAKPOINT_CONDITION_NONE)
		return -EINVAL;

	if (child->thread.dabr)
		return -ENOSPC;

	if ((unsigned long)bp_info->addr >= TASK_SIZE)
		return -EIO;

	dabr = (unsigned long)bp_info->addr & ~7UL;
	dabr |= DABR_TRANSLATION;
	if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_READ)
		dabr |= DABR_DATA_READ;
	if (bp_info->trigger_type & PPC_BREAKPOINT_TRIGGER_WRITE)
		dabr |= DABR_DATA_WRITE;

	child->thread.dabr = dabr;

	return 1;
#endif 
}

static long ppc_del_hwdebug(struct task_struct *child, long addr, long data)
{
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
	int rc;

	if (data <= 4)
		rc = del_instruction_bp(child, (int)data);
	else
		rc = del_dac(child, (int)data - 4);

	if (!rc) {
		if (!DBCR_ACTIVE_EVENTS(child->thread.dbcr0,
					child->thread.dbcr1)) {
			child->thread.dbcr0 &= ~DBCR0_IDM;
			child->thread.regs->msr &= ~MSR_DE;
		}
	}
	return rc;
#else
	if (data != 1)
		return -EINVAL;
	if (child->thread.dabr == 0)
		return -ENOENT;

	child->thread.dabr = 0;

	return 0;
#endif
}

static long arch_ptrace_old(struct task_struct *child, long request,
			    unsigned long addr, unsigned long data)
{
	void __user *datavp = (void __user *) data;

	switch (request) {
	case PPC_PTRACE_GETREGS:	
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_GPR, 0, 32 * sizeof(long),
					   datavp);

	case PPC_PTRACE_SETREGS:	
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_GPR, 0, 32 * sizeof(long),
					     datavp);

	case PPC_PTRACE_GETFPREGS:	
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_FPR, 0, 32 * sizeof(double),
					   datavp);

	case PPC_PTRACE_SETFPREGS:	
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_FPR, 0, 32 * sizeof(double),
					     datavp);
	}

	return -EPERM;
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret = -EPERM;
	void __user *datavp = (void __user *) data;
	unsigned long __user *datalp = datavp;

	switch (request) {
	
	case PTRACE_PEEKUSR: {
		unsigned long index, tmp;

		ret = -EIO;
		
#ifdef CONFIG_PPC32
		index = addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR)
		    || (child->thread.regs == NULL))
#else
		index = addr >> 3;
		if ((addr & 7) || (index > PT_FPSCR))
#endif
			break;

		CHECK_FULL_REGS(child->thread.regs);
		if (index < PT_FPR0) {
			tmp = ptrace_get_reg(child, (int) index);
		} else {
			unsigned int fpidx = index - PT_FPR0;

			flush_fp_to_thread(child);
			if (fpidx < (PT_FPSCR - PT_FPR0))
				tmp = ((unsigned long *)child->thread.fpr)
					[fpidx * TS_FPRWIDTH];
			else
				tmp = child->thread.fpscr.val;
		}
		ret = put_user(tmp, datalp);
		break;
	}

	
	case PTRACE_POKEUSR: {
		unsigned long index;

		ret = -EIO;
		
#ifdef CONFIG_PPC32
		index = addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR)
		    || (child->thread.regs == NULL))
#else
		index = addr >> 3;
		if ((addr & 7) || (index > PT_FPSCR))
#endif
			break;

		CHECK_FULL_REGS(child->thread.regs);
		if (index < PT_FPR0) {
			ret = ptrace_put_reg(child, index, data);
		} else {
			unsigned int fpidx = index - PT_FPR0;

			flush_fp_to_thread(child);
			if (fpidx < (PT_FPSCR - PT_FPR0))
				((unsigned long *)child->thread.fpr)
					[fpidx * TS_FPRWIDTH] = data;
			else
				child->thread.fpscr.val = data;
			ret = 0;
		}
		break;
	}

	case PPC_PTRACE_GETHWDBGINFO: {
		struct ppc_debug_info dbginfo;

		dbginfo.version = 1;
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		dbginfo.num_instruction_bps = CONFIG_PPC_ADV_DEBUG_IACS;
		dbginfo.num_data_bps = CONFIG_PPC_ADV_DEBUG_DACS;
		dbginfo.num_condition_regs = CONFIG_PPC_ADV_DEBUG_DVCS;
		dbginfo.data_bp_alignment = 4;
		dbginfo.sizeof_condition = 4;
		dbginfo.features = PPC_DEBUG_FEATURE_INSN_BP_RANGE |
				   PPC_DEBUG_FEATURE_INSN_BP_MASK;
#ifdef CONFIG_PPC_ADV_DEBUG_DAC_RANGE
		dbginfo.features |=
				   PPC_DEBUG_FEATURE_DATA_BP_RANGE |
				   PPC_DEBUG_FEATURE_DATA_BP_MASK;
#endif
#else 
		dbginfo.num_instruction_bps = 0;
		dbginfo.num_data_bps = 1;
		dbginfo.num_condition_regs = 0;
#ifdef CONFIG_PPC64
		dbginfo.data_bp_alignment = 8;
#else
		dbginfo.data_bp_alignment = 4;
#endif
		dbginfo.sizeof_condition = 0;
		dbginfo.features = 0;
#endif 

		if (!access_ok(VERIFY_WRITE, datavp,
			       sizeof(struct ppc_debug_info)))
			return -EFAULT;
		ret = __copy_to_user(datavp, &dbginfo,
				     sizeof(struct ppc_debug_info)) ?
		      -EFAULT : 0;
		break;
	}

	case PPC_PTRACE_SETHWDEBUG: {
		struct ppc_hw_breakpoint bp_info;

		if (!access_ok(VERIFY_READ, datavp,
			       sizeof(struct ppc_hw_breakpoint)))
			return -EFAULT;
		ret = __copy_from_user(&bp_info, datavp,
				       sizeof(struct ppc_hw_breakpoint)) ?
		      -EFAULT : 0;
		if (!ret)
			ret = ppc_set_hwdebug(child, &bp_info);
		break;
	}

	case PPC_PTRACE_DELHWDEBUG: {
		ret = ppc_del_hwdebug(child, addr, data);
		break;
	}

	case PTRACE_GET_DEBUGREG: {
		ret = -EINVAL;
		
		if (addr > 0)
			break;
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		ret = put_user(child->thread.dac1, datalp);
#else
		ret = put_user(child->thread.dabr, datalp);
#endif
		break;
	}

	case PTRACE_SET_DEBUGREG:
		ret = ptrace_set_debugreg(child, addr, data);
		break;

#ifdef CONFIG_PPC64
	case PTRACE_GETREGS64:
#endif
	case PTRACE_GETREGS:	
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_GPR,
					   0, sizeof(struct pt_regs),
					   datavp);

#ifdef CONFIG_PPC64
	case PTRACE_SETREGS64:
#endif
	case PTRACE_SETREGS:	
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_GPR,
					     0, sizeof(struct pt_regs),
					     datavp);

	case PTRACE_GETFPREGS: 
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_FPR,
					   0, sizeof(elf_fpregset_t),
					   datavp);

	case PTRACE_SETFPREGS: 
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_FPR,
					     0, sizeof(elf_fpregset_t),
					     datavp);

#ifdef CONFIG_ALTIVEC
	case PTRACE_GETVRREGS:
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_VMX,
					   0, (33 * sizeof(vector128) +
					       sizeof(u32)),
					   datavp);

	case PTRACE_SETVRREGS:
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_VMX,
					     0, (33 * sizeof(vector128) +
						 sizeof(u32)),
					     datavp);
#endif
#ifdef CONFIG_VSX
	case PTRACE_GETVSRREGS:
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_VSX,
					   0, 32 * sizeof(double),
					   datavp);

	case PTRACE_SETVSRREGS:
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_VSX,
					     0, 32 * sizeof(double),
					     datavp);
#endif
#ifdef CONFIG_SPE
	case PTRACE_GETEVRREGS:
		
		return copy_regset_to_user(child, &user_ppc_native_view,
					   REGSET_SPE, 0, 35 * sizeof(u32),
					   datavp);

	case PTRACE_SETEVRREGS:
		
		return copy_regset_from_user(child, &user_ppc_native_view,
					     REGSET_SPE, 0, 35 * sizeof(u32),
					     datavp);
#endif

	
	case PPC_PTRACE_GETREGS: 
	case PPC_PTRACE_SETREGS: 
	case PPC_PTRACE_GETFPREGS: 
	case PPC_PTRACE_SETFPREGS: 
		ret = arch_ptrace_old(child, request, addr, data);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}
	return ret;
}

long do_syscall_trace_enter(struct pt_regs *regs)
{
	long ret = 0;

	secure_computing(regs->gpr[0]);

	if (test_thread_flag(TIF_SYSCALL_TRACE) &&
	    tracehook_report_syscall_entry(regs))
		ret = -1L;

	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_enter(regs, regs->gpr[0]);

#ifdef CONFIG_PPC64
	if (!is_32bit_task())
		audit_syscall_entry(AUDIT_ARCH_PPC64,
				    regs->gpr[0],
				    regs->gpr[3], regs->gpr[4],
				    regs->gpr[5], regs->gpr[6]);
	else
#endif
		audit_syscall_entry(AUDIT_ARCH_PPC,
				    regs->gpr[0],
				    regs->gpr[3] & 0xffffffff,
				    regs->gpr[4] & 0xffffffff,
				    regs->gpr[5] & 0xffffffff,
				    regs->gpr[6] & 0xffffffff);

	return ret ?: regs->gpr[0];
}

void do_syscall_trace_leave(struct pt_regs *regs)
{
	int step;

	audit_syscall_exit(regs);

	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_exit(regs, regs->result);

	step = test_thread_flag(TIF_SINGLESTEP);
	if (step || test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, step);
}
