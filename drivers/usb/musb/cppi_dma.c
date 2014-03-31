/*
 * Copyright (C) 2005-2006 by Texas Instruments
 *
 * This file implements a DMA  interface using TI's CPPI DMA.
 * For now it's DaVinci-only, but CPPI isn't specific to DaVinci or USB.
 * The TUSB6020, using VLYNQ, has CPPI that looks much like DaVinci.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "musb_core.h"
#include "musb_debug.h"
#include "cppi_dma.h"



#define NUM_TXCHAN_BD       64
#define NUM_RXCHAN_BD       64

static inline void cpu_drain_writebuffer(void)
{
	wmb();
#ifdef	CONFIG_CPU_ARM926T
	asm("mcr p15, 0, r0, c7, c10, 4 @ drain write buffer\n");
#endif
}

static inline struct cppi_descriptor *cppi_bd_alloc(struct cppi_channel *c)
{
	struct cppi_descriptor	*bd = c->freelist;

	if (bd)
		c->freelist = bd->next;
	return bd;
}

static inline void
cppi_bd_free(struct cppi_channel *c, struct cppi_descriptor *bd)
{
	if (!bd)
		return;
	bd->next = c->freelist;
	c->freelist = bd;
}


static void cppi_reset_rx(struct cppi_rx_stateram __iomem *rx)
{
	musb_writel(&rx->rx_skipbytes, 0, 0);
	musb_writel(&rx->rx_head, 0, 0);
	musb_writel(&rx->rx_sop, 0, 0);
	musb_writel(&rx->rx_current, 0, 0);
	musb_writel(&rx->rx_buf_current, 0, 0);
	musb_writel(&rx->rx_len_len, 0, 0);
	musb_writel(&rx->rx_cnt_cnt, 0, 0);
}

static void cppi_reset_tx(struct cppi_tx_stateram __iomem *tx, u32 ptr)
{
	musb_writel(&tx->tx_head, 0, 0);
	musb_writel(&tx->tx_buf, 0, 0);
	musb_writel(&tx->tx_current, 0, 0);
	musb_writel(&tx->tx_buf_current, 0, 0);
	musb_writel(&tx->tx_info, 0, 0);
	musb_writel(&tx->tx_rem_len, 0, 0);
	
	musb_writel(&tx->tx_complete, 0, ptr);
}

static void __init cppi_pool_init(struct cppi *cppi, struct cppi_channel *c)
{
	int	j;

	
	c->head = NULL;
	c->tail = NULL;
	c->last_processed = NULL;
	c->channel.status = MUSB_DMA_STATUS_UNKNOWN;
	c->controller = cppi;
	c->is_rndis = 0;
	c->freelist = NULL;

	
	for (j = 0; j < NUM_TXCHAN_BD + 1; j++) {
		struct cppi_descriptor	*bd;
		dma_addr_t		dma;

		bd = dma_pool_alloc(cppi->pool, GFP_KERNEL, &dma);
		bd->dma = dma;
		cppi_bd_free(c, bd);
	}
}

static int cppi_channel_abort(struct dma_channel *);

static void cppi_pool_free(struct cppi_channel *c)
{
	struct cppi		*cppi = c->controller;
	struct cppi_descriptor	*bd;

	(void) cppi_channel_abort(&c->channel);
	c->channel.status = MUSB_DMA_STATUS_UNKNOWN;
	c->controller = NULL;

	
	bd = c->last_processed;
	do {
		if (bd)
			dma_pool_free(cppi->pool, bd, bd->dma);
		bd = cppi_bd_alloc(c);
	} while (bd);
	c->last_processed = NULL;
}

static int __init cppi_controller_start(struct dma_controller *c)
{
	struct cppi	*controller;
	void __iomem	*tibase;
	int		i;

	controller = container_of(c, struct cppi, controller);

	
	for (i = 0; i < ARRAY_SIZE(controller->tx); i++) {
		controller->tx[i].transmit = true;
		controller->tx[i].index = i;
	}
	for (i = 0; i < ARRAY_SIZE(controller->rx); i++) {
		controller->rx[i].transmit = false;
		controller->rx[i].index = i;
	}

	
	for (i = 0; i < ARRAY_SIZE(controller->tx); i++)
		cppi_pool_init(controller, controller->tx + i);
	for (i = 0; i < ARRAY_SIZE(controller->rx); i++)
		cppi_pool_init(controller, controller->rx + i);

	tibase =  controller->tibase;
	INIT_LIST_HEAD(&controller->tx_complete);

	
	for (i = 0; i < ARRAY_SIZE(controller->tx); i++) {
		struct cppi_channel	*tx_ch = controller->tx + i;
		struct cppi_tx_stateram __iomem *tx;

		INIT_LIST_HEAD(&tx_ch->tx_complete);

		tx = tibase + DAVINCI_TXCPPI_STATERAM_OFFSET(i);
		tx_ch->state_ram = tx;
		cppi_reset_tx(tx, 0);
	}
	for (i = 0; i < ARRAY_SIZE(controller->rx); i++) {
		struct cppi_channel	*rx_ch = controller->rx + i;
		struct cppi_rx_stateram __iomem *rx;

		INIT_LIST_HEAD(&rx_ch->tx_complete);

		rx = tibase + DAVINCI_RXCPPI_STATERAM_OFFSET(i);
		rx_ch->state_ram = rx;
		cppi_reset_rx(rx);
	}

	
	musb_writel(tibase, DAVINCI_TXCPPI_INTENAB_REG,
			DAVINCI_DMA_ALL_CHANNELS_ENABLE);
	musb_writel(tibase, DAVINCI_RXCPPI_INTENAB_REG,
			DAVINCI_DMA_ALL_CHANNELS_ENABLE);

	
	musb_writel(tibase, DAVINCI_TXCPPI_CTRL_REG, DAVINCI_DMA_CTRL_ENABLE);
	musb_writel(tibase, DAVINCI_RXCPPI_CTRL_REG, DAVINCI_DMA_CTRL_ENABLE);

	
	musb_writel(tibase, DAVINCI_RNDIS_REG, 0);
	musb_writel(tibase, DAVINCI_AUTOREQ_REG, 0);

	return 0;
}


static int cppi_controller_stop(struct dma_controller *c)
{
	struct cppi		*controller;
	void __iomem		*tibase;
	int			i;
	struct musb		*musb;

	controller = container_of(c, struct cppi, controller);
	musb = controller->musb;

	tibase = controller->tibase;
	
	musb_writel(tibase, DAVINCI_TXCPPI_INTCLR_REG,
			DAVINCI_DMA_ALL_CHANNELS_ENABLE);
	musb_writel(tibase, DAVINCI_RXCPPI_INTCLR_REG,
			DAVINCI_DMA_ALL_CHANNELS_ENABLE);

	dev_dbg(musb->controller, "Tearing down RX and TX Channels\n");
	for (i = 0; i < ARRAY_SIZE(controller->tx); i++) {
		
		controller->tx[i].last_processed = NULL;
		cppi_pool_free(controller->tx + i);
	}
	for (i = 0; i < ARRAY_SIZE(controller->rx); i++)
		cppi_pool_free(controller->rx + i);

	
	musb_writel(tibase, DAVINCI_TXCPPI_CTRL_REG, DAVINCI_DMA_CTRL_DISABLE);
	musb_writel(tibase, DAVINCI_RXCPPI_CTRL_REG, DAVINCI_DMA_CTRL_DISABLE);

	return 0;
}

static inline void core_rxirq_disable(void __iomem *tibase, unsigned epnum)
{
	musb_writel(tibase, DAVINCI_USB_INT_MASK_CLR_REG, 1 << (epnum + 8));
}

static inline void core_rxirq_enable(void __iomem *tibase, unsigned epnum)
{
	musb_writel(tibase, DAVINCI_USB_INT_MASK_SET_REG, 1 << (epnum + 8));
}


static struct dma_channel *
cppi_channel_allocate(struct dma_controller *c,
		struct musb_hw_ep *ep, u8 transmit)
{
	struct cppi		*controller;
	u8			index;
	struct cppi_channel	*cppi_ch;
	void __iomem		*tibase;
	struct musb		*musb;

	controller = container_of(c, struct cppi, controller);
	tibase = controller->tibase;
	musb = controller->musb;

	
	index = ep->epnum - 1;

	if (transmit) {
		if (index >= ARRAY_SIZE(controller->tx)) {
			dev_dbg(musb->controller, "no %cX%d CPPI channel\n", 'T', index);
			return NULL;
		}
		cppi_ch = controller->tx + index;
	} else {
		if (index >= ARRAY_SIZE(controller->rx)) {
			dev_dbg(musb->controller, "no %cX%d CPPI channel\n", 'R', index);
			return NULL;
		}
		cppi_ch = controller->rx + index;
		core_rxirq_disable(tibase, ep->epnum);
	}

	if (cppi_ch->hw_ep)
		dev_dbg(musb->controller, "re-allocating DMA%d %cX channel %p\n",
				index, transmit ? 'T' : 'R', cppi_ch);
	cppi_ch->hw_ep = ep;
	cppi_ch->channel.status = MUSB_DMA_STATUS_FREE;
	cppi_ch->channel.max_len = 0x7fffffff;

	dev_dbg(musb->controller, "Allocate CPPI%d %cX\n", index, transmit ? 'T' : 'R');
	return &cppi_ch->channel;
}

static void cppi_channel_release(struct dma_channel *channel)
{
	struct cppi_channel	*c;
	void __iomem		*tibase;

	

	c = container_of(channel, struct cppi_channel, channel);
	tibase = c->controller->tibase;
	if (!c->hw_ep)
		dev_dbg(c->controller->musb->controller,
			"releasing idle DMA channel %p\n", c);
	else if (!c->transmit)
		core_rxirq_enable(tibase, c->index + 1);

	
	c->hw_ep = NULL;
	channel->status = MUSB_DMA_STATUS_UNKNOWN;
}

static void
cppi_dump_rx(int level, struct cppi_channel *c, const char *tag)
{
	void __iomem			*base = c->controller->mregs;
	struct cppi_rx_stateram __iomem	*rx = c->state_ram;

	musb_ep_select(base, c->index + 1);

	dev_dbg(c->controller->musb->controller,
		"RX DMA%d%s: %d left, csr %04x, "
		"%08x H%08x S%08x C%08x, "
		"B%08x L%08x %08x .. %08x"
		"\n",
		c->index, tag,
		musb_readl(c->controller->tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + 4 * c->index),
		musb_readw(c->hw_ep->regs, MUSB_RXCSR),

		musb_readl(&rx->rx_skipbytes, 0),
		musb_readl(&rx->rx_head, 0),
		musb_readl(&rx->rx_sop, 0),
		musb_readl(&rx->rx_current, 0),

		musb_readl(&rx->rx_buf_current, 0),
		musb_readl(&rx->rx_len_len, 0),
		musb_readl(&rx->rx_cnt_cnt, 0),
		musb_readl(&rx->rx_complete, 0)
		);
}

static void
cppi_dump_tx(int level, struct cppi_channel *c, const char *tag)
{
	void __iomem			*base = c->controller->mregs;
	struct cppi_tx_stateram __iomem	*tx = c->state_ram;

	musb_ep_select(base, c->index + 1);

	dev_dbg(c->controller->musb->controller,
		"TX DMA%d%s: csr %04x, "
		"H%08x S%08x C%08x %08x, "
		"F%08x L%08x .. %08x"
		"\n",
		c->index, tag,
		musb_readw(c->hw_ep->regs, MUSB_TXCSR),

		musb_readl(&tx->tx_head, 0),
		musb_readl(&tx->tx_buf, 0),
		musb_readl(&tx->tx_current, 0),
		musb_readl(&tx->tx_buf_current, 0),

		musb_readl(&tx->tx_info, 0),
		musb_readl(&tx->tx_rem_len, 0),
		
		musb_readl(&tx->tx_complete, 0)
		);
}

static inline void
cppi_rndis_update(struct cppi_channel *c, int is_rx,
		void __iomem *tibase, int is_rndis)
{
	
	if (c->is_rndis != is_rndis) {
		u32	value = musb_readl(tibase, DAVINCI_RNDIS_REG);
		u32	temp = 1 << (c->index);

		if (is_rx)
			temp <<= 16;
		if (is_rndis)
			value |= temp;
		else
			value &= ~temp;
		musb_writel(tibase, DAVINCI_RNDIS_REG, value);
		c->is_rndis = is_rndis;
	}
}

#ifdef CONFIG_USB_MUSB_DEBUG
static void cppi_dump_rxbd(const char *tag, struct cppi_descriptor *bd)
{
	pr_debug("RXBD/%s %08x: "
			"nxt %08x buf %08x off.blen %08x opt.plen %08x\n",
			tag, bd->dma,
			bd->hw_next, bd->hw_bufp, bd->hw_off_len,
			bd->hw_options);
}
#endif

static void cppi_dump_rxq(int level, const char *tag, struct cppi_channel *rx)
{
#ifdef CONFIG_USB_MUSB_DEBUG
	struct cppi_descriptor	*bd;

	if (!_dbg_level(level))
		return;
	cppi_dump_rx(level, rx, tag);
	if (rx->last_processed)
		cppi_dump_rxbd("last", rx->last_processed);
	for (bd = rx->head; bd; bd = bd->next)
		cppi_dump_rxbd("active", bd);
#endif
}


static inline int cppi_autoreq_update(struct cppi_channel *rx,
		void __iomem *tibase, int onepacket, unsigned n_bds)
{
	u32	val;

#ifdef	RNDIS_RX_IS_USABLE
	u32	tmp;
	

	
	tmp = musb_readl(tibase, DAVINCI_AUTOREQ_REG);
	val = tmp & ~((0x3) << (rx->index * 2));

	if (!onepacket) {
#if 0
		
		val |= ((0x3) << (rx->index * 2));
		n_bds--;
#else
		
		val |= ((0x1) << (rx->index * 2));
#endif
	}

	if (val != tmp) {
		int n = 100;

		
		musb_writel(tibase, DAVINCI_AUTOREQ_REG, val);
		do {
			tmp = musb_readl(tibase, DAVINCI_AUTOREQ_REG);
			if (tmp == val)
				break;
			cpu_relax();
		} while (n-- > 0);
	}
#endif

	
	if (n_bds && rx->channel.actual_len) {
		void __iomem	*regs = rx->hw_ep->regs;

		val = musb_readw(regs, MUSB_RXCSR);
		if (!(val & MUSB_RXCSR_H_REQPKT)) {
			val |= MUSB_RXCSR_H_REQPKT | MUSB_RXCSR_H_WZC_BITS;
			musb_writew(regs, MUSB_RXCSR, val);
			
			val = musb_readw(regs, MUSB_RXCSR);
		}
	}
	return n_bds;
}



/*
 * CPPI TX:
 * ========
 * TX is a lot more reasonable than RX; it doesn't need to run in
 * irq-per-packet mode very often.  RNDIS mode seems to behave too
 * (except how it handles the exactly-N-packets case).  Building a
 * txdma queue with multiple requests (urb or usb_request) looks
 * like it would work ... but fault handling would need much testing.
 *
 * The main issue with TX mode RNDIS relates to transfer lengths that
 * are an exact multiple of the packet length.  It appears that there's
 * a hiccup in that case (maybe the DMA completes before the ZLP gets
 * written?) boiling down to not being able to rely on CPPI writing any
 * terminating zero length packet before the next transfer is written.
 * So that's punted to PIO; better yet, gadget drivers can avoid it.
 *
 * Plus, there's allegedly an undocumented constraint that rndis transfer
 * length be a multiple of 64 bytes ... but the chip doesn't act that
 * way, and we really don't _want_ that behavior anyway.
 *
 * On TX, "transparent" mode works ... although experiments have shown
 * problems trying to use the SOP/EOP bits in different USB packets.
 *
 * REVISIT try to handle terminating zero length packets using CPPI
 * instead of doing it by PIO after an IRQ.  (Meanwhile, make Ethernet
 * links avoid that issue by forcing them to avoid zlps.)
 */
static void
cppi_next_tx_segment(struct musb *musb, struct cppi_channel *tx)
{
	unsigned		maxpacket = tx->maxpacket;
	dma_addr_t		addr = tx->buf_dma + tx->offset;
	size_t			length = tx->buf_len - tx->offset;
	struct cppi_descriptor	*bd;
	unsigned		n_bds;
	unsigned		i;
	struct cppi_tx_stateram	__iomem *tx_ram = tx->state_ram;
	int			rndis;

	rndis = (maxpacket & 0x3f) == 0
		&& length > maxpacket
		&& length < 0xffff
		&& (length % maxpacket) != 0;

	if (rndis) {
		maxpacket = length;
		n_bds = 1;
	} else {
		n_bds = length / maxpacket;
		if (!length || (length % maxpacket))
			n_bds++;
		n_bds = min(n_bds, (unsigned) NUM_TXCHAN_BD);
		length = min(n_bds * maxpacket, length);
	}

	dev_dbg(musb->controller, "TX DMA%d, pktSz %d %s bds %d dma 0x%llx len %u\n",
			tx->index,
			maxpacket,
			rndis ? "rndis" : "transparent",
			n_bds,
			(unsigned long long)addr, length);

	cppi_rndis_update(tx, 0, musb->ctrl_base, rndis);


	bd = tx->freelist;
	tx->head = bd;
	tx->last_processed = NULL;


	for (i = 0; i < n_bds; ) {
		if (++i < n_bds && bd->next)
			bd->hw_next = bd->next->dma;
		else
			bd->hw_next = 0;

		bd->hw_bufp = tx->buf_dma + tx->offset;

		if ((tx->offset + maxpacket) <= tx->buf_len) {
			tx->offset += maxpacket;
			bd->hw_off_len = maxpacket;
			bd->hw_options = CPPI_SOP_SET | CPPI_EOP_SET
				| CPPI_OWN_SET | maxpacket;
		} else {
			
			u32		partial_len;

			partial_len = tx->buf_len - tx->offset;
			tx->offset = tx->buf_len;
			bd->hw_off_len = partial_len;

			bd->hw_options = CPPI_SOP_SET | CPPI_EOP_SET
				| CPPI_OWN_SET | partial_len;
			if (partial_len == 0)
				bd->hw_options |= CPPI_ZERO_SET;
		}

		dev_dbg(musb->controller, "TXBD %p: nxt %08x buf %08x len %04x opt %08x\n",
				bd, bd->hw_next, bd->hw_bufp,
				bd->hw_off_len, bd->hw_options);

		
		tx->tail = bd;
		bd = bd->next;
	}

	
	cpu_drain_writebuffer();

	
	musb_writel(&tx_ram->tx_head, 0, (u32)tx->freelist->dma);

	cppi_dump_tx(5, tx, "/S");
}



static bool cppi_rx_rndis = 1;

module_param(cppi_rx_rndis, bool, 0);
MODULE_PARM_DESC(cppi_rx_rndis, "enable/disable RX RNDIS heuristic");


static void
cppi_next_rx_segment(struct musb *musb, struct cppi_channel *rx, int onepacket)
{
	unsigned		maxpacket = rx->maxpacket;
	dma_addr_t		addr = rx->buf_dma + rx->offset;
	size_t			length = rx->buf_len - rx->offset;
	struct cppi_descriptor	*bd, *tail;
	unsigned		n_bds;
	unsigned		i;
	void __iomem		*tibase = musb->ctrl_base;
	int			is_rndis = 0;
	struct cppi_rx_stateram	__iomem *rx_ram = rx->state_ram;

	if (onepacket) {
		
		n_bds = 1;

		
		if (cppi_rx_rndis
				&& is_peripheral_active(musb)
				&& length > maxpacket
				&& (length & ~0xffff) == 0
				&& (length & 0x0fff) != 0
				&& (length & (maxpacket - 1)) == 0) {
			maxpacket = length;
			is_rndis = 1;
		}
	} else {
		
		if (length > 0xffff) {
			n_bds = 0xffff / maxpacket;
			length = n_bds * maxpacket;
		} else {
			n_bds = length / maxpacket;
			if (length % maxpacket)
				n_bds++;
		}
		if (n_bds == 1)
			onepacket = 1;
		else
			n_bds = min(n_bds, (unsigned) NUM_RXCHAN_BD);
	}

	if (is_host_active(musb))
		n_bds = cppi_autoreq_update(rx, tibase, onepacket, n_bds);

	cppi_rndis_update(rx, 1, musb->ctrl_base, is_rndis);

	length = min(n_bds * maxpacket, length);

	dev_dbg(musb->controller, "RX DMA%d seg, maxp %d %s bds %d (cnt %d) "
			"dma 0x%llx len %u %u/%u\n",
			rx->index, maxpacket,
			onepacket
				? (is_rndis ? "rndis" : "onepacket")
				: "multipacket",
			n_bds,
			musb_readl(tibase,
				DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4))
					& 0xffff,
			(unsigned long long)addr, length,
			rx->channel.actual_len, rx->buf_len);

	bd = cppi_bd_alloc(rx);
	rx->head = bd;

	
	for (i = 0, tail = NULL; bd && i < n_bds; i++, tail = bd) {
		u32	bd_len;

		if (i) {
			bd = cppi_bd_alloc(rx);
			if (!bd)
				break;
			tail->next = bd;
			tail->hw_next = bd->dma;
		}
		bd->hw_next = 0;

		
		if (maxpacket < length)
			bd_len = maxpacket;
		else
			bd_len = length;

		bd->hw_bufp = addr;
		addr += bd_len;
		rx->offset += bd_len;

		bd->hw_off_len = (0  << 16) + bd_len;
		bd->buflen = bd_len;

		bd->hw_options = CPPI_OWN_SET | (i == 0 ? length : 0);
		length -= bd_len;
	}

	
	if (!tail) {
		WARNING("rx dma%d -- no BDs? need %d\n", rx->index, n_bds);
		return;
	} else if (i < n_bds)
		WARNING("rx dma%d -- only %d of %d BDs\n", rx->index, i, n_bds);

	tail->next = NULL;
	tail->hw_next = 0;

	bd = rx->head;
	rx->tail = tail;

	bd->hw_options |= CPPI_SOP_SET;
	tail->hw_options |= CPPI_EOP_SET;

#ifdef CONFIG_USB_MUSB_DEBUG
	if (_dbg_level(5)) {
		struct cppi_descriptor	*d;

		for (d = rx->head; d; d = d->next)
			cppi_dump_rxbd("S", d);
	}
#endif

	
	tail = rx->last_processed;
	if (tail) {
		tail->next = bd;
		tail->hw_next = bd->dma;
	}

	core_rxirq_enable(tibase, rx->index + 1);

	
	cpu_drain_writebuffer();

	musb_writel(&rx_ram->rx_head, 0, bd->dma);

	i = musb_readl(tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4))
			& 0xffff;

	if (!i)
		musb_writel(tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4),
			n_bds + 2);
	else if (n_bds > (i - 3))
		musb_writel(tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4),
			n_bds - (i - 3));

	i = musb_readl(tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4))
			& 0xffff;
	if (i < (2 + n_bds)) {
		dev_dbg(musb->controller, "bufcnt%d underrun - %d (for %d)\n",
					rx->index, i, n_bds);
		musb_writel(tibase,
			DAVINCI_RXCPPI_BUFCNT0_REG + (rx->index * 4),
			n_bds + 2);
	}

	cppi_dump_rx(4, rx, "/S");
}

static int cppi_channel_program(struct dma_channel *ch,
		u16 maxpacket, u8 mode,
		dma_addr_t dma_addr, u32 len)
{
	struct cppi_channel	*cppi_ch;
	struct cppi		*controller;
	struct musb		*musb;

	cppi_ch = container_of(ch, struct cppi_channel, channel);
	controller = cppi_ch->controller;
	musb = controller->musb;

	switch (ch->status) {
	case MUSB_DMA_STATUS_BUS_ABORT:
	case MUSB_DMA_STATUS_CORE_ABORT:
		
		WARNING("%cX DMA%d not cleaned up after abort!\n",
				cppi_ch->transmit ? 'T' : 'R',
				cppi_ch->index);
		
		break;
	case MUSB_DMA_STATUS_BUSY:
		WARNING("program active channel?  %cX DMA%d\n",
				cppi_ch->transmit ? 'T' : 'R',
				cppi_ch->index);
		
		break;
	case MUSB_DMA_STATUS_UNKNOWN:
		dev_dbg(musb->controller, "%cX DMA%d not allocated!\n",
				cppi_ch->transmit ? 'T' : 'R',
				cppi_ch->index);
		
	case MUSB_DMA_STATUS_FREE:
		break;
	}

	ch->status = MUSB_DMA_STATUS_BUSY;

	
	cppi_ch->buf_dma = dma_addr;
	cppi_ch->offset = 0;
	cppi_ch->maxpacket = maxpacket;
	cppi_ch->buf_len = len;
	cppi_ch->channel.actual_len = 0;

	
	if (cppi_ch->transmit)
		cppi_next_tx_segment(musb, cppi_ch);
	else
		cppi_next_rx_segment(musb, cppi_ch, mode);

	return true;
}

static bool cppi_rx_scan(struct cppi *cppi, unsigned ch)
{
	struct cppi_channel		*rx = &cppi->rx[ch];
	struct cppi_rx_stateram __iomem	*state = rx->state_ram;
	struct cppi_descriptor		*bd;
	struct cppi_descriptor		*last = rx->last_processed;
	bool				completed = false;
	bool				acked = false;
	int				i;
	dma_addr_t			safe2ack;
	void __iomem			*regs = rx->hw_ep->regs;
	struct musb			*musb = cppi->musb;

	cppi_dump_rx(6, rx, "/K");

	bd = last ? last->next : rx->head;
	if (!bd)
		return false;

	
	for (i = 0, safe2ack = musb_readl(&state->rx_complete, 0);
			(safe2ack || completed) && bd && i < NUM_RXCHAN_BD;
			i++, bd = bd->next) {
		u16	len;

		
		rmb();
		if (!completed && (bd->hw_options & CPPI_OWN_SET))
			break;

		dev_dbg(musb->controller, "C/RXBD %llx: nxt %08x buf %08x "
			"off.len %08x opt.len %08x (%d)\n",
			(unsigned long long)bd->dma, bd->hw_next, bd->hw_bufp,
			bd->hw_off_len, bd->hw_options,
			rx->channel.actual_len);

		
		if ((bd->hw_options & CPPI_SOP_SET) && !completed)
			len = bd->hw_off_len & CPPI_RECV_PKTLEN_MASK;
		else
			len = 0;

		if (bd->hw_options & CPPI_EOQ_MASK)
			completed = true;

		if (!completed && len < bd->buflen) {
			completed = true;
			dev_dbg(musb->controller, "rx short %d/%d (%d)\n",
					len, bd->buflen,
					rx->channel.actual_len);
		}

		if (bd->dma == safe2ack) {
			musb_writel(&state->rx_complete, 0, safe2ack);
			safe2ack = musb_readl(&state->rx_complete, 0);
			acked = true;
			if (bd->dma == safe2ack)
				safe2ack = 0;
		}

		rx->channel.actual_len += len;

		cppi_bd_free(rx, last);
		last = bd;

		
		if (bd->hw_next == 0)
			completed = true;
	}
	rx->last_processed = last;

	
	if (!acked && last) {
		int	csr;

		if (safe2ack == 0 || safe2ack == rx->last_processed->dma)
			musb_writel(&state->rx_complete, 0, safe2ack);
		if (safe2ack == 0) {
			cppi_bd_free(rx, last);
			rx->last_processed = NULL;

			WARN_ON(rx->head);
		}
		musb_ep_select(cppi->mregs, rx->index + 1);
		csr = musb_readw(regs, MUSB_RXCSR);
		if (csr & MUSB_RXCSR_DMAENAB) {
			dev_dbg(musb->controller, "list%d %p/%p, last %llx%s, csr %04x\n",
				rx->index,
				rx->head, rx->tail,
				rx->last_processed
					? (unsigned long long)
						rx->last_processed->dma
					: 0,
				completed ? ", completed" : "",
				csr);
			cppi_dump_rxq(4, "/what?", rx);
		}
	}
	if (!completed) {
		int	csr;

		rx->head = bd;

		csr = musb_readw(rx->hw_ep->regs, MUSB_RXCSR);
		if (is_host_active(cppi->musb)
				&& bd
				&& !(csr & MUSB_RXCSR_H_REQPKT)) {
			csr |= MUSB_RXCSR_H_REQPKT;
			musb_writew(regs, MUSB_RXCSR,
					MUSB_RXCSR_H_WZC_BITS | csr);
			csr = musb_readw(rx->hw_ep->regs, MUSB_RXCSR);
		}
	} else {
		rx->head = NULL;
		rx->tail = NULL;
	}

	cppi_dump_rx(6, rx, completed ? "/completed" : "/cleaned");
	return completed;
}

irqreturn_t cppi_interrupt(int irq, void *dev_id)
{
	struct musb		*musb = dev_id;
	struct cppi		*cppi;
	void __iomem		*tibase;
	struct musb_hw_ep	*hw_ep = NULL;
	u32			rx, tx;
	int			i, index;
	unsigned long		uninitialized_var(flags);

	cppi = container_of(musb->dma_controller, struct cppi, controller);
	if (cppi->irq)
		spin_lock_irqsave(&musb->lock, flags);

	tibase = musb->ctrl_base;

	tx = musb_readl(tibase, DAVINCI_TXCPPI_MASKED_REG);
	rx = musb_readl(tibase, DAVINCI_RXCPPI_MASKED_REG);

	if (!tx && !rx) {
		if (cppi->irq)
			spin_unlock_irqrestore(&musb->lock, flags);
		return IRQ_NONE;
	}

	dev_dbg(musb->controller, "CPPI IRQ Tx%x Rx%x\n", tx, rx);

	
	for (index = 0; tx; tx = tx >> 1, index++) {
		struct cppi_channel		*tx_ch;
		struct cppi_tx_stateram __iomem	*tx_ram;
		bool				completed = false;
		struct cppi_descriptor		*bd;

		if (!(tx & 1))
			continue;

		tx_ch = cppi->tx + index;
		tx_ram = tx_ch->state_ram;


		cppi_dump_tx(5, tx_ch, "/E");

		bd = tx_ch->head;

		if (NULL == bd) {
			dev_dbg(musb->controller, "null BD\n");
			musb_writel(&tx_ram->tx_complete, 0, 0);
			continue;
		}

		
		for (i = 0; !completed && bd && i < NUM_TXCHAN_BD;
				i++, bd = bd->next) {
			u16	len;

			
			rmb();
			if (bd->hw_options & CPPI_OWN_SET)
				break;

			dev_dbg(musb->controller, "C/TXBD %p n %x b %x off %x opt %x\n",
					bd, bd->hw_next, bd->hw_bufp,
					bd->hw_off_len, bd->hw_options);

			len = bd->hw_off_len & CPPI_BUFFER_LEN_MASK;
			tx_ch->channel.actual_len += len;

			tx_ch->last_processed = bd;

			
				musb_writel(&tx_ram->tx_complete, 0, bd->dma);

			
			if (bd->hw_next == 0)
				completed = true;
		}

		
		if (completed) {
			

			
			if (tx_ch->offset >= tx_ch->buf_len) {
				tx_ch->head = NULL;
				tx_ch->tail = NULL;
				tx_ch->channel.status = MUSB_DMA_STATUS_FREE;

				hw_ep = tx_ch->hw_ep;

				musb_dma_completion(musb, index + 1, 1);

			} else {
				cppi_next_tx_segment(musb, tx_ch);
			}
		} else
			tx_ch->head = bd;
	}

	
	for (index = 0; rx; rx = rx >> 1, index++) {

		if (rx & 1) {
			struct cppi_channel		*rx_ch;

			rx_ch = cppi->rx + index;

			
			if (!cppi_rx_scan(cppi, index))
				continue;

			
			if (rx_ch->channel.actual_len != rx_ch->buf_len
					&& rx_ch->channel.actual_len
						== rx_ch->offset) {
				cppi_next_rx_segment(musb, rx_ch, 1);
				continue;
			}

			
			rx_ch->channel.status = MUSB_DMA_STATUS_FREE;

			hw_ep = rx_ch->hw_ep;

			core_rxirq_disable(tibase, index + 1);
			musb_dma_completion(musb, index + 1, 0);
		}
	}

	
	musb_writel(tibase, DAVINCI_CPPI_EOI_REG, 0);

	if (cppi->irq)
		spin_unlock_irqrestore(&musb->lock, flags);

	return IRQ_HANDLED;
}

struct dma_controller *__init
dma_controller_create(struct musb *musb, void __iomem *mregs)
{
	struct cppi		*controller;
	struct device		*dev = musb->controller;
	struct platform_device	*pdev = to_platform_device(dev);
	int			irq = platform_get_irq_byname(pdev, "dma");

	controller = kzalloc(sizeof *controller, GFP_KERNEL);
	if (!controller)
		return NULL;

	controller->mregs = mregs;
	controller->tibase = mregs - DAVINCI_BASE_OFFSET;

	controller->musb = musb;
	controller->controller.start = cppi_controller_start;
	controller->controller.stop = cppi_controller_stop;
	controller->controller.channel_alloc = cppi_channel_allocate;
	controller->controller.channel_release = cppi_channel_release;
	controller->controller.channel_program = cppi_channel_program;
	controller->controller.channel_abort = cppi_channel_abort;


	
	controller->pool = dma_pool_create("cppi",
			controller->musb->controller,
			sizeof(struct cppi_descriptor),
			CPPI_DESCRIPTOR_ALIGN, 0);
	if (!controller->pool) {
		kfree(controller);
		return NULL;
	}

	if (irq > 0) {
		if (request_irq(irq, cppi_interrupt, 0, "cppi-dma", musb)) {
			dev_err(dev, "request_irq %d failed!\n", irq);
			dma_controller_destroy(&controller->controller);
			return NULL;
		}
		controller->irq = irq;
	}

	return &controller->controller;
}

void dma_controller_destroy(struct dma_controller *c)
{
	struct cppi	*cppi;

	cppi = container_of(c, struct cppi, controller);

	if (cppi->irq)
		free_irq(cppi->irq, cppi->musb);

	
	dma_pool_destroy(cppi->pool);

	kfree(cppi);
}

static int cppi_channel_abort(struct dma_channel *channel)
{
	struct cppi_channel	*cppi_ch;
	struct cppi		*controller;
	void __iomem		*mbase;
	void __iomem		*tibase;
	void __iomem		*regs;
	u32			value;
	struct cppi_descriptor	*queue;

	cppi_ch = container_of(channel, struct cppi_channel, channel);

	controller = cppi_ch->controller;

	switch (channel->status) {
	case MUSB_DMA_STATUS_BUS_ABORT:
	case MUSB_DMA_STATUS_CORE_ABORT:
		
	case MUSB_DMA_STATUS_BUSY:
		
		regs = cppi_ch->hw_ep->regs;
		break;
	case MUSB_DMA_STATUS_UNKNOWN:
	case MUSB_DMA_STATUS_FREE:
		return 0;
	default:
		return -EINVAL;
	}

	if (!cppi_ch->transmit && cppi_ch->head)
		cppi_dump_rxq(3, "/abort", cppi_ch);

	mbase = controller->mregs;
	tibase = controller->tibase;

	queue = cppi_ch->head;
	cppi_ch->head = NULL;
	cppi_ch->tail = NULL;

	musb_ep_select(mbase, cppi_ch->index + 1);

	if (cppi_ch->transmit) {
		struct cppi_tx_stateram __iomem *tx_ram;
		

		cppi_dump_tx(6, cppi_ch, " (teardown)");

		
		do {
			value = musb_readl(tibase, DAVINCI_TXCPPI_TEAR_REG);
		} while (!(value & CPPI_TEAR_READY));
		musb_writel(tibase, DAVINCI_TXCPPI_TEAR_REG, cppi_ch->index);

		tx_ram = cppi_ch->state_ram;
		do {
			value = musb_readl(&tx_ram->tx_complete, 0);
		} while (0xFFFFFFFC != value);


		value = musb_readw(regs, MUSB_TXCSR);
		value &= ~MUSB_TXCSR_DMAENAB;
		value |= MUSB_TXCSR_FLUSHFIFO;
		musb_writew(regs, MUSB_TXCSR, value);
		musb_writew(regs, MUSB_TXCSR, value);

		cppi_reset_tx(tx_ram, 1);
		cppi_ch->head = NULL;
		musb_writel(&tx_ram->tx_complete, 0, 1);
		cppi_dump_tx(5, cppi_ch, " (done teardown)");


	} else  {
		u16			csr;


		core_rxirq_disable(tibase, cppi_ch->index + 1);

		
		if (is_host_active(cppi_ch->controller->musb)) {
			value = musb_readl(tibase, DAVINCI_AUTOREQ_REG);
			value &= ~((0x3) << (cppi_ch->index * 2));
			musb_writel(tibase, DAVINCI_AUTOREQ_REG, value);
		}

		csr = musb_readw(regs, MUSB_RXCSR);

		
		if (is_host_active(cppi_ch->controller->musb)) {
			csr |= MUSB_RXCSR_H_WZC_BITS;
			csr &= ~MUSB_RXCSR_H_REQPKT;
		} else
			csr |= MUSB_RXCSR_P_WZC_BITS;

		
		csr &= ~(MUSB_RXCSR_DMAENAB);
		musb_writew(regs, MUSB_RXCSR, csr);
		csr = musb_readw(regs, MUSB_RXCSR);

		if (channel->status == MUSB_DMA_STATUS_BUSY)
			udelay(50);

		cppi_rx_scan(controller, cppi_ch->index);

		cppi_reset_rx(cppi_ch->state_ram);

		

		
		cppi_dump_rx(5, cppi_ch, " (done abort)");

		
		cppi_bd_free(cppi_ch, cppi_ch->last_processed);
		cppi_ch->last_processed = NULL;

		while (queue) {
			struct cppi_descriptor	*tmp = queue->next;

			cppi_bd_free(cppi_ch, queue);
			queue = tmp;
		}
	}

	channel->status = MUSB_DMA_STATUS_FREE;
	cppi_ch->buf_dma = 0;
	cppi_ch->offset = 0;
	cppi_ch->buf_len = 0;
	cppi_ch->maxpacket = 0;
	return 0;
}

