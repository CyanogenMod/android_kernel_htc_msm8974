/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010 QLogic Corporation.
 * All rights reserved.
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
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/module.h>

#include "qib.h"
#include "qib_common.h"

#define QIB_MIN_USER_CTXT_BUFCNT 7

#define QLOGIC_IB_R_SOFTWARE_MASK 0xFF
#define QLOGIC_IB_R_SOFTWARE_SHIFT 24
#define QLOGIC_IB_R_EMULATOR_MASK (1ULL<<62)

ushort qib_cfgctxts;
module_param_named(cfgctxts, qib_cfgctxts, ushort, S_IRUGO);
MODULE_PARM_DESC(cfgctxts, "Set max number of contexts to use");

ushort qib_mini_init;
module_param_named(mini_init, qib_mini_init, ushort, S_IRUGO);
MODULE_PARM_DESC(mini_init, "If set, do minimal diag init");

unsigned qib_n_krcv_queues;
module_param_named(krcvqs, qib_n_krcv_queues, uint, S_IRUGO);
MODULE_PARM_DESC(krcvqs, "number of kernel receive queues per IB port");

unsigned qib_wc_pat = 1; 
module_param_named(wc_pat, qib_wc_pat, uint, S_IRUGO);
MODULE_PARM_DESC(wc_pat, "enable write-combining via PAT mechanism");

struct workqueue_struct *qib_cq_wq;

static void verify_interrupt(unsigned long);

static struct idr qib_unit_table;
u32 qib_cpulist_count;
unsigned long *qib_cpulist;

void qib_set_ctxtcnt(struct qib_devdata *dd)
{
	if (!qib_cfgctxts) {
		dd->cfgctxts = dd->first_user_ctxt + num_online_cpus();
		if (dd->cfgctxts > dd->ctxtcnt)
			dd->cfgctxts = dd->ctxtcnt;
	} else if (qib_cfgctxts < dd->num_pports)
		dd->cfgctxts = dd->ctxtcnt;
	else if (qib_cfgctxts <= dd->ctxtcnt)
		dd->cfgctxts = qib_cfgctxts;
	else
		dd->cfgctxts = dd->ctxtcnt;
}

int qib_create_ctxts(struct qib_devdata *dd)
{
	unsigned i;
	int ret;

	dd->rcd = kzalloc(sizeof(*dd->rcd) * dd->ctxtcnt, GFP_KERNEL);
	if (!dd->rcd) {
		qib_dev_err(dd, "Unable to allocate ctxtdata array, "
			    "failing\n");
		ret = -ENOMEM;
		goto done;
	}

	
	for (i = 0; i < dd->first_user_ctxt; ++i) {
		struct qib_pportdata *ppd;
		struct qib_ctxtdata *rcd;

		if (dd->skip_kctxt_mask & (1 << i))
			continue;

		ppd = dd->pport + (i % dd->num_pports);
		rcd = qib_create_ctxtdata(ppd, i);
		if (!rcd) {
			qib_dev_err(dd, "Unable to allocate ctxtdata"
				    " for Kernel ctxt, failing\n");
			ret = -ENOMEM;
			goto done;
		}
		rcd->pkeys[0] = QIB_DEFAULT_P_KEY;
		rcd->seq_cnt = 1;
	}
	ret = 0;
done:
	return ret;
}

struct qib_ctxtdata *qib_create_ctxtdata(struct qib_pportdata *ppd, u32 ctxt)
{
	struct qib_devdata *dd = ppd->dd;
	struct qib_ctxtdata *rcd;

	rcd = kzalloc(sizeof(*rcd), GFP_KERNEL);
	if (rcd) {
		INIT_LIST_HEAD(&rcd->qp_wait_list);
		rcd->ppd = ppd;
		rcd->dd = dd;
		rcd->cnt = 1;
		rcd->ctxt = ctxt;
		dd->rcd[ctxt] = rcd;

		dd->f_init_ctxt(rcd);

		rcd->rcvegrbuf_size = 0x8000;
		rcd->rcvegrbufs_perchunk =
			rcd->rcvegrbuf_size / dd->rcvegrbufsize;
		rcd->rcvegrbuf_chunks = (rcd->rcvegrcnt +
			rcd->rcvegrbufs_perchunk - 1) /
			rcd->rcvegrbufs_perchunk;
		BUG_ON(!is_power_of_2(rcd->rcvegrbufs_perchunk));
		rcd->rcvegrbufs_perchunk_shift =
			ilog2(rcd->rcvegrbufs_perchunk);
	}
	return rcd;
}

void qib_init_pportdata(struct qib_pportdata *ppd, struct qib_devdata *dd,
			u8 hw_pidx, u8 port)
{
	ppd->dd = dd;
	ppd->hw_pidx = hw_pidx;
	ppd->port = port; 

	spin_lock_init(&ppd->sdma_lock);
	spin_lock_init(&ppd->lflags_lock);
	init_waitqueue_head(&ppd->state_wait);

	init_timer(&ppd->symerr_clear_timer);
	ppd->symerr_clear_timer.function = qib_clear_symerror_on_linkup;
	ppd->symerr_clear_timer.data = (unsigned long)ppd;
}

static int init_pioavailregs(struct qib_devdata *dd)
{
	int ret, pidx;
	u64 *status_page;

	dd->pioavailregs_dma = dma_alloc_coherent(
		&dd->pcidev->dev, PAGE_SIZE, &dd->pioavailregs_phys,
		GFP_KERNEL);
	if (!dd->pioavailregs_dma) {
		qib_dev_err(dd, "failed to allocate PIOavail reg area "
			    "in memory\n");
		ret = -ENOMEM;
		goto done;
	}

	status_page = (u64 *)
		((char *) dd->pioavailregs_dma +
		 ((2 * L1_CACHE_BYTES +
		   dd->pioavregs * sizeof(u64)) & ~L1_CACHE_BYTES));
	
	dd->devstatusp = status_page;
	*status_page++ = 0;
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		dd->pport[pidx].statusp = status_page;
		*status_page++ = 0;
	}

	dd->freezemsg = (char *) status_page;
	*dd->freezemsg = 0;
	
	ret = (char *) status_page - (char *) dd->pioavailregs_dma;
	dd->freezelen = PAGE_SIZE - ret;

	ret = 0;

done:
	return ret;
}

static void init_shadow_tids(struct qib_devdata *dd)
{
	struct page **pages;
	dma_addr_t *addrs;

	pages = vzalloc(dd->cfgctxts * dd->rcvtidcnt * sizeof(struct page *));
	if (!pages) {
		qib_dev_err(dd, "failed to allocate shadow page * "
			    "array, no expected sends!\n");
		goto bail;
	}

	addrs = vzalloc(dd->cfgctxts * dd->rcvtidcnt * sizeof(dma_addr_t));
	if (!addrs) {
		qib_dev_err(dd, "failed to allocate shadow dma handle "
			    "array, no expected sends!\n");
		goto bail_free;
	}

	dd->pageshadow = pages;
	dd->physshadow = addrs;
	return;

bail_free:
	vfree(pages);
bail:
	dd->pageshadow = NULL;
}

static int loadtime_init(struct qib_devdata *dd)
{
	int ret = 0;

	if (((dd->revision >> QLOGIC_IB_R_SOFTWARE_SHIFT) &
	     QLOGIC_IB_R_SOFTWARE_MASK) != QIB_CHIP_SWVERSION) {
		qib_dev_err(dd, "Driver only handles version %d, "
			    "chip swversion is %d (%llx), failng\n",
			    QIB_CHIP_SWVERSION,
			    (int)(dd->revision >>
				QLOGIC_IB_R_SOFTWARE_SHIFT) &
			    QLOGIC_IB_R_SOFTWARE_MASK,
			    (unsigned long long) dd->revision);
		ret = -ENOSYS;
		goto done;
	}

	if (dd->revision & QLOGIC_IB_R_EMULATOR_MASK)
		qib_devinfo(dd->pcidev, "%s", dd->boardversion);

	spin_lock_init(&dd->pioavail_lock);
	spin_lock_init(&dd->sendctrl_lock);
	spin_lock_init(&dd->uctxt_lock);
	spin_lock_init(&dd->qib_diag_trans_lock);
	spin_lock_init(&dd->eep_st_lock);
	mutex_init(&dd->eep_lock);

	if (qib_mini_init)
		goto done;

	ret = init_pioavailregs(dd);
	init_shadow_tids(dd);

	qib_get_eeprom_info(dd);

	
	init_timer(&dd->intrchk_timer);
	dd->intrchk_timer.function = verify_interrupt;
	dd->intrchk_timer.data = (unsigned long) dd;

done:
	return ret;
}

static int init_after_reset(struct qib_devdata *dd)
{
	int i;

	for (i = 0; i < dd->num_pports; ++i) {
		dd->f_rcvctrl(dd->pport + i, QIB_RCVCTRL_CTXT_DIS |
				  QIB_RCVCTRL_INTRAVAIL_DIS |
				  QIB_RCVCTRL_TAILUPD_DIS, -1);
		
		dd->f_sendctrl(dd->pport + i, QIB_SENDCTRL_SEND_DIS |
			QIB_SENDCTRL_AVAIL_DIS);
	}

	return 0;
}

static void enable_chip(struct qib_devdata *dd)
{
	u64 rcvmask;
	int i;

	for (i = 0; i < dd->num_pports; ++i)
		dd->f_sendctrl(dd->pport + i, QIB_SENDCTRL_SEND_ENB |
			QIB_SENDCTRL_AVAIL_ENB);
	rcvmask = QIB_RCVCTRL_CTXT_ENB | QIB_RCVCTRL_INTRAVAIL_ENB;
	rcvmask |= (dd->flags & QIB_NODMA_RTAIL) ?
		  QIB_RCVCTRL_TAILUPD_DIS : QIB_RCVCTRL_TAILUPD_ENB;
	for (i = 0; dd->rcd && i < dd->first_user_ctxt; ++i) {
		struct qib_ctxtdata *rcd = dd->rcd[i];

		if (rcd)
			dd->f_rcvctrl(rcd->ppd, rcvmask, i);
	}
	dd->freectxts = dd->cfgctxts - dd->first_user_ctxt;
}

static void verify_interrupt(unsigned long opaque)
{
	struct qib_devdata *dd = (struct qib_devdata *) opaque;

	if (!dd)
		return; 

	if (dd->int_counter == 0) {
		if (!dd->f_intr_fallback(dd))
			dev_err(&dd->pcidev->dev, "No interrupts detected, "
				"not usable.\n");
		else 
			mod_timer(&dd->intrchk_timer, jiffies + HZ/2);
	}
}

static void init_piobuf_state(struct qib_devdata *dd)
{
	int i, pidx;
	u32 uctxts;

	dd->f_sendctrl(dd->pport, QIB_SENDCTRL_DISARM_ALL);
	for (pidx = 0; pidx < dd->num_pports; ++pidx)
		dd->f_sendctrl(dd->pport + pidx, QIB_SENDCTRL_FLUSH);

	uctxts = dd->cfgctxts - dd->first_user_ctxt;
	dd->ctxts_extrabuf = dd->pbufsctxt ?
		dd->lastctxt_piobuf - (dd->pbufsctxt * uctxts) : 0;

	for (i = 0; i < dd->pioavregs; i++) {
		__le64 tmp;

		tmp = dd->pioavailregs_dma[i];
		dd->pioavailshadow[i] = le64_to_cpu(tmp);
	}
	while (i < ARRAY_SIZE(dd->pioavailshadow))
		dd->pioavailshadow[i++] = 0; 

	
	qib_chg_pioavailkernel(dd, 0, dd->piobcnt2k + dd->piobcnt4k,
			       TXCHK_CHG_TYPE_KERN, NULL);
	dd->f_initvl15_bufs(dd);
}

int qib_init(struct qib_devdata *dd, int reinit)
{
	int ret = 0, pidx, lastfail = 0;
	u32 portok = 0;
	unsigned i;
	struct qib_ctxtdata *rcd;
	struct qib_pportdata *ppd;
	unsigned long flags;

	
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~(QIBL_LINKACTIVE | QIBL_LINKARMED |
				 QIBL_LINKDOWN | QIBL_LINKINIT |
				 QIBL_LINKV);
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	}

	if (reinit)
		ret = init_after_reset(dd);
	else
		ret = loadtime_init(dd);
	if (ret)
		goto done;

	
	if (qib_mini_init)
		return 0;

	ret = dd->f_late_initreg(dd);
	if (ret)
		goto done;

	
	for (i = 0; dd->rcd && i < dd->first_user_ctxt; ++i) {
		rcd = dd->rcd[i];
		if (!rcd)
			continue;

		lastfail = qib_create_rcvhdrq(dd, rcd);
		if (!lastfail)
			lastfail = qib_setup_eagerbufs(rcd);
		if (lastfail) {
			qib_dev_err(dd, "failed to allocate kernel ctxt's "
				    "rcvhdrq and/or egr bufs\n");
			continue;
		}
	}

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		int mtu;
		if (lastfail)
			ret = lastfail;
		ppd = dd->pport + pidx;
		mtu = ib_mtu_enum_to_int(qib_ibmtu);
		if (mtu == -1) {
			mtu = QIB_DEFAULT_MTU;
			qib_ibmtu = 0; 
		}
		
		ppd->init_ibmaxlen = min(mtu > 2048 ?
					 dd->piosize4k : dd->piosize2k,
					 dd->rcvegrbufsize +
					 (dd->rcvhdrentsize << 2));
		ppd->ibmaxlen = ppd->init_ibmaxlen;
		qib_set_mtu(ppd, mtu);

		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags |= QIBL_IB_LINK_DISABLED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);

		lastfail = dd->f_bringup_serdes(ppd);
		if (lastfail) {
			qib_devinfo(dd->pcidev,
				 "Failed to bringup IB port %u\n", ppd->port);
			lastfail = -ENETDOWN;
			continue;
		}

		portok++;
	}

	if (!portok) {
		
		if (!ret && lastfail)
			ret = lastfail;
		else if (!ret)
			ret = -ENETDOWN;
		
	}

	enable_chip(dd);

	init_piobuf_state(dd);

done:
	if (!ret) {
		
		for (pidx = 0; pidx < dd->num_pports; ++pidx) {
			ppd = dd->pport + pidx;
			*ppd->statusp |= QIB_STATUS_CHIP_PRESENT |
				QIB_STATUS_INITTED;
			if (!ppd->link_speed_enabled)
				continue;
			if (dd->flags & QIB_HAS_SEND_DMA)
				ret = qib_setup_sdma(ppd);
			init_timer(&ppd->hol_timer);
			ppd->hol_timer.function = qib_hol_event;
			ppd->hol_timer.data = (unsigned long)ppd;
			ppd->hol_state = QIB_HOL_UP;
		}

		
		dd->f_set_intr_state(dd, 1);

		mod_timer(&dd->intrchk_timer, jiffies + HZ/2);
		
		mod_timer(&dd->stats_timer, jiffies + HZ * ACTIVITY_TIMER);
	}

	
	return ret;
}


int __attribute__((weak)) qib_enable_wc(struct qib_devdata *dd)
{
	return -EOPNOTSUPP;
}

void __attribute__((weak)) qib_disable_wc(struct qib_devdata *dd)
{
}

static inline struct qib_devdata *__qib_lookup(int unit)
{
	return idr_find(&qib_unit_table, unit);
}

struct qib_devdata *qib_lookup(int unit)
{
	struct qib_devdata *dd;
	unsigned long flags;

	spin_lock_irqsave(&qib_devs_lock, flags);
	dd = __qib_lookup(unit);
	spin_unlock_irqrestore(&qib_devs_lock, flags);

	return dd;
}

static void qib_stop_timers(struct qib_devdata *dd)
{
	struct qib_pportdata *ppd;
	int pidx;

	if (dd->stats_timer.data) {
		del_timer_sync(&dd->stats_timer);
		dd->stats_timer.data = 0;
	}
	if (dd->intrchk_timer.data) {
		del_timer_sync(&dd->intrchk_timer);
		dd->intrchk_timer.data = 0;
	}
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		if (ppd->hol_timer.data)
			del_timer_sync(&ppd->hol_timer);
		if (ppd->led_override_timer.data) {
			del_timer_sync(&ppd->led_override_timer);
			atomic_set(&ppd->led_override_timer_active, 0);
		}
		if (ppd->symerr_clear_timer.data)
			del_timer_sync(&ppd->symerr_clear_timer);
	}
}

static void qib_shutdown_device(struct qib_devdata *dd)
{
	struct qib_pportdata *ppd;
	unsigned pidx;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;

		spin_lock_irq(&ppd->lflags_lock);
		ppd->lflags &= ~(QIBL_LINKDOWN | QIBL_LINKINIT |
				 QIBL_LINKARMED | QIBL_LINKACTIVE |
				 QIBL_LINKV);
		spin_unlock_irq(&ppd->lflags_lock);
		*ppd->statusp &= ~(QIB_STATUS_IB_CONF | QIB_STATUS_IB_READY);
	}
	dd->flags &= ~QIB_INITTED;

	
	dd->f_set_intr_state(dd, 0);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		dd->f_rcvctrl(ppd, QIB_RCVCTRL_TAILUPD_DIS |
				   QIB_RCVCTRL_CTXT_DIS |
				   QIB_RCVCTRL_INTRAVAIL_DIS |
				   QIB_RCVCTRL_PKEY_ENB, -1);
		dd->f_sendctrl(ppd, QIB_SENDCTRL_CLEAR);
	}

	udelay(20);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;
		dd->f_setextled(ppd, 0); 

		if (dd->flags & QIB_HAS_SEND_DMA)
			qib_teardown_sdma(ppd);

		dd->f_sendctrl(ppd, QIB_SENDCTRL_AVAIL_DIS |
				    QIB_SENDCTRL_SEND_DIS);
		dd->f_quiet_serdes(ppd);
	}

	qib_update_eeprom_log(dd);
}

void qib_free_ctxtdata(struct qib_devdata *dd, struct qib_ctxtdata *rcd)
{
	if (!rcd)
		return;

	if (rcd->rcvhdrq) {
		dma_free_coherent(&dd->pcidev->dev, rcd->rcvhdrq_size,
				  rcd->rcvhdrq, rcd->rcvhdrq_phys);
		rcd->rcvhdrq = NULL;
		if (rcd->rcvhdrtail_kvaddr) {
			dma_free_coherent(&dd->pcidev->dev, PAGE_SIZE,
					  rcd->rcvhdrtail_kvaddr,
					  rcd->rcvhdrqtailaddr_phys);
			rcd->rcvhdrtail_kvaddr = NULL;
		}
	}
	if (rcd->rcvegrbuf) {
		unsigned e;

		for (e = 0; e < rcd->rcvegrbuf_chunks; e++) {
			void *base = rcd->rcvegrbuf[e];
			size_t size = rcd->rcvegrbuf_size;

			dma_free_coherent(&dd->pcidev->dev, size,
					  base, rcd->rcvegrbuf_phys[e]);
		}
		kfree(rcd->rcvegrbuf);
		rcd->rcvegrbuf = NULL;
		kfree(rcd->rcvegrbuf_phys);
		rcd->rcvegrbuf_phys = NULL;
		rcd->rcvegrbuf_chunks = 0;
	}

	kfree(rcd->tid_pg_list);
	vfree(rcd->user_event_mask);
	vfree(rcd->subctxt_uregbase);
	vfree(rcd->subctxt_rcvegrbuf);
	vfree(rcd->subctxt_rcvhdr_base);
	kfree(rcd);
}

static void qib_verify_pioperf(struct qib_devdata *dd)
{
	u32 pbnum, cnt, lcnt;
	u32 __iomem *piobuf;
	u32 *addr;
	u64 msecs, emsecs;

	piobuf = dd->f_getsendbuf(dd->pport, 0ULL, &pbnum);
	if (!piobuf) {
		qib_devinfo(dd->pcidev,
			 "No PIObufs for checking perf, skipping\n");
		return;
	}

	cnt = 1024;

	addr = vmalloc(cnt);
	if (!addr) {
		qib_devinfo(dd->pcidev,
			 "Couldn't get memory for checking PIO perf,"
			 " skipping\n");
		goto done;
	}

	preempt_disable();  
	msecs = 1 + jiffies_to_msecs(jiffies);
	for (lcnt = 0; lcnt < 10000U; lcnt++) {
		
		if (jiffies_to_msecs(jiffies) >= msecs)
			break;
		udelay(1);
	}

	dd->f_set_armlaunch(dd, 0);

	writeq(0, piobuf);
	qib_flush_wc();

	msecs = jiffies_to_msecs(jiffies);
	for (emsecs = lcnt = 0; emsecs <= 5UL; lcnt++) {
		qib_pio_copy(piobuf + 64, addr, cnt >> 2);
		emsecs = jiffies_to_msecs(jiffies) - msecs;
	}

	
	if (lcnt < (emsecs * 1024U))
		qib_dev_err(dd,
			    "Performance problem: bandwidth to PIO buffers is "
			    "only %u MiB/sec\n",
			    lcnt / (u32) emsecs);

	preempt_enable();

	vfree(addr);

done:
	
	dd->f_sendctrl(dd->pport, QIB_SENDCTRL_DISARM_BUF(pbnum));
	qib_sendbuf_done(dd, pbnum);
	dd->f_set_armlaunch(dd, 1);
}


void qib_free_devdata(struct qib_devdata *dd)
{
	unsigned long flags;

	spin_lock_irqsave(&qib_devs_lock, flags);
	idr_remove(&qib_unit_table, dd->unit);
	list_del(&dd->list);
	spin_unlock_irqrestore(&qib_devs_lock, flags);

	ib_dealloc_device(&dd->verbs_dev.ibdev);
}

struct qib_devdata *qib_alloc_devdata(struct pci_dev *pdev, size_t extra)
{
	unsigned long flags;
	struct qib_devdata *dd;
	int ret;

	if (!idr_pre_get(&qib_unit_table, GFP_KERNEL)) {
		dd = ERR_PTR(-ENOMEM);
		goto bail;
	}

	dd = (struct qib_devdata *) ib_alloc_device(sizeof(*dd) + extra);
	if (!dd) {
		dd = ERR_PTR(-ENOMEM);
		goto bail;
	}

	spin_lock_irqsave(&qib_devs_lock, flags);
	ret = idr_get_new(&qib_unit_table, dd, &dd->unit);
	if (ret >= 0)
		list_add(&dd->list, &qib_dev_list);
	spin_unlock_irqrestore(&qib_devs_lock, flags);

	if (ret < 0) {
		qib_early_err(&pdev->dev,
			      "Could not allocate unit ID: error %d\n", -ret);
		ib_dealloc_device(&dd->verbs_dev.ibdev);
		dd = ERR_PTR(ret);
		goto bail;
	}

	if (!qib_cpulist_count) {
		u32 count = num_online_cpus();
		qib_cpulist = kzalloc(BITS_TO_LONGS(count) *
				      sizeof(long), GFP_KERNEL);
		if (qib_cpulist)
			qib_cpulist_count = count;
		else
			qib_early_err(&pdev->dev, "Could not alloc cpulist "
				      "info, cpu affinity might be wrong\n");
	}

bail:
	return dd;
}

void qib_disable_after_error(struct qib_devdata *dd)
{
	if (dd->flags & QIB_INITTED) {
		u32 pidx;

		dd->flags &= ~QIB_INITTED;
		if (dd->pport)
			for (pidx = 0; pidx < dd->num_pports; ++pidx) {
				struct qib_pportdata *ppd;

				ppd = dd->pport + pidx;
				if (dd->flags & QIB_PRESENT) {
					qib_set_linkstate(ppd,
						QIB_IB_LINKDOWN_DISABLE);
					dd->f_setextled(ppd, 0);
				}
				*ppd->statusp &= ~QIB_STATUS_IB_READY;
			}
	}

	if (dd->devstatusp)
		*dd->devstatusp |= QIB_STATUS_HWERROR;
}

static void __devexit qib_remove_one(struct pci_dev *);
static int __devinit qib_init_one(struct pci_dev *,
				  const struct pci_device_id *);

#define DRIVER_LOAD_MSG "QLogic " QIB_DRV_NAME " loaded: "
#define PFX QIB_DRV_NAME ": "

static DEFINE_PCI_DEVICE_TABLE(qib_pci_tbl) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_PATHSCALE, PCI_DEVICE_ID_QLOGIC_IB_6120) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_IB_7220) },
	{ PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_IB_7322) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, qib_pci_tbl);

struct pci_driver qib_driver = {
	.name = QIB_DRV_NAME,
	.probe = qib_init_one,
	.remove = __devexit_p(qib_remove_one),
	.id_table = qib_pci_tbl,
	.err_handler = &qib_pci_err_handler,
};

static int __init qlogic_ib_init(void)
{
	int ret;

	ret = qib_dev_init();
	if (ret)
		goto bail;

	qib_cq_wq = create_singlethread_workqueue("qib_cq");
	if (!qib_cq_wq) {
		ret = -ENOMEM;
		goto bail_dev;
	}

	idr_init(&qib_unit_table);
	if (!idr_pre_get(&qib_unit_table, GFP_KERNEL)) {
		printk(KERN_ERR QIB_DRV_NAME ": idr_pre_get() failed\n");
		ret = -ENOMEM;
		goto bail_cq_wq;
	}

	ret = pci_register_driver(&qib_driver);
	if (ret < 0) {
		printk(KERN_ERR QIB_DRV_NAME
		       ": Unable to register driver: error %d\n", -ret);
		goto bail_unit;
	}

	
	if (qib_init_qibfs())
		printk(KERN_ERR QIB_DRV_NAME ": Unable to register ipathfs\n");
	goto bail; 

bail_unit:
	idr_destroy(&qib_unit_table);
bail_cq_wq:
	destroy_workqueue(qib_cq_wq);
bail_dev:
	qib_dev_cleanup();
bail:
	return ret;
}

module_init(qlogic_ib_init);

static void __exit qlogic_ib_cleanup(void)
{
	int ret;

	ret = qib_exit_qibfs();
	if (ret)
		printk(KERN_ERR QIB_DRV_NAME ": "
			"Unable to cleanup counter filesystem: "
			"error %d\n", -ret);

	pci_unregister_driver(&qib_driver);

	destroy_workqueue(qib_cq_wq);

	qib_cpulist_count = 0;
	kfree(qib_cpulist);

	idr_destroy(&qib_unit_table);
	qib_dev_cleanup();
}

module_exit(qlogic_ib_cleanup);

static void cleanup_device_data(struct qib_devdata *dd)
{
	int ctxt;
	int pidx;
	struct qib_ctxtdata **tmp;
	unsigned long flags;

	
	for (pidx = 0; pidx < dd->num_pports; ++pidx)
		if (dd->pport[pidx].statusp)
			*dd->pport[pidx].statusp &= ~QIB_STATUS_CHIP_PRESENT;

	if (!qib_wc_pat)
		qib_disable_wc(dd);

	if (dd->pioavailregs_dma) {
		dma_free_coherent(&dd->pcidev->dev, PAGE_SIZE,
				  (void *) dd->pioavailregs_dma,
				  dd->pioavailregs_phys);
		dd->pioavailregs_dma = NULL;
	}

	if (dd->pageshadow) {
		struct page **tmpp = dd->pageshadow;
		dma_addr_t *tmpd = dd->physshadow;
		int i, cnt = 0;

		for (ctxt = 0; ctxt < dd->cfgctxts; ctxt++) {
			int ctxt_tidbase = ctxt * dd->rcvtidcnt;
			int maxtid = ctxt_tidbase + dd->rcvtidcnt;

			for (i = ctxt_tidbase; i < maxtid; i++) {
				if (!tmpp[i])
					continue;
				pci_unmap_page(dd->pcidev, tmpd[i],
					       PAGE_SIZE, PCI_DMA_FROMDEVICE);
				qib_release_user_pages(&tmpp[i], 1);
				tmpp[i] = NULL;
				cnt++;
			}
		}

		tmpp = dd->pageshadow;
		dd->pageshadow = NULL;
		vfree(tmpp);
	}

	spin_lock_irqsave(&dd->uctxt_lock, flags);
	tmp = dd->rcd;
	dd->rcd = NULL;
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);
	for (ctxt = 0; tmp && ctxt < dd->ctxtcnt; ctxt++) {
		struct qib_ctxtdata *rcd = tmp[ctxt];

		tmp[ctxt] = NULL; 
		qib_free_ctxtdata(dd, rcd);
	}
	kfree(tmp);
	kfree(dd->boardname);
}

static void qib_postinit_cleanup(struct qib_devdata *dd)
{
	if (dd->f_cleanup)
		dd->f_cleanup(dd);

	qib_pcie_ddcleanup(dd);

	cleanup_device_data(dd);

	qib_free_devdata(dd);
}

static int __devinit qib_init_one(struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	int ret, j, pidx, initfail;
	struct qib_devdata *dd = NULL;

	ret = qib_pcie_init(pdev, ent);
	if (ret)
		goto bail;

	switch (ent->device) {
	case PCI_DEVICE_ID_QLOGIC_IB_6120:
#ifdef CONFIG_PCI_MSI
		dd = qib_init_iba6120_funcs(pdev, ent);
#else
		qib_early_err(&pdev->dev, "QLogic PCIE device 0x%x cannot "
		      "work if CONFIG_PCI_MSI is not enabled\n",
		      ent->device);
		dd = ERR_PTR(-ENODEV);
#endif
		break;

	case PCI_DEVICE_ID_QLOGIC_IB_7220:
		dd = qib_init_iba7220_funcs(pdev, ent);
		break;

	case PCI_DEVICE_ID_QLOGIC_IB_7322:
		dd = qib_init_iba7322_funcs(pdev, ent);
		break;

	default:
		qib_early_err(&pdev->dev, "Failing on unknown QLogic "
			      "deviceid 0x%x\n", ent->device);
		ret = -ENODEV;
	}

	if (IS_ERR(dd))
		ret = PTR_ERR(dd);
	if (ret)
		goto bail; 

	
	initfail = qib_init(dd, 0);

	ret = qib_register_ib_device(dd);

	if (!qib_mini_init && !initfail && !ret)
		dd->flags |= QIB_INITTED;

	j = qib_device_create(dd);
	if (j)
		qib_dev_err(dd, "Failed to create /dev devices: %d\n", -j);
	j = qibfs_add(dd);
	if (j)
		qib_dev_err(dd, "Failed filesystem setup for counters: %d\n",
			    -j);

	if (qib_mini_init || initfail || ret) {
		qib_stop_timers(dd);
		flush_workqueue(ib_wq);
		for (pidx = 0; pidx < dd->num_pports; ++pidx)
			dd->f_quiet_serdes(dd->pport + pidx);
		if (qib_mini_init)
			goto bail;
		if (!j) {
			(void) qibfs_remove(dd);
			qib_device_remove(dd);
		}
		if (!ret)
			qib_unregister_ib_device(dd);
		qib_postinit_cleanup(dd);
		if (initfail)
			ret = initfail;
		goto bail;
	}

	if (!qib_wc_pat) {
		ret = qib_enable_wc(dd);
		if (ret) {
			qib_dev_err(dd, "Write combining not enabled "
				    "(err %d): performance may be poor\n",
				    -ret);
			ret = 0;
		}
	}

	qib_verify_pioperf(dd);
bail:
	return ret;
}

static void __devexit qib_remove_one(struct pci_dev *pdev)
{
	struct qib_devdata *dd = pci_get_drvdata(pdev);
	int ret;

	
	qib_unregister_ib_device(dd);

	if (!qib_mini_init)
		qib_shutdown_device(dd);

	qib_stop_timers(dd);

	
	flush_workqueue(ib_wq);

	ret = qibfs_remove(dd);
	if (ret)
		qib_dev_err(dd, "Failed counters filesystem cleanup: %d\n",
			    -ret);

	qib_device_remove(dd);

	qib_postinit_cleanup(dd);
}

int qib_create_rcvhdrq(struct qib_devdata *dd, struct qib_ctxtdata *rcd)
{
	unsigned amt;

	if (!rcd->rcvhdrq) {
		dma_addr_t phys_hdrqtail;
		gfp_t gfp_flags;

		amt = ALIGN(dd->rcvhdrcnt * dd->rcvhdrentsize *
			    sizeof(u32), PAGE_SIZE);
		gfp_flags = (rcd->ctxt >= dd->first_user_ctxt) ?
			GFP_USER : GFP_KERNEL;
		rcd->rcvhdrq = dma_alloc_coherent(
			&dd->pcidev->dev, amt, &rcd->rcvhdrq_phys,
			gfp_flags | __GFP_COMP);

		if (!rcd->rcvhdrq) {
			qib_dev_err(dd, "attempt to allocate %d bytes "
				    "for ctxt %u rcvhdrq failed\n",
				    amt, rcd->ctxt);
			goto bail;
		}

		if (rcd->ctxt >= dd->first_user_ctxt) {
			rcd->user_event_mask = vmalloc_user(PAGE_SIZE);
			if (!rcd->user_event_mask)
				goto bail_free_hdrq;
		}

		if (!(dd->flags & QIB_NODMA_RTAIL)) {
			rcd->rcvhdrtail_kvaddr = dma_alloc_coherent(
				&dd->pcidev->dev, PAGE_SIZE, &phys_hdrqtail,
				gfp_flags);
			if (!rcd->rcvhdrtail_kvaddr)
				goto bail_free;
			rcd->rcvhdrqtailaddr_phys = phys_hdrqtail;
		}

		rcd->rcvhdrq_size = amt;
	}

	
	memset(rcd->rcvhdrq, 0, rcd->rcvhdrq_size);
	if (rcd->rcvhdrtail_kvaddr)
		memset(rcd->rcvhdrtail_kvaddr, 0, PAGE_SIZE);
	return 0;

bail_free:
	qib_dev_err(dd, "attempt to allocate 1 page for ctxt %u "
		    "rcvhdrqtailaddr failed\n", rcd->ctxt);
	vfree(rcd->user_event_mask);
	rcd->user_event_mask = NULL;
bail_free_hdrq:
	dma_free_coherent(&dd->pcidev->dev, amt, rcd->rcvhdrq,
			  rcd->rcvhdrq_phys);
	rcd->rcvhdrq = NULL;
bail:
	return -ENOMEM;
}

int qib_setup_eagerbufs(struct qib_ctxtdata *rcd)
{
	struct qib_devdata *dd = rcd->dd;
	unsigned e, egrcnt, egrperchunk, chunk, egrsize, egroff;
	size_t size;
	gfp_t gfp_flags;

	gfp_flags = __GFP_WAIT | __GFP_IO | __GFP_COMP;

	egrcnt = rcd->rcvegrcnt;
	egroff = rcd->rcvegr_tid_base;
	egrsize = dd->rcvegrbufsize;

	chunk = rcd->rcvegrbuf_chunks;
	egrperchunk = rcd->rcvegrbufs_perchunk;
	size = rcd->rcvegrbuf_size;
	if (!rcd->rcvegrbuf) {
		rcd->rcvegrbuf =
			kzalloc(chunk * sizeof(rcd->rcvegrbuf[0]),
				GFP_KERNEL);
		if (!rcd->rcvegrbuf)
			goto bail;
	}
	if (!rcd->rcvegrbuf_phys) {
		rcd->rcvegrbuf_phys =
			kmalloc(chunk * sizeof(rcd->rcvegrbuf_phys[0]),
				GFP_KERNEL);
		if (!rcd->rcvegrbuf_phys)
			goto bail_rcvegrbuf;
	}
	for (e = 0; e < rcd->rcvegrbuf_chunks; e++) {
		if (rcd->rcvegrbuf[e])
			continue;
		rcd->rcvegrbuf[e] =
			dma_alloc_coherent(&dd->pcidev->dev, size,
					   &rcd->rcvegrbuf_phys[e],
					   gfp_flags);
		if (!rcd->rcvegrbuf[e])
			goto bail_rcvegrbuf_phys;
	}

	rcd->rcvegr_phys = rcd->rcvegrbuf_phys[0];

	for (e = chunk = 0; chunk < rcd->rcvegrbuf_chunks; chunk++) {
		dma_addr_t pa = rcd->rcvegrbuf_phys[chunk];
		unsigned i;

		
		memset(rcd->rcvegrbuf[chunk], 0, size);

		for (i = 0; e < egrcnt && i < egrperchunk; e++, i++) {
			dd->f_put_tid(dd, e + egroff +
					  (u64 __iomem *)
					  ((char __iomem *)
					   dd->kregbase +
					   dd->rcvegrbase),
					  RCVHQ_RCV_TYPE_EAGER, pa);
			pa += egrsize;
		}
		cond_resched(); 
	}

	return 0;

bail_rcvegrbuf_phys:
	for (e = 0; e < rcd->rcvegrbuf_chunks && rcd->rcvegrbuf[e]; e++)
		dma_free_coherent(&dd->pcidev->dev, size,
				  rcd->rcvegrbuf[e], rcd->rcvegrbuf_phys[e]);
	kfree(rcd->rcvegrbuf_phys);
	rcd->rcvegrbuf_phys = NULL;
bail_rcvegrbuf:
	kfree(rcd->rcvegrbuf);
	rcd->rcvegrbuf = NULL;
bail:
	return -ENOMEM;
}

int init_chip_wc_pat(struct qib_devdata *dd, u32 vl15buflen)
{
	u64 __iomem *qib_kregbase = NULL;
	void __iomem *qib_piobase = NULL;
	u64 __iomem *qib_userbase = NULL;
	u64 qib_kreglen;
	u64 qib_pio2koffset = dd->piobufbase & 0xffffffff;
	u64 qib_pio4koffset = dd->piobufbase >> 32;
	u64 qib_pio2klen = dd->piobcnt2k * dd->palign;
	u64 qib_pio4klen = dd->piobcnt4k * dd->align4k;
	u64 qib_physaddr = dd->physaddr;
	u64 qib_piolen;
	u64 qib_userlen = 0;

	iounmap(dd->kregbase);
	dd->kregbase = NULL;

	if (dd->piobcnt4k == 0) {
		qib_kreglen = qib_pio2koffset;
		qib_piolen = qib_pio2klen;
	} else if (qib_pio2koffset < qib_pio4koffset) {
		qib_kreglen = qib_pio2koffset;
		qib_piolen = qib_pio4koffset + qib_pio4klen - qib_kreglen;
	} else {
		qib_kreglen = qib_pio4koffset;
		qib_piolen = qib_pio2koffset + qib_pio2klen - qib_kreglen;
	}
	qib_piolen += vl15buflen;
	
	if (dd->uregbase > qib_kreglen)
		qib_userlen = dd->ureg_align * dd->cfgctxts;

	
	qib_kregbase = ioremap_nocache(qib_physaddr, qib_kreglen);
	if (!qib_kregbase)
		goto bail;

	qib_piobase = ioremap_wc(qib_physaddr + qib_kreglen, qib_piolen);
	if (!qib_piobase)
		goto bail_kregbase;

	if (qib_userlen) {
		qib_userbase = ioremap_nocache(qib_physaddr + dd->uregbase,
					       qib_userlen);
		if (!qib_userbase)
			goto bail_piobase;
	}

	dd->kregbase = qib_kregbase;
	dd->kregend = (u64 __iomem *)
		((char __iomem *) qib_kregbase + qib_kreglen);
	dd->piobase = qib_piobase;
	dd->pio2kbase = (void __iomem *)
		(((char __iomem *) dd->piobase) +
		 qib_pio2koffset - qib_kreglen);
	if (dd->piobcnt4k)
		dd->pio4kbase = (void __iomem *)
			(((char __iomem *) dd->piobase) +
			 qib_pio4koffset - qib_kreglen);
	if (qib_userlen)
		
		dd->userbase = qib_userbase;
	return 0;

bail_piobase:
	iounmap(qib_piobase);
bail_kregbase:
	iounmap(qib_kregbase);
bail:
	return -ENOMEM;
}
