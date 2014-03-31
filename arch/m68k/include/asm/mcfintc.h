
/*
 *	mcfintc.h -- support definitions for the simple ColdFire
 *		     Interrupt Controller
 *
 * 	(C) Copyright 2009,  Greg Ungerer <gerg@uclinux.org>
 */

#ifndef	mcfintc_h
#define	mcfintc_h


#define	MCFSIM_ICR_AUTOVEC	0x80		
#define	MCFSIM_ICR_LEVEL0	0x00		
#define	MCFSIM_ICR_LEVEL1	0x04		
#define	MCFSIM_ICR_LEVEL2	0x08		
#define	MCFSIM_ICR_LEVEL3	0x0c		
#define	MCFSIM_ICR_LEVEL4	0x10		
#define	MCFSIM_ICR_LEVEL5	0x14		
#define	MCFSIM_ICR_LEVEL6	0x18		
#define	MCFSIM_ICR_LEVEL7	0x1c		

#define	MCFSIM_ICR_PRI0		0x00		
#define	MCFSIM_ICR_PRI1		0x01		
#define	MCFSIM_ICR_PRI2		0x02		
#define	MCFSIM_ICR_PRI3		0x03		

#define	MCFINTC_EINT1		1		
#define	MCFINTC_EINT2		2		
#define	MCFINTC_EINT3		3		
#define	MCFINTC_EINT4		4		
#define	MCFINTC_EINT5		5		
#define	MCFINTC_EINT6		6		
#define	MCFINTC_EINT7		7		
#define	MCFINTC_SWT		8		
#define	MCFINTC_TIMER1		9
#define	MCFINTC_TIMER2		10
#define	MCFINTC_I2C		11		
#define	MCFINTC_UART0		12
#define	MCFINTC_UART1		13
#define	MCFINTC_DMA0		14
#define	MCFINTC_DMA1		15
#define	MCFINTC_DMA2		16
#define	MCFINTC_DMA3		17
#define	MCFINTC_QSPI		18

#ifndef __ASSEMBLER__

extern unsigned char mcf_irq2imr[];
static inline void mcf_mapirq2imr(int irq, int imr)
{
	mcf_irq2imr[irq] = imr;
}

void mcf_autovector(int irq);
void mcf_setimr(int index);
void mcf_clrimr(int index);
#endif

#endif	
