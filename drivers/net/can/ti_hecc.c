/*
 * TI HECC (CAN) device driver
 *
 * This driver supports TI's HECC (High End CAN Controller module) and the
 * specs for the same is available at <http://www.ti.com>
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed as is WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/platform/ti_hecc.h>

#define DRV_NAME "ti_hecc"
#define HECC_MODULE_VERSION     "0.7"
MODULE_VERSION(HECC_MODULE_VERSION);
#define DRV_DESC "TI High End CAN Controller Driver " HECC_MODULE_VERSION

#define HECC_MAX_MAILBOXES	32	
#define MAX_TX_PRIO		0x3F	

#define HECC_MB_TX_SHIFT	2 
#define HECC_MAX_TX_MBOX	BIT(HECC_MB_TX_SHIFT)

#define HECC_TX_PRIO_SHIFT	(HECC_MB_TX_SHIFT)
#define HECC_TX_PRIO_MASK	(MAX_TX_PRIO << HECC_MB_TX_SHIFT)
#define HECC_TX_MB_MASK		(HECC_MAX_TX_MBOX - 1)
#define HECC_TX_MASK		((HECC_MAX_TX_MBOX - 1) | HECC_TX_PRIO_MASK)
#define HECC_TX_MBOX_MASK	(~(BIT(HECC_MAX_TX_MBOX) - 1))
#define HECC_DEF_NAPI_WEIGHT	HECC_MAX_RX_MBOX


#define HECC_MAX_RX_MBOX	(HECC_MAX_MAILBOXES - HECC_MAX_TX_MBOX)
#define HECC_RX_BUFFER_MBOX	12 
#define HECC_RX_FIRST_MBOX	(HECC_MAX_MAILBOXES - 1)
#define HECC_RX_HIGH_MBOX_MASK	(~(BIT(HECC_RX_BUFFER_MBOX) - 1))

#define HECC_CANME		0x0	
#define HECC_CANMD		0x4	
#define HECC_CANTRS		0x8	
#define HECC_CANTRR		0xC	
#define HECC_CANTA		0x10	
#define HECC_CANAA		0x14	
#define HECC_CANRMP		0x18	
#define HECC_CANRML		0x1C	
#define HECC_CANRFP		0x20	
#define HECC_CANGAM		0x24	
#define HECC_CANMC		0x28	
#define HECC_CANBTC		0x2C	
#define HECC_CANES		0x30	
#define HECC_CANTEC		0x34	
#define HECC_CANREC		0x38	
#define HECC_CANGIF0		0x3C	
#define HECC_CANGIM		0x40	
#define HECC_CANGIF1		0x44	
#define HECC_CANMIM		0x48	
#define HECC_CANMIL		0x4C	
#define HECC_CANOPC		0x50	
#define HECC_CANTIOC		0x54	
#define HECC_CANRIOC		0x58	
#define HECC_CANLNT		0x5C	
#define HECC_CANTOC		0x60	
#define HECC_CANTOS		0x64	
#define HECC_CANTIOCE		0x68	
#define HECC_CANRIOCE		0x6C	

#define HECC_CANMID		0x0
#define HECC_CANMCF		0x4
#define HECC_CANMDL		0x8
#define HECC_CANMDH		0xC

#define HECC_SET_REG		0xFFFFFFFF
#define HECC_CANID_MASK		0x3FF	
#define HECC_CCE_WAIT_COUNT     100	

#define HECC_CANMC_SCM		BIT(13)	
#define HECC_CANMC_CCR		BIT(12)	
#define HECC_CANMC_PDR		BIT(11)	
#define HECC_CANMC_ABO		BIT(7)	
#define HECC_CANMC_STM		BIT(6)	
#define HECC_CANMC_SRES		BIT(5)	

#define HECC_CANTIOC_EN		BIT(3)	
#define HECC_CANRIOC_EN		BIT(3)	

#define HECC_CANMID_IDE		BIT(31)	
#define HECC_CANMID_AME		BIT(30)	
#define HECC_CANMID_AAM		BIT(29)	

#define HECC_CANES_FE		BIT(24)	
#define HECC_CANES_BE		BIT(23)	
#define HECC_CANES_SA1		BIT(22)	
#define HECC_CANES_CRCE		BIT(21)	
#define HECC_CANES_SE		BIT(20)	
#define HECC_CANES_ACKE		BIT(19)	
#define HECC_CANES_BO		BIT(18)	
#define HECC_CANES_EP		BIT(17)	
#define HECC_CANES_EW		BIT(16)	
#define HECC_CANES_SMA		BIT(5)	
#define HECC_CANES_CCE		BIT(4)	
#define HECC_CANES_PDA		BIT(3)	

#define HECC_CANBTC_SAM		BIT(7)	

#define HECC_BUS_ERROR		(HECC_CANES_FE | HECC_CANES_BE |\
				HECC_CANES_CRCE | HECC_CANES_SE |\
				HECC_CANES_ACKE)

#define HECC_CANMCF_RTR		BIT(4)	

#define HECC_CANGIF_MAIF	BIT(17)	
#define HECC_CANGIF_TCOIF	BIT(16) 
#define HECC_CANGIF_GMIF	BIT(15)	
#define HECC_CANGIF_AAIF	BIT(14)	
#define HECC_CANGIF_WDIF	BIT(13)	
#define HECC_CANGIF_WUIF	BIT(12)	
#define HECC_CANGIF_RMLIF	BIT(11)	
#define HECC_CANGIF_BOIF	BIT(10)	
#define HECC_CANGIF_EPIF	BIT(9)	
#define HECC_CANGIF_WLIF	BIT(8)	
#define HECC_CANGIF_MBOX_MASK	0x1F	
#define HECC_CANGIM_I1EN	BIT(1)	
#define HECC_CANGIM_I0EN	BIT(0)	
#define HECC_CANGIM_DEF_MASK	0x700	
#define HECC_CANGIM_SIL		BIT(2)	

static struct can_bittiming_const ti_hecc_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

struct ti_hecc_priv {
	struct can_priv can;	
	struct napi_struct napi;
	struct net_device *ndev;
	struct clk *clk;
	void __iomem *base;
	u32 scc_ram_offset;
	u32 hecc_ram_offset;
	u32 mbx_offset;
	u32 int_line;
	spinlock_t mbx_lock; 
	u32 tx_head;
	u32 tx_tail;
	u32 rx_next;
	void (*transceiver_switch)(int);
};

static inline int get_tx_head_mb(struct ti_hecc_priv *priv)
{
	return priv->tx_head & HECC_TX_MB_MASK;
}

static inline int get_tx_tail_mb(struct ti_hecc_priv *priv)
{
	return priv->tx_tail & HECC_TX_MB_MASK;
}

static inline int get_tx_head_prio(struct ti_hecc_priv *priv)
{
	return (priv->tx_head >> HECC_TX_PRIO_SHIFT) & MAX_TX_PRIO;
}

static inline void hecc_write_lam(struct ti_hecc_priv *priv, u32 mbxno, u32 val)
{
	__raw_writel(val, priv->base + priv->hecc_ram_offset + mbxno * 4);
}

static inline void hecc_write_mbx(struct ti_hecc_priv *priv, u32 mbxno,
	u32 reg, u32 val)
{
	__raw_writel(val, priv->base + priv->mbx_offset + mbxno * 0x10 +
			reg);
}

static inline u32 hecc_read_mbx(struct ti_hecc_priv *priv, u32 mbxno, u32 reg)
{
	return __raw_readl(priv->base + priv->mbx_offset + mbxno * 0x10 +
			reg);
}

static inline void hecc_write(struct ti_hecc_priv *priv, u32 reg, u32 val)
{
	__raw_writel(val, priv->base + reg);
}

static inline u32 hecc_read(struct ti_hecc_priv *priv, int reg)
{
	return __raw_readl(priv->base + reg);
}

static inline void hecc_set_bit(struct ti_hecc_priv *priv, int reg,
	u32 bit_mask)
{
	hecc_write(priv, reg, hecc_read(priv, reg) | bit_mask);
}

static inline void hecc_clear_bit(struct ti_hecc_priv *priv, int reg,
	u32 bit_mask)
{
	hecc_write(priv, reg, hecc_read(priv, reg) & ~bit_mask);
}

static inline u32 hecc_get_bit(struct ti_hecc_priv *priv, int reg, u32 bit_mask)
{
	return (hecc_read(priv, reg) & bit_mask) ? 1 : 0;
}

static int ti_hecc_get_state(const struct net_device *ndev,
	enum can_state *state)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	*state = priv->can.state;
	return 0;
}

static int ti_hecc_set_btc(struct ti_hecc_priv *priv)
{
	struct can_bittiming *bit_timing = &priv->can.bittiming;
	u32 can_btc;

	can_btc = (bit_timing->phase_seg2 - 1) & 0x7;
	can_btc |= ((bit_timing->phase_seg1 + bit_timing->prop_seg - 1)
			& 0xF) << 3;
	if (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES) {
		if (bit_timing->brp > 4)
			can_btc |= HECC_CANBTC_SAM;
		else
			netdev_warn(priv->ndev, "WARN: Triple"
				"sampling not set due to h/w limitations");
	}
	can_btc |= ((bit_timing->sjw - 1) & 0x3) << 8;
	can_btc |= ((bit_timing->brp - 1) & 0xFF) << 16;

	

	hecc_write(priv, HECC_CANBTC, can_btc);
	netdev_info(priv->ndev, "setting CANBTC=%#x\n", can_btc);

	return 0;
}

static void ti_hecc_transceiver_switch(const struct ti_hecc_priv *priv,
					int on)
{
	if (priv->transceiver_switch)
		priv->transceiver_switch(on);
}

static void ti_hecc_reset(struct net_device *ndev)
{
	u32 cnt;
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	netdev_dbg(ndev, "resetting hecc ...\n");
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_SRES);

	
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_CCR);

	cnt = HECC_CCE_WAIT_COUNT;
	while (!hecc_get_bit(priv, HECC_CANES, HECC_CANES_CCE) && cnt != 0) {
		--cnt;
		udelay(10);
	}

	ti_hecc_set_btc(priv);

	
	hecc_write(priv, HECC_CANMC, 0);


	cnt = HECC_CCE_WAIT_COUNT;
	while (hecc_get_bit(priv, HECC_CANES, HECC_CANES_CCE) && cnt != 0) {
		--cnt;
		udelay(10);
	}

	
	hecc_write(priv, HECC_CANTIOC, HECC_CANTIOC_EN);
	hecc_write(priv, HECC_CANRIOC, HECC_CANRIOC_EN);

	
	hecc_write(priv, HECC_CANTA, HECC_SET_REG);
	hecc_write(priv, HECC_CANRMP, HECC_SET_REG);
	hecc_write(priv, HECC_CANGIF0, HECC_SET_REG);
	hecc_write(priv, HECC_CANGIF1, HECC_SET_REG);
	hecc_write(priv, HECC_CANME, 0);
	hecc_write(priv, HECC_CANMD, 0);

	
	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_SCM);
}

static void ti_hecc_start(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	u32 cnt, mbxno, mbx_mask;

	
	ti_hecc_reset(ndev);

	priv->tx_head = priv->tx_tail = HECC_TX_MASK;
	priv->rx_next = HECC_RX_FIRST_MBOX;

	
	hecc_write(priv, HECC_CANGAM, HECC_SET_REG);

	
	for (cnt = 0; cnt < HECC_MAX_RX_MBOX; cnt++) {
		mbxno = HECC_MAX_MAILBOXES - 1 - cnt;
		mbx_mask = BIT(mbxno);
		hecc_clear_bit(priv, HECC_CANME, mbx_mask);
		hecc_write_mbx(priv, mbxno, HECC_CANMID, HECC_CANMID_AME);
		hecc_write_lam(priv, mbxno, HECC_SET_REG);
		hecc_set_bit(priv, HECC_CANMD, mbx_mask);
		hecc_set_bit(priv, HECC_CANME, mbx_mask);
		hecc_set_bit(priv, HECC_CANMIM, mbx_mask);
	}

	
	hecc_write(priv, HECC_CANOPC, HECC_SET_REG);
	if (priv->int_line) {
		hecc_write(priv, HECC_CANMIL, HECC_SET_REG);
		hecc_write(priv, HECC_CANGIM, HECC_CANGIM_DEF_MASK |
			HECC_CANGIM_I1EN | HECC_CANGIM_SIL);
	} else {
		hecc_write(priv, HECC_CANMIL, 0);
		hecc_write(priv, HECC_CANGIM,
			HECC_CANGIM_DEF_MASK | HECC_CANGIM_I0EN);
	}
	priv->can.state = CAN_STATE_ERROR_ACTIVE;
}

static void ti_hecc_stop(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	
	hecc_write(priv, HECC_CANGIM, 0);
	hecc_write(priv, HECC_CANMIM, 0);
	hecc_write(priv, HECC_CANME, 0);
	priv->can.state = CAN_STATE_STOPPED;
}

static int ti_hecc_do_set_mode(struct net_device *ndev, enum can_mode mode)
{
	int ret = 0;

	switch (mode) {
	case CAN_MODE_START:
		ti_hecc_start(ndev);
		netif_wake_queue(ndev);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}

static int ti_hecc_get_berr_counter(const struct net_device *ndev,
					struct can_berr_counter *bec)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	bec->txerr = hecc_read(priv, HECC_CANTEC);
	bec->rxerr = hecc_read(priv, HECC_CANREC);

	return 0;
}

static netdev_tx_t ti_hecc_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 mbxno, mbx_mask, data;
	unsigned long flags;

	if (can_dropped_invalid_skb(ndev, skb))
		return NETDEV_TX_OK;

	mbxno = get_tx_head_mb(priv);
	mbx_mask = BIT(mbxno);
	spin_lock_irqsave(&priv->mbx_lock, flags);
	if (unlikely(hecc_read(priv, HECC_CANME) & mbx_mask)) {
		spin_unlock_irqrestore(&priv->mbx_lock, flags);
		netif_stop_queue(ndev);
		netdev_err(priv->ndev,
			"BUG: TX mbx not ready tx_head=%08X, tx_tail=%08X\n",
			priv->tx_head, priv->tx_tail);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_irqrestore(&priv->mbx_lock, flags);

	
	data = cf->can_dlc | (get_tx_head_prio(priv) << 8);
	if (cf->can_id & CAN_RTR_FLAG) 
		data |= HECC_CANMCF_RTR;
	hecc_write_mbx(priv, mbxno, HECC_CANMCF, data);

	if (cf->can_id & CAN_EFF_FLAG) 
		data = (cf->can_id & CAN_EFF_MASK) | HECC_CANMID_IDE;
	else 
		data = (cf->can_id & CAN_SFF_MASK) << 18;
	hecc_write_mbx(priv, mbxno, HECC_CANMID, data);
	hecc_write_mbx(priv, mbxno, HECC_CANMDL,
		be32_to_cpu(*(u32 *)(cf->data)));
	if (cf->can_dlc > 4)
		hecc_write_mbx(priv, mbxno, HECC_CANMDH,
			be32_to_cpu(*(u32 *)(cf->data + 4)));
	else
		*(u32 *)(cf->data + 4) = 0;
	can_put_echo_skb(skb, ndev, mbxno);

	spin_lock_irqsave(&priv->mbx_lock, flags);
	--priv->tx_head;
	if ((hecc_read(priv, HECC_CANME) & BIT(get_tx_head_mb(priv))) ||
		(priv->tx_head & HECC_TX_MASK) == HECC_TX_MASK) {
		netif_stop_queue(ndev);
	}
	hecc_set_bit(priv, HECC_CANME, mbx_mask);
	spin_unlock_irqrestore(&priv->mbx_lock, flags);

	hecc_clear_bit(priv, HECC_CANMD, mbx_mask);
	hecc_set_bit(priv, HECC_CANMIM, mbx_mask);
	hecc_write(priv, HECC_CANTRS, mbx_mask);

	return NETDEV_TX_OK;
}

static int ti_hecc_rx_pkt(struct ti_hecc_priv *priv, int mbxno)
{
	struct net_device_stats *stats = &priv->ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 data, mbx_mask;
	unsigned long flags;

	skb = alloc_can_skb(priv->ndev, &cf);
	if (!skb) {
		if (printk_ratelimit())
			netdev_err(priv->ndev,
				"ti_hecc_rx_pkt: alloc_can_skb() failed\n");
		return -ENOMEM;
	}

	mbx_mask = BIT(mbxno);
	data = hecc_read_mbx(priv, mbxno, HECC_CANMID);
	if (data & HECC_CANMID_IDE)
		cf->can_id = (data & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		cf->can_id = (data >> 18) & CAN_SFF_MASK;
	data = hecc_read_mbx(priv, mbxno, HECC_CANMCF);
	if (data & HECC_CANMCF_RTR)
		cf->can_id |= CAN_RTR_FLAG;
	cf->can_dlc = get_can_dlc(data & 0xF);
	data = hecc_read_mbx(priv, mbxno, HECC_CANMDL);
	*(u32 *)(cf->data) = cpu_to_be32(data);
	if (cf->can_dlc > 4) {
		data = hecc_read_mbx(priv, mbxno, HECC_CANMDH);
		*(u32 *)(cf->data + 4) = cpu_to_be32(data);
	} else {
		*(u32 *)(cf->data + 4) = 0;
	}
	spin_lock_irqsave(&priv->mbx_lock, flags);
	hecc_clear_bit(priv, HECC_CANME, mbx_mask);
	hecc_write(priv, HECC_CANRMP, mbx_mask);
	
	if (priv->rx_next < HECC_RX_BUFFER_MBOX)
		hecc_set_bit(priv, HECC_CANME, mbx_mask);
	spin_unlock_irqrestore(&priv->mbx_lock, flags);

	stats->rx_bytes += cf->can_dlc;
	netif_receive_skb(skb);
	stats->rx_packets++;

	return 0;
}

static int ti_hecc_rx_poll(struct napi_struct *napi, int quota)
{
	struct net_device *ndev = napi->dev;
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	u32 num_pkts = 0;
	u32 mbx_mask;
	unsigned long pending_pkts, flags;

	if (!netif_running(ndev))
		return 0;

	while ((pending_pkts = hecc_read(priv, HECC_CANRMP)) &&
		num_pkts < quota) {
		mbx_mask = BIT(priv->rx_next); 
		if (mbx_mask & pending_pkts) {
			if (ti_hecc_rx_pkt(priv, priv->rx_next) < 0)
				return num_pkts;
			++num_pkts;
		} else if (priv->rx_next > HECC_RX_BUFFER_MBOX) {
			break; 
		}
		--priv->rx_next;
		if (priv->rx_next == HECC_RX_BUFFER_MBOX) {
			
			spin_lock_irqsave(&priv->mbx_lock, flags);
			mbx_mask = hecc_read(priv, HECC_CANME);
			mbx_mask |= HECC_RX_HIGH_MBOX_MASK;
			hecc_write(priv, HECC_CANME, mbx_mask);
			spin_unlock_irqrestore(&priv->mbx_lock, flags);
		} else if (priv->rx_next == HECC_MAX_TX_MBOX - 1) {
			priv->rx_next = HECC_RX_FIRST_MBOX;
			break;
		}
	}

	
	if (hecc_read(priv, HECC_CANRMP) == 0) {
		napi_complete(napi);
		
		mbx_mask = hecc_read(priv, HECC_CANMIM);
		mbx_mask |= HECC_TX_MBOX_MASK;
		hecc_write(priv, HECC_CANMIM, mbx_mask);
	}

	return num_pkts;
}

static int ti_hecc_error(struct net_device *ndev, int int_status,
	int err_status)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;

	
	skb = alloc_can_err_skb(ndev, &cf);
	if (!skb) {
		if (printk_ratelimit())
			netdev_err(priv->ndev,
				"ti_hecc_error: alloc_can_err_skb() failed\n");
		return -ENOMEM;
	}

	if (int_status & HECC_CANGIF_WLIF) { 
		if ((int_status & HECC_CANGIF_BOIF) == 0) {
			priv->can.state = CAN_STATE_ERROR_WARNING;
			++priv->can.can_stats.error_warning;
			cf->can_id |= CAN_ERR_CRTL;
			if (hecc_read(priv, HECC_CANTEC) > 96)
				cf->data[1] |= CAN_ERR_CRTL_TX_WARNING;
			if (hecc_read(priv, HECC_CANREC) > 96)
				cf->data[1] |= CAN_ERR_CRTL_RX_WARNING;
		}
		hecc_set_bit(priv, HECC_CANES, HECC_CANES_EW);
		netdev_dbg(priv->ndev, "Error Warning interrupt\n");
		hecc_clear_bit(priv, HECC_CANMC, HECC_CANMC_CCR);
	}

	if (int_status & HECC_CANGIF_EPIF) { 
		if ((int_status & HECC_CANGIF_BOIF) == 0) {
			priv->can.state = CAN_STATE_ERROR_PASSIVE;
			++priv->can.can_stats.error_passive;
			cf->can_id |= CAN_ERR_CRTL;
			if (hecc_read(priv, HECC_CANTEC) > 127)
				cf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE;
			if (hecc_read(priv, HECC_CANREC) > 127)
				cf->data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
		}
		hecc_set_bit(priv, HECC_CANES, HECC_CANES_EP);
		netdev_dbg(priv->ndev, "Error passive interrupt\n");
		hecc_clear_bit(priv, HECC_CANMC, HECC_CANMC_CCR);
	}

	if ((int_status & HECC_CANGIF_BOIF) || (err_status & HECC_CANES_BO)) {
		priv->can.state = CAN_STATE_BUS_OFF;
		cf->can_id |= CAN_ERR_BUSOFF;
		hecc_set_bit(priv, HECC_CANES, HECC_CANES_BO);
		hecc_clear_bit(priv, HECC_CANMC, HECC_CANMC_CCR);
		
		hecc_write(priv, HECC_CANGIM, 0);
		can_bus_off(ndev);
	}

	if (err_status & HECC_BUS_ERROR) {
		++priv->can.can_stats.bus_error;
		cf->can_id |= CAN_ERR_BUSERROR | CAN_ERR_PROT;
		cf->data[2] |= CAN_ERR_PROT_UNSPEC;
		if (err_status & HECC_CANES_FE) {
			hecc_set_bit(priv, HECC_CANES, HECC_CANES_FE);
			cf->data[2] |= CAN_ERR_PROT_FORM;
		}
		if (err_status & HECC_CANES_BE) {
			hecc_set_bit(priv, HECC_CANES, HECC_CANES_BE);
			cf->data[2] |= CAN_ERR_PROT_BIT;
		}
		if (err_status & HECC_CANES_SE) {
			hecc_set_bit(priv, HECC_CANES, HECC_CANES_SE);
			cf->data[2] |= CAN_ERR_PROT_STUFF;
		}
		if (err_status & HECC_CANES_CRCE) {
			hecc_set_bit(priv, HECC_CANES, HECC_CANES_CRCE);
			cf->data[2] |= CAN_ERR_PROT_LOC_CRC_SEQ |
					CAN_ERR_PROT_LOC_CRC_DEL;
		}
		if (err_status & HECC_CANES_ACKE) {
			hecc_set_bit(priv, HECC_CANES, HECC_CANES_ACKE);
			cf->data[2] |= CAN_ERR_PROT_LOC_ACK |
					CAN_ERR_PROT_LOC_ACK_DEL;
		}
	}

	netif_rx(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

	return 0;
}

static irqreturn_t ti_hecc_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = (struct net_device *)dev_id;
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	u32 mbxno, mbx_mask, int_status, err_status;
	unsigned long ack, flags;

	int_status = hecc_read(priv,
		(priv->int_line) ? HECC_CANGIF1 : HECC_CANGIF0);

	if (!int_status)
		return IRQ_NONE;

	err_status = hecc_read(priv, HECC_CANES);
	if (err_status & (HECC_BUS_ERROR | HECC_CANES_BO |
		HECC_CANES_EP | HECC_CANES_EW))
			ti_hecc_error(ndev, int_status, err_status);

	if (int_status & HECC_CANGIF_GMIF) {
		while (priv->tx_tail - priv->tx_head > 0) {
			mbxno = get_tx_tail_mb(priv);
			mbx_mask = BIT(mbxno);
			if (!(mbx_mask & hecc_read(priv, HECC_CANTA)))
				break;
			hecc_clear_bit(priv, HECC_CANMIM, mbx_mask);
			hecc_write(priv, HECC_CANTA, mbx_mask);
			spin_lock_irqsave(&priv->mbx_lock, flags);
			hecc_clear_bit(priv, HECC_CANME, mbx_mask);
			spin_unlock_irqrestore(&priv->mbx_lock, flags);
			stats->tx_bytes += hecc_read_mbx(priv, mbxno,
						HECC_CANMCF) & 0xF;
			stats->tx_packets++;
			can_get_echo_skb(ndev, mbxno);
			--priv->tx_tail;
		}

		
		if (((priv->tx_head == priv->tx_tail) &&
		((priv->tx_head & HECC_TX_MASK) != HECC_TX_MASK)) ||
		(((priv->tx_tail & HECC_TX_MASK) == HECC_TX_MASK) &&
		((priv->tx_head & HECC_TX_MASK) == HECC_TX_MASK)))
			netif_wake_queue(ndev);

		
		if (hecc_read(priv, HECC_CANRMP)) {
			ack = hecc_read(priv, HECC_CANMIM);
			ack &= BIT(HECC_MAX_TX_MBOX) - 1;
			hecc_write(priv, HECC_CANMIM, ack);
			napi_schedule(&priv->napi);
		}
	}

	
	if (priv->int_line) {
		hecc_write(priv, HECC_CANGIF1, HECC_SET_REG);
		int_status = hecc_read(priv, HECC_CANGIF1);
	} else {
		hecc_write(priv, HECC_CANGIF0, HECC_SET_REG);
		int_status = hecc_read(priv, HECC_CANGIF0);
	}

	return IRQ_HANDLED;
}

static int ti_hecc_open(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);
	int err;

	err = request_irq(ndev->irq, ti_hecc_interrupt, IRQF_SHARED,
			ndev->name, ndev);
	if (err) {
		netdev_err(ndev, "error requesting interrupt\n");
		return err;
	}

	ti_hecc_transceiver_switch(priv, 1);

	
	err = open_candev(ndev);
	if (err) {
		netdev_err(ndev, "open_candev() failed %d\n", err);
		ti_hecc_transceiver_switch(priv, 0);
		free_irq(ndev->irq, ndev);
		return err;
	}

	ti_hecc_start(ndev);
	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	return 0;
}

static int ti_hecc_close(struct net_device *ndev)
{
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	ti_hecc_stop(ndev);
	free_irq(ndev->irq, ndev);
	close_candev(ndev);
	ti_hecc_transceiver_switch(priv, 0);

	return 0;
}

static const struct net_device_ops ti_hecc_netdev_ops = {
	.ndo_open		= ti_hecc_open,
	.ndo_stop		= ti_hecc_close,
	.ndo_start_xmit		= ti_hecc_xmit,
};

static int ti_hecc_probe(struct platform_device *pdev)
{
	struct net_device *ndev = (struct net_device *)0;
	struct ti_hecc_priv *priv;
	struct ti_hecc_platform_data *pdata;
	struct resource *mem, *irq;
	void __iomem *addr;
	int err = -ENODEV;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		goto probe_exit;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No mem resources\n");
		goto probe_exit;
	}
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "No irq resource\n");
		goto probe_exit;
	}
	if (!request_mem_region(mem->start, resource_size(mem), pdev->name)) {
		dev_err(&pdev->dev, "HECC region already claimed\n");
		err = -EBUSY;
		goto probe_exit;
	}
	addr = ioremap(mem->start, resource_size(mem));
	if (!addr) {
		dev_err(&pdev->dev, "ioremap failed\n");
		err = -ENOMEM;
		goto probe_exit_free_region;
	}

	ndev = alloc_candev(sizeof(struct ti_hecc_priv), HECC_MAX_TX_MBOX);
	if (!ndev) {
		dev_err(&pdev->dev, "alloc_candev failed\n");
		err = -ENOMEM;
		goto probe_exit_iounmap;
	}

	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->base = addr;
	priv->scc_ram_offset = pdata->scc_ram_offset;
	priv->hecc_ram_offset = pdata->hecc_ram_offset;
	priv->mbx_offset = pdata->mbx_offset;
	priv->int_line = pdata->int_line;
	priv->transceiver_switch = pdata->transceiver_switch;

	priv->can.bittiming_const = &ti_hecc_bittiming_const;
	priv->can.do_set_mode = ti_hecc_do_set_mode;
	priv->can.do_get_state = ti_hecc_get_state;
	priv->can.do_get_berr_counter = ti_hecc_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES;

	spin_lock_init(&priv->mbx_lock);
	ndev->irq = irq->start;
	ndev->flags |= IFF_ECHO;
	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->netdev_ops = &ti_hecc_netdev_ops;

	priv->clk = clk_get(&pdev->dev, "hecc_ck");
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "No clock available\n");
		err = PTR_ERR(priv->clk);
		priv->clk = NULL;
		goto probe_exit_candev;
	}
	priv->can.clock.freq = clk_get_rate(priv->clk);
	netif_napi_add(ndev, &priv->napi, ti_hecc_rx_poll,
		HECC_DEF_NAPI_WEIGHT);

	clk_enable(priv->clk);
	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev, "register_candev() failed\n");
		goto probe_exit_clk;
	}
	dev_info(&pdev->dev, "device registered (reg_base=%p, irq=%u)\n",
		priv->base, (u32) ndev->irq);

	return 0;

probe_exit_clk:
	clk_put(priv->clk);
probe_exit_candev:
	free_candev(ndev);
probe_exit_iounmap:
	iounmap(addr);
probe_exit_free_region:
	release_mem_region(mem->start, resource_size(mem));
probe_exit:
	return err;
}

static int __devexit ti_hecc_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(ndev);

	clk_disable(priv->clk);
	clk_put(priv->clk);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(priv->base);
	release_mem_region(res->start, resource_size(res));
	unregister_candev(ndev);
	free_candev(ndev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}


#ifdef CONFIG_PM
static int ti_hecc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(dev);

	if (netif_running(dev)) {
		netif_stop_queue(dev);
		netif_device_detach(dev);
	}

	hecc_set_bit(priv, HECC_CANMC, HECC_CANMC_PDR);
	priv->can.state = CAN_STATE_SLEEPING;

	clk_disable(priv->clk);

	return 0;
}

static int ti_hecc_resume(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct ti_hecc_priv *priv = netdev_priv(dev);

	clk_enable(priv->clk);

	hecc_clear_bit(priv, HECC_CANMC, HECC_CANMC_PDR);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	if (netif_running(dev)) {
		netif_device_attach(dev);
		netif_start_queue(dev);
	}

	return 0;
}
#else
#define ti_hecc_suspend NULL
#define ti_hecc_resume NULL
#endif

static struct platform_driver ti_hecc_driver = {
	.driver = {
		.name    = DRV_NAME,
		.owner   = THIS_MODULE,
	},
	.probe = ti_hecc_probe,
	.remove = __devexit_p(ti_hecc_remove),
	.suspend = ti_hecc_suspend,
	.resume = ti_hecc_resume,
};

module_platform_driver(ti_hecc_driver);

MODULE_AUTHOR("Anant Gole <anantgole@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRV_DESC);
