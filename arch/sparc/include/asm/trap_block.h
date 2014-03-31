#ifndef _SPARC_TRAP_BLOCK_H
#define _SPARC_TRAP_BLOCK_H

#include <asm/hypervisor.h>
#include <asm/asi.h>

#ifndef __ASSEMBLY__


struct thread_info;
struct trap_per_cpu {
	struct thread_info	*thread;
	unsigned long		pgd_paddr;
	unsigned long		cpu_mondo_pa;
	unsigned long		dev_mondo_pa;

	unsigned long		resum_mondo_pa;
	unsigned long		resum_kernel_buf_pa;
	unsigned long		nonresum_mondo_pa;
	unsigned long		nonresum_kernel_buf_pa;

	struct hv_fault_status	fault_info;

	unsigned long		cpu_mondo_block_pa;
	unsigned long		cpu_list_pa;
	unsigned long		tsb_huge;
	unsigned long		tsb_huge_temp;

	unsigned long		irq_worklist_pa;
	unsigned int		cpu_mondo_qmask;
	unsigned int		dev_mondo_qmask;
	unsigned int		resum_qmask;
	unsigned int		nonresum_qmask;
	unsigned long		__per_cpu_base;
} __attribute__((aligned(64)));
extern struct trap_per_cpu trap_block[NR_CPUS];
extern void init_cur_cpu_trap(struct thread_info *);
extern void setup_tba(void);
extern int ncpus_probed;

extern unsigned long real_hard_smp_processor_id(void);

struct cpuid_patch_entry {
	unsigned int	addr;
	unsigned int	cheetah_safari[4];
	unsigned int	cheetah_jbus[4];
	unsigned int	starfire[4];
	unsigned int	sun4v[4];
};
extern struct cpuid_patch_entry __cpuid_patch, __cpuid_patch_end;

struct sun4v_1insn_patch_entry {
	unsigned int	addr;
	unsigned int	insn;
};
extern struct sun4v_1insn_patch_entry __sun4v_1insn_patch,
	__sun4v_1insn_patch_end;

struct sun4v_2insn_patch_entry {
	unsigned int	addr;
	unsigned int	insns[2];
};
extern struct sun4v_2insn_patch_entry __sun4v_2insn_patch,
	__sun4v_2insn_patch_end;


#endif 

#define TRAP_PER_CPU_THREAD		0x00
#define TRAP_PER_CPU_PGD_PADDR		0x08
#define TRAP_PER_CPU_CPU_MONDO_PA	0x10
#define TRAP_PER_CPU_DEV_MONDO_PA	0x18
#define TRAP_PER_CPU_RESUM_MONDO_PA	0x20
#define TRAP_PER_CPU_RESUM_KBUF_PA	0x28
#define TRAP_PER_CPU_NONRESUM_MONDO_PA	0x30
#define TRAP_PER_CPU_NONRESUM_KBUF_PA	0x38
#define TRAP_PER_CPU_FAULT_INFO		0x40
#define TRAP_PER_CPU_CPU_MONDO_BLOCK_PA	0xc0
#define TRAP_PER_CPU_CPU_LIST_PA	0xc8
#define TRAP_PER_CPU_TSB_HUGE		0xd0
#define TRAP_PER_CPU_TSB_HUGE_TEMP	0xd8
#define TRAP_PER_CPU_IRQ_WORKLIST_PA	0xe0
#define TRAP_PER_CPU_CPU_MONDO_QMASK	0xe8
#define TRAP_PER_CPU_DEV_MONDO_QMASK	0xec
#define TRAP_PER_CPU_RESUM_QMASK	0xf0
#define TRAP_PER_CPU_NONRESUM_QMASK	0xf4
#define TRAP_PER_CPU_PER_CPU_BASE	0xf8

#define TRAP_BLOCK_SZ_SHIFT		8

#include <asm/scratchpad.h>

#define __GET_CPUID(REG)				\
		\
661:	ldxa		[%g0] ASI_UPA_CONFIG, REG;	\
	srlx		REG, 17, REG;			\
	 and		REG, 0x1f, REG;			\
	nop;						\
	.section	.cpuid_patch, "ax";		\
				\
	.word		661b;				\
			\
	ldxa		[%g0] ASI_SAFARI_CONFIG, REG;	\
	srlx		REG, 17, REG;			\
	and		REG, 0x3ff, REG;		\
	nop;						\
			\
	ldxa		[%g0] ASI_JBUS_CONFIG, REG;	\
	srlx		REG, 17, REG;			\
	and		REG, 0x1f, REG;			\
	nop;						\
				\
	sethi		%hi(0x1fff40000d0 >> 9), REG;	\
	sllx		REG, 9, REG;			\
	or		REG, 0xd0, REG;			\
	lduwa		[REG] ASI_PHYS_BYPASS_EC_E, REG;\
				\
	mov		SCRATCHPAD_CPUID, REG;		\
	ldxa		[REG] ASI_SCRATCHPAD, REG;	\
	nop;						\
	nop;						\
	.previous;

#ifdef CONFIG_SMP

#define TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	__GET_CPUID(TMP)			\
	sethi	%hi(trap_block), DEST;		\
	sllx	TMP, TRAP_BLOCK_SZ_SHIFT, TMP;	\
	or	DEST, %lo(trap_block), DEST;	\
	add	DEST, TMP, DEST;		\

#define TRAP_LOAD_PGD_PHYS(DEST, TMP)		\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	ldx	[DEST + TRAP_PER_CPU_PGD_PADDR], DEST;

#define TRAP_LOAD_IRQ_WORK_PA(DEST, TMP)	\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	add	DEST, TRAP_PER_CPU_IRQ_WORKLIST_PA, DEST;

#define TRAP_LOAD_THREAD_REG(DEST, TMP)		\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	ldx	[DEST + TRAP_PER_CPU_THREAD], DEST;

#define LOAD_PER_CPU_BASE(DEST, THR, REG1, REG2, REG3)	\
	lduh	[THR + TI_CPU], REG1;			\
	sethi	%hi(trap_block), REG2;			\
	sllx	REG1, TRAP_BLOCK_SZ_SHIFT, REG1;	\
	or	REG2, %lo(trap_block), REG2;		\
	add	REG2, REG1, REG2;			\
	ldx	[REG2 + TRAP_PER_CPU_PER_CPU_BASE], DEST;

#else

#define TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	sethi	%hi(trap_block), DEST;		\
	or	DEST, %lo(trap_block), DEST;	\

#define TRAP_LOAD_PGD_PHYS(DEST, TMP)		\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	ldx	[DEST + TRAP_PER_CPU_PGD_PADDR], DEST;

#define TRAP_LOAD_IRQ_WORK_PA(DEST, TMP)	\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	add	DEST, TRAP_PER_CPU_IRQ_WORKLIST_PA, DEST;

#define TRAP_LOAD_THREAD_REG(DEST, TMP)		\
	TRAP_LOAD_TRAP_BLOCK(DEST, TMP)		\
	ldx	[DEST + TRAP_PER_CPU_THREAD], DEST;

#define LOAD_PER_CPU_BASE(DEST, THR, REG1, REG2, REG3)

#endif 

#endif 
