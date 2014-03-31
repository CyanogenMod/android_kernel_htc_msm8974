/*
 * Copyright IBM Corporation 2001, 2005, 2006
 * Copyright Dave Engebretsen & Todd Inglett 2001
 * Copyright Linas Vepstas 2005, 2006
 * Copyright 2001-2012 IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Please address comments and feedback to Linas Vepstas <linas@austin.ibm.com>
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/rbtree.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <linux/of.h>

#include <linux/atomic.h>
#include <asm/eeh.h>
#include <asm/eeh_event.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/ppc-pci.h>
#include <asm/rtas.h>



#define EEH_MAX_FAILS	2100000

#define PCI_BUS_RESET_WAIT_MSEC (60*1000)

struct eeh_ops *eeh_ops = NULL;

int eeh_subsystem_enabled;
EXPORT_SYMBOL(eeh_subsystem_enabled);

static DEFINE_RAW_SPINLOCK(confirm_error_lock);

#define EEH_PCI_REGS_LOG_LEN 4096
static unsigned char pci_regs_buf[EEH_PCI_REGS_LOG_LEN];

struct eeh_stats {
	u64 no_device;		
	u64 no_dn;		
	u64 no_cfg_addr;	
	u64 ignored_check;	
	u64 total_mmio_ffs;	
	u64 false_positives;	
	u64 slot_resets;	
};

static struct eeh_stats eeh_stats;

#define IS_BRIDGE(class_code) (((class_code)<<16) == PCI_BASE_CLASS_BRIDGE)

static size_t eeh_gather_pci_data(struct eeh_dev *edev, char * buf, size_t len)
{
	struct device_node *dn = eeh_dev_to_of_node(edev);
	struct pci_dev *dev = eeh_dev_to_pci_dev(edev);
	u32 cfg;
	int cap, i;
	int n = 0;

	n += scnprintf(buf+n, len-n, "%s\n", dn->full_name);
	printk(KERN_WARNING "EEH: of node=%s\n", dn->full_name);

	eeh_ops->read_config(dn, PCI_VENDOR_ID, 4, &cfg);
	n += scnprintf(buf+n, len-n, "dev/vend:%08x\n", cfg);
	printk(KERN_WARNING "EEH: PCI device/vendor: %08x\n", cfg);

	eeh_ops->read_config(dn, PCI_COMMAND, 4, &cfg);
	n += scnprintf(buf+n, len-n, "cmd/stat:%x\n", cfg);
	printk(KERN_WARNING "EEH: PCI cmd/status register: %08x\n", cfg);

	if (!dev) {
		printk(KERN_WARNING "EEH: no PCI device for this of node\n");
		return n;
	}

	
	if (dev->class >> 16 == PCI_BASE_CLASS_BRIDGE) {
		eeh_ops->read_config(dn, PCI_SEC_STATUS, 2, &cfg);
		n += scnprintf(buf+n, len-n, "sec stat:%x\n", cfg);
		printk(KERN_WARNING "EEH: Bridge secondary status: %04x\n", cfg);

		eeh_ops->read_config(dn, PCI_BRIDGE_CONTROL, 2, &cfg);
		n += scnprintf(buf+n, len-n, "brdg ctl:%x\n", cfg);
		printk(KERN_WARNING "EEH: Bridge control: %04x\n", cfg);
	}

	
	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (cap) {
		eeh_ops->read_config(dn, cap, 4, &cfg);
		n += scnprintf(buf+n, len-n, "pcix-cmd:%x\n", cfg);
		printk(KERN_WARNING "EEH: PCI-X cmd: %08x\n", cfg);

		eeh_ops->read_config(dn, cap+4, 4, &cfg);
		n += scnprintf(buf+n, len-n, "pcix-stat:%x\n", cfg);
		printk(KERN_WARNING "EEH: PCI-X status: %08x\n", cfg);
	}

	
	cap = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (cap) {
		n += scnprintf(buf+n, len-n, "pci-e cap10:\n");
		printk(KERN_WARNING
		       "EEH: PCI-E capabilities and status follow:\n");

		for (i=0; i<=8; i++) {
			eeh_ops->read_config(dn, cap+4*i, 4, &cfg);
			n += scnprintf(buf+n, len-n, "%02x:%x\n", 4*i, cfg);
			printk(KERN_WARNING "EEH: PCI-E %02x: %08x\n", i, cfg);
		}

		cap = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
		if (cap) {
			n += scnprintf(buf+n, len-n, "pci-e AER:\n");
			printk(KERN_WARNING
			       "EEH: PCI-E AER capability register set follows:\n");

			for (i=0; i<14; i++) {
				eeh_ops->read_config(dn, cap+4*i, 4, &cfg);
				n += scnprintf(buf+n, len-n, "%02x:%x\n", 4*i, cfg);
				printk(KERN_WARNING "EEH: PCI-E AER %02x: %08x\n", i, cfg);
			}
		}
	}

	
	if (dev->class >> 16 == PCI_BASE_CLASS_BRIDGE) {
		struct device_node *child;

		for_each_child_of_node(dn, child) {
			if (of_node_to_eeh_dev(child))
				n += eeh_gather_pci_data(of_node_to_eeh_dev(child), buf+n, len-n);
		}
	}

	return n;
}

void eeh_slot_error_detail(struct eeh_dev *edev, int severity)
{
	size_t loglen = 0;
	pci_regs_buf[0] = 0;

	eeh_pci_enable(edev, EEH_OPT_THAW_MMIO);
	eeh_ops->configure_bridge(eeh_dev_to_of_node(edev));
	eeh_restore_bars(edev);
	loglen = eeh_gather_pci_data(edev, pci_regs_buf, EEH_PCI_REGS_LOG_LEN);

	eeh_ops->get_log(eeh_dev_to_of_node(edev), severity, pci_regs_buf, loglen);
}

static inline unsigned long eeh_token_to_phys(unsigned long token)
{
	pte_t *ptep;
	unsigned long pa;

	ptep = find_linux_pte(init_mm.pgd, token);
	if (!ptep)
		return token;
	pa = pte_pfn(*ptep) << PAGE_SHIFT;

	return pa | (token & (PAGE_SIZE-1));
}

struct device_node *eeh_find_device_pe(struct device_node *dn)
{
	while (dn->parent && of_node_to_eeh_dev(dn->parent) &&
	       (of_node_to_eeh_dev(dn->parent)->mode & EEH_MODE_SUPPORTED)) {
		dn = dn->parent;
	}
	return dn;
}

static void __eeh_mark_slot(struct device_node *parent, int mode_flag)
{
	struct device_node *dn;

	for_each_child_of_node(parent, dn) {
		if (of_node_to_eeh_dev(dn)) {
			
			struct pci_dev *dev = of_node_to_eeh_dev(dn)->pdev;

			of_node_to_eeh_dev(dn)->mode |= mode_flag;

			if (dev && dev->driver)
				dev->error_state = pci_channel_io_frozen;

			__eeh_mark_slot(dn, mode_flag);
		}
	}
}

void eeh_mark_slot(struct device_node *dn, int mode_flag)
{
	struct pci_dev *dev;
	dn = eeh_find_device_pe(dn);

	
	if (!pcibios_find_pci_bus(dn) && of_node_to_eeh_dev(dn->parent))
		dn = dn->parent;

	of_node_to_eeh_dev(dn)->mode |= mode_flag;

	
	dev = of_node_to_eeh_dev(dn)->pdev;
	if (dev)
		dev->error_state = pci_channel_io_frozen;

	__eeh_mark_slot(dn, mode_flag);
}

static void __eeh_clear_slot(struct device_node *parent, int mode_flag)
{
	struct device_node *dn;

	for_each_child_of_node(parent, dn) {
		if (of_node_to_eeh_dev(dn)) {
			of_node_to_eeh_dev(dn)->mode &= ~mode_flag;
			of_node_to_eeh_dev(dn)->check_count = 0;
			__eeh_clear_slot(dn, mode_flag);
		}
	}
}

void eeh_clear_slot(struct device_node *dn, int mode_flag)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&confirm_error_lock, flags);
	
	dn = eeh_find_device_pe(dn);
	
	
	if (!pcibios_find_pci_bus(dn) && of_node_to_eeh_dev(dn->parent))
		dn = dn->parent;

	of_node_to_eeh_dev(dn)->mode &= ~mode_flag;
	of_node_to_eeh_dev(dn)->check_count = 0;
	__eeh_clear_slot(dn, mode_flag);
	raw_spin_unlock_irqrestore(&confirm_error_lock, flags);
}

int eeh_dn_check_failure(struct device_node *dn, struct pci_dev *dev)
{
	int ret;
	unsigned long flags;
	struct eeh_dev *edev;
	int rc = 0;
	const char *location;

	eeh_stats.total_mmio_ffs++;

	if (!eeh_subsystem_enabled)
		return 0;

	if (!dn) {
		eeh_stats.no_dn++;
		return 0;
	}
	dn = eeh_find_device_pe(dn);
	edev = of_node_to_eeh_dev(dn);

	
	if (!(edev->mode & EEH_MODE_SUPPORTED) ||
	    edev->mode & EEH_MODE_NOCHECK) {
		eeh_stats.ignored_check++;
		pr_debug("EEH: Ignored check (%x) for %s %s\n",
			edev->mode, eeh_pci_name(dev), dn->full_name);
		return 0;
	}

	if (!edev->config_addr && !edev->pe_config_addr) {
		eeh_stats.no_cfg_addr++;
		return 0;
	}

	raw_spin_lock_irqsave(&confirm_error_lock, flags);
	rc = 1;
	if (edev->mode & EEH_MODE_ISOLATED) {
		edev->check_count++;
		if (edev->check_count % EEH_MAX_FAILS == 0) {
			location = of_get_property(dn, "ibm,loc-code", NULL);
			printk(KERN_ERR "EEH: %d reads ignored for recovering device at "
				"location=%s driver=%s pci addr=%s\n",
				edev->check_count, location,
				eeh_driver_name(dev), eeh_pci_name(dev));
			printk(KERN_ERR "EEH: Might be infinite loop in %s driver\n",
				eeh_driver_name(dev));
			dump_stack();
		}
		goto dn_unlock;
	}

	ret = eeh_ops->get_state(dn, NULL);

	if ((ret < 0) ||
	    (ret == EEH_STATE_NOT_SUPPORT) ||
	    (ret & (EEH_STATE_MMIO_ACTIVE | EEH_STATE_DMA_ACTIVE)) ==
	    (EEH_STATE_MMIO_ACTIVE | EEH_STATE_DMA_ACTIVE)) {
		eeh_stats.false_positives++;
		edev->false_positives ++;
		rc = 0;
		goto dn_unlock;
	}

	eeh_stats.slot_resets++;
 
	eeh_mark_slot(dn, EEH_MODE_ISOLATED);
	raw_spin_unlock_irqrestore(&confirm_error_lock, flags);

	eeh_send_failure_event(edev);

	dump_stack();
	return 1;

dn_unlock:
	raw_spin_unlock_irqrestore(&confirm_error_lock, flags);
	return rc;
}

EXPORT_SYMBOL_GPL(eeh_dn_check_failure);

unsigned long eeh_check_failure(const volatile void __iomem *token, unsigned long val)
{
	unsigned long addr;
	struct pci_dev *dev;
	struct device_node *dn;

	
	addr = eeh_token_to_phys((unsigned long __force) token);
	dev = pci_addr_cache_get_device(addr);
	if (!dev) {
		eeh_stats.no_device++;
		return val;
	}

	dn = pci_device_to_OF_node(dev);
	eeh_dn_check_failure(dn, dev);

	pci_dev_put(dev);
	return val;
}

EXPORT_SYMBOL(eeh_check_failure);


int eeh_pci_enable(struct eeh_dev *edev, int function)
{
	int rc;
	struct device_node *dn = eeh_dev_to_of_node(edev);

	rc = eeh_ops->set_option(dn, function);
	if (rc)
		printk(KERN_WARNING "EEH: Unexpected state change %d, err=%d dn=%s\n",
		        function, rc, dn->full_name);

	rc = eeh_ops->wait_state(dn, PCI_BUS_RESET_WAIT_MSEC);
	if (rc > 0 && (rc & EEH_STATE_MMIO_ENABLED) &&
	   (function == EEH_OPT_THAW_MMIO))
		return 0;

	return rc;
}

int pcibios_set_pcie_reset_state(struct pci_dev *dev, enum pcie_reset_state state)
{
	struct device_node *dn = pci_device_to_OF_node(dev);

	switch (state) {
	case pcie_deassert_reset:
		eeh_ops->reset(dn, EEH_RESET_DEACTIVATE);
		break;
	case pcie_hot_reset:
		eeh_ops->reset(dn, EEH_RESET_HOT);
		break;
	case pcie_warm_reset:
		eeh_ops->reset(dn, EEH_RESET_FUNDAMENTAL);
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

void __eeh_set_pe_freset(struct device_node *parent, unsigned int *freset)
{
	struct device_node *dn;

	for_each_child_of_node(parent, dn) {
		if (of_node_to_eeh_dev(dn)) {
			struct pci_dev *dev = of_node_to_eeh_dev(dn)->pdev;

			if (dev && dev->driver)
				*freset |= dev->needs_freset;

			__eeh_set_pe_freset(dn, freset);
		}
	}
}

void eeh_set_pe_freset(struct device_node *dn, unsigned int *freset)
{
	struct pci_dev *dev;
	dn = eeh_find_device_pe(dn);

	
	if (!pcibios_find_pci_bus(dn) && of_node_to_eeh_dev(dn->parent))
		dn = dn->parent;

	dev = of_node_to_eeh_dev(dn)->pdev;
	if (dev)
		*freset |= dev->needs_freset;

	__eeh_set_pe_freset(dn, freset);
}

static void eeh_reset_pe_once(struct eeh_dev *edev)
{
	unsigned int freset = 0;
	struct device_node *dn = eeh_dev_to_of_node(edev);

	eeh_set_pe_freset(dn, &freset);

	if (freset)
		eeh_ops->reset(dn, EEH_RESET_FUNDAMENTAL);
	else
		eeh_ops->reset(dn, EEH_RESET_HOT);

#define PCI_BUS_RST_HOLD_TIME_MSEC 250
	msleep(PCI_BUS_RST_HOLD_TIME_MSEC);
	
	eeh_clear_slot(dn, EEH_MODE_ISOLATED);

	eeh_ops->reset(dn, EEH_RESET_DEACTIVATE);

#define PCI_BUS_SETTLE_TIME_MSEC 1800
	msleep(PCI_BUS_SETTLE_TIME_MSEC);
}

int eeh_reset_pe(struct eeh_dev *edev)
{
	int i, rc;
	struct device_node *dn = eeh_dev_to_of_node(edev);

	
	for (i=0; i<3; i++) {
		eeh_reset_pe_once(edev);

		rc = eeh_ops->wait_state(dn, PCI_BUS_RESET_WAIT_MSEC);
		if (rc == (EEH_STATE_MMIO_ACTIVE | EEH_STATE_DMA_ACTIVE))
			return 0;

		if (rc < 0) {
			printk(KERN_ERR "EEH: unrecoverable slot failure %s\n",
			       dn->full_name);
			return -1;
		}
		printk(KERN_ERR "EEH: bus reset %d failed on slot %s, rc=%d\n",
		       i+1, dn->full_name, rc);
	}

	return -1;
}


static inline void eeh_restore_one_device_bars(struct eeh_dev *edev)
{
	int i;
	u32 cmd;
	struct device_node *dn = eeh_dev_to_of_node(edev);

	if (!edev->phb)
		return;

	for (i=4; i<10; i++) {
		eeh_ops->write_config(dn, i*4, 4, edev->config_space[i]);
	}

	
	eeh_ops->write_config(dn, 12*4, 4, edev->config_space[12]);

#define BYTE_SWAP(OFF) (8*((OFF)/4)+3-(OFF))
#define SAVED_BYTE(OFF) (((u8 *)(edev->config_space))[BYTE_SWAP(OFF)])

	eeh_ops->write_config(dn, PCI_CACHE_LINE_SIZE, 1,
	            SAVED_BYTE(PCI_CACHE_LINE_SIZE));

	eeh_ops->write_config(dn, PCI_LATENCY_TIMER, 1,
	            SAVED_BYTE(PCI_LATENCY_TIMER));

	
	eeh_ops->write_config(dn, 15*4, 4, edev->config_space[15]);

	eeh_ops->read_config(dn, PCI_COMMAND, 4, &cmd);
	if (edev->config_space[1] & PCI_COMMAND_PARITY)
		cmd |= PCI_COMMAND_PARITY;
	else
		cmd &= ~PCI_COMMAND_PARITY;
	if (edev->config_space[1] & PCI_COMMAND_SERR)
		cmd |= PCI_COMMAND_SERR;
	else
		cmd &= ~PCI_COMMAND_SERR;
	eeh_ops->write_config(dn, PCI_COMMAND, 4, cmd);
}

void eeh_restore_bars(struct eeh_dev *edev)
{
	struct device_node *dn;
	if (!edev)
		return;
	
	if ((edev->mode & EEH_MODE_SUPPORTED) && !IS_BRIDGE(edev->class_code))
		eeh_restore_one_device_bars(edev);

	for_each_child_of_node(eeh_dev_to_of_node(edev), dn)
		eeh_restore_bars(of_node_to_eeh_dev(dn));
}

static void eeh_save_bars(struct eeh_dev *edev)
{
	int i;
	struct device_node *dn;

	if (!edev)
		return;
	dn = eeh_dev_to_of_node(edev);
	
	for (i = 0; i < 16; i++)
		eeh_ops->read_config(dn, i * 4, 4, &edev->config_space[i]);
}

static void *eeh_early_enable(struct device_node *dn, void *data)
{
	int ret;
	const u32 *class_code = of_get_property(dn, "class-code", NULL);
	const u32 *vendor_id = of_get_property(dn, "vendor-id", NULL);
	const u32 *device_id = of_get_property(dn, "device-id", NULL);
	const u32 *regs;
	int enable;
	struct eeh_dev *edev = of_node_to_eeh_dev(dn);

	edev->class_code = 0;
	edev->mode = 0;
	edev->check_count = 0;
	edev->freeze_count = 0;
	edev->false_positives = 0;

	if (!of_device_is_available(dn))
		return NULL;

	
	if (!class_code || !vendor_id || !device_id)
		return NULL;

	
	if (dn->type && !strcmp(dn->type, "isa")) {
		edev->mode |= EEH_MODE_NOCHECK;
		return NULL;
	}
	edev->class_code = *class_code;

	regs = of_get_property(dn, "reg", NULL);
	if (regs) {
		
		
		ret = eeh_ops->set_option(dn, EEH_OPT_ENABLE);

		enable = 0;
		if (ret == 0) {
			edev->config_addr = regs[0];

			edev->pe_config_addr = eeh_ops->get_pe_addr(dn);

			ret = eeh_ops->get_state(dn, NULL);
			if (ret > 0 && ret != EEH_STATE_NOT_SUPPORT)
				enable = 1;
		}

		if (enable) {
			eeh_subsystem_enabled = 1;
			edev->mode |= EEH_MODE_SUPPORTED;

			pr_debug("EEH: %s: eeh enabled, config=%x pe_config=%x\n",
				 dn->full_name, edev->config_addr,
				 edev->pe_config_addr);
		} else {

			if (dn->parent && of_node_to_eeh_dev(dn->parent) &&
			    (of_node_to_eeh_dev(dn->parent)->mode & EEH_MODE_SUPPORTED)) {
				
				edev->mode |= EEH_MODE_SUPPORTED;
				edev->config_addr = of_node_to_eeh_dev(dn->parent)->config_addr;
				return NULL;
			}
		}
	} else {
		printk(KERN_WARNING "EEH: %s: unable to get reg property.\n",
		       dn->full_name);
	}

	eeh_save_bars(edev);
	return NULL;
}

int __init eeh_ops_register(struct eeh_ops *ops)
{
	if (!ops->name) {
		pr_warning("%s: Invalid EEH ops name for %p\n",
			__func__, ops);
		return -EINVAL;
	}

	if (eeh_ops && eeh_ops != ops) {
		pr_warning("%s: EEH ops of platform %s already existing (%s)\n",
			__func__, eeh_ops->name, ops->name);
		return -EEXIST;
	}

	eeh_ops = ops;

	return 0;
}

int __exit eeh_ops_unregister(const char *name)
{
	if (!name || !strlen(name)) {
		pr_warning("%s: Invalid EEH ops name\n",
			__func__);
		return -EINVAL;
	}

	if (eeh_ops && !strcmp(eeh_ops->name, name)) {
		eeh_ops = NULL;
		return 0;
	}

	return -EEXIST;
}

void __init eeh_init(void)
{
	struct pci_controller *hose, *tmp;
	struct device_node *phb;
	int ret;

	
	if (!eeh_ops) {
		pr_warning("%s: Platform EEH operation not found\n",
			__func__);
		return;
	} else if ((ret = eeh_ops->init())) {
		pr_warning("%s: Failed to call platform init function (%d)\n",
			__func__, ret);
		return;
	}

	raw_spin_lock_init(&confirm_error_lock);

	
	list_for_each_entry_safe(hose, tmp, &hose_list, list_node) {
		phb = hose->dn;
		traverse_pci_devices(phb, eeh_early_enable, NULL);
	}

	if (eeh_subsystem_enabled)
		printk(KERN_INFO "EEH: PCI Enhanced I/O Error Handling Enabled\n");
	else
		printk(KERN_WARNING "EEH: No capable adapters found\n");
}

static void eeh_add_device_early(struct device_node *dn)
{
	struct pci_controller *phb;

	if (!dn || !of_node_to_eeh_dev(dn))
		return;
	phb = of_node_to_eeh_dev(dn)->phb;

	
	if (NULL == phb || 0 == phb->buid)
		return;

	eeh_early_enable(dn, NULL);
}

void eeh_add_device_tree_early(struct device_node *dn)
{
	struct device_node *sib;

	for_each_child_of_node(dn, sib)
		eeh_add_device_tree_early(sib);
	eeh_add_device_early(dn);
}
EXPORT_SYMBOL_GPL(eeh_add_device_tree_early);

static void eeh_add_device_late(struct pci_dev *dev)
{
	struct device_node *dn;
	struct eeh_dev *edev;

	if (!dev || !eeh_subsystem_enabled)
		return;

	pr_debug("EEH: Adding device %s\n", pci_name(dev));

	dn = pci_device_to_OF_node(dev);
	edev = of_node_to_eeh_dev(dn);
	if (edev->pdev == dev) {
		pr_debug("EEH: Already referenced !\n");
		return;
	}
	WARN_ON(edev->pdev);

	pci_dev_get(dev);
	edev->pdev = dev;
	dev->dev.archdata.edev = edev;

	pci_addr_cache_insert_device(dev);
	eeh_sysfs_add_device(dev);
}

void eeh_add_device_tree_late(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
 		eeh_add_device_late(dev);
 		if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
 			struct pci_bus *subbus = dev->subordinate;
 			if (subbus)
 				eeh_add_device_tree_late(subbus);
 		}
	}
}
EXPORT_SYMBOL_GPL(eeh_add_device_tree_late);

static void eeh_remove_device(struct pci_dev *dev)
{
	struct eeh_dev *edev;

	if (!dev || !eeh_subsystem_enabled)
		return;
	edev = pci_dev_to_eeh_dev(dev);

	
	pr_debug("EEH: Removing device %s\n", pci_name(dev));

	if (!edev || !edev->pdev) {
		pr_debug("EEH: Not referenced !\n");
		return;
	}
	edev->pdev = NULL;
	dev->dev.archdata.edev = NULL;
	pci_dev_put(dev);

	pci_addr_cache_remove_device(dev);
	eeh_sysfs_remove_device(dev);
}

void eeh_remove_bus_device(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->subordinate;
	struct pci_dev *child, *tmp;

	eeh_remove_device(dev);

	if (bus && dev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
		list_for_each_entry_safe(child, tmp, &bus->devices, bus_list)
			 eeh_remove_bus_device(child);
	}
}
EXPORT_SYMBOL_GPL(eeh_remove_bus_device);

static int proc_eeh_show(struct seq_file *m, void *v)
{
	if (0 == eeh_subsystem_enabled) {
		seq_printf(m, "EEH Subsystem is globally disabled\n");
		seq_printf(m, "eeh_total_mmio_ffs=%llu\n", eeh_stats.total_mmio_ffs);
	} else {
		seq_printf(m, "EEH Subsystem is enabled\n");
		seq_printf(m,
				"no device=%llu\n"
				"no device node=%llu\n"
				"no config address=%llu\n"
				"check not wanted=%llu\n"
				"eeh_total_mmio_ffs=%llu\n"
				"eeh_false_positives=%llu\n"
				"eeh_slot_resets=%llu\n",
				eeh_stats.no_device,
				eeh_stats.no_dn,
				eeh_stats.no_cfg_addr,
				eeh_stats.ignored_check,
				eeh_stats.total_mmio_ffs,
				eeh_stats.false_positives,
				eeh_stats.slot_resets);
	}

	return 0;
}

static int proc_eeh_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_eeh_show, NULL);
}

static const struct file_operations proc_eeh_operations = {
	.open      = proc_eeh_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int __init eeh_init_proc(void)
{
	if (machine_is(pseries))
		proc_create("powerpc/eeh", 0, NULL, &proc_eeh_operations);
	return 0;
}
__initcall(eeh_init_proc);
