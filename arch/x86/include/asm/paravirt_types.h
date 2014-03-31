#ifndef _ASM_X86_PARAVIRT_TYPES_H
#define _ASM_X86_PARAVIRT_TYPES_H

#define CLBR_NONE 0
#define CLBR_EAX  (1 << 0)
#define CLBR_ECX  (1 << 1)
#define CLBR_EDX  (1 << 2)
#define CLBR_EDI  (1 << 3)

#ifdef CONFIG_X86_32
#define CLBR_ANY  ((1 << 4) - 1)

#define CLBR_ARG_REGS	(CLBR_EAX | CLBR_EDX | CLBR_ECX)
#define CLBR_RET_REG	(CLBR_EAX | CLBR_EDX)
#define CLBR_SCRATCH	(0)
#else
#define CLBR_RAX  CLBR_EAX
#define CLBR_RCX  CLBR_ECX
#define CLBR_RDX  CLBR_EDX
#define CLBR_RDI  CLBR_EDI
#define CLBR_RSI  (1 << 4)
#define CLBR_R8   (1 << 5)
#define CLBR_R9   (1 << 6)
#define CLBR_R10  (1 << 7)
#define CLBR_R11  (1 << 8)

#define CLBR_ANY  ((1 << 9) - 1)

#define CLBR_ARG_REGS	(CLBR_RDI | CLBR_RSI | CLBR_RDX | \
			 CLBR_RCX | CLBR_R8 | CLBR_R9)
#define CLBR_RET_REG	(CLBR_RAX)
#define CLBR_SCRATCH	(CLBR_R10 | CLBR_R11)

#endif 

#define CLBR_CALLEE_SAVE ((CLBR_ARG_REGS | CLBR_SCRATCH) & ~CLBR_RET_REG)

#ifndef __ASSEMBLY__

#include <asm/desc_defs.h>
#include <asm/kmap_types.h>
#include <asm/pgtable_types.h>

struct page;
struct thread_struct;
struct desc_ptr;
struct tss_struct;
struct mm_struct;
struct desc_struct;
struct task_struct;
struct cpumask;

struct paravirt_callee_save {
	void *func;
};

struct pv_info {
	unsigned int kernel_rpl;
	int shared_kernel_pmd;

#ifdef CONFIG_X86_64
	u16 extra_user_64bit_cs;  
#endif

	int paravirt_enabled;
	const char *name;
};

struct pv_init_ops {
	unsigned (*patch)(u8 type, u16 clobber, void *insnbuf,
			  unsigned long addr, unsigned len);
};


struct pv_lazy_ops {
	
	void (*enter)(void);
	void (*leave)(void);
};

struct pv_time_ops {
	unsigned long long (*sched_clock)(void);
	unsigned long long (*steal_clock)(int cpu);
	unsigned long (*get_tsc_khz)(void);
};

struct pv_cpu_ops {
	
	unsigned long (*get_debugreg)(int regno);
	void (*set_debugreg)(int regno, unsigned long value);

	void (*clts)(void);

	unsigned long (*read_cr0)(void);
	void (*write_cr0)(unsigned long);

	unsigned long (*read_cr4_safe)(void);
	unsigned long (*read_cr4)(void);
	void (*write_cr4)(unsigned long);

#ifdef CONFIG_X86_64
	unsigned long (*read_cr8)(void);
	void (*write_cr8)(unsigned long);
#endif

	
	void (*load_tr_desc)(void);
	void (*load_gdt)(const struct desc_ptr *);
	void (*load_idt)(const struct desc_ptr *);
	void (*store_gdt)(struct desc_ptr *);
	void (*store_idt)(struct desc_ptr *);
	void (*set_ldt)(const void *desc, unsigned entries);
	unsigned long (*store_tr)(void);
	void (*load_tls)(struct thread_struct *t, unsigned int cpu);
#ifdef CONFIG_X86_64
	void (*load_gs_index)(unsigned int idx);
#endif
	void (*write_ldt_entry)(struct desc_struct *ldt, int entrynum,
				const void *desc);
	void (*write_gdt_entry)(struct desc_struct *,
				int entrynum, const void *desc, int size);
	void (*write_idt_entry)(gate_desc *,
				int entrynum, const gate_desc *gate);
	void (*alloc_ldt)(struct desc_struct *ldt, unsigned entries);
	void (*free_ldt)(struct desc_struct *ldt, unsigned entries);

	void (*load_sp0)(struct tss_struct *tss, struct thread_struct *t);

	void (*set_iopl_mask)(unsigned mask);

	void (*wbinvd)(void);
	void (*io_delay)(void);

	
	void (*cpuid)(unsigned int *eax, unsigned int *ebx,
		      unsigned int *ecx, unsigned int *edx);

	u64 (*read_msr)(unsigned int msr, int *err);
	int (*rdmsr_regs)(u32 *regs);
	int (*write_msr)(unsigned int msr, unsigned low, unsigned high);
	int (*wrmsr_regs)(u32 *regs);

	u64 (*read_tsc)(void);
	u64 (*read_pmc)(int counter);
	unsigned long long (*read_tscp)(unsigned int *aux);

	void (*irq_enable_sysexit)(void);

	void (*usergs_sysret64)(void);

	void (*usergs_sysret32)(void);

	void (*iret)(void);

	void (*swapgs)(void);

	void (*start_context_switch)(struct task_struct *prev);
	void (*end_context_switch)(struct task_struct *next);
};

struct pv_irq_ops {
	struct paravirt_callee_save save_fl;
	struct paravirt_callee_save restore_fl;
	struct paravirt_callee_save irq_disable;
	struct paravirt_callee_save irq_enable;

	void (*safe_halt)(void);
	void (*halt)(void);

#ifdef CONFIG_X86_64
	void (*adjust_exception_frame)(void);
#endif
};

struct pv_apic_ops {
#ifdef CONFIG_X86_LOCAL_APIC
	void (*startup_ipi_hook)(int phys_apicid,
				 unsigned long start_eip,
				 unsigned long start_esp);
#endif
};

struct pv_mmu_ops {
	unsigned long (*read_cr2)(void);
	void (*write_cr2)(unsigned long);

	unsigned long (*read_cr3)(void);
	void (*write_cr3)(unsigned long);

	void (*activate_mm)(struct mm_struct *prev,
			    struct mm_struct *next);
	void (*dup_mmap)(struct mm_struct *oldmm,
			 struct mm_struct *mm);
	void (*exit_mmap)(struct mm_struct *mm);


	
	void (*flush_tlb_user)(void);
	void (*flush_tlb_kernel)(void);
	void (*flush_tlb_single)(unsigned long addr);
	void (*flush_tlb_others)(const struct cpumask *cpus,
				 struct mm_struct *mm,
				 unsigned long va);

	
	int  (*pgd_alloc)(struct mm_struct *mm);
	void (*pgd_free)(struct mm_struct *mm, pgd_t *pgd);

	void (*alloc_pte)(struct mm_struct *mm, unsigned long pfn);
	void (*alloc_pmd)(struct mm_struct *mm, unsigned long pfn);
	void (*alloc_pud)(struct mm_struct *mm, unsigned long pfn);
	void (*release_pte)(unsigned long pfn);
	void (*release_pmd)(unsigned long pfn);
	void (*release_pud)(unsigned long pfn);

	
	void (*set_pte)(pte_t *ptep, pte_t pteval);
	void (*set_pte_at)(struct mm_struct *mm, unsigned long addr,
			   pte_t *ptep, pte_t pteval);
	void (*set_pmd)(pmd_t *pmdp, pmd_t pmdval);
	void (*set_pmd_at)(struct mm_struct *mm, unsigned long addr,
			   pmd_t *pmdp, pmd_t pmdval);
	void (*pte_update)(struct mm_struct *mm, unsigned long addr,
			   pte_t *ptep);
	void (*pte_update_defer)(struct mm_struct *mm,
				 unsigned long addr, pte_t *ptep);
	void (*pmd_update)(struct mm_struct *mm, unsigned long addr,
			   pmd_t *pmdp);
	void (*pmd_update_defer)(struct mm_struct *mm,
				 unsigned long addr, pmd_t *pmdp);

	pte_t (*ptep_modify_prot_start)(struct mm_struct *mm, unsigned long addr,
					pte_t *ptep);
	void (*ptep_modify_prot_commit)(struct mm_struct *mm, unsigned long addr,
					pte_t *ptep, pte_t pte);

	struct paravirt_callee_save pte_val;
	struct paravirt_callee_save make_pte;

	struct paravirt_callee_save pgd_val;
	struct paravirt_callee_save make_pgd;

#if PAGETABLE_LEVELS >= 3
#ifdef CONFIG_X86_PAE
	void (*set_pte_atomic)(pte_t *ptep, pte_t pteval);
	void (*pte_clear)(struct mm_struct *mm, unsigned long addr,
			  pte_t *ptep);
	void (*pmd_clear)(pmd_t *pmdp);

#endif	

	void (*set_pud)(pud_t *pudp, pud_t pudval);

	struct paravirt_callee_save pmd_val;
	struct paravirt_callee_save make_pmd;

#if PAGETABLE_LEVELS == 4
	struct paravirt_callee_save pud_val;
	struct paravirt_callee_save make_pud;

	void (*set_pgd)(pgd_t *pudp, pgd_t pgdval);
#endif	
#endif	

	struct pv_lazy_ops lazy_mode;

	

	void (*set_fixmap)(unsigned  idx,
			   phys_addr_t phys, pgprot_t flags);
};

struct arch_spinlock;
struct pv_lock_ops {
	int (*spin_is_locked)(struct arch_spinlock *lock);
	int (*spin_is_contended)(struct arch_spinlock *lock);
	void (*spin_lock)(struct arch_spinlock *lock);
	void (*spin_lock_flags)(struct arch_spinlock *lock, unsigned long flags);
	int (*spin_trylock)(struct arch_spinlock *lock);
	void (*spin_unlock)(struct arch_spinlock *lock);
};

struct paravirt_patch_template {
	struct pv_init_ops pv_init_ops;
	struct pv_time_ops pv_time_ops;
	struct pv_cpu_ops pv_cpu_ops;
	struct pv_irq_ops pv_irq_ops;
	struct pv_apic_ops pv_apic_ops;
	struct pv_mmu_ops pv_mmu_ops;
	struct pv_lock_ops pv_lock_ops;
};

extern struct pv_info pv_info;
extern struct pv_init_ops pv_init_ops;
extern struct pv_time_ops pv_time_ops;
extern struct pv_cpu_ops pv_cpu_ops;
extern struct pv_irq_ops pv_irq_ops;
extern struct pv_apic_ops pv_apic_ops;
extern struct pv_mmu_ops pv_mmu_ops;
extern struct pv_lock_ops pv_lock_ops;

#define PARAVIRT_PATCH(x)					\
	(offsetof(struct paravirt_patch_template, x) / sizeof(void *))

#define paravirt_type(op)				\
	[paravirt_typenum] "i" (PARAVIRT_PATCH(op)),	\
	[paravirt_opptr] "i" (&(op))
#define paravirt_clobber(clobber)		\
	[paravirt_clobber] "i" (clobber)

#define _paravirt_alt(insn_string, type, clobber)	\
	"771:\n\t" insn_string "\n" "772:\n"		\
	".pushsection .parainstructions,\"a\"\n"	\
	_ASM_ALIGN "\n"					\
	_ASM_PTR " 771b\n"				\
	"  .byte " type "\n"				\
	"  .byte 772b-771b\n"				\
	"  .short " clobber "\n"			\
	".popsection\n"

#define paravirt_alt(insn_string)					\
	_paravirt_alt(insn_string, "%c[paravirt_typenum]", "%c[paravirt_clobber]")

#define DEF_NATIVE(ops, name, code) 					\
	extern const char start_##ops##_##name[], end_##ops##_##name[];	\
	asm("start_" #ops "_" #name ": " code "; end_" #ops "_" #name ":")

unsigned paravirt_patch_nop(void);
unsigned paravirt_patch_ident_32(void *insnbuf, unsigned len);
unsigned paravirt_patch_ident_64(void *insnbuf, unsigned len);
unsigned paravirt_patch_ignore(unsigned len);
unsigned paravirt_patch_call(void *insnbuf,
			     const void *target, u16 tgt_clobbers,
			     unsigned long addr, u16 site_clobbers,
			     unsigned len);
unsigned paravirt_patch_jmp(void *insnbuf, const void *target,
			    unsigned long addr, unsigned len);
unsigned paravirt_patch_default(u8 type, u16 clobbers, void *insnbuf,
				unsigned long addr, unsigned len);

unsigned paravirt_patch_insns(void *insnbuf, unsigned len,
			      const char *start, const char *end);

unsigned native_patch(u8 type, u16 clobbers, void *ibuf,
		      unsigned long addr, unsigned len);

int paravirt_disable_iospace(void);

#define PARAVIRT_CALL	"call *%c[paravirt_opptr];"

#ifdef CONFIG_X86_32
#define PVOP_VCALL_ARGS				\
	unsigned long __eax = __eax, __edx = __edx, __ecx = __ecx
#define PVOP_CALL_ARGS			PVOP_VCALL_ARGS

#define PVOP_CALL_ARG1(x)		"a" ((unsigned long)(x))
#define PVOP_CALL_ARG2(x)		"d" ((unsigned long)(x))
#define PVOP_CALL_ARG3(x)		"c" ((unsigned long)(x))

#define PVOP_VCALL_CLOBBERS		"=a" (__eax), "=d" (__edx),	\
					"=c" (__ecx)
#define PVOP_CALL_CLOBBERS		PVOP_VCALL_CLOBBERS

#define PVOP_VCALLEE_CLOBBERS		"=a" (__eax), "=d" (__edx)
#define PVOP_CALLEE_CLOBBERS		PVOP_VCALLEE_CLOBBERS

#define EXTRA_CLOBBERS
#define VEXTRA_CLOBBERS
#else  
#define PVOP_VCALL_ARGS					\
	unsigned long __edi = __edi, __esi = __esi,	\
		__edx = __edx, __ecx = __ecx, __eax = __eax
#define PVOP_CALL_ARGS		PVOP_VCALL_ARGS

#define PVOP_CALL_ARG1(x)		"D" ((unsigned long)(x))
#define PVOP_CALL_ARG2(x)		"S" ((unsigned long)(x))
#define PVOP_CALL_ARG3(x)		"d" ((unsigned long)(x))
#define PVOP_CALL_ARG4(x)		"c" ((unsigned long)(x))

#define PVOP_VCALL_CLOBBERS	"=D" (__edi),				\
				"=S" (__esi), "=d" (__edx),		\
				"=c" (__ecx)
#define PVOP_CALL_CLOBBERS	PVOP_VCALL_CLOBBERS, "=a" (__eax)

#define PVOP_VCALLEE_CLOBBERS	"=a" (__eax)
#define PVOP_CALLEE_CLOBBERS	PVOP_VCALLEE_CLOBBERS

#define EXTRA_CLOBBERS	 , "r8", "r9", "r10", "r11"
#define VEXTRA_CLOBBERS	 , "rax", "r8", "r9", "r10", "r11"
#endif	

#ifdef CONFIG_PARAVIRT_DEBUG
#define PVOP_TEST_NULL(op)	BUG_ON(op == NULL)
#else
#define PVOP_TEST_NULL(op)	((void)op)
#endif

#define ____PVOP_CALL(rettype, op, clbr, call_clbr, extra_clbr,		\
		      pre, post, ...)					\
	({								\
		rettype __ret;						\
		PVOP_CALL_ARGS;						\
		PVOP_TEST_NULL(op);					\
			\
				\
		if (sizeof(rettype) > sizeof(unsigned long)) {		\
			asm volatile(pre				\
				     paravirt_alt(PARAVIRT_CALL)	\
				     post				\
				     : call_clbr			\
				     : paravirt_type(op),		\
				       paravirt_clobber(clbr),		\
				       ##__VA_ARGS__			\
				     : "memory", "cc" extra_clbr);	\
			__ret = (rettype)((((u64)__edx) << 32) | __eax); \
		} else {						\
			asm volatile(pre				\
				     paravirt_alt(PARAVIRT_CALL)	\
				     post				\
				     : call_clbr			\
				     : paravirt_type(op),		\
				       paravirt_clobber(clbr),		\
				       ##__VA_ARGS__			\
				     : "memory", "cc" extra_clbr);	\
			__ret = (rettype)__eax;				\
		}							\
		__ret;							\
	})

#define __PVOP_CALL(rettype, op, pre, post, ...)			\
	____PVOP_CALL(rettype, op, CLBR_ANY, PVOP_CALL_CLOBBERS,	\
		      EXTRA_CLOBBERS, pre, post, ##__VA_ARGS__)

#define __PVOP_CALLEESAVE(rettype, op, pre, post, ...)			\
	____PVOP_CALL(rettype, op.func, CLBR_RET_REG,			\
		      PVOP_CALLEE_CLOBBERS, ,				\
		      pre, post, ##__VA_ARGS__)


#define ____PVOP_VCALL(op, clbr, call_clbr, extra_clbr, pre, post, ...)	\
	({								\
		PVOP_VCALL_ARGS;					\
		PVOP_TEST_NULL(op);					\
		asm volatile(pre					\
			     paravirt_alt(PARAVIRT_CALL)		\
			     post					\
			     : call_clbr				\
			     : paravirt_type(op),			\
			       paravirt_clobber(clbr),			\
			       ##__VA_ARGS__				\
			     : "memory", "cc" extra_clbr);		\
	})

#define __PVOP_VCALL(op, pre, post, ...)				\
	____PVOP_VCALL(op, CLBR_ANY, PVOP_VCALL_CLOBBERS,		\
		       VEXTRA_CLOBBERS,					\
		       pre, post, ##__VA_ARGS__)

#define __PVOP_VCALLEESAVE(op, pre, post, ...)				\
	____PVOP_VCALL(op.func, CLBR_RET_REG,				\
		      PVOP_VCALLEE_CLOBBERS, ,				\
		      pre, post, ##__VA_ARGS__)



#define PVOP_CALL0(rettype, op)						\
	__PVOP_CALL(rettype, op, "", "")
#define PVOP_VCALL0(op)							\
	__PVOP_VCALL(op, "", "")

#define PVOP_CALLEE0(rettype, op)					\
	__PVOP_CALLEESAVE(rettype, op, "", "")
#define PVOP_VCALLEE0(op)						\
	__PVOP_VCALLEESAVE(op, "", "")


#define PVOP_CALL1(rettype, op, arg1)					\
	__PVOP_CALL(rettype, op, "", "", PVOP_CALL_ARG1(arg1))
#define PVOP_VCALL1(op, arg1)						\
	__PVOP_VCALL(op, "", "", PVOP_CALL_ARG1(arg1))

#define PVOP_CALLEE1(rettype, op, arg1)					\
	__PVOP_CALLEESAVE(rettype, op, "", "", PVOP_CALL_ARG1(arg1))
#define PVOP_VCALLEE1(op, arg1)						\
	__PVOP_VCALLEESAVE(op, "", "", PVOP_CALL_ARG1(arg1))


#define PVOP_CALL2(rettype, op, arg1, arg2)				\
	__PVOP_CALL(rettype, op, "", "", PVOP_CALL_ARG1(arg1),		\
		    PVOP_CALL_ARG2(arg2))
#define PVOP_VCALL2(op, arg1, arg2)					\
	__PVOP_VCALL(op, "", "", PVOP_CALL_ARG1(arg1),			\
		     PVOP_CALL_ARG2(arg2))

#define PVOP_CALLEE2(rettype, op, arg1, arg2)				\
	__PVOP_CALLEESAVE(rettype, op, "", "", PVOP_CALL_ARG1(arg1),	\
			  PVOP_CALL_ARG2(arg2))
#define PVOP_VCALLEE2(op, arg1, arg2)					\
	__PVOP_VCALLEESAVE(op, "", "", PVOP_CALL_ARG1(arg1),		\
			   PVOP_CALL_ARG2(arg2))


#define PVOP_CALL3(rettype, op, arg1, arg2, arg3)			\
	__PVOP_CALL(rettype, op, "", "", PVOP_CALL_ARG1(arg1),		\
		    PVOP_CALL_ARG2(arg2), PVOP_CALL_ARG3(arg3))
#define PVOP_VCALL3(op, arg1, arg2, arg3)				\
	__PVOP_VCALL(op, "", "", PVOP_CALL_ARG1(arg1),			\
		     PVOP_CALL_ARG2(arg2), PVOP_CALL_ARG3(arg3))

#ifdef CONFIG_X86_32
#define PVOP_CALL4(rettype, op, arg1, arg2, arg3, arg4)			\
	__PVOP_CALL(rettype, op,					\
		    "push %[_arg4];", "lea 4(%%esp),%%esp;",		\
		    PVOP_CALL_ARG1(arg1), PVOP_CALL_ARG2(arg2),		\
		    PVOP_CALL_ARG3(arg3), [_arg4] "mr" ((u32)(arg4)))
#define PVOP_VCALL4(op, arg1, arg2, arg3, arg4)				\
	__PVOP_VCALL(op,						\
		    "push %[_arg4];", "lea 4(%%esp),%%esp;",		\
		    "0" ((u32)(arg1)), "1" ((u32)(arg2)),		\
		    "2" ((u32)(arg3)), [_arg4] "mr" ((u32)(arg4)))
#else
#define PVOP_CALL4(rettype, op, arg1, arg2, arg3, arg4)			\
	__PVOP_CALL(rettype, op, "", "",				\
		    PVOP_CALL_ARG1(arg1), PVOP_CALL_ARG2(arg2),		\
		    PVOP_CALL_ARG3(arg3), PVOP_CALL_ARG4(arg4))
#define PVOP_VCALL4(op, arg1, arg2, arg3, arg4)				\
	__PVOP_VCALL(op, "", "",					\
		     PVOP_CALL_ARG1(arg1), PVOP_CALL_ARG2(arg2),	\
		     PVOP_CALL_ARG3(arg3), PVOP_CALL_ARG4(arg4))
#endif

enum paravirt_lazy_mode {
	PARAVIRT_LAZY_NONE,
	PARAVIRT_LAZY_MMU,
	PARAVIRT_LAZY_CPU,
};

enum paravirt_lazy_mode paravirt_get_lazy_mode(void);
void paravirt_start_context_switch(struct task_struct *prev);
void paravirt_end_context_switch(struct task_struct *next);

void paravirt_enter_lazy_mmu(void);
void paravirt_leave_lazy_mmu(void);

void _paravirt_nop(void);
u32 _paravirt_ident_32(u32);
u64 _paravirt_ident_64(u64);

#define paravirt_nop	((void *)_paravirt_nop)

struct paravirt_patch_site {
	u8 *instr; 		
	u8 instrtype;		
	u8 len;			
	u16 clobbers;		
};

extern struct paravirt_patch_site __parainstructions[],
	__parainstructions_end[];

#endif	

#endif	
