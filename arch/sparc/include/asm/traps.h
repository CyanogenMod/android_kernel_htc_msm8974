/*
 * traps.h:  Format of entries for the Sparc trap table.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_TRAPS_H
#define _SPARC_TRAPS_H

#define NUM_SPARC_TRAPS  255

#ifndef __ASSEMBLY__
#ifdef __KERNEL__
struct tt_entry {
	unsigned long inst_one;
	unsigned long inst_two;
	unsigned long inst_three;
	unsigned long inst_four;
};

extern struct tt_entry *sparc_ttable;

#endif 
#endif 


#define SPARC_MOV_CONST_L3(const) (0xa6102000 | (const&0xfff))

#define SPARC_BRANCH(dest_addr, inst_addr) \
          (0x10800000 | (((dest_addr-inst_addr)>>2)&0x3fffff))

#define SPARC_RD_PSR_L0  (0xa1480000)
#define SPARC_RD_WIM_L3  (0xa7500000)
#define SPARC_NOP (0x01000000)

#define SP_TRAP_TFLT    0x1          
#define SP_TRAP_II      0x2          
#define SP_TRAP_PI      0x3          
#define SP_TRAP_FPD     0x4          
#define SP_TRAP_WOVF    0x5          
#define SP_TRAP_WUNF    0x6          
#define SP_TRAP_MNA     0x7          
#define SP_TRAP_FPE     0x8          
#define SP_TRAP_DFLT    0x9          
#define SP_TRAP_TOF     0xa          
#define SP_TRAP_WDOG    0xb          
#define SP_TRAP_IRQ1    0x11         
#define SP_TRAP_IRQ2    0x12         
#define SP_TRAP_IRQ3    0x13         
#define SP_TRAP_IRQ4    0x14         
#define SP_TRAP_IRQ5    0x15         
#define SP_TRAP_IRQ6    0x16         
#define SP_TRAP_IRQ7    0x17         
#define SP_TRAP_IRQ8    0x18         
#define SP_TRAP_IRQ9    0x19         
#define SP_TRAP_IRQ10   0x1a         
#define SP_TRAP_IRQ11   0x1b         
#define SP_TRAP_IRQ12   0x1c         
#define SP_TRAP_IRQ13   0x1d         
#define SP_TRAP_IRQ14   0x1e         
#define SP_TRAP_IRQ15   0x1f         
#define SP_TRAP_RACC    0x20         
#define SP_TRAP_IACC    0x21         
#define SP_TRAP_CPDIS   0x24         
#define SP_TRAP_BADFL   0x25         
#define SP_TRAP_CPEXP   0x28         
#define SP_TRAP_DACC    0x29         
#define SP_TRAP_DIVZ    0x2a         
#define SP_TRAP_DSTORE  0x2b         
#define SP_TRAP_DMM     0x2c         
#define SP_TRAP_IMM     0x3c         

#define SP_TRAP_SUNOS   0x80         
#define SP_TRAP_SBPT    0x81         
#define SP_TRAP_SDIVZ   0x82         
#define SP_TRAP_FWIN    0x83         
#define SP_TRAP_CWIN    0x84         
#define SP_TRAP_RCHK    0x85         
#define SP_TRAP_FUNA    0x86         
#define SP_TRAP_IOWFL   0x87         
#define SP_TRAP_SOLARIS 0x88         
#define SP_TRAP_NETBSD  0x89         
#define SP_TRAP_LINUX   0x90         

#define ST_SYSCALL              0x00
#define ST_BREAKPOINT           0x01
#define ST_DIV0                 0x02
#define ST_FLUSH_WINDOWS        0x03
#define ST_CLEAN_WINDOWS        0x04
#define ST_RANGE_CHECK          0x05
#define ST_FIX_ALIGN            0x06
#define ST_INT_OVERFLOW         0x07

#define SP_TRAP_KBPT1   0xfe         
#define SP_TRAP_KBPT2   0xff         

#define BAD_TRAP_P(level) \
        ((level > SP_TRAP_WDOG && level < SP_TRAP_IRQ1) || \
	 (level > SP_TRAP_IACC && level < SP_TRAP_CPDIS) || \
	 (level > SP_TRAP_BADFL && level < SP_TRAP_CPEXP) || \
	 (level > SP_TRAP_DMM && level < SP_TRAP_IMM) || \
	 (level > SP_TRAP_IMM && level < SP_TRAP_SUNOS) || \
	 (level > SP_TRAP_LINUX && level < SP_TRAP_KBPT1))

#define HW_TRAP_P(level) ((level > 0) && (level < SP_TRAP_SUNOS))

#define SW_TRAP_P(level) ((level >= SP_TRAP_SUNOS) && (level <= SP_TRAP_KBPT2))

#define SCALL_TRAP_P(level) ((level == SP_TRAP_SUNOS) || \
			     (level == SP_TRAP_SOLARIS) || \
			     (level == SP_TRAP_NETBSD) || \
			     (level == SP_TRAP_LINUX))

#endif 
