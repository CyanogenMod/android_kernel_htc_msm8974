/*
 * Copyright(c) 2007 - 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/workqueue.h>
#include <net/dcbnl.h>
#include <net/dcbevent.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <net/rtnetlink.h>

#include <scsi/fc/fc_encaps.h>
#include <scsi/fc/fc_fip.h>

#include <scsi/libfc.h>
#include <scsi/fc_frame.h>
#include <scsi/libfcoe.h>

#include "fcoe.h"

MODULE_AUTHOR("Open-FCoE.org");
MODULE_DESCRIPTION("FCoE");
MODULE_LICENSE("GPL v2");

static unsigned int fcoe_ddp_min = 4096;
module_param_named(ddp_min, fcoe_ddp_min, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ddp_min, "Minimum I/O size in bytes for "	\
		 "Direct Data Placement (DDP).");

unsigned int fcoe_debug_logging;
module_param_named(debug_logging, fcoe_debug_logging, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug_logging, "a bit mask of logging levels");

static DEFINE_MUTEX(fcoe_config_mutex);

static struct workqueue_struct *fcoe_wq;

static DECLARE_COMPLETION(fcoe_flush_completion);

static LIST_HEAD(fcoe_hostlist);
static DEFINE_PER_CPU(struct fcoe_percpu_s, fcoe_percpu);

static int fcoe_reset(struct Scsi_Host *);
static int fcoe_xmit(struct fc_lport *, struct fc_frame *);
static int fcoe_rcv(struct sk_buff *, struct net_device *,
		    struct packet_type *, struct net_device *);
static int fcoe_percpu_receive_thread(void *);
static void fcoe_percpu_clean(struct fc_lport *);
static int fcoe_link_speed_update(struct fc_lport *);
static int fcoe_link_ok(struct fc_lport *);

static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *);
static int fcoe_hostlist_add(const struct fc_lport *);

static int fcoe_device_notification(struct notifier_block *, ulong, void *);
static void fcoe_dev_setup(void);
static void fcoe_dev_cleanup(void);
static struct fcoe_interface
*fcoe_hostlist_lookup_port(const struct net_device *);

static int fcoe_fip_recv(struct sk_buff *, struct net_device *,
			 struct packet_type *, struct net_device *);

static void fcoe_fip_send(struct fcoe_ctlr *, struct sk_buff *);
static void fcoe_update_src_mac(struct fc_lport *, u8 *);
static u8 *fcoe_get_src_mac(struct fc_lport *);
static void fcoe_destroy_work(struct work_struct *);

static int fcoe_ddp_setup(struct fc_lport *, u16, struct scatterlist *,
			  unsigned int);
static int fcoe_ddp_done(struct fc_lport *, u16);
static int fcoe_ddp_target(struct fc_lport *, u16, struct scatterlist *,
			   unsigned int);
static int fcoe_cpu_callback(struct notifier_block *, unsigned long, void *);
static int fcoe_dcb_app_notification(struct notifier_block *notifier,
				     ulong event, void *ptr);

static bool fcoe_match(struct net_device *netdev);
static int fcoe_create(struct net_device *netdev, enum fip_state fip_mode);
static int fcoe_destroy(struct net_device *netdev);
static int fcoe_enable(struct net_device *netdev);
static int fcoe_disable(struct net_device *netdev);

static struct fc_seq *fcoe_elsct_send(struct fc_lport *,
				      u32 did, struct fc_frame *,
				      unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *, u32 timeout);
static void fcoe_recv_frame(struct sk_buff *skb);

static void fcoe_get_lesb(struct fc_lport *, struct fc_els_lesb *);

static struct notifier_block fcoe_notifier = {
	.notifier_call = fcoe_device_notification,
};

static struct notifier_block fcoe_cpu_notifier = {
	.notifier_call = fcoe_cpu_callback,
};

static struct notifier_block dcb_notifier = {
	.notifier_call = fcoe_dcb_app_notification,
};

static struct scsi_transport_template *fcoe_nport_scsi_transport;
static struct scsi_transport_template *fcoe_vport_scsi_transport;

static int fcoe_vport_destroy(struct fc_vport *);
static int fcoe_vport_create(struct fc_vport *, bool disabled);
static int fcoe_vport_disable(struct fc_vport *, bool disable);
static void fcoe_set_vport_symbolic_name(struct fc_vport *);
static void fcoe_set_port_id(struct fc_lport *, u32, struct fc_frame *);

static struct libfc_function_template fcoe_libfc_fcn_templ = {
	.frame_send = fcoe_xmit,
	.ddp_setup = fcoe_ddp_setup,
	.ddp_done = fcoe_ddp_done,
	.ddp_target = fcoe_ddp_target,
	.elsct_send = fcoe_elsct_send,
	.get_lesb = fcoe_get_lesb,
	.lport_set_port_id = fcoe_set_port_id,
};

static struct fc_function_template fcoe_nport_fc_functions = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,
	.show_host_serial_number = 1,
	.show_host_manufacturer = 1,
	.show_host_model = 1,
	.show_host_model_description = 1,
	.show_host_hardware_version = 1,
	.show_host_driver_version = 1,
	.show_host_firmware_version = 1,
	.show_host_optionrom_version = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

	.vport_create = fcoe_vport_create,
	.vport_delete = fcoe_vport_destroy,
	.vport_disable = fcoe_vport_disable,
	.set_vport_symbolic_name = fcoe_set_vport_symbolic_name,

	.bsg_request = fc_lport_bsg_request,
};

static struct fc_function_template fcoe_vport_fc_functions = {
	.show_host_node_name = 1,
	.show_host_port_name = 1,
	.show_host_supported_classes = 1,
	.show_host_supported_fc4s = 1,
	.show_host_active_fc4s = 1,
	.show_host_maxframe_size = 1,
	.show_host_serial_number = 1,
	.show_host_manufacturer = 1,
	.show_host_model = 1,
	.show_host_model_description = 1,
	.show_host_hardware_version = 1,
	.show_host_driver_version = 1,
	.show_host_firmware_version = 1,
	.show_host_optionrom_version = 1,

	.show_host_port_id = 1,
	.show_host_supported_speeds = 1,
	.get_host_speed = fc_get_host_speed,
	.show_host_speed = 1,
	.show_host_port_type = 1,
	.get_host_port_state = fc_get_host_port_state,
	.show_host_port_state = 1,
	.show_host_symbolic_name = 1,

	.dd_fcrport_size = sizeof(struct fc_rport_libfc_priv),
	.show_rport_maxframe_size = 1,
	.show_rport_supported_classes = 1,

	.show_host_fabric_name = 1,
	.show_starget_node_name = 1,
	.show_starget_port_name = 1,
	.show_starget_port_id = 1,
	.set_rport_dev_loss_tmo = fc_set_rport_loss_tmo,
	.show_rport_dev_loss_tmo = 1,
	.get_fc_host_stats = fc_get_host_stats,
	.issue_fc_host_lip = fcoe_reset,

	.terminate_rport_io = fc_rport_terminate_io,

	.bsg_request = fc_lport_bsg_request,
};

static struct scsi_host_template fcoe_shost_template = {
	.module = THIS_MODULE,
	.name = "FCoE Driver",
	.proc_name = FCOE_NAME,
	.queuecommand = fc_queuecommand,
	.eh_abort_handler = fc_eh_abort,
	.eh_device_reset_handler = fc_eh_device_reset,
	.eh_host_reset_handler = fc_eh_host_reset,
	.slave_alloc = fc_slave_alloc,
	.change_queue_depth = fc_change_queue_depth,
	.change_queue_type = fc_change_queue_type,
	.this_id = -1,
	.cmd_per_lun = 3,
	.can_queue = FCOE_MAX_OUTSTANDING_COMMANDS,
	.use_clustering = ENABLE_CLUSTERING,
	.sg_tablesize = SG_ALL,
	.max_sectors = 0xffff,
};

static int fcoe_interface_setup(struct fcoe_interface *fcoe,
				struct net_device *netdev)
{
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct netdev_hw_addr *ha;
	struct net_device *real_dev;
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	fcoe->netdev = netdev;

	
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_enable) {
		if (ops->ndo_fcoe_enable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to enable FCoE"
					" specific feature for LLD.\n");
	}

	
	if (netdev->priv_flags & IFF_BONDING && netdev->flags & IFF_MASTER) {
		FCOE_NETDEV_DBG(netdev, "Bonded interfaces not supported\n");
		return -EOPNOTSUPP;
	}

	real_dev = (netdev->priv_flags & IFF_802_1Q_VLAN) ?
		vlan_dev_real_dev(netdev) : netdev;
	fcoe->realdev = real_dev;
	rcu_read_lock();
	for_each_dev_addr(real_dev, ha) {
		if ((ha->type == NETDEV_HW_ADDR_T_SAN) &&
		    (is_valid_ether_addr(ha->addr))) {
			memcpy(fip->ctl_src_addr, ha->addr, ETH_ALEN);
			fip->spma = 1;
			break;
		}
	}
	rcu_read_unlock();

	
	if (!fip->spma)
		memcpy(fip->ctl_src_addr, netdev->dev_addr, netdev->addr_len);

	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_uc_add(netdev, flogi_maddr);
	if (fip->spma)
		dev_uc_add(netdev, fip->ctl_src_addr);
	if (fip->mode == FIP_MODE_VN2VN) {
		dev_mc_add(netdev, FIP_ALL_VN2VN_MACS);
		dev_mc_add(netdev, FIP_ALL_P2P_MACS);
	} else
		dev_mc_add(netdev, FIP_ALL_ENODE_MACS);

	fcoe->fcoe_packet_type.func = fcoe_rcv;
	fcoe->fcoe_packet_type.type = __constant_htons(ETH_P_FCOE);
	fcoe->fcoe_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fcoe_packet_type);

	fcoe->fip_packet_type.func = fcoe_fip_recv;
	fcoe->fip_packet_type.type = htons(ETH_P_FIP);
	fcoe->fip_packet_type.dev = netdev;
	dev_add_pack(&fcoe->fip_packet_type);

	return 0;
}

static struct fcoe_interface *fcoe_interface_create(struct net_device *netdev,
						    enum fip_state fip_mode)
{
	struct fcoe_interface *fcoe;
	int err;

	if (!try_module_get(THIS_MODULE)) {
		FCOE_NETDEV_DBG(netdev,
				"Could not get a reference to the module\n");
		fcoe = ERR_PTR(-EBUSY);
		goto out;
	}

	fcoe = kzalloc(sizeof(*fcoe), GFP_KERNEL);
	if (!fcoe) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate fcoe structure\n");
		fcoe = ERR_PTR(-ENOMEM);
		goto out_putmod;
	}

	dev_hold(netdev);

	fcoe_ctlr_init(&fcoe->ctlr, fip_mode);
	fcoe->ctlr.send = fcoe_fip_send;
	fcoe->ctlr.update_mac = fcoe_update_src_mac;
	fcoe->ctlr.get_src_addr = fcoe_get_src_mac;

	err = fcoe_interface_setup(fcoe, netdev);
	if (err) {
		fcoe_ctlr_destroy(&fcoe->ctlr);
		kfree(fcoe);
		dev_put(netdev);
		fcoe = ERR_PTR(err);
		goto out_putmod;
	}

	goto out;

out_putmod:
	module_put(THIS_MODULE);
out:
	return fcoe;
}

static void fcoe_interface_cleanup(struct fcoe_interface *fcoe)
{
	struct net_device *netdev = fcoe->netdev;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	u8 flogi_maddr[ETH_ALEN];
	const struct net_device_ops *ops;

	rtnl_lock();

	__dev_remove_pack(&fcoe->fcoe_packet_type);
	__dev_remove_pack(&fcoe->fip_packet_type);
	synchronize_net();

	
	memcpy(flogi_maddr, (u8[6]) FC_FCOE_FLOGI_MAC, ETH_ALEN);
	dev_uc_del(netdev, flogi_maddr);
	if (fip->spma)
		dev_uc_del(netdev, fip->ctl_src_addr);
	if (fip->mode == FIP_MODE_VN2VN) {
		dev_mc_del(netdev, FIP_ALL_VN2VN_MACS);
		dev_mc_del(netdev, FIP_ALL_P2P_MACS);
	} else
		dev_mc_del(netdev, FIP_ALL_ENODE_MACS);

	
	ops = netdev->netdev_ops;
	if (ops->ndo_fcoe_disable) {
		if (ops->ndo_fcoe_disable(netdev))
			FCOE_NETDEV_DBG(netdev, "Failed to disable FCoE"
					" specific feature for LLD.\n");
	}

	rtnl_unlock();

	
	
	fcoe_ctlr_destroy(fip);
	kfree(fcoe);
	dev_put(netdev);
	module_put(THIS_MODULE);
}

static int fcoe_fip_recv(struct sk_buff *skb, struct net_device *netdev,
			 struct packet_type *ptype,
			 struct net_device *orig_dev)
{
	struct fcoe_interface *fcoe;

	fcoe = container_of(ptype, struct fcoe_interface, fip_packet_type);
	fcoe_ctlr_recv(&fcoe->ctlr, skb);
	return 0;
}

static void fcoe_port_send(struct fcoe_port *port, struct sk_buff *skb)
{
	if (port->fcoe_pending_queue.qlen)
		fcoe_check_wait_queue(port->lport, skb);
	else if (fcoe_start_io(skb))
		fcoe_check_wait_queue(port->lport, skb);
}

static void fcoe_fip_send(struct fcoe_ctlr *fip, struct sk_buff *skb)
{
	skb->dev = fcoe_from_ctlr(fip)->netdev;
	fcoe_port_send(lport_priv(fip->lp), skb);
}

static void fcoe_update_src_mac(struct fc_lport *lport, u8 *addr)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;

	rtnl_lock();
	if (!is_zero_ether_addr(port->data_src_addr))
		dev_uc_del(fcoe->netdev, port->data_src_addr);
	if (!is_zero_ether_addr(addr))
		dev_uc_add(fcoe->netdev, addr);
	memcpy(port->data_src_addr, addr, ETH_ALEN);
	rtnl_unlock();
}

static u8 *fcoe_get_src_mac(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);

	return port->data_src_addr;
}

static int fcoe_lport_config(struct fc_lport *lport)
{
	lport->link_up = 0;
	lport->qfull = 0;
	lport->max_retry_count = 3;
	lport->max_rport_retry_count = 3;
	lport->e_d_tov = 2 * 1000;	
	lport->r_a_tov = 2 * 2 * 1000;
	lport->service_params = (FCP_SPPF_INIT_FCN | FCP_SPPF_RD_XRDY_DIS |
				 FCP_SPPF_RETRY | FCP_SPPF_CONF_COMPL);
	lport->does_npiv = 1;

	fc_lport_init_stats(lport);

	
	fc_lport_config(lport);

	
	lport->crc_offload = 0;
	lport->seq_offload = 0;
	lport->lro_enabled = 0;
	lport->lro_xid = 0;
	lport->lso_max = 0;

	return 0;
}

static void fcoe_netdev_features_change(struct fc_lport *lport,
					struct net_device *netdev)
{
	mutex_lock(&lport->lp_mutex);

	if (netdev->features & NETIF_F_SG)
		lport->sg_supp = 1;
	else
		lport->sg_supp = 0;

	if (netdev->features & NETIF_F_FCOE_CRC) {
		lport->crc_offload = 1;
		FCOE_NETDEV_DBG(netdev, "Supports FCCRC offload\n");
	} else {
		lport->crc_offload = 0;
	}

	if (netdev->features & NETIF_F_FSO) {
		lport->seq_offload = 1;
		lport->lso_max = netdev->gso_max_size;
		FCOE_NETDEV_DBG(netdev, "Supports LSO for max len 0x%x\n",
				lport->lso_max);
	} else {
		lport->seq_offload = 0;
		lport->lso_max = 0;
	}

	if (netdev->fcoe_ddp_xid) {
		lport->lro_enabled = 1;
		lport->lro_xid = netdev->fcoe_ddp_xid;
		FCOE_NETDEV_DBG(netdev, "Supports LRO for max xid 0x%x\n",
				lport->lro_xid);
	} else {
		lport->lro_enabled = 0;
		lport->lro_xid = 0;
	}

	mutex_unlock(&lport->lp_mutex);
}

static int fcoe_netdev_config(struct fc_lport *lport, struct net_device *netdev)
{
	u32 mfs;
	u64 wwnn, wwpn;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	
	port = lport_priv(lport);
	fcoe = port->priv;

	mfs = netdev->mtu;
	if (netdev->features & NETIF_F_FCOE_MTU) {
		mfs = FCOE_MTU;
		FCOE_NETDEV_DBG(netdev, "Supports FCOE_MTU of %d bytes\n", mfs);
	}
	mfs -= (sizeof(struct fcoe_hdr) + sizeof(struct fcoe_crc_eof));
	if (fc_set_mfs(lport, mfs))
		return -EINVAL;

	
	fcoe_netdev_features_change(lport, netdev);

	skb_queue_head_init(&port->fcoe_pending_queue);
	port->fcoe_pending_queue_active = 0;
	setup_timer(&port->timer, fcoe_queue_timer, (unsigned long)lport);

	fcoe_link_speed_update(lport);

	if (!lport->vport) {
		if (fcoe_get_wwn(netdev, &wwnn, NETDEV_FCOE_WWNN))
			wwnn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr, 1, 0);
		fc_set_wwnn(lport, wwnn);
		if (fcoe_get_wwn(netdev, &wwpn, NETDEV_FCOE_WWPN))
			wwpn = fcoe_wwn_from_mac(fcoe->ctlr.ctl_src_addr,
						 2, 0);
		fc_set_wwpn(lport, wwpn);
	}

	return 0;
}

static int fcoe_shost_config(struct fc_lport *lport, struct device *dev)
{
	int rc = 0;

	
	lport->host->max_lun = FCOE_MAX_LUN;
	lport->host->max_id = FCOE_MAX_FCP_TARGET;
	lport->host->max_channel = 0;
	lport->host->max_cmd_len = FCOE_MAX_CMD_LEN;

	if (lport->vport)
		lport->host->transportt = fcoe_vport_scsi_transport;
	else
		lport->host->transportt = fcoe_nport_scsi_transport;

	
	rc = scsi_add_host(lport->host, dev);
	if (rc) {
		FCOE_NETDEV_DBG(fcoe_netdev(lport), "fcoe_shost_config: "
				"error on scsi_add_host\n");
		return rc;
	}

	if (!lport->vport)
		fc_host_max_npiv_vports(lport->host) = USHRT_MAX;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name);

	return 0;
}


static void fcoe_fdmi_info(struct fc_lport *lport, struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;
	struct net_device *realdev;
	int rc;
	struct netdev_fcoe_hbainfo fdmi;

	port = lport_priv(lport);
	fcoe = port->priv;
	realdev = fcoe->realdev;

	if (!realdev)
		return;

	
	if (lport->vport)
		return;

	if (realdev->netdev_ops->ndo_fcoe_get_hbainfo) {
		memset(&fdmi, 0, sizeof(fdmi));
		rc = realdev->netdev_ops->ndo_fcoe_get_hbainfo(realdev,
							       &fdmi);
		if (rc) {
			printk(KERN_INFO "fcoe: Failed to retrieve FDMI "
					"information from netdev.\n");
			return;
		}

		snprintf(fc_host_serial_number(lport->host),
			 FC_SERIAL_NUMBER_SIZE,
			 "%s",
			 fdmi.serial_number);
		snprintf(fc_host_manufacturer(lport->host),
			 FC_SERIAL_NUMBER_SIZE,
			 "%s",
			 fdmi.manufacturer);
		snprintf(fc_host_model(lport->host),
			 FC_SYMBOLIC_NAME_SIZE,
			 "%s",
			 fdmi.model);
		snprintf(fc_host_model_description(lport->host),
			 FC_SYMBOLIC_NAME_SIZE,
			 "%s",
			 fdmi.model_description);
		snprintf(fc_host_hardware_version(lport->host),
			 FC_VERSION_STRING_SIZE,
			 "%s",
			 fdmi.hardware_version);
		snprintf(fc_host_driver_version(lport->host),
			 FC_VERSION_STRING_SIZE,
			 "%s",
			 fdmi.driver_version);
		snprintf(fc_host_optionrom_version(lport->host),
			 FC_VERSION_STRING_SIZE,
			 "%s",
			 fdmi.optionrom_version);
		snprintf(fc_host_firmware_version(lport->host),
			 FC_VERSION_STRING_SIZE,
			 "%s",
			 fdmi.firmware_version);

		
		lport->fdmi_enabled = 1;
	} else {
		lport->fdmi_enabled = 0;
		printk(KERN_INFO "fcoe: No FDMI support.\n");
	}
}

static bool fcoe_oem_match(struct fc_frame *fp)
{
	struct fc_frame_header *fh = fc_frame_header_get(fp);
	struct fcp_cmnd *fcp;

	if (fc_fcp_is_read(fr_fsp(fp)) &&
	    (fr_fsp(fp)->data_len > fcoe_ddp_min))
		return true;
	else if ((fr_fsp(fp) == NULL) &&
		 (fh->fh_r_ctl == FC_RCTL_DD_UNSOL_CMD) &&
		 (ntohs(fh->fh_rx_id) == FC_XID_UNKNOWN)) {
		fcp = fc_frame_payload_get(fp, sizeof(*fcp));
		if ((fcp->fc_flags & FCP_CFL_WRDATA) &&
		    (ntohl(fcp->fc_dl) > fcoe_ddp_min))
			return true;
	}
	return false;
}

static inline int fcoe_em_config(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;
	struct fcoe_interface *oldfcoe = NULL;
	struct net_device *old_real_dev, *cur_real_dev;
	u16 min_xid = FCOE_MIN_XID;
	u16 max_xid = FCOE_MAX_XID;

	if (!lport->lro_enabled || !lport->lro_xid ||
	    (lport->lro_xid >= max_xid)) {
		lport->lro_xid = 0;
		goto skip_oem;
	}

	if (fcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
		cur_real_dev = vlan_dev_real_dev(fcoe->netdev);
	else
		cur_real_dev = fcoe->netdev;

	list_for_each_entry(oldfcoe, &fcoe_hostlist, list) {
		if (oldfcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
			old_real_dev = vlan_dev_real_dev(oldfcoe->netdev);
		else
			old_real_dev = oldfcoe->netdev;

		if (cur_real_dev == old_real_dev) {
			fcoe->oem = oldfcoe->oem;
			break;
		}
	}

	if (fcoe->oem) {
		if (!fc_exch_mgr_add(lport, fcoe->oem, fcoe_oem_match)) {
			printk(KERN_ERR "fcoe_em_config: failed to add "
			       "offload em:%p on interface:%s\n",
			       fcoe->oem, fcoe->netdev->name);
			return -ENOMEM;
		}
	} else {
		fcoe->oem = fc_exch_mgr_alloc(lport, FC_CLASS_3,
					      FCOE_MIN_XID, lport->lro_xid,
					      fcoe_oem_match);
		if (!fcoe->oem) {
			printk(KERN_ERR "fcoe_em_config: failed to allocate "
			       "em for offload exches on interface:%s\n",
			       fcoe->netdev->name);
			return -ENOMEM;
		}
	}

	min_xid += lport->lro_xid + 1;

skip_oem:
	if (!fc_exch_mgr_alloc(lport, FC_CLASS_3, min_xid, max_xid, NULL)) {
		printk(KERN_ERR "fcoe_em_config: failed to "
		       "allocate em on interface %s\n", fcoe->netdev->name);
		return -ENOMEM;
	}

	return 0;
}

static void fcoe_if_destroy(struct fc_lport *lport)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;
	struct net_device *netdev = fcoe->netdev;

	FCOE_NETDEV_DBG(netdev, "Destroying interface\n");

	
	fc_fabric_logoff(lport);

	
	fc_lport_destroy(lport);

	
	del_timer_sync(&port->timer);

	
	fcoe_clean_pending_queue(lport);

	rtnl_lock();
	if (!is_zero_ether_addr(port->data_src_addr))
		dev_uc_del(netdev, port->data_src_addr);
	rtnl_unlock();

	
	fcoe_percpu_clean(lport);

	
	fc_remove_host(lport->host);
	scsi_remove_host(lport->host);

	
	fc_fcp_destroy(lport);

	
	fc_exch_mgr_free(lport);

	
	fc_lport_free_stats(lport);

	
	scsi_host_put(lport->host);
}

static int fcoe_ddp_setup(struct fc_lport *lport, u16 xid,
			  struct scatterlist *sgl, unsigned int sgc)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netdev->netdev_ops->ndo_fcoe_ddp_setup)
		return netdev->netdev_ops->ndo_fcoe_ddp_setup(netdev,
							      xid, sgl,
							      sgc);

	return 0;
}

static int fcoe_ddp_target(struct fc_lport *lport, u16 xid,
			   struct scatterlist *sgl, unsigned int sgc)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netdev->netdev_ops->ndo_fcoe_ddp_target)
		return netdev->netdev_ops->ndo_fcoe_ddp_target(netdev, xid,
							       sgl, sgc);

	return 0;
}


static int fcoe_ddp_done(struct fc_lport *lport, u16 xid)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netdev->netdev_ops->ndo_fcoe_ddp_done)
		return netdev->netdev_ops->ndo_fcoe_ddp_done(netdev, xid);
	return 0;
}

static struct fc_lport *fcoe_if_create(struct fcoe_interface *fcoe,
				       struct device *parent, int npiv)
{
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *lport, *n_port;
	struct fcoe_port *port;
	struct Scsi_Host *shost;
	int rc;
	struct fc_vport *vport = dev_to_vport(parent);

	FCOE_NETDEV_DBG(netdev, "Create Interface\n");

	if (!npiv)
		lport = libfc_host_alloc(&fcoe_shost_template, sizeof(*port));
	else
		lport = libfc_vport_create(vport, sizeof(*port));

	if (!lport) {
		FCOE_NETDEV_DBG(netdev, "Could not allocate host structure\n");
		rc = -ENOMEM;
		goto out;
	}
	port = lport_priv(lport);
	port->lport = lport;
	port->priv = fcoe;
	port->max_queue_depth = FCOE_MAX_QUEUE_DEPTH;
	port->min_queue_depth = FCOE_MIN_QUEUE_DEPTH;
	INIT_WORK(&port->destroy_work, fcoe_destroy_work);

	
	rc = fcoe_lport_config(lport);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure lport for the "
				"interface\n");
		goto out_host_put;
	}

	if (npiv) {
		FCOE_NETDEV_DBG(netdev, "Setting vport names, "
				"%16.16llx %16.16llx\n",
				vport->node_name, vport->port_name);
		fc_set_wwnn(lport, vport->node_name);
		fc_set_wwpn(lport, vport->port_name);
	}

	
	rc = fcoe_netdev_config(lport, netdev);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure netdev for the "
				"interface\n");
		goto out_lp_destroy;
	}

	
	rc = fcoe_shost_config(lport, parent);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure shost for the "
				"interface\n");
		goto out_lp_destroy;
	}

	
	rc = fcoe_libfc_config(lport, &fcoe->ctlr, &fcoe_libfc_fcn_templ, 1);
	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure libfc for the "
				"interface\n");
		goto out_lp_destroy;
	}

	
	fcoe_fdmi_info(lport, netdev);

	if (!npiv)
		
		rc = fcoe_em_config(lport);
	else {
		shost = vport_to_shost(vport);
		n_port = shost_priv(shost);
		rc = fc_exch_mgr_list_clone(n_port, lport);
	}

	if (rc) {
		FCOE_NETDEV_DBG(netdev, "Could not configure the EM\n");
		goto out_lp_destroy;
	}

	return lport;

out_lp_destroy:
	fc_exch_mgr_free(lport);
out_host_put:
	scsi_host_put(lport->host);
out:
	return ERR_PTR(rc);
}

static int __init fcoe_if_init(void)
{
	
	fcoe_nport_scsi_transport =
		fc_attach_transport(&fcoe_nport_fc_functions);
	fcoe_vport_scsi_transport =
		fc_attach_transport(&fcoe_vport_fc_functions);

	if (!fcoe_nport_scsi_transport) {
		printk(KERN_ERR "fcoe: Failed to attach to the FC transport\n");
		return -ENODEV;
	}

	return 0;
}

static int __exit fcoe_if_exit(void)
{
	fc_release_transport(fcoe_nport_scsi_transport);
	fc_release_transport(fcoe_vport_scsi_transport);
	fcoe_nport_scsi_transport = NULL;
	fcoe_vport_scsi_transport = NULL;
	return 0;
}

static void fcoe_percpu_thread_create(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;

	p = &per_cpu(fcoe_percpu, cpu);

	thread = kthread_create_on_node(fcoe_percpu_receive_thread,
					(void *)p, cpu_to_node(cpu),
					"fcoethread/%d", cpu);

	if (likely(!IS_ERR(thread))) {
		kthread_bind(thread, cpu);
		wake_up_process(thread);

		spin_lock_bh(&p->fcoe_rx_list.lock);
		p->thread = thread;
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
}

static void fcoe_percpu_thread_destroy(unsigned int cpu)
{
	struct fcoe_percpu_s *p;
	struct task_struct *thread;
	struct page *crc_eof;
	struct sk_buff *skb;
#ifdef CONFIG_SMP
	struct fcoe_percpu_s *p0;
	unsigned targ_cpu = get_cpu();
#endif 

	FCOE_DBG("Destroying receive thread for CPU %d\n", cpu);

	
	p = &per_cpu(fcoe_percpu, cpu);
	spin_lock_bh(&p->fcoe_rx_list.lock);
	thread = p->thread;
	p->thread = NULL;
	crc_eof = p->crc_eof_page;
	p->crc_eof_page = NULL;
	p->crc_eof_offset = 0;
	spin_unlock_bh(&p->fcoe_rx_list.lock);

#ifdef CONFIG_SMP
	if (cpu != targ_cpu) {
		p0 = &per_cpu(fcoe_percpu, targ_cpu);
		spin_lock_bh(&p0->fcoe_rx_list.lock);
		if (p0->thread) {
			FCOE_DBG("Moving frames from CPU %d to CPU %d\n",
				 cpu, targ_cpu);

			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				__skb_queue_tail(&p0->fcoe_rx_list, skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		} else {
			while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
				kfree_skb(skb);
			spin_unlock_bh(&p0->fcoe_rx_list.lock);
		}
	} else {
		spin_lock_bh(&p->fcoe_rx_list.lock);
		while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
			kfree_skb(skb);
		spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
	put_cpu();
#else
	spin_lock_bh(&p->fcoe_rx_list.lock);
	while ((skb = __skb_dequeue(&p->fcoe_rx_list)) != NULL)
		kfree_skb(skb);
	spin_unlock_bh(&p->fcoe_rx_list.lock);
#endif

	if (thread)
		kthread_stop(thread);

	if (crc_eof)
		put_page(crc_eof);
}

static int fcoe_cpu_callback(struct notifier_block *nfb,
			     unsigned long action, void *hcpu)
{
	unsigned cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		FCOE_DBG("CPU %x online: Create Rx thread\n", cpu);
		fcoe_percpu_thread_create(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		FCOE_DBG("CPU %x offline: Remove Rx thread\n", cpu);
		fcoe_percpu_thread_destroy(cpu);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static inline unsigned int fcoe_select_cpu(void)
{
	static unsigned int selected_cpu;

	selected_cpu = cpumask_next(selected_cpu, cpu_online_mask);
	if (selected_cpu >= nr_cpu_ids)
		selected_cpu = cpumask_first(cpu_online_mask);

	return selected_cpu;
}

static int fcoe_rcv(struct sk_buff *skb, struct net_device *netdev,
	     struct packet_type *ptype, struct net_device *olddev)
{
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_interface *fcoe;
	struct fc_frame_header *fh;
	struct fcoe_percpu_s *fps;
	struct ethhdr *eh;
	unsigned int cpu;

	fcoe = container_of(ptype, struct fcoe_interface, fcoe_packet_type);
	lport = fcoe->ctlr.lp;
	if (unlikely(!lport)) {
		FCOE_NETDEV_DBG(netdev, "Cannot find hba structure");
		goto err2;
	}
	if (!lport->link_up)
		goto err2;

	FCOE_NETDEV_DBG(netdev, "skb_info: len:%d data_len:%d head:%p "
			"data:%p tail:%p end:%p sum:%d dev:%s",
			skb->len, skb->data_len, skb->head, skb->data,
			skb_tail_pointer(skb), skb_end_pointer(skb),
			skb->csum, skb->dev ? skb->dev->name : "<NULL>");

	eh = eth_hdr(skb);

	if (is_fip_mode(&fcoe->ctlr) &&
	    compare_ether_addr(eh->h_source, fcoe->ctlr.dest_addr)) {
		FCOE_NETDEV_DBG(netdev, "wrong source mac address:%pM\n",
				eh->h_source);
		goto err;
	}

	if (unlikely((skb->len < FCOE_MIN_FRAME) ||
		     !pskb_may_pull(skb, FCOE_HEADER_LEN)))
		goto err;

	skb_set_transport_header(skb, sizeof(struct fcoe_hdr));
	fh = (struct fc_frame_header *) skb_transport_header(skb);

	if (ntoh24(&eh->h_dest[3]) != ntoh24(fh->fh_d_id)) {
		FCOE_NETDEV_DBG(netdev, "FC frame d_id mismatch with MAC:%pM\n",
				eh->h_dest);
		goto err;
	}

	fr = fcoe_dev_from_skb(skb);
	fr->fr_dev = lport;

	if (ntoh24(fh->fh_f_ctl) & FC_FC_EX_CTX)
		cpu = ntohs(fh->fh_ox_id) & fc_cpu_mask;
	else {
		if (ntohs(fh->fh_rx_id) == FC_XID_UNKNOWN)
			cpu = fcoe_select_cpu();
		else
			cpu = ntohs(fh->fh_rx_id) & fc_cpu_mask;
	}

	if (cpu >= nr_cpu_ids)
		goto err;

	fps = &per_cpu(fcoe_percpu, cpu);
	spin_lock(&fps->fcoe_rx_list.lock);
	if (unlikely(!fps->thread)) {
		FCOE_NETDEV_DBG(netdev, "CPU is online, but no receive thread "
				"ready for incoming skb- using first online "
				"CPU.\n");

		spin_unlock(&fps->fcoe_rx_list.lock);
		cpu = cpumask_first(cpu_online_mask);
		fps = &per_cpu(fcoe_percpu, cpu);
		spin_lock(&fps->fcoe_rx_list.lock);
		if (!fps->thread) {
			spin_unlock(&fps->fcoe_rx_list.lock);
			goto err;
		}
	}


	__skb_queue_tail(&fps->fcoe_rx_list, skb);
	if (fps->thread->state == TASK_INTERRUPTIBLE)
		wake_up_process(fps->thread);
	spin_unlock(&fps->fcoe_rx_list.lock);

	return 0;
err:
	per_cpu_ptr(lport->dev_stats, get_cpu())->ErrorFrames++;
	put_cpu();
err2:
	kfree_skb(skb);
	return -1;
}

static int fcoe_alloc_paged_crc_eof(struct sk_buff *skb, int tlen)
{
	struct fcoe_percpu_s *fps;
	int rc;

	fps = &get_cpu_var(fcoe_percpu);
	rc = fcoe_get_paged_crc_eof(skb, tlen, fps);
	put_cpu_var(fcoe_percpu);

	return rc;
}

static int fcoe_xmit(struct fc_lport *lport, struct fc_frame *fp)
{
	int wlen;
	u32 crc;
	struct ethhdr *eh;
	struct fcoe_crc_eof *cp;
	struct sk_buff *skb;
	struct fcoe_dev_stats *stats;
	struct fc_frame_header *fh;
	unsigned int hlen;		
	unsigned int tlen;		
	unsigned int elen;		
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;
	u8 sof, eof;
	struct fcoe_hdr *hp;

	WARN_ON((fr_len(fp) % sizeof(u32)) != 0);

	fh = fc_frame_header_get(fp);
	skb = fp_skb(fp);
	wlen = skb->len / FCOE_WORD_TO_BYTE;

	if (!lport->link_up) {
		kfree_skb(skb);
		return 0;
	}

	if (unlikely(fh->fh_type == FC_TYPE_ELS) &&
	    fcoe_ctlr_els_send(&fcoe->ctlr, lport, skb))
		return 0;

	sof = fr_sof(fp);
	eof = fr_eof(fp);

	elen = sizeof(struct ethhdr);
	hlen = sizeof(struct fcoe_hdr);
	tlen = sizeof(struct fcoe_crc_eof);
	wlen = (skb->len - tlen + sizeof(crc)) / FCOE_WORD_TO_BYTE;

	
	if (likely(lport->crc_offload)) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->csum_start = skb_headroom(skb);
		skb->csum_offset = skb->len;
		crc = 0;
	} else {
		skb->ip_summed = CHECKSUM_NONE;
		crc = fcoe_fc_crc(fp);
	}

	
	if (skb_is_nonlinear(skb)) {
		skb_frag_t *frag;
		if (fcoe_alloc_paged_crc_eof(skb, tlen)) {
			kfree_skb(skb);
			return -ENOMEM;
		}
		frag = &skb_shinfo(skb)->frags[skb_shinfo(skb)->nr_frags - 1];
		cp = kmap_atomic(skb_frag_page(frag))
			+ frag->page_offset;
	} else {
		cp = (struct fcoe_crc_eof *)skb_put(skb, tlen);
	}

	memset(cp, 0, sizeof(*cp));
	cp->fcoe_eof = eof;
	cp->fcoe_crc32 = cpu_to_le32(~crc);

	if (skb_is_nonlinear(skb)) {
		kunmap_atomic(cp);
		cp = NULL;
	}

	
	skb_push(skb, elen + hlen);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb->mac_len = elen;
	skb->protocol = htons(ETH_P_FCOE);
	skb->priority = port->priority;

	if (fcoe->netdev->priv_flags & IFF_802_1Q_VLAN &&
	    fcoe->realdev->features & NETIF_F_HW_VLAN_TX) {
		skb->vlan_tci = VLAN_TAG_PRESENT |
				vlan_dev_vlan_id(fcoe->netdev);
		skb->dev = fcoe->realdev;
	} else
		skb->dev = fcoe->netdev;

	
	eh = eth_hdr(skb);
	eh->h_proto = htons(ETH_P_FCOE);
	memcpy(eh->h_dest, fcoe->ctlr.dest_addr, ETH_ALEN);
	if (fcoe->ctlr.map_dest)
		memcpy(eh->h_dest + 3, fh->fh_d_id, 3);

	if (unlikely(fcoe->ctlr.flogi_oxid != FC_XID_UNKNOWN))
		memcpy(eh->h_source, fcoe->ctlr.ctl_src_addr, ETH_ALEN);
	else
		memcpy(eh->h_source, port->data_src_addr, ETH_ALEN);

	hp = (struct fcoe_hdr *)(eh + 1);
	memset(hp, 0, sizeof(*hp));
	if (FC_FCOE_VER)
		FC_FCOE_ENCAPS_VER(hp, FC_FCOE_VER);
	hp->fcoe_sof = sof;

	
	if (lport->seq_offload && fr_max_payload(fp)) {
		skb_shinfo(skb)->gso_type = SKB_GSO_FCOE;
		skb_shinfo(skb)->gso_size = fr_max_payload(fp);
	} else {
		skb_shinfo(skb)->gso_type = 0;
		skb_shinfo(skb)->gso_size = 0;
	}
	
	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	stats->TxFrames++;
	stats->TxWords += wlen;
	put_cpu();

	
	fr_dev(fp) = lport;
	fcoe_port_send(port, skb);
	return 0;
}

static void fcoe_percpu_flush_done(struct sk_buff *skb)
{
	complete(&fcoe_flush_completion);
}

static inline int fcoe_filter_frames(struct fc_lport *lport,
				     struct fc_frame *fp)
{
	struct fcoe_interface *fcoe;
	struct fc_frame_header *fh;
	struct sk_buff *skb = (struct sk_buff *)fp;
	struct fcoe_dev_stats *stats;

	if (lport->crc_offload && skb->ip_summed == CHECKSUM_UNNECESSARY)
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
	else
		fr_flags(fp) |= FCPHF_CRC_UNCHECKED;

	fh = (struct fc_frame_header *) skb_transport_header(skb);
	fh = fc_frame_header_get(fp);
	if (fh->fh_r_ctl == FC_RCTL_DD_SOL_DATA && fh->fh_type == FC_TYPE_FCP)
		return 0;

	fcoe = ((struct fcoe_port *)lport_priv(lport))->priv;
	if (is_fip_mode(&fcoe->ctlr) && fc_frame_payload_op(fp) == ELS_LOGO &&
	    ntoh24(fh->fh_s_id) == FC_FID_FLOGI) {
		FCOE_DBG("fcoe: dropping FCoE lport LOGO in fip mode\n");
		return -EINVAL;
	}

	if (!(fr_flags(fp) & FCPHF_CRC_UNCHECKED) ||
	    le32_to_cpu(fr_crc(fp)) == ~crc32(~0, skb->data, skb->len)) {
		fr_flags(fp) &= ~FCPHF_CRC_UNCHECKED;
		return 0;
	}

	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	stats->InvalidCRCCount++;
	if (stats->InvalidCRCCount < 5)
		printk(KERN_WARNING "fcoe: dropping frame with CRC error\n");
	put_cpu();
	return -EINVAL;
}

static void fcoe_recv_frame(struct sk_buff *skb)
{
	u32 fr_len;
	struct fc_lport *lport;
	struct fcoe_rcv_info *fr;
	struct fcoe_dev_stats *stats;
	struct fcoe_crc_eof crc_eof;
	struct fc_frame *fp;
	struct fcoe_port *port;
	struct fcoe_hdr *hp;

	fr = fcoe_dev_from_skb(skb);
	lport = fr->fr_dev;
	if (unlikely(!lport)) {
		if (skb->destructor != fcoe_percpu_flush_done)
			FCOE_NETDEV_DBG(skb->dev, "NULL lport in skb");
		kfree_skb(skb);
		return;
	}

	FCOE_NETDEV_DBG(skb->dev, "skb_info: len:%d data_len:%d "
			"head:%p data:%p tail:%p end:%p sum:%d dev:%s",
			skb->len, skb->data_len,
			skb->head, skb->data, skb_tail_pointer(skb),
			skb_end_pointer(skb), skb->csum,
			skb->dev ? skb->dev->name : "<NULL>");

	port = lport_priv(lport);
	skb_linearize(skb); 

	hp = (struct fcoe_hdr *) skb_network_header(skb);

	stats = per_cpu_ptr(lport->dev_stats, get_cpu());
	if (unlikely(FC_FCOE_DECAPS_VER(hp) != FC_FCOE_VER)) {
		if (stats->ErrorFrames < 5)
			printk(KERN_WARNING "fcoe: FCoE version "
			       "mismatch: The frame has "
			       "version %x, but the "
			       "initiator supports version "
			       "%x\n", FC_FCOE_DECAPS_VER(hp),
			       FC_FCOE_VER);
		goto drop;
	}

	skb_pull(skb, sizeof(struct fcoe_hdr));
	fr_len = skb->len - sizeof(struct fcoe_crc_eof);

	stats->RxFrames++;
	stats->RxWords += fr_len / FCOE_WORD_TO_BYTE;

	fp = (struct fc_frame *)skb;
	fc_frame_init(fp);
	fr_dev(fp) = lport;
	fr_sof(fp) = hp->fcoe_sof;

	
	if (skb_copy_bits(skb, fr_len, &crc_eof, sizeof(crc_eof)))
		goto drop;
	fr_eof(fp) = crc_eof.fcoe_eof;
	fr_crc(fp) = crc_eof.fcoe_crc32;
	if (pskb_trim(skb, fr_len))
		goto drop;

	if (!fcoe_filter_frames(lport, fp)) {
		put_cpu();
		fc_exch_recv(lport, fp);
		return;
	}
drop:
	stats->ErrorFrames++;
	put_cpu();
	kfree_skb(skb);
}

static int fcoe_percpu_receive_thread(void *arg)
{
	struct fcoe_percpu_s *p = arg;
	struct sk_buff *skb;
	struct sk_buff_head tmp;

	skb_queue_head_init(&tmp);

	set_user_nice(current, -20);

	while (!kthread_should_stop()) {

		spin_lock_bh(&p->fcoe_rx_list.lock);
		skb_queue_splice_init(&p->fcoe_rx_list, &tmp);
		spin_unlock_bh(&p->fcoe_rx_list.lock);

		while ((skb = __skb_dequeue(&tmp)) != NULL)
			fcoe_recv_frame(skb);

		spin_lock_bh(&p->fcoe_rx_list.lock);
		if (!skb_queue_len(&p->fcoe_rx_list)) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_bh(&p->fcoe_rx_list.lock);
			schedule();
			set_current_state(TASK_RUNNING);
		} else
			spin_unlock_bh(&p->fcoe_rx_list.lock);
	}
	return 0;
}

static void fcoe_dev_setup(void)
{
	register_dcbevent_notifier(&dcb_notifier);
	register_netdevice_notifier(&fcoe_notifier);
}

static void fcoe_dev_cleanup(void)
{
	unregister_dcbevent_notifier(&dcb_notifier);
	unregister_netdevice_notifier(&fcoe_notifier);
}

static struct fcoe_interface *
fcoe_hostlist_lookup_realdev_port(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	struct net_device *real_dev;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev->priv_flags & IFF_802_1Q_VLAN)
			real_dev = vlan_dev_real_dev(fcoe->netdev);
		else
			real_dev = fcoe->netdev;

		if (netdev == real_dev)
			return fcoe;
	}
	return NULL;
}

static int fcoe_dcb_app_notification(struct notifier_block *notifier,
				     ulong event, void *ptr)
{
	struct dcb_app_type *entry = ptr;
	struct fcoe_interface *fcoe;
	struct net_device *netdev;
	struct fcoe_port *port;
	int prio;

	if (entry->app.selector != DCB_APP_IDTYPE_ETHTYPE)
		return NOTIFY_OK;

	netdev = dev_get_by_index(&init_net, entry->ifindex);
	if (!netdev)
		return NOTIFY_OK;

	fcoe = fcoe_hostlist_lookup_realdev_port(netdev);
	dev_put(netdev);
	if (!fcoe)
		return NOTIFY_OK;

	if (entry->dcbx & DCB_CAP_DCBX_VER_CEE)
		prio = ffs(entry->app.priority) - 1;
	else
		prio = entry->app.priority;

	if (prio < 0)
		return NOTIFY_OK;

	if (entry->app.protocol == ETH_P_FIP ||
	    entry->app.protocol == ETH_P_FCOE)
		fcoe->ctlr.priority = prio;

	if (entry->app.protocol == ETH_P_FCOE) {
		port = lport_priv(fcoe->ctlr.lp);
		port->priority = prio;
	}

	return NOTIFY_OK;
}

static int fcoe_device_notification(struct notifier_block *notifier,
				    ulong event, void *ptr)
{
	struct fc_lport *lport = NULL;
	struct net_device *netdev = ptr;
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;
	struct fcoe_dev_stats *stats;
	u32 link_possible = 1;
	u32 mfs;
	int rc = NOTIFY_OK;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev == netdev) {
			lport = fcoe->ctlr.lp;
			break;
		}
	}
	if (!lport) {
		rc = NOTIFY_DONE;
		goto out;
	}

	switch (event) {
	case NETDEV_DOWN:
	case NETDEV_GOING_DOWN:
		link_possible = 0;
		break;
	case NETDEV_UP:
	case NETDEV_CHANGE:
		break;
	case NETDEV_CHANGEMTU:
		if (netdev->features & NETIF_F_FCOE_MTU)
			break;
		mfs = netdev->mtu - (sizeof(struct fcoe_hdr) +
				     sizeof(struct fcoe_crc_eof));
		if (mfs >= FC_MIN_MAX_FRAME)
			fc_set_mfs(lport, mfs);
		break;
	case NETDEV_REGISTER:
		break;
	case NETDEV_UNREGISTER:
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
		queue_work(fcoe_wq, &port->destroy_work);
		goto out;
		break;
	case NETDEV_FEAT_CHANGE:
		fcoe_netdev_features_change(lport, netdev);
		break;
	default:
		FCOE_NETDEV_DBG(netdev, "Unknown event %ld "
				"from netdev netlink\n", event);
	}

	fcoe_link_speed_update(lport);

	if (link_possible && !fcoe_link_ok(lport))
		fcoe_ctlr_link_up(&fcoe->ctlr);
	else if (fcoe_ctlr_link_down(&fcoe->ctlr)) {
		stats = per_cpu_ptr(lport->dev_stats, get_cpu());
		stats->LinkFailureCount++;
		put_cpu();
		fcoe_clean_pending_queue(lport);
	}
out:
	return rc;
}

static int fcoe_disable(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);

	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (fcoe) {
		fcoe_ctlr_link_down(&fcoe->ctlr);
		fcoe_clean_pending_queue(fcoe->ctlr.lp);
	} else
		rc = -ENODEV;

	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

static int fcoe_enable(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	rtnl_unlock();

	if (!fcoe)
		rc = -ENODEV;
	else if (!fcoe_link_ok(fcoe->ctlr.lp))
		fcoe_ctlr_link_up(&fcoe->ctlr);

	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

static int fcoe_destroy(struct net_device *netdev)
{
	struct fcoe_interface *fcoe;
	struct fc_lport *lport;
	struct fcoe_port *port;
	int rc = 0;

	mutex_lock(&fcoe_config_mutex);
	rtnl_lock();
	fcoe = fcoe_hostlist_lookup_port(netdev);
	if (!fcoe) {
		rc = -ENODEV;
		goto out_nodev;
	}
	lport = fcoe->ctlr.lp;
	port = lport_priv(lport);
	list_del(&fcoe->list);
	queue_work(fcoe_wq, &port->destroy_work);
out_nodev:
	rtnl_unlock();
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

static void fcoe_destroy_work(struct work_struct *work)
{
	struct fcoe_port *port;
	struct fcoe_interface *fcoe;

	port = container_of(work, struct fcoe_port, destroy_work);
	mutex_lock(&fcoe_config_mutex);

	fcoe = port->priv;
	fcoe_if_destroy(port->lport);
	fcoe_interface_cleanup(fcoe);

	mutex_unlock(&fcoe_config_mutex);
}

static bool fcoe_match(struct net_device *netdev)
{
	return true;
}

static void fcoe_dcb_create(struct fcoe_interface *fcoe)
{
#ifdef CONFIG_DCB
	int dcbx;
	u8 fup, up;
	struct net_device *netdev = fcoe->realdev;
	struct fcoe_port *port = lport_priv(fcoe->ctlr.lp);
	struct dcb_app app = {
				.priority = 0,
				.protocol = ETH_P_FCOE
			     };

	
	if (netdev && netdev->dcbnl_ops && netdev->dcbnl_ops->getdcbx) {
		dcbx = netdev->dcbnl_ops->getdcbx(netdev);

		if (dcbx & DCB_CAP_DCBX_VER_IEEE) {
			app.selector = IEEE_8021QAZ_APP_SEL_ETHERTYPE;
			up = dcb_ieee_getapp_mask(netdev, &app);
			app.protocol = ETH_P_FIP;
			fup = dcb_ieee_getapp_mask(netdev, &app);
		} else {
			app.selector = DCB_APP_IDTYPE_ETHTYPE;
			up = dcb_getapp(netdev, &app);
			app.protocol = ETH_P_FIP;
			fup = dcb_getapp(netdev, &app);
		}

		port->priority = ffs(up) ? ffs(up) - 1 : 0;
		fcoe->ctlr.priority = ffs(fup) ? ffs(fup) - 1 : port->priority;
	}
#endif
}

static int fcoe_create(struct net_device *netdev, enum fip_state fip_mode)
{
	int rc = 0;
	struct fcoe_interface *fcoe;
	struct fc_lport *lport;

	mutex_lock(&fcoe_config_mutex);
	rtnl_lock();

	
	if (fcoe_hostlist_lookup(netdev)) {
		rc = -EEXIST;
		goto out_nodev;
	}

	fcoe = fcoe_interface_create(netdev, fip_mode);
	if (IS_ERR(fcoe)) {
		rc = PTR_ERR(fcoe);
		goto out_nodev;
	}

	lport = fcoe_if_create(fcoe, &netdev->dev, 0);
	if (IS_ERR(lport)) {
		printk(KERN_ERR "fcoe: Failed to create interface (%s)\n",
		       netdev->name);
		rc = -EIO;
		rtnl_unlock();
		fcoe_interface_cleanup(fcoe);
		goto out_nortnl;
	}

	
	fcoe->ctlr.lp = lport;

	
	fcoe_dcb_create(fcoe);

	
	fcoe_hostlist_add(lport);

	
	lport->boot_time = jiffies;
	fc_fabric_login(lport);
	if (!fcoe_link_ok(lport)) {
		rtnl_unlock();
		fcoe_ctlr_link_up(&fcoe->ctlr);
		mutex_unlock(&fcoe_config_mutex);
		return rc;
	}

out_nodev:
	rtnl_unlock();
out_nortnl:
	mutex_unlock(&fcoe_config_mutex);
	return rc;
}

static int fcoe_link_speed_update(struct fc_lport *lport)
{
	struct net_device *netdev = fcoe_netdev(lport);
	struct ethtool_cmd ecmd;

	if (!__ethtool_get_settings(netdev, &ecmd)) {
		lport->link_supported_speeds &=
			~(FC_PORTSPEED_1GBIT | FC_PORTSPEED_10GBIT);
		if (ecmd.supported & (SUPPORTED_1000baseT_Half |
				      SUPPORTED_1000baseT_Full))
			lport->link_supported_speeds |= FC_PORTSPEED_1GBIT;
		if (ecmd.supported & SUPPORTED_10000baseT_Full)
			lport->link_supported_speeds |=
				FC_PORTSPEED_10GBIT;
		switch (ethtool_cmd_speed(&ecmd)) {
		case SPEED_1000:
			lport->link_speed = FC_PORTSPEED_1GBIT;
			break;
		case SPEED_10000:
			lport->link_speed = FC_PORTSPEED_10GBIT;
			break;
		}
		return 0;
	}
	return -1;
}

static int fcoe_link_ok(struct fc_lport *lport)
{
	struct net_device *netdev = fcoe_netdev(lport);

	if (netif_oper_up(netdev))
		return 0;
	return -1;
}

static void fcoe_percpu_clean(struct fc_lport *lport)
{
	struct fcoe_percpu_s *pp;
	struct sk_buff *skb;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		pp = &per_cpu(fcoe_percpu, cpu);

		if (!pp->thread || !cpu_online(cpu))
			continue;

		skb = dev_alloc_skb(0);
		if (!skb) {
			spin_unlock_bh(&pp->fcoe_rx_list.lock);
			continue;
		}
		skb->destructor = fcoe_percpu_flush_done;

		spin_lock_bh(&pp->fcoe_rx_list.lock);
		__skb_queue_tail(&pp->fcoe_rx_list, skb);
		if (pp->fcoe_rx_list.qlen == 1)
			wake_up_process(pp->thread);
		spin_unlock_bh(&pp->fcoe_rx_list.lock);

		wait_for_completion(&fcoe_flush_completion);
	}
}

static int fcoe_reset(struct Scsi_Host *shost)
{
	struct fc_lport *lport = shost_priv(shost);
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;

	fcoe_ctlr_link_down(&fcoe->ctlr);
	fcoe_clean_pending_queue(fcoe->ctlr.lp);
	if (!fcoe_link_ok(fcoe->ctlr.lp))
		fcoe_ctlr_link_up(&fcoe->ctlr);
	return 0;
}

static struct fcoe_interface *
fcoe_hostlist_lookup_port(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	list_for_each_entry(fcoe, &fcoe_hostlist, list) {
		if (fcoe->netdev == netdev)
			return fcoe;
	}
	return NULL;
}

static struct fc_lport *fcoe_hostlist_lookup(const struct net_device *netdev)
{
	struct fcoe_interface *fcoe;

	fcoe = fcoe_hostlist_lookup_port(netdev);
	return (fcoe) ? fcoe->ctlr.lp : NULL;
}

static int fcoe_hostlist_add(const struct fc_lport *lport)
{
	struct fcoe_interface *fcoe;
	struct fcoe_port *port;

	fcoe = fcoe_hostlist_lookup_port(fcoe_netdev(lport));
	if (!fcoe) {
		port = lport_priv(lport);
		fcoe = port->priv;
		list_add_tail(&fcoe->list, &fcoe_hostlist);
	}
	return 0;
}


static struct fcoe_transport fcoe_sw_transport = {
	.name = {FCOE_TRANSPORT_DEFAULT},
	.attached = false,
	.list = LIST_HEAD_INIT(fcoe_sw_transport.list),
	.match = fcoe_match,
	.create = fcoe_create,
	.destroy = fcoe_destroy,
	.enable = fcoe_enable,
	.disable = fcoe_disable,
};

static int __init fcoe_init(void)
{
	struct fcoe_percpu_s *p;
	unsigned int cpu;
	int rc = 0;

	fcoe_wq = alloc_workqueue("fcoe", 0, 0);
	if (!fcoe_wq)
		return -ENOMEM;

	
	rc = fcoe_transport_attach(&fcoe_sw_transport);
	if (rc) {
		printk(KERN_ERR "failed to register an fcoe transport, check "
			"if libfcoe is loaded\n");
		return rc;
	}

	mutex_lock(&fcoe_config_mutex);

	for_each_possible_cpu(cpu) {
		p = &per_cpu(fcoe_percpu, cpu);
		skb_queue_head_init(&p->fcoe_rx_list);
	}

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_create(cpu);

	
	rc = register_hotcpu_notifier(&fcoe_cpu_notifier);
	if (rc)
		goto out_free;

	
	fcoe_dev_setup();

	rc = fcoe_if_init();
	if (rc)
		goto out_free;

	mutex_unlock(&fcoe_config_mutex);
	return 0;

out_free:
	for_each_online_cpu(cpu) {
		fcoe_percpu_thread_destroy(cpu);
	}
	mutex_unlock(&fcoe_config_mutex);
	destroy_workqueue(fcoe_wq);
	return rc;
}
module_init(fcoe_init);

static void __exit fcoe_exit(void)
{
	struct fcoe_interface *fcoe, *tmp;
	struct fcoe_port *port;
	unsigned int cpu;

	mutex_lock(&fcoe_config_mutex);

	fcoe_dev_cleanup();

	
	rtnl_lock();
	list_for_each_entry_safe(fcoe, tmp, &fcoe_hostlist, list) {
		list_del(&fcoe->list);
		port = lport_priv(fcoe->ctlr.lp);
		queue_work(fcoe_wq, &port->destroy_work);
	}
	rtnl_unlock();

	unregister_hotcpu_notifier(&fcoe_cpu_notifier);

	for_each_online_cpu(cpu)
		fcoe_percpu_thread_destroy(cpu);

	mutex_unlock(&fcoe_config_mutex);

	destroy_workqueue(fcoe_wq);

	fcoe_if_exit();

	
	fcoe_transport_detach(&fcoe_sw_transport);
}
module_exit(fcoe_exit);

static void fcoe_flogi_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fcoe_ctlr *fip = arg;
	struct fc_exch *exch = fc_seq_exch(seq);
	struct fc_lport *lport = exch->lp;
	u8 *mac;

	if (IS_ERR(fp))
		goto done;

	mac = fr_cb(fp)->granted_mac;
	
	if (is_zero_ether_addr(mac))
		fcoe_ctlr_recv_flogi(fip, lport, fp);
	if (!is_zero_ether_addr(mac))
		fcoe_update_src_mac(lport, mac);
done:
	fc_lport_flogi_resp(seq, fp, lport);
}

static void fcoe_logo_resp(struct fc_seq *seq, struct fc_frame *fp, void *arg)
{
	struct fc_lport *lport = arg;
	static u8 zero_mac[ETH_ALEN] = { 0 };

	if (!IS_ERR(fp))
		fcoe_update_src_mac(lport, zero_mac);
	fc_lport_logo_resp(seq, fp, lport);
}

static struct fc_seq *fcoe_elsct_send(struct fc_lport *lport, u32 did,
				      struct fc_frame *fp, unsigned int op,
				      void (*resp)(struct fc_seq *,
						   struct fc_frame *,
						   void *),
				      void *arg, u32 timeout)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;
	struct fcoe_ctlr *fip = &fcoe->ctlr;
	struct fc_frame_header *fh = fc_frame_header_get(fp);

	switch (op) {
	case ELS_FLOGI:
	case ELS_FDISC:
		if (lport->point_to_multipoint)
			break;
		return fc_elsct_send(lport, did, fp, op, fcoe_flogi_resp,
				     fip, timeout);
	case ELS_LOGO:
		
		if (ntoh24(fh->fh_d_id) != FC_FID_FLOGI)
			break;
		return fc_elsct_send(lport, did, fp, op, fcoe_logo_resp,
				     lport, timeout);
	}
	return fc_elsct_send(lport, did, fp, op, resp, arg, timeout);
}

static int fcoe_vport_create(struct fc_vport *vport, bool disabled)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fcoe_port *port = lport_priv(n_port);
	struct fcoe_interface *fcoe = port->priv;
	struct net_device *netdev = fcoe->netdev;
	struct fc_lport *vn_port;
	int rc;
	char buf[32];

	rc = fcoe_validate_vport_create(vport);
	if (rc) {
		fcoe_wwn_to_str(vport->port_name, buf, sizeof(buf));
		printk(KERN_ERR "fcoe: Failed to create vport, "
			"WWPN (0x%s) already exists\n",
			buf);
		return rc;
	}

	mutex_lock(&fcoe_config_mutex);
	rtnl_lock();
	vn_port = fcoe_if_create(fcoe, &vport->dev, 1);
	rtnl_unlock();
	mutex_unlock(&fcoe_config_mutex);

	if (IS_ERR(vn_port)) {
		printk(KERN_ERR "fcoe: fcoe_vport_create(%s) failed\n",
		       netdev->name);
		return -EIO;
	}

	if (disabled) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
	} else {
		vn_port->boot_time = jiffies;
		fc_fabric_login(vn_port);
		fc_vport_setlink(vn_port);
	}
	return 0;
}

static int fcoe_vport_destroy(struct fc_vport *vport)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_lport *n_port = shost_priv(shost);
	struct fc_lport *vn_port = vport->dd_data;

	mutex_lock(&n_port->lp_mutex);
	list_del(&vn_port->list);
	mutex_unlock(&n_port->lp_mutex);

	mutex_lock(&fcoe_config_mutex);
	fcoe_if_destroy(vn_port);
	mutex_unlock(&fcoe_config_mutex);

	return 0;
}

static int fcoe_vport_disable(struct fc_vport *vport, bool disable)
{
	struct fc_lport *lport = vport->dd_data;

	if (disable) {
		fc_vport_set_state(vport, FC_VPORT_DISABLED);
		fc_fabric_logoff(lport);
	} else {
		lport->boot_time = jiffies;
		fc_fabric_login(lport);
		fc_vport_setlink(lport);
	}

	return 0;
}

static void fcoe_set_vport_symbolic_name(struct fc_vport *vport)
{
	struct fc_lport *lport = vport->dd_data;
	struct fc_frame *fp;
	size_t len;

	snprintf(fc_host_symbolic_name(lport->host), FC_SYMBOLIC_NAME_SIZE,
		 "%s v%s over %s : %s", FCOE_NAME, FCOE_VERSION,
		 fcoe_netdev(lport)->name, vport->symbolic_name);

	if (lport->state != LPORT_ST_READY)
		return;

	len = strnlen(fc_host_symbolic_name(lport->host), 255);
	fp = fc_frame_alloc(lport,
			    sizeof(struct fc_ct_hdr) +
			    sizeof(struct fc_ns_rspn) + len);
	if (!fp)
		return;
	lport->tt.elsct_send(lport, FC_FID_DIR_SERV, fp, FC_NS_RSPN_ID,
			     NULL, NULL, 3 * lport->r_a_tov);
}

static void fcoe_get_lesb(struct fc_lport *lport,
			 struct fc_els_lesb *fc_lesb)
{
	struct net_device *netdev = fcoe_netdev(lport);

	__fcoe_get_lesb(lport, fc_lesb, netdev);
}

static void fcoe_set_port_id(struct fc_lport *lport,
			     u32 port_id, struct fc_frame *fp)
{
	struct fcoe_port *port = lport_priv(lport);
	struct fcoe_interface *fcoe = port->priv;

	if (fp && fc_frame_payload_op(fp) == ELS_FLOGI)
		fcoe_ctlr_recv_flogi(&fcoe->ctlr, lport, fp);
}
