/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 *
 * This file is licenced under the GPL.
 */

#include <linux/irq.h>
#include <linux/slab.h>

static void urb_free_priv (struct ohci_hcd *hc, urb_priv_t *urb_priv)
{
	int		last = urb_priv->length - 1;

	if (last >= 0) {
		int		i;
		struct td	*td;

		for (i = 0; i <= last; i++) {
			td = urb_priv->td [i];
			if (td)
				td_free (hc, td);
		}
	}

	list_del (&urb_priv->pending);
	kfree (urb_priv);
}


static void
finish_urb(struct ohci_hcd *ohci, struct urb *urb, int status)
__releases(ohci->lock)
__acquires(ohci->lock)
{
	

	urb_free_priv (ohci, urb->hcpriv);
	if (likely(status == -EINPROGRESS))
		status = 0;

	switch (usb_pipetype (urb->pipe)) {
	case PIPE_ISOCHRONOUS:
		ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs--;
		if (ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs == 0) {
			if (quirk_amdiso(ohci))
				usb_amd_quirk_pll_enable();
			if (quirk_amdprefetch(ohci))
				sb800_prefetch(ohci, 0);
		}
		break;
	case PIPE_INTERRUPT:
		ohci_to_hcd(ohci)->self.bandwidth_int_reqs--;
		break;
	}

#ifdef OHCI_VERBOSE_DEBUG
	urb_print(urb, "RET", usb_pipeout (urb->pipe), status);
#endif

	
	usb_hcd_unlink_urb_from_ep(ohci_to_hcd(ohci), urb);
	spin_unlock (&ohci->lock);
	usb_hcd_giveback_urb(ohci_to_hcd(ohci), urb, status);
	spin_lock (&ohci->lock);

	
	if (ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs == 0
			&& ohci_to_hcd(ohci)->self.bandwidth_int_reqs == 0) {
		ohci->hc_control &= ~(OHCI_CTRL_PLE|OHCI_CTRL_IE);
		ohci_writel (ohci, ohci->hc_control, &ohci->regs->control);
	}
}



static int balance (struct ohci_hcd *ohci, int interval, int load)
{
	int	i, branch = -ENOSPC;

	
	if (interval > NUM_INTS)
		interval = NUM_INTS;

	for (i = 0; i < interval ; i++) {
		if (branch < 0 || ohci->load [branch] > ohci->load [i]) {
			int	j;

			
			for (j = i; j < NUM_INTS; j += interval) {
				if ((ohci->load [j] + load) > 900)
					break;
			}
			if (j < NUM_INTS)
				continue;
			branch = i;
		}
	}
	return branch;
}


static void periodic_link (struct ohci_hcd *ohci, struct ed *ed)
{
	unsigned	i;

	ohci_vdbg (ohci, "link %sed %p branch %d [%dus.], interval %d\n",
		(ed->hwINFO & cpu_to_hc32 (ohci, ED_ISO)) ? "iso " : "",
		ed, ed->branch, ed->load, ed->interval);

	for (i = ed->branch; i < NUM_INTS; i += ed->interval) {
		struct ed	**prev = &ohci->periodic [i];
		__hc32		*prev_p = &ohci->hcca->int_table [i];
		struct ed	*here = *prev;

		while (here && ed != here) {
			if (ed->interval > here->interval)
				break;
			prev = &here->ed_next;
			prev_p = &here->hwNextED;
			here = *prev;
		}
		if (ed != here) {
			ed->ed_next = here;
			if (here)
				ed->hwNextED = *prev_p;
			wmb ();
			*prev = ed;
			*prev_p = cpu_to_hc32(ohci, ed->dma);
			wmb();
		}
		ohci->load [i] += ed->load;
	}
	ohci_to_hcd(ohci)->self.bandwidth_allocated += ed->load / ed->interval;
}


static int ed_schedule (struct ohci_hcd *ohci, struct ed *ed)
{
	int	branch;

	ed->state = ED_OPER;
	ed->ed_prev = NULL;
	ed->ed_next = NULL;
	ed->hwNextED = 0;
	if (quirk_zfmicro(ohci)
			&& (ed->type == PIPE_INTERRUPT)
			&& !(ohci->eds_scheduled++))
		mod_timer(&ohci->unlink_watchdog, round_jiffies(jiffies + HZ));
	wmb ();

	switch (ed->type) {
	case PIPE_CONTROL:
		if (ohci->ed_controltail == NULL) {
			WARN_ON (ohci->hc_control & OHCI_CTRL_CLE);
			ohci_writel (ohci, ed->dma,
					&ohci->regs->ed_controlhead);
		} else {
			ohci->ed_controltail->ed_next = ed;
			ohci->ed_controltail->hwNextED = cpu_to_hc32 (ohci,
								ed->dma);
		}
		ed->ed_prev = ohci->ed_controltail;
		if (!ohci->ed_controltail && !ohci->ed_rm_list) {
			wmb();
			ohci->hc_control |= OHCI_CTRL_CLE;
			ohci_writel (ohci, 0, &ohci->regs->ed_controlcurrent);
			ohci_writel (ohci, ohci->hc_control,
					&ohci->regs->control);
		}
		ohci->ed_controltail = ed;
		break;

	case PIPE_BULK:
		if (ohci->ed_bulktail == NULL) {
			WARN_ON (ohci->hc_control & OHCI_CTRL_BLE);
			ohci_writel (ohci, ed->dma, &ohci->regs->ed_bulkhead);
		} else {
			ohci->ed_bulktail->ed_next = ed;
			ohci->ed_bulktail->hwNextED = cpu_to_hc32 (ohci,
								ed->dma);
		}
		ed->ed_prev = ohci->ed_bulktail;
		if (!ohci->ed_bulktail && !ohci->ed_rm_list) {
			wmb();
			ohci->hc_control |= OHCI_CTRL_BLE;
			ohci_writel (ohci, 0, &ohci->regs->ed_bulkcurrent);
			ohci_writel (ohci, ohci->hc_control,
					&ohci->regs->control);
		}
		ohci->ed_bulktail = ed;
		break;

	
	
	default:
		branch = balance (ohci, ed->interval, ed->load);
		if (branch < 0) {
			ohci_dbg (ohci,
				"ERR %d, interval %d msecs, load %d\n",
				branch, ed->interval, ed->load);
			
			return branch;
		}
		ed->branch = branch;
		periodic_link (ohci, ed);
	}

	return 0;
}


static void periodic_unlink (struct ohci_hcd *ohci, struct ed *ed)
{
	int	i;

	for (i = ed->branch; i < NUM_INTS; i += ed->interval) {
		struct ed	*temp;
		struct ed	**prev = &ohci->periodic [i];
		__hc32		*prev_p = &ohci->hcca->int_table [i];

		while (*prev && (temp = *prev) != ed) {
			prev_p = &temp->hwNextED;
			prev = &temp->ed_next;
		}
		if (*prev) {
			*prev_p = ed->hwNextED;
			*prev = ed->ed_next;
		}
		ohci->load [i] -= ed->load;
	}
	ohci_to_hcd(ohci)->self.bandwidth_allocated -= ed->load / ed->interval;

	ohci_vdbg (ohci, "unlink %sed %p branch %d [%dus.], interval %d\n",
		(ed->hwINFO & cpu_to_hc32 (ohci, ED_ISO)) ? "iso " : "",
		ed, ed->branch, ed->load, ed->interval);
}

static void ed_deschedule (struct ohci_hcd *ohci, struct ed *ed)
{
	ed->hwINFO |= cpu_to_hc32 (ohci, ED_SKIP);
	wmb ();
	ed->state = ED_UNLINK;

	switch (ed->type) {
	case PIPE_CONTROL:
		
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_CLE;
				ohci_writel (ohci, ohci->hc_control,
						&ohci->regs->control);
				
			} else
				ohci_writel (ohci,
					hc32_to_cpup (ohci, &ed->hwNextED),
					&ohci->regs->ed_controlhead);
		} else {
			ed->ed_prev->ed_next = ed->ed_next;
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		
		if (ohci->ed_controltail == ed) {
			ohci->ed_controltail = ed->ed_prev;
			if (ohci->ed_controltail)
				ohci->ed_controltail->ed_next = NULL;
		} else if (ed->ed_next) {
			ed->ed_next->ed_prev = ed->ed_prev;
		}
		break;

	case PIPE_BULK:
		
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_BLE;
				ohci_writel (ohci, ohci->hc_control,
						&ohci->regs->control);
				
			} else
				ohci_writel (ohci,
					hc32_to_cpup (ohci, &ed->hwNextED),
					&ohci->regs->ed_bulkhead);
		} else {
			ed->ed_prev->ed_next = ed->ed_next;
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		
		if (ohci->ed_bulktail == ed) {
			ohci->ed_bulktail = ed->ed_prev;
			if (ohci->ed_bulktail)
				ohci->ed_bulktail->ed_next = NULL;
		} else if (ed->ed_next) {
			ed->ed_next->ed_prev = ed->ed_prev;
		}
		break;

	
	
	default:
		periodic_unlink (ohci, ed);
		break;
	}
}



static struct ed *ed_get (
	struct ohci_hcd		*ohci,
	struct usb_host_endpoint *ep,
	struct usb_device	*udev,
	unsigned int		pipe,
	int			interval
) {
	struct ed		*ed;
	unsigned long		flags;

	spin_lock_irqsave (&ohci->lock, flags);

	if (!(ed = ep->hcpriv)) {
		struct td	*td;
		int		is_out;
		u32		info;

		ed = ed_alloc (ohci, GFP_ATOMIC);
		if (!ed) {
			
			goto done;
		}

		
		td = td_alloc (ohci, GFP_ATOMIC);
		if (!td) {
			
			ed_free (ohci, ed);
			ed = NULL;
			goto done;
		}
		ed->dummy = td;
		ed->hwTailP = cpu_to_hc32 (ohci, td->td_dma);
		ed->hwHeadP = ed->hwTailP;	
		ed->state = ED_IDLE;

		is_out = !(ep->desc.bEndpointAddress & USB_DIR_IN);

		info = usb_pipedevice (pipe);
		ed->type = usb_pipetype(pipe);

		info |= (ep->desc.bEndpointAddress & ~USB_DIR_IN) << 7;
		info |= usb_endpoint_maxp(&ep->desc) << 16;
		if (udev->speed == USB_SPEED_LOW)
			info |= ED_LOWSPEED;
		
		if (ed->type != PIPE_CONTROL) {
			info |= is_out ? ED_OUT : ED_IN;
			if (ed->type != PIPE_BULK) {
				
				if (ed->type == PIPE_ISOCHRONOUS)
					info |= ED_ISO;
				else if (interval > 32)	
					interval = 32;
				ed->interval = interval;
				ed->load = usb_calc_bus_time (
					udev->speed, !is_out,
					ed->type == PIPE_ISOCHRONOUS,
					usb_endpoint_maxp(&ep->desc))
						/ 1000;
			}
		}
		ed->hwINFO = cpu_to_hc32(ohci, info);

		ep->hcpriv = ed;
	}

done:
	spin_unlock_irqrestore (&ohci->lock, flags);
	return ed;
}


static void start_ed_unlink (struct ohci_hcd *ohci, struct ed *ed)
{
	ed->hwINFO |= cpu_to_hc32 (ohci, ED_DEQUEUE);
	ed_deschedule (ohci, ed);

	
	ed->ed_next = ohci->ed_rm_list;
	ed->ed_prev = NULL;
	ohci->ed_rm_list = ed;

	
	ohci_writel (ohci, OHCI_INTR_SF, &ohci->regs->intrstatus);
	ohci_writel (ohci, OHCI_INTR_SF, &ohci->regs->intrenable);
	
	(void) ohci_readl (ohci, &ohci->regs->control);

	ed->tick = ohci_frame_no(ohci) + 1;

}



static void
td_fill (struct ohci_hcd *ohci, u32 info,
	dma_addr_t data, int len,
	struct urb *urb, int index)
{
	struct td		*td, *td_pt;
	struct urb_priv		*urb_priv = urb->hcpriv;
	int			is_iso = info & TD_ISO;
	int			hash;

	

	if (index != (urb_priv->length - 1)
			|| (urb->transfer_flags & URB_NO_INTERRUPT))
		info |= TD_DI_SET (6);

	
	td_pt = urb_priv->td [index];

	
	td = urb_priv->td [index] = urb_priv->ed->dummy;
	urb_priv->ed->dummy = td_pt;

	td->ed = urb_priv->ed;
	td->next_dl_td = NULL;
	td->index = index;
	td->urb = urb;
	td->data_dma = data;
	if (!len)
		data = 0;

	td->hwINFO = cpu_to_hc32 (ohci, info);
	if (is_iso) {
		td->hwCBP = cpu_to_hc32 (ohci, data & 0xFFFFF000);
		*ohci_hwPSWp(ohci, td, 0) = cpu_to_hc16 (ohci,
						(data & 0x0FFF) | 0xE000);
		td->ed->last_iso = info & 0xffff;
	} else {
		td->hwCBP = cpu_to_hc32 (ohci, data);
	}
	if (data)
		td->hwBE = cpu_to_hc32 (ohci, data + len - 1);
	else
		td->hwBE = 0;
	td->hwNextTD = cpu_to_hc32 (ohci, td_pt->td_dma);

	
	list_add_tail (&td->td_list, &td->ed->td_list);

	
	hash = TD_HASH_FUNC (td->td_dma);
	td->td_hash = ohci->td_hash [hash];
	ohci->td_hash [hash] = td;

	
	wmb ();
	td->ed->hwTailP = td->hwNextTD;
}


static void td_submit_urb (
	struct ohci_hcd	*ohci,
	struct urb	*urb
) {
	struct urb_priv	*urb_priv = urb->hcpriv;
	dma_addr_t	data;
	int		data_len = urb->transfer_buffer_length;
	int		cnt = 0;
	u32		info = 0;
	int		is_out = usb_pipeout (urb->pipe);
	int		periodic = 0;

	if (!usb_gettoggle (urb->dev, usb_pipeendpoint (urb->pipe), is_out)) {
		usb_settoggle (urb->dev, usb_pipeendpoint (urb->pipe),
			is_out, 1);
		urb_priv->ed->hwHeadP &= ~cpu_to_hc32 (ohci, ED_C);
	}

	urb_priv->td_cnt = 0;
	list_add (&urb_priv->pending, &ohci->pending);

	if (data_len)
		data = urb->transfer_dma;
	else
		data = 0;

	switch (urb_priv->ed->type) {

	case PIPE_INTERRUPT:
		
		periodic = ohci_to_hcd(ohci)->self.bandwidth_int_reqs++ == 0
			&& ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs == 0;
		
	case PIPE_BULK:
		info = is_out
			? TD_T_TOGGLE | TD_CC | TD_DP_OUT
			: TD_T_TOGGLE | TD_CC | TD_DP_IN;
		
		while (data_len > 4096) {
			td_fill (ohci, info, data, 4096, urb, cnt);
			data += 4096;
			data_len -= 4096;
			cnt++;
		}
		
		if (!(urb->transfer_flags & URB_SHORT_NOT_OK))
			info |= TD_R;
		td_fill (ohci, info, data, data_len, urb, cnt);
		cnt++;
		if ((urb->transfer_flags & URB_ZERO_PACKET)
				&& cnt < urb_priv->length) {
			td_fill (ohci, info, 0, 0, urb, cnt);
			cnt++;
		}
		
		if (urb_priv->ed->type == PIPE_BULK) {
			wmb ();
			ohci_writel (ohci, OHCI_BLF, &ohci->regs->cmdstatus);
		}
		break;

	case PIPE_CONTROL:
		info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
		td_fill (ohci, info, urb->setup_dma, 8, urb, cnt++);
		if (data_len > 0) {
			info = TD_CC | TD_R | TD_T_DATA1;
			info |= is_out ? TD_DP_OUT : TD_DP_IN;
			
			td_fill (ohci, info, data, data_len, urb, cnt++);
		}
		info = (is_out || data_len == 0)
			? TD_CC | TD_DP_IN | TD_T_DATA1
			: TD_CC | TD_DP_OUT | TD_T_DATA1;
		td_fill (ohci, info, data, 0, urb, cnt++);
		
		wmb ();
		ohci_writel (ohci, OHCI_CLF, &ohci->regs->cmdstatus);
		break;

	case PIPE_ISOCHRONOUS:
		for (cnt = 0; cnt < urb->number_of_packets; cnt++) {
			int	frame = urb->start_frame;

			
			
			
			frame += cnt * urb->interval;
			frame &= 0xffff;
			td_fill (ohci, TD_CC | TD_ISO | frame,
				data + urb->iso_frame_desc [cnt].offset,
				urb->iso_frame_desc [cnt].length, urb, cnt);
		}
		if (ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs == 0) {
			if (quirk_amdiso(ohci))
				usb_amd_quirk_pll_disable();
			if (quirk_amdprefetch(ohci))
				sb800_prefetch(ohci, 1);
		}
		periodic = ohci_to_hcd(ohci)->self.bandwidth_isoc_reqs++ == 0
			&& ohci_to_hcd(ohci)->self.bandwidth_int_reqs == 0;
		break;
	}

	
	if (periodic) {
		wmb ();
		ohci->hc_control |= OHCI_CTRL_PLE|OHCI_CTRL_IE;
		ohci_writel (ohci, ohci->hc_control, &ohci->regs->control);
	}

	
}


static int td_done(struct ohci_hcd *ohci, struct urb *urb, struct td *td)
{
	u32	tdINFO = hc32_to_cpup (ohci, &td->hwINFO);
	int	cc = 0;
	int	status = -EINPROGRESS;

	list_del (&td->td_list);

	
	if (tdINFO & TD_ISO) {
		u16	tdPSW = ohci_hwPSW(ohci, td, 0);
		int	dlen = 0;


		cc = (tdPSW >> 12) & 0xF;
		if (tdINFO & TD_CC)	
			return status;

		if (usb_pipeout (urb->pipe))
			dlen = urb->iso_frame_desc [td->index].length;
		else {
			
			if (cc == TD_DATAUNDERRUN)
				cc = TD_CC_NOERROR;
			dlen = tdPSW & 0x3ff;
		}
		urb->actual_length += dlen;
		urb->iso_frame_desc [td->index].actual_length = dlen;
		urb->iso_frame_desc [td->index].status = cc_to_error [cc];

		if (cc != TD_CC_NOERROR)
			ohci_vdbg (ohci,
				"urb %p iso td %p (%d) len %d cc %d\n",
				urb, td, 1 + td->index, dlen, cc);

	} else {
		int	type = usb_pipetype (urb->pipe);
		u32	tdBE = hc32_to_cpup (ohci, &td->hwBE);

		cc = TD_CC_GET (tdINFO);

		
		if (cc == TD_DATAUNDERRUN
				&& !(urb->transfer_flags & URB_SHORT_NOT_OK))
			cc = TD_CC_NOERROR;
		if (cc != TD_CC_NOERROR && cc < 0x0E)
			status = cc_to_error[cc];

		
		if ((type != PIPE_CONTROL || td->index != 0) && tdBE != 0) {
			if (td->hwCBP == 0)
				urb->actual_length += tdBE - td->data_dma + 1;
			else
				urb->actual_length +=
					  hc32_to_cpup (ohci, &td->hwCBP)
					- td->data_dma;
		}

		if (cc != TD_CC_NOERROR && cc < 0x0E)
			ohci_vdbg (ohci,
				"urb %p td %p (%d) cc %d, len=%d/%d\n",
				urb, td, 1 + td->index, cc,
				urb->actual_length,
				urb->transfer_buffer_length);
	}
	return status;
}


static void ed_halted(struct ohci_hcd *ohci, struct td *td, int cc)
{
	struct urb		*urb = td->urb;
	urb_priv_t		*urb_priv = urb->hcpriv;
	struct ed		*ed = td->ed;
	struct list_head	*tmp = td->td_list.next;
	__hc32			toggle = ed->hwHeadP & cpu_to_hc32 (ohci, ED_C);

	ed->hwINFO |= cpu_to_hc32 (ohci, ED_SKIP);
	wmb ();
	ed->hwHeadP &= ~cpu_to_hc32 (ohci, ED_H);

	while (tmp != &ed->td_list) {
		struct td	*next;

		next = list_entry (tmp, struct td, td_list);
		tmp = next->td_list.next;

		if (next->urb != urb)
			break;


		list_del(&next->td_list);
		urb_priv->td_cnt++;
		ed->hwHeadP = next->hwNextTD | toggle;
	}

	switch (cc) {
	case TD_DATAUNDERRUN:
		if ((urb->transfer_flags & URB_SHORT_NOT_OK) == 0)
			break;
		
	case TD_CC_STALL:
		if (usb_pipecontrol (urb->pipe))
			break;
		
	default:
		ohci_dbg (ohci,
			"urb %p path %s ep%d%s %08x cc %d --> status %d\n",
			urb, urb->dev->devpath,
			usb_pipeendpoint (urb->pipe),
			usb_pipein (urb->pipe) ? "in" : "out",
			hc32_to_cpu (ohci, td->hwINFO),
			cc, cc_to_error [cc]);
	}
}

static struct td *dl_reverse_done_list (struct ohci_hcd *ohci)
{
	u32		td_dma;
	struct td	*td_rev = NULL;
	struct td	*td = NULL;

	td_dma = hc32_to_cpup (ohci, &ohci->hcca->done_head);
	ohci->hcca->done_head = 0;
	wmb();

	while (td_dma) {
		int		cc;

		td = dma_to_td (ohci, td_dma);
		if (!td) {
			ohci_err (ohci, "bad entry %8x\n", td_dma);
			break;
		}

		td->hwINFO |= cpu_to_hc32 (ohci, TD_DONE);
		cc = TD_CC_GET (hc32_to_cpup (ohci, &td->hwINFO));

		if (cc != TD_CC_NOERROR
				&& (td->ed->hwHeadP & cpu_to_hc32 (ohci, ED_H)))
			ed_halted(ohci, td, cc);

		td->next_dl_td = td_rev;
		td_rev = td;
		td_dma = hc32_to_cpup (ohci, &td->hwNextTD);
	}
	return td_rev;
}


static void
finish_unlinks (struct ohci_hcd *ohci, u16 tick)
{
	struct ed	*ed, **last;

rescan_all:
	for (last = &ohci->ed_rm_list, ed = *last; ed != NULL; ed = *last) {
		struct list_head	*entry, *tmp;
		int			completed, modified;
		__hc32			*prev;

		if (likely(ohci->rh_state == OHCI_RH_RUNNING)) {
			if (tick_before (tick, ed->tick)) {
skip_ed:
				last = &ed->ed_next;
				continue;
			}

			if (!list_empty (&ed->td_list)) {
				struct td	*td;
				u32		head;

				td = list_entry (ed->td_list.next, struct td,
							td_list);
				head = hc32_to_cpu (ohci, ed->hwHeadP) &
								TD_MASK;

				
				if (td->td_dma != head) {
					if (ed == ohci->ed_to_check)
						ohci->ed_to_check = NULL;
					else
						goto skip_ed;
				}
			}
		}

		*last = ed->ed_next;
		ed->ed_next = NULL;
		modified = 0;

rescan_this:
		completed = 0;
		prev = &ed->hwHeadP;
		list_for_each_safe (entry, tmp, &ed->td_list) {
			struct td	*td;
			struct urb	*urb;
			urb_priv_t	*urb_priv;
			__hc32		savebits;
			u32		tdINFO;

			td = list_entry (entry, struct td, td_list);
			urb = td->urb;
			urb_priv = td->urb->hcpriv;

			if (!urb->unlinked) {
				prev = &td->hwNextTD;
				continue;
			}

			
			savebits = *prev & ~cpu_to_hc32 (ohci, TD_MASK);
			*prev = td->hwNextTD | savebits;

			tdINFO = hc32_to_cpup(ohci, &td->hwINFO);
			if ((tdINFO & TD_T) == TD_T_DATA0)
				ed->hwHeadP &= ~cpu_to_hc32(ohci, ED_C);
			else if ((tdINFO & TD_T) == TD_T_DATA1)
				ed->hwHeadP |= cpu_to_hc32(ohci, ED_C);

			
			td_done (ohci, urb, td);
			urb_priv->td_cnt++;

			
			if (urb_priv->td_cnt == urb_priv->length) {
				modified = completed = 1;
				finish_urb(ohci, urb, 0);
			}
		}
		if (completed && !list_empty (&ed->td_list))
			goto rescan_this;

		
		ed->state = ED_IDLE;
		if (quirk_zfmicro(ohci) && ed->type == PIPE_INTERRUPT)
			ohci->eds_scheduled--;
		ed->hwHeadP &= ~cpu_to_hc32(ohci, ED_H);
		ed->hwNextED = 0;
		wmb ();
		ed->hwINFO &= ~cpu_to_hc32 (ohci, ED_SKIP | ED_DEQUEUE);

		
		if (!list_empty (&ed->td_list)) {
			if (ohci->rh_state == OHCI_RH_RUNNING)
				ed_schedule (ohci, ed);
		}

		if (modified)
			goto rescan_all;
	}

	
	if (ohci->rh_state == OHCI_RH_RUNNING && !ohci->ed_rm_list) {
		u32	command = 0, control = 0;

		if (ohci->ed_controltail) {
			command |= OHCI_CLF;
			if (quirk_zfmicro(ohci))
				mdelay(1);
			if (!(ohci->hc_control & OHCI_CTRL_CLE)) {
				control |= OHCI_CTRL_CLE;
				ohci_writel (ohci, 0,
					&ohci->regs->ed_controlcurrent);
			}
		}
		if (ohci->ed_bulktail) {
			command |= OHCI_BLF;
			if (quirk_zfmicro(ohci))
				mdelay(1);
			if (!(ohci->hc_control & OHCI_CTRL_BLE)) {
				control |= OHCI_CTRL_BLE;
				ohci_writel (ohci, 0,
					&ohci->regs->ed_bulkcurrent);
			}
		}

		
		if (control) {
			ohci->hc_control |= control;
			if (quirk_zfmicro(ohci))
				mdelay(1);
			ohci_writel (ohci, ohci->hc_control,
					&ohci->regs->control);
		}
		if (command) {
			if (quirk_zfmicro(ohci))
				mdelay(1);
			ohci_writel (ohci, command, &ohci->regs->cmdstatus);
		}
	}
}




static void takeback_td(struct ohci_hcd *ohci, struct td *td)
{
	struct urb	*urb = td->urb;
	urb_priv_t	*urb_priv = urb->hcpriv;
	struct ed	*ed = td->ed;
	int		status;

	
	status = td_done(ohci, urb, td);
	urb_priv->td_cnt++;

	
	if (urb_priv->td_cnt == urb_priv->length)
		finish_urb(ohci, urb, status);

	
	if (list_empty(&ed->td_list)) {
		if (ed->state == ED_OPER)
			start_ed_unlink(ohci, ed);

	
	} else if ((ed->hwINFO & cpu_to_hc32(ohci, ED_SKIP | ED_DEQUEUE))
			== cpu_to_hc32(ohci, ED_SKIP)) {
		td = list_entry(ed->td_list.next, struct td, td_list);
		if (!(td->hwINFO & cpu_to_hc32(ohci, TD_DONE))) {
			ed->hwINFO &= ~cpu_to_hc32(ohci, ED_SKIP);
			
			switch (ed->type) {
			case PIPE_CONTROL:
				ohci_writel(ohci, OHCI_CLF,
						&ohci->regs->cmdstatus);
				break;
			case PIPE_BULK:
				ohci_writel(ohci, OHCI_BLF,
						&ohci->regs->cmdstatus);
				break;
			}
		}
	}
}

static void
dl_done_list (struct ohci_hcd *ohci)
{
	struct td	*td = dl_reverse_done_list (ohci);

	while (td) {
		struct td	*td_next = td->next_dl_td;
		takeback_td(ohci, td);
		td = td_next;
	}
}
