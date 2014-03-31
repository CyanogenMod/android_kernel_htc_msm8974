/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2006 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ioc4.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <asm/io.h>



#define IOC4_CALIBRATE_COUNT 63		
#define IOC4_CALIBRATE_CYCLES 256	
#define IOC4_CALIBRATE_DISCARD 2	
#define IOC4_CALIBRATE_LOW_MHZ 25	
#define IOC4_CALIBRATE_HIGH_MHZ 75	
#define IOC4_CALIBRATE_DEFAULT_MHZ 66	


static DEFINE_MUTEX(ioc4_mutex);

static LIST_HEAD(ioc4_devices);
static LIST_HEAD(ioc4_submodules);

int
ioc4_register_submodule(struct ioc4_submodule *is)
{
	struct ioc4_driver_data *idd;

	mutex_lock(&ioc4_mutex);
	list_add(&is->is_list, &ioc4_submodules);

	
	if (!is->is_probe)
		goto out;

	list_for_each_entry(idd, &ioc4_devices, idd_list) {
		if (is->is_probe(idd)) {
			printk(KERN_WARNING
			       "%s: IOC4 submodule %s probe failed "
			       "for pci_dev %s",
			       __func__, module_name(is->is_owner),
			       pci_name(idd->idd_pdev));
		}
	}
 out:
	mutex_unlock(&ioc4_mutex);
	return 0;
}

void
ioc4_unregister_submodule(struct ioc4_submodule *is)
{
	struct ioc4_driver_data *idd;

	mutex_lock(&ioc4_mutex);
	list_del(&is->is_list);

	
	if (!is->is_remove)
		goto out;

	list_for_each_entry(idd, &ioc4_devices, idd_list) {
		if (is->is_remove(idd)) {
			printk(KERN_WARNING
			       "%s: IOC4 submodule %s remove failed "
			       "for pci_dev %s.\n",
			       __func__, module_name(is->is_owner),
			       pci_name(idd->idd_pdev));
		}
	}
 out:
	mutex_unlock(&ioc4_mutex);
}


#define IOC4_CALIBRATE_LOW_LIMIT \
	(1000*IOC4_EXTINT_COUNT_DIVISOR/IOC4_CALIBRATE_LOW_MHZ)
#define IOC4_CALIBRATE_HIGH_LIMIT \
	(1000*IOC4_EXTINT_COUNT_DIVISOR/IOC4_CALIBRATE_HIGH_MHZ)
#define IOC4_CALIBRATE_DEFAULT \
	(1000*IOC4_EXTINT_COUNT_DIVISOR/IOC4_CALIBRATE_DEFAULT_MHZ)

#define IOC4_CALIBRATE_END \
	(IOC4_CALIBRATE_CYCLES + IOC4_CALIBRATE_DISCARD)

#define IOC4_INT_OUT_MODE_TOGGLE 0x7	

static void __devinit
ioc4_clock_calibrate(struct ioc4_driver_data *idd)
{
	union ioc4_int_out int_out;
	union ioc4_gpcr gpcr;
	unsigned int state, last_state = 1;
	struct timespec start_ts, end_ts;
	uint64_t start, end, period;
	unsigned int count = 0;

	
	gpcr.raw = 0;
	gpcr.fields.dir = IOC4_GPCR_DIR_0;
	gpcr.fields.int_out_en = 1;
	writel(gpcr.raw, &idd->idd_misc_regs->gpcr_s.raw);

	
	writel(0, &idd->idd_misc_regs->int_out.raw);
	mmiowb();

	
	int_out.raw = 0;
	int_out.fields.count = IOC4_CALIBRATE_COUNT;
	int_out.fields.mode = IOC4_INT_OUT_MODE_TOGGLE;
	int_out.fields.diag = 0;
	writel(int_out.raw, &idd->idd_misc_regs->int_out.raw);
	mmiowb();

	
	do {
		int_out.raw = readl(&idd->idd_misc_regs->int_out.raw);
		state = int_out.fields.int_out;
		if (!last_state && state) {
			count++;
			if (count == IOC4_CALIBRATE_END) {
				ktime_get_ts(&end_ts);
				break;
			} else if (count == IOC4_CALIBRATE_DISCARD)
				ktime_get_ts(&start_ts);
		}
		last_state = state;
	} while (1);

	end = end_ts.tv_sec * NSEC_PER_SEC + end_ts.tv_nsec;
	start = start_ts.tv_sec * NSEC_PER_SEC + start_ts.tv_nsec;
	period = (end - start) /
		(IOC4_CALIBRATE_CYCLES * 2 * (IOC4_CALIBRATE_COUNT + 1));

	
	if (period > IOC4_CALIBRATE_LOW_LIMIT ||
	    period < IOC4_CALIBRATE_HIGH_LIMIT) {
		printk(KERN_INFO
		       "IOC4 %s: Clock calibration failed.  Assuming"
		       "PCI clock is %d ns.\n",
		       pci_name(idd->idd_pdev),
		       IOC4_CALIBRATE_DEFAULT / IOC4_EXTINT_COUNT_DIVISOR);
		period = IOC4_CALIBRATE_DEFAULT;
	} else {
		u64 ns = period;

		do_div(ns, IOC4_EXTINT_COUNT_DIVISOR);
		printk(KERN_DEBUG
		       "IOC4 %s: PCI clock is %llu ns.\n",
		       pci_name(idd->idd_pdev), (unsigned long long)ns);
	}

	idd->count_period = period;
}

static unsigned int __devinit
ioc4_variant(struct ioc4_driver_data *idd)
{
	struct pci_dev *pdev = NULL;
	int found = 0;

	
	do {
		pdev = pci_get_device(PCI_VENDOR_ID_QLOGIC,
				      PCI_DEVICE_ID_QLOGIC_ISP12160, pdev);
		if (pdev &&
		    idd->idd_pdev->bus->number == pdev->bus->number &&
		    3 == PCI_SLOT(pdev->devfn))
			found = 1;
	} while (pdev && !found);
	if (NULL != pdev) {
		pci_dev_put(pdev);
		return IOC4_VARIANT_IO9;
	}

	
	pdev = NULL;
	do {
		pdev = pci_get_device(PCI_VENDOR_ID_VITESSE,
				      PCI_DEVICE_ID_VITESSE_VSC7174, pdev);
		if (pdev &&
		    idd->idd_pdev->bus->number == pdev->bus->number &&
		    3 == PCI_SLOT(pdev->devfn))
			found = 1;
	} while (pdev && !found);
	if (NULL != pdev) {
		pci_dev_put(pdev);
		return IOC4_VARIANT_IO10;
	}

	
	return IOC4_VARIANT_PCI_RT;
}

static void
ioc4_load_modules(struct work_struct *work)
{
	request_module("sgiioc4");
}

static DECLARE_WORK(ioc4_load_modules_work, ioc4_load_modules);

static int __devinit
ioc4_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
{
	struct ioc4_driver_data *idd;
	struct ioc4_submodule *is;
	uint32_t pcmd;
	int ret;

	
	if ((ret = pci_enable_device(pdev))) {
		printk(KERN_WARNING
		       "%s: Failed to enable IOC4 device for pci_dev %s.\n",
		       __func__, pci_name(pdev));
		goto out;
	}
	pci_set_master(pdev);

	
	idd = kmalloc(sizeof(struct ioc4_driver_data), GFP_KERNEL);
	if (!idd) {
		printk(KERN_WARNING
		       "%s: Failed to allocate IOC4 data for pci_dev %s.\n",
		       __func__, pci_name(pdev));
		ret = -ENODEV;
		goto out_idd;
	}
	idd->idd_pdev = pdev;
	idd->idd_pci_id = pci_id;

	idd->idd_bar0 = pci_resource_start(idd->idd_pdev, 0);
	if (!idd->idd_bar0) {
		printk(KERN_WARNING
		       "%s: Unable to find IOC4 misc resource "
		       "for pci_dev %s.\n",
		       __func__, pci_name(idd->idd_pdev));
		ret = -ENODEV;
		goto out_pci;
	}
	if (!request_mem_region(idd->idd_bar0, sizeof(struct ioc4_misc_regs),
			    "ioc4_misc")) {
		printk(KERN_WARNING
		       "%s: Unable to request IOC4 misc region "
		       "for pci_dev %s.\n",
		       __func__, pci_name(idd->idd_pdev));
		ret = -ENODEV;
		goto out_pci;
	}
	idd->idd_misc_regs = ioremap(idd->idd_bar0,
				     sizeof(struct ioc4_misc_regs));
	if (!idd->idd_misc_regs) {
		printk(KERN_WARNING
		       "%s: Unable to remap IOC4 misc region "
		       "for pci_dev %s.\n",
		       __func__, pci_name(idd->idd_pdev));
		ret = -ENODEV;
		goto out_misc_region;
	}

	

	
	idd->idd_variant = ioc4_variant(idd);
	printk(KERN_INFO "IOC4 %s: %s card detected.\n", pci_name(pdev),
	       idd->idd_variant == IOC4_VARIANT_IO9 ? "IO9" :
	       idd->idd_variant == IOC4_VARIANT_PCI_RT ? "PCI-RT" :
	       idd->idd_variant == IOC4_VARIANT_IO10 ? "IO10" : "unknown");

	
	pci_read_config_dword(idd->idd_pdev, PCI_COMMAND, &pcmd);
	pci_write_config_dword(idd->idd_pdev, PCI_COMMAND,
			       pcmd | PCI_COMMAND_PARITY | PCI_COMMAND_SERR);

	
	ioc4_clock_calibrate(idd);

	
	writel(~0, &idd->idd_misc_regs->other_iec.raw);
	writel(~0, &idd->idd_misc_regs->sio_iec);
	
	writel(~0, &idd->idd_misc_regs->other_ir.raw);
	writel(~0, &idd->idd_misc_regs->sio_ir);

	
	idd->idd_serial_data = NULL;
	pci_set_drvdata(idd->idd_pdev, idd);

	mutex_lock(&ioc4_mutex);
	list_add_tail(&idd->idd_list, &ioc4_devices);

	
	list_for_each_entry(is, &ioc4_submodules, is_list) {
		if (is->is_probe && is->is_probe(idd)) {
			printk(KERN_WARNING
			       "%s: IOC4 submodule 0x%s probe failed "
			       "for pci_dev %s.\n",
			       __func__, module_name(is->is_owner),
			       pci_name(idd->idd_pdev));
		}
	}
	mutex_unlock(&ioc4_mutex);

	if (idd->idd_variant != IOC4_VARIANT_PCI_RT) {
		printk(KERN_INFO "IOC4 loading sgiioc4 submodule\n");
		schedule_work(&ioc4_load_modules_work);
	}

	return 0;

out_misc_region:
	release_mem_region(idd->idd_bar0, sizeof(struct ioc4_misc_regs));
out_pci:
	kfree(idd);
out_idd:
	pci_disable_device(pdev);
out:
	return ret;
}

static void __devexit
ioc4_remove(struct pci_dev *pdev)
{
	struct ioc4_submodule *is;
	struct ioc4_driver_data *idd;

	idd = pci_get_drvdata(pdev);

	
	mutex_lock(&ioc4_mutex);
	list_for_each_entry(is, &ioc4_submodules, is_list) {
		if (is->is_remove && is->is_remove(idd)) {
			printk(KERN_WARNING
			       "%s: IOC4 submodule 0x%s remove failed "
			       "for pci_dev %s.\n",
			       __func__, module_name(is->is_owner),
			       pci_name(idd->idd_pdev));
		}
	}
	mutex_unlock(&ioc4_mutex);

	
	iounmap(idd->idd_misc_regs);
	if (!idd->idd_bar0) {
		printk(KERN_WARNING
		       "%s: Unable to get IOC4 misc mapping for pci_dev %s. "
		       "Device removal may be incomplete.\n",
		       __func__, pci_name(idd->idd_pdev));
	}
	release_mem_region(idd->idd_bar0, sizeof(struct ioc4_misc_regs));

	
	pci_disable_device(pdev);

	
	mutex_lock(&ioc4_mutex);
	list_del(&idd->idd_list);
	mutex_unlock(&ioc4_mutex);
	kfree(idd);
}

static struct pci_device_id ioc4_id_table[] = {
	{PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC4, PCI_ANY_ID,
	 PCI_ANY_ID, 0x0b4000, 0xFFFFFF},
	{0}
};

static struct pci_driver ioc4_driver = {
	.name = "IOC4",
	.id_table = ioc4_id_table,
	.probe = ioc4_probe,
	.remove = __devexit_p(ioc4_remove),
};

MODULE_DEVICE_TABLE(pci, ioc4_id_table);


static int __init
ioc4_init(void)
{
	return pci_register_driver(&ioc4_driver);
}

static void __exit
ioc4_exit(void)
{
	
	flush_work_sync(&ioc4_load_modules_work);
	pci_unregister_driver(&ioc4_driver);
}

module_init(ioc4_init);
module_exit(ioc4_exit);

MODULE_AUTHOR("Brent Casavant - Silicon Graphics, Inc. <bcasavan@sgi.com>");
MODULE_DESCRIPTION("PCI driver master module for SGI IOC4 Base-IO Card");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(ioc4_register_submodule);
EXPORT_SYMBOL(ioc4_unregister_submodule);
