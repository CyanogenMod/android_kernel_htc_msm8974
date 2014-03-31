/*
 * c67x00-sched.c: Cypress C67X00 USB Host Controller Driver - TD scheduling
 *
 * Copyright (C) 2006-2008 Barco N.V.
 *    Derived from the Cypress cy7c67200/300 ezusb linux driver and
 *    based on multiple host controller drivers inside the linux kernel.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA.
 */

#include <linux/kthread.h>
#include <linux/slab.h>

#include "c67x00.h"
#include "c67x00-hcd.h"

#define SETUP_STAGE		0
#define DATA_STAGE		1
#define STATUS_STAGE		2


struct c67x00_ep_data {
	struct list_head queue;
	struct list_head node;
	struct usb_host_endpoint *hep;
	struct usb_device *dev;
	u16 next_frame;		
};

struct c67x00_td {
	
	__le16 ly_base_addr;	
	__le16 port_length;	
	u8 pid_ep;		
	u8 dev_addr;		
	u8 ctrl_reg;		
	u8 status;		
	u8 retry_cnt;		
#define TT_OFFSET		2
#define TT_CONTROL		0
#define TT_ISOCHRONOUS		1
#define TT_BULK			2
#define TT_INTERRUPT		3
	u8 residue;		
	__le16 next_td_addr;	
	
	struct list_head td_list;
	u16 td_addr;
	void *data;
	struct urb *urb;
	unsigned long privdata;

	struct c67x00_ep_data *ep_data;
	unsigned int pipe;
};

struct c67x00_urb_priv {
	struct list_head hep_node;
	struct urb *urb;
	int port;
	int cnt;		
	int status;
	struct c67x00_ep_data *ep_data;
};

#define td_udev(td)	((td)->ep_data->dev)

#define CY_TD_SIZE		12

#define TD_PIDEP_OFFSET		0x04
#define TD_PIDEPMASK_PID	0xF0
#define TD_PIDEPMASK_EP		0x0F
#define TD_PORTLENMASK_DL	0x02FF
#define TD_PORTLENMASK_PN	0xC000

#define TD_STATUS_OFFSET	0x07
#define TD_STATUSMASK_ACK	0x01
#define TD_STATUSMASK_ERR	0x02
#define TD_STATUSMASK_TMOUT	0x04
#define TD_STATUSMASK_SEQ	0x08
#define TD_STATUSMASK_SETUP	0x10
#define TD_STATUSMASK_OVF	0x20
#define TD_STATUSMASK_NAK	0x40
#define TD_STATUSMASK_STALL	0x80

#define TD_ERROR_MASK		(TD_STATUSMASK_ERR | TD_STATUSMASK_TMOUT | \
				 TD_STATUSMASK_STALL)

#define TD_RETRYCNT_OFFSET	0x08
#define TD_RETRYCNTMASK_ACT_FLG	0x10
#define TD_RETRYCNTMASK_TX_TYPE	0x0C
#define TD_RETRYCNTMASK_RTY_CNT	0x03

#define TD_RESIDUE_OVERFLOW	0x80

#define TD_PID_IN		0x90

#define td_residue(td)		((__s8)(td->residue))
#define td_ly_base_addr(td)	(__le16_to_cpu((td)->ly_base_addr))
#define td_port_length(td)	(__le16_to_cpu((td)->port_length))
#define td_next_td_addr(td)	(__le16_to_cpu((td)->next_td_addr))

#define td_active(td)		((td)->retry_cnt & TD_RETRYCNTMASK_ACT_FLG)
#define td_length(td)		(td_port_length(td) & TD_PORTLENMASK_DL)

#define td_sequence_ok(td)	(!td->status || \
				 (!(td->status & TD_STATUSMASK_SEQ) ==	\
				  !(td->ctrl_reg & SEQ_SEL)))

#define td_acked(td)		(!td->status || \
				 (td->status & TD_STATUSMASK_ACK))
#define td_actual_bytes(td)	(td_length(td) - td_residue(td))


#ifdef DEBUG

static void dbg_td(struct c67x00_hcd *c67x00, struct c67x00_td *td, char *msg)
{
	struct device *dev = c67x00_hcd_dev(c67x00);

	dev_dbg(dev, "### %s at 0x%04x\n", msg, td->td_addr);
	dev_dbg(dev, "urb:      0x%p\n", td->urb);
	dev_dbg(dev, "endpoint:   %4d\n", usb_pipeendpoint(td->pipe));
	dev_dbg(dev, "pipeout:    %4d\n", usb_pipeout(td->pipe));
	dev_dbg(dev, "ly_base_addr: 0x%04x\n", td_ly_base_addr(td));
	dev_dbg(dev, "port_length:  0x%04x\n", td_port_length(td));
	dev_dbg(dev, "pid_ep:         0x%02x\n", td->pid_ep);
	dev_dbg(dev, "dev_addr:       0x%02x\n", td->dev_addr);
	dev_dbg(dev, "ctrl_reg:       0x%02x\n", td->ctrl_reg);
	dev_dbg(dev, "status:         0x%02x\n", td->status);
	dev_dbg(dev, "retry_cnt:      0x%02x\n", td->retry_cnt);
	dev_dbg(dev, "residue:        0x%02x\n", td->residue);
	dev_dbg(dev, "next_td_addr: 0x%04x\n", td_next_td_addr(td));
	dev_dbg(dev, "data:");
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1,
		       td->data, td_length(td), 1);
}
#else				

static inline void
dbg_td(struct c67x00_hcd *c67x00, struct c67x00_td *td, char *msg) { }

#endif				


static inline u16 c67x00_get_current_frame_number(struct c67x00_hcd *c67x00)
{
	return c67x00_ll_husb_get_frame(c67x00->sie) & HOST_FRAME_MASK;
}

static inline u16 frame_add(u16 a, u16 b)
{
	return (a + b) & HOST_FRAME_MASK;
}

static inline int frame_after(u16 a, u16 b)
{
	return ((HOST_FRAME_MASK + a - b) & HOST_FRAME_MASK) <
	    (HOST_FRAME_MASK / 2);
}

static inline int frame_after_eq(u16 a, u16 b)
{
	return ((HOST_FRAME_MASK + 1 + a - b) & HOST_FRAME_MASK) <
	    (HOST_FRAME_MASK / 2);
}


static void c67x00_release_urb(struct c67x00_hcd *c67x00, struct urb *urb)
{
	struct c67x00_td *td;
	struct c67x00_urb_priv *urbp;

	BUG_ON(!urb);

	c67x00->urb_count--;

	if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
		c67x00->urb_iso_count--;
		if (c67x00->urb_iso_count == 0)
			c67x00->max_frame_bw = MAX_FRAME_BW_STD;
	}

	list_for_each_entry(td, &c67x00->td_list, td_list)
		if (urb == td->urb)
			td->urb = NULL;

	urbp = urb->hcpriv;
	urb->hcpriv = NULL;
	list_del(&urbp->hep_node);
	kfree(urbp);
}


static struct c67x00_ep_data *
c67x00_ep_data_alloc(struct c67x00_hcd *c67x00, struct urb *urb)
{
	struct usb_host_endpoint *hep = urb->ep;
	struct c67x00_ep_data *ep_data;
	int type;

	c67x00->current_frame = c67x00_get_current_frame_number(c67x00);

	
	if (hep->hcpriv) {
		ep_data = hep->hcpriv;
		if (frame_after(c67x00->current_frame, ep_data->next_frame))
			ep_data->next_frame =
			    frame_add(c67x00->current_frame, 1);
		return hep->hcpriv;
	}

	
	ep_data = kzalloc(sizeof(*ep_data), GFP_ATOMIC);
	if (!ep_data)
		return NULL;

	INIT_LIST_HEAD(&ep_data->queue);
	INIT_LIST_HEAD(&ep_data->node);
	ep_data->hep = hep;

	ep_data->dev = usb_get_dev(urb->dev);
	hep->hcpriv = ep_data;

	
	ep_data->next_frame = frame_add(c67x00->current_frame, 1);

	type = usb_pipetype(urb->pipe);
	if (list_empty(&ep_data->node)) {
		list_add(&ep_data->node, &c67x00->list[type]);
	} else {
		struct c67x00_ep_data *prev;

		list_for_each_entry(prev, &c67x00->list[type], node) {
			if (prev->hep->desc.bEndpointAddress >
			    hep->desc.bEndpointAddress) {
				list_add(&ep_data->node, prev->node.prev);
				break;
			}
		}
	}

	return ep_data;
}

static int c67x00_ep_data_free(struct usb_host_endpoint *hep)
{
	struct c67x00_ep_data *ep_data = hep->hcpriv;

	if (!ep_data)
		return 0;

	if (!list_empty(&ep_data->queue))
		return -EBUSY;

	usb_put_dev(ep_data->dev);
	list_del(&ep_data->queue);
	list_del(&ep_data->node);

	kfree(ep_data);
	hep->hcpriv = NULL;

	return 0;
}

void c67x00_endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
	struct c67x00_hcd *c67x00 = hcd_to_c67x00_hcd(hcd);
	unsigned long flags;

	if (!list_empty(&ep->urb_list))
		dev_warn(c67x00_hcd_dev(c67x00), "error: urb list not empty\n");

	spin_lock_irqsave(&c67x00->lock, flags);

	
	while (c67x00_ep_data_free(ep)) {
		
		spin_unlock_irqrestore(&c67x00->lock, flags);

		INIT_COMPLETION(c67x00->endpoint_disable);
		c67x00_sched_kick(c67x00);
		wait_for_completion_timeout(&c67x00->endpoint_disable, 1 * HZ);

		spin_lock_irqsave(&c67x00->lock, flags);
	}

	spin_unlock_irqrestore(&c67x00->lock, flags);
}


static inline int get_root_port(struct usb_device *dev)
{
	while (dev->parent->parent)
		dev = dev->parent;
	return dev->portnum;
}

int c67x00_urb_enqueue(struct usb_hcd *hcd,
		       struct urb *urb, gfp_t mem_flags)
{
	int ret;
	unsigned long flags;
	struct c67x00_urb_priv *urbp;
	struct c67x00_hcd *c67x00 = hcd_to_c67x00_hcd(hcd);
	int port = get_root_port(urb->dev)-1;

	spin_lock_irqsave(&c67x00->lock, flags);

	
	if (!HC_IS_RUNNING(hcd->state)) {
		ret = -ENODEV;
		goto err_not_linked;
	}

	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (ret)
		goto err_not_linked;

	
	urbp = kzalloc(sizeof(*urbp), mem_flags);
	if (!urbp) {
		ret = -ENOMEM;
		goto err_urbp;
	}

	INIT_LIST_HEAD(&urbp->hep_node);
	urbp->urb = urb;
	urbp->port = port;

	urbp->ep_data = c67x00_ep_data_alloc(c67x00, urb);

	if (!urbp->ep_data) {
		ret = -ENOMEM;
		goto err_epdata;
	}


	urb->hcpriv = urbp;

	urb->actual_length = 0;	

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		urb->interval = SETUP_STAGE;
		break;
	case PIPE_INTERRUPT:
		break;
	case PIPE_BULK:
		break;
	case PIPE_ISOCHRONOUS:
		if (c67x00->urb_iso_count == 0)
			c67x00->max_frame_bw = MAX_FRAME_BW_ISO;
		c67x00->urb_iso_count++;
		
		if (list_empty(&urbp->ep_data->queue))
			urb->start_frame = urbp->ep_data->next_frame;
		else {
			
			struct urb *last_urb;

			last_urb = list_entry(urbp->ep_data->queue.prev,
					      struct c67x00_urb_priv,
					      hep_node)->urb;
			urb->start_frame =
			    frame_add(last_urb->start_frame,
				      last_urb->number_of_packets *
				      last_urb->interval);
		}
		urbp->cnt = 0;
		break;
	}

	
	list_add_tail(&urbp->hep_node, &urbp->ep_data->queue);

	
	if (!c67x00->urb_count++)
		c67x00_ll_hpi_enable_sofeop(c67x00->sie);

	c67x00_sched_kick(c67x00);
	spin_unlock_irqrestore(&c67x00->lock, flags);

	return 0;

err_epdata:
	kfree(urbp);
err_urbp:
	usb_hcd_unlink_urb_from_ep(hcd, urb);
err_not_linked:
	spin_unlock_irqrestore(&c67x00->lock, flags);

	return ret;
}

int c67x00_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct c67x00_hcd *c67x00 = hcd_to_c67x00_hcd(hcd);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&c67x00->lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc)
		goto done;

	c67x00_release_urb(c67x00, urb);
	usb_hcd_unlink_urb_from_ep(hcd, urb);

	spin_unlock(&c67x00->lock);
	usb_hcd_giveback_urb(hcd, urb, status);
	spin_lock(&c67x00->lock);

	spin_unlock_irqrestore(&c67x00->lock, flags);

	return 0;

 done:
	spin_unlock_irqrestore(&c67x00->lock, flags);
	return rc;
}


static void
c67x00_giveback_urb(struct c67x00_hcd *c67x00, struct urb *urb, int status)
{
	struct c67x00_urb_priv *urbp;

	if (!urb)
		return;

	urbp = urb->hcpriv;
	urbp->status = status;

	list_del_init(&urbp->hep_node);

	c67x00_release_urb(c67x00, urb);
	usb_hcd_unlink_urb_from_ep(c67x00_hcd_to_hcd(c67x00), urb);
	spin_unlock(&c67x00->lock);
	usb_hcd_giveback_urb(c67x00_hcd_to_hcd(c67x00), urb, urbp->status);
	spin_lock(&c67x00->lock);
}


static int c67x00_claim_frame_bw(struct c67x00_hcd *c67x00, struct urb *urb,
				 int len, int periodic)
{
	struct c67x00_urb_priv *urbp = urb->hcpriv;
	int bit_time;


	
	if (urbp->ep_data->dev->speed == USB_SPEED_LOW) {
		
		if (usb_pipein(urb->pipe))
			bit_time = 80240 + 7578*len;
		else
			bit_time = 80260 + 7467*len;
	} else {
		
		if (usb_pipeisoc(urb->pipe))
			bit_time = usb_pipein(urb->pipe) ? 9050 : 7840;
		else
			bit_time = 11250;
		bit_time += 936*len;
	}

	bit_time = ((bit_time+50) / 100) + 106;

	if (unlikely(bit_time + c67x00->bandwidth_allocated >=
		     c67x00->max_frame_bw))
		return -EMSGSIZE;

	if (unlikely(c67x00->next_td_addr + CY_TD_SIZE >=
		     c67x00->td_base_addr + SIE_TD_SIZE))
		return -EMSGSIZE;

	if (unlikely(c67x00->next_buf_addr + len >=
		     c67x00->buf_base_addr + SIE_TD_BUF_SIZE))
		return -EMSGSIZE;

	if (periodic) {
		if (unlikely(bit_time + c67x00->periodic_bw_allocated >=
			     MAX_PERIODIC_BW(c67x00->max_frame_bw)))
			return -EMSGSIZE;
		c67x00->periodic_bw_allocated += bit_time;
	}

	c67x00->bandwidth_allocated += bit_time;
	return 0;
}


static int c67x00_create_td(struct c67x00_hcd *c67x00, struct urb *urb,
			    void *data, int len, int pid, int toggle,
			    unsigned long privdata)
{
	struct c67x00_td *td;
	struct c67x00_urb_priv *urbp = urb->hcpriv;
	const __u8 active_flag = 1, retry_cnt = 1;
	__u8 cmd = 0;
	int tt = 0;

	if (c67x00_claim_frame_bw(c67x00, urb, len, usb_pipeisoc(urb->pipe)
				  || usb_pipeint(urb->pipe)))
		return -EMSGSIZE;	

	td = kzalloc(sizeof(*td), GFP_ATOMIC);
	if (!td)
		return -ENOMEM;

	td->pipe = urb->pipe;
	td->ep_data = urbp->ep_data;

	if ((td_udev(td)->speed == USB_SPEED_LOW) &&
	    !(c67x00->low_speed_ports & (1 << urbp->port)))
		cmd |= PREAMBLE_EN;

	switch (usb_pipetype(td->pipe)) {
	case PIPE_ISOCHRONOUS:
		tt = TT_ISOCHRONOUS;
		cmd |= ISO_EN;
		break;
	case PIPE_CONTROL:
		tt = TT_CONTROL;
		break;
	case PIPE_BULK:
		tt = TT_BULK;
		break;
	case PIPE_INTERRUPT:
		tt = TT_INTERRUPT;
		break;
	}

	if (toggle)
		cmd |= SEQ_SEL;

	cmd |= ARM_EN;

	
	td->td_addr = c67x00->next_td_addr;
	c67x00->next_td_addr = c67x00->next_td_addr + CY_TD_SIZE;

	
	td->ly_base_addr = __cpu_to_le16(c67x00->next_buf_addr);
	td->port_length = __cpu_to_le16((c67x00->sie->sie_num << 15) |
					(urbp->port << 14) | (len & 0x3FF));
	td->pid_ep = ((pid & 0xF) << TD_PIDEP_OFFSET) |
	    (usb_pipeendpoint(td->pipe) & 0xF);
	td->dev_addr = usb_pipedevice(td->pipe) & 0x7F;
	td->ctrl_reg = cmd;
	td->status = 0;
	td->retry_cnt = (tt << TT_OFFSET) | (active_flag << 4) | retry_cnt;
	td->residue = 0;
	td->next_td_addr = __cpu_to_le16(c67x00->next_td_addr);

	
	td->data = data;
	td->urb = urb;
	td->privdata = privdata;

	c67x00->next_buf_addr += (len + 1) & ~0x01;	

	list_add_tail(&td->td_list, &c67x00->td_list);
	return 0;
}

static inline void c67x00_release_td(struct c67x00_td *td)
{
	list_del_init(&td->td_list);
	kfree(td);
}


static int c67x00_add_data_urb(struct c67x00_hcd *c67x00, struct urb *urb)
{
	int remaining;
	int toggle;
	int pid;
	int ret = 0;
	int maxps;
	int need_empty;

	toggle = usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			       usb_pipeout(urb->pipe));
	remaining = urb->transfer_buffer_length - urb->actual_length;

	maxps = usb_maxpacket(urb->dev, urb->pipe, usb_pipeout(urb->pipe));

	need_empty = (urb->transfer_flags & URB_ZERO_PACKET) &&
	    usb_pipeout(urb->pipe) && !(remaining % maxps);

	while (remaining || need_empty) {
		int len;
		char *td_buf;

		len = (remaining > maxps) ? maxps : remaining;
		if (!len)
			need_empty = 0;

		pid = usb_pipeout(urb->pipe) ? USB_PID_OUT : USB_PID_IN;
		td_buf = urb->transfer_buffer + urb->transfer_buffer_length -
		    remaining;
		ret = c67x00_create_td(c67x00, urb, td_buf, len, pid, toggle,
				       DATA_STAGE);
		if (ret)
			return ret;	

		toggle ^= 1;
		remaining -= len;
		if (usb_pipecontrol(urb->pipe))
			break;
	}

	return 0;
}

static int c67x00_add_ctrl_urb(struct c67x00_hcd *c67x00, struct urb *urb)
{
	int ret;
	int pid;

	switch (urb->interval) {
	default:
	case SETUP_STAGE:
		ret = c67x00_create_td(c67x00, urb, urb->setup_packet,
				       8, USB_PID_SETUP, 0, SETUP_STAGE);
		if (ret)
			return ret;
		urb->interval = SETUP_STAGE;
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			      usb_pipeout(urb->pipe), 1);
		break;
	case DATA_STAGE:
		if (urb->transfer_buffer_length) {
			ret = c67x00_add_data_urb(c67x00, urb);
			if (ret)
				return ret;
			break;
		}		
	case STATUS_STAGE:
		pid = !usb_pipeout(urb->pipe) ? USB_PID_OUT : USB_PID_IN;
		ret = c67x00_create_td(c67x00, urb, NULL, 0, pid, 1,
				       STATUS_STAGE);
		if (ret)
			return ret;
		break;
	}

	return 0;
}

static int c67x00_add_int_urb(struct c67x00_hcd *c67x00, struct urb *urb)
{
	struct c67x00_urb_priv *urbp = urb->hcpriv;

	if (frame_after_eq(c67x00->current_frame, urbp->ep_data->next_frame)) {
		urbp->ep_data->next_frame =
		    frame_add(urbp->ep_data->next_frame, urb->interval);
		return c67x00_add_data_urb(c67x00, urb);
	}
	return 0;
}

static int c67x00_add_iso_urb(struct c67x00_hcd *c67x00, struct urb *urb)
{
	struct c67x00_urb_priv *urbp = urb->hcpriv;

	if (frame_after_eq(c67x00->current_frame, urbp->ep_data->next_frame)) {
		char *td_buf;
		int len, pid, ret;

		BUG_ON(urbp->cnt >= urb->number_of_packets);

		td_buf = urb->transfer_buffer +
		    urb->iso_frame_desc[urbp->cnt].offset;
		len = urb->iso_frame_desc[urbp->cnt].length;
		pid = usb_pipeout(urb->pipe) ? USB_PID_OUT : USB_PID_IN;

		ret = c67x00_create_td(c67x00, urb, td_buf, len, pid, 0,
				       urbp->cnt);
		if (ret) {
			printk(KERN_DEBUG "create failed: %d\n", ret);
			urb->iso_frame_desc[urbp->cnt].actual_length = 0;
			urb->iso_frame_desc[urbp->cnt].status = ret;
			if (urbp->cnt + 1 == urb->number_of_packets)
				c67x00_giveback_urb(c67x00, urb, 0);
		}

		urbp->ep_data->next_frame =
		    frame_add(urbp->ep_data->next_frame, urb->interval);
		urbp->cnt++;
	}
	return 0;
}


static void c67x00_fill_from_list(struct c67x00_hcd *c67x00, int type,
				  int (*add)(struct c67x00_hcd *, struct urb *))
{
	struct c67x00_ep_data *ep_data;
	struct urb *urb;

	
	list_for_each_entry(ep_data, &c67x00->list[type], node) {
		if (!list_empty(&ep_data->queue)) {
			
			
			urb = list_entry(ep_data->queue.next,
					 struct c67x00_urb_priv,
					 hep_node)->urb;
			add(c67x00, urb);
		}
	}
}

static void c67x00_fill_frame(struct c67x00_hcd *c67x00)
{
	struct c67x00_td *td, *ttd;

	
	if (!list_empty(&c67x00->td_list)) {
		dev_warn(c67x00_hcd_dev(c67x00),
			 "TD list not empty! This should not happen!\n");
		list_for_each_entry_safe(td, ttd, &c67x00->td_list, td_list) {
			dbg_td(c67x00, td, "Unprocessed td");
			c67x00_release_td(td);
		}
	}

	
	c67x00->bandwidth_allocated = 0;
	c67x00->periodic_bw_allocated = 0;

	c67x00->next_td_addr = c67x00->td_base_addr;
	c67x00->next_buf_addr = c67x00->buf_base_addr;

	
	c67x00_fill_from_list(c67x00, PIPE_ISOCHRONOUS, c67x00_add_iso_urb);
	c67x00_fill_from_list(c67x00, PIPE_INTERRUPT, c67x00_add_int_urb);
	c67x00_fill_from_list(c67x00, PIPE_CONTROL, c67x00_add_ctrl_urb);
	c67x00_fill_from_list(c67x00, PIPE_BULK, c67x00_add_data_urb);
}


static inline void
c67x00_parse_td(struct c67x00_hcd *c67x00, struct c67x00_td *td)
{
	c67x00_ll_read_mem_le16(c67x00->sie->dev,
				td->td_addr, td, CY_TD_SIZE);

	if (usb_pipein(td->pipe) && td_actual_bytes(td))
		c67x00_ll_read_mem_le16(c67x00->sie->dev, td_ly_base_addr(td),
					td->data, td_actual_bytes(td));
}

static int c67x00_td_to_error(struct c67x00_hcd *c67x00, struct c67x00_td *td)
{
	if (td->status & TD_STATUSMASK_ERR) {
		dbg_td(c67x00, td, "ERROR_FLAG");
		return -EILSEQ;
	}
	if (td->status & TD_STATUSMASK_STALL) {
		
		return -EPIPE;
	}
	if (td->status & TD_STATUSMASK_TMOUT) {
		dbg_td(c67x00, td, "TIMEOUT");
		return -ETIMEDOUT;
	}

	return 0;
}

static inline int c67x00_end_of_data(struct c67x00_td *td)
{
	int maxps, need_empty, remaining;
	struct urb *urb = td->urb;
	int act_bytes;

	act_bytes = td_actual_bytes(td);

	if (unlikely(!act_bytes))
		return 1;	

	maxps = usb_maxpacket(td_udev(td), td->pipe, usb_pipeout(td->pipe));

	if (unlikely(act_bytes < maxps))
		return 1;	

	remaining = urb->transfer_buffer_length - urb->actual_length;
	need_empty = (urb->transfer_flags & URB_ZERO_PACKET) &&
	    usb_pipeout(urb->pipe) && !(remaining % maxps);

	if (unlikely(!remaining && !need_empty))
		return 1;

	return 0;
}


static inline void c67x00_clear_pipe(struct c67x00_hcd *c67x00,
				     struct c67x00_td *last_td)
{
	struct c67x00_td *td, *tmp;
	td = last_td;
	tmp = last_td;
	while (td->td_list.next != &c67x00->td_list) {
		td = list_entry(td->td_list.next, struct c67x00_td, td_list);
		if (td->pipe == last_td->pipe) {
			c67x00_release_td(td);
			td = tmp;
		}
		tmp = td;
	}
}


static void c67x00_handle_successful_td(struct c67x00_hcd *c67x00,
					struct c67x00_td *td)
{
	struct urb *urb = td->urb;

	if (!urb)
		return;

	urb->actual_length += td_actual_bytes(td);

	switch (usb_pipetype(td->pipe)) {
		
	case PIPE_CONTROL:
		switch (td->privdata) {
		case SETUP_STAGE:
			urb->interval =
			    urb->transfer_buffer_length ?
			    DATA_STAGE : STATUS_STAGE;
			
			urb->actual_length = 0;
			break;

		case DATA_STAGE:
			if (c67x00_end_of_data(td)) {
				urb->interval = STATUS_STAGE;
				c67x00_clear_pipe(c67x00, td);
			}
			break;

		case STATUS_STAGE:
			urb->interval = 0;
			c67x00_giveback_urb(c67x00, urb, 0);
			break;
		}
		break;

	case PIPE_INTERRUPT:
	case PIPE_BULK:
		if (unlikely(c67x00_end_of_data(td))) {
			c67x00_clear_pipe(c67x00, td);
			c67x00_giveback_urb(c67x00, urb, 0);
		}
		break;
	}
}

static void c67x00_handle_isoc(struct c67x00_hcd *c67x00, struct c67x00_td *td)
{
	struct urb *urb = td->urb;
	struct c67x00_urb_priv *urbp;
	int cnt;

	if (!urb)
		return;

	urbp = urb->hcpriv;
	cnt = td->privdata;

	if (td->status & TD_ERROR_MASK)
		urb->error_count++;

	urb->iso_frame_desc[cnt].actual_length = td_actual_bytes(td);
	urb->iso_frame_desc[cnt].status = c67x00_td_to_error(c67x00, td);
	if (cnt + 1 == urb->number_of_packets)	
		c67x00_giveback_urb(c67x00, urb, 0);
}


static inline void c67x00_check_td_list(struct c67x00_hcd *c67x00)
{
	struct c67x00_td *td, *tmp;
	struct urb *urb;
	int ack_ok;
	int clear_endpoint;

	list_for_each_entry_safe(td, tmp, &c67x00->td_list, td_list) {
		
		c67x00_parse_td(c67x00, td);
		urb = td->urb;	
		ack_ok = 0;
		clear_endpoint = 1;

		
		if (usb_pipeisoc(td->pipe)) {
			clear_endpoint = 0;
			c67x00_handle_isoc(c67x00, td);
			goto cont;
		}

		if (td->status & TD_ERROR_MASK) {
			c67x00_giveback_urb(c67x00, urb,
					    c67x00_td_to_error(c67x00, td));
			goto cont;
		}

		if ((td->status & TD_STATUSMASK_NAK) || !td_sequence_ok(td) ||
		    !td_acked(td))
			goto cont;

		
		ack_ok = 1;

		if (unlikely(td->status & TD_STATUSMASK_OVF)) {
			if (td_residue(td) & TD_RESIDUE_OVERFLOW) {
				
				c67x00_giveback_urb(c67x00, urb, -EOVERFLOW);
				goto cont;
			}
		}

		clear_endpoint = 0;
		c67x00_handle_successful_td(c67x00, td);

cont:
		if (clear_endpoint)
			c67x00_clear_pipe(c67x00, td);
		if (ack_ok)
			usb_settoggle(td_udev(td), usb_pipeendpoint(td->pipe),
				      usb_pipeout(td->pipe),
				      !(td->ctrl_reg & SEQ_SEL));
		
		tmp = list_entry(td->td_list.next, typeof(*td), td_list);
		c67x00_release_td(td);
	}
}


static inline int c67x00_all_tds_processed(struct c67x00_hcd *c67x00)
{
	return !c67x00_ll_husb_get_current_td(c67x00->sie);
}

static void c67x00_send_td(struct c67x00_hcd *c67x00, struct c67x00_td *td)
{
	int len = td_length(td);

	if (len && ((td->pid_ep & TD_PIDEPMASK_PID) != TD_PID_IN))
		c67x00_ll_write_mem_le16(c67x00->sie->dev, td_ly_base_addr(td),
					 td->data, len);

	c67x00_ll_write_mem_le16(c67x00->sie->dev,
				 td->td_addr, td, CY_TD_SIZE);
}

static void c67x00_send_frame(struct c67x00_hcd *c67x00)
{
	struct c67x00_td *td;

	if (list_empty(&c67x00->td_list))
		dev_warn(c67x00_hcd_dev(c67x00),
			 "%s: td list should not be empty here!\n",
			 __func__);

	list_for_each_entry(td, &c67x00->td_list, td_list) {
		if (td->td_list.next == &c67x00->td_list)
			td->next_td_addr = 0;	

		c67x00_send_td(c67x00, td);
	}

	c67x00_ll_husb_set_current_td(c67x00->sie, c67x00->td_base_addr);
}


static void c67x00_do_work(struct c67x00_hcd *c67x00)
{
	spin_lock(&c67x00->lock);
	
	if (!c67x00_all_tds_processed(c67x00))
		goto out;

	c67x00_check_td_list(c67x00);

	complete(&c67x00->endpoint_disable);

	if (!list_empty(&c67x00->td_list))
		goto out;

	c67x00->current_frame = c67x00_get_current_frame_number(c67x00);
	if (c67x00->current_frame == c67x00->last_frame)
		goto out;	
	c67x00->last_frame = c67x00->current_frame;

	
	if (!c67x00->urb_count) {
		c67x00_ll_hpi_disable_sofeop(c67x00->sie);
		goto out;
	}

	c67x00_fill_frame(c67x00);
	if (!list_empty(&c67x00->td_list))
		
		c67x00_send_frame(c67x00);

 out:
	spin_unlock(&c67x00->lock);
}


static void c67x00_sched_tasklet(unsigned long __c67x00)
{
	struct c67x00_hcd *c67x00 = (struct c67x00_hcd *)__c67x00;
	c67x00_do_work(c67x00);
}

void c67x00_sched_kick(struct c67x00_hcd *c67x00)
{
	tasklet_hi_schedule(&c67x00->tasklet);
}

int c67x00_sched_start_scheduler(struct c67x00_hcd *c67x00)
{
	tasklet_init(&c67x00->tasklet, c67x00_sched_tasklet,
		     (unsigned long)c67x00);
	return 0;
}

void c67x00_sched_stop_scheduler(struct c67x00_hcd *c67x00)
{
	tasklet_kill(&c67x00->tasklet);
}
