#ifndef _ASM_PROC_INTCTL_REGS_H
#define _ASM_PROC_INTCTL_REGS_H

#ifndef _ASM_INTCTL_REGS_H
# error "please don't include this file directly"
#endif

#define IAGR			__SYSREG(0xd4000100, u16)

#define IAGR_GN			0x00fc

#define __GET_XIRQ_TRIGGER(X, Z) (((Z) >> ((X) * 2)) & 3)

#define __SET_XIRQ_TRIGGER(X, Y, Z)		\
({						\
	typeof(Z) x = (Z);			\
	x &= ~(3 << ((X) * 2));			\
	x |= ((Y) & 3) << ((X) * 2);		\
	(Z) = x;				\
})

#define EXTMD			__SYSREG(0xd4000200, u16)
#define GET_XIRQ_TRIGGER(X)	__GET_XIRQ_TRIGGER(X, EXTMD)
#define SET_XIRQ_TRIGGER(X, Y)	__SET_XIRQ_TRIGGER(X, Y, EXTMD)

#endif 
