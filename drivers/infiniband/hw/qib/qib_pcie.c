/*
 * Copyright (c) 2008, 2009 QLogic Corporation. All rights reserved.
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
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/aer.h>
#include <linux/module.h>

#include "qib.h"


static int qib_tune_pcie_caps(struct qib_devdata *);
static int qib_tune_pcie_coalesce(struct qib_devdata *);

int qib_pcie_init(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ret;

	ret = pci_enable_device(pdev);
	if (ret) {
		qib_early_err(&pdev->dev, "pci enable failed: error %d\n",
			      -ret);
		goto done;
	}

	ret = pci_request_regions(pdev, QIB_DRV_NAME);
	if (ret) {
		qib_devinfo(pdev, "pci_request_regions fails: err %d\n", -ret);
		goto bail;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (ret) {
			qib_devinfo(pdev, "Unable to set DMA mask: %d\n", ret);
			goto bail;
		}
		ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	} else
		ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (ret) {
		qib_early_err(&pdev->dev,
			      "Unable to set DMA consistent mask: %d\n", ret);
		goto bail;
	}

	pci_set_master(pdev);
	ret = pci_enable_pcie_error_reporting(pdev);
	if (ret) {
		qib_early_err(&pdev->dev,
			      "Unable to enable pcie error reporting: %d\n",
			      ret);
		ret = 0;
	}
	goto done;

bail:
	pci_disable_device(pdev);
	pci_release_regions(pdev);
done:
	return ret;
}

int qib_pcie_ddinit(struct qib_devdata *dd, struct pci_dev *pdev,
		    const struct pci_device_id *ent)
{
	unsigned long len;
	resource_size_t addr;

	dd->pcidev = pdev;
	pci_set_drvdata(pdev, dd);

	addr = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

#if defined(__powerpc__)
	
	dd->kregbase = __ioremap(addr, len, _PAGE_NO_CACHE | _PAGE_WRITETHRU);
#else
	dd->kregbase = ioremap_nocache(addr, len);
#endif

	if (!dd->kregbase)
		return -ENOMEM;

	dd->kregend = (u64 __iomem *)((void __iomem *) dd->kregbase + len);
	dd->physaddr = addr;        

	dd->pcibar0 = addr;
	dd->pcibar1 = addr >> 32;
	dd->deviceid = ent->device; 
	dd->vendorid = ent->vendor;

	return 0;
}

void qib_pcie_ddcleanup(struct qib_devdata *dd)
{
	u64 __iomem *base = (void __iomem *) dd->kregbase;

	dd->kregbase = NULL;
	iounmap(base);
	if (dd->piobase)
		iounmap(dd->piobase);
	if (dd->userbase)
		iounmap(dd->userbase);
	if (dd->piovl15base)
		iounmap(dd->piovl15base);

	pci_disable_device(dd->pcidev);
	pci_release_regions(dd->pcidev);

	pci_set_drvdata(dd->pcidev, NULL);
}

static void qib_msix_setup(struct qib_devdata *dd, int pos, u32 *msixcnt,
			   struct qib_msix_entry *qib_msix_entry)
{
	int ret;
	u32 tabsize = 0;
	u16 msix_flags;
	struct msix_entry *msix_entry;
	int i;

	msix_entry = kmalloc(*msixcnt * sizeof(*msix_entry), GFP_KERNEL);
	if (!msix_entry) {
		ret = -ENOMEM;
		goto do_intx;
	}
	for (i = 0; i < *msixcnt; i++)
		msix_entry[i] = qib_msix_entry[i].msix;

	pci_read_config_word(dd->pcidev, pos + PCI_MSIX_FLAGS, &msix_flags);
	tabsize = 1 + (msix_flags & PCI_MSIX_FLAGS_QSIZE);
	if (tabsize > *msixcnt)
		tabsize = *msixcnt;
	ret = pci_enable_msix(dd->pcidev, msix_entry, tabsize);
	if (ret > 0) {
		tabsize = ret;
		ret = pci_enable_msix(dd->pcidev, msix_entry, tabsize);
	}
do_intx:
	if (ret) {
		qib_dev_err(dd, "pci_enable_msix %d vectors failed: %d, "
			    "falling back to INTx\n", tabsize, ret);
		tabsize = 0;
	}
	for (i = 0; i < tabsize; i++)
		qib_msix_entry[i].msix = msix_entry[i];
	kfree(msix_entry);
	*msixcnt = tabsize;

	if (ret)
		qib_enable_intx(dd->pcidev);

}

static int qib_msi_setup(struct qib_devdata *dd, int pos)
{
	struct pci_dev *pdev = dd->pcidev;
	u16 control;
	int ret;

	ret = pci_enable_msi(pdev);
	if (ret)
		qib_dev_err(dd, "pci_enable_msi failed: %d, "
			    "interrupts may not work\n", ret);
	

	pci_read_config_dword(pdev, pos + PCI_MSI_ADDRESS_LO,
			      &dd->msi_lo);
	pci_read_config_dword(pdev, pos + PCI_MSI_ADDRESS_HI,
			      &dd->msi_hi);
	pci_read_config_word(pdev, pos + PCI_MSI_FLAGS, &control);
	
	pci_read_config_word(pdev, pos + ((control & PCI_MSI_FLAGS_64BIT)
				    ? 12 : 8),
			     &dd->msi_data);
	return ret;
}

int qib_pcie_params(struct qib_devdata *dd, u32 minw, u32 *nent,
		    struct qib_msix_entry *entry)
{
	u16 linkstat, speed;
	int pos = 0, pose, ret = 1;

	pose = pci_pcie_cap(dd->pcidev);
	if (!pose) {
		qib_dev_err(dd, "Can't find PCI Express capability!\n");
		
		dd->lbus_width = 1;
		dd->lbus_speed = 2500; 
		goto bail;
	}

	pos = pci_find_capability(dd->pcidev, PCI_CAP_ID_MSIX);
	if (nent && *nent && pos) {
		qib_msix_setup(dd, pos, nent, entry);
		ret = 0; 
	} else {
		pos = pci_find_capability(dd->pcidev, PCI_CAP_ID_MSI);
		if (pos)
			ret = qib_msi_setup(dd, pos);
		else
			qib_dev_err(dd, "No PCI MSI or MSIx capability!\n");
	}
	if (!pos)
		qib_enable_intx(dd->pcidev);

	pci_read_config_word(dd->pcidev, pose + PCI_EXP_LNKSTA, &linkstat);
	speed = linkstat & 0xf;
	linkstat >>= 4;
	linkstat &= 0x1f;
	dd->lbus_width = linkstat;

	switch (speed) {
	case 1:
		dd->lbus_speed = 2500; 
		break;
	case 2:
		dd->lbus_speed = 5000; 
		break;
	default: 
		dd->lbus_speed = 2500;
		break;
	}

	if (minw && linkstat < minw)
		qib_dev_err(dd,
			    "PCIe width %u (x%u HCA), performance reduced\n",
			    linkstat, minw);

	qib_tune_pcie_caps(dd);

	qib_tune_pcie_coalesce(dd);

bail:
	
	snprintf(dd->lbus_info, sizeof(dd->lbus_info),
		 "PCIe,%uMHz,x%u\n", dd->lbus_speed, dd->lbus_width);
	return ret;
}

int qib_reinit_intr(struct qib_devdata *dd)
{
	int pos;
	u16 control;
	int ret = 0;

	
	if (!dd->msi_lo)
		goto bail;

	pos = pci_find_capability(dd->pcidev, PCI_CAP_ID_MSI);
	if (!pos) {
		qib_dev_err(dd, "Can't find MSI capability, "
			    "can't restore MSI settings\n");
		ret = 0;
		
		goto bail;
	}
	pci_write_config_dword(dd->pcidev, pos + PCI_MSI_ADDRESS_LO,
			       dd->msi_lo);
	pci_write_config_dword(dd->pcidev, pos + PCI_MSI_ADDRESS_HI,
			       dd->msi_hi);
	pci_read_config_word(dd->pcidev, pos + PCI_MSI_FLAGS, &control);
	if (!(control & PCI_MSI_FLAGS_ENABLE)) {
		control |= PCI_MSI_FLAGS_ENABLE;
		pci_write_config_word(dd->pcidev, pos + PCI_MSI_FLAGS,
				      control);
	}
	
	pci_write_config_word(dd->pcidev, pos +
			      ((control & PCI_MSI_FLAGS_64BIT) ? 12 : 8),
			      dd->msi_data);
	ret = 1;
bail:
	if (!ret && (dd->flags & QIB_HAS_INTX)) {
		qib_enable_intx(dd->pcidev);
		ret = 1;
	}

	
	pci_set_master(dd->pcidev);

	return ret;
}

void qib_nomsi(struct qib_devdata *dd)
{
	dd->msi_lo = 0;
	pci_disable_msi(dd->pcidev);
}

void qib_nomsix(struct qib_devdata *dd)
{
	pci_disable_msix(dd->pcidev);
}

void qib_enable_intx(struct pci_dev *pdev)
{
	u16 cw, new;
	int pos;

	
	pci_read_config_word(pdev, PCI_COMMAND, &cw);
	new = cw & ~PCI_COMMAND_INTX_DISABLE;
	if (new != cw)
		pci_write_config_word(pdev, PCI_COMMAND, new);

	pos = pci_find_capability(pdev, PCI_CAP_ID_MSI);
	if (pos) {
		
		pci_read_config_word(pdev, pos + PCI_MSI_FLAGS, &cw);
		new = cw & ~PCI_MSI_FLAGS_ENABLE;
		if (new != cw)
			pci_write_config_word(pdev, pos + PCI_MSI_FLAGS, new);
	}
	pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
	if (pos) {
		
		pci_read_config_word(pdev, pos + PCI_MSIX_FLAGS, &cw);
		new = cw & ~PCI_MSIX_FLAGS_ENABLE;
		if (new != cw)
			pci_write_config_word(pdev, pos + PCI_MSIX_FLAGS, new);
	}
}

void qib_pcie_getcmd(struct qib_devdata *dd, u16 *cmd, u8 *iline, u8 *cline)
{
	pci_read_config_word(dd->pcidev, PCI_COMMAND, cmd);
	pci_read_config_byte(dd->pcidev, PCI_INTERRUPT_LINE, iline);
	pci_read_config_byte(dd->pcidev, PCI_CACHE_LINE_SIZE, cline);
}

void qib_pcie_reenable(struct qib_devdata *dd, u16 cmd, u8 iline, u8 cline)
{
	int r;
	r = pci_write_config_dword(dd->pcidev, PCI_BASE_ADDRESS_0,
				   dd->pcibar0);
	if (r)
		qib_dev_err(dd, "rewrite of BAR0 failed: %d\n", r);
	r = pci_write_config_dword(dd->pcidev, PCI_BASE_ADDRESS_1,
				   dd->pcibar1);
	if (r)
		qib_dev_err(dd, "rewrite of BAR1 failed: %d\n", r);
	
	pci_write_config_word(dd->pcidev, PCI_COMMAND, cmd);
	pci_write_config_byte(dd->pcidev, PCI_INTERRUPT_LINE, iline);
	pci_write_config_byte(dd->pcidev, PCI_CACHE_LINE_SIZE, cline);
	r = pci_enable_device(dd->pcidev);
	if (r)
		qib_dev_err(dd, "pci_enable_device failed after "
			    "reset: %d\n", r);
}


static int fld2val(int wd, int mask)
{
	int lsbmask;

	if (!mask)
		return 0;
	wd &= mask;
	lsbmask = mask ^ (mask & (mask - 1));
	wd /= lsbmask;
	return wd;
}

static int val2fld(int wd, int mask)
{
	int lsbmask;

	if (!mask)
		return 0;
	lsbmask = mask ^ (mask & (mask - 1));
	wd *= lsbmask;
	return wd;
}

static int qib_pcie_coalesce;
module_param_named(pcie_coalesce, qib_pcie_coalesce, int, S_IRUGO);
MODULE_PARM_DESC(pcie_coalesce, "tune PCIe colescing on some Intel chipsets");

static int qib_tune_pcie_coalesce(struct qib_devdata *dd)
{
	int r;
	struct pci_dev *parent;
	int ppos;
	u16 devid;
	u32 mask, bits, val;

	if (!qib_pcie_coalesce)
		return 0;

	
	parent = dd->pcidev->bus->self;
	if (parent->bus->parent) {
		qib_devinfo(dd->pcidev, "Parent not root\n");
		return 1;
	}
	ppos = pci_pcie_cap(parent);
	if (!ppos)
		return 1;
	if (parent->vendor != 0x8086)
		return 1;

	devid = parent->device;
	if (devid >= 0x25e2 && devid <= 0x25fa) {
		
		if (parent->revision <= 0xb2)
			bits = 1U << 10;
		else
			bits = 7U << 10;
		mask = (3U << 24) | (7U << 10);
	} else if (devid >= 0x65e2 && devid <= 0x65fa) {
		
		bits = 1U << 10;
		mask = (3U << 24) | (7U << 10);
	} else if (devid >= 0x4021 && devid <= 0x402e) {
		
		bits = 7U << 10;
		mask = 7U << 10;
	} else if (devid >= 0x3604 && devid <= 0x360a) {
		
		bits = 7U << 10;
		mask = (3U << 24) | (7U << 10);
	} else {
		
		return 1;
	}
	pci_read_config_dword(parent, 0x48, &val);
	val &= ~mask;
	val |= bits;
	r = pci_write_config_dword(parent, 0x48, val);
	return 0;
}

static int qib_pcie_caps;
module_param_named(pcie_caps, qib_pcie_caps, int, S_IRUGO);
MODULE_PARM_DESC(pcie_caps, "Max PCIe tuning: Payload (0..3), ReadReq (4..7)");

static int qib_tune_pcie_caps(struct qib_devdata *dd)
{
	int ret = 1; 
	struct pci_dev *parent;
	int ppos, epos;
	u16 pcaps, pctl, ecaps, ectl;
	int rc_sup, ep_sup;
	int rc_cur, ep_cur;

	
	parent = dd->pcidev->bus->self;
	if (parent->bus->parent) {
		qib_devinfo(dd->pcidev, "Parent not root\n");
		goto bail;
	}
	ppos = pci_pcie_cap(parent);
	if (ppos) {
		pci_read_config_word(parent, ppos + PCI_EXP_DEVCAP, &pcaps);
		pci_read_config_word(parent, ppos + PCI_EXP_DEVCTL, &pctl);
	} else
		goto bail;
	
	epos = pci_pcie_cap(dd->pcidev);
	if (epos) {
		pci_read_config_word(dd->pcidev, epos + PCI_EXP_DEVCAP, &ecaps);
		pci_read_config_word(dd->pcidev, epos + PCI_EXP_DEVCTL, &ectl);
	} else
		goto bail;
	ret = 0;
	
	rc_sup = fld2val(pcaps, PCI_EXP_DEVCAP_PAYLOAD);
	ep_sup = fld2val(ecaps, PCI_EXP_DEVCAP_PAYLOAD);
	if (rc_sup > ep_sup)
		rc_sup = ep_sup;

	rc_cur = fld2val(pctl, PCI_EXP_DEVCTL_PAYLOAD);
	ep_cur = fld2val(ectl, PCI_EXP_DEVCTL_PAYLOAD);

	
	if (rc_sup > (qib_pcie_caps & 7))
		rc_sup = qib_pcie_caps & 7;
	
	if (rc_sup > rc_cur) {
		rc_cur = rc_sup;
		pctl = (pctl & ~PCI_EXP_DEVCTL_PAYLOAD) |
			val2fld(rc_cur, PCI_EXP_DEVCTL_PAYLOAD);
		pci_write_config_word(parent, ppos + PCI_EXP_DEVCTL, pctl);
	}
	
	if (rc_sup > ep_cur) {
		ep_cur = rc_sup;
		ectl = (ectl & ~PCI_EXP_DEVCTL_PAYLOAD) |
			val2fld(ep_cur, PCI_EXP_DEVCTL_PAYLOAD);
		pci_write_config_word(dd->pcidev, epos + PCI_EXP_DEVCTL, ectl);
	}

	rc_sup = 5;
	if (rc_sup > ((qib_pcie_caps >> 4) & 7))
		rc_sup = (qib_pcie_caps >> 4) & 7;
	rc_cur = fld2val(pctl, PCI_EXP_DEVCTL_READRQ);
	ep_cur = fld2val(ectl, PCI_EXP_DEVCTL_READRQ);

	if (rc_sup > rc_cur) {
		rc_cur = rc_sup;
		pctl = (pctl & ~PCI_EXP_DEVCTL_READRQ) |
			val2fld(rc_cur, PCI_EXP_DEVCTL_READRQ);
		pci_write_config_word(parent, ppos + PCI_EXP_DEVCTL, pctl);
	}
	if (rc_sup > ep_cur) {
		ep_cur = rc_sup;
		ectl = (ectl & ~PCI_EXP_DEVCTL_READRQ) |
			val2fld(ep_cur, PCI_EXP_DEVCTL_READRQ);
		pci_write_config_word(dd->pcidev, epos + PCI_EXP_DEVCTL, ectl);
	}
bail:
	return ret;
}

static pci_ers_result_t
qib_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct qib_devdata *dd = pci_get_drvdata(pdev);
	pci_ers_result_t ret = PCI_ERS_RESULT_RECOVERED;

	switch (state) {
	case pci_channel_io_normal:
		qib_devinfo(pdev, "State Normal, ignoring\n");
		break;

	case pci_channel_io_frozen:
		qib_devinfo(pdev, "State Frozen, requesting reset\n");
		pci_disable_device(pdev);
		ret = PCI_ERS_RESULT_NEED_RESET;
		break;

	case pci_channel_io_perm_failure:
		qib_devinfo(pdev, "State Permanent Failure, disabling\n");
		if (dd) {
			
			dd->flags &= ~QIB_PRESENT;
			qib_disable_after_error(dd);
		}
		 
		ret =  PCI_ERS_RESULT_DISCONNECT;
		break;

	default: 
		qib_devinfo(pdev, "QIB PCI errors detected (state %d)\n",
			state);
		break;
	}
	return ret;
}

static pci_ers_result_t
qib_pci_mmio_enabled(struct pci_dev *pdev)
{
	u64 words = 0U;
	struct qib_devdata *dd = pci_get_drvdata(pdev);
	pci_ers_result_t ret = PCI_ERS_RESULT_RECOVERED;

	if (dd && dd->pport) {
		words = dd->f_portcntr(dd->pport, QIBPORTCNTR_WORDRCV);
		if (words == ~0ULL)
			ret = PCI_ERS_RESULT_NEED_RESET;
	}
	qib_devinfo(pdev, "QIB mmio_enabled function called, "
		 "read wordscntr %Lx, returning %d\n", words, ret);
	return  ret;
}

static pci_ers_result_t
qib_pci_slot_reset(struct pci_dev *pdev)
{
	qib_devinfo(pdev, "QIB link_reset function called, ignored\n");
	return PCI_ERS_RESULT_CAN_RECOVER;
}

static pci_ers_result_t
qib_pci_link_reset(struct pci_dev *pdev)
{
	qib_devinfo(pdev, "QIB link_reset function called, ignored\n");
	return PCI_ERS_RESULT_CAN_RECOVER;
}

static void
qib_pci_resume(struct pci_dev *pdev)
{
	struct qib_devdata *dd = pci_get_drvdata(pdev);
	qib_devinfo(pdev, "QIB resume function called\n");
	pci_cleanup_aer_uncorrect_error_status(pdev);
	qib_init(dd, 1); 
}

struct pci_error_handlers qib_pci_err_handler = {
	.error_detected = qib_pci_error_detected,
	.mmio_enabled = qib_pci_mmio_enabled,
	.link_reset = qib_pci_link_reset,
	.slot_reset = qib_pci_slot_reset,
	.resume = qib_pci_resume,
};
