/*
 *  pxa2xx_ssp.h
 *
 *  Copyright (C) 2003 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver supports the following PXA CPU/SSP ports:-
 *
 *       PXA250     SSP
 *       PXA255     SSP, NSSP
 *       PXA26x     SSP, NSSP, ASSP
 *       PXA27x     SSP1, SSP2, SSP3
 *       PXA3xx     SSP1, SSP2, SSP3, SSP4
 */

#ifndef __LINUX_SSP_H
#define __LINUX_SSP_H

#include <linux/list.h>
#include <linux/io.h>


#define SSCR0		(0x00)  
#define SSCR1		(0x04)  
#define SSSR		(0x08)  
#define SSITR		(0x0C)  
#define SSDR		(0x10)  

#define SSTO		(0x28)  
#define SSPSP		(0x2C)  
#define SSTSA		(0x30)  
#define SSRSA		(0x34)  
#define SSTSS		(0x38)  
#define SSACD		(0x3C)  
#define SSACDD		(0x40)	

#define SSCR0_DSS	(0x0000000f)	
#define SSCR0_DataSize(x)  ((x) - 1)	
#define SSCR0_FRF	(0x00000030)	
#define SSCR0_Motorola	(0x0 << 4)	
#define SSCR0_TI	(0x1 << 4)	
#define SSCR0_National	(0x2 << 4)	
#define SSCR0_ECS	(1 << 6)	
#define SSCR0_SSE	(1 << 7)	
#define SSCR0_SCR(x)	((x) << 8)	

#define SSCR0_EDSS	(1 << 20)	
#define SSCR0_NCS	(1 << 21)	
#define SSCR0_RIM	(1 << 22)	
#define SSCR0_TUM	(1 << 23)	
#define SSCR0_FRDC	(0x07000000)	
#define SSCR0_SlotsPerFrm(x) (((x) - 1) << 24)	
#define SSCR0_FPCKE	(1 << 29)	
#define SSCR0_ACS	(1 << 30)	
#define SSCR0_MOD	(1 << 31)	


#define SSCR1_RIE	(1 << 0)	
#define SSCR1_TIE	(1 << 1)	
#define SSCR1_LBM	(1 << 2)	
#define SSCR1_SPO	(1 << 3)	
#define SSCR1_SPH	(1 << 4)	
#define SSCR1_MWDS	(1 << 5)	

#define SSSR_ALT_FRM_MASK	3	
#define SSSR_TNF	(1 << 2)	
#define SSSR_RNE	(1 << 3)	
#define SSSR_BSY	(1 << 4)	
#define SSSR_TFS	(1 << 5)	
#define SSSR_RFS	(1 << 6)	
#define SSSR_ROR	(1 << 7)	

#ifdef CONFIG_ARCH_PXA
#define RX_THRESH_DFLT	8
#define TX_THRESH_DFLT	8

#define SSSR_TFL_MASK	(0xf << 8)	
#define SSSR_RFL_MASK	(0xf << 12)	

#define SSCR1_TFT	(0x000003c0)	
#define SSCR1_TxTresh(x) (((x) - 1) << 6) 
#define SSCR1_RFT	(0x00003c00)	
#define SSCR1_RxTresh(x) (((x) - 1) << 10) 

#else

#define RX_THRESH_DFLT	2
#define TX_THRESH_DFLT	2

#define SSSR_TFL_MASK	(0x3 << 8)	
#define SSSR_RFL_MASK	(0x3 << 12)	

#define SSCR1_TFT	(0x000000c0)	
#define SSCR1_TxTresh(x) (((x) - 1) << 6) 
#define SSCR1_RFT	(0x00000c00)	
#define SSCR1_RxTresh(x) (((x) - 1) << 10) 
#endif

#define SSCR0_TISSP		(1 << 4)	
#define SSCR0_PSP		(3 << 4)	
#define SSCR1_TTELP		(1 << 31)	
#define SSCR1_TTE		(1 << 30)	
#define SSCR1_EBCEI		(1 << 29)	
#define SSCR1_SCFR		(1 << 28)	
#define SSCR1_ECRA		(1 << 27)	
#define SSCR1_ECRB		(1 << 26)	
#define SSCR1_SCLKDIR		(1 << 25)	
#define SSCR1_SFRMDIR		(1 << 24)	
#define SSCR1_RWOT		(1 << 23)	
#define SSCR1_TRAIL		(1 << 22)	
#define SSCR1_TSRE		(1 << 21)	
#define SSCR1_RSRE		(1 << 20)	
#define SSCR1_TINTE		(1 << 19)	
#define SSCR1_PINTE		(1 << 18)	
#define SSCR1_IFS		(1 << 16)	
#define SSCR1_STRF		(1 << 15)	
#define SSCR1_EFWR		(1 << 14)	

#define SSSR_BCE		(1 << 23)	
#define SSSR_CSS		(1 << 22)	
#define SSSR_TUR		(1 << 21)	
#define SSSR_EOC		(1 << 20)	
#define SSSR_TINT		(1 << 19)	
#define SSSR_PINT		(1 << 18)	


#define SSPSP_SCMODE(x)		((x) << 0)	
#define SSPSP_SFRMP		(1 << 2)	
#define SSPSP_ETDS		(1 << 3)	
#define SSPSP_STRTDLY(x)	((x) << 4)	
#define SSPSP_DMYSTRT(x)	((x) << 7)	
#define SSPSP_SFRMDLY(x)	((x) << 9)	
#define SSPSP_SFRMWDTH(x)	((x) << 16)	
#define SSPSP_DMYSTOP(x)	((x) << 23)	
#define SSPSP_FSRT		(1 << 25)	

#define SSPSP_EDMYSTRT(x)	((x) << 26)     
#define SSPSP_EDMYSTOP(x)	((x) << 28)     
#define SSPSP_TIMING_MASK	(0x7f8001f0)

#define SSACD_SCDB		(1 << 3)	
#define SSACD_ACPS(x)		((x) << 4)	
#define SSACD_ACDS(x)		((x) << 0)	
#define SSACD_SCDX8		(1 << 7)	

enum pxa_ssp_type {
	SSP_UNDEFINED = 0,
	PXA25x_SSP,  
	PXA25x_NSSP, 
	PXA27x_SSP,
	PXA168_SSP,
	CE4100_SSP,
};

struct ssp_device {
	struct platform_device *pdev;
	struct list_head	node;

	struct clk	*clk;
	void __iomem	*mmio_base;
	unsigned long	phys_base;

	const char	*label;
	int		port_id;
	int		type;
	int		use_count;
	int		irq;
	int		drcmr_rx;
	int		drcmr_tx;
};

/**
 * pxa_ssp_write_reg - Write to a SSP register
 *
 * @dev: SSP device to access
 * @reg: Register to write to
 * @val: Value to be written.
 */
static inline void pxa_ssp_write_reg(struct ssp_device *dev, u32 reg, u32 val)
{
	__raw_writel(val, dev->mmio_base + reg);
}

static inline u32 pxa_ssp_read_reg(struct ssp_device *dev, u32 reg)
{
	return __raw_readl(dev->mmio_base + reg);
}

struct ssp_device *pxa_ssp_request(int port, const char *label);
void pxa_ssp_free(struct ssp_device *);
#endif
