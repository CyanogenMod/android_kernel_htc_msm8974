/*
 * EHCI HCD (Host Controller Driver) PCI Bus Glue.
 *
 * Copyright (c) 2000-2004 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CONFIG_PCI
#error "This file is PCI bus glue.  CONFIG_PCI must be defined."
#endif

#define PCI_DEVICE_ID_INTEL_CE4100_USB	0x2e70


static int ehci_pci_reinit(struct ehci_hcd *ehci, struct pci_dev *pdev)
{
	int			retval;


	
	retval = pci_set_mwi(pdev);
	if (!retval)
		ehci_dbg(ehci, "MWI active\n");

	return 0;
}

static int ehci_pci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	struct pci_dev		*p_smbus;
	u8			rev;
	u32			temp;
	int			retval;

	switch (pdev->vendor) {
	case PCI_VENDOR_ID_TOSHIBA_2:
		
		if (pdev->device == 0x01b5) {
#ifdef CONFIG_USB_EHCI_BIG_ENDIAN_MMIO
			ehci->big_endian_mmio = 1;
#else
			ehci_warn(ehci,
				  "unsupported big endian Toshiba quirk\n");
#endif
		}
		break;
	}

	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs +
		HC_LENGTH(ehci, ehci_readl(ehci, &ehci->caps->hc_capbase));

	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	switch (pdev->vendor) {
	case PCI_VENDOR_ID_NVIDIA:
		switch (pdev->device) {
		case 0x003c:	
		case 0x005b:	
		case 0x00d8:	
		case 0x00e8:	
			if (pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(31)) < 0)
				ehci_warn(ehci, "can't enable NVidia "
					"workaround for >2GB RAM\n");
			break;
		}
		break;
	}

	
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	if ((pdev->vendor == PCI_VENDOR_ID_AMD && pdev->device == 0x7808) ||
	    (pdev->vendor == PCI_VENDOR_ID_ATI && pdev->device == 0x4396)) {
		ehci->use_dummy_qh = 1;
		ehci_info(ehci, "applying AMD SB700/SB800/Hudson-2/3 EHCI "
				"dummy qh workaround\n");
	}

	
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	switch (pdev->vendor) {
	case PCI_VENDOR_ID_NEC:
		ehci->need_io_watchdog = 0;
		break;
	case PCI_VENDOR_ID_INTEL:
		ehci->need_io_watchdog = 0;
		ehci->fs_i_thresh = 1;
		if (pdev->device == 0x27cc) {
			ehci->broken_periodic = 1;
			ehci_info(ehci, "using broken periodic workaround\n");
		}
		if (pdev->device == 0x0806 || pdev->device == 0x0811
				|| pdev->device == 0x0829) {
			ehci_info(ehci, "disable lpm for langwell/penwell\n");
			ehci->has_lpm = 0;
		}
		if (pdev->device == PCI_DEVICE_ID_INTEL_CE4100_USB) {
			hcd->has_tt = 1;
			tdi_reset(ehci);
		}
		if (pdev->subsystem_vendor == PCI_VENDOR_ID_ASUSTEK) {
			
			if (pdev->device == 0x1c26 || pdev->device == 0x1c2d) {
				ehci_info(ehci, "broken D3 during system sleep on ASUS\n");
				hcd->broken_pci_sleep = 1;
				device_set_wakeup_capable(&pdev->dev, false);
			}
		}
		break;
	case PCI_VENDOR_ID_TDI:
		if (pdev->device == PCI_DEVICE_ID_TDI_EHCI) {
			hcd->has_tt = 1;
			tdi_reset(ehci);
		}
		break;
	case PCI_VENDOR_ID_AMD:
		
		if (usb_amd_find_chipset_info())
			ehci->amd_pll_fix = 1;
		
		if (pdev->device == 0x7463) {
			ehci_info(ehci, "ignoring AMD8111 (errata)\n");
			retval = -EIO;
			goto done;
		}
		break;
	case PCI_VENDOR_ID_NVIDIA:
		switch (pdev->device) {
		case 0x0068:
			if (pdev->revision < 0xa4)
				ehci->no_selective_suspend = 1;
			break;

		case 0x0d9d:
			ehci_info(ehci, "disable lpm/ppcd for nvidia mcp89");
			ehci->has_lpm = 0;
			ehci->has_ppcd = 0;
			ehci->command &= ~CMD_PPCEE;
			break;
		}
		break;
	case PCI_VENDOR_ID_VIA:
		if (pdev->device == 0x3104 && (pdev->revision & 0xf0) == 0x60) {
			u8 tmp;

			pci_read_config_byte(pdev, 0x4b, &tmp);
			if (tmp & 0x20)
				break;
			pci_write_config_byte(pdev, 0x4b, tmp | 0x20);
		}
		break;
	case PCI_VENDOR_ID_ATI:
		
		if (usb_amd_find_chipset_info())
			ehci->amd_pll_fix = 1;
		if ((pdev->device == 0x4386) || (pdev->device == 0x4396)) {
			p_smbus = pci_get_device(PCI_VENDOR_ID_ATI,
						 PCI_DEVICE_ID_ATI_SBX00_SMBUS,
						 NULL);
			if (!p_smbus)
				break;
			rev = p_smbus->revision;
			if ((pdev->device == 0x4386) || (rev == 0x3a)
			    || (rev == 0x3b)) {
				u8 tmp;
				ehci_info(ehci, "applying AMD SB600/SB700 USB "
					"freeze workaround\n");
				pci_read_config_byte(pdev, 0x53, &tmp);
				pci_write_config_byte(pdev, 0x53, tmp | (1<<3));
			}
			pci_dev_put(p_smbus);
		}
		break;
	case PCI_VENDOR_ID_NETMOS:
		
		ehci_info(ehci, "applying MosChip frame-index workaround\n");
		ehci->frame_index_bug = 1;
		break;
	}

	
	temp = pci_find_capability(pdev, 0x0a);
	if (temp) {
		pci_read_config_dword(pdev, temp, &temp);
		temp >>= 16;
		if ((temp & (3 << 13)) == (1 << 13)) {
			temp &= 0x1fff;
			ehci->debug = ehci_to_hcd(ehci)->regs + temp;
			temp = ehci_readl(ehci, &ehci->debug->control);
			ehci_info(ehci, "debug port %d%s\n",
				HCS_DEBUG_PORT(ehci->hcs_params),
				(temp & DBGP_ENABLED)
					? " IN USE"
					: "");
			if (!(temp & DBGP_ENABLED))
				ehci->debug = NULL;
		}
	}

	ehci_reset(ehci);

	
	temp = HCS_N_CC(ehci->hcs_params) * HCS_N_PCC(ehci->hcs_params);
	temp &= 0x0f;
	if (temp && HCS_N_PORTS(ehci->hcs_params) > temp) {
		ehci_dbg(ehci, "bogus port configuration: "
			"cc=%d x pcc=%d < ports=%d\n",
			HCS_N_CC(ehci->hcs_params),
			HCS_N_PCC(ehci->hcs_params),
			HCS_N_PORTS(ehci->hcs_params));

		switch (pdev->vendor) {
		case 0x17a0:		
			
			temp |= (ehci->hcs_params & ~0xf);
			ehci->hcs_params = temp;
			break;
		case PCI_VENDOR_ID_NVIDIA:
			
			break;
		}
	}

	
	pci_read_config_byte(pdev, 0x60, &ehci->sbrn);
	if (pdev->vendor == PCI_VENDOR_ID_STMICRO
	    && pdev->device == PCI_DEVICE_ID_STMICRO_USB_HOST)
		ehci->sbrn = 0x20; 

	if (!device_can_wakeup(&pdev->dev)) {
		u16	port_wake;

		pci_read_config_word(pdev, 0x62, &port_wake);
		if (port_wake & 0x0001) {
			dev_warn(&pdev->dev, "Enabling legacy PCI PM\n");
			device_set_wakeup_capable(&pdev->dev, 1);
		}
	}

#ifdef	CONFIG_USB_SUSPEND
	if (ehci->no_selective_suspend && device_can_wakeup(&pdev->dev))
		ehci_warn(ehci, "selective suspend/wakeup unavailable\n");
#endif

	ehci_port_power(ehci, 1);
	retval = ehci_pci_reinit(ehci, pdev);
done:
	return retval;
}


#ifdef	CONFIG_PM



static int ehci_pci_suspend(struct usb_hcd *hcd, bool do_wakeup)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	unsigned long		flags;
	int			rc = 0;

	if (time_before(jiffies, ehci->next_statechange))
		msleep(10);

	ehci_prepare_ports_for_controller_suspend(ehci, do_wakeup);
	spin_lock_irqsave (&ehci->lock, flags);
	ehci_writel(ehci, 0, &ehci->regs->intr_enable);
	(void)ehci_readl(ehci, &ehci->regs->intr_enable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	spin_unlock_irqrestore (&ehci->lock, flags);

	
	

	return rc;
}

static bool usb_is_intel_switchable_ehci(struct pci_dev *pdev)
{
	return pdev->class == PCI_CLASS_SERIAL_USB_EHCI &&
		pdev->vendor == PCI_VENDOR_ID_INTEL &&
		pdev->device == 0x1E26;
}

static void ehci_enable_xhci_companion(void)
{
	struct pci_dev		*companion = NULL;

	
	for_each_pci_dev(companion) {
		if (!usb_is_intel_switchable_xhci(companion))
			continue;
		usb_enable_xhci_ports(companion);
		return;
	}
}

static int ehci_pci_resume(struct usb_hcd *hcd, bool hibernated)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);

	/* The BIOS on systems with the Intel Panther Point chipset may or may
	 * not support xHCI natively.  That means that during system resume, it
	 * may switch the ports back to EHCI so that users can use their
	 * keyboard to select a kernel from GRUB after resume from hibernate.
	 *
	 * The BIOS is supposed to remember whether the OS had xHCI ports
	 * enabled before resume, and switch the ports back to xHCI when the
	 * BIOS/OS semaphore is written, but we all know we can't trust BIOS
	 * writers.
	 *
	 * Unconditionally switch the ports back to xHCI after a system resume.
	 * We can't tell whether the EHCI or xHCI controller will be resumed
	 * first, so we have to do the port switchover in both drivers.  Writing
	 * a '1' to the port switchover registers should have no effect if the
	 * port was already switched over.
	 */
	if (usb_is_intel_switchable_ehci(pdev))
		ehci_enable_xhci_companion();

	

	if (time_before(jiffies, ehci->next_statechange))
		msleep(100);

	
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	if (ehci_readl(ehci, &ehci->regs->configured_flag) == FLAG_CF &&
				!hibernated) {
		int	mask = INTR_MASK;

		ehci_prepare_ports_for_controller_resume(ehci);
		if (!hcd->self.root_hub->do_remote_wakeup)
			mask &= ~STS_PCD;
		ehci_writel(ehci, mask, &ehci->regs->intr_enable);
		ehci_readl(ehci, &ehci->regs->intr_enable);
		return 0;
	}

	usb_root_hub_lost_power(hcd->self.root_hub);

	(void) ehci_halt(ehci);
	(void) ehci_reset(ehci);
	(void) ehci_pci_reinit(ehci, pdev);

	
	spin_lock_irq(&ehci->lock);
	if (ehci->reclaim)
		end_unlink_async(ehci);
	ehci_work(ehci);
	spin_unlock_irq(&ehci->lock);

	ehci_writel(ehci, ehci->command, &ehci->regs->command);
	ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);
	ehci_readl(ehci, &ehci->regs->command);	

	
	ehci_port_power(ehci, 1);

	ehci->rh_state = EHCI_RH_SUSPENDED;
	return 0;
}
#endif

static int ehci_update_device(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int rc = 0;

	if (!udev->parent) 
		rc = -1;
	
	if (ehci->has_lpm && !udev->parent->parent) {
		rc = ehci_lpm_set_da(ehci, udev->devnum, udev->portnum);
		if (!rc)
			rc = ehci_lpm_check(ehci, udev->portnum);
	}
	return rc;
}

static const struct hc_driver ehci_pci_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"EHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ehci_hcd),

	.irq =			ehci_irq,
	.flags =		HCD_MEMORY | HCD_USB2,

	.reset =		ehci_pci_setup,
	.start =		ehci_run,
#ifdef	CONFIG_PM
	.pci_suspend =		ehci_pci_suspend,
	.pci_resume =		ehci_pci_resume,
#endif
	.stop =			ehci_stop,
	.shutdown =		ehci_shutdown,

	.urb_enqueue =		ehci_urb_enqueue,
	.urb_dequeue =		ehci_urb_dequeue,
	.endpoint_disable =	ehci_endpoint_disable,
	.endpoint_reset =	ehci_endpoint_reset,

	.get_frame_number =	ehci_get_frame,

	.hub_status_data =	ehci_hub_status_data,
	.hub_control =		ehci_hub_control,
	.bus_suspend =		ehci_bus_suspend,
	.bus_resume =		ehci_bus_resume,
	.relinquish_port =	ehci_relinquish_port,
	.port_handed_over =	ehci_port_handed_over,

	.update_device =	ehci_update_device,

	.clear_tt_buffer_complete	= ehci_clear_tt_buffer_complete,
};


static const struct pci_device_id pci_ids [] = { {
	
	PCI_DEVICE_CLASS(PCI_CLASS_SERIAL_USB_EHCI, ~0),
	.driver_data =	(unsigned long) &ehci_pci_hc_driver,
	}, {
	PCI_VDEVICE(STMICRO, PCI_DEVICE_ID_STMICRO_USB_HOST),
	.driver_data = (unsigned long) &ehci_pci_hc_driver,
	},
	{  }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static struct pci_driver ehci_pci_driver = {
	.name =		(char *) hcd_name,
	.id_table =	pci_ids,

	.probe =	usb_hcd_pci_probe,
	.remove =	usb_hcd_pci_remove,
	.shutdown = 	usb_hcd_pci_shutdown,

#ifdef CONFIG_PM_SLEEP
	.driver =	{
		.pm =	&usb_hcd_pci_pm_ops
	},
#endif
};
