/*
 * PCI Error Recovery Driver for RPA-compliant PPC64 platform.
 * Copyright IBM Corp. 2004 2005
 * Copyright Linas Vepstas <linas@linas.org> 2004, 2005
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send comments and feedback to Linas Vepstas <linas@austin.ibm.com>
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <asm/eeh.h>
#include <asm/eeh_event.h>
#include <asm/ppc-pci.h>
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <asm/rtas.h>

static inline const char *eeh_pcid_name(struct pci_dev *pdev)
{
	if (pdev && pdev->dev.driver)
		return pdev->dev.driver->name;
	return "";
}

#if 0
static void print_device_node_tree(struct pci_dn *pdn, int dent)
{
	int i;
	struct device_node *pc;

	if (!pdn)
		return;
	for (i = 0; i < dent; i++)
		printk(" ");
	printk("dn=%s mode=%x \tcfg_addr=%x pe_addr=%x \tfull=%s\n",
		pdn->node->name, pdn->eeh_mode, pdn->eeh_config_addr,
		pdn->eeh_pe_config_addr, pdn->node->full_name);
	dent += 3;
	pc = pdn->node->child;
	while (pc) {
		print_device_node_tree(PCI_DN(pc), dent);
		pc = pc->sibling;
	}
}
#endif

static void eeh_disable_irq(struct pci_dev *dev)
{
	struct eeh_dev *edev = pci_dev_to_eeh_dev(dev);

	if (dev->msi_enabled || dev->msix_enabled)
		return;

	if (!irq_has_action(dev->irq))
		return;

	edev->mode |= EEH_MODE_IRQ_DISABLED;
	disable_irq_nosync(dev->irq);
}

static void eeh_enable_irq(struct pci_dev *dev)
{
	struct eeh_dev *edev = pci_dev_to_eeh_dev(dev);

	if ((edev->mode) & EEH_MODE_IRQ_DISABLED) {
		edev->mode &= ~EEH_MODE_IRQ_DISABLED;
		enable_irq(dev->irq);
	}
}

static int eeh_report_error(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_frozen;

	if (!driver)
		return 0;

	eeh_disable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->error_detected)
		return 0;

	rc = driver->err_handler->error_detected(dev, pci_channel_io_frozen);

	
	if (rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;
	if (*res == PCI_ERS_RESULT_NONE) *res = rc;

	return 0;
}

static int eeh_report_mmio_enabled(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	if (!driver ||
	    !driver->err_handler ||
	    !driver->err_handler->mmio_enabled)
		return 0;

	rc = driver->err_handler->mmio_enabled(dev);

	
	if (rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;
	if (*res == PCI_ERS_RESULT_NONE) *res = rc;

	return 0;
}

static int eeh_report_reset(struct pci_dev *dev, void *userdata)
{
	enum pci_ers_result rc, *res = userdata;
	struct pci_driver *driver = dev->driver;

	if (!driver)
		return 0;

	dev->error_state = pci_channel_io_normal;

	eeh_enable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->slot_reset)
		return 0;

	rc = driver->err_handler->slot_reset(dev);
	if ((*res == PCI_ERS_RESULT_NONE) ||
	    (*res == PCI_ERS_RESULT_RECOVERED)) *res = rc;
	if (*res == PCI_ERS_RESULT_DISCONNECT &&
	     rc == PCI_ERS_RESULT_NEED_RESET) *res = rc;

	return 0;
}

static int eeh_report_resume(struct pci_dev *dev, void *userdata)
{
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_normal;

	if (!driver)
		return 0;

	eeh_enable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->resume)
		return 0;

	driver->err_handler->resume(dev);

	return 0;
}

static int eeh_report_failure(struct pci_dev *dev, void *userdata)
{
	struct pci_driver *driver = dev->driver;

	dev->error_state = pci_channel_io_perm_failure;

	if (!driver)
		return 0;

	eeh_disable_irq(dev);

	if (!driver->err_handler ||
	    !driver->err_handler->error_detected)
		return 0;

	driver->err_handler->error_detected(dev, pci_channel_io_perm_failure);

	return 0;
}

static int eeh_reset_device(struct eeh_dev *edev, struct pci_bus *bus)
{
	struct device_node *dn;
	int cnt, rc;

	
	cnt = edev->freeze_count;

	if (bus)
		pcibios_remove_pci_devices(bus);

	rc = eeh_reset_pe(edev);
	if (rc)
		return rc;

	
	dn = eeh_dev_to_of_node(edev);
	if (!pcibios_find_pci_bus(dn) && of_node_to_eeh_dev(dn->parent))
		dn = dn->parent->child;

	while (dn) {
		struct eeh_dev *pedev = of_node_to_eeh_dev(dn);

		
		if (edev->pe_config_addr == pedev->pe_config_addr) {
			eeh_ops->configure_bridge(dn);
			eeh_restore_bars(pedev);
 		}
		dn = dn->sibling;
	}

	if (bus) {
		ssleep(5);
		pcibios_add_pci_devices(bus);
	}
	edev->freeze_count = cnt;

	return 0;
}

#define MAX_WAIT_FOR_RECOVERY 150

struct eeh_dev *handle_eeh_events(struct eeh_event *event)
{
	struct device_node *frozen_dn;
	struct eeh_dev *frozen_edev;
	struct pci_bus *frozen_bus;
	int rc = 0;
	enum pci_ers_result result = PCI_ERS_RESULT_NONE;
	const char *location, *pci_str, *drv_str, *bus_pci_str, *bus_drv_str;

	frozen_dn = eeh_find_device_pe(eeh_dev_to_of_node(event->edev));
	if (!frozen_dn) {
		location = of_get_property(eeh_dev_to_of_node(event->edev), "ibm,loc-code", NULL);
		location = location ? location : "unknown";
		printk(KERN_ERR "EEH: Error: Cannot find partition endpoint "
		                "for location=%s pci addr=%s\n",
			location, eeh_pci_name(eeh_dev_to_pci_dev(event->edev)));
		return NULL;
	}

	frozen_bus = pcibios_find_pci_bus(frozen_dn);
	location = of_get_property(frozen_dn, "ibm,loc-code", NULL);
	location = location ? location : "unknown";

	if (!frozen_bus)
		frozen_bus = pcibios_find_pci_bus(frozen_dn->parent);

	if (!frozen_bus) {
		printk(KERN_ERR "EEH: Cannot find PCI bus "
		        "for location=%s dn=%s\n",
		        location, frozen_dn->full_name);
		return NULL;
	}

	frozen_edev = of_node_to_eeh_dev(frozen_dn);
	frozen_edev->freeze_count++;
	pci_str = eeh_pci_name(eeh_dev_to_pci_dev(event->edev));
	drv_str = eeh_pcid_name(eeh_dev_to_pci_dev(event->edev));

	if (frozen_edev->freeze_count > EEH_MAX_ALLOWED_FREEZES)
		goto excess_failures;

	printk(KERN_WARNING
	   "EEH: This PCI device has failed %d times in the last hour:\n",
		frozen_edev->freeze_count);

	if (frozen_edev->pdev) {
		bus_pci_str = pci_name(frozen_edev->pdev);
		bus_drv_str = eeh_pcid_name(frozen_edev->pdev);
		printk(KERN_WARNING
			"EEH: Bus location=%s driver=%s pci addr=%s\n",
			location, bus_drv_str, bus_pci_str);
	}

	printk(KERN_WARNING
		"EEH: Device location=%s driver=%s pci addr=%s\n",
		location, drv_str, pci_str);

	pci_walk_bus(frozen_bus, eeh_report_error, &result);

	rc = eeh_ops->wait_state(eeh_dev_to_of_node(frozen_edev), MAX_WAIT_FOR_RECOVERY*1000);
	if (rc < 0 || rc == EEH_STATE_NOT_SUPPORT) {
		printk(KERN_WARNING "EEH: Permanent failure\n");
		goto hard_fail;
	}

	eeh_slot_error_detail(frozen_edev, EEH_LOG_TEMP);

	if (result == PCI_ERS_RESULT_NONE) {
		rc = eeh_reset_device(frozen_edev, frozen_bus);
		if (rc) {
			printk(KERN_WARNING "EEH: Unable to reset, rc=%d\n", rc);
			goto hard_fail;
		}
	}

	
	if (result == PCI_ERS_RESULT_CAN_RECOVER) {
		rc = eeh_pci_enable(frozen_edev, EEH_OPT_THAW_MMIO);

		if (rc < 0)
			goto hard_fail;
		if (rc) {
			result = PCI_ERS_RESULT_NEED_RESET;
		} else {
			result = PCI_ERS_RESULT_NONE;
			pci_walk_bus(frozen_bus, eeh_report_mmio_enabled, &result);
		}
	}

	
	if (result == PCI_ERS_RESULT_CAN_RECOVER) {
		rc = eeh_pci_enable(frozen_edev, EEH_OPT_THAW_DMA);

		if (rc < 0)
			goto hard_fail;
		if (rc)
			result = PCI_ERS_RESULT_NEED_RESET;
		else
			result = PCI_ERS_RESULT_RECOVERED;
	}

	
	if (result == PCI_ERS_RESULT_DISCONNECT) {
		printk(KERN_WARNING "EEH: Device driver gave up\n");
		goto hard_fail;
	}

	
	if (result == PCI_ERS_RESULT_NEED_RESET) {
		rc = eeh_reset_device(frozen_edev, NULL);
		if (rc) {
			printk(KERN_WARNING "EEH: Cannot reset, rc=%d\n", rc);
			goto hard_fail;
		}
		result = PCI_ERS_RESULT_NONE;
		pci_walk_bus(frozen_bus, eeh_report_reset, &result);
	}

	
	if ((result != PCI_ERS_RESULT_RECOVERED) &&
	    (result != PCI_ERS_RESULT_NONE)) {
		printk(KERN_WARNING "EEH: Not recovered\n");
		goto hard_fail;
	}

	
	pci_walk_bus(frozen_bus, eeh_report_resume, NULL);

	return frozen_edev;
	
excess_failures:
	printk(KERN_ERR
	   "EEH: PCI device at location=%s driver=%s pci addr=%s\n"
		"has failed %d times in the last hour "
		"and has been permanently disabled.\n"
		"Please try reseating this device or replacing it.\n",
		location, drv_str, pci_str, frozen_edev->freeze_count);
	goto perm_error;

hard_fail:
	printk(KERN_ERR
	   "EEH: Unable to recover from failure of PCI device "
	   "at location=%s driver=%s pci addr=%s\n"
	   "Please try reseating this device or replacing it.\n",
		location, drv_str, pci_str);

perm_error:
	eeh_slot_error_detail(frozen_edev, EEH_LOG_PERM);

	
	pci_walk_bus(frozen_bus, eeh_report_failure, NULL);

	
	pcibios_remove_pci_devices(frozen_bus);

	return NULL;
}

