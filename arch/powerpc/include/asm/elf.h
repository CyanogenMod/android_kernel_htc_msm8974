#ifndef _ASM_POWERPC_ELF_H
#define _ASM_POWERPC_ELF_H

#ifdef __KERNEL__
#include <linux/sched.h>	
#include <asm/page.h>
#include <asm/string.h>
#endif

#include <linux/types.h>

#include <asm/ptrace.h>
#include <asm/cputable.h>
#include <asm/auxvec.h>

#define R_PPC_NONE		0
#define R_PPC_ADDR32		1	
#define R_PPC_ADDR24		2	
#define R_PPC_ADDR16		3	
#define R_PPC_ADDR16_LO		4	
#define R_PPC_ADDR16_HI		5	
#define R_PPC_ADDR16_HA		6	
#define R_PPC_ADDR14		7	
#define R_PPC_ADDR14_BRTAKEN	8
#define R_PPC_ADDR14_BRNTAKEN	9
#define R_PPC_REL24		10	
#define R_PPC_REL14		11	
#define R_PPC_REL14_BRTAKEN	12
#define R_PPC_REL14_BRNTAKEN	13
#define R_PPC_GOT16		14
#define R_PPC_GOT16_LO		15
#define R_PPC_GOT16_HI		16
#define R_PPC_GOT16_HA		17
#define R_PPC_PLTREL24		18
#define R_PPC_COPY		19
#define R_PPC_GLOB_DAT		20
#define R_PPC_JMP_SLOT		21
#define R_PPC_RELATIVE		22
#define R_PPC_LOCAL24PC		23
#define R_PPC_UADDR32		24
#define R_PPC_UADDR16		25
#define R_PPC_REL32		26
#define R_PPC_PLT32		27
#define R_PPC_PLTREL32		28
#define R_PPC_PLT16_LO		29
#define R_PPC_PLT16_HI		30
#define R_PPC_PLT16_HA		31
#define R_PPC_SDAREL16		32
#define R_PPC_SECTOFF		33
#define R_PPC_SECTOFF_LO	34
#define R_PPC_SECTOFF_HI	35
#define R_PPC_SECTOFF_HA	36

#define R_PPC_TLS		67 
#define R_PPC_DTPMOD32		68 
#define R_PPC_TPREL16		69 
#define R_PPC_TPREL16_LO	70 
#define R_PPC_TPREL16_HI	71 
#define R_PPC_TPREL16_HA	72 
#define R_PPC_TPREL32		73 
#define R_PPC_DTPREL16		74 
#define R_PPC_DTPREL16_LO	75 
#define R_PPC_DTPREL16_HI	76 
#define R_PPC_DTPREL16_HA	77 
#define R_PPC_DTPREL32		78 
#define R_PPC_GOT_TLSGD16	79 
#define R_PPC_GOT_TLSGD16_LO	80 
#define R_PPC_GOT_TLSGD16_HI	81 
#define R_PPC_GOT_TLSGD16_HA	82 
#define R_PPC_GOT_TLSLD16	83 
#define R_PPC_GOT_TLSLD16_LO	84 
#define R_PPC_GOT_TLSLD16_HI	85 
#define R_PPC_GOT_TLSLD16_HA	86 
#define R_PPC_GOT_TPREL16	87 
#define R_PPC_GOT_TPREL16_LO	88 
#define R_PPC_GOT_TPREL16_HI	89 
#define R_PPC_GOT_TPREL16_HA	90 
#define R_PPC_GOT_DTPREL16	91 
#define R_PPC_GOT_DTPREL16_LO	92 
#define R_PPC_GOT_DTPREL16_HI	93 
#define R_PPC_GOT_DTPREL16_HA	94 

#define R_PPC_NUM		95

/*
 * ELF register definitions..
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define ELF_NGREG	48	
#define ELF_NFPREG	33	

typedef unsigned long elf_greg_t64;
typedef elf_greg_t64 elf_gregset_t64[ELF_NGREG];

typedef unsigned int elf_greg_t32;
typedef elf_greg_t32 elf_gregset_t32[ELF_NGREG];
typedef elf_gregset_t32 compat_elf_gregset_t;

#ifdef __powerpc64__
# define ELF_NVRREG32	33	
# define ELF_NVRREG	34	
# define ELF_NVSRHALFREG 32	
# define ELF_GREG_TYPE	elf_greg_t64
#else
# define ELF_NEVRREG	34	
# define ELF_NVRREG	33	
# define ELF_GREG_TYPE	elf_greg_t32
# define ELF_ARCH	EM_PPC
# define ELF_CLASS	ELFCLASS32
# define ELF_DATA	ELFDATA2MSB
#endif 

#ifndef ELF_ARCH
# define ELF_ARCH	EM_PPC64
# define ELF_CLASS	ELFCLASS64
# define ELF_DATA	ELFDATA2MSB
  typedef elf_greg_t64 elf_greg_t;
  typedef elf_gregset_t64 elf_gregset_t;
#else
  
  typedef elf_greg_t32 elf_greg_t;
  typedef elf_gregset_t32 elf_gregset_t;
#endif 

typedef double elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

typedef __vector128 elf_vrreg_t;
typedef elf_vrreg_t elf_vrregset_t[ELF_NVRREG];
#ifdef __powerpc64__
typedef elf_vrreg_t elf_vrregset_t32[ELF_NVRREG32];
typedef elf_fpreg_t elf_vsrreghalf_t32[ELF_NVSRHALFREG];
#endif

#ifdef __KERNEL__
#define elf_check_arch(x) ((x)->e_machine == ELF_ARCH)
#define compat_elf_check_arch(x)	((x)->e_machine == EM_PPC)

#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE	PAGE_SIZE


extern unsigned long randomize_et_dyn(unsigned long base);
#define ELF_ET_DYN_BASE		(randomize_et_dyn(0x20000000))

#define PPC_ELF_CORE_COPY_REGS(elf_regs, regs) \
	int i, nregs = min(sizeof(*regs) / sizeof(unsigned long), \
			   (size_t)ELF_NGREG);			  \
	for (i = 0; i < nregs; i++) \
		elf_regs[i] = ((unsigned long *) regs)[i]; \
	memset(&elf_regs[i], 0, (ELF_NGREG - i) * sizeof(elf_regs[0]))

static inline void ppc_elf_core_copy_regs(elf_gregset_t elf_regs,
					  struct pt_regs *regs)
{
	PPC_ELF_CORE_COPY_REGS(elf_regs, regs);
}
#define ELF_CORE_COPY_REGS(gregs, regs) ppc_elf_core_copy_regs(gregs, regs);

typedef elf_vrregset_t elf_fpxregset_t;

# define ELF_HWCAP	(cur_cpu_spec->cpu_user_features)


#define ELF_PLATFORM	(cur_cpu_spec->platform)

#define ELF_BASE_PLATFORM (powerpc_base_platform)

#ifdef __powerpc64__
# define ELF_PLAT_INIT(_r, load_addr)	do {	\
	_r->gpr[2] = load_addr; 		\
} while (0)
#endif 

#ifdef __powerpc64__
# define SET_PERSONALITY(ex)					\
do {								\
	if ((ex).e_ident[EI_CLASS] == ELFCLASS32)		\
		set_thread_flag(TIF_32BIT);			\
	else							\
		clear_thread_flag(TIF_32BIT);			\
	if (personality(current->personality) != PER_LINUX32)	\
		set_personality(PER_LINUX |			\
			(current->personality & (~PER_MASK)));	\
} while (0)
# define elf_read_implies_exec(ex, exec_stk) (is_32bit_task() ? \
		(exec_stk == EXSTACK_DEFAULT) : 0)
#else 
# define SET_PERSONALITY(ex) \
  set_personality(PER_LINUX | (current->personality & (~PER_MASK)))
# define elf_read_implies_exec(ex, exec_stk) (exec_stk == EXSTACK_DEFAULT)
#endif 

extern int dcache_bsize;
extern int icache_bsize;
extern int ucache_bsize;

#define ARCH_HAS_SETUP_ADDITIONAL_PAGES
struct linux_binprm;
extern int arch_setup_additional_pages(struct linux_binprm *bprm,
				       int uses_interp);
#define VDSO_AUX_ENT(a,b) NEW_AUX_ENT(a,b)

#define STACK_RND_MASK (is_32bit_task() ? \
	(0x7ff >> (PAGE_SHIFT - 12)) : \
	(0x3ffff >> (PAGE_SHIFT - 12)))

extern unsigned long arch_randomize_brk(struct mm_struct *mm);
#define arch_randomize_brk arch_randomize_brk

#endif 

#define ARCH_DLINFO							\
do {									\
					\
	NEW_AUX_ENT(AT_IGNOREPPC, AT_IGNOREPPC);			\
	NEW_AUX_ENT(AT_IGNOREPPC, AT_IGNOREPPC);			\
							\
	NEW_AUX_ENT(AT_DCACHEBSIZE, dcache_bsize);			\
	NEW_AUX_ENT(AT_ICACHEBSIZE, icache_bsize);			\
	NEW_AUX_ENT(AT_UCACHEBSIZE, ucache_bsize);			\
	VDSO_AUX_ENT(AT_SYSINFO_EHDR, current->mm->context.vdso_base);	\
} while (0)

#define R_PPC64_NONE    R_PPC_NONE
#define R_PPC64_ADDR32  R_PPC_ADDR32  
#define R_PPC64_ADDR24  R_PPC_ADDR24  
#define R_PPC64_ADDR16  R_PPC_ADDR16  
#define R_PPC64_ADDR16_LO R_PPC_ADDR16_LO 
#define R_PPC64_ADDR16_HI R_PPC_ADDR16_HI 
#define R_PPC64_ADDR16_HA R_PPC_ADDR16_HA 
#define R_PPC64_ADDR14 R_PPC_ADDR14   
#define R_PPC64_ADDR14_BRTAKEN  R_PPC_ADDR14_BRTAKEN
#define R_PPC64_ADDR14_BRNTAKEN R_PPC_ADDR14_BRNTAKEN
#define R_PPC64_REL24   R_PPC_REL24 
#define R_PPC64_REL14   R_PPC_REL14 
#define R_PPC64_REL14_BRTAKEN   R_PPC_REL14_BRTAKEN
#define R_PPC64_REL14_BRNTAKEN  R_PPC_REL14_BRNTAKEN
#define R_PPC64_GOT16     R_PPC_GOT16
#define R_PPC64_GOT16_LO  R_PPC_GOT16_LO
#define R_PPC64_GOT16_HI  R_PPC_GOT16_HI
#define R_PPC64_GOT16_HA  R_PPC_GOT16_HA

#define R_PPC64_COPY      R_PPC_COPY
#define R_PPC64_GLOB_DAT  R_PPC_GLOB_DAT
#define R_PPC64_JMP_SLOT  R_PPC_JMP_SLOT
#define R_PPC64_RELATIVE  R_PPC_RELATIVE

#define R_PPC64_UADDR32   R_PPC_UADDR32
#define R_PPC64_UADDR16   R_PPC_UADDR16
#define R_PPC64_REL32     R_PPC_REL32
#define R_PPC64_PLT32     R_PPC_PLT32
#define R_PPC64_PLTREL32  R_PPC_PLTREL32
#define R_PPC64_PLT16_LO  R_PPC_PLT16_LO
#define R_PPC64_PLT16_HI  R_PPC_PLT16_HI
#define R_PPC64_PLT16_HA  R_PPC_PLT16_HA

#define R_PPC64_SECTOFF     R_PPC_SECTOFF
#define R_PPC64_SECTOFF_LO  R_PPC_SECTOFF_LO
#define R_PPC64_SECTOFF_HI  R_PPC_SECTOFF_HI
#define R_PPC64_SECTOFF_HA  R_PPC_SECTOFF_HA
#define R_PPC64_ADDR30          37  
#define R_PPC64_ADDR64          38  
#define R_PPC64_ADDR16_HIGHER   39  
#define R_PPC64_ADDR16_HIGHERA  40  
#define R_PPC64_ADDR16_HIGHEST  41  
#define R_PPC64_ADDR16_HIGHESTA 42  
#define R_PPC64_UADDR64     43  
#define R_PPC64_REL64       44  
#define R_PPC64_PLT64       45  
#define R_PPC64_PLTREL64    46  
#define R_PPC64_TOC16       47  
#define R_PPC64_TOC16_LO    48  
#define R_PPC64_TOC16_HI    49  
#define R_PPC64_TOC16_HA    50  
#define R_PPC64_TOC         51  
#define R_PPC64_PLTGOT16    52  
#define R_PPC64_PLTGOT16_LO 53  
#define R_PPC64_PLTGOT16_HI 54  
#define R_PPC64_PLTGOT16_HA 55  

#define R_PPC64_ADDR16_DS      56 
#define R_PPC64_ADDR16_LO_DS   57 
#define R_PPC64_GOT16_DS       58 
#define R_PPC64_GOT16_LO_DS    59 
#define R_PPC64_PLT16_LO_DS    60 
#define R_PPC64_SECTOFF_DS     61 
#define R_PPC64_SECTOFF_LO_DS  62 
#define R_PPC64_TOC16_DS       63 
#define R_PPC64_TOC16_LO_DS    64 
#define R_PPC64_PLTGOT16_DS    65 
#define R_PPC64_PLTGOT16_LO_DS 66 

#define R_PPC64_TLS		67 
#define R_PPC64_DTPMOD64	68 
#define R_PPC64_TPREL16		69 
#define R_PPC64_TPREL16_LO	70 
#define R_PPC64_TPREL16_HI	71 
#define R_PPC64_TPREL16_HA	72 
#define R_PPC64_TPREL64		73 
#define R_PPC64_DTPREL16	74 
#define R_PPC64_DTPREL16_LO	75 
#define R_PPC64_DTPREL16_HI	76 
#define R_PPC64_DTPREL16_HA	77 
#define R_PPC64_DTPREL64	78 
#define R_PPC64_GOT_TLSGD16	79 
#define R_PPC64_GOT_TLSGD16_LO	80 
#define R_PPC64_GOT_TLSGD16_HI	81 
#define R_PPC64_GOT_TLSGD16_HA	82 
#define R_PPC64_GOT_TLSLD16	83 
#define R_PPC64_GOT_TLSLD16_LO	84 
#define R_PPC64_GOT_TLSLD16_HI	85 
#define R_PPC64_GOT_TLSLD16_HA	86 
#define R_PPC64_GOT_TPREL16_DS	87 
#define R_PPC64_GOT_TPREL16_LO_DS 88 
#define R_PPC64_GOT_TPREL16_HI	89 
#define R_PPC64_GOT_TPREL16_HA	90 
#define R_PPC64_GOT_DTPREL16_DS	91 
#define R_PPC64_GOT_DTPREL16_LO_DS 92 
#define R_PPC64_GOT_DTPREL16_HI	93 
#define R_PPC64_GOT_DTPREL16_HA	94 
#define R_PPC64_TPREL16_DS	95 
#define R_PPC64_TPREL16_LO_DS	96 
#define R_PPC64_TPREL16_HIGHER	97 
#define R_PPC64_TPREL16_HIGHERA	98 
#define R_PPC64_TPREL16_HIGHEST	99 
#define R_PPC64_TPREL16_HIGHESTA 100 
#define R_PPC64_DTPREL16_DS	101 
#define R_PPC64_DTPREL16_LO_DS	102 
#define R_PPC64_DTPREL16_HIGHER	103 
#define R_PPC64_DTPREL16_HIGHERA 104 
#define R_PPC64_DTPREL16_HIGHEST 105 
#define R_PPC64_DTPREL16_HIGHESTA 106 

#define R_PPC64_NUM		107

struct ppc64_opd_entry
{
	unsigned long funcaddr;
	unsigned long r2;
};

#ifdef  __KERNEL__

#ifdef CONFIG_SPU_BASE
#define NT_SPU		1

#define ARCH_HAVE_EXTRA_ELF_NOTES

#endif 

#endif 

#endif 
