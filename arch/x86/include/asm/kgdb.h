#ifndef _ASM_X86_KGDB_H
#define _ASM_X86_KGDB_H

/*
 * Copyright (C) 2001-2004 Amit S. Kale
 * Copyright (C) 2008 Wind River Systems, Inc.
 */

#define BUFMAX			1024

#ifdef CONFIG_X86_32
enum regnames {
	GDB_AX,			
	GDB_CX,			
	GDB_DX,			
	GDB_BX,			
	GDB_SP,			
	GDB_BP,			
	GDB_SI,			
	GDB_DI,			
	GDB_PC,			
	GDB_PS,			
	GDB_CS,			
	GDB_SS,			
	GDB_DS,			
	GDB_ES,			
	GDB_FS,			
	GDB_GS,			
};
#define GDB_ORIG_AX		41
#define DBG_MAX_REG_NUM		16
#define NUMREGBYTES		((GDB_GS+1)*4)
#else 
enum regnames {
	GDB_AX,			
	GDB_BX,			
	GDB_CX,			
	GDB_DX,			
	GDB_SI,			
	GDB_DI,			
	GDB_BP,			
	GDB_SP,			
	GDB_R8,			
	GDB_R9,			
	GDB_R10,		
	GDB_R11,		
	GDB_R12,		
	GDB_R13,		
	GDB_R14,		
	GDB_R15,		
	GDB_PC,			
	GDB_PS,			
	GDB_CS,			
	GDB_SS,			
	GDB_DS,			
	GDB_ES,			
	GDB_FS,			
	GDB_GS,			
};
#define GDB_ORIG_AX		57
#define DBG_MAX_REG_NUM		24
#define NUMREGBYTES		((17 * 8) + (5 * 4))
#endif 

static inline void arch_kgdb_breakpoint(void)
{
	asm("   int $3");
}
#define BREAK_INSTR_SIZE	1
#define CACHE_FLUSH_IS_SAFE	1
#define GDB_ADJUSTS_BREAK_OFFSET

extern int kgdb_ll_trap(int cmd, const char *str,
			struct pt_regs *regs, long err, int trap, int sig);

#endif 
