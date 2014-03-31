/*
 * drivers/pci/pcie/aer/aerdrv.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * This file implements the AER root port service driver. The driver will
 * register an irq handler. When root port triggers an AER interrupt, the irq
 * handler will collect root port status and schedule a work.
 *
 * Copyright (C) 2006 Intel Corp.
 *	Tom Long Nguyen (tom.l.nguyen@intel.com)
 *	Zhang Yanmin (yanmin.zhang@intel.com)
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci-acpi.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pcieport_if.h>
#include <linux/slab.h>

#include "aerdrv.h"
#include "../../pci.h"

#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "tom.l.nguyen@intel.com"
#define DRIVER_DESC "Root Port Advanced Error Reporting Driver"
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

static int __devinit aer_probe(struct pcie_device *dev);
static void aer_remove(struct pcie_device *dev);
static pci_ers_result_t aer_error_detected(struct pci_dev *dev,
	enum pci_channel_state error);
static void aer_error_resume(struct pci_dev *dev);
static pci_ers_result_t aer_root_reset(struct pci_dev *dev);

static struct pci_error_handlers aer_error_handlers = {
	.error_detected = aer_error_detected,
	.resume		= aer_error_resume,
};

static struct pcie_port_service_driver aerdriver = {
	.name		= "aer",
	.port_type	= PCI_EXP_TYPE_ROOT_PORT,
	.service	= PCIE_PORT_SERVICE_AER,

	.probe		= aer_probe,
	.remove		= aer_remove,

	.err_handler	= &aer_error_handlers,

	.reset_link	= aer_root_reset,
};

static int pcie_aer_disable;

void pci_no_aer(void)
{
	pcie_aer_disable = 1;	
}

bool pci_aer_available(void)
{
	return !pcie_aer_disable && pci_msi_enabled();
}

static int set_device_error_reporting(struct pci_dev *dev, void *data)
{
	bool enable = *((bool *)data);

	if ((dev->pcie_type == PCI_EXP_TYPE_ROOT_PORT) ||
	    (dev->pcie_type == PCI_EXP_TYPE_UPSTREAM) ||
	    (dev->pcie_type == PCI_EXP_TYPE_DOWNSTREAM)) {
		if (enable)
			pci_enable_pcie_error_reporting(dev);
		else
			pci_disable_pcie_error_reporting(dev);
	}

	if (enable)
		pcie_set_ecrc_checking(dev);

	return 0;
}

static void set_downstream_devices_error_reporting(struct pci_dev *dev,
						   bool enable)
{
	set_device_error_reporting(dev, &enable);

	if (!dev->subordinate)
		return;
	pci_walk_bus(dev->subordinate, set_device_error_reporting, &enable);
}

static void aer_enable_rootport(struct aer_rpc *rpc)
{
	struct pci_dev *pdev = rpc->rpd->port;
	int pos, aer_pos;
	u16 reg16;
	u32 reg32;

	pos = pci_pcie_cap(pdev);
	
	pci_read_config_word(pdev, pos+PCI_EXP_DEVSTA, &reg16);
	pci_write_config_word(pdev, pos+PCI_EXP_DEVSTA, reg16);

	
	pci_read_config_word(pdev, pos + PCI_EXP_RTCTL, &reg16);
	reg16 &= ~(SYSTEM_ERROR_INTR_ON_MESG_MASK);
	pci_write_config_word(pdev, pos + PCI_EXP_RTCTL, reg16);

	aer_pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR);
	
	pci_read_config_dword(pdev, aer_pos + PCI_ERR_ROOT_STATUS, &reg32);
	pci_write_config_dword(pdev, aer_pos + PCI_ERR_ROOT_STATUS, reg32);
	pci_read_config_dword(pdev, aer_pos + PCI_ERR_COR_STATUS, &reg32);
	pci_write_config_dword(pdev, aer_pos + PCI_ERR_COR_STATUS, reg32);
	pci_read_config_dword(pdev, aer_pos + PCI_ERR_UNCOR_STATUS, &reg32);
	pci_write_config_dword(pdev, aer_pos + PCI_ERR_UNCOR_STATUS, reg32);

	set_downstream_devices_error_reporting(pdev, true);

	
	pci_read_config_dword(pdev, aer_pos + PCI_ERR_ROOT_COMMAND, &reg32);
	reg32 |= ROOT_PORT_INTR_ON_MESG_MASK;
	pci_write_config_dword(pdev, aer_pos + PCI_ERR_ROOT_COMMAND, reg32);
}

static void aer_disable_rootport(struct aer_rpc *rpc)
{
	struct pci_dev *pdev = rpc->rpd->port;
	u32 reg32;
	int pos;

	set_downstream_devices_error_reporting(pdev, false);

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR);
	
	pci_read_config_dword(pdev, pos + PCI_ERR_ROOT_COMMAND, &reg32);
	reg32 &= ~ROOT_PORT_INTR_ON_MESG_MASK;
	pci_write_config_dword(pdev, pos + PCI_ERR_ROOT_COMMAND, reg32);

	
	pci_read_config_dword(pdev, pos + PCI_ERR_ROOT_STATUS, &reg32);
	pci_write_config_dword(pdev, pos + PCI_ERR_ROOT_STATUS, reg32);
}

irqreturn_t aer_irq(int irq, void *context)
{
	unsigned int status, id;
	struct pcie_device *pdev = (struct pcie_device *)context;
	struct aer_rpc *rpc = get_service_data(pdev);
	int next_prod_idx;
	unsigned long flags;
	int pos;

	pos = pci_find_ext_capability(pdev->port, PCI_EXT_CAP_ID_ERR);
	spin_lock_irqsave(&rpc->e_lock, flags);

	
	pci_read_config_dword(pdev->port, pos + PCI_ERR_ROOT_STATUS, &status);
	if (!(status & (PCI_ERR_ROOT_UNCOR_RCV|PCI_ERR_ROOT_COR_RCV))) {
		spin_unlock_irqrestore(&rpc->e_lock, flags);
		return IRQ_NONE;
	}

	
	pci_read_config_dword(pdev->port, pos + PCI_ERR_ROOT_ERR_SRC, &id);
	pci_write_config_dword(pdev->port, pos + PCI_ERR_ROOT_STATUS, status);

	
	next_prod_idx = rpc->prod_idx + 1;
	if (next_prod_idx == AER_ERROR_SOURCES_MAX)
		next_prod_idx = 0;
	if (next_prod_idx == rpc->cons_idx) {
		spin_unlock_irqrestore(&rpc->e_lock, flags);
		return IRQ_HANDLED;
	}
	rpc->e_sources[rpc->prod_idx].status =  status;
	rpc->e_sources[rpc->prod_idx].id = id;
	rpc->prod_idx = next_prod_idx;
	spin_unlock_irqrestore(&rpc->e_lock, flags);

	
	schedule_work(&rpc->dpc_handler);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(aer_irq);

static struct aer_rpc *aer_alloc_rpc(struct pcie_device *dev)
{
	struct aer_rpc *rpc;

	rpc = kzalloc(sizeof(struct aer_rpc), GFP_KERNEL);
	if (!rpc)
		return NULL;

	
	spin_lock_init(&rpc->e_lock);

	rpc->rpd = dev;
	INIT_WORK(&rpc->dpc_handler, aer_isr);
	mutex_init(&rpc->rpc_mutex);
	init_waitqueue_head(&rpc->wait_release);

	
	set_service_data(dev, rpc);

	return rpc;
}

static void aer_remove(struct pcie_device *dev)
{
	struct aer_rpc *rpc = get_service_data(dev);

	if (rpc) {
		
		if (rpc->isr)
			free_irq(dev->irq, dev);

		wait_event(rpc->wait_release, rpc->prod_idx == rpc->cons_idx);

		aer_disable_rootport(rpc);
		kfree(rpc);
		set_service_data(dev, NULL);
	}
}

static int __devinit aer_probe(struct pcie_device *dev)
{
	int status;
	struct aer_rpc *rpc;
	struct device *device = &dev->device;

	
	status = aer_init(dev);
	if (status)
		return status;

	
	rpc = aer_alloc_rpc(dev);
	if (!rpc) {
		dev_printk(KERN_DEBUG, device, "alloc rpc failed\n");
		aer_remove(dev);
		return -ENOMEM;
	}

	
	status = request_irq(dev->irq, aer_irq, IRQF_SHARED, "aerdrv", dev);
	if (status) {
		dev_printk(KERN_DEBUG, device, "request IRQ failed\n");
		aer_remove(dev);
		return status;
	}

	rpc->isr = 1;

	aer_enable_rootport(rpc);

	return status;
}

static pci_ers_result_t aer_root_reset(struct pci_dev *dev)
{
	u32 reg32;
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);

	
	pci_read_config_dword(dev, pos + PCI_ERR_ROOT_COMMAND, &reg32);
	reg32 &= ~ROOT_PORT_INTR_ON_MESG_MASK;
	pci_write_config_dword(dev, pos + PCI_ERR_ROOT_COMMAND, reg32);

	aer_do_secondary_bus_reset(dev);
	dev_printk(KERN_DEBUG, &dev->dev, "Root Port link has been reset\n");

	
	pci_read_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, &reg32);
	pci_write_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, reg32);

	
	pci_read_config_dword(dev, pos + PCI_ERR_ROOT_COMMAND, &reg32);
	reg32 |= ROOT_PORT_INTR_ON_MESG_MASK;
	pci_write_config_dword(dev, pos + PCI_ERR_ROOT_COMMAND, reg32);

	return PCI_ERS_RESULT_RECOVERED;
}

static pci_ers_result_t aer_error_detected(struct pci_dev *dev,
			enum pci_channel_state error)
{
	
	return PCI_ERS_RESULT_CAN_RECOVER;
}

static void aer_error_resume(struct pci_dev *dev)
{
	int pos;
	u32 status, mask;
	u16 reg16;

	
	pos = pci_pcie_cap(dev);
	pci_read_config_word(dev, pos + PCI_EXP_DEVSTA, &reg16);
	pci_write_config_word(dev, pos + PCI_EXP_DEVSTA, reg16);

	
	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
	pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS, &status);
	pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_SEVER, &mask);
	if (dev->error_state == pci_channel_io_normal)
		status &= ~mask; 
	else
		status &= mask; 
	pci_write_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS, status);
}

static int __init aer_service_init(void)
{
	if (!pci_aer_available() || aer_acpi_firmware_first())
		return -ENXIO;
	return pcie_port_service_register(&aerdriver);
}

static void __exit aer_service_exit(void)
{
	pcie_port_service_unregister(&aerdriver);
}

module_init(aer_service_init);
module_exit(aer_service_exit);
