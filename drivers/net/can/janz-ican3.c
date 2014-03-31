/*
 * Janz MODULbus VMOD-ICAN3 CAN Interface Driver
 *
 * Copyright (c) 2010 Ira W. Snyder <iws@ovro.caltech.edu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/netdevice.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

#include <linux/mfd/janz.h>
#include <asm/io.h>

#define DPM_NUM_PAGES		256
#define DPM_PAGE_SIZE		256
#define DPM_PAGE_ADDR(p)	((p) * DPM_PAGE_SIZE)

#define QUEUE_OLD_CONTROL	0
#define QUEUE_OLD_RB0		1
#define QUEUE_OLD_RB1		2
#define QUEUE_OLD_WB0		3
#define QUEUE_OLD_WB1		4

#define MSYNC_PEER		0x00		
#define MSYNC_LOCL		0x01		
#define TARGET_RUNNING		0x02

#define MSYNC_RB0		0x01
#define MSYNC_RB1		0x02
#define MSYNC_RBLW		0x04
#define MSYNC_RB_MASK		(MSYNC_RB0 | MSYNC_RB1)

#define MSYNC_WB0		0x10
#define MSYNC_WB1		0x20
#define MSYNC_WBLW		0x40
#define MSYNC_WB_MASK		(MSYNC_WB0 | MSYNC_WB1)

#define QUEUE_TOHOST		5
#define QUEUE_FROMHOST_MID	6
#define QUEUE_FROMHOST_HIGH	7
#define QUEUE_FROMHOST_LOW	8

#define DPM_FREE_START		9

#define DESC_VALID		0x80
#define DESC_WRAP		0x40
#define DESC_INTERRUPT		0x20
#define DESC_IVALID		0x10
#define DESC_LEN(len)		(len)

#define MSG_CONNECTI		0x02
#define MSG_DISCONNECT		0x03
#define MSG_IDVERS		0x04
#define MSG_MSGLOST		0x05
#define MSG_NEWHOSTIF		0x08
#define MSG_INQUIRY		0x0a
#define MSG_SETAFILMASK		0x10
#define MSG_INITFDPMQUEUE	0x11
#define MSG_HWCONF		0x12
#define MSG_FMSGLOST		0x15
#define MSG_CEVTIND		0x37
#define MSG_CBTRREQ		0x41
#define MSG_COFFREQ		0x42
#define MSG_CONREQ		0x43
#define MSG_CCONFREQ		0x47

#define INQUIRY_STATUS		0x00
#define INQUIRY_TERMINATION	0x01
#define INQUIRY_EXTENDED	0x04

#define SETAFILMASK_REJECT	0x00
#define SETAFILMASK_FASTIF	0x02

#define HWCONF_TERMINATE_ON	0x01
#define HWCONF_TERMINATE_OFF	0x00

#define CEVTIND_EI		0x01
#define CEVTIND_DOI		0x02
#define CEVTIND_LOST		0x04
#define CEVTIND_FULL		0x08
#define CEVTIND_BEI		0x10

#define CEVTIND_CHIP_SJA1000	0x02

#define ICAN3_BUSERR_QUOTA_MAX	255

#define ICAN3_ECHO	0x10
#define ICAN3_EFF_RTR	0x40
#define ICAN3_SFF_RTR	0x10
#define ICAN3_EFF	0x80

#define ICAN3_CAN_TYPE_MASK	0x0f
#define ICAN3_CAN_TYPE_SFF	0x00
#define ICAN3_CAN_TYPE_EFF	0x01

#define ICAN3_CAN_DLC_MASK	0x0f


#define SR_BS		0x80
#define SR_ES		0x40
#define SR_TS		0x20
#define SR_RS		0x10
#define SR_TCS		0x08
#define SR_TBS		0x04
#define SR_DOS		0x02
#define SR_RBS		0x01

#define SR_CRIT (SR_BS|SR_ES)

#define ECC_SEG		0x1F
#define ECC_DIR		0x20
#define ECC_ERR		6
#define ECC_BIT		0x00
#define ECC_FORM	0x40
#define ECC_STUFF	0x80
#define ECC_MASK	0xc0

#define ICAN3_NEW_BUFFERS	16

#define ICAN3_TX_BUFFERS	512
#define ICAN3_RX_BUFFERS	1024

#define ICAN3_CAN_CLOCK		8000000

#define DRV_NAME "janz-ican3"

struct ican3_dpm_control {
	
	u8 window_address;
	u8 unused1;

	u8 interrupt;
	u8 unused2;

	
	u8 hwreset;
	u8 unused3;

	
	u8 tpuinterrupt;
};

struct ican3_dev {

	
	struct can_priv can;

	
	struct net_device *ndev;
	struct napi_struct napi;

	
	struct device *dev;

	
	unsigned int num;

	
	struct janz_cmodio_onboard_regs __iomem *ctrl;
	struct ican3_dpm_control __iomem *dpmctrl;
	void __iomem *dpm;
	int irq;

	
	struct completion termination_comp;
	bool termination_enabled;

	
	struct completion buserror_comp;
	struct can_berr_counter bec;

	
	unsigned int iftype;

	spinlock_t lock;

	
	unsigned int rx_int;
	unsigned int rx_num;
	unsigned int tx_num;

	
	unsigned int fastrx_start;
	unsigned int fastrx_int;
	unsigned int fastrx_num;
	unsigned int fasttx_start;
	unsigned int fasttx_num;

	
	unsigned int free_page;
};

struct ican3_msg {
	u8 control;
	u8 spec;
	__le16 len;
	u8 data[252];
};

struct ican3_new_desc {
	u8 control;
	u8 pointer;
};

struct ican3_fast_desc {
	u8 control;
	u8 command;
	u8 data[14];
};

static inline void ican3_set_page(struct ican3_dev *mod, unsigned int page)
{
	BUG_ON(page >= DPM_NUM_PAGES);
	iowrite8(page, &mod->dpmctrl->window_address);
}


static int ican3_old_recv_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	unsigned int mbox, mbox_page;
	u8 locl, peer, xord;

	
	ican3_set_page(mod, QUEUE_OLD_CONTROL);
	peer = ioread8(mod->dpm + MSYNC_PEER);
	locl = ioread8(mod->dpm + MSYNC_LOCL);
	xord = locl ^ peer;

	if ((xord & MSYNC_RB_MASK) == 0x00) {
		dev_dbg(mod->dev, "no mbox for reading\n");
		return -ENOMEM;
	}

	
	if ((xord & MSYNC_RB_MASK) == MSYNC_RB_MASK)
		mbox = (xord & MSYNC_RBLW) ? MSYNC_RB0 : MSYNC_RB1;
	else
		mbox = (xord & MSYNC_RB0) ? MSYNC_RB0 : MSYNC_RB1;

	
	mbox_page = (mbox == MSYNC_RB0) ? QUEUE_OLD_RB0 : QUEUE_OLD_RB1;
	ican3_set_page(mod, mbox_page);
	memcpy_fromio(msg, mod->dpm, sizeof(*msg));

	locl ^= mbox;

	ican3_set_page(mod, QUEUE_OLD_CONTROL);
	iowrite8(locl, mod->dpm + MSYNC_LOCL);
	return 0;
}

static int ican3_old_send_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	unsigned int mbox, mbox_page;
	u8 locl, peer, xord;

	
	ican3_set_page(mod, QUEUE_OLD_CONTROL);
	peer = ioread8(mod->dpm + MSYNC_PEER);
	locl = ioread8(mod->dpm + MSYNC_LOCL);
	xord = locl ^ peer;

	if ((xord & MSYNC_WB_MASK) == MSYNC_WB_MASK) {
		dev_err(mod->dev, "no mbox for writing\n");
		return -ENOMEM;
	}

	
	mbox = (xord & MSYNC_WB0) ? MSYNC_WB1 : MSYNC_WB0;

	
	mbox_page = (mbox == MSYNC_WB0) ? QUEUE_OLD_WB0 : QUEUE_OLD_WB1;
	ican3_set_page(mod, mbox_page);
	memcpy_toio(mod->dpm, msg, sizeof(*msg));

	locl ^= mbox;
	if (mbox == MSYNC_WB1)
		locl |= MSYNC_WBLW;

	ican3_set_page(mod, QUEUE_OLD_CONTROL);
	iowrite8(locl, mod->dpm + MSYNC_LOCL);
	return 0;
}


static void __devinit ican3_init_new_host_interface(struct ican3_dev *mod)
{
	struct ican3_new_desc desc;
	unsigned long flags;
	void __iomem *dst;
	int i;

	spin_lock_irqsave(&mod->lock, flags);

	
	mod->rx_num = 0;
	mod->rx_int = 0;

	
	ican3_set_page(mod, QUEUE_TOHOST);
	dst = mod->dpm;

	
	for (i = 0; i < ICAN3_NEW_BUFFERS; i++) {
		desc.control = DESC_INTERRUPT | DESC_LEN(1); 
		desc.pointer = mod->free_page;

		
		if (i == ICAN3_NEW_BUFFERS - 1)
			desc.control |= DESC_WRAP;

		memcpy_toio(dst, &desc, sizeof(desc));
		dst += sizeof(desc);
		mod->free_page++;
	}

	
	ican3_set_page(mod, QUEUE_FROMHOST_MID);
	dst = mod->dpm;

	
	mod->tx_num = 0;

	
	for (i = 0; i < ICAN3_NEW_BUFFERS; i++) {
		desc.control = DESC_VALID | DESC_LEN(1); 
		desc.pointer = mod->free_page;

		
		if (i == ICAN3_NEW_BUFFERS - 1)
			desc.control |= DESC_WRAP;

		memcpy_toio(dst, &desc, sizeof(desc));
		dst += sizeof(desc);
		mod->free_page++;
	}

	
	ican3_set_page(mod, QUEUE_FROMHOST_HIGH);
	dst = mod->dpm;

	
	desc.control = DESC_VALID | DESC_WRAP | DESC_LEN(1); 
	desc.pointer = mod->free_page;
	memcpy_toio(dst, &desc, sizeof(desc));
	mod->free_page++;

	
	ican3_set_page(mod, QUEUE_FROMHOST_LOW);
	dst = mod->dpm;

	
	desc.control = DESC_VALID | DESC_WRAP | DESC_LEN(1); 
	desc.pointer = mod->free_page;
	memcpy_toio(dst, &desc, sizeof(desc));
	mod->free_page++;

	spin_unlock_irqrestore(&mod->lock, flags);
}


static void __devinit ican3_init_fast_host_interface(struct ican3_dev *mod)
{
	struct ican3_fast_desc desc;
	unsigned long flags;
	unsigned int addr;
	void __iomem *dst;
	int i;

	spin_lock_irqsave(&mod->lock, flags);

	
	mod->fastrx_start = mod->free_page;
	mod->fastrx_num = 0;
	mod->fastrx_int = 0;

	
	memset(&desc, 0, sizeof(desc));
	desc.control = 0x00;
	desc.command = 1;

	
	addr = 0;
	for (i = 0; i < ICAN3_RX_BUFFERS; i++) {

		
		if (i == ICAN3_RX_BUFFERS - 1)
			desc.control |= DESC_WRAP;

		
		ican3_set_page(mod, mod->free_page);

		
		dst = mod->dpm + addr;
		memcpy_toio(dst, &desc, sizeof(desc));
		addr += sizeof(desc);

		
		if (addr >= DPM_PAGE_SIZE) {
			addr = 0;
			mod->free_page++;
		}
	}

	
	if (addr != 0)
		mod->free_page++;

	
	mod->fasttx_start = mod->free_page;
	mod->fasttx_num = 0;

	
	memset(&desc, 0, sizeof(desc));
	desc.control = DESC_VALID;
	desc.command = 1;

	
	addr = 0;
	for (i = 0; i < ICAN3_TX_BUFFERS; i++) {

		
		if (i == ICAN3_TX_BUFFERS - 1)
			desc.control |= DESC_WRAP;

		
		ican3_set_page(mod, mod->free_page);

		
		dst = mod->dpm + addr;
		memcpy_toio(dst, &desc, sizeof(desc));
		addr += sizeof(desc);

		
		if (addr >= DPM_PAGE_SIZE) {
			addr = 0;
			mod->free_page++;
		}
	}

	spin_unlock_irqrestore(&mod->lock, flags);
}


static int ican3_new_send_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	struct ican3_new_desc desc;
	void __iomem *desc_addr = mod->dpm + (mod->tx_num * sizeof(desc));

	
	ican3_set_page(mod, QUEUE_FROMHOST_MID);
	memcpy_fromio(&desc, desc_addr, sizeof(desc));

	if (!(desc.control & DESC_VALID)) {
		dev_dbg(mod->dev, "%s: no free buffers\n", __func__);
		return -ENOMEM;
	}

	
	ican3_set_page(mod, desc.pointer);
	memcpy_toio(mod->dpm, msg, sizeof(*msg));

	
	ican3_set_page(mod, QUEUE_FROMHOST_MID);
	desc.control ^= DESC_VALID;
	memcpy_toio(desc_addr, &desc, sizeof(desc));

	
	mod->tx_num = (desc.control & DESC_WRAP) ? 0 : (mod->tx_num + 1);
	return 0;
}

static int ican3_new_recv_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	struct ican3_new_desc desc;
	void __iomem *desc_addr = mod->dpm + (mod->rx_num * sizeof(desc));

	
	ican3_set_page(mod, QUEUE_TOHOST);
	memcpy_fromio(&desc, desc_addr, sizeof(desc));

	if (!(desc.control & DESC_VALID)) {
		dev_dbg(mod->dev, "%s: no buffers to recv\n", __func__);
		return -ENOMEM;
	}

	
	ican3_set_page(mod, desc.pointer);
	memcpy_fromio(msg, mod->dpm, sizeof(*msg));

	
	ican3_set_page(mod, QUEUE_TOHOST);
	desc.control ^= DESC_VALID;
	memcpy_toio(desc_addr, &desc, sizeof(desc));

	
	mod->rx_num = (desc.control & DESC_WRAP) ? 0 : (mod->rx_num + 1);
	return 0;
}


static int ican3_send_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mod->lock, flags);

	if (mod->iftype == 0)
		ret = ican3_old_send_msg(mod, msg);
	else
		ret = ican3_new_send_msg(mod, msg);

	spin_unlock_irqrestore(&mod->lock, flags);
	return ret;
}

static int ican3_recv_msg(struct ican3_dev *mod, struct ican3_msg *msg)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mod->lock, flags);

	if (mod->iftype == 0)
		ret = ican3_old_recv_msg(mod, msg);
	else
		ret = ican3_new_recv_msg(mod, msg);

	spin_unlock_irqrestore(&mod->lock, flags);
	return ret;
}


static int __devinit ican3_msg_connect(struct ican3_dev *mod)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_CONNECTI;
	msg.len = cpu_to_le16(0);

	return ican3_send_msg(mod, &msg);
}

static int __devexit ican3_msg_disconnect(struct ican3_dev *mod)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_DISCONNECT;
	msg.len = cpu_to_le16(0);

	return ican3_send_msg(mod, &msg);
}

static int __devinit ican3_msg_newhostif(struct ican3_dev *mod)
{
	struct ican3_msg msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_NEWHOSTIF;
	msg.len = cpu_to_le16(0);

	
	WARN_ON(mod->iftype != 0);

	ret = ican3_send_msg(mod, &msg);
	if (ret)
		return ret;

	
	mod->iftype = 1;
	return 0;
}

static int __devinit ican3_msg_fasthostif(struct ican3_dev *mod)
{
	struct ican3_msg msg;
	unsigned int addr;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_INITFDPMQUEUE;
	msg.len = cpu_to_le16(8);

	
	addr = DPM_PAGE_ADDR(mod->fastrx_start);
	msg.data[0] = addr & 0xff;
	msg.data[1] = (addr >> 8) & 0xff;
	msg.data[2] = (addr >> 16) & 0xff;
	msg.data[3] = (addr >> 24) & 0xff;

	
	addr = DPM_PAGE_ADDR(mod->fasttx_start);
	msg.data[4] = addr & 0xff;
	msg.data[5] = (addr >> 8) & 0xff;
	msg.data[6] = (addr >> 16) & 0xff;
	msg.data[7] = (addr >> 24) & 0xff;

	
	WARN_ON(mod->iftype != 1);

	return ican3_send_msg(mod, &msg);
}

static int __devinit ican3_set_id_filter(struct ican3_dev *mod, bool accept)
{
	struct ican3_msg msg;
	int ret;

	
	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_SETAFILMASK;
	msg.len = cpu_to_le16(5);
	msg.data[0] = 0x00; 
	msg.data[1] = 0x00; 
	msg.data[2] = 0xff; 
	msg.data[3] = 0x07; 

	
	msg.data[4] = accept ? SETAFILMASK_FASTIF : SETAFILMASK_REJECT;

	ret = ican3_send_msg(mod, &msg);
	if (ret)
		return ret;

	
	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_SETAFILMASK;
	msg.len = cpu_to_le16(13);
	msg.data[0] = 0;    
	msg.data[1] = 0x00; 
	msg.data[2] = 0x00;
	msg.data[3] = 0x00;
	msg.data[4] = 0x20; 
	msg.data[5] = 0xff; 
	msg.data[6] = 0xff;
	msg.data[7] = 0xff;
	msg.data[8] = 0x3f; 

	
	msg.data[9] = accept ? SETAFILMASK_FASTIF : SETAFILMASK_REJECT;

	return ican3_send_msg(mod, &msg);
}

static int ican3_set_bus_state(struct ican3_dev *mod, bool on)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = on ? MSG_CONREQ : MSG_COFFREQ;
	msg.len = cpu_to_le16(0);

	return ican3_send_msg(mod, &msg);
}

static int ican3_set_termination(struct ican3_dev *mod, bool on)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_HWCONF;
	msg.len = cpu_to_le16(2);
	msg.data[0] = 0x00;
	msg.data[1] = on ? HWCONF_TERMINATE_ON : HWCONF_TERMINATE_OFF;

	return ican3_send_msg(mod, &msg);
}

static int ican3_send_inquiry(struct ican3_dev *mod, u8 subspec)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_INQUIRY;
	msg.len = cpu_to_le16(2);
	msg.data[0] = subspec;
	msg.data[1] = 0x00;

	return ican3_send_msg(mod, &msg);
}

static int ican3_set_buserror(struct ican3_dev *mod, u8 quota)
{
	struct ican3_msg msg;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_CCONFREQ;
	msg.len = cpu_to_le16(2);
	msg.data[0] = 0x00;
	msg.data[1] = quota;

	return ican3_send_msg(mod, &msg);
}


static void ican3_to_can_frame(struct ican3_dev *mod,
			       struct ican3_fast_desc *desc,
			       struct can_frame *cf)
{
	if ((desc->command & ICAN3_CAN_TYPE_MASK) == ICAN3_CAN_TYPE_SFF) {
		if (desc->data[1] & ICAN3_SFF_RTR)
			cf->can_id |= CAN_RTR_FLAG;

		cf->can_id |= desc->data[0] << 3;
		cf->can_id |= (desc->data[1] & 0xe0) >> 5;
		cf->can_dlc = desc->data[1] & ICAN3_CAN_DLC_MASK;
		memcpy(cf->data, &desc->data[2], sizeof(cf->data));
	} else {
		cf->can_dlc = desc->data[0] & ICAN3_CAN_DLC_MASK;
		if (desc->data[0] & ICAN3_EFF_RTR)
			cf->can_id |= CAN_RTR_FLAG;

		if (desc->data[0] & ICAN3_EFF) {
			cf->can_id |= CAN_EFF_FLAG;
			cf->can_id |= desc->data[2] << 21; 
			cf->can_id |= desc->data[3] << 13; 
			cf->can_id |= desc->data[4] << 5;  
			cf->can_id |= (desc->data[5] & 0xf8) >> 3;
		} else {
			cf->can_id |= desc->data[2] << 3;  
			cf->can_id |= desc->data[3] >> 5;  
		}

		memcpy(cf->data, &desc->data[6], sizeof(cf->data));
	}
}

static void can_frame_to_ican3(struct ican3_dev *mod,
			       struct can_frame *cf,
			       struct ican3_fast_desc *desc)
{
	
	memset(desc->data, 0, sizeof(desc->data));

	
	desc->command = ICAN3_CAN_TYPE_EFF;
	desc->data[0] |= cf->can_dlc;
	desc->data[1] |= ICAN3_ECHO;

	if (cf->can_id & CAN_RTR_FLAG)
		desc->data[0] |= ICAN3_EFF_RTR;

	
	if (cf->can_id & CAN_EFF_FLAG) {
		desc->data[0] |= ICAN3_EFF;
		desc->data[2] = (cf->can_id & 0x1fe00000) >> 21; 
		desc->data[3] = (cf->can_id & 0x001fe000) >> 13; 
		desc->data[4] = (cf->can_id & 0x00001fe0) >> 5;  
		desc->data[5] = (cf->can_id & 0x0000001f) << 3;  
	} else {
		desc->data[2] = (cf->can_id & 0x7F8) >> 3; 
		desc->data[3] = (cf->can_id & 0x007) << 5; 
	}

	
	memcpy(&desc->data[6], cf->data, sizeof(cf->data));
}


static void ican3_handle_idvers(struct ican3_dev *mod, struct ican3_msg *msg)
{
	dev_dbg(mod->dev, "IDVERS response: %s\n", msg->data);
}

static void ican3_handle_msglost(struct ican3_dev *mod, struct ican3_msg *msg)
{
	struct net_device *dev = mod->ndev;
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;

	if (msg->spec == MSG_MSGLOST) {
		dev_err(mod->dev, "lost %d control messages\n", msg->data[0]);
		return;
	}

	skb = alloc_can_err_skb(dev, &cf);
	if (skb) {
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
		stats->rx_errors++;
		stats->rx_bytes += cf->can_dlc;
		netif_rx(skb);
	}
}

static int ican3_handle_cevtind(struct ican3_dev *mod, struct ican3_msg *msg)
{
	struct net_device *dev = mod->ndev;
	struct net_device_stats *stats = &dev->stats;
	enum can_state state = mod->can.state;
	u8 status, isrc, rxerr, txerr;
	struct can_frame *cf;
	struct sk_buff *skb;

	
	if (msg->data[1] != CEVTIND_CHIP_SJA1000) {
		dev_err(mod->dev, "unable to handle errors on non-SJA1000\n");
		return -ENODEV;
	}

	
	if (le16_to_cpu(msg->len) < 6) {
		dev_err(mod->dev, "error message too short\n");
		return -EINVAL;
	}

	skb = alloc_can_err_skb(dev, &cf);
	if (skb == NULL)
		return -ENOMEM;

	isrc = msg->data[0];
	status = msg->data[3];
	rxerr = msg->data[4];
	txerr = msg->data[5];

	
	if (isrc == CEVTIND_DOI || isrc == CEVTIND_LOST) {
		dev_dbg(mod->dev, "data overrun interrupt\n");
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
		stats->rx_over_errors++;
		stats->rx_errors++;
	}

	
	if (isrc == CEVTIND_EI) {
		dev_dbg(mod->dev, "error warning + passive interrupt\n");
		if (status & SR_BS) {
			state = CAN_STATE_BUS_OFF;
			cf->can_id |= CAN_ERR_BUSOFF;
			can_bus_off(dev);
		} else if (status & SR_ES) {
			if (rxerr >= 128 || txerr >= 128)
				state = CAN_STATE_ERROR_PASSIVE;
			else
				state = CAN_STATE_ERROR_WARNING;
		} else {
			state = CAN_STATE_ERROR_ACTIVE;
		}
	}

	
	if (isrc == CEVTIND_BEI) {
		u8 ecc = msg->data[2];

		dev_dbg(mod->dev, "bus error interrupt\n");
		mod->can.can_stats.bus_error++;
		stats->rx_errors++;
		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

		switch (ecc & ECC_MASK) {
		case ECC_BIT:
			cf->data[2] |= CAN_ERR_PROT_BIT;
			break;
		case ECC_FORM:
			cf->data[2] |= CAN_ERR_PROT_FORM;
			break;
		case ECC_STUFF:
			cf->data[2] |= CAN_ERR_PROT_STUFF;
			break;
		default:
			cf->data[2] |= CAN_ERR_PROT_UNSPEC;
			cf->data[3] = ecc & ECC_SEG;
			break;
		}

		if ((ecc & ECC_DIR) == 0)
			cf->data[2] |= CAN_ERR_PROT_TX;

		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	if (state != mod->can.state && (state == CAN_STATE_ERROR_WARNING ||
					state == CAN_STATE_ERROR_PASSIVE)) {
		cf->can_id |= CAN_ERR_CRTL;
		if (state == CAN_STATE_ERROR_WARNING) {
			mod->can.can_stats.error_warning++;
			cf->data[1] = (txerr > rxerr) ?
				CAN_ERR_CRTL_TX_WARNING :
				CAN_ERR_CRTL_RX_WARNING;
		} else {
			mod->can.can_stats.error_passive++;
			cf->data[1] = (txerr > rxerr) ?
				CAN_ERR_CRTL_TX_PASSIVE :
				CAN_ERR_CRTL_RX_PASSIVE;
		}

		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	mod->can.state = state;
	stats->rx_errors++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);
	return 0;
}

static void ican3_handle_inquiry(struct ican3_dev *mod, struct ican3_msg *msg)
{
	switch (msg->data[0]) {
	case INQUIRY_STATUS:
	case INQUIRY_EXTENDED:
		mod->bec.rxerr = msg->data[5];
		mod->bec.txerr = msg->data[6];
		complete(&mod->buserror_comp);
		break;
	case INQUIRY_TERMINATION:
		mod->termination_enabled = msg->data[6] & HWCONF_TERMINATE_ON;
		complete(&mod->termination_comp);
		break;
	default:
		dev_err(mod->dev, "received an unknown inquiry response\n");
		break;
	}
}

static void ican3_handle_unknown_message(struct ican3_dev *mod,
					struct ican3_msg *msg)
{
	dev_warn(mod->dev, "received unknown message: spec 0x%.2x length %d\n",
			   msg->spec, le16_to_cpu(msg->len));
}

static void ican3_handle_message(struct ican3_dev *mod, struct ican3_msg *msg)
{
	dev_dbg(mod->dev, "%s: modno %d spec 0x%.2x len %d bytes\n", __func__,
			   mod->num, msg->spec, le16_to_cpu(msg->len));

	switch (msg->spec) {
	case MSG_IDVERS:
		ican3_handle_idvers(mod, msg);
		break;
	case MSG_MSGLOST:
	case MSG_FMSGLOST:
		ican3_handle_msglost(mod, msg);
		break;
	case MSG_CEVTIND:
		ican3_handle_cevtind(mod, msg);
		break;
	case MSG_INQUIRY:
		ican3_handle_inquiry(mod, msg);
		break;
	default:
		ican3_handle_unknown_message(mod, msg);
		break;
	}
}

static bool ican3_txok(struct ican3_dev *mod)
{
	struct ican3_fast_desc __iomem *desc;
	u8 control;

	
	ican3_set_page(mod, mod->fasttx_start + (mod->fasttx_num / 16));
	desc = mod->dpm + ((mod->fasttx_num % 16) * sizeof(*desc));
	control = ioread8(&desc->control);

	
	if (!(control & DESC_VALID))
		return false;

	return true;
}

static int ican3_recv_skb(struct ican3_dev *mod)
{
	struct net_device *ndev = mod->ndev;
	struct net_device_stats *stats = &ndev->stats;
	struct ican3_fast_desc desc;
	void __iomem *desc_addr;
	struct can_frame *cf;
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&mod->lock, flags);

	
	ican3_set_page(mod, mod->fastrx_start + (mod->fastrx_num / 16));
	desc_addr = mod->dpm + ((mod->fastrx_num % 16) * sizeof(desc));
	memcpy_fromio(&desc, desc_addr, sizeof(desc));

	spin_unlock_irqrestore(&mod->lock, flags);

	
	if (!(desc.control & DESC_VALID))
		return -ENOBUFS;

	
	skb = alloc_can_skb(ndev, &cf);
	if (unlikely(skb == NULL)) {
		stats->rx_dropped++;
		goto err_noalloc;
	}

	
	ican3_to_can_frame(mod, &desc, cf);

	
	netif_receive_skb(skb);
	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

err_noalloc:
	
	desc.control ^= DESC_VALID;

	spin_lock_irqsave(&mod->lock, flags);

	ican3_set_page(mod, mod->fastrx_start + (mod->fastrx_num / 16));
	memcpy_toio(desc_addr, &desc, 1);

	
	mod->fastrx_num = (desc.control & DESC_WRAP) ? 0
						     : (mod->fastrx_num + 1);

	
	spin_unlock_irqrestore(&mod->lock, flags);
	return 0;
}

static int ican3_napi(struct napi_struct *napi, int budget)
{
	struct ican3_dev *mod = container_of(napi, struct ican3_dev, napi);
	struct ican3_msg msg;
	unsigned long flags;
	int received = 0;
	int ret;

	
	while (true) {
		ret = ican3_recv_msg(mod, &msg);
		if (ret)
			break;

		ican3_handle_message(mod, &msg);
	}

	
	while (received < budget) {
		ret = ican3_recv_skb(mod);
		if (ret)
			break;

		received++;
	}

	if (received < budget)
		napi_complete(napi);

	spin_lock_irqsave(&mod->lock, flags);

	
	if (netif_queue_stopped(mod->ndev) && ican3_txok(mod))
		netif_wake_queue(mod->ndev);

	spin_unlock_irqrestore(&mod->lock, flags);

	
	iowrite8(1 << mod->num, &mod->ctrl->int_enable);
	return received;
}

static irqreturn_t ican3_irq(int irq, void *dev_id)
{
	struct ican3_dev *mod = dev_id;
	u8 stat;

	stat = ioread8(&mod->ctrl->int_disable) & (1 << mod->num);
	if (stat == (1 << mod->num))
		return IRQ_NONE;

	
	ioread8(&mod->dpmctrl->interrupt);

	
	iowrite8(1 << mod->num, &mod->ctrl->int_disable);
	napi_schedule(&mod->napi);
	return IRQ_HANDLED;
}


static int ican3_reset_module(struct ican3_dev *mod)
{
	u8 val = 1 << mod->num;
	unsigned long start;
	u8 runold, runnew;

	
	iowrite8(1 << mod->num, &mod->ctrl->int_disable);

	
	mod->free_page = DPM_FREE_START;

	ican3_set_page(mod, QUEUE_OLD_CONTROL);
	runold = ioread8(mod->dpm + TARGET_RUNNING);

	
	iowrite8(val, &mod->ctrl->reset_assert);
	iowrite8(val, &mod->ctrl->reset_deassert);

	
	start = jiffies;
	do {
		ican3_set_page(mod, QUEUE_OLD_CONTROL);
		runnew = ioread8(mod->dpm + TARGET_RUNNING);
		if (runnew == (runold ^ 0xff))
			return 0;

		msleep(10);
	} while (time_before(jiffies, start + HZ / 4));

	dev_err(mod->dev, "failed to reset CAN module\n");
	return -ETIMEDOUT;
}

static void __devexit ican3_shutdown_module(struct ican3_dev *mod)
{
	ican3_msg_disconnect(mod);
	ican3_reset_module(mod);
}

static int __devinit ican3_startup_module(struct ican3_dev *mod)
{
	int ret;

	ret = ican3_reset_module(mod);
	if (ret) {
		dev_err(mod->dev, "unable to reset module\n");
		return ret;
	}

	
	iowrite8(1 << mod->num, &mod->ctrl->int_enable);

	ret = ican3_msg_connect(mod);
	if (ret) {
		dev_err(mod->dev, "unable to connect to module\n");
		return ret;
	}

	ican3_init_new_host_interface(mod);
	ret = ican3_msg_newhostif(mod);
	if (ret) {
		dev_err(mod->dev, "unable to switch to new-style interface\n");
		return ret;
	}

	
	ret = ican3_set_termination(mod, true);
	if (ret) {
		dev_err(mod->dev, "unable to enable termination\n");
		return ret;
	}

	
	ret = ican3_set_buserror(mod, ICAN3_BUSERR_QUOTA_MAX);
	if (ret) {
		dev_err(mod->dev, "unable to set bus-error\n");
		return ret;
	}

	ican3_init_fast_host_interface(mod);
	ret = ican3_msg_fasthostif(mod);
	if (ret) {
		dev_err(mod->dev, "unable to switch to fast host interface\n");
		return ret;
	}

	ret = ican3_set_id_filter(mod, true);
	if (ret) {
		dev_err(mod->dev, "unable to set acceptance filter\n");
		return ret;
	}

	return 0;
}


static int ican3_open(struct net_device *ndev)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	u8 quota;
	int ret;

	
	ret = open_candev(ndev);
	if (ret) {
		dev_err(mod->dev, "unable to start CAN layer\n");
		return ret;
	}

	
	if (mod->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING)
		quota = ICAN3_BUSERR_QUOTA_MAX;
	else
		quota = 0;

	ret = ican3_set_buserror(mod, quota);
	if (ret) {
		dev_err(mod->dev, "unable to set bus-error\n");
		close_candev(ndev);
		return ret;
	}

	
	ret = ican3_set_bus_state(mod, true);
	if (ret) {
		dev_err(mod->dev, "unable to set bus-on\n");
		close_candev(ndev);
		return ret;
	}

	
	mod->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_start_queue(ndev);

	return 0;
}

static int ican3_stop(struct net_device *ndev)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	int ret;

	
	netif_stop_queue(ndev);
	mod->can.state = CAN_STATE_STOPPED;

	
	ret = ican3_set_bus_state(mod, false);
	if (ret) {
		dev_err(mod->dev, "unable to set bus-off\n");
		return ret;
	}

	
	close_candev(ndev);
	return 0;
}

static int ican3_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf = (struct can_frame *)skb->data;
	struct ican3_fast_desc desc;
	void __iomem *desc_addr;
	unsigned long flags;

	spin_lock_irqsave(&mod->lock, flags);

	
	if (!ican3_txok(mod)) {
		dev_err(mod->dev, "no free descriptors, stopping queue\n");
		netif_stop_queue(ndev);
		spin_unlock_irqrestore(&mod->lock, flags);
		return NETDEV_TX_BUSY;
	}

	
	ican3_set_page(mod, mod->fasttx_start + (mod->fasttx_num / 16));
	desc_addr = mod->dpm + ((mod->fasttx_num % 16) * sizeof(desc));
	memset(&desc, 0, sizeof(desc));
	memcpy_fromio(&desc, desc_addr, 1);

	
	can_frame_to_ican3(mod, cf, &desc);

	desc.control |= DESC_IVALID;
	memcpy_toio(desc_addr, &desc, sizeof(desc));

	
	iowrite8(0x01, &mod->dpmctrl->interrupt);

	desc.control ^= DESC_VALID;
	memcpy_toio(desc_addr, &desc, sizeof(desc));

	
	mod->fasttx_num = (desc.control & DESC_WRAP) ? 0
						     : (mod->fasttx_num + 1);

	
	stats->tx_packets++;
	stats->tx_bytes += cf->can_dlc;
	kfree_skb(skb);


	
	if (!ican3_txok(mod))
		netif_stop_queue(ndev);

	spin_unlock_irqrestore(&mod->lock, flags);
	return NETDEV_TX_OK;
}

static const struct net_device_ops ican3_netdev_ops = {
	.ndo_open	= ican3_open,
	.ndo_stop	= ican3_stop,
	.ndo_start_xmit	= ican3_xmit,
};


static struct can_bittiming_const ican3_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

static int ican3_set_bittiming(struct net_device *ndev)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	struct can_bittiming *bt = &mod->can.bittiming;
	struct ican3_msg msg;
	u8 btr0, btr1;

	btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
	btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) |
		(((bt->phase_seg2 - 1) & 0x7) << 4);
	if (mod->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		btr1 |= 0x80;

	memset(&msg, 0, sizeof(msg));
	msg.spec = MSG_CBTRREQ;
	msg.len = cpu_to_le16(4);
	msg.data[0] = 0x00;
	msg.data[1] = 0x00;
	msg.data[2] = btr0;
	msg.data[3] = btr1;

	return ican3_send_msg(mod, &msg);
}

static int ican3_set_mode(struct net_device *ndev, enum can_mode mode)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	int ret;

	if (mode != CAN_MODE_START)
		return -ENOTSUPP;

	
	ret = ican3_set_bus_state(mod, true);
	if (ret) {
		dev_err(mod->dev, "unable to set bus-on\n");
		return ret;
	}

	
	mod->can.state = CAN_STATE_ERROR_ACTIVE;

	if (netif_queue_stopped(ndev))
		netif_wake_queue(ndev);

	return 0;
}

static int ican3_get_berr_counter(const struct net_device *ndev,
				  struct can_berr_counter *bec)
{
	struct ican3_dev *mod = netdev_priv(ndev);
	int ret;

	ret = ican3_send_inquiry(mod, INQUIRY_STATUS);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&mod->buserror_comp, HZ);
	if (ret <= 0) {
		dev_info(mod->dev, "%s timed out\n", __func__);
		return -ETIMEDOUT;
	}

	bec->rxerr = mod->bec.rxerr;
	bec->txerr = mod->bec.txerr;
	return 0;
}


static ssize_t ican3_sysfs_show_term(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct ican3_dev *mod = netdev_priv(to_net_dev(dev));
	int ret;

	ret = ican3_send_inquiry(mod, INQUIRY_TERMINATION);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&mod->termination_comp, HZ);
	if (ret <= 0) {
		dev_info(mod->dev, "%s timed out\n", __func__);
		return -ETIMEDOUT;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", mod->termination_enabled);
}

static ssize_t ican3_sysfs_set_term(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct ican3_dev *mod = netdev_priv(to_net_dev(dev));
	unsigned long enable;
	int ret;

	if (strict_strtoul(buf, 0, &enable))
		return -EINVAL;

	ret = ican3_set_termination(mod, enable);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(termination, S_IWUSR | S_IRUGO, ican3_sysfs_show_term,
						   ican3_sysfs_set_term);

static struct attribute *ican3_sysfs_attrs[] = {
	&dev_attr_termination.attr,
	NULL,
};

static struct attribute_group ican3_sysfs_attr_group = {
	.attrs = ican3_sysfs_attrs,
};


static int __devinit ican3_probe(struct platform_device *pdev)
{
	struct janz_platform_data *pdata;
	struct net_device *ndev;
	struct ican3_dev *mod;
	struct resource *res;
	struct device *dev;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -ENXIO;

	dev_dbg(&pdev->dev, "probe: module number %d\n", pdata->modno);

	
	dev = &pdev->dev;

	
	ndev = alloc_candev(sizeof(*mod), 0);
	if (!ndev) {
		dev_err(dev, "unable to allocate CANdev\n");
		ret = -ENOMEM;
		goto out_return;
	}

	platform_set_drvdata(pdev, ndev);
	mod = netdev_priv(ndev);
	mod->ndev = ndev;
	mod->dev = &pdev->dev;
	mod->num = pdata->modno;
	netif_napi_add(ndev, &mod->napi, ican3_napi, ICAN3_RX_BUFFERS);
	spin_lock_init(&mod->lock);
	init_completion(&mod->termination_comp);
	init_completion(&mod->buserror_comp);

	
	ndev->sysfs_groups[0] = &ican3_sysfs_attr_group;

	
	mod->free_page = DPM_FREE_START;

	ndev->netdev_ops = &ican3_netdev_ops;
	ndev->flags |= IFF_ECHO;
	SET_NETDEV_DEV(ndev, &pdev->dev);

	mod->can.clock.freq = ICAN3_CAN_CLOCK;
	mod->can.bittiming_const = &ican3_bittiming_const;
	mod->can.do_set_bittiming = ican3_set_bittiming;
	mod->can.do_set_mode = ican3_set_mode;
	mod->can.do_get_berr_counter = ican3_get_berr_counter;
	mod->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES
				    | CAN_CTRLMODE_BERR_REPORTING;

	
	mod->irq = platform_get_irq(pdev, 0);
	if (mod->irq < 0) {
		dev_err(dev, "IRQ line not found\n");
		ret = -ENODEV;
		goto out_free_ndev;
	}

	ndev->irq = mod->irq;

	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "MODULbus registers not found\n");
		ret = -ENODEV;
		goto out_free_ndev;
	}

	mod->dpm = ioremap(res->start, resource_size(res));
	if (!mod->dpm) {
		dev_err(dev, "MODULbus registers not ioremap\n");
		ret = -ENOMEM;
		goto out_free_ndev;
	}

	mod->dpmctrl = mod->dpm + DPM_PAGE_SIZE;

	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "CONTROL registers not found\n");
		ret = -ENODEV;
		goto out_iounmap_dpm;
	}

	mod->ctrl = ioremap(res->start, resource_size(res));
	if (!mod->ctrl) {
		dev_err(dev, "CONTROL registers not ioremap\n");
		ret = -ENOMEM;
		goto out_iounmap_dpm;
	}

	
	iowrite8(1 << mod->num, &mod->ctrl->int_disable);
	ret = request_irq(mod->irq, ican3_irq, IRQF_SHARED, DRV_NAME, mod);
	if (ret) {
		dev_err(dev, "unable to request IRQ\n");
		goto out_iounmap_ctrl;
	}

	
	napi_enable(&mod->napi);
	ret = ican3_startup_module(mod);
	if (ret) {
		dev_err(dev, "%s: unable to start CANdev\n", __func__);
		goto out_free_irq;
	}

	
	ret = register_candev(ndev);
	if (ret) {
		dev_err(dev, "%s: unable to register CANdev\n", __func__);
		goto out_free_irq;
	}

	dev_info(dev, "module %d: registered CAN device\n", pdata->modno);
	return 0;

out_free_irq:
	napi_disable(&mod->napi);
	iowrite8(1 << mod->num, &mod->ctrl->int_disable);
	free_irq(mod->irq, mod);
out_iounmap_ctrl:
	iounmap(mod->ctrl);
out_iounmap_dpm:
	iounmap(mod->dpm);
out_free_ndev:
	free_candev(ndev);
out_return:
	return ret;
}

static int __devexit ican3_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ican3_dev *mod = netdev_priv(ndev);

	
	unregister_netdev(ndev);
	napi_disable(&mod->napi);
	iowrite8(1 << mod->num, &mod->ctrl->int_disable);
	free_irq(mod->irq, mod);

	
	ican3_shutdown_module(mod);

	
	iounmap(mod->ctrl);
	iounmap(mod->dpm);

	free_candev(ndev);

	return 0;
}

static struct platform_driver ican3_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= ican3_probe,
	.remove		= __devexit_p(ican3_remove),
};

module_platform_driver(ican3_driver);

MODULE_AUTHOR("Ira W. Snyder <iws@ovro.caltech.edu>");
MODULE_DESCRIPTION("Janz MODULbus VMOD-ICAN3 Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:janz-ican3");
