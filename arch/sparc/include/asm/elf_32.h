#ifndef __ASMSPARC_ELF_H
#define __ASMSPARC_ELF_H


#include <asm/ptrace.h>

#define STT_REGISTER		13

#define	R_SPARC_NONE		0
#define	R_SPARC_8		1
#define	R_SPARC_16		2
#define	R_SPARC_32		3
#define	R_SPARC_DISP8		4
#define	R_SPARC_DISP16		5
#define	R_SPARC_DISP32		6
#define	R_SPARC_WDISP30		7
#define	R_SPARC_WDISP22		8
#define	R_SPARC_HI22		9
#define	R_SPARC_22		10
#define	R_SPARC_13		11
#define	R_SPARC_LO10		12
#define	R_SPARC_GOT10		13
#define	R_SPARC_GOT13		14
#define	R_SPARC_GOT22		15
#define	R_SPARC_PC10		16
#define	R_SPARC_PC22		17
#define	R_SPARC_WPLT30		18
#define	R_SPARC_COPY		19
#define	R_SPARC_GLOB_DAT	20
#define	R_SPARC_JMP_SLOT	21
#define	R_SPARC_RELATIVE	22
#define	R_SPARC_UA32		23
#define R_SPARC_PLT32		24
#define R_SPARC_HIPLT22		25
#define R_SPARC_LOPLT10		26
#define R_SPARC_PCPLT32		27
#define R_SPARC_PCPLT22		28
#define R_SPARC_PCPLT10		29
#define R_SPARC_10		30
#define R_SPARC_11		31
#define R_SPARC_64		32
#define R_SPARC_OLO10		33
#define R_SPARC_WDISP16		40
#define R_SPARC_WDISP19		41
#define R_SPARC_7		43
#define R_SPARC_5		44
#define R_SPARC_6		45


#define HWCAP_SPARC_FLUSH       1    
#define HWCAP_SPARC_STBAR       2
#define HWCAP_SPARC_SWAP        4
#define HWCAP_SPARC_MULDIV      8
#define HWCAP_SPARC_V9		16
#define HWCAP_SPARC_ULTRA3	32

#define CORE_DUMP_USE_REGSET

typedef unsigned long elf_greg_t;
#define ELF_NGREG 38
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct {
	union {
		unsigned long	pr_regs[32];
		double		pr_dregs[16];
	} pr_fr;
	unsigned long __unused;
	unsigned long	pr_fsr;
	unsigned char	pr_qcnt;
	unsigned char	pr_q_entrysize;
	unsigned char	pr_en;
	unsigned int	pr_q[64];
} elf_fpregset_t;

#include <asm/mbus.h>

#define elf_check_arch(x) ((x)->e_machine == EM_SPARC)

#define ELF_ARCH	EM_SPARC
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB

#define ELF_EXEC_PAGESIZE	4096



#define ELF_ET_DYN_BASE         (TASK_UNMAPPED_BASE)


#define ELF_HWCAP	((ARCH_SUN4C) ? 0 : \
			 (HWCAP_SPARC_FLUSH | HWCAP_SPARC_STBAR | \
			  HWCAP_SPARC_SWAP | \
			  ((srmmu_modtype != Cypress && \
			    srmmu_modtype != Cypress_vE && \
			    srmmu_modtype != Cypress_vD) ? \
			   HWCAP_SPARC_MULDIV : 0)))


#define ELF_PLATFORM	(NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif 
