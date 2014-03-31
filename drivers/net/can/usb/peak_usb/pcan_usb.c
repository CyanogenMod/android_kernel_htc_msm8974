/*
 * CAN driver for PEAK System PCAN-USB adapter
 * Derived from the PCAN project file driver/src/pcan_usb.c
 *
 * Copyright (C) 2003-2010 PEAK System-Technik GmbH
 * Copyright (C) 2011-2012 Stephane Grosjean <s.grosjean@peak-system.com>
 *
 * Many thanks to Klaus Hitschler <klaus.hitschler@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/netdevice.h>
#include <linux/usb.h>
#include <linux/module.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

#include "pcan_usb_core.h"

MODULE_SUPPORTED_DEVICE("PEAK-System PCAN-USB adapter");

#define PCAN_USB_EP_CMDOUT		1
#define PCAN_USB_EP_CMDIN		(PCAN_USB_EP_CMDOUT | USB_DIR_IN)
#define PCAN_USB_EP_MSGOUT		2
#define PCAN_USB_EP_MSGIN		(PCAN_USB_EP_MSGOUT | USB_DIR_IN)

#define PCAN_USB_CMD_FUNC		0
#define PCAN_USB_CMD_NUM		1
#define PCAN_USB_CMD_ARGS		2
#define PCAN_USB_CMD_ARGS_LEN		14
#define PCAN_USB_CMD_LEN		(PCAN_USB_CMD_ARGS + \
					 PCAN_USB_CMD_ARGS_LEN)

#define PCAN_USB_COMMAND_TIMEOUT	1000

#define PCAN_USB_STARTUP_TIMEOUT	10

#define PCAN_USB_RX_BUFFER_SIZE		64
#define PCAN_USB_TX_BUFFER_SIZE		64

#define PCAN_USB_MSG_HEADER_LEN		2

#define PCAN_USB_CRYSTAL_HZ		16000000

#define PCAN_USB_STATUSLEN_TIMESTAMP	(1 << 7)
#define PCAN_USB_STATUSLEN_INTERNAL	(1 << 6)
#define PCAN_USB_STATUSLEN_EXT_ID	(1 << 5)
#define PCAN_USB_STATUSLEN_RTR		(1 << 4)
#define PCAN_USB_STATUSLEN_DLC		(0xf)

#define PCAN_USB_ERROR_TXFULL		0x01
#define PCAN_USB_ERROR_RXQOVR		0x02
#define PCAN_USB_ERROR_BUS_LIGHT	0x04
#define PCAN_USB_ERROR_BUS_HEAVY	0x08
#define PCAN_USB_ERROR_BUS_OFF		0x10
#define PCAN_USB_ERROR_RXQEMPTY		0x20
#define PCAN_USB_ERROR_QOVR		0x40
#define PCAN_USB_ERROR_TXQFULL		0x80

#define SJA1000_MODE_NORMAL		0x00
#define SJA1000_MODE_INIT		0x01

#define PCAN_USB_TS_DIV_SHIFTER		20
#define PCAN_USB_TS_US_PER_TICK		44739243

#define PCAN_USB_REC_ERROR		1
#define PCAN_USB_REC_ANALOG		2
#define PCAN_USB_REC_BUSLOAD		3
#define PCAN_USB_REC_TS			4
#define PCAN_USB_REC_BUSEVT		5

struct pcan_usb {
	struct peak_usb_device dev;
	struct peak_time_ref time_ref;
	struct timer_list restart_timer;
};

struct pcan_usb_msg_context {
	u16 ts16;
	u8 prev_ts8;
	u8 *ptr;
	u8 *end;
	u8 rec_cnt;
	u8 rec_idx;
	u8 rec_data_idx;
	struct net_device *netdev;
	struct pcan_usb *pdev;
};

static int pcan_usb_send_cmd(struct peak_usb_device *dev, u8 f, u8 n, u8 *p)
{
	int err;
	int actual_length;

	
	if (!(dev->state & PCAN_USB_STATE_CONNECTED))
		return 0;

	dev->cmd_buf[PCAN_USB_CMD_FUNC] = f;
	dev->cmd_buf[PCAN_USB_CMD_NUM] = n;

	if (p)
		memcpy(dev->cmd_buf + PCAN_USB_CMD_ARGS,
			p, PCAN_USB_CMD_ARGS_LEN);

	err = usb_bulk_msg(dev->udev,
			usb_sndbulkpipe(dev->udev, PCAN_USB_EP_CMDOUT),
			dev->cmd_buf, PCAN_USB_CMD_LEN, &actual_length,
			PCAN_USB_COMMAND_TIMEOUT);
	if (err)
		netdev_err(dev->netdev,
			"sending cmd f=0x%x n=0x%x failure: %d\n",
			f, n, err);
	return err;
}

static int pcan_usb_wait_rsp(struct peak_usb_device *dev, u8 f, u8 n, u8 *p)
{
	int err;
	int actual_length;

	
	if (!(dev->state & PCAN_USB_STATE_CONNECTED))
		return 0;

	
	err = pcan_usb_send_cmd(dev, f, n, NULL);
	if (err)
		return err;

	err = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, PCAN_USB_EP_CMDIN),
		dev->cmd_buf, PCAN_USB_CMD_LEN, &actual_length,
		PCAN_USB_COMMAND_TIMEOUT);
	if (err)
		netdev_err(dev->netdev,
			"waiting rsp f=0x%x n=0x%x failure: %d\n", f, n, err);
	else if (p)
		memcpy(p, dev->cmd_buf + PCAN_USB_CMD_ARGS,
			PCAN_USB_CMD_ARGS_LEN);

	return err;
}

static int pcan_usb_set_sja1000(struct peak_usb_device *dev, u8 mode)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[1] = mode,
	};

	return pcan_usb_send_cmd(dev, 9, 2, args);
}

static int pcan_usb_set_bus(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, 3, 2, args);
}

static int pcan_usb_set_silent(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, 3, 3, args);
}

static int pcan_usb_set_ext_vcc(struct peak_usb_device *dev, u8 onoff)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN] = {
		[0] = !!onoff,
	};

	return pcan_usb_send_cmd(dev, 10, 2, args);
}

static int pcan_usb_set_bittiming(struct peak_usb_device *dev,
				  struct can_bittiming *bt)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	u8 btr0, btr1;

	btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
	btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) |
		(((bt->phase_seg2 - 1) & 0x7) << 4);
	if (dev->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		btr1 |= 0x80;

	netdev_info(dev->netdev, "setting BTR0=0x%02x BTR1=0x%02x\n",
		btr0, btr1);

	args[0] = btr1;
	args[1] = btr0;

	return pcan_usb_send_cmd(dev, 1, 2, args);
}

static int pcan_usb_write_mode(struct peak_usb_device *dev, u8 onoff)
{
	int err;

	err = pcan_usb_set_bus(dev, onoff);
	if (err)
		return err;

	if (!onoff) {
		err = pcan_usb_set_sja1000(dev, SJA1000_MODE_INIT);
	} else {
		
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT));
	}

	return err;
}

static void pcan_usb_restart(unsigned long arg)
{
	
	peak_usb_restart_complete((struct peak_usb_device *)arg);
}

static void pcan_usb_restart_pending(struct urb *urb)
{
	struct pcan_usb *pdev = urb->context;

	
	mod_timer(&pdev->restart_timer,
			jiffies + msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT));

	
	peak_usb_async_complete(urb);
}

static int pcan_usb_restart_async(struct peak_usb_device *dev, struct urb *urb,
				  u8 *buf)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);

	if (timer_pending(&pdev->restart_timer))
		return -EBUSY;

	
	buf[PCAN_USB_CMD_FUNC] = 3;
	buf[PCAN_USB_CMD_NUM] = 2;
	buf[PCAN_USB_CMD_ARGS] = 1;

	usb_fill_bulk_urb(urb, dev->udev,
			usb_sndbulkpipe(dev->udev, PCAN_USB_EP_CMDOUT),
			buf, PCAN_USB_CMD_LEN,
			pcan_usb_restart_pending, pdev);

	return usb_submit_urb(urb, GFP_ATOMIC);
}

static int pcan_usb_get_serial(struct peak_usb_device *dev, u32 *serial_number)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	int err;

	err = pcan_usb_wait_rsp(dev, 6, 1, args);
	if (err) {
		netdev_err(dev->netdev, "getting serial failure: %d\n", err);
	} else if (serial_number) {
		u32 tmp32;

		memcpy(&tmp32, args, 4);
		*serial_number = le32_to_cpu(tmp32);
	}

	return err;
}

static int pcan_usb_get_device_id(struct peak_usb_device *dev, u32 *device_id)
{
	u8 args[PCAN_USB_CMD_ARGS_LEN];
	int err;

	err = pcan_usb_wait_rsp(dev, 4, 1, args);
	if (err)
		netdev_err(dev->netdev, "getting device id failure: %d\n", err);
	else if (device_id)
		*device_id = args[0];

	return err;
}

static int pcan_usb_update_ts(struct pcan_usb_msg_context *mc)
{
	u16 tmp16;

	if ((mc->ptr+2) > mc->end)
		return -EINVAL;

	memcpy(&tmp16, mc->ptr, 2);

	mc->ts16 = le16_to_cpu(tmp16);

	if (mc->rec_idx > 0)
		peak_usb_update_ts_now(&mc->pdev->time_ref, mc->ts16);
	else
		peak_usb_set_ts_now(&mc->pdev->time_ref, mc->ts16);

	return 0;
}

static int pcan_usb_decode_ts(struct pcan_usb_msg_context *mc, u8 first_packet)
{
	
	if (first_packet) {
		u16 tmp16;

		if ((mc->ptr + 2) > mc->end)
			return -EINVAL;

		memcpy(&tmp16, mc->ptr, 2);
		mc->ptr += 2;

		mc->ts16 = le16_to_cpu(tmp16);
		mc->prev_ts8 = mc->ts16 & 0x00ff;
	} else {
		u8 ts8;

		if ((mc->ptr + 1) > mc->end)
			return -EINVAL;

		ts8 = *mc->ptr++;

		if (ts8 < mc->prev_ts8)
			mc->ts16 += 0x100;

		mc->ts16 &= 0xff00;
		mc->ts16 |= ts8;
		mc->prev_ts8 = ts8;
	}

	return 0;
}

static int pcan_usb_decode_error(struct pcan_usb_msg_context *mc, u8 n,
				 u8 status_len)
{
	struct sk_buff *skb;
	struct can_frame *cf;
	struct timeval tv;
	enum can_state new_state;

	
	if (n == PCAN_USB_ERROR_QOVR)
		if (!mc->pdev->time_ref.tick_count)
			return 0;

	new_state = mc->pdev->dev.can.state;

	switch (mc->pdev->dev.can.state) {
	case CAN_STATE_ERROR_ACTIVE:
		if (n & PCAN_USB_ERROR_BUS_LIGHT) {
			new_state = CAN_STATE_ERROR_WARNING;
			break;
		}

	case CAN_STATE_ERROR_WARNING:
		if (n & PCAN_USB_ERROR_BUS_HEAVY) {
			new_state = CAN_STATE_ERROR_PASSIVE;
			break;
		}
		if (n & PCAN_USB_ERROR_BUS_OFF) {
			new_state = CAN_STATE_BUS_OFF;
			break;
		}
		if (n & (PCAN_USB_ERROR_RXQOVR | PCAN_USB_ERROR_QOVR)) {
			new_state = CAN_STATE_MAX;
			break;
		}
		if ((n & PCAN_USB_ERROR_BUS_LIGHT) == 0) {
			
			mc->pdev->dev.can.state = CAN_STATE_ERROR_ACTIVE;
			return 0;
		}
		break;

	case CAN_STATE_ERROR_PASSIVE:
		if (n & PCAN_USB_ERROR_BUS_OFF) {
			new_state = CAN_STATE_BUS_OFF;
			break;
		}
		if (n & PCAN_USB_ERROR_BUS_LIGHT) {
			new_state = CAN_STATE_ERROR_WARNING;
			break;
		}
		if (n & (PCAN_USB_ERROR_RXQOVR | PCAN_USB_ERROR_QOVR)) {
			new_state = CAN_STATE_MAX;
			break;
		}

		if ((n & PCAN_USB_ERROR_BUS_HEAVY) == 0) {
			
			mc->pdev->dev.can.state = CAN_STATE_ERROR_ACTIVE;
			return 0;
		}
		break;

	default:
		
		return 0;
	}

	
	if (mc->pdev->dev.can.state == new_state)
		return 0;

	
	skb = alloc_can_err_skb(mc->netdev, &cf);
	if (!skb)
		return -ENOMEM;

	switch (new_state) {
	case CAN_STATE_BUS_OFF:
		cf->can_id |= CAN_ERR_BUSOFF;
		can_bus_off(mc->netdev);
		break;

	case CAN_STATE_ERROR_PASSIVE:
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] |= CAN_ERR_CRTL_TX_PASSIVE |
			       CAN_ERR_CRTL_RX_PASSIVE;
		mc->pdev->dev.can.can_stats.error_passive++;
		break;

	case CAN_STATE_ERROR_WARNING:
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] |= CAN_ERR_CRTL_TX_WARNING |
			       CAN_ERR_CRTL_RX_WARNING;
		mc->pdev->dev.can.can_stats.error_warning++;
		break;

	default:
		
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
		mc->netdev->stats.rx_over_errors++;
		mc->netdev->stats.rx_errors++;

		new_state = mc->pdev->dev.can.state;
		break;
	}

	mc->pdev->dev.can.state = new_state;

	if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP) {
		peak_usb_get_ts_tv(&mc->pdev->time_ref, mc->ts16, &tv);
		skb->tstamp = timeval_to_ktime(tv);
	}

	netif_rx(skb);
	mc->netdev->stats.rx_packets++;
	mc->netdev->stats.rx_bytes += cf->can_dlc;

	return 0;
}

static int pcan_usb_decode_status(struct pcan_usb_msg_context *mc,
				  u8 status_len)
{
	u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
	u8 f, n;
	int err;

	
	if ((mc->ptr + 2) > mc->end)
		return -EINVAL;

	f = mc->ptr[PCAN_USB_CMD_FUNC];
	n = mc->ptr[PCAN_USB_CMD_NUM];
	mc->ptr += PCAN_USB_CMD_ARGS;

	if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP) {
		int err = pcan_usb_decode_ts(mc, !mc->rec_idx);

		if (err)
			return err;
	}

	switch (f) {
	case PCAN_USB_REC_ERROR:
		err = pcan_usb_decode_error(mc, n, status_len);
		if (err)
			return err;
		break;

	case PCAN_USB_REC_ANALOG:
		
		rec_len = 2;
		break;

	case PCAN_USB_REC_BUSLOAD:
		
		rec_len = 1;
		break;

	case PCAN_USB_REC_TS:
		
		if (pcan_usb_update_ts(mc))
			return -EINVAL;
		break;

	case PCAN_USB_REC_BUSEVT:
		
		if (n & PCAN_USB_ERROR_TXQFULL)
			netdev_dbg(mc->netdev, "device Tx queue full)\n");
		break;
	default:
		netdev_err(mc->netdev, "unexpected function %u\n", f);
		break;
	}

	if ((mc->ptr + rec_len) > mc->end)
		return -EINVAL;

	mc->ptr += rec_len;

	return 0;
}

static int pcan_usb_decode_data(struct pcan_usb_msg_context *mc, u8 status_len)
{
	u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
	struct sk_buff *skb;
	struct can_frame *cf;
	struct timeval tv;

	skb = alloc_can_skb(mc->netdev, &cf);
	if (!skb)
		return -ENOMEM;

	if (status_len & PCAN_USB_STATUSLEN_EXT_ID) {
		u32 tmp32;

		if ((mc->ptr + 4) > mc->end)
			goto decode_failed;

		memcpy(&tmp32, mc->ptr, 4);
		mc->ptr += 4;

		cf->can_id = le32_to_cpu(tmp32 >> 3) | CAN_EFF_FLAG;
	} else {
		u16 tmp16;

		if ((mc->ptr + 2) > mc->end)
			goto decode_failed;

		memcpy(&tmp16, mc->ptr, 2);
		mc->ptr += 2;

		cf->can_id = le16_to_cpu(tmp16 >> 5);
	}

	cf->can_dlc = get_can_dlc(rec_len);

	
	if (pcan_usb_decode_ts(mc, !mc->rec_data_idx))
		goto decode_failed;

	
	memset(cf->data, 0x0, sizeof(cf->data));
	if (status_len & PCAN_USB_STATUSLEN_RTR) {
		cf->can_id |= CAN_RTR_FLAG;
	} else {
		if ((mc->ptr + rec_len) > mc->end)
			goto decode_failed;

		memcpy(cf->data, mc->ptr, rec_len);
		mc->ptr += rec_len;
	}

	
	peak_usb_get_ts_tv(&mc->pdev->time_ref, mc->ts16, &tv);
	skb->tstamp = timeval_to_ktime(tv);

	
	netif_rx(skb);

	
	mc->netdev->stats.rx_packets++;
	mc->netdev->stats.rx_bytes += cf->can_dlc;

	return 0;

decode_failed:
	dev_kfree_skb(skb);
	return -EINVAL;
}

static int pcan_usb_decode_msg(struct peak_usb_device *dev, u8 *ibuf, u32 lbuf)
{
	struct pcan_usb_msg_context mc = {
		.rec_cnt = ibuf[1],
		.ptr = ibuf + PCAN_USB_MSG_HEADER_LEN,
		.end = ibuf + lbuf,
		.netdev = dev->netdev,
		.pdev = container_of(dev, struct pcan_usb, dev),
	};
	int err;

	for (err = 0; mc.rec_idx < mc.rec_cnt && !err; mc.rec_idx++) {
		u8 sl = *mc.ptr++;

		
		if (sl & PCAN_USB_STATUSLEN_INTERNAL) {
			err = pcan_usb_decode_status(&mc, sl);
		
		} else {
			err = pcan_usb_decode_data(&mc, sl);
			mc.rec_data_idx++;
		}
	}

	return err;
}

static int pcan_usb_decode_buf(struct peak_usb_device *dev, struct urb *urb)
{
	int err = 0;

	if (urb->actual_length > PCAN_USB_MSG_HEADER_LEN) {
		err = pcan_usb_decode_msg(dev, urb->transfer_buffer,
			urb->actual_length);

	} else if (urb->actual_length > 0) {
		netdev_err(dev->netdev, "usb message length error (%u)\n",
			urb->actual_length);
		err = -EINVAL;
	}

	return err;
}

static int pcan_usb_encode_msg(struct peak_usb_device *dev, struct sk_buff *skb,
			       u8 *obuf, size_t *size)
{
	struct net_device *netdev = dev->netdev;
	struct net_device_stats *stats = &netdev->stats;
	struct can_frame *cf = (struct can_frame *)skb->data;
	u8 *pc;

	obuf[0] = 2;
	obuf[1] = 1;

	pc = obuf + PCAN_USB_MSG_HEADER_LEN;

	
	*pc = cf->can_dlc;
	if (cf->can_id & CAN_RTR_FLAG)
		*pc |= PCAN_USB_STATUSLEN_RTR;

	
	if (cf->can_id & CAN_EFF_FLAG) {
		__le32 tmp32 = cpu_to_le32((cf->can_id & CAN_ERR_MASK) << 3);

		*pc |= PCAN_USB_STATUSLEN_EXT_ID;
		memcpy(++pc, &tmp32, 4);
		pc += 4;
	} else {
		__le16 tmp16 = cpu_to_le16((cf->can_id & CAN_ERR_MASK) << 5);

		memcpy(++pc, &tmp16, 2);
		pc += 2;
	}

	
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		memcpy(pc, cf->data, cf->can_dlc);
		pc += cf->can_dlc;
	}

	obuf[(*size)-1] = (u8)(stats->tx_packets & 0xff);

	return 0;
}

static int pcan_usb_start(struct peak_usb_device *dev)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);

	
	peak_usb_init_time_ref(&pdev->time_ref, &pcan_usb);

	
	if (dev->device_rev > 3) {
		int err;

		err = pcan_usb_set_silent(dev,
				dev->can.ctrlmode & CAN_CTRLMODE_LISTENONLY);
		if (err)
			return err;
	}

	return pcan_usb_set_ext_vcc(dev, 0);
}

static int pcan_usb_init(struct peak_usb_device *dev)
{
	struct pcan_usb *pdev = container_of(dev, struct pcan_usb, dev);
	u32 serial_number;
	int err;

	
	init_timer(&pdev->restart_timer);
	pdev->restart_timer.function = pcan_usb_restart;
	pdev->restart_timer.data = (unsigned long)dev;

	err = pcan_usb_get_serial(dev, &serial_number);
	if (err) {
		dev_err(dev->netdev->dev.parent,
			"unable to read %s serial number (err %d)\n",
			pcan_usb.name, err);
		return err;
	}

	dev_info(dev->netdev->dev.parent,
		 "PEAK-System %s adapter hwrev %u serial %08X (%u channel)\n",
		 pcan_usb.name, dev->device_rev, serial_number,
		 pcan_usb.ctrl_count);

	return 0;
}

static int pcan_usb_probe(struct usb_interface *intf)
{
	struct usb_host_interface *if_desc;
	int i;

	if_desc = intf->altsetting;

	
	for (i = 0; i < if_desc->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *ep = &if_desc->endpoint[i].desc;

		switch (ep->bEndpointAddress) {
		case PCAN_USB_EP_CMDOUT:
		case PCAN_USB_EP_CMDIN:
		case PCAN_USB_EP_MSGOUT:
		case PCAN_USB_EP_MSGIN:
			break;
		default:
			return -ENODEV;
		}
	}

	return 0;
}

struct peak_usb_adapter pcan_usb = {
	.name = "PCAN-USB",
	.device_id = PCAN_USB_PRODUCT_ID,
	.ctrl_count = 1,
	.clock = {
		.freq = PCAN_USB_CRYSTAL_HZ / 2 ,
	},
	.bittiming_const = {
		.name = "pcan_usb",
		.tseg1_min = 1,
		.tseg1_max = 16,
		.tseg2_min = 1,
		.tseg2_max = 8,
		.sjw_max = 4,
		.brp_min = 1,
		.brp_max = 64,
		.brp_inc = 1,
	},

	
	.sizeof_dev_private = sizeof(struct pcan_usb),

	
	.ts_used_bits = 16,
	.ts_period = 24575, 
	.us_per_ts_scale = PCAN_USB_TS_US_PER_TICK, 
	.us_per_ts_shift = PCAN_USB_TS_DIV_SHIFTER, 

	
	.ep_msg_in = PCAN_USB_EP_MSGIN,
	.ep_msg_out = {PCAN_USB_EP_MSGOUT},

	
	.rx_buffer_size = PCAN_USB_RX_BUFFER_SIZE,
	.tx_buffer_size = PCAN_USB_TX_BUFFER_SIZE,

	
	.intf_probe = pcan_usb_probe,
	.dev_init = pcan_usb_init,
	.dev_set_bus = pcan_usb_write_mode,
	.dev_set_bittiming = pcan_usb_set_bittiming,
	.dev_get_device_id = pcan_usb_get_device_id,
	.dev_decode_buf = pcan_usb_decode_buf,
	.dev_encode_msg = pcan_usb_encode_msg,
	.dev_start = pcan_usb_start,
	.dev_restart_async = pcan_usb_restart_async,
};
