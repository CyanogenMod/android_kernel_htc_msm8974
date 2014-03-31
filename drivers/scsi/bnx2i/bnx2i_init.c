/* bnx2i.c: Broadcom NetXtreme II iSCSI driver.
 *
 * Copyright (c) 2006 - 2011 Broadcom Corporation
 * Copyright (c) 2007, 2008 Red Hat, Inc.  All rights reserved.
 * Copyright (c) 2007, 2008 Mike Christie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Written by: Anil Veerabhadrappa (anilgv@broadcom.com)
 * Maintained by: Eddie Wai (eddie.wai@broadcom.com)
 */

#include "bnx2i.h"

static struct list_head adapter_list = LIST_HEAD_INIT(adapter_list);
static u32 adapter_count;

#define DRV_MODULE_NAME		"bnx2i"
#define DRV_MODULE_VERSION	"2.7.0.3"
#define DRV_MODULE_RELDATE	"Jun 15, 2011"

static char version[] __devinitdata =
		"Broadcom NetXtreme II iSCSI Driver " DRV_MODULE_NAME \
		" v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";


MODULE_AUTHOR("Anil Veerabhadrappa <anilgv@broadcom.com> and "
	      "Eddie Wai <eddie.wai@broadcom.com>");

MODULE_DESCRIPTION("Broadcom NetXtreme II BCM5706/5708/5709/57710/57711/57712"
		   "/57800/57810/57840 iSCSI Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

static DEFINE_MUTEX(bnx2i_dev_lock);

unsigned int event_coal_min = 24;
module_param(event_coal_min, int, 0664);
MODULE_PARM_DESC(event_coal_min, "Event Coalescing Minimum Commands");

unsigned int event_coal_div = 2;
module_param(event_coal_div, int, 0664);
MODULE_PARM_DESC(event_coal_div, "Event Coalescing Divide Factor");

unsigned int en_tcp_dack = 1;
module_param(en_tcp_dack, int, 0664);
MODULE_PARM_DESC(en_tcp_dack, "Enable TCP Delayed ACK");

unsigned int error_mask1 = 0x00;
module_param(error_mask1, uint, 0664);
MODULE_PARM_DESC(error_mask1, "Config FW iSCSI Error Mask #1");

unsigned int error_mask2 = 0x00;
module_param(error_mask2, uint, 0664);
MODULE_PARM_DESC(error_mask2, "Config FW iSCSI Error Mask #2");

unsigned int sq_size;
module_param(sq_size, int, 0664);
MODULE_PARM_DESC(sq_size, "Configure SQ size");

unsigned int rq_size = BNX2I_RQ_WQES_DEFAULT;
module_param(rq_size, int, 0664);
MODULE_PARM_DESC(rq_size, "Configure RQ size");

u64 iscsi_error_mask = 0x00;

DEFINE_PER_CPU(struct bnx2i_percpu_s, bnx2i_percpu);

static int bnx2i_cpu_callback(struct notifier_block *nfb,
			      unsigned long action, void *hcpu);
static struct notifier_block bnx2i_cpu_notifier = {
	.notifier_call = bnx2i_cpu_callback,
};


void bnx2i_identify_device(struct bnx2i_hba *hba)
{
	hba->cnic_dev_type = 0;
	if ((hba->pci_did == PCI_DEVICE_ID_NX2_5706) ||
	    (hba->pci_did == PCI_DEVICE_ID_NX2_5706S))
		set_bit(BNX2I_NX2_DEV_5706, &hba->cnic_dev_type);
	else if ((hba->pci_did == PCI_DEVICE_ID_NX2_5708) ||
	    (hba->pci_did == PCI_DEVICE_ID_NX2_5708S))
		set_bit(BNX2I_NX2_DEV_5708, &hba->cnic_dev_type);
	else if ((hba->pci_did == PCI_DEVICE_ID_NX2_5709) ||
	    (hba->pci_did == PCI_DEVICE_ID_NX2_5709S)) {
		set_bit(BNX2I_NX2_DEV_5709, &hba->cnic_dev_type);
		hba->mail_queue_access = BNX2I_MQ_BIN_MODE;
	} else if (hba->pci_did == PCI_DEVICE_ID_NX2_57710    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57711    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57711E   ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57712    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57712E   ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57800    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57800_MF ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57800_VF ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57810    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57810_MF ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57810_VF ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57840    ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57840_MF ||
		   hba->pci_did == PCI_DEVICE_ID_NX2_57840_VF)
		set_bit(BNX2I_NX2_DEV_57710, &hba->cnic_dev_type);
	else
		printk(KERN_ALERT "bnx2i: unknown device, 0x%x\n",
				  hba->pci_did);
}


struct bnx2i_hba *get_adapter_list_head(void)
{
	struct bnx2i_hba *hba = NULL;
	struct bnx2i_hba *tmp_hba;

	if (!adapter_count)
		goto hba_not_found;

	mutex_lock(&bnx2i_dev_lock);
	list_for_each_entry(tmp_hba, &adapter_list, link) {
		if (tmp_hba->cnic && tmp_hba->cnic->cm_select_dev) {
			hba = tmp_hba;
			break;
		}
	}
	mutex_unlock(&bnx2i_dev_lock);
hba_not_found:
	return hba;
}


struct bnx2i_hba *bnx2i_find_hba_for_cnic(struct cnic_dev *cnic)
{
	struct bnx2i_hba *hba, *temp;

	mutex_lock(&bnx2i_dev_lock);
	list_for_each_entry_safe(hba, temp, &adapter_list, link) {
		if (hba->cnic == cnic) {
			mutex_unlock(&bnx2i_dev_lock);
			return hba;
		}
	}
	mutex_unlock(&bnx2i_dev_lock);
	return NULL;
}


void bnx2i_start(void *handle)
{
#define BNX2I_INIT_POLL_TIME	(1000 / HZ)
	struct bnx2i_hba *hba = handle;
	int i = HZ;

	BUG_ON(!hba->cnic->max_iscsi_conn);

	bnx2i_send_fw_iscsi_init_msg(hba);
	while (!test_bit(ADAPTER_STATE_UP, &hba->adapter_state) && i--)
		msleep(BNX2I_INIT_POLL_TIME);
}


static void bnx2i_chip_cleanup(struct bnx2i_hba *hba)
{
	struct bnx2i_endpoint *bnx2i_ep;
	struct list_head *pos, *tmp;

	if (hba->ofld_conns_active) {
		printk(KERN_ALERT "bnx2i: (%s) chip cleanup for %d active "
			"connections\n", hba->netdev->name,
			hba->ofld_conns_active);
		mutex_lock(&hba->net_dev_lock);
		list_for_each_safe(pos, tmp, &hba->ep_active_list) {
			bnx2i_ep = list_entry(pos, struct bnx2i_endpoint, link);
			
			bnx2i_hw_ep_disconnect(bnx2i_ep);
			bnx2i_ep->cm_sk = NULL;
		}
		mutex_unlock(&hba->net_dev_lock);
	}
}


void bnx2i_stop(void *handle)
{
	struct bnx2i_hba *hba = handle;
	int conns_active;
	int wait_delay = 1 * HZ;

	
	if (!test_and_set_bit(ADAPTER_STATE_GOING_DOWN,
			      &hba->adapter_state)) {
		iscsi_host_for_each_session(hba->shost,
					    bnx2i_drop_session);
		wait_delay = hba->hba_shutdown_tmo;
	}
	wait_event_interruptible_timeout(hba->eh_wait,
					 (list_empty(&hba->ep_ofld_list) &&
					 list_empty(&hba->ep_destroy_list)),
					 2 * HZ);
	while (hba->ofld_conns_active) {
		conns_active = hba->ofld_conns_active;
		wait_event_interruptible_timeout(hba->eh_wait,
				(hba->ofld_conns_active != conns_active),
				wait_delay);
		if (hba->ofld_conns_active == conns_active)
			break;
	}
	bnx2i_chip_cleanup(hba);

	clear_bit(ADAPTER_STATE_GOING_DOWN, &hba->adapter_state);
	clear_bit(ADAPTER_STATE_UP, &hba->adapter_state);
}


static int bnx2i_init_one(struct bnx2i_hba *hba, struct cnic_dev *cnic)
{
	int rc;

	mutex_lock(&bnx2i_dev_lock);
	if (!cnic->max_iscsi_conn) {
		printk(KERN_ALERT "bnx2i: dev %s does not support "
			"iSCSI\n", hba->netdev->name);
		rc = -EOPNOTSUPP;
		goto out;
	}

	hba->cnic = cnic;
	rc = cnic->register_device(cnic, CNIC_ULP_ISCSI, hba);
	if (!rc) {
		hba->age++;
		set_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
		list_add_tail(&hba->link, &adapter_list);
		adapter_count++;
	} else if (rc == -EBUSY) 	
		printk(KERN_ALERT "bnx2i, duplicate registration"
				  "hba=%p, cnic=%p\n", hba, cnic);
	else if (rc == -EAGAIN)
		printk(KERN_ERR "bnx2i, driver not registered\n");
	else if (rc == -EINVAL)
		printk(KERN_ERR "bnx2i, invalid type %d\n", CNIC_ULP_ISCSI);
	else
		printk(KERN_ERR "bnx2i dev reg, unknown error, %d\n", rc);

out:
	mutex_unlock(&bnx2i_dev_lock);

	return rc;
}


void bnx2i_ulp_init(struct cnic_dev *dev)
{
	struct bnx2i_hba *hba;

	
	hba = bnx2i_alloc_hba(dev);
	if (!hba) {
		printk(KERN_ERR "bnx2i init: hba initialization failed\n");
		return;
	}

	
	clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
	if (bnx2i_init_one(hba, dev)) {
		printk(KERN_ERR "bnx2i - hba %p init failed\n", hba);
		bnx2i_free_hba(hba);
	}
}


void bnx2i_ulp_exit(struct cnic_dev *dev)
{
	struct bnx2i_hba *hba;

	hba = bnx2i_find_hba_for_cnic(dev);
	if (!hba) {
		printk(KERN_INFO "bnx2i_ulp_exit: hba not "
				 "found, dev 0x%p\n", dev);
		return;
	}
	mutex_lock(&bnx2i_dev_lock);
	list_del_init(&hba->link);
	adapter_count--;

	if (test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
		hba->cnic->unregister_device(hba->cnic, CNIC_ULP_ISCSI);
		clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
	}
	mutex_unlock(&bnx2i_dev_lock);

	bnx2i_free_hba(hba);
}


static void bnx2i_percpu_thread_create(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(bnx2i_percpu, cpu);

	thread = kthread_create_on_node(bnx2i_percpu_io_thread, (void *)p,
					cpu_to_node(cpu),
					"bnx2i_thread/%d", cpu);
	
	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		p->iothread = thread;
		wake_up_process(thread);
	}
}


static void bnx2i_percpu_thread_destroy(unsigned int cpu)
{
	struct bnx2i_percpu_s *p;
	struct task_struct *thread;
	struct bnx2i_work *work, *tmp;

	
	p = &per_cpu(bnx2i_percpu, cpu);
	spin_lock_bh(&p->p_work_lock);
	thread = p->iothread;
	p->iothread = NULL;

	
	list_for_each_entry_safe(work, tmp, &p->work_list, list) {
		list_del_init(&work->list);
		bnx2i_process_scsi_cmd_resp(work->session,
					    work->bnx2i_conn, &work->cqe);
		kfree(work);
	}

	spin_unlock_bh(&p->p_work_lock);
	if (thread)
		kthread_stop(thread);
}


static int bnx2i_cpu_callback(struct notifier_block *nfb,
			      unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		printk(KERN_INFO "bnx2i: CPU %x online: Create Rx thread\n",
			cpu);
		bnx2i_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		printk(KERN_INFO "CPU %x offline: Remove Rx thread\n", cpu);
		bnx2i_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}


static int __init bnx2i_mod_init(void)
{
	int err;
	unsigned cpu = 0;
	struct bnx2i_percpu_s *p;

	printk(KERN_INFO "%s", version);

	if (sq_size && !is_power_of_2(sq_size))
		sq_size = roundup_pow_of_two(sq_size);

	mutex_init(&bnx2i_dev_lock);

	bnx2i_scsi_xport_template =
			iscsi_register_transport(&bnx2i_iscsi_transport);
	if (!bnx2i_scsi_xport_template) {
		printk(KERN_ERR "Could not register bnx2i transport.\n");
		err = -ENOMEM;
		goto out;
	}

	err = cnic_register_driver(CNIC_ULP_ISCSI, &bnx2i_cnic_cb);
	if (err) {
		printk(KERN_ERR "Could not register bnx2i cnic driver.\n");
		goto unreg_xport;
	}

	
	for_each_possible_cpu(cpu) {
		p = &per_cpu(bnx2i_percpu, cpu);
		INIT_LIST_HEAD(&p->work_list);
		spin_lock_init(&p->p_work_lock);
		p->iothread = NULL;
	}

	for_each_online_cpu(cpu)
		bnx2i_percpu_thread_create(cpu);

	
	register_hotcpu_notifier(&bnx2i_cpu_notifier);

	return 0;

unreg_xport:
	iscsi_unregister_transport(&bnx2i_iscsi_transport);
out:
	return err;
}


static void __exit bnx2i_mod_exit(void)
{
	struct bnx2i_hba *hba;
	unsigned cpu = 0;

	mutex_lock(&bnx2i_dev_lock);
	while (!list_empty(&adapter_list)) {
		hba = list_entry(adapter_list.next, struct bnx2i_hba, link);
		list_del(&hba->link);
		adapter_count--;

		if (test_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic)) {
			bnx2i_chip_cleanup(hba);
			hba->cnic->unregister_device(hba->cnic, CNIC_ULP_ISCSI);
			clear_bit(BNX2I_CNIC_REGISTERED, &hba->reg_with_cnic);
		}

		bnx2i_free_hba(hba);
	}
	mutex_unlock(&bnx2i_dev_lock);

	unregister_hotcpu_notifier(&bnx2i_cpu_notifier);

	for_each_online_cpu(cpu)
		bnx2i_percpu_thread_destroy(cpu);

	iscsi_unregister_transport(&bnx2i_iscsi_transport);
	cnic_unregister_driver(CNIC_ULP_ISCSI);
}

module_init(bnx2i_mod_init);
module_exit(bnx2i_mod_exit);
