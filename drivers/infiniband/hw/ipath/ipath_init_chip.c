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
#include <linux/netdevice.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/vmalloc.h>

#include "ipath_kernel.h"
#include "ipath_common.h"

#define IPATH_MIN_USER_PORT_BUFCNT 7

static ushort ipath_cfgports;

module_param_named(cfgports, ipath_cfgports, ushort, S_IRUGO);
MODULE_PARM_DESC(cfgports, "Set max number of ports to use");

static ushort ipath_kpiobufs;

static int ipath_set_kpiobufs(const char *val, struct kernel_param *kp);

module_param_call(kpiobufs, ipath_set_kpiobufs, param_get_ushort,
		  &ipath_kpiobufs, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(kpiobufs, "Set number of PIO buffers for driver");

static int create_port0_egr(struct ipath_devdata *dd)
{
	unsigned e, egrcnt;
	struct ipath_skbinfo *skbinfo;
	int ret;

	egrcnt = dd->ipath_p0_rcvegrcnt;

	skbinfo = vmalloc(sizeof(*dd->ipath_port0_skbinfo) * egrcnt);
	if (skbinfo == NULL) {
		ipath_dev_err(dd, "allocation error for eager TID "
			      "skb array\n");
		ret = -ENOMEM;
		goto bail;
	}
	for (e = 0; e < egrcnt; e++) {
		skbinfo[e].skb = ipath_alloc_skb(dd, GFP_KERNEL);
		if (!skbinfo[e].skb) {
			ipath_dev_err(dd, "SKB allocation error for "
				      "eager TID %u\n", e);
			while (e != 0)
				dev_kfree_skb(skbinfo[--e].skb);
			vfree(skbinfo);
			ret = -ENOMEM;
			goto bail;
		}
	}
	dd->ipath_port0_skbinfo = skbinfo;

	for (e = 0; e < egrcnt; e++) {
		dd->ipath_port0_skbinfo[e].phys =
		  ipath_map_single(dd->pcidev,
				   dd->ipath_port0_skbinfo[e].skb->data,
				   dd->ipath_ibmaxlen, PCI_DMA_FROMDEVICE);
		dd->ipath_f_put_tid(dd, e + (u64 __iomem *)
				    ((char __iomem *) dd->ipath_kregbase +
				     dd->ipath_rcvegrbase),
				    RCVHQ_RCV_TYPE_EAGER,
				    dd->ipath_port0_skbinfo[e].phys);
	}

	ret = 0;

bail:
	return ret;
}

static int bringup_link(struct ipath_devdata *dd)
{
	u64 val, ibc;
	int ret = 0;

	
	dd->ipath_control &= ~INFINIPATH_C_LINKENABLE;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_control,
			 dd->ipath_control);

	val = (dd->ipath_ibmaxlen >> 2) + 1;
	ibc = val << dd->ibcc_mpl_shift;

	
	ibc |= 0x5ULL << INFINIPATH_IBCC_FLOWCTRLWATERMARK_SHIFT;
	ibc |= 0x3ULL << INFINIPATH_IBCC_FLOWCTRLPERIOD_SHIFT;
	
	ibc |= 0xfULL << INFINIPATH_IBCC_PHYERRTHRESHOLD_SHIFT;
	
	ibc |= 4ULL << INFINIPATH_IBCC_CREDITSCALE_SHIFT;
	
	ibc |= 0xfULL << INFINIPATH_IBCC_OVERRUNTHRESHOLD_SHIFT;
	
	dd->ipath_ibcctrl = ibc;
	ibc |= INFINIPATH_IBCC_LINKINITCMD_DISABLE <<
		INFINIPATH_IBCC_LINKINITCMD_SHIFT;
	dd->ipath_flags |= IPATH_IB_LINK_DISABLED;
	ipath_cdbg(VERBOSE, "Writing 0x%llx to ibcctrl\n",
		   (unsigned long long) ibc);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_ibcctrl, ibc);

	
	val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);

	ret = dd->ipath_f_bringup_serdes(dd);

	if (ret)
		dev_info(&dd->pcidev->dev, "Could not initialize SerDes, "
			 "not usable\n");
	else {
		
		dd->ipath_control |= INFINIPATH_C_LINKENABLE;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_control,
				 dd->ipath_control);
	}

	return ret;
}

static struct ipath_portdata *create_portdata0(struct ipath_devdata *dd)
{
	struct ipath_portdata *pd = NULL;

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (pd) {
		pd->port_dd = dd;
		pd->port_cnt = 1;
		
		pd->port_pkeys[0] = IPATH_DEFAULT_P_KEY;
		pd->port_seq_cnt = 1;
	}
	return pd;
}

static int init_chip_first(struct ipath_devdata *dd)
{
	struct ipath_portdata *pd;
	int ret = 0;
	u64 val;

	spin_lock_init(&dd->ipath_kernel_tid_lock);
	spin_lock_init(&dd->ipath_user_tid_lock);
	spin_lock_init(&dd->ipath_sendctrl_lock);
	spin_lock_init(&dd->ipath_uctxt_lock);
	spin_lock_init(&dd->ipath_sdma_lock);
	spin_lock_init(&dd->ipath_gpio_lock);
	spin_lock_init(&dd->ipath_eep_st_lock);
	spin_lock_init(&dd->ipath_sdepb_lock);
	mutex_init(&dd->ipath_eep_lock);

	dd->ipath_f_config_ports(dd, ipath_cfgports);
	if (!ipath_cfgports)
		dd->ipath_cfgports = dd->ipath_portcnt;
	else if (ipath_cfgports <= dd->ipath_portcnt) {
		dd->ipath_cfgports = ipath_cfgports;
		ipath_dbg("Configured to use %u ports out of %u in chip\n",
			  dd->ipath_cfgports, ipath_read_kreg32(dd,
			  dd->ipath_kregs->kr_portcnt));
	} else {
		dd->ipath_cfgports = dd->ipath_portcnt;
		ipath_dbg("Tried to configured to use %u ports; chip "
			  "only supports %u\n", ipath_cfgports,
			  ipath_read_kreg32(dd,
				  dd->ipath_kregs->kr_portcnt));
	}
	dd->ipath_pd = kzalloc(sizeof(*dd->ipath_pd) * dd->ipath_portcnt,
			       GFP_KERNEL);

	if (!dd->ipath_pd) {
		ipath_dev_err(dd, "Unable to allocate portdata array, "
			      "failing\n");
		ret = -ENOMEM;
		goto done;
	}

	pd = create_portdata0(dd);
	if (!pd) {
		ipath_dev_err(dd, "Unable to allocate portdata for port "
			      "0, failing\n");
		ret = -ENOMEM;
		goto done;
	}
	dd->ipath_pd[0] = pd;

	dd->ipath_rcvtidcnt =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvtidcnt);
	dd->ipath_rcvtidbase =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvtidbase);
	dd->ipath_rcvegrcnt =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvegrcnt);
	dd->ipath_rcvegrbase =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvegrbase);
	dd->ipath_palign =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_pagealign);
	dd->ipath_piobufbase =
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_sendpiobufbase);
	val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_sendpiosize);
	dd->ipath_piosize2k = val & ~0U;
	dd->ipath_piosize4k = val >> 32;
	if (dd->ipath_piosize4k == 0 && ipath_mtu4096)
		ipath_mtu4096 = 0; 
	dd->ipath_ibmtu = ipath_mtu4096 ? 4096 : 2048;
	val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_sendpiobufcnt);
	dd->ipath_piobcnt2k = val & ~0U;
	dd->ipath_piobcnt4k = val >> 32;
	dd->ipath_pio2kbase =
		(u32 __iomem *) (((char __iomem *) dd->ipath_kregbase) +
				 (dd->ipath_piobufbase & 0xffffffff));
	if (dd->ipath_piobcnt4k) {
		dd->ipath_pio4kbase = (u32 __iomem *)
			(((char __iomem *) dd->ipath_kregbase) +
			 (dd->ipath_piobufbase >> 32));
		dd->ipath_4kalign = ALIGN(dd->ipath_piosize4k,
					  dd->ipath_palign);
		ipath_dbg("%u 2k(%x) piobufs @ %p, %u 4k(%x) @ %p "
			  "(%x aligned)\n",
			  dd->ipath_piobcnt2k, dd->ipath_piosize2k,
			  dd->ipath_pio2kbase, dd->ipath_piobcnt4k,
			  dd->ipath_piosize4k, dd->ipath_pio4kbase,
			  dd->ipath_4kalign);
	}
	else ipath_dbg("%u 2k piobufs @ %p\n",
		       dd->ipath_piobcnt2k, dd->ipath_pio2kbase);

done:
	return ret;
}

static int init_chip_reset(struct ipath_devdata *dd)
{
	u32 rtmp;
	int i;
	unsigned long flags;

	dd->ipath_rcvctrl &= ~(1ULL << dd->ipath_r_tailupd_shift);
	for (i = 0; i < dd->ipath_portcnt; i++) {
		clear_bit(dd->ipath_r_portenable_shift + i,
			  &dd->ipath_rcvctrl);
		clear_bit(dd->ipath_r_intravail_shift + i,
			  &dd->ipath_rcvctrl);
	}
	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvctrl,
		dd->ipath_rcvctrl);

	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	dd->ipath_sendctrl = 0U; 
	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl, dd->ipath_sendctrl);
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_control, 0ULL);

	rtmp = ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvtidcnt);
	if (rtmp != dd->ipath_rcvtidcnt)
		dev_info(&dd->pcidev->dev, "tidcnt was %u before "
			 "reset, now %u, using original\n",
			 dd->ipath_rcvtidcnt, rtmp);
	rtmp = ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvtidbase);
	if (rtmp != dd->ipath_rcvtidbase)
		dev_info(&dd->pcidev->dev, "tidbase was %u before "
			 "reset, now %u, using original\n",
			 dd->ipath_rcvtidbase, rtmp);
	rtmp = ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvegrcnt);
	if (rtmp != dd->ipath_rcvegrcnt)
		dev_info(&dd->pcidev->dev, "egrcnt was %u before "
			 "reset, now %u, using original\n",
			 dd->ipath_rcvegrcnt, rtmp);
	rtmp = ipath_read_kreg32(dd, dd->ipath_kregs->kr_rcvegrbase);
	if (rtmp != dd->ipath_rcvegrbase)
		dev_info(&dd->pcidev->dev, "egrbase was %u before "
			 "reset, now %u, using original\n",
			 dd->ipath_rcvegrbase, rtmp);

	return 0;
}

static int init_pioavailregs(struct ipath_devdata *dd)
{
	int ret;

	dd->ipath_pioavailregs_dma = dma_alloc_coherent(
		&dd->pcidev->dev, PAGE_SIZE, &dd->ipath_pioavailregs_phys,
		GFP_KERNEL);
	if (!dd->ipath_pioavailregs_dma) {
		ipath_dev_err(dd, "failed to allocate PIOavail reg area "
			      "in memory\n");
		ret = -ENOMEM;
		goto done;
	}

	dd->ipath_statusp = (u64 *)
		((char *)dd->ipath_pioavailregs_dma +
		 ((2 * L1_CACHE_BYTES +
		   dd->ipath_pioavregs * sizeof(u64)) & ~L1_CACHE_BYTES));
	
	*dd->ipath_statusp = dd->_ipath_status;
	dd->ipath_freezemsg = (char *)&dd->ipath_statusp[1];
	
	dd->ipath_freezelen = L1_CACHE_BYTES - sizeof(dd->ipath_statusp[0]);

	ret = 0;

done:
	return ret;
}

static void init_shadow_tids(struct ipath_devdata *dd)
{
	struct page **pages;
	dma_addr_t *addrs;

	pages = vzalloc(dd->ipath_cfgports * dd->ipath_rcvtidcnt *
			sizeof(struct page *));
	if (!pages) {
		ipath_dev_err(dd, "failed to allocate shadow page * "
			      "array, no expected sends!\n");
		dd->ipath_pageshadow = NULL;
		return;
	}

	addrs = vmalloc(dd->ipath_cfgports * dd->ipath_rcvtidcnt *
			sizeof(dma_addr_t));
	if (!addrs) {
		ipath_dev_err(dd, "failed to allocate shadow dma handle "
			      "array, no expected sends!\n");
		vfree(pages);
		dd->ipath_pageshadow = NULL;
		return;
	}

	dd->ipath_pageshadow = pages;
	dd->ipath_physshadow = addrs;
}

static void enable_chip(struct ipath_devdata *dd, int reinit)
{
	u32 val;
	u64 rcvmask;
	unsigned long flags;
	int i;

	if (!reinit)
		init_waitqueue_head(&ipath_state_wait);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvctrl,
			 dd->ipath_rcvctrl);

	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	
	dd->ipath_sendctrl = INFINIPATH_S_PIOENABLE |
		INFINIPATH_S_PIOBUFAVAILUPD;

	if (dd->ipath_pioupd_thresh)
		dd->ipath_sendctrl |= dd->ipath_pioupd_thresh
			<< INFINIPATH_S_UPDTHRESH_SHIFT;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl, dd->ipath_sendctrl);
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);

	rcvmask = 1ULL;
	dd->ipath_rcvctrl |= (rcvmask << dd->ipath_r_portenable_shift) |
		(rcvmask << dd->ipath_r_intravail_shift);
	if (!(dd->ipath_flags & IPATH_NODMA_RTAIL))
		dd->ipath_rcvctrl |= (1ULL << dd->ipath_r_tailupd_shift);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvctrl,
			 dd->ipath_rcvctrl);

	dd->ipath_flags |= IPATH_INITTED;

	val = ipath_read_ureg32(dd, ur_rcvegrindextail, 0);
	ipath_write_ureg(dd, ur_rcvegrindexhead, val, 0);

	
	ipath_write_ureg(dd, ur_rcvhdrhead,
			 dd->ipath_rhdrhead_intr_off |
			 dd->ipath_pd[0]->port_head, 0);

	for (i = 0; i < dd->ipath_pioavregs; i++) {
		__le64 pioavail;

		if (i > 3 && (dd->ipath_flags & IPATH_SWAP_PIOBUFS))
			pioavail = dd->ipath_pioavailregs_dma[i ^ 1];
		else
			pioavail = dd->ipath_pioavailregs_dma[i];
		dd->ipath_pioavailshadow[i] = le64_to_cpu(pioavail);
	}
	
	dd->ipath_flags |= IPATH_PRESENT;
}

static int init_housekeeping(struct ipath_devdata *dd, int reinit)
{
	char boardn[40];
	int ret = 0;

	dd->ipath_rcvhdrsize = 0;

	dd->ipath_flags |= IPATH_LINKUNK | IPATH_PRESENT;
	dd->ipath_flags &= ~(IPATH_LINKACTIVE | IPATH_LINKARMED |
			     IPATH_LINKDOWN | IPATH_LINKINIT);

	ipath_cdbg(VERBOSE, "Try to read spc chip revision\n");
	dd->ipath_revision =
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_revision);

	dd->ipath_sregbase =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_sendregbase);
	dd->ipath_cregbase =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_counterregbase);
	dd->ipath_uregbase =
		ipath_read_kreg32(dd, dd->ipath_kregs->kr_userregbase);
	ipath_cdbg(VERBOSE, "ipath_kregbase %p, sendbase %x usrbase %x, "
		   "cntrbase %x\n", dd->ipath_kregbase, dd->ipath_sregbase,
		   dd->ipath_uregbase, dd->ipath_cregbase);
	if ((dd->ipath_revision & 0xffffffff) == 0xffffffff
	    || (dd->ipath_sregbase & 0xffffffff) == 0xffffffff
	    || (dd->ipath_cregbase & 0xffffffff) == 0xffffffff
	    || (dd->ipath_uregbase & 0xffffffff) == 0xffffffff) {
		ipath_dev_err(dd, "Register read failures from chip, "
			      "giving up initialization\n");
		dd->ipath_flags &= ~IPATH_PRESENT;
		ret = -ENODEV;
		goto done;
	}


	
	ipath_write_kreg (dd, dd->ipath_kregs->kr_hwdiagctrl, 0);

	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear,
			 INFINIPATH_E_RESET);

	ipath_cdbg(VERBOSE, "Revision %llx (PCI %x)\n",
		   (unsigned long long) dd->ipath_revision,
		   dd->ipath_pcirev);

	if (((dd->ipath_revision >> INFINIPATH_R_SOFTWARE_SHIFT) &
	     INFINIPATH_R_SOFTWARE_MASK) != IPATH_CHIP_SWVERSION) {
		ipath_dev_err(dd, "Driver only handles version %d, "
			      "chip swversion is %d (%llx), failng\n",
			      IPATH_CHIP_SWVERSION,
			      (int)(dd->ipath_revision >>
				    INFINIPATH_R_SOFTWARE_SHIFT) &
			      INFINIPATH_R_SOFTWARE_MASK,
			      (unsigned long long) dd->ipath_revision);
		ret = -ENOSYS;
		goto done;
	}
	dd->ipath_majrev = (u8) ((dd->ipath_revision >>
				  INFINIPATH_R_CHIPREVMAJOR_SHIFT) &
				 INFINIPATH_R_CHIPREVMAJOR_MASK);
	dd->ipath_minrev = (u8) ((dd->ipath_revision >>
				  INFINIPATH_R_CHIPREVMINOR_SHIFT) &
				 INFINIPATH_R_CHIPREVMINOR_MASK);
	dd->ipath_boardrev = (u8) ((dd->ipath_revision >>
				    INFINIPATH_R_BOARDID_SHIFT) &
				   INFINIPATH_R_BOARDID_MASK);

	ret = dd->ipath_f_get_boardname(dd, boardn, sizeof boardn);

	snprintf(dd->ipath_boardversion, sizeof(dd->ipath_boardversion),
		 "ChipABI %u.%u, %s, InfiniPath%u %u.%u, PCI %u, "
		 "SW Compat %u\n",
		 IPATH_CHIP_VERS_MAJ, IPATH_CHIP_VERS_MIN, boardn,
		 (unsigned)(dd->ipath_revision >> INFINIPATH_R_ARCH_SHIFT) &
		 INFINIPATH_R_ARCH_MASK,
		 dd->ipath_majrev, dd->ipath_minrev, dd->ipath_pcirev,
		 (unsigned)(dd->ipath_revision >>
			    INFINIPATH_R_SOFTWARE_SHIFT) &
		 INFINIPATH_R_SOFTWARE_MASK);

	ipath_dbg("%s", dd->ipath_boardversion);

	if (ret)
		goto done;

	if (reinit)
		ret = init_chip_reset(dd);
	else
		ret = init_chip_first(dd);

done:
	return ret;
}

static void verify_interrupt(unsigned long opaque)
{
	struct ipath_devdata *dd = (struct ipath_devdata *) opaque;

	if (!dd)
		return; 

	if (dd->ipath_int_counter == 0) {
		if (!dd->ipath_f_intr_fallback(dd))
			dev_err(&dd->pcidev->dev, "No interrupts detected, "
				"not usable.\n");
		else 
			mod_timer(&dd->ipath_intrchk_timer, jiffies + HZ/2);
	} else
		ipath_cdbg(VERBOSE, "%u interrupts at timer check\n",
			dd->ipath_int_counter);
}

int ipath_init_chip(struct ipath_devdata *dd, int reinit)
{
	int ret = 0;
	u32 kpiobufs, defkbufs;
	u32 piobufs, uports;
	u64 val;
	struct ipath_portdata *pd;
	gfp_t gfp_flags = GFP_USER | __GFP_COMP;

	ret = init_housekeeping(dd, reinit);
	if (ret)
		goto done;

	if (ret == 2) {
		
		ret = -EPERM;
		goto done;
	}

	dd->ipath_rcvhdrcnt = max(dd->ipath_p0_rcvegrcnt, dd->ipath_rcvegrcnt);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvhdrcnt,
			 dd->ipath_rcvhdrcnt);

	piobufs = dd->ipath_piobcnt2k + dd->ipath_piobcnt4k;
	dd->ipath_pioavregs = ALIGN(piobufs, sizeof(u64) * BITS_PER_BYTE / 2)
		/ (sizeof(u64) * BITS_PER_BYTE / 2);
	uports = dd->ipath_cfgports ? dd->ipath_cfgports - 1 : 0;
	if (piobufs > 144)
		defkbufs = 32 + dd->ipath_pioreserved;
	else
		defkbufs = 16 + dd->ipath_pioreserved;

	if (ipath_kpiobufs && (ipath_kpiobufs +
		(uports * IPATH_MIN_USER_PORT_BUFCNT)) > piobufs) {
		int i = (int) piobufs -
			(int) (uports * IPATH_MIN_USER_PORT_BUFCNT);
		if (i < 1)
			i = 1;
		dev_info(&dd->pcidev->dev, "Allocating %d PIO bufs of "
			 "%d for kernel leaves too few for %d user ports "
			 "(%d each); using %u\n", ipath_kpiobufs,
			 piobufs, uports, IPATH_MIN_USER_PORT_BUFCNT, i);
		kpiobufs = i;
	} else if (ipath_kpiobufs)
		kpiobufs = ipath_kpiobufs;
	else
		kpiobufs = defkbufs;
	dd->ipath_lastport_piobuf = piobufs - kpiobufs;
	dd->ipath_pbufsport =
		uports ? dd->ipath_lastport_piobuf / uports : 0;
	
	dd->ipath_ports_extrabuf = dd->ipath_lastport_piobuf -
		(dd->ipath_pbufsport * uports);
	if (dd->ipath_ports_extrabuf)
		ipath_dbg("%u pbufs/port leaves some unused, add 1 buffer to "
			"ports <= %u\n", dd->ipath_pbufsport,
			dd->ipath_ports_extrabuf);
	dd->ipath_lastpioindex = 0;
	dd->ipath_lastpioindexl = dd->ipath_piobcnt2k;
	
	ipath_cdbg(VERBOSE, "%d PIO bufs for kernel out of %d total %u "
		   "each for %u user ports\n", kpiobufs,
		   piobufs, dd->ipath_pbufsport, uports);
	ret = dd->ipath_f_early_init(dd);
	if (ret) {
		ipath_dev_err(dd, "Early initialization failure\n");
		goto done;
	}

	dd->ipath_hdrqlast =
		dd->ipath_rcvhdrentsize * (dd->ipath_rcvhdrcnt - 1);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvhdrentsize,
			 dd->ipath_rcvhdrentsize);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvhdrsize,
			 dd->ipath_rcvhdrsize);

	if (!reinit) {
		ret = init_pioavailregs(dd);
		init_shadow_tids(dd);
		if (ret)
			goto done;
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendpioavailaddr,
			 dd->ipath_pioavailregs_phys);

	val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_sendpioavailaddr);
	if (val != dd->ipath_pioavailregs_phys) {
		ipath_dev_err(dd, "Catastrophic software error, "
			      "SendPIOAvailAddr written as %lx, "
			      "read back as %llx\n",
			      (unsigned long) dd->ipath_pioavailregs_phys,
			      (unsigned long long) val);
		ret = -EINVAL;
		goto done;
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvbthqp, IPATH_KD_QP);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrmask, 0ULL);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrclear,
			 ~0ULL&~INFINIPATH_HWE_MEMBISTFAILED);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_control, 0ULL);

	if (bringup_link(dd)) {
		dev_info(&dd->pcidev->dev, "Failed to bringup IB link\n");
		ret = -ENETDOWN;
		goto done;
	}

	dd->ipath_f_init_hwerrors(dd);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrclear,
			 ~0ULL&~INFINIPATH_HWE_MEMBISTFAILED);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrmask,
			 dd->ipath_hwerrmask);

	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear, -1LL);
	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask,
			 ~dd->ipath_maskederrs);
	dd->ipath_maskederrs = 0; 
	dd->ipath_errormask =
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_errormask);
	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear, -1LL);

	dd->ipath_f_tidtemplate(dd);

	pd = dd->ipath_pd[0];
	if (reinit) {
		struct ipath_portdata *npd;

		npd = create_portdata0(dd);
		if (npd) {
			ipath_free_pddata(dd, pd);
			dd->ipath_pd[0] = npd;
			pd = npd;
		} else {
			ipath_dev_err(dd, "Unable to allocate portdata"
				      " for port 0, failing\n");
			ret = -ENOMEM;
			goto done;
		}
	}
	ret = ipath_create_rcvhdrq(dd, pd);
	if (!ret)
		ret = create_port0_egr(dd);
	if (ret) {
		ipath_dev_err(dd, "failed to allocate kernel port's "
			      "rcvhdrq and/or egr bufs\n");
		goto done;
	}
	else
		enable_chip(dd, reinit);

	
	ipath_chg_pioavailkernel(dd, 0, piobufs, 1);

	ipath_cancel_sends(dd, 1);

	if (!reinit) {
		dd->ipath_dummy_hdrq = dma_alloc_coherent(
			&dd->pcidev->dev, dd->ipath_pd[0]->port_rcvhdrq_size,
			&dd->ipath_dummy_hdrq_phys,
			gfp_flags);
		if (!dd->ipath_dummy_hdrq) {
			dev_info(&dd->pcidev->dev,
				"Couldn't allocate 0x%lx bytes for dummy hdrq\n",
				dd->ipath_pd[0]->port_rcvhdrq_size);
			
			dd->ipath_dummy_hdrq_phys = 0UL;
		}
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear, 0ULL);

	if (!dd->ipath_stats_timer_active) {
		init_timer(&dd->ipath_stats_timer);
		dd->ipath_stats_timer.function = ipath_get_faststats;
		dd->ipath_stats_timer.data = (unsigned long) dd;
		
		dd->ipath_stats_timer.expires = jiffies + 5 * HZ;
		
		add_timer(&dd->ipath_stats_timer);
		dd->ipath_stats_timer_active = 1;
	}

	
	if (dd->ipath_flags & IPATH_HAS_SEND_DMA)
		ret = setup_sdma(dd);

	
	init_timer(&dd->ipath_hol_timer);
	dd->ipath_hol_timer.function = ipath_hol_event;
	dd->ipath_hol_timer.data = (unsigned long)dd;
	dd->ipath_hol_state = IPATH_HOL_UP;

done:
	if (!ret) {
		*dd->ipath_statusp |= IPATH_STATUS_CHIP_PRESENT;
		if (!dd->ipath_f_intrsetup(dd)) {
			
			ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask,
					 -1LL);
			
			ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear,
					 0ULL);
			
			*dd->ipath_statusp |= IPATH_STATUS_INITTED;

			if (!reinit) {
				init_timer(&dd->ipath_intrchk_timer);
				dd->ipath_intrchk_timer.function =
					verify_interrupt;
				dd->ipath_intrchk_timer.data =
					(unsigned long) dd;
			}
			dd->ipath_intrchk_timer.expires = jiffies + HZ/2;
			add_timer(&dd->ipath_intrchk_timer);
		} else
			ipath_dev_err(dd, "No interrupts enabled, couldn't "
				      "setup interrupt address\n");

		if (dd->ipath_cfgports > ipath_stats.sps_nports)
			ipath_stats.sps_nports = dd->ipath_cfgports;
	} else
		ipath_dbg("Failed (%d) to initialize chip\n", ret);

	return ret;
}

static int ipath_set_kpiobufs(const char *str, struct kernel_param *kp)
{
	struct ipath_devdata *dd;
	unsigned long flags;
	unsigned short val;
	int ret;

	ret = ipath_parse_ushort(str, &val);

	spin_lock_irqsave(&ipath_devs_lock, flags);

	if (ret < 0)
		goto bail;

	if (val == 0) {
		ret = -EINVAL;
		goto bail;
	}

	list_for_each_entry(dd, &ipath_dev_list, ipath_list) {
		if (dd->ipath_kregbase)
			continue;
		if (val > (dd->ipath_piobcnt2k + dd->ipath_piobcnt4k -
			   (dd->ipath_cfgports *
			    IPATH_MIN_USER_PORT_BUFCNT)))
		{
			ipath_dev_err(
				dd,
				"Allocating %d PIO bufs for kernel leaves "
				"too few for %d user ports (%d each)\n",
				val, dd->ipath_cfgports - 1,
				IPATH_MIN_USER_PORT_BUFCNT);
			ret = -EINVAL;
			goto bail;
		}
		dd->ipath_lastport_piobuf =
			dd->ipath_piobcnt2k + dd->ipath_piobcnt4k - val;
	}

	ipath_kpiobufs = val;
	ret = 0;
bail:
	spin_unlock_irqrestore(&ipath_devs_lock, flags);

	return ret;
}
