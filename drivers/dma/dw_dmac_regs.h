/*
 * Driver for the Synopsys DesignWare AHB DMA Controller
 *
 * Copyright (C) 2005-2007 Atmel Corporation
 * Copyright (C) 2010-2011 ST Microelectronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/dw_dmac.h>

#define DW_DMA_MAX_NR_CHANNELS	8

enum dw_dma_fc {
	DW_DMA_FC_D_M2M,
	DW_DMA_FC_D_M2P,
	DW_DMA_FC_D_P2M,
	DW_DMA_FC_D_P2P,
	DW_DMA_FC_P_P2M,
	DW_DMA_FC_SP_P2P,
	DW_DMA_FC_P_M2P,
	DW_DMA_FC_DP_P2P,
};

#define DW_REG(name)		u32 name; u32 __pad_##name

struct dw_dma_chan_regs {
	DW_REG(SAR);		
	DW_REG(DAR);		
	DW_REG(LLP);		
	u32	CTL_LO;		
	u32	CTL_HI;		
	DW_REG(SSTAT);
	DW_REG(DSTAT);
	DW_REG(SSTATAR);
	DW_REG(DSTATAR);
	u32	CFG_LO;		
	u32	CFG_HI;		
	DW_REG(SGR);
	DW_REG(DSR);
};

struct dw_dma_irq_regs {
	DW_REG(XFER);
	DW_REG(BLOCK);
	DW_REG(SRC_TRAN);
	DW_REG(DST_TRAN);
	DW_REG(ERROR);
};

struct dw_dma_regs {
	
	struct dw_dma_chan_regs	CHAN[DW_DMA_MAX_NR_CHANNELS];

	
	struct dw_dma_irq_regs	RAW;		
	struct dw_dma_irq_regs	STATUS;		
	struct dw_dma_irq_regs	MASK;		
	struct dw_dma_irq_regs	CLEAR;		

	DW_REG(STATUS_INT);			

	
	DW_REG(REQ_SRC);
	DW_REG(REQ_DST);
	DW_REG(SGL_REQ_SRC);
	DW_REG(SGL_REQ_DST);
	DW_REG(LAST_SRC);
	DW_REG(LAST_DST);

	
	DW_REG(CFG);
	DW_REG(CH_EN);
	DW_REG(ID);
	DW_REG(TEST);

	
};

#define DWC_CTLL_INT_EN		(1 << 0)	
#define DWC_CTLL_DST_WIDTH(n)	((n)<<1)	
#define DWC_CTLL_SRC_WIDTH(n)	((n)<<4)
#define DWC_CTLL_DST_INC	(0<<7)		
#define DWC_CTLL_DST_DEC	(1<<7)
#define DWC_CTLL_DST_FIX	(2<<7)
#define DWC_CTLL_SRC_INC	(0<<7)		
#define DWC_CTLL_SRC_DEC	(1<<9)
#define DWC_CTLL_SRC_FIX	(2<<9)
#define DWC_CTLL_DST_MSIZE(n)	((n)<<11)	
#define DWC_CTLL_SRC_MSIZE(n)	((n)<<14)
#define DWC_CTLL_S_GATH_EN	(1 << 17)	
#define DWC_CTLL_D_SCAT_EN	(1 << 18)	
#define DWC_CTLL_FC(n)		((n) << 20)
#define DWC_CTLL_FC_M2M		(0 << 20)	
#define DWC_CTLL_FC_M2P		(1 << 20)	
#define DWC_CTLL_FC_P2M		(2 << 20)	
#define DWC_CTLL_FC_P2P		(3 << 20)	
#define DWC_CTLL_DMS(n)		((n)<<23)	
#define DWC_CTLL_SMS(n)		((n)<<25)	
#define DWC_CTLL_LLP_D_EN	(1 << 27)	
#define DWC_CTLL_LLP_S_EN	(1 << 28)	

#define DWC_CTLH_DONE		0x00001000
#define DWC_CTLH_BLOCK_TS_MASK	0x00000fff

#define DWC_CFGL_CH_PRIOR_MASK	(0x7 << 5)	
#define DWC_CFGL_CH_PRIOR(x)	((x) << 5)	
#define DWC_CFGL_CH_SUSP	(1 << 8)	
#define DWC_CFGL_FIFO_EMPTY	(1 << 9)	
#define DWC_CFGL_HS_DST		(1 << 10)	
#define DWC_CFGL_HS_SRC		(1 << 11)	
#define DWC_CFGL_MAX_BURST(x)	((x) << 20)
#define DWC_CFGL_RELOAD_SAR	(1 << 30)
#define DWC_CFGL_RELOAD_DAR	(1 << 31)

#define DWC_CFGH_DS_UPD_EN	(1 << 5)
#define DWC_CFGH_SS_UPD_EN	(1 << 6)

#define DWC_SGR_SGI(x)		((x) << 0)
#define DWC_SGR_SGC(x)		((x) << 20)

#define DWC_DSR_DSI(x)		((x) << 0)
#define DWC_DSR_DSC(x)		((x) << 20)

#define DW_CFG_DMA_EN		(1 << 0)

#define DW_REGLEN		0x400

enum dw_dmac_flags {
	DW_DMA_IS_CYCLIC = 0,
};

struct dw_dma_chan {
	struct dma_chan		chan;
	void __iomem		*ch_regs;
	u8			mask;
	u8			priority;
	bool			paused;
	bool			initialized;

	spinlock_t		lock;

	
	unsigned long		flags;
	struct list_head	active_list;
	struct list_head	queue;
	struct list_head	free_list;
	struct dw_cyclic_desc	*cdesc;

	unsigned int		descs_allocated;

	
	struct dma_slave_config dma_sconfig;
};

static inline struct dw_dma_chan_regs __iomem *
__dwc_regs(struct dw_dma_chan *dwc)
{
	return dwc->ch_regs;
}

#define channel_readl(dwc, name) \
	readl(&(__dwc_regs(dwc)->name))
#define channel_writel(dwc, name, val) \
	writel((val), &(__dwc_regs(dwc)->name))

static inline struct dw_dma_chan *to_dw_dma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct dw_dma_chan, chan);
}

struct dw_dma {
	struct dma_device	dma;
	void __iomem		*regs;
	struct tasklet_struct	tasklet;
	struct clk		*clk;

	u8			all_chan_mask;

	struct dw_dma_chan	chan[0];
};

static inline struct dw_dma_regs __iomem *__dw_regs(struct dw_dma *dw)
{
	return dw->regs;
}

#define dma_readl(dw, name) \
	readl(&(__dw_regs(dw)->name))
#define dma_writel(dw, name, val) \
	writel((val), &(__dw_regs(dw)->name))

#define channel_set_bit(dw, reg, mask) \
	dma_writel(dw, reg, ((mask) << 8) | (mask))
#define channel_clear_bit(dw, reg, mask) \
	dma_writel(dw, reg, ((mask) << 8) | 0)

static inline struct dw_dma *to_dw_dma(struct dma_device *ddev)
{
	return container_of(ddev, struct dw_dma, dma);
}

struct dw_lli {
	
	dma_addr_t	sar;
	dma_addr_t	dar;
	dma_addr_t	llp;		
	u32		ctllo;
	/* values that may get written back: */
	u32		ctlhi;
	u32		sstat;
	u32		dstat;
};

struct dw_desc {
	
	struct dw_lli			lli;

	
	struct list_head		desc_node;
	struct list_head		tx_list;
	struct dma_async_tx_descriptor	txd;
	size_t				len;
};

static inline struct dw_desc *
txd_to_dw_desc(struct dma_async_tx_descriptor *txd)
{
	return container_of(txd, struct dw_desc, txd);
}
