/*
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __ASMBFIN_ELF_H
#define __ASMBFIN_ELF_H


#include <asm/ptrace.h>
#include <asm/user.h>

#define EF_BFIN_PIC		0x00000001	
#define EF_BFIN_FDPIC		0x00000002	
#define EF_BFIN_CODE_IN_L1	0x00000010	
#define EF_BFIN_DATA_IN_L1	0x00000020	
#define EF_BFIN_CODE_IN_L2	0x00000040	
#define EF_BFIN_DATA_IN_L2	0x00000080	

#if 1	
typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct { } elf_fpregset_t;
#endif

#define elf_check_arch(x) ((x)->e_machine == EM_BLACKFIN)

#define elf_check_fdpic(x) ((x)->e_flags & EF_BFIN_FDPIC )
#define elf_check_const_displacement(x) ((x)->e_flags & EF_BFIN_PIC)


#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_BLACKFIN

#define ELF_PLAT_INIT(_r)	_r->p1 = 0

#define ELF_FDPIC_PLAT_INIT(_regs, _exec_map_addr, _interp_map_addr, _dynamic_addr)	\
do {											\
	_regs->r7	= 0;						\
	_regs->p0	= _exec_map_addr;				\
	_regs->p1	= _interp_map_addr;				\
	_regs->p2	= _dynamic_addr;				\
} while(0)

#if 0
#define CORE_DUMP_USE_REGSET
#endif
#define ELF_FDPIC_CORE_EFLAGS	EF_BFIN_FDPIC
#define ELF_EXEC_PAGESIZE	4096

#define R_BFIN_UNUSED0         0   
#define R_BFIN_PCREL5M2        1   
#define R_BFIN_UNUSED1         2   
#define R_BFIN_PCREL10         3   
#define R_BFIN_PCREL12_JUMP    4   
#define R_BFIN_RIMM16          5   
#define R_BFIN_LUIMM16         6   
#define R_BFIN_HUIMM16         7   
#define R_BFIN_PCREL12_JUMP_S  8   
#define R_BFIN_PCREL24_JUMP_X  9   
#define R_BFIN_PCREL24         10  
#define R_BFIN_UNUSEDB         11  
#define R_BFIN_UNUSEDC         12  
#define R_BFIN_PCREL24_JUMP_L  13  
#define R_BFIN_PCREL24_CALL_X  14  
#define R_BFIN_VAR_EQ_SYMB     15  
#define R_BFIN_BYTE_DATA       16  
#define R_BFIN_BYTE2_DATA      17  
#define R_BFIN_BYTE4_DATA      18  
#define R_BFIN_PCREL11         19  
#define R_BFIN_UNUSED14        20  
#define R_BFIN_UNUSED15        21  

#define R_BFIN_PUSH            0xE0
#define R_BFIN_CONST           0xE1
#define R_BFIN_ADD             0xE2
#define R_BFIN_SUB             0xE3
#define R_BFIN_MULT            0xE4
#define R_BFIN_DIV             0xE5
#define R_BFIN_MOD             0xE6
#define R_BFIN_LSHIFT          0xE7
#define R_BFIN_RSHIFT          0xE8
#define R_BFIN_AND             0xE9
#define R_BFIN_OR              0xEA
#define R_BFIN_XOR             0xEB
#define R_BFIN_LAND            0xEC
#define R_BFIN_LOR             0xED
#define R_BFIN_LEN             0xEE
#define R_BFIN_NEG             0xEF
#define R_BFIN_COMP            0xF0
#define R_BFIN_PAGE            0xF1
#define R_BFIN_HWPAGE          0xF2
#define R_BFIN_ADDR            0xF3


#define ELF_ET_DYN_BASE         0xD0000000UL

#define ELF_CORE_COPY_REGS(pr_reg, regs)	\
        memcpy((char *) &pr_reg, (char *)regs,  \
               sizeof(struct pt_regs));
#define ELF_CORE_COPY_FPREGS(...) 0	


#define ELF_HWCAP	(0)


#define ELF_PLATFORM  (NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif
