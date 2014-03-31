/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#ifndef _ASM_TILE_PROCESSOR_H
#define _ASM_TILE_PROCESSOR_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/percpu.h>

#include <arch/chip.h>
#include <arch/spr_def.h>

struct task_struct;
struct thread_struct;

typedef struct {
	unsigned long seg;
} mm_segment_t;

void *current_text_addr(void);

#if CHIP_HAS_TILE_DMA()
struct tile_dma_state {
	int enabled;
	unsigned long src;
	unsigned long dest;
	unsigned long strides;
	unsigned long chunk_size;
	unsigned long src_chunk;
	unsigned long dest_chunk;
	unsigned long byte;
	unsigned long status;
};

#define DMA_STATUS_MASK \
  (SPR_DMA_STATUS__RUNNING_MASK | SPR_DMA_STATUS__DONE_MASK)
#endif

struct async_tlb {
	short fault_num;         
	char is_fault;           
	char is_write;           
	unsigned long address;   
};

#ifdef CONFIG_HARDWALL
struct hardwall_info;
#endif

struct thread_struct {
	
	unsigned long  ksp;
	
	unsigned long  pc;
	
	unsigned long  usp0;
	
	pid_t creator_pid;
#if CHIP_HAS_TILE_DMA()
	
	struct tile_dma_state tile_dma_state;
#endif
	
	unsigned long ex_context[2];
	
	unsigned long system_save[4];
	
	unsigned long long interrupt_mask;
	
	unsigned long intctrl_0;
#if CHIP_HAS_PROC_STATUS_SPR()
	
	unsigned long proc_status;
#endif
#if !CHIP_HAS_FIXED_INTVEC_BASE()
	
	unsigned long interrupt_vector_base;
#endif
#if CHIP_HAS_TILE_RTF_HWM()
	
	unsigned long tile_rtf_hwm;
#endif
#if CHIP_HAS_DSTREAM_PF()
	
	unsigned long dstream_pf;
#endif
#ifdef CONFIG_HARDWALL
	
	struct hardwall_info *hardwall;
	
	struct list_head hardwall_list;
#endif
#if CHIP_HAS_TILE_DMA()
	
	struct async_tlb dma_async_tlb;
#endif
#if CHIP_HAS_SN_PROC()
	
	int sn_proc_running;
	
	struct async_tlb sn_async_tlb;
#endif
};

#endif 

#define STACK_TOP_DELTA 8

#ifdef __tilegx__
#define KSTK_PTREGS_GAP  48
#else
#define KSTK_PTREGS_GAP  56
#endif

#ifndef __ASSEMBLY__

#ifdef __tilegx__
#define TASK_SIZE_MAX		(MEM_LOW_END + 1)
#else
#define TASK_SIZE_MAX		PAGE_OFFSET
#endif

#ifdef CONFIG_COMPAT
#define COMPAT_TASK_SIZE	(1UL << 31)
#define TASK_SIZE		((current_thread_info()->status & TS_COMPAT) ?\
				 COMPAT_TASK_SIZE : TASK_SIZE_MAX)
#else
#define TASK_SIZE		TASK_SIZE_MAX
#endif

#define VDSO_BASE		(TASK_SIZE - PAGE_SIZE)

#define STACK_TOP		VDSO_BASE

#define STACK_TOP_MAX		TASK_SIZE_MAX

#define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE / 3))

#define HAVE_ARCH_PICK_MMAP_LAYOUT

#define INIT_THREAD {                                                   \
	.ksp = (unsigned long)init_stack + THREAD_SIZE - STACK_TOP_DELTA, \
	.interrupt_mask = -1ULL                                         \
}

DECLARE_PER_CPU(unsigned long, boot_sp);

DECLARE_PER_CPU(unsigned long, boot_pc);

static inline void start_thread(struct pt_regs *regs,
				unsigned long pc, unsigned long usp)
{
	regs->pc = pc;
	regs->sp = usp;
}

static inline void release_thread(struct task_struct *dead_task)
{
	
}

#define prepare_to_copy(tsk)	do { } while (0)

extern int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

extern int do_work_pending(struct pt_regs *regs, u32 flags);


#define thread_saved_pc(t)   ((t)->thread.pc)

unsigned long get_wchan(struct task_struct *p);

#define task_ksp0(task) ((unsigned long)(task)->stack + THREAD_SIZE)

#define KSTK_TOP(task)	(task_ksp0(task) - STACK_TOP_DELTA)
#define task_pt_regs(task) \
  ((struct pt_regs *)(task_ksp0(task) - KSTK_PTREGS_GAP) - 1)
#define task_sp(task)	(task_pt_regs(task)->sp)
#define task_pc(task)	(task_pt_regs(task)->pc)
#define KSTK_EIP(task)	task_pc(task)
#define KSTK_ESP(task)	task_sp(task)

#ifdef __tilegx__
# define REGFMT "0x%016lx"
#else
# define REGFMT "0x%08lx"
#endif

static inline void cpu_relax(void)
{
	__insn_mfspr(SPR_PASS);
	barrier();
}

struct seq_operations;
extern const struct seq_operations cpuinfo_op;

extern char chip_model[64];

extern int node_controller[];

#if CHIP_HAS_CBOX_HOME_MAP()
extern int hash_default;

extern int kstack_hash;

#define uheap_hash hash_default

#else
#define hash_default 0
#define kstack_hash 0
#define uheap_hash 0
#endif

extern int kdata_huge;

#define ARCH_HAS_PREFETCH
#define prefetch(x) __builtin_prefetch(x)
#define PREFETCH_STRIDE CHIP_L2_LINE_SIZE()

#ifdef __tilegx__
#define prefetch_L1(x) __insn_prefetch_l1_fault((void *)(x))
#else
#define prefetch_L1(x) __insn_prefetch_L1((void *)(x))
#endif

#else 

#define CPU_RELAX       mfspr zero, SPR_PASS

#endif 

#if SPR_EX_CONTEXT_1_1__PL_SHIFT != 0
# error Fix assembly assumptions about PL
#endif

#if SPR_EX_CONTEXT_1_1__PL_SHIFT != SPR_EX_CONTEXT_0_1__PL_SHIFT || \
    SPR_EX_CONTEXT_1_1__PL_RMASK != SPR_EX_CONTEXT_0_1__PL_RMASK || \
    SPR_EX_CONTEXT_1_1__ICS_SHIFT != SPR_EX_CONTEXT_0_1__ICS_SHIFT || \
    SPR_EX_CONTEXT_1_1__ICS_RMASK != SPR_EX_CONTEXT_0_1__ICS_RMASK
# error Fix assumptions that EX1 macros work for both PL0 and PL1
#endif

#define EX1_PL(ex1) \
  (((ex1) >> SPR_EX_CONTEXT_1_1__PL_SHIFT) & SPR_EX_CONTEXT_1_1__PL_RMASK)
#define EX1_ICS(ex1) \
  (((ex1) >> SPR_EX_CONTEXT_1_1__ICS_SHIFT) & SPR_EX_CONTEXT_1_1__ICS_RMASK)
#define PL_ICS_EX1(pl, ics) \
  (((pl) << SPR_EX_CONTEXT_1_1__PL_SHIFT) | \
   ((ics) << SPR_EX_CONTEXT_1_1__ICS_SHIFT))

#define USER_PL 0
#if CONFIG_KERNEL_PL == 2
#define GUEST_PL 1
#endif
#define KERNEL_PL CONFIG_KERNEL_PL

#define CPU_LOG_MASK_VALUE 12
#define CPU_MASK_VALUE ((1 << CPU_LOG_MASK_VALUE) - 1)
#if CONFIG_NR_CPUS > CPU_MASK_VALUE
# error Too many cpus!
#endif
#define raw_smp_processor_id() \
	((int)__insn_mfspr(SPR_SYSTEM_SAVE_K_0) & CPU_MASK_VALUE)
#define get_current_ksp0() \
	(__insn_mfspr(SPR_SYSTEM_SAVE_K_0) & ~CPU_MASK_VALUE)
#define next_current_ksp0(task) ({ \
	unsigned long __ksp0 = task_ksp0(task); \
	int __cpu = raw_smp_processor_id(); \
	BUG_ON(__ksp0 & CPU_MASK_VALUE); \
	__ksp0 | __cpu; \
})

#endif 
