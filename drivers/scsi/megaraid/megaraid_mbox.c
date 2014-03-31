/*
 *
 *			Linux MegaRAID device driver
 *
 * Copyright (c) 2003-2004  LSI Logic Corporation.
 *
 *	   This program is free software; you can redistribute it and/or
 *	   modify it under the terms of the GNU General Public License
 *	   as published by the Free Software Foundation; either version
 *	   2 of the License, or (at your option) any later version.
 *
 * FILE		: megaraid_mbox.c
 * Version	: v2.20.5.1 (Nov 16 2006)
 *
 * Authors:
 * 	Atul Mukker		<Atul.Mukker@lsi.com>
 * 	Sreenivas Bagalkote	<Sreenivas.Bagalkote@lsi.com>
 * 	Manoj Jose		<Manoj.Jose@lsi.com>
 * 	Seokmann Ju
 *
 * List of supported controllers
 *
 * OEM	Product Name			VID	DID	SSVID	SSID
 * ---	------------			---	---	----	----
 * Dell PERC3/QC			101E	1960	1028	0471
 * Dell PERC3/DC			101E	1960	1028	0493
 * Dell PERC3/SC			101E	1960	1028	0475
 * Dell PERC3/Di			1028	1960	1028	0123
 * Dell PERC4/SC			1000	1960	1028	0520
 * Dell PERC4/DC			1000	1960	1028	0518
 * Dell PERC4/QC			1000	0407	1028	0531
 * Dell PERC4/Di			1028	000F	1028	014A
 * Dell PERC 4e/Si			1028	0013	1028	016c
 * Dell PERC 4e/Di			1028	0013	1028	016d
 * Dell PERC 4e/Di			1028	0013	1028	016e
 * Dell PERC 4e/Di			1028	0013	1028	016f
 * Dell PERC 4e/Di			1028	0013	1028	0170
 * Dell PERC 4e/DC			1000	0408	1028	0002
 * Dell PERC 4e/SC			1000	0408	1028	0001
 *
 *
 * LSI MegaRAID SCSI 320-0		1000	1960	1000	A520
 * LSI MegaRAID SCSI 320-1		1000	1960	1000	0520
 * LSI MegaRAID SCSI 320-2		1000	1960	1000	0518
 * LSI MegaRAID SCSI 320-0X		1000	0407	1000	0530
 * LSI MegaRAID SCSI 320-2X		1000	0407	1000	0532
 * LSI MegaRAID SCSI 320-4X		1000	0407	1000	0531
 * LSI MegaRAID SCSI 320-1E		1000	0408	1000	0001
 * LSI MegaRAID SCSI 320-2E		1000	0408	1000	0002
 * LSI MegaRAID SATA 150-4		1000	1960	1000	4523
 * LSI MegaRAID SATA 150-6		1000	1960	1000	0523
 * LSI MegaRAID SATA 300-4X		1000	0409	1000	3004
 * LSI MegaRAID SATA 300-8X		1000	0409	1000	3008
 *
 * INTEL RAID Controller SRCU42X	1000	0407	8086	0532
 * INTEL RAID Controller SRCS16		1000	1960	8086	0523
 * INTEL RAID Controller SRCU42E	1000	0408	8086	0002
 * INTEL RAID Controller SRCZCRX	1000	0407	8086	0530
 * INTEL RAID Controller SRCS28X	1000	0409	8086	3008
 * INTEL RAID Controller SROMBU42E	1000	0408	8086	3431
 * INTEL RAID Controller SROMBU42E	1000	0408	8086	3499
 * INTEL RAID Controller SRCU51L	1000	1960	8086	0520
 *
 * FSC	MegaRAID PCI Express ROMB	1000	0408	1734	1065
 *
 * ACER	MegaRAID ROMB-2E		1000	0408	1025	004D
 *
 * NEC	MegaRAID PCI Express ROMB	1000	0408	1033	8287
 *
 * For history of changes, see Documentation/scsi/ChangeLog.megaraid
 */

#include <linux/slab.h>
#include <linux/module.h>
#include "megaraid_mbox.h"

static int megaraid_init(void);
static void megaraid_exit(void);

static int megaraid_probe_one(struct pci_dev*, const struct pci_device_id *);
static void megaraid_detach_one(struct pci_dev *);
static void megaraid_mbox_shutdown(struct pci_dev *);

static int megaraid_io_attach(adapter_t *);
static void megaraid_io_detach(adapter_t *);

static int megaraid_init_mbox(adapter_t *);
static void megaraid_fini_mbox(adapter_t *);

static int megaraid_alloc_cmd_packets(adapter_t *);
static void megaraid_free_cmd_packets(adapter_t *);

static int megaraid_mbox_setup_dma_pools(adapter_t *);
static void megaraid_mbox_teardown_dma_pools(adapter_t *);

static int megaraid_sysfs_alloc_resources(adapter_t *);
static void megaraid_sysfs_free_resources(adapter_t *);

static int megaraid_abort_handler(struct scsi_cmnd *);
static int megaraid_reset_handler(struct scsi_cmnd *);

static int mbox_post_sync_cmd(adapter_t *, uint8_t []);
static int mbox_post_sync_cmd_fast(adapter_t *, uint8_t []);
static int megaraid_busywait_mbox(mraid_device_t *);
static int megaraid_mbox_product_info(adapter_t *);
static int megaraid_mbox_extended_cdb(adapter_t *);
static int megaraid_mbox_support_ha(adapter_t *, uint16_t *);
static int megaraid_mbox_support_random_del(adapter_t *);
static int megaraid_mbox_get_max_sg(adapter_t *);
static void megaraid_mbox_enum_raid_scsi(adapter_t *);
static void megaraid_mbox_flush_cache(adapter_t *);
static int megaraid_mbox_fire_sync_cmd(adapter_t *);

static void megaraid_mbox_display_scb(adapter_t *, scb_t *);
static void megaraid_mbox_setup_device_map(adapter_t *);

static int megaraid_queue_command(struct Scsi_Host *, struct scsi_cmnd *);
static scb_t *megaraid_mbox_build_cmd(adapter_t *, struct scsi_cmnd *, int *);
static void megaraid_mbox_runpendq(adapter_t *, scb_t *);
static void megaraid_mbox_prepare_pthru(adapter_t *, scb_t *,
		struct scsi_cmnd *);
static void megaraid_mbox_prepare_epthru(adapter_t *, scb_t *,
		struct scsi_cmnd *);

static irqreturn_t megaraid_isr(int, void *);

static void megaraid_mbox_dpc(unsigned long);

static ssize_t megaraid_sysfs_show_app_hndl(struct device *, struct device_attribute *attr, char *);
static ssize_t megaraid_sysfs_show_ldnum(struct device *, struct device_attribute *attr, char *);

static int megaraid_cmm_register(adapter_t *);
static int megaraid_cmm_unregister(adapter_t *);
static int megaraid_mbox_mm_handler(unsigned long, uioc_t *, uint32_t);
static int megaraid_mbox_mm_command(adapter_t *, uioc_t *);
static void megaraid_mbox_mm_done(adapter_t *, scb_t *);
static int gather_hbainfo(adapter_t *, mraid_hba_info_t *);
static int wait_till_fw_empty(adapter_t *);



MODULE_AUTHOR("megaraidlinux@lsi.com");
MODULE_DESCRIPTION("LSI Logic MegaRAID Mailbox Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MEGARAID_VERSION);


static int megaraid_expose_unconf_disks = 0;
module_param_named(unconf_disks, megaraid_expose_unconf_disks, int, 0);
MODULE_PARM_DESC(unconf_disks,
	"Set to expose unconfigured disks to kernel (default=0)");

static unsigned int max_mbox_busy_wait = MBOX_BUSY_WAIT;
module_param_named(busy_wait, max_mbox_busy_wait, int, 0);
MODULE_PARM_DESC(busy_wait,
	"Max wait for mailbox in microseconds if busy (default=10)");

static unsigned int megaraid_max_sectors = MBOX_MAX_SECTORS;
module_param_named(max_sectors, megaraid_max_sectors, int, 0);
MODULE_PARM_DESC(max_sectors,
	"Maximum number of sectors per IO command (default=128)");

static unsigned int megaraid_cmd_per_lun = MBOX_DEF_CMD_PER_LUN;
module_param_named(cmd_per_lun, megaraid_cmd_per_lun, int, 0);
MODULE_PARM_DESC(cmd_per_lun,
	"Maximum number of commands per logical unit (default=64)");


static unsigned int megaraid_fast_load = 0;
module_param_named(fast_load, megaraid_fast_load, int, 0);
MODULE_PARM_DESC(fast_load,
	"Faster loading of the driver, skips physical devices! (default=0)");


int mraid_debug_level = CL_ANN;
module_param_named(debug_level, mraid_debug_level, int, 0);
MODULE_PARM_DESC(debug_level, "Debug level for driver (default=0)");

static uint8_t megaraid_mbox_version[8] =
	{ 0x02, 0x20, 0x04, 0x06, 3, 7, 20, 5 };


static struct pci_device_id pci_id_table_g[] =  {
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4_DI_DISCOVERY,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4_DI_DISCOVERY,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_PERC4_SC,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4_SC,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_PERC4_DC,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4_DC,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_VERDE,
		PCI_ANY_ID,
		PCI_ANY_ID,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4_DI_EVERGLADES,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4_DI_EVERGLADES,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4E_SI_BIGBEND,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4E_SI_BIGBEND,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4E_DI_KOBUK,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4E_DI_KOBUK,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4E_DI_CORVETTE,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4E_DI_CORVETTE,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4E_DI_EXPEDITION,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4E_DI_EXPEDITION,
	},
	{
		PCI_VENDOR_ID_DELL,
		PCI_DEVICE_ID_PERC4E_DI_GUADALUPE,
		PCI_VENDOR_ID_DELL,
		PCI_SUBSYS_ID_PERC4E_DI_GUADALUPE,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_DOBSON,
		PCI_ANY_ID,
		PCI_ANY_ID,
	},
	{
		PCI_VENDOR_ID_AMI,
		PCI_DEVICE_ID_AMI_MEGARAID3,
		PCI_ANY_ID,
		PCI_ANY_ID,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_AMI_MEGARAID3,
		PCI_ANY_ID,
		PCI_ANY_ID,
	},
	{
		PCI_VENDOR_ID_LSI_LOGIC,
		PCI_DEVICE_ID_LINDSAY,
		PCI_ANY_ID,
		PCI_ANY_ID,
	},
	{0}	
};
MODULE_DEVICE_TABLE(pci, pci_id_table_g);


static struct pci_driver megaraid_pci_driver = {
	.name		= "megaraid",
	.id_table	= pci_id_table_g,
	.probe		= megaraid_probe_one,
	.remove		= __devexit_p(megaraid_detach_one),
	.shutdown	= megaraid_mbox_shutdown,
};




DEVICE_ATTR(megaraid_mbox_app_hndl, S_IRUSR, megaraid_sysfs_show_app_hndl,
		NULL);

static struct device_attribute *megaraid_shost_attrs[] = {
	&dev_attr_megaraid_mbox_app_hndl,
	NULL,
};


DEVICE_ATTR(megaraid_mbox_ld, S_IRUSR, megaraid_sysfs_show_ldnum, NULL);

static struct device_attribute *megaraid_sdev_attrs[] = {
	&dev_attr_megaraid_mbox_ld,
	NULL,
};

static int megaraid_change_queue_depth(struct scsi_device *sdev, int qdepth,
				       int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (qdepth > MBOX_MAX_SCSI_CMDS)
		qdepth = MBOX_MAX_SCSI_CMDS;
	scsi_adjust_queue_depth(sdev, 0, qdepth);
	return sdev->queue_depth;
}

static struct scsi_host_template megaraid_template_g = {
	.module				= THIS_MODULE,
	.name				= "LSI Logic MegaRAID driver",
	.proc_name			= "megaraid",
	.queuecommand			= megaraid_queue_command,
	.eh_abort_handler		= megaraid_abort_handler,
	.eh_device_reset_handler	= megaraid_reset_handler,
	.eh_bus_reset_handler		= megaraid_reset_handler,
	.eh_host_reset_handler		= megaraid_reset_handler,
	.change_queue_depth		= megaraid_change_queue_depth,
	.use_clustering			= ENABLE_CLUSTERING,
	.sdev_attrs			= megaraid_sdev_attrs,
	.shost_attrs			= megaraid_shost_attrs,
};


static int __init
megaraid_init(void)
{
	int	rval;

	
	con_log(CL_ANN, (KERN_INFO "megaraid: %s %s\n", MEGARAID_VERSION,
		MEGARAID_EXT_VERSION));

	
	if (megaraid_cmd_per_lun > MBOX_MAX_SCSI_CMDS) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid mailbox: max commands per lun reset to %d\n",
			MBOX_MAX_SCSI_CMDS));

		megaraid_cmd_per_lun = MBOX_MAX_SCSI_CMDS;
	}


	
	rval = pci_register_driver(&megaraid_pci_driver);
	if (rval < 0) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: could not register hotplug support.\n"));
	}

	return rval;
}


static void __exit
megaraid_exit(void)
{
	con_log(CL_DLEVEL1, (KERN_NOTICE "megaraid: unloading framework\n"));

	
	pci_unregister_driver(&megaraid_pci_driver);

	return;
}


static int __devinit
megaraid_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	adapter_t	*adapter;


	
	con_log(CL_ANN, (KERN_INFO
		"megaraid: probe new device %#4.04x:%#4.04x:%#4.04x:%#4.04x: ",
		pdev->vendor, pdev->device, pdev->subsystem_vendor,
		pdev->subsystem_device));

	con_log(CL_ANN, ("bus %d:slot %d:func %d\n", pdev->bus->number,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn)));

	if (pci_enable_device(pdev)) {
		con_log(CL_ANN, (KERN_WARNING
				"megaraid: pci_enable_device failed\n"));

		return -ENODEV;
	}

	
	pci_set_master(pdev);

	
	adapter = kzalloc(sizeof(adapter_t), GFP_KERNEL);

	if (adapter == NULL) {
		con_log(CL_ANN, (KERN_WARNING
		"megaraid: out of memory, %s %d.\n", __func__, __LINE__));

		goto out_probe_one;
	}


	
	adapter->unique_id	= pdev->bus->number << 8 | pdev->devfn;
	adapter->irq		= pdev->irq;
	adapter->pdev		= pdev;

	atomic_set(&adapter->being_detached, 0);

	
	
	if (pci_set_dma_mask(adapter->pdev, DMA_BIT_MASK(32)) != 0) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: pci_set_dma_mask failed:%d\n", __LINE__));

		goto out_free_adapter;
	}


	
	spin_lock_init(&adapter->lock);

	
	
	INIT_LIST_HEAD(&adapter->kscb_pool);
	spin_lock_init(SCSI_FREE_LIST_LOCK(adapter));

	INIT_LIST_HEAD(&adapter->pend_list);
	spin_lock_init(PENDING_LIST_LOCK(adapter));

	INIT_LIST_HEAD(&adapter->completed_list);
	spin_lock_init(COMPLETED_LIST_LOCK(adapter));


	
	if (megaraid_init_mbox(adapter) != 0) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: maibox adapter did not initialize\n"));

		goto out_free_adapter;
	}

	
	if (megaraid_cmm_register(adapter) != 0) {

		con_log(CL_ANN, (KERN_WARNING
		"megaraid: could not register with management module\n"));

		goto out_fini_mbox;
	}

	
	pci_set_drvdata(pdev, adapter);

	
	if (megaraid_io_attach(adapter) != 0) {

		con_log(CL_ANN, (KERN_WARNING "megaraid: io attach failed\n"));

		goto out_cmm_unreg;
	}

	return 0;

out_cmm_unreg:
	pci_set_drvdata(pdev, NULL);
	megaraid_cmm_unregister(adapter);
out_fini_mbox:
	megaraid_fini_mbox(adapter);
out_free_adapter:
	kfree(adapter);
out_probe_one:
	pci_disable_device(pdev);

	return -ENODEV;
}


static void
megaraid_detach_one(struct pci_dev *pdev)
{
	adapter_t		*adapter;
	struct Scsi_Host	*host;


	
	adapter = pci_get_drvdata(pdev);

	if (!adapter) {
		con_log(CL_ANN, (KERN_CRIT
		"megaraid: Invalid detach on %#4.04x:%#4.04x:%#4.04x:%#4.04x\n",
			pdev->vendor, pdev->device, pdev->subsystem_vendor,
			pdev->subsystem_device));

		return;
	}
	else {
		con_log(CL_ANN, (KERN_NOTICE
		"megaraid: detaching device %#4.04x:%#4.04x:%#4.04x:%#4.04x\n",
			pdev->vendor, pdev->device, pdev->subsystem_vendor,
			pdev->subsystem_device));
	}


	host = adapter->host;

	
	
	
	
	atomic_set(&adapter->being_detached, 1);

	
	megaraid_io_detach(adapter);

	
	
	
	pci_set_drvdata(pdev, NULL);

	
	
	
	
	megaraid_cmm_unregister(adapter);

	
	megaraid_fini_mbox(adapter);

	kfree(adapter);

	scsi_host_put(host);

	pci_disable_device(pdev);

	return;
}


static void
megaraid_mbox_shutdown(struct pci_dev *pdev)
{
	adapter_t		*adapter = pci_get_drvdata(pdev);
	static int		counter;

	if (!adapter) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: null device in shutdown\n"));
		return;
	}

	
	con_log(CL_ANN, (KERN_INFO "megaraid: flushing adapter %d...",
		counter++));

	megaraid_mbox_flush_cache(adapter);

	con_log(CL_ANN, ("done\n"));
}


static int
megaraid_io_attach(adapter_t *adapter)
{
	struct Scsi_Host	*host;

	
	host = scsi_host_alloc(&megaraid_template_g, 8);
	if (!host) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid mbox: scsi_register failed\n"));

		return -1;
	}

	SCSIHOST2ADAP(host)	= (caddr_t)adapter;
	adapter->host		= host;

	host->irq		= adapter->irq;
	host->unique_id		= adapter->unique_id;
	host->can_queue		= adapter->max_cmds;
	host->this_id		= adapter->init_id;
	host->sg_tablesize	= adapter->sglen;
	host->max_sectors	= adapter->max_sectors;
	host->cmd_per_lun	= adapter->cmd_per_lun;
	host->max_channel	= adapter->max_channel;
	host->max_id		= adapter->max_target;
	host->max_lun		= adapter->max_lun;


	
	if (scsi_add_host(host, &adapter->pdev->dev)) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid mbox: scsi_add_host failed\n"));

		scsi_host_put(host);

		return -1;
	}

	scsi_scan_host(host);

	return 0;
}


static void
megaraid_io_detach(adapter_t *adapter)
{
	struct Scsi_Host	*host;

	con_log(CL_DLEVEL1, (KERN_INFO "megaraid: io detach\n"));

	host = adapter->host;

	scsi_remove_host(host);

	return;
}



static int __devinit
megaraid_init_mbox(adapter_t *adapter)
{
	struct pci_dev		*pdev;
	mraid_device_t		*raid_dev;
	int			i;
	uint32_t		magic64;


	adapter->ito	= MBOX_TIMEOUT;
	pdev		= adapter->pdev;

	raid_dev = kzalloc(sizeof(mraid_device_t), GFP_KERNEL);
	if (raid_dev == NULL) return -1;


	adapter->raid_device	= (caddr_t)raid_dev;
	raid_dev->fast_load	= megaraid_fast_load;


	
	raid_dev->baseport = pci_resource_start(pdev, 0);

	if (pci_request_regions(pdev, "MegaRAID: LSI Logic Corporation") != 0) {

		con_log(CL_ANN, (KERN_WARNING
				"megaraid: mem region busy\n"));

		goto out_free_raid_dev;
	}

	raid_dev->baseaddr = ioremap_nocache(raid_dev->baseport, 128);

	if (!raid_dev->baseaddr) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: could not map hba memory\n") );

		goto out_release_regions;
	}

	
	spin_lock_init(&raid_dev->mailbox_lock);

	
	if (megaraid_alloc_cmd_packets(adapter) != 0)
		goto out_iounmap;


	if (megaraid_mbox_fire_sync_cmd(adapter))
		con_log(CL_ANN, ("megaraid: sync cmd failed\n"));


	
	if (request_irq(adapter->irq, megaraid_isr, IRQF_SHARED, "megaraid",
		adapter)) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: Couldn't register IRQ %d!\n", adapter->irq));
		goto out_alloc_cmds;

	}

	
	if (megaraid_mbox_product_info(adapter) != 0)
		goto out_free_irq;

	
	adapter->max_cdb_sz = 10;
	if (megaraid_mbox_extended_cdb(adapter) == 0) {
		adapter->max_cdb_sz = 16;
	}

	adapter->ha		= 0;
	adapter->init_id	= -1;
	if (megaraid_mbox_support_ha(adapter, &adapter->init_id) == 0) {
		adapter->ha = 1;
	}

	megaraid_mbox_setup_device_map(adapter);

	
	if (megaraid_mbox_support_random_del(adapter)) {

		
		
		
		for (i = 0; i <= MAX_LOGICAL_DRIVES_40LD; i++) {
			adapter->device_ids[adapter->max_channel][i] += 0x80;
		}
		adapter->device_ids[adapter->max_channel][adapter->init_id] =
			0xFF;

		raid_dev->random_del_supported = 1;
	}

	adapter->sglen = megaraid_mbox_get_max_sg(adapter);

	
	
	megaraid_mbox_enum_raid_scsi(adapter);

	adapter->max_sectors = megaraid_max_sectors;

	adapter->cmd_per_lun = megaraid_cmd_per_lun;

	if (megaraid_sysfs_alloc_resources(adapter) != 0)
		goto out_free_irq;

	
	
	pci_read_config_dword(adapter->pdev, PCI_CONF_AMISIG64, &magic64);

	if (((magic64 == HBA_SIGNATURE_64_BIT) &&
		((adapter->pdev->subsystem_device !=
		PCI_SUBSYS_ID_MEGARAID_SATA_150_6) &&
		(adapter->pdev->subsystem_device !=
		PCI_SUBSYS_ID_MEGARAID_SATA_150_4))) ||
		(adapter->pdev->vendor == PCI_VENDOR_ID_LSI_LOGIC &&
		adapter->pdev->device == PCI_DEVICE_ID_VERDE) ||
		(adapter->pdev->vendor == PCI_VENDOR_ID_LSI_LOGIC &&
		adapter->pdev->device == PCI_DEVICE_ID_DOBSON) ||
		(adapter->pdev->vendor == PCI_VENDOR_ID_LSI_LOGIC &&
		adapter->pdev->device == PCI_DEVICE_ID_LINDSAY) ||
		(adapter->pdev->vendor == PCI_VENDOR_ID_DELL &&
		adapter->pdev->device == PCI_DEVICE_ID_PERC4_DI_EVERGLADES) ||
		(adapter->pdev->vendor == PCI_VENDOR_ID_DELL &&
		adapter->pdev->device == PCI_DEVICE_ID_PERC4E_DI_KOBUK)) {
		if (pci_set_dma_mask(adapter->pdev, DMA_BIT_MASK(64))) {
			con_log(CL_ANN, (KERN_WARNING
				"megaraid: DMA mask for 64-bit failed\n"));

			if (pci_set_dma_mask (adapter->pdev, DMA_BIT_MASK(32))) {
				con_log(CL_ANN, (KERN_WARNING
					"megaraid: 32-bit DMA mask failed\n"));
				goto out_free_sysfs_res;
			}
		}
	}

	
	tasklet_init(&adapter->dpc_h, megaraid_mbox_dpc,
			(unsigned long)adapter);

	con_log(CL_DLEVEL1, (KERN_INFO
		"megaraid mbox hba successfully initialized\n"));

	return 0;

out_free_sysfs_res:
	megaraid_sysfs_free_resources(adapter);
out_free_irq:
	free_irq(adapter->irq, adapter);
out_alloc_cmds:
	megaraid_free_cmd_packets(adapter);
out_iounmap:
	iounmap(raid_dev->baseaddr);
out_release_regions:
	pci_release_regions(pdev);
out_free_raid_dev:
	kfree(raid_dev);

	return -1;
}


static void
megaraid_fini_mbox(adapter_t *adapter)
{
	mraid_device_t *raid_dev = ADAP2RAIDDEV(adapter);

	
	megaraid_mbox_flush_cache(adapter);

	tasklet_kill(&adapter->dpc_h);

	megaraid_sysfs_free_resources(adapter);

	megaraid_free_cmd_packets(adapter);

	free_irq(adapter->irq, adapter);

	iounmap(raid_dev->baseaddr);

	pci_release_regions(adapter->pdev);

	kfree(raid_dev);

	return;
}


static int
megaraid_alloc_cmd_packets(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	struct pci_dev		*pdev;
	unsigned long		align;
	scb_t			*scb;
	mbox_ccb_t		*ccb;
	struct mraid_pci_blk	*epthru_pci_blk;
	struct mraid_pci_blk	*sg_pci_blk;
	struct mraid_pci_blk	*mbox_pci_blk;
	int			i;

	pdev = adapter->pdev;

	raid_dev->una_mbox64 = pci_alloc_consistent(adapter->pdev,
			sizeof(mbox64_t), &raid_dev->una_mbox64_dma);

	if (!raid_dev->una_mbox64) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));
		return -1;
	}
	memset(raid_dev->una_mbox64, 0, sizeof(mbox64_t));

	raid_dev->mbox	= &raid_dev->una_mbox64->mbox32;

	raid_dev->mbox	= (mbox_t *)((((unsigned long)raid_dev->mbox) + 15) &
				(~0UL ^ 0xFUL));

	raid_dev->mbox64 = (mbox64_t *)(((unsigned long)raid_dev->mbox) - 8);

	align = ((void *)raid_dev->mbox -
			((void *)&raid_dev->una_mbox64->mbox32));

	raid_dev->mbox_dma = (unsigned long)raid_dev->una_mbox64_dma + 8 +
			align;

	
	adapter->ibuf = pci_alloc_consistent(pdev, MBOX_IBUF_SIZE,
				&adapter->ibuf_dma_h);
	if (!adapter->ibuf) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));

		goto out_free_common_mbox;
	}
	memset(adapter->ibuf, 0, MBOX_IBUF_SIZE);

	
	

	adapter->kscb_list = kcalloc(MBOX_MAX_SCSI_CMDS, sizeof(scb_t), GFP_KERNEL);

	if (adapter->kscb_list == NULL) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));
		goto out_free_ibuf;
	}

	
	if (megaraid_mbox_setup_dma_pools(adapter) != 0) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));
		goto out_free_scb_list;
	}

	
	epthru_pci_blk	= raid_dev->epthru_pool;
	sg_pci_blk	= raid_dev->sg_pool;
	mbox_pci_blk	= raid_dev->mbox_pool;

	for (i = 0; i < MBOX_MAX_SCSI_CMDS; i++) {
		scb			= adapter->kscb_list + i;
		ccb			= raid_dev->ccb_list + i;

		ccb->mbox	= (mbox_t *)(mbox_pci_blk[i].vaddr + 16);
		ccb->raw_mbox	= (uint8_t *)ccb->mbox;
		ccb->mbox64	= (mbox64_t *)(mbox_pci_blk[i].vaddr + 8);
		ccb->mbox_dma_h	= (unsigned long)mbox_pci_blk[i].dma_addr + 16;

		
		if (ccb->mbox_dma_h & 0x0F) {
			con_log(CL_ANN, (KERN_CRIT
				"megaraid mbox: not aligned on 16-bytes\n"));

			goto out_teardown_dma_pools;
		}

		ccb->epthru		= (mraid_epassthru_t *)
						epthru_pci_blk[i].vaddr;
		ccb->epthru_dma_h	= epthru_pci_blk[i].dma_addr;
		ccb->pthru		= (mraid_passthru_t *)ccb->epthru;
		ccb->pthru_dma_h	= ccb->epthru_dma_h;


		ccb->sgl64		= (mbox_sgl64 *)sg_pci_blk[i].vaddr;
		ccb->sgl_dma_h		= sg_pci_blk[i].dma_addr;
		ccb->sgl32		= (mbox_sgl32 *)ccb->sgl64;

		scb->ccb		= (caddr_t)ccb;
		scb->gp			= 0;

		scb->sno		= i;	

		scb->scp		= NULL;
		scb->state		= SCB_FREE;
		scb->dma_direction	= PCI_DMA_NONE;
		scb->dma_type		= MRAID_DMA_NONE;
		scb->dev_channel	= -1;
		scb->dev_target		= -1;

		
		list_add_tail(&scb->list, &adapter->kscb_pool);
	}

	return 0;

out_teardown_dma_pools:
	megaraid_mbox_teardown_dma_pools(adapter);
out_free_scb_list:
	kfree(adapter->kscb_list);
out_free_ibuf:
	pci_free_consistent(pdev, MBOX_IBUF_SIZE, (void *)adapter->ibuf,
		adapter->ibuf_dma_h);
out_free_common_mbox:
	pci_free_consistent(adapter->pdev, sizeof(mbox64_t),
		(caddr_t)raid_dev->una_mbox64, raid_dev->una_mbox64_dma);

	return -1;
}


static void
megaraid_free_cmd_packets(adapter_t *adapter)
{
	mraid_device_t *raid_dev = ADAP2RAIDDEV(adapter);

	megaraid_mbox_teardown_dma_pools(adapter);

	kfree(adapter->kscb_list);

	pci_free_consistent(adapter->pdev, MBOX_IBUF_SIZE,
		(void *)adapter->ibuf, adapter->ibuf_dma_h);

	pci_free_consistent(adapter->pdev, sizeof(mbox64_t),
		(caddr_t)raid_dev->una_mbox64, raid_dev->una_mbox64_dma);
	return;
}


static int
megaraid_mbox_setup_dma_pools(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	struct mraid_pci_blk	*epthru_pci_blk;
	struct mraid_pci_blk	*sg_pci_blk;
	struct mraid_pci_blk	*mbox_pci_blk;
	int			i;



	
	raid_dev->mbox_pool_handle = pci_pool_create("megaraid mbox pool",
						adapter->pdev,
						sizeof(mbox64_t) + 16,
						16, 0);

	if (raid_dev->mbox_pool_handle == NULL) {
		goto fail_setup_dma_pool;
	}

	mbox_pci_blk = raid_dev->mbox_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS; i++) {
		mbox_pci_blk[i].vaddr = pci_pool_alloc(
						raid_dev->mbox_pool_handle,
						GFP_KERNEL,
						&mbox_pci_blk[i].dma_addr);
		if (!mbox_pci_blk[i].vaddr) {
			goto fail_setup_dma_pool;
		}
	}

	raid_dev->epthru_pool_handle = pci_pool_create("megaraid mbox pthru",
			adapter->pdev, sizeof(mraid_epassthru_t), 128, 0);

	if (raid_dev->epthru_pool_handle == NULL) {
		goto fail_setup_dma_pool;
	}

	epthru_pci_blk = raid_dev->epthru_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS; i++) {
		epthru_pci_blk[i].vaddr = pci_pool_alloc(
						raid_dev->epthru_pool_handle,
						GFP_KERNEL,
						&epthru_pci_blk[i].dma_addr);
		if (!epthru_pci_blk[i].vaddr) {
			goto fail_setup_dma_pool;
		}
	}


	
	
	raid_dev->sg_pool_handle = pci_pool_create("megaraid mbox sg",
					adapter->pdev,
					sizeof(mbox_sgl64) * MBOX_MAX_SG_SIZE,
					512, 0);

	if (raid_dev->sg_pool_handle == NULL) {
		goto fail_setup_dma_pool;
	}

	sg_pci_blk = raid_dev->sg_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS; i++) {
		sg_pci_blk[i].vaddr = pci_pool_alloc(
						raid_dev->sg_pool_handle,
						GFP_KERNEL,
						&sg_pci_blk[i].dma_addr);
		if (!sg_pci_blk[i].vaddr) {
			goto fail_setup_dma_pool;
		}
	}

	return 0;

fail_setup_dma_pool:
	megaraid_mbox_teardown_dma_pools(adapter);
	return -1;
}


static void
megaraid_mbox_teardown_dma_pools(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	struct mraid_pci_blk	*epthru_pci_blk;
	struct mraid_pci_blk	*sg_pci_blk;
	struct mraid_pci_blk	*mbox_pci_blk;
	int			i;


	sg_pci_blk = raid_dev->sg_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS && sg_pci_blk[i].vaddr; i++) {
		pci_pool_free(raid_dev->sg_pool_handle, sg_pci_blk[i].vaddr,
			sg_pci_blk[i].dma_addr);
	}
	if (raid_dev->sg_pool_handle)
		pci_pool_destroy(raid_dev->sg_pool_handle);


	epthru_pci_blk = raid_dev->epthru_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS && epthru_pci_blk[i].vaddr; i++) {
		pci_pool_free(raid_dev->epthru_pool_handle,
			epthru_pci_blk[i].vaddr, epthru_pci_blk[i].dma_addr);
	}
	if (raid_dev->epthru_pool_handle)
		pci_pool_destroy(raid_dev->epthru_pool_handle);


	mbox_pci_blk = raid_dev->mbox_pool;
	for (i = 0; i < MBOX_MAX_SCSI_CMDS && mbox_pci_blk[i].vaddr; i++) {
		pci_pool_free(raid_dev->mbox_pool_handle,
			mbox_pci_blk[i].vaddr, mbox_pci_blk[i].dma_addr);
	}
	if (raid_dev->mbox_pool_handle)
		pci_pool_destroy(raid_dev->mbox_pool_handle);

	return;
}


static scb_t *
megaraid_alloc_scb(adapter_t *adapter, struct scsi_cmnd *scp)
{
	struct list_head	*head = &adapter->kscb_pool;
	scb_t			*scb = NULL;
	unsigned long		flags;

	
	spin_lock_irqsave(SCSI_FREE_LIST_LOCK(adapter), flags);

	if (list_empty(head)) {
		spin_unlock_irqrestore(SCSI_FREE_LIST_LOCK(adapter), flags);
		return NULL;
	}

	scb = list_entry(head->next, scb_t, list);
	list_del_init(&scb->list);

	spin_unlock_irqrestore(SCSI_FREE_LIST_LOCK(adapter), flags);

	scb->state	= SCB_ACTIVE;
	scb->scp	= scp;
	scb->dma_type	= MRAID_DMA_NONE;

	return scb;
}


static inline void
megaraid_dealloc_scb(adapter_t *adapter, scb_t *scb)
{
	unsigned long		flags;

	
	scb->state	= SCB_FREE;
	scb->scp	= NULL;
	spin_lock_irqsave(SCSI_FREE_LIST_LOCK(adapter), flags);

	list_add(&scb->list, &adapter->kscb_pool);

	spin_unlock_irqrestore(SCSI_FREE_LIST_LOCK(adapter), flags);

	return;
}


static int
megaraid_mbox_mksgl(adapter_t *adapter, scb_t *scb)
{
	struct scatterlist	*sgl;
	mbox_ccb_t		*ccb;
	struct scsi_cmnd	*scp;
	int			sgcnt;
	int			i;


	scp	= scb->scp;
	ccb	= (mbox_ccb_t *)scb->ccb;

	sgcnt = scsi_dma_map(scp);
	BUG_ON(sgcnt < 0 || sgcnt > adapter->sglen);

	
	if (!sgcnt)
		return 0;

	scb->dma_type = MRAID_DMA_WSG;

	scsi_for_each_sg(scp, sgl, sgcnt, i) {
		ccb->sgl64[i].address	= sg_dma_address(sgl);
		ccb->sgl64[i].length	= sg_dma_len(sgl);
	}

	
	return sgcnt;
}


static int
mbox_post_cmd(adapter_t *adapter, scb_t *scb)
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mbox64_t	*mbox64;
	mbox_t		*mbox;
	mbox_ccb_t	*ccb;
	unsigned long	flags;
	unsigned int	i = 0;


	ccb	= (mbox_ccb_t *)scb->ccb;
	mbox	= raid_dev->mbox;
	mbox64	= raid_dev->mbox64;

	spin_lock_irqsave(MAILBOX_LOCK(raid_dev), flags);

	if (unlikely(mbox->busy)) {
		do {
			udelay(1);
			i++;
			rmb();
		} while(mbox->busy && (i < max_mbox_busy_wait));

		if (mbox->busy) {

			spin_unlock_irqrestore(MAILBOX_LOCK(raid_dev), flags);

			return -1;
		}
	}


	
	memcpy((caddr_t)mbox64, (caddr_t)ccb->mbox64, 22);
	mbox->cmdid = scb->sno;

	adapter->outstanding_cmds++;

	if (scb->dma_direction == PCI_DMA_TODEVICE)
		pci_dma_sync_sg_for_device(adapter->pdev,
					   scsi_sglist(scb->scp),
					   scsi_sg_count(scb->scp),
					   PCI_DMA_TODEVICE);

	mbox->busy	= 1;	
	mbox->poll	= 0;
	mbox->ack	= 0;
	wmb();

	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x1);

	spin_unlock_irqrestore(MAILBOX_LOCK(raid_dev), flags);

	return 0;
}


static int
megaraid_queue_command_lck(struct scsi_cmnd *scp, void (*done)(struct scsi_cmnd *))
{
	adapter_t	*adapter;
	scb_t		*scb;
	int		if_busy;

	adapter		= SCP2ADAPTER(scp);
	scp->scsi_done	= done;
	scp->result	= 0;

	if_busy	= 0;
	scb = megaraid_mbox_build_cmd(adapter, scp, &if_busy);
	if (!scb) {	
		done(scp);
		return 0;
	}

	megaraid_mbox_runpendq(adapter, scb);
	return if_busy;
}

static DEF_SCSI_QCMD(megaraid_queue_command)

static scb_t *
megaraid_mbox_build_cmd(adapter_t *adapter, struct scsi_cmnd *scp, int *busy)
{
	mraid_device_t		*rdev = ADAP2RAIDDEV(adapter);
	int			channel;
	int			target;
	int			islogical;
	mbox_ccb_t		*ccb;
	mraid_passthru_t	*pthru;
	mbox64_t		*mbox64;
	mbox_t			*mbox;
	scb_t			*scb;
	char			skip[] = "skipping";
	char			scan[] = "scanning";
	char			*ss;


	MRAID_GET_DEVICE_MAP(adapter, scp, channel, target, islogical);

	if (islogical) {
		switch (scp->cmnd[0]) {
		case TEST_UNIT_READY:
			if (!adapter->ha) {
				scp->result = (DID_OK << 16);
				return NULL;
			}

			if (!(scb = megaraid_alloc_scb(adapter, scp))) {
				scp->result = (DID_ERROR << 16);
				*busy = 1;
				return NULL;
			}

			scb->dma_direction	= scp->sc_data_direction;
			scb->dev_channel	= 0xFF;
			scb->dev_target		= target;
			ccb			= (mbox_ccb_t *)scb->ccb;

			ccb->raw_mbox[0]	= CLUSTER_CMD;
			ccb->raw_mbox[2]	= RESERVATION_STATUS;
			ccb->raw_mbox[3]	= target;

			return scb;

		case MODE_SENSE:
		{
			struct scatterlist	*sgl;
			caddr_t			vaddr;

			sgl = scsi_sglist(scp);
			if (sg_page(sgl)) {
				vaddr = (caddr_t) sg_virt(&sgl[0]);

				memset(vaddr, 0, scp->cmnd[4]);
			}
			else {
				con_log(CL_ANN, (KERN_WARNING
						 "megaraid mailbox: invalid sg:%d\n",
						 __LINE__));
			}
		}
		scp->result = (DID_OK << 16);
		return NULL;

		case INQUIRY:
			if (!(rdev->last_disp & (1L << SCP2CHANNEL(scp)))) {

				con_log(CL_ANN, (KERN_INFO
					"scsi[%d]: scanning scsi channel %d",
					adapter->host->host_no,
					SCP2CHANNEL(scp)));

				con_log(CL_ANN, (
					" [virtual] for logical drives\n"));

				rdev->last_disp |= (1L << SCP2CHANNEL(scp));
			}

			if (scp->cmnd[1] & MEGA_SCSI_INQ_EVPD) {
				scp->sense_buffer[0] = 0x70;
				scp->sense_buffer[2] = ILLEGAL_REQUEST;
				scp->sense_buffer[12] = MEGA_INVALID_FIELD_IN_CDB;
				scp->result = CHECK_CONDITION << 1;
				return NULL;
			}

			

		case READ_CAPACITY:
			if (SCP2LUN(scp)) {
				scp->result = (DID_BAD_TARGET << 16);
				return NULL;
			}
			if ((target % 0x80) >= MAX_LOGICAL_DRIVES_40LD) {
				scp->result = (DID_BAD_TARGET << 16);
				return NULL;
			}


			
			if (!(scb = megaraid_alloc_scb(adapter, scp))) {
				scp->result = (DID_ERROR << 16);
				*busy = 1;
				return NULL;
			}

			ccb			= (mbox_ccb_t *)scb->ccb;
			scb->dev_channel	= 0xFF;
			scb->dev_target		= target;
			pthru			= ccb->pthru;
			mbox			= ccb->mbox;
			mbox64			= ccb->mbox64;

			pthru->timeout		= 0;
			pthru->ars		= 1;
			pthru->reqsenselen	= 14;
			pthru->islogical	= 1;
			pthru->logdrv		= target;
			pthru->cdblen		= scp->cmd_len;
			memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

			mbox->cmd		= MBOXCMD_PASSTHRU64;
			scb->dma_direction	= scp->sc_data_direction;

			pthru->dataxferlen	= scsi_bufflen(scp);
			pthru->dataxferaddr	= ccb->sgl_dma_h;
			pthru->numsge		= megaraid_mbox_mksgl(adapter,
							scb);

			mbox->xferaddr		= 0xFFFFFFFF;
			mbox64->xferaddr_lo	= (uint32_t )ccb->pthru_dma_h;
			mbox64->xferaddr_hi	= 0;

			return scb;

		case READ_6:
		case WRITE_6:
		case READ_10:
		case WRITE_10:
		case READ_12:
		case WRITE_12:

			if (!(scb = megaraid_alloc_scb(adapter, scp))) {
				scp->result = (DID_ERROR << 16);
				*busy = 1;
				return NULL;
			}
			ccb			= (mbox_ccb_t *)scb->ccb;
			scb->dev_channel	= 0xFF;
			scb->dev_target		= target;
			mbox			= ccb->mbox;
			mbox64			= ccb->mbox64;
			mbox->logdrv		= target;

			mbox->cmd = (scp->cmnd[0] & 0x02) ?  MBOXCMD_LWRITE64:
					MBOXCMD_LREAD64 ;

			if (scp->cmd_len == 6) {
				mbox->numsectors = (uint32_t)scp->cmnd[4];
				mbox->lba =
					((uint32_t)scp->cmnd[1] << 16)	|
					((uint32_t)scp->cmnd[2] << 8)	|
					(uint32_t)scp->cmnd[3];

				mbox->lba &= 0x1FFFFF;
			}

			else if (scp->cmd_len == 10) {
				mbox->numsectors =
					(uint32_t)scp->cmnd[8] |
					((uint32_t)scp->cmnd[7] << 8);
				mbox->lba =
					((uint32_t)scp->cmnd[2] << 24) |
					((uint32_t)scp->cmnd[3] << 16) |
					((uint32_t)scp->cmnd[4] << 8) |
					(uint32_t)scp->cmnd[5];
			}

			else if (scp->cmd_len == 12) {
				mbox->lba =
					((uint32_t)scp->cmnd[2] << 24) |
					((uint32_t)scp->cmnd[3] << 16) |
					((uint32_t)scp->cmnd[4] << 8) |
					(uint32_t)scp->cmnd[5];

				mbox->numsectors =
					((uint32_t)scp->cmnd[6] << 24) |
					((uint32_t)scp->cmnd[7] << 16) |
					((uint32_t)scp->cmnd[8] << 8) |
					(uint32_t)scp->cmnd[9];
			}
			else {
				con_log(CL_ANN, (KERN_WARNING
					"megaraid: unsupported CDB length\n"));

				megaraid_dealloc_scb(adapter, scb);

				scp->result = (DID_ERROR << 16);
				return NULL;
			}

			scb->dma_direction = scp->sc_data_direction;

			
			mbox64->xferaddr_lo	= (uint32_t )ccb->sgl_dma_h;
			mbox->numsge		= megaraid_mbox_mksgl(adapter,
							scb);
			mbox->xferaddr		= 0xFFFFFFFF;
			mbox64->xferaddr_hi	= 0;

			return scb;

		case RESERVE:
		case RELEASE:
			if (!adapter->ha) {
				scp->result = (DID_BAD_TARGET << 16);
				return NULL;
			}

			if (!(scb = megaraid_alloc_scb(adapter, scp))) {
				scp->result = (DID_ERROR << 16);
				*busy = 1;
				return NULL;
			}

			ccb			= (mbox_ccb_t *)scb->ccb;
			scb->dev_channel	= 0xFF;
			scb->dev_target		= target;
			ccb->raw_mbox[0]	= CLUSTER_CMD;
			ccb->raw_mbox[2]	=  (scp->cmnd[0] == RESERVE) ?
						RESERVE_LD : RELEASE_LD;

			ccb->raw_mbox[3]	= target;
			scb->dma_direction	= scp->sc_data_direction;

			return scb;

		default:
			scp->result = (DID_BAD_TARGET << 16);
			return NULL;
		}
	}
	else { 

		
		if (target > 15 || SCP2LUN(scp) > 7) {
			scp->result = (DID_BAD_TARGET << 16);
			return NULL;
		}

		
		
		
		if (rdev->fast_load && (target == 15) &&
			(SCP2CHANNEL(scp) == adapter->max_channel -1)) {

			con_log(CL_ANN, (KERN_INFO
			"megaraid[%d]: physical device scan re-enabled\n",
				adapter->host->host_no));
			rdev->fast_load = 0;
		}

		if (!(rdev->last_disp & (1L << SCP2CHANNEL(scp)))) {

			ss = rdev->fast_load ? skip : scan;

			con_log(CL_ANN, (KERN_INFO
				"scsi[%d]: %s scsi channel %d [Phy %d]",
				adapter->host->host_no, ss, SCP2CHANNEL(scp),
				channel));

			con_log(CL_ANN, (
				" for non-raid devices\n"));

			rdev->last_disp |= (1L << SCP2CHANNEL(scp));
		}

		
		if (rdev->fast_load) {
			scp->result = (DID_BAD_TARGET << 16);
			return NULL;
		}

		
		if (!(scb = megaraid_alloc_scb(adapter, scp))) {
			scp->result = (DID_ERROR << 16);
			*busy = 1;
			return NULL;
		}

		ccb			= (mbox_ccb_t *)scb->ccb;
		scb->dev_channel	= channel;
		scb->dev_target		= target;
		scb->dma_direction	= scp->sc_data_direction;
		mbox			= ccb->mbox;
		mbox64			= ccb->mbox64;

		
		if (adapter->max_cdb_sz == 16) {
			mbox->cmd		= MBOXCMD_EXTPTHRU;

			megaraid_mbox_prepare_epthru(adapter, scb, scp);

			mbox64->xferaddr_lo	= (uint32_t)ccb->epthru_dma_h;
			mbox64->xferaddr_hi	= 0;
			mbox->xferaddr		= 0xFFFFFFFF;
		}
		else {
			mbox->cmd = MBOXCMD_PASSTHRU64;

			megaraid_mbox_prepare_pthru(adapter, scb, scp);

			mbox64->xferaddr_lo	= (uint32_t)ccb->pthru_dma_h;
			mbox64->xferaddr_hi	= 0;
			mbox->xferaddr		= 0xFFFFFFFF;
		}
		return scb;
	}

	
}


static void
megaraid_mbox_runpendq(adapter_t *adapter, scb_t *scb_q)
{
	scb_t			*scb;
	unsigned long		flags;

	spin_lock_irqsave(PENDING_LIST_LOCK(adapter), flags);

	if (scb_q) {
		scb_q->state = SCB_PENDQ;
		list_add_tail(&scb_q->list, &adapter->pend_list);
	}

	
	if (adapter->quiescent) {
		spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter), flags);
		return;
	}

	while (!list_empty(&adapter->pend_list)) {

		assert_spin_locked(PENDING_LIST_LOCK(adapter));

		scb = list_entry(adapter->pend_list.next, scb_t, list);

		
		
		

		list_del_init(&scb->list);

		spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter), flags);

		
		
		

		scb->state = SCB_ISSUED;

		if (mbox_post_cmd(adapter, scb) != 0) {

			spin_lock_irqsave(PENDING_LIST_LOCK(adapter), flags);

			scb->state = SCB_PENDQ;

			list_add(&scb->list, &adapter->pend_list);

			spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter),
				flags);

			return;
		}

		spin_lock_irqsave(PENDING_LIST_LOCK(adapter), flags);
	}

	spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter), flags);


	return;
}


static void
megaraid_mbox_prepare_pthru(adapter_t *adapter, scb_t *scb,
		struct scsi_cmnd *scp)
{
	mbox_ccb_t		*ccb;
	mraid_passthru_t	*pthru;
	uint8_t			channel;
	uint8_t			target;

	ccb	= (mbox_ccb_t *)scb->ccb;
	pthru	= ccb->pthru;
	channel	= scb->dev_channel;
	target	= scb->dev_target;

	
	pthru->timeout		= 4;	
	pthru->ars		= 1;
	pthru->islogical	= 0;
	pthru->channel		= 0;
	pthru->target		= (channel << 4) | target;
	pthru->logdrv		= SCP2LUN(scp);
	pthru->reqsenselen	= 14;
	pthru->cdblen		= scp->cmd_len;

	memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

	if (scsi_bufflen(scp)) {
		pthru->dataxferlen	= scsi_bufflen(scp);
		pthru->dataxferaddr	= ccb->sgl_dma_h;
		pthru->numsge		= megaraid_mbox_mksgl(adapter, scb);
	}
	else {
		pthru->dataxferaddr	= 0;
		pthru->dataxferlen	= 0;
		pthru->numsge		= 0;
	}
	return;
}


static void
megaraid_mbox_prepare_epthru(adapter_t *adapter, scb_t *scb,
		struct scsi_cmnd *scp)
{
	mbox_ccb_t		*ccb;
	mraid_epassthru_t	*epthru;
	uint8_t			channel;
	uint8_t			target;

	ccb	= (mbox_ccb_t *)scb->ccb;
	epthru	= ccb->epthru;
	channel	= scb->dev_channel;
	target	= scb->dev_target;

	
	epthru->timeout		= 4;	
	epthru->ars		= 1;
	epthru->islogical	= 0;
	epthru->channel		= 0;
	epthru->target		= (channel << 4) | target;
	epthru->logdrv		= SCP2LUN(scp);
	epthru->reqsenselen	= 14;
	epthru->cdblen		= scp->cmd_len;

	memcpy(epthru->cdb, scp->cmnd, scp->cmd_len);

	if (scsi_bufflen(scp)) {
		epthru->dataxferlen	= scsi_bufflen(scp);
		epthru->dataxferaddr	= ccb->sgl_dma_h;
		epthru->numsge		= megaraid_mbox_mksgl(adapter, scb);
	}
	else {
		epthru->dataxferaddr	= 0;
		epthru->dataxferlen	= 0;
		epthru->numsge		= 0;
	}
	return;
}


static int
megaraid_ack_sequence(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	mbox_t			*mbox;
	scb_t			*scb;
	uint8_t			nstatus;
	uint8_t			completed[MBOX_MAX_FIRMWARE_STATUS];
	struct list_head	clist;
	int			handled;
	uint32_t		dword;
	unsigned long		flags;
	int			i, j;


	mbox	= raid_dev->mbox;

	
	INIT_LIST_HEAD(&clist);

	
	handled = 0;
	spin_lock_irqsave(MAILBOX_LOCK(raid_dev), flags);
	do {
		dword = RDOUTDOOR(raid_dev);
		if (dword != 0x10001234) break;

		handled = 1;

		WROUTDOOR(raid_dev, 0x10001234);

		nstatus = 0;
		
		for (i = 0; i < 0xFFFFF; i++) {
			if (mbox->numstatus != 0xFF) {
				nstatus = mbox->numstatus;
				break;
			}
			rmb();
		}
		mbox->numstatus = 0xFF;

		adapter->outstanding_cmds -= nstatus;

		for (i = 0; i < nstatus; i++) {

			
			for (j = 0; j < 0xFFFFF; j++) {
				if (mbox->completed[i] != 0xFF) break;
				rmb();
			}
			completed[i]		= mbox->completed[i];
			mbox->completed[i]	= 0xFF;

			if (completed[i] == 0xFF) {
				con_log(CL_ANN, (KERN_CRIT
				"megaraid: command posting timed out\n"));

				BUG();
				continue;
			}

			
			if (completed[i] >= MBOX_MAX_SCSI_CMDS) {
				
				scb = adapter->uscb_list + (completed[i] -
						MBOX_MAX_SCSI_CMDS);
			}
			else {
				
				scb = adapter->kscb_list + completed[i];
			}

			scb->status = mbox->status;
			list_add_tail(&scb->list, &clist);
		}

		
		WRINDOOR(raid_dev, 0x02);

	} while(1);

	spin_unlock_irqrestore(MAILBOX_LOCK(raid_dev), flags);


	
	
	spin_lock_irqsave(COMPLETED_LIST_LOCK(adapter), flags);

	list_splice(&clist, &adapter->completed_list);

	spin_unlock_irqrestore(COMPLETED_LIST_LOCK(adapter), flags);


	
	if (handled)
		tasklet_schedule(&adapter->dpc_h);

	return handled;
}


static irqreturn_t
megaraid_isr(int irq, void *devp)
{
	adapter_t	*adapter = devp;
	int		handled;

	handled = megaraid_ack_sequence(adapter);

	
	if (!adapter->quiescent) {
		megaraid_mbox_runpendq(adapter, NULL);
	}

	return IRQ_RETVAL(handled);
}


static void
megaraid_mbox_sync_scb(adapter_t *adapter, scb_t *scb)
{
	mbox_ccb_t	*ccb;

	ccb	= (mbox_ccb_t *)scb->ccb;

	if (scb->dma_direction == PCI_DMA_FROMDEVICE)
		pci_dma_sync_sg_for_cpu(adapter->pdev,
					scsi_sglist(scb->scp),
					scsi_sg_count(scb->scp),
					PCI_DMA_FROMDEVICE);

	scsi_dma_unmap(scb->scp);
	return;
}


static void
megaraid_mbox_dpc(unsigned long devp)
{
	adapter_t		*adapter = (adapter_t *)devp;
	mraid_device_t		*raid_dev;
	struct list_head	clist;
	struct scatterlist	*sgl;
	scb_t			*scb;
	scb_t			*tmp;
	struct scsi_cmnd	*scp;
	mraid_passthru_t	*pthru;
	mraid_epassthru_t	*epthru;
	mbox_ccb_t		*ccb;
	int			islogical;
	int			pdev_index;
	int			pdev_state;
	mbox_t			*mbox;
	unsigned long		flags;
	uint8_t			c;
	int			status;
	uioc_t			*kioc;


	if (!adapter) return;

	raid_dev = ADAP2RAIDDEV(adapter);

	
	INIT_LIST_HEAD(&clist);

	spin_lock_irqsave(COMPLETED_LIST_LOCK(adapter), flags);

	list_splice_init(&adapter->completed_list, &clist);

	spin_unlock_irqrestore(COMPLETED_LIST_LOCK(adapter), flags);


	list_for_each_entry_safe(scb, tmp, &clist, list) {

		status		= scb->status;
		scp		= scb->scp;
		ccb		= (mbox_ccb_t *)scb->ccb;
		pthru		= ccb->pthru;
		epthru		= ccb->epthru;
		mbox		= ccb->mbox;

		
		if (scb->state != SCB_ISSUED) {
			con_log(CL_ANN, (KERN_CRIT
			"megaraid critical err: invalid command %d:%d:%p\n",
				scb->sno, scb->state, scp));
			BUG();
			continue;	
		}

		
		if (scb->sno >= MBOX_MAX_SCSI_CMDS) {
			scb->state	= SCB_FREE;
			scb->status	= status;

			
			list_del_init(&scb->list);

			kioc			= (uioc_t *)scb->gp;
			kioc->status		= 0;

			megaraid_mbox_mm_done(adapter, scb);

			continue;
		}

		
		if (scb->state & SCB_ABORT) {
			con_log(CL_ANN, (KERN_NOTICE
			"megaraid: aborted cmd [%x] completed\n",
				scb->sno));
		}

		islogical = MRAID_IS_LOGICAL(adapter, scp);
		if (scp->cmnd[0] == INQUIRY && status == 0 && islogical == 0
				&& IS_RAID_CH(raid_dev, scb->dev_channel)) {

			sgl = scsi_sglist(scp);
			if (sg_page(sgl)) {
				c = *(unsigned char *) sg_virt(&sgl[0]);
			} else {
				con_log(CL_ANN, (KERN_WARNING
						 "megaraid mailbox: invalid sg:%d\n",
						 __LINE__));
				c = 0;
			}

			if ((c & 0x1F ) == TYPE_DISK) {
				pdev_index = (scb->dev_channel * 16) +
					scb->dev_target;
				pdev_state =
					raid_dev->pdrv_state[pdev_index] & 0x0F;

				if (pdev_state == PDRV_ONLINE		||
					pdev_state == PDRV_FAILED	||
					pdev_state == PDRV_RBLD		||
					pdev_state == PDRV_HOTSPARE	||
					megaraid_expose_unconf_disks == 0) {

					status = 0xF0;
				}
			}
		}

		
		switch (status) {

		case 0x00:

			scp->result = (DID_OK << 16);
			break;

		case 0x02:

			
			if (mbox->cmd == MBOXCMD_PASSTHRU ||
				mbox->cmd == MBOXCMD_PASSTHRU64) {

				memcpy(scp->sense_buffer, pthru->reqsensearea,
						14);

				scp->result = DRIVER_SENSE << 24 |
					DID_OK << 16 | CHECK_CONDITION << 1;
			}
			else {
				if (mbox->cmd == MBOXCMD_EXTPTHRU) {

					memcpy(scp->sense_buffer,
						epthru->reqsensearea, 14);

					scp->result = DRIVER_SENSE << 24 |
						DID_OK << 16 |
						CHECK_CONDITION << 1;
				} else {
					scp->sense_buffer[0] = 0x70;
					scp->sense_buffer[2] = ABORTED_COMMAND;
					scp->result = CHECK_CONDITION << 1;
				}
			}
			break;

		case 0x08:

			scp->result = DID_BUS_BUSY << 16 | status;
			break;

		default:

			if (scp->cmnd[0] == TEST_UNIT_READY) {
				scp->result = DID_ERROR << 16 |
					RESERVATION_CONFLICT << 1;
			}
			else
			if (status == 1 && (scp->cmnd[0] == RESERVE ||
					 scp->cmnd[0] == RELEASE)) {

				scp->result = DID_ERROR << 16 |
					RESERVATION_CONFLICT << 1;
			}
			else {
				scp->result = DID_BAD_TARGET << 16 | status;
			}
		}

		
		if (status) {
			megaraid_mbox_display_scb(adapter, scb);
		}

		
		
		megaraid_mbox_sync_scb(adapter, scb);

		
		list_del_init(&scb->list);

		
		megaraid_dealloc_scb(adapter, scb);

		
		scp->scsi_done(scp);
	}

	return;
}


static int
megaraid_abort_handler(struct scsi_cmnd *scp)
{
	adapter_t		*adapter;
	mraid_device_t		*raid_dev;
	scb_t			*scb;
	scb_t			*tmp;
	int			found;
	unsigned long		flags;
	int			i;


	adapter		= SCP2ADAPTER(scp);
	raid_dev	= ADAP2RAIDDEV(adapter);

	con_log(CL_ANN, (KERN_WARNING
		"megaraid: aborting cmd=%x <c=%d t=%d l=%d>\n",
		scp->cmnd[0], SCP2CHANNEL(scp),
		SCP2TARGET(scp), SCP2LUN(scp)));

	
	if (raid_dev->hw_error) {
		con_log(CL_ANN, (KERN_NOTICE
			"megaraid: hw error, not aborting\n"));
		return FAILED;
	}

	
	
	
	
	scb = NULL;
	spin_lock_irqsave(COMPLETED_LIST_LOCK(adapter), flags);
	list_for_each_entry_safe(scb, tmp, &adapter->completed_list, list) {

		if (scb->scp == scp) {	

			list_del_init(&scb->list);	

			con_log(CL_ANN, (KERN_WARNING
			"megaraid: %d[%d:%d], abort from completed list\n",
				scb->sno, scb->dev_channel, scb->dev_target));

			scp->result = (DID_ABORT << 16);
			scp->scsi_done(scp);

			megaraid_dealloc_scb(adapter, scb);

			spin_unlock_irqrestore(COMPLETED_LIST_LOCK(adapter),
				flags);

			return SUCCESS;
		}
	}
	spin_unlock_irqrestore(COMPLETED_LIST_LOCK(adapter), flags);


	
	
	
	spin_lock_irqsave(PENDING_LIST_LOCK(adapter), flags);
	list_for_each_entry_safe(scb, tmp, &adapter->pend_list, list) {

		if (scb->scp == scp) {	

			list_del_init(&scb->list);	

			ASSERT(!(scb->state & SCB_ISSUED));

			con_log(CL_ANN, (KERN_WARNING
				"megaraid abort: [%d:%d], driver owner\n",
				scb->dev_channel, scb->dev_target));

			scp->result = (DID_ABORT << 16);
			scp->scsi_done(scp);

			megaraid_dealloc_scb(adapter, scb);

			spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter),
				flags);

			return SUCCESS;
		}
	}
	spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter), flags);


	
	
	
	
	found = 0;
	spin_lock_irq(&adapter->lock);
	for (i = 0; i < MBOX_MAX_SCSI_CMDS; i++) {
		scb = adapter->kscb_list + i;

		if (scb->scp == scp) {

			found = 1;

			if (!(scb->state & SCB_ISSUED)) {
				con_log(CL_ANN, (KERN_WARNING
				"megaraid abort: %d[%d:%d], invalid state\n",
				scb->sno, scb->dev_channel, scb->dev_target));
				BUG();
			}
			else {
				con_log(CL_ANN, (KERN_WARNING
				"megaraid abort: %d[%d:%d], fw owner\n",
				scb->sno, scb->dev_channel, scb->dev_target));
			}
		}
	}
	spin_unlock_irq(&adapter->lock);

	if (!found) {
		con_log(CL_ANN, (KERN_WARNING "megaraid abort: do now own\n"));

		
		return SUCCESS;
	}

	
	
	
	return FAILED;
}

static int
megaraid_reset_handler(struct scsi_cmnd *scp)
{
	adapter_t	*adapter;
	scb_t		*scb;
	scb_t		*tmp;
	mraid_device_t	*raid_dev;
	unsigned long	flags;
	uint8_t		raw_mbox[sizeof(mbox_t)];
	int		rval;
	int		recovery_window;
	int		recovering;
	int		i;
	uioc_t		*kioc;

	adapter		= SCP2ADAPTER(scp);
	raid_dev	= ADAP2RAIDDEV(adapter);

	
	if (raid_dev->hw_error) {
		con_log(CL_ANN, (KERN_NOTICE
			"megaraid: hw error, cannot reset\n"));
		return FAILED;
	}


	
	
	
	
	
	spin_lock_irqsave(PENDING_LIST_LOCK(adapter), flags);
	list_for_each_entry_safe(scb, tmp, &adapter->pend_list, list) {
		list_del_init(&scb->list);	

		if (scb->sno >= MBOX_MAX_SCSI_CMDS) {
			con_log(CL_ANN, (KERN_WARNING
			"megaraid: IOCTL packet with %d[%d:%d] being reset\n",
			scb->sno, scb->dev_channel, scb->dev_target));

			scb->status = -1;

			kioc			= (uioc_t *)scb->gp;
			kioc->status		= -EFAULT;

			megaraid_mbox_mm_done(adapter, scb);
		} else {
			if (scb->scp == scp) {	
				con_log(CL_ANN, (KERN_WARNING
					"megaraid: %d[%d:%d], reset from pending list\n",
					scb->sno, scb->dev_channel, scb->dev_target));
			} else {
				con_log(CL_ANN, (KERN_WARNING
				"megaraid: IO packet with %d[%d:%d] being reset\n",
				scb->sno, scb->dev_channel, scb->dev_target));
			}

			scb->scp->result = (DID_RESET << 16);
			scb->scp->scsi_done(scb->scp);

			megaraid_dealloc_scb(adapter, scb);
		}
	}
	spin_unlock_irqrestore(PENDING_LIST_LOCK(adapter), flags);

	if (adapter->outstanding_cmds) {
		con_log(CL_ANN, (KERN_NOTICE
			"megaraid: %d outstanding commands. Max wait %d sec\n",
			adapter->outstanding_cmds,
			(MBOX_RESET_WAIT + MBOX_RESET_EXT_WAIT)));
	}

	recovery_window = MBOX_RESET_WAIT + MBOX_RESET_EXT_WAIT;

	recovering = adapter->outstanding_cmds;

	for (i = 0; i < recovery_window; i++) {

		megaraid_ack_sequence(adapter);

		
		if (!(i % 5)) {
			con_log(CL_ANN, (
			"megaraid mbox: Wait for %d commands to complete:%d\n",
				adapter->outstanding_cmds,
				(MBOX_RESET_WAIT + MBOX_RESET_EXT_WAIT) - i));
		}

		
		if (adapter->outstanding_cmds == 0) {
			break;
		}

		msleep(1000);
	}

	spin_lock(&adapter->lock);

	
	if (adapter->outstanding_cmds) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid mbox: critical hardware error!\n"));

		raid_dev->hw_error = 1;

		rval = FAILED;
		goto out;
	}
	else {
		con_log(CL_ANN, (KERN_NOTICE
		"megaraid mbox: reset sequence completed successfully\n"));
	}


	
	if (!adapter->ha) {
		rval = SUCCESS;
		goto out;
	}

	
	raw_mbox[0] = CLUSTER_CMD;
	raw_mbox[2] = RESET_RESERVATIONS;

	rval = SUCCESS;
	if (mbox_post_sync_cmd_fast(adapter, raw_mbox) == 0) {
		con_log(CL_ANN,
			(KERN_INFO "megaraid: reservation reset\n"));
	}
	else {
		rval = FAILED;
		con_log(CL_ANN, (KERN_WARNING
				"megaraid: reservation reset failed\n"));
	}

 out:
	spin_unlock_irq(&adapter->lock);
	return rval;
}


static int
mbox_post_sync_cmd(adapter_t *adapter, uint8_t raw_mbox[])
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mbox64_t	*mbox64;
	mbox_t		*mbox;
	uint8_t		status;
	int		i;


	mbox64	= raid_dev->mbox64;
	mbox	= raid_dev->mbox;

	if (megaraid_busywait_mbox(raid_dev) != 0)
		goto blocked_mailbox;

	memcpy((caddr_t)mbox, (caddr_t)raw_mbox, 16);
	mbox->cmdid		= 0xFE;
	mbox->busy		= 1;
	mbox->poll		= 0;
	mbox->ack		= 0;
	mbox->numstatus		= 0xFF;
	mbox->status		= 0xFF;

	wmb();
	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x1);

	
	
	
	if (mbox->numstatus == 0xFF) {	
		udelay(25);

		for (i = 0; mbox->numstatus == 0xFF && i < 1000; i++) {
			rmb();
			msleep(1);
		}


		if (i == 1000) {
			con_log(CL_ANN, (KERN_NOTICE
				"megaraid mailbox: wait for FW to boot      "));

			for (i = 0; (mbox->numstatus == 0xFF) &&
					(i < MBOX_RESET_WAIT); i++) {
				rmb();
				con_log(CL_ANN, ("\b\b\b\b\b[%03d]",
							MBOX_RESET_WAIT - i));
				msleep(1000);
			}

			if (i == MBOX_RESET_WAIT) {

				con_log(CL_ANN, (
				"\nmegaraid mailbox: status not available\n"));

				return -1;
			}
			con_log(CL_ANN, ("\b\b\b\b\b[ok] \n"));
		}
	}

	
	if (mbox->poll != 0x77) {
		udelay(25);

		for (i = 0; (mbox->poll != 0x77) && (i < 1000); i++) {
			rmb();
			msleep(1);
		}

		if (i == 1000) {
			con_log(CL_ANN, (KERN_WARNING
			"megaraid mailbox: could not get poll semaphore\n"));
			return -1;
		}
	}

	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x2);
	wmb();

	
	if (RDINDOOR(raid_dev) & 0x2) {
		udelay(25);

		for (i = 0; (RDINDOOR(raid_dev) & 0x2) && (i < 1000); i++) {
			rmb();
			msleep(1);
		}

		if (i == 1000) {
			con_log(CL_ANN, (KERN_WARNING
				"megaraid mailbox: could not acknowledge\n"));
			return -1;
		}
	}
	mbox->poll	= 0;
	mbox->ack	= 0x77;

	status = mbox->status;

	
	
	mbox->numstatus	= 0xFF;
	mbox->status	= 0xFF;
	for (i = 0; i < MBOX_MAX_FIRMWARE_STATUS; i++) {
		mbox->completed[i] = 0xFF;
	}

	return status;

blocked_mailbox:

	con_log(CL_ANN, (KERN_WARNING "megaraid: blocked mailbox\n") );
	return -1;
}


static int
mbox_post_sync_cmd_fast(adapter_t *adapter, uint8_t raw_mbox[])
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mbox_t		*mbox;
	long		i;


	mbox	= raid_dev->mbox;

	
	if (mbox->busy) return -1;

	
	memcpy((caddr_t)mbox, (caddr_t)raw_mbox, 14);
	mbox->cmdid		= 0xFE;
	mbox->busy		= 1;
	mbox->poll		= 0;
	mbox->ack		= 0;
	mbox->numstatus		= 0xFF;
	mbox->status		= 0xFF;

	wmb();
	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x1);

	for (i = 0; i < MBOX_SYNC_WAIT_CNT; i++) {
		if (mbox->numstatus != 0xFF) break;
		rmb();
		udelay(MBOX_SYNC_DELAY_200);
	}

	if (i == MBOX_SYNC_WAIT_CNT) {
		
		con_log(CL_ANN, (KERN_CRIT
			"megaraid: fast sync command timed out\n"));
	}

	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x2);
	wmb();

	return mbox->status;
}


static int
megaraid_busywait_mbox(mraid_device_t *raid_dev)
{
	mbox_t	*mbox = raid_dev->mbox;
	int	i = 0;

	if (mbox->busy) {
		udelay(25);
		for (i = 0; mbox->busy && i < 1000; i++)
			msleep(1);
	}

	if (i < 1000) return 0;
	else return -1;
}


static int
megaraid_mbox_product_info(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	mbox_t			*mbox;
	uint8_t			raw_mbox[sizeof(mbox_t)];
	mraid_pinfo_t		*pinfo;
	dma_addr_t		pinfo_dma_h;
	mraid_inquiry3_t	*mraid_inq3;
	int			i;


	memset((caddr_t)raw_mbox, 0, sizeof(raw_mbox));
	mbox = (mbox_t *)raw_mbox;

	pinfo = pci_alloc_consistent(adapter->pdev, sizeof(mraid_pinfo_t),
			&pinfo_dma_h);

	if (pinfo == NULL) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));

		return -1;
	}
	memset(pinfo, 0, sizeof(mraid_pinfo_t));

	mbox->xferaddr = (uint32_t)adapter->ibuf_dma_h;
	memset((void *)adapter->ibuf, 0, MBOX_IBUF_SIZE);

	raw_mbox[0] = FC_NEW_CONFIG;
	raw_mbox[2] = NC_SUBOP_ENQUIRY3;
	raw_mbox[3] = ENQ3_GET_SOLICITED_FULL;

	
	if (mbox_post_sync_cmd(adapter, raw_mbox) != 0) {

		con_log(CL_ANN, (KERN_WARNING "megaraid: Inquiry3 failed\n"));

		pci_free_consistent(adapter->pdev, sizeof(mraid_pinfo_t),
			pinfo, pinfo_dma_h);

		return -1;
	}

	mraid_inq3 = (mraid_inquiry3_t *)adapter->ibuf;
	for (i = 0; i < MBOX_MAX_PHYSICAL_DRIVES; i++) {
		raid_dev->pdrv_state[i] = mraid_inq3->pdrv_state[i];
	}

	memset((caddr_t)raw_mbox, 0, sizeof(raw_mbox));
	mbox->xferaddr = (uint32_t)pinfo_dma_h;

	raw_mbox[0] = FC_NEW_CONFIG;
	raw_mbox[2] = NC_SUBOP_PRODUCT_INFO;

	if (mbox_post_sync_cmd(adapter, raw_mbox) != 0) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: product info failed\n"));

		pci_free_consistent(adapter->pdev, sizeof(mraid_pinfo_t),
			pinfo, pinfo_dma_h);

		return -1;
	}

	adapter->max_channel = pinfo->nchannels;

	adapter->max_target	= MAX_LOGICAL_DRIVES_40LD + 1;
	adapter->max_lun	= 8;	

	adapter->max_cmds	= MBOX_MAX_SCSI_CMDS;

	memset(adapter->fw_version, 0, VERSION_SIZE);
	memset(adapter->bios_version, 0, VERSION_SIZE);

	memcpy(adapter->fw_version, pinfo->fw_version, 4);
	adapter->fw_version[4] = 0;

	memcpy(adapter->bios_version, pinfo->bios_version, 4);
	adapter->bios_version[4] = 0;

	con_log(CL_ANN, (KERN_NOTICE
		"megaraid: fw version:[%s] bios version:[%s]\n",
		adapter->fw_version, adapter->bios_version));

	pci_free_consistent(adapter->pdev, sizeof(mraid_pinfo_t), pinfo,
			pinfo_dma_h);

	return 0;
}



static int
megaraid_mbox_extended_cdb(adapter_t *adapter)
{
	mbox_t		*mbox;
	uint8_t		raw_mbox[sizeof(mbox_t)];
	int		rval;

	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(raw_mbox));
	mbox->xferaddr	= (uint32_t)adapter->ibuf_dma_h;

	memset((void *)adapter->ibuf, 0, MBOX_IBUF_SIZE);

	raw_mbox[0] = MAIN_MISC_OPCODE;
	raw_mbox[2] = SUPPORT_EXT_CDB;

	rval = 0;
	if (mbox_post_sync_cmd(adapter, raw_mbox) != 0) {
		rval = -1;
	}

	return rval;
}


static int
megaraid_mbox_support_ha(adapter_t *adapter, uint16_t *init_id)
{
	mbox_t		*mbox;
	uint8_t		raw_mbox[sizeof(mbox_t)];
	int		rval;


	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(raw_mbox));

	mbox->xferaddr = (uint32_t)adapter->ibuf_dma_h;

	memset((void *)adapter->ibuf, 0, MBOX_IBUF_SIZE);

	raw_mbox[0] = GET_TARGET_ID;

	
	*init_id = 7;
	rval =  -1;
	if (mbox_post_sync_cmd(adapter, raw_mbox) == 0) {

		*init_id = *(uint8_t *)adapter->ibuf;

		con_log(CL_ANN, (KERN_INFO
			"megaraid: cluster firmware, initiator ID: %d\n",
			*init_id));

		rval =  0;
	}

	return rval;
}


static int
megaraid_mbox_support_random_del(adapter_t *adapter)
{
	mbox_t		*mbox;
	uint8_t		raw_mbox[sizeof(mbox_t)];
	int		rval;

	if (adapter->pdev->vendor == PCI_VENDOR_ID_AMI &&
	    adapter->pdev->device == PCI_DEVICE_ID_AMI_MEGARAID3 &&
	    adapter->pdev->subsystem_vendor == PCI_VENDOR_ID_DELL &&
	    adapter->pdev->subsystem_device == PCI_SUBSYS_ID_CERC_ATA100_4CH &&
	    (adapter->fw_version[0] > '6' ||
	     (adapter->fw_version[0] == '6' &&
	      adapter->fw_version[2] > '6') ||
	     (adapter->fw_version[0] == '6'
	      && adapter->fw_version[2] == '6'
	      && adapter->fw_version[3] > '1'))) {
		con_log(CL_DLEVEL1, ("megaraid: disable random deletion\n"));
		return 0;
	}

	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(mbox_t));

	raw_mbox[0] = FC_DEL_LOGDRV;
	raw_mbox[2] = OP_SUP_DEL_LOGDRV;

	
	rval = 0;
	if (mbox_post_sync_cmd(adapter, raw_mbox) == 0) {

		con_log(CL_DLEVEL1, ("megaraid: supports random deletion\n"));

		rval =  1;
	}

	return rval;
}


static int
megaraid_mbox_get_max_sg(adapter_t *adapter)
{
	mbox_t		*mbox;
	uint8_t		raw_mbox[sizeof(mbox_t)];
	int		nsg;


	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(mbox_t));

	mbox->xferaddr = (uint32_t)adapter->ibuf_dma_h;

	memset((void *)adapter->ibuf, 0, MBOX_IBUF_SIZE);

	raw_mbox[0] = MAIN_MISC_OPCODE;
	raw_mbox[2] = GET_MAX_SG_SUPPORT;

	
	if (mbox_post_sync_cmd(adapter, raw_mbox) == 0) {
		nsg =  *(uint8_t *)adapter->ibuf;
	}
	else {
		nsg =  MBOX_DEFAULT_SG_SIZE;
	}

	if (nsg > MBOX_MAX_SG_SIZE) nsg = MBOX_MAX_SG_SIZE;

	return nsg;
}


static void
megaraid_mbox_enum_raid_scsi(adapter_t *adapter)
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mbox_t		*mbox;
	uint8_t		raw_mbox[sizeof(mbox_t)];


	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(mbox_t));

	mbox->xferaddr = (uint32_t)adapter->ibuf_dma_h;

	memset((void *)adapter->ibuf, 0, MBOX_IBUF_SIZE);

	raw_mbox[0] = CHNL_CLASS;
	raw_mbox[2] = GET_CHNL_CLASS;

	
	
	raid_dev->channel_class = 0xFF;
	if (mbox_post_sync_cmd(adapter, raw_mbox) == 0) {
		raid_dev->channel_class =  *(uint8_t *)adapter->ibuf;
	}

	return;
}


static void
megaraid_mbox_flush_cache(adapter_t *adapter)
{
	mbox_t	*mbox;
	uint8_t	raw_mbox[sizeof(mbox_t)];


	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(mbox_t));

	raw_mbox[0] = FLUSH_ADAPTER;

	if (mbox_post_sync_cmd(adapter, raw_mbox) != 0) {
		con_log(CL_ANN, ("megaraid: flush adapter failed\n"));
	}

	raw_mbox[0] = FLUSH_SYSTEM;

	if (mbox_post_sync_cmd(adapter, raw_mbox) != 0) {
		con_log(CL_ANN, ("megaraid: flush disks cache failed\n"));
	}

	return;
}


static int
megaraid_mbox_fire_sync_cmd(adapter_t *adapter)
{
	mbox_t	*mbox;
	uint8_t	raw_mbox[sizeof(mbox_t)];
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mbox64_t *mbox64;
	int	status = 0;
	int i;
	uint32_t dword;

	mbox = (mbox_t *)raw_mbox;

	memset((caddr_t)raw_mbox, 0, sizeof(mbox_t));

	raw_mbox[0] = 0xFF;

	mbox64	= raid_dev->mbox64;
	mbox	= raid_dev->mbox;

	
	if (megaraid_busywait_mbox(raid_dev) != 0) {
		status = 1;
		goto blocked_mailbox;
	}

	
	memcpy((caddr_t)mbox, (caddr_t)raw_mbox, 16);
	mbox->cmdid		= 0xFE;
	mbox->busy		= 1;
	mbox->poll		= 0;
	mbox->ack		= 0;
	mbox->numstatus		= 0;
	mbox->status		= 0;

	wmb();
	WRINDOOR(raid_dev, raid_dev->mbox_dma | 0x1);


	i = 0;
	status = 0;
	while (!mbox->numstatus && mbox->cmd == 0xFF) {
		rmb();
		msleep(1);
		i++;
		if (i > 1000 * 60) {
			status = 1;
			break;
		}
	}
	if (mbox->numstatus == 1)
		status = 1; 

	
	dword = RDOUTDOOR(raid_dev);
	WROUTDOOR(raid_dev, dword);
	WRINDOOR(raid_dev,2);

	return status;

blocked_mailbox:
	con_log(CL_ANN, (KERN_WARNING "megaraid: blocked mailbox\n"));
	return status;
}

static void
megaraid_mbox_display_scb(adapter_t *adapter, scb_t *scb)
{
	mbox_ccb_t		*ccb;
	struct scsi_cmnd	*scp;
	mbox_t			*mbox;
	int			level;
	int			i;


	ccb	= (mbox_ccb_t *)scb->ccb;
	scp	= scb->scp;
	mbox	= ccb->mbox;

	level = CL_DLEVEL3;

	con_log(level, (KERN_NOTICE
		"megaraid mailbox: status:%#x cmd:%#x id:%#x ", scb->status,
		mbox->cmd, scb->sno));

	con_log(level, ("sec:%#x lba:%#x addr:%#x ld:%d sg:%d\n",
		mbox->numsectors, mbox->lba, mbox->xferaddr, mbox->logdrv,
		mbox->numsge));

	if (!scp) return;

	con_log(level, (KERN_NOTICE "scsi cmnd: "));

	for (i = 0; i < scp->cmd_len; i++) {
		con_log(level, ("%#2.02x ", scp->cmnd[i]));
	}

	con_log(level, ("\n"));

	return;
}


static void
megaraid_mbox_setup_device_map(adapter_t *adapter)
{
	uint8_t		c;
	uint8_t		t;

	for (t = 0; t < LSI_MAX_LOGICAL_DRIVES_64LD; t++)
		adapter->device_ids[adapter->max_channel][t] =
			(t < adapter->init_id) ?  t : t - 1;

	adapter->device_ids[adapter->max_channel][adapter->init_id] = 0xFF;

	for (c = 0; c < adapter->max_channel; c++)
		for (t = 0; t < LSI_MAX_LOGICAL_DRIVES_64LD; t++)
			adapter->device_ids[c][t] = (c << 8) | t;
}




static int
megaraid_cmm_register(adapter_t *adapter)
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	mraid_mmadp_t	adp;
	scb_t		*scb;
	mbox_ccb_t	*ccb;
	int		rval;
	int		i;

	
	adapter->uscb_list = kcalloc(MBOX_MAX_USER_CMDS, sizeof(scb_t), GFP_KERNEL);

	if (adapter->uscb_list == NULL) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));
		return -1;
	}


	
	
	INIT_LIST_HEAD(&adapter->uscb_pool);

	spin_lock_init(USER_FREE_LIST_LOCK(adapter));



	
	
	
	
	for (i = 0; i < MBOX_MAX_USER_CMDS; i++) {

		scb			= adapter->uscb_list + i;
		ccb			= raid_dev->uccb_list + i;

		scb->ccb		= (caddr_t)ccb;
		ccb->mbox64		= raid_dev->umbox64 + i;
		ccb->mbox		= &ccb->mbox64->mbox32;
		ccb->raw_mbox		= (uint8_t *)ccb->mbox;

		scb->gp			= 0;

		
		
		scb->sno		= i + MBOX_MAX_SCSI_CMDS;

		scb->scp		= NULL;
		scb->state		= SCB_FREE;
		scb->dma_direction	= PCI_DMA_NONE;
		scb->dma_type		= MRAID_DMA_NONE;
		scb->dev_channel	= -1;
		scb->dev_target		= -1;

		
		list_add_tail(&scb->list, &adapter->uscb_pool);
	}

	adp.unique_id		= adapter->unique_id;
	adp.drvr_type		= DRVRTYPE_MBOX;
	adp.drvr_data		= (unsigned long)adapter;
	adp.pdev		= adapter->pdev;
	adp.issue_uioc		= megaraid_mbox_mm_handler;
	adp.timeout		= MBOX_RESET_WAIT + MBOX_RESET_EXT_WAIT;
	adp.max_kioc		= MBOX_MAX_USER_CMDS;

	if ((rval = mraid_mm_register_adp(&adp)) != 0) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid mbox: did not register with CMM\n"));

		kfree(adapter->uscb_list);
	}

	return rval;
}


static int
megaraid_cmm_unregister(adapter_t *adapter)
{
	kfree(adapter->uscb_list);
	mraid_mm_unregister_adp(adapter->unique_id);
	return 0;
}


static int
megaraid_mbox_mm_handler(unsigned long drvr_data, uioc_t *kioc, uint32_t action)
{
	adapter_t *adapter;

	if (action != IOCTL_ISSUE) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: unsupported management action:%#2x\n",
			action));
		return (-ENOTSUPP);
	}

	adapter = (adapter_t *)drvr_data;

	
	if (atomic_read(&adapter->being_detached)) {
		con_log(CL_ANN, (KERN_WARNING
			"megaraid: reject management request, detaching\n"));
		return (-ENODEV);
	}

	switch (kioc->opcode) {

	case GET_ADAP_INFO:

		kioc->status =  gather_hbainfo(adapter, (mraid_hba_info_t *)
					(unsigned long)kioc->buf_vaddr);

		kioc->done(kioc);

		return kioc->status;

	case MBOX_CMD:

		return megaraid_mbox_mm_command(adapter, kioc);

	default:
		kioc->status = (-EINVAL);
		kioc->done(kioc);
		return (-EINVAL);
	}

	return 0;	
}

static int
megaraid_mbox_mm_command(adapter_t *adapter, uioc_t *kioc)
{
	struct list_head	*head = &adapter->uscb_pool;
	mbox64_t		*mbox64;
	uint8_t			*raw_mbox;
	scb_t			*scb;
	mbox_ccb_t		*ccb;
	unsigned long		flags;

	
	spin_lock_irqsave(USER_FREE_LIST_LOCK(adapter), flags);

	if (list_empty(head)) {	

		con_log(CL_ANN, (KERN_WARNING
			"megaraid mbox: bug in cmm handler, lost resources\n"));

		spin_unlock_irqrestore(USER_FREE_LIST_LOCK(adapter), flags);

		return (-EINVAL);
	}

	scb = list_entry(head->next, scb_t, list);
	list_del_init(&scb->list);

	spin_unlock_irqrestore(USER_FREE_LIST_LOCK(adapter), flags);

	scb->state		= SCB_ACTIVE;
	scb->dma_type		= MRAID_DMA_NONE;
	scb->dma_direction	= PCI_DMA_NONE;

	ccb		= (mbox_ccb_t *)scb->ccb;
	mbox64		= (mbox64_t *)(unsigned long)kioc->cmdbuf;
	raw_mbox	= (uint8_t *)&mbox64->mbox32;

	memcpy(ccb->mbox64, mbox64, sizeof(mbox64_t));

	scb->gp		= (unsigned long)kioc;

	if (raw_mbox[0] == FC_DEL_LOGDRV && raw_mbox[2] == OP_DEL_LOGDRV) {

		if (wait_till_fw_empty(adapter)) {
			con_log(CL_ANN, (KERN_NOTICE
				"megaraid mbox: LD delete, timed out\n"));

			kioc->status = -ETIME;

			scb->status = -1;

			megaraid_mbox_mm_done(adapter, scb);

			return (-ETIME);
		}

		INIT_LIST_HEAD(&scb->list);

		scb->state = SCB_ISSUED;
		if (mbox_post_cmd(adapter, scb) != 0) {

			con_log(CL_ANN, (KERN_NOTICE
				"megaraid mbox: LD delete, mailbox busy\n"));

			kioc->status = -EBUSY;

			scb->status = -1;

			megaraid_mbox_mm_done(adapter, scb);

			return (-EBUSY);
		}

		return 0;
	}

	
	megaraid_mbox_runpendq(adapter, scb);

	return 0;
}


static int
wait_till_fw_empty(adapter_t *adapter)
{
	unsigned long	flags = 0;
	int		i;


	spin_lock_irqsave(&adapter->lock, flags);
	adapter->quiescent++;
	spin_unlock_irqrestore(&adapter->lock, flags);

	for (i = 0; i < 60 && adapter->outstanding_cmds; i++) {
		con_log(CL_DLEVEL1, (KERN_INFO
			"megaraid: FW has %d pending commands\n",
			adapter->outstanding_cmds));

		msleep(1000);
	}

	return adapter->outstanding_cmds;
}


static void
megaraid_mbox_mm_done(adapter_t *adapter, scb_t *scb)
{
	uioc_t			*kioc;
	mbox64_t		*mbox64;
	uint8_t			*raw_mbox;
	unsigned long		flags;

	kioc			= (uioc_t *)scb->gp;
	mbox64			= (mbox64_t *)(unsigned long)kioc->cmdbuf;
	mbox64->mbox32.status	= scb->status;
	raw_mbox		= (uint8_t *)&mbox64->mbox32;


	
	scb->state	= SCB_FREE;
	scb->scp	= NULL;

	spin_lock_irqsave(USER_FREE_LIST_LOCK(adapter), flags);

	list_add(&scb->list, &adapter->uscb_pool);

	spin_unlock_irqrestore(USER_FREE_LIST_LOCK(adapter), flags);

	
	
	if (raw_mbox[0] == FC_DEL_LOGDRV && raw_mbox[2] == OP_DEL_LOGDRV) {

		adapter->quiescent--;

		megaraid_mbox_runpendq(adapter, NULL);
	}

	kioc->done(kioc);

	return;
}


static int
gather_hbainfo(adapter_t *adapter, mraid_hba_info_t *hinfo)
{
	uint8_t	dmajor;

	dmajor			= megaraid_mbox_version[0];

	hinfo->pci_vendor_id	= adapter->pdev->vendor;
	hinfo->pci_device_id	= adapter->pdev->device;
	hinfo->subsys_vendor_id	= adapter->pdev->subsystem_vendor;
	hinfo->subsys_device_id	= adapter->pdev->subsystem_device;

	hinfo->pci_bus		= adapter->pdev->bus->number;
	hinfo->pci_dev_fn	= adapter->pdev->devfn;
	hinfo->pci_slot		= PCI_SLOT(adapter->pdev->devfn);
	hinfo->irq		= adapter->host->irq;
	hinfo->baseport		= ADAP2RAIDDEV(adapter)->baseport;

	hinfo->unique_id	= (hinfo->pci_bus << 8) | adapter->pdev->devfn;
	hinfo->host_no		= adapter->host->host_no;

	return 0;
}




static int
megaraid_sysfs_alloc_resources(adapter_t *adapter)
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	int		rval = 0;

	raid_dev->sysfs_uioc = kmalloc(sizeof(uioc_t), GFP_KERNEL);

	raid_dev->sysfs_mbox64 = kmalloc(sizeof(mbox64_t), GFP_KERNEL);

	raid_dev->sysfs_buffer = pci_alloc_consistent(adapter->pdev,
			PAGE_SIZE, &raid_dev->sysfs_buffer_dma);

	if (!raid_dev->sysfs_uioc || !raid_dev->sysfs_mbox64 ||
		!raid_dev->sysfs_buffer) {

		con_log(CL_ANN, (KERN_WARNING
			"megaraid: out of memory, %s %d\n", __func__,
			__LINE__));

		rval = -ENOMEM;

		megaraid_sysfs_free_resources(adapter);
	}

	mutex_init(&raid_dev->sysfs_mtx);

	init_waitqueue_head(&raid_dev->sysfs_wait_q);

	return rval;
}


static void
megaraid_sysfs_free_resources(adapter_t *adapter)
{
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);

	kfree(raid_dev->sysfs_uioc);
	kfree(raid_dev->sysfs_mbox64);

	if (raid_dev->sysfs_buffer) {
		pci_free_consistent(adapter->pdev, PAGE_SIZE,
			raid_dev->sysfs_buffer, raid_dev->sysfs_buffer_dma);
	}
}


static void
megaraid_sysfs_get_ldmap_done(uioc_t *uioc)
{
	adapter_t	*adapter = (adapter_t *)uioc->buf_vaddr;
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);

	uioc->status = 0;

	wake_up(&raid_dev->sysfs_wait_q);
}


static void
megaraid_sysfs_get_ldmap_timeout(unsigned long data)
{
	uioc_t		*uioc = (uioc_t *)data;
	adapter_t	*adapter = (adapter_t *)uioc->buf_vaddr;
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);

	uioc->status = -ETIME;

	wake_up(&raid_dev->sysfs_wait_q);
}


static int
megaraid_sysfs_get_ldmap(adapter_t *adapter)
{
	mraid_device_t		*raid_dev = ADAP2RAIDDEV(adapter);
	uioc_t			*uioc;
	mbox64_t		*mbox64;
	mbox_t			*mbox;
	char			*raw_mbox;
	struct timer_list	sysfs_timer;
	struct timer_list	*timerp;
	caddr_t			ldmap;
	int			rval = 0;

	mutex_lock(&raid_dev->sysfs_mtx);

	uioc	= raid_dev->sysfs_uioc;
	mbox64	= raid_dev->sysfs_mbox64;
	ldmap	= raid_dev->sysfs_buffer;

	memset(uioc, 0, sizeof(uioc_t));
	memset(mbox64, 0, sizeof(mbox64_t));
	memset(ldmap, 0, sizeof(raid_dev->curr_ldmap));

	mbox		= &mbox64->mbox32;
	raw_mbox	= (char *)mbox;
	uioc->cmdbuf    = (uint64_t)(unsigned long)mbox64;
	uioc->buf_vaddr	= (caddr_t)adapter;
	uioc->status	= -ENODATA;
	uioc->done	= megaraid_sysfs_get_ldmap_done;

	mbox->xferaddr = (uint32_t)raid_dev->sysfs_buffer_dma;

	raw_mbox[0] = FC_DEL_LOGDRV;
	raw_mbox[2] = OP_GET_LDID_MAP;

	timerp	= &sysfs_timer;
	init_timer(timerp);

	timerp->function	= megaraid_sysfs_get_ldmap_timeout;
	timerp->data		= (unsigned long)uioc;
	timerp->expires		= jiffies + 60 * HZ;

	add_timer(timerp);

	rval = megaraid_mbox_mm_command(adapter, uioc);

	if (rval == 0) {	
		wait_event(raid_dev->sysfs_wait_q, (uioc->status != -ENODATA));

		if (uioc->status == -ETIME) {
			con_log(CL_ANN, (KERN_NOTICE
				"megaraid: sysfs get ld map timed out\n"));

			rval = -ETIME;
		}
		else {
			rval = mbox->status;
		}

		if (rval == 0) {
			memcpy(raid_dev->curr_ldmap, ldmap,
				sizeof(raid_dev->curr_ldmap));
		}
		else {
			con_log(CL_ANN, (KERN_NOTICE
				"megaraid: get ld map failed with %x\n", rval));
		}
	}
	else {
		con_log(CL_ANN, (KERN_NOTICE
			"megaraid: could not issue ldmap command:%x\n", rval));
	}


	del_timer_sync(timerp);

	mutex_unlock(&raid_dev->sysfs_mtx);

	return rval;
}


static ssize_t
megaraid_sysfs_show_app_hndl(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	adapter_t	*adapter = (adapter_t *)SCSIHOST2ADAP(shost);
	uint32_t	app_hndl;

	app_hndl = mraid_mm_adapter_app_handle(adapter->unique_id);

	return snprintf(buf, 8, "%u\n", app_hndl);
}


static ssize_t
megaraid_sysfs_show_ldnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	adapter_t	*adapter = (adapter_t *)SCSIHOST2ADAP(sdev->host);
	mraid_device_t	*raid_dev = ADAP2RAIDDEV(adapter);
	int		scsi_id = -1;
	int		logical_drv = -1;
	int		ldid_map = -1;
	uint32_t	app_hndl = 0;
	int		mapped_sdev_id;
	int		rval;
	int		i;

	if (raid_dev->random_del_supported &&
			MRAID_IS_LOGICAL_SDEV(adapter, sdev)) {

		rval = megaraid_sysfs_get_ldmap(adapter);
		if (rval == 0) {

			for (i = 0; i < MAX_LOGICAL_DRIVES_40LD; i++) {

				mapped_sdev_id = sdev->id;

				if (sdev->id > adapter->init_id) {
					mapped_sdev_id -= 1;
				}

				if (raid_dev->curr_ldmap[i] == mapped_sdev_id) {

					scsi_id = sdev->id;

					logical_drv = i;

					ldid_map = raid_dev->curr_ldmap[i];

					app_hndl = mraid_mm_adapter_app_handle(
							adapter->unique_id);

					break;
				}
			}
		}
		else {
			con_log(CL_ANN, (KERN_NOTICE
				"megaraid: sysfs get ld map failed: %x\n",
				rval));
		}
	}

	return snprintf(buf, 36, "%d %d %d %d\n", scsi_id, logical_drv,
			ldid_map, app_hndl);
}


module_init(megaraid_init);
module_exit(megaraid_exit);

