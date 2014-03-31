/*
 * ISP116x HCD (Host Controller Driver) for USB.
 *
 * Derived from the SL811 HCD, rewritten for ISP116x.
 * Copyright (C) 2005 Olav Kongas <ok@artecdesign.ee>
 *
 * Portions:
 * Copyright (C) 2004 Psion Teklogix (for NetBook PRO)
 * Copyright (C) 2004 David Brownell
 *
 * Periodic scheduling is based on Roman's OHCI code
 * Copyright (C) 1999 Roman Weissgaerber
 *
 */


#define USE_PLATFORM_DELAY


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/isp116x.h>
#include <linux/usb/hcd.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/byteorder.h>

#include "isp116x.h"

#define DRIVER_VERSION	"03 Nov 2005"
#define DRIVER_DESC	"ISP116x USB Host Controller Driver"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static const char hcd_name[] = "isp116x-hcd";


static void write_ptddata_to_fifo(struct isp116x *isp116x, void *buf, int len)
{
	u8 *dp = (u8 *) buf;
	u16 *dp2 = (u16 *) buf;
	u16 w;
	int quot = len % 4;

	
	
	

	if ((unsigned long)dp2 & 1) {
		
		for (; len > 1; len -= 2) {
			w = *dp++;
			w |= *dp++ << 8;
			isp116x_raw_write_data16(isp116x, w);
		}
		if (len)
			isp116x_write_data16(isp116x, (u16) * dp);
	} else {
		
		for (; len > 1; len -= 2) {
			
			isp116x_raw_write_data16(isp116x, cpu_to_le16(*dp2++));
		}

		if (len)
			isp116x_write_data16(isp116x, 0xff & *((u8 *) dp2));
	}
	if (quot == 1 || quot == 2)
		isp116x_raw_write_data16(isp116x, 0);
}

static void read_ptddata_from_fifo(struct isp116x *isp116x, void *buf, int len)
{
	u8 *dp = (u8 *) buf;
	u16 *dp2 = (u16 *) buf;
	u16 w;
	int quot = len % 4;

	
	
	

	if ((unsigned long)dp2 & 1) {
		
		for (; len > 1; len -= 2) {
			w = isp116x_raw_read_data16(isp116x);
			*dp++ = w & 0xff;
			*dp++ = (w >> 8) & 0xff;
		}

		if (len)
			*dp = 0xff & isp116x_read_data16(isp116x);
	} else {
		
		for (; len > 1; len -= 2) {
			
			*dp2++ = le16_to_cpu(isp116x_raw_read_data16(isp116x));
		}

		if (len)
			*(u8 *) dp2 = 0xff & isp116x_read_data16(isp116x);
	}
	if (quot == 1 || quot == 2)
		isp116x_raw_read_data16(isp116x);
}

static void pack_fifo(struct isp116x *isp116x)
{
	struct isp116x_ep *ep;
	struct ptd *ptd;
	int buflen = isp116x->atl_last_dir == PTD_DIR_IN
	    ? isp116x->atl_bufshrt : isp116x->atl_buflen;

	isp116x_write_reg16(isp116x, HCuPINT, HCuPINT_AIIEOT);
	isp116x_write_reg16(isp116x, HCXFERCTR, buflen);
	isp116x_write_addr(isp116x, HCATLPORT | ISP116x_WRITE_OFFSET);
	for (ep = isp116x->atl_active; ep; ep = ep->active) {
		ptd = &ep->ptd;
		dump_ptd(ptd);
		dump_ptd_out_data(ptd, ep->data);
		isp116x_write_data16(isp116x, ptd->count);
		isp116x_write_data16(isp116x, ptd->mps);
		isp116x_write_data16(isp116x, ptd->len);
		isp116x_write_data16(isp116x, ptd->faddr);
		buflen -= sizeof(struct ptd);
		
		if (ep->active || (isp116x->atl_last_dir != PTD_DIR_IN)) {
			write_ptddata_to_fifo(isp116x, ep->data, ep->length);
			buflen -= ALIGN(ep->length, 4);
		}
	}
	BUG_ON(buflen);
}

static void unpack_fifo(struct isp116x *isp116x)
{
	struct isp116x_ep *ep;
	struct ptd *ptd;
	int buflen = isp116x->atl_last_dir == PTD_DIR_IN
	    ? isp116x->atl_buflen : isp116x->atl_bufshrt;

	isp116x_write_reg16(isp116x, HCuPINT, HCuPINT_AIIEOT);
	isp116x_write_reg16(isp116x, HCXFERCTR, buflen);
	isp116x_write_addr(isp116x, HCATLPORT);
	for (ep = isp116x->atl_active; ep; ep = ep->active) {
		ptd = &ep->ptd;
		ptd->count = isp116x_read_data16(isp116x);
		ptd->mps = isp116x_read_data16(isp116x);
		ptd->len = isp116x_read_data16(isp116x);
		ptd->faddr = isp116x_read_data16(isp116x);
		buflen -= sizeof(struct ptd);
		
		if (ep->active || (isp116x->atl_last_dir == PTD_DIR_IN)) {
			read_ptddata_from_fifo(isp116x, ep->data, ep->length);
			buflen -= ALIGN(ep->length, 4);
		}
		dump_ptd(ptd);
		dump_ptd_in_data(ptd, ep->data);
	}
	BUG_ON(buflen);
}


static void preproc_atl_queue(struct isp116x *isp116x)
{
	struct isp116x_ep *ep;
	struct urb *urb;
	struct ptd *ptd;
	u16 len;

	for (ep = isp116x->atl_active; ep; ep = ep->active) {
		u16 toggle = 0, dir = PTD_DIR_SETUP;

		BUG_ON(list_empty(&ep->hep->urb_list));
		urb = container_of(ep->hep->urb_list.next,
				   struct urb, urb_list);
		ptd = &ep->ptd;
		len = ep->length;
		ep->data = (unsigned char *)urb->transfer_buffer
		    + urb->actual_length;

		switch (ep->nextpid) {
		case USB_PID_IN:
			toggle = usb_gettoggle(urb->dev, ep->epnum, 0);
			dir = PTD_DIR_IN;
			break;
		case USB_PID_OUT:
			toggle = usb_gettoggle(urb->dev, ep->epnum, 1);
			dir = PTD_DIR_OUT;
			break;
		case USB_PID_SETUP:
			len = sizeof(struct usb_ctrlrequest);
			ep->data = urb->setup_packet;
			break;
		case USB_PID_ACK:
			toggle = 1;
			len = 0;
			dir = (urb->transfer_buffer_length
			       && usb_pipein(urb->pipe))
			    ? PTD_DIR_OUT : PTD_DIR_IN;
			break;
		default:
			ERR("%s %d: ep->nextpid %d\n", __func__, __LINE__,
			    ep->nextpid);
			BUG();
		}

		ptd->count = PTD_CC_MSK | PTD_ACTIVE_MSK | PTD_TOGGLE(toggle);
		ptd->mps = PTD_MPS(ep->maxpacket)
		    | PTD_SPD(urb->dev->speed == USB_SPEED_LOW)
		    | PTD_EP(ep->epnum);
		ptd->len = PTD_LEN(len) | PTD_DIR(dir);
		ptd->faddr = PTD_FA(usb_pipedevice(urb->pipe));
		if (!ep->active) {
			ptd->mps |= PTD_LAST_MSK;
			isp116x->atl_last_dir = dir;
		}
		isp116x->atl_bufshrt = sizeof(struct ptd) + isp116x->atl_buflen;
		isp116x->atl_buflen = isp116x->atl_bufshrt + ALIGN(len, 4);
	}
}

static void finish_request(struct isp116x *isp116x, struct isp116x_ep *ep,
			   struct urb *urb, int status)
__releases(isp116x->lock) __acquires(isp116x->lock)
{
	unsigned i;

	ep->error_count = 0;

	if (usb_pipecontrol(urb->pipe))
		ep->nextpid = USB_PID_SETUP;

	urb_dbg(urb, "Finish");

	usb_hcd_unlink_urb_from_ep(isp116x_to_hcd(isp116x), urb);
	spin_unlock(&isp116x->lock);
	usb_hcd_giveback_urb(isp116x_to_hcd(isp116x), urb, status);
	spin_lock(&isp116x->lock);

	
	if (!list_empty(&ep->hep->urb_list))
		return;

	
	if (!list_empty(&ep->schedule)) {
		list_del_init(&ep->schedule);
		return;
	}

	
	DBG("deschedule qh%d/%p branch %d\n", ep->period, ep, ep->branch);
	for (i = ep->branch; i < PERIODIC_SIZE; i += ep->period) {
		struct isp116x_ep *temp;
		struct isp116x_ep **prev = &isp116x->periodic[i];

		while (*prev && ((temp = *prev) != ep))
			prev = &temp->next;
		if (*prev)
			*prev = ep->next;
		isp116x->load[i] -= ep->load;
	}
	ep->branch = PERIODIC_SIZE;
	isp116x_to_hcd(isp116x)->self.bandwidth_allocated -=
	    ep->load / ep->period;

	
	if (!--isp116x->periodic_count) {
		isp116x->irqenb &= ~HCuPINT_SOF;
		isp116x->irqenb |= HCuPINT_ATL;
	}
}

static void postproc_atl_queue(struct isp116x *isp116x)
{
	struct isp116x_ep *ep;
	struct urb *urb;
	struct usb_device *udev;
	struct ptd *ptd;
	int short_not_ok;
	int status;
	u8 cc;

	for (ep = isp116x->atl_active; ep; ep = ep->active) {
		BUG_ON(list_empty(&ep->hep->urb_list));
		urb =
		    container_of(ep->hep->urb_list.next, struct urb, urb_list);
		udev = urb->dev;
		ptd = &ep->ptd;
		cc = PTD_GET_CC(ptd);
		short_not_ok = 1;
		status = -EINPROGRESS;

		if (cc == TD_DATAUNDERRUN) {
			if (!(urb->transfer_flags & URB_SHORT_NOT_OK) ||
					usb_pipecontrol(urb->pipe)) {
				DBG("Allowed or control data underrun\n");
				cc = TD_CC_NOERROR;
				short_not_ok = 0;
			} else {
				ep->error_count = 1;
				usb_settoggle(udev, ep->epnum,
					      ep->nextpid == USB_PID_OUT,
					      PTD_GET_TOGGLE(ptd));
				urb->actual_length += PTD_GET_COUNT(ptd);
				status = cc_to_error[TD_DATAUNDERRUN];
				goto done;
			}
		}

		if (cc != TD_CC_NOERROR && cc != TD_NOTACCESSED
		    && (++ep->error_count >= 3 || cc == TD_CC_STALL
			|| cc == TD_DATAOVERRUN)) {
			status = cc_to_error[cc];
			if (ep->nextpid == USB_PID_ACK)
				ep->nextpid = 0;
			goto done;
		}
		if (usb_pipeint(urb->pipe) && !PTD_GET_LEN(ptd)) {
			status = 0;
			goto done;
		}

		if (ep->error_count
		    && (cc == TD_CC_NOERROR || cc == TD_NOTACCESSED))
			ep->error_count = 0;

		if (ep->nextpid == USB_PID_OUT)
			usb_settoggle(udev, ep->epnum, 1, PTD_GET_TOGGLE(ptd)
				      ^ (ep->error_count > 0));
		else if (ep->nextpid == USB_PID_IN)
			usb_settoggle(udev, ep->epnum, 0, PTD_GET_TOGGLE(ptd)
				      ^ (ep->error_count > 0));

		switch (ep->nextpid) {
		case USB_PID_IN:
		case USB_PID_OUT:
			urb->actual_length += PTD_GET_COUNT(ptd);
			if (PTD_GET_ACTIVE(ptd)
			    || (cc != TD_CC_NOERROR && cc < 0x0E))
				break;
			if (urb->transfer_buffer_length != urb->actual_length) {
				if (short_not_ok)
					break;
			} else {
				if (urb->transfer_flags & URB_ZERO_PACKET
				    && ep->nextpid == USB_PID_OUT
				    && !(PTD_GET_COUNT(ptd) % ep->maxpacket)) {
					DBG("Zero packet requested\n");
					break;
				}
			}
			
			if (usb_pipecontrol(urb->pipe))
				ep->nextpid = USB_PID_ACK;
			else
				status = 0;
			break;
		case USB_PID_SETUP:
			if (PTD_GET_ACTIVE(ptd)
			    || (cc != TD_CC_NOERROR && cc < 0x0E))
				break;
			if (urb->transfer_buffer_length == urb->actual_length)
				ep->nextpid = USB_PID_ACK;
			else if (usb_pipeout(urb->pipe)) {
				usb_settoggle(udev, 0, 1, 1);
				ep->nextpid = USB_PID_OUT;
			} else {
				usb_settoggle(udev, 0, 0, 1);
				ep->nextpid = USB_PID_IN;
			}
			break;
		case USB_PID_ACK:
			if (PTD_GET_ACTIVE(ptd)
			    || (cc != TD_CC_NOERROR && cc < 0x0E))
				break;
			status = 0;
			ep->nextpid = 0;
			break;
		default:
			BUG();
		}

 done:
		if (status != -EINPROGRESS || urb->unlinked)
			finish_request(isp116x, ep, urb, status);
	}
}

static void start_atl_transfers(struct isp116x *isp116x)
{
	struct isp116x_ep *last_ep = NULL, *ep;
	struct urb *urb;
	u16 load = 0;
	int len, index, speed, byte_time;

	if (atomic_read(&isp116x->atl_finishing))
		return;

	if (!HC_IS_RUNNING(isp116x_to_hcd(isp116x)->state))
		return;

	
	if (isp116x_read_reg16(isp116x, HCBUFSTAT) & HCBUFSTAT_ATL_FULL)
		return;

	isp116x->atl_active = NULL;
	isp116x->atl_buflen = isp116x->atl_bufshrt = 0;

	
	if (isp116x->periodic_count) {
		isp116x->fmindex = index =
		    (isp116x->fmindex + 1) & (PERIODIC_SIZE - 1);
		if ((load = isp116x->load[index])) {
			isp116x->atl_active = last_ep =
			    isp116x->periodic[index];
			while (last_ep->next)
				last_ep = (last_ep->active = last_ep->next);
			last_ep->active = NULL;
		}
	}

	
	list_for_each_entry(ep, &isp116x->async, schedule) {
		urb = container_of(ep->hep->urb_list.next,
				   struct urb, urb_list);
		speed = urb->dev->speed;
		byte_time = speed == USB_SPEED_LOW
		    ? BYTE_TIME_LOWSPEED : BYTE_TIME_FULLSPEED;

		if (ep->nextpid == USB_PID_SETUP) {
			len = sizeof(struct usb_ctrlrequest);
		} else if (ep->nextpid == USB_PID_ACK) {
			len = 0;
		} else {
			
			len = (MAX_LOAD_LIMIT - load) / byte_time;

			
			len = min(len, speed == USB_SPEED_LOW ?
				  MAX_TRANSFER_SIZE_LOWSPEED :
				  MAX_TRANSFER_SIZE_FULLSPEED);

			if (len <
			    (urb->transfer_buffer_length -
			     urb->actual_length)) {
				len -= len % ep->maxpacket;
				if (!len)
					continue;
			} else
				len = urb->transfer_buffer_length -
				    urb->actual_length;
			BUG_ON(len < 0);
		}

		load += len * byte_time;
		if (load > MAX_LOAD_LIMIT)
			break;

		ep->active = NULL;
		ep->length = len;
		if (last_ep)
			last_ep->active = ep;
		else
			isp116x->atl_active = ep;
		last_ep = ep;
	}

	
	if ((&isp116x->async)->next != (&isp116x->async)->prev)
		list_move(&isp116x->async, (&isp116x->async)->next);

	if (isp116x->atl_active) {
		preproc_atl_queue(isp116x);
		pack_fifo(isp116x);
	}
}

static void finish_atl_transfers(struct isp116x *isp116x)
{
	if (!isp116x->atl_active)
		return;
	
	if (!(isp116x_read_reg16(isp116x, HCBUFSTAT) & HCBUFSTAT_ATL_DONE))
		return;

	atomic_inc(&isp116x->atl_finishing);
	unpack_fifo(isp116x);
	postproc_atl_queue(isp116x);
	atomic_dec(&isp116x->atl_finishing);
}

static irqreturn_t isp116x_irq(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	u16 irqstat;
	irqreturn_t ret = IRQ_NONE;

	spin_lock(&isp116x->lock);
	isp116x_write_reg16(isp116x, HCuPINTENB, 0);
	irqstat = isp116x_read_reg16(isp116x, HCuPINT);
	isp116x_write_reg16(isp116x, HCuPINT, irqstat);

	if (irqstat & (HCuPINT_ATL | HCuPINT_SOF)) {
		ret = IRQ_HANDLED;
		finish_atl_transfers(isp116x);
	}

	if (irqstat & HCuPINT_OPR) {
		u32 intstat = isp116x_read_reg32(isp116x, HCINTSTAT);
		isp116x_write_reg32(isp116x, HCINTSTAT, intstat);
		if (intstat & HCINT_UE) {
			ERR("Unrecoverable error, HC is dead!\n");
			hcd->state = HC_STATE_HALT;
			usb_hc_died(hcd);
			ret = IRQ_HANDLED;
			goto done;
		}
		if (intstat & HCINT_RHSC)
			mod_timer(&hcd->rh_timer, jiffies
				  + msecs_to_jiffies(20) + 1);
		if (intstat & HCINT_RD) {
			DBG("---- remote wakeup\n");
			usb_hcd_resume_root_hub(hcd);
		}
		irqstat &= ~HCuPINT_OPR;
		ret = IRQ_HANDLED;
	}

	if (irqstat & (HCuPINT_ATL | HCuPINT_SOF)) {
		start_atl_transfers(isp116x);
	}

	isp116x_write_reg16(isp116x, HCuPINTENB, isp116x->irqenb);
      done:
	spin_unlock(&isp116x->lock);
	return ret;
}



#define	MAX_PERIODIC_LOAD	600
static int balance(struct isp116x *isp116x, u16 period, u16 load)
{
	int i, branch = -ENOSPC;

	for (i = 0; i < period; i++) {
		if (branch < 0 || isp116x->load[branch] > isp116x->load[i]) {
			int j;

			for (j = i; j < PERIODIC_SIZE; j += period) {
				if ((isp116x->load[j] + load)
				    > MAX_PERIODIC_LOAD)
					break;
			}
			if (j < PERIODIC_SIZE)
				continue;
			branch = i;
		}
	}
	return branch;
}



static int isp116x_urb_enqueue(struct usb_hcd *hcd,
			       struct urb *urb,
			       gfp_t mem_flags)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	struct usb_device *udev = urb->dev;
	unsigned int pipe = urb->pipe;
	int is_out = !usb_pipein(pipe);
	int type = usb_pipetype(pipe);
	int epnum = usb_pipeendpoint(pipe);
	struct usb_host_endpoint *hep = urb->ep;
	struct isp116x_ep *ep = NULL;
	unsigned long flags;
	int i;
	int ret = 0;

	urb_dbg(urb, "Enqueue");

	if (type == PIPE_ISOCHRONOUS) {
		ERR("Isochronous transfers not supported\n");
		urb_dbg(urb, "Refused to enqueue");
		return -ENXIO;
	}
	
	if (!hep->hcpriv) {
		ep = kzalloc(sizeof *ep, mem_flags);
		if (!ep)
			return -ENOMEM;
	}

	spin_lock_irqsave(&isp116x->lock, flags);
	if (!HC_IS_RUNNING(hcd->state)) {
		kfree(ep);
		ret = -ENODEV;
		goto fail_not_linked;
	}
	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (ret) {
		kfree(ep);
		goto fail_not_linked;
	}

	if (hep->hcpriv)
		ep = hep->hcpriv;
	else {
		INIT_LIST_HEAD(&ep->schedule);
		ep->udev = udev;
		ep->epnum = epnum;
		ep->maxpacket = usb_maxpacket(udev, urb->pipe, is_out);
		usb_settoggle(udev, epnum, is_out, 0);

		if (type == PIPE_CONTROL) {
			ep->nextpid = USB_PID_SETUP;
		} else if (is_out) {
			ep->nextpid = USB_PID_OUT;
		} else {
			ep->nextpid = USB_PID_IN;
		}

		if (urb->interval) {
			/*
			   With INT URBs submitted, the driver works with SOF
			   interrupt enabled and ATL interrupt disabled. After
			   the PTDs are written to fifo ram, the chip starts
			   fifo processing and usb transfers after the next
			   SOF and continues until the transfers are finished
			   (succeeded or failed) or the frame ends. Therefore,
			   the transfers occur only in every second frame,
			   while fifo reading/writing and data processing
			   occur in every other second frame. */
			if (urb->interval < 2)
				urb->interval = 2;
			if (urb->interval > 2 * PERIODIC_SIZE)
				urb->interval = 2 * PERIODIC_SIZE;
			ep->period = urb->interval >> 1;
			ep->branch = PERIODIC_SIZE;
			ep->load = usb_calc_bus_time(udev->speed,
						     !is_out,
						     (type == PIPE_ISOCHRONOUS),
						     usb_maxpacket(udev, pipe,
								   is_out)) /
			    1000;
		}
		hep->hcpriv = ep;
		ep->hep = hep;
	}

	
	switch (type) {
	case PIPE_CONTROL:
	case PIPE_BULK:
		if (list_empty(&ep->schedule))
			list_add_tail(&ep->schedule, &isp116x->async);
		break;
	case PIPE_INTERRUPT:
		urb->interval = ep->period;
		ep->length = min_t(u32, ep->maxpacket,
				 urb->transfer_buffer_length);

		
		if (ep->branch < PERIODIC_SIZE)
			break;

		ep->branch = ret = balance(isp116x, ep->period, ep->load);
		if (ret < 0)
			goto fail;
		ret = 0;

		urb->start_frame = (isp116x->fmindex & (PERIODIC_SIZE - 1))
		    + ep->branch;

		DBG("schedule qh%d/%p branch %d\n", ep->period, ep, ep->branch);
		for (i = ep->branch; i < PERIODIC_SIZE; i += ep->period) {
			struct isp116x_ep **prev = &isp116x->periodic[i];
			struct isp116x_ep *here = *prev;

			while (here && ep != here) {
				if (ep->period > here->period)
					break;
				prev = &here->next;
				here = *prev;
			}
			if (ep != here) {
				ep->next = here;
				*prev = ep;
			}
			isp116x->load[i] += ep->load;
		}
		hcd->self.bandwidth_allocated += ep->load / ep->period;

		
		if (!isp116x->periodic_count++) {
			isp116x->irqenb &= ~HCuPINT_ATL;
			isp116x->irqenb |= HCuPINT_SOF;
			isp116x_write_reg16(isp116x, HCuPINTENB,
					    isp116x->irqenb);
		}
	}

	urb->hcpriv = hep;
	start_atl_transfers(isp116x);

      fail:
	if (ret)
		usb_hcd_unlink_urb_from_ep(hcd, urb);
      fail_not_linked:
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return ret;
}

static int isp116x_urb_dequeue(struct usb_hcd *hcd, struct urb *urb,
		int status)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	struct usb_host_endpoint *hep;
	struct isp116x_ep *ep, *ep_act;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&isp116x->lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc)
		goto done;

	hep = urb->hcpriv;
	ep = hep->hcpriv;
	WARN_ON(hep != ep->hep);

	
	if (ep->hep->urb_list.next == &urb->urb_list)
		
		for (ep_act = isp116x->atl_active; ep_act;
		     ep_act = ep_act->active)
			if (ep_act == ep) {
				VDBG("dequeue, urb %p active; wait for irq\n",
				     urb);
				urb = NULL;
				break;
			}

	if (urb)
		finish_request(isp116x, ep, urb, status);
 done:
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return rc;
}

static void isp116x_endpoint_disable(struct usb_hcd *hcd,
				     struct usb_host_endpoint *hep)
{
	int i;
	struct isp116x_ep *ep = hep->hcpriv;

	if (!ep)
		return;

	
	for (i = 0; i < 100 && !list_empty(&hep->urb_list); i++)
		msleep(3);
	if (!list_empty(&hep->urb_list))
		WARNING("ep %p not empty?\n", ep);

	kfree(ep);
	hep->hcpriv = NULL;
}

static int isp116x_get_frame(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	u32 fmnum;
	unsigned long flags;

	spin_lock_irqsave(&isp116x->lock, flags);
	fmnum = isp116x_read_reg32(isp116x, HCFMNUM);
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return (int)fmnum;
}

static int isp116x_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	int ports, i, changed = 0;
	unsigned long flags;

	if (!HC_IS_RUNNING(hcd->state))
		return -ESHUTDOWN;

	if (timer_pending(&hcd->rh_timer))
		return 0;

	ports = isp116x->rhdesca & RH_A_NDP;
	spin_lock_irqsave(&isp116x->lock, flags);
	isp116x->rhstatus = isp116x_read_reg32(isp116x, HCRHSTATUS);
	if (isp116x->rhstatus & (RH_HS_LPSC | RH_HS_OCIC))
		buf[0] = changed = 1;
	else
		buf[0] = 0;

	for (i = 0; i < ports; i++) {
		u32 status = isp116x_read_reg32(isp116x, i ? HCRHPORT2 : HCRHPORT1);

		if (status & (RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC
			      | RH_PS_OCIC | RH_PS_PRSC)) {
			changed = 1;
			buf[0] |= 1 << (i + 1);
		}
	}
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return changed;
}

static void isp116x_hub_descriptor(struct isp116x *isp116x,
				   struct usb_hub_descriptor *desc)
{
	u32 reg = isp116x->rhdesca;

	desc->bDescriptorType = 0x29;
	desc->bDescLength = 9;
	desc->bHubContrCurrent = 0;
	desc->bNbrPorts = (u8) (reg & 0x3);
	
	desc->wHubCharacteristics = cpu_to_le16((u16) ((reg >> 8) & 0x1f));
	desc->bPwrOn2PwrGood = (u8) ((reg >> 24) & 0xff);
	
	desc->u.hs.DeviceRemovable[0] = 0;
	desc->u.hs.DeviceRemovable[1] = ~0;
}

static inline void root_port_reset(struct isp116x *isp116x, unsigned port)
{
	u32 tmp;
	unsigned long flags, t;

	t = jiffies + msecs_to_jiffies(100);

	while (time_before(jiffies, t)) {
		spin_lock_irqsave(&isp116x->lock, flags);
		
		for (;;) {
			tmp = isp116x_read_reg32(isp116x, port ?
						 HCRHPORT2 : HCRHPORT1);
			if (!(tmp & RH_PS_PRS))
				break;
			udelay(500);
		}
		
		if (!(tmp & RH_PS_CCS)) {
			spin_unlock_irqrestore(&isp116x->lock, flags);
			break;
		}
		
		isp116x_write_reg32(isp116x, port ? HCRHPORT2 :
				    HCRHPORT1, (RH_PS_PRS));
		spin_unlock_irqrestore(&isp116x->lock, flags);
		msleep(10);
	}
}

static int isp116x_hub_control(struct usb_hcd *hcd,
			       u16 typeReq,
			       u16 wValue, u16 wIndex, char *buf, u16 wLength)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	int ret = 0;
	unsigned long flags;
	int ports = isp116x->rhdesca & RH_A_NDP;
	u32 tmp = 0;

	switch (typeReq) {
	case ClearHubFeature:
		DBG("ClearHubFeature: ");
		switch (wValue) {
		case C_HUB_OVER_CURRENT:
			DBG("C_HUB_OVER_CURRENT\n");
			spin_lock_irqsave(&isp116x->lock, flags);
			isp116x_write_reg32(isp116x, HCRHSTATUS, RH_HS_OCIC);
			spin_unlock_irqrestore(&isp116x->lock, flags);
		case C_HUB_LOCAL_POWER:
			DBG("C_HUB_LOCAL_POWER\n");
			break;
		default:
			goto error;
		}
		break;
	case SetHubFeature:
		DBG("SetHubFeature: ");
		switch (wValue) {
		case C_HUB_OVER_CURRENT:
		case C_HUB_LOCAL_POWER:
			DBG("C_HUB_OVER_CURRENT or C_HUB_LOCAL_POWER\n");
			break;
		default:
			goto error;
		}
		break;
	case GetHubDescriptor:
		DBG("GetHubDescriptor\n");
		isp116x_hub_descriptor(isp116x,
				       (struct usb_hub_descriptor *)buf);
		break;
	case GetHubStatus:
		DBG("GetHubStatus\n");
		*(__le32 *) buf = 0;
		break;
	case GetPortStatus:
		DBG("GetPortStatus\n");
		if (!wIndex || wIndex > ports)
			goto error;
		spin_lock_irqsave(&isp116x->lock, flags);
		tmp = isp116x_read_reg32(isp116x, (--wIndex) ? HCRHPORT2 : HCRHPORT1);
		spin_unlock_irqrestore(&isp116x->lock, flags);
		*(__le32 *) buf = cpu_to_le32(tmp);
		DBG("GetPortStatus: port[%d]  %08x\n", wIndex + 1, tmp);
		break;
	case ClearPortFeature:
		DBG("ClearPortFeature: ");
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			DBG("USB_PORT_FEAT_ENABLE\n");
			tmp = RH_PS_CCS;
			break;
		case USB_PORT_FEAT_C_ENABLE:
			DBG("USB_PORT_FEAT_C_ENABLE\n");
			tmp = RH_PS_PESC;
			break;
		case USB_PORT_FEAT_SUSPEND:
			DBG("USB_PORT_FEAT_SUSPEND\n");
			tmp = RH_PS_POCI;
			break;
		case USB_PORT_FEAT_C_SUSPEND:
			DBG("USB_PORT_FEAT_C_SUSPEND\n");
			tmp = RH_PS_PSSC;
			break;
		case USB_PORT_FEAT_POWER:
			DBG("USB_PORT_FEAT_POWER\n");
			tmp = RH_PS_LSDA;
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			DBG("USB_PORT_FEAT_C_CONNECTION\n");
			tmp = RH_PS_CSC;
			break;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			DBG("USB_PORT_FEAT_C_OVER_CURRENT\n");
			tmp = RH_PS_OCIC;
			break;
		case USB_PORT_FEAT_C_RESET:
			DBG("USB_PORT_FEAT_C_RESET\n");
			tmp = RH_PS_PRSC;
			break;
		default:
			goto error;
		}
		spin_lock_irqsave(&isp116x->lock, flags);
		isp116x_write_reg32(isp116x, wIndex
				    ? HCRHPORT2 : HCRHPORT1, tmp);
		spin_unlock_irqrestore(&isp116x->lock, flags);
		break;
	case SetPortFeature:
		DBG("SetPortFeature: ");
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			DBG("USB_PORT_FEAT_SUSPEND\n");
			spin_lock_irqsave(&isp116x->lock, flags);
			isp116x_write_reg32(isp116x, wIndex
					    ? HCRHPORT2 : HCRHPORT1, RH_PS_PSS);
			spin_unlock_irqrestore(&isp116x->lock, flags);
			break;
		case USB_PORT_FEAT_POWER:
			DBG("USB_PORT_FEAT_POWER\n");
			spin_lock_irqsave(&isp116x->lock, flags);
			isp116x_write_reg32(isp116x, wIndex
					    ? HCRHPORT2 : HCRHPORT1, RH_PS_PPS);
			spin_unlock_irqrestore(&isp116x->lock, flags);
			break;
		case USB_PORT_FEAT_RESET:
			DBG("USB_PORT_FEAT_RESET\n");
			root_port_reset(isp116x, wIndex);
			break;
		default:
			goto error;
		}
		break;

	default:
	      error:
		
		DBG("PROTOCOL STALL\n");
		ret = -EPIPE;
	}
	return ret;
}


#ifdef CONFIG_DEBUG_FS

static void dump_irq(struct seq_file *s, char *label, u16 mask)
{
	seq_printf(s, "%s %04x%s%s%s%s%s%s\n", label, mask,
		   mask & HCuPINT_CLKRDY ? " clkrdy" : "",
		   mask & HCuPINT_SUSP ? " susp" : "",
		   mask & HCuPINT_OPR ? " opr" : "",
		   mask & HCuPINT_AIIEOT ? " eot" : "",
		   mask & HCuPINT_ATL ? " atl" : "",
		   mask & HCuPINT_SOF ? " sof" : "");
}

static void dump_int(struct seq_file *s, char *label, u32 mask)
{
	seq_printf(s, "%s %08x%s%s%s%s%s%s%s\n", label, mask,
		   mask & HCINT_MIE ? " MIE" : "",
		   mask & HCINT_RHSC ? " rhsc" : "",
		   mask & HCINT_FNO ? " fno" : "",
		   mask & HCINT_UE ? " ue" : "",
		   mask & HCINT_RD ? " rd" : "",
		   mask & HCINT_SF ? " sof" : "", mask & HCINT_SO ? " so" : "");
}

static int isp116x_show_dbg(struct seq_file *s, void *unused)
{
	struct isp116x *isp116x = s->private;

	seq_printf(s, "%s\n%s version %s\n",
		   isp116x_to_hcd(isp116x)->product_desc, hcd_name,
		   DRIVER_VERSION);

	if (HC_IS_SUSPENDED(isp116x_to_hcd(isp116x)->state)) {
		seq_printf(s, "HCD is suspended\n");
		return 0;
	}
	if (!HC_IS_RUNNING(isp116x_to_hcd(isp116x)->state)) {
		seq_printf(s, "HCD not running\n");
		return 0;
	}

	spin_lock_irq(&isp116x->lock);
	dump_irq(s, "hc_irq_enable", isp116x_read_reg16(isp116x, HCuPINTENB));
	dump_irq(s, "hc_irq_status", isp116x_read_reg16(isp116x, HCuPINT));
	dump_int(s, "hc_int_enable", isp116x_read_reg32(isp116x, HCINTENB));
	dump_int(s, "hc_int_status", isp116x_read_reg32(isp116x, HCINTSTAT));
	isp116x_show_regs_seq(isp116x, s);
	spin_unlock_irq(&isp116x->lock);
	seq_printf(s, "\n");

	return 0;
}

static int isp116x_open_seq(struct inode *inode, struct file *file)
{
	return single_open(file, isp116x_show_dbg, inode->i_private);
}

static const struct file_operations isp116x_debug_fops = {
	.open = isp116x_open_seq,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int create_debug_file(struct isp116x *isp116x)
{
	isp116x->dentry = debugfs_create_file(hcd_name,
					      S_IRUGO, NULL, isp116x,
					      &isp116x_debug_fops);
	if (!isp116x->dentry)
		return -ENOMEM;
	return 0;
}

static void remove_debug_file(struct isp116x *isp116x)
{
	debugfs_remove(isp116x->dentry);
}

#else

#define	create_debug_file(d)	0
#define	remove_debug_file(d)	do{}while(0)

#endif				


static int isp116x_sw_reset(struct isp116x *isp116x)
{
	int retries = 15;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&isp116x->lock, flags);
	isp116x_write_reg16(isp116x, HCSWRES, HCSWRES_MAGIC);
	isp116x_write_reg32(isp116x, HCCMDSTAT, HCCMDSTAT_HCR);
	while (--retries) {
		
		mdelay(1);
		if (!(isp116x_read_reg32(isp116x, HCCMDSTAT) & HCCMDSTAT_HCR))
			break;
	}
	if (!retries) {
		ERR("Software reset timeout\n");
		ret = -ETIME;
	}
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return ret;
}

static int isp116x_reset(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	unsigned long t;
	u16 clkrdy = 0;
	int ret, timeout = 15  ;

	ret = isp116x_sw_reset(isp116x);
	if (ret)
		return ret;

	t = jiffies + msecs_to_jiffies(timeout);
	while (time_before_eq(jiffies, t)) {
		msleep(4);
		spin_lock_irq(&isp116x->lock);
		clkrdy = isp116x_read_reg16(isp116x, HCuPINT) & HCuPINT_CLKRDY;
		spin_unlock_irq(&isp116x->lock);
		if (clkrdy)
			break;
	}
	if (!clkrdy) {
		ERR("Clock not ready after %dms\n", timeout);
		ERR("Please make sure that the H_WAKEUP pin is pulled low!\n");
		ret = -ENODEV;
	}
	return ret;
}

static void isp116x_stop(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&isp116x->lock, flags);
	isp116x_write_reg16(isp116x, HCuPINTENB, 0);

	val = isp116x_read_reg32(isp116x, HCRHDESCA);
	val &= ~(RH_A_NPS | RH_A_PSM);
	isp116x_write_reg32(isp116x, HCRHDESCA, val);
	isp116x_write_reg32(isp116x, HCRHSTATUS, RH_HS_LPS);
	spin_unlock_irqrestore(&isp116x->lock, flags);

	isp116x_sw_reset(isp116x);
}

static int isp116x_start(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	struct isp116x_platform_data *board = isp116x->board;
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&isp116x->lock, flags);

	
	isp116x_write_reg16(isp116x, HCuPINT, 0xff);
	isp116x_write_reg16(isp116x, HCuPINTENB, 0);

	val = isp116x_read_reg16(isp116x, HCCHIPID);
	if ((val & HCCHIPID_MASK) != HCCHIPID_MAGIC) {
		ERR("Invalid chip ID %04x\n", val);
		spin_unlock_irqrestore(&isp116x->lock, flags);
		return -ENODEV;
	}

	
	hcd->uses_new_polling = 1;

	isp116x_write_reg16(isp116x, HCITLBUFLEN, ISP116x_ITL_BUFSIZE);
	isp116x_write_reg16(isp116x, HCATLBUFLEN, ISP116x_ATL_BUFSIZE);

	
	val = HCHWCFG_INT_ENABLE | HCHWCFG_DBWIDTH(1);
	if (board->sel15Kres)
		val |= HCHWCFG_15KRSEL;
	
	if (board->remote_wakeup_enable)
		val |= HCHWCFG_CLKNOTSTOP;
	if (board->oc_enable)
		val |= HCHWCFG_ANALOG_OC;
	if (board->int_act_high)
		val |= HCHWCFG_INT_POL;
	if (board->int_edge_triggered)
		val |= HCHWCFG_INT_TRIGGER;
	isp116x_write_reg16(isp116x, HCHWCFG, val);

	
	val = (25 << 24) & RH_A_POTPGT;
	val |= RH_A_PSM;
	
	val |= RH_A_OCPM;
	isp116x_write_reg32(isp116x, HCRHDESCA, val);
	isp116x->rhdesca = isp116x_read_reg32(isp116x, HCRHDESCA);

	val = RH_B_PPCM;
	isp116x_write_reg32(isp116x, HCRHDESCB, val);
	isp116x->rhdescb = isp116x_read_reg32(isp116x, HCRHDESCB);

	val = 0;
	if (board->remote_wakeup_enable) {
		if (!device_can_wakeup(hcd->self.controller))
			device_init_wakeup(hcd->self.controller, 1);
		val |= RH_HS_DRWE;
	}
	isp116x_write_reg32(isp116x, HCRHSTATUS, val);
	isp116x->rhstatus = isp116x_read_reg32(isp116x, HCRHSTATUS);

	isp116x_write_reg32(isp116x, HCFMINTVL, 0x27782edf);

	hcd->state = HC_STATE_RUNNING;

	
	isp116x->intenb = HCINT_MIE | HCINT_RHSC | HCINT_UE;
	if (board->remote_wakeup_enable)
		isp116x->intenb |= HCINT_RD;
	isp116x->irqenb = HCuPINT_ATL | HCuPINT_OPR;	
	isp116x_write_reg32(isp116x, HCINTENB, isp116x->intenb);
	isp116x_write_reg16(isp116x, HCuPINTENB, isp116x->irqenb);

	
	val = HCCONTROL_USB_OPER;
	if (board->remote_wakeup_enable)
		val |= HCCONTROL_RWE;
	isp116x_write_reg32(isp116x, HCCONTROL, val);

	
	isp116x_write_reg32(isp116x, HCRHPORT1, RH_PS_CCS);
	isp116x_write_reg32(isp116x, HCRHPORT2, RH_PS_CCS);

	isp116x_show_regs_log(isp116x);
	spin_unlock_irqrestore(&isp116x->lock, flags);
	return 0;
}

#ifdef	CONFIG_PM

static int isp116x_bus_suspend(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	unsigned long flags;
	u32 val;
	int ret = 0;

	spin_lock_irqsave(&isp116x->lock, flags);
	val = isp116x_read_reg32(isp116x, HCCONTROL);

	switch (val & HCCONTROL_HCFS) {
	case HCCONTROL_USB_OPER:
		spin_unlock_irqrestore(&isp116x->lock, flags);
		val &= (~HCCONTROL_HCFS & ~HCCONTROL_RWE);
		val |= HCCONTROL_USB_SUSPEND;
		if (hcd->self.root_hub->do_remote_wakeup)
			val |= HCCONTROL_RWE;
		
		msleep(2);
		spin_lock_irqsave(&isp116x->lock, flags);
		isp116x_write_reg32(isp116x, HCCONTROL, val);
		spin_unlock_irqrestore(&isp116x->lock, flags);
		
		msleep(5);
		break;
	case HCCONTROL_USB_RESUME:
		isp116x_write_reg32(isp116x, HCCONTROL,
				    (val & ~HCCONTROL_HCFS) |
				    HCCONTROL_USB_RESET);
	case HCCONTROL_USB_RESET:
		ret = -EBUSY;
	default:		
		spin_unlock_irqrestore(&isp116x->lock, flags);
		break;
	}

	return ret;
}

static int isp116x_bus_resume(struct usb_hcd *hcd)
{
	struct isp116x *isp116x = hcd_to_isp116x(hcd);
	u32 val;

	msleep(5);
	spin_lock_irq(&isp116x->lock);

	val = isp116x_read_reg32(isp116x, HCCONTROL);
	switch (val & HCCONTROL_HCFS) {
	case HCCONTROL_USB_SUSPEND:
		val &= ~HCCONTROL_HCFS;
		val |= HCCONTROL_USB_RESUME;
		isp116x_write_reg32(isp116x, HCCONTROL, val);
	case HCCONTROL_USB_RESUME:
		break;
	case HCCONTROL_USB_OPER:
		spin_unlock_irq(&isp116x->lock);
		return 0;
	default:
		spin_unlock_irq(&isp116x->lock);
		DBG("Chip has been reset while suspended. Reinit from scratch.\n");
		isp116x_reset(hcd);
		isp116x_start(hcd);
		isp116x_hub_control(hcd, SetPortFeature,
				    USB_PORT_FEAT_POWER, 1, NULL, 0);
		if ((isp116x->rhdesca & RH_A_NDP) == 2)
			isp116x_hub_control(hcd, SetPortFeature,
					    USB_PORT_FEAT_POWER, 2, NULL, 0);
		return 0;
	}

	val = isp116x->rhdesca & RH_A_NDP;
	while (val--) {
		u32 stat =
		    isp116x_read_reg32(isp116x, val ? HCRHPORT2 : HCRHPORT1);
		
		if (!(stat & RH_PS_PSS))
			continue;
		DBG("%s: Resuming port %d\n", __func__, val);
		isp116x_write_reg32(isp116x, RH_PS_POCI, val
				    ? HCRHPORT2 : HCRHPORT1);
	}
	spin_unlock_irq(&isp116x->lock);

	hcd->state = HC_STATE_RESUMING;
	msleep(20);

	
	spin_lock_irq(&isp116x->lock);
	val = isp116x_read_reg32(isp116x, HCCONTROL);
	isp116x_write_reg32(isp116x, HCCONTROL,
			    (val & ~HCCONTROL_HCFS) | HCCONTROL_USB_OPER);
	spin_unlock_irq(&isp116x->lock);
	hcd->state = HC_STATE_RUNNING;

	return 0;
}

#else

#define	isp116x_bus_suspend	NULL
#define	isp116x_bus_resume	NULL

#endif

static struct hc_driver isp116x_hc_driver = {
	.description = hcd_name,
	.product_desc = "ISP116x Host Controller",
	.hcd_priv_size = sizeof(struct isp116x),

	.irq = isp116x_irq,
	.flags = HCD_USB11,

	.reset = isp116x_reset,
	.start = isp116x_start,
	.stop = isp116x_stop,

	.urb_enqueue = isp116x_urb_enqueue,
	.urb_dequeue = isp116x_urb_dequeue,
	.endpoint_disable = isp116x_endpoint_disable,

	.get_frame_number = isp116x_get_frame,

	.hub_status_data = isp116x_hub_status_data,
	.hub_control = isp116x_hub_control,
	.bus_suspend = isp116x_bus_suspend,
	.bus_resume = isp116x_bus_resume,
};


static int isp116x_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct isp116x *isp116x;
	struct resource *res;

	if (!hcd)
		return 0;
	isp116x = hcd_to_isp116x(hcd);
	remove_debug_file(isp116x);
	usb_remove_hcd(hcd);

	iounmap(isp116x->data_reg);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	release_mem_region(res->start, 2);
	iounmap(isp116x->addr_reg);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, 2);

	usb_put_hcd(hcd);
	return 0;
}

static int __devinit isp116x_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct isp116x *isp116x;
	struct resource *addr, *data, *ires;
	void __iomem *addr_reg;
	void __iomem *data_reg;
	int irq;
	int ret = 0;
	unsigned long irqflags;

	if (usb_disabled())
		return -ENODEV;

	if (pdev->num_resources < 3) {
		ret = -ENODEV;
		goto err1;
	}

	data = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ires = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	if (!addr || !data || !ires) {
		ret = -ENODEV;
		goto err1;
	}

	irq = ires->start;
	irqflags = ires->flags & IRQF_TRIGGER_MASK;

	if (pdev->dev.dma_mask) {
		DBG("DMA not supported\n");
		ret = -EINVAL;
		goto err1;
	}

	if (!request_mem_region(addr->start, 2, hcd_name)) {
		ret = -EBUSY;
		goto err1;
	}
	addr_reg = ioremap(addr->start, resource_size(addr));
	if (addr_reg == NULL) {
		ret = -ENOMEM;
		goto err2;
	}
	if (!request_mem_region(data->start, 2, hcd_name)) {
		ret = -EBUSY;
		goto err3;
	}
	data_reg = ioremap(data->start, resource_size(data));
	if (data_reg == NULL) {
		ret = -ENOMEM;
		goto err4;
	}

	
	hcd = usb_create_hcd(&isp116x_hc_driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		ret = -ENOMEM;
		goto err5;
	}
	
	hcd->rsrc_start = addr->start;
	isp116x = hcd_to_isp116x(hcd);
	isp116x->data_reg = data_reg;
	isp116x->addr_reg = addr_reg;
	spin_lock_init(&isp116x->lock);
	INIT_LIST_HEAD(&isp116x->async);
	isp116x->board = pdev->dev.platform_data;

	if (!isp116x->board) {
		ERR("Platform data structure not initialized\n");
		ret = -ENODEV;
		goto err6;
	}
	if (isp116x_check_platform_delay(isp116x)) {
		ERR("USE_PLATFORM_DELAY defined, but delay function not "
		    "implemented.\n");
		ERR("See comments in drivers/usb/host/isp116x-hcd.c\n");
		ret = -ENODEV;
		goto err6;
	}

	ret = usb_add_hcd(hcd, irq, irqflags);
	if (ret)
		goto err6;

	ret = create_debug_file(isp116x);
	if (ret) {
		ERR("Couldn't create debugfs entry\n");
		goto err7;
	}

	return 0;

      err7:
	usb_remove_hcd(hcd);
      err6:
	usb_put_hcd(hcd);
      err5:
	iounmap(data_reg);
      err4:
	release_mem_region(data->start, 2);
      err3:
	iounmap(addr_reg);
      err2:
	release_mem_region(addr->start, 2);
      err1:
	ERR("init error, %d\n", ret);
	return ret;
}

#ifdef	CONFIG_PM
static int isp116x_suspend(struct platform_device *dev, pm_message_t state)
{
	VDBG("%s: state %x\n", __func__, state.event);
	return 0;
}

static int isp116x_resume(struct platform_device *dev)
{
	VDBG("%s\n", __func__);
	return 0;
}

#else

#define	isp116x_suspend    NULL
#define	isp116x_resume     NULL

#endif

MODULE_ALIAS("platform:isp116x-hcd");

static struct platform_driver isp116x_driver = {
	.probe = isp116x_probe,
	.remove = isp116x_remove,
	.suspend = isp116x_suspend,
	.resume = isp116x_resume,
	.driver = {
		.name = (char *)hcd_name,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(isp116x_driver);
