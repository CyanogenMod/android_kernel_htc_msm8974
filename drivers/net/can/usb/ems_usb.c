/*
 * CAN driver for EMS Dr. Thomas Wuensche CPC-USB/ARM7
 *
 * Copyright (C) 2004-2009 EMS Dr. Thomas Wuensche
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/usb.h>

#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>

MODULE_AUTHOR("Sebastian Haas <haas@ems-wuensche.com>");
MODULE_DESCRIPTION("CAN driver for EMS Dr. Thomas Wuensche CAN/USB interfaces");
MODULE_LICENSE("GPL v2");

#define CONTR_CAN_MESSAGE 0x04
#define CONTR_CAN_STATE   0x0C
#define CONTR_BUS_ERROR   0x1C

#define CONTR_CONT_OFF 0
#define CONTR_CONT_ON  1
#define CONTR_ONCE     2

#define CPC_MSG_TYPE_CAN_FRAME       1  
#define CPC_MSG_TYPE_RTR_FRAME       8  
#define CPC_MSG_TYPE_CAN_PARAMS      12 
#define CPC_MSG_TYPE_CAN_STATE       14 
#define CPC_MSG_TYPE_EXT_CAN_FRAME   16 
#define CPC_MSG_TYPE_EXT_RTR_FRAME   17 
#define CPC_MSG_TYPE_CONTROL         19 
#define CPC_MSG_TYPE_CONFIRM         20 
#define CPC_MSG_TYPE_OVERRUN         21 
#define CPC_MSG_TYPE_CAN_FRAME_ERROR 23 
#define CPC_MSG_TYPE_ERR_COUNTER     25 

#define CPC_CMD_TYPE_CAN_FRAME     1   
#define CPC_CMD_TYPE_CONTROL       3   
#define CPC_CMD_TYPE_CAN_PARAMS    6   
#define CPC_CMD_TYPE_RTR_FRAME     13  
#define CPC_CMD_TYPE_CAN_STATE     14  
#define CPC_CMD_TYPE_EXT_CAN_FRAME 15  
#define CPC_CMD_TYPE_EXT_RTR_FRAME 16  
#define CPC_CMD_TYPE_CAN_EXIT      200 

#define CPC_CMD_TYPE_INQ_ERR_COUNTER 25 
#define CPC_CMD_TYPE_CLEAR_MSG_QUEUE 8  
#define CPC_CMD_TYPE_CLEAR_CMD_QUEUE 28 

#define CPC_CC_TYPE_SJA1000 2 

#define CPC_CAN_ECODE_ERRFRAME 0x01 

#define CPC_OVR_EVENT_CAN       0x01
#define CPC_OVR_EVENT_CANSTATE  0x02
#define CPC_OVR_EVENT_BUSERROR  0x04

#define CPC_OVR_HW 0x80

#define CPC_MSG_HEADER_LEN   11
#define CPC_CAN_MSG_MIN_SIZE 5

#define USB_CPCUSB_VENDOR_ID 0x12D6

#define USB_CPCUSB_ARM7_PRODUCT_ID 0x0444

#define SJA1000_MOD_NORMAL 0x00
#define SJA1000_MOD_RM     0x01

#define SJA1000_ECC_SEG   0x1F
#define SJA1000_ECC_DIR   0x20
#define SJA1000_ECC_ERR   0x06
#define SJA1000_ECC_BIT   0x00
#define SJA1000_ECC_FORM  0x40
#define SJA1000_ECC_STUFF 0x80
#define SJA1000_ECC_MASK  0xc0

#define SJA1000_SR_BS 0x80
#define SJA1000_SR_ES 0x40

#define SJA1000_DEFAULT_OUTPUT_CONTROL 0xDA

#define EMS_USB_ARM7_CLOCK 8000000

struct cpc_can_msg {
	u32 id;
	u8 length;
	u8 msg[8];
};

struct cpc_sja1000_params {
	u8 mode;
	u8 acc_code0;
	u8 acc_code1;
	u8 acc_code2;
	u8 acc_code3;
	u8 acc_mask0;
	u8 acc_mask1;
	u8 acc_mask2;
	u8 acc_mask3;
	u8 btr0;
	u8 btr1;
	u8 outp_contr;
};

struct cpc_can_params {
	u8 cc_type;

	
	union {
		struct cpc_sja1000_params sja1000;
	} cc_params;
};

struct cpc_confirm {
	u8 error; 
};

struct cpc_overrun {
	u8 event;
	u8 count;
};

struct cpc_sja1000_can_error {
	u8 ecc;
	u8 rxerr;
	u8 txerr;
};

struct cpc_can_error {
	u8 ecode;

	struct {
		u8 cc_type;

		
		union {
			struct cpc_sja1000_can_error sja1000;
		} regs;
	} cc;
};

struct cpc_can_err_counter {
	u8 rx;
	u8 tx;
};

struct __packed ems_cpc_msg {
	u8 type;	
	u8 length;	
	u8 msgid;	
	u32 ts_sec;	
	u32 ts_nsec;	

	union {
		u8 generic[64];
		struct cpc_can_msg can_msg;
		struct cpc_can_params can_params;
		struct cpc_confirm confirmation;
		struct cpc_overrun overrun;
		struct cpc_can_error error;
		struct cpc_can_err_counter err_counter;
		u8 can_state;
	} msg;
};

static struct usb_device_id ems_usb_table[] = {
	{USB_DEVICE(USB_CPCUSB_VENDOR_ID, USB_CPCUSB_ARM7_PRODUCT_ID)},
	{} 
};

MODULE_DEVICE_TABLE(usb, ems_usb_table);

#define RX_BUFFER_SIZE      64
#define CPC_HEADER_SIZE     4
#define INTR_IN_BUFFER_SIZE 4

#define MAX_RX_URBS 10
#define MAX_TX_URBS 10

struct ems_usb;

struct ems_tx_urb_context {
	struct ems_usb *dev;

	u32 echo_index;
	u8 dlc;
};

struct ems_usb {
	struct can_priv can; 
	int open_time;

	struct sk_buff *echo_skb[MAX_TX_URBS];

	struct usb_device *udev;
	struct net_device *netdev;

	atomic_t active_tx_urbs;
	struct usb_anchor tx_submitted;
	struct ems_tx_urb_context tx_contexts[MAX_TX_URBS];

	struct usb_anchor rx_submitted;

	struct urb *intr_urb;

	u8 *tx_msg_buffer;

	u8 *intr_in_buffer;
	unsigned int free_slots; 

	struct ems_cpc_msg active_params; 
};

static void ems_usb_read_interrupt_callback(struct urb *urb)
{
	struct ems_usb *dev = urb->context;
	struct net_device *netdev = dev->netdev;
	int err;

	if (!netif_device_present(netdev))
		return;

	switch (urb->status) {
	case 0:
		dev->free_slots = dev->intr_in_buffer[1];
		break;

	case -ECONNRESET: 
	case -ENOENT:
	case -ESHUTDOWN:
		return;

	default:
		netdev_info(netdev, "Rx interrupt aborted %d\n", urb->status);
		break;
	}

	err = usb_submit_urb(urb, GFP_ATOMIC);

	if (err == -ENODEV)
		netif_device_detach(netdev);
	else if (err)
		netdev_err(netdev, "failed resubmitting intr urb: %d\n", err);
}

static void ems_usb_rx_can_msg(struct ems_usb *dev, struct ems_cpc_msg *msg)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	int i;
	struct net_device_stats *stats = &dev->netdev->stats;

	skb = alloc_can_skb(dev->netdev, &cf);
	if (skb == NULL)
		return;

	cf->can_id = le32_to_cpu(msg->msg.can_msg.id);
	cf->can_dlc = get_can_dlc(msg->msg.can_msg.length & 0xF);

	if (msg->type == CPC_MSG_TYPE_EXT_CAN_FRAME ||
	    msg->type == CPC_MSG_TYPE_EXT_RTR_FRAME)
		cf->can_id |= CAN_EFF_FLAG;

	if (msg->type == CPC_MSG_TYPE_RTR_FRAME ||
	    msg->type == CPC_MSG_TYPE_EXT_RTR_FRAME) {
		cf->can_id |= CAN_RTR_FLAG;
	} else {
		for (i = 0; i < cf->can_dlc; i++)
			cf->data[i] = msg->msg.can_msg.msg[i];
	}

	netif_rx(skb);

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
}

static void ems_usb_rx_err(struct ems_usb *dev, struct ems_cpc_msg *msg)
{
	struct can_frame *cf;
	struct sk_buff *skb;
	struct net_device_stats *stats = &dev->netdev->stats;

	skb = alloc_can_err_skb(dev->netdev, &cf);
	if (skb == NULL)
		return;

	if (msg->type == CPC_MSG_TYPE_CAN_STATE) {
		u8 state = msg->msg.can_state;

		if (state & SJA1000_SR_BS) {
			dev->can.state = CAN_STATE_BUS_OFF;
			cf->can_id |= CAN_ERR_BUSOFF;

			can_bus_off(dev->netdev);
		} else if (state & SJA1000_SR_ES) {
			dev->can.state = CAN_STATE_ERROR_WARNING;
			dev->can.can_stats.error_warning++;
		} else {
			dev->can.state = CAN_STATE_ERROR_ACTIVE;
			dev->can.can_stats.error_passive++;
		}
	} else if (msg->type == CPC_MSG_TYPE_CAN_FRAME_ERROR) {
		u8 ecc = msg->msg.error.cc.regs.sja1000.ecc;
		u8 txerr = msg->msg.error.cc.regs.sja1000.txerr;
		u8 rxerr = msg->msg.error.cc.regs.sja1000.rxerr;

		
		dev->can.can_stats.bus_error++;
		stats->rx_errors++;

		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

		switch (ecc & SJA1000_ECC_MASK) {
		case SJA1000_ECC_BIT:
			cf->data[2] |= CAN_ERR_PROT_BIT;
			break;
		case SJA1000_ECC_FORM:
			cf->data[2] |= CAN_ERR_PROT_FORM;
			break;
		case SJA1000_ECC_STUFF:
			cf->data[2] |= CAN_ERR_PROT_STUFF;
			break;
		default:
			cf->data[2] |= CAN_ERR_PROT_UNSPEC;
			cf->data[3] = ecc & SJA1000_ECC_SEG;
			break;
		}

		
		if ((ecc & SJA1000_ECC_DIR) == 0)
			cf->data[2] |= CAN_ERR_PROT_TX;

		if (dev->can.state == CAN_STATE_ERROR_WARNING ||
		    dev->can.state == CAN_STATE_ERROR_PASSIVE) {
			cf->data[1] = (txerr > rxerr) ?
			    CAN_ERR_CRTL_TX_PASSIVE : CAN_ERR_CRTL_RX_PASSIVE;
		}
	} else if (msg->type == CPC_MSG_TYPE_OVERRUN) {
		cf->can_id |= CAN_ERR_CRTL;
		cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;

		stats->rx_over_errors++;
		stats->rx_errors++;
	}

	netif_rx(skb);

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
}

static void ems_usb_read_bulk_callback(struct urb *urb)
{
	struct ems_usb *dev = urb->context;
	struct net_device *netdev;
	int retval;

	netdev = dev->netdev;

	if (!netif_device_present(netdev))
		return;

	switch (urb->status) {
	case 0: 
		break;

	case -ENOENT:
		return;

	default:
		netdev_info(netdev, "Rx URB aborted (%d)\n", urb->status);
		goto resubmit_urb;
	}

	if (urb->actual_length > CPC_HEADER_SIZE) {
		struct ems_cpc_msg *msg;
		u8 *ibuf = urb->transfer_buffer;
		u8 msg_count, again, start;

		msg_count = ibuf[0] & ~0x80;
		again = ibuf[0] & 0x80;

		start = CPC_HEADER_SIZE;

		while (msg_count) {
			msg = (struct ems_cpc_msg *)&ibuf[start];

			switch (msg->type) {
			case CPC_MSG_TYPE_CAN_STATE:
				
				ems_usb_rx_err(dev, msg);
				break;

			case CPC_MSG_TYPE_CAN_FRAME:
			case CPC_MSG_TYPE_EXT_CAN_FRAME:
			case CPC_MSG_TYPE_RTR_FRAME:
			case CPC_MSG_TYPE_EXT_RTR_FRAME:
				ems_usb_rx_can_msg(dev, msg);
				break;

			case CPC_MSG_TYPE_CAN_FRAME_ERROR:
				
				ems_usb_rx_err(dev, msg);
				break;

			case CPC_MSG_TYPE_OVERRUN:
				
				ems_usb_rx_err(dev, msg);
				break;
			}

			start += CPC_MSG_HEADER_LEN + msg->length;
			msg_count--;

			if (start > urb->transfer_buffer_length) {
				netdev_err(netdev, "format error\n");
				break;
			}
		}
	}

resubmit_urb:
	usb_fill_bulk_urb(urb, dev->udev, usb_rcvbulkpipe(dev->udev, 2),
			  urb->transfer_buffer, RX_BUFFER_SIZE,
			  ems_usb_read_bulk_callback, dev);

	retval = usb_submit_urb(urb, GFP_ATOMIC);

	if (retval == -ENODEV)
		netif_device_detach(netdev);
	else if (retval)
		netdev_err(netdev,
			   "failed resubmitting read bulk urb: %d\n", retval);
}

static void ems_usb_write_bulk_callback(struct urb *urb)
{
	struct ems_tx_urb_context *context = urb->context;
	struct ems_usb *dev;
	struct net_device *netdev;

	BUG_ON(!context);

	dev = context->dev;
	netdev = dev->netdev;

	
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);

	atomic_dec(&dev->active_tx_urbs);

	if (!netif_device_present(netdev))
		return;

	if (urb->status)
		netdev_info(netdev, "Tx URB aborted (%d)\n", urb->status);

	netdev->trans_start = jiffies;

	
	netdev->stats.tx_packets++;
	netdev->stats.tx_bytes += context->dlc;

	can_get_echo_skb(netdev, context->echo_index);

	
	context->echo_index = MAX_TX_URBS;

	if (netif_queue_stopped(netdev))
		netif_wake_queue(netdev);
}

static int ems_usb_command_msg(struct ems_usb *dev, struct ems_cpc_msg *msg)
{
	int actual_length;

	
	memcpy(&dev->tx_msg_buffer[CPC_HEADER_SIZE], msg,
	       msg->length + CPC_MSG_HEADER_LEN);

	
	memset(&dev->tx_msg_buffer[0], 0, CPC_HEADER_SIZE);

	return usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, 2),
			    &dev->tx_msg_buffer[0],
			    msg->length + CPC_MSG_HEADER_LEN + CPC_HEADER_SIZE,
			    &actual_length, 1000);
}

static int ems_usb_write_mode(struct ems_usb *dev, u8 mode)
{
	dev->active_params.msg.can_params.cc_params.sja1000.mode = mode;

	return ems_usb_command_msg(dev, &dev->active_params);
}

static int ems_usb_control_cmd(struct ems_usb *dev, u8 val)
{
	struct ems_cpc_msg cmd;

	cmd.type = CPC_CMD_TYPE_CONTROL;
	cmd.length = CPC_MSG_HEADER_LEN + 1;

	cmd.msgid = 0;

	cmd.msg.generic[0] = val;

	return ems_usb_command_msg(dev, &cmd);
}

static int ems_usb_start(struct ems_usb *dev)
{
	struct net_device *netdev = dev->netdev;
	int err, i;

	dev->intr_in_buffer[0] = 0;
	dev->free_slots = 15; 

	for (i = 0; i < MAX_RX_URBS; i++) {
		struct urb *urb = NULL;
		u8 *buf = NULL;

		
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			netdev_err(netdev, "No memory left for URBs\n");
			err = -ENOMEM;
			break;
		}

		buf = usb_alloc_coherent(dev->udev, RX_BUFFER_SIZE, GFP_KERNEL,
					 &urb->transfer_dma);
		if (!buf) {
			netdev_err(netdev, "No memory left for USB buffer\n");
			usb_free_urb(urb);
			err = -ENOMEM;
			break;
		}

		usb_fill_bulk_urb(urb, dev->udev, usb_rcvbulkpipe(dev->udev, 2),
				  buf, RX_BUFFER_SIZE,
				  ems_usb_read_bulk_callback, dev);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		usb_anchor_urb(urb, &dev->rx_submitted);

		err = usb_submit_urb(urb, GFP_KERNEL);
		if (err) {
			usb_unanchor_urb(urb);
			usb_free_coherent(dev->udev, RX_BUFFER_SIZE, buf,
					  urb->transfer_dma);
			break;
		}

		
		usb_free_urb(urb);
	}

	
	if (i == 0) {
		netdev_warn(netdev, "couldn't setup read URBs\n");
		return err;
	}

	
	if (i < MAX_RX_URBS)
		netdev_warn(netdev, "rx performance may be slow\n");

	
	usb_fill_int_urb(dev->intr_urb, dev->udev,
			 usb_rcvintpipe(dev->udev, 1),
			 dev->intr_in_buffer,
			 INTR_IN_BUFFER_SIZE,
			 ems_usb_read_interrupt_callback, dev, 1);

	err = usb_submit_urb(dev->intr_urb, GFP_KERNEL);
	if (err) {
		netdev_warn(netdev, "intr URB submit failed: %d\n", err);

		return err;
	}

	
	err = ems_usb_control_cmd(dev, CONTR_CAN_MESSAGE | CONTR_CONT_ON);
	if (err)
		goto failed;

	
	err = ems_usb_control_cmd(dev, CONTR_CAN_STATE | CONTR_CONT_ON);
	if (err)
		goto failed;

	
	err = ems_usb_control_cmd(dev, CONTR_BUS_ERROR | CONTR_CONT_ON);
	if (err)
		goto failed;

	err = ems_usb_write_mode(dev, SJA1000_MOD_NORMAL);
	if (err)
		goto failed;

	dev->can.state = CAN_STATE_ERROR_ACTIVE;

	return 0;

failed:
	netdev_warn(netdev, "couldn't submit control: %d\n", err);

	return err;
}

static void unlink_all_urbs(struct ems_usb *dev)
{
	int i;

	usb_unlink_urb(dev->intr_urb);

	usb_kill_anchored_urbs(&dev->rx_submitted);

	usb_kill_anchored_urbs(&dev->tx_submitted);
	atomic_set(&dev->active_tx_urbs, 0);

	for (i = 0; i < MAX_TX_URBS; i++)
		dev->tx_contexts[i].echo_index = MAX_TX_URBS;
}

static int ems_usb_open(struct net_device *netdev)
{
	struct ems_usb *dev = netdev_priv(netdev);
	int err;

	err = ems_usb_write_mode(dev, SJA1000_MOD_RM);
	if (err)
		return err;

	
	err = open_candev(netdev);
	if (err)
		return err;

	
	err = ems_usb_start(dev);
	if (err) {
		if (err == -ENODEV)
			netif_device_detach(dev->netdev);

		netdev_warn(netdev, "couldn't start device: %d\n", err);

		close_candev(netdev);

		return err;
	}

	dev->open_time = jiffies;

	netif_start_queue(netdev);

	return 0;
}

static netdev_tx_t ems_usb_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct ems_usb *dev = netdev_priv(netdev);
	struct ems_tx_urb_context *context = NULL;
	struct net_device_stats *stats = &netdev->stats;
	struct can_frame *cf = (struct can_frame *)skb->data;
	struct ems_cpc_msg *msg;
	struct urb *urb;
	u8 *buf;
	int i, err;
	size_t size = CPC_HEADER_SIZE + CPC_MSG_HEADER_LEN
			+ sizeof(struct cpc_can_msg);

	if (can_dropped_invalid_skb(netdev, skb))
		return NETDEV_TX_OK;

	
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		netdev_err(netdev, "No memory left for URBs\n");
		goto nomem;
	}

	buf = usb_alloc_coherent(dev->udev, size, GFP_ATOMIC, &urb->transfer_dma);
	if (!buf) {
		netdev_err(netdev, "No memory left for USB buffer\n");
		usb_free_urb(urb);
		goto nomem;
	}

	msg = (struct ems_cpc_msg *)&buf[CPC_HEADER_SIZE];

	msg->msg.can_msg.id = cf->can_id & CAN_ERR_MASK;
	msg->msg.can_msg.length = cf->can_dlc;

	if (cf->can_id & CAN_RTR_FLAG) {
		msg->type = cf->can_id & CAN_EFF_FLAG ?
			CPC_CMD_TYPE_EXT_RTR_FRAME : CPC_CMD_TYPE_RTR_FRAME;

		msg->length = CPC_CAN_MSG_MIN_SIZE;
	} else {
		msg->type = cf->can_id & CAN_EFF_FLAG ?
			CPC_CMD_TYPE_EXT_CAN_FRAME : CPC_CMD_TYPE_CAN_FRAME;

		for (i = 0; i < cf->can_dlc; i++)
			msg->msg.can_msg.msg[i] = cf->data[i];

		msg->length = CPC_CAN_MSG_MIN_SIZE + cf->can_dlc;
	}

	
	msg->msg.can_msg.id = cpu_to_le32(msg->msg.can_msg.id);

	for (i = 0; i < MAX_TX_URBS; i++) {
		if (dev->tx_contexts[i].echo_index == MAX_TX_URBS) {
			context = &dev->tx_contexts[i];
			break;
		}
	}

	if (!context) {
		usb_unanchor_urb(urb);
		usb_free_coherent(dev->udev, size, buf, urb->transfer_dma);

		netdev_warn(netdev, "couldn't find free context\n");

		return NETDEV_TX_BUSY;
	}

	context->dev = dev;
	context->echo_index = i;
	context->dlc = cf->can_dlc;

	usb_fill_bulk_urb(urb, dev->udev, usb_sndbulkpipe(dev->udev, 2), buf,
			  size, ems_usb_write_bulk_callback, context);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	usb_anchor_urb(urb, &dev->tx_submitted);

	can_put_echo_skb(skb, netdev, context->echo_index);

	atomic_inc(&dev->active_tx_urbs);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (unlikely(err)) {
		can_free_echo_skb(netdev, context->echo_index);

		usb_unanchor_urb(urb);
		usb_free_coherent(dev->udev, size, buf, urb->transfer_dma);
		dev_kfree_skb(skb);

		atomic_dec(&dev->active_tx_urbs);

		if (err == -ENODEV) {
			netif_device_detach(netdev);
		} else {
			netdev_warn(netdev, "failed tx_urb %d\n", err);

			stats->tx_dropped++;
		}
	} else {
		netdev->trans_start = jiffies;

		
		if (atomic_read(&dev->active_tx_urbs) >= MAX_TX_URBS ||
		    dev->free_slots < 5) {
			netif_stop_queue(netdev);
		}
	}

	usb_free_urb(urb);

	return NETDEV_TX_OK;

nomem:
	dev_kfree_skb(skb);
	stats->tx_dropped++;

	return NETDEV_TX_OK;
}

static int ems_usb_close(struct net_device *netdev)
{
	struct ems_usb *dev = netdev_priv(netdev);

	
	unlink_all_urbs(dev);

	netif_stop_queue(netdev);

	
	if (ems_usb_write_mode(dev, SJA1000_MOD_RM))
		netdev_warn(netdev, "couldn't stop device");

	close_candev(netdev);

	dev->open_time = 0;

	return 0;
}

static const struct net_device_ops ems_usb_netdev_ops = {
	.ndo_open = ems_usb_open,
	.ndo_stop = ems_usb_close,
	.ndo_start_xmit = ems_usb_start_xmit,
};

static struct can_bittiming_const ems_usb_bittiming_const = {
	.name = "ems_usb",
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

static int ems_usb_set_mode(struct net_device *netdev, enum can_mode mode)
{
	struct ems_usb *dev = netdev_priv(netdev);

	if (!dev->open_time)
		return -EINVAL;

	switch (mode) {
	case CAN_MODE_START:
		if (ems_usb_write_mode(dev, SJA1000_MOD_NORMAL))
			netdev_warn(netdev, "couldn't start device");

		if (netif_queue_stopped(netdev))
			netif_wake_queue(netdev);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int ems_usb_set_bittiming(struct net_device *netdev)
{
	struct ems_usb *dev = netdev_priv(netdev);
	struct can_bittiming *bt = &dev->can.bittiming;
	u8 btr0, btr1;

	btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
	btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) |
		(((bt->phase_seg2 - 1) & 0x7) << 4);
	if (dev->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		btr1 |= 0x80;

	netdev_info(netdev, "setting BTR0=0x%02x BTR1=0x%02x\n", btr0, btr1);

	dev->active_params.msg.can_params.cc_params.sja1000.btr0 = btr0;
	dev->active_params.msg.can_params.cc_params.sja1000.btr1 = btr1;

	return ems_usb_command_msg(dev, &dev->active_params);
}

static void init_params_sja1000(struct ems_cpc_msg *msg)
{
	struct cpc_sja1000_params *sja1000 =
		&msg->msg.can_params.cc_params.sja1000;

	msg->type = CPC_CMD_TYPE_CAN_PARAMS;
	msg->length = sizeof(struct cpc_can_params);
	msg->msgid = 0;

	msg->msg.can_params.cc_type = CPC_CC_TYPE_SJA1000;

	
	sja1000->acc_code0 = 0x00;
	sja1000->acc_code1 = 0x00;
	sja1000->acc_code2 = 0x00;
	sja1000->acc_code3 = 0x00;

	
	sja1000->acc_mask0 = 0xFF;
	sja1000->acc_mask1 = 0xFF;
	sja1000->acc_mask2 = 0xFF;
	sja1000->acc_mask3 = 0xFF;

	sja1000->btr0 = 0;
	sja1000->btr1 = 0;

	sja1000->outp_contr = SJA1000_DEFAULT_OUTPUT_CONTROL;
	sja1000->mode = SJA1000_MOD_RM;
}

static int ems_usb_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
{
	struct net_device *netdev;
	struct ems_usb *dev;
	int i, err = -ENOMEM;

	netdev = alloc_candev(sizeof(struct ems_usb), MAX_TX_URBS);
	if (!netdev) {
		dev_err(&intf->dev, "ems_usb: Couldn't alloc candev\n");
		return -ENOMEM;
	}

	dev = netdev_priv(netdev);

	dev->udev = interface_to_usbdev(intf);
	dev->netdev = netdev;

	dev->can.state = CAN_STATE_STOPPED;
	dev->can.clock.freq = EMS_USB_ARM7_CLOCK;
	dev->can.bittiming_const = &ems_usb_bittiming_const;
	dev->can.do_set_bittiming = ems_usb_set_bittiming;
	dev->can.do_set_mode = ems_usb_set_mode;
	dev->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES;

	netdev->netdev_ops = &ems_usb_netdev_ops;

	netdev->flags |= IFF_ECHO; 

	init_usb_anchor(&dev->rx_submitted);

	init_usb_anchor(&dev->tx_submitted);
	atomic_set(&dev->active_tx_urbs, 0);

	for (i = 0; i < MAX_TX_URBS; i++)
		dev->tx_contexts[i].echo_index = MAX_TX_URBS;

	dev->intr_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->intr_urb) {
		dev_err(&intf->dev, "Couldn't alloc intr URB\n");
		goto cleanup_candev;
	}

	dev->intr_in_buffer = kzalloc(INTR_IN_BUFFER_SIZE, GFP_KERNEL);
	if (!dev->intr_in_buffer) {
		dev_err(&intf->dev, "Couldn't alloc Intr buffer\n");
		goto cleanup_intr_urb;
	}

	dev->tx_msg_buffer = kzalloc(CPC_HEADER_SIZE +
				     sizeof(struct ems_cpc_msg), GFP_KERNEL);
	if (!dev->tx_msg_buffer) {
		dev_err(&intf->dev, "Couldn't alloc Tx buffer\n");
		goto cleanup_intr_in_buffer;
	}

	usb_set_intfdata(intf, dev);

	SET_NETDEV_DEV(netdev, &intf->dev);

	init_params_sja1000(&dev->active_params);

	err = ems_usb_command_msg(dev, &dev->active_params);
	if (err) {
		netdev_err(netdev, "couldn't initialize controller: %d\n", err);
		goto cleanup_tx_msg_buffer;
	}

	err = register_candev(netdev);
	if (err) {
		netdev_err(netdev, "couldn't register CAN device: %d\n", err);
		goto cleanup_tx_msg_buffer;
	}

	return 0;

cleanup_tx_msg_buffer:
	kfree(dev->tx_msg_buffer);

cleanup_intr_in_buffer:
	kfree(dev->intr_in_buffer);

cleanup_intr_urb:
	usb_free_urb(dev->intr_urb);

cleanup_candev:
	free_candev(netdev);

	return err;
}

static void ems_usb_disconnect(struct usb_interface *intf)
{
	struct ems_usb *dev = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	if (dev) {
		unregister_netdev(dev->netdev);
		free_candev(dev->netdev);

		unlink_all_urbs(dev);

		usb_free_urb(dev->intr_urb);

		kfree(dev->intr_in_buffer);
	}
}

static struct usb_driver ems_usb_driver = {
	.name = "ems_usb",
	.probe = ems_usb_probe,
	.disconnect = ems_usb_disconnect,
	.id_table = ems_usb_table,
};

module_usb_driver(ems_usb_driver);
