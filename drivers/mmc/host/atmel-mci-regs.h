/*
 * Atmel MultiMedia Card Interface driver
 *
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __DRIVERS_MMC_ATMEL_MCI_H__
#define __DRIVERS_MMC_ATMEL_MCI_H__

#define ATMCI_CR			0x0000	
# define ATMCI_CR_MCIEN			(  1 <<  0)	
# define ATMCI_CR_MCIDIS		(  1 <<  1)	
# define ATMCI_CR_PWSEN			(  1 <<  2)	
# define ATMCI_CR_PWSDIS		(  1 <<  3)	
# define ATMCI_CR_SWRST			(  1 <<  7)	
#define ATMCI_MR			0x0004	
# define ATMCI_MR_CLKDIV(x)		((x) <<  0)	
# define ATMCI_MR_PWSDIV(x)		((x) <<  8)	
# define ATMCI_MR_RDPROOF		(  1 << 11)	
# define ATMCI_MR_WRPROOF		(  1 << 12)	
# define ATMCI_MR_PDCFBYTE		(  1 << 13)	
# define ATMCI_MR_PDCPADV		(  1 << 14)	
# define ATMCI_MR_PDCMODE		(  1 << 15)	
# define ATMCI_MR_CLKODD(x)		((x) << 16)	
#define ATMCI_DTOR			0x0008	
# define ATMCI_DTOCYC(x)		((x) <<  0)	
# define ATMCI_DTOMUL(x)		((x) <<  4)	
#define ATMCI_SDCR			0x000c	
# define ATMCI_SDCSEL_SLOT_A		(  0 <<  0)	
# define ATMCI_SDCSEL_SLOT_B		(  1 <<  0)	
# define ATMCI_SDCSEL_MASK		(  3 <<  0)
# define ATMCI_SDCBUS_1BIT		(  0 <<  6)	
# define ATMCI_SDCBUS_4BIT		(  2 <<  6)	
# define ATMCI_SDCBUS_8BIT		(  3 <<  6)	
# define ATMCI_SDCBUS_MASK		(  3 <<  6)
#define ATMCI_ARGR			0x0010	
#define ATMCI_CMDR			0x0014	
# define ATMCI_CMDR_CMDNB(x)		((x) <<  0)	
# define ATMCI_CMDR_RSPTYP_NONE		(  0 <<  6)	
# define ATMCI_CMDR_RSPTYP_48BIT	(  1 <<  6)	
# define ATMCI_CMDR_RSPTYP_136BIT	(  2 <<  6)	
# define ATMCI_CMDR_SPCMD_INIT		(  1 <<  8)	
# define ATMCI_CMDR_SPCMD_SYNC		(  2 <<  8)	
# define ATMCI_CMDR_SPCMD_INT		(  4 <<  8)	
# define ATMCI_CMDR_SPCMD_INTRESP	(  5 <<  8)	
# define ATMCI_CMDR_OPDCMD		(  1 << 11)	
# define ATMCI_CMDR_MAXLAT_5CYC		(  0 << 12)	
# define ATMCI_CMDR_MAXLAT_64CYC	(  1 << 12)	
# define ATMCI_CMDR_START_XFER		(  1 << 16)	
# define ATMCI_CMDR_STOP_XFER		(  2 << 16)	
# define ATMCI_CMDR_TRDIR_WRITE		(  0 << 18)	
# define ATMCI_CMDR_TRDIR_READ		(  1 << 18)	
# define ATMCI_CMDR_BLOCK		(  0 << 19)	
# define ATMCI_CMDR_MULTI_BLOCK		(  1 << 19)	
# define ATMCI_CMDR_STREAM		(  2 << 19)	
# define ATMCI_CMDR_SDIO_BYTE		(  4 << 19)	
# define ATMCI_CMDR_SDIO_BLOCK		(  5 << 19)	
# define ATMCI_CMDR_SDIO_SUSPEND	(  1 << 24)	
# define ATMCI_CMDR_SDIO_RESUME		(  2 << 24)	
#define ATMCI_BLKR			0x0018	
# define ATMCI_BCNT(x)			((x) <<  0)	
# define ATMCI_BLKLEN(x)		((x) << 16)	
#define ATMCI_CSTOR			0x001c	
# define ATMCI_CSTOCYC(x)		((x) <<  0)	
# define ATMCI_CSTOMUL(x)		((x) <<  4)	
#define ATMCI_RSPR			0x0020	
#define ATMCI_RSPR1			0x0024	
#define ATMCI_RSPR2			0x0028	
#define ATMCI_RSPR3			0x002c	
#define ATMCI_RDR			0x0030	
#define ATMCI_TDR			0x0034	
#define ATMCI_SR			0x0040	
#define ATMCI_IER			0x0044	
#define ATMCI_IDR			0x0048	
#define ATMCI_IMR			0x004c	
# define ATMCI_CMDRDY			(  1 <<   0)	
# define ATMCI_RXRDY			(  1 <<   1)	
# define ATMCI_TXRDY			(  1 <<   2)	
# define ATMCI_BLKE			(  1 <<   3)	
# define ATMCI_DTIP			(  1 <<   4)	
# define ATMCI_NOTBUSY			(  1 <<   5)	
# define ATMCI_ENDRX			(  1 <<   6)    
# define ATMCI_ENDTX			(  1 <<   7)    
# define ATMCI_SDIOIRQA			(  1 <<   8)	
# define ATMCI_SDIOIRQB			(  1 <<   9)	
# define ATMCI_SDIOWAIT			(  1 <<  12)    
# define ATMCI_CSRCV			(  1 <<  13)    
# define ATMCI_RXBUFF			(  1 <<  14)    
# define ATMCI_TXBUFE			(  1 <<  15)    
# define ATMCI_RINDE			(  1 <<  16)	
# define ATMCI_RDIRE			(  1 <<  17)	
# define ATMCI_RCRCE			(  1 <<  18)	
# define ATMCI_RENDE			(  1 <<  19)	
# define ATMCI_RTOE			(  1 <<  20)	
# define ATMCI_DCRCE			(  1 <<  21)	
# define ATMCI_DTOE			(  1 <<  22)	
# define ATMCI_CSTOE			(  1 <<  23)    
# define ATMCI_BLKOVRE			(  1 <<  24)    
# define ATMCI_DMADONE			(  1 <<  25)    
# define ATMCI_FIFOEMPTY		(  1 <<  26)    
# define ATMCI_XFRDONE			(  1 <<  27)    
# define ATMCI_ACKRCV			(  1 <<  28)    
# define ATMCI_ACKRCVE			(  1 <<  29)    
# define ATMCI_OVRE			(  1 <<  30)	
# define ATMCI_UNRE			(  1 <<  31)	
#define ATMCI_DMA			0x0050	
# define ATMCI_DMA_OFFSET(x)		((x) <<  0)	
# define ATMCI_DMA_CHKSIZE(x)		((x) <<  4)	
# define ATMCI_DMAEN			(  1 <<  8)	
#define ATMCI_CFG			0x0054	
# define ATMCI_CFG_FIFOMODE_1DATA	(  1 <<  0)	
# define ATMCI_CFG_FERRCTRL_COR		(  1 <<  4)	
# define ATMCI_CFG_HSMODE		(  1 <<  8)	
# define ATMCI_CFG_LSYNC		(  1 << 12)	
#define ATMCI_WPMR			0x00e4	
# define ATMCI_WP_EN			(  1 <<  0)	
# define ATMCI_WP_KEY			(0x4d4349 << 8)	
#define ATMCI_WPSR			0x00e8	
# define ATMCI_GET_WP_VS(x)		((x) & 0x0f)
# define ATMCI_GET_WP_VSRC(x)		(((x) >> 8) & 0xffff)
#define ATMCI_VERSION			0x00FC  
#define ATMCI_FIFO_APERTURE		0x0200	

#define ATMCI_REGS_SIZE		0x100

#define atmci_readl(port,reg)				\
	__raw_readl((port)->regs + reg)
#define atmci_writel(port,reg,value)			\
	__raw_writel((value), (port)->regs + reg)

#endif 
