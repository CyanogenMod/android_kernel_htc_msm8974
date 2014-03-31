
/* include/asm-m68knommu/MC68328.h: '328 control registers
 *
 * Copyright (C) 1999  Vladimir Gurevich <vgurevic@cisco.com>
 *                     Bear & Hare Software, Inc.
 *
 * Based on include/asm-m68knommu/MC68332.h
 * Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *
 */

#ifndef _MC68328_H_
#define _MC68328_H_

#define BYTE_REF(addr) (*((volatile unsigned char*)addr))
#define WORD_REF(addr) (*((volatile unsigned short*)addr))
#define LONG_REF(addr) (*((volatile unsigned long*)addr))

#define PUT_FIELD(field, val) (((val) << field##_SHIFT) & field##_MASK)
#define GET_FIELD(reg, field) (((reg) & field##_MASK) >> field##_SHIFT)

 
#define SCR_ADDR	0xfffff000
#define SCR		BYTE_REF(SCR_ADDR)

#define SCR_WDTH8	0x01	
#define SCR_DMAP	0x04	
#define SCR_SO		0x08	
#define SCR_BETEN	0x10	
#define SCR_PRV		0x20	
#define SCR_WPV		0x40	
#define SCR_BETO	0x80	

#define MRR_ADDR 0xfffff004
#define MRR      LONG_REF(MRR_ADDR)
 


#define GRPBASEA_ADDR	0xfffff100
#define GRPBASEB_ADDR	0xfffff102
#define GRPBASEC_ADDR	0xfffff104
#define GRPBASED_ADDR	0xfffff106

#define GRPBASEA	WORD_REF(GRPBASEA_ADDR)
#define GRPBASEB	WORD_REF(GRPBASEB_ADDR)
#define GRPBASEC	WORD_REF(GRPBASEC_ADDR)
#define GRPBASED	WORD_REF(GRPBASED_ADDR)

#define GRPBASE_V	  0x0001	
#define GRPBASE_GBA_MASK  0xfff0	

#define GRPMASKA_ADDR	0xfffff108
#define GRPMASKB_ADDR	0xfffff10a
#define GRPMASKC_ADDR	0xfffff10c
#define GRPMASKD_ADDR	0xfffff10e

#define GRPMASKA	WORD_REF(GRPMASKA_ADDR)
#define GRPMASKB	WORD_REF(GRPMASKB_ADDR)
#define GRPMASKC	WORD_REF(GRPMASKC_ADDR)
#define GRPMASKD	WORD_REF(GRPMASKD_ADDR)

#define GRMMASK_GMA_MASK 0xfffff0	

#define CSA0_ADDR	0xfffff110
#define CSA1_ADDR	0xfffff114
#define CSA2_ADDR	0xfffff118
#define CSA3_ADDR	0xfffff11c

#define CSA0		LONG_REF(CSA0_ADDR)
#define CSA1		LONG_REF(CSA1_ADDR)
#define CSA2		LONG_REF(CSA2_ADDR)
#define CSA3		LONG_REF(CSA3_ADDR)

#define CSA_WAIT_MASK	0x00000007	
#define CSA_WAIT_SHIFT	0
#define CSA_RO		0x00000008	
#define CSA_AM_MASK	0x0000ff00	
#define CSA_AM_SHIFT	8
#define CSA_BUSW	0x00010000	
#define CSA_AC_MASK	0xff000000	
#define CSA_AC_SHIFT	24

#define CSB0_ADDR	0xfffff120
#define CSB1_ADDR	0xfffff124
#define CSB2_ADDR	0xfffff128
#define CSB3_ADDR	0xfffff12c

#define CSB0		LONG_REF(CSB0_ADDR)
#define CSB1		LONG_REF(CSB1_ADDR)
#define CSB2		LONG_REF(CSB2_ADDR)
#define CSB3		LONG_REF(CSB3_ADDR)

#define CSB_WAIT_MASK	0x00000007	
#define CSB_WAIT_SHIFT	0
#define CSB_RO		0x00000008	
#define CSB_AM_MASK	0x0000ff00	
#define CSB_AM_SHIFT	8
#define CSB_BUSW	0x00010000	
#define CSB_AC_MASK	0xff000000	
#define CSB_AC_SHIFT	24

#define CSC0_ADDR	0xfffff130
#define CSC1_ADDR	0xfffff134
#define CSC2_ADDR	0xfffff138
#define CSC3_ADDR	0xfffff13c

#define CSC0		LONG_REF(CSC0_ADDR)
#define CSC1		LONG_REF(CSC1_ADDR)
#define CSC2		LONG_REF(CSC2_ADDR)
#define CSC3		LONG_REF(CSC3_ADDR)

#define CSC_WAIT_MASK	0x00000007	
#define CSC_WAIT_SHIFT	0
#define CSC_RO		0x00000008	
#define CSC_AM_MASK	0x0000fff0	
#define CSC_AM_SHIFT	4
#define CSC_BUSW	0x00010000	
#define CSC_AC_MASK	0xfff00000	
#define CSC_AC_SHIFT	20

#define CSD0_ADDR	0xfffff140
#define CSD1_ADDR	0xfffff144
#define CSD2_ADDR	0xfffff148
#define CSD3_ADDR	0xfffff14c

#define CSD0		LONG_REF(CSD0_ADDR)
#define CSD1		LONG_REF(CSD1_ADDR)
#define CSD2		LONG_REF(CSD2_ADDR)
#define CSD3		LONG_REF(CSD3_ADDR)

#define CSD_WAIT_MASK	0x00000007	
#define CSD_WAIT_SHIFT	0
#define CSD_RO		0x00000008	
#define CSD_AM_MASK	0x0000fff0	
#define CSD_AM_SHIFT	4
#define CSD_BUSW	0x00010000	
#define CSD_AC_MASK	0xfff00000	
#define CSD_AC_SHIFT	20

 
#define PLLCR_ADDR	0xfffff200
#define PLLCR		WORD_REF(PLLCR_ADDR)

#define PLLCR_DISPLL	       0x0008	
#define PLLCR_CLKEN	       0x0010	
#define PLLCR_SYSCLK_SEL_MASK  0x0700	
#define PLLCR_SYSCLK_SEL_SHIFT 8
#define PLLCR_PIXCLK_SEL_MASK  0x3800	
#define PLLCR_PIXCLK_SEL_SHIFT 11

#define PLLCR_LCDCLK_SEL_MASK	PLLCR_PIXCLK_SEL_MASK
#define PLLCR_LCDCLK_SEL_SHIFT	PLLCR_PIXCLK_SEL_SHIFT

#define PLLFSR_ADDR	0xfffff202
#define PLLFSR		WORD_REF(PLLFSR_ADDR)

#define PLLFSR_PC_MASK	0x00ff		
#define PLLFSR_PC_SHIFT 0
#define PLLFSR_QC_MASK	0x0f00		
#define PLLFSR_QC_SHIFT 8
#define PLLFSR_PROT	0x4000		
#define PLLFSR_CLK32	0x8000		

#define PCTRL_ADDR	0xfffff207
#define PCTRL		BYTE_REF(PCTRL_ADDR)

#define PCTRL_WIDTH_MASK	0x1f	
#define PCTRL_WIDTH_SHIFT	0
#define PCTRL_STOP		0x40	 
#define PCTRL_PCEN		0x80	


#define IVR_ADDR	0xfffff300
#define IVR		BYTE_REF(IVR_ADDR)

#define IVR_VECTOR_MASK 0xF8

#define ICR_ADRR	0xfffff302
#define ICR		WORD_REF(ICR_ADDR)

#define ICR_ET6		0x0100	
#define ICR_ET3		0x0200	
#define ICR_ET2		0x0400	
#define ICR_ET1		0x0800	
#define ICR_POL6	0x1000	
#define ICR_POL3	0x2000	
#define ICR_POL2	0x4000	
#define ICR_POL1	0x8000	

#define IMR_ADDR	0xfffff304
#define IMR		LONG_REF(IMR_ADDR)
 
#define SPIM_IRQ_NUM	0	
#define	TMR2_IRQ_NUM	1	
#define UART_IRQ_NUM	2		
#define	WDT_IRQ_NUM	3	
#define RTC_IRQ_NUM	4	
#define	KB_IRQ_NUM	6	
#define PWM_IRQ_NUM	7	
#define	INT0_IRQ_NUM	8	
#define	INT1_IRQ_NUM	9	
#define	INT2_IRQ_NUM	10	
#define	INT3_IRQ_NUM	11	
#define	INT4_IRQ_NUM	12	
#define	INT5_IRQ_NUM	13	
#define	INT6_IRQ_NUM	14	
#define	INT7_IRQ_NUM	15	
#define IRQ1_IRQ_NUM	16	
#define IRQ2_IRQ_NUM	17	
#define IRQ3_IRQ_NUM	18	
#define IRQ6_IRQ_NUM	19	
#define PEN_IRQ_NUM	20	
#define SPIS_IRQ_NUM	21	
#define TMR1_IRQ_NUM	22	
#define IRQ7_IRQ_NUM	23	

#define SPI_IRQ_NUM	SPIM_IRQ_NUM
#define TMR_IRQ_NUM	TMR1_IRQ_NUM
 
#define IMR_MSPIM 	(1 << SPIM _IRQ_NUM)	
#define	IMR_MTMR2	(1 << TMR2_IRQ_NUM)	
#define IMR_MUART	(1 << UART_IRQ_NUM)		
#define	IMR_MWDT	(1 << WDT_IRQ_NUM)	
#define IMR_MRTC	(1 << RTC_IRQ_NUM)	
#define	IMR_MKB		(1 << KB_IRQ_NUM)	
#define IMR_MPWM	(1 << PWM_IRQ_NUM)	
#define	IMR_MINT0	(1 << INT0_IRQ_NUM)	
#define	IMR_MINT1	(1 << INT1_IRQ_NUM)	
#define	IMR_MINT2	(1 << INT2_IRQ_NUM)	
#define	IMR_MINT3	(1 << INT3_IRQ_NUM)	
#define	IMR_MINT4	(1 << INT4_IRQ_NUM)	
#define	IMR_MINT5	(1 << INT5_IRQ_NUM)	
#define	IMR_MINT6	(1 << INT6_IRQ_NUM)	
#define	IMR_MINT7	(1 << INT7_IRQ_NUM)	
#define IMR_MIRQ1	(1 << IRQ1_IRQ_NUM)	
#define IMR_MIRQ2	(1 << IRQ2_IRQ_NUM)	
#define IMR_MIRQ3	(1 << IRQ3_IRQ_NUM)	
#define IMR_MIRQ6	(1 << IRQ6_IRQ_NUM)	
#define IMR_MPEN	(1 << PEN_IRQ_NUM)	
#define IMR_MSPIS	(1 << SPIS_IRQ_NUM)	
#define IMR_MTMR1	(1 << TMR1_IRQ_NUM)	
#define IMR_MIRQ7	(1 << IRQ7_IRQ_NUM)	

#define IMR_MSPI	IMR_MSPIM
#define IMR_MTMR	IMR_MTMR1

#define IWR_ADDR	0xfffff308
#define IWR		LONG_REF(IWR_ADDR)

#define IWR_SPIM 	(1 << SPIM _IRQ_NUM)	
#define	IWR_TMR2	(1 << TMR2_IRQ_NUM)	
#define IWR_UART	(1 << UART_IRQ_NUM)		
#define	IWR_WDT		(1 << WDT_IRQ_NUM)	
#define IWR_RTC		(1 << RTC_IRQ_NUM)	
#define	IWR_KB		(1 << KB_IRQ_NUM)	
#define IWR_PWM		(1 << PWM_IRQ_NUM)	
#define	IWR_INT0	(1 << INT0_IRQ_NUM)	
#define	IWR_INT1	(1 << INT1_IRQ_NUM)	
#define	IWR_INT2	(1 << INT2_IRQ_NUM)	
#define	IWR_INT3	(1 << INT3_IRQ_NUM)	
#define	IWR_INT4	(1 << INT4_IRQ_NUM)	
#define	IWR_INT5	(1 << INT5_IRQ_NUM)	
#define	IWR_INT6	(1 << INT6_IRQ_NUM)	
#define	IWR_INT7	(1 << INT7_IRQ_NUM)	
#define IWR_IRQ1	(1 << IRQ1_IRQ_NUM)	
#define IWR_IRQ2	(1 << IRQ2_IRQ_NUM)	
#define IWR_IRQ3	(1 << IRQ3_IRQ_NUM)	
#define IWR_IRQ6	(1 << IRQ6_IRQ_NUM)	
#define IWR_PEN		(1 << PEN_IRQ_NUM)	
#define IWR_SPIS	(1 << SPIS_IRQ_NUM)	
#define IWR_TMR1	(1 << TMR1_IRQ_NUM)	
#define IWR_IRQ7	(1 << IRQ7_IRQ_NUM)	

#define ISR_ADDR	0xfffff30c
#define ISR		LONG_REF(ISR_ADDR)

#define ISR_SPIM 	(1 << SPIM _IRQ_NUM)	
#define	ISR_TMR2	(1 << TMR2_IRQ_NUM)	
#define ISR_UART	(1 << UART_IRQ_NUM)		
#define	ISR_WDT		(1 << WDT_IRQ_NUM)	
#define ISR_RTC		(1 << RTC_IRQ_NUM)	
#define	ISR_KB		(1 << KB_IRQ_NUM)	
#define ISR_PWM		(1 << PWM_IRQ_NUM)	
#define	ISR_INT0	(1 << INT0_IRQ_NUM)	
#define	ISR_INT1	(1 << INT1_IRQ_NUM)	
#define	ISR_INT2	(1 << INT2_IRQ_NUM)	
#define	ISR_INT3	(1 << INT3_IRQ_NUM)	
#define	ISR_INT4	(1 << INT4_IRQ_NUM)	
#define	ISR_INT5	(1 << INT5_IRQ_NUM)	
#define	ISR_INT6	(1 << INT6_IRQ_NUM)	
#define	ISR_INT7	(1 << INT7_IRQ_NUM)	
#define ISR_IRQ1	(1 << IRQ1_IRQ_NUM)	
#define ISR_IRQ2	(1 << IRQ2_IRQ_NUM)	
#define ISR_IRQ3	(1 << IRQ3_IRQ_NUM)	
#define ISR_IRQ6	(1 << IRQ6_IRQ_NUM)	
#define ISR_PEN		(1 << PEN_IRQ_NUM)	
#define ISR_SPIS	(1 << SPIS_IRQ_NUM)	
#define ISR_TMR1	(1 << TMR1_IRQ_NUM)	
#define ISR_IRQ7	(1 << IRQ7_IRQ_NUM)	

#define ISR_SPI	ISR_SPIM
#define ISR_TMR	ISR_TMR1

#define IPR_ADDR	0xfffff310
#define IPR		LONG_REF(IPR_ADDR)

#define IPR_SPIM 	(1 << SPIM _IRQ_NUM)	
#define	IPR_TMR2	(1 << TMR2_IRQ_NUM)	
#define IPR_UART	(1 << UART_IRQ_NUM)		
#define	IPR_WDT		(1 << WDT_IRQ_NUM)	
#define IPR_RTC		(1 << RTC_IRQ_NUM)	
#define	IPR_KB		(1 << KB_IRQ_NUM)	
#define IPR_PWM		(1 << PWM_IRQ_NUM)	
#define	IPR_INT0	(1 << INT0_IRQ_NUM)	
#define	IPR_INT1	(1 << INT1_IRQ_NUM)	
#define	IPR_INT2	(1 << INT2_IRQ_NUM)	
#define	IPR_INT3	(1 << INT3_IRQ_NUM)	
#define	IPR_INT4	(1 << INT4_IRQ_NUM)	
#define	IPR_INT5	(1 << INT5_IRQ_NUM)	
#define	IPR_INT6	(1 << INT6_IRQ_NUM)	
#define	IPR_INT7	(1 << INT7_IRQ_NUM)	
#define IPR_IRQ1	(1 << IRQ1_IRQ_NUM)	
#define IPR_IRQ2	(1 << IRQ2_IRQ_NUM)	
#define IPR_IRQ3	(1 << IRQ3_IRQ_NUM)	
#define IPR_IRQ6	(1 << IRQ6_IRQ_NUM)	
#define IPR_PEN		(1 << PEN_IRQ_NUM)	
#define IPR_SPIS	(1 << SPIS_IRQ_NUM)	
#define IPR_TMR1	(1 << TMR1_IRQ_NUM)	
#define IPR_IRQ7	(1 << IRQ7_IRQ_NUM)	

#define IPR_SPI	IPR_SPIM
#define IPR_TMR	IPR_TMR1


#define PADIR_ADDR	0xfffff400		
#define PADATA_ADDR	0xfffff401		
#define PASEL_ADDR	0xfffff403		

#define PADIR		BYTE_REF(PADIR_ADDR)
#define PADATA		BYTE_REF(PADATA_ADDR)
#define PASEL		BYTE_REF(PASEL_ADDR)

#define PA(x)           (1 << (x))
#define PA_A(x)		PA((x) - 16)	

#define PA_A16		PA(0)		
#define PA_A17		PA(1)		
#define PA_A18		PA(2)		
#define PA_A19		PA(3)		
#define PA_A20		PA(4)		
#define PA_A21		PA(5)		
#define PA_A22		PA(6)		
#define PA_A23		PA(7)		

#define PBDIR_ADDR	0xfffff408		
#define PBDATA_ADDR	0xfffff409		
#define PBSEL_ADDR	0xfffff40b		

#define PBDIR		BYTE_REF(PBDIR_ADDR)
#define PBDATA		BYTE_REF(PBDATA_ADDR)
#define PBSEL		BYTE_REF(PBSEL_ADDR)

#define PB(x)           (1 << (x))
#define PB_D(x)		PB(x)		

#define PB_D0		PB(0)		
#define PB_D1		PB(1)		
#define PB_D2		PB(2)		
#define PB_D3		PB(3)		
#define PB_D4		PB(4)		
#define PB_D5		PB(5)		
#define PB_D6		PB(6)		
#define PB_D7		PB(7)		

#define PCDIR_ADDR	0xfffff410		
#define PCDATA_ADDR	0xfffff411		
#define PCSEL_ADDR	0xfffff413		

#define PCDIR		BYTE_REF(PCDIR_ADDR)
#define PCDATA		BYTE_REF(PCDATA_ADDR)
#define PCSEL		BYTE_REF(PCSEL_ADDR)

#define PC(x)           (1 << (x))

#define PC_WE		PC(6)		
#define PC_DTACK	PC(5)		
#define PC_IRQ7		PC(4)		
#define PC_LDS		PC(2)		
#define PC_UDS		PC(1)		
#define PC_MOCLK	PC(0)		

#define PDDIR_ADDR	0xfffff418		
#define PDDATA_ADDR	0xfffff419		
#define PDPUEN_ADDR	0xfffff41a		
#define PDPOL_ADDR	0xfffff41c		
#define PDIRQEN_ADDR	0xfffff41d		
#define	PDIQEG_ADDR	0xfffff41f		

#define PDDIR		BYTE_REF(PDDIR_ADDR)
#define PDDATA		BYTE_REF(PDDATA_ADDR)
#define PDPUEN		BYTE_REF(PDPUEN_ADDR)
#define	PDPOL		BYTE_REF(PDPOL_ADDR)
#define PDIRQEN		BYTE_REF(PDIRQEN_ADDR)
#define PDIQEG		BYTE_REF(PDIQEG_ADDR)

#define PD(x)           (1 << (x))
#define PD_KB(x)	PD(x)		

#define PD_KB0		PD(0)	
#define PD_KB1		PD(1)	
#define PD_KB2		PD(2)	
#define PD_KB3		PD(3)	
#define PD_KB4		PD(4)	
#define PD_KB5		PD(5)	
#define PD_KB6		PD(6)	
#define PD_KB7		PD(7)	

#define PEDIR_ADDR	0xfffff420		
#define PEDATA_ADDR	0xfffff421		
#define PEPUEN_ADDR	0xfffff422		
#define PESEL_ADDR	0xfffff423		

#define PEDIR		BYTE_REF(PEDIR_ADDR)
#define PEDATA		BYTE_REF(PEDATA_ADDR)
#define PEPUEN		BYTE_REF(PEPUEN_ADDR)
#define PESEL		BYTE_REF(PESEL_ADDR)

#define PE(x)           (1 << (x))

#define PE_CSA1		PE(1)	
#define PE_CSA2		PE(2)	
#define PE_CSA3		PE(3)	
#define PE_CSB0		PE(4)	
#define PE_CSB1		PE(5)	
#define PE_CSB2		PE(6)	
#define PE_CSB3		PE(7)	

#define PFDIR_ADDR	0xfffff428		
#define PFDATA_ADDR	0xfffff429		
#define PFPUEN_ADDR	0xfffff42a		
#define PFSEL_ADDR	0xfffff42b		

#define PFDIR		BYTE_REF(PFDIR_ADDR)
#define PFDATA		BYTE_REF(PFDATA_ADDR)
#define PFPUEN		BYTE_REF(PFPUEN_ADDR)
#define PFSEL		BYTE_REF(PFSEL_ADDR)

#define PF(x)           (1 << (x))
#define PF_A(x)		PF((x) - 24)	

#define PF_A24		PF(0)	
#define PF_A25		PF(1)	
#define PF_A26		PF(2)	
#define PF_A27		PF(3)	
#define PF_A28		PF(4)	
#define PF_A29		PF(5)	
#define PF_A30		PF(6)	
#define PF_A31		PF(7)	

#define PGDIR_ADDR	0xfffff430		
#define PGDATA_ADDR	0xfffff431		
#define PGPUEN_ADDR	0xfffff432		
#define PGSEL_ADDR	0xfffff433		

#define PGDIR		BYTE_REF(PGDIR_ADDR)
#define PGDATA		BYTE_REF(PGDATA_ADDR)
#define PGPUEN		BYTE_REF(PGPUEN_ADDR)
#define PGSEL		BYTE_REF(PGSEL_ADDR)

#define PG(x)           (1 << (x))

#define PG_UART_TXD	PG(0)	
#define PG_UART_RXD	PG(1)	
#define PG_PWMOUT	PG(2)	
#define PG_TOUT2	PG(3)   
#define PG_TIN2		PG(4)	
#define PG_TOUT1	PG(5)   
#define PG_TIN1		PG(6)	
#define PG_RTCOUT	PG(7)	

#define PJDIR_ADDR	0xfffff438		
#define PJDATA_ADDR	0xfffff439		
#define PJSEL_ADDR	0xfffff43b		

#define PJDIR		BYTE_REF(PJDIR_ADDR)
#define PJDATA		BYTE_REF(PJDATA_ADDR)
#define PJSEL		BYTE_REF(PJSEL_ADDR)

#define PJ(x)           (1 << (x)) 

#define PJ_CSD3		PJ(7)	

#define PKDIR_ADDR	0xfffff440		
#define PKDATA_ADDR	0xfffff441		
#define PKPUEN_ADDR	0xfffff442		
#define PKSEL_ADDR	0xfffff443		

#define PKDIR		BYTE_REF(PKDIR_ADDR)
#define PKDATA		BYTE_REF(PKDATA_ADDR)
#define PKPUEN		BYTE_REF(PKPUEN_ADDR)
#define PKSEL		BYTE_REF(PKSEL_ADDR)

#define PK(x)           (1 << (x))

#define PMDIR_ADDR	0xfffff438		
#define PMDATA_ADDR	0xfffff439		
#define PMPUEN_ADDR	0xfffff43a		
#define PMSEL_ADDR	0xfffff43b		

#define PMDIR		BYTE_REF(PMDIR_ADDR)
#define PMDATA		BYTE_REF(PMDATA_ADDR)
#define PMPUEN		BYTE_REF(PMPUEN_ADDR)
#define PMSEL		BYTE_REF(PMSEL_ADDR)

#define PM(x)           (1 << (x))


#define PWMC_ADDR	0xfffff500
#define PWMC		WORD_REF(PWMC_ADDR)

#define PWMC_CLKSEL_MASK	0x0007	
#define PWMC_CLKSEL_SHIFT	0
#define PWMC_PWMEN		0x0010	
#define PMNC_POL		0x0020	
#define PWMC_PIN		0x0080	
#define PWMC_LOAD		0x0100	
#define PWMC_IRQEN		0x4000	
#define PWMC_CLKSRC		0x8000	

#define PWMC_EN	PWMC_PWMEN

#define PWMP_ADDR	0xfffff502
#define PWMP		WORD_REF(PWMP_ADDR)

#define PWMW_ADDR	0xfffff504
#define PWMW		WORD_REF(PWMW_ADDR)

#define PWMCNT_ADDR	0xfffff506
#define PWMCNT		WORD_REF(PWMCNT_ADDR)


#define TCTL1_ADDR	0xfffff600
#define TCTL1		WORD_REF(TCTL1_ADDR)
#define TCTL2_ADDR	0xfffff60c
#define TCTL2		WORD_REF(TCTL2_ADDR)

#define	TCTL_TEN		0x0001	
#define TCTL_CLKSOURCE_MASK 	0x000e	
#define   TCTL_CLKSOURCE_STOP	   0x0000	
#define   TCTL_CLKSOURCE_SYSCLK	   0x0002	
#define   TCTL_CLKSOURCE_SYSCLK_16 0x0004	
#define   TCTL_CLKSOURCE_TIN	   0x0006	
#define   TCTL_CLKSOURCE_32KHZ	   0x0008	
#define TCTL_IRQEN		0x0010	
#define TCTL_OM			0x0020	
#define TCTL_CAP_MASK		0x00c0	
#define	  TCTL_CAP_RE		0x0040		
#define   TCTL_CAP_FE		0x0080		
#define TCTL_FRR		0x0010	

#define TCTL_ADDR	TCTL1_ADDR
#define TCTL		TCTL1

#define TPRER1_ADDR	0xfffff602
#define TPRER1		WORD_REF(TPRER1_ADDR)
#define TPRER2_ADDR	0xfffff60e
#define TPRER2		WORD_REF(TPRER2_ADDR)

#define TPRER_ADDR	TPRER1_ADDR
#define TPRER		TPRER1

#define TCMP1_ADDR	0xfffff604
#define TCMP1		WORD_REF(TCMP1_ADDR)
#define TCMP2_ADDR	0xfffff610
#define TCMP2		WORD_REF(TCMP2_ADDR)

#define TCMP_ADDR	TCMP1_ADDR
#define TCMP		TCMP1

#define TCR1_ADDR	0xfffff606
#define TCR1		WORD_REF(TCR1_ADDR)
#define TCR2_ADDR	0xfffff612
#define TCR2		WORD_REF(TCR2_ADDR)

#define TCR_ADDR	TCR1_ADDR
#define TCR		TCR1

#define TCN1_ADDR	0xfffff608
#define TCN1		WORD_REF(TCN1_ADDR)
#define TCN2_ADDR	0xfffff614
#define TCN2		WORD_REF(TCN2_ADDR)

#define TCN_ADDR	TCN1_ADDR
#define TCN		TCN

#define TSTAT1_ADDR	0xfffff60a
#define TSTAT1		WORD_REF(TSTAT1_ADDR)
#define TSTAT2_ADDR	0xfffff616
#define TSTAT2		WORD_REF(TSTAT2_ADDR)

#define TSTAT_COMP	0x0001		
#define TSTAT_CAPT	0x0001		

#define TSTAT_ADDR	TSTAT1_ADDR
#define TSTAT		TSTAT1

#define WRR_ADDR	0xfffff61a
#define WRR		WORD_REF(WRR_ADDR)

#define WCN_ADDR	0xfffff61c
#define WCN		WORD_REF(WCN_ADDR)

#define WCSR_ADDR	0xfffff618
#define WCSR		WORD_REF(WCSR_ADDR)

#define WCSR_WDEN	0x0001	
#define WCSR_FI		0x0002	
#define WCSR_WRST	0x0004	


#define SPISR_ADDR	0xfffff700
#define SPISR		WORD_REF(SPISR_ADDR)

#define SPISR_DATA_ADDR	0xfffff701
#define SPISR_DATA	BYTE_REF(SPISR_DATA_ADDR)

#define SPISR_DATA_MASK	 0x00ff	
#define SPISR_DATA_SHIFT 0
#define SPISR_SPISEN	 0x0100	
#define SPISR_POL	 0x0200	
#define SPISR_PHA	 0x0400	
#define SPISR_OVWR	 0x0800	/* Data buffer has been overwritten */
#define SPISR_DATARDY	 0x1000	
#define SPISR_ENPOL	 0x2000	
#define SPISR_IRQEN	 0x4000	
#define SPISR_SPISIRQ	 0x8000	


#define SPIMDATA_ADDR	0xfffff800
#define SPIMDATA	WORD_REF(SPIMDATA_ADDR)

#define SPIMCONT_ADDR	0xfffff802
#define SPIMCONT	WORD_REF(SPIMCONT_ADDR)

#define SPIMCONT_BIT_COUNT_MASK	 0x000f	
#define SPIMCONT_BIT_COUNT_SHIFT 0
#define SPIMCONT_POL		 0x0010	
#define	SPIMCONT_PHA		 0x0020	
#define SPIMCONT_IRQEN		 0x0040 
#define SPIMCONT_SPIMIRQ	 0x0080	
#define SPIMCONT_XCH		 0x0100	
#define SPIMCONT_RSPIMEN	 0x0200	
#define SPIMCONT_DATA_RATE_MASK	 0xe000	
#define SPIMCONT_DATA_RATE_SHIFT 13

#define SPIMCONT_IRQ	SPIMCONT_SPIMIRQ
#define SPIMCONT_ENABLE	SPIMCONT_SPIMEN

#define USTCNT_ADDR	0xfffff900
#define USTCNT		WORD_REF(USTCNT_ADDR)

#define USTCNT_TXAVAILEN	0x0001	
#define USTCNT_TXHALFEN		0x0002	
#define USTCNT_TXEMPTYEN	0x0004	
#define USTCNT_RXREADYEN	0x0008	
#define USTCNT_RXHALFEN		0x0010	
#define USTCNT_RXFULLEN		0x0020	
#define USTCNT_CTSDELTAEN	0x0040	
#define USTCNT_GPIODELTAEN	0x0080	
#define USTCNT_8_7		0x0100	
#define USTCNT_STOP		0x0200	
#define USTCNT_ODD_EVEN		0x0400	
#define	USTCNT_PARITYEN		0x0800	
#define USTCNT_CLKMODE		0x1000	
#define	USTCNT_TXEN		0x2000	
#define USTCNT_RXEN		0x4000	
#define USTCNT_UARTEN		0x8000	

#define USTCNT_TXAE	USTCNT_TXAVAILEN 
#define USTCNT_TXHE	USTCNT_TXHALFEN
#define USTCNT_TXEE	USTCNT_TXEMPTYEN
#define USTCNT_RXRE	USTCNT_RXREADYEN
#define USTCNT_RXHE	USTCNT_RXHALFEN
#define USTCNT_RXFE	USTCNT_RXFULLEN
#define USTCNT_CTSD	USTCNT_CTSDELTAEN
#define USTCNT_ODD	USTCNT_ODD_EVEN
#define USTCNT_PEN	USTCNT_PARITYEN
#define USTCNT_CLKM	USTCNT_CLKMODE
#define USTCNT_UEN	USTCNT_UARTEN

#define UBAUD_ADDR	0xfffff902
#define UBAUD		WORD_REF(UBAUD_ADDR)

#define UBAUD_PRESCALER_MASK	0x003f	
#define UBAUD_PRESCALER_SHIFT	0
#define UBAUD_DIVIDE_MASK	0x0700	
#define UBAUD_DIVIDE_SHIFT	8
#define UBAUD_BAUD_SRC		0x0800	
#define UBAUD_GPIOSRC		0x1000	
#define UBAUD_GPIODIR		0x2000	
#define UBAUD_GPIO		0x4000	
#define UBAUD_GPIODELTA		0x8000	

#define URX_ADDR	0xfffff904
#define URX		WORD_REF(URX_ADDR)

#define URX_RXDATA_ADDR	0xfffff905
#define URX_RXDATA	BYTE_REF(URX_RXDATA_ADDR)

#define URX_RXDATA_MASK	 0x00ff	
#define URX_RXDATA_SHIFT 0
#define URX_PARITY_ERROR 0x0100	
#define URX_BREAK	 0x0200	
#define URX_FRAME_ERROR	 0x0400	
#define URX_OVRUN	 0x0800	
#define URX_DATA_READY	 0x2000	
#define URX_FIFO_HALF	 0x4000 
#define URX_FIFO_FULL	 0x8000	

#define UTX_ADDR	0xfffff906
#define UTX		WORD_REF(UTX_ADDR)

#define UTX_TXDATA_ADDR	0xfffff907
#define UTX_TXDATA	BYTE_REF(UTX_TXDATA_ADDR)

#define UTX_TXDATA_MASK	 0x00ff	
#define UTX_TXDATA_SHIFT 0
#define UTX_CTS_DELTA	 0x0100	
#define UTX_CTS_STATUS	 0x0200	
#define	UTX_IGNORE_CTS	 0x0800	
#define UTX_SEND_BREAK	 0x1000	
#define UTX_TX_AVAIL	 0x2000	
#define UTX_FIFO_HALF	 0x4000	
#define UTX_FIFO_EMPTY	 0x8000	

#define UTX_CTS_STAT	UTX_CTS_STATUS
#define UTX_NOCTS	UTX_IGNORE_CTS

#define UMISC_ADDR	0xfffff908
#define UMISC		WORD_REF(UMISC_ADDR)

#define UMISC_TX_POL	 0x0004	
#define UMISC_RX_POL	 0x0008	
#define UMISC_IRDA_LOOP	 0x0010	
#define UMISC_IRDA_EN	 0x0020	
#define UMISC_RTS	 0x0040	
#define UMISC_RTSCONT	 0x0080	
#define UMISC_LOOP	 0x1000	
#define UMISC_FORCE_PERR 0x2000	
#define UMISC_CLKSRC	 0x4000	


typedef volatile struct {
  volatile unsigned short int ustcnt;
  volatile unsigned short int ubaud;
  union {
    volatile unsigned short int w;
    struct {
      volatile unsigned char status;
      volatile unsigned char rxdata;
    } b;
  } urx;
  union {
    volatile unsigned short int w;
    struct {
      volatile unsigned char status;
      volatile unsigned char txdata;
    } b;
  } utx;
  volatile unsigned short int umisc;
  volatile unsigned short int pad1;
  volatile unsigned short int pad2;
  volatile unsigned short int pad3;
} __attribute__((packed)) m68328_uart;



#define LSSA_ADDR	0xfffffa00
#define LSSA		LONG_REF(LSSA_ADDR)

#define LSSA_SSA_MASK	0xfffffffe	

#define LVPW_ADDR	0xfffffa05
#define LVPW		BYTE_REF(LVPW_ADDR)

#define LXMAX_ADDR	0xfffffa08
#define LXMAX		WORD_REF(LXMAX_ADDR)

#define LXMAX_XM_MASK	0x02ff		

#define LYMAX_ADDR	0xfffffa0a
#define LYMAX		WORD_REF(LYMAX_ADDR)

#define LYMAX_YM_MASK	0x02ff		

#define LCXP_ADDR	0xfffffa18
#define LCXP		WORD_REF(LCXP_ADDR)

#define LCXP_CC_MASK	0xc000		
#define   LCXP_CC_TRAMSPARENT	0x0000
#define   LCXP_CC_BLACK		0x4000
#define   LCXP_CC_REVERSED	0x8000
#define   LCXP_CC_WHITE		0xc000
#define LCXP_CXP_MASK	0x02ff		

#define LCYP_ADDR	0xfffffa1a
#define LCYP		WORD_REF(LCYP_ADDR)

#define LCYP_CYP_MASK	0x01ff		

#define LCWCH_ADDR	0xfffffa1c
#define LCWCH		WORD_REF(LCWCH_ADDR)

#define LCWCH_CH_MASK	0x001f		
#define LCWCH_CH_SHIFT	0
#define LCWCH_CW_MASK	0x1f00		
#define LCWCH_CW_SHIFT	8

#define LBLKC_ADDR	0xfffffa1f
#define LBLKC		BYTE_REF(LBLKC_ADDR)

#define LBLKC_BD_MASK	0x7f	
#define LBLKC_BD_SHIFT	0
#define LBLKC_BKEN	0x80	

#define LPICF_ADDR	0xfffffa20
#define LPICF		BYTE_REF(LPICF_ADDR)

#define LPICF_GS_MASK	 0x01	 
#define	  LPICF_GS_BW	   0x00
#define   LPICF_GS_GRAY_4  0x01
#define LPICF_PBSIZ_MASK 0x06	
#define   LPICF_PBSIZ_1	   0x00
#define   LPICF_PBSIZ_2    0x02
#define   LPICF_PBSIZ_4    0x04

#define LPOLCF_ADDR	0xfffffa21
#define LPOLCF		BYTE_REF(LPOLCF_ADDR)

#define LPOLCF_PIXPOL	0x01	
#define LPOLCF_LPPOL	0x02	
#define LPOLCF_FLMPOL	0x04	
#define LPOLCF_LCKPOL	0x08	

#define LACDRC_ADDR	0xfffffa23
#define LACDRC		BYTE_REF(LACDRC_ADDR)

#define LACDRC_ACD_MASK	 0x0f	
#define LACDRC_ACD_SHIFT 0

#define LPXCD_ADDR	0xfffffa25
#define LPXCD		BYTE_REF(LPXCD_ADDR)

#define	LPXCD_PCD_MASK	0x3f 	
#define LPXCD_PCD_SHIFT	0

#define LCKCON_ADDR	0xfffffa27
#define LCKCON		BYTE_REF(LCKCON_ADDR)

#define LCKCON_PCDS	 0x01	
#define LCKCON_DWIDTH	 0x02	
#define LCKCON_DWS_MASK	 0x3c	
#define LCKCON_DWS_SHIFT 2
#define LCKCON_DMA16	 0x40	
#define LCKCON_LCDON	 0x80	

#define LCKCON_DW_MASK	LCKCON_DWS_MASK
#define LCKCON_DW_SHIFT	LCKCON_DWS_SHIFT

#define LLBAR_ADDR	0xfffffa29
#define LLBAR		BYTE_REF(LLBAR_ADDR)

#define LLBAR_LBAR_MASK	 0x7f	
#define LLBAR_LBAR_SHIFT 0

#define LOTCR_ADDR	0xfffffa2b
#define LOTCR		BYTE_REF(LOTCR_ADDR)

#define LPOSR_ADDR	0xfffffa2d
#define LPOSR		BYTE_REF(LPOSR_ADDR)

#define LPOSR_BOS	0x08	
#define LPOSR_POS_MASK	0x07	
#define LPOSR_POS_SHIFT	0

#define LFRCM_ADDR	0xfffffa31
#define LFRCM		BYTE_REF(LFRCM_ADDR)

#define LFRCM_YMOD_MASK	 0x0f	
#define LFRCM_YMOD_SHIFT 0
#define LFRCM_XMOD_MASK	 0xf0	
#define LFRCM_XMOD_SHIFT 4

#define LGPMR_ADDR	0xfffffa32
#define LGPMR		WORD_REF(LGPMR_ADDR)

#define LGPMR_GLEVEL3_MASK	0x000f
#define LGPMR_GLEVEL3_SHIFT	0 
#define LGPMR_GLEVEL2_MASK	0x00f0
#define LGPMR_GLEVEL2_SHIFT	4 
#define LGPMR_GLEVEL0_MASK	0x0f00
#define LGPMR_GLEVEL0_SHIFT	8 
#define LGPMR_GLEVEL1_MASK	0xf000
#define LGPMR_GLEVEL1_SHIFT	12


#define RTCTIME_ADDR	0xfffffb00
#define RTCTIME		LONG_REF(RTCTIME_ADDR)

#define RTCTIME_SECONDS_MASK	0x0000003f	
#define RTCTIME_SECONDS_SHIFT	0
#define RTCTIME_MINUTES_MASK	0x003f0000	
#define RTCTIME_MINUTES_SHIFT	16
#define RTCTIME_HOURS_MASK	0x1f000000	
#define RTCTIME_HOURS_SHIFT	24

#define RTCALRM_ADDR    0xfffffb04
#define RTCALRM         LONG_REF(RTCALRM_ADDR)

#define RTCALRM_SECONDS_MASK    0x0000003f      
#define RTCALRM_SECONDS_SHIFT   0
#define RTCALRM_MINUTES_MASK    0x003f0000      
#define RTCALRM_MINUTES_SHIFT   16
#define RTCALRM_HOURS_MASK      0x1f000000      
#define RTCALRM_HOURS_SHIFT     24

#define RTCCTL_ADDR	0xfffffb0c
#define RTCCTL		WORD_REF(RTCCTL_ADDR)

#define RTCCTL_384	0x0020	
#define RTCCTL_ENABLE	0x0080	

#define RTCCTL_XTL	RTCCTL_384
#define RTCCTL_EN	RTCCTL_ENABLE

#define RTCISR_ADDR	0xfffffb0e
#define RTCISR		WORD_REF(RTCISR_ADDR)

#define RTCISR_SW	0x0001	
#define RTCISR_MIN	0x0002	
#define RTCISR_ALM	0x0004	
#define RTCISR_DAY	0x0008	
#define RTCISR_1HZ	0x0010	

#define RTCIENR_ADDR	0xfffffb10
#define RTCIENR		WORD_REF(RTCIENR_ADDR)

#define RTCIENR_SW	0x0001	
#define RTCIENR_MIN	0x0002	
#define RTCIENR_ALM	0x0004	
#define RTCIENR_DAY	0x0008	
#define RTCIENR_1HZ	0x0010	

#define STPWCH_ADDR	0xfffffb12
#define STPWCH		WORD_REF(STPWCH)

#define STPWCH_CNT_MASK	 0x00ff	
#define SPTWCH_CNT_SHIFT 0

#endif 
