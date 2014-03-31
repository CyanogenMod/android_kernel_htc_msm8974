/*
 * Copyright (c) 2006, 2007, 2008 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "ipath_kernel.h"
#include "ipath_verbs.h"
#include "ipath_common.h"


void ipath_disarm_senderrbufs(struct ipath_devdata *dd)
{
	u32 piobcnt;
	unsigned long sbuf[4];
	piobcnt = dd->ipath_piobcnt2k + dd->ipath_piobcnt4k;
	
	sbuf[0] = ipath_read_kreg64(
		dd, dd->ipath_kregs->kr_sendbuffererror);
	sbuf[1] = ipath_read_kreg64(
		dd, dd->ipath_kregs->kr_sendbuffererror + 1);
	if (piobcnt > 128)
		sbuf[2] = ipath_read_kreg64(
			dd, dd->ipath_kregs->kr_sendbuffererror + 2);
	if (piobcnt > 192)
		sbuf[3] = ipath_read_kreg64(
			dd, dd->ipath_kregs->kr_sendbuffererror + 3);
	else
		sbuf[3] = 0;

	if (sbuf[0] || sbuf[1] || (piobcnt > 128 && (sbuf[2] || sbuf[3]))) {
		int i;
		if (ipath_debug & (__IPATH_PKTDBG|__IPATH_DBG) &&
			dd->ipath_lastcancel > jiffies) {
			__IPATH_DBG_WHICH(__IPATH_PKTDBG|__IPATH_DBG,
					  "SendbufErrs %lx %lx", sbuf[0],
					  sbuf[1]);
			if (ipath_debug & __IPATH_PKTDBG && piobcnt > 128)
				printk(" %lx %lx ", sbuf[2], sbuf[3]);
			printk("\n");
		}

		for (i = 0; i < piobcnt; i++)
			if (test_bit(i, sbuf))
				ipath_disarm_piobufs(dd, i, 1);
		
		dd->ipath_lastcancel = jiffies+3;
	}
}


#define E_SUM_PKTERRS \
	(INFINIPATH_E_RHDRLEN | INFINIPATH_E_RBADTID | \
	 INFINIPATH_E_RBADVERSION | INFINIPATH_E_RHDR | \
	 INFINIPATH_E_RLONGPKTLEN | INFINIPATH_E_RSHORTPKTLEN | \
	 INFINIPATH_E_RMAXPKTLEN | INFINIPATH_E_RMINPKTLEN | \
	 INFINIPATH_E_RFORMATERR | INFINIPATH_E_RUNSUPVL | \
	 INFINIPATH_E_RUNEXPCHAR | INFINIPATH_E_REBP)

#define E_SUM_ERRS \
	(INFINIPATH_E_SPIOARMLAUNCH | INFINIPATH_E_SUNEXPERRPKTNUM | \
	 INFINIPATH_E_SDROPPEDDATAPKT | INFINIPATH_E_SDROPPEDSMPPKT | \
	 INFINIPATH_E_SMAXPKTLEN | INFINIPATH_E_SUNSUPVL | \
	 INFINIPATH_E_SMINPKTLEN | INFINIPATH_E_SPKTLEN | \
	 INFINIPATH_E_INVALIDADDR)

#define E_SPKT_ERRS_IGNORE \
	 (INFINIPATH_E_SDROPPEDDATAPKT | INFINIPATH_E_SDROPPEDSMPPKT | \
	 INFINIPATH_E_SMAXPKTLEN | INFINIPATH_E_SMINPKTLEN | \
	 INFINIPATH_E_SPKTLEN)

#define E_SUM_LINK_PKTERRS \
	(INFINIPATH_E_SDROPPEDDATAPKT | INFINIPATH_E_SDROPPEDSMPPKT | \
	 INFINIPATH_E_SMINPKTLEN | INFINIPATH_E_SPKTLEN | \
	 INFINIPATH_E_RSHORTPKTLEN | INFINIPATH_E_RMINPKTLEN | \
	 INFINIPATH_E_RUNEXPCHAR)

static u64 handle_e_sum_errs(struct ipath_devdata *dd, ipath_err_t errs)
{
	u64 ignore_this_time = 0;

	ipath_disarm_senderrbufs(dd);
	if ((errs & E_SUM_LINK_PKTERRS) &&
	    !(dd->ipath_flags & IPATH_LINKACTIVE)) {
		ipath_dbg("Ignoring packet errors %llx, because link not "
			  "ACTIVE\n", (unsigned long long) errs);
		ignore_this_time = errs & E_SUM_LINK_PKTERRS;
	}

	return ignore_this_time;
}

#define INFINIPATH_HWE_TXEMEMPARITYERR_MSG(a) \
	{ \
		.mask = ( INFINIPATH_HWE_TXEMEMPARITYERR_##a <<    \
			  INFINIPATH_HWE_TXEMEMPARITYERR_SHIFT ),   \
		.msg = "TXE " #a " Memory Parity"	     \
	}
#define INFINIPATH_HWE_RXEMEMPARITYERR_MSG(a) \
	{ \
		.mask = ( INFINIPATH_HWE_RXEMEMPARITYERR_##a <<    \
			  INFINIPATH_HWE_RXEMEMPARITYERR_SHIFT ),   \
		.msg = "RXE " #a " Memory Parity"	     \
	}

static const struct ipath_hwerror_msgs ipath_generic_hwerror_msgs[] = {
	INFINIPATH_HWE_MSG(IBCBUSFRSPCPARITYERR, "IPATH2IB Parity"),
	INFINIPATH_HWE_MSG(IBCBUSTOSPCPARITYERR, "IB2IPATH Parity"),

	INFINIPATH_HWE_TXEMEMPARITYERR_MSG(PIOBUF),
	INFINIPATH_HWE_TXEMEMPARITYERR_MSG(PIOPBC),
	INFINIPATH_HWE_TXEMEMPARITYERR_MSG(PIOLAUNCHFIFO),

	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(RCVBUF),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(LOOKUPQ),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(EAGERTID),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(EXPTID),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(FLAGBUF),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(DATAINFO),
	INFINIPATH_HWE_RXEMEMPARITYERR_MSG(HDRINFO),
};

static void ipath_format_hwmsg(char *msg, size_t msgl, const char *hwmsg)
{
	strlcat(msg, "[", msgl);
	strlcat(msg, hwmsg, msgl);
	strlcat(msg, "]", msgl);
}

void ipath_format_hwerrors(u64 hwerrs,
			   const struct ipath_hwerror_msgs *hwerrmsgs,
			   size_t nhwerrmsgs,
			   char *msg, size_t msgl)
{
	int i;
	const int glen =
	    sizeof(ipath_generic_hwerror_msgs) /
	    sizeof(ipath_generic_hwerror_msgs[0]);

	for (i=0; i<glen; i++) {
		if (hwerrs & ipath_generic_hwerror_msgs[i].mask) {
			ipath_format_hwmsg(msg, msgl,
					   ipath_generic_hwerror_msgs[i].msg);
		}
	}

	for (i=0; i<nhwerrmsgs; i++) {
		if (hwerrs & hwerrmsgs[i].mask) {
			ipath_format_hwmsg(msg, msgl, hwerrmsgs[i].msg);
		}
	}
}

static char *ib_linkstate(struct ipath_devdata *dd, u64 ibcs)
{
	char *ret;
	u32 state;

	state = ipath_ib_state(dd, ibcs);
	if (state == dd->ib_init)
		ret = "Init";
	else if (state == dd->ib_arm)
		ret = "Arm";
	else if (state == dd->ib_active)
		ret = "Active";
	else
		ret = "Down";
	return ret;
}

void signal_ib_event(struct ipath_devdata *dd, enum ib_event_type ev)
{
	struct ib_event event;

	event.device = &dd->verbs_dev->ibdev;
	event.element.port_num = 1;
	event.event = ev;
	ib_dispatch_event(&event);
}

static void handle_e_ibstatuschanged(struct ipath_devdata *dd,
				     ipath_err_t errs)
{
	u32 ltstate, lstate, ibstate, lastlstate;
	u32 init = dd->ib_init;
	u32 arm = dd->ib_arm;
	u32 active = dd->ib_active;
	const u64 ibcs = ipath_read_kreg64(dd, dd->ipath_kregs->kr_ibcstatus);

	lstate = ipath_ib_linkstate(dd, ibcs); 
	ibstate = ipath_ib_state(dd, ibcs);
	
	lastlstate = ipath_ib_linkstate(dd, dd->ipath_lastibcstat);
	ltstate = ipath_ib_linktrstate(dd, ibcs); 

	if ((ltstate == INFINIPATH_IBCS_LT_STATE_RECOVERRETRAIN) ||
	    (ltstate == INFINIPATH_IBCS_LT_STATE_RECOVERWAITRMT) ||
	    (ltstate == INFINIPATH_IBCS_LT_STATE_RECOVERIDLE))
		goto done;

	if (lstate >= INFINIPATH_IBCS_L_STATE_INIT &&
		lastlstate == INFINIPATH_IBCS_L_STATE_DOWN) {
		
		if (dd->ipath_f_ib_updown(dd, 1, ibcs)) {
			
			dd->ipath_flags &= ~IPATH_IB_LINK_DISABLED;
			ipath_cdbg(LINKVERB, "LinkUp handled, skipped\n");
			goto skip_ibchange; 
		}
	} else if ((lastlstate >= INFINIPATH_IBCS_L_STATE_INIT ||
		(dd->ipath_flags & IPATH_IB_FORCE_NOTIFY)) &&
		ltstate <= INFINIPATH_IBCS_LT_STATE_CFGWAITRMT &&
		ltstate != INFINIPATH_IBCS_LT_STATE_LINKUP) {
		int handled;
		handled = dd->ipath_f_ib_updown(dd, 0, ibcs);
		dd->ipath_flags &= ~IPATH_IB_FORCE_NOTIFY;
		if (handled) {
			ipath_cdbg(LINKVERB, "LinkDown handled, skipped\n");
			goto skip_ibchange; 
		}
	}

	if ((ibstate != arm && ibstate != active) &&
	    (dd->ipath_flags & (IPATH_LINKARMED | IPATH_LINKACTIVE))) {
		dev_info(&dd->pcidev->dev, "Link state changed from %s "
			 "to %s\n", (dd->ipath_flags & IPATH_LINKARMED) ?
			 "ARM" : "ACTIVE", ib_linkstate(dd, ibcs));
	}

	if (ltstate == INFINIPATH_IBCS_LT_STATE_POLLACTIVE ||
	    ltstate == INFINIPATH_IBCS_LT_STATE_POLLQUIET) {
		u32 lastlts;
		lastlts = ipath_ib_linktrstate(dd, dd->ipath_lastibcstat);
		if (lastlts == INFINIPATH_IBCS_LT_STATE_POLLACTIVE ||
		    lastlts == INFINIPATH_IBCS_LT_STATE_POLLQUIET) {
			if (!(dd->ipath_flags & IPATH_IB_AUTONEG_INPROG) &&
			     (++dd->ipath_ibpollcnt == 40)) {
				dd->ipath_flags |= IPATH_NOCABLE;
				*dd->ipath_statusp |=
					IPATH_STATUS_IB_NOCABLE;
				ipath_cdbg(LINKVERB, "Set NOCABLE\n");
			}
			ipath_cdbg(LINKVERB, "POLL change to %s (%x)\n",
				ipath_ibcstatus_str[ltstate], ibstate);
			goto skip_ibchange;
		}
	}

	dd->ipath_ibpollcnt = 0; 
	ipath_stats.sps_iblink++;

	if (ibstate != init && dd->ipath_lastlinkrecov && ipath_linkrecovery) {
		u64 linkrecov;
		linkrecov = ipath_snap_cntr(dd,
			dd->ipath_cregs->cr_iblinkerrrecovcnt);
		if (linkrecov != dd->ipath_lastlinkrecov) {
			ipath_dbg("IB linkrecov up %Lx (%s %s) recov %Lu\n",
				(unsigned long long) ibcs,
				ib_linkstate(dd, ibcs),
				ipath_ibcstatus_str[ltstate],
				(unsigned long long) linkrecov);
			
			dd->ipath_lastlinkrecov = 0;
			ipath_set_linkstate(dd, IPATH_IB_LINKDOWN);
			goto skip_ibchange;
		}
	}

	if (ibstate == init || ibstate == arm || ibstate == active) {
		*dd->ipath_statusp &= ~IPATH_STATUS_IB_NOCABLE;
		if (ibstate == init || ibstate == arm) {
			*dd->ipath_statusp &= ~IPATH_STATUS_IB_READY;
			if (dd->ipath_flags & IPATH_LINKACTIVE)
				signal_ib_event(dd, IB_EVENT_PORT_ERR);
		}
		if (ibstate == arm) {
			dd->ipath_flags |= IPATH_LINKARMED;
			dd->ipath_flags &= ~(IPATH_LINKUNK |
				IPATH_LINKINIT | IPATH_LINKDOWN |
				IPATH_LINKACTIVE | IPATH_NOCABLE);
			ipath_hol_down(dd);
		} else  if (ibstate == init) {
			dd->ipath_flags |= IPATH_LINKINIT |
				IPATH_LINKDOWN;
			dd->ipath_flags &= ~(IPATH_LINKUNK |
				IPATH_LINKARMED | IPATH_LINKACTIVE |
				IPATH_NOCABLE);
			ipath_hol_down(dd);
		} else {  
			dd->ipath_lastlinkrecov = ipath_snap_cntr(dd,
				dd->ipath_cregs->cr_iblinkerrrecovcnt);
			*dd->ipath_statusp |=
				IPATH_STATUS_IB_READY | IPATH_STATUS_IB_CONF;
			dd->ipath_flags |= IPATH_LINKACTIVE;
			dd->ipath_flags &= ~(IPATH_LINKUNK | IPATH_LINKINIT
				| IPATH_LINKDOWN | IPATH_LINKARMED |
				IPATH_NOCABLE);
			if (dd->ipath_flags & IPATH_HAS_SEND_DMA)
				ipath_restart_sdma(dd);
			signal_ib_event(dd, IB_EVENT_PORT_ACTIVE);
			
			dd->ipath_f_setextled(dd, lstate, ltstate);
			ipath_hol_up(dd);
		}

		if (lstate == lastlstate)
			ipath_cdbg(LINKVERB, "Unchanged from last: %s "
				"(%x)\n", ib_linkstate(dd, ibcs), ibstate);
		else
			ipath_cdbg(VERBOSE, "Unit %u: link up to %s %s (%x)\n",
				  dd->ipath_unit, ib_linkstate(dd, ibcs),
				  ipath_ibcstatus_str[ltstate],  ibstate);
	} else { 
		if (dd->ipath_flags & IPATH_LINKACTIVE)
			signal_ib_event(dd, IB_EVENT_PORT_ERR);
		dd->ipath_flags |= IPATH_LINKDOWN;
		dd->ipath_flags &= ~(IPATH_LINKUNK | IPATH_LINKINIT
				     | IPATH_LINKACTIVE |
				     IPATH_LINKARMED);
		*dd->ipath_statusp &= ~IPATH_STATUS_IB_READY;
		dd->ipath_lli_counter = 0;

		if (lastlstate != INFINIPATH_IBCS_L_STATE_DOWN)
			ipath_cdbg(VERBOSE, "Unit %u link state down "
				   "(state 0x%x), from %s\n",
				   dd->ipath_unit, lstate,
				   ib_linkstate(dd, dd->ipath_lastibcstat));
		else
			ipath_cdbg(LINKVERB, "Unit %u link state changed "
				   "to %s (0x%x) from down (%x)\n",
				   dd->ipath_unit,
				   ipath_ibcstatus_str[ltstate],
				   ibstate, lastlstate);
	}

skip_ibchange:
	dd->ipath_lastibcstat = ibcs;
done:
	return;
}

static void handle_supp_msgs(struct ipath_devdata *dd,
			     unsigned supp_msgs, char *msg, u32 msgsz)
{
	if (dd->ipath_lasterror & ~INFINIPATH_E_IBSTATUSCHANGED) {
		int iserr;
		ipath_err_t mask;
		iserr = ipath_decode_err(dd, msg, msgsz,
					 dd->ipath_lasterror &
					 ~INFINIPATH_E_IBSTATUSCHANGED);

		mask = INFINIPATH_E_RRCVEGRFULL | INFINIPATH_E_RRCVHDRFULL |
			INFINIPATH_E_PKTERRS | INFINIPATH_E_SDMADISABLED;

		
		if (ipath_debug & __IPATH_DBG)
			mask &= ~INFINIPATH_E_SDMADISABLED;

		if (dd->ipath_lasterror & ~mask)
			ipath_dev_err(dd, "Suppressed %u messages for "
				      "fast-repeating errors (%s) (%llx)\n",
				      supp_msgs, msg,
				      (unsigned long long)
				      dd->ipath_lasterror);
		else {
			if (iserr)
				ipath_dbg("Suppressed %u messages for %s\n",
					  supp_msgs, msg);
			else
				ipath_cdbg(ERRPKT,
					"Suppressed %u messages for %s\n",
					  supp_msgs, msg);
		}
	}
}

static unsigned handle_frequent_errors(struct ipath_devdata *dd,
				       ipath_err_t errs, char *msg,
				       u32 msgsz, int *noprint)
{
	unsigned long nc;
	static unsigned long nextmsg_time;
	static unsigned nmsgs, supp_msgs;

	nc = jiffies;
	if (nmsgs > 10) {
		if (time_before(nc, nextmsg_time)) {
			*noprint = 1;
			if (!supp_msgs++)
				nextmsg_time = nc + HZ * 3;
		}
		else if (supp_msgs) {
			handle_supp_msgs(dd, supp_msgs, msg, msgsz);
			supp_msgs = 0;
			nmsgs = 0;
		}
	}
	else if (!nmsgs++ || time_after(nc, nextmsg_time))
		nextmsg_time = nc + HZ / 2;

	return supp_msgs;
}

static void handle_sdma_errors(struct ipath_devdata *dd, ipath_err_t errs)
{
	unsigned long flags;
	int expected;

	if (ipath_debug & __IPATH_DBG) {
		char msg[128];
		ipath_decode_err(dd, msg, sizeof msg, errs &
			INFINIPATH_E_SDMAERRS);
		ipath_dbg("errors %lx (%s)\n", (unsigned long)errs, msg);
	}
	if (ipath_debug & __IPATH_VERBDBG) {
		unsigned long tl, hd, status, lengen;
		tl = ipath_read_kreg64(dd, dd->ipath_kregs->kr_senddmatail);
		hd = ipath_read_kreg64(dd, dd->ipath_kregs->kr_senddmahead);
		status = ipath_read_kreg64(dd
			, dd->ipath_kregs->kr_senddmastatus);
		lengen = ipath_read_kreg64(dd,
			dd->ipath_kregs->kr_senddmalengen);
		ipath_cdbg(VERBOSE, "sdma tl 0x%lx hd 0x%lx status 0x%lx "
			"lengen 0x%lx\n", tl, hd, status, lengen);
	}

	spin_lock_irqsave(&dd->ipath_sdma_lock, flags);
	__set_bit(IPATH_SDMA_DISABLED, &dd->ipath_sdma_status);
	expected = test_bit(IPATH_SDMA_ABORTING, &dd->ipath_sdma_status);
	spin_unlock_irqrestore(&dd->ipath_sdma_lock, flags);
	if (!expected)
		ipath_cancel_sends(dd, 1);
}

static void handle_sdma_intr(struct ipath_devdata *dd, u64 istat)
{
	unsigned long flags;
	int expected;

	if ((istat & INFINIPATH_I_SDMAINT) &&
	    !test_bit(IPATH_SDMA_SHUTDOWN, &dd->ipath_sdma_status))
		ipath_sdma_intr(dd);

	if (istat & INFINIPATH_I_SDMADISABLED) {
		expected = test_bit(IPATH_SDMA_ABORTING,
			&dd->ipath_sdma_status);
		ipath_dbg("%s SDmaDisabled intr\n",
			expected ? "expected" : "unexpected");
		spin_lock_irqsave(&dd->ipath_sdma_lock, flags);
		__set_bit(IPATH_SDMA_DISABLED, &dd->ipath_sdma_status);
		spin_unlock_irqrestore(&dd->ipath_sdma_lock, flags);
		if (!expected)
			ipath_cancel_sends(dd, 1);
		if (!test_bit(IPATH_SDMA_SHUTDOWN, &dd->ipath_sdma_status))
			tasklet_hi_schedule(&dd->ipath_sdma_abort_task);
	}
}

static int handle_hdrq_full(struct ipath_devdata *dd)
{
	int chkerrpkts = 0;
	u32 hd, tl;
	u32 i;

	ipath_stats.sps_hdrqfull++;
	for (i = 0; i < dd->ipath_cfgports; i++) {
		struct ipath_portdata *pd = dd->ipath_pd[i];

		if (i == 0) {
			if (pd->port_head != ipath_get_hdrqtail(pd))
				chkerrpkts |= 1 << i;
			continue;
		}

		
		if (!pd || !pd->port_cnt)
			continue;

		
		if (dd->ipath_flags & IPATH_NODMA_RTAIL)
			tl = ipath_read_ureg32(dd, ur_rcvhdrtail, i);
		else
			tl = ipath_get_rcvhdrtail(pd);
		if (tl == pd->port_lastrcvhdrqtail)
			continue;

		hd = ipath_read_ureg32(dd, ur_rcvhdrhead, i);
		if (hd == (tl + 1) || (!hd && tl == dd->ipath_hdrqlast)) {
			pd->port_lastrcvhdrqtail = tl;
			pd->port_hdrqfull++;
			
			wmb();
			wake_up_interruptible(&pd->port_wait);
		}
	}

	return chkerrpkts;
}

static int handle_errors(struct ipath_devdata *dd, ipath_err_t errs)
{
	char msg[128];
	u64 ignore_this_time = 0;
	u64 iserr = 0;
	int chkerrpkts = 0, noprint = 0;
	unsigned supp_msgs;
	int log_idx;

	errs &= dd->ipath_errormask & ~dd->ipath_maskederrs;

	supp_msgs = handle_frequent_errors(dd, errs, msg, (u32)sizeof msg,
		&noprint);

	
	if (errs & INFINIPATH_E_HARDWARE) {
		
		dd->ipath_f_handle_hwerrors(dd, msg, sizeof msg);
	} else {
		u64 mask;
		for (log_idx = 0; log_idx < IPATH_EEP_LOG_CNT; ++log_idx) {
			mask = dd->ipath_eep_st_masks[log_idx].errs_to_log;
			if (errs & mask)
				ipath_inc_eeprom_err(dd, log_idx, 1);
		}
	}

	if (errs & INFINIPATH_E_SDMAERRS)
		handle_sdma_errors(dd, errs);

	if (!noprint && (errs & ~dd->ipath_e_bitsextant))
		ipath_dev_err(dd, "error interrupt with unknown errors "
			      "%llx set\n", (unsigned long long)
			      (errs & ~dd->ipath_e_bitsextant));

	if (errs & E_SUM_ERRS)
		ignore_this_time = handle_e_sum_errs(dd, errs);
	else if ((errs & E_SUM_LINK_PKTERRS) &&
	    !(dd->ipath_flags & IPATH_LINKACTIVE)) {
		ipath_dbg("Ignoring packet errors %llx, because link not "
			  "ACTIVE\n", (unsigned long long) errs);
		ignore_this_time = errs & E_SUM_LINK_PKTERRS;
	}

	if (supp_msgs == 250000) {
		int s_iserr;
		dd->ipath_maskederrs |= dd->ipath_lasterror | errs;

		dd->ipath_errormask &= ~dd->ipath_maskederrs;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask,
				 dd->ipath_errormask);
		s_iserr = ipath_decode_err(dd, msg, sizeof msg,
					   dd->ipath_maskederrs);

		if (dd->ipath_maskederrs &
		    ~(INFINIPATH_E_RRCVEGRFULL |
		      INFINIPATH_E_RRCVHDRFULL | INFINIPATH_E_PKTERRS))
			ipath_dev_err(dd, "Temporarily disabling "
			    "error(s) %llx reporting; too frequent (%s)\n",
				(unsigned long long) dd->ipath_maskederrs,
				msg);
		else {
			if (s_iserr)
				ipath_dbg("Temporarily disabling reporting "
				    "too frequent queue full errors (%s)\n",
				    msg);
			else
				ipath_cdbg(ERRPKT,
				    "Temporarily disabling reporting too"
				    " frequent packet errors (%s)\n",
				    msg);
		}

		dd->ipath_unmasktime = jiffies + HZ * 180;
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear, errs);
	if (ignore_this_time)
		errs &= ~ignore_this_time;
	if (errs & ~dd->ipath_lasterror) {
		errs &= ~dd->ipath_lasterror;
		
		dd->ipath_lasterror |= errs &
			~(INFINIPATH_E_HARDWARE |
			  INFINIPATH_E_IBSTATUSCHANGED);
	}

	if (errs & INFINIPATH_E_SENDSPECIALTRIGGER) {
		dd->ipath_spectriggerhit++;
		ipath_dbg("%lu special trigger hits\n",
			dd->ipath_spectriggerhit);
	}

	
	if ((errs & (INFINIPATH_E_SPKTLEN | INFINIPATH_E_SPIOARMLAUNCH)) &&
		dd->ipath_lastcancel > jiffies) {
		
		ipath_cdbg(VERBOSE,
			"Suppressed %s error (%llx) after sendbuf cancel\n",
			(errs &  INFINIPATH_E_SPIOARMLAUNCH) ?
			"armlaunch" : "sendpktlen", (unsigned long long)errs);
		errs &= ~(INFINIPATH_E_SPIOARMLAUNCH | INFINIPATH_E_SPKTLEN);
	}

	if (!errs)
		return 0;

	if (!noprint) {
		ipath_err_t mask;
		mask = INFINIPATH_E_IBSTATUSCHANGED |
			INFINIPATH_E_RRCVEGRFULL | INFINIPATH_E_RRCVHDRFULL |
			INFINIPATH_E_HARDWARE | INFINIPATH_E_SDMADISABLED;

		
		if (ipath_debug & __IPATH_DBG)
			mask &= ~INFINIPATH_E_SDMADISABLED;

		ipath_decode_err(dd, msg, sizeof msg, errs & ~mask);
	} else
		
		*msg = 0;

	if (errs & E_SUM_PKTERRS) {
		ipath_stats.sps_pkterrs++;
		chkerrpkts = 1;
	}
	if (errs & E_SUM_ERRS)
		ipath_stats.sps_errs++;

	if (errs & (INFINIPATH_E_RICRC | INFINIPATH_E_RVCRC)) {
		ipath_stats.sps_crcerrs++;
		chkerrpkts = 1;
	}
	iserr = errs & ~(E_SUM_PKTERRS | INFINIPATH_E_PKTERRS);


	if (errs & INFINIPATH_E_RRCVHDRFULL)
		chkerrpkts |= handle_hdrq_full(dd);
	if (errs & INFINIPATH_E_RRCVEGRFULL) {
		struct ipath_portdata *pd = dd->ipath_pd[0];

		ipath_stats.sps_etidfull++;
		if (pd->port_head != ipath_get_hdrqtail(pd))
			chkerrpkts |= 1;
	}

	if (errs & INFINIPATH_E_RIBLOSTLINK) {
		errs |= INFINIPATH_E_IBSTATUSCHANGED;
		ipath_stats.sps_iblink++;
		dd->ipath_flags |= IPATH_LINKDOWN;
		dd->ipath_flags &= ~(IPATH_LINKUNK | IPATH_LINKINIT
				     | IPATH_LINKARMED | IPATH_LINKACTIVE);
		*dd->ipath_statusp &= ~IPATH_STATUS_IB_READY;

		ipath_dbg("Lost link, link now down (%s)\n",
			ipath_ibcstatus_str[ipath_read_kreg64(dd,
			dd->ipath_kregs->kr_ibcstatus) & 0xf]);
	}
	if (errs & INFINIPATH_E_IBSTATUSCHANGED)
		handle_e_ibstatuschanged(dd, errs);

	if (errs & INFINIPATH_E_RESET) {
		if (!noprint)
			ipath_dev_err(dd, "Got reset, requires re-init "
				      "(unload and reload driver)\n");
		dd->ipath_flags &= ~IPATH_INITTED;	
		
		*dd->ipath_statusp |= IPATH_STATUS_HWERROR;
		*dd->ipath_statusp &= ~IPATH_STATUS_IB_CONF;
	}

	if (!noprint && *msg) {
		if (iserr)
			ipath_dev_err(dd, "%s error\n", msg);
	}
	if (dd->ipath_state_wanted & dd->ipath_flags) {
		ipath_cdbg(VERBOSE, "driver wanted state %x, iflags now %x, "
			   "waking\n", dd->ipath_state_wanted,
			   dd->ipath_flags);
		wake_up_interruptible(&ipath_state_wait);
	}

	return chkerrpkts;
}

/*
 * try to cleanup as much as possible for anything that might have gone
 * wrong while in freeze mode, such as pio buffers being written by user
 * processes (causing armlaunch), send errors due to going into freeze mode,
 * etc., and try to avoid causing extra interrupts while doing so.
 * Forcibly update the in-memory pioavail register copies after cleanup
 * because the chip won't do it while in freeze mode (the register values
 * themselves are kept correct).
 * Make sure that we don't lose any important interrupts by using the chip
 * feature that says that writing 0 to a bit in *clear that is set in
 * *status will cause an interrupt to be generated again (if allowed by
 * the *mask value).
 */
void ipath_clear_freeze(struct ipath_devdata *dd)
{
	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask, 0ULL);

	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask, 0ULL);

	ipath_cancel_sends(dd, 1);

	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_control,
			 dd->ipath_control);
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);

	
	ipath_force_pio_avail_update(dd);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrclear, 0ULL);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear,
		E_SPKT_ERRS_IGNORE);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask,
		dd->ipath_errormask);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask, -1LL);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear, 0ULL);
}



static noinline void ipath_bad_intr(struct ipath_devdata *dd, u32 *unexpectp)
{

	if (++*unexpectp > 100) {
		if (++*unexpectp > 105) {
			if (dd->pcidev && dd->ipath_irq) {
				ipath_dev_err(dd, "Now %u unexpected "
					      "interrupts, unregistering "
					      "interrupt handler\n",
					      *unexpectp);
				ipath_dbg("free_irq of irq %d\n",
					  dd->ipath_irq);
				dd->ipath_f_free_irq(dd);
			}
		}
		if (ipath_read_ireg(dd, dd->ipath_kregs->kr_intmask)) {
			ipath_dev_err(dd, "%u unexpected interrupts, "
				      "disabling interrupts completely\n",
				      *unexpectp);
			ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask,
					 0ULL);
		}
	} else if (*unexpectp > 1)
		ipath_dbg("Interrupt when not ready, should not happen, "
			  "ignoring\n");
}

static noinline void ipath_bad_regread(struct ipath_devdata *dd)
{
	static int allbits;

	

	ipath_dev_err(dd,
		      "Read of interrupt status failed (all bits set)\n");
	if (allbits++) {
		
		ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask, 0ULL);
		if (allbits == 2) {
			ipath_dev_err(dd, "Still bad interrupt status, "
				      "unregistering interrupt\n");
			dd->ipath_f_free_irq(dd);
		} else if (allbits > 2) {
			if ((allbits % 10000) == 0)
				printk(".");
		} else
			ipath_dev_err(dd, "Disabling interrupts, "
				      "multiple errors\n");
	}
}

static void handle_layer_pioavail(struct ipath_devdata *dd)
{
	unsigned long flags;
	int ret;

	ret = ipath_ib_piobufavail(dd->verbs_dev);
	if (ret > 0)
		goto set;

	return;
set:
	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	dd->ipath_sendctrl |= INFINIPATH_S_PIOINTBUFAVAIL;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			 dd->ipath_sendctrl);
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
}

static void handle_urcv(struct ipath_devdata *dd, u64 istat)
{
	u64 portr;
	int i;
	int rcvdint = 0;

	rmb();
	portr = ((istat >> dd->ipath_i_rcvavail_shift) &
		 dd->ipath_i_rcvavail_mask) |
		((istat >> dd->ipath_i_rcvurg_shift) &
		 dd->ipath_i_rcvurg_mask);
	for (i = 1; i < dd->ipath_cfgports; i++) {
		struct ipath_portdata *pd = dd->ipath_pd[i];

		if (portr & (1 << i) && pd && pd->port_cnt) {
			if (test_and_clear_bit(IPATH_PORT_WAITING_RCV,
					       &pd->port_flag)) {
				clear_bit(i + dd->ipath_r_intravail_shift,
					  &dd->ipath_rcvctrl);
				wake_up_interruptible(&pd->port_wait);
				rcvdint = 1;
			} else if (test_and_clear_bit(IPATH_PORT_WAITING_URG,
						      &pd->port_flag)) {
				pd->port_urgent++;
				wake_up_interruptible(&pd->port_wait);
			}
		}
	}
	if (rcvdint) {
		ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvctrl,
				 dd->ipath_rcvctrl);
	}
}

irqreturn_t ipath_intr(int irq, void *data)
{
	struct ipath_devdata *dd = data;
	u64 istat, chk0rcv = 0;
	ipath_err_t estat = 0;
	irqreturn_t ret;
	static unsigned unexpected = 0;
	u64 kportrbits;

	ipath_stats.sps_ints++;

	if (dd->ipath_int_counter != (u32) -1)
		dd->ipath_int_counter++;

	if (!(dd->ipath_flags & IPATH_PRESENT)) {
		return IRQ_HANDLED;
	}


	if (!(dd->ipath_flags & IPATH_INITTED)) {
		ipath_bad_intr(dd, &unexpected);
		ret = IRQ_NONE;
		goto bail;
	}

	istat = ipath_read_ireg(dd, dd->ipath_kregs->kr_intstatus);

	if (unlikely(!istat)) {
		ipath_stats.sps_nullintr++;
		ret = IRQ_NONE; 
		goto bail;
	}
	if (unlikely(istat == -1)) {
		ipath_bad_regread(dd);
		
		ret = IRQ_NONE;
		goto bail;
	}

	if (unexpected)
		unexpected = 0;

	if (unlikely(istat & ~dd->ipath_i_bitsextant))
		ipath_dev_err(dd,
			      "interrupt with unknown interrupts %Lx set\n",
			      (unsigned long long)
			      istat & ~dd->ipath_i_bitsextant);
	else if (istat & ~INFINIPATH_I_ERROR) 
		ipath_cdbg(VERBOSE, "intr stat=0x%Lx\n",
			(unsigned long long) istat);

	if (istat & INFINIPATH_I_ERROR) {
		ipath_stats.sps_errints++;
		estat = ipath_read_kreg64(dd,
					  dd->ipath_kregs->kr_errorstatus);
		if (!estat)
			dev_info(&dd->pcidev->dev, "error interrupt (%Lx), "
				 "but no error bits set!\n",
				 (unsigned long long) istat);
		else if (estat == -1LL)
			ipath_dev_err(dd, "Read of error status failed "
				      "(all bits set); ignoring\n");
		else
			chk0rcv |= handle_errors(dd, estat);
	}

	if (istat & INFINIPATH_I_GPIO) {
		u32 gpiostatus;
		u32 to_clear = 0;

		gpiostatus = ipath_read_kreg32(
			dd, dd->ipath_kregs->kr_gpio_status);
		
		if ((gpiostatus & IPATH_GPIO_ERRINTR_MASK) &&
		    (dd->ipath_flags & IPATH_GPIO_ERRINTRS)) {
			
			to_clear |= (gpiostatus & IPATH_GPIO_ERRINTR_MASK);

			if (gpiostatus & (1 << IPATH_GPIO_RXUVL_BIT)) {
				ipath_dbg("FlowCtl on UnsupVL\n");
				dd->ipath_rxfc_unsupvl_errs++;
			}
			if (gpiostatus & (1 << IPATH_GPIO_OVRUN_BIT)) {
				ipath_dbg("Overrun Threshold exceeded\n");
				dd->ipath_overrun_thresh_errs++;
			}
			if (gpiostatus & (1 << IPATH_GPIO_LLI_BIT)) {
				ipath_dbg("Local Link Integrity error\n");
				dd->ipath_lli_errs++;
			}
			gpiostatus &= ~IPATH_GPIO_ERRINTR_MASK;
		}
		
		if ((gpiostatus & (1 << IPATH_GPIO_PORT0_BIT)) &&
		    (dd->ipath_flags & IPATH_GPIO_INTR)) {
			to_clear |= (1 << IPATH_GPIO_PORT0_BIT);
			gpiostatus &= ~(1 << IPATH_GPIO_PORT0_BIT);
			chk0rcv = 1;
		}
		if (gpiostatus) {
			const u32 mask = (u32) dd->ipath_gpio_mask;

			if (mask & gpiostatus) {
				ipath_dbg("Unexpected GPIO IRQ bits %x\n",
				  gpiostatus & mask);
				to_clear |= (gpiostatus & mask);
				dd->ipath_gpio_mask &= ~(gpiostatus & mask);
				ipath_write_kreg(dd,
					dd->ipath_kregs->kr_gpio_mask,
					dd->ipath_gpio_mask);
			}
		}
		if (to_clear) {
			ipath_write_kreg(dd, dd->ipath_kregs->kr_gpio_clear,
					(u64) to_clear);
		}
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear, istat);

	kportrbits = (1ULL << dd->ipath_i_rcvavail_shift) |
		(1ULL << dd->ipath_i_rcvurg_shift);
	if (chk0rcv || (istat & kportrbits)) {
		istat &= ~kportrbits;
		ipath_kreceive(dd->ipath_pd[0]);
	}

	if (istat & ((dd->ipath_i_rcvavail_mask << dd->ipath_i_rcvavail_shift) |
		     (dd->ipath_i_rcvurg_mask << dd->ipath_i_rcvurg_shift)))
		handle_urcv(dd, istat);

	if (istat & (INFINIPATH_I_SDMAINT | INFINIPATH_I_SDMADISABLED))
		handle_sdma_intr(dd, istat);

	if (istat & INFINIPATH_I_SPIOBUFAVAIL) {
		unsigned long flags;

		spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
		dd->ipath_sendctrl &= ~INFINIPATH_S_PIOINTBUFAVAIL;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
				 dd->ipath_sendctrl);
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
		spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);

		
		handle_layer_pioavail(dd);
	}

	ret = IRQ_HANDLED;

bail:
	return ret;
}
