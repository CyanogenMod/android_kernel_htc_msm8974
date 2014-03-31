/*
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __BFIN_ENTRY_H
#define __BFIN_ENTRY_H

#include <asm/setup.h>
#include <asm/page.h>

#ifdef __ASSEMBLY__

#define	LFLUSH_I_AND_D	0x00000808
#define	LSIGTRAP	5


#define SAVE_ALL_SYS		save_context_no_interrupts
#ifdef CONFIG_IPIPE
# define LOAD_IPIPE_IPEND \
	P0.l = lo(IPEND); \
	P0.h = hi(IPEND); \
	R1 = [P0];
#else
# define LOAD_IPIPE_IPEND
#endif

#if ANOMALY_05000283 || ANOMALY_05000315
# define ANOMALY_283_315_WORKAROUND(preg, dreg)		\
	cc = dreg == dreg;				\
	preg.h = HI(CHIPID);				\
	preg.l = LO(CHIPID);				\
	if cc jump 1f;					\
	dreg.l = W[preg];				\
1:
#else
# define ANOMALY_283_315_WORKAROUND(preg, dreg)
#endif 

#ifndef CONFIG_EXACT_HWERR
# ifndef CONFIG_DEBUG_KERNEL
#define INTERRUPT_ENTRY(N)						\
    [--sp] = SYSCFG;							\
    [--sp] = P0;						\
    [--sp] = R0;						\
    [--sp] = (R7:0,P5:0);						\
    R0 = (N);								\
    LOAD_IPIPE_IPEND							\
    jump __common_int_entry;
# else 
#define INTERRUPT_ENTRY(N)						\
    [--sp] = SYSCFG;							\
    [--sp] = P0;						\
    [--sp] = R0;						\
    [--sp] = (R7:0,P5:0);						\
    p0.l = lo(IPEND);							\
    p0.h = hi(IPEND);							\
    r1 = [p0];								\
    R0 = (N);								\
    LOAD_IPIPE_IPEND							\
    jump __common_int_entry;
# endif 

#define TIMER_INTERRUPT_ENTRY(N)					\
    [--sp] = SYSCFG;							\
    [--sp] = P0;						\
    [--sp] = R0;						\
    [--sp] = (R7:0,P5:0);						\
    p0.l = lo(IPEND);							\
    p0.h = hi(IPEND);							\
    r1 = [p0];								\
    R0 = (N);								\
    jump __common_int_entry;
#else 

#define INTERRUPT_ENTRY(N)						\
    [--sp] = SYSCFG;							\
    [--sp] = P0;						\
    [--sp] = R0;						\
    [--sp] = (R7:0,P5:0);						\
    R1 = ASTAT;								\
    ANOMALY_283_315_WORKAROUND(p0, r0)					\
    P0.L = LO(ILAT);							\
    P0.H = HI(ILAT);							\
    NOP;								\
    SSYNC;								\
    SSYNC;								\
    R0 = [P0];								\
    CC = BITTST(R0, EVT_IVHW_P);					\
    IF CC JUMP 1f;							\
    ASTAT = R1;								\
    p0.l = lo(IPEND);							\
    p0.h = hi(IPEND);							\
    r1 = [p0];								\
    R0 = (N);								\
    LOAD_IPIPE_IPEND							\
    jump __common_int_entry;						\
1:  ASTAT = R1;								\
    RAISE N;								\
    (R7:0, P5:0) = [SP++];						\
    SP += 0x8;								\
    SYSCFG = [SP++];							\
    CSYNC;								\
    RTI;

#define TIMER_INTERRUPT_ENTRY(N)					\
    [--sp] = SYSCFG;							\
    [--sp] = P0;						\
    [--sp] = R0;						\
    [--sp] = (R7:0,P5:0);						\
    R1 = ASTAT;								\
    ANOMALY_283_315_WORKAROUND(p0, r0)					\
    P0.L = LO(ILAT);							\
    P0.H = HI(ILAT);							\
    NOP;								\
    SSYNC;								\
    SSYNC;								\
    R0 = [P0];								\
    CC = BITTST(R0, EVT_IVHW_P);					\
    IF CC JUMP 1f;							\
    ASTAT = R1;								\
    p0.l = lo(IPEND);							\
    p0.h = hi(IPEND);							\
    r1 = [p0];								\
    R0 = (N);								\
    jump __common_int_entry;						\
1:  ASTAT = R1;								\
    RAISE N;								\
    (R7:0, P5:0) = [SP++];						\
    SP += 0x8;								\
    SYSCFG = [SP++];							\
    CSYNC;								\
    RTI;
#endif	

#define SAVE_CONTEXT_SYSCALL	save_context_syscall
#define SAVE_CONTEXT		save_context_with_interrupts
#define SAVE_CONTEXT_CPLB	save_context_cplb

#define RESTORE_ALL_SYS		restore_context_no_interrupts
#define RESTORE_CONTEXT		restore_context_with_interrupts
#define RESTORE_CONTEXT_CPLB	restore_context_cplb

#endif				
#endif				
