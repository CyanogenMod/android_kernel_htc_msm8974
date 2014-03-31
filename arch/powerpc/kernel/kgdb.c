/*
 * PowerPC backend to the KGDB stub.
 *
 * 1998 (c) Michael AK Tesch (tesch@cs.wisc.edu)
 * Copyright (C) 2003 Timesys Corporation.
 * Copyright (C) 2004-2006 MontaVista Software, Inc.
 * PPC64 Mods (C) 2005 Frank Rowand (frowand@mvista.com)
 * PPC32 support restored by Vitaly Wool <vwool@ru.mvista.com> and
 * Sergei Shtylyov <sshtylyov@ru.mvista.com>
 * Copyright (C) 2007-2008 Wind River Systems, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program as licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kgdb.h>
#include <linux/smp.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <linux/kdebug.h>
#include <asm/current.h>
#include <asm/processor.h>
#include <asm/machdep.h>
#include <asm/debug.h>

static struct hard_trap_info
{
	unsigned int tt;		
	unsigned char signo;		
} hard_trap_info[] = {
	{ 0x0100, 0x02   },		
	{ 0x0200, 0x0b  },		
	{ 0x0300, 0x0b  },		
	{ 0x0400, 0x0b  },		
	{ 0x0500, 0x02   },		
	{ 0x0600, 0x0a   },		
	{ 0x0700, 0x05  },		
	{ 0x0800, 0x08   },		
	{ 0x0900, 0x0e  },		
	{ 0x0c00, 0x14  },		
#if defined(CONFIG_40x) || defined(CONFIG_BOOKE)
	{ 0x2002, 0x05  },		
#if defined(CONFIG_FSL_BOOKE)
	{ 0x2010, 0x08   },		
	{ 0x2020, 0x08   },		
	{ 0x2030, 0x08   },		
	{ 0x2040, 0x08   },		
	{ 0x2050, 0x08   },		
	{ 0x2060, 0x0e   },		
	{ 0x2900, 0x08   },		
	{ 0x3100, 0x0e  },		
	{ 0x3200, 0x02   }, 	
#else 
	{ 0x1000, 0x0e  },		
	{ 0x1010, 0x0e  },		
	{ 0x1020, 0x02   }, 	
	{ 0x2010, 0x08   },		
	{ 0x2020, 0x08   },		
#endif
#else 
	{ 0x0d00, 0x05  },		
#if defined(CONFIG_8xx)
	{ 0x1000, 0x04   },		
#else 
	{ 0x0f00, 0x04   },		
	{ 0x0f20, 0x08   },		
	{ 0x1300, 0x05  }, 	
#if defined(CONFIG_PPC64)
	{ 0x1200, 0x05   },		
	{ 0x1500, 0x04   },		
	{ 0x1600, 0x04   },		
	{ 0x1700, 0x08   },		
	{ 0x1800, 0x04   },		
#else 
	{ 0x1400, 0x02   },		
	{ 0x1600, 0x08   },		
	{ 0x1700, 0x04   },		
	{ 0x2000, 0x05  },		
#endif
#endif
#endif
	{ 0x0000, 0x00 }			
};

static int computeSignal(unsigned int tt)
{
	struct hard_trap_info *ht;

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		if (ht->tt == tt)
			return ht->signo;

	return SIGHUP;		
}

static int kgdb_call_nmi_hook(struct pt_regs *regs)
{
	kgdb_nmicallback(raw_smp_processor_id(), regs);
	return 0;
}

#ifdef CONFIG_SMP
void kgdb_roundup_cpus(unsigned long flags)
{
	smp_send_debugger_break();
}
#endif

static int kgdb_debugger(struct pt_regs *regs)
{
	return !kgdb_handle_exception(1, computeSignal(TRAP(regs)),
				      DIE_OOPS, regs);
}

static int kgdb_handle_breakpoint(struct pt_regs *regs)
{
	if (user_mode(regs))
		return 0;

	if (kgdb_handle_exception(1, SIGTRAP, 0, regs) != 0)
		return 0;

	if (*(u32 *) (regs->nip) == *(u32 *) (&arch_kgdb_ops.gdb_bpt_instr))
		regs->nip += BREAK_INSTR_SIZE;

	return 1;
}

static int kgdb_singlestep(struct pt_regs *regs)
{
	struct thread_info *thread_info, *exception_thread_info;

	if (user_mode(regs))
		return 0;

	thread_info = (struct thread_info *)(regs->gpr[1] & ~(THREAD_SIZE-1));
	exception_thread_info = current_thread_info();

	if (thread_info != exception_thread_info)
		memcpy(exception_thread_info, thread_info, sizeof *thread_info);

	kgdb_handle_exception(0, SIGTRAP, 0, regs);

	if (thread_info != exception_thread_info)
		memcpy(thread_info, exception_thread_info, sizeof *thread_info);

	return 1;
}

static int kgdb_iabr_match(struct pt_regs *regs)
{
	if (user_mode(regs))
		return 0;

	if (kgdb_handle_exception(0, computeSignal(TRAP(regs)), 0, regs) != 0)
		return 0;
	return 1;
}

static int kgdb_dabr_match(struct pt_regs *regs)
{
	if (user_mode(regs))
		return 0;

	if (kgdb_handle_exception(0, computeSignal(TRAP(regs)), 0, regs) != 0)
		return 0;
	return 1;
}

#define PACK64(ptr, src) do { *(ptr++) = (src); } while (0)

#define PACK32(ptr, src) do {          \
	u32 *ptr32;                   \
	ptr32 = (u32 *)ptr;           \
	*(ptr32++) = (src);           \
	ptr = (unsigned long *)ptr32; \
	} while (0)

void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	struct pt_regs *regs = (struct pt_regs *)(p->thread.ksp +
						  STACK_FRAME_OVERHEAD);
	unsigned long *ptr = gdb_regs;
	int reg;

	memset(gdb_regs, 0, NUMREGBYTES);

	
	for (reg = 0; reg < 3; reg++)
		PACK64(ptr, regs->gpr[reg]);

	
	ptr += 11;

	
	for (reg = 14; reg < 32; reg++)
		PACK64(ptr, regs->gpr[reg]);

#ifdef CONFIG_FSL_BOOKE
#ifdef CONFIG_SPE
	for (reg = 0; reg < 32; reg++)
		PACK64(ptr, p->thread.evr[reg]);
#else
	ptr += 32;
#endif
#else
	
	ptr += 32 * 8 / sizeof(long);
#endif

	PACK64(ptr, regs->nip);
	PACK64(ptr, regs->msr);
	PACK32(ptr, regs->ccr);
	PACK64(ptr, regs->link);
	PACK64(ptr, regs->ctr);
	PACK32(ptr, regs->xer);

	BUG_ON((unsigned long)ptr >
	       (unsigned long)(((void *)gdb_regs) + NUMREGBYTES));
}

#define GDB_SIZEOF_REG sizeof(unsigned long)
#define GDB_SIZEOF_REG_U32 sizeof(u32)

#ifdef CONFIG_FSL_BOOKE
#define GDB_SIZEOF_FLOAT_REG sizeof(unsigned long)
#else
#define GDB_SIZEOF_FLOAT_REG sizeof(u64)
#endif

struct dbg_reg_def_t dbg_reg_def[DBG_MAX_REG_NUM] =
{
	{ "r0", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[0]) },
	{ "r1", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[1]) },
	{ "r2", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[2]) },
	{ "r3", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[3]) },
	{ "r4", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[4]) },
	{ "r5", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[5]) },
	{ "r6", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[6]) },
	{ "r7", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[7]) },
	{ "r8", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[8]) },
	{ "r9", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[9]) },
	{ "r10", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[10]) },
	{ "r11", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[11]) },
	{ "r12", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[12]) },
	{ "r13", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[13]) },
	{ "r14", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[14]) },
	{ "r15", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[15]) },
	{ "r16", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[16]) },
	{ "r17", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[17]) },
	{ "r18", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[18]) },
	{ "r19", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[19]) },
	{ "r20", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[20]) },
	{ "r21", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[21]) },
	{ "r22", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[22]) },
	{ "r23", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[23]) },
	{ "r24", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[24]) },
	{ "r25", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[25]) },
	{ "r26", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[26]) },
	{ "r27", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[27]) },
	{ "r28", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[28]) },
	{ "r29", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[29]) },
	{ "r30", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[30]) },
	{ "r31", GDB_SIZEOF_REG, offsetof(struct pt_regs, gpr[31]) },

	{ "f0", GDB_SIZEOF_FLOAT_REG, 0 },
	{ "f1", GDB_SIZEOF_FLOAT_REG, 1 },
	{ "f2", GDB_SIZEOF_FLOAT_REG, 2 },
	{ "f3", GDB_SIZEOF_FLOAT_REG, 3 },
	{ "f4", GDB_SIZEOF_FLOAT_REG, 4 },
	{ "f5", GDB_SIZEOF_FLOAT_REG, 5 },
	{ "f6", GDB_SIZEOF_FLOAT_REG, 6 },
	{ "f7", GDB_SIZEOF_FLOAT_REG, 7 },
	{ "f8", GDB_SIZEOF_FLOAT_REG, 8 },
	{ "f9", GDB_SIZEOF_FLOAT_REG, 9 },
	{ "f10", GDB_SIZEOF_FLOAT_REG, 10 },
	{ "f11", GDB_SIZEOF_FLOAT_REG, 11 },
	{ "f12", GDB_SIZEOF_FLOAT_REG, 12 },
	{ "f13", GDB_SIZEOF_FLOAT_REG, 13 },
	{ "f14", GDB_SIZEOF_FLOAT_REG, 14 },
	{ "f15", GDB_SIZEOF_FLOAT_REG, 15 },
	{ "f16", GDB_SIZEOF_FLOAT_REG, 16 },
	{ "f17", GDB_SIZEOF_FLOAT_REG, 17 },
	{ "f18", GDB_SIZEOF_FLOAT_REG, 18 },
	{ "f19", GDB_SIZEOF_FLOAT_REG, 19 },
	{ "f20", GDB_SIZEOF_FLOAT_REG, 20 },
	{ "f21", GDB_SIZEOF_FLOAT_REG, 21 },
	{ "f22", GDB_SIZEOF_FLOAT_REG, 22 },
	{ "f23", GDB_SIZEOF_FLOAT_REG, 23 },
	{ "f24", GDB_SIZEOF_FLOAT_REG, 24 },
	{ "f25", GDB_SIZEOF_FLOAT_REG, 25 },
	{ "f26", GDB_SIZEOF_FLOAT_REG, 26 },
	{ "f27", GDB_SIZEOF_FLOAT_REG, 27 },
	{ "f28", GDB_SIZEOF_FLOAT_REG, 28 },
	{ "f29", GDB_SIZEOF_FLOAT_REG, 29 },
	{ "f30", GDB_SIZEOF_FLOAT_REG, 30 },
	{ "f31", GDB_SIZEOF_FLOAT_REG, 31 },

	{ "pc", GDB_SIZEOF_REG, offsetof(struct pt_regs, nip) },
	{ "msr", GDB_SIZEOF_REG, offsetof(struct pt_regs, msr) },
	{ "cr", GDB_SIZEOF_REG_U32, offsetof(struct pt_regs, ccr) },
	{ "lr", GDB_SIZEOF_REG, offsetof(struct pt_regs, link) },
	{ "ctr", GDB_SIZEOF_REG_U32, offsetof(struct pt_regs, ctr) },
	{ "xer", GDB_SIZEOF_REG, offsetof(struct pt_regs, xer) },
};

char *dbg_get_reg(int regno, void *mem, struct pt_regs *regs)
{
	if (regno >= DBG_MAX_REG_NUM || regno < 0)
		return NULL;

	if (regno < 32 || regno >= 64)
		
		
		memcpy(mem, (void *)regs + dbg_reg_def[regno].offset,
				dbg_reg_def[regno].size);

	if (regno >= 32 && regno < 64) {
		
#if defined(CONFIG_FSL_BOOKE) && defined(CONFIG_SPE)
		if (current)
			memcpy(mem, &current->thread.evr[regno-32],
					dbg_reg_def[regno].size);
#else
		
		memset(mem, 0, dbg_reg_def[regno].size);
#endif
	}

	return dbg_reg_def[regno].name;
}

int dbg_set_reg(int regno, void *mem, struct pt_regs *regs)
{
	if (regno >= DBG_MAX_REG_NUM || regno < 0)
		return -EINVAL;

	if (regno < 32 || regno >= 64)
		
		
		memcpy((void *)regs + dbg_reg_def[regno].offset, mem,
				dbg_reg_def[regno].size);

	if (regno >= 32 && regno < 64) {
		
#if defined(CONFIG_FSL_BOOKE) && defined(CONFIG_SPE)
		memcpy(&current->thread.evr[regno-32], mem,
				dbg_reg_def[regno].size);
#else
		
		return 0;
#endif
	}

	return 0;
}

void kgdb_arch_set_pc(struct pt_regs *regs, unsigned long pc)
{
	regs->nip = pc;
}

int kgdb_arch_handle_exception(int vector, int signo, int err_code,
			       char *remcom_in_buffer, char *remcom_out_buffer,
			       struct pt_regs *linux_regs)
{
	char *ptr = &remcom_in_buffer[1];
	unsigned long addr;

	switch (remcom_in_buffer[0]) {
	case 's':
	case 'c':
		
		if (kgdb_hex2long(&ptr, &addr))
			linux_regs->nip = addr;

		atomic_set(&kgdb_cpu_doing_single_step, -1);
		
		if (remcom_in_buffer[0] == 's') {
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
			mtspr(SPRN_DBCR0,
			      mfspr(SPRN_DBCR0) | DBCR0_IC | DBCR0_IDM);
			linux_regs->msr |= MSR_DE;
#else
			linux_regs->msr |= MSR_SE;
#endif
			kgdb_single_step = 1;
			atomic_set(&kgdb_cpu_doing_single_step,
				   raw_smp_processor_id());
		}
		return 0;
	}

	return -1;
}

struct kgdb_arch arch_kgdb_ops = {
	.gdb_bpt_instr = {0x7d, 0x82, 0x10, 0x08},
};

static int kgdb_not_implemented(struct pt_regs *regs)
{
	return 0;
}

static void *old__debugger_ipi;
static void *old__debugger;
static void *old__debugger_bpt;
static void *old__debugger_sstep;
static void *old__debugger_iabr_match;
static void *old__debugger_dabr_match;
static void *old__debugger_fault_handler;

int kgdb_arch_init(void)
{
	old__debugger_ipi = __debugger_ipi;
	old__debugger = __debugger;
	old__debugger_bpt = __debugger_bpt;
	old__debugger_sstep = __debugger_sstep;
	old__debugger_iabr_match = __debugger_iabr_match;
	old__debugger_dabr_match = __debugger_dabr_match;
	old__debugger_fault_handler = __debugger_fault_handler;

	__debugger_ipi = kgdb_call_nmi_hook;
	__debugger = kgdb_debugger;
	__debugger_bpt = kgdb_handle_breakpoint;
	__debugger_sstep = kgdb_singlestep;
	__debugger_iabr_match = kgdb_iabr_match;
	__debugger_dabr_match = kgdb_dabr_match;
	__debugger_fault_handler = kgdb_not_implemented;

	return 0;
}

void kgdb_arch_exit(void)
{
	__debugger_ipi = old__debugger_ipi;
	__debugger = old__debugger;
	__debugger_bpt = old__debugger_bpt;
	__debugger_sstep = old__debugger_sstep;
	__debugger_iabr_match = old__debugger_iabr_match;
	__debugger_dabr_match = old__debugger_dabr_match;
	__debugger_fault_handler = old__debugger_fault_handler;
}
