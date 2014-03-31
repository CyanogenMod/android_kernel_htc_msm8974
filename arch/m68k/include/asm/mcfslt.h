
/*
 *	mcfslt.h -- ColdFire internal Slice (SLT) timer support defines.
 *
 *	(C) Copyright 2004, Greg Ungerer (gerg@snapgear.com)
 *	(C) Copyright 2009, Philippe De Muyter (phdm@macqel.be)
 */

#ifndef mcfslt_h
#define mcfslt_h

#define MCFSLT_TIMER0		0x900	
#define MCFSLT_TIMER1		0x910	


#define MCFSLT_STCNT		0x00	
#define MCFSLT_SCR		0x04	
#define MCFSLT_SCNT		0x08	
#define MCFSLT_SSR		0x0C	

#define MCFSLT_SCR_RUN		0x04000000	
#define MCFSLT_SCR_IEN		0x02000000	
#define MCFSLT_SCR_TEN		0x01000000	

#define MCFSLT_SSR_BE		0x02000000	
#define MCFSLT_SSR_TE		0x01000000	

#endif	
