/*
 * CAN bus driver for Bosch C_CAN controller
 *
 * Copyright (C) 2010 ST Microelectronics
 * Bhupesh Sharma <bhupesh.sharma@st.com>
 *
 * Borrowed heavily from the C_CAN driver originally written by:
 * Copyright (C) 2007
 * - Sascha Hauer, Marc Kleine-Budde, Pengutronix <s.hauer@pengutronix.de>
 * - Simon Kallweit, intefo AG <simon.kallweit@intefo.ch>
 *
 * TX and RX NAPI implementation has been borrowed from at91 CAN driver
 * written by:
 * Copyright
 * (C) 2007 by Hans J. Koch <hjk@hansjkoch.de>
 * (C) 2008, 2009 by Marc Kleine-Budde <kernel@pengutronix.de>
 *
 * Bosch C_CAN controller is compliant to CAN protocol version 2.0 part A and B.
 * Bosch C_CAN user manual can be obtained from:
 * http://www.semiconductors.bosch.de/media/en/pdf/ipmodules_1/c_can/
 * users_manual_c_can.pdf
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/list.h>
#include <linux/io.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

#include "c_can.h"

#define CONTROL_TEST		BIT(7)
#define CONTROL_CCE		BIT(6)
#define CONTROL_DISABLE_AR	BIT(5)
#define CONTROL_ENABLE_AR	(0 << 5)
#define CONTROL_EIE		BIT(3)
#define CONTROL_SIE		BIT(2)
#define CONTROL_IE		BIT(1)
#define CONTROL_INIT		BIT(0)

#define TEST_RX			BIT(7)
#define TEST_TX1		BIT(6)
#define TEST_TX2		BIT(5)
#define TEST_LBACK		BIT(4)
#define TEST_SILENT		BIT(3)
#define TEST_BASIC		BIT(2)

#define STATUS_BOFF		BIT(7)
#define STATUS_EWARN		BIT(6)
#define STATUS_EPASS		BIT(5)
#define STATUS_RXOK		BIT(4)
#define STATUS_TXOK		BIT(3)

#define ERR_CNT_TEC_MASK	0xff
#define ERR_CNT_TEC_SHIFT	0
#define ERR_CNT_REC_SHIFT	8
#define ERR_CNT_REC_MASK	(0x7f << ERR_CNT_REC_SHIFT)
#define ERR_CNT_RP_SHIFT	15
#define ERR_CNT_RP_MASK		(0x1 << ERR_CNT_RP_SHIFT)

#define BTR_BRP_MASK		0x3f
#define BTR_BRP_SHIFT		0
#define BTR_SJW_SHIFT		6
#define BTR_SJW_MASK		(0x3 << BTR_SJW_SHIFT)
#define BTR_TSEG1_SHIFT		8
#define BTR_TSEG1_MASK		(0xf << BTR_TSEG1_SHIFT)
#define BTR_TSEG2_SHIFT		12
#define BTR_TSEG2_MASK		(0x7 << BTR_TSEG2_SHIFT)

#define BRP_EXT_BRPE_MASK	0x0f
#define BRP_EXT_BRPE_SHIFT	0

#define IF_COMR_BUSY		BIT(15)

#define IF_COMM_WR		BIT(7)
#define IF_COMM_MASK		BIT(6)
#define IF_COMM_ARB		BIT(5)
#define IF_COMM_CONTROL		BIT(4)
#define IF_COMM_CLR_INT_PND	BIT(3)
#define IF_COMM_TXRQST		BIT(2)
#define IF_COMM_DATAA		BIT(1)
#define IF_COMM_DATAB		BIT(0)
#define IF_COMM_ALL		(IF_COMM_MASK | IF_COMM_ARB | \
				IF_COMM_CONTROL | IF_COMM_TXRQST | \
				IF_COMM_DATAA | IF_COMM_DATAB)

#define IF_ARB_MSGVAL		BIT(15)
#define IF_ARB_MSGXTD		BIT(14)
#define IF_ARB_TRANSMIT		BIT(13)

#define IF_MCONT_NEWDAT		BIT(15)
#define IF_MCONT_MSGLST		BIT(14)
#define IF_MCONT_CLR_MSGLST	(0 << 14)
#define IF_MCONT_INTPND		BIT(13)
#define IF_MCONT_UMASK		BIT(12)
#define IF_MCONT_TXIE		BIT(11)
#define IF_MCONT_RXIE		BIT(10)
#define IF_MCONT_RMTEN		BIT(9)
#define IF_MCONT_TXRQST		BIT(8)
#define IF_MCONT_EOB		BIT(7)
#define IF_MCONT_DLC_MASK	0xf

#define IFX_WRITE_LOW_16BIT(x)	((x) & 0xFFFF)
#define IFX_WRITE_HIGH_16BIT(x)	(((x) & 0xFFFF0000) >> 16)

#define C_CAN_NO_OF_OBJECTS	32
#define C_CAN_MSG_OBJ_RX_NUM	16
#define C_CAN_MSG_OBJ_TX_NUM	16

#define C_CAN_MSG_OBJ_RX_FIRST	1
#define C_CAN_MSG_OBJ_RX_LAST	(C_CAN_MSG_OBJ_RX_FIRST + \
				C_CAN_MSG_OBJ_RX_NUM - 1)

#define C_CAN_MSG_OBJ_TX_FIRST	(C_CAN_MSG_OBJ_RX_LAST + 1)
#define C_CAN_MSG_OBJ_TX_LAST	(C_CAN_MSG_OBJ_TX_FIRST + \
				C_CAN_MSG_OBJ_TX_NUM - 1)

#define C_CAN_MSG_OBJ_RX_SPLIT	9
#define C_CAN_MSG_RX_LOW_LAST	(C_CAN_MSG_OBJ_RX_SPLIT - 1)

#define C_CAN_NEXT_MSG_OBJ_MASK	(C_CAN_MSG_OBJ_TX_NUM - 1)
#define RECEIVE_OBJECT_BITS	0x0000ffff

#define STATUS_INTERRUPT	0x8000

#define ENABLE_ALL_INTERRUPTS	1
#define DISABLE_ALL_INTERRUPTS	0

#define MIN_TIMEOUT_VALUE	6

#define C_CAN_NAPI_WEIGHT	C_CAN_MSG_OBJ_RX_NUM

enum c_can_lec_type {
	LEC_NO_ERROR = 0,
	LEC_STUFF_ERROR,
	LEC_FORM_ERROR,
	LEC_ACK_ERROR,
	LEC_BIT1_ERROR,
	LEC_BIT0_ERROR,
	LEC_CRC_ERROR,
	LEC_UNUSED,
};

enum c_can_bus_error_types {
	C_CAN_NO_ERROR = 0,
	C_CAN_BUS_OFF,
	C_CAN_ERROR_WARNING,
	C_CAN_ERROR_PASSIVE,
};

static struct can_bittiming_const c_can_bittiming_const = {
	.name = KBUILD_MODNAME,
	.tseg1_min = 2,		
	.tseg1_max = 16,
	.tseg2_min = 1,		
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 1024,	
	.brp_inc = 1,
};

static inline int get_tx_next_msg_obj(const struct c_can_priv *priv)
{
	return (priv->tx_next & C_CAN_NEXT_MSG_OBJ_MASK) +
			C_CAN_MSG_OBJ_TX_FIRST;
}

static inline int get_tx_echo_msg_obj(const struct c_can_priv *priv)
{
	return (priv->tx_echo & C_CAN_NEXT_MSG_OBJ_MASK) +
			C_CAN_MSG_OBJ_TX_FIRST;
}

static u32 c_can_read_reg32(struct c_can_priv *priv, void *reg)
{
	u32 val = priv->read_reg(priv, reg);
	val |= ((u32) priv->read_reg(priv, reg + 2)) << 16;
	return val;
}

static void c_can_enable_all_interrupts(struct c_can_priv *priv,
						int enable)
{
	unsigned int cntrl_save = priv->read_reg(priv,
						&priv->regs->control);

	if (enable)
		cntrl_save |= (CONTROL_SIE | CONTROL_EIE | CONTROL_IE);
	else
		cntrl_save &= ~(CONTROL_EIE | CONTROL_IE | CONTROL_SIE);

	priv->write_reg(priv, &priv->regs->control, cntrl_save);
}

static inline int c_can_msg_obj_is_busy(struct c_can_priv *priv, int iface)
{
	int count = MIN_TIMEOUT_VALUE;

	while (count && priv->read_reg(priv,
				&priv->regs->ifregs[iface].com_req) &
				IF_COMR_BUSY) {
		count--;
		udelay(1);
	}

	if (!count)
		return 1;

	return 0;
}

static inline void c_can_object_get(struct net_device *dev,
					int iface, int objno, int mask)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].com_mask,
			IFX_WRITE_LOW_16BIT(mask));
	priv->write_reg(priv, &priv->regs->ifregs[iface].com_req,
			IFX_WRITE_LOW_16BIT(objno));

	if (c_can_msg_obj_is_busy(priv, iface))
		netdev_err(dev, "timed out in object get\n");
}

static inline void c_can_object_put(struct net_device *dev,
					int iface, int objno, int mask)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].com_mask,
			(IF_COMM_WR | IFX_WRITE_LOW_16BIT(mask)));
	priv->write_reg(priv, &priv->regs->ifregs[iface].com_req,
			IFX_WRITE_LOW_16BIT(objno));

	if (c_can_msg_obj_is_busy(priv, iface))
		netdev_err(dev, "timed out in object put\n");
}

static void c_can_write_msg_object(struct net_device *dev,
			int iface, struct can_frame *frame, int objno)
{
	int i;
	u16 flags = 0;
	unsigned int id;
	struct c_can_priv *priv = netdev_priv(dev);

	if (!(frame->can_id & CAN_RTR_FLAG))
		flags |= IF_ARB_TRANSMIT;

	if (frame->can_id & CAN_EFF_FLAG) {
		id = frame->can_id & CAN_EFF_MASK;
		flags |= IF_ARB_MSGXTD;
	} else
		id = ((frame->can_id & CAN_SFF_MASK) << 18);

	flags |= IF_ARB_MSGVAL;

	priv->write_reg(priv, &priv->regs->ifregs[iface].arb1,
				IFX_WRITE_LOW_16BIT(id));
	priv->write_reg(priv, &priv->regs->ifregs[iface].arb2, flags |
				IFX_WRITE_HIGH_16BIT(id));

	for (i = 0; i < frame->can_dlc; i += 2) {
		priv->write_reg(priv, &priv->regs->ifregs[iface].data[i / 2],
				frame->data[i] | (frame->data[i + 1] << 8));
	}

	
	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl,
			IF_MCONT_TXIE | IF_MCONT_TXRQST | IF_MCONT_EOB |
			frame->can_dlc);
	c_can_object_put(dev, iface, objno, IF_COMM_ALL);
}

static inline void c_can_mark_rx_msg_obj(struct net_device *dev,
						int iface, int ctrl_mask,
						int obj)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl,
			ctrl_mask & ~(IF_MCONT_MSGLST | IF_MCONT_INTPND));
	c_can_object_put(dev, iface, obj, IF_COMM_CONTROL);

}

static inline void c_can_activate_all_lower_rx_msg_obj(struct net_device *dev,
						int iface,
						int ctrl_mask)
{
	int i;
	struct c_can_priv *priv = netdev_priv(dev);

	for (i = C_CAN_MSG_OBJ_RX_FIRST; i <= C_CAN_MSG_RX_LOW_LAST; i++) {
		priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl,
				ctrl_mask & ~(IF_MCONT_MSGLST |
					IF_MCONT_INTPND | IF_MCONT_NEWDAT));
		c_can_object_put(dev, iface, i, IF_COMM_CONTROL);
	}
}

static inline void c_can_activate_rx_msg_obj(struct net_device *dev,
						int iface, int ctrl_mask,
						int obj)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl,
			ctrl_mask & ~(IF_MCONT_MSGLST |
				IF_MCONT_INTPND | IF_MCONT_NEWDAT));
	c_can_object_put(dev, iface, obj, IF_COMM_CONTROL);
}

static void c_can_handle_lost_msg_obj(struct net_device *dev,
					int iface, int objno)
{
	struct c_can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct sk_buff *skb;
	struct can_frame *frame;

	netdev_err(dev, "msg lost in buffer %d\n", objno);

	c_can_object_get(dev, iface, objno, IF_COMM_ALL & ~IF_COMM_TXRQST);

	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl,
			IF_MCONT_CLR_MSGLST);

	c_can_object_put(dev, 0, objno, IF_COMM_CONTROL);

	
	skb = alloc_can_err_skb(dev, &frame);
	if (unlikely(!skb))
		return;

	frame->can_id |= CAN_ERR_CRTL;
	frame->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
	stats->rx_errors++;
	stats->rx_over_errors++;

	netif_receive_skb(skb);
}

static int c_can_read_msg_object(struct net_device *dev, int iface, int ctrl)
{
	u16 flags, data;
	int i;
	unsigned int val;
	struct c_can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct sk_buff *skb;
	struct can_frame *frame;

	skb = alloc_can_skb(dev, &frame);
	if (!skb) {
		stats->rx_dropped++;
		return -ENOMEM;
	}

	frame->can_dlc = get_can_dlc(ctrl & 0x0F);

	flags =	priv->read_reg(priv, &priv->regs->ifregs[iface].arb2);
	val = priv->read_reg(priv, &priv->regs->ifregs[iface].arb1) |
		(flags << 16);

	if (flags & IF_ARB_MSGXTD)
		frame->can_id = (val & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		frame->can_id = (val >> 18) & CAN_SFF_MASK;

	if (flags & IF_ARB_TRANSMIT)
		frame->can_id |= CAN_RTR_FLAG;
	else {
		for (i = 0; i < frame->can_dlc; i += 2) {
			data = priv->read_reg(priv,
				&priv->regs->ifregs[iface].data[i / 2]);
			frame->data[i] = data;
			frame->data[i + 1] = data >> 8;
		}
	}

	netif_receive_skb(skb);

	stats->rx_packets++;
	stats->rx_bytes += frame->can_dlc;

	return 0;
}

static void c_can_setup_receive_object(struct net_device *dev, int iface,
					int objno, unsigned int mask,
					unsigned int id, unsigned int mcont)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].mask1,
			IFX_WRITE_LOW_16BIT(mask));
	priv->write_reg(priv, &priv->regs->ifregs[iface].mask2,
			IFX_WRITE_HIGH_16BIT(mask));

	priv->write_reg(priv, &priv->regs->ifregs[iface].arb1,
			IFX_WRITE_LOW_16BIT(id));
	priv->write_reg(priv, &priv->regs->ifregs[iface].arb2,
			(IF_ARB_MSGVAL | IFX_WRITE_HIGH_16BIT(id)));

	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl, mcont);
	c_can_object_put(dev, iface, objno, IF_COMM_ALL & ~IF_COMM_TXRQST);

	netdev_dbg(dev, "obj no:%d, msgval:0x%08x\n", objno,
			c_can_read_reg32(priv, &priv->regs->msgval1));
}

static void c_can_inval_msg_object(struct net_device *dev, int iface, int objno)
{
	struct c_can_priv *priv = netdev_priv(dev);

	priv->write_reg(priv, &priv->regs->ifregs[iface].arb1, 0);
	priv->write_reg(priv, &priv->regs->ifregs[iface].arb2, 0);
	priv->write_reg(priv, &priv->regs->ifregs[iface].msg_cntrl, 0);

	c_can_object_put(dev, iface, objno, IF_COMM_ARB | IF_COMM_CONTROL);

	netdev_dbg(dev, "obj no:%d, msgval:0x%08x\n", objno,
			c_can_read_reg32(priv, &priv->regs->msgval1));
}

static inline int c_can_is_next_tx_obj_busy(struct c_can_priv *priv, int objno)
{
	int val = c_can_read_reg32(priv, &priv->regs->txrqst1);

	if (val & (1 << (objno - 1)))
		return 1;

	return 0;
}

static netdev_tx_t c_can_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	u32 msg_obj_no;
	struct c_can_priv *priv = netdev_priv(dev);
	struct can_frame *frame = (struct can_frame *)skb->data;

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	msg_obj_no = get_tx_next_msg_obj(priv);

	
	c_can_write_msg_object(dev, 0, frame, msg_obj_no);
	can_put_echo_skb(skb, dev, msg_obj_no - C_CAN_MSG_OBJ_TX_FIRST);

	priv->tx_next++;
	if (c_can_is_next_tx_obj_busy(priv, get_tx_next_msg_obj(priv)) ||
			(priv->tx_next & C_CAN_NEXT_MSG_OBJ_MASK) == 0)
		netif_stop_queue(dev);

	return NETDEV_TX_OK;
}

static int c_can_set_bittiming(struct net_device *dev)
{
	unsigned int reg_btr, reg_brpe, ctrl_save;
	u8 brp, brpe, sjw, tseg1, tseg2;
	u32 ten_bit_brp;
	struct c_can_priv *priv = netdev_priv(dev);
	const struct can_bittiming *bt = &priv->can.bittiming;

	
	ten_bit_brp = bt->brp - 1;
	brp = ten_bit_brp & BTR_BRP_MASK;
	brpe = ten_bit_brp >> 6;

	sjw = bt->sjw - 1;
	tseg1 = bt->prop_seg + bt->phase_seg1 - 1;
	tseg2 = bt->phase_seg2 - 1;
	reg_btr = brp | (sjw << BTR_SJW_SHIFT) | (tseg1 << BTR_TSEG1_SHIFT) |
			(tseg2 << BTR_TSEG2_SHIFT);
	reg_brpe = brpe & BRP_EXT_BRPE_MASK;

	netdev_info(dev,
		"setting BTR=%04x BRPE=%04x\n", reg_btr, reg_brpe);

	ctrl_save = priv->read_reg(priv, &priv->regs->control);
	priv->write_reg(priv, &priv->regs->control,
			ctrl_save | CONTROL_CCE | CONTROL_INIT);
	priv->write_reg(priv, &priv->regs->btr, reg_btr);
	priv->write_reg(priv, &priv->regs->brp_ext, reg_brpe);
	priv->write_reg(priv, &priv->regs->control, ctrl_save);

	return 0;
}

static void c_can_configure_msg_objects(struct net_device *dev)
{
	int i;

	
	for (i = C_CAN_MSG_OBJ_RX_FIRST; i <= C_CAN_NO_OF_OBJECTS; i++)
		c_can_inval_msg_object(dev, 0, i);

	
	for (i = C_CAN_MSG_OBJ_RX_FIRST; i < C_CAN_MSG_OBJ_RX_LAST; i++)
		c_can_setup_receive_object(dev, 0, i, 0, 0,
			(IF_MCONT_RXIE | IF_MCONT_UMASK) & ~IF_MCONT_EOB);

	c_can_setup_receive_object(dev, 0, C_CAN_MSG_OBJ_RX_LAST, 0, 0,
			IF_MCONT_EOB | IF_MCONT_RXIE | IF_MCONT_UMASK);
}

static void c_can_chip_config(struct net_device *dev)
{
	struct c_can_priv *priv = netdev_priv(dev);

	
	priv->write_reg(priv, &priv->regs->control,
			CONTROL_ENABLE_AR);

	if (priv->can.ctrlmode & (CAN_CTRLMODE_LISTENONLY &
					CAN_CTRLMODE_LOOPBACK)) {
		
		priv->write_reg(priv, &priv->regs->control, CONTROL_EIE |
				CONTROL_SIE | CONTROL_IE | CONTROL_TEST);
		priv->write_reg(priv, &priv->regs->test,
				TEST_LBACK | TEST_SILENT);
	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {
		
		priv->write_reg(priv, &priv->regs->control, CONTROL_EIE |
				CONTROL_SIE | CONTROL_IE | CONTROL_TEST);
		priv->write_reg(priv, &priv->regs->test, TEST_LBACK);
	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
		
		priv->write_reg(priv, &priv->regs->control, CONTROL_EIE |
				CONTROL_SIE | CONTROL_IE | CONTROL_TEST);
		priv->write_reg(priv, &priv->regs->test, TEST_SILENT);
	} else
		
		priv->write_reg(priv, &priv->regs->control,
				CONTROL_EIE | CONTROL_SIE | CONTROL_IE);

	
	c_can_configure_msg_objects(dev);

	
	priv->write_reg(priv, &priv->regs->status, LEC_UNUSED);

	
	c_can_set_bittiming(dev);
}

static void c_can_start(struct net_device *dev)
{
	struct c_can_priv *priv = netdev_priv(dev);

	
	c_can_chip_config(dev);

	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	
	priv->tx_next = priv->tx_echo = 0;

	
	c_can_enable_all_interrupts(priv, ENABLE_ALL_INTERRUPTS);
}

static void c_can_stop(struct net_device *dev)
{
	struct c_can_priv *priv = netdev_priv(dev);

	
	c_can_enable_all_interrupts(priv, DISABLE_ALL_INTERRUPTS);

	
	priv->can.state = CAN_STATE_STOPPED;
}

static int c_can_set_mode(struct net_device *dev, enum can_mode mode)
{
	switch (mode) {
	case CAN_MODE_START:
		c_can_start(dev);
		netif_wake_queue(dev);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int c_can_get_berr_counter(const struct net_device *dev,
					struct can_berr_counter *bec)
{
	unsigned int reg_err_counter;
	struct c_can_priv *priv = netdev_priv(dev);

	reg_err_counter = priv->read_reg(priv, &priv->regs->err_cnt);
	bec->rxerr = (reg_err_counter & ERR_CNT_REC_MASK) >>
				ERR_CNT_REC_SHIFT;
	bec->txerr = reg_err_counter & ERR_CNT_TEC_MASK;

	return 0;
}

static void c_can_do_tx(struct net_device *dev)
{
	u32 val;
	u32 msg_obj_no;
	struct c_can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;

	for (; (priv->tx_next - priv->tx_echo) > 0; priv->tx_echo++) {
		msg_obj_no = get_tx_echo_msg_obj(priv);
		val = c_can_read_reg32(priv, &priv->regs->txrqst1);
		if (!(val & (1 << msg_obj_no))) {
			can_get_echo_skb(dev,
					msg_obj_no - C_CAN_MSG_OBJ_TX_FIRST);
			stats->tx_bytes += priv->read_reg(priv,
					&priv->regs->ifregs[0].msg_cntrl)
					& IF_MCONT_DLC_MASK;
			stats->tx_packets++;
			c_can_inval_msg_object(dev, 0, msg_obj_no);
		}
	}

	
	if (((priv->tx_next & C_CAN_NEXT_MSG_OBJ_MASK) != 0) ||
			((priv->tx_echo & C_CAN_NEXT_MSG_OBJ_MASK) == 0))
		netif_wake_queue(dev);
}

static int c_can_do_rx_poll(struct net_device *dev, int quota)
{
	u32 num_rx_pkts = 0;
	unsigned int msg_obj, msg_ctrl_save;
	struct c_can_priv *priv = netdev_priv(dev);
	u32 val = c_can_read_reg32(priv, &priv->regs->intpnd1);

	for (msg_obj = C_CAN_MSG_OBJ_RX_FIRST;
			msg_obj <= C_CAN_MSG_OBJ_RX_LAST && quota > 0;
			val = c_can_read_reg32(priv, &priv->regs->intpnd1),
			msg_obj++) {
		if (val & (1 << (msg_obj - 1))) {
			c_can_object_get(dev, 0, msg_obj, IF_COMM_ALL &
					~IF_COMM_TXRQST);
			msg_ctrl_save = priv->read_reg(priv,
					&priv->regs->ifregs[0].msg_cntrl);

			if (msg_ctrl_save & IF_MCONT_EOB)
				return num_rx_pkts;

			if (msg_ctrl_save & IF_MCONT_MSGLST) {
				c_can_handle_lost_msg_obj(dev, 0, msg_obj);
				num_rx_pkts++;
				quota--;
				continue;
			}

			if (!(msg_ctrl_save & IF_MCONT_NEWDAT))
				continue;

			
			c_can_read_msg_object(dev, 0, msg_ctrl_save);

			if (msg_obj < C_CAN_MSG_RX_LOW_LAST)
				c_can_mark_rx_msg_obj(dev, 0,
						msg_ctrl_save, msg_obj);
			else if (msg_obj > C_CAN_MSG_RX_LOW_LAST)
				
				c_can_activate_rx_msg_obj(dev, 0,
						msg_ctrl_save, msg_obj);
			else if (msg_obj == C_CAN_MSG_RX_LOW_LAST)
				
				c_can_activate_all_lower_rx_msg_obj(dev,
						0, msg_ctrl_save);

			num_rx_pkts++;
			quota--;
		}
	}

	return num_rx_pkts;
}

static inline int c_can_has_and_handle_berr(struct c_can_priv *priv)
{
	return (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING) &&
		(priv->current_status & LEC_UNUSED);
}

static int c_can_handle_state_change(struct net_device *dev,
				enum c_can_bus_error_types error_type)
{
	unsigned int reg_err_counter;
	unsigned int rx_err_passive;
	struct c_can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	struct can_berr_counter bec;

	
	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;

	c_can_get_berr_counter(dev, &bec);
	reg_err_counter = priv->read_reg(priv, &priv->regs->err_cnt);
	rx_err_passive = (reg_err_counter & ERR_CNT_RP_MASK) >>
				ERR_CNT_RP_SHIFT;

	switch (error_type) {
	case C_CAN_ERROR_WARNING:
		
		priv->can.can_stats.error_warning++;
		priv->can.state = CAN_STATE_ERROR_WARNING;
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = (bec.txerr > bec.rxerr) ?
			CAN_ERR_CRTL_TX_WARNING :
			CAN_ERR_CRTL_RX_WARNING;
		cf->data[6] = bec.txerr;
		cf->data[7] = bec.rxerr;

		break;
	case C_CAN_ERROR_PASSIVE:
		
		priv->can.can_stats.error_passive++;
		priv->can.state = CAN_STATE_ERROR_PASSIVE;
		cf->can_id |= CAN_ERR_CRTL;
		if (rx_err_passive)
			cf->data[1] |= CAN_ERR_CRTL_RX_PASSIVE;
		if (bec.txerr > 127)
			cf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE;

		cf->data[6] = bec.txerr;
		cf->data[7] = bec.rxerr;
		break;
	case C_CAN_BUS_OFF:
		
		priv->can.state = CAN_STATE_BUS_OFF;
		cf->can_id |= CAN_ERR_BUSOFF;
		c_can_enable_all_interrupts(priv, DISABLE_ALL_INTERRUPTS);
		can_bus_off(dev);
		break;
	default:
		break;
	}

	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

	return 1;
}

static int c_can_handle_bus_err(struct net_device *dev,
				enum c_can_lec_type lec_type)
{
	struct c_can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;

	if (lec_type == LEC_UNUSED || lec_type == LEC_NO_ERROR)
		return 0;

	
	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;


	
	priv->can.can_stats.bus_error++;
	stats->rx_errors++;
	cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;
	cf->data[2] |= CAN_ERR_PROT_UNSPEC;

	switch (lec_type) {
	case LEC_STUFF_ERROR:
		netdev_dbg(dev, "stuff error\n");
		cf->data[2] |= CAN_ERR_PROT_STUFF;
		break;
	case LEC_FORM_ERROR:
		netdev_dbg(dev, "form error\n");
		cf->data[2] |= CAN_ERR_PROT_FORM;
		break;
	case LEC_ACK_ERROR:
		netdev_dbg(dev, "ack error\n");
		cf->data[2] |= (CAN_ERR_PROT_LOC_ACK |
				CAN_ERR_PROT_LOC_ACK_DEL);
		break;
	case LEC_BIT1_ERROR:
		netdev_dbg(dev, "bit1 error\n");
		cf->data[2] |= CAN_ERR_PROT_BIT1;
		break;
	case LEC_BIT0_ERROR:
		netdev_dbg(dev, "bit0 error\n");
		cf->data[2] |= CAN_ERR_PROT_BIT0;
		break;
	case LEC_CRC_ERROR:
		netdev_dbg(dev, "CRC error\n");
		cf->data[2] |= (CAN_ERR_PROT_LOC_CRC_SEQ |
				CAN_ERR_PROT_LOC_CRC_DEL);
		break;
	default:
		break;
	}

	
	priv->write_reg(priv, &priv->regs->status, LEC_UNUSED);

	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

	return 1;
}

static int c_can_poll(struct napi_struct *napi, int quota)
{
	u16 irqstatus;
	int lec_type = 0;
	int work_done = 0;
	struct net_device *dev = napi->dev;
	struct c_can_priv *priv = netdev_priv(dev);

	irqstatus = priv->read_reg(priv, &priv->regs->interrupt);
	if (!irqstatus)
		goto end;

	
	if (irqstatus == STATUS_INTERRUPT) {
		priv->current_status = priv->read_reg(priv,
					&priv->regs->status);

		
		if (priv->current_status & STATUS_TXOK)
			priv->write_reg(priv, &priv->regs->status,
					priv->current_status & ~STATUS_TXOK);

		if (priv->current_status & STATUS_RXOK)
			priv->write_reg(priv, &priv->regs->status,
					priv->current_status & ~STATUS_RXOK);

		
		if ((priv->current_status & STATUS_EWARN) &&
				(!(priv->last_status & STATUS_EWARN))) {
			netdev_dbg(dev, "entered error warning state\n");
			work_done += c_can_handle_state_change(dev,
						C_CAN_ERROR_WARNING);
		}
		if ((priv->current_status & STATUS_EPASS) &&
				(!(priv->last_status & STATUS_EPASS))) {
			netdev_dbg(dev, "entered error passive state\n");
			work_done += c_can_handle_state_change(dev,
						C_CAN_ERROR_PASSIVE);
		}
		if ((priv->current_status & STATUS_BOFF) &&
				(!(priv->last_status & STATUS_BOFF))) {
			netdev_dbg(dev, "entered bus off state\n");
			work_done += c_can_handle_state_change(dev,
						C_CAN_BUS_OFF);
		}

		
		if ((!(priv->current_status & STATUS_BOFF)) &&
				(priv->last_status & STATUS_BOFF)) {
			netdev_dbg(dev, "left bus off state\n");
			priv->can.state = CAN_STATE_ERROR_ACTIVE;
		}
		if ((!(priv->current_status & STATUS_EPASS)) &&
				(priv->last_status & STATUS_EPASS)) {
			netdev_dbg(dev, "left error passive state\n");
			priv->can.state = CAN_STATE_ERROR_ACTIVE;
		}

		priv->last_status = priv->current_status;

		
		lec_type = c_can_has_and_handle_berr(priv);
		if (lec_type)
			work_done += c_can_handle_bus_err(dev, lec_type);
	} else if ((irqstatus >= C_CAN_MSG_OBJ_RX_FIRST) &&
			(irqstatus <= C_CAN_MSG_OBJ_RX_LAST)) {
		
		work_done += c_can_do_rx_poll(dev, (quota - work_done));
	} else if ((irqstatus >= C_CAN_MSG_OBJ_TX_FIRST) &&
			(irqstatus <= C_CAN_MSG_OBJ_TX_LAST)) {
		
		c_can_do_tx(dev);
	}

end:
	if (work_done < quota) {
		napi_complete(napi);
		
		c_can_enable_all_interrupts(priv, ENABLE_ALL_INTERRUPTS);
	}

	return work_done;
}

static irqreturn_t c_can_isr(int irq, void *dev_id)
{
	u16 irqstatus;
	struct net_device *dev = (struct net_device *)dev_id;
	struct c_can_priv *priv = netdev_priv(dev);

	irqstatus = priv->read_reg(priv, &priv->regs->interrupt);
	if (!irqstatus)
		return IRQ_NONE;

	
	c_can_enable_all_interrupts(priv, DISABLE_ALL_INTERRUPTS);
	napi_schedule(&priv->napi);

	return IRQ_HANDLED;
}

static int c_can_open(struct net_device *dev)
{
	int err;
	struct c_can_priv *priv = netdev_priv(dev);

	
	err = open_candev(dev);
	if (err) {
		netdev_err(dev, "failed to open can device\n");
		return err;
	}

	
	err = request_irq(dev->irq, &c_can_isr, IRQF_SHARED, dev->name,
				dev);
	if (err < 0) {
		netdev_err(dev, "failed to request interrupt\n");
		goto exit_irq_fail;
	}

	
	c_can_start(dev);

	napi_enable(&priv->napi);
	netif_start_queue(dev);

	return 0;

exit_irq_fail:
	close_candev(dev);
	return err;
}

static int c_can_close(struct net_device *dev)
{
	struct c_can_priv *priv = netdev_priv(dev);

	netif_stop_queue(dev);
	napi_disable(&priv->napi);
	c_can_stop(dev);
	free_irq(dev->irq, dev);
	close_candev(dev);

	return 0;
}

struct net_device *alloc_c_can_dev(void)
{
	struct net_device *dev;
	struct c_can_priv *priv;

	dev = alloc_candev(sizeof(struct c_can_priv), C_CAN_MSG_OBJ_TX_NUM);
	if (!dev)
		return NULL;

	priv = netdev_priv(dev);
	netif_napi_add(dev, &priv->napi, c_can_poll, C_CAN_NAPI_WEIGHT);

	priv->dev = dev;
	priv->can.bittiming_const = &c_can_bittiming_const;
	priv->can.do_set_mode = c_can_set_mode;
	priv->can.do_get_berr_counter = c_can_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
					CAN_CTRLMODE_LISTENONLY |
					CAN_CTRLMODE_BERR_REPORTING;

	return dev;
}
EXPORT_SYMBOL_GPL(alloc_c_can_dev);

void free_c_can_dev(struct net_device *dev)
{
	free_candev(dev);
}
EXPORT_SYMBOL_GPL(free_c_can_dev);

static const struct net_device_ops c_can_netdev_ops = {
	.ndo_open = c_can_open,
	.ndo_stop = c_can_close,
	.ndo_start_xmit = c_can_start_xmit,
};

int register_c_can_dev(struct net_device *dev)
{
	dev->flags |= IFF_ECHO;	
	dev->netdev_ops = &c_can_netdev_ops;

	return register_candev(dev);
}
EXPORT_SYMBOL_GPL(register_c_can_dev);

void unregister_c_can_dev(struct net_device *dev)
{
	struct c_can_priv *priv = netdev_priv(dev);

	
	c_can_enable_all_interrupts(priv, DISABLE_ALL_INTERRUPTS);

	unregister_candev(dev);
}
EXPORT_SYMBOL_GPL(unregister_c_can_dev);

MODULE_AUTHOR("Bhupesh Sharma <bhupesh.sharma@st.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CAN bus driver for Bosch C_CAN controller");
