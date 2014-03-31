/*
 * Timer/Counter Unit (TC) registers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ATMEL_TC_H
#define ATMEL_TC_H

#include <linux/compiler.h>
#include <linux/list.h>


struct clk;

struct atmel_tcb_config {
	size_t	counter_width;
};

struct atmel_tc {
	struct platform_device	*pdev;
	struct resource		*iomem;
	void __iomem		*regs;
	struct atmel_tcb_config	*tcb_config;
	int			irq[3];
	struct clk		*clk[3];
	struct list_head	node;
};

extern struct atmel_tc *atmel_tc_alloc(unsigned block, const char *name);
extern void atmel_tc_free(struct atmel_tc *tc);

extern const u8 atmel_tc_divisors[5];



#define ATMEL_TC_BCR	0xc0		
#define     ATMEL_TC_SYNC	(1 << 0)	

#define ATMEL_TC_BMR	0xc4		
#define     ATMEL_TC_TC0XC0S	(3 << 0)	
#define        ATMEL_TC_TC0XC0S_TCLK0	(0 << 0)
#define        ATMEL_TC_TC0XC0S_NONE	(1 << 0)
#define        ATMEL_TC_TC0XC0S_TIOA1	(2 << 0)
#define        ATMEL_TC_TC0XC0S_TIOA2	(3 << 0)
#define     ATMEL_TC_TC1XC1S	(3 << 2)	
#define        ATMEL_TC_TC1XC1S_TCLK1	(0 << 2)
#define        ATMEL_TC_TC1XC1S_NONE	(1 << 2)
#define        ATMEL_TC_TC1XC1S_TIOA0	(2 << 2)
#define        ATMEL_TC_TC1XC1S_TIOA2	(3 << 2)
#define     ATMEL_TC_TC2XC2S	(3 << 4)	
#define        ATMEL_TC_TC2XC2S_TCLK2	(0 << 4)
#define        ATMEL_TC_TC2XC2S_NONE	(1 << 4)
#define        ATMEL_TC_TC2XC2S_TIOA0	(2 << 4)
#define        ATMEL_TC_TC2XC2S_TIOA1	(3 << 4)


#define ATMEL_TC_CHAN(idx)	((idx)*0x40)
#define ATMEL_TC_REG(idx, reg)	(ATMEL_TC_CHAN(idx) + ATMEL_TC_ ## reg)

#define ATMEL_TC_CCR	0x00		
#define     ATMEL_TC_CLKEN	(1 << 0)	
#define     ATMEL_TC_CLKDIS	(1 << 1)	
#define     ATMEL_TC_SWTRG	(1 << 2)	

#define ATMEL_TC_CMR	0x04		

#define     ATMEL_TC_TCCLKS	(7 << 0)	
#define        ATMEL_TC_TIMER_CLOCK1	(0 << 0)
#define        ATMEL_TC_TIMER_CLOCK2	(1 << 0)
#define        ATMEL_TC_TIMER_CLOCK3	(2 << 0)
#define        ATMEL_TC_TIMER_CLOCK4	(3 << 0)
#define        ATMEL_TC_TIMER_CLOCK5	(4 << 0)
#define        ATMEL_TC_XC0		(5 << 0)
#define        ATMEL_TC_XC1		(6 << 0)
#define        ATMEL_TC_XC2		(7 << 0)
#define     ATMEL_TC_CLKI	(1 << 3)	
#define     ATMEL_TC_BURST	(3 << 4)	
#define        ATMEL_TC_GATE_NONE	(0 << 4)
#define        ATMEL_TC_GATE_XC0	(1 << 4)
#define        ATMEL_TC_GATE_XC1	(2 << 4)
#define        ATMEL_TC_GATE_XC2	(3 << 4)
#define     ATMEL_TC_WAVE	(1 << 15)	

#define     ATMEL_TC_LDBSTOP	(1 << 6)	
#define     ATMEL_TC_LDBDIS	(1 << 7)	
#define     ATMEL_TC_ETRGEDG	(3 << 8)	
#define        ATMEL_TC_ETRGEDG_NONE	(0 << 8)
#define        ATMEL_TC_ETRGEDG_RISING	(1 << 8)
#define        ATMEL_TC_ETRGEDG_FALLING	(2 << 8)
#define        ATMEL_TC_ETRGEDG_BOTH	(3 << 8)
#define     ATMEL_TC_ABETRG	(1 << 10)	
#define     ATMEL_TC_CPCTRG	(1 << 14)	
#define     ATMEL_TC_LDRA	(3 << 16)	
#define        ATMEL_TC_LDRA_NONE	(0 << 16)
#define        ATMEL_TC_LDRA_RISING	(1 << 16)
#define        ATMEL_TC_LDRA_FALLING	(2 << 16)
#define        ATMEL_TC_LDRA_BOTH	(3 << 16)
#define     ATMEL_TC_LDRB	(3 << 18)	
#define        ATMEL_TC_LDRB_NONE	(0 << 18)
#define        ATMEL_TC_LDRB_RISING	(1 << 18)
#define        ATMEL_TC_LDRB_FALLING	(2 << 18)
#define        ATMEL_TC_LDRB_BOTH	(3 << 18)

#define     ATMEL_TC_CPCSTOP	(1 <<  6)	
#define     ATMEL_TC_CPCDIS	(1 <<  7)	
#define     ATMEL_TC_EEVTEDG	(3 <<  8)	
#define        ATMEL_TC_EEVTEDG_NONE	(0 << 8)
#define        ATMEL_TC_EEVTEDG_RISING	(1 << 8)
#define        ATMEL_TC_EEVTEDG_FALLING	(2 << 8)
#define        ATMEL_TC_EEVTEDG_BOTH	(3 << 8)
#define     ATMEL_TC_EEVT	(3 << 10)	
#define        ATMEL_TC_EEVT_TIOB	(0 << 10)
#define        ATMEL_TC_EEVT_XC0	(1 << 10)
#define        ATMEL_TC_EEVT_XC1	(2 << 10)
#define        ATMEL_TC_EEVT_XC2	(3 << 10)
#define     ATMEL_TC_ENETRG	(1 << 12)	
#define     ATMEL_TC_WAVESEL	(3 << 13)	
#define        ATMEL_TC_WAVESEL_UP	(0 << 13)
#define        ATMEL_TC_WAVESEL_UPDOWN	(1 << 13)
#define        ATMEL_TC_WAVESEL_UP_AUTO	(2 << 13)
#define        ATMEL_TC_WAVESEL_UPDOWN_AUTO (3 << 13)
#define     ATMEL_TC_ACPA	(3 << 16)	
#define        ATMEL_TC_ACPA_NONE	(0 << 16)
#define        ATMEL_TC_ACPA_SET	(1 << 16)
#define        ATMEL_TC_ACPA_CLEAR	(2 << 16)
#define        ATMEL_TC_ACPA_TOGGLE	(3 << 16)
#define     ATMEL_TC_ACPC	(3 << 18)	
#define        ATMEL_TC_ACPC_NONE	(0 << 18)
#define        ATMEL_TC_ACPC_SET	(1 << 18)
#define        ATMEL_TC_ACPC_CLEAR	(2 << 18)
#define        ATMEL_TC_ACPC_TOGGLE	(3 << 18)
#define     ATMEL_TC_AEEVT	(3 << 20)	
#define        ATMEL_TC_AEEVT_NONE	(0 << 20)
#define        ATMEL_TC_AEEVT_SET	(1 << 20)
#define        ATMEL_TC_AEEVT_CLEAR	(2 << 20)
#define        ATMEL_TC_AEEVT_TOGGLE	(3 << 20)
#define     ATMEL_TC_ASWTRG	(3 << 22)	
#define        ATMEL_TC_ASWTRG_NONE	(0 << 22)
#define        ATMEL_TC_ASWTRG_SET	(1 << 22)
#define        ATMEL_TC_ASWTRG_CLEAR	(2 << 22)
#define        ATMEL_TC_ASWTRG_TOGGLE	(3 << 22)
#define     ATMEL_TC_BCPB	(3 << 24)	
#define        ATMEL_TC_BCPB_NONE	(0 << 24)
#define        ATMEL_TC_BCPB_SET	(1 << 24)
#define        ATMEL_TC_BCPB_CLEAR	(2 << 24)
#define        ATMEL_TC_BCPB_TOGGLE	(3 << 24)
#define     ATMEL_TC_BCPC	(3 << 26)	
#define        ATMEL_TC_BCPC_NONE	(0 << 26)
#define        ATMEL_TC_BCPC_SET	(1 << 26)
#define        ATMEL_TC_BCPC_CLEAR	(2 << 26)
#define        ATMEL_TC_BCPC_TOGGLE	(3 << 26)
#define     ATMEL_TC_BEEVT	(3 << 28)	
#define        ATMEL_TC_BEEVT_NONE	(0 << 28)
#define        ATMEL_TC_BEEVT_SET	(1 << 28)
#define        ATMEL_TC_BEEVT_CLEAR	(2 << 28)
#define        ATMEL_TC_BEEVT_TOGGLE	(3 << 28)
#define     ATMEL_TC_BSWTRG	(3 << 30)	
#define        ATMEL_TC_BSWTRG_NONE	(0 << 30)
#define        ATMEL_TC_BSWTRG_SET	(1 << 30)
#define        ATMEL_TC_BSWTRG_CLEAR	(2 << 30)
#define        ATMEL_TC_BSWTRG_TOGGLE	(3 << 30)

#define ATMEL_TC_CV	0x10		
#define ATMEL_TC_RA	0x14		
#define ATMEL_TC_RB	0x18		
#define ATMEL_TC_RC	0x1c		

#define ATMEL_TC_SR	0x20		
#define     ATMEL_TC_CLKSTA	(1 << 16)	
#define     ATMEL_TC_MTIOA	(1 << 17)	
#define     ATMEL_TC_MTIOB	(1 << 18)	

#define ATMEL_TC_IER	0x24		
#define ATMEL_TC_IDR	0x28		
#define ATMEL_TC_IMR	0x2c		

#define     ATMEL_TC_COVFS	(1 <<  0)	
#define     ATMEL_TC_LOVRS	(1 <<  1)	
#define     ATMEL_TC_CPAS	(1 <<  2)	
#define     ATMEL_TC_CPBS	(1 <<  3)	
#define     ATMEL_TC_CPCS	(1 <<  4)	
#define     ATMEL_TC_LDRAS	(1 <<  5)	
#define     ATMEL_TC_LDRBS	(1 <<  6)	
#define     ATMEL_TC_ETRGS	(1 <<  7)	

#endif
