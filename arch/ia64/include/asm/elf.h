#ifndef _ASM_IA64_ELF_H
#define _ASM_IA64_ELF_H

/*
 * ELF-specific definitions.
 *
 * Copyright (C) 1998-1999, 2002-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */


#include <asm/fpu.h>
#include <asm/page.h>
#include <asm/auxvec.h>

#define elf_check_arch(x) ((x)->e_machine == EM_IA_64)

#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_IA_64

#define CORE_DUMP_USE_REGSET

#define EF_IA_64_LINUX_EXECUTABLE_STACK	0x1	

#define ELF_EXEC_PAGESIZE	PAGE_SIZE

#define ELF_ET_DYN_BASE		(TASK_UNMAPPED_BASE + 0x800000000UL)

#define PT_IA_64_UNWIND		0x70000001

#define R_IA64_NONE		0x00	
#define R_IA64_IMM14		0x21	
#define R_IA64_IMM22		0x22	
#define R_IA64_IMM64		0x23	
#define R_IA64_DIR32MSB		0x24	
#define R_IA64_DIR32LSB		0x25	
#define R_IA64_DIR64MSB		0x26	
#define R_IA64_DIR64LSB		0x27	
#define R_IA64_GPREL22		0x2a	
#define R_IA64_GPREL64I		0x2b	
#define R_IA64_GPREL32MSB	0x2c	
#define R_IA64_GPREL32LSB	0x2d	
#define R_IA64_GPREL64MSB	0x2e	
#define R_IA64_GPREL64LSB	0x2f	
#define R_IA64_LTOFF22		0x32	
#define R_IA64_LTOFF64I		0x33	
#define R_IA64_PLTOFF22		0x3a	
#define R_IA64_PLTOFF64I	0x3b	
#define R_IA64_PLTOFF64MSB	0x3e	
#define R_IA64_PLTOFF64LSB	0x3f	
#define R_IA64_FPTR64I		0x43	
#define R_IA64_FPTR32MSB	0x44	
#define R_IA64_FPTR32LSB	0x45	
#define R_IA64_FPTR64MSB	0x46	
#define R_IA64_FPTR64LSB	0x47	
#define R_IA64_PCREL60B		0x48	
#define R_IA64_PCREL21B		0x49	
#define R_IA64_PCREL21M		0x4a	
#define R_IA64_PCREL21F		0x4b	
#define R_IA64_PCREL32MSB	0x4c	
#define R_IA64_PCREL32LSB	0x4d	
#define R_IA64_PCREL64MSB	0x4e	
#define R_IA64_PCREL64LSB	0x4f	
#define R_IA64_LTOFF_FPTR22	0x52	
#define R_IA64_LTOFF_FPTR64I	0x53	
#define R_IA64_LTOFF_FPTR32MSB	0x54	
#define R_IA64_LTOFF_FPTR32LSB	0x55	
#define R_IA64_LTOFF_FPTR64MSB	0x56	
#define R_IA64_LTOFF_FPTR64LSB	0x57	
#define R_IA64_SEGREL32MSB	0x5c	
#define R_IA64_SEGREL32LSB	0x5d	
#define R_IA64_SEGREL64MSB	0x5e	
#define R_IA64_SEGREL64LSB	0x5f	
#define R_IA64_SECREL32MSB	0x64	
#define R_IA64_SECREL32LSB	0x65	
#define R_IA64_SECREL64MSB	0x66	
#define R_IA64_SECREL64LSB	0x67	
#define R_IA64_REL32MSB		0x6c	
#define R_IA64_REL32LSB		0x6d	
#define R_IA64_REL64MSB		0x6e	
#define R_IA64_REL64LSB		0x6f	
#define R_IA64_LTV32MSB		0x74	
#define R_IA64_LTV32LSB		0x75	
#define R_IA64_LTV64MSB		0x76	
#define R_IA64_LTV64LSB		0x77	
#define R_IA64_PCREL21BI	0x79	
#define R_IA64_PCREL22		0x7a	
#define R_IA64_PCREL64I		0x7b	
#define R_IA64_IPLTMSB		0x80	
#define R_IA64_IPLTLSB		0x81	
#define R_IA64_COPY		0x84	
#define R_IA64_SUB		0x85	
#define R_IA64_LTOFF22X		0x86	
#define R_IA64_LDXMOV		0x87	
#define R_IA64_TPREL14		0x91	
#define R_IA64_TPREL22		0x92	
#define R_IA64_TPREL64I		0x93	
#define R_IA64_TPREL64MSB	0x96	
#define R_IA64_TPREL64LSB	0x97	
#define R_IA64_LTOFF_TPREL22	0x9a	
#define R_IA64_DTPMOD64MSB	0xa6	
#define R_IA64_DTPMOD64LSB	0xa7	
#define R_IA64_LTOFF_DTPMOD22	0xaa	
#define R_IA64_DTPREL14		0xb1	
#define R_IA64_DTPREL22		0xb2	
#define R_IA64_DTPREL64I	0xb3	
#define R_IA64_DTPREL32MSB	0xb4	
#define R_IA64_DTPREL32LSB	0xb5	
#define R_IA64_DTPREL64MSB	0xb6	
#define R_IA64_DTPREL64LSB	0xb7	
#define R_IA64_LTOFF_DTPREL22	0xba	

#define SHF_IA_64_SHORT		0x10000000	

extern void ia64_init_addr_space (void);
#define ELF_PLAT_INIT(_r, load_addr)	ia64_init_addr_space()


#define ELF_NGREG	128	
#define ELF_NFPREG	128	

#define ELF_GR_0_OFFSET     0
#define ELF_NAT_OFFSET     (32 * sizeof(elf_greg_t))
#define ELF_PR_OFFSET      (33 * sizeof(elf_greg_t))
#define ELF_BR_0_OFFSET    (34 * sizeof(elf_greg_t))
#define ELF_CR_IIP_OFFSET  (42 * sizeof(elf_greg_t))
#define ELF_CFM_OFFSET     (43 * sizeof(elf_greg_t))
#define ELF_CR_IPSR_OFFSET (44 * sizeof(elf_greg_t))
#define ELF_GR_OFFSET(i)   (ELF_GR_0_OFFSET + i * sizeof(elf_greg_t))
#define ELF_BR_OFFSET(i)   (ELF_BR_0_OFFSET + i * sizeof(elf_greg_t))
#define ELF_AR_RSC_OFFSET  (45 * sizeof(elf_greg_t))
#define ELF_AR_BSP_OFFSET  (46 * sizeof(elf_greg_t))
#define ELF_AR_BSPSTORE_OFFSET (47 * sizeof(elf_greg_t))
#define ELF_AR_RNAT_OFFSET (48 * sizeof(elf_greg_t))
#define ELF_AR_CCV_OFFSET  (49 * sizeof(elf_greg_t))
#define ELF_AR_UNAT_OFFSET (50 * sizeof(elf_greg_t))
#define ELF_AR_FPSR_OFFSET (51 * sizeof(elf_greg_t))
#define ELF_AR_PFS_OFFSET  (52 * sizeof(elf_greg_t))
#define ELF_AR_LC_OFFSET   (53 * sizeof(elf_greg_t))
#define ELF_AR_EC_OFFSET   (54 * sizeof(elf_greg_t))
#define ELF_AR_CSD_OFFSET  (55 * sizeof(elf_greg_t))
#define ELF_AR_SSD_OFFSET  (56 * sizeof(elf_greg_t))
#define ELF_AR_END_OFFSET  (57 * sizeof(elf_greg_t))

typedef unsigned long elf_fpxregset_t;

typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct ia64_fpreg elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];



struct pt_regs;	
extern void ia64_elf_core_copy_regs (struct pt_regs *src, elf_gregset_t dst);
#define ELF_CORE_COPY_REGS(_dest,_regs)	ia64_elf_core_copy_regs(_regs, _dest);

#define ELF_HWCAP 	0

#define ELF_PLATFORM	NULL

#define SET_PERSONALITY(ex)	\
	set_personality((current->personality & ~PER_MASK) | PER_LINUX)

#define elf_read_implies_exec(ex, executable_stack)					\
	((executable_stack!=EXSTACK_DISABLE_X) && ((ex).e_flags & EF_IA_64_LINUX_EXECUTABLE_STACK) != 0)

struct task_struct;

#define GATE_EHDR	((const struct elfhdr *) GATE_ADDR)

#define ARCH_DLINFO								\
do {										\
	extern char __kernel_syscall_via_epc[];					\
	NEW_AUX_ENT(AT_SYSINFO, (unsigned long) __kernel_syscall_via_epc);	\
	NEW_AUX_ENT(AT_SYSINFO_EHDR, (unsigned long) GATE_EHDR);		\
} while (0)

struct got_entry {
	uint64_t val;
};

struct fdesc {
	uint64_t ip;
	uint64_t gp;
};

#endif 
