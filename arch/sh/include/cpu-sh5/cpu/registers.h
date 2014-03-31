#ifndef __ASM_SH_CPU_SH5_REGISTERS_H
#define __ASM_SH_CPU_SH5_REGISTERS_H

/*
 * include/asm-sh/cpu-sh5/registers.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 * Copyright (C) 2004  Richard Curnow
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#ifdef __ASSEMBLY__

#define SR	cr0
#define SSR	cr1
#define PSSR	cr2
			
#define INTEVT	cr4
#define EXPEVT	cr5
#define PEXPEVT	cr6
#define TRA	cr7
#define SPC	cr8
#define PSPC	cr9
#define RESVEC	cr10
#define VBR	cr11
			
#define TEA	cr13
			
#define DCR	cr16
#define KCR0	cr17
#define KCR1	cr18
			
			
#define CTC	cr62
#define USR	cr63

#define RET	r2
#define ARG1	r2
#define ARG2	r3
#define ARG3	r4
#define ARG4	r5
#define ARG5	r6
#define ARG6	r7
#define SP	r15
#define LINK	r18
#define ZERO	r63

#define SR_RESET_VAL	0x0000000050008000
#define SR_HARMLESS	0x00000000500080f0	
#define SR_ENABLE_FPU	0xffffffffffff7fff	

#if defined (CONFIG_SH64_SR_WATCH)
#define SR_ENABLE_MMU	0x0000000084000000	
#else
#define SR_ENABLE_MMU	0x0000000080000000	
#endif

#define SR_UNBLOCK_EXC	0xffffffffefffffff	
#define SR_BLOCK_EXC	0x0000000010000000	

#else	

#define __str(x)  #x

#define __SR __str(SR)
#define __SSR __str(SSR)
#define __PSSR __str(PSSR)
#define __INTEVT __str(INTEVT)
#define __EXPEVT __str(EXPEVT)
#define __PEXPEVT __str(PEXPEVT)
#define __TRA __str(TRA)
#define __SPC __str(SPC)
#define __PSPC __str(PSPC)
#define __RESVEC __str(RESVEC)
#define __VBR __str(VBR)
#define __TEA __str(TEA)
#define __DCR __str(DCR)
#define __KCR0 __str(KCR0)
#define __KCR1 __str(KCR1)
#define __CTC __str(CTC)
#define __USR __str(USR)

#endif 
#endif 
