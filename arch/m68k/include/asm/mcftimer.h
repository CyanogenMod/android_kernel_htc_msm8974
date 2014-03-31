
/*
 *	mcftimer.h -- ColdFire internal TIMER support defines.
 *
 *	(C) Copyright 1999-2006, Greg Ungerer <gerg@snapgear.com>
 * 	(C) Copyright 2000, Lineo Inc. (www.lineo.com) 
 */

#ifndef	mcftimer_h
#define	mcftimer_h

#define	MCFTIMER_TMR		0x00		
#define	MCFTIMER_TRR		0x04		
#define	MCFTIMER_TCR		0x08		
#define	MCFTIMER_TCN		0x0C		
#if defined(CONFIG_M532x)
#define	MCFTIMER_TER		0x03		
#else
#define	MCFTIMER_TER		0x11		
#endif

#define	MCFTIMER_TMR_PREMASK	0xff00		
#define	MCFTIMER_TMR_DISCE	0x0000		
#define	MCFTIMER_TMR_ANYCE	0x00c0		
#define	MCFTIMER_TMR_FALLCE	0x0080		
#define	MCFTIMER_TMR_RISECE	0x0040		
#define	MCFTIMER_TMR_ENOM	0x0020		
#define	MCFTIMER_TMR_DISOM	0x0000		
#define	MCFTIMER_TMR_ENORI	0x0010		
#define	MCFTIMER_TMR_DISORI	0x0000		
#define	MCFTIMER_TMR_RESTART	0x0008		
#define	MCFTIMER_TMR_FREERUN	0x0000		
#define	MCFTIMER_TMR_CLKTIN	0x0006		
#define	MCFTIMER_TMR_CLK16	0x0004		
#define	MCFTIMER_TMR_CLK1	0x0002		
#define	MCFTIMER_TMR_CLKSTOP	0x0000		
#define	MCFTIMER_TMR_ENABLE	0x0001		
#define	MCFTIMER_TMR_DISABLE	0x0000		

#define	MCFTIMER_TER_CAP	0x01		
#define	MCFTIMER_TER_REF	0x02		

#endif	
