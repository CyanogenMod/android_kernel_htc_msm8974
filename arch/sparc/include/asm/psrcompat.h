#ifndef _SPARC64_PSRCOMPAT_H
#define _SPARC64_PSRCOMPAT_H

#include <asm/pstate.h>

#define PSR_CWP     0x0000001f         
#define PSR_ET      0x00000020         
#define PSR_PS      0x00000040         
#define PSR_S       0x00000080         
#define PSR_PIL     0x00000f00         
#define PSR_EF      0x00001000         
#define PSR_EC      0x00002000         
#define PSR_SYSCALL 0x00004000         
#define PSR_LE      0x00008000         
#define PSR_ICC     0x00f00000         
#define PSR_C       0x00100000         
#define PSR_V       0x00200000         
#define PSR_Z       0x00400000         
#define PSR_N       0x00800000         
#define PSR_VERS    0x0f000000         
#define PSR_IMPL    0xf0000000         

#define PSR_V8PLUS  0xff000000         
#define PSR_XCC	    0x000f0000         

static inline unsigned int tstate_to_psr(unsigned long tstate)
{
	return ((tstate & TSTATE_CWP)			|
		PSR_S					|
		((tstate & TSTATE_ICC) >> 12)		|
		((tstate & TSTATE_XCC) >> 20)		|
		((tstate & TSTATE_SYSCALL) ? PSR_SYSCALL : 0) |
		PSR_V8PLUS);
}

static inline unsigned long psr_to_tstate_icc(unsigned int psr)
{
	unsigned long tstate = ((unsigned long)(psr & PSR_ICC)) << 12;
	if ((psr & (PSR_VERS|PSR_IMPL)) == PSR_V8PLUS)
		tstate |= ((unsigned long)(psr & PSR_XCC)) << 20;
	return tstate;
}

#endif 
