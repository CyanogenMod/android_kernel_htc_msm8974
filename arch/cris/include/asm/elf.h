#ifndef __ASMCRIS_ELF_H
#define __ASMCRIS_ELF_H


#include <asm/user.h>

#define R_CRIS_NONE             0
#define R_CRIS_8                1
#define R_CRIS_16               2
#define R_CRIS_32               3
#define R_CRIS_8_PCREL          4
#define R_CRIS_16_PCREL         5
#define R_CRIS_32_PCREL         6
#define R_CRIS_GNU_VTINHERIT    7
#define R_CRIS_GNU_VTENTRY      8
#define R_CRIS_COPY             9
#define R_CRIS_GLOB_DAT         10
#define R_CRIS_JUMP_SLOT        11
#define R_CRIS_RELATIVE         12
#define R_CRIS_16_GOT           13
#define R_CRIS_32_GOT           14
#define R_CRIS_16_GOTPLT        15
#define R_CRIS_32_GOTPLT        16
#define R_CRIS_32_GOTREL        17
#define R_CRIS_32_PLT_GOTREL    18
#define R_CRIS_32_PLT_PCREL     19

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof (struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef unsigned long elf_fpregset_t;

#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_CRIS

#include <arch/elf.h>

#define EF_CRIS_UNDERSCORE		0x00000001

#define EF_CRIS_VARIANT_MASK		0x0000000e

#define EF_CRIS_VARIANT_ANY_V0_V10	0x00000000

#define EF_CRIS_VARIANT_V32		0x00000002

#define EF_CRIS_VARIANT_COMMON_V10_V32	0x00000004

#define ELF_EXEC_PAGESIZE	8192


#define ELF_ET_DYN_BASE         (2 * TASK_SIZE / 3)


#define ELF_HWCAP       (0)


#define ELF_PLATFORM  (NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif
