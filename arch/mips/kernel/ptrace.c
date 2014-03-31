/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 Ross Biro
 * Copyright (C) Linus Torvalds
 * Copyright (C) 1994, 95, 96, 97, 98, 2000 Ralf Baechle
 * Copyright (C) 1996 David S. Miller
 * Kevin D. Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999 MIPS Technologies, Inc.
 * Copyright (C) 2000 Ulf Carlsson
 *
 * At this time Linux/MIPS64 only supports syscall tracing, even for 32-bit
 * binaries.
 */
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/smp.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/audit.h>
#include <linux/seccomp.h>

#include <asm/byteorder.h>
#include <asm/cpu.h>
#include <asm/dsp.h>
#include <asm/fpu.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/bootinfo.h>
#include <asm/reg.h>

void ptrace_disable(struct task_struct *child)
{
	
	clear_tsk_thread_flag(child, TIF_LOAD_WATCH);
}

int ptrace_getregs(struct task_struct *child, __s64 __user *data)
{
	struct pt_regs *regs;
	int i;

	if (!access_ok(VERIFY_WRITE, data, 38 * 8))
		return -EIO;

	regs = task_pt_regs(child);

	for (i = 0; i < 32; i++)
		__put_user((long)regs->regs[i], data + i);
	__put_user((long)regs->lo, data + EF_LO - EF_R0);
	__put_user((long)regs->hi, data + EF_HI - EF_R0);
	__put_user((long)regs->cp0_epc, data + EF_CP0_EPC - EF_R0);
	__put_user((long)regs->cp0_badvaddr, data + EF_CP0_BADVADDR - EF_R0);
	__put_user((long)regs->cp0_status, data + EF_CP0_STATUS - EF_R0);
	__put_user((long)regs->cp0_cause, data + EF_CP0_CAUSE - EF_R0);

	return 0;
}

int ptrace_setregs(struct task_struct *child, __s64 __user *data)
{
	struct pt_regs *regs;
	int i;

	if (!access_ok(VERIFY_READ, data, 38 * 8))
		return -EIO;

	regs = task_pt_regs(child);

	for (i = 0; i < 32; i++)
		__get_user(regs->regs[i], data + i);
	__get_user(regs->lo, data + EF_LO - EF_R0);
	__get_user(regs->hi, data + EF_HI - EF_R0);
	__get_user(regs->cp0_epc, data + EF_CP0_EPC - EF_R0);

	/* badvaddr, status, and cause may not be written.  */

	return 0;
}

int ptrace_getfpregs(struct task_struct *child, __u32 __user *data)
{
	int i;
	unsigned int tmp;

	if (!access_ok(VERIFY_WRITE, data, 33 * 8))
		return -EIO;

	if (tsk_used_math(child)) {
		fpureg_t *fregs = get_fpu_regs(child);
		for (i = 0; i < 32; i++)
			__put_user(fregs[i], i + (__u64 __user *) data);
	} else {
		for (i = 0; i < 32; i++)
			__put_user((__u64) -1, i + (__u64 __user *) data);
	}

	__put_user(child->thread.fpu.fcr31, data + 64);

	preempt_disable();
	if (cpu_has_fpu) {
		unsigned int flags;

		if (cpu_has_mipsmt) {
			unsigned int vpflags = dvpe();
			flags = read_c0_status();
			__enable_fpu();
			__asm__ __volatile__("cfc1\t%0,$0" : "=r" (tmp));
			write_c0_status(flags);
			evpe(vpflags);
		} else {
			flags = read_c0_status();
			__enable_fpu();
			__asm__ __volatile__("cfc1\t%0,$0" : "=r" (tmp));
			write_c0_status(flags);
		}
	} else {
		tmp = 0;
	}
	preempt_enable();
	__put_user(tmp, data + 65);

	return 0;
}

int ptrace_setfpregs(struct task_struct *child, __u32 __user *data)
{
	fpureg_t *fregs;
	int i;

	if (!access_ok(VERIFY_READ, data, 33 * 8))
		return -EIO;

	fregs = get_fpu_regs(child);

	for (i = 0; i < 32; i++)
		__get_user(fregs[i], i + (__u64 __user *) data);

	__get_user(child->thread.fpu.fcr31, data + 64);

	/* FIR may not be written.  */

	return 0;
}

int ptrace_get_watch_regs(struct task_struct *child,
			  struct pt_watch_regs __user *addr)
{
	enum pt_watch_style style;
	int i;

	if (!cpu_has_watch || current_cpu_data.watch_reg_use_cnt == 0)
		return -EIO;
	if (!access_ok(VERIFY_WRITE, addr, sizeof(struct pt_watch_regs)))
		return -EIO;

#ifdef CONFIG_32BIT
	style = pt_watch_style_mips32;
#define WATCH_STYLE mips32
#else
	style = pt_watch_style_mips64;
#define WATCH_STYLE mips64
#endif

	__put_user(style, &addr->style);
	__put_user(current_cpu_data.watch_reg_use_cnt,
		   &addr->WATCH_STYLE.num_valid);
	for (i = 0; i < current_cpu_data.watch_reg_use_cnt; i++) {
		__put_user(child->thread.watch.mips3264.watchlo[i],
			   &addr->WATCH_STYLE.watchlo[i]);
		__put_user(child->thread.watch.mips3264.watchhi[i] & 0xfff,
			   &addr->WATCH_STYLE.watchhi[i]);
		__put_user(current_cpu_data.watch_reg_masks[i],
			   &addr->WATCH_STYLE.watch_masks[i]);
	}
	for (; i < 8; i++) {
		__put_user(0, &addr->WATCH_STYLE.watchlo[i]);
		__put_user(0, &addr->WATCH_STYLE.watchhi[i]);
		__put_user(0, &addr->WATCH_STYLE.watch_masks[i]);
	}

	return 0;
}

int ptrace_set_watch_regs(struct task_struct *child,
			  struct pt_watch_regs __user *addr)
{
	int i;
	int watch_active = 0;
	unsigned long lt[NUM_WATCH_REGS];
	u16 ht[NUM_WATCH_REGS];

	if (!cpu_has_watch || current_cpu_data.watch_reg_use_cnt == 0)
		return -EIO;
	if (!access_ok(VERIFY_READ, addr, sizeof(struct pt_watch_regs)))
		return -EIO;
	
	for (i = 0; i < current_cpu_data.watch_reg_use_cnt; i++) {
		__get_user(lt[i], &addr->WATCH_STYLE.watchlo[i]);
#ifdef CONFIG_32BIT
		if (lt[i] & __UA_LIMIT)
			return -EINVAL;
#else
		if (test_tsk_thread_flag(child, TIF_32BIT_ADDR)) {
			if (lt[i] & 0xffffffff80000000UL)
				return -EINVAL;
		} else {
			if (lt[i] & __UA_LIMIT)
				return -EINVAL;
		}
#endif
		__get_user(ht[i], &addr->WATCH_STYLE.watchhi[i]);
		if (ht[i] & ~0xff8)
			return -EINVAL;
	}
	
	for (i = 0; i < current_cpu_data.watch_reg_use_cnt; i++) {
		if (lt[i] & 7)
			watch_active = 1;
		child->thread.watch.mips3264.watchlo[i] = lt[i];
		
		child->thread.watch.mips3264.watchhi[i] = ht[i];
	}

	if (watch_active)
		set_tsk_thread_flag(child, TIF_LOAD_WATCH);
	else
		clear_tsk_thread_flag(child, TIF_LOAD_WATCH);

	return 0;
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret;
	void __user *addrp = (void __user *) addr;
	void __user *datavp = (void __user *) data;
	unsigned long __user *datalp = (void __user *) data;

	switch (request) {
	
	case PTRACE_PEEKTEXT: 
	case PTRACE_PEEKDATA:
		ret = generic_ptrace_peekdata(child, addr, data);
		break;

	
	case PTRACE_PEEKUSR: {
		struct pt_regs *regs;
		unsigned long tmp = 0;

		regs = task_pt_regs(child);
		ret = 0;  

		switch (addr) {
		case 0 ... 31:
			tmp = regs->regs[addr];
			break;
		case FPR_BASE ... FPR_BASE + 31:
			if (tsk_used_math(child)) {
				fpureg_t *fregs = get_fpu_regs(child);

#ifdef CONFIG_32BIT
				if (addr & 1)
					tmp = (unsigned long) (fregs[((addr & ~1) - 32)] >> 32);
				else
					tmp = (unsigned long) (fregs[(addr - 32)] & 0xffffffff);
#endif
#ifdef CONFIG_64BIT
				tmp = fregs[addr - FPR_BASE];
#endif
			} else {
				tmp = -1;	
			}
			break;
		case PC:
			tmp = regs->cp0_epc;
			break;
		case CAUSE:
			tmp = regs->cp0_cause;
			break;
		case BADVADDR:
			tmp = regs->cp0_badvaddr;
			break;
		case MMHI:
			tmp = regs->hi;
			break;
		case MMLO:
			tmp = regs->lo;
			break;
#ifdef CONFIG_CPU_HAS_SMARTMIPS
		case ACX:
			tmp = regs->acx;
			break;
#endif
		case FPC_CSR:
			tmp = child->thread.fpu.fcr31;
			break;
		case FPC_EIR: {	
			unsigned int flags;
#ifdef CONFIG_MIPS_MT_SMTC
			unsigned long irqflags;
			unsigned int mtflags;
#endif 

			preempt_disable();
			if (!cpu_has_fpu) {
				preempt_enable();
				break;
			}

#ifdef CONFIG_MIPS_MT_SMTC
			
			local_irq_save(irqflags);
			mtflags = dmt();
#endif 
			if (cpu_has_mipsmt) {
				unsigned int vpflags = dvpe();
				flags = read_c0_status();
				__enable_fpu();
				__asm__ __volatile__("cfc1\t%0,$0": "=r" (tmp));
				write_c0_status(flags);
				evpe(vpflags);
			} else {
				flags = read_c0_status();
				__enable_fpu();
				__asm__ __volatile__("cfc1\t%0,$0": "=r" (tmp));
				write_c0_status(flags);
			}
#ifdef CONFIG_MIPS_MT_SMTC
			emt(mtflags);
			local_irq_restore(irqflags);
#endif 
			preempt_enable();
			break;
		}
		case DSP_BASE ... DSP_BASE + 5: {
			dspreg_t *dregs;

			if (!cpu_has_dsp) {
				tmp = 0;
				ret = -EIO;
				goto out;
			}
			dregs = __get_dsp_regs(child);
			tmp = (unsigned long) (dregs[addr - DSP_BASE]);
			break;
		}
		case DSP_CONTROL:
			if (!cpu_has_dsp) {
				tmp = 0;
				ret = -EIO;
				goto out;
			}
			tmp = child->thread.dsp.dspcontrol;
			break;
		default:
			tmp = 0;
			ret = -EIO;
			goto out;
		}
		ret = put_user(tmp, datalp);
		break;
	}

	
	case PTRACE_POKETEXT: 
	case PTRACE_POKEDATA:
		ret = generic_ptrace_pokedata(child, addr, data);
		break;

	case PTRACE_POKEUSR: {
		struct pt_regs *regs;
		ret = 0;
		regs = task_pt_regs(child);

		switch (addr) {
		case 0 ... 31:
			regs->regs[addr] = data;
			break;
		case FPR_BASE ... FPR_BASE + 31: {
			fpureg_t *fregs = get_fpu_regs(child);

			if (!tsk_used_math(child)) {
				
				memset(&child->thread.fpu, ~0,
				       sizeof(child->thread.fpu));
				child->thread.fpu.fcr31 = 0;
			}
#ifdef CONFIG_32BIT
			if (addr & 1) {
				fregs[(addr & ~1) - FPR_BASE] &= 0xffffffff;
				fregs[(addr & ~1) - FPR_BASE] |= ((unsigned long long) data) << 32;
			} else {
				fregs[addr - FPR_BASE] &= ~0xffffffffLL;
				fregs[addr - FPR_BASE] |= data;
			}
#endif
#ifdef CONFIG_64BIT
			fregs[addr - FPR_BASE] = data;
#endif
			break;
		}
		case PC:
			regs->cp0_epc = data;
			break;
		case MMHI:
			regs->hi = data;
			break;
		case MMLO:
			regs->lo = data;
			break;
#ifdef CONFIG_CPU_HAS_SMARTMIPS
		case ACX:
			regs->acx = data;
			break;
#endif
		case FPC_CSR:
			child->thread.fpu.fcr31 = data;
			break;
		case DSP_BASE ... DSP_BASE + 5: {
			dspreg_t *dregs;

			if (!cpu_has_dsp) {
				ret = -EIO;
				break;
			}

			dregs = __get_dsp_regs(child);
			dregs[addr - DSP_BASE] = data;
			break;
		}
		case DSP_CONTROL:
			if (!cpu_has_dsp) {
				ret = -EIO;
				break;
			}
			child->thread.dsp.dspcontrol = data;
			break;
		default:
			
			ret = -EIO;
			break;
		}
		break;
		}

	case PTRACE_GETREGS:
		ret = ptrace_getregs(child, datavp);
		break;

	case PTRACE_SETREGS:
		ret = ptrace_setregs(child, datavp);
		break;

	case PTRACE_GETFPREGS:
		ret = ptrace_getfpregs(child, datavp);
		break;

	case PTRACE_SETFPREGS:
		ret = ptrace_setfpregs(child, datavp);
		break;

	case PTRACE_GET_THREAD_AREA:
		ret = put_user(task_thread_info(child)->tp_value, datalp);
		break;

	case PTRACE_GET_WATCH_REGS:
		ret = ptrace_get_watch_regs(child, addrp);
		break;

	case PTRACE_SET_WATCH_REGS:
		ret = ptrace_set_watch_regs(child, addrp);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}
 out:
	return ret;
}

static inline int audit_arch(void)
{
	int arch = EM_MIPS;
#ifdef CONFIG_64BIT
	arch |=  __AUDIT_ARCH_64BIT;
#endif
#if defined(__LITTLE_ENDIAN)
	arch |=  __AUDIT_ARCH_LE;
#endif
	return arch;
}

asmlinkage void syscall_trace_enter(struct pt_regs *regs)
{
	
	secure_computing(regs->regs[2]);

	if (!(current->ptrace & PT_PTRACED))
		goto out;

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		goto out;

	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD) ?
	                         0x80 : 0));

	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}

out:
	audit_syscall_entry(audit_arch(), regs->regs[2],
			    regs->regs[4], regs->regs[5],
			    regs->regs[6], regs->regs[7]);
}

asmlinkage void syscall_trace_leave(struct pt_regs *regs)
{
	audit_syscall_exit(regs);

	if (!(current->ptrace & PT_PTRACED))
		return;

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;

	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD) ?
	                         0x80 : 0));

	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
