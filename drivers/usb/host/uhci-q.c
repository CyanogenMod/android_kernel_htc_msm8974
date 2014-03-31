/*
 * Universal Host Controller Interface driver for USB.
 *
 * Maintainer: Alan Stern <stern@rowland.harvard.edu>
 *
 * (C) Copyright 1999 Linus Torvalds
 * (C) Copyright 1999-2002 Johannes Erdfelt, johannes@erdfelt.com
 * (C) Copyright 1999 Randy Dunlap
 * (C) Copyright 1999 Georg Acher, acher@in.tum.de
 * (C) Copyright 1999 Deti Fliegl, deti@fliegl.de
 * (C) Copyright 1999 Thomas Sailer, sailer@ife.ee.ethz.ch
 * (C) Copyright 1999 Roman Weissgaerber, weissg@vienna.at
 * (C) Copyright 2000 Yggdrasil Computing, Inc. (port of new PCI interface
 *               support from usb-ohci.c by Adam Richter, adam@yggdrasil.com).
 * (C) Copyright 1999 Gregory P. Smith (from usb-ohci.c)
 * (C) Copyright 2004-2007 Alan Stern, stern@rowland.harvard.edu
 */


static void uhci_set_next_interrupt(struct uhci_hcd *uhci)
{
	if (uhci->is_stopped)
		mod_timer(&uhci_to_hcd(uhci)->rh_timer, jiffies);
	uhci->term_td->status |= cpu_to_hc32(uhci, TD_CTRL_IOC);
}

static inline void uhci_clear_next_interrupt(struct uhci_hcd *uhci)
{
	uhci->term_td->status &= ~cpu_to_hc32(uhci, TD_CTRL_IOC);
}


static void uhci_fsbr_on(struct uhci_hcd *uhci)
{
	struct uhci_qh *lqh;

	uhci->fsbr_is_on = 1;
	lqh = list_entry(uhci->skel_async_qh->node.prev,
			struct uhci_qh, node);
	lqh->link = LINK_TO_QH(uhci, uhci->skel_term_qh);
}

static void uhci_fsbr_off(struct uhci_hcd *uhci)
{
	struct uhci_qh *lqh;

	uhci->fsbr_is_on = 0;
	lqh = list_entry(uhci->skel_async_qh->node.prev,
			struct uhci_qh, node);
	lqh->link = UHCI_PTR_TERM(uhci);
}

static void uhci_add_fsbr(struct uhci_hcd *uhci, struct urb *urb)
{
	struct urb_priv *urbp = urb->hcpriv;

	if (!(urb->transfer_flags & URB_NO_FSBR))
		urbp->fsbr = 1;
}

static void uhci_urbp_wants_fsbr(struct uhci_hcd *uhci, struct urb_priv *urbp)
{
	if (urbp->fsbr) {
		uhci->fsbr_is_wanted = 1;
		if (!uhci->fsbr_is_on)
			uhci_fsbr_on(uhci);
		else if (uhci->fsbr_expiring) {
			uhci->fsbr_expiring = 0;
			del_timer(&uhci->fsbr_timer);
		}
	}
}

static void uhci_fsbr_timeout(unsigned long _uhci)
{
	struct uhci_hcd *uhci = (struct uhci_hcd *) _uhci;
	unsigned long flags;

	spin_lock_irqsave(&uhci->lock, flags);
	if (uhci->fsbr_expiring) {
		uhci->fsbr_expiring = 0;
		uhci_fsbr_off(uhci);
	}
	spin_unlock_irqrestore(&uhci->lock, flags);
}


static struct uhci_td *uhci_alloc_td(struct uhci_hcd *uhci)
{
	dma_addr_t dma_handle;
	struct uhci_td *td;

	td = dma_pool_alloc(uhci->td_pool, GFP_ATOMIC, &dma_handle);
	if (!td)
		return NULL;

	td->dma_handle = dma_handle;
	td->frame = -1;

	INIT_LIST_HEAD(&td->list);
	INIT_LIST_HEAD(&td->fl_list);

	return td;
}

static void uhci_free_td(struct uhci_hcd *uhci, struct uhci_td *td)
{
	if (!list_empty(&td->list))
		dev_WARN(uhci_dev(uhci), "td %p still in list!\n", td);
	if (!list_empty(&td->fl_list))
		dev_WARN(uhci_dev(uhci), "td %p still in fl_list!\n", td);

	dma_pool_free(uhci->td_pool, td, td->dma_handle);
}

static inline void uhci_fill_td(struct uhci_hcd *uhci, struct uhci_td *td,
		u32 status, u32 token, u32 buffer)
{
	td->status = cpu_to_hc32(uhci, status);
	td->token = cpu_to_hc32(uhci, token);
	td->buffer = cpu_to_hc32(uhci, buffer);
}

static void uhci_add_td_to_urbp(struct uhci_td *td, struct urb_priv *urbp)
{
	list_add_tail(&td->list, &urbp->td_list);
}

static void uhci_remove_td_from_urbp(struct uhci_td *td)
{
	list_del_init(&td->list);
}

static inline void uhci_insert_td_in_frame_list(struct uhci_hcd *uhci,
		struct uhci_td *td, unsigned framenum)
{
	framenum &= (UHCI_NUMFRAMES - 1);

	td->frame = framenum;

	
	if (uhci->frame_cpu[framenum]) {
		struct uhci_td *ftd, *ltd;

		ftd = uhci->frame_cpu[framenum];
		ltd = list_entry(ftd->fl_list.prev, struct uhci_td, fl_list);

		list_add_tail(&td->fl_list, &ftd->fl_list);

		td->link = ltd->link;
		wmb();
		ltd->link = LINK_TO_TD(uhci, td);
	} else {
		td->link = uhci->frame[framenum];
		wmb();
		uhci->frame[framenum] = LINK_TO_TD(uhci, td);
		uhci->frame_cpu[framenum] = td;
	}
}

static inline void uhci_remove_td_from_frame_list(struct uhci_hcd *uhci,
		struct uhci_td *td)
{
	
	if (td->frame == -1) {
		WARN_ON(!list_empty(&td->fl_list));
		return;
	}

	if (uhci->frame_cpu[td->frame] == td) {
		if (list_empty(&td->fl_list)) {
			uhci->frame[td->frame] = td->link;
			uhci->frame_cpu[td->frame] = NULL;
		} else {
			struct uhci_td *ntd;

			ntd = list_entry(td->fl_list.next,
					 struct uhci_td,
					 fl_list);
			uhci->frame[td->frame] = LINK_TO_TD(uhci, ntd);
			uhci->frame_cpu[td->frame] = ntd;
		}
	} else {
		struct uhci_td *ptd;

		ptd = list_entry(td->fl_list.prev, struct uhci_td, fl_list);
		ptd->link = td->link;
	}

	list_del_init(&td->fl_list);
	td->frame = -1;
}

static inline void uhci_remove_tds_from_frame(struct uhci_hcd *uhci,
		unsigned int framenum)
{
	struct uhci_td *ftd, *ltd;

	framenum &= (UHCI_NUMFRAMES - 1);

	ftd = uhci->frame_cpu[framenum];
	if (ftd) {
		ltd = list_entry(ftd->fl_list.prev, struct uhci_td, fl_list);
		uhci->frame[framenum] = ltd->link;
		uhci->frame_cpu[framenum] = NULL;

		while (!list_empty(&ftd->fl_list))
			list_del_init(ftd->fl_list.prev);
	}
}

static void uhci_unlink_isochronous_tds(struct uhci_hcd *uhci, struct urb *urb)
{
	struct urb_priv *urbp = (struct urb_priv *) urb->hcpriv;
	struct uhci_td *td;

	list_for_each_entry(td, &urbp->td_list, list)
		uhci_remove_td_from_frame_list(uhci, td);
}

static struct uhci_qh *uhci_alloc_qh(struct uhci_hcd *uhci,
		struct usb_device *udev, struct usb_host_endpoint *hep)
{
	dma_addr_t dma_handle;
	struct uhci_qh *qh;

	qh = dma_pool_alloc(uhci->qh_pool, GFP_ATOMIC, &dma_handle);
	if (!qh)
		return NULL;

	memset(qh, 0, sizeof(*qh));
	qh->dma_handle = dma_handle;

	qh->element = UHCI_PTR_TERM(uhci);
	qh->link = UHCI_PTR_TERM(uhci);

	INIT_LIST_HEAD(&qh->queue);
	INIT_LIST_HEAD(&qh->node);

	if (udev) {		
		qh->type = usb_endpoint_type(&hep->desc);
		if (qh->type != USB_ENDPOINT_XFER_ISOC) {
			qh->dummy_td = uhci_alloc_td(uhci);
			if (!qh->dummy_td) {
				dma_pool_free(uhci->qh_pool, qh, dma_handle);
				return NULL;
			}
		}
		qh->state = QH_STATE_IDLE;
		qh->hep = hep;
		qh->udev = udev;
		hep->hcpriv = qh;

		if (qh->type == USB_ENDPOINT_XFER_INT ||
				qh->type == USB_ENDPOINT_XFER_ISOC)
			qh->load = usb_calc_bus_time(udev->speed,
					usb_endpoint_dir_in(&hep->desc),
					qh->type == USB_ENDPOINT_XFER_ISOC,
					usb_endpoint_maxp(&hep->desc))
				/ 1000 + 1;

	} else {		
		qh->state = QH_STATE_ACTIVE;
		qh->type = -1;
	}
	return qh;
}

static void uhci_free_qh(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	WARN_ON(qh->state != QH_STATE_IDLE && qh->udev);
	if (!list_empty(&qh->queue))
		dev_WARN(uhci_dev(uhci), "qh %p list not empty!\n", qh);

	list_del(&qh->node);
	if (qh->udev) {
		qh->hep->hcpriv = NULL;
		if (qh->dummy_td)
			uhci_free_td(uhci, qh->dummy_td);
	}
	dma_pool_free(uhci->qh_pool, qh, qh->dma_handle);
}

static int uhci_cleanup_queue(struct uhci_hcd *uhci, struct uhci_qh *qh,
		struct urb *urb)
{
	struct urb_priv *urbp = urb->hcpriv;
	struct uhci_td *td;
	int ret = 1;

	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		ret = (uhci->frame_number + uhci->is_stopped !=
				qh->unlink_frame);
		goto done;
	}

	if (qh->queue.next != &urbp->node) {
		struct urb_priv *purbp;
		struct uhci_td *ptd;

		purbp = list_entry(urbp->node.prev, struct urb_priv, node);
		WARN_ON(list_empty(&purbp->td_list));
		ptd = list_entry(purbp->td_list.prev, struct uhci_td,
				list);
		td = list_entry(urbp->td_list.prev, struct uhci_td,
				list);
		ptd->link = td->link;
		goto done;
	}

	if (qh_element(qh) == UHCI_PTR_TERM(uhci))
		goto done;
	qh->element = UHCI_PTR_TERM(uhci);

	
	if (qh->type == USB_ENDPOINT_XFER_CONTROL)
		goto done;

	
	WARN_ON(list_empty(&urbp->td_list));
	td = list_entry(urbp->td_list.next, struct uhci_td, list);
	qh->needs_fixup = 1;
	qh->initial_toggle = uhci_toggle(td_token(uhci, td));

done:
	return ret;
}

static void uhci_fixup_toggles(struct uhci_hcd *uhci, struct uhci_qh *qh,
			int skip_first)
{
	struct urb_priv *urbp = NULL;
	struct uhci_td *td;
	unsigned int toggle = qh->initial_toggle;
	unsigned int pipe;

	if (skip_first)
		urbp = list_entry(qh->queue.next, struct urb_priv, node);

	else if (qh_element(qh) != UHCI_PTR_TERM(uhci))
		toggle = 2;

	urbp = list_prepare_entry(urbp, &qh->queue, node);
	list_for_each_entry_continue(urbp, &qh->queue, node) {

		td = list_entry(urbp->td_list.next, struct uhci_td, list);
		if (toggle > 1 || uhci_toggle(td_token(uhci, td)) == toggle) {
			td = list_entry(urbp->td_list.prev, struct uhci_td,
					list);
			toggle = uhci_toggle(td_token(uhci, td)) ^ 1;

		
		} else {
			list_for_each_entry(td, &urbp->td_list, list) {
				td->token ^= cpu_to_hc32(uhci,
							TD_TOKEN_TOGGLE);
				toggle ^= 1;
			}
		}
	}

	wmb();
	pipe = list_entry(qh->queue.next, struct urb_priv, node)->urb->pipe;
	usb_settoggle(qh->udev, usb_pipeendpoint(pipe),
			usb_pipeout(pipe), toggle);
	qh->needs_fixup = 0;
}

static inline void link_iso(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	list_add_tail(&qh->node, &uhci->skel_iso_qh->node);

	
}

static void link_interrupt(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct uhci_qh *pqh;

	list_add_tail(&qh->node, &uhci->skelqh[qh->skel]->node);

	pqh = list_entry(qh->node.prev, struct uhci_qh, node);
	qh->link = pqh->link;
	wmb();
	pqh->link = LINK_TO_QH(uhci, qh);
}

static void link_async(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct uhci_qh *pqh;
	__hc32 link_to_new_qh;

	list_for_each_entry_reverse(pqh, &uhci->skel_async_qh->node, node) {
		if (pqh->skel <= qh->skel)
			break;
	}
	list_add(&qh->node, &pqh->node);

	
	qh->link = pqh->link;
	wmb();
	link_to_new_qh = LINK_TO_QH(uhci, qh);
	pqh->link = link_to_new_qh;

	if (pqh->skel < SKEL_FSBR && qh->skel >= SKEL_FSBR)
		uhci->skel_term_qh->link = link_to_new_qh;
}

static void uhci_activate_qh(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	WARN_ON(list_empty(&qh->queue));

	if (qh_element(qh) == UHCI_PTR_TERM(uhci)) {
		struct urb_priv *urbp = list_entry(qh->queue.next,
				struct urb_priv, node);
		struct uhci_td *td = list_entry(urbp->td_list.next,
				struct uhci_td, list);

		qh->element = LINK_TO_TD(uhci, td);
	}

	
	qh->wait_expired = 0;
	qh->advance_jiffies = jiffies;

	if (qh->state == QH_STATE_ACTIVE)
		return;
	qh->state = QH_STATE_ACTIVE;

	if (qh == uhci->next_qh)
		uhci->next_qh = list_entry(qh->node.next, struct uhci_qh,
				node);
	list_del(&qh->node);

	if (qh->skel == SKEL_ISO)
		link_iso(uhci, qh);
	else if (qh->skel < SKEL_ASYNC)
		link_interrupt(uhci, qh);
	else
		link_async(uhci, qh);
}

static void unlink_interrupt(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct uhci_qh *pqh;

	pqh = list_entry(qh->node.prev, struct uhci_qh, node);
	pqh->link = qh->link;
	mb();
}

static void unlink_async(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct uhci_qh *pqh;
	__hc32 link_to_next_qh = qh->link;

	pqh = list_entry(qh->node.prev, struct uhci_qh, node);
	pqh->link = link_to_next_qh;

	if (pqh->skel < SKEL_FSBR && qh->skel >= SKEL_FSBR)
		uhci->skel_term_qh->link = link_to_next_qh;
	mb();
}

static void uhci_unlink_qh(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	if (qh->state == QH_STATE_UNLINKING)
		return;
	WARN_ON(qh->state != QH_STATE_ACTIVE || !qh->udev);
	qh->state = QH_STATE_UNLINKING;

	
	if (qh->skel == SKEL_ISO)
		;
	else if (qh->skel < SKEL_ASYNC)
		unlink_interrupt(uhci, qh);
	else
		unlink_async(uhci, qh);

	uhci_get_current_frame_number(uhci);
	qh->unlink_frame = uhci->frame_number;

	
	if (list_empty(&uhci->skel_unlink_qh->node) || uhci->is_stopped)
		uhci_set_next_interrupt(uhci);

	
	if (qh == uhci->next_qh)
		uhci->next_qh = list_entry(qh->node.next, struct uhci_qh,
				node);
	list_move_tail(&qh->node, &uhci->skel_unlink_qh->node);
}

static void uhci_make_qh_idle(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	WARN_ON(qh->state == QH_STATE_ACTIVE);

	if (qh == uhci->next_qh)
		uhci->next_qh = list_entry(qh->node.next, struct uhci_qh,
				node);
	list_move(&qh->node, &uhci->idle_qh_list);
	qh->state = QH_STATE_IDLE;

	
	if (qh->post_td) {
		uhci_free_td(uhci, qh->post_td);
		qh->post_td = NULL;
	}

	
	if (uhci->num_waiting)
		wake_up_all(&uhci->waitqh);
}

static int uhci_highest_load(struct uhci_hcd *uhci, int phase, int period)
{
	int highest_load = uhci->load[phase];

	for (phase += period; phase < MAX_PHASE; phase += period)
		highest_load = max_t(int, highest_load, uhci->load[phase]);
	return highest_load;
}

static int uhci_check_bandwidth(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	int minimax_load;

	if (qh->phase >= 0)
		minimax_load = uhci_highest_load(uhci, qh->phase, qh->period);
	else {
		int phase, load;
		int max_phase = min_t(int, MAX_PHASE, qh->period);

		qh->phase = 0;
		minimax_load = uhci_highest_load(uhci, qh->phase, qh->period);
		for (phase = 1; phase < max_phase; ++phase) {
			load = uhci_highest_load(uhci, phase, qh->period);
			if (load < minimax_load) {
				minimax_load = load;
				qh->phase = phase;
			}
		}
	}

	
	if (minimax_load + qh->load > 900) {
		dev_dbg(uhci_dev(uhci), "bandwidth allocation failed: "
				"period %d, phase %d, %d + %d us\n",
				qh->period, qh->phase, minimax_load, qh->load);
		return -ENOSPC;
	}
	return 0;
}

static void uhci_reserve_bandwidth(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	int i;
	int load = qh->load;
	char *p = "??";

	for (i = qh->phase; i < MAX_PHASE; i += qh->period) {
		uhci->load[i] += load;
		uhci->total_load += load;
	}
	uhci_to_hcd(uhci)->self.bandwidth_allocated =
			uhci->total_load / MAX_PHASE;
	switch (qh->type) {
	case USB_ENDPOINT_XFER_INT:
		++uhci_to_hcd(uhci)->self.bandwidth_int_reqs;
		p = "INT";
		break;
	case USB_ENDPOINT_XFER_ISOC:
		++uhci_to_hcd(uhci)->self.bandwidth_isoc_reqs;
		p = "ISO";
		break;
	}
	qh->bandwidth_reserved = 1;
	dev_dbg(uhci_dev(uhci),
			"%s dev %d ep%02x-%s, period %d, phase %d, %d us\n",
			"reserve", qh->udev->devnum,
			qh->hep->desc.bEndpointAddress, p,
			qh->period, qh->phase, load);
}

static void uhci_release_bandwidth(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	int i;
	int load = qh->load;
	char *p = "??";

	for (i = qh->phase; i < MAX_PHASE; i += qh->period) {
		uhci->load[i] -= load;
		uhci->total_load -= load;
	}
	uhci_to_hcd(uhci)->self.bandwidth_allocated =
			uhci->total_load / MAX_PHASE;
	switch (qh->type) {
	case USB_ENDPOINT_XFER_INT:
		--uhci_to_hcd(uhci)->self.bandwidth_int_reqs;
		p = "INT";
		break;
	case USB_ENDPOINT_XFER_ISOC:
		--uhci_to_hcd(uhci)->self.bandwidth_isoc_reqs;
		p = "ISO";
		break;
	}
	qh->bandwidth_reserved = 0;
	dev_dbg(uhci_dev(uhci),
			"%s dev %d ep%02x-%s, period %d, phase %d, %d us\n",
			"release", qh->udev->devnum,
			qh->hep->desc.bEndpointAddress, p,
			qh->period, qh->phase, load);
}

static inline struct urb_priv *uhci_alloc_urb_priv(struct uhci_hcd *uhci,
		struct urb *urb)
{
	struct urb_priv *urbp;

	urbp = kmem_cache_zalloc(uhci_up_cachep, GFP_ATOMIC);
	if (!urbp)
		return NULL;

	urbp->urb = urb;
	urb->hcpriv = urbp;

	INIT_LIST_HEAD(&urbp->node);
	INIT_LIST_HEAD(&urbp->td_list);

	return urbp;
}

static void uhci_free_urb_priv(struct uhci_hcd *uhci,
		struct urb_priv *urbp)
{
	struct uhci_td *td, *tmp;

	if (!list_empty(&urbp->node))
		dev_WARN(uhci_dev(uhci), "urb %p still on QH's list!\n",
				urbp->urb);

	list_for_each_entry_safe(td, tmp, &urbp->td_list, list) {
		uhci_remove_td_from_urbp(td);
		uhci_free_td(uhci, td);
	}

	kmem_cache_free(uhci_up_cachep, urbp);
}

static int uhci_map_status(int status, int dir_out)
{
	if (!status)
		return 0;
	if (status & TD_CTRL_BITSTUFF)			
		return -EPROTO;
	if (status & TD_CTRL_CRCTIMEO) {		
		if (dir_out)
			return -EPROTO;
		else
			return -EILSEQ;
	}
	if (status & TD_CTRL_BABBLE)			
		return -EOVERFLOW;
	if (status & TD_CTRL_DBUFERR)			
		return -ENOSR;
	if (status & TD_CTRL_STALLED)			
		return -EPIPE;
	return 0;
}

static int uhci_submit_control(struct uhci_hcd *uhci, struct urb *urb,
		struct uhci_qh *qh)
{
	struct uhci_td *td;
	unsigned long destination, status;
	int maxsze = usb_endpoint_maxp(&qh->hep->desc);
	int len = urb->transfer_buffer_length;
	dma_addr_t data = urb->transfer_dma;
	__hc32 *plink;
	struct urb_priv *urbp = urb->hcpriv;
	int skel;

	
	destination = (urb->pipe & PIPE_DEVEP_MASK) | USB_PID_SETUP;

	
	status = uhci_maxerr(3);
	if (urb->dev->speed == USB_SPEED_LOW)
		status |= TD_CTRL_LS;

	td = qh->dummy_td;
	uhci_add_td_to_urbp(td, urbp);
	uhci_fill_td(uhci, td, status, destination | uhci_explen(8),
			urb->setup_dma);
	plink = &td->link;
	status |= TD_CTRL_ACTIVE;

	if (usb_pipeout(urb->pipe) || len == 0)
		destination ^= (USB_PID_SETUP ^ USB_PID_OUT);
	else {
		destination ^= (USB_PID_SETUP ^ USB_PID_IN);
		status |= TD_CTRL_SPD;
	}

	while (len > 0) {
		int pktsze = maxsze;

		if (len <= pktsze) {		
			pktsze = len;
			status &= ~TD_CTRL_SPD;
		}

		td = uhci_alloc_td(uhci);
		if (!td)
			goto nomem;
		*plink = LINK_TO_TD(uhci, td);

		
		destination ^= TD_TOKEN_TOGGLE;

		uhci_add_td_to_urbp(td, urbp);
		uhci_fill_td(uhci, td, status,
			destination | uhci_explen(pktsze), data);
		plink = &td->link;

		data += pktsze;
		len -= pktsze;
	}

	td = uhci_alloc_td(uhci);
	if (!td)
		goto nomem;
	*plink = LINK_TO_TD(uhci, td);

	
	destination ^= (USB_PID_IN ^ USB_PID_OUT);
	destination |= TD_TOKEN_TOGGLE;		

	uhci_add_td_to_urbp(td, urbp);
	uhci_fill_td(uhci, td, status | TD_CTRL_IOC,
			destination | uhci_explen(0), 0);
	plink = &td->link;

	td = uhci_alloc_td(uhci);
	if (!td)
		goto nomem;
	*plink = LINK_TO_TD(uhci, td);

	uhci_fill_td(uhci, td, 0, USB_PID_OUT | uhci_explen(0), 0);
	wmb();
	qh->dummy_td->status |= cpu_to_hc32(uhci, TD_CTRL_ACTIVE);
	qh->dummy_td = td;

	if (urb->dev->speed == USB_SPEED_LOW ||
			urb->dev->state != USB_STATE_CONFIGURED)
		skel = SKEL_LS_CONTROL;
	else {
		skel = SKEL_FS_CONTROL;
		uhci_add_fsbr(uhci, urb);
	}
	if (qh->state != QH_STATE_ACTIVE)
		qh->skel = skel;
	return 0;

nomem:
	
	uhci_remove_td_from_urbp(qh->dummy_td);
	return -ENOMEM;
}

static int uhci_submit_common(struct uhci_hcd *uhci, struct urb *urb,
		struct uhci_qh *qh)
{
	struct uhci_td *td;
	unsigned long destination, status;
	int maxsze = usb_endpoint_maxp(&qh->hep->desc);
	int len = urb->transfer_buffer_length;
	int this_sg_len;
	dma_addr_t data;
	__hc32 *plink;
	struct urb_priv *urbp = urb->hcpriv;
	unsigned int toggle;
	struct scatterlist  *sg;
	int i;

	if (len < 0)
		return -EINVAL;

	
	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid(urb->pipe);
	toggle = usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			 usb_pipeout(urb->pipe));

	
	status = uhci_maxerr(3);
	if (urb->dev->speed == USB_SPEED_LOW)
		status |= TD_CTRL_LS;
	if (usb_pipein(urb->pipe))
		status |= TD_CTRL_SPD;

	i = urb->num_mapped_sgs;
	if (len > 0 && i > 0) {
		sg = urb->sg;
		data = sg_dma_address(sg);

		this_sg_len = min_t(int, sg_dma_len(sg), len);
	} else {
		sg = NULL;
		data = urb->transfer_dma;
		this_sg_len = len;
	}
	plink = NULL;
	td = qh->dummy_td;
	for (;;) {	
		int pktsze = maxsze;

		if (len <= pktsze) {		
			pktsze = len;
			if (!(urb->transfer_flags & URB_SHORT_NOT_OK))
				status &= ~TD_CTRL_SPD;
		}

		if (plink) {
			td = uhci_alloc_td(uhci);
			if (!td)
				goto nomem;
			*plink = LINK_TO_TD(uhci, td);
		}
		uhci_add_td_to_urbp(td, urbp);
		uhci_fill_td(uhci, td, status,
				destination | uhci_explen(pktsze) |
					(toggle << TD_TOKEN_TOGGLE_SHIFT),
				data);
		plink = &td->link;
		status |= TD_CTRL_ACTIVE;

		toggle ^= 1;
		data += pktsze;
		this_sg_len -= pktsze;
		len -= maxsze;
		if (this_sg_len <= 0) {
			if (--i <= 0 || len <= 0)
				break;
			sg = sg_next(sg);
			data = sg_dma_address(sg);
			this_sg_len = min_t(int, sg_dma_len(sg), len);
		}
	}

	if ((urb->transfer_flags & URB_ZERO_PACKET) &&
			usb_pipeout(urb->pipe) && len == 0 &&
			urb->transfer_buffer_length > 0) {
		td = uhci_alloc_td(uhci);
		if (!td)
			goto nomem;
		*plink = LINK_TO_TD(uhci, td);

		uhci_add_td_to_urbp(td, urbp);
		uhci_fill_td(uhci, td, status,
				destination | uhci_explen(0) |
					(toggle << TD_TOKEN_TOGGLE_SHIFT),
				data);
		plink = &td->link;

		toggle ^= 1;
	}

	td->status |= cpu_to_hc32(uhci, TD_CTRL_IOC);

	td = uhci_alloc_td(uhci);
	if (!td)
		goto nomem;
	*plink = LINK_TO_TD(uhci, td);

	uhci_fill_td(uhci, td, 0, USB_PID_OUT | uhci_explen(0), 0);
	wmb();
	qh->dummy_td->status |= cpu_to_hc32(uhci, TD_CTRL_ACTIVE);
	qh->dummy_td = td;

	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe),
			usb_pipeout(urb->pipe), toggle);
	return 0;

nomem:
	
	uhci_remove_td_from_urbp(qh->dummy_td);
	return -ENOMEM;
}

static int uhci_submit_bulk(struct uhci_hcd *uhci, struct urb *urb,
		struct uhci_qh *qh)
{
	int ret;

	
	if (urb->dev->speed == USB_SPEED_LOW)
		return -EINVAL;

	if (qh->state != QH_STATE_ACTIVE)
		qh->skel = SKEL_BULK;
	ret = uhci_submit_common(uhci, urb, qh);
	if (ret == 0)
		uhci_add_fsbr(uhci, urb);
	return ret;
}

static int uhci_submit_interrupt(struct uhci_hcd *uhci, struct urb *urb,
		struct uhci_qh *qh)
{
	int ret;


	if (!qh->bandwidth_reserved) {
		int exponent;

		
		for (exponent = 7; exponent >= 0; --exponent) {
			if ((1 << exponent) <= urb->interval)
				break;
		}
		if (exponent < 0)
			return -EINVAL;

		
		do {
			qh->period = 1 << exponent;
			qh->skel = SKEL_INDEX(exponent);

			qh->phase = (qh->period / 2) & (MAX_PHASE - 1);
			ret = uhci_check_bandwidth(uhci, qh);
		} while (ret != 0 && --exponent >= 0);
		if (ret)
			return ret;
	} else if (qh->period > urb->interval)
		return -EINVAL;		

	ret = uhci_submit_common(uhci, urb, qh);
	if (ret == 0) {
		urb->interval = qh->period;
		if (!qh->bandwidth_reserved)
			uhci_reserve_bandwidth(uhci, qh);
	}
	return ret;
}

static int uhci_fixup_short_transfer(struct uhci_hcd *uhci,
		struct uhci_qh *qh, struct urb_priv *urbp)
{
	struct uhci_td *td;
	struct list_head *tmp;
	int ret;

	td = list_entry(urbp->td_list.prev, struct uhci_td, list);
	if (qh->type == USB_ENDPOINT_XFER_CONTROL) {

		WARN_ON(list_empty(&urbp->td_list));
		qh->element = LINK_TO_TD(uhci, td);
		tmp = td->list.prev;
		ret = -EINPROGRESS;

	} else {

		qh->initial_toggle =
			uhci_toggle(td_token(uhci, qh->post_td)) ^ 1;
		uhci_fixup_toggles(uhci, qh, 1);

		if (list_empty(&urbp->td_list))
			td = qh->post_td;
		qh->element = td->link;
		tmp = urbp->td_list.prev;
		ret = 0;
	}

	
	while (tmp != &urbp->td_list) {
		td = list_entry(tmp, struct uhci_td, list);
		tmp = tmp->prev;

		uhci_remove_td_from_urbp(td);
		uhci_free_td(uhci, td);
	}
	return ret;
}

static int uhci_result_common(struct uhci_hcd *uhci, struct urb *urb)
{
	struct urb_priv *urbp = urb->hcpriv;
	struct uhci_qh *qh = urbp->qh;
	struct uhci_td *td, *tmp;
	unsigned status;
	int ret = 0;

	list_for_each_entry_safe(td, tmp, &urbp->td_list, list) {
		unsigned int ctrlstat;
		int len;

		ctrlstat = td_status(uhci, td);
		status = uhci_status_bits(ctrlstat);
		if (status & TD_CTRL_ACTIVE)
			return -EINPROGRESS;

		len = uhci_actual_length(ctrlstat);
		urb->actual_length += len;

		if (status) {
			ret = uhci_map_status(status,
					uhci_packetout(td_token(uhci, td)));
			if ((debug == 1 && ret != -EPIPE) || debug > 1) {
				
				dev_dbg(&urb->dev->dev,
						"%s: failed with status %x\n",
						__func__, status);

				if (debug > 1 && errbuf) {
					
					uhci_show_qh(uhci, urbp->qh, errbuf,
							ERRBUF_LEN, 0);
					lprintk(errbuf);
				}
			}

		
		} else if (len < uhci_expected_length(td_token(uhci, td))) {

			if (qh->type == USB_ENDPOINT_XFER_CONTROL) {
				if (td->list.next != urbp->td_list.prev)
					ret = 1;
			}

			
			else if (urb->transfer_flags & URB_SHORT_NOT_OK)
				ret = -EREMOTEIO;

			
			else if (&td->list != urbp->td_list.prev)
				ret = 1;
		}

		uhci_remove_td_from_urbp(td);
		if (qh->post_td)
			uhci_free_td(uhci, qh->post_td);
		qh->post_td = td;

		if (ret != 0)
			goto err;
	}
	return ret;

err:
	if (ret < 0) {
		qh->element = UHCI_PTR_TERM(uhci);
		qh->is_stopped = 1;
		qh->needs_fixup = (qh->type != USB_ENDPOINT_XFER_CONTROL);
		qh->initial_toggle = uhci_toggle(td_token(uhci, td)) ^
				(ret == -EREMOTEIO);

	} else		
		ret = uhci_fixup_short_transfer(uhci, qh, urbp);
	return ret;
}

static int uhci_submit_isochronous(struct uhci_hcd *uhci, struct urb *urb,
		struct uhci_qh *qh)
{
	struct uhci_td *td = NULL;	
	int i, frame;
	unsigned long destination, status;
	struct urb_priv *urbp = (struct urb_priv *) urb->hcpriv;

	
	if (urb->interval >= UHCI_NUMFRAMES ||
			urb->number_of_packets >= UHCI_NUMFRAMES)
		return -EFBIG;

	
	if (!qh->bandwidth_reserved) {
		qh->period = urb->interval;
		if (urb->transfer_flags & URB_ISO_ASAP) {
			qh->phase = -1;		
			i = uhci_check_bandwidth(uhci, qh);
			if (i)
				return i;

			
			uhci_get_current_frame_number(uhci);
			frame = uhci->frame_number + 10;

			urb->start_frame = frame + ((qh->phase - frame) &
					(qh->period - 1));
		} else {
			i = urb->start_frame - uhci->last_iso_frame;
			if (i <= 0 || i >= UHCI_NUMFRAMES)
				return -EINVAL;
			qh->phase = urb->start_frame & (qh->period - 1);
			i = uhci_check_bandwidth(uhci, qh);
			if (i)
				return i;
		}

	} else if (qh->period != urb->interval) {
		return -EINVAL;		

	} else {
		
		if (list_empty(&qh->queue)) {
			frame = qh->iso_frame;
		} else {
			struct urb *lurb;

			lurb = list_entry(qh->queue.prev,
					struct urb_priv, node)->urb;
			frame = lurb->start_frame +
					lurb->number_of_packets *
					lurb->interval;
		}
		if (urb->transfer_flags & URB_ISO_ASAP) {
			uhci_get_current_frame_number(uhci);
			if (uhci_frame_before_eq(frame, uhci->frame_number)) {
				frame = uhci->frame_number + 1;
				frame += ((qh->phase - frame) &
					(qh->period - 1));
			}
		}	
		urb->start_frame = frame;
	}

	
	if (uhci_frame_before_eq(uhci->last_iso_frame + UHCI_NUMFRAMES,
			urb->start_frame + urb->number_of_packets *
				urb->interval))
		return -EFBIG;

	status = TD_CTRL_ACTIVE | TD_CTRL_IOS;
	destination = (urb->pipe & PIPE_DEVEP_MASK) | usb_packetid(urb->pipe);

	for (i = 0; i < urb->number_of_packets; i++) {
		td = uhci_alloc_td(uhci);
		if (!td)
			return -ENOMEM;

		uhci_add_td_to_urbp(td, urbp);
		uhci_fill_td(uhci, td, status, destination |
				uhci_explen(urb->iso_frame_desc[i].length),
				urb->transfer_dma +
					urb->iso_frame_desc[i].offset);
	}

	
	td->status |= cpu_to_hc32(uhci, TD_CTRL_IOC);

	
	frame = urb->start_frame;
	list_for_each_entry(td, &urbp->td_list, list) {
		uhci_insert_td_in_frame_list(uhci, td, frame);
		frame += qh->period;
	}

	if (list_empty(&qh->queue)) {
		qh->iso_packet_desc = &urb->iso_frame_desc[0];
		qh->iso_frame = urb->start_frame;
	}

	qh->skel = SKEL_ISO;
	if (!qh->bandwidth_reserved)
		uhci_reserve_bandwidth(uhci, qh);
	return 0;
}

static int uhci_result_isochronous(struct uhci_hcd *uhci, struct urb *urb)
{
	struct uhci_td *td, *tmp;
	struct urb_priv *urbp = urb->hcpriv;
	struct uhci_qh *qh = urbp->qh;

	list_for_each_entry_safe(td, tmp, &urbp->td_list, list) {
		unsigned int ctrlstat;
		int status;
		int actlength;

		if (uhci_frame_before_eq(uhci->cur_iso_frame, qh->iso_frame))
			return -EINPROGRESS;

		uhci_remove_tds_from_frame(uhci, qh->iso_frame);

		ctrlstat = td_status(uhci, td);
		if (ctrlstat & TD_CTRL_ACTIVE) {
			status = -EXDEV;	
		} else {
			status = uhci_map_status(uhci_status_bits(ctrlstat),
					usb_pipeout(urb->pipe));
			actlength = uhci_actual_length(ctrlstat);

			urb->actual_length += actlength;
			qh->iso_packet_desc->actual_length = actlength;
			qh->iso_packet_desc->status = status;
		}
		if (status)
			urb->error_count++;

		uhci_remove_td_from_urbp(td);
		uhci_free_td(uhci, td);
		qh->iso_frame += qh->period;
		++qh->iso_packet_desc;
	}
	return 0;
}

static int uhci_urb_enqueue(struct usb_hcd *hcd,
		struct urb *urb, gfp_t mem_flags)
{
	int ret;
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	unsigned long flags;
	struct urb_priv *urbp;
	struct uhci_qh *qh;

	spin_lock_irqsave(&uhci->lock, flags);

	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (ret)
		goto done_not_linked;

	ret = -ENOMEM;
	urbp = uhci_alloc_urb_priv(uhci, urb);
	if (!urbp)
		goto done;

	if (urb->ep->hcpriv)
		qh = urb->ep->hcpriv;
	else {
		qh = uhci_alloc_qh(uhci, urb->dev, urb->ep);
		if (!qh)
			goto err_no_qh;
	}
	urbp->qh = qh;

	switch (qh->type) {
	case USB_ENDPOINT_XFER_CONTROL:
		ret = uhci_submit_control(uhci, urb, qh);
		break;
	case USB_ENDPOINT_XFER_BULK:
		ret = uhci_submit_bulk(uhci, urb, qh);
		break;
	case USB_ENDPOINT_XFER_INT:
		ret = uhci_submit_interrupt(uhci, urb, qh);
		break;
	case USB_ENDPOINT_XFER_ISOC:
		urb->error_count = 0;
		ret = uhci_submit_isochronous(uhci, urb, qh);
		break;
	}
	if (ret != 0)
		goto err_submit_failed;

	
	list_add_tail(&urbp->node, &qh->queue);

	if (qh->queue.next == &urbp->node && !qh->is_stopped) {
		uhci_activate_qh(uhci, qh);
		uhci_urbp_wants_fsbr(uhci, urbp);
	}
	goto done;

err_submit_failed:
	if (qh->state == QH_STATE_IDLE)
		uhci_make_qh_idle(uhci, qh);	
err_no_qh:
	uhci_free_urb_priv(uhci, urbp);
done:
	if (ret)
		usb_hcd_unlink_urb_from_ep(hcd, urb);
done_not_linked:
	spin_unlock_irqrestore(&uhci->lock, flags);
	return ret;
}

static int uhci_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	unsigned long flags;
	struct uhci_qh *qh;
	int rc;

	spin_lock_irqsave(&uhci->lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc)
		goto done;

	qh = ((struct urb_priv *) urb->hcpriv)->qh;

	
	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		uhci_unlink_isochronous_tds(uhci, urb);
		mb();

		
		uhci_get_current_frame_number(uhci);
		if (uhci_frame_before_eq(urb->start_frame, uhci->frame_number))
			qh->unlink_frame = uhci->frame_number;
	}

	uhci_unlink_qh(uhci, qh);

done:
	spin_unlock_irqrestore(&uhci->lock, flags);
	return rc;
}

static void uhci_giveback_urb(struct uhci_hcd *uhci, struct uhci_qh *qh,
		struct urb *urb, int status)
__releases(uhci->lock)
__acquires(uhci->lock)
{
	struct urb_priv *urbp = (struct urb_priv *) urb->hcpriv;

	if (qh->type == USB_ENDPOINT_XFER_CONTROL) {

		urb->actual_length -= min_t(u32, 8, urb->actual_length);
	}

	else if (qh->type == USB_ENDPOINT_XFER_ISOC &&
			urbp->node.prev == &qh->queue &&
			urbp->node.next != &qh->queue) {
		struct urb *nurb = list_entry(urbp->node.next,
				struct urb_priv, node)->urb;

		qh->iso_packet_desc = &nurb->iso_frame_desc[0];
		qh->iso_frame = nurb->start_frame;
	}

	list_del_init(&urbp->node);
	if (list_empty(&qh->queue) && qh->needs_fixup) {
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe),
				usb_pipeout(urb->pipe), qh->initial_toggle);
		qh->needs_fixup = 0;
	}

	uhci_free_urb_priv(uhci, urbp);
	usb_hcd_unlink_urb_from_ep(uhci_to_hcd(uhci), urb);

	spin_unlock(&uhci->lock);
	usb_hcd_giveback_urb(uhci_to_hcd(uhci), urb, status);
	spin_lock(&uhci->lock);

	if (list_empty(&qh->queue)) {
		uhci_unlink_qh(uhci, qh);
		if (qh->bandwidth_reserved)
			uhci_release_bandwidth(uhci, qh);
	}
}

#define QH_FINISHED_UNLINKING(qh)			\
		(qh->state == QH_STATE_UNLINKING &&	\
		uhci->frame_number + uhci->is_stopped != qh->unlink_frame)

static void uhci_scan_qh(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct urb_priv *urbp;
	struct urb *urb;
	int status;

	while (!list_empty(&qh->queue)) {
		urbp = list_entry(qh->queue.next, struct urb_priv, node);
		urb = urbp->urb;

		if (qh->type == USB_ENDPOINT_XFER_ISOC)
			status = uhci_result_isochronous(uhci, urb);
		else
			status = uhci_result_common(uhci, urb);
		if (status == -EINPROGRESS)
			break;

		if (urb->unlinked) {
			if (QH_FINISHED_UNLINKING(qh))
				qh->is_stopped = 1;
			else if (!qh->is_stopped)
				return;
		}

		uhci_giveback_urb(uhci, qh, urb, status);
		if (status < 0)
			break;
	}

	if (QH_FINISHED_UNLINKING(qh))
		qh->is_stopped = 1;
	else if (!qh->is_stopped)
		return;

	
restart:
	list_for_each_entry(urbp, &qh->queue, node) {
		urb = urbp->urb;
		if (urb->unlinked) {

			if (!uhci_cleanup_queue(uhci, qh, urb)) {
				qh->is_stopped = 0;
				return;
			}
			uhci_giveback_urb(uhci, qh, urb, 0);
			goto restart;
		}
	}
	qh->is_stopped = 0;

	if (!list_empty(&qh->queue)) {
		if (qh->needs_fixup)
			uhci_fixup_toggles(uhci, qh, 0);

		urbp = list_entry(qh->queue.next, struct urb_priv, node);
		if (urbp->fsbr && qh->wait_expired) {
			struct uhci_td *td = list_entry(urbp->td_list.next,
					struct uhci_td, list);

			td->status |= cpu_to_hc32(uhci, TD_CTRL_IOC);
		}

		uhci_activate_qh(uhci, qh);
	}

	else if (QH_FINISHED_UNLINKING(qh))
		uhci_make_qh_idle(uhci, qh);
}

static int uhci_advance_check(struct uhci_hcd *uhci, struct uhci_qh *qh)
{
	struct urb_priv *urbp = NULL;
	struct uhci_td *td;
	int ret = 1;
	unsigned status;

	if (qh->type == USB_ENDPOINT_XFER_ISOC)
		goto done;

	if (qh->state != QH_STATE_ACTIVE) {
		urbp = NULL;
		status = 0;

	} else {
		urbp = list_entry(qh->queue.next, struct urb_priv, node);
		td = list_entry(urbp->td_list.next, struct uhci_td, list);
		status = td_status(uhci, td);
		if (!(status & TD_CTRL_ACTIVE)) {

			
			qh->wait_expired = 0;
			qh->advance_jiffies = jiffies;
			goto done;
		}
		ret = uhci->is_stopped;
	}

	
	if (qh->wait_expired)
		goto done;

	if (time_after(jiffies, qh->advance_jiffies + QH_WAIT_TIMEOUT)) {

		
		if (qh->post_td && qh_element(qh) ==
			LINK_TO_TD(uhci, qh->post_td)) {
			qh->element = qh->post_td->link;
			qh->advance_jiffies = jiffies;
			ret = 1;
			goto done;
		}

		qh->wait_expired = 1;

		if (urbp && urbp->fsbr && !(status & TD_CTRL_IOC))
			uhci_unlink_qh(uhci, qh);

	} else {
		
		if (urbp)
			uhci_urbp_wants_fsbr(uhci, urbp);
	}

done:
	return ret;
}

static void uhci_scan_schedule(struct uhci_hcd *uhci)
{
	int i;
	struct uhci_qh *qh;

	
	if (uhci->scan_in_progress) {
		uhci->need_rescan = 1;
		return;
	}
	uhci->scan_in_progress = 1;
rescan:
	uhci->need_rescan = 0;
	uhci->fsbr_is_wanted = 0;

	uhci_clear_next_interrupt(uhci);
	uhci_get_current_frame_number(uhci);
	uhci->cur_iso_frame = uhci->frame_number;

	
	for (i = 0; i < UHCI_NUM_SKELQH - 1; ++i) {
		uhci->next_qh = list_entry(uhci->skelqh[i]->node.next,
				struct uhci_qh, node);
		while ((qh = uhci->next_qh) != uhci->skelqh[i]) {
			uhci->next_qh = list_entry(qh->node.next,
					struct uhci_qh, node);

			if (uhci_advance_check(uhci, qh)) {
				uhci_scan_qh(uhci, qh);
				if (qh->state == QH_STATE_ACTIVE) {
					uhci_urbp_wants_fsbr(uhci,
	list_entry(qh->queue.next, struct urb_priv, node));
				}
			}
		}
	}

	uhci->last_iso_frame = uhci->cur_iso_frame;
	if (uhci->need_rescan)
		goto rescan;
	uhci->scan_in_progress = 0;

	if (uhci->fsbr_is_on && !uhci->fsbr_is_wanted &&
			!uhci->fsbr_expiring) {
		uhci->fsbr_expiring = 1;
		mod_timer(&uhci->fsbr_timer, jiffies + FSBR_OFF_DELAY);
	}

	if (list_empty(&uhci->skel_unlink_qh->node))
		uhci_clear_next_interrupt(uhci);
	else
		uhci_set_next_interrupt(uhci);
}
