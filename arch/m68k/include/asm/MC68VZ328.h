
/* include/asm-m68knommu/MC68VZ328.h: 'VZ328 control registers
 *
 * Copyright (c) 2000-2001	Lineo Inc. <www.lineo.com>
 * Copyright (c) 2000-2001	Lineo Canada Corp. <www.lineo.ca>
 * Copyright (C) 1999		Vladimir Gurevich <vgurevic@cisco.com>
 * 				Bare & Hare Software, Inc.
 * Based on include/asm-m68knommu/MC68332.h
 * Copyright (C) 1998  Kenneth Albanowski <kjahds@kjahds.com>,
 *                     The Silver Hammer Group, Ltd.
 *
 * M68VZ328 fixes by Evan Stawnyczy <evan@lineo.com>
 * vz multiport fixes by Michael Leslie <mleslie@lineo.com>
 */

#ifndef _MC68VZ328_H_
#define _MC68VZ328_H_

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
#define MRR	 LONG_REF(MRR_ADDR)

 
#define CSGBA_ADDR	0xfffff100
#define CSGBB_ADDR	0xfffff102

#define CSGBC_ADDR	0xfffff104
#define CSGBD_ADDR	0xfffff106

#define CSGBA		WORD_REF(CSGBA_ADDR)
#define CSGBB		WORD_REF(CSGBB_ADDR)
#define CSGBC		WORD_REF(CSGBC_ADDR)
#define CSGBD		WORD_REF(CSGBD_ADDR)

#define CSA_ADDR	0xfffff110
#define CSB_ADDR	0xfffff112
#define CSC_ADDR	0xfffff114
#define CSD_ADDR	0xfffff116

#define CSA		WORD_REF(CSA_ADDR)
#define CSB		WORD_REF(CSB_ADDR)
#define CSC		WORD_REF(CSC_ADDR)
#define CSD		WORD_REF(CSD_ADDR)

#define CSA_EN		0x0001		
#define CSA_SIZ_MASK	0x000e		
#define CSA_SIZ_SHIFT   1
#define CSA_WS_MASK	0x0070		
#define CSA_WS_SHIFT    4
#define CSA_BSW		0x0080		
#define CSA_FLASH	0x0100		
#define CSA_RO		0x8000		

#define CSB_EN		0x0001		
#define CSB_SIZ_MASK	0x000e		
#define CSB_SIZ_SHIFT   1
#define CSB_WS_MASK	0x0070		
#define CSB_WS_SHIFT    4
#define CSB_BSW		0x0080		
#define CSB_FLASH	0x0100		
#define CSB_UPSIZ_MASK	0x1800		
#define CSB_UPSIZ_SHIFT 11
#define CSB_ROP		0x2000		
#define CSB_SOP		0x4000		
#define CSB_RO		0x8000		

#define CSC_EN		0x0001		
#define CSC_SIZ_MASK	0x000e		
#define CSC_SIZ_SHIFT   1
#define CSC_WS_MASK	0x0070		
#define CSC_WS_SHIFT    4
#define CSC_BSW		0x0080		
#define CSC_FLASH	0x0100		
#define CSC_UPSIZ_MASK	0x1800		
#define CSC_UPSIZ_SHIFT 11
#define CSC_ROP		0x2000		
#define CSC_SOP		0x4000		
#define CSC_RO		0x8000		

#define CSD_EN		0x0001		
#define CSD_SIZ_MASK	0x000e		
#define CSD_SIZ_SHIFT   1
#define CSD_WS_MASK	0x0070		
#define CSD_WS_SHIFT    4
#define CSD_BSW		0x0080		
#define CSD_FLASH	0x0100		
#define CSD_DRAM	0x0200		
#define	CSD_COMB	0x0400		
#define CSD_UPSIZ_MASK	0x1800		
#define CSD_UPSIZ_SHIFT 11
#define CSD_ROP		0x2000		
#define CSD_SOP		0x4000		
#define CSD_RO		0x8000		

#define EMUCS_ADDR	0xfffff118
#define EMUCS		WORD_REF(EMUCS_ADDR)

#define EMUCS_WS_MASK	0x0070
#define EMUCS_WS_SHIFT	4


#define PLLCR_ADDR	0xfffff200
#define PLLCR		WORD_REF(PLLCR_ADDR)

#define PLLCR_DISPLL	       0x0008	
#define PLLCR_CLKEN	       0x0010	
#define PLLCR_PRESC	       0x0020	
#define PLLCR_SYSCLK_SEL_MASK  0x0700	
#define PLLCR_SYSCLK_SEL_SHIFT 8
#define PLLCR_LCDCLK_SEL_MASK  0x3800	
#define PLLCR_LCDCLK_SEL_SHIFT 11

#define PLLCR_PIXCLK_SEL_MASK	PLLCR_LCDCLK_SEL_MASK
#define PLLCR_PIXCLK_SEL_SHIFT	PLLCR_LCDCLK_SEL_SHIFT

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
#define PCTRL_PCEN		0x80	


#define IVR_ADDR	0xfffff300
#define IVR		BYTE_REF(IVR_ADDR)

#define IVR_VECTOR_MASK 0xF8

#define ICR_ADDR	0xfffff302
#define ICR		WORD_REF(ICR_ADDR)

#define ICR_POL5	0x0080	
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

#define SPI2_IRQ_NUM	0	
#define TMR_IRQ_NUM	1	
#define UART1_IRQ_NUM	2		
#define	WDT_IRQ_NUM	3	
#define RTC_IRQ_NUM	4	
#define TMR2_IRQ_NUM	5	
#define	KB_IRQ_NUM	6	
#define PWM1_IRQ_NUM	7	
#define	INT0_IRQ_NUM	8	
#define	INT1_IRQ_NUM	9	
#define	INT2_IRQ_NUM	10	
#define	INT3_IRQ_NUM	11	
#define UART2_IRQ_NUM	12		
#define PWM2_IRQ_NUM	13	
#define IRQ1_IRQ_NUM	16	
#define IRQ2_IRQ_NUM	17	
#define IRQ3_IRQ_NUM	18	
#define IRQ6_IRQ_NUM	19	
#define IRQ5_IRQ_NUM	20	
#define SPI1_IRQ_NUM	21	
#define SAM_IRQ_NUM	22	
#define EMIQ_IRQ_NUM	23	

#define SPI_IRQ_NUM	SPI2_IRQ_NUM

#define SPIM_IRQ_NUM	SPI_IRQ_NUM
#define TMR1_IRQ_NUM	TMR_IRQ_NUM
#define UART_IRQ_NUM	UART1_IRQ_NUM

#define IMR_MSPI 	(1 << SPI_IRQ_NUM)	
#define	IMR_MTMR	(1 << TMR_IRQ_NUM)	
#define IMR_MUART	(1 << UART_IRQ_NUM)		
#define	IMR_MWDT	(1 << WDT_IRQ_NUM)	
#define IMR_MRTC	(1 << RTC_IRQ_NUM)	
#define	IMR_MKB		(1 << KB_IRQ_NUM)	
#define IMR_MPWM	(1 << PWM_IRQ_NUM)	
#define	IMR_MINT0	(1 << INT0_IRQ_NUM)	
#define	IMR_MINT1	(1 << INT1_IRQ_NUM)	
#define	IMR_MINT2	(1 << INT2_IRQ_NUM)	
#define	IMR_MINT3	(1 << INT3_IRQ_NUM)	
#define IMR_MIRQ1	(1 << IRQ1_IRQ_NUM)	
#define IMR_MIRQ2	(1 << IRQ2_IRQ_NUM)	
#define IMR_MIRQ3	(1 << IRQ3_IRQ_NUM)	
#define IMR_MIRQ6	(1 << IRQ6_IRQ_NUM)	
#define IMR_MIRQ5	(1 << IRQ5_IRQ_NUM)	
#define IMR_MSAM	(1 << SAM_IRQ_NUM)	
#define IMR_MEMIQ	(1 << EMIQ_IRQ_NUM)	

#define IMR_MSPIM	IMR_MSPI
#define IMR_MTMR1	IMR_MTMR

#define ISR_ADDR	0xfffff30c
#define ISR		LONG_REF(ISR_ADDR)

#define ISR_SPI 	(1 << SPI_IRQ_NUM)	
#define	ISR_TMR		(1 << TMR_IRQ_NUM)	
#define ISR_UART	(1 << UART_IRQ_NUM)		
#define	ISR_WDT		(1 << WDT_IRQ_NUM)	
#define ISR_RTC		(1 << RTC_IRQ_NUM)	
#define	ISR_KB		(1 << KB_IRQ_NUM)	
#define ISR_PWM		(1 << PWM_IRQ_NUM)	
#define	ISR_INT0	(1 << INT0_IRQ_NUM)	
#define	ISR_INT1	(1 << INT1_IRQ_NUM)	
#define	ISR_INT2	(1 << INT2_IRQ_NUM)	
#define	ISR_INT3	(1 << INT3_IRQ_NUM)	
#define ISR_IRQ1	(1 << IRQ1_IRQ_NUM)	
#define ISR_IRQ2	(1 << IRQ2_IRQ_NUM)	
#define ISR_IRQ3	(1 << IRQ3_IRQ_NUM)	
#define ISR_IRQ6	(1 << IRQ6_IRQ_NUM)	
#define ISR_IRQ5	(1 << IRQ5_IRQ_NUM)	
#define ISR_SAM		(1 << SAM_IRQ_NUM)	
#define ISR_EMIQ	(1 << EMIQ_IRQ_NUM)	

#define ISR_SPIM	ISR_SPI
#define ISR_TMR1	ISR_TMR

#define IPR_ADDR	0xfffff30c
#define IPR		LONG_REF(IPR_ADDR)

#define IPR_SPI 	(1 << SPI_IRQ_NUM)	
#define	IPR_TMR		(1 << TMR_IRQ_NUM)	
#define IPR_UART	(1 << UART_IRQ_NUM)		
#define	IPR_WDT		(1 << WDT_IRQ_NUM)	
#define IPR_RTC		(1 << RTC_IRQ_NUM)	
#define	IPR_KB		(1 << KB_IRQ_NUM)	
#define IPR_PWM		(1 << PWM_IRQ_NUM)	
#define	IPR_INT0	(1 << INT0_IRQ_NUM)	
#define	IPR_INT1	(1 << INT1_IRQ_NUM)	
#define	IPR_INT2	(1 << INT2_IRQ_NUM)	
#define	IPR_INT3	(1 << INT3_IRQ_NUM)	
#define IPR_IRQ1	(1 << IRQ1_IRQ_NUM)	
#define IPR_IRQ2	(1 << IRQ2_IRQ_NUM)	
#define IPR_IRQ3	(1 << IRQ3_IRQ_NUM)	
#define IPR_IRQ6	(1 << IRQ6_IRQ_NUM)	
#define IPR_IRQ5	(1 << IRQ5_IRQ_NUM)	
#define IPR_SAM		(1 << SAM_IRQ_NUM)	
#define IPR_EMIQ	(1 << EMIQ_IRQ_NUM)	

#define IPR_SPIM	IPR_SPI
#define IPR_TMR1	IPR_TMR


#define PADIR_ADDR	0xfffff400		
#define PADATA_ADDR	0xfffff401		
#define PAPUEN_ADDR	0xfffff402		

#define PADIR		BYTE_REF(PADIR_ADDR)
#define PADATA		BYTE_REF(PADATA_ADDR)
#define PAPUEN		BYTE_REF(PAPUEN_ADDR)

#define PA(x)		(1 << (x))

#define PBDIR_ADDR	0xfffff408		
#define PBDATA_ADDR	0xfffff409		
#define PBPUEN_ADDR	0xfffff40a		
#define PBSEL_ADDR	0xfffff40b		

#define PBDIR		BYTE_REF(PBDIR_ADDR)
#define PBDATA		BYTE_REF(PBDATA_ADDR)
#define PBPUEN		BYTE_REF(PBPUEN_ADDR)
#define PBSEL		BYTE_REF(PBSEL_ADDR)

#define PB(x)		(1 << (x))

#define PB_CSB0		0x01	
#define PB_CSB1		0x02	
#define PB_CSC0_RAS0	0x04    	
#define PB_CSC1_RAS1	0x08    	
#define PB_CSD0_CAS0	0x10    	
#define PB_CSD1_CAS1	0x20    
#define PB_TIN_TOUT	0x40	
#define PB_PWMO		0x80	

#define PCDIR_ADDR	0xfffff410		
#define PCDATA_ADDR	0xfffff411		
#define PCPDEN_ADDR	0xfffff412		
#define PCSEL_ADDR	0xfffff413		

#define PCDIR		BYTE_REF(PCDIR_ADDR)
#define PCDATA		BYTE_REF(PCDATA_ADDR)
#define PCPDEN		BYTE_REF(PCPDEN_ADDR)
#define PCSEL		BYTE_REF(PCSEL_ADDR)

#define PC(x)		(1 << (x))

#define PC_LD0		0x01	
#define PC_LD1		0x02	
#define PC_LD2		0x04	
#define PC_LD3		0x08	
#define PC_LFLM		0x10	
#define PC_LLP 		0x20	
#define PC_LCLK		0x40	
#define PC_LACD		0x80	

#define PDDIR_ADDR	0xfffff418		
#define PDDATA_ADDR	0xfffff419		
#define PDPUEN_ADDR	0xfffff41a		
#define PDSEL_ADDR	0xfffff41b		
#define PDPOL_ADDR	0xfffff41c		
#define PDIRQEN_ADDR	0xfffff41d		
#define PDKBEN_ADDR	0xfffff41e		
#define	PDIQEG_ADDR	0xfffff41f		

#define PDDIR		BYTE_REF(PDDIR_ADDR)
#define PDDATA		BYTE_REF(PDDATA_ADDR)
#define PDPUEN		BYTE_REF(PDPUEN_ADDR)
#define PDSEL		BYTE_REF(PDSEL_ADDR)
#define	PDPOL		BYTE_REF(PDPOL_ADDR)
#define PDIRQEN		BYTE_REF(PDIRQEN_ADDR)
#define PDKBEN		BYTE_REF(PDKBEN_ADDR)
#define PDIQEG		BYTE_REF(PDIQEG_ADDR)

#define PD(x)		(1 << (x))

#define PD_INT0		0x01	
#define PD_INT1		0x02	
#define PD_INT2		0x04	
#define PD_INT3		0x08	
#define PD_IRQ1		0x10	
#define PD_IRQ2		0x20	
#define PD_IRQ3		0x40	
#define PD_IRQ6		0x80	

#define PEDIR_ADDR	0xfffff420		
#define PEDATA_ADDR	0xfffff421		
#define PEPUEN_ADDR	0xfffff422		
#define PESEL_ADDR	0xfffff423		

#define PEDIR		BYTE_REF(PEDIR_ADDR)
#define PEDATA		BYTE_REF(PEDATA_ADDR)
#define PEPUEN		BYTE_REF(PEPUEN_ADDR)
#define PESEL		BYTE_REF(PESEL_ADDR)

#define PE(x)		(1 << (x))

#define PE_SPMTXD	0x01	
#define PE_SPMRXD	0x02	
#define PE_SPMCLK	0x04	
#define PE_DWE		0x08	
#define PE_RXD		0x10	
#define PE_TXD		0x20	
#define PE_RTS		0x40	
#define PE_CTS		0x80	

#define PFDIR_ADDR	0xfffff428		
#define PFDATA_ADDR	0xfffff429		
#define PFPUEN_ADDR	0xfffff42a		
#define PFSEL_ADDR	0xfffff42b		

#define PFDIR		BYTE_REF(PFDIR_ADDR)
#define PFDATA		BYTE_REF(PFDATA_ADDR)
#define PFPUEN		BYTE_REF(PFPUEN_ADDR)
#define PFSEL		BYTE_REF(PFSEL_ADDR)

#define PF(x)		(1 << (x))

#define PF_LCONTRAST	0x01	
#define PF_IRQ5         0x02    
#define PF_CLKO         0x04    
#define PF_A20          0x08    
#define PF_A21          0x10    
#define PF_A22          0x20    
#define PF_A23          0x40    
#define PF_CSA1		0x80    

#define PGDIR_ADDR	0xfffff430		
#define PGDATA_ADDR	0xfffff431		
#define PGPUEN_ADDR	0xfffff432		
#define PGSEL_ADDR	0xfffff433		

#define PGDIR		BYTE_REF(PGDIR_ADDR)
#define PGDATA		BYTE_REF(PGDATA_ADDR)
#define PGPUEN		BYTE_REF(PGPUEN_ADDR)
#define PGSEL		BYTE_REF(PGSEL_ADDR)

#define PG(x)		(1 << (x))

#define PG_BUSW_DTACK	0x01	
#define PG_A0		0x02	
#define PG_EMUIRQ	0x04	
#define PG_HIZ_P_D	0x08	
#define PG_EMUCS        0x10	
#define PG_EMUBRK	0x20	

#define PJDIR_ADDR	0xfffff438		
#define PJDATA_ADDR	0xfffff439		
#define PJPUEN_ADDR	0xfffff43A		
#define PJSEL_ADDR	0xfffff43B		

#define PJDIR		BYTE_REF(PJDIR_ADDR)
#define PJDATA		BYTE_REF(PJDATA_ADDR)
#define PJPUEN		BYTE_REF(PJPUEN_ADDR)
#define PJSEL		BYTE_REF(PJSEL_ADDR)

#define PJ(x)		(1 << (x))

#define PKDIR_ADDR	0xfffff440		
#define PKDATA_ADDR	0xfffff441		
#define PKPUEN_ADDR	0xfffff442		
#define PKSEL_ADDR	0xfffff443		

#define PKDIR		BYTE_REF(PKDIR_ADDR)
#define PKDATA		BYTE_REF(PKDATA_ADDR)
#define PKPUEN		BYTE_REF(PKPUEN_ADDR)
#define PKSEL		BYTE_REF(PKSEL_ADDR)

#define PK(x)		(1 << (x))

#define PK_DATAREADY		0x01	
#define PK_PWM2		0x01	
#define PK_R_W		0x02	
#define PK_LDS		0x04	
#define PK_UDS		0x08	
#define PK_LD4		0x10	
#define PK_LD5 		0x20	
#define PK_LD6		0x40	
#define PK_LD7		0x80	

#define PJDIR_ADDR	0xfffff438		
#define PJDATA_ADDR	0xfffff439		
#define PJPUEN_ADDR	0xfffff43A		
#define PJSEL_ADDR	0xfffff43B		

#define PJDIR		BYTE_REF(PJDIR_ADDR)
#define PJDATA		BYTE_REF(PJDATA_ADDR)
#define PJPUEN		BYTE_REF(PJPUEN_ADDR)
#define PJSEL		BYTE_REF(PJSEL_ADDR)

#define PJ(x)		(1 << (x))

#define PJ_MOSI 	0x01	
#define PJ_MISO		0x02	
#define PJ_SPICLK1  	0x04	
#define PJ_SS   	0x08	
#define PJ_RXD2         0x10	
#define PJ_TXD2  	0x20	
#define PJ_RTS2  	0x40	
#define PJ_CTS2  	0x80	

#define PMDIR_ADDR	0xfffff448		
#define PMDATA_ADDR	0xfffff449		
#define PMPUEN_ADDR	0xfffff44a		
#define PMSEL_ADDR	0xfffff44b		

#define PMDIR		BYTE_REF(PMDIR_ADDR)
#define PMDATA		BYTE_REF(PMDATA_ADDR)
#define PMPUEN		BYTE_REF(PMPUEN_ADDR)
#define PMSEL		BYTE_REF(PMSEL_ADDR)

#define PM(x)		(1 << (x))

#define PM_SDCLK	0x01	
#define PM_SDCE		0x02	
#define PM_DQMH 	0x04	
#define PM_DQML 	0x08	
#define PM_SDA10        0x10	
#define PM_DMOE 	0x20	


#define PWMC_ADDR	0xfffff500
#define PWMC		WORD_REF(PWMC_ADDR)

#define PWMC_CLKSEL_MASK	0x0003	
#define PWMC_CLKSEL_SHIFT	0
#define PWMC_REPEAT_MASK	0x000c	
#define PWMC_REPEAT_SHIFT	2
#define PWMC_EN			0x0010	
#define PMNC_FIFOAV		0x0020	
#define PWMC_IRQEN		0x0040	
#define PWMC_IRQ		0x0080	
#define PWMC_PRESCALER_MASK	0x7f00	
#define PWMC_PRESCALER_SHIFT	8
#define PWMC_CLKSRC		0x8000	

#define PWMC_PWMEN	PWMC_EN

#define PWMS_ADDR	0xfffff502
#define PWMS		WORD_REF(PWMS_ADDR)

#define PWMP_ADDR	0xfffff504
#define PWMP		BYTE_REF(PWMP_ADDR)

#define PWMCNT_ADDR	0xfffff505
#define PWMCNT		BYTE_REF(PWMCNT_ADDR)


#define TCTL_ADDR	0xfffff600
#define TCTL		WORD_REF(TCTL_ADDR)

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

#define TCTL1_ADDR	TCTL_ADDR
#define TCTL1		TCTL

#define TPRER_ADDR	0xfffff602
#define TPRER		WORD_REF(TPRER_ADDR)

#define TPRER1_ADDR	TPRER_ADDR
#define TPRER1		TPRER

#define TCMP_ADDR	0xfffff604
#define TCMP		WORD_REF(TCMP_ADDR)

#define TCMP1_ADDR	TCMP_ADDR
#define TCMP1		TCMP

#define TCR_ADDR	0xfffff606
#define TCR		WORD_REF(TCR_ADDR)

#define TCR1_ADDR	TCR_ADDR
#define TCR1		TCR

#define TCN_ADDR	0xfffff608
#define TCN		WORD_REF(TCN_ADDR)

#define TCN1_ADDR	TCN_ADDR
#define TCN1		TCN

#define TSTAT_ADDR	0xfffff60a
#define TSTAT		WORD_REF(TSTAT_ADDR)

#define TSTAT_COMP	0x0001		
#define TSTAT_CAPT	0x0001		

#define TSTAT1_ADDR	TSTAT_ADDR
#define TSTAT1		TSTAT


#define SPIMDATA_ADDR	0xfffff800
#define SPIMDATA	WORD_REF(SPIMDATA_ADDR)

#define SPIMCONT_ADDR	0xfffff802
#define SPIMCONT	WORD_REF(SPIMCONT_ADDR)

#define SPIMCONT_BIT_COUNT_MASK	 0x000f	
#define SPIMCONT_BIT_COUNT_SHIFT 0
#define SPIMCONT_POL		 0x0010	
#define	SPIMCONT_PHA		 0x0020	
#define SPIMCONT_IRQEN		 0x0040 
#define SPIMCONT_IRQ		 0x0080	
#define SPIMCONT_XCH		 0x0100	
#define SPIMCONT_ENABLE		 0x0200	
#define SPIMCONT_DATA_RATE_MASK	 0xe000	
#define SPIMCONT_DATA_RATE_SHIFT 13

#define SPIMCONT_SPIMIRQ	SPIMCONT_IRQ
#define SPIMCONT_SPIMEN		SPIMCONT_ENABLE



#define USTCNT_ADDR	0xfffff900
#define USTCNT		WORD_REF(USTCNT_ADDR)

#define USTCNT_TXAE	0x0001	
#define USTCNT_TXHE	0x0002	
#define USTCNT_TXEE	0x0004	
#define USTCNT_RXRE	0x0008	
#define USTCNT_RXHE	0x0010	
#define USTCNT_RXFE	0x0020	
#define USTCNT_CTSD	0x0040	
#define USTCNT_ODEN	0x0080	
#define USTCNT_8_7	0x0100	
#define USTCNT_STOP	0x0200	
#define USTCNT_ODD	0x0400	
#define	USTCNT_PEN	0x0800	
#define USTCNT_CLKM	0x1000	
#define	USTCNT_TXEN	0x2000	
#define USTCNT_RXEN	0x4000	
#define USTCNT_UEN	0x8000	

#define USTCNT_TXAVAILEN	USTCNT_TXAE
#define USTCNT_TXHALFEN		USTCNT_TXHE
#define USTCNT_TXEMPTYEN	USTCNT_TXEE
#define USTCNT_RXREADYEN	USTCNT_RXRE
#define USTCNT_RXHALFEN		USTCNT_RXHE
#define USTCNT_RXFULLEN		USTCNT_RXFE
#define USTCNT_CTSDELTAEN	USTCNT_CTSD
#define USTCNT_ODD_EVEN		USTCNT_ODD
#define USTCNT_PARITYEN		USTCNT_PEN
#define USTCNT_CLKMODE		USTCNT_CLKM
#define USTCNT_UARTEN		USTCNT_UEN

#define UBAUD_ADDR	0xfffff902
#define UBAUD		WORD_REF(UBAUD_ADDR)

#define UBAUD_PRESCALER_MASK	0x003f	
#define UBAUD_PRESCALER_SHIFT	0
#define UBAUD_DIVIDE_MASK	0x0700	
#define UBAUD_DIVIDE_SHIFT	8
#define UBAUD_BAUD_SRC		0x0800	
#define UBAUD_UCLKDIR		0x2000	

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
#define URX_OLD_DATA	 0x1000	
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
#define UTX_CTS_STAT	 0x0200	
#define	UTX_BUSY	 0x0400	
#define	UTX_NOCTS	 0x0800	
#define UTX_SEND_BREAK	 0x1000	
#define UTX_TX_AVAIL	 0x2000	
#define UTX_FIFO_HALF	 0x4000	
#define UTX_FIFO_EMPTY	 0x8000	

#define UTX_CTS_STATUS	UTX_CTS_STAT
#define UTX_IGNORE_CTS	UTX_NOCTS

#define UMISC_ADDR	0xfffff908
#define UMISC		WORD_REF(UMISC_ADDR)

#define UMISC_TX_POL	 0x0004	
#define UMISC_RX_POL	 0x0008	
#define UMISC_IRDA_LOOP	 0x0010	
#define UMISC_IRDA_EN	 0x0020	
#define UMISC_RTS	 0x0040	
#define UMISC_RTSCONT	 0x0080	
#define UMISC_IR_TEST	 0x0400	
#define UMISC_BAUD_RESET 0x0800	
#define UMISC_LOOP	 0x1000	
#define UMISC_FORCE_PERR 0x2000	
#define UMISC_CLKSRC	 0x4000	
#define UMISC_BAUD_TEST	 0x8000	

#define NIPR_ADDR	0xfffff90a
#define NIPR		WORD_REF(NIPR_ADDR)

#define NIPR_STEP_VALUE_MASK	0x00ff	
#define NIPR_STEP_VALUE_SHIFT	0
#define NIPR_SELECT_MASK	0x0700	
#define NIPR_SELECT_SHIFT	8
#define NIPR_PRE_SEL		0x8000	


typedef struct {
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
  volatile unsigned short int nipr;
  volatile unsigned short int hmark;
  volatile unsigned short int unused;
} __attribute__((packed)) m68328_uart;





#define LSSA_ADDR	0xfffffa00
#define LSSA		LONG_REF(LSSA_ADDR)

#define LSSA_SSA_MASK	0x1ffffffe	

#define LVPW_ADDR	0xfffffa05
#define LVPW		BYTE_REF(LVPW_ADDR)

#define LXMAX_ADDR	0xfffffa08
#define LXMAX		WORD_REF(LXMAX_ADDR)

#define LXMAX_XM_MASK	0x02f0		

#define LYMAX_ADDR	0xfffffa0a
#define LYMAX		WORD_REF(LYMAX_ADDR)

#define LYMAX_YM_MASK	0x01ff		

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

#define LPICF_GS_MASK	 0x03	 
#define	  LPICF_GS_BW	   0x00
#define   LPICF_GS_GRAY_4  0x01
#define   LPICF_GS_GRAY_16 0x02
#define LPICF_PBSIZ_MASK 0x0c	
#define   LPICF_PBSIZ_1	   0x00
#define   LPICF_PBSIZ_2    0x04
#define   LPICF_PBSIZ_4    0x08

#define LPOLCF_ADDR	0xfffffa21
#define LPOLCF		BYTE_REF(LPOLCF_ADDR)

#define LPOLCF_PIXPOL	0x01	
#define LPOLCF_LPPOL	0x02	
#define LPOLCF_FLMPOL	0x04	
#define LPOLCF_LCKPOL	0x08	

#define LACDRC_ADDR	0xfffffa23
#define LACDRC		BYTE_REF(LACDRC_ADDR)

#define LACDRC_ACDSLT	 0x80	
#define LACDRC_ACD_MASK	 0x0f	
#define LACDRC_ACD_SHIFT 0

#define LPXCD_ADDR	0xfffffa25
#define LPXCD		BYTE_REF(LPXCD_ADDR)

#define	LPXCD_PCD_MASK	0x3f 	
#define LPXCD_PCD_SHIFT	0

#define LCKCON_ADDR	0xfffffa27
#define LCKCON		BYTE_REF(LCKCON_ADDR)

#define LCKCON_DWS_MASK	 0x0f	
#define LCKCON_DWS_SHIFT 0
#define LCKCON_DWIDTH	 0x40	
#define LCKCON_LCDON	 0x80	

#define LCKCON_DW_MASK  LCKCON_DWS_MASK
#define LCKCON_DW_SHIFT LCKCON_DWS_SHIFT
 
#define LRRA_ADDR	0xfffffa29
#define LRRA		BYTE_REF(LRRA_ADDR)

#define LPOSR_ADDR	0xfffffa2d
#define LPOSR		BYTE_REF(LPOSR_ADDR)

#define LPOSR_POS_MASK	0x0f	
#define LPOSR_POS_SHIFT	0

#define LFRCM_ADDR	0xfffffa31
#define LFRCM		BYTE_REF(LFRCM_ADDR)

#define LFRCM_YMOD_MASK	 0x0f	
#define LFRCM_YMOD_SHIFT 0
#define LFRCM_XMOD_MASK	 0xf0	
#define LFRCM_XMOD_SHIFT 4

#define LGPMR_ADDR	0xfffffa33
#define LGPMR		BYTE_REF(LGPMR_ADDR)

#define LGPMR_G1_MASK	0x0f
#define LGPMR_G1_SHIFT	0
#define LGPMR_G2_MASK	0xf0
#define LGPMR_G2_SHIFT	4

#define PWMR_ADDR	0xfffffa36
#define PWMR		WORD_REF(PWMR_ADDR)

#define PWMR_PW_MASK	0x00ff	
#define PWMR_PW_SHIFT	0
#define PWMR_CCPEN	0x0100	
#define PWMR_SRC_MASK	0x0600	
#define   PWMR_SRC_LINE	  0x0000	
#define   PWMR_SRC_PIXEL  0x0200	
#define   PWMR_SRC_LCD    0x4000	


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

#define WATCHDOG_ADDR	0xfffffb0a
#define WATCHDOG	WORD_REF(WATCHDOG_ADDR)

#define WATCHDOG_EN	0x0001	
#define WATCHDOG_ISEL	0x0002	
#define WATCHDOG_INTF	0x0080	
#define WATCHDOG_CNT_MASK  0x0300	
#define WATCHDOG_CNT_SHIFT 8

#define RTCCTL_ADDR	0xfffffb0c
#define RTCCTL		WORD_REF(RTCCTL_ADDR)

#define RTCCTL_XTL	0x0020	
#define RTCCTL_EN	0x0080	

#define RTCCTL_384	RTCCTL_XTL
#define RTCCTL_ENABLE	RTCCTL_EN

#define RTCISR_ADDR	0xfffffb0e
#define RTCISR		WORD_REF(RTCISR_ADDR)

#define RTCISR_SW	0x0001	
#define RTCISR_MIN	0x0002	
#define RTCISR_ALM	0x0004	
#define RTCISR_DAY	0x0008	
#define RTCISR_1HZ	0x0010	
#define RTCISR_HR	0x0020	
#define RTCISR_SAM0	0x0100	 
#define RTCISR_SAM1	0x0200	 
#define RTCISR_SAM2	0x0400	 
#define RTCISR_SAM3	0x0800	 
#define RTCISR_SAM4	0x1000	 
#define RTCISR_SAM5	0x2000	 
#define RTCISR_SAM6	0x4000	 
#define RTCISR_SAM7	0x8000	 

#define RTCIENR_ADDR	0xfffffb10
#define RTCIENR		WORD_REF(RTCIENR_ADDR)

#define RTCIENR_SW	0x0001	
#define RTCIENR_MIN	0x0002	
#define RTCIENR_ALM	0x0004	
#define RTCIENR_DAY	0x0008	
#define RTCIENR_1HZ	0x0010	
#define RTCIENR_HR	0x0020	
#define RTCIENR_SAM0	0x0100	 
#define RTCIENR_SAM1	0x0200	 
#define RTCIENR_SAM2	0x0400	 
#define RTCIENR_SAM3	0x0800	 
#define RTCIENR_SAM4	0x1000	 
#define RTCIENR_SAM5	0x2000	 
#define RTCIENR_SAM6	0x4000	 
#define RTCIENR_SAM7	0x8000	 

#define STPWCH_ADDR	0xfffffb12
#define STPWCH		WORD_REF(STPWCH_ADDR)

#define STPWCH_CNT_MASK	 0x003f	
#define SPTWCH_CNT_SHIFT 0

#define DAYR_ADDR	0xfffffb1a
#define DAYR		WORD_REF(DAYR_ADDR)

#define DAYR_DAYS_MASK	0x1ff	
#define DAYR_DAYS_SHIFT 0

#define DAYALARM_ADDR	0xfffffb1c
#define DAYALARM	WORD_REF(DAYALARM_ADDR)

#define DAYALARM_DAYSAL_MASK	0x01ff	
#define DAYALARM_DAYSAL_SHIFT 	0


#define DRAMMC_ADDR	0xfffffc00
#define DRAMMC		WORD_REF(DRAMMC_ADDR)

#define DRAMMC_ROW12_MASK	0xc000	
#define   DRAMMC_ROW12_PA10	0x0000
#define   DRAMMC_ROW12_PA21	0x4000	
#define   DRAMMC_ROW12_PA23	0x8000
#define	DRAMMC_ROW0_MASK	0x3000	
#define	  DRAMMC_ROW0_PA11	0x0000
#define   DRAMMC_ROW0_PA22	0x1000
#define   DRAMMC_ROW0_PA23	0x2000
#define DRAMMC_ROW11		0x0800	
#define DRAMMC_ROW10		0x0400	
#define	DRAMMC_ROW9		0x0200	
#define DRAMMC_ROW8		0x0100	
#define DRAMMC_COL10		0x0080	
#define DRAMMC_COL9		0x0040	
#define DRAMMC_COL8		0x0020	
#define DRAMMC_REF_MASK		0x001f	
#define DRAMMC_REF_SHIFT	0

#define DRAMC_ADDR	0xfffffc02
#define DRAMC		WORD_REF(DRAMC_ADDR)

#define DRAMC_DWE	   0x0001	
#define DRAMC_RST	   0x0002	
#define DRAMC_LPR	   0x0004	
#define DRAMC_SLW	   0x0008	
#define DRAMC_LSP	   0x0010	
#define DRAMC_MSW	   0x0020	
#define DRAMC_WS_MASK	   0x00c0	
#define DRAMC_WS_SHIFT	   6
#define DRAMC_PGSZ_MASK    0x0300	
#define DRAMC_PGSZ_SHIFT   8
#define   DRAMC_PGSZ_256K  0x0000	
#define   DRAMC_PGSZ_512K  0x0100
#define   DRAMC_PGSZ_1024K 0x0200
#define	  DRAMC_PGSZ_2048K 0x0300
#define DRAMC_EDO	   0x0400	
#define DRAMC_CLK	   0x0800	
#define DRAMC_BC_MASK	   0x3000	
#define DRAMC_BC_SHIFT	   12
#define DRAMC_RM	   0x4000	
#define DRAMC_EN	   0x8000	



#define ICEMACR_ADDR	0xfffffd00
#define ICEMACR		LONG_REF(ICEMACR_ADDR)

#define ICEMAMR_ADDR	0xfffffd04
#define ICEMAMR		LONG_REF(ICEMAMR_ADDR)

#define ICEMCCR_ADDR	0xfffffd08
#define ICEMCCR		WORD_REF(ICEMCCR_ADDR)

#define ICEMCCR_PD	0x0001	
#define ICEMCCR_RW	0x0002	

#define ICEMCMR_ADDR	0xfffffd0a
#define ICEMCMR		WORD_REF(ICEMCMR_ADDR)

#define ICEMCMR_PDM	0x0001	
#define ICEMCMR_RWM	0x0002	

#define ICEMCR_ADDR	0xfffffd0c
#define ICEMCR		WORD_REF(ICEMCR_ADDR)

#define ICEMCR_CEN	0x0001	
#define ICEMCR_PBEN	0x0002	
#define ICEMCR_SB	0x0004	
#define ICEMCR_HMDIS	0x0008	
#define ICEMCR_BBIEN	0x0010	

#define ICEMSR_ADDR	0xfffffd0e
#define ICEMSR		WORD_REF(ICEMSR_ADDR)

#define ICEMSR_EMUEN	0x0001	
#define ICEMSR_BRKIRQ	0x0002	
#define ICEMSR_BBIRQ	0x0004	
#define ICEMSR_EMIRQ	0x0008	

#endif 
