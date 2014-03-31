
/*
 *	mcfpit.h -- ColdFire internal PIT timer support defines.
 *
 *	(C) Copyright 2003, Greg Ungerer (gerg@snapgear.com)
 */

#ifndef	mcfpit_h
#define	mcfpit_h

#define	MCFPIT_PCSR		0x0		
#define	MCFPIT_PMR		0x2		
#define	MCFPIT_PCNTR		0x4		

#define	MCFPIT_PCSR_CLK1	0x0000		
#define	MCFPIT_PCSR_CLK2	0x0100		
#define	MCFPIT_PCSR_CLK4	0x0200		
#define	MCFPIT_PCSR_CLK8	0x0300		
#define	MCFPIT_PCSR_CLK16	0x0400		
#define	MCFPIT_PCSR_CLK32	0x0500		
#define	MCFPIT_PCSR_CLK64	0x0600		
#define	MCFPIT_PCSR_CLK128	0x0700		
#define	MCFPIT_PCSR_CLK256	0x0800		
#define	MCFPIT_PCSR_CLK512	0x0900		
#define	MCFPIT_PCSR_CLK1024	0x0a00		
#define	MCFPIT_PCSR_CLK2048	0x0b00		
#define	MCFPIT_PCSR_CLK4096	0x0c00		
#define	MCFPIT_PCSR_CLK8192	0x0d00		
#define	MCFPIT_PCSR_CLK16384	0x0e00		
#define	MCFPIT_PCSR_CLK32768	0x0f00		
#define	MCFPIT_PCSR_DOZE	0x0040		
#define	MCFPIT_PCSR_HALTED	0x0020		
#define	MCFPIT_PCSR_OVW		0x0010		
#define	MCFPIT_PCSR_PIE		0x0008		
#define	MCFPIT_PCSR_PIF		0x0004		
#define	MCFPIT_PCSR_RLD		0x0002		
#define	MCFPIT_PCSR_EN		0x0001		
#define	MCFPIT_PCSR_DISABLE	0x0000		

#endif	
