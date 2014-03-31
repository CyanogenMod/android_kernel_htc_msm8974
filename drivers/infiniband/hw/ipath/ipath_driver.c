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

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/idr.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "ipath_kernel.h"
#include "ipath_verbs.h"

static void ipath_update_pio_bufs(struct ipath_devdata *);

const char *ipath_get_unit_name(int unit)
{
	static char iname[16];
	snprintf(iname, sizeof iname, "infinipath%u", unit);
	return iname;
}

#define DRIVER_LOAD_MSG "QLogic " IPATH_DRV_NAME " loaded: "
#define PFX IPATH_DRV_NAME ": "

const char ib_ipath_version[] = IPATH_IDSTR "\n";

static struct idr unit_table;
DEFINE_SPINLOCK(ipath_devs_lock);
LIST_HEAD(ipath_dev_list);

wait_queue_head_t ipath_state_wait;

unsigned ipath_debug = __IPATH_INFO;

module_param_named(debug, ipath_debug, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(debug, "mask for debug prints");
EXPORT_SYMBOL_GPL(ipath_debug);

unsigned ipath_mtu4096 = 1; 
module_param_named(mtu4096, ipath_mtu4096, uint, S_IRUGO);
MODULE_PARM_DESC(mtu4096, "enable MTU of 4096 bytes, if supported");

static unsigned ipath_hol_timeout_ms = 13000;
module_param_named(hol_timeout_ms, ipath_hol_timeout_ms, uint, S_IRUGO);
MODULE_PARM_DESC(hol_timeout_ms,
	"duration of user app suspension after link failure");

unsigned ipath_linkrecovery = 1;
module_param_named(linkrecovery, ipath_linkrecovery, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(linkrecovery, "enable workaround for link recovery issue");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("QLogic <support@qlogic.com>");
MODULE_DESCRIPTION("QLogic InfiniPath driver");

const char *ipath_ibcstatus_str[] = {
	"Disabled",
	"LinkUp",
	"PollActive",
	"PollQuiet",
	"SleepDelay",
	"SleepQuiet",
	"LState6",		
	"LState7",		
	"CfgDebounce",
	"CfgRcvfCfg",
	"CfgWaitRmt",
	"CfgIdle",
	"RecovRetrain",
	"CfgTxRevLane",		
	"RecovWaitRmt",
	"RecovIdle",
	
	"CfgEnhanced",
	"CfgTest",
	"CfgWaitRmtTest",
	"CfgWaitCfgEnhanced",
	"SendTS_T",
	"SendTstIdles",
	"RcvTS_T",
	"SendTst_TS1s",
	"LTState18", "LTState19", "LTState1A", "LTState1B",
	"LTState1C", "LTState1D", "LTState1E", "LTState1F"
};

static void __devexit ipath_remove_one(struct pci_dev *);
static int __devinit ipath_init_one(struct pci_dev *,
				    const struct pci_device_id *);

#define PCI_VENDOR_ID_PATHSCALE 0x1fc1
#define PCI_DEVICE_ID_INFINIPATH_HT 0xd

#define STATUS_TIMEOUT 60

static const struct pci_device_id ipath_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_PATHSCALE, PCI_DEVICE_ID_INFINIPATH_HT) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, ipath_pci_tbl);

static struct pci_driver ipath_driver = {
	.name = IPATH_DRV_NAME,
	.probe = ipath_init_one,
	.remove = __devexit_p(ipath_remove_one),
	.id_table = ipath_pci_tbl,
	.driver = {
		.groups = ipath_driver_attr_groups,
	},
};

static inline void read_bars(struct ipath_devdata *dd, struct pci_dev *dev,
			     u32 *bar0, u32 *bar1)
{
	int ret;

	ret = pci_read_config_dword(dev, PCI_BASE_ADDRESS_0, bar0);
	if (ret)
		ipath_dev_err(dd, "failed to read bar0 before enable: "
			      "error %d\n", -ret);

	ret = pci_read_config_dword(dev, PCI_BASE_ADDRESS_1, bar1);
	if (ret)
		ipath_dev_err(dd, "failed to read bar1 before enable: "
			      "error %d\n", -ret);

	ipath_dbg("Read bar0 %x bar1 %x\n", *bar0, *bar1);
}

static void ipath_free_devdata(struct pci_dev *pdev,
			       struct ipath_devdata *dd)
{
	unsigned long flags;

	pci_set_drvdata(pdev, NULL);

	if (dd->ipath_unit != -1) {
		spin_lock_irqsave(&ipath_devs_lock, flags);
		idr_remove(&unit_table, dd->ipath_unit);
		list_del(&dd->ipath_list);
		spin_unlock_irqrestore(&ipath_devs_lock, flags);
	}
	vfree(dd);
}

static struct ipath_devdata *ipath_alloc_devdata(struct pci_dev *pdev)
{
	unsigned long flags;
	struct ipath_devdata *dd;
	int ret;

	if (!idr_pre_get(&unit_table, GFP_KERNEL)) {
		dd = ERR_PTR(-ENOMEM);
		goto bail;
	}

	dd = vzalloc(sizeof(*dd));
	if (!dd) {
		dd = ERR_PTR(-ENOMEM);
		goto bail;
	}
	dd->ipath_unit = -1;

	spin_lock_irqsave(&ipath_devs_lock, flags);

	ret = idr_get_new(&unit_table, dd, &dd->ipath_unit);
	if (ret < 0) {
		printk(KERN_ERR IPATH_DRV_NAME
		       ": Could not allocate unit ID: error %d\n", -ret);
		ipath_free_devdata(pdev, dd);
		dd = ERR_PTR(ret);
		goto bail_unlock;
	}

	dd->pcidev = pdev;
	pci_set_drvdata(pdev, dd);

	list_add(&dd->ipath_list, &ipath_dev_list);

bail_unlock:
	spin_unlock_irqrestore(&ipath_devs_lock, flags);

bail:
	return dd;
}

static inline struct ipath_devdata *__ipath_lookup(int unit)
{
	return idr_find(&unit_table, unit);
}

struct ipath_devdata *ipath_lookup(int unit)
{
	struct ipath_devdata *dd;
	unsigned long flags;

	spin_lock_irqsave(&ipath_devs_lock, flags);
	dd = __ipath_lookup(unit);
	spin_unlock_irqrestore(&ipath_devs_lock, flags);

	return dd;
}

int ipath_count_units(int *npresentp, int *nupp, int *maxportsp)
{
	int nunits, npresent, nup;
	struct ipath_devdata *dd;
	unsigned long flags;
	int maxports;

	nunits = npresent = nup = maxports = 0;

	spin_lock_irqsave(&ipath_devs_lock, flags);

	list_for_each_entry(dd, &ipath_dev_list, ipath_list) {
		nunits++;
		if ((dd->ipath_flags & IPATH_PRESENT) && dd->ipath_kregbase)
			npresent++;
		if (dd->ipath_lid &&
		    !(dd->ipath_flags & (IPATH_DISABLED | IPATH_LINKDOWN
					 | IPATH_LINKUNK)))
			nup++;
		if (dd->ipath_cfgports > maxports)
			maxports = dd->ipath_cfgports;
	}

	spin_unlock_irqrestore(&ipath_devs_lock, flags);

	if (npresentp)
		*npresentp = npresent;
	if (nupp)
		*nupp = nup;
	if (maxportsp)
		*maxportsp = maxports;

	return nunits;
}


int __attribute__((weak)) ipath_enable_wc(struct ipath_devdata *dd)
{
	return -EOPNOTSUPP;
}

void __attribute__((weak)) ipath_disable_wc(struct ipath_devdata *dd)
{
}

static void ipath_verify_pioperf(struct ipath_devdata *dd)
{
	u32 pbnum, cnt, lcnt;
	u32 __iomem *piobuf;
	u32 *addr;
	u64 msecs, emsecs;

	piobuf = ipath_getpiobuf(dd, 0, &pbnum);
	if (!piobuf) {
		dev_info(&dd->pcidev->dev,
			"No PIObufs for checking perf, skipping\n");
		return;
	}

	cnt = 1024;

	addr = vmalloc(cnt);
	if (!addr) {
		dev_info(&dd->pcidev->dev,
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

	ipath_disable_armlaunch(dd);

	if ((dd->ipath_flags & IPATH_HAS_PBC_CNT))
		writeq(1UL << 63, piobuf);
	else
		writeq(0, piobuf);
	ipath_flush_wc();

	msecs = jiffies_to_msecs(jiffies);
	for (emsecs = lcnt = 0; emsecs <= 5UL; lcnt++) {
		__iowrite32_copy(piobuf + 64, addr, cnt >> 2);
		emsecs = jiffies_to_msecs(jiffies) - msecs;
	}

	
	if (lcnt < (emsecs * 1024U))
		ipath_dev_err(dd,
			"Performance problem: bandwidth to PIO buffers is "
			"only %u MiB/sec\n",
			lcnt / (u32) emsecs);
	else
		ipath_dbg("PIO buffer bandwidth %u MiB/sec is OK\n",
			lcnt / (u32) emsecs);

	preempt_enable();

	vfree(addr);

done:
	
	ipath_disarm_piobufs(dd, pbnum, 1);
	ipath_enable_armlaunch(dd);
}

static void cleanup_device(struct ipath_devdata *dd);

static int __devinit ipath_init_one(struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	int ret, len, j;
	struct ipath_devdata *dd;
	unsigned long long addr;
	u32 bar0 = 0, bar1 = 0;

	dd = ipath_alloc_devdata(pdev);
	if (IS_ERR(dd)) {
		ret = PTR_ERR(dd);
		printk(KERN_ERR IPATH_DRV_NAME
		       ": Could not allocate devdata: error %d\n", -ret);
		goto bail;
	}

	ipath_cdbg(VERBOSE, "initializing unit #%u\n", dd->ipath_unit);

	ret = pci_enable_device(pdev);
	if (ret) {
		ipath_dev_err(dd, "enable unit %d failed: error %d\n",
			      dd->ipath_unit, -ret);
		goto bail_devdata;
	}
	addr = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);
	ipath_cdbg(VERBOSE, "regbase (0) %llx len %d irq %d, vend %x/%x "
		   "driver_data %lx\n", addr, len, pdev->irq, ent->vendor,
		   ent->device, ent->driver_data);

	read_bars(dd, pdev, &bar0, &bar1);

	if (!bar1 && !(bar0 & ~0xf)) {
		if (addr) {
			dev_info(&pdev->dev, "BAR is 0 (probable RESET), "
				 "rewriting as %llx\n", addr);
			ret = pci_write_config_dword(
				pdev, PCI_BASE_ADDRESS_0, addr);
			if (ret) {
				ipath_dev_err(dd, "rewrite of BAR0 "
					      "failed: err %d\n", -ret);
				goto bail_disable;
			}
			ret = pci_write_config_dword(
				pdev, PCI_BASE_ADDRESS_1, addr >> 32);
			if (ret) {
				ipath_dev_err(dd, "rewrite of BAR1 "
					      "failed: err %d\n", -ret);
				goto bail_disable;
			}
		} else {
			ipath_dev_err(dd, "BAR is 0 (probable RESET), "
				      "not usable until reboot\n");
			ret = -ENODEV;
			goto bail_disable;
		}
	}

	ret = pci_request_regions(pdev, IPATH_DRV_NAME);
	if (ret) {
		dev_info(&pdev->dev, "pci_request_regions unit %u fails: "
			 "err %d\n", dd->ipath_unit, -ret);
		goto bail_disable;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (ret) {
			dev_info(&pdev->dev,
				"Unable to set DMA mask for unit %u: %d\n",
				dd->ipath_unit, ret);
			goto bail_regions;
		}
		else {
			ipath_dbg("No 64bit DMA mask, used 32 bit mask\n");
			ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
			if (ret)
				dev_info(&pdev->dev,
					"Unable to set DMA consistent mask "
					"for unit %u: %d\n",
					dd->ipath_unit, ret);

		}
	}
	else {
		ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (ret)
			dev_info(&pdev->dev,
				"Unable to set DMA consistent mask "
				"for unit %u: %d\n",
				dd->ipath_unit, ret);
	}

	pci_set_master(pdev);

	dd->ipath_pcibar0 = addr;
	dd->ipath_pcibar1 = addr >> 32;
	dd->ipath_deviceid = ent->device;	
	dd->ipath_vendorid = ent->vendor;

	
	switch (ent->device) {
	case PCI_DEVICE_ID_INFINIPATH_HT:
		ipath_init_iba6110_funcs(dd);
		break;

	default:
		ipath_dev_err(dd, "Found unknown QLogic deviceid 0x%x, "
			      "failing\n", ent->device);
		return -ENODEV;
	}

	for (j = 0; j < 6; j++) {
		if (!pdev->resource[j].start)
			continue;
		ipath_cdbg(VERBOSE, "BAR %d %pR, len %llx\n",
			   j, &pdev->resource[j],
			   (unsigned long long)pci_resource_len(pdev, j));
	}

	if (!addr) {
		ipath_dev_err(dd, "No valid address in BAR 0!\n");
		ret = -ENODEV;
		goto bail_regions;
	}

	dd->ipath_pcirev = pdev->revision;

#if defined(__powerpc__)
	
	dd->ipath_kregbase = __ioremap(addr, len,
		(_PAGE_NO_CACHE|_PAGE_WRITETHRU));
#else
	dd->ipath_kregbase = ioremap_nocache(addr, len);
#endif

	if (!dd->ipath_kregbase) {
		ipath_dbg("Unable to map io addr %llx to kvirt, failing\n",
			  addr);
		ret = -ENOMEM;
		goto bail_iounmap;
	}
	dd->ipath_kregend = (u64 __iomem *)
		((void __iomem *)dd->ipath_kregbase + len);
	dd->ipath_physaddr = addr;	
	
	ipath_cdbg(VERBOSE, "mapped io addr %llx to kregbase %p\n",
		   addr, dd->ipath_kregbase);

	if (dd->ipath_f_bus(dd, pdev))
		ipath_dev_err(dd, "Failed to setup config space; "
			      "continuing anyway\n");

	if (!dd->ipath_irq)
		ipath_dev_err(dd, "irq is 0, BIOS error?  Interrupts won't "
			      "work\n");
	else {
		ret = request_irq(dd->ipath_irq, ipath_intr, IRQF_SHARED,
				  IPATH_DRV_NAME, dd);
		if (ret) {
			ipath_dev_err(dd, "Couldn't setup irq handler, "
				      "irq=%d: %d\n", dd->ipath_irq, ret);
			goto bail_iounmap;
		}
	}

	ret = ipath_init_chip(dd, 0);	
	if (ret)
		goto bail_irqsetup;

	ret = ipath_enable_wc(dd);

	if (ret) {
		ipath_dev_err(dd, "Write combining not enabled "
			      "(err %d): performance may be poor\n",
			      -ret);
		ret = 0;
	}

	ipath_verify_pioperf(dd);

	ipath_device_create_group(&pdev->dev, dd);
	ipathfs_add_device(dd);
	ipath_user_add(dd);
	ipath_diag_add(dd);
	ipath_register_ib_device(dd);

	goto bail;

bail_irqsetup:
	cleanup_device(dd);

	if (dd->ipath_irq)
		dd->ipath_f_free_irq(dd);

	if (dd->ipath_f_cleanup)
		dd->ipath_f_cleanup(dd);

bail_iounmap:
	iounmap((volatile void __iomem *) dd->ipath_kregbase);

bail_regions:
	pci_release_regions(pdev);

bail_disable:
	pci_disable_device(pdev);

bail_devdata:
	ipath_free_devdata(pdev, dd);

bail:
	return ret;
}

static void cleanup_device(struct ipath_devdata *dd)
{
	int port;
	struct ipath_portdata **tmp;
	unsigned long flags;

	if (*dd->ipath_statusp & IPATH_STATUS_CHIP_PRESENT) {
		
		*dd->ipath_statusp &= ~IPATH_STATUS_CHIP_PRESENT;
		if (dd->ipath_kregbase) {
			dd->ipath_kregbase = NULL;
			dd->ipath_uregbase = 0;
			dd->ipath_sregbase = 0;
			dd->ipath_cregbase = 0;
			dd->ipath_kregsize = 0;
		}
		ipath_disable_wc(dd);
	}

	if (dd->ipath_spectriggerhit)
		dev_info(&dd->pcidev->dev, "%lu special trigger hits\n",
			 dd->ipath_spectriggerhit);

	if (dd->ipath_pioavailregs_dma) {
		dma_free_coherent(&dd->pcidev->dev, PAGE_SIZE,
				  (void *) dd->ipath_pioavailregs_dma,
				  dd->ipath_pioavailregs_phys);
		dd->ipath_pioavailregs_dma = NULL;
	}
	if (dd->ipath_dummy_hdrq) {
		dma_free_coherent(&dd->pcidev->dev,
			dd->ipath_pd[0]->port_rcvhdrq_size,
			dd->ipath_dummy_hdrq, dd->ipath_dummy_hdrq_phys);
		dd->ipath_dummy_hdrq = NULL;
	}

	if (dd->ipath_pageshadow) {
		struct page **tmpp = dd->ipath_pageshadow;
		dma_addr_t *tmpd = dd->ipath_physshadow;
		int i, cnt = 0;

		ipath_cdbg(VERBOSE, "Unlocking any expTID pages still "
			   "locked\n");
		for (port = 0; port < dd->ipath_cfgports; port++) {
			int port_tidbase = port * dd->ipath_rcvtidcnt;
			int maxtid = port_tidbase + dd->ipath_rcvtidcnt;
			for (i = port_tidbase; i < maxtid; i++) {
				if (!tmpp[i])
					continue;
				pci_unmap_page(dd->pcidev, tmpd[i],
					PAGE_SIZE, PCI_DMA_FROMDEVICE);
				ipath_release_user_pages(&tmpp[i], 1);
				tmpp[i] = NULL;
				cnt++;
			}
		}
		if (cnt) {
			ipath_stats.sps_pageunlocks += cnt;
			ipath_cdbg(VERBOSE, "There were still %u expTID "
				   "entries locked\n", cnt);
		}
		if (ipath_stats.sps_pagelocks ||
		    ipath_stats.sps_pageunlocks)
			ipath_cdbg(VERBOSE, "%llu pages locked, %llu "
				   "unlocked via ipath_m{un}lock\n",
				   (unsigned long long)
				   ipath_stats.sps_pagelocks,
				   (unsigned long long)
				   ipath_stats.sps_pageunlocks);

		ipath_cdbg(VERBOSE, "Free shadow page tid array at %p\n",
			   dd->ipath_pageshadow);
		tmpp = dd->ipath_pageshadow;
		dd->ipath_pageshadow = NULL;
		vfree(tmpp);

		dd->ipath_egrtidbase = NULL;
	}

	spin_lock_irqsave(&dd->ipath_uctxt_lock, flags);
	tmp = dd->ipath_pd;
	dd->ipath_pd = NULL;
	spin_unlock_irqrestore(&dd->ipath_uctxt_lock, flags);
	for (port = 0; port < dd->ipath_portcnt; port++) {
		struct ipath_portdata *pd = tmp[port];
		tmp[port] = NULL; 
		ipath_free_pddata(dd, pd);
	}
	kfree(tmp);
}

static void __devexit ipath_remove_one(struct pci_dev *pdev)
{
	struct ipath_devdata *dd = pci_get_drvdata(pdev);

	ipath_cdbg(VERBOSE, "removing, pdev=%p, dd=%p\n", pdev, dd);

	ipath_shutdown_device(dd);

	flush_workqueue(ib_wq);

	if (dd->verbs_dev)
		ipath_unregister_ib_device(dd->verbs_dev);

	ipath_diag_remove(dd);
	ipath_user_remove(dd);
	ipathfs_remove_device(dd);
	ipath_device_remove_group(&pdev->dev, dd);

	ipath_cdbg(VERBOSE, "Releasing pci memory regions, dd %p, "
		   "unit %u\n", dd, (u32) dd->ipath_unit);

	cleanup_device(dd);

	if (dd->ipath_irq) {
		ipath_cdbg(VERBOSE, "unit %u free irq %d\n",
			   dd->ipath_unit, dd->ipath_irq);
		dd->ipath_f_free_irq(dd);
	} else
		ipath_dbg("irq is 0, not doing free_irq "
			  "for unit %u\n", dd->ipath_unit);
	if (dd->ipath_f_cleanup)
		
		dd->ipath_f_cleanup(dd);

	ipath_cdbg(VERBOSE, "Unmapping kregbase %p\n", dd->ipath_kregbase);
	iounmap((volatile void __iomem *) dd->ipath_kregbase);
	pci_release_regions(pdev);
	ipath_cdbg(VERBOSE, "calling pci_disable_device\n");
	pci_disable_device(pdev);

	ipath_free_devdata(pdev, dd);
}

DEFINE_MUTEX(ipath_mutex);

static DEFINE_SPINLOCK(ipath_pioavail_lock);

void ipath_disarm_piobufs(struct ipath_devdata *dd, unsigned first,
			  unsigned cnt)
{
	unsigned i, last = first + cnt;
	unsigned long flags;

	ipath_cdbg(PKT, "disarm %u PIObufs first=%u\n", cnt, first);
	for (i = first; i < last; i++) {
		spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			dd->ipath_sendctrl | INFINIPATH_S_DISARM |
			(i << INFINIPATH_S_DISARMPIOBUF_SHIFT));
		
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
		spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
	}
	
	ipath_force_pio_avail_update(dd);
}

int ipath_wait_linkstate(struct ipath_devdata *dd, u32 state, int msecs)
{
	dd->ipath_state_wanted = state;
	wait_event_interruptible_timeout(ipath_state_wait,
					 (dd->ipath_flags & state),
					 msecs_to_jiffies(msecs));
	dd->ipath_state_wanted = 0;

	if (!(dd->ipath_flags & state)) {
		u64 val;
		ipath_cdbg(VERBOSE, "Didn't reach linkstate %s within %u"
			   " ms\n",
			   
			   (state & IPATH_LINKINIT) ? "INIT" :
			   ((state & IPATH_LINKDOWN) ? "DOWN" :
			    ((state & IPATH_LINKARMED) ? "ARM" : "ACTIVE")),
			   msecs);
		val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_ibcstatus);
		ipath_cdbg(VERBOSE, "ibcc=%llx ibcstatus=%llx (%s)\n",
			   (unsigned long long) ipath_read_kreg64(
				   dd, dd->ipath_kregs->kr_ibcctrl),
			   (unsigned long long) val,
			   ipath_ibcstatus_str[val & dd->ibcs_lts_mask]);
	}
	return (dd->ipath_flags & state) ? 0 : -ETIMEDOUT;
}

static void decode_sdma_errs(struct ipath_devdata *dd, ipath_err_t err,
	char *buf, size_t blen)
{
	static const struct {
		ipath_err_t err;
		const char *msg;
	} errs[] = {
		{ INFINIPATH_E_SDMAGENMISMATCH, "SDmaGenMismatch" },
		{ INFINIPATH_E_SDMAOUTOFBOUND, "SDmaOutOfBound" },
		{ INFINIPATH_E_SDMATAILOUTOFBOUND, "SDmaTailOutOfBound" },
		{ INFINIPATH_E_SDMABASE, "SDmaBase" },
		{ INFINIPATH_E_SDMA1STDESC, "SDma1stDesc" },
		{ INFINIPATH_E_SDMARPYTAG, "SDmaRpyTag" },
		{ INFINIPATH_E_SDMADWEN, "SDmaDwEn" },
		{ INFINIPATH_E_SDMAMISSINGDW, "SDmaMissingDw" },
		{ INFINIPATH_E_SDMAUNEXPDATA, "SDmaUnexpData" },
		{ INFINIPATH_E_SDMADESCADDRMISALIGN, "SDmaDescAddrMisalign" },
		{ INFINIPATH_E_SENDBUFMISUSE, "SendBufMisuse" },
		{ INFINIPATH_E_SDMADISABLED, "SDmaDisabled" },
	};
	int i;
	int expected;
	size_t bidx = 0;

	for (i = 0; i < ARRAY_SIZE(errs); i++) {
		expected = (errs[i].err != INFINIPATH_E_SDMADISABLED) ? 0 :
			test_bit(IPATH_SDMA_ABORTING, &dd->ipath_sdma_status);
		if ((err & errs[i].err) && !expected)
			bidx += snprintf(buf + bidx, blen - bidx,
					 "%s ", errs[i].msg);
	}
}

int ipath_decode_err(struct ipath_devdata *dd, char *buf, size_t blen,
	ipath_err_t err)
{
	int iserr = 1;
	*buf = '\0';
	if (err & INFINIPATH_E_PKTERRS) {
		if (!(err & ~INFINIPATH_E_PKTERRS))
			iserr = 0; 
		if (ipath_debug & __IPATH_ERRPKTDBG) {
			if (err & INFINIPATH_E_REBP)
				strlcat(buf, "EBP ", blen);
			if (err & INFINIPATH_E_RVCRC)
				strlcat(buf, "VCRC ", blen);
			if (err & INFINIPATH_E_RICRC) {
				strlcat(buf, "CRC ", blen);
				
				err &= INFINIPATH_E_RICRC;
			}
			if (err & INFINIPATH_E_RSHORTPKTLEN)
				strlcat(buf, "rshortpktlen ", blen);
			if (err & INFINIPATH_E_SDROPPEDDATAPKT)
				strlcat(buf, "sdroppeddatapkt ", blen);
			if (err & INFINIPATH_E_SPKTLEN)
				strlcat(buf, "spktlen ", blen);
		}
		if ((err & INFINIPATH_E_RICRC) &&
			!(err&(INFINIPATH_E_RVCRC|INFINIPATH_E_REBP)))
			strlcat(buf, "CRC ", blen);
		if (!iserr)
			goto done;
	}
	if (err & INFINIPATH_E_RHDRLEN)
		strlcat(buf, "rhdrlen ", blen);
	if (err & INFINIPATH_E_RBADTID)
		strlcat(buf, "rbadtid ", blen);
	if (err & INFINIPATH_E_RBADVERSION)
		strlcat(buf, "rbadversion ", blen);
	if (err & INFINIPATH_E_RHDR)
		strlcat(buf, "rhdr ", blen);
	if (err & INFINIPATH_E_SENDSPECIALTRIGGER)
		strlcat(buf, "sendspecialtrigger ", blen);
	if (err & INFINIPATH_E_RLONGPKTLEN)
		strlcat(buf, "rlongpktlen ", blen);
	if (err & INFINIPATH_E_RMAXPKTLEN)
		strlcat(buf, "rmaxpktlen ", blen);
	if (err & INFINIPATH_E_RMINPKTLEN)
		strlcat(buf, "rminpktlen ", blen);
	if (err & INFINIPATH_E_SMINPKTLEN)
		strlcat(buf, "sminpktlen ", blen);
	if (err & INFINIPATH_E_RFORMATERR)
		strlcat(buf, "rformaterr ", blen);
	if (err & INFINIPATH_E_RUNSUPVL)
		strlcat(buf, "runsupvl ", blen);
	if (err & INFINIPATH_E_RUNEXPCHAR)
		strlcat(buf, "runexpchar ", blen);
	if (err & INFINIPATH_E_RIBFLOW)
		strlcat(buf, "ribflow ", blen);
	if (err & INFINIPATH_E_SUNDERRUN)
		strlcat(buf, "sunderrun ", blen);
	if (err & INFINIPATH_E_SPIOARMLAUNCH)
		strlcat(buf, "spioarmlaunch ", blen);
	if (err & INFINIPATH_E_SUNEXPERRPKTNUM)
		strlcat(buf, "sunexperrpktnum ", blen);
	if (err & INFINIPATH_E_SDROPPEDSMPPKT)
		strlcat(buf, "sdroppedsmppkt ", blen);
	if (err & INFINIPATH_E_SMAXPKTLEN)
		strlcat(buf, "smaxpktlen ", blen);
	if (err & INFINIPATH_E_SUNSUPVL)
		strlcat(buf, "sunsupVL ", blen);
	if (err & INFINIPATH_E_INVALIDADDR)
		strlcat(buf, "invalidaddr ", blen);
	if (err & INFINIPATH_E_RRCVEGRFULL)
		strlcat(buf, "rcvegrfull ", blen);
	if (err & INFINIPATH_E_RRCVHDRFULL)
		strlcat(buf, "rcvhdrfull ", blen);
	if (err & INFINIPATH_E_IBSTATUSCHANGED)
		strlcat(buf, "ibcstatuschg ", blen);
	if (err & INFINIPATH_E_RIBLOSTLINK)
		strlcat(buf, "riblostlink ", blen);
	if (err & INFINIPATH_E_HARDWARE)
		strlcat(buf, "hardware ", blen);
	if (err & INFINIPATH_E_RESET)
		strlcat(buf, "reset ", blen);
	if (err & INFINIPATH_E_SDMAERRS)
		decode_sdma_errs(dd, err, buf, blen);
	if (err & INFINIPATH_E_INVALIDEEPCMD)
		strlcat(buf, "invalideepromcmd ", blen);
done:
	return iserr;
}

static void get_rhf_errstring(u32 err, char *msg, size_t len)
{
	
	*msg = '\0';

	if (err & INFINIPATH_RHF_H_ICRCERR)
		strlcat(msg, "icrcerr ", len);
	if (err & INFINIPATH_RHF_H_VCRCERR)
		strlcat(msg, "vcrcerr ", len);
	if (err & INFINIPATH_RHF_H_PARITYERR)
		strlcat(msg, "parityerr ", len);
	if (err & INFINIPATH_RHF_H_LENERR)
		strlcat(msg, "lenerr ", len);
	if (err & INFINIPATH_RHF_H_MTUERR)
		strlcat(msg, "mtuerr ", len);
	if (err & INFINIPATH_RHF_H_IHDRERR)
		
		strlcat(msg, "ipathhdrerr ", len);
	if (err & INFINIPATH_RHF_H_TIDERR)
		strlcat(msg, "tiderr ", len);
	if (err & INFINIPATH_RHF_H_MKERR)
		
		strlcat(msg, "invalid ipathhdr ", len);
	if (err & INFINIPATH_RHF_H_IBERR)
		strlcat(msg, "iberr ", len);
	if (err & INFINIPATH_RHF_L_SWA)
		strlcat(msg, "swA ", len);
	if (err & INFINIPATH_RHF_L_SWB)
		strlcat(msg, "swB ", len);
}

static inline void *ipath_get_egrbuf(struct ipath_devdata *dd, u32 bufnum)
{
	return dd->ipath_port0_skbinfo ?
		(void *) dd->ipath_port0_skbinfo[bufnum].skb->data : NULL;
}

struct sk_buff *ipath_alloc_skb(struct ipath_devdata *dd,
				gfp_t gfp_mask)
{
	struct sk_buff *skb;
	u32 len;


	len = dd->ipath_ibmaxlen + 4;

	if (dd->ipath_flags & IPATH_4BYTE_TID) {
		len += 2047;
	}

	skb = __dev_alloc_skb(len, gfp_mask);
	if (!skb) {
		ipath_dev_err(dd, "Failed to allocate skbuff, length %u\n",
			      len);
		goto bail;
	}

	skb_reserve(skb, 4);

	if (dd->ipath_flags & IPATH_4BYTE_TID) {
		u32 una = (unsigned long)skb->data & 2047;
		if (una)
			skb_reserve(skb, 2048 - una);
	}

bail:
	return skb;
}

static void ipath_rcv_hdrerr(struct ipath_devdata *dd,
			     u32 eflags,
			     u32 l,
			     u32 etail,
			     __le32 *rhf_addr,
			     struct ipath_message_header *hdr)
{
	char emsg[128];

	get_rhf_errstring(eflags, emsg, sizeof emsg);
	ipath_cdbg(PKT, "RHFerrs %x hdrqtail=%x typ=%u "
		   "tlen=%x opcode=%x egridx=%x: %s\n",
		   eflags, l,
		   ipath_hdrget_rcv_type(rhf_addr),
		   ipath_hdrget_length_in_bytes(rhf_addr),
		   be32_to_cpu(hdr->bth[0]) >> 24,
		   etail, emsg);

	
	if (eflags & (INFINIPATH_RHF_H_ICRCERR | INFINIPATH_RHF_H_VCRCERR)) {
		u8 n = (dd->ipath_ibcctrl >>
			INFINIPATH_IBCC_PHYERRTHRESHOLD_SHIFT) &
			INFINIPATH_IBCC_PHYERRTHRESHOLD_MASK;

		if (++dd->ipath_lli_counter > n) {
			dd->ipath_lli_counter = 0;
			dd->ipath_lli_errors++;
		}
	}
}

void ipath_kreceive(struct ipath_portdata *pd)
{
	struct ipath_devdata *dd = pd->port_dd;
	__le32 *rhf_addr;
	void *ebuf;
	const u32 rsize = dd->ipath_rcvhdrentsize;	
	const u32 maxcnt = dd->ipath_rcvhdrcnt * rsize;	
	u32 etail = -1, l, hdrqtail;
	struct ipath_message_header *hdr;
	u32 eflags, i, etype, tlen, pkttot = 0, updegr = 0, reloop = 0;
	static u64 totcalls;	
	int last;

	l = pd->port_head;
	rhf_addr = (__le32 *) pd->port_rcvhdrq + l + dd->ipath_rhf_offset;
	if (dd->ipath_flags & IPATH_NODMA_RTAIL) {
		u32 seq = ipath_hdrget_seq(rhf_addr);

		if (seq != pd->port_seq_cnt)
			goto bail;
		hdrqtail = 0;
	} else {
		hdrqtail = ipath_get_rcvhdrtail(pd);
		if (l == hdrqtail)
			goto bail;
		smp_rmb();
	}

reloop:
	for (last = 0, i = 1; !last; i += !last) {
		hdr = dd->ipath_f_get_msgheader(dd, rhf_addr);
		eflags = ipath_hdrget_err_flags(rhf_addr);
		etype = ipath_hdrget_rcv_type(rhf_addr);
		
		tlen = ipath_hdrget_length_in_bytes(rhf_addr);
		ebuf = NULL;
		if ((dd->ipath_flags & IPATH_NODMA_RTAIL) ?
		    ipath_hdrget_use_egr_buf(rhf_addr) :
		    (etype != RCVHQ_RCV_TYPE_EXPECTED)) {
			etail = ipath_hdrget_index(rhf_addr);
			updegr = 1;
			if (tlen > sizeof(*hdr) ||
			    etype == RCVHQ_RCV_TYPE_NON_KD)
				ebuf = ipath_get_egrbuf(dd, etail);
		}


		if (etype != RCVHQ_RCV_TYPE_NON_KD &&
		    etype != RCVHQ_RCV_TYPE_ERROR &&
		    ipath_hdrget_ipath_ver(hdr->iph.ver_port_tid_offset) !=
		    IPS_PROTO_VERSION)
			ipath_cdbg(PKT, "Bad InfiniPath protocol version "
				   "%x\n", etype);

		if (unlikely(eflags))
			ipath_rcv_hdrerr(dd, eflags, l, etail, rhf_addr, hdr);
		else if (etype == RCVHQ_RCV_TYPE_NON_KD) {
			ipath_ib_rcv(dd->verbs_dev, (u32 *)hdr, ebuf, tlen);
			if (dd->ipath_lli_counter)
				dd->ipath_lli_counter--;
		} else if (etype == RCVHQ_RCV_TYPE_EAGER) {
			u8 opcode = be32_to_cpu(hdr->bth[0]) >> 24;
			u32 qp = be32_to_cpu(hdr->bth[1]) & 0xffffff;
			ipath_cdbg(PKT, "typ %x, opcode %x (eager, "
				   "qp=%x), len %x; ignored\n",
				   etype, opcode, qp, tlen);
		}
		else if (etype == RCVHQ_RCV_TYPE_EXPECTED)
			ipath_dbg("Bug: Expected TID, opcode %x; ignored\n",
				  be32_to_cpu(hdr->bth[0]) >> 24);
		else {
			ipath_cdbg(ERRPKT, "Error Pkt, but no eflags! egrbuf"
				  " %x, len %x hdrq+%x rhf: %Lx\n",
				  etail, tlen, l, (unsigned long long)
				  le64_to_cpu(*(__le64 *) rhf_addr));
			if (ipath_debug & __IPATH_ERRPKTDBG) {
				u32 j, *d, dw = rsize-2;
				if (rsize > (tlen>>2))
					dw = tlen>>2;
				d = (u32 *)hdr;
				printk(KERN_DEBUG "EPkt rcvhdr(%x dw):\n",
					dw);
				for (j = 0; j < dw; j++)
					printk(KERN_DEBUG "%8x%s", d[j],
						(j%8) == 7 ? "\n" : " ");
				printk(KERN_DEBUG ".\n");
			}
		}
		l += rsize;
		if (l >= maxcnt)
			l = 0;
		rhf_addr = (__le32 *) pd->port_rcvhdrq +
			l + dd->ipath_rhf_offset;
		if (dd->ipath_flags & IPATH_NODMA_RTAIL) {
			u32 seq = ipath_hdrget_seq(rhf_addr);

			if (++pd->port_seq_cnt > 13)
				pd->port_seq_cnt = 1;
			if (seq != pd->port_seq_cnt)
				last = 1;
		} else if (l == hdrqtail)
			last = 1;
		if (last || !(i & 0xf)) {
			u64 lval = l;

			
			if (last)
				lval |= dd->ipath_rhdrhead_intr_off;
			ipath_write_ureg(dd, ur_rcvhdrhead, lval,
				pd->port_port);
			if (updegr) {
				ipath_write_ureg(dd, ur_rcvegrindexhead,
						 etail, pd->port_port);
				updegr = 0;
			}
		}
	}

	if (!dd->ipath_rhdrhead_intr_off && !reloop &&
	    !(dd->ipath_flags & IPATH_NODMA_RTAIL)) {
		u32 hqtail = ipath_get_rcvhdrtail(pd);
		if (hqtail != hdrqtail) {
			hdrqtail = hqtail;
			reloop = 1; 
			goto reloop;
		}
	}

	pkttot += i;

	pd->port_head = l;

	if (pkttot > ipath_stats.sps_maxpkts_call)
		ipath_stats.sps_maxpkts_call = pkttot;
	ipath_stats.sps_port0pkts += pkttot;
	ipath_stats.sps_avgpkts_call =
		ipath_stats.sps_port0pkts / ++totcalls;

bail:;
}


static void ipath_update_pio_bufs(struct ipath_devdata *dd)
{
	unsigned long flags;
	int i;
	const unsigned piobregs = (unsigned)dd->ipath_pioavregs;

	if (!dd->ipath_pioavailregs_dma) {
		ipath_dbg("Update shadow pioavail, but regs_dma NULL!\n");
		return;
	}
	if (ipath_debug & __IPATH_VERBDBG) {
		
		volatile __le64 *dma = dd->ipath_pioavailregs_dma;
		unsigned long *shadow = dd->ipath_pioavailshadow;

		ipath_cdbg(PKT, "Refill avail, dma0=%llx shad0=%lx, "
			   "d1=%llx s1=%lx, d2=%llx s2=%lx, d3=%llx "
			   "s3=%lx\n",
			   (unsigned long long) le64_to_cpu(dma[0]),
			   shadow[0],
			   (unsigned long long) le64_to_cpu(dma[1]),
			   shadow[1],
			   (unsigned long long) le64_to_cpu(dma[2]),
			   shadow[2],
			   (unsigned long long) le64_to_cpu(dma[3]),
			   shadow[3]);
		if (piobregs > 4)
			ipath_cdbg(
				PKT, "2nd group, dma4=%llx shad4=%lx, "
				"d5=%llx s5=%lx, d6=%llx s6=%lx, "
				"d7=%llx s7=%lx\n",
				(unsigned long long) le64_to_cpu(dma[4]),
				shadow[4],
				(unsigned long long) le64_to_cpu(dma[5]),
				shadow[5],
				(unsigned long long) le64_to_cpu(dma[6]),
				shadow[6],
				(unsigned long long) le64_to_cpu(dma[7]),
				shadow[7]);
	}
	spin_lock_irqsave(&ipath_pioavail_lock, flags);
	for (i = 0; i < piobregs; i++) {
		u64 pchbusy, pchg, piov, pnew;
		if (i > 3 && (dd->ipath_flags & IPATH_SWAP_PIOBUFS))
			piov = le64_to_cpu(dd->ipath_pioavailregs_dma[i ^ 1]);
		else
			piov = le64_to_cpu(dd->ipath_pioavailregs_dma[i]);
		pchg = dd->ipath_pioavailkernel[i] &
			~(dd->ipath_pioavailshadow[i] ^ piov);
		pchbusy = pchg << INFINIPATH_SENDPIOAVAIL_BUSY_SHIFT;
		if (pchg && (pchbusy & dd->ipath_pioavailshadow[i])) {
			pnew = dd->ipath_pioavailshadow[i] & ~pchbusy;
			pnew |= piov & pchbusy;
			dd->ipath_pioavailshadow[i] = pnew;
		}
	}
	spin_unlock_irqrestore(&ipath_pioavail_lock, flags);
}

static void ipath_reset_availshadow(struct ipath_devdata *dd)
{
	int i, im;
	unsigned long flags;

	spin_lock_irqsave(&ipath_pioavail_lock, flags);
	for (i = 0; i < dd->ipath_pioavregs; i++) {
		u64 val, oldval;
		
		im = (i > 3 && (dd->ipath_flags & IPATH_SWAP_PIOBUFS)) ?
			i ^ 1 : i;
		val = le64_to_cpu(dd->ipath_pioavailregs_dma[im]);
		oldval = dd->ipath_pioavailshadow[i];
		dd->ipath_pioavailshadow[i] = val |
			((~dd->ipath_pioavailkernel[i] <<
			INFINIPATH_SENDPIOAVAIL_BUSY_SHIFT) &
			0xaaaaaaaaaaaaaaaaULL); 
		if (oldval != dd->ipath_pioavailshadow[i])
			ipath_dbg("shadow[%d] was %Lx, now %lx\n",
				i, (unsigned long long) oldval,
				dd->ipath_pioavailshadow[i]);
	}
	spin_unlock_irqrestore(&ipath_pioavail_lock, flags);
}

int ipath_setrcvhdrsize(struct ipath_devdata *dd, unsigned rhdrsize)
{
	int ret = 0;

	if (dd->ipath_flags & IPATH_RCVHDRSZ_SET) {
		if (dd->ipath_rcvhdrsize != rhdrsize) {
			dev_info(&dd->pcidev->dev,
				 "Error: can't set protocol header "
				 "size %u, already %u\n",
				 rhdrsize, dd->ipath_rcvhdrsize);
			ret = -EAGAIN;
		} else
			ipath_cdbg(VERBOSE, "Reuse same protocol header "
				   "size %u\n", dd->ipath_rcvhdrsize);
	} else if (rhdrsize > (dd->ipath_rcvhdrentsize -
			       (sizeof(u64) / sizeof(u32)))) {
		ipath_dbg("Error: can't set protocol header size %u "
			  "(> max %u)\n", rhdrsize,
			  dd->ipath_rcvhdrentsize -
			  (u32) (sizeof(u64) / sizeof(u32)));
		ret = -EOVERFLOW;
	} else {
		dd->ipath_flags |= IPATH_RCVHDRSZ_SET;
		dd->ipath_rcvhdrsize = rhdrsize;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvhdrsize,
				 dd->ipath_rcvhdrsize);
		ipath_cdbg(VERBOSE, "Set protocol header size to %u\n",
			   dd->ipath_rcvhdrsize);
	}
	return ret;
}

static noinline void no_pio_bufs(struct ipath_devdata *dd)
{
	unsigned long *shadow = dd->ipath_pioavailshadow;
	__le64 *dma = (__le64 *)dd->ipath_pioavailregs_dma;

	dd->ipath_upd_pio_shadow = 1;

	ipath_stats.sps_nopiobufs++;
	if (!(++dd->ipath_consec_nopiobuf % 100000)) {
		ipath_force_pio_avail_update(dd); 
		ipath_dbg("%u tries no piobufavail ts%lx; dmacopy: "
			"%llx %llx %llx %llx\n"
			"ipath  shadow:  %lx %lx %lx %lx\n",
			dd->ipath_consec_nopiobuf,
			(unsigned long)get_cycles(),
			(unsigned long long) le64_to_cpu(dma[0]),
			(unsigned long long) le64_to_cpu(dma[1]),
			(unsigned long long) le64_to_cpu(dma[2]),
			(unsigned long long) le64_to_cpu(dma[3]),
			shadow[0], shadow[1], shadow[2], shadow[3]);
		if ((dd->ipath_piobcnt2k + dd->ipath_piobcnt4k) >
		    (sizeof(shadow[0]) * 4 * 4))
			ipath_dbg("2nd group: dmacopy: "
				  "%llx %llx %llx %llx\n"
				  "ipath  shadow:  %lx %lx %lx %lx\n",
				  (unsigned long long)le64_to_cpu(dma[4]),
				  (unsigned long long)le64_to_cpu(dma[5]),
				  (unsigned long long)le64_to_cpu(dma[6]),
				  (unsigned long long)le64_to_cpu(dma[7]),
				  shadow[4], shadow[5], shadow[6], shadow[7]);

		
		ipath_reset_availshadow(dd);
	}
}

static u32 __iomem *ipath_getpiobuf_range(struct ipath_devdata *dd,
	u32 *pbufnum, u32 first, u32 last, u32 firsti)
{
	int i, j, updated = 0;
	unsigned piobcnt;
	unsigned long flags;
	unsigned long *shadow = dd->ipath_pioavailshadow;
	u32 __iomem *buf;

	piobcnt = last - first;
	if (dd->ipath_upd_pio_shadow) {
		ipath_update_pio_bufs(dd);
		updated++;
		i = first;
	} else
		i = firsti;
rescan:
	spin_lock_irqsave(&ipath_pioavail_lock, flags);
	for (j = 0; j < piobcnt; j++, i++) {
		if (i >= last)
			i = first;
		if (__test_and_set_bit((2 * i) + 1, shadow))
			continue;
		
		__change_bit(2 * i, shadow);
		break;
	}
	spin_unlock_irqrestore(&ipath_pioavail_lock, flags);

	if (j == piobcnt) {
		if (!updated) {
			ipath_update_pio_bufs(dd);
			updated++;
			i = first;
			goto rescan;
		} else if (updated == 1 && piobcnt <=
			((dd->ipath_sendctrl
			>> INFINIPATH_S_UPDTHRESH_SHIFT) &
			INFINIPATH_S_UPDTHRESH_MASK)) {
			ipath_force_pio_avail_update(dd);
			ipath_update_pio_bufs(dd);
			updated++;
			i = first;
			goto rescan;
		}

		no_pio_bufs(dd);
		buf = NULL;
	} else {
		if (i < dd->ipath_piobcnt2k)
			buf = (u32 __iomem *) (dd->ipath_pio2kbase +
					       i * dd->ipath_palign);
		else
			buf = (u32 __iomem *)
				(dd->ipath_pio4kbase +
				 (i - dd->ipath_piobcnt2k) * dd->ipath_4kalign);
		if (pbufnum)
			*pbufnum = i;
	}

	return buf;
}

u32 __iomem *ipath_getpiobuf(struct ipath_devdata *dd, u32 plen, u32 *pbufnum)
{
	u32 __iomem *buf;
	u32 pnum, nbufs;
	u32 first, lasti;

	if (plen + 1 >= IPATH_SMALLBUF_DWORDS) {
		first = dd->ipath_piobcnt2k;
		lasti = dd->ipath_lastpioindexl;
	} else {
		first = 0;
		lasti = dd->ipath_lastpioindex;
	}
	nbufs = dd->ipath_piobcnt2k + dd->ipath_piobcnt4k;
	buf = ipath_getpiobuf_range(dd, &pnum, first, nbufs, lasti);

	if (buf) {
		if (plen + 1 >= IPATH_SMALLBUF_DWORDS)
			dd->ipath_lastpioindexl = pnum + 1;
		else
			dd->ipath_lastpioindex = pnum + 1;
		if (dd->ipath_upd_pio_shadow)
			dd->ipath_upd_pio_shadow = 0;
		if (dd->ipath_consec_nopiobuf)
			dd->ipath_consec_nopiobuf = 0;
		ipath_cdbg(VERBOSE, "Return piobuf%u %uk @ %p\n",
			   pnum, (pnum < dd->ipath_piobcnt2k) ? 2 : 4, buf);
		if (pbufnum)
			*pbufnum = pnum;

	}
	return buf;
}

void ipath_chg_pioavailkernel(struct ipath_devdata *dd, unsigned start,
			      unsigned len, int avail)
{
	unsigned long flags;
	unsigned end, cnt = 0;

	
	start *= 2;
	end = start + len * 2;

	spin_lock_irqsave(&ipath_pioavail_lock, flags);
	
	while (start < end) {
		if (avail) {
			unsigned long dma;
			int i, im;
			
			i = start / BITS_PER_LONG;
			im = (i > 3 && (dd->ipath_flags & IPATH_SWAP_PIOBUFS)) ?
				i ^ 1 : i;
			__clear_bit(INFINIPATH_SENDPIOAVAIL_BUSY_SHIFT
				+ start, dd->ipath_pioavailshadow);
			dma = (unsigned long) le64_to_cpu(
				dd->ipath_pioavailregs_dma[im]);
			if (test_bit((INFINIPATH_SENDPIOAVAIL_CHECK_SHIFT
				+ start) % BITS_PER_LONG, &dma))
				__set_bit(INFINIPATH_SENDPIOAVAIL_CHECK_SHIFT
					+ start, dd->ipath_pioavailshadow);
			else
				__clear_bit(INFINIPATH_SENDPIOAVAIL_CHECK_SHIFT
					+ start, dd->ipath_pioavailshadow);
			__set_bit(start, dd->ipath_pioavailkernel);
		} else {
			__set_bit(start + INFINIPATH_SENDPIOAVAIL_BUSY_SHIFT,
				dd->ipath_pioavailshadow);
			__clear_bit(start, dd->ipath_pioavailkernel);
		}
		start += 2;
	}

	if (dd->ipath_pioupd_thresh) {
		end = 2 * (dd->ipath_piobcnt2k + dd->ipath_piobcnt4k);
		cnt = bitmap_weight(dd->ipath_pioavailkernel, end);
	}
	spin_unlock_irqrestore(&ipath_pioavail_lock, flags);

	if (!avail && len < cnt)
		cnt = len;
	if (cnt < dd->ipath_pioupd_thresh) {
		dd->ipath_pioupd_thresh = cnt;
		ipath_dbg("Decreased pio update threshold to %u\n",
			dd->ipath_pioupd_thresh);
		spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
		dd->ipath_sendctrl &= ~(INFINIPATH_S_UPDTHRESH_MASK
			<< INFINIPATH_S_UPDTHRESH_SHIFT);
		dd->ipath_sendctrl |= dd->ipath_pioupd_thresh
			<< INFINIPATH_S_UPDTHRESH_SHIFT;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			dd->ipath_sendctrl);
		spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
	}
}

int ipath_create_rcvhdrq(struct ipath_devdata *dd,
			 struct ipath_portdata *pd)
{
	int ret = 0;

	if (!pd->port_rcvhdrq) {
		dma_addr_t phys_hdrqtail;
		gfp_t gfp_flags = GFP_USER | __GFP_COMP;
		int amt = ALIGN(dd->ipath_rcvhdrcnt * dd->ipath_rcvhdrentsize *
				sizeof(u32), PAGE_SIZE);

		pd->port_rcvhdrq = dma_alloc_coherent(
			&dd->pcidev->dev, amt, &pd->port_rcvhdrq_phys,
			gfp_flags);

		if (!pd->port_rcvhdrq) {
			ipath_dev_err(dd, "attempt to allocate %d bytes "
				      "for port %u rcvhdrq failed\n",
				      amt, pd->port_port);
			ret = -ENOMEM;
			goto bail;
		}

		if (!(dd->ipath_flags & IPATH_NODMA_RTAIL)) {
			pd->port_rcvhdrtail_kvaddr = dma_alloc_coherent(
				&dd->pcidev->dev, PAGE_SIZE, &phys_hdrqtail,
				GFP_KERNEL);
			if (!pd->port_rcvhdrtail_kvaddr) {
				ipath_dev_err(dd, "attempt to allocate 1 page "
					"for port %u rcvhdrqtailaddr "
					"failed\n", pd->port_port);
				ret = -ENOMEM;
				dma_free_coherent(&dd->pcidev->dev, amt,
					pd->port_rcvhdrq,
					pd->port_rcvhdrq_phys);
				pd->port_rcvhdrq = NULL;
				goto bail;
			}
			pd->port_rcvhdrqtailaddr_phys = phys_hdrqtail;
			ipath_cdbg(VERBOSE, "port %d hdrtailaddr, %llx "
				   "physical\n", pd->port_port,
				   (unsigned long long) phys_hdrqtail);
		}

		pd->port_rcvhdrq_size = amt;

		ipath_cdbg(VERBOSE, "%d pages at %p (phys %lx) size=%lu "
			   "for port %u rcvhdr Q\n",
			   amt >> PAGE_SHIFT, pd->port_rcvhdrq,
			   (unsigned long) pd->port_rcvhdrq_phys,
			   (unsigned long) pd->port_rcvhdrq_size,
			   pd->port_port);
	}
	else
		ipath_cdbg(VERBOSE, "reuse port %d rcvhdrq @%p %llx phys; "
			   "hdrtailaddr@%p %llx physical\n",
			   pd->port_port, pd->port_rcvhdrq,
			   (unsigned long long) pd->port_rcvhdrq_phys,
			   pd->port_rcvhdrtail_kvaddr, (unsigned long long)
			   pd->port_rcvhdrqtailaddr_phys);

	
	memset(pd->port_rcvhdrq, 0, pd->port_rcvhdrq_size);
	if (pd->port_rcvhdrtail_kvaddr)
		memset(pd->port_rcvhdrtail_kvaddr, 0, PAGE_SIZE);

	ipath_write_kreg_port(dd, dd->ipath_kregs->kr_rcvhdrtailaddr,
			      pd->port_port, pd->port_rcvhdrqtailaddr_phys);
	ipath_write_kreg_port(dd, dd->ipath_kregs->kr_rcvhdraddr,
			      pd->port_port, pd->port_rcvhdrq_phys);

bail:
	return ret;
}


void ipath_cancel_sends(struct ipath_devdata *dd, int restore_sendctrl)
{
	unsigned long flags;

	if (dd->ipath_flags & IPATH_IB_AUTONEG_INPROG) {
		ipath_cdbg(VERBOSE, "Ignore while in autonegotiation\n");
		goto bail;
	}
	if (dd->ipath_flags & IPATH_HAS_SEND_DMA) {
		int skip_cancel;
		unsigned long *statp = &dd->ipath_sdma_status;

		spin_lock_irqsave(&dd->ipath_sdma_lock, flags);
		skip_cancel =
			test_and_set_bit(IPATH_SDMA_ABORTING, statp)
			&& !test_bit(IPATH_SDMA_DISABLED, statp);
		spin_unlock_irqrestore(&dd->ipath_sdma_lock, flags);
		if (skip_cancel)
			goto bail;
	}

	ipath_dbg("Cancelling all in-progress send buffers\n");

	
	dd->ipath_lastcancel = jiffies + HZ / 2;

	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	dd->ipath_sendctrl &= ~(INFINIPATH_S_PIOBUFAVAILUPD
		| INFINIPATH_S_PIOENABLE);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
		dd->ipath_sendctrl | INFINIPATH_S_ABORT);
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);

	
	ipath_disarm_piobufs(dd, 0,
		dd->ipath_piobcnt2k + dd->ipath_piobcnt4k);

	if (dd->ipath_flags & IPATH_HAS_SEND_DMA)
		set_bit(IPATH_SDMA_DISARMED, &dd->ipath_sdma_status);

	if (restore_sendctrl) {
		
		spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
		dd->ipath_sendctrl |= INFINIPATH_S_PIOBUFAVAILUPD |
			INFINIPATH_S_PIOENABLE;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			dd->ipath_sendctrl);
		
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
		spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
	}

	if ((dd->ipath_flags & IPATH_HAS_SEND_DMA) &&
	    !test_bit(IPATH_SDMA_DISABLED, &dd->ipath_sdma_status) &&
	    test_bit(IPATH_SDMA_RUNNING, &dd->ipath_sdma_status)) {
		spin_lock_irqsave(&dd->ipath_sdma_lock, flags);
		
		dd->ipath_sdma_abort_intr_timeout = jiffies + HZ;
		dd->ipath_sdma_reset_wait = 200;
		if (!test_bit(IPATH_SDMA_SHUTDOWN, &dd->ipath_sdma_status))
			tasklet_hi_schedule(&dd->ipath_sdma_abort_task);
		spin_unlock_irqrestore(&dd->ipath_sdma_lock, flags);
	}
bail:;
}

void ipath_force_pio_avail_update(struct ipath_devdata *dd)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	if (dd->ipath_sendctrl & INFINIPATH_S_PIOBUFAVAILUPD) {
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			dd->ipath_sendctrl & ~INFINIPATH_S_PIOBUFAVAILUPD);
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
		ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl,
			dd->ipath_sendctrl);
		ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	}
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);
}

static void ipath_set_ib_lstate(struct ipath_devdata *dd, int linkcmd,
				int linitcmd)
{
	u64 mod_wd;
	static const char *what[4] = {
		[0] = "NOP",
		[INFINIPATH_IBCC_LINKCMD_DOWN] = "DOWN",
		[INFINIPATH_IBCC_LINKCMD_ARMED] = "ARMED",
		[INFINIPATH_IBCC_LINKCMD_ACTIVE] = "ACTIVE"
	};

	if (linitcmd == INFINIPATH_IBCC_LINKINITCMD_DISABLE) {
		preempt_disable();
		dd->ipath_flags |= IPATH_IB_LINK_DISABLED;
		preempt_enable();
	} else if (linitcmd) {
		preempt_disable();
		dd->ipath_flags &= ~IPATH_IB_LINK_DISABLED;
		preempt_enable();
	}

	mod_wd = (linkcmd << dd->ibcc_lc_shift) |
		(linitcmd << INFINIPATH_IBCC_LINKINITCMD_SHIFT);
	ipath_cdbg(VERBOSE,
		"Moving unit %u to %s (initcmd=0x%x), current ltstate is %s\n",
		dd->ipath_unit, what[linkcmd], linitcmd,
		ipath_ibcstatus_str[ipath_ib_linktrstate(dd,
			ipath_read_kreg64(dd, dd->ipath_kregs->kr_ibcstatus))]);

	ipath_write_kreg(dd, dd->ipath_kregs->kr_ibcctrl,
			 dd->ipath_ibcctrl | mod_wd);
	
	(void) ipath_read_kreg64(dd, dd->ipath_kregs->kr_ibcstatus);
}

int ipath_set_linkstate(struct ipath_devdata *dd, u8 newstate)
{
	u32 lstate;
	int ret;

	switch (newstate) {
	case IPATH_IB_LINKDOWN_ONLY:
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_DOWN, 0);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINKDOWN:
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_DOWN,
					INFINIPATH_IBCC_LINKINITCMD_POLL);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINKDOWN_SLEEP:
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_DOWN,
					INFINIPATH_IBCC_LINKINITCMD_SLEEP);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINKDOWN_DISABLE:
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_DOWN,
					INFINIPATH_IBCC_LINKINITCMD_DISABLE);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINKARM:
		if (dd->ipath_flags & IPATH_LINKARMED) {
			ret = 0;
			goto bail;
		}
		if (!(dd->ipath_flags &
		      (IPATH_LINKINIT | IPATH_LINKACTIVE))) {
			ret = -EINVAL;
			goto bail;
		}
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_ARMED, 0);

		lstate = IPATH_LINKARMED | IPATH_LINKACTIVE;
		break;

	case IPATH_IB_LINKACTIVE:
		if (dd->ipath_flags & IPATH_LINKACTIVE) {
			ret = 0;
			goto bail;
		}
		if (!(dd->ipath_flags & IPATH_LINKARMED)) {
			ret = -EINVAL;
			goto bail;
		}
		ipath_set_ib_lstate(dd, INFINIPATH_IBCC_LINKCMD_ACTIVE, 0);
		lstate = IPATH_LINKACTIVE;
		break;

	case IPATH_IB_LINK_LOOPBACK:
		dev_info(&dd->pcidev->dev, "Enabling IB local loopback\n");
		dd->ipath_ibcctrl |= INFINIPATH_IBCC_LOOPBACK;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_ibcctrl,
				 dd->ipath_ibcctrl);

		
		dd->ipath_f_set_ib_cfg(dd, IPATH_IB_CFG_HRTBT,
				       IPATH_IB_HRTBT_OFF);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINK_EXTERNAL:
		dev_info(&dd->pcidev->dev,
			"Disabling IB local loopback (normal)\n");
		dd->ipath_f_set_ib_cfg(dd, IPATH_IB_CFG_HRTBT,
				       IPATH_IB_HRTBT_ON);
		dd->ipath_ibcctrl &= ~INFINIPATH_IBCC_LOOPBACK;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_ibcctrl,
				 dd->ipath_ibcctrl);
		
		ret = 0;
		goto bail;

	case IPATH_IB_LINK_HRTBT:
		ret = dd->ipath_f_set_ib_cfg(dd, IPATH_IB_CFG_HRTBT,
			IPATH_IB_HRTBT_ON);
		goto bail;

	case IPATH_IB_LINK_NO_HRTBT:
		ret = dd->ipath_f_set_ib_cfg(dd, IPATH_IB_CFG_HRTBT,
			IPATH_IB_HRTBT_OFF);
		goto bail;

	default:
		ipath_dbg("Invalid linkstate 0x%x requested\n", newstate);
		ret = -EINVAL;
		goto bail;
	}
	ret = ipath_wait_linkstate(dd, lstate, 2000);

bail:
	return ret;
}

int ipath_set_mtu(struct ipath_devdata *dd, u16 arg)
{
	u32 piosize;
	int changed = 0;
	int ret;

	if (arg != 256 && arg != 512 && arg != 1024 && arg != 2048 &&
	    (arg != 4096 || !ipath_mtu4096)) {
		ipath_dbg("Trying to set invalid mtu %u, failing\n", arg);
		ret = -EINVAL;
		goto bail;
	}
	if (dd->ipath_ibmtu == arg) {
		ret = 0;        
		goto bail;
	}

	piosize = dd->ipath_ibmaxlen;
	dd->ipath_ibmtu = arg;

	if (arg >= (piosize - IPATH_PIO_MAXIBHDR)) {
		
		if (piosize != dd->ipath_init_ibmaxlen) {
			if (arg > piosize && arg <= dd->ipath_init_ibmaxlen)
				piosize = dd->ipath_init_ibmaxlen;
			dd->ipath_ibmaxlen = piosize;
			changed = 1;
		}
	} else if ((arg + IPATH_PIO_MAXIBHDR) != dd->ipath_ibmaxlen) {
		piosize = arg + IPATH_PIO_MAXIBHDR;
		ipath_cdbg(VERBOSE, "ibmaxlen was 0x%x, setting to 0x%x "
			   "(mtu 0x%x)\n", dd->ipath_ibmaxlen, piosize,
			   arg);
		dd->ipath_ibmaxlen = piosize;
		changed = 1;
	}

	if (changed) {
		u64 ibc = dd->ipath_ibcctrl, ibdw;
		dd->ipath_ibmaxlen = piosize - 2 * sizeof(u32);
		ibdw = (dd->ipath_ibmaxlen >> 2) + 1;
		ibc &= ~(INFINIPATH_IBCC_MAXPKTLEN_MASK <<
			 dd->ibcc_mpl_shift);
		ibc |= ibdw << dd->ibcc_mpl_shift;
		dd->ipath_ibcctrl = ibc;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_ibcctrl,
				 dd->ipath_ibcctrl);
		dd->ipath_f_tidtemplate(dd);
	}

	ret = 0;

bail:
	return ret;
}

int ipath_set_lid(struct ipath_devdata *dd, u32 lid, u8 lmc)
{
	dd->ipath_lid = lid;
	dd->ipath_lmc = lmc;

	dd->ipath_f_set_ib_cfg(dd, IPATH_IB_CFG_LIDLMC, lid |
		(~((1U << lmc) - 1)) << 16);

	dev_info(&dd->pcidev->dev, "We got a lid: 0x%x\n", lid);

	return 0;
}


void ipath_write_kreg_port(const struct ipath_devdata *dd, ipath_kreg regno,
			  unsigned port, u64 value)
{
	u16 where;

	if (port < dd->ipath_portcnt &&
	    (regno == dd->ipath_kregs->kr_rcvhdraddr ||
	     regno == dd->ipath_kregs->kr_rcvhdrtailaddr))
		where = regno + port;
	else
		where = -1;

	ipath_write_kreg(dd, where, value);
}

#define LED_OVER_FREQ_SHIFT 8
#define LED_OVER_FREQ_MASK (0xFF<<LED_OVER_FREQ_SHIFT)
#define LED_OVER_BOTH_OFF (8)

static void ipath_run_led_override(unsigned long opaque)
{
	struct ipath_devdata *dd = (struct ipath_devdata *)opaque;
	int timeoff;
	int pidx;
	u64 lstate, ltstate, val;

	if (!(dd->ipath_flags & IPATH_INITTED))
		return;

	pidx = dd->ipath_led_override_phase++ & 1;
	dd->ipath_led_override = dd->ipath_led_override_vals[pidx];
	timeoff = dd->ipath_led_override_timeoff;

	val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_ibcstatus);
	ltstate = ipath_ib_linktrstate(dd, val);
	lstate = ipath_ib_linkstate(dd, val);

	dd->ipath_f_setextled(dd, lstate, ltstate);
	mod_timer(&dd->ipath_led_override_timer, jiffies + timeoff);
}

void ipath_set_led_override(struct ipath_devdata *dd, unsigned int val)
{
	int timeoff, freq;

	if (!(dd->ipath_flags & IPATH_INITTED))
		return;

	
	timeoff = HZ;
	freq = (val & LED_OVER_FREQ_MASK) >> LED_OVER_FREQ_SHIFT;

	if (freq) {
		
		dd->ipath_led_override_vals[0] = val & 0xF;
		dd->ipath_led_override_vals[1] = (val >> 4) & 0xF;
		timeoff = (HZ << 4)/freq;
	} else {
		
		dd->ipath_led_override_vals[0] = val & 0xF;
		dd->ipath_led_override_vals[1] = val & 0xF;
	}
	dd->ipath_led_override_timeoff = timeoff;

	if (atomic_inc_return(&dd->ipath_led_override_timer_active) == 1) {
		
		init_timer(&dd->ipath_led_override_timer);
		dd->ipath_led_override_timer.function =
						 ipath_run_led_override;
		dd->ipath_led_override_timer.data = (unsigned long) dd;
		dd->ipath_led_override_timer.expires = jiffies + 1;
		add_timer(&dd->ipath_led_override_timer);
	} else
		atomic_dec(&dd->ipath_led_override_timer_active);
}

void ipath_shutdown_device(struct ipath_devdata *dd)
{
	unsigned long flags;

	ipath_dbg("Shutting down the device\n");

	ipath_hol_up(dd); 

	dd->ipath_flags |= IPATH_LINKUNK;
	dd->ipath_flags &= ~(IPATH_INITTED | IPATH_LINKDOWN |
			     IPATH_LINKINIT | IPATH_LINKARMED |
			     IPATH_LINKACTIVE);
	*dd->ipath_statusp &= ~(IPATH_STATUS_IB_CONF |
				IPATH_STATUS_IB_READY);

	
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask, 0ULL);

	dd->ipath_rcvctrl = 0;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_rcvctrl,
			 dd->ipath_rcvctrl);

	if (dd->ipath_flags & IPATH_HAS_SEND_DMA)
		teardown_sdma(dd);

	spin_lock_irqsave(&dd->ipath_sendctrl_lock, flags);
	dd->ipath_sendctrl = 0;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_sendctrl, dd->ipath_sendctrl);
	
	ipath_read_kreg64(dd, dd->ipath_kregs->kr_scratch);
	spin_unlock_irqrestore(&dd->ipath_sendctrl_lock, flags);

	udelay(5);

	dd->ipath_f_setextled(dd, 0, 0); 

	ipath_set_ib_lstate(dd, 0, INFINIPATH_IBCC_LINKINITCMD_DISABLE);
	ipath_cancel_sends(dd, 0);

	signal_ib_event(dd, IB_EVENT_PORT_ERR);

	
	dd->ipath_control &= ~INFINIPATH_C_LINKENABLE;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_control,
			 dd->ipath_control | INFINIPATH_C_FREEZEMODE);

	dd->ipath_f_quiet_serdes(dd);

	
	del_timer_sync(&dd->ipath_hol_timer);
	if (dd->ipath_stats_timer_active) {
		del_timer_sync(&dd->ipath_stats_timer);
		dd->ipath_stats_timer_active = 0;
	}
	if (dd->ipath_intrchk_timer.data) {
		del_timer_sync(&dd->ipath_intrchk_timer);
		dd->ipath_intrchk_timer.data = 0;
	}
	if (atomic_read(&dd->ipath_led_override_timer_active)) {
		del_timer_sync(&dd->ipath_led_override_timer);
		atomic_set(&dd->ipath_led_override_timer_active, 0);
	}

	ipath_write_kreg(dd, dd->ipath_kregs->kr_hwerrclear,
			 ~0ULL & ~INFINIPATH_HWE_MEMBISTFAILED);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear, -1LL);
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intclear, -1LL);

	ipath_cdbg(VERBOSE, "Flush time and errors to EEPROM\n");
	ipath_update_eeprom_log(dd);
}

void ipath_free_pddata(struct ipath_devdata *dd, struct ipath_portdata *pd)
{
	if (!pd)
		return;

	if (pd->port_rcvhdrq) {
		ipath_cdbg(VERBOSE, "free closed port %d rcvhdrq @ %p "
			   "(size=%lu)\n", pd->port_port, pd->port_rcvhdrq,
			   (unsigned long) pd->port_rcvhdrq_size);
		dma_free_coherent(&dd->pcidev->dev, pd->port_rcvhdrq_size,
				  pd->port_rcvhdrq, pd->port_rcvhdrq_phys);
		pd->port_rcvhdrq = NULL;
		if (pd->port_rcvhdrtail_kvaddr) {
			dma_free_coherent(&dd->pcidev->dev, PAGE_SIZE,
					 pd->port_rcvhdrtail_kvaddr,
					 pd->port_rcvhdrqtailaddr_phys);
			pd->port_rcvhdrtail_kvaddr = NULL;
		}
	}
	if (pd->port_port && pd->port_rcvegrbuf) {
		unsigned e;

		for (e = 0; e < pd->port_rcvegrbuf_chunks; e++) {
			void *base = pd->port_rcvegrbuf[e];
			size_t size = pd->port_rcvegrbuf_size;

			ipath_cdbg(VERBOSE, "egrbuf free(%p, %lu), "
				   "chunk %u/%u\n", base,
				   (unsigned long) size,
				   e, pd->port_rcvegrbuf_chunks);
			dma_free_coherent(&dd->pcidev->dev, size,
				base, pd->port_rcvegrbuf_phys[e]);
		}
		kfree(pd->port_rcvegrbuf);
		pd->port_rcvegrbuf = NULL;
		kfree(pd->port_rcvegrbuf_phys);
		pd->port_rcvegrbuf_phys = NULL;
		pd->port_rcvegrbuf_chunks = 0;
	} else if (pd->port_port == 0 && dd->ipath_port0_skbinfo) {
		unsigned e;
		struct ipath_skbinfo *skbinfo = dd->ipath_port0_skbinfo;

		dd->ipath_port0_skbinfo = NULL;
		ipath_cdbg(VERBOSE, "free closed port %d "
			   "ipath_port0_skbinfo @ %p\n", pd->port_port,
			   skbinfo);
		for (e = 0; e < dd->ipath_p0_rcvegrcnt; e++)
			if (skbinfo[e].skb) {
				pci_unmap_single(dd->pcidev, skbinfo[e].phys,
						 dd->ipath_ibmaxlen,
						 PCI_DMA_FROMDEVICE);
				dev_kfree_skb(skbinfo[e].skb);
			}
		vfree(skbinfo);
	}
	kfree(pd->port_tid_pg_list);
	vfree(pd->subport_uregbase);
	vfree(pd->subport_rcvegrbuf);
	vfree(pd->subport_rcvhdr_base);
	kfree(pd);
}

static int __init infinipath_init(void)
{
	int ret;

	if (ipath_debug & __IPATH_DBG)
		printk(KERN_INFO DRIVER_LOAD_MSG "%s", ib_ipath_version);

	idr_init(&unit_table);
	if (!idr_pre_get(&unit_table, GFP_KERNEL)) {
		printk(KERN_ERR IPATH_DRV_NAME ": idr_pre_get() failed\n");
		ret = -ENOMEM;
		goto bail;
	}

	ret = pci_register_driver(&ipath_driver);
	if (ret < 0) {
		printk(KERN_ERR IPATH_DRV_NAME
		       ": Unable to register driver: error %d\n", -ret);
		goto bail_unit;
	}

	ret = ipath_init_ipathfs();
	if (ret < 0) {
		printk(KERN_ERR IPATH_DRV_NAME ": Unable to create "
		       "ipathfs: error %d\n", -ret);
		goto bail_pci;
	}

	goto bail;

bail_pci:
	pci_unregister_driver(&ipath_driver);

bail_unit:
	idr_destroy(&unit_table);

bail:
	return ret;
}

static void __exit infinipath_cleanup(void)
{
	ipath_exit_ipathfs();

	ipath_cdbg(VERBOSE, "Unregistering pci driver\n");
	pci_unregister_driver(&ipath_driver);

	idr_destroy(&unit_table);
}

int ipath_reset_device(int unit)
{
	int ret, i;
	struct ipath_devdata *dd = ipath_lookup(unit);
	unsigned long flags;

	if (!dd) {
		ret = -ENODEV;
		goto bail;
	}

	if (atomic_read(&dd->ipath_led_override_timer_active)) {
		
		del_timer_sync(&dd->ipath_led_override_timer);
		atomic_set(&dd->ipath_led_override_timer_active, 0);
	}

	
	dd->ipath_led_override = LED_OVER_BOTH_OFF;
	dd->ipath_f_setextled(dd, 0, 0);

	dev_info(&dd->pcidev->dev, "Reset on unit %u requested\n", unit);

	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT)) {
		dev_info(&dd->pcidev->dev, "Invalid unit number %u or "
			 "not initialized or not present\n", unit);
		ret = -ENXIO;
		goto bail;
	}

	spin_lock_irqsave(&dd->ipath_uctxt_lock, flags);
	if (dd->ipath_pd)
		for (i = 1; i < dd->ipath_cfgports; i++) {
			if (!dd->ipath_pd[i] || !dd->ipath_pd[i]->port_cnt)
				continue;
			spin_unlock_irqrestore(&dd->ipath_uctxt_lock, flags);
			ipath_dbg("unit %u port %d is in use "
				  "(PID %u cmd %s), can't reset\n",
				  unit, i,
				  pid_nr(dd->ipath_pd[i]->port_pid),
				  dd->ipath_pd[i]->port_comm);
			ret = -EBUSY;
			goto bail;
		}
	spin_unlock_irqrestore(&dd->ipath_uctxt_lock, flags);

	if (dd->ipath_flags & IPATH_HAS_SEND_DMA)
		teardown_sdma(dd);

	dd->ipath_flags &= ~IPATH_INITTED;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_intmask, 0ULL);
	ret = dd->ipath_f_reset(dd);
	if (ret == 1) {
		ipath_dbg("Reinitializing unit %u after reset attempt\n",
			  unit);
		ret = ipath_init_chip(dd, 1);
	} else
		ret = -EAGAIN;
	if (ret)
		ipath_dev_err(dd, "Reinitialize unit %u after "
			      "reset failed with %d\n", unit, ret);
	else
		dev_info(&dd->pcidev->dev, "Reinitialized unit %u after "
			 "resetting\n", unit);

bail:
	return ret;
}

static int ipath_signal_procs(struct ipath_devdata *dd, int sig)
{
	int i, sub, any = 0;
	struct pid *pid;
	unsigned long flags;

	if (!dd->ipath_pd)
		return 0;

	spin_lock_irqsave(&dd->ipath_uctxt_lock, flags);
	for (i = 1; i < dd->ipath_cfgports; i++) {
		if (!dd->ipath_pd[i] || !dd->ipath_pd[i]->port_cnt)
			continue;
		pid = dd->ipath_pd[i]->port_pid;
		if (!pid)
			continue;

		dev_info(&dd->pcidev->dev, "context %d in use "
			  "(PID %u), sending signal %d\n",
			  i, pid_nr(pid), sig);
		kill_pid(pid, sig, 1);
		any++;
		for (sub = 0; sub < INFINIPATH_MAX_SUBPORT; sub++) {
			pid = dd->ipath_pd[i]->port_subpid[sub];
			if (!pid)
				continue;
			dev_info(&dd->pcidev->dev, "sub-context "
				"%d:%d in use (PID %u), sending "
				"signal %d\n", i, sub, pid_nr(pid), sig);
			kill_pid(pid, sig, 1);
			any++;
		}
	}
	spin_unlock_irqrestore(&dd->ipath_uctxt_lock, flags);
	return any;
}

static void ipath_hol_signal_down(struct ipath_devdata *dd)
{
	if (ipath_signal_procs(dd, SIGSTOP))
		ipath_dbg("Stopped some processes\n");
	ipath_cancel_sends(dd, 1);
}


static void ipath_hol_signal_up(struct ipath_devdata *dd)
{
	if (ipath_signal_procs(dd, SIGCONT))
		ipath_dbg("Continued some processes\n");
}

void ipath_hol_down(struct ipath_devdata *dd)
{
	dd->ipath_hol_state = IPATH_HOL_DOWN;
	ipath_hol_signal_down(dd);
	dd->ipath_hol_next = IPATH_HOL_DOWNCONT;
	dd->ipath_hol_timer.expires = jiffies +
		msecs_to_jiffies(ipath_hol_timeout_ms);
	mod_timer(&dd->ipath_hol_timer, dd->ipath_hol_timer.expires);
}

void ipath_hol_up(struct ipath_devdata *dd)
{
	ipath_hol_signal_up(dd);
	dd->ipath_hol_state = IPATH_HOL_UP;
}

void ipath_hol_event(unsigned long opaque)
{
	struct ipath_devdata *dd = (struct ipath_devdata *)opaque;

	if (dd->ipath_hol_next == IPATH_HOL_DOWNSTOP
		&& dd->ipath_hol_state != IPATH_HOL_UP) {
		dd->ipath_hol_next = IPATH_HOL_DOWNCONT;
		ipath_dbg("Stopping processes\n");
		ipath_hol_signal_down(dd);
	} else { 
		dd->ipath_hol_next = IPATH_HOL_DOWNSTOP;
		ipath_dbg("Continuing processes\n");
		ipath_hol_signal_up(dd);
	}
	if (dd->ipath_hol_state == IPATH_HOL_UP)
		ipath_dbg("link's up, don't resched timer\n");
	else {
		dd->ipath_hol_timer.expires = jiffies +
			msecs_to_jiffies(ipath_hol_timeout_ms);
		mod_timer(&dd->ipath_hol_timer,
			dd->ipath_hol_timer.expires);
	}
}

int ipath_set_rx_pol_inv(struct ipath_devdata *dd, u8 new_pol_inv)
{
	u64 val;

	if (new_pol_inv > INFINIPATH_XGXS_RX_POL_MASK)
		return -1;
	if (dd->ipath_rx_pol_inv != new_pol_inv) {
		dd->ipath_rx_pol_inv = new_pol_inv;
		val = ipath_read_kreg64(dd, dd->ipath_kregs->kr_xgxsconfig);
		val &= ~(INFINIPATH_XGXS_RX_POL_MASK <<
			 INFINIPATH_XGXS_RX_POL_SHIFT);
		val |= ((u64)dd->ipath_rx_pol_inv) <<
			INFINIPATH_XGXS_RX_POL_SHIFT;
		ipath_write_kreg(dd, dd->ipath_kregs->kr_xgxsconfig, val);
	}
	return 0;
}

void ipath_enable_armlaunch(struct ipath_devdata *dd)
{
	dd->ipath_lasterror &= ~INFINIPATH_E_SPIOARMLAUNCH;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errorclear,
		INFINIPATH_E_SPIOARMLAUNCH);
	dd->ipath_errormask |= INFINIPATH_E_SPIOARMLAUNCH;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask,
		dd->ipath_errormask);
}

void ipath_disable_armlaunch(struct ipath_devdata *dd)
{
	
	dd->ipath_maskederrs &= ~INFINIPATH_E_SPIOARMLAUNCH;
	dd->ipath_errormask &= ~INFINIPATH_E_SPIOARMLAUNCH;
	ipath_write_kreg(dd, dd->ipath_kregs->kr_errormask,
		dd->ipath_errormask);
}

module_init(infinipath_init);
module_exit(infinipath_cleanup);
