
/*
 *	linux/arch/m68knommu/platform/532x/config.c
 *
 *	Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2000, Lineo (www.lineo.com)
 *	Yaroslav Vinogradov yaroslav.vinogradov@freescale.com
 *	Copyright Freescale Semiconductor, Inc 2006
 *	Copyright (c) 2006, emlix, Sebastian Hess <sh@emlix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>
#include <asm/mcfdma.h>
#include <asm/mcfwdebug.h>


#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)

static void __init m532x_qspi_init(void)
{
	
	writew(0x01f0, MCF_GPIO_PAR_QSPI);
}

#endif 


static void __init m532x_uarts_init(void)
{
	
	MCF_GPIO_PAR_UART |= 0x0FFF;
}


static void __init m532x_fec_init(void)
{
	
	MCF_GPIO_PAR_FECI2C |= (MCF_GPIO_PAR_FECI2C_PAR_MDC_EMDC |
		MCF_GPIO_PAR_FECI2C_PAR_MDIO_EMDIO);
	MCF_GPIO_PAR_FEC = (MCF_GPIO_PAR_FEC_PAR_FEC_7W_FEC |
		MCF_GPIO_PAR_FEC_PAR_FEC_MII_FEC);
}


void __init config_BSP(char *commandp, int size)
{
#if !defined(CONFIG_BOOTPARAM)
	
	memcpy(commandp, (char *) 0x4000, 4);
	if(strncmp(commandp, "kcl ", 4) == 0){
		memcpy(commandp, (char *) 0x4004, size);
		commandp[size-1] = 0;
	} else {
		memset(commandp, 0, size);
	}
#endif

	mach_sched_init = hw_timer_init;
	m532x_uarts_init();
	m532x_fec_init();
#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)
	m532x_qspi_init();
#endif

#ifdef CONFIG_BDM_DISABLE
	wdebug(MCFDEBUG_CSR, MCFDEBUG_CSR_PSTCLK);
#endif
}

#define MAX_FVCO	500000	
#define MAX_FSYS	80000 	
#define MIN_FSYS	58333 	
#define FREF		16000   


#define MAX_MFD		135     
#define MIN_MFD		88      
#define BUSDIV		6       

#define MIN_LPD		(1 << 0)    
#define MAX_LPD		(1 << 15)   
#define DEFAULT_LPD	(1 << 1)	

#define SYS_CLK_KHZ	80000
#define SYSTEM_PERIOD	12.5
  
#define SDRAM_BL	8	
#define SDRAM_TWR	2	
#define SDRAM_CASL	2.5	
#define SDRAM_TRCD	2	
#define SDRAM_TRP	2	
#define SDRAM_TRFC	7	
#define SDRAM_TREFI	7800	

#define EXT_SRAM_ADDRESS	(0xC0000000)
#define FLASH_ADDRESS		(0x00000000)
#define SDRAM_ADDRESS		(0x40000000)

#define NAND_FLASH_ADDRESS	(0xD0000000)

int sys_clk_khz = 0;
int sys_clk_mhz = 0;

void wtm_init(void);
void scm_init(void);
void gpio_init(void);
void fbcs_init(void);
void sdramc_init(void);
int  clock_pll (int fsys, int flags);
int  clock_limp (int);
int  clock_exit_limp (void);
int  get_sys_clock (void);

asmlinkage void __init sysinit(void)
{
	sys_clk_khz = clock_pll(0, 0);
	sys_clk_mhz = sys_clk_khz/1000;
	
	wtm_init();
	scm_init();
	gpio_init();
	fbcs_init();
	sdramc_init();
}

void wtm_init(void)
{
	
	MCF_WTM_WCR = 0;
}

#define MCF_SCM_BCR_GBW		(0x00000100)
#define MCF_SCM_BCR_GBR		(0x00000200)

void scm_init(void)
{
	
	MCF_SCM_MPR = 0x77777777;
    
	MCF_SCM_PACRA = 0;
	MCF_SCM_PACRB = 0;
	MCF_SCM_PACRC = 0;
	MCF_SCM_PACRD = 0;
	MCF_SCM_PACRE = 0;
	MCF_SCM_PACRF = 0;

	
	MCF_SCM_BCR = (MCF_SCM_BCR_GBR | MCF_SCM_BCR_GBW);
}


void fbcs_init(void)
{
	MCF_GPIO_PAR_CS = 0x0000003E;

	
	MCF_FBCS1_CSAR = 0x10080000;

	MCF_FBCS1_CSCR = 0x002A3780;
	MCF_FBCS1_CSMR = (MCF_FBCS_CSMR_BAM_2M | MCF_FBCS_CSMR_V);

	
	*((u16 *)(0x10080000)) = 0xFFFF;

	
	MCF_FBCS1_CSAR = EXT_SRAM_ADDRESS;
	MCF_FBCS1_CSCR = (MCF_FBCS_CSCR_PS_16
			| MCF_FBCS_CSCR_AA
			| MCF_FBCS_CSCR_SBM
			| MCF_FBCS_CSCR_WS(1));
	MCF_FBCS1_CSMR = (MCF_FBCS_CSMR_BAM_512K
			| MCF_FBCS_CSMR_V);

	
	MCF_FBCS0_CSAR = FLASH_ADDRESS;
	MCF_FBCS0_CSCR = (MCF_FBCS_CSCR_PS_16
			| MCF_FBCS_CSCR_BEM
			| MCF_FBCS_CSCR_AA
			| MCF_FBCS_CSCR_SBM
			| MCF_FBCS_CSCR_WS(7));
	MCF_FBCS0_CSMR = (MCF_FBCS_CSMR_BAM_32M
			| MCF_FBCS_CSMR_V);
}

void sdramc_init(void)
{
	if (!(MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)) {
		
		
		
		MCF_SDRAMC_SDCS0 = (0
			| MCF_SDRAMC_SDCS_BA(SDRAM_ADDRESS)
			| MCF_SDRAMC_SDCS_CSSZ(MCF_SDRAMC_SDCS_CSSZ_32MBYTE));

	MCF_SDRAMC_SDCFG1 = (0
		| MCF_SDRAMC_SDCFG1_SRD2RW((int)((SDRAM_CASL + 2) + 0.5 ))
		| MCF_SDRAMC_SDCFG1_SWT2RD(SDRAM_TWR + 1)
		| MCF_SDRAMC_SDCFG1_RDLAT((int)((SDRAM_CASL*2) + 2))
		| MCF_SDRAMC_SDCFG1_ACT2RW((int)((SDRAM_TRCD ) + 0.5))
		| MCF_SDRAMC_SDCFG1_PRE2ACT((int)((SDRAM_TRP ) + 0.5))
		| MCF_SDRAMC_SDCFG1_REF2ACT((int)(((SDRAM_TRFC) ) + 0.5))
		| MCF_SDRAMC_SDCFG1_WTLAT(3));
	MCF_SDRAMC_SDCFG2 = (0
		| MCF_SDRAMC_SDCFG2_BRD2PRE(SDRAM_BL/2 + 1)
		| MCF_SDRAMC_SDCFG2_BWT2RW(SDRAM_BL/2 + SDRAM_TWR)
		| MCF_SDRAMC_SDCFG2_BRD2WT((int)((SDRAM_CASL+SDRAM_BL/2-1.0)+0.5))
		| MCF_SDRAMC_SDCFG2_BL(SDRAM_BL-1));

            
        MCF_SDRAMC_SDCR = (0
		| MCF_SDRAMC_SDCR_MODE_EN
		| MCF_SDRAMC_SDCR_CKE
		| MCF_SDRAMC_SDCR_DDR
		| MCF_SDRAMC_SDCR_MUX(1)
		| MCF_SDRAMC_SDCR_RCNT((int)(((SDRAM_TREFI/(SYSTEM_PERIOD*64)) - 1) + 0.5))
		| MCF_SDRAMC_SDCR_PS_16
		| MCF_SDRAMC_SDCR_IPALL);            

	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LEMR
		| MCF_SDRAMC_SDMR_AD(0x0)
		| MCF_SDRAMC_SDMR_CMD);

	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LMR
		| MCF_SDRAMC_SDMR_AD(0x163)
		| MCF_SDRAMC_SDMR_CMD);

	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IPALL;

	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IREF;
	MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_IREF;

	MCF_SDRAMC_SDMR = (0
		| MCF_SDRAMC_SDMR_BNKAD_LMR
		| MCF_SDRAMC_SDMR_AD(0x063)
		| MCF_SDRAMC_SDMR_CMD);
				
	MCF_SDRAMC_SDCR &= ~MCF_SDRAMC_SDCR_MODE_EN;
	MCF_SDRAMC_SDCR |= (0
		| MCF_SDRAMC_SDCR_REF
		| MCF_SDRAMC_SDCR_DQS_OE(0xC));
	}
}

void gpio_init(void)
{
	
	MCF_GPIO_PAR_UART = ( 0
		| MCF_GPIO_PAR_UART_PAR_URXD0
		| MCF_GPIO_PAR_UART_PAR_UTXD0);

	MCF_GPIO_PAR_TIMER = 0x00;
	__raw_writeb(0x08, MCFGPIO_PDDR_TIMER);
	__raw_writeb(0x00, MCFGPIO_PCLRR_TIMER);

}

int clock_pll(int fsys, int flags)
{
	int fref, temp, fout, mfd;
	u32 i;

	fref = FREF;
        
	if (fsys == 0) {
		
		mfd = MCF_PLL_PFDR;

		return (fref * mfd / (BUSDIV * 4));
	}

	
	if (fsys > MAX_FSYS)
		fsys = MAX_FSYS;
	if (fsys < MIN_FSYS)
		fsys = MIN_FSYS;

	temp = 100 * fsys / fref;
	mfd = 4 * BUSDIV * temp / 100;
    	    	    	
	
	fout = (fref * mfd / (BUSDIV * 4));

	if (MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)
		
		MCF_SDRAMC_SDCR &= ~MCF_SDRAMC_SDCR_CKE;


	
	clock_limp(DEFAULT_LPD);
     					
	
	MCF_PLL_PODR = (0
		| MCF_PLL_PODR_CPUDIV(BUSDIV/3)
		| MCF_PLL_PODR_BUSDIV(BUSDIV));
						
	MCF_PLL_PFDR = mfd;
		
	
	clock_exit_limp();
	
	if (MCF_SDRAMC_SDCR & MCF_SDRAMC_SDCR_REF)
		
		MCF_SDRAMC_SDCR |= MCF_SDRAMC_SDCR_CKE;

	
	MCF_SDRAMC_LIMP_FIX = MCF_SDRAMC_REFRESH;

	
	for (i = 0; i < 0x200; i++)
		;

	return fout;
}

int clock_limp(int div)
{
	u32 temp;

	
	if (div < MIN_LPD)
		div = MIN_LPD;
	if (div > MAX_LPD)
		div = MAX_LPD;
    
	temp = (MCF_CCM_CDR & MCF_CCM_CDR_SSIDIV(0xF));
      
	
	MCF_CCM_CDR = ( 0
		| MCF_CCM_CDR_LPDIV(div)
		| MCF_CCM_CDR_SSIDIV(temp));
    
	MCF_CCM_MISCCR |= MCF_CCM_MISCCR_LIMP;
    
	return (FREF/(3*(1 << div)));
}

int clock_exit_limp(void)
{
	int fout;
	
	
	MCF_CCM_MISCCR = (MCF_CCM_MISCCR & ~ MCF_CCM_MISCCR_LIMP);

	
	while (!(MCF_CCM_MISCCR & MCF_CCM_MISCCR_PLL_LOCK))
		;
	
	fout = get_sys_clock();

	return fout;
}

int get_sys_clock(void)
{
	int divider;
	
	
	if (MCF_CCM_MISCCR & MCF_CCM_MISCCR_LIMP) {
		divider = MCF_CCM_CDR & MCF_CCM_CDR_LPDIV(0xF);
		return (FREF/(2 << divider));
	}
	else
		return ((FREF * MCF_PLL_PFDR) / (BUSDIV * 4));
}
