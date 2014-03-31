/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2010 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/iscsi_boot_sysfs.h>
#include <linux/inet.h>

#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>

#include "ql4_def.h"
#include "ql4_version.h"
#include "ql4_glbl.h"
#include "ql4_dbg.h"
#include "ql4_inline.h"

static char qla4xxx_version_str[40];

static struct kmem_cache *srb_cachep;

static int ql4xdisablesysfsboot = 1;
module_param(ql4xdisablesysfsboot, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ql4xdisablesysfsboot,
		 " Set to disable exporting boot targets to sysfs.\n"
		 "\t\t  0 - Export boot targets\n"
		 "\t\t  1 - Do not export boot targets (Default)");

int ql4xdontresethba;
module_param(ql4xdontresethba, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ql4xdontresethba,
		 " Don't reset the HBA for driver recovery.\n"
		 "\t\t  0 - It will reset HBA (Default)\n"
		 "\t\t  1 - It will NOT reset HBA");

int ql4xextended_error_logging;
module_param(ql4xextended_error_logging, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ql4xextended_error_logging,
		 " Option to enable extended error logging.\n"
		 "\t\t  0 - no logging (Default)\n"
		 "\t\t  2 - debug logging");

int ql4xenablemsix = 1;
module_param(ql4xenablemsix, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ql4xenablemsix,
		 " Set to enable MSI or MSI-X interrupt mechanism.\n"
		 "\t\t  0 = enable INTx interrupt mechanism.\n"
		 "\t\t  1 = enable MSI-X interrupt mechanism (Default).\n"
		 "\t\t  2 = enable MSI interrupt mechanism.");

#define QL4_DEF_QDEPTH 32
static int ql4xmaxqdepth = QL4_DEF_QDEPTH;
module_param(ql4xmaxqdepth, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ql4xmaxqdepth,
		 " Maximum queue depth to report for target devices.\n"
		 "\t\t  Default: 32.");

static int ql4xsess_recovery_tmo = QL4_SESS_RECOVERY_TMO;
module_param(ql4xsess_recovery_tmo, int, S_IRUGO);
MODULE_PARM_DESC(ql4xsess_recovery_tmo,
		" Target Session Recovery Timeout.\n"
		"\t\t  Default: 120 sec.");

static int qla4xxx_wait_for_hba_online(struct scsi_qla_host *ha);
static void qla4xxx_config_dma_addressing(struct scsi_qla_host *ha);

static int qla4xxx_session_get_param(struct iscsi_cls_session *cls_sess,
				     enum iscsi_param param, char *buf);
static int qla4xxx_conn_get_param(struct iscsi_cls_conn *conn,
				  enum iscsi_param param, char *buf);
static int qla4xxx_host_get_param(struct Scsi_Host *shost,
				  enum iscsi_host_param param, char *buf);
static int qla4xxx_iface_set_param(struct Scsi_Host *shost, void *data,
				   uint32_t len);
static int qla4xxx_get_iface_param(struct iscsi_iface *iface,
				   enum iscsi_param_type param_type,
				   int param, char *buf);
static enum blk_eh_timer_return qla4xxx_eh_cmd_timed_out(struct scsi_cmnd *sc);
static struct iscsi_endpoint *qla4xxx_ep_connect(struct Scsi_Host *shost,
						 struct sockaddr *dst_addr,
						 int non_blocking);
static int qla4xxx_ep_poll(struct iscsi_endpoint *ep, int timeout_ms);
static void qla4xxx_ep_disconnect(struct iscsi_endpoint *ep);
static int qla4xxx_get_ep_param(struct iscsi_endpoint *ep,
				enum iscsi_param param, char *buf);
static int qla4xxx_conn_start(struct iscsi_cls_conn *conn);
static struct iscsi_cls_conn *
qla4xxx_conn_create(struct iscsi_cls_session *cls_sess, uint32_t conn_idx);
static int qla4xxx_conn_bind(struct iscsi_cls_session *cls_session,
			     struct iscsi_cls_conn *cls_conn,
			     uint64_t transport_fd, int is_leading);
static void qla4xxx_conn_destroy(struct iscsi_cls_conn *conn);
static struct iscsi_cls_session *
qla4xxx_session_create(struct iscsi_endpoint *ep, uint16_t cmds_max,
			uint16_t qdepth, uint32_t initial_cmdsn);
static void qla4xxx_session_destroy(struct iscsi_cls_session *sess);
static void qla4xxx_task_work(struct work_struct *wdata);
static int qla4xxx_alloc_pdu(struct iscsi_task *, uint8_t);
static int qla4xxx_task_xmit(struct iscsi_task *);
static void qla4xxx_task_cleanup(struct iscsi_task *);
static void qla4xxx_fail_session(struct iscsi_cls_session *cls_session);
static void qla4xxx_conn_get_stats(struct iscsi_cls_conn *cls_conn,
				   struct iscsi_stats *stats);
static int qla4xxx_send_ping(struct Scsi_Host *shost, uint32_t iface_num,
			     uint32_t iface_type, uint32_t payload_size,
			     uint32_t pid, struct sockaddr *dst_addr);
static int qla4xxx_get_chap_list(struct Scsi_Host *shost, uint16_t chap_tbl_idx,
				 uint32_t *num_entries, char *buf);
static int qla4xxx_delete_chap(struct Scsi_Host *shost, uint16_t chap_tbl_idx);

static int qla4xxx_queuecommand(struct Scsi_Host *h, struct scsi_cmnd *cmd);
static int qla4xxx_eh_abort(struct scsi_cmnd *cmd);
static int qla4xxx_eh_device_reset(struct scsi_cmnd *cmd);
static int qla4xxx_eh_target_reset(struct scsi_cmnd *cmd);
static int qla4xxx_eh_host_reset(struct scsi_cmnd *cmd);
static int qla4xxx_slave_alloc(struct scsi_device *device);
static int qla4xxx_slave_configure(struct scsi_device *device);
static void qla4xxx_slave_destroy(struct scsi_device *sdev);
static umode_t ql4_attr_is_visible(int param_type, int param);
static int qla4xxx_host_reset(struct Scsi_Host *shost, int reset_type);

static struct qla4_8xxx_legacy_intr_set legacy_intr[] =
    QLA82XX_LEGACY_INTR_CONFIG;

static struct scsi_host_template qla4xxx_driver_template = {
	.module			= THIS_MODULE,
	.name			= DRIVER_NAME,
	.proc_name		= DRIVER_NAME,
	.queuecommand		= qla4xxx_queuecommand,

	.eh_abort_handler	= qla4xxx_eh_abort,
	.eh_device_reset_handler = qla4xxx_eh_device_reset,
	.eh_target_reset_handler = qla4xxx_eh_target_reset,
	.eh_host_reset_handler	= qla4xxx_eh_host_reset,
	.eh_timed_out		= qla4xxx_eh_cmd_timed_out,

	.slave_configure	= qla4xxx_slave_configure,
	.slave_alloc		= qla4xxx_slave_alloc,
	.slave_destroy		= qla4xxx_slave_destroy,

	.this_id		= -1,
	.cmd_per_lun		= 3,
	.use_clustering		= ENABLE_CLUSTERING,
	.sg_tablesize		= SG_ALL,

	.max_sectors		= 0xFFFF,
	.shost_attrs		= qla4xxx_host_attrs,
	.host_reset		= qla4xxx_host_reset,
	.vendor_id		= SCSI_NL_VID_TYPE_PCI | PCI_VENDOR_ID_QLOGIC,
};

static struct iscsi_transport qla4xxx_iscsi_transport = {
	.owner			= THIS_MODULE,
	.name			= DRIVER_NAME,
	.caps			= CAP_TEXT_NEGO |
				  CAP_DATA_PATH_OFFLOAD | CAP_HDRDGST |
				  CAP_DATADGST | CAP_LOGIN_OFFLOAD |
				  CAP_MULTI_R2T,
	.attr_is_visible	= ql4_attr_is_visible,
	.create_session         = qla4xxx_session_create,
	.destroy_session        = qla4xxx_session_destroy,
	.start_conn             = qla4xxx_conn_start,
	.create_conn            = qla4xxx_conn_create,
	.bind_conn              = qla4xxx_conn_bind,
	.stop_conn              = iscsi_conn_stop,
	.destroy_conn           = qla4xxx_conn_destroy,
	.set_param              = iscsi_set_param,
	.get_conn_param		= qla4xxx_conn_get_param,
	.get_session_param	= qla4xxx_session_get_param,
	.get_ep_param           = qla4xxx_get_ep_param,
	.ep_connect		= qla4xxx_ep_connect,
	.ep_poll		= qla4xxx_ep_poll,
	.ep_disconnect		= qla4xxx_ep_disconnect,
	.get_stats		= qla4xxx_conn_get_stats,
	.send_pdu		= iscsi_conn_send_pdu,
	.xmit_task		= qla4xxx_task_xmit,
	.cleanup_task		= qla4xxx_task_cleanup,
	.alloc_pdu		= qla4xxx_alloc_pdu,

	.get_host_param		= qla4xxx_host_get_param,
	.set_iface_param	= qla4xxx_iface_set_param,
	.get_iface_param	= qla4xxx_get_iface_param,
	.bsg_request		= qla4xxx_bsg_request,
	.send_ping		= qla4xxx_send_ping,
	.get_chap		= qla4xxx_get_chap_list,
	.delete_chap		= qla4xxx_delete_chap,
};

static struct scsi_transport_template *qla4xxx_scsi_transport;

static int qla4xxx_send_ping(struct Scsi_Host *shost, uint32_t iface_num,
			     uint32_t iface_type, uint32_t payload_size,
			     uint32_t pid, struct sockaddr *dst_addr)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	struct sockaddr_in *addr;
	struct sockaddr_in6 *addr6;
	uint32_t options = 0;
	uint8_t ipaddr[IPv6_ADDR_LEN];
	int rval;

	memset(ipaddr, 0, IPv6_ADDR_LEN);
	
	if ((iface_type == ISCSI_IFACE_TYPE_IPV4) &&
	    (dst_addr->sa_family == AF_INET)) {
		addr = (struct sockaddr_in *)dst_addr;
		memcpy(ipaddr, &addr->sin_addr.s_addr, IP_ADDR_LEN);
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: IPv4 Ping src: %pI4 "
				  "dest: %pI4\n", __func__,
				  &ha->ip_config.ip_address, ipaddr));
		rval = qla4xxx_ping_iocb(ha, options, payload_size, pid,
					 ipaddr);
		if (rval)
			rval = -EINVAL;
	} else if ((iface_type == ISCSI_IFACE_TYPE_IPV6) &&
		   (dst_addr->sa_family == AF_INET6)) {
		
		addr6 = (struct sockaddr_in6 *)dst_addr;
		memcpy(ipaddr, &addr6->sin6_addr.in6_u.u6_addr8, IPv6_ADDR_LEN);

		options |= PING_IPV6_PROTOCOL_ENABLE;

		
		if ((iface_num == 0) || (iface_num == 1)) {
			DEBUG2(ql4_printk(KERN_INFO, ha, "%s: LinkLocal Ping "
					  "src: %pI6 dest: %pI6\n", __func__,
					  &ha->ip_config.ipv6_link_local_addr,
					  ipaddr));
			options |= PING_IPV6_LINKLOCAL_ADDR;
			rval = qla4xxx_ping_iocb(ha, options, payload_size,
						 pid, ipaddr);
		} else {
			ql4_printk(KERN_WARNING, ha, "%s: iface num = %d "
				   "not supported\n", __func__, iface_num);
			rval = -ENOSYS;
			goto exit_send_ping;
		}

		if (rval != QLA_SUCCESS) {
			options &= ~PING_IPV6_LINKLOCAL_ADDR;
			if (iface_num == 0) {
				options |= PING_IPV6_ADDR0;
				DEBUG2(ql4_printk(KERN_INFO, ha, "%s: IPv6 "
						  "Ping src: %pI6 "
						  "dest: %pI6\n", __func__,
						  &ha->ip_config.ipv6_addr0,
						  ipaddr));
			} else if (iface_num == 1) {
				options |= PING_IPV6_ADDR1;
				DEBUG2(ql4_printk(KERN_INFO, ha, "%s: IPv6 "
						  "Ping src: %pI6 "
						  "dest: %pI6\n", __func__,
						  &ha->ip_config.ipv6_addr1,
						  ipaddr));
			}
			rval = qla4xxx_ping_iocb(ha, options, payload_size,
						 pid, ipaddr);
			if (rval)
				rval = -EINVAL;
		}
	} else
		rval = -ENOSYS;
exit_send_ping:
	return rval;
}

static umode_t ql4_attr_is_visible(int param_type, int param)
{
	switch (param_type) {
	case ISCSI_HOST_PARAM:
		switch (param) {
		case ISCSI_HOST_PARAM_HWADDRESS:
		case ISCSI_HOST_PARAM_IPADDRESS:
		case ISCSI_HOST_PARAM_INITIATOR_NAME:
		case ISCSI_HOST_PARAM_PORT_STATE:
		case ISCSI_HOST_PARAM_PORT_SPEED:
			return S_IRUGO;
		default:
			return 0;
		}
	case ISCSI_PARAM:
		switch (param) {
		case ISCSI_PARAM_PERSISTENT_ADDRESS:
		case ISCSI_PARAM_PERSISTENT_PORT:
		case ISCSI_PARAM_CONN_ADDRESS:
		case ISCSI_PARAM_CONN_PORT:
		case ISCSI_PARAM_TARGET_NAME:
		case ISCSI_PARAM_TPGT:
		case ISCSI_PARAM_TARGET_ALIAS:
		case ISCSI_PARAM_MAX_BURST:
		case ISCSI_PARAM_MAX_R2T:
		case ISCSI_PARAM_FIRST_BURST:
		case ISCSI_PARAM_MAX_RECV_DLENGTH:
		case ISCSI_PARAM_MAX_XMIT_DLENGTH:
		case ISCSI_PARAM_IFACE_NAME:
		case ISCSI_PARAM_CHAP_OUT_IDX:
		case ISCSI_PARAM_CHAP_IN_IDX:
		case ISCSI_PARAM_USERNAME:
		case ISCSI_PARAM_PASSWORD:
		case ISCSI_PARAM_USERNAME_IN:
		case ISCSI_PARAM_PASSWORD_IN:
			return S_IRUGO;
		default:
			return 0;
		}
	case ISCSI_NET_PARAM:
		switch (param) {
		case ISCSI_NET_PARAM_IPV4_ADDR:
		case ISCSI_NET_PARAM_IPV4_SUBNET:
		case ISCSI_NET_PARAM_IPV4_GW:
		case ISCSI_NET_PARAM_IPV4_BOOTPROTO:
		case ISCSI_NET_PARAM_IFACE_ENABLE:
		case ISCSI_NET_PARAM_IPV6_LINKLOCAL:
		case ISCSI_NET_PARAM_IPV6_ADDR:
		case ISCSI_NET_PARAM_IPV6_ROUTER:
		case ISCSI_NET_PARAM_IPV6_ADDR_AUTOCFG:
		case ISCSI_NET_PARAM_IPV6_LINKLOCAL_AUTOCFG:
		case ISCSI_NET_PARAM_VLAN_ID:
		case ISCSI_NET_PARAM_VLAN_PRIORITY:
		case ISCSI_NET_PARAM_VLAN_ENABLED:
		case ISCSI_NET_PARAM_MTU:
		case ISCSI_NET_PARAM_PORT:
			return S_IRUGO;
		default:
			return 0;
		}
	}

	return 0;
}

static int qla4xxx_get_chap_list(struct Scsi_Host *shost, uint16_t chap_tbl_idx,
				  uint32_t *num_entries, char *buf)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	struct ql4_chap_table *chap_table;
	struct iscsi_chap_rec *chap_rec;
	int max_chap_entries = 0;
	int valid_chap_entries = 0;
	int ret = 0, i;

	if (is_qla8022(ha))
		max_chap_entries = (ha->hw.flt_chap_size / 2) /
					sizeof(struct ql4_chap_table);
	else
		max_chap_entries = MAX_CHAP_ENTRIES_40XX;

	ql4_printk(KERN_INFO, ha, "%s: num_entries = %d, CHAP idx = %d\n",
			__func__, *num_entries, chap_tbl_idx);

	if (!buf) {
		ret = -ENOMEM;
		goto exit_get_chap_list;
	}

	chap_rec = (struct iscsi_chap_rec *) buf;
	mutex_lock(&ha->chap_sem);
	for (i = chap_tbl_idx; i < max_chap_entries; i++) {
		chap_table = (struct ql4_chap_table *)ha->chap_list + i;
		if (chap_table->cookie !=
		    __constant_cpu_to_le16(CHAP_VALID_COOKIE))
			continue;

		chap_rec->chap_tbl_idx = i;
		strncpy(chap_rec->username, chap_table->name,
			ISCSI_CHAP_AUTH_NAME_MAX_LEN);
		strncpy(chap_rec->password, chap_table->secret,
			QL4_CHAP_MAX_SECRET_LEN);
		chap_rec->password_length = chap_table->secret_len;

		if (chap_table->flags & BIT_7) 
			chap_rec->chap_type = CHAP_TYPE_OUT;

		if (chap_table->flags & BIT_6) 
			chap_rec->chap_type = CHAP_TYPE_IN;

		chap_rec++;

		valid_chap_entries++;
		if (valid_chap_entries == *num_entries)
			break;
		else
			continue;
	}
	mutex_unlock(&ha->chap_sem);

exit_get_chap_list:
	ql4_printk(KERN_INFO, ha, "%s: Valid CHAP Entries = %d\n",
			__func__,  valid_chap_entries);
	*num_entries = valid_chap_entries;
	return ret;
}

static int __qla4xxx_is_chap_active(struct device *dev, void *data)
{
	int ret = 0;
	uint16_t *chap_tbl_idx = (uint16_t *) data;
	struct iscsi_cls_session *cls_session;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;

	if (!iscsi_is_session_dev(dev))
		goto exit_is_chap_active;

	cls_session = iscsi_dev_to_session(dev);
	sess = cls_session->dd_data;
	ddb_entry = sess->dd_data;

	if (iscsi_session_chkready(cls_session))
		goto exit_is_chap_active;

	if (ddb_entry->chap_tbl_idx == *chap_tbl_idx)
		ret = 1;

exit_is_chap_active:
	return ret;
}

static int qla4xxx_is_chap_active(struct Scsi_Host *shost,
				  uint16_t chap_tbl_idx)
{
	int ret = 0;

	ret = device_for_each_child(&shost->shost_gendev, &chap_tbl_idx,
				    __qla4xxx_is_chap_active);

	return ret;
}

static int qla4xxx_delete_chap(struct Scsi_Host *shost, uint16_t chap_tbl_idx)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	struct ql4_chap_table *chap_table;
	dma_addr_t chap_dma;
	int max_chap_entries = 0;
	uint32_t offset = 0;
	uint32_t chap_size;
	int ret = 0;

	chap_table = dma_pool_alloc(ha->chap_dma_pool, GFP_KERNEL, &chap_dma);
	if (chap_table == NULL)
		return -ENOMEM;

	memset(chap_table, 0, sizeof(struct ql4_chap_table));

	if (is_qla8022(ha))
		max_chap_entries = (ha->hw.flt_chap_size / 2) /
				   sizeof(struct ql4_chap_table);
	else
		max_chap_entries = MAX_CHAP_ENTRIES_40XX;

	if (chap_tbl_idx > max_chap_entries) {
		ret = -EINVAL;
		goto exit_delete_chap;
	}

	ret = qla4xxx_is_chap_active(shost, chap_tbl_idx);
	if (ret) {
		ql4_printk(KERN_INFO, ha, "CHAP entry %d is in use, cannot "
			   "delete from flash\n", chap_tbl_idx);
		ret = -EBUSY;
		goto exit_delete_chap;
	}

	chap_size = sizeof(struct ql4_chap_table);
	if (is_qla40XX(ha))
		offset = FLASH_CHAP_OFFSET | (chap_tbl_idx * chap_size);
	else {
		offset = FLASH_RAW_ACCESS_ADDR + (ha->hw.flt_region_chap << 2);
		if (ha->port_num == 1)
			offset += (ha->hw.flt_chap_size / 2);
		offset += (chap_tbl_idx * chap_size);
	}

	ret = qla4xxx_get_flash(ha, chap_dma, offset, chap_size);
	if (ret != QLA_SUCCESS) {
		ret = -EINVAL;
		goto exit_delete_chap;
	}

	DEBUG2(ql4_printk(KERN_INFO, ha, "Chap Cookie: x%x\n",
			  __le16_to_cpu(chap_table->cookie)));

	if (__le16_to_cpu(chap_table->cookie) != CHAP_VALID_COOKIE) {
		ql4_printk(KERN_ERR, ha, "No valid chap entry found\n");
		goto exit_delete_chap;
	}

	chap_table->cookie = __constant_cpu_to_le16(0xFFFF);

	offset = FLASH_CHAP_OFFSET |
			(chap_tbl_idx * sizeof(struct ql4_chap_table));
	ret = qla4xxx_set_flash(ha, chap_dma, offset, chap_size,
				FLASH_OPT_RMW_COMMIT);
	if (ret == QLA_SUCCESS && ha->chap_list) {
		mutex_lock(&ha->chap_sem);
		
		memcpy((struct ql4_chap_table *)ha->chap_list + chap_tbl_idx,
			chap_table, sizeof(struct ql4_chap_table));
		mutex_unlock(&ha->chap_sem);
	}
	if (ret != QLA_SUCCESS)
		ret =  -EINVAL;

exit_delete_chap:
	dma_pool_free(ha->chap_dma_pool, chap_table, chap_dma);
	return ret;
}

static int qla4xxx_get_iface_param(struct iscsi_iface *iface,
				   enum iscsi_param_type param_type,
				   int param, char *buf)
{
	struct Scsi_Host *shost = iscsi_iface_to_shost(iface);
	struct scsi_qla_host *ha = to_qla_host(shost);
	int len = -ENOSYS;

	if (param_type != ISCSI_NET_PARAM)
		return -ENOSYS;

	switch (param) {
	case ISCSI_NET_PARAM_IPV4_ADDR:
		len = sprintf(buf, "%pI4\n", &ha->ip_config.ip_address);
		break;
	case ISCSI_NET_PARAM_IPV4_SUBNET:
		len = sprintf(buf, "%pI4\n", &ha->ip_config.subnet_mask);
		break;
	case ISCSI_NET_PARAM_IPV4_GW:
		len = sprintf(buf, "%pI4\n", &ha->ip_config.gateway);
		break;
	case ISCSI_NET_PARAM_IFACE_ENABLE:
		if (iface->iface_type == ISCSI_IFACE_TYPE_IPV4)
			len = sprintf(buf, "%s\n",
				      (ha->ip_config.ipv4_options &
				       IPOPT_IPV4_PROTOCOL_ENABLE) ?
				      "enabled" : "disabled");
		else if (iface->iface_type == ISCSI_IFACE_TYPE_IPV6)
			len = sprintf(buf, "%s\n",
				      (ha->ip_config.ipv6_options &
				       IPV6_OPT_IPV6_PROTOCOL_ENABLE) ?
				       "enabled" : "disabled");
		break;
	case ISCSI_NET_PARAM_IPV4_BOOTPROTO:
		len = sprintf(buf, "%s\n",
			      (ha->ip_config.tcp_options & TCPOPT_DHCP_ENABLE) ?
			      "dhcp" : "static");
		break;
	case ISCSI_NET_PARAM_IPV6_ADDR:
		if (iface->iface_num == 0)
			len = sprintf(buf, "%pI6\n", &ha->ip_config.ipv6_addr0);
		if (iface->iface_num == 1)
			len = sprintf(buf, "%pI6\n", &ha->ip_config.ipv6_addr1);
		break;
	case ISCSI_NET_PARAM_IPV6_LINKLOCAL:
		len = sprintf(buf, "%pI6\n",
			      &ha->ip_config.ipv6_link_local_addr);
		break;
	case ISCSI_NET_PARAM_IPV6_ROUTER:
		len = sprintf(buf, "%pI6\n",
			      &ha->ip_config.ipv6_default_router_addr);
		break;
	case ISCSI_NET_PARAM_IPV6_ADDR_AUTOCFG:
		len = sprintf(buf, "%s\n",
			      (ha->ip_config.ipv6_addl_options &
			       IPV6_ADDOPT_NEIGHBOR_DISCOVERY_ADDR_ENABLE) ?
			       "nd" : "static");
		break;
	case ISCSI_NET_PARAM_IPV6_LINKLOCAL_AUTOCFG:
		len = sprintf(buf, "%s\n",
			      (ha->ip_config.ipv6_addl_options &
			       IPV6_ADDOPT_AUTOCONFIG_LINK_LOCAL_ADDR) ?
			       "auto" : "static");
		break;
	case ISCSI_NET_PARAM_VLAN_ID:
		if (iface->iface_type == ISCSI_IFACE_TYPE_IPV4)
			len = sprintf(buf, "%d\n",
				      (ha->ip_config.ipv4_vlan_tag &
				       ISCSI_MAX_VLAN_ID));
		else if (iface->iface_type == ISCSI_IFACE_TYPE_IPV6)
			len = sprintf(buf, "%d\n",
				      (ha->ip_config.ipv6_vlan_tag &
				       ISCSI_MAX_VLAN_ID));
		break;
	case ISCSI_NET_PARAM_VLAN_PRIORITY:
		if (iface->iface_type == ISCSI_IFACE_TYPE_IPV4)
			len = sprintf(buf, "%d\n",
				      ((ha->ip_config.ipv4_vlan_tag >> 13) &
					ISCSI_MAX_VLAN_PRIORITY));
		else if (iface->iface_type == ISCSI_IFACE_TYPE_IPV6)
			len = sprintf(buf, "%d\n",
				      ((ha->ip_config.ipv6_vlan_tag >> 13) &
					ISCSI_MAX_VLAN_PRIORITY));
		break;
	case ISCSI_NET_PARAM_VLAN_ENABLED:
		if (iface->iface_type == ISCSI_IFACE_TYPE_IPV4)
			len = sprintf(buf, "%s\n",
				      (ha->ip_config.ipv4_options &
				       IPOPT_VLAN_TAGGING_ENABLE) ?
				       "enabled" : "disabled");
		else if (iface->iface_type == ISCSI_IFACE_TYPE_IPV6)
			len = sprintf(buf, "%s\n",
				      (ha->ip_config.ipv6_options &
				       IPV6_OPT_VLAN_TAGGING_ENABLE) ?
				       "enabled" : "disabled");
		break;
	case ISCSI_NET_PARAM_MTU:
		len = sprintf(buf, "%d\n", ha->ip_config.eth_mtu_size);
		break;
	case ISCSI_NET_PARAM_PORT:
		if (iface->iface_type == ISCSI_IFACE_TYPE_IPV4)
			len = sprintf(buf, "%d\n", ha->ip_config.ipv4_port);
		else if (iface->iface_type == ISCSI_IFACE_TYPE_IPV6)
			len = sprintf(buf, "%d\n", ha->ip_config.ipv6_port);
		break;
	default:
		len = -ENOSYS;
	}

	return len;
}

static struct iscsi_endpoint *
qla4xxx_ep_connect(struct Scsi_Host *shost, struct sockaddr *dst_addr,
		   int non_blocking)
{
	int ret;
	struct iscsi_endpoint *ep;
	struct qla_endpoint *qla_ep;
	struct scsi_qla_host *ha;
	struct sockaddr_in *addr;
	struct sockaddr_in6 *addr6;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	if (!shost) {
		ret = -ENXIO;
		printk(KERN_ERR "%s: shost is NULL\n",
		       __func__);
		return ERR_PTR(ret);
	}

	ha = iscsi_host_priv(shost);

	ep = iscsi_create_endpoint(sizeof(struct qla_endpoint));
	if (!ep) {
		ret = -ENOMEM;
		return ERR_PTR(ret);
	}

	qla_ep = ep->dd_data;
	memset(qla_ep, 0, sizeof(struct qla_endpoint));
	if (dst_addr->sa_family == AF_INET) {
		memcpy(&qla_ep->dst_addr, dst_addr, sizeof(struct sockaddr_in));
		addr = (struct sockaddr_in *)&qla_ep->dst_addr;
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: %pI4\n", __func__,
				  (char *)&addr->sin_addr));
	} else if (dst_addr->sa_family == AF_INET6) {
		memcpy(&qla_ep->dst_addr, dst_addr,
		       sizeof(struct sockaddr_in6));
		addr6 = (struct sockaddr_in6 *)&qla_ep->dst_addr;
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: %pI6\n", __func__,
				  (char *)&addr6->sin6_addr));
	}

	qla_ep->host = shost;

	return ep;
}

static int qla4xxx_ep_poll(struct iscsi_endpoint *ep, int timeout_ms)
{
	struct qla_endpoint *qla_ep;
	struct scsi_qla_host *ha;
	int ret = 0;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	qla_ep = ep->dd_data;
	ha = to_qla_host(qla_ep->host);

	if (adapter_up(ha) && !test_bit(AF_BUILD_DDB_LIST, &ha->flags))
		ret = 1;

	return ret;
}

static void qla4xxx_ep_disconnect(struct iscsi_endpoint *ep)
{
	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	iscsi_destroy_endpoint(ep);
}

static int qla4xxx_get_ep_param(struct iscsi_endpoint *ep,
				enum iscsi_param param,
				char *buf)
{
	struct qla_endpoint *qla_ep = ep->dd_data;
	struct sockaddr *dst_addr;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));

	switch (param) {
	case ISCSI_PARAM_CONN_PORT:
	case ISCSI_PARAM_CONN_ADDRESS:
		if (!qla_ep)
			return -ENOTCONN;

		dst_addr = (struct sockaddr *)&qla_ep->dst_addr;
		if (!dst_addr)
			return -ENOTCONN;

		return iscsi_conn_get_addr_param((struct sockaddr_storage *)
						 &qla_ep->dst_addr, param, buf);
	default:
		return -ENOSYS;
	}
}

static void qla4xxx_conn_get_stats(struct iscsi_cls_conn *cls_conn,
				   struct iscsi_stats *stats)
{
	struct iscsi_session *sess;
	struct iscsi_cls_session *cls_sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;
	struct ql_iscsi_stats *ql_iscsi_stats;
	int stats_size;
	int ret;
	dma_addr_t iscsi_stats_dma;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));

	cls_sess = iscsi_conn_to_session(cls_conn);
	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	stats_size = PAGE_ALIGN(sizeof(struct ql_iscsi_stats));
	
	ql_iscsi_stats = dma_alloc_coherent(&ha->pdev->dev, stats_size,
					    &iscsi_stats_dma, GFP_KERNEL);
	if (!ql_iscsi_stats) {
		ql4_printk(KERN_ERR, ha,
			   "Unable to allocate memory for iscsi stats\n");
		goto exit_get_stats;
	}

	ret =  qla4xxx_get_mgmt_data(ha, ddb_entry->fw_ddb_index, stats_size,
				     iscsi_stats_dma);
	if (ret != QLA_SUCCESS) {
		ql4_printk(KERN_ERR, ha,
			   "Unable to retreive iscsi stats\n");
		goto free_stats;
	}

	
	stats->txdata_octets = le64_to_cpu(ql_iscsi_stats->tx_data_octets);
	stats->rxdata_octets = le64_to_cpu(ql_iscsi_stats->rx_data_octets);
	
	stats->noptx_pdus = le32_to_cpu(ql_iscsi_stats->tx_nopout_pdus);
	stats->scsicmd_pdus = le32_to_cpu(ql_iscsi_stats->tx_scsi_cmd_pdus);
	stats->tmfcmd_pdus = le32_to_cpu(ql_iscsi_stats->tx_tmf_cmd_pdus);
	stats->login_pdus = le32_to_cpu(ql_iscsi_stats->tx_login_cmd_pdus);
	stats->text_pdus = le32_to_cpu(ql_iscsi_stats->tx_text_cmd_pdus);
	stats->dataout_pdus = le32_to_cpu(ql_iscsi_stats->tx_scsi_write_pdus);
	stats->logout_pdus = le32_to_cpu(ql_iscsi_stats->tx_logout_cmd_pdus);
	stats->snack_pdus = le32_to_cpu(ql_iscsi_stats->tx_snack_req_pdus);
	
	stats->noprx_pdus = le32_to_cpu(ql_iscsi_stats->rx_nopin_pdus);
	stats->scsirsp_pdus = le32_to_cpu(ql_iscsi_stats->rx_scsi_resp_pdus);
	stats->tmfrsp_pdus = le32_to_cpu(ql_iscsi_stats->rx_tmf_resp_pdus);
	stats->textrsp_pdus = le32_to_cpu(ql_iscsi_stats->rx_text_resp_pdus);
	stats->datain_pdus = le32_to_cpu(ql_iscsi_stats->rx_scsi_read_pdus);
	stats->logoutrsp_pdus =
			le32_to_cpu(ql_iscsi_stats->rx_logout_resp_pdus);
	stats->r2t_pdus = le32_to_cpu(ql_iscsi_stats->rx_r2t_pdus);
	stats->async_pdus = le32_to_cpu(ql_iscsi_stats->rx_async_pdus);
	stats->rjt_pdus = le32_to_cpu(ql_iscsi_stats->rx_reject_pdus);

free_stats:
	dma_free_coherent(&ha->pdev->dev, stats_size, ql_iscsi_stats,
			  iscsi_stats_dma);
exit_get_stats:
	return;
}

static enum blk_eh_timer_return qla4xxx_eh_cmd_timed_out(struct scsi_cmnd *sc)
{
	struct iscsi_cls_session *session;
	struct iscsi_session *sess;
	unsigned long flags;
	enum blk_eh_timer_return ret = BLK_EH_NOT_HANDLED;

	session = starget_to_session(scsi_target(sc->device));
	sess = session->dd_data;

	spin_lock_irqsave(&session->lock, flags);
	if (session->state == ISCSI_SESSION_FAILED)
		ret = BLK_EH_RESET_TIMER;
	spin_unlock_irqrestore(&session->lock, flags);

	return ret;
}

static void qla4xxx_set_port_speed(struct Scsi_Host *shost)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	struct iscsi_cls_host *ihost = shost->shost_data;
	uint32_t speed = ISCSI_PORT_SPEED_UNKNOWN;

	qla4xxx_get_firmware_state(ha);

	switch (ha->addl_fw_state & 0x0F00) {
	case FW_ADDSTATE_LINK_SPEED_10MBPS:
		speed = ISCSI_PORT_SPEED_10MBPS;
		break;
	case FW_ADDSTATE_LINK_SPEED_100MBPS:
		speed = ISCSI_PORT_SPEED_100MBPS;
		break;
	case FW_ADDSTATE_LINK_SPEED_1GBPS:
		speed = ISCSI_PORT_SPEED_1GBPS;
		break;
	case FW_ADDSTATE_LINK_SPEED_10GBPS:
		speed = ISCSI_PORT_SPEED_10GBPS;
		break;
	}
	ihost->port_speed = speed;
}

static void qla4xxx_set_port_state(struct Scsi_Host *shost)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	struct iscsi_cls_host *ihost = shost->shost_data;
	uint32_t state = ISCSI_PORT_STATE_DOWN;

	if (test_bit(AF_LINK_UP, &ha->flags))
		state = ISCSI_PORT_STATE_UP;

	ihost->port_state = state;
}

static int qla4xxx_host_get_param(struct Scsi_Host *shost,
				  enum iscsi_host_param param, char *buf)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	int len;

	switch (param) {
	case ISCSI_HOST_PARAM_HWADDRESS:
		len = sysfs_format_mac(buf, ha->my_mac, MAC_ADDR_LEN);
		break;
	case ISCSI_HOST_PARAM_IPADDRESS:
		len = sprintf(buf, "%pI4\n", &ha->ip_config.ip_address);
		break;
	case ISCSI_HOST_PARAM_INITIATOR_NAME:
		len = sprintf(buf, "%s\n", ha->name_string);
		break;
	case ISCSI_HOST_PARAM_PORT_STATE:
		qla4xxx_set_port_state(shost);
		len = sprintf(buf, "%s\n", iscsi_get_port_state_name(shost));
		break;
	case ISCSI_HOST_PARAM_PORT_SPEED:
		qla4xxx_set_port_speed(shost);
		len = sprintf(buf, "%s\n", iscsi_get_port_speed_name(shost));
		break;
	default:
		return -ENOSYS;
	}

	return len;
}

static void qla4xxx_create_ipv4_iface(struct scsi_qla_host *ha)
{
	if (ha->iface_ipv4)
		return;

	
	ha->iface_ipv4 = iscsi_create_iface(ha->host,
					    &qla4xxx_iscsi_transport,
					    ISCSI_IFACE_TYPE_IPV4, 0, 0);
	if (!ha->iface_ipv4)
		ql4_printk(KERN_ERR, ha, "Could not create IPv4 iSCSI "
			   "iface0.\n");
}

static void qla4xxx_create_ipv6_iface(struct scsi_qla_host *ha)
{
	if (!ha->iface_ipv6_0)
		
		ha->iface_ipv6_0 = iscsi_create_iface(ha->host,
						      &qla4xxx_iscsi_transport,
						      ISCSI_IFACE_TYPE_IPV6, 0,
						      0);
	if (!ha->iface_ipv6_0)
		ql4_printk(KERN_ERR, ha, "Could not create IPv6 iSCSI "
			   "iface0.\n");

	if (!ha->iface_ipv6_1)
		
		ha->iface_ipv6_1 = iscsi_create_iface(ha->host,
						      &qla4xxx_iscsi_transport,
						      ISCSI_IFACE_TYPE_IPV6, 1,
						      0);
	if (!ha->iface_ipv6_1)
		ql4_printk(KERN_ERR, ha, "Could not create IPv6 iSCSI "
			   "iface1.\n");
}

static void qla4xxx_create_ifaces(struct scsi_qla_host *ha)
{
	if (ha->ip_config.ipv4_options & IPOPT_IPV4_PROTOCOL_ENABLE)
		qla4xxx_create_ipv4_iface(ha);

	if (ha->ip_config.ipv6_options & IPV6_OPT_IPV6_PROTOCOL_ENABLE)
		qla4xxx_create_ipv6_iface(ha);
}

static void qla4xxx_destroy_ipv4_iface(struct scsi_qla_host *ha)
{
	if (ha->iface_ipv4) {
		iscsi_destroy_iface(ha->iface_ipv4);
		ha->iface_ipv4 = NULL;
	}
}

static void qla4xxx_destroy_ipv6_iface(struct scsi_qla_host *ha)
{
	if (ha->iface_ipv6_0) {
		iscsi_destroy_iface(ha->iface_ipv6_0);
		ha->iface_ipv6_0 = NULL;
	}
	if (ha->iface_ipv6_1) {
		iscsi_destroy_iface(ha->iface_ipv6_1);
		ha->iface_ipv6_1 = NULL;
	}
}

static void qla4xxx_destroy_ifaces(struct scsi_qla_host *ha)
{
	qla4xxx_destroy_ipv4_iface(ha);
	qla4xxx_destroy_ipv6_iface(ha);
}

static void qla4xxx_set_ipv6(struct scsi_qla_host *ha,
			     struct iscsi_iface_param_info *iface_param,
			     struct addr_ctrl_blk *init_fw_cb)
{
	switch (iface_param->param) {
	case ISCSI_NET_PARAM_IPV6_ADDR:
		if (iface_param->iface_num & 0x1)
			
			memcpy(init_fw_cb->ipv6_addr1, iface_param->value,
			       sizeof(init_fw_cb->ipv6_addr1));
		else
			
			memcpy(init_fw_cb->ipv6_addr0, iface_param->value,
			       sizeof(init_fw_cb->ipv6_addr0));
		break;
	case ISCSI_NET_PARAM_IPV6_LINKLOCAL:
		if (iface_param->iface_num & 0x1)
			break;
		memcpy(init_fw_cb->ipv6_if_id, &iface_param->value[8],
		       sizeof(init_fw_cb->ipv6_if_id));
		break;
	case ISCSI_NET_PARAM_IPV6_ROUTER:
		if (iface_param->iface_num & 0x1)
			break;
		memcpy(init_fw_cb->ipv6_dflt_rtr_addr, iface_param->value,
		       sizeof(init_fw_cb->ipv6_dflt_rtr_addr));
		break;
	case ISCSI_NET_PARAM_IPV6_ADDR_AUTOCFG:
		
		if (iface_param->iface_num & 0x1)
			break;

		if (iface_param->value[0] == ISCSI_IPV6_AUTOCFG_DISABLE)
			init_fw_cb->ipv6_addtl_opts &=
				cpu_to_le16(
				  ~IPV6_ADDOPT_NEIGHBOR_DISCOVERY_ADDR_ENABLE);
		else if (iface_param->value[0] == ISCSI_IPV6_AUTOCFG_ND_ENABLE)
			init_fw_cb->ipv6_addtl_opts |=
				cpu_to_le16(
				  IPV6_ADDOPT_NEIGHBOR_DISCOVERY_ADDR_ENABLE);
		else
			ql4_printk(KERN_ERR, ha, "Invalid autocfg setting for "
				   "IPv6 addr\n");
		break;
	case ISCSI_NET_PARAM_IPV6_LINKLOCAL_AUTOCFG:
		
		if (iface_param->iface_num & 0x1)
			break;

		if (iface_param->value[0] ==
		    ISCSI_IPV6_LINKLOCAL_AUTOCFG_ENABLE)
			init_fw_cb->ipv6_addtl_opts |= cpu_to_le16(
					IPV6_ADDOPT_AUTOCONFIG_LINK_LOCAL_ADDR);
		else if (iface_param->value[0] ==
			 ISCSI_IPV6_LINKLOCAL_AUTOCFG_DISABLE)
			init_fw_cb->ipv6_addtl_opts &= cpu_to_le16(
				       ~IPV6_ADDOPT_AUTOCONFIG_LINK_LOCAL_ADDR);
		else
			ql4_printk(KERN_ERR, ha, "Invalid autocfg setting for "
				   "IPv6 linklocal addr\n");
		break;
	case ISCSI_NET_PARAM_IPV6_ROUTER_AUTOCFG:
		
		if (iface_param->iface_num & 0x1)
			break;

		if (iface_param->value[0] == ISCSI_IPV6_ROUTER_AUTOCFG_ENABLE)
			memset(init_fw_cb->ipv6_dflt_rtr_addr, 0,
			       sizeof(init_fw_cb->ipv6_dflt_rtr_addr));
		break;
	case ISCSI_NET_PARAM_IFACE_ENABLE:
		if (iface_param->value[0] == ISCSI_IFACE_ENABLE) {
			init_fw_cb->ipv6_opts |=
				cpu_to_le16(IPV6_OPT_IPV6_PROTOCOL_ENABLE);
			qla4xxx_create_ipv6_iface(ha);
		} else {
			init_fw_cb->ipv6_opts &=
				cpu_to_le16(~IPV6_OPT_IPV6_PROTOCOL_ENABLE &
					    0xFFFF);
			qla4xxx_destroy_ipv6_iface(ha);
		}
		break;
	case ISCSI_NET_PARAM_VLAN_TAG:
		if (iface_param->len != sizeof(init_fw_cb->ipv6_vlan_tag))
			break;
		init_fw_cb->ipv6_vlan_tag =
				cpu_to_be16(*(uint16_t *)iface_param->value);
		break;
	case ISCSI_NET_PARAM_VLAN_ENABLED:
		if (iface_param->value[0] == ISCSI_VLAN_ENABLE)
			init_fw_cb->ipv6_opts |=
				cpu_to_le16(IPV6_OPT_VLAN_TAGGING_ENABLE);
		else
			init_fw_cb->ipv6_opts &=
				cpu_to_le16(~IPV6_OPT_VLAN_TAGGING_ENABLE);
		break;
	case ISCSI_NET_PARAM_MTU:
		init_fw_cb->eth_mtu_size =
				cpu_to_le16(*(uint16_t *)iface_param->value);
		break;
	case ISCSI_NET_PARAM_PORT:
		
		if (iface_param->iface_num & 0x1)
			break;

		init_fw_cb->ipv6_port =
				cpu_to_le16(*(uint16_t *)iface_param->value);
		break;
	default:
		ql4_printk(KERN_ERR, ha, "Unknown IPv6 param = %d\n",
			   iface_param->param);
		break;
	}
}

static void qla4xxx_set_ipv4(struct scsi_qla_host *ha,
			     struct iscsi_iface_param_info *iface_param,
			     struct addr_ctrl_blk *init_fw_cb)
{
	switch (iface_param->param) {
	case ISCSI_NET_PARAM_IPV4_ADDR:
		memcpy(init_fw_cb->ipv4_addr, iface_param->value,
		       sizeof(init_fw_cb->ipv4_addr));
		break;
	case ISCSI_NET_PARAM_IPV4_SUBNET:
		memcpy(init_fw_cb->ipv4_subnet,	iface_param->value,
		       sizeof(init_fw_cb->ipv4_subnet));
		break;
	case ISCSI_NET_PARAM_IPV4_GW:
		memcpy(init_fw_cb->ipv4_gw_addr, iface_param->value,
		       sizeof(init_fw_cb->ipv4_gw_addr));
		break;
	case ISCSI_NET_PARAM_IPV4_BOOTPROTO:
		if (iface_param->value[0] == ISCSI_BOOTPROTO_DHCP)
			init_fw_cb->ipv4_tcp_opts |=
					cpu_to_le16(TCPOPT_DHCP_ENABLE);
		else if (iface_param->value[0] == ISCSI_BOOTPROTO_STATIC)
			init_fw_cb->ipv4_tcp_opts &=
					cpu_to_le16(~TCPOPT_DHCP_ENABLE);
		else
			ql4_printk(KERN_ERR, ha, "Invalid IPv4 bootproto\n");
		break;
	case ISCSI_NET_PARAM_IFACE_ENABLE:
		if (iface_param->value[0] == ISCSI_IFACE_ENABLE) {
			init_fw_cb->ipv4_ip_opts |=
				cpu_to_le16(IPOPT_IPV4_PROTOCOL_ENABLE);
			qla4xxx_create_ipv4_iface(ha);
		} else {
			init_fw_cb->ipv4_ip_opts &=
				cpu_to_le16(~IPOPT_IPV4_PROTOCOL_ENABLE &
					    0xFFFF);
			qla4xxx_destroy_ipv4_iface(ha);
		}
		break;
	case ISCSI_NET_PARAM_VLAN_TAG:
		if (iface_param->len != sizeof(init_fw_cb->ipv4_vlan_tag))
			break;
		init_fw_cb->ipv4_vlan_tag =
				cpu_to_be16(*(uint16_t *)iface_param->value);
		break;
	case ISCSI_NET_PARAM_VLAN_ENABLED:
		if (iface_param->value[0] == ISCSI_VLAN_ENABLE)
			init_fw_cb->ipv4_ip_opts |=
					cpu_to_le16(IPOPT_VLAN_TAGGING_ENABLE);
		else
			init_fw_cb->ipv4_ip_opts &=
					cpu_to_le16(~IPOPT_VLAN_TAGGING_ENABLE);
		break;
	case ISCSI_NET_PARAM_MTU:
		init_fw_cb->eth_mtu_size =
				cpu_to_le16(*(uint16_t *)iface_param->value);
		break;
	case ISCSI_NET_PARAM_PORT:
		init_fw_cb->ipv4_port =
				cpu_to_le16(*(uint16_t *)iface_param->value);
		break;
	default:
		ql4_printk(KERN_ERR, ha, "Unknown IPv4 param = %d\n",
			   iface_param->param);
		break;
	}
}

static void
qla4xxx_initcb_to_acb(struct addr_ctrl_blk *init_fw_cb)
{
	struct addr_ctrl_blk_def *acb;
	acb = (struct addr_ctrl_blk_def *)init_fw_cb;
	memset(acb->reserved1, 0, sizeof(acb->reserved1));
	memset(acb->reserved2, 0, sizeof(acb->reserved2));
	memset(acb->reserved3, 0, sizeof(acb->reserved3));
	memset(acb->reserved4, 0, sizeof(acb->reserved4));
	memset(acb->reserved5, 0, sizeof(acb->reserved5));
	memset(acb->reserved6, 0, sizeof(acb->reserved6));
	memset(acb->reserved7, 0, sizeof(acb->reserved7));
	memset(acb->reserved8, 0, sizeof(acb->reserved8));
	memset(acb->reserved9, 0, sizeof(acb->reserved9));
	memset(acb->reserved10, 0, sizeof(acb->reserved10));
	memset(acb->reserved11, 0, sizeof(acb->reserved11));
	memset(acb->reserved12, 0, sizeof(acb->reserved12));
	memset(acb->reserved13, 0, sizeof(acb->reserved13));
	memset(acb->reserved14, 0, sizeof(acb->reserved14));
	memset(acb->reserved15, 0, sizeof(acb->reserved15));
}

static int
qla4xxx_iface_set_param(struct Scsi_Host *shost, void *data, uint32_t len)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	int rval = 0;
	struct iscsi_iface_param_info *iface_param = NULL;
	struct addr_ctrl_blk *init_fw_cb = NULL;
	dma_addr_t init_fw_cb_dma;
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	uint32_t rem = len;
	struct nlattr *attr;

	init_fw_cb = dma_alloc_coherent(&ha->pdev->dev,
					sizeof(struct addr_ctrl_blk),
					&init_fw_cb_dma, GFP_KERNEL);
	if (!init_fw_cb) {
		ql4_printk(KERN_ERR, ha, "%s: Unable to alloc init_cb\n",
			   __func__);
		return -ENOMEM;
	}

	memset(init_fw_cb, 0, sizeof(struct addr_ctrl_blk));
	memset(&mbox_cmd, 0, sizeof(mbox_cmd));
	memset(&mbox_sts, 0, sizeof(mbox_sts));

	if (qla4xxx_get_ifcb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb_dma)) {
		ql4_printk(KERN_ERR, ha, "%s: get ifcb failed\n", __func__);
		rval = -EIO;
		goto exit_init_fw_cb;
	}

	nla_for_each_attr(attr, data, len, rem) {
		iface_param = nla_data(attr);

		if (iface_param->param_type != ISCSI_NET_PARAM)
			continue;

		switch (iface_param->iface_type) {
		case ISCSI_IFACE_TYPE_IPV4:
			switch (iface_param->iface_num) {
			case 0:
				qla4xxx_set_ipv4(ha, iface_param, init_fw_cb);
				break;
			default:
				
				ql4_printk(KERN_ERR, ha, "Invalid IPv4 iface "
					   "number = %d\n",
					   iface_param->iface_num);
				break;
			}
			break;
		case ISCSI_IFACE_TYPE_IPV6:
			switch (iface_param->iface_num) {
			case 0:
			case 1:
				qla4xxx_set_ipv6(ha, iface_param, init_fw_cb);
				break;
			default:
				
				ql4_printk(KERN_ERR, ha, "Invalid IPv6 iface "
					   "number = %d\n",
					   iface_param->iface_num);
				break;
			}
			break;
		default:
			ql4_printk(KERN_ERR, ha, "Invalid iface type\n");
			break;
		}
	}

	init_fw_cb->cookie = cpu_to_le32(0x11BEAD5A);

	rval = qla4xxx_set_flash(ha, init_fw_cb_dma, FLASH_SEGMENT_IFCB,
				 sizeof(struct addr_ctrl_blk),
				 FLASH_OPT_RMW_COMMIT);
	if (rval != QLA_SUCCESS) {
		ql4_printk(KERN_ERR, ha, "%s: set flash mbx failed\n",
			   __func__);
		rval = -EIO;
		goto exit_init_fw_cb;
	}

	rval = qla4xxx_disable_acb(ha);
	if (rval != QLA_SUCCESS) {
		ql4_printk(KERN_ERR, ha, "%s: disable acb mbx failed\n",
			   __func__);
		rval = -EIO;
		goto exit_init_fw_cb;
	}

	wait_for_completion_timeout(&ha->disable_acb_comp,
				    DISABLE_ACB_TOV * HZ);

	qla4xxx_initcb_to_acb(init_fw_cb);

	rval = qla4xxx_set_acb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb_dma);
	if (rval != QLA_SUCCESS) {
		ql4_printk(KERN_ERR, ha, "%s: set acb mbx failed\n",
			   __func__);
		rval = -EIO;
		goto exit_init_fw_cb;
	}

	memset(init_fw_cb, 0, sizeof(struct addr_ctrl_blk));
	qla4xxx_update_local_ifcb(ha, &mbox_cmd[0], &mbox_sts[0], init_fw_cb,
				  init_fw_cb_dma);

exit_init_fw_cb:
	dma_free_coherent(&ha->pdev->dev, sizeof(struct addr_ctrl_blk),
			  init_fw_cb, init_fw_cb_dma);

	return rval;
}

static int qla4xxx_session_get_param(struct iscsi_cls_session *cls_sess,
				     enum iscsi_param param, char *buf)
{
	struct iscsi_session *sess = cls_sess->dd_data;
	struct ddb_entry *ddb_entry = sess->dd_data;
	struct scsi_qla_host *ha = ddb_entry->ha;
	int rval, len;
	uint16_t idx;

	switch (param) {
	case ISCSI_PARAM_CHAP_IN_IDX:
		rval = qla4xxx_get_chap_index(ha, sess->username_in,
					      sess->password_in, BIDI_CHAP,
					      &idx);
		if (rval)
			return -EINVAL;

		len = sprintf(buf, "%hu\n", idx);
		break;
	case ISCSI_PARAM_CHAP_OUT_IDX:
		rval = qla4xxx_get_chap_index(ha, sess->username,
					      sess->password, LOCAL_CHAP,
					      &idx);
		if (rval)
			return -EINVAL;

		len = sprintf(buf, "%hu\n", idx);
		break;
	default:
		return iscsi_session_get_param(cls_sess, param, buf);
	}

	return len;
}

static int qla4xxx_conn_get_param(struct iscsi_cls_conn *cls_conn,
				  enum iscsi_param param, char *buf)
{
	struct iscsi_conn *conn;
	struct qla_conn *qla_conn;
	struct sockaddr *dst_addr;
	int len = 0;

	conn = cls_conn->dd_data;
	qla_conn = conn->dd_data;
	dst_addr = &qla_conn->qla_ep->dst_addr;

	switch (param) {
	case ISCSI_PARAM_CONN_PORT:
	case ISCSI_PARAM_CONN_ADDRESS:
		return iscsi_conn_get_addr_param((struct sockaddr_storage *)
						 dst_addr, param, buf);
	default:
		return iscsi_conn_get_param(cls_conn, param, buf);
	}

	return len;

}

int qla4xxx_get_ddb_index(struct scsi_qla_host *ha, uint16_t *ddb_index)
{
	uint32_t mbx_sts = 0;
	uint16_t tmp_ddb_index;
	int ret;

get_ddb_index:
	tmp_ddb_index = find_first_zero_bit(ha->ddb_idx_map, MAX_DDB_ENTRIES);

	if (tmp_ddb_index >= MAX_DDB_ENTRIES) {
		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "Free DDB index not available\n"));
		ret = QLA_ERROR;
		goto exit_get_ddb_index;
	}

	if (test_and_set_bit(tmp_ddb_index, ha->ddb_idx_map))
		goto get_ddb_index;

	DEBUG2(ql4_printk(KERN_INFO, ha,
			  "Found a free DDB index at %d\n", tmp_ddb_index));
	ret = qla4xxx_req_ddb_entry(ha, tmp_ddb_index, &mbx_sts);
	if (ret == QLA_ERROR) {
		if (mbx_sts == MBOX_STS_COMMAND_ERROR) {
			ql4_printk(KERN_INFO, ha,
				   "DDB index = %d not available trying next\n",
				   tmp_ddb_index);
			goto get_ddb_index;
		}
		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "Free FW DDB not available\n"));
	}

	*ddb_index = tmp_ddb_index;

exit_get_ddb_index:
	return ret;
}

static int qla4xxx_match_ipaddress(struct scsi_qla_host *ha,
				   struct ddb_entry *ddb_entry,
				   char *existing_ipaddr,
				   char *user_ipaddr)
{
	uint8_t dst_ipaddr[IPv6_ADDR_LEN];
	char formatted_ipaddr[DDB_IPADDR_LEN];
	int status = QLA_SUCCESS, ret = 0;

	if (ddb_entry->fw_ddb_entry.options & DDB_OPT_IPV6_DEVICE) {
		ret = in6_pton(user_ipaddr, strlen(user_ipaddr), dst_ipaddr,
			       '\0', NULL);
		if (ret == 0) {
			status = QLA_ERROR;
			goto out_match;
		}
		ret = sprintf(formatted_ipaddr, "%pI6", dst_ipaddr);
	} else {
		ret = in4_pton(user_ipaddr, strlen(user_ipaddr), dst_ipaddr,
			       '\0', NULL);
		if (ret == 0) {
			status = QLA_ERROR;
			goto out_match;
		}
		ret = sprintf(formatted_ipaddr, "%pI4", dst_ipaddr);
	}

	if (strcmp(existing_ipaddr, formatted_ipaddr))
		status = QLA_ERROR;

out_match:
	return status;
}

static int qla4xxx_match_fwdb_session(struct scsi_qla_host *ha,
				      struct iscsi_cls_conn *cls_conn)
{
	int idx = 0, max_ddbs, rval;
	struct iscsi_cls_session *cls_sess = iscsi_conn_to_session(cls_conn);
	struct iscsi_session *sess, *existing_sess;
	struct iscsi_conn *conn, *existing_conn;
	struct ddb_entry *ddb_entry;

	sess = cls_sess->dd_data;
	conn = cls_conn->dd_data;

	if (sess->targetname == NULL ||
	    conn->persistent_address == NULL ||
	    conn->persistent_port == 0)
		return QLA_ERROR;

	max_ddbs =  is_qla40XX(ha) ? MAX_DEV_DB_ENTRIES_40XX :
				     MAX_DEV_DB_ENTRIES;

	for (idx = 0; idx < max_ddbs; idx++) {
		ddb_entry = qla4xxx_lookup_ddb_by_fw_index(ha, idx);
		if (ddb_entry == NULL)
			continue;

		if (ddb_entry->ddb_type != FLASH_DDB)
			continue;

		existing_sess = ddb_entry->sess->dd_data;
		existing_conn = ddb_entry->conn->dd_data;

		if (existing_sess->targetname == NULL ||
		    existing_conn->persistent_address == NULL ||
		    existing_conn->persistent_port == 0)
			continue;

		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "IQN = %s User IQN = %s\n",
				  existing_sess->targetname,
				  sess->targetname));

		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "IP = %s User IP = %s\n",
				  existing_conn->persistent_address,
				  conn->persistent_address));

		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "Port = %d User Port = %d\n",
				  existing_conn->persistent_port,
				  conn->persistent_port));

		if (strcmp(existing_sess->targetname, sess->targetname))
			continue;
		rval = qla4xxx_match_ipaddress(ha, ddb_entry,
					existing_conn->persistent_address,
					conn->persistent_address);
		if (rval == QLA_ERROR)
			continue;
		if (existing_conn->persistent_port != conn->persistent_port)
			continue;
		break;
	}

	if (idx == max_ddbs)
		return QLA_ERROR;

	DEBUG2(ql4_printk(KERN_INFO, ha,
			  "Match found in fwdb sessions\n"));
	return QLA_SUCCESS;
}

static struct iscsi_cls_session *
qla4xxx_session_create(struct iscsi_endpoint *ep,
			uint16_t cmds_max, uint16_t qdepth,
			uint32_t initial_cmdsn)
{
	struct iscsi_cls_session *cls_sess;
	struct scsi_qla_host *ha;
	struct qla_endpoint *qla_ep;
	struct ddb_entry *ddb_entry;
	uint16_t ddb_index;
	struct iscsi_session *sess;
	struct sockaddr *dst_addr;
	int ret;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	if (!ep) {
		printk(KERN_ERR "qla4xxx: missing ep.\n");
		return NULL;
	}

	qla_ep = ep->dd_data;
	dst_addr = (struct sockaddr *)&qla_ep->dst_addr;
	ha = to_qla_host(qla_ep->host);

	ret = qla4xxx_get_ddb_index(ha, &ddb_index);
	if (ret == QLA_ERROR)
		return NULL;

	cls_sess = iscsi_session_setup(&qla4xxx_iscsi_transport, qla_ep->host,
				       cmds_max, sizeof(struct ddb_entry),
				       sizeof(struct ql4_task_data),
				       initial_cmdsn, ddb_index);
	if (!cls_sess)
		return NULL;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ddb_entry->fw_ddb_index = ddb_index;
	ddb_entry->fw_ddb_device_state = DDB_DS_NO_CONNECTION_ACTIVE;
	ddb_entry->ha = ha;
	ddb_entry->sess = cls_sess;
	ddb_entry->unblock_sess = qla4xxx_unblock_ddb;
	ddb_entry->ddb_change = qla4xxx_ddb_change;
	cls_sess->recovery_tmo = ql4xsess_recovery_tmo;
	ha->fw_ddb_index_map[ddb_entry->fw_ddb_index] = ddb_entry;
	ha->tot_ddbs++;

	return cls_sess;
}

static void qla4xxx_session_destroy(struct iscsi_cls_session *cls_sess)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;
	unsigned long flags;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	qla4xxx_clear_ddb_entry(ha, ddb_entry->fw_ddb_index);

	spin_lock_irqsave(&ha->hardware_lock, flags);
	qla4xxx_free_ddb(ha, ddb_entry);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
	iscsi_session_teardown(cls_sess);
}

static struct iscsi_cls_conn *
qla4xxx_conn_create(struct iscsi_cls_session *cls_sess, uint32_t conn_idx)
{
	struct iscsi_cls_conn *cls_conn;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	cls_conn = iscsi_conn_setup(cls_sess, sizeof(struct qla_conn),
				    conn_idx);
	if (!cls_conn)
		return NULL;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ddb_entry->conn = cls_conn;

	return cls_conn;
}

static int qla4xxx_conn_bind(struct iscsi_cls_session *cls_session,
			     struct iscsi_cls_conn *cls_conn,
			     uint64_t transport_fd, int is_leading)
{
	struct iscsi_conn *conn;
	struct qla_conn *qla_conn;
	struct iscsi_endpoint *ep;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));

	if (iscsi_conn_bind(cls_session, cls_conn, is_leading))
		return -EINVAL;
	ep = iscsi_lookup_endpoint(transport_fd);
	conn = cls_conn->dd_data;
	qla_conn = conn->dd_data;
	qla_conn->qla_ep = ep->dd_data;
	return 0;
}

static int qla4xxx_conn_start(struct iscsi_cls_conn *cls_conn)
{
	struct iscsi_cls_session *cls_sess = iscsi_conn_to_session(cls_conn);
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;
	struct dev_db_entry *fw_ddb_entry = NULL;
	dma_addr_t fw_ddb_entry_dma;
	uint32_t mbx_sts = 0;
	int ret = 0;
	int status = QLA_SUCCESS;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	ret = qla4xxx_match_fwdb_session(ha, cls_conn);
	if (ret == QLA_SUCCESS) {
		ql4_printk(KERN_INFO, ha,
			   "Session already exist in FW.\n");
		ret = -EEXIST;
		goto exit_conn_start;
	}

	fw_ddb_entry = dma_alloc_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
					  &fw_ddb_entry_dma, GFP_KERNEL);
	if (!fw_ddb_entry) {
		ql4_printk(KERN_ERR, ha,
			   "%s: Unable to allocate dma buffer\n", __func__);
		ret = -ENOMEM;
		goto exit_conn_start;
	}

	ret = qla4xxx_set_param_ddbentry(ha, ddb_entry, cls_conn, &mbx_sts);
	if (ret) {
		if (mbx_sts)
			if (ddb_entry->fw_ddb_device_state ==
						DDB_DS_SESSION_ACTIVE) {
				ddb_entry->unblock_sess(ddb_entry->sess);
				goto exit_set_param;
			}

		ql4_printk(KERN_ERR, ha, "%s: Failed set param for index[%d]\n",
			   __func__, ddb_entry->fw_ddb_index);
		goto exit_conn_start;
	}

	status = qla4xxx_conn_open(ha, ddb_entry->fw_ddb_index);
	if (status == QLA_ERROR) {
		ql4_printk(KERN_ERR, ha, "%s: Login failed: %s\n", __func__,
			   sess->targetname);
		ret = -EINVAL;
		goto exit_conn_start;
	}

	if (ddb_entry->fw_ddb_device_state == DDB_DS_NO_CONNECTION_ACTIVE)
		ddb_entry->fw_ddb_device_state = DDB_DS_LOGIN_IN_PROCESS;

	DEBUG2(printk(KERN_INFO "%s: DDB state [%d]\n", __func__,
		      ddb_entry->fw_ddb_device_state));

exit_set_param:
	ret = 0;

exit_conn_start:
	if (fw_ddb_entry)
		dma_free_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
				  fw_ddb_entry, fw_ddb_entry_dma);
	return ret;
}

static void qla4xxx_conn_destroy(struct iscsi_cls_conn *cls_conn)
{
	struct iscsi_cls_session *cls_sess = iscsi_conn_to_session(cls_conn);
	struct iscsi_session *sess;
	struct scsi_qla_host *ha;
	struct ddb_entry *ddb_entry;
	int options;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	options = LOGOUT_OPTION_CLOSE_SESSION;
	if (qla4xxx_session_logout_ddb(ha, ddb_entry, options) == QLA_ERROR)
		ql4_printk(KERN_ERR, ha, "%s: Logout failed\n", __func__);
}

static void qla4xxx_task_work(struct work_struct *wdata)
{
	struct ql4_task_data *task_data;
	struct scsi_qla_host *ha;
	struct passthru_status *sts;
	struct iscsi_task *task;
	struct iscsi_hdr *hdr;
	uint8_t *data;
	uint32_t data_len;
	struct iscsi_conn *conn;
	int hdr_len;
	itt_t itt;

	task_data = container_of(wdata, struct ql4_task_data, task_work);
	ha = task_data->ha;
	task = task_data->task;
	sts = &task_data->sts;
	hdr_len = sizeof(struct iscsi_hdr);

	DEBUG3(printk(KERN_INFO "Status returned\n"));
	DEBUG3(qla4xxx_dump_buffer(sts, 64));
	DEBUG3(printk(KERN_INFO "Response buffer"));
	DEBUG3(qla4xxx_dump_buffer(task_data->resp_buffer, 64));

	conn = task->conn;

	switch (sts->completionStatus) {
	case PASSTHRU_STATUS_COMPLETE:
		hdr = (struct iscsi_hdr *)task_data->resp_buffer;
		
		itt = sts->handle;
		hdr->itt = itt;
		data = task_data->resp_buffer + hdr_len;
		data_len = task_data->resp_len - hdr_len;
		iscsi_complete_pdu(conn, hdr, data, data_len);
		break;
	default:
		ql4_printk(KERN_ERR, ha, "Passthru failed status = 0x%x\n",
			   sts->completionStatus);
		break;
	}
	return;
}

static int qla4xxx_alloc_pdu(struct iscsi_task *task, uint8_t opcode)
{
	struct ql4_task_data *task_data;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;
	int hdr_len;

	sess = task->conn->session;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;
	task_data = task->dd_data;
	memset(task_data, 0, sizeof(struct ql4_task_data));

	if (task->sc) {
		ql4_printk(KERN_INFO, ha,
			   "%s: SCSI Commands not implemented\n", __func__);
		return -EINVAL;
	}

	hdr_len = sizeof(struct iscsi_hdr);
	task_data->ha = ha;
	task_data->task = task;

	if (task->data_count) {
		task_data->data_dma = dma_map_single(&ha->pdev->dev, task->data,
						     task->data_count,
						     PCI_DMA_TODEVICE);
	}

	DEBUG2(ql4_printk(KERN_INFO, ha, "%s: MaxRecvLen %u, iscsi hrd %d\n",
		      __func__, task->conn->max_recv_dlength, hdr_len));

	task_data->resp_len = task->conn->max_recv_dlength + hdr_len;
	task_data->resp_buffer = dma_alloc_coherent(&ha->pdev->dev,
						    task_data->resp_len,
						    &task_data->resp_dma,
						    GFP_ATOMIC);
	if (!task_data->resp_buffer)
		goto exit_alloc_pdu;

	task_data->req_len = task->data_count + hdr_len;
	task_data->req_buffer = dma_alloc_coherent(&ha->pdev->dev,
						   task_data->req_len,
						   &task_data->req_dma,
						   GFP_ATOMIC);
	if (!task_data->req_buffer)
		goto exit_alloc_pdu;

	task->hdr = task_data->req_buffer;

	INIT_WORK(&task_data->task_work, qla4xxx_task_work);

	return 0;

exit_alloc_pdu:
	if (task_data->resp_buffer)
		dma_free_coherent(&ha->pdev->dev, task_data->resp_len,
				  task_data->resp_buffer, task_data->resp_dma);

	if (task_data->req_buffer)
		dma_free_coherent(&ha->pdev->dev, task_data->req_len,
				  task_data->req_buffer, task_data->req_dma);
	return -ENOMEM;
}

static void qla4xxx_task_cleanup(struct iscsi_task *task)
{
	struct ql4_task_data *task_data;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;
	int hdr_len;

	hdr_len = sizeof(struct iscsi_hdr);
	sess = task->conn->session;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;
	task_data = task->dd_data;

	if (task->data_count) {
		dma_unmap_single(&ha->pdev->dev, task_data->data_dma,
				 task->data_count, PCI_DMA_TODEVICE);
	}

	DEBUG2(ql4_printk(KERN_INFO, ha, "%s: MaxRecvLen %u, iscsi hrd %d\n",
		      __func__, task->conn->max_recv_dlength, hdr_len));

	dma_free_coherent(&ha->pdev->dev, task_data->resp_len,
			  task_data->resp_buffer, task_data->resp_dma);
	dma_free_coherent(&ha->pdev->dev, task_data->req_len,
			  task_data->req_buffer, task_data->req_dma);
	return;
}

static int qla4xxx_task_xmit(struct iscsi_task *task)
{
	struct scsi_cmnd *sc = task->sc;
	struct iscsi_session *sess = task->conn->session;
	struct ddb_entry *ddb_entry = sess->dd_data;
	struct scsi_qla_host *ha = ddb_entry->ha;

	if (!sc)
		return qla4xxx_send_passthru0(task);

	ql4_printk(KERN_INFO, ha, "%s: scsi cmd xmit not implemented\n",
		   __func__);
	return -ENOSYS;
}

static void qla4xxx_copy_fwddb_param(struct scsi_qla_host *ha,
				     struct dev_db_entry *fw_ddb_entry,
				     struct iscsi_cls_session *cls_sess,
				     struct iscsi_cls_conn *cls_conn)
{
	int buflen = 0;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct iscsi_conn *conn;
	char ip_addr[DDB_IPADDR_LEN];
	uint16_t options = 0;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	conn = cls_conn->dd_data;

	ddb_entry->chap_tbl_idx = le16_to_cpu(fw_ddb_entry->chap_tbl_idx);

	conn->max_recv_dlength = BYTE_UNITS *
			  le16_to_cpu(fw_ddb_entry->iscsi_max_rcv_data_seg_len);

	conn->max_xmit_dlength = BYTE_UNITS *
			  le16_to_cpu(fw_ddb_entry->iscsi_max_snd_data_seg_len);

	sess->initial_r2t_en =
			    (BIT_10 & le16_to_cpu(fw_ddb_entry->iscsi_options));

	sess->max_r2t = le16_to_cpu(fw_ddb_entry->iscsi_max_outsnd_r2t);

	sess->imm_data_en = (BIT_11 & le16_to_cpu(fw_ddb_entry->iscsi_options));

	sess->first_burst = BYTE_UNITS *
			       le16_to_cpu(fw_ddb_entry->iscsi_first_burst_len);

	sess->max_burst = BYTE_UNITS *
				 le16_to_cpu(fw_ddb_entry->iscsi_max_burst_len);

	sess->time2wait = le16_to_cpu(fw_ddb_entry->iscsi_def_time2wait);

	sess->time2retain = le16_to_cpu(fw_ddb_entry->iscsi_def_time2retain);

	conn->persistent_port = le16_to_cpu(fw_ddb_entry->port);

	sess->tpgt = le32_to_cpu(fw_ddb_entry->tgt_portal_grp);

	options = le16_to_cpu(fw_ddb_entry->options);
	if (options & DDB_OPT_IPV6_DEVICE)
		sprintf(ip_addr, "%pI6", fw_ddb_entry->ip_addr);
	else
		sprintf(ip_addr, "%pI4", fw_ddb_entry->ip_addr);

	iscsi_set_param(cls_conn, ISCSI_PARAM_TARGET_NAME,
			(char *)fw_ddb_entry->iscsi_name, buflen);
	iscsi_set_param(cls_conn, ISCSI_PARAM_INITIATOR_NAME,
			(char *)ha->name_string, buflen);
	iscsi_set_param(cls_conn, ISCSI_PARAM_PERSISTENT_ADDRESS,
			(char *)ip_addr, buflen);
	iscsi_set_param(cls_conn, ISCSI_PARAM_TARGET_ALIAS,
			(char *)fw_ddb_entry->iscsi_alias, buflen);
}

void qla4xxx_update_session_conn_fwddb_param(struct scsi_qla_host *ha,
					     struct ddb_entry *ddb_entry)
{
	struct iscsi_cls_session *cls_sess;
	struct iscsi_cls_conn *cls_conn;
	uint32_t ddb_state;
	dma_addr_t fw_ddb_entry_dma;
	struct dev_db_entry *fw_ddb_entry;

	fw_ddb_entry = dma_alloc_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
					  &fw_ddb_entry_dma, GFP_KERNEL);
	if (!fw_ddb_entry) {
		ql4_printk(KERN_ERR, ha,
			   "%s: Unable to allocate dma buffer\n", __func__);
		goto exit_session_conn_fwddb_param;
	}

	if (qla4xxx_get_fwddb_entry(ha, ddb_entry->fw_ddb_index, fw_ddb_entry,
				    fw_ddb_entry_dma, NULL, NULL, &ddb_state,
				    NULL, NULL, NULL) == QLA_ERROR) {
		DEBUG2(ql4_printk(KERN_ERR, ha, "scsi%ld: %s: failed "
				  "get_ddb_entry for fw_ddb_index %d\n",
				  ha->host_no, __func__,
				  ddb_entry->fw_ddb_index));
		goto exit_session_conn_fwddb_param;
	}

	cls_sess = ddb_entry->sess;

	cls_conn = ddb_entry->conn;

	
	qla4xxx_copy_fwddb_param(ha, fw_ddb_entry, cls_sess, cls_conn);

exit_session_conn_fwddb_param:
	if (fw_ddb_entry)
		dma_free_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
				  fw_ddb_entry, fw_ddb_entry_dma);
}

void qla4xxx_update_session_conn_param(struct scsi_qla_host *ha,
				       struct ddb_entry *ddb_entry)
{
	struct iscsi_cls_session *cls_sess;
	struct iscsi_cls_conn *cls_conn;
	struct iscsi_session *sess;
	struct iscsi_conn *conn;
	uint32_t ddb_state;
	dma_addr_t fw_ddb_entry_dma;
	struct dev_db_entry *fw_ddb_entry;

	fw_ddb_entry = dma_alloc_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
					  &fw_ddb_entry_dma, GFP_KERNEL);
	if (!fw_ddb_entry) {
		ql4_printk(KERN_ERR, ha,
			   "%s: Unable to allocate dma buffer\n", __func__);
		goto exit_session_conn_param;
	}

	if (qla4xxx_get_fwddb_entry(ha, ddb_entry->fw_ddb_index, fw_ddb_entry,
				    fw_ddb_entry_dma, NULL, NULL, &ddb_state,
				    NULL, NULL, NULL) == QLA_ERROR) {
		DEBUG2(ql4_printk(KERN_ERR, ha, "scsi%ld: %s: failed "
				  "get_ddb_entry for fw_ddb_index %d\n",
				  ha->host_no, __func__,
				  ddb_entry->fw_ddb_index));
		goto exit_session_conn_param;
	}

	cls_sess = ddb_entry->sess;
	sess = cls_sess->dd_data;

	cls_conn = ddb_entry->conn;
	conn = cls_conn->dd_data;

	
	ddb_entry->default_relogin_timeout =
		(le16_to_cpu(fw_ddb_entry->def_timeout) > LOGIN_TOV) &&
		 (le16_to_cpu(fw_ddb_entry->def_timeout) < LOGIN_TOV * 10) ?
		 le16_to_cpu(fw_ddb_entry->def_timeout) : LOGIN_TOV;
	ddb_entry->default_time2wait =
				le16_to_cpu(fw_ddb_entry->iscsi_def_time2wait);

	
	ddb_entry->chap_tbl_idx = le16_to_cpu(fw_ddb_entry->chap_tbl_idx);
	conn->max_recv_dlength = BYTE_UNITS *
			  le16_to_cpu(fw_ddb_entry->iscsi_max_rcv_data_seg_len);

	conn->max_xmit_dlength = BYTE_UNITS *
			  le16_to_cpu(fw_ddb_entry->iscsi_max_snd_data_seg_len);

	sess->initial_r2t_en =
			    (BIT_10 & le16_to_cpu(fw_ddb_entry->iscsi_options));

	sess->max_r2t = le16_to_cpu(fw_ddb_entry->iscsi_max_outsnd_r2t);

	sess->imm_data_en = (BIT_11 & le16_to_cpu(fw_ddb_entry->iscsi_options));

	sess->first_burst = BYTE_UNITS *
			       le16_to_cpu(fw_ddb_entry->iscsi_first_burst_len);

	sess->max_burst = BYTE_UNITS *
				 le16_to_cpu(fw_ddb_entry->iscsi_max_burst_len);

	sess->time2wait = le16_to_cpu(fw_ddb_entry->iscsi_def_time2wait);

	sess->time2retain = le16_to_cpu(fw_ddb_entry->iscsi_def_time2retain);

	sess->tpgt = le32_to_cpu(fw_ddb_entry->tgt_portal_grp);

	memcpy(sess->initiatorname, ha->name_string,
	       min(sizeof(ha->name_string), sizeof(sess->initiatorname)));

	iscsi_set_param(cls_conn, ISCSI_PARAM_TARGET_ALIAS,
			(char *)fw_ddb_entry->iscsi_alias, 0);

exit_session_conn_param:
	if (fw_ddb_entry)
		dma_free_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
				  fw_ddb_entry, fw_ddb_entry_dma);
}


static void qla4xxx_start_timer(struct scsi_qla_host *ha, void *func,
				unsigned long interval)
{
	DEBUG(printk("scsi: %s: Starting timer thread for adapter %d\n",
		     __func__, ha->host->host_no));
	init_timer(&ha->timer);
	ha->timer.expires = jiffies + interval * HZ;
	ha->timer.data = (unsigned long)ha;
	ha->timer.function = (void (*)(unsigned long))func;
	add_timer(&ha->timer);
	ha->timer_active = 1;
}

static void qla4xxx_stop_timer(struct scsi_qla_host *ha)
{
	del_timer_sync(&ha->timer);
	ha->timer_active = 0;
}

void qla4xxx_mark_device_missing(struct iscsi_cls_session *cls_session)
{
	iscsi_block_session(cls_session);
}

void qla4xxx_mark_all_devices_missing(struct scsi_qla_host *ha)
{
	iscsi_host_for_each_session(ha->host, qla4xxx_mark_device_missing);
}

static struct srb* qla4xxx_get_new_srb(struct scsi_qla_host *ha,
				       struct ddb_entry *ddb_entry,
				       struct scsi_cmnd *cmd)
{
	struct srb *srb;

	srb = mempool_alloc(ha->srb_mempool, GFP_ATOMIC);
	if (!srb)
		return srb;

	kref_init(&srb->srb_ref);
	srb->ha = ha;
	srb->ddb = ddb_entry;
	srb->cmd = cmd;
	srb->flags = 0;
	CMD_SP(cmd) = (void *)srb;

	return srb;
}

static void qla4xxx_srb_free_dma(struct scsi_qla_host *ha, struct srb *srb)
{
	struct scsi_cmnd *cmd = srb->cmd;

	if (srb->flags & SRB_DMA_VALID) {
		scsi_dma_unmap(cmd);
		srb->flags &= ~SRB_DMA_VALID;
	}
	CMD_SP(cmd) = NULL;
}

void qla4xxx_srb_compl(struct kref *ref)
{
	struct srb *srb = container_of(ref, struct srb, srb_ref);
	struct scsi_cmnd *cmd = srb->cmd;
	struct scsi_qla_host *ha = srb->ha;

	qla4xxx_srb_free_dma(ha, srb);

	mempool_free(srb, ha->srb_mempool);

	cmd->scsi_done(cmd);
}

static int qla4xxx_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha = to_qla_host(host);
	struct ddb_entry *ddb_entry = cmd->device->hostdata;
	struct iscsi_cls_session *sess = ddb_entry->sess;
	struct srb *srb;
	int rval;

	if (test_bit(AF_EEH_BUSY, &ha->flags)) {
		if (test_bit(AF_PCI_CHANNEL_IO_PERM_FAILURE, &ha->flags))
			cmd->result = DID_NO_CONNECT << 16;
		else
			cmd->result = DID_REQUEUE << 16;
		goto qc_fail_command;
	}

	if (!sess) {
		cmd->result = DID_IMM_RETRY << 16;
		goto qc_fail_command;
	}

	rval = iscsi_session_chkready(sess);
	if (rval) {
		cmd->result = rval;
		goto qc_fail_command;
	}

	if (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_ACTIVE, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	    test_bit(DPC_HA_UNRECOVERABLE, &ha->dpc_flags) ||
	    test_bit(DPC_HA_NEED_QUIESCENT, &ha->dpc_flags) ||
	    !test_bit(AF_ONLINE, &ha->flags) ||
	    !test_bit(AF_LINK_UP, &ha->flags) ||
	    test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags))
		goto qc_host_busy;

	srb = qla4xxx_get_new_srb(ha, ddb_entry, cmd);
	if (!srb)
		goto qc_host_busy;

	rval = qla4xxx_send_command_to_isp(ha, srb);
	if (rval != QLA_SUCCESS)
		goto qc_host_busy_free_sp;

	return 0;

qc_host_busy_free_sp:
	qla4xxx_srb_free_dma(ha, srb);
	mempool_free(srb, ha->srb_mempool);

qc_host_busy:
	return SCSI_MLQUEUE_HOST_BUSY;

qc_fail_command:
	cmd->scsi_done(cmd);

	return 0;
}

static void qla4xxx_mem_free(struct scsi_qla_host *ha)
{
	if (ha->queues)
		dma_free_coherent(&ha->pdev->dev, ha->queues_len, ha->queues,
				  ha->queues_dma);

	ha->queues_len = 0;
	ha->queues = NULL;
	ha->queues_dma = 0;
	ha->request_ring = NULL;
	ha->request_dma = 0;
	ha->response_ring = NULL;
	ha->response_dma = 0;
	ha->shadow_regs = NULL;
	ha->shadow_regs_dma = 0;

	
	if (ha->srb_mempool)
		mempool_destroy(ha->srb_mempool);

	ha->srb_mempool = NULL;

	if (ha->chap_dma_pool)
		dma_pool_destroy(ha->chap_dma_pool);

	if (ha->chap_list)
		vfree(ha->chap_list);
	ha->chap_list = NULL;

	if (ha->fw_ddb_dma_pool)
		dma_pool_destroy(ha->fw_ddb_dma_pool);

	
	if (is_qla8022(ha)) {
		if (ha->nx_pcibase)
			iounmap(
			    (struct device_reg_82xx __iomem *)ha->nx_pcibase);
	} else if (ha->reg)
		iounmap(ha->reg);
	pci_release_regions(ha->pdev);
}

static int qla4xxx_mem_alloc(struct scsi_qla_host *ha)
{
	unsigned long align;

	
	ha->queues_len = ((REQUEST_QUEUE_DEPTH * QUEUE_SIZE) +
			  (RESPONSE_QUEUE_DEPTH * QUEUE_SIZE) +
			  sizeof(struct shadow_regs) +
			  MEM_ALIGN_VALUE +
			  (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	ha->queues = dma_alloc_coherent(&ha->pdev->dev, ha->queues_len,
					&ha->queues_dma, GFP_KERNEL);
	if (ha->queues == NULL) {
		ql4_printk(KERN_WARNING, ha,
		    "Memory Allocation failed - queues.\n");

		goto mem_alloc_error_exit;
	}
	memset(ha->queues, 0, ha->queues_len);

	align = 0;
	if ((unsigned long)ha->queues_dma & (MEM_ALIGN_VALUE - 1))
		align = MEM_ALIGN_VALUE - ((unsigned long)ha->queues_dma &
					   (MEM_ALIGN_VALUE - 1));

	
	ha->request_dma = ha->queues_dma + align;
	ha->request_ring = (struct queue_entry *) (ha->queues + align);
	ha->response_dma = ha->queues_dma + align +
		(REQUEST_QUEUE_DEPTH * QUEUE_SIZE);
	ha->response_ring = (struct queue_entry *) (ha->queues + align +
						    (REQUEST_QUEUE_DEPTH *
						     QUEUE_SIZE));
	ha->shadow_regs_dma = ha->queues_dma + align +
		(REQUEST_QUEUE_DEPTH * QUEUE_SIZE) +
		(RESPONSE_QUEUE_DEPTH * QUEUE_SIZE);
	ha->shadow_regs = (struct shadow_regs *) (ha->queues + align +
						  (REQUEST_QUEUE_DEPTH *
						   QUEUE_SIZE) +
						  (RESPONSE_QUEUE_DEPTH *
						   QUEUE_SIZE));

	
	ha->srb_mempool = mempool_create(SRB_MIN_REQ, mempool_alloc_slab,
					 mempool_free_slab, srb_cachep);
	if (ha->srb_mempool == NULL) {
		ql4_printk(KERN_WARNING, ha,
		    "Memory Allocation failed - SRB Pool.\n");

		goto mem_alloc_error_exit;
	}

	ha->chap_dma_pool = dma_pool_create("ql4_chap", &ha->pdev->dev,
					    CHAP_DMA_BLOCK_SIZE, 8, 0);

	if (ha->chap_dma_pool == NULL) {
		ql4_printk(KERN_WARNING, ha,
		    "%s: chap_dma_pool allocation failed..\n", __func__);
		goto mem_alloc_error_exit;
	}

	ha->fw_ddb_dma_pool = dma_pool_create("ql4_fw_ddb", &ha->pdev->dev,
					      DDB_DMA_BLOCK_SIZE, 8, 0);

	if (ha->fw_ddb_dma_pool == NULL) {
		ql4_printk(KERN_WARNING, ha,
			   "%s: fw_ddb_dma_pool allocation failed..\n",
			   __func__);
		goto mem_alloc_error_exit;
	}

	return QLA_SUCCESS;

mem_alloc_error_exit:
	qla4xxx_mem_free(ha);
	return QLA_ERROR;
}

static int qla4_8xxx_check_temp(struct scsi_qla_host *ha)
{
	uint32_t temp, temp_state, temp_val;
	int status = QLA_SUCCESS;

	temp = qla4_8xxx_rd_32(ha, CRB_TEMP_STATE);

	temp_state = qla82xx_get_temp_state(temp);
	temp_val = qla82xx_get_temp_val(temp);

	if (temp_state == QLA82XX_TEMP_PANIC) {
		ql4_printk(KERN_WARNING, ha, "Device temperature %d degrees C"
			   " exceeds maximum allowed. Hardware has been shut"
			   " down.\n", temp_val);
		status = QLA_ERROR;
	} else if (temp_state == QLA82XX_TEMP_WARN) {
		if (ha->temperature == QLA82XX_TEMP_NORMAL)
			ql4_printk(KERN_WARNING, ha, "Device temperature %d"
				   " degrees C exceeds operating range."
				   " Immediate action needed.\n", temp_val);
	} else {
		if (ha->temperature == QLA82XX_TEMP_WARN)
			ql4_printk(KERN_INFO, ha, "Device temperature is"
				   " now %d degrees C in normal range.\n",
				   temp_val);
	}
	ha->temperature = temp_state;
	return status;
}

static int qla4_8xxx_check_fw_alive(struct scsi_qla_host *ha)
{
	uint32_t fw_heartbeat_counter;
	int status = QLA_SUCCESS;

	fw_heartbeat_counter = qla4_8xxx_rd_32(ha, QLA82XX_PEG_ALIVE_COUNTER);
	
	if (fw_heartbeat_counter == 0xffffffff) {
		DEBUG2(printk(KERN_WARNING "scsi%ld: %s: Device in frozen "
		    "state, QLA82XX_PEG_ALIVE_COUNTER is 0xffffffff\n",
		    ha->host_no, __func__));
		return status;
	}

	if (ha->fw_heartbeat_counter == fw_heartbeat_counter) {
		ha->seconds_since_last_heartbeat++;
		
		if (ha->seconds_since_last_heartbeat == 2) {
			ha->seconds_since_last_heartbeat = 0;

			ql4_printk(KERN_INFO, ha,
				   "scsi(%ld): %s, Dumping hw/fw registers:\n "
				   " PEG_HALT_STATUS1: 0x%x, PEG_HALT_STATUS2:"
				   " 0x%x,\n PEG_NET_0_PC: 0x%x, PEG_NET_1_PC:"
				   " 0x%x,\n PEG_NET_2_PC: 0x%x, PEG_NET_3_PC:"
				   " 0x%x,\n PEG_NET_4_PC: 0x%x\n",
				   ha->host_no, __func__,
				   qla4_8xxx_rd_32(ha,
						   QLA82XX_PEG_HALT_STATUS1),
				   qla4_8xxx_rd_32(ha,
						   QLA82XX_PEG_HALT_STATUS2),
				   qla4_8xxx_rd_32(ha, QLA82XX_CRB_PEG_NET_0 +
						   0x3c),
				   qla4_8xxx_rd_32(ha, QLA82XX_CRB_PEG_NET_1 +
						   0x3c),
				   qla4_8xxx_rd_32(ha, QLA82XX_CRB_PEG_NET_2 +
						   0x3c),
				   qla4_8xxx_rd_32(ha, QLA82XX_CRB_PEG_NET_3 +
						   0x3c),
				   qla4_8xxx_rd_32(ha, QLA82XX_CRB_PEG_NET_4 +
						   0x3c));
			status = QLA_ERROR;
		}
	} else
		ha->seconds_since_last_heartbeat = 0;

	ha->fw_heartbeat_counter = fw_heartbeat_counter;
	return status;
}

void qla4_8xxx_watchdog(struct scsi_qla_host *ha)
{
	uint32_t dev_state, halt_status;

	
	if (!(test_bit(DPC_RESET_ACTIVE, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	    test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags))) {
		dev_state = qla4_8xxx_rd_32(ha, QLA82XX_CRB_DEV_STATE);

		if (qla4_8xxx_check_temp(ha)) {
			ql4_printk(KERN_INFO, ha, "disabling pause"
				   " transmit on port 0 & 1.\n");
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_NIU + 0x98,
					CRB_NIU_XG_PAUSE_CTL_P0 |
					CRB_NIU_XG_PAUSE_CTL_P1);
			set_bit(DPC_HA_UNRECOVERABLE, &ha->dpc_flags);
			qla4xxx_wake_dpc(ha);
		} else if (dev_state == QLA82XX_DEV_NEED_RESET &&
		    !test_bit(DPC_RESET_HA, &ha->dpc_flags)) {
			if (!ql4xdontresethba) {
				ql4_printk(KERN_INFO, ha, "%s: HW State: "
				    "NEED RESET!\n", __func__);
				set_bit(DPC_RESET_HA, &ha->dpc_flags);
				qla4xxx_wake_dpc(ha);
			}
		} else if (dev_state == QLA82XX_DEV_NEED_QUIESCENT &&
		    !test_bit(DPC_HA_NEED_QUIESCENT, &ha->dpc_flags)) {
			ql4_printk(KERN_INFO, ha, "%s: HW State: NEED QUIES!\n",
			    __func__);
			set_bit(DPC_HA_NEED_QUIESCENT, &ha->dpc_flags);
			qla4xxx_wake_dpc(ha);
		} else  {
			
			if (qla4_8xxx_check_fw_alive(ha)) {
				ql4_printk(KERN_INFO, ha, "disabling pause"
					   " transmit on port 0 & 1.\n");
				qla4_8xxx_wr_32(ha, QLA82XX_CRB_NIU + 0x98,
						CRB_NIU_XG_PAUSE_CTL_P0 |
						CRB_NIU_XG_PAUSE_CTL_P1);
				halt_status = qla4_8xxx_rd_32(ha,
						QLA82XX_PEG_HALT_STATUS1);

				if (QLA82XX_FWERROR_CODE(halt_status) == 0x67)
					ql4_printk(KERN_ERR, ha, "%s:"
						   " Firmware aborted with"
						   " error code 0x00006700."
						   " Device is being reset\n",
						   __func__);

				if (halt_status & HALT_STATUS_UNRECOVERABLE)
					set_bit(DPC_HA_UNRECOVERABLE,
						&ha->dpc_flags);
				else {
					ql4_printk(KERN_INFO, ha, "%s: detect "
						   "abort needed!\n", __func__);
					set_bit(DPC_RESET_HA, &ha->dpc_flags);
				}
				qla4xxx_mailbox_premature_completion(ha);
				qla4xxx_wake_dpc(ha);
			}
		}
	}
}

static void qla4xxx_check_relogin_flash_ddb(struct iscsi_cls_session *cls_sess)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	if (!(ddb_entry->ddb_type == FLASH_DDB))
		return;

	if (adapter_up(ha) && !test_bit(DF_RELOGIN, &ddb_entry->flags) &&
	    !iscsi_is_session_online(cls_sess)) {
		if (atomic_read(&ddb_entry->retry_relogin_timer) !=
		    INVALID_ENTRY) {
			if (atomic_read(&ddb_entry->retry_relogin_timer) ==
					0) {
				atomic_set(&ddb_entry->retry_relogin_timer,
					   INVALID_ENTRY);
				set_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags);
				set_bit(DF_RELOGIN, &ddb_entry->flags);
				DEBUG2(ql4_printk(KERN_INFO, ha,
				       "%s: index [%d] login device\n",
					__func__, ddb_entry->fw_ddb_index));
			} else
				atomic_dec(&ddb_entry->retry_relogin_timer);
		}
	}

	
	if (atomic_read(&ddb_entry->relogin_timer) &&
	    (atomic_dec_and_test(&ddb_entry->relogin_timer) != 0)) {
		if (!iscsi_is_session_online(cls_sess)) {
			
			atomic_inc(&ddb_entry->relogin_retry_count);
			DEBUG2(ql4_printk(KERN_INFO, ha,
				"%s: index[%d] relogin timed out-retrying"
				" relogin (%d), retry (%d)\n", __func__,
				ddb_entry->fw_ddb_index,
				atomic_read(&ddb_entry->relogin_retry_count),
				ddb_entry->default_time2wait + 4));
			set_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags);
			atomic_set(&ddb_entry->retry_relogin_timer,
				   ddb_entry->default_time2wait + 4);
		}
	}
}

static void qla4xxx_timer(struct scsi_qla_host *ha)
{
	int start_dpc = 0;
	uint16_t w;

	iscsi_host_for_each_session(ha->host, qla4xxx_check_relogin_flash_ddb);

	if (test_bit(AF_EEH_BUSY, &ha->flags)) {
		mod_timer(&ha->timer, jiffies + HZ);
		return;
	}

	
	if (!pci_channel_offline(ha->pdev))
		pci_read_config_word(ha->pdev, PCI_VENDOR_ID, &w);

	if (is_qla8022(ha)) {
		qla4_8xxx_watchdog(ha);
	}

	if (!is_qla8022(ha)) {
		
		if (ha->firmware_options & FWOPT_HEARTBEAT_ENABLE &&
		    ha->heartbeat_interval != 0) {
			ha->seconds_since_last_heartbeat++;
			if (ha->seconds_since_last_heartbeat >
			    ha->heartbeat_interval + 2)
				set_bit(DPC_RESET_HA, &ha->dpc_flags);
		}
	}

	
	if (!list_empty(&ha->work_list))
		start_dpc++;

	
	if (start_dpc ||
	     test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	     test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags) ||
	     test_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags) ||
	     test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags) ||
	     test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	     test_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags) ||
	     test_bit(DPC_LINK_CHANGED, &ha->dpc_flags) ||
	     test_bit(DPC_HA_UNRECOVERABLE, &ha->dpc_flags) ||
	     test_bit(DPC_HA_NEED_QUIESCENT, &ha->dpc_flags) ||
	     test_bit(DPC_AEN, &ha->dpc_flags)) {
		DEBUG2(printk("scsi%ld: %s: scheduling dpc routine"
			      " - dpc flags = 0x%lx\n",
			      ha->host_no, __func__, ha->dpc_flags));
		qla4xxx_wake_dpc(ha);
	}

	
	mod_timer(&ha->timer, jiffies + HZ);

	DEBUG2(ha->seconds_since_last_intr++);
}

static int qla4xxx_cmd_wait(struct scsi_qla_host *ha)
{
	uint32_t index = 0;
	unsigned long flags;
	struct scsi_cmnd *cmd;

	unsigned long wtime = jiffies + (WAIT_CMD_TOV * HZ);

	DEBUG2(ql4_printk(KERN_INFO, ha, "Wait up to %d seconds for cmds to "
	    "complete\n", WAIT_CMD_TOV));

	while (!time_after_eq(jiffies, wtime)) {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		
		for (index = 0; index < ha->host->can_queue; index++) {
			cmd = scsi_host_find_tag(ha->host, index);
			if (cmd != NULL && CMD_SP(cmd))
				break;
		}
		spin_unlock_irqrestore(&ha->hardware_lock, flags);

		
		if (index == ha->host->can_queue)
			return QLA_SUCCESS;

		msleep(1000);
	}
	return QLA_ERROR;
}

int qla4xxx_hw_reset(struct scsi_qla_host *ha)
{
	uint32_t ctrl_status;
	unsigned long flags = 0;

	DEBUG2(printk(KERN_ERR "scsi%ld: %s\n", ha->host_no, __func__));

	if (ql4xxx_lock_drvr_wait(ha) != QLA_SUCCESS)
		return QLA_ERROR;

	spin_lock_irqsave(&ha->hardware_lock, flags);

	ctrl_status = readw(&ha->reg->ctrl_status);
	if ((ctrl_status & CSR_SCSI_RESET_INTR) != 0)
		writel(set_rmask(CSR_SCSI_RESET_INTR), &ha->reg->ctrl_status);

	
	writel(set_rmask(CSR_SOFT_RESET), &ha->reg->ctrl_status);
	readl(&ha->reg->ctrl_status);

	spin_unlock_irqrestore(&ha->hardware_lock, flags);
	return QLA_SUCCESS;
}

int qla4xxx_soft_reset(struct scsi_qla_host *ha)
{
	uint32_t max_wait_time;
	unsigned long flags = 0;
	int status;
	uint32_t ctrl_status;

	status = qla4xxx_hw_reset(ha);
	if (status != QLA_SUCCESS)
		return status;

	status = QLA_ERROR;
	
	max_wait_time = RESET_INTR_TOV;
	do {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		ctrl_status = readw(&ha->reg->ctrl_status);
		spin_unlock_irqrestore(&ha->hardware_lock, flags);

		if ((ctrl_status & CSR_NET_RESET_INTR) == 0)
			break;

		msleep(1000);
	} while ((--max_wait_time));

	if ((ctrl_status & CSR_NET_RESET_INTR) != 0) {
		DEBUG2(printk(KERN_WARNING
			      "scsi%ld: Network Reset Intr not cleared by "
			      "Network function, clearing it now!\n",
			      ha->host_no));
		spin_lock_irqsave(&ha->hardware_lock, flags);
		writel(set_rmask(CSR_NET_RESET_INTR), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
	}

	
	max_wait_time = SOFT_RESET_TOV;
	do {
		spin_lock_irqsave(&ha->hardware_lock, flags);
		ctrl_status = readw(&ha->reg->ctrl_status);
		spin_unlock_irqrestore(&ha->hardware_lock, flags);

		if ((ctrl_status & CSR_SOFT_RESET) == 0) {
			status = QLA_SUCCESS;
			break;
		}

		msleep(1000);
	} while ((--max_wait_time));

	spin_lock_irqsave(&ha->hardware_lock, flags);
	ctrl_status = readw(&ha->reg->ctrl_status);
	if ((ctrl_status & CSR_SCSI_RESET_INTR) != 0) {
		writel(set_rmask(CSR_SCSI_RESET_INTR), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	if (max_wait_time == 0) {
		
		spin_lock_irqsave(&ha->hardware_lock, flags);
		writel(set_rmask(CSR_FORCE_SOFT_RESET), &ha->reg->ctrl_status);
		readl(&ha->reg->ctrl_status);
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		
		max_wait_time = SOFT_RESET_TOV;
		do {
			spin_lock_irqsave(&ha->hardware_lock, flags);
			ctrl_status = readw(&ha->reg->ctrl_status);
			spin_unlock_irqrestore(&ha->hardware_lock, flags);

			if ((ctrl_status & CSR_FORCE_SOFT_RESET) == 0) {
				status = QLA_SUCCESS;
				break;
			}

			msleep(1000);
		} while ((--max_wait_time));
	}

	return status;
}

static void qla4xxx_abort_active_cmds(struct scsi_qla_host *ha, int res)
{
	struct srb *srb;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&ha->hardware_lock, flags);
	for (i = 0; i < ha->host->can_queue; i++) {
		srb = qla4xxx_del_from_active_array(ha, i);
		if (srb != NULL) {
			srb->cmd->result = res;
			kref_put(&srb->srb_ref, qla4xxx_srb_compl);
		}
	}
	spin_unlock_irqrestore(&ha->hardware_lock, flags);
}

void qla4xxx_dead_adapter_cleanup(struct scsi_qla_host *ha)
{
	clear_bit(AF_ONLINE, &ha->flags);

	
	ql4_printk(KERN_INFO, ha, "Disabling the board\n");

	qla4xxx_abort_active_cmds(ha, DID_NO_CONNECT << 16);
	qla4xxx_mark_all_devices_missing(ha);
	clear_bit(AF_INIT_DONE, &ha->flags);
}

static void qla4xxx_fail_session(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;

	sess = cls_session->dd_data;
	ddb_entry = sess->dd_data;
	ddb_entry->fw_ddb_device_state = DDB_DS_SESSION_FAILED;

	if (ddb_entry->ddb_type == FLASH_DDB)
		iscsi_block_session(ddb_entry->sess);
	else
		iscsi_session_failure(cls_session->dd_data,
				      ISCSI_ERR_CONN_FAILED);
}

static int qla4xxx_recover_adapter(struct scsi_qla_host *ha)
{
	int status = QLA_ERROR;
	uint8_t reset_chip = 0;
	uint32_t dev_state;
	unsigned long wait;

	
	scsi_block_requests(ha->host);
	clear_bit(AF_ONLINE, &ha->flags);
	clear_bit(AF_LINK_UP, &ha->flags);

	DEBUG2(ql4_printk(KERN_INFO, ha, "%s: adapter OFFLINE\n", __func__));

	set_bit(DPC_RESET_ACTIVE, &ha->dpc_flags);

	iscsi_host_for_each_session(ha->host, qla4xxx_fail_session);

	if (test_bit(DPC_RESET_HA, &ha->dpc_flags))
		reset_chip = 1;

	if (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags)) {
		status = QLA_SUCCESS;
		goto recover_ha_init_adapter;
	}

	if (is_qla8022(ha) && !reset_chip &&
	    test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags)) {

		DEBUG2(ql4_printk(KERN_INFO, ha,
		    "scsi%ld: %s - Performing stop_firmware...\n",
		    ha->host_no, __func__));
		status = ha->isp_ops->reset_firmware(ha);
		if (status == QLA_SUCCESS) {
			if (!test_bit(AF_FW_RECOVERY, &ha->flags))
				qla4xxx_cmd_wait(ha);
			ha->isp_ops->disable_intrs(ha);
			qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);
			qla4xxx_abort_active_cmds(ha, DID_RESET << 16);
		} else {
			reset_chip = 1;
			clear_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags);
			set_bit(DPC_RESET_HA, &ha->dpc_flags);
		}
	}

	if (!is_qla8022(ha) || reset_chip) {
		if (!is_qla8022(ha))
			goto chip_reset;

		if (test_bit(AF_FW_RECOVERY, &ha->flags))
			goto chip_reset;

		wait = jiffies + (FW_ALIVE_WAIT_TOV * HZ);
		while (time_before(jiffies, wait)) {
			if (qla4_8xxx_check_fw_alive(ha)) {
				qla4xxx_mailbox_premature_completion(ha);
				break;
			}

			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout(HZ);
		}

		if (!test_bit(AF_FW_RECOVERY, &ha->flags))
			qla4xxx_cmd_wait(ha);
chip_reset:
		qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);
		qla4xxx_abort_active_cmds(ha, DID_RESET << 16);
		DEBUG2(ql4_printk(KERN_INFO, ha,
		    "scsi%ld: %s - Performing chip reset..\n",
		    ha->host_no, __func__));
		status = ha->isp_ops->reset_chip(ha);
	}

	
	qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);

recover_ha_init_adapter:
	
	if (status == QLA_SUCCESS) {
		if (!is_qla8022(ha) && (ha->mac_index == 3))
			ssleep(6);

		status = qla4xxx_initialize_adapter(ha, RESET_ADAPTER);
	}

	if (!test_bit(AF_ONLINE, &ha->flags) &&
	    !test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags)) {
		if (is_qla8022(ha)) {
			qla4_8xxx_idc_lock(ha);
			dev_state = qla4_8xxx_rd_32(ha, QLA82XX_CRB_DEV_STATE);
			qla4_8xxx_idc_unlock(ha);
			if (dev_state == QLA82XX_DEV_FAILED) {
				ql4_printk(KERN_INFO, ha, "%s: don't retry "
					   "recover adapter. H/W is in Failed "
					   "state\n", __func__);
				qla4xxx_dead_adapter_cleanup(ha);
				clear_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
				clear_bit(DPC_RESET_HA, &ha->dpc_flags);
				clear_bit(DPC_RESET_HA_FW_CONTEXT,
						&ha->dpc_flags);
				status = QLA_ERROR;

				goto exit_recover;
			}
		}

		if (!test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags)) {
			ha->retry_reset_ha_cnt = MAX_RESET_HA_RETRIES;
			DEBUG2(printk("scsi%ld: recover adapter - retrying "
				      "(%d) more times\n", ha->host_no,
				      ha->retry_reset_ha_cnt));
			set_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
			status = QLA_ERROR;
		} else {
			if (ha->retry_reset_ha_cnt > 0) {
				
				ha->retry_reset_ha_cnt--;
				DEBUG2(printk("scsi%ld: recover adapter - "
					      "retry remaining %d\n",
					      ha->host_no,
					      ha->retry_reset_ha_cnt));
				status = QLA_ERROR;
			}

			if (ha->retry_reset_ha_cnt == 0) {
				DEBUG2(printk("scsi%ld: recover adapter "
					      "failed - board disabled\n",
					      ha->host_no));
				qla4xxx_dead_adapter_cleanup(ha);
				clear_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
				clear_bit(DPC_RESET_HA, &ha->dpc_flags);
				clear_bit(DPC_RESET_HA_FW_CONTEXT,
					  &ha->dpc_flags);
				status = QLA_ERROR;
			}
		}
	} else {
		clear_bit(DPC_RESET_HA, &ha->dpc_flags);
		clear_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags);
		clear_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags);
	}

exit_recover:
	ha->adapter_error_count++;

	if (test_bit(AF_ONLINE, &ha->flags))
		ha->isp_ops->enable_intrs(ha);

	scsi_unblock_requests(ha->host);

	clear_bit(DPC_RESET_ACTIVE, &ha->dpc_flags);
	DEBUG2(printk("scsi%ld: recover adapter: %s\n", ha->host_no,
	    status == QLA_ERROR ? "FAILED" : "SUCCEEDED"));

	return status;
}

static void qla4xxx_relogin_devices(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_session->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;
	if (!iscsi_is_session_online(cls_session)) {
		if (ddb_entry->fw_ddb_device_state == DDB_DS_SESSION_ACTIVE) {
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: ddb[%d]"
				   " unblock session\n", ha->host_no, __func__,
				   ddb_entry->fw_ddb_index);
			iscsi_unblock_session(ddb_entry->sess);
		} else {
			
			if (ddb_entry->ddb_type == FLASH_DDB) {
				if (!test_bit(DF_RELOGIN, &ddb_entry->flags))
					qla4xxx_arm_relogin_timer(ddb_entry);
			} else
				iscsi_session_failure(cls_session->dd_data,
						      ISCSI_ERR_CONN_FAILED);
		}
	}
}

int qla4xxx_unblock_flash_ddb(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_session->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;
	ql4_printk(KERN_INFO, ha, "scsi%ld: %s: ddb[%d]"
		   " unblock session\n", ha->host_no, __func__,
		   ddb_entry->fw_ddb_index);

	iscsi_unblock_session(ddb_entry->sess);

	
	if (test_bit(AF_ONLINE, &ha->flags)) {
		ql4_printk(KERN_INFO, ha, "scsi%ld: %s: ddb[%d]"
			   " start scan\n", ha->host_no, __func__,
			   ddb_entry->fw_ddb_index);
		scsi_queue_work(ha->host, &ddb_entry->sess->scan_work);
	}
	return QLA_SUCCESS;
}

int qla4xxx_unblock_ddb(struct iscsi_cls_session *cls_session)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_session->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;
	ql4_printk(KERN_INFO, ha, "scsi%ld: %s: ddb[%d]"
		   " unblock user space session\n", ha->host_no, __func__,
		   ddb_entry->fw_ddb_index);
	iscsi_conn_start(ddb_entry->conn);
	iscsi_conn_login_event(ddb_entry->conn,
			       ISCSI_CONN_STATE_LOGGED_IN);

	return QLA_SUCCESS;
}

static void qla4xxx_relogin_all_devices(struct scsi_qla_host *ha)
{
	iscsi_host_for_each_session(ha->host, qla4xxx_relogin_devices);
}

static void qla4xxx_relogin_flash_ddb(struct iscsi_cls_session *cls_sess)
{
	uint16_t relogin_timer;
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	relogin_timer = max(ddb_entry->default_relogin_timeout,
			    (uint16_t)RELOGIN_TOV);
	atomic_set(&ddb_entry->relogin_timer, relogin_timer);

	DEBUG2(ql4_printk(KERN_INFO, ha,
			  "scsi%ld: Relogin index [%d]. TOV=%d\n", ha->host_no,
			  ddb_entry->fw_ddb_index, relogin_timer));

	qla4xxx_login_flash_ddb(cls_sess);
}

static void qla4xxx_dpc_relogin(struct iscsi_cls_session *cls_sess)
{
	struct iscsi_session *sess;
	struct ddb_entry *ddb_entry;
	struct scsi_qla_host *ha;

	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ha = ddb_entry->ha;

	if (!(ddb_entry->ddb_type == FLASH_DDB))
		return;

	if (test_and_clear_bit(DF_RELOGIN, &ddb_entry->flags) &&
	    !iscsi_is_session_online(cls_sess)) {
		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "relogin issued\n"));
		qla4xxx_relogin_flash_ddb(cls_sess);
	}
}

void qla4xxx_wake_dpc(struct scsi_qla_host *ha)
{
	if (ha->dpc_thread)
		queue_work(ha->dpc_thread, &ha->dpc_work);
}

static struct qla4_work_evt *
qla4xxx_alloc_work(struct scsi_qla_host *ha, uint32_t data_size,
		   enum qla4_work_type type)
{
	struct qla4_work_evt *e;
	uint32_t size = sizeof(struct qla4_work_evt) + data_size;

	e = kzalloc(size, GFP_ATOMIC);
	if (!e)
		return NULL;

	INIT_LIST_HEAD(&e->list);
	e->type = type;
	return e;
}

static void qla4xxx_post_work(struct scsi_qla_host *ha,
			     struct qla4_work_evt *e)
{
	unsigned long flags;

	spin_lock_irqsave(&ha->work_lock, flags);
	list_add_tail(&e->list, &ha->work_list);
	spin_unlock_irqrestore(&ha->work_lock, flags);
	qla4xxx_wake_dpc(ha);
}

int qla4xxx_post_aen_work(struct scsi_qla_host *ha,
			  enum iscsi_host_event_code aen_code,
			  uint32_t data_size, uint8_t *data)
{
	struct qla4_work_evt *e;

	e = qla4xxx_alloc_work(ha, data_size, QLA4_EVENT_AEN);
	if (!e)
		return QLA_ERROR;

	e->u.aen.code = aen_code;
	e->u.aen.data_size = data_size;
	memcpy(e->u.aen.data, data, data_size);

	qla4xxx_post_work(ha, e);

	return QLA_SUCCESS;
}

int qla4xxx_post_ping_evt_work(struct scsi_qla_host *ha,
			       uint32_t status, uint32_t pid,
			       uint32_t data_size, uint8_t *data)
{
	struct qla4_work_evt *e;

	e = qla4xxx_alloc_work(ha, data_size, QLA4_EVENT_PING_STATUS);
	if (!e)
		return QLA_ERROR;

	e->u.ping.status = status;
	e->u.ping.pid = pid;
	e->u.ping.data_size = data_size;
	memcpy(e->u.ping.data, data, data_size);

	qla4xxx_post_work(ha, e);

	return QLA_SUCCESS;
}

static void qla4xxx_do_work(struct scsi_qla_host *ha)
{
	struct qla4_work_evt *e, *tmp;
	unsigned long flags;
	LIST_HEAD(work);

	spin_lock_irqsave(&ha->work_lock, flags);
	list_splice_init(&ha->work_list, &work);
	spin_unlock_irqrestore(&ha->work_lock, flags);

	list_for_each_entry_safe(e, tmp, &work, list) {
		list_del_init(&e->list);

		switch (e->type) {
		case QLA4_EVENT_AEN:
			iscsi_post_host_event(ha->host_no,
					      &qla4xxx_iscsi_transport,
					      e->u.aen.code,
					      e->u.aen.data_size,
					      e->u.aen.data);
			break;
		case QLA4_EVENT_PING_STATUS:
			iscsi_ping_comp_event(ha->host_no,
					      &qla4xxx_iscsi_transport,
					      e->u.ping.status,
					      e->u.ping.pid,
					      e->u.ping.data_size,
					      e->u.ping.data);
			break;
		default:
			ql4_printk(KERN_WARNING, ha, "event type: 0x%x not "
				   "supported", e->type);
		}
		kfree(e);
	}
}

static void qla4xxx_do_dpc(struct work_struct *work)
{
	struct scsi_qla_host *ha =
		container_of(work, struct scsi_qla_host, dpc_work);
	int status = QLA_ERROR;

	DEBUG2(printk("scsi%ld: %s: DPC handler waking up."
	    "flags = 0x%08lx, dpc_flags = 0x%08lx\n",
	    ha->host_no, __func__, ha->flags, ha->dpc_flags))

	
	if (!test_bit(AF_INIT_DONE, &ha->flags))
		return;

	if (test_bit(AF_EEH_BUSY, &ha->flags)) {
		DEBUG2(printk(KERN_INFO "scsi%ld: %s: flags = %lx\n",
		    ha->host_no, __func__, ha->flags));
		return;
	}

	
	qla4xxx_do_work(ha);

	if (is_qla8022(ha)) {
		if (test_bit(DPC_HA_UNRECOVERABLE, &ha->dpc_flags)) {
			qla4_8xxx_idc_lock(ha);
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_FAILED);
			qla4_8xxx_idc_unlock(ha);
			ql4_printk(KERN_INFO, ha, "HW State: FAILED\n");
			qla4_8xxx_device_state_handler(ha);
		}
		if (test_and_clear_bit(DPC_HA_NEED_QUIESCENT, &ha->dpc_flags)) {
			qla4_8xxx_need_qsnt_handler(ha);
		}
	}

	if (!test_bit(DPC_RESET_ACTIVE, &ha->dpc_flags) &&
	    (test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	    test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags))) {
		if (ql4xdontresethba) {
			DEBUG2(printk("scsi%ld: %s: Don't Reset HBA\n",
			    ha->host_no, __func__));
			clear_bit(DPC_RESET_HA, &ha->dpc_flags);
			clear_bit(DPC_RESET_HA_INTR, &ha->dpc_flags);
			clear_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags);
			goto dpc_post_reset_ha;
		}
		if (test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags) ||
		    test_bit(DPC_RESET_HA, &ha->dpc_flags))
			qla4xxx_recover_adapter(ha);

		if (test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags)) {
			uint8_t wait_time = RESET_INTR_TOV;

			while ((readw(&ha->reg->ctrl_status) &
				(CSR_SOFT_RESET | CSR_FORCE_SOFT_RESET)) != 0) {
				if (--wait_time == 0)
					break;
				msleep(1000);
			}
			if (wait_time == 0)
				DEBUG2(printk("scsi%ld: %s: SR|FSR "
					      "bit not cleared-- resetting\n",
					      ha->host_no, __func__));
			qla4xxx_abort_active_cmds(ha, DID_RESET << 16);
			if (ql4xxx_lock_drvr_wait(ha) == QLA_SUCCESS) {
				qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);
				status = qla4xxx_recover_adapter(ha);
			}
			clear_bit(DPC_RESET_HA_INTR, &ha->dpc_flags);
			if (status == QLA_SUCCESS)
				ha->isp_ops->enable_intrs(ha);
		}
	}

dpc_post_reset_ha:
	
	if (test_and_clear_bit(DPC_AEN, &ha->dpc_flags))
		qla4xxx_process_aen(ha, PROCESS_ALL_AENS);

	
	if (test_and_clear_bit(DPC_GET_DHCP_IP_ADDR, &ha->dpc_flags))
		qla4xxx_get_dhcp_ip_address(ha);

	
	if (adapter_up(ha) &&
	    test_and_clear_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags)) {
		iscsi_host_for_each_session(ha->host, qla4xxx_dpc_relogin);
	}

	
	if (test_and_clear_bit(DPC_LINK_CHANGED, &ha->dpc_flags)) {
		if (!test_bit(AF_LINK_UP, &ha->flags)) {
			
			qla4xxx_mark_all_devices_missing(ha);
		} else {
			if (test_and_clear_bit(AF_BUILD_DDB_LIST, &ha->flags)) {
				qla4xxx_build_ddb_list(ha, ha->is_reset);
				iscsi_host_for_each_session(ha->host,
						qla4xxx_login_flash_ddb);
			} else
				qla4xxx_relogin_all_devices(ha);
		}
	}
}

static void qla4xxx_free_adapter(struct scsi_qla_host *ha)
{
	qla4xxx_abort_active_cmds(ha, DID_NO_CONNECT << 16);

	if (test_bit(AF_INTERRUPTS_ON, &ha->flags)) {
		
		ha->isp_ops->disable_intrs(ha);
	}

	
	if (ha->timer_active)
		qla4xxx_stop_timer(ha);

	
	if (ha->dpc_thread)
		destroy_workqueue(ha->dpc_thread);

	
	if (ha->task_wq)
		destroy_workqueue(ha->task_wq);

	
	ha->isp_ops->reset_firmware(ha);

	if (is_qla8022(ha)) {
		qla4_8xxx_idc_lock(ha);
		qla4_8xxx_clear_drv_active(ha);
		qla4_8xxx_idc_unlock(ha);
	}

	
	if (test_and_clear_bit(AF_IRQ_ATTACHED, &ha->flags))
		qla4xxx_free_irqs(ha);

	
	qla4xxx_mem_free(ha);
}

int qla4_8xxx_iospace_config(struct scsi_qla_host *ha)
{
	int status = 0;
	unsigned long mem_base, mem_len, db_base, db_len;
	struct pci_dev *pdev = ha->pdev;

	status = pci_request_regions(pdev, DRIVER_NAME);
	if (status) {
		printk(KERN_WARNING
		    "scsi(%ld) Failed to reserve PIO regions (%s) "
		    "status=%d\n", ha->host_no, pci_name(pdev), status);
		goto iospace_error_exit;
	}

	DEBUG2(printk(KERN_INFO "%s: revision-id=%d\n",
	    __func__, pdev->revision));
	ha->revision_id = pdev->revision;

	
	mem_base = pci_resource_start(pdev, 0); 
	mem_len = pci_resource_len(pdev, 0);
	DEBUG2(printk(KERN_INFO "%s: ioremap from %lx a size of %lx\n",
	    __func__, mem_base, mem_len));

	
	ha->nx_pcibase = (unsigned long)ioremap(mem_base, mem_len);
	if (!ha->nx_pcibase) {
		printk(KERN_ERR
		    "cannot remap MMIO (%s), aborting\n", pci_name(pdev));
		pci_release_regions(ha->pdev);
		goto iospace_error_exit;
	}

	

	
	ha->qla4_8xxx_reg =
	    (struct device_reg_82xx  __iomem *)((uint8_t *)ha->nx_pcibase +
	    0xbc000 + (ha->pdev->devfn << 11));

	db_base = pci_resource_start(pdev, 4);  
	db_len = pci_resource_len(pdev, 4);

	ha->nx_db_wr_ptr = (ha->pdev->devfn == 4 ? QLA82XX_CAM_RAM_DB1 :
	    QLA82XX_CAM_RAM_DB2);

	return 0;
iospace_error_exit:
	return -ENOMEM;
}

int qla4xxx_iospace_config(struct scsi_qla_host *ha)
{
	unsigned long pio, pio_len, pio_flags;
	unsigned long mmio, mmio_len, mmio_flags;

	pio = pci_resource_start(ha->pdev, 0);
	pio_len = pci_resource_len(ha->pdev, 0);
	pio_flags = pci_resource_flags(ha->pdev, 0);
	if (pio_flags & IORESOURCE_IO) {
		if (pio_len < MIN_IOBASE_LEN) {
			ql4_printk(KERN_WARNING, ha,
				"Invalid PCI I/O region size\n");
			pio = 0;
		}
	} else {
		ql4_printk(KERN_WARNING, ha, "region #0 not a PIO resource\n");
		pio = 0;
	}

	
	mmio = pci_resource_start(ha->pdev, 1);
	mmio_len = pci_resource_len(ha->pdev, 1);
	mmio_flags = pci_resource_flags(ha->pdev, 1);

	if (!(mmio_flags & IORESOURCE_MEM)) {
		ql4_printk(KERN_ERR, ha,
		    "region #0 not an MMIO resource, aborting\n");

		goto iospace_error_exit;
	}

	if (mmio_len < MIN_IOBASE_LEN) {
		ql4_printk(KERN_ERR, ha,
		    "Invalid PCI mem region size, aborting\n");
		goto iospace_error_exit;
	}

	if (pci_request_regions(ha->pdev, DRIVER_NAME)) {
		ql4_printk(KERN_WARNING, ha,
		    "Failed to reserve PIO/MMIO regions\n");

		goto iospace_error_exit;
	}

	ha->pio_address = pio;
	ha->pio_length = pio_len;
	ha->reg = ioremap(mmio, MIN_IOBASE_LEN);
	if (!ha->reg) {
		ql4_printk(KERN_ERR, ha,
		    "cannot remap MMIO, aborting\n");

		goto iospace_error_exit;
	}

	return 0;

iospace_error_exit:
	return -ENOMEM;
}

static struct isp_operations qla4xxx_isp_ops = {
	.iospace_config         = qla4xxx_iospace_config,
	.pci_config             = qla4xxx_pci_config,
	.disable_intrs          = qla4xxx_disable_intrs,
	.enable_intrs           = qla4xxx_enable_intrs,
	.start_firmware         = qla4xxx_start_firmware,
	.intr_handler           = qla4xxx_intr_handler,
	.interrupt_service_routine = qla4xxx_interrupt_service_routine,
	.reset_chip             = qla4xxx_soft_reset,
	.reset_firmware         = qla4xxx_hw_reset,
	.queue_iocb             = qla4xxx_queue_iocb,
	.complete_iocb          = qla4xxx_complete_iocb,
	.rd_shdw_req_q_out      = qla4xxx_rd_shdw_req_q_out,
	.rd_shdw_rsp_q_in       = qla4xxx_rd_shdw_rsp_q_in,
	.get_sys_info           = qla4xxx_get_sys_info,
};

static struct isp_operations qla4_8xxx_isp_ops = {
	.iospace_config         = qla4_8xxx_iospace_config,
	.pci_config             = qla4_8xxx_pci_config,
	.disable_intrs          = qla4_8xxx_disable_intrs,
	.enable_intrs           = qla4_8xxx_enable_intrs,
	.start_firmware         = qla4_8xxx_load_risc,
	.intr_handler           = qla4_8xxx_intr_handler,
	.interrupt_service_routine = qla4_8xxx_interrupt_service_routine,
	.reset_chip             = qla4_8xxx_isp_reset,
	.reset_firmware         = qla4_8xxx_stop_firmware,
	.queue_iocb             = qla4_8xxx_queue_iocb,
	.complete_iocb          = qla4_8xxx_complete_iocb,
	.rd_shdw_req_q_out      = qla4_8xxx_rd_shdw_req_q_out,
	.rd_shdw_rsp_q_in       = qla4_8xxx_rd_shdw_rsp_q_in,
	.get_sys_info           = qla4_8xxx_get_sys_info,
};

uint16_t qla4xxx_rd_shdw_req_q_out(struct scsi_qla_host *ha)
{
	return (uint16_t)le32_to_cpu(ha->shadow_regs->req_q_out);
}

uint16_t qla4_8xxx_rd_shdw_req_q_out(struct scsi_qla_host *ha)
{
	return (uint16_t)le32_to_cpu(readl(&ha->qla4_8xxx_reg->req_q_out));
}

uint16_t qla4xxx_rd_shdw_rsp_q_in(struct scsi_qla_host *ha)
{
	return (uint16_t)le32_to_cpu(ha->shadow_regs->rsp_q_in);
}

uint16_t qla4_8xxx_rd_shdw_rsp_q_in(struct scsi_qla_host *ha)
{
	return (uint16_t)le32_to_cpu(readl(&ha->qla4_8xxx_reg->rsp_q_in));
}

static ssize_t qla4xxx_show_boot_eth_info(void *data, int type, char *buf)
{
	struct scsi_qla_host *ha = data;
	char *str = buf;
	int rc;

	switch (type) {
	case ISCSI_BOOT_ETH_FLAGS:
		rc = sprintf(str, "%d\n", SYSFS_FLAG_FW_SEL_BOOT);
		break;
	case ISCSI_BOOT_ETH_INDEX:
		rc = sprintf(str, "0\n");
		break;
	case ISCSI_BOOT_ETH_MAC:
		rc = sysfs_format_mac(str, ha->my_mac,
				      MAC_ADDR_LEN);
		break;
	default:
		rc = -ENOSYS;
		break;
	}
	return rc;
}

static umode_t qla4xxx_eth_get_attr_visibility(void *data, int type)
{
	int rc;

	switch (type) {
	case ISCSI_BOOT_ETH_FLAGS:
	case ISCSI_BOOT_ETH_MAC:
	case ISCSI_BOOT_ETH_INDEX:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static ssize_t qla4xxx_show_boot_ini_info(void *data, int type, char *buf)
{
	struct scsi_qla_host *ha = data;
	char *str = buf;
	int rc;

	switch (type) {
	case ISCSI_BOOT_INI_INITIATOR_NAME:
		rc = sprintf(str, "%s\n", ha->name_string);
		break;
	default:
		rc = -ENOSYS;
		break;
	}
	return rc;
}

static umode_t qla4xxx_ini_get_attr_visibility(void *data, int type)
{
	int rc;

	switch (type) {
	case ISCSI_BOOT_INI_INITIATOR_NAME:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static ssize_t
qla4xxx_show_boot_tgt_info(struct ql4_boot_session_info *boot_sess, int type,
			   char *buf)
{
	struct ql4_conn_info *boot_conn = &boot_sess->conn_list[0];
	char *str = buf;
	int rc;

	switch (type) {
	case ISCSI_BOOT_TGT_NAME:
		rc = sprintf(buf, "%s\n", (char *)&boot_sess->target_name);
		break;
	case ISCSI_BOOT_TGT_IP_ADDR:
		if (boot_sess->conn_list[0].dest_ipaddr.ip_type == 0x1)
			rc = sprintf(buf, "%pI4\n",
				     &boot_conn->dest_ipaddr.ip_address);
		else
			rc = sprintf(str, "%pI6\n",
				     &boot_conn->dest_ipaddr.ip_address);
		break;
	case ISCSI_BOOT_TGT_PORT:
			rc = sprintf(str, "%d\n", boot_conn->dest_port);
		break;
	case ISCSI_BOOT_TGT_CHAP_NAME:
		rc = sprintf(str,  "%.*s\n",
			     boot_conn->chap.target_chap_name_length,
			     (char *)&boot_conn->chap.target_chap_name);
		break;
	case ISCSI_BOOT_TGT_CHAP_SECRET:
		rc = sprintf(str,  "%.*s\n",
			     boot_conn->chap.target_secret_length,
			     (char *)&boot_conn->chap.target_secret);
		break;
	case ISCSI_BOOT_TGT_REV_CHAP_NAME:
		rc = sprintf(str,  "%.*s\n",
			     boot_conn->chap.intr_chap_name_length,
			     (char *)&boot_conn->chap.intr_chap_name);
		break;
	case ISCSI_BOOT_TGT_REV_CHAP_SECRET:
		rc = sprintf(str,  "%.*s\n",
			     boot_conn->chap.intr_secret_length,
			     (char *)&boot_conn->chap.intr_secret);
		break;
	case ISCSI_BOOT_TGT_FLAGS:
		rc = sprintf(str, "%d\n", SYSFS_FLAG_FW_SEL_BOOT);
		break;
	case ISCSI_BOOT_TGT_NIC_ASSOC:
		rc = sprintf(str, "0\n");
		break;
	default:
		rc = -ENOSYS;
		break;
	}
	return rc;
}

static ssize_t qla4xxx_show_boot_tgt_pri_info(void *data, int type, char *buf)
{
	struct scsi_qla_host *ha = data;
	struct ql4_boot_session_info *boot_sess = &(ha->boot_tgt.boot_pri_sess);

	return qla4xxx_show_boot_tgt_info(boot_sess, type, buf);
}

static ssize_t qla4xxx_show_boot_tgt_sec_info(void *data, int type, char *buf)
{
	struct scsi_qla_host *ha = data;
	struct ql4_boot_session_info *boot_sess = &(ha->boot_tgt.boot_sec_sess);

	return qla4xxx_show_boot_tgt_info(boot_sess, type, buf);
}

static umode_t qla4xxx_tgt_get_attr_visibility(void *data, int type)
{
	int rc;

	switch (type) {
	case ISCSI_BOOT_TGT_NAME:
	case ISCSI_BOOT_TGT_IP_ADDR:
	case ISCSI_BOOT_TGT_PORT:
	case ISCSI_BOOT_TGT_CHAP_NAME:
	case ISCSI_BOOT_TGT_CHAP_SECRET:
	case ISCSI_BOOT_TGT_REV_CHAP_NAME:
	case ISCSI_BOOT_TGT_REV_CHAP_SECRET:
	case ISCSI_BOOT_TGT_NIC_ASSOC:
	case ISCSI_BOOT_TGT_FLAGS:
		rc = S_IRUGO;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static void qla4xxx_boot_release(void *data)
{
	struct scsi_qla_host *ha = data;

	scsi_host_put(ha->host);
}

static int get_fw_boot_info(struct scsi_qla_host *ha, uint16_t ddb_index[])
{
	dma_addr_t buf_dma;
	uint32_t addr, pri_addr, sec_addr;
	uint32_t offset;
	uint16_t func_num;
	uint8_t val;
	uint8_t *buf = NULL;
	size_t size = 13 * sizeof(uint8_t);
	int ret = QLA_SUCCESS;

	func_num = PCI_FUNC(ha->pdev->devfn);

	ql4_printk(KERN_INFO, ha, "%s: Get FW boot info for 0x%x func %d\n",
		   __func__, ha->pdev->device, func_num);

	if (is_qla40XX(ha)) {
		if (func_num == 1) {
			addr = NVRAM_PORT0_BOOT_MODE;
			pri_addr = NVRAM_PORT0_BOOT_PRI_TGT;
			sec_addr = NVRAM_PORT0_BOOT_SEC_TGT;
		} else if (func_num == 3) {
			addr = NVRAM_PORT1_BOOT_MODE;
			pri_addr = NVRAM_PORT1_BOOT_PRI_TGT;
			sec_addr = NVRAM_PORT1_BOOT_SEC_TGT;
		} else {
			ret = QLA_ERROR;
			goto exit_boot_info;
		}

		
		val = rd_nvram_byte(ha, addr);
		if (!(val & 0x07)) {
			DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Adapter boot "
					  "options : 0x%x\n", __func__, val));
			ret = QLA_ERROR;
			goto exit_boot_info;
		}

		
		val = rd_nvram_byte(ha, pri_addr);
		if (val & BIT_7)
			ddb_index[0] = (val & 0x7f);

		
		val = rd_nvram_byte(ha, sec_addr);
		if (val & BIT_7)
			ddb_index[1] = (val & 0x7f);

	} else if (is_qla8022(ha)) {
		buf = dma_alloc_coherent(&ha->pdev->dev, size,
					 &buf_dma, GFP_KERNEL);
		if (!buf) {
			DEBUG2(ql4_printk(KERN_ERR, ha,
					  "%s: Unable to allocate dma buffer\n",
					   __func__));
			ret = QLA_ERROR;
			goto exit_boot_info;
		}

		if (ha->port_num == 0)
			offset = BOOT_PARAM_OFFSET_PORT0;
		else if (ha->port_num == 1)
			offset = BOOT_PARAM_OFFSET_PORT1;
		else {
			ret = QLA_ERROR;
			goto exit_boot_info_free;
		}
		addr = FLASH_RAW_ACCESS_ADDR + (ha->hw.flt_iscsi_param * 4) +
		       offset;
		if (qla4xxx_get_flash(ha, buf_dma, addr,
				      13 * sizeof(uint8_t)) != QLA_SUCCESS) {
			DEBUG2(ql4_printk(KERN_ERR, ha, "scsi%ld: %s: Get Flash"
					  " failed\n", ha->host_no, __func__));
			ret = QLA_ERROR;
			goto exit_boot_info_free;
		}
		
		if (!(buf[1] & 0x07)) {
			DEBUG2(ql4_printk(KERN_INFO, ha, "Firmware boot options"
					  " : 0x%x\n", buf[1]));
			ret = QLA_ERROR;
			goto exit_boot_info_free;
		}

		
		if (buf[2] & BIT_7)
			ddb_index[0] = buf[2] & 0x7f;

		
		if (buf[11] & BIT_7)
			ddb_index[1] = buf[11] & 0x7f;
	} else {
		ret = QLA_ERROR;
		goto exit_boot_info;
	}

	DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Primary target ID %d, Secondary"
			  " target ID %d\n", __func__, ddb_index[0],
			  ddb_index[1]));

exit_boot_info_free:
	dma_free_coherent(&ha->pdev->dev, size, buf, buf_dma);
exit_boot_info:
	ha->pri_ddb_idx = ddb_index[0];
	ha->sec_ddb_idx = ddb_index[1];
	return ret;
}

static int qla4xxx_get_bidi_chap(struct scsi_qla_host *ha, char *username,
			    char *password)
{
	int i, ret = -EINVAL;
	int max_chap_entries = 0;
	struct ql4_chap_table *chap_table;

	if (is_qla8022(ha))
		max_chap_entries = (ha->hw.flt_chap_size / 2) /
						sizeof(struct ql4_chap_table);
	else
		max_chap_entries = MAX_CHAP_ENTRIES_40XX;

	if (!ha->chap_list) {
		ql4_printk(KERN_ERR, ha, "Do not have CHAP table cache\n");
		return ret;
	}

	mutex_lock(&ha->chap_sem);
	for (i = 0; i < max_chap_entries; i++) {
		chap_table = (struct ql4_chap_table *)ha->chap_list + i;
		if (chap_table->cookie !=
		    __constant_cpu_to_le16(CHAP_VALID_COOKIE)) {
			continue;
		}

		if (chap_table->flags & BIT_7) 
			continue;

		if (!(chap_table->flags & BIT_6)) 
			continue;

		strncpy(password, chap_table->secret, QL4_CHAP_MAX_SECRET_LEN);
		strncpy(username, chap_table->name, QL4_CHAP_MAX_NAME_LEN);
		ret = 0;
		break;
	}
	mutex_unlock(&ha->chap_sem);

	return ret;
}


static int qla4xxx_get_boot_target(struct scsi_qla_host *ha,
				   struct ql4_boot_session_info *boot_sess,
				   uint16_t ddb_index)
{
	struct ql4_conn_info *boot_conn = &boot_sess->conn_list[0];
	struct dev_db_entry *fw_ddb_entry;
	dma_addr_t fw_ddb_entry_dma;
	uint16_t idx;
	uint16_t options;
	int ret = QLA_SUCCESS;

	fw_ddb_entry = dma_alloc_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
					  &fw_ddb_entry_dma, GFP_KERNEL);
	if (!fw_ddb_entry) {
		DEBUG2(ql4_printk(KERN_ERR, ha,
				  "%s: Unable to allocate dma buffer.\n",
				  __func__));
		ret = QLA_ERROR;
		return ret;
	}

	if (qla4xxx_bootdb_by_index(ha, fw_ddb_entry,
				   fw_ddb_entry_dma, ddb_index)) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: No Flash DDB found at "
				  "index [%d]\n", __func__, ddb_index));
		ret = QLA_ERROR;
		goto exit_boot_target;
	}

	
	memcpy(boot_sess->target_name, fw_ddb_entry->iscsi_name,
	       min(sizeof(boot_sess->target_name),
		   sizeof(fw_ddb_entry->iscsi_name)));

	options = le16_to_cpu(fw_ddb_entry->options);
	if (options & DDB_OPT_IPV6_DEVICE) {
		memcpy(&boot_conn->dest_ipaddr.ip_address,
		       &fw_ddb_entry->ip_addr[0], IPv6_ADDR_LEN);
	} else {
		boot_conn->dest_ipaddr.ip_type = 0x1;
		memcpy(&boot_conn->dest_ipaddr.ip_address,
		       &fw_ddb_entry->ip_addr[0], IP_ADDR_LEN);
	}

	boot_conn->dest_port = le16_to_cpu(fw_ddb_entry->port);

	
	idx = __le16_to_cpu(fw_ddb_entry->chap_tbl_idx);

	if (BIT_7 & le16_to_cpu(fw_ddb_entry->iscsi_options))	{

		DEBUG2(ql4_printk(KERN_INFO, ha, "Setting chap\n"));

		ret = qla4xxx_get_chap(ha, (char *)&boot_conn->chap.
				       target_chap_name,
				       (char *)&boot_conn->chap.target_secret,
				       idx);
		if (ret) {
			ql4_printk(KERN_ERR, ha, "Failed to set chap\n");
			ret = QLA_ERROR;
			goto exit_boot_target;
		}

		boot_conn->chap.target_chap_name_length = QL4_CHAP_MAX_NAME_LEN;
		boot_conn->chap.target_secret_length = QL4_CHAP_MAX_SECRET_LEN;
	}

	if (BIT_4 & le16_to_cpu(fw_ddb_entry->iscsi_options)) {

		DEBUG2(ql4_printk(KERN_INFO, ha, "Setting BIDI chap\n"));

		ret = qla4xxx_get_bidi_chap(ha,
				    (char *)&boot_conn->chap.intr_chap_name,
				    (char *)&boot_conn->chap.intr_secret);

		if (ret) {
			ql4_printk(KERN_ERR, ha, "Failed to set BIDI chap\n");
			ret = QLA_ERROR;
			goto exit_boot_target;
		}

		boot_conn->chap.intr_chap_name_length = QL4_CHAP_MAX_NAME_LEN;
		boot_conn->chap.intr_secret_length = QL4_CHAP_MAX_SECRET_LEN;
	}

exit_boot_target:
	dma_free_coherent(&ha->pdev->dev, sizeof(*fw_ddb_entry),
			  fw_ddb_entry, fw_ddb_entry_dma);
	return ret;
}

static int qla4xxx_get_boot_info(struct scsi_qla_host *ha)
{
	uint16_t ddb_index[2];
	int ret = QLA_ERROR;
	int rval;

	memset(ddb_index, 0, sizeof(ddb_index));
	ddb_index[0] = 0xffff;
	ddb_index[1] = 0xffff;
	ret = get_fw_boot_info(ha, ddb_index);
	if (ret != QLA_SUCCESS) {
		DEBUG2(ql4_printk(KERN_INFO, ha,
				"%s: No boot target configured.\n", __func__));
		return ret;
	}

	if (ql4xdisablesysfsboot)
		return QLA_SUCCESS;

	if (ddb_index[0] == 0xffff)
		goto sec_target;

	rval = qla4xxx_get_boot_target(ha, &(ha->boot_tgt.boot_pri_sess),
				      ddb_index[0]);
	if (rval != QLA_SUCCESS) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Primary boot target not "
				  "configured\n", __func__));
	} else
		ret = QLA_SUCCESS;

sec_target:
	if (ddb_index[1] == 0xffff)
		goto exit_get_boot_info;

	rval = qla4xxx_get_boot_target(ha, &(ha->boot_tgt.boot_sec_sess),
				      ddb_index[1]);
	if (rval != QLA_SUCCESS) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Secondary boot target not"
				  " configured\n", __func__));
	} else
		ret = QLA_SUCCESS;

exit_get_boot_info:
	return ret;
}

static int qla4xxx_setup_boot_info(struct scsi_qla_host *ha)
{
	struct iscsi_boot_kobj *boot_kobj;

	if (qla4xxx_get_boot_info(ha) != QLA_SUCCESS)
		return QLA_ERROR;

	if (ql4xdisablesysfsboot) {
		ql4_printk(KERN_INFO, ha,
			   "%s: syfsboot disabled - driver will trigger login "
			   "and publish session for discovery .\n", __func__);
		return QLA_SUCCESS;
	}


	ha->boot_kset = iscsi_boot_create_host_kset(ha->host->host_no);
	if (!ha->boot_kset)
		goto kset_free;

	if (!scsi_host_get(ha->host))
		goto kset_free;
	boot_kobj = iscsi_boot_create_target(ha->boot_kset, 0, ha,
					     qla4xxx_show_boot_tgt_pri_info,
					     qla4xxx_tgt_get_attr_visibility,
					     qla4xxx_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(ha->host))
		goto kset_free;
	boot_kobj = iscsi_boot_create_target(ha->boot_kset, 1, ha,
					     qla4xxx_show_boot_tgt_sec_info,
					     qla4xxx_tgt_get_attr_visibility,
					     qla4xxx_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(ha->host))
		goto kset_free;
	boot_kobj = iscsi_boot_create_initiator(ha->boot_kset, 0, ha,
					       qla4xxx_show_boot_ini_info,
					       qla4xxx_ini_get_attr_visibility,
					       qla4xxx_boot_release);
	if (!boot_kobj)
		goto put_host;

	if (!scsi_host_get(ha->host))
		goto kset_free;
	boot_kobj = iscsi_boot_create_ethernet(ha->boot_kset, 0, ha,
					       qla4xxx_show_boot_eth_info,
					       qla4xxx_eth_get_attr_visibility,
					       qla4xxx_boot_release);
	if (!boot_kobj)
		goto put_host;

	return QLA_SUCCESS;

put_host:
	scsi_host_put(ha->host);
kset_free:
	iscsi_boot_destroy_kset(ha->boot_kset);
	return -ENOMEM;
}


static void qla4xxx_create_chap_list(struct scsi_qla_host *ha)
{
	int rval = 0;
	uint8_t *chap_flash_data = NULL;
	uint32_t offset;
	dma_addr_t chap_dma;
	uint32_t chap_size = 0;

	if (is_qla40XX(ha))
		chap_size = MAX_CHAP_ENTRIES_40XX  *
					sizeof(struct ql4_chap_table);
	else	
		chap_size = ha->hw.flt_chap_size / 2;

	chap_flash_data = dma_alloc_coherent(&ha->pdev->dev, chap_size,
					  &chap_dma, GFP_KERNEL);
	if (!chap_flash_data) {
		ql4_printk(KERN_ERR, ha, "No memory for chap_flash_data\n");
		return;
	}
	if (is_qla40XX(ha))
		offset = FLASH_CHAP_OFFSET;
	else {
		offset = FLASH_RAW_ACCESS_ADDR + (ha->hw.flt_region_chap << 2);
		if (ha->port_num == 1)
			offset += chap_size;
	}

	rval = qla4xxx_get_flash(ha, chap_dma, offset, chap_size);
	if (rval != QLA_SUCCESS)
		goto exit_chap_list;

	if (ha->chap_list == NULL)
		ha->chap_list = vmalloc(chap_size);
	if (ha->chap_list == NULL) {
		ql4_printk(KERN_ERR, ha, "No memory for ha->chap_list\n");
		goto exit_chap_list;
	}

	memcpy(ha->chap_list, chap_flash_data, chap_size);

exit_chap_list:
	dma_free_coherent(&ha->pdev->dev, chap_size,
			chap_flash_data, chap_dma);
}

static void qla4xxx_get_param_ddb(struct ddb_entry *ddb_entry,
				  struct ql4_tuple_ddb *tddb)
{
	struct scsi_qla_host *ha;
	struct iscsi_cls_session *cls_sess;
	struct iscsi_cls_conn *cls_conn;
	struct iscsi_session *sess;
	struct iscsi_conn *conn;

	DEBUG2(printk(KERN_INFO "Func: %s\n", __func__));
	ha = ddb_entry->ha;
	cls_sess = ddb_entry->sess;
	sess = cls_sess->dd_data;
	cls_conn = ddb_entry->conn;
	conn = cls_conn->dd_data;

	tddb->tpgt = sess->tpgt;
	tddb->port = conn->persistent_port;
	strncpy(tddb->iscsi_name, sess->targetname, ISCSI_NAME_SIZE);
	strncpy(tddb->ip_addr, conn->persistent_address, DDB_IPADDR_LEN);
}

static void qla4xxx_convert_param_ddb(struct dev_db_entry *fw_ddb_entry,
				      struct ql4_tuple_ddb *tddb)
{
	uint16_t options = 0;

	tddb->tpgt = le32_to_cpu(fw_ddb_entry->tgt_portal_grp);
	memcpy(&tddb->iscsi_name[0], &fw_ddb_entry->iscsi_name[0],
	       min(sizeof(tddb->iscsi_name), sizeof(fw_ddb_entry->iscsi_name)));

	options = le16_to_cpu(fw_ddb_entry->options);
	if (options & DDB_OPT_IPV6_DEVICE)
		sprintf(tddb->ip_addr, "%pI6", fw_ddb_entry->ip_addr);
	else
		sprintf(tddb->ip_addr, "%pI4", fw_ddb_entry->ip_addr);

	tddb->port = le16_to_cpu(fw_ddb_entry->port);
	memcpy(&tddb->isid[0], &fw_ddb_entry->isid[0], sizeof(tddb->isid));
}

static int qla4xxx_compare_tuple_ddb(struct scsi_qla_host *ha,
				     struct ql4_tuple_ddb *old_tddb,
				     struct ql4_tuple_ddb *new_tddb,
				     uint8_t is_isid_compare)
{
	if (strcmp(old_tddb->iscsi_name, new_tddb->iscsi_name))
		return QLA_ERROR;

	if (strcmp(old_tddb->ip_addr, new_tddb->ip_addr))
		return QLA_ERROR;

	if (old_tddb->port != new_tddb->port)
		return QLA_ERROR;

	if (is_isid_compare) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: old ISID [%02x%02x%02x"
			"%02x%02x%02x] New ISID [%02x%02x%02x%02x%02x%02x]\n",
			__func__, old_tddb->isid[5], old_tddb->isid[4],
			old_tddb->isid[3], old_tddb->isid[2], old_tddb->isid[1],
			old_tddb->isid[0], new_tddb->isid[5], new_tddb->isid[4],
			new_tddb->isid[3], new_tddb->isid[2], new_tddb->isid[1],
			new_tddb->isid[0]));

		if (memcmp(&old_tddb->isid[0], &new_tddb->isid[0],
			   sizeof(old_tddb->isid)))
			return QLA_ERROR;
	}

	DEBUG2(ql4_printk(KERN_INFO, ha,
			  "Match Found, fw[%d,%d,%s,%s], [%d,%d,%s,%s]",
			  old_tddb->port, old_tddb->tpgt, old_tddb->ip_addr,
			  old_tddb->iscsi_name, new_tddb->port, new_tddb->tpgt,
			  new_tddb->ip_addr, new_tddb->iscsi_name));

	return QLA_SUCCESS;
}

static int qla4xxx_is_session_exists(struct scsi_qla_host *ha,
				     struct dev_db_entry *fw_ddb_entry)
{
	struct ddb_entry *ddb_entry;
	struct ql4_tuple_ddb *fw_tddb = NULL;
	struct ql4_tuple_ddb *tmp_tddb = NULL;
	int idx;
	int ret = QLA_ERROR;

	fw_tddb = vzalloc(sizeof(*fw_tddb));
	if (!fw_tddb) {
		DEBUG2(ql4_printk(KERN_WARNING, ha,
				  "Memory Allocation failed.\n"));
		ret = QLA_SUCCESS;
		goto exit_check;
	}

	tmp_tddb = vzalloc(sizeof(*tmp_tddb));
	if (!tmp_tddb) {
		DEBUG2(ql4_printk(KERN_WARNING, ha,
				  "Memory Allocation failed.\n"));
		ret = QLA_SUCCESS;
		goto exit_check;
	}

	qla4xxx_convert_param_ddb(fw_ddb_entry, fw_tddb);

	for (idx = 0; idx < MAX_DDB_ENTRIES; idx++) {
		ddb_entry = qla4xxx_lookup_ddb_by_fw_index(ha, idx);
		if (ddb_entry == NULL)
			continue;

		qla4xxx_get_param_ddb(ddb_entry, tmp_tddb);
		if (!qla4xxx_compare_tuple_ddb(ha, fw_tddb, tmp_tddb, false)) {
			ret = QLA_SUCCESS; 
			goto exit_check;
		}
	}

exit_check:
	if (fw_tddb)
		vfree(fw_tddb);
	if (tmp_tddb)
		vfree(tmp_tddb);
	return ret;
}

static int qla4xxx_is_flash_ddb_exists(struct scsi_qla_host *ha,
				       struct list_head *list_nt,
				       struct dev_db_entry *fw_ddb_entry)
{
	struct qla_ddb_index  *nt_ddb_idx, *nt_ddb_idx_tmp;
	struct ql4_tuple_ddb *fw_tddb = NULL;
	struct ql4_tuple_ddb *tmp_tddb = NULL;
	int ret = QLA_ERROR;

	fw_tddb = vzalloc(sizeof(*fw_tddb));
	if (!fw_tddb) {
		DEBUG2(ql4_printk(KERN_WARNING, ha,
				  "Memory Allocation failed.\n"));
		ret = QLA_SUCCESS;
		goto exit_check;
	}

	tmp_tddb = vzalloc(sizeof(*tmp_tddb));
	if (!tmp_tddb) {
		DEBUG2(ql4_printk(KERN_WARNING, ha,
				  "Memory Allocation failed.\n"));
		ret = QLA_SUCCESS;
		goto exit_check;
	}

	qla4xxx_convert_param_ddb(fw_ddb_entry, fw_tddb);

	list_for_each_entry_safe(nt_ddb_idx, nt_ddb_idx_tmp, list_nt, list) {
		qla4xxx_convert_param_ddb(&nt_ddb_idx->fw_ddb, tmp_tddb);
		if (!qla4xxx_compare_tuple_ddb(ha, fw_tddb, tmp_tddb, true)) {
			ret = QLA_SUCCESS; 
			goto exit_check;
		}
	}

exit_check:
	if (fw_tddb)
		vfree(fw_tddb);
	if (tmp_tddb)
		vfree(tmp_tddb);
	return ret;
}

static void qla4xxx_free_ddb_list(struct list_head *list_ddb)
{
	struct qla_ddb_index  *ddb_idx, *ddb_idx_tmp;

	list_for_each_entry_safe(ddb_idx, ddb_idx_tmp, list_ddb, list) {
		list_del_init(&ddb_idx->list);
		vfree(ddb_idx);
	}
}

static struct iscsi_endpoint *qla4xxx_get_ep_fwdb(struct scsi_qla_host *ha,
					struct dev_db_entry *fw_ddb_entry)
{
	struct iscsi_endpoint *ep;
	struct sockaddr_in *addr;
	struct sockaddr_in6 *addr6;
	struct sockaddr *dst_addr;
	char *ip;

	
	dst_addr = vmalloc(sizeof(*dst_addr));
	if (!dst_addr)
		return NULL;

	if (fw_ddb_entry->options & DDB_OPT_IPV6_DEVICE) {
		dst_addr->sa_family = AF_INET6;
		addr6 = (struct sockaddr_in6 *)dst_addr;
		ip = (char *)&addr6->sin6_addr;
		memcpy(ip, fw_ddb_entry->ip_addr, IPv6_ADDR_LEN);
		addr6->sin6_port = htons(le16_to_cpu(fw_ddb_entry->port));

	} else {
		dst_addr->sa_family = AF_INET;
		addr = (struct sockaddr_in *)dst_addr;
		ip = (char *)&addr->sin_addr;
		memcpy(ip, fw_ddb_entry->ip_addr, IP_ADDR_LEN);
		addr->sin_port = htons(le16_to_cpu(fw_ddb_entry->port));
	}

	ep = qla4xxx_ep_connect(ha->host, dst_addr, 0);
	vfree(dst_addr);
	return ep;
}

static int qla4xxx_verify_boot_idx(struct scsi_qla_host *ha, uint16_t idx)
{
	if (ql4xdisablesysfsboot)
		return QLA_SUCCESS;
	if (idx == ha->pri_ddb_idx || idx == ha->sec_ddb_idx)
		return QLA_ERROR;
	return QLA_SUCCESS;
}

static void qla4xxx_setup_flash_ddb_entry(struct scsi_qla_host *ha,
					  struct ddb_entry *ddb_entry)
{
	uint16_t def_timeout;

	ddb_entry->ddb_type = FLASH_DDB;
	ddb_entry->fw_ddb_index = INVALID_ENTRY;
	ddb_entry->fw_ddb_device_state = DDB_DS_NO_CONNECTION_ACTIVE;
	ddb_entry->ha = ha;
	ddb_entry->unblock_sess = qla4xxx_unblock_flash_ddb;
	ddb_entry->ddb_change = qla4xxx_flash_ddb_change;

	atomic_set(&ddb_entry->retry_relogin_timer, INVALID_ENTRY);
	atomic_set(&ddb_entry->relogin_timer, 0);
	atomic_set(&ddb_entry->relogin_retry_count, 0);
	def_timeout = le16_to_cpu(ddb_entry->fw_ddb_entry.def_timeout);
	ddb_entry->default_relogin_timeout =
		(def_timeout > LOGIN_TOV) && (def_timeout < LOGIN_TOV * 10) ?
		def_timeout : LOGIN_TOV;
	ddb_entry->default_time2wait =
		le16_to_cpu(ddb_entry->fw_ddb_entry.iscsi_def_time2wait);
}

static void qla4xxx_wait_for_ip_configuration(struct scsi_qla_host *ha)
{
	uint32_t idx = 0;
	uint32_t ip_idx[IP_ADDR_COUNT] = {0, 1, 2, 3}; 
	uint32_t sts[MBOX_REG_COUNT];
	uint32_t ip_state;
	unsigned long wtime;
	int ret;

	wtime = jiffies + (HZ * IP_CONFIG_TOV);
	do {
		for (idx = 0; idx < IP_ADDR_COUNT; idx++) {
			if (ip_idx[idx] == -1)
				continue;

			ret = qla4xxx_get_ip_state(ha, 0, ip_idx[idx], sts);

			if (ret == QLA_ERROR) {
				ip_idx[idx] = -1;
				continue;
			}

			ip_state = (sts[1] & IP_STATE_MASK) >> IP_STATE_SHIFT;

			DEBUG2(ql4_printk(KERN_INFO, ha,
					  "Waiting for IP state for idx = %d, state = 0x%x\n",
					  ip_idx[idx], ip_state));
			if (ip_state == IP_ADDRSTATE_UNCONFIGURED ||
			    ip_state == IP_ADDRSTATE_INVALID ||
			    ip_state == IP_ADDRSTATE_PREFERRED ||
			    ip_state == IP_ADDRSTATE_DEPRICATED ||
			    ip_state == IP_ADDRSTATE_DISABLING)
				ip_idx[idx] = -1;
		}

		
		if ((ip_idx[0] == -1) &&
		    (ip_idx[1] == -1) &&
		    (ip_idx[2] == -1) &&
		    (ip_idx[3] == -1))
			break;
		schedule_timeout_uninterruptible(HZ);
	} while (time_after(wtime, jiffies));
}

static void qla4xxx_build_st_list(struct scsi_qla_host *ha,
				  struct list_head *list_st)
{
	struct qla_ddb_index  *st_ddb_idx;
	int max_ddbs;
	int fw_idx_size;
	struct dev_db_entry *fw_ddb_entry;
	dma_addr_t fw_ddb_dma;
	int ret;
	uint32_t idx = 0, next_idx = 0;
	uint32_t state = 0, conn_err = 0;
	uint16_t conn_id = 0;

	fw_ddb_entry = dma_pool_alloc(ha->fw_ddb_dma_pool, GFP_KERNEL,
				      &fw_ddb_dma);
	if (fw_ddb_entry == NULL) {
		DEBUG2(ql4_printk(KERN_ERR, ha, "Out of memory\n"));
		goto exit_st_list;
	}

	max_ddbs =  is_qla40XX(ha) ? MAX_DEV_DB_ENTRIES_40XX :
				     MAX_DEV_DB_ENTRIES;
	fw_idx_size = sizeof(struct qla_ddb_index);

	for (idx = 0; idx < max_ddbs; idx = next_idx) {
		ret = qla4xxx_get_fwddb_entry(ha, idx, fw_ddb_entry, fw_ddb_dma,
					      NULL, &next_idx, &state,
					      &conn_err, NULL, &conn_id);
		if (ret == QLA_ERROR)
			break;

		
		if (state == DDB_DS_UNASSIGNED)
			goto continue_next_st;

		
		if (strlen((char *) fw_ddb_entry->iscsi_name) != 0)
			goto continue_next_st;

		st_ddb_idx = vzalloc(fw_idx_size);
		if (!st_ddb_idx)
			break;

		st_ddb_idx->fw_ddb_idx = idx;

		list_add_tail(&st_ddb_idx->list, list_st);
continue_next_st:
		if (next_idx == 0)
			break;
	}

exit_st_list:
	if (fw_ddb_entry)
		dma_pool_free(ha->fw_ddb_dma_pool, fw_ddb_entry, fw_ddb_dma);
}

static void qla4xxx_remove_failed_ddb(struct scsi_qla_host *ha,
				      struct list_head *list_ddb)
{
	struct qla_ddb_index  *ddb_idx, *ddb_idx_tmp;
	uint32_t next_idx = 0;
	uint32_t state = 0, conn_err = 0;
	int ret;

	list_for_each_entry_safe(ddb_idx, ddb_idx_tmp, list_ddb, list) {
		ret = qla4xxx_get_fwddb_entry(ha, ddb_idx->fw_ddb_idx,
					      NULL, 0, NULL, &next_idx, &state,
					      &conn_err, NULL, NULL);
		if (ret == QLA_ERROR)
			continue;

		if (state == DDB_DS_NO_CONNECTION_ACTIVE ||
		    state == DDB_DS_SESSION_FAILED) {
			list_del_init(&ddb_idx->list);
			vfree(ddb_idx);
		}
	}
}

static int qla4xxx_sess_conn_setup(struct scsi_qla_host *ha,
				   struct dev_db_entry *fw_ddb_entry,
				   int is_reset)
{
	struct iscsi_cls_session *cls_sess;
	struct iscsi_session *sess;
	struct iscsi_cls_conn *cls_conn;
	struct iscsi_endpoint *ep;
	uint16_t cmds_max = 32;
	uint16_t conn_id = 0;
	uint32_t initial_cmdsn = 0;
	int ret = QLA_SUCCESS;

	struct ddb_entry *ddb_entry = NULL;

	cls_sess = iscsi_session_setup(&qla4xxx_iscsi_transport, ha->host,
				       cmds_max, sizeof(struct ddb_entry),
				       sizeof(struct ql4_task_data),
				       initial_cmdsn, INVALID_ENTRY);
	if (!cls_sess) {
		ret = QLA_ERROR;
		goto exit_setup;
	}

	module_put(qla4xxx_iscsi_transport.owner);
	sess = cls_sess->dd_data;
	ddb_entry = sess->dd_data;
	ddb_entry->sess = cls_sess;

	cls_sess->recovery_tmo = ql4xsess_recovery_tmo;
	memcpy(&ddb_entry->fw_ddb_entry, fw_ddb_entry,
	       sizeof(struct dev_db_entry));

	qla4xxx_setup_flash_ddb_entry(ha, ddb_entry);

	cls_conn = iscsi_conn_setup(cls_sess, sizeof(struct qla_conn), conn_id);

	if (!cls_conn) {
		ret = QLA_ERROR;
		goto exit_setup;
	}

	ddb_entry->conn = cls_conn;

	
	ep = qla4xxx_get_ep_fwdb(ha, fw_ddb_entry);
	if (ep) {
		ep->conn = cls_conn;
		cls_conn->ep = ep;
	} else {
		DEBUG2(ql4_printk(KERN_ERR, ha, "Unable to get ep\n"));
		ret = QLA_ERROR;
		goto exit_setup;
	}

	
	qla4xxx_copy_fwddb_param(ha, fw_ddb_entry, cls_sess, cls_conn);

	if (is_reset == RESET_ADAPTER) {
		iscsi_block_session(cls_sess);
		set_bit(DPC_RELOGIN_DEVICE, &ha->dpc_flags);
		set_bit(DF_RELOGIN, &ddb_entry->flags);
	}

exit_setup:
	return ret;
}

static void qla4xxx_build_nt_list(struct scsi_qla_host *ha,
				  struct list_head *list_nt, int is_reset)
{
	struct dev_db_entry *fw_ddb_entry;
	dma_addr_t fw_ddb_dma;
	int max_ddbs;
	int fw_idx_size;
	int ret;
	uint32_t idx = 0, next_idx = 0;
	uint32_t state = 0, conn_err = 0;
	uint16_t conn_id = 0;
	struct qla_ddb_index  *nt_ddb_idx;

	fw_ddb_entry = dma_pool_alloc(ha->fw_ddb_dma_pool, GFP_KERNEL,
				      &fw_ddb_dma);
	if (fw_ddb_entry == NULL) {
		DEBUG2(ql4_printk(KERN_ERR, ha, "Out of memory\n"));
		goto exit_nt_list;
	}
	max_ddbs =  is_qla40XX(ha) ? MAX_DEV_DB_ENTRIES_40XX :
				     MAX_DEV_DB_ENTRIES;
	fw_idx_size = sizeof(struct qla_ddb_index);

	for (idx = 0; idx < max_ddbs; idx = next_idx) {
		ret = qla4xxx_get_fwddb_entry(ha, idx, fw_ddb_entry, fw_ddb_dma,
					      NULL, &next_idx, &state,
					      &conn_err, NULL, &conn_id);
		if (ret == QLA_ERROR)
			break;

		if (qla4xxx_verify_boot_idx(ha, idx) != QLA_SUCCESS)
			goto continue_next_nt;

		
		if (strlen((char *) fw_ddb_entry->iscsi_name) == 0)
			goto continue_next_nt;

		if (!(state == DDB_DS_NO_CONNECTION_ACTIVE ||
		    state == DDB_DS_SESSION_FAILED))
			goto continue_next_nt;

		DEBUG2(ql4_printk(KERN_INFO, ha,
				  "Adding  DDB to session = 0x%x\n", idx));
		if (is_reset == INIT_ADAPTER) {
			nt_ddb_idx = vmalloc(fw_idx_size);
			if (!nt_ddb_idx)
				break;

			nt_ddb_idx->fw_ddb_idx = idx;

			memcpy(&nt_ddb_idx->fw_ddb, fw_ddb_entry,
			       sizeof(struct dev_db_entry));

			if (qla4xxx_is_flash_ddb_exists(ha, list_nt,
					fw_ddb_entry) == QLA_SUCCESS) {
				vfree(nt_ddb_idx);
				goto continue_next_nt;
			}
			list_add_tail(&nt_ddb_idx->list, list_nt);
		} else if (is_reset == RESET_ADAPTER) {
			if (qla4xxx_is_session_exists(ha, fw_ddb_entry) ==
								QLA_SUCCESS)
				goto continue_next_nt;
		}

		ret = qla4xxx_sess_conn_setup(ha, fw_ddb_entry, is_reset);
		if (ret == QLA_ERROR)
			goto exit_nt_list;

continue_next_nt:
		if (next_idx == 0)
			break;
	}

exit_nt_list:
	if (fw_ddb_entry)
		dma_pool_free(ha->fw_ddb_dma_pool, fw_ddb_entry, fw_ddb_dma);
}

void qla4xxx_build_ddb_list(struct scsi_qla_host *ha, int is_reset)
{
	uint16_t tmo = 0;
	struct list_head list_st, list_nt;
	struct qla_ddb_index  *st_ddb_idx, *st_ddb_idx_tmp;
	unsigned long wtime;

	if (!test_bit(AF_LINK_UP, &ha->flags)) {
		set_bit(AF_BUILD_DDB_LIST, &ha->flags);
		ha->is_reset = is_reset;
		return;
	}

	INIT_LIST_HEAD(&list_st);
	INIT_LIST_HEAD(&list_nt);

	qla4xxx_build_st_list(ha, &list_st);

	qla4xxx_wait_for_ip_configuration(ha);

	
	list_for_each_entry_safe(st_ddb_idx, st_ddb_idx_tmp, &list_st, list) {
		qla4xxx_conn_open(ha, st_ddb_idx->fw_ddb_idx);
	}

	
	tmo = ((ha->def_timeout > LOGIN_TOV) &&
	       (ha->def_timeout < LOGIN_TOV * 10) ?
	       ha->def_timeout : LOGIN_TOV);

	DEBUG2(ql4_printk(KERN_INFO, ha,
			  "Default time to wait for build ddb %d\n", tmo));

	wtime = jiffies + (HZ * tmo);
	do {
		if (list_empty(&list_st))
			break;

		qla4xxx_remove_failed_ddb(ha, &list_st);
		schedule_timeout_uninterruptible(HZ / 10);
	} while (time_after(wtime, jiffies));

	
	qla4xxx_free_ddb_list(&list_st);

	qla4xxx_build_nt_list(ha, &list_nt, is_reset);

	qla4xxx_free_ddb_list(&list_nt);

	qla4xxx_free_ddb_index(ha);
}

static int __devinit qla4xxx_probe_adapter(struct pci_dev *pdev,
					   const struct pci_device_id *ent)
{
	int ret = -ENODEV, status;
	struct Scsi_Host *host;
	struct scsi_qla_host *ha;
	uint8_t init_retry_count = 0;
	char buf[34];
	struct qla4_8xxx_legacy_intr_set *nx_legacy_intr;
	uint32_t dev_state;

	if (pci_enable_device(pdev))
		return -1;

	host = iscsi_host_alloc(&qla4xxx_driver_template, sizeof(*ha), 0);
	if (host == NULL) {
		printk(KERN_WARNING
		       "qla4xxx: Couldn't allocate host from scsi layer!\n");
		goto probe_disable_device;
	}

	
	ha = to_qla_host(host);
	memset(ha, 0, sizeof(*ha));

	
	ha->pdev = pdev;
	ha->host = host;
	ha->host_no = host->host_no;

	pci_enable_pcie_error_reporting(pdev);

	
	if (is_qla8022(ha)) {
		ha->isp_ops = &qla4_8xxx_isp_ops;
		rwlock_init(&ha->hw_lock);
		ha->qdr_sn_window = -1;
		ha->ddr_mn_window = -1;
		ha->curr_window = 255;
		ha->func_num = PCI_FUNC(ha->pdev->devfn);
		nx_legacy_intr = &legacy_intr[ha->func_num];
		ha->nx_legacy_intr.int_vec_bit = nx_legacy_intr->int_vec_bit;
		ha->nx_legacy_intr.tgt_status_reg =
			nx_legacy_intr->tgt_status_reg;
		ha->nx_legacy_intr.tgt_mask_reg = nx_legacy_intr->tgt_mask_reg;
		ha->nx_legacy_intr.pci_int_reg = nx_legacy_intr->pci_int_reg;
	} else {
		ha->isp_ops = &qla4xxx_isp_ops;
	}

	
	if (is_qla8022(ha))
		pdev->needs_freset = 1;

	
	ret = ha->isp_ops->iospace_config(ha);
	if (ret)
		goto probe_failed_ioconfig;

	ql4_printk(KERN_INFO, ha, "Found an ISP%04x, irq %d, iobase 0x%p\n",
		   pdev->device, pdev->irq, ha->reg);

	qla4xxx_config_dma_addressing(ha);

	
	INIT_LIST_HEAD(&ha->free_srb_q);

	mutex_init(&ha->mbox_sem);
	mutex_init(&ha->chap_sem);
	init_completion(&ha->mbx_intr_comp);
	init_completion(&ha->disable_acb_comp);

	spin_lock_init(&ha->hardware_lock);

	
	INIT_LIST_HEAD(&ha->work_list);

	
	if (qla4xxx_mem_alloc(ha)) {
		ql4_printk(KERN_WARNING, ha,
		    "[ERROR] Failed to allocate memory for adapter\n");

		ret = -ENOMEM;
		goto probe_failed;
	}

	host->cmd_per_lun = 3;
	host->max_channel = 0;
	host->max_lun = MAX_LUNS - 1;
	host->max_id = MAX_TARGETS;
	host->max_cmd_len = IOCB_MAX_CDB_LEN;
	host->can_queue = MAX_SRBS ;
	host->transportt = qla4xxx_scsi_transport;

	ret = scsi_init_shared_tag_map(host, MAX_SRBS);
	if (ret) {
		ql4_printk(KERN_WARNING, ha,
			   "%s: scsi_init_shared_tag_map failed\n", __func__);
		goto probe_failed;
	}

	pci_set_drvdata(pdev, ha);

	ret = scsi_add_host(host, &pdev->dev);
	if (ret)
		goto probe_failed;

	if (is_qla8022(ha))
		(void) qla4_8xxx_get_flash_info(ha);

	status = qla4xxx_initialize_adapter(ha, INIT_ADAPTER);
	while ((!test_bit(AF_ONLINE, &ha->flags)) &&
	    init_retry_count++ < MAX_INIT_RETRIES) {

		if (is_qla8022(ha)) {
			qla4_8xxx_idc_lock(ha);
			dev_state = qla4_8xxx_rd_32(ha, QLA82XX_CRB_DEV_STATE);
			qla4_8xxx_idc_unlock(ha);
			if (dev_state == QLA82XX_DEV_FAILED) {
				ql4_printk(KERN_WARNING, ha, "%s: don't retry "
				    "initialize adapter. H/W is in failed state\n",
				    __func__);
				break;
			}
		}
		DEBUG2(printk("scsi: %s: retrying adapter initialization "
			      "(%d)\n", __func__, init_retry_count));

		if (ha->isp_ops->reset_chip(ha) == QLA_ERROR)
			continue;

		status = qla4xxx_initialize_adapter(ha, INIT_ADAPTER);
	}

	if (!test_bit(AF_ONLINE, &ha->flags)) {
		ql4_printk(KERN_WARNING, ha, "Failed to initialize adapter\n");

		if (is_qla8022(ha) && ql4xdontresethba) {
			
			DEBUG2(printk(KERN_ERR "HW STATE: FAILED\n"));
			qla4_8xxx_idc_lock(ha);
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_FAILED);
			qla4_8xxx_idc_unlock(ha);
		}
		ret = -ENODEV;
		goto remove_host;
	}

	
	DEBUG2(printk("scsi: %s: Starting kernel thread for "
		      "qla4xxx_dpc\n", __func__));
	sprintf(buf, "qla4xxx_%lu_dpc", ha->host_no);
	ha->dpc_thread = create_singlethread_workqueue(buf);
	if (!ha->dpc_thread) {
		ql4_printk(KERN_WARNING, ha, "Unable to start DPC thread!\n");
		ret = -ENODEV;
		goto remove_host;
	}
	INIT_WORK(&ha->dpc_work, qla4xxx_do_dpc);

	sprintf(buf, "qla4xxx_%lu_task", ha->host_no);
	ha->task_wq = alloc_workqueue(buf, WQ_MEM_RECLAIM, 1);
	if (!ha->task_wq) {
		ql4_printk(KERN_WARNING, ha, "Unable to start task thread!\n");
		ret = -ENODEV;
		goto remove_host;
	}

	if (!is_qla8022(ha)) {
		ret = qla4xxx_request_irqs(ha);
		if (ret) {
			ql4_printk(KERN_WARNING, ha, "Failed to reserve "
			    "interrupt %d already in use.\n", pdev->irq);
			goto remove_host;
		}
	}

	pci_save_state(ha->pdev);
	ha->isp_ops->enable_intrs(ha);

	
	qla4xxx_start_timer(ha, qla4xxx_timer, 1);

	set_bit(AF_INIT_DONE, &ha->flags);

	printk(KERN_INFO
	       " QLogic iSCSI HBA Driver version: %s\n"
	       "  QLogic ISP%04x @ %s, host#=%ld, fw=%02d.%02d.%02d.%02d\n",
	       qla4xxx_version_str, ha->pdev->device, pci_name(ha->pdev),
	       ha->host_no, ha->firmware_version[0], ha->firmware_version[1],
	       ha->patch_number, ha->build_number);

	if (qla4xxx_setup_boot_info(ha))
		ql4_printk(KERN_ERR, ha,
			   "%s: No iSCSI boot target configured\n", __func__);

		
	qla4xxx_build_ddb_list(ha, INIT_ADAPTER);
	iscsi_host_for_each_session(ha->host, qla4xxx_login_flash_ddb);

	qla4xxx_create_chap_list(ha);

	qla4xxx_create_ifaces(ha);
	return 0;

remove_host:
	scsi_remove_host(ha->host);

probe_failed:
	qla4xxx_free_adapter(ha);

probe_failed_ioconfig:
	pci_disable_pcie_error_reporting(pdev);
	scsi_host_put(ha->host);

probe_disable_device:
	pci_disable_device(pdev);

	return ret;
}

static void qla4xxx_prevent_other_port_reinit(struct scsi_qla_host *ha)
{
	struct scsi_qla_host *other_ha = NULL;
	struct pci_dev *other_pdev = NULL;
	int fn = ISP4XXX_PCI_FN_2;

	
	if (PCI_FUNC(ha->pdev->devfn) & BIT_1)
		fn = ISP4XXX_PCI_FN_1;

	other_pdev =
		pci_get_domain_bus_and_slot(pci_domain_nr(ha->pdev->bus),
		ha->pdev->bus->number, PCI_DEVFN(PCI_SLOT(ha->pdev->devfn),
		fn));

	
	if (other_pdev) {
		if (atomic_read(&other_pdev->enable_cnt)) {
			other_ha = pci_get_drvdata(other_pdev);
			if (other_ha) {
				set_bit(AF_HA_REMOVAL, &other_ha->flags);
				DEBUG2(ql4_printk(KERN_INFO, ha, "%s: "
				    "Prevent %s reinit\n", __func__,
				    dev_name(&other_ha->pdev->dev)));
			}
		}
		pci_dev_put(other_pdev);
	}
}

static void qla4xxx_destroy_fw_ddb_session(struct scsi_qla_host *ha)
{
	struct ddb_entry *ddb_entry;
	int options;
	int idx;

	for (idx = 0; idx < MAX_DDB_ENTRIES; idx++) {

		ddb_entry = qla4xxx_lookup_ddb_by_fw_index(ha, idx);
		if ((ddb_entry != NULL) &&
		    (ddb_entry->ddb_type == FLASH_DDB)) {

			options = LOGOUT_OPTION_CLOSE_SESSION;
			if (qla4xxx_session_logout_ddb(ha, ddb_entry, options)
			    == QLA_ERROR)
				ql4_printk(KERN_ERR, ha, "%s: Logout failed\n",
					   __func__);

			qla4xxx_clear_ddb_entry(ha, ddb_entry->fw_ddb_index);
			try_module_get(qla4xxx_iscsi_transport.owner);
			iscsi_destroy_endpoint(ddb_entry->conn->ep);
			qla4xxx_free_ddb(ha, ddb_entry);
			iscsi_session_teardown(ddb_entry->sess);
		}
	}
}
static void __devexit qla4xxx_remove_adapter(struct pci_dev *pdev)
{
	struct scsi_qla_host *ha;

	ha = pci_get_drvdata(pdev);

	if (!is_qla8022(ha))
		qla4xxx_prevent_other_port_reinit(ha);

	
	qla4xxx_destroy_ifaces(ha);

	if ((!ql4xdisablesysfsboot) && ha->boot_kset)
		iscsi_boot_destroy_kset(ha->boot_kset);

	qla4xxx_destroy_fw_ddb_session(ha);

	scsi_remove_host(ha->host);

	qla4xxx_free_adapter(ha);

	scsi_host_put(ha->host);

	pci_disable_pcie_error_reporting(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static void qla4xxx_config_dma_addressing(struct scsi_qla_host *ha)
{
	int retval;

	
	if (pci_set_dma_mask(ha->pdev, DMA_BIT_MASK(64)) == 0) {
		if (pci_set_consistent_dma_mask(ha->pdev, DMA_BIT_MASK(64))) {
			dev_dbg(&ha->pdev->dev,
				  "Failed to set 64 bit PCI consistent mask; "
				   "using 32 bit.\n");
			retval = pci_set_consistent_dma_mask(ha->pdev,
							     DMA_BIT_MASK(32));
		}
	} else
		retval = pci_set_dma_mask(ha->pdev, DMA_BIT_MASK(32));
}

static int qla4xxx_slave_alloc(struct scsi_device *sdev)
{
	struct iscsi_cls_session *cls_sess;
	struct iscsi_session *sess;
	struct ddb_entry *ddb;
	int queue_depth = QL4_DEF_QDEPTH;

	cls_sess = starget_to_session(sdev->sdev_target);
	sess = cls_sess->dd_data;
	ddb = sess->dd_data;

	sdev->hostdata = ddb;
	sdev->tagged_supported = 1;

	if (ql4xmaxqdepth != 0 && ql4xmaxqdepth <= 0xffffU)
		queue_depth = ql4xmaxqdepth;

	scsi_activate_tcq(sdev, queue_depth);
	return 0;
}

static int qla4xxx_slave_configure(struct scsi_device *sdev)
{
	sdev->tagged_supported = 1;
	return 0;
}

static void qla4xxx_slave_destroy(struct scsi_device *sdev)
{
	scsi_deactivate_tcq(sdev, 1);
}

struct srb *qla4xxx_del_from_active_array(struct scsi_qla_host *ha,
    uint32_t index)
{
	struct srb *srb = NULL;
	struct scsi_cmnd *cmd = NULL;

	cmd = scsi_host_find_tag(ha->host, index);
	if (!cmd)
		return srb;

	srb = (struct srb *)CMD_SP(cmd);
	if (!srb)
		return srb;

	
	if (srb->flags & SRB_DMA_VALID) {
		ha->req_q_count += srb->iocb_cnt;
		ha->iocb_cnt -= srb->iocb_cnt;
		if (srb->cmd)
			srb->cmd->host_scribble =
				(unsigned char *)(unsigned long) MAX_SRBS;
	}
	return srb;
}

static int qla4xxx_eh_wait_on_command(struct scsi_qla_host *ha,
				      struct scsi_cmnd *cmd)
{
	int done = 0;
	struct srb *rp;
	uint32_t max_wait_time = EH_WAIT_CMD_TOV;
	int ret = SUCCESS;

	if (unlikely(pci_channel_offline(ha->pdev)) ||
	    (test_bit(AF_EEH_BUSY, &ha->flags))) {
		ql4_printk(KERN_WARNING, ha, "scsi%ld: Return from %s\n",
		    ha->host_no, __func__);
		return ret;
	}

	do {
		
		rp = (struct srb *) CMD_SP(cmd);
		if (rp == NULL) {
			done++;
			break;
		}

		msleep(2000);
	} while (max_wait_time--);

	return done;
}

static int qla4xxx_wait_for_hba_online(struct scsi_qla_host *ha)
{
	unsigned long wait_online;

	wait_online = jiffies + (HBA_ONLINE_TOV * HZ);
	while (time_before(jiffies, wait_online)) {

		if (adapter_up(ha))
			return QLA_SUCCESS;

		msleep(2000);
	}

	return QLA_ERROR;
}

static int qla4xxx_eh_wait_for_commands(struct scsi_qla_host *ha,
					struct scsi_target *stgt,
					struct scsi_device *sdev)
{
	int cnt;
	int status = 0;
	struct scsi_cmnd *cmd;

	for (cnt = 0; cnt < ha->host->can_queue; cnt++) {
		cmd = scsi_host_find_tag(ha->host, cnt);
		if (cmd && stgt == scsi_target(cmd->device) &&
		    (!sdev || sdev == cmd->device)) {
			if (!qla4xxx_eh_wait_on_command(ha, cmd)) {
				status++;
				break;
			}
		}
	}
	return status;
}

static int qla4xxx_eh_abort(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
	unsigned int id = cmd->device->id;
	unsigned int lun = cmd->device->lun;
	unsigned long flags;
	struct srb *srb = NULL;
	int ret = SUCCESS;
	int wait = 0;

	ql4_printk(KERN_INFO, ha,
	    "scsi%ld:%d:%d: Abort command issued cmd=%p\n",
	    ha->host_no, id, lun, cmd);

	spin_lock_irqsave(&ha->hardware_lock, flags);
	srb = (struct srb *) CMD_SP(cmd);
	if (!srb) {
		spin_unlock_irqrestore(&ha->hardware_lock, flags);
		return SUCCESS;
	}
	kref_get(&srb->srb_ref);
	spin_unlock_irqrestore(&ha->hardware_lock, flags);

	if (qla4xxx_abort_task(ha, srb) != QLA_SUCCESS) {
		DEBUG3(printk("scsi%ld:%d:%d: Abort_task mbx failed.\n",
		    ha->host_no, id, lun));
		ret = FAILED;
	} else {
		DEBUG3(printk("scsi%ld:%d:%d: Abort_task mbx success.\n",
		    ha->host_no, id, lun));
		wait = 1;
	}

	kref_put(&srb->srb_ref, qla4xxx_srb_compl);

	
	if (wait) {
		if (!qla4xxx_eh_wait_on_command(ha, cmd)) {
			DEBUG2(printk("scsi%ld:%d:%d: Abort handler timed out\n",
			    ha->host_no, id, lun));
			ret = FAILED;
		}
	}

	ql4_printk(KERN_INFO, ha,
	    "scsi%ld:%d:%d: Abort command - %s\n",
	    ha->host_no, id, lun, (ret == SUCCESS) ? "succeeded" : "failed");

	return ret;
}

static int qla4xxx_eh_device_reset(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
	struct ddb_entry *ddb_entry = cmd->device->hostdata;
	int ret = FAILED, stat;

	if (!ddb_entry)
		return ret;

	ret = iscsi_block_scsi_eh(cmd);
	if (ret)
		return ret;
	ret = FAILED;

	ql4_printk(KERN_INFO, ha,
		   "scsi%ld:%d:%d:%d: DEVICE RESET ISSUED.\n", ha->host_no,
		   cmd->device->channel, cmd->device->id, cmd->device->lun);

	DEBUG2(printk(KERN_INFO
		      "scsi%ld: DEVICE_RESET cmd=%p jiffies = 0x%lx, to=%x,"
		      "dpc_flags=%lx, status=%x allowed=%d\n", ha->host_no,
		      cmd, jiffies, cmd->request->timeout / HZ,
		      ha->dpc_flags, cmd->result, cmd->allowed));

	
	stat = qla4xxx_reset_lun(ha, ddb_entry, cmd->device->lun);
	if (stat != QLA_SUCCESS) {
		ql4_printk(KERN_INFO, ha, "DEVICE RESET FAILED. %d\n", stat);
		goto eh_dev_reset_done;
	}

	if (qla4xxx_eh_wait_for_commands(ha, scsi_target(cmd->device),
					 cmd->device)) {
		ql4_printk(KERN_INFO, ha,
			   "DEVICE RESET FAILED - waiting for "
			   "commands.\n");
		goto eh_dev_reset_done;
	}

	
	if (qla4xxx_send_marker_iocb(ha, ddb_entry, cmd->device->lun,
		MM_LUN_RESET) != QLA_SUCCESS)
		goto eh_dev_reset_done;

	ql4_printk(KERN_INFO, ha,
		   "scsi(%ld:%d:%d:%d): DEVICE RESET SUCCEEDED.\n",
		   ha->host_no, cmd->device->channel, cmd->device->id,
		   cmd->device->lun);

	ret = SUCCESS;

eh_dev_reset_done:

	return ret;
}

static int qla4xxx_eh_target_reset(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha = to_qla_host(cmd->device->host);
	struct ddb_entry *ddb_entry = cmd->device->hostdata;
	int stat, ret;

	if (!ddb_entry)
		return FAILED;

	ret = iscsi_block_scsi_eh(cmd);
	if (ret)
		return ret;

	starget_printk(KERN_INFO, scsi_target(cmd->device),
		       "WARM TARGET RESET ISSUED.\n");

	DEBUG2(printk(KERN_INFO
		      "scsi%ld: TARGET_DEVICE_RESET cmd=%p jiffies = 0x%lx, "
		      "to=%x,dpc_flags=%lx, status=%x allowed=%d\n",
		      ha->host_no, cmd, jiffies, cmd->request->timeout / HZ,
		      ha->dpc_flags, cmd->result, cmd->allowed));

	stat = qla4xxx_reset_target(ha, ddb_entry);
	if (stat != QLA_SUCCESS) {
		starget_printk(KERN_INFO, scsi_target(cmd->device),
			       "WARM TARGET RESET FAILED.\n");
		return FAILED;
	}

	if (qla4xxx_eh_wait_for_commands(ha, scsi_target(cmd->device),
					 NULL)) {
		starget_printk(KERN_INFO, scsi_target(cmd->device),
			       "WARM TARGET DEVICE RESET FAILED - "
			       "waiting for commands.\n");
		return FAILED;
	}

	
	if (qla4xxx_send_marker_iocb(ha, ddb_entry, cmd->device->lun,
		MM_TGT_WARM_RESET) != QLA_SUCCESS) {
		starget_printk(KERN_INFO, scsi_target(cmd->device),
			       "WARM TARGET DEVICE RESET FAILED - "
			       "marker iocb failed.\n");
		return FAILED;
	}

	starget_printk(KERN_INFO, scsi_target(cmd->device),
		       "WARM TARGET RESET SUCCEEDED.\n");
	return SUCCESS;
}

static int qla4xxx_is_eh_active(struct Scsi_Host *shost)
{
	if (shost->shost_state == SHOST_RECOVERY)
		return 1;
	return 0;
}

static int qla4xxx_eh_host_reset(struct scsi_cmnd *cmd)
{
	int return_status = FAILED;
	struct scsi_qla_host *ha;

	ha = to_qla_host(cmd->device->host);

	if (ql4xdontresethba) {
		DEBUG2(printk("scsi%ld: %s: Don't Reset HBA\n",
		     ha->host_no, __func__));

		
		if (qla4xxx_is_eh_active(cmd->device->host))
			qla4xxx_abort_active_cmds(ha, DID_ABORT << 16);

		return FAILED;
	}

	ql4_printk(KERN_INFO, ha,
		   "scsi(%ld:%d:%d:%d): HOST RESET ISSUED.\n", ha->host_no,
		   cmd->device->channel, cmd->device->id, cmd->device->lun);

	if (qla4xxx_wait_for_hba_online(ha) != QLA_SUCCESS) {
		DEBUG2(printk("scsi%ld:%d: %s: Unable to reset host.  Adapter "
			      "DEAD.\n", ha->host_no, cmd->device->channel,
			      __func__));

		return FAILED;
	}

	if (!test_bit(DPC_RESET_HA, &ha->dpc_flags)) {
		if (is_qla8022(ha))
			set_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags);
		else
			set_bit(DPC_RESET_HA, &ha->dpc_flags);
	}

	if (qla4xxx_recover_adapter(ha) == QLA_SUCCESS)
		return_status = SUCCESS;

	ql4_printk(KERN_INFO, ha, "HOST RESET %s.\n",
		   return_status == FAILED ? "FAILED" : "SUCCEEDED");

	return return_status;
}

static int qla4xxx_context_reset(struct scsi_qla_host *ha)
{
	uint32_t mbox_cmd[MBOX_REG_COUNT];
	uint32_t mbox_sts[MBOX_REG_COUNT];
	struct addr_ctrl_blk_def *acb = NULL;
	uint32_t acb_len = sizeof(struct addr_ctrl_blk_def);
	int rval = QLA_SUCCESS;
	dma_addr_t acb_dma;

	acb = dma_alloc_coherent(&ha->pdev->dev,
				 sizeof(struct addr_ctrl_blk_def),
				 &acb_dma, GFP_KERNEL);
	if (!acb) {
		ql4_printk(KERN_ERR, ha, "%s: Unable to alloc acb\n",
			   __func__);
		rval = -ENOMEM;
		goto exit_port_reset;
	}

	memset(acb, 0, acb_len);

	rval = qla4xxx_get_acb(ha, acb_dma, PRIMARI_ACB, acb_len);
	if (rval != QLA_SUCCESS) {
		rval = -EIO;
		goto exit_free_acb;
	}

	rval = qla4xxx_disable_acb(ha);
	if (rval != QLA_SUCCESS) {
		rval = -EIO;
		goto exit_free_acb;
	}

	wait_for_completion_timeout(&ha->disable_acb_comp,
				    DISABLE_ACB_TOV * HZ);

	rval = qla4xxx_set_acb(ha, &mbox_cmd[0], &mbox_sts[0], acb_dma);
	if (rval != QLA_SUCCESS) {
		rval = -EIO;
		goto exit_free_acb;
	}

exit_free_acb:
	dma_free_coherent(&ha->pdev->dev, sizeof(struct addr_ctrl_blk_def),
			  acb, acb_dma);
exit_port_reset:
	DEBUG2(ql4_printk(KERN_INFO, ha, "%s %s\n", __func__,
			  rval == QLA_SUCCESS ? "SUCCEEDED" : "FAILED"));
	return rval;
}

static int qla4xxx_host_reset(struct Scsi_Host *shost, int reset_type)
{
	struct scsi_qla_host *ha = to_qla_host(shost);
	int rval = QLA_SUCCESS;

	if (ql4xdontresethba) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Don't Reset HBA\n",
				  __func__));
		rval = -EPERM;
		goto exit_host_reset;
	}

	rval = qla4xxx_wait_for_hba_online(ha);
	if (rval != QLA_SUCCESS) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: Unable to reset host "
				  "adapter\n", __func__));
		rval = -EIO;
		goto exit_host_reset;
	}

	if (test_bit(DPC_RESET_HA, &ha->dpc_flags))
		goto recover_adapter;

	switch (reset_type) {
	case SCSI_ADAPTER_RESET:
		set_bit(DPC_RESET_HA, &ha->dpc_flags);
		break;
	case SCSI_FIRMWARE_RESET:
		if (!test_bit(DPC_RESET_HA, &ha->dpc_flags)) {
			if (is_qla8022(ha))
				
				set_bit(DPC_RESET_HA_FW_CONTEXT,
					&ha->dpc_flags);
			else {
				rval = qla4xxx_context_reset(ha);
				goto exit_host_reset;
			}
		}
		break;
	}

recover_adapter:
	rval = qla4xxx_recover_adapter(ha);
	if (rval != QLA_SUCCESS) {
		DEBUG2(ql4_printk(KERN_INFO, ha, "%s: recover adapter fail\n",
				  __func__));
		rval = -EIO;
	}

exit_host_reset:
	return rval;
}

static pci_ers_result_t
qla4xxx_pci_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct scsi_qla_host *ha = pci_get_drvdata(pdev);

	ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: error detected:state %x\n",
	    ha->host_no, __func__, state);

	if (!is_aer_supported(ha))
		return PCI_ERS_RESULT_NONE;

	switch (state) {
	case pci_channel_io_normal:
		clear_bit(AF_EEH_BUSY, &ha->flags);
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		set_bit(AF_EEH_BUSY, &ha->flags);
		qla4xxx_mailbox_premature_completion(ha);
		qla4xxx_free_irqs(ha);
		pci_disable_device(pdev);
		
		qla4xxx_abort_active_cmds(ha, DID_RESET << 16);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		set_bit(AF_EEH_BUSY, &ha->flags);
		set_bit(AF_PCI_CHANNEL_IO_PERM_FAILURE, &ha->flags);
		qla4xxx_abort_active_cmds(ha, DID_NO_CONNECT << 16);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t
qla4xxx_pci_mmio_enabled(struct pci_dev *pdev)
{
	struct scsi_qla_host *ha = pci_get_drvdata(pdev);

	if (!is_aer_supported(ha))
		return PCI_ERS_RESULT_NONE;

	return PCI_ERS_RESULT_RECOVERED;
}

static uint32_t qla4_8xxx_error_recovery(struct scsi_qla_host *ha)
{
	uint32_t rval = QLA_ERROR;
	uint32_t ret = 0;
	int fn;
	struct pci_dev *other_pdev = NULL;

	ql4_printk(KERN_WARNING, ha, "scsi%ld: In %s\n", ha->host_no, __func__);

	set_bit(DPC_RESET_ACTIVE, &ha->dpc_flags);

	if (test_bit(AF_ONLINE, &ha->flags)) {
		clear_bit(AF_ONLINE, &ha->flags);
		clear_bit(AF_LINK_UP, &ha->flags);
		iscsi_host_for_each_session(ha->host, qla4xxx_fail_session);
		qla4xxx_process_aen(ha, FLUSH_DDB_CHANGED_AENS);
	}

	fn = PCI_FUNC(ha->pdev->devfn);
	while (fn > 0) {
		fn--;
		ql4_printk(KERN_INFO, ha, "scsi%ld: %s: Finding PCI device at "
		    "func %x\n", ha->host_no, __func__, fn);
		other_pdev =
		    pci_get_domain_bus_and_slot(pci_domain_nr(ha->pdev->bus),
		    ha->pdev->bus->number, PCI_DEVFN(PCI_SLOT(ha->pdev->devfn),
		    fn));

		if (!other_pdev)
			continue;

		if (atomic_read(&other_pdev->enable_cnt)) {
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: Found PCI "
			    "func in enabled state%x\n", ha->host_no,
			    __func__, fn);
			pci_dev_put(other_pdev);
			break;
		}
		pci_dev_put(other_pdev);
	}

	if (!fn) {
		ql4_printk(KERN_INFO, ha, "scsi%ld: %s: devfn being reset "
		    "0x%x is the owner\n", ha->host_no, __func__,
		    ha->pdev->devfn);

		qla4_8xxx_idc_lock(ha);
		qla4_8xxx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
		    QLA82XX_DEV_COLD);

		qla4_8xxx_wr_32(ha, QLA82XX_CRB_DRV_IDC_VERSION,
		    QLA82XX_IDC_VERSION);

		qla4_8xxx_idc_unlock(ha);
		clear_bit(AF_FW_RECOVERY, &ha->flags);
		rval = qla4xxx_initialize_adapter(ha, RESET_ADAPTER);
		qla4_8xxx_idc_lock(ha);

		if (rval != QLA_SUCCESS) {
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: HW State: "
			    "FAILED\n", ha->host_no, __func__);
			qla4_8xxx_clear_drv_active(ha);
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_FAILED);
		} else {
			ql4_printk(KERN_INFO, ha, "scsi%ld: %s: HW State: "
			    "READY\n", ha->host_no, __func__);
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_DEV_STATE,
			    QLA82XX_DEV_READY);
			
			qla4_8xxx_wr_32(ha, QLA82XX_CRB_DRV_STATE, 0);
			qla4_8xxx_set_drv_active(ha);
			ret = qla4xxx_request_irqs(ha);
			if (ret) {
				ql4_printk(KERN_WARNING, ha, "Failed to "
				    "reserve interrupt %d already in use.\n",
				    ha->pdev->irq);
				rval = QLA_ERROR;
			} else {
				ha->isp_ops->enable_intrs(ha);
				rval = QLA_SUCCESS;
			}
		}
		qla4_8xxx_idc_unlock(ha);
	} else {
		ql4_printk(KERN_INFO, ha, "scsi%ld: %s: devfn 0x%x is not "
		    "the reset owner\n", ha->host_no, __func__,
		    ha->pdev->devfn);
		if ((qla4_8xxx_rd_32(ha, QLA82XX_CRB_DEV_STATE) ==
		    QLA82XX_DEV_READY)) {
			clear_bit(AF_FW_RECOVERY, &ha->flags);
			rval = qla4xxx_initialize_adapter(ha, RESET_ADAPTER);
			if (rval == QLA_SUCCESS) {
				ret = qla4xxx_request_irqs(ha);
				if (ret) {
					ql4_printk(KERN_WARNING, ha, "Failed to"
					    " reserve interrupt %d already in"
					    " use.\n", ha->pdev->irq);
					rval = QLA_ERROR;
				} else {
					ha->isp_ops->enable_intrs(ha);
					rval = QLA_SUCCESS;
				}
			}
			qla4_8xxx_idc_lock(ha);
			qla4_8xxx_set_drv_active(ha);
			qla4_8xxx_idc_unlock(ha);
		}
	}
	clear_bit(DPC_RESET_ACTIVE, &ha->dpc_flags);
	return rval;
}

static pci_ers_result_t
qla4xxx_pci_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t ret = PCI_ERS_RESULT_DISCONNECT;
	struct scsi_qla_host *ha = pci_get_drvdata(pdev);
	int rc;

	ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: slot_reset\n",
	    ha->host_no, __func__);

	if (!is_aer_supported(ha))
		return PCI_ERS_RESULT_NONE;

	pci_restore_state(pdev);

	pci_save_state(pdev);

	
	rc = pci_enable_device(pdev);
	if (rc) {
		ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: Can't re-enable "
		    "device after reset\n", ha->host_no, __func__);
		goto exit_slot_reset;
	}

	ha->isp_ops->disable_intrs(ha);

	if (is_qla8022(ha)) {
		if (qla4_8xxx_error_recovery(ha) == QLA_SUCCESS) {
			ret = PCI_ERS_RESULT_RECOVERED;
			goto exit_slot_reset;
		} else
			goto exit_slot_reset;
	}

exit_slot_reset:
	ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: Return=%x\n"
	    "device after reset\n", ha->host_no, __func__, ret);
	return ret;
}

static void
qla4xxx_pci_resume(struct pci_dev *pdev)
{
	struct scsi_qla_host *ha = pci_get_drvdata(pdev);
	int ret;

	ql4_printk(KERN_WARNING, ha, "scsi%ld: %s: pci_resume\n",
	    ha->host_no, __func__);

	ret = qla4xxx_wait_for_hba_online(ha);
	if (ret != QLA_SUCCESS) {
		ql4_printk(KERN_ERR, ha, "scsi%ld: %s: the device failed to "
		    "resume I/O from slot/link_reset\n", ha->host_no,
		     __func__);
	}

	pci_cleanup_aer_uncorrect_error_status(pdev);
	clear_bit(AF_EEH_BUSY, &ha->flags);
}

static struct pci_error_handlers qla4xxx_err_handler = {
	.error_detected = qla4xxx_pci_error_detected,
	.mmio_enabled = qla4xxx_pci_mmio_enabled,
	.slot_reset = qla4xxx_pci_slot_reset,
	.resume = qla4xxx_pci_resume,
};

static struct pci_device_id qla4xxx_pci_tbl[] = {
	{
		.vendor		= PCI_VENDOR_ID_QLOGIC,
		.device		= PCI_DEVICE_ID_QLOGIC_ISP4010,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	},
	{
		.vendor		= PCI_VENDOR_ID_QLOGIC,
		.device		= PCI_DEVICE_ID_QLOGIC_ISP4022,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	},
	{
		.vendor		= PCI_VENDOR_ID_QLOGIC,
		.device		= PCI_DEVICE_ID_QLOGIC_ISP4032,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
	},
	{
		.vendor         = PCI_VENDOR_ID_QLOGIC,
		.device         = PCI_DEVICE_ID_QLOGIC_ISP8022,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
	},
	{0, 0},
};
MODULE_DEVICE_TABLE(pci, qla4xxx_pci_tbl);

static struct pci_driver qla4xxx_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= qla4xxx_pci_tbl,
	.probe		= qla4xxx_probe_adapter,
	.remove		= qla4xxx_remove_adapter,
	.err_handler = &qla4xxx_err_handler,
};

static int __init qla4xxx_module_init(void)
{
	int ret;

	
	srb_cachep = kmem_cache_create("qla4xxx_srbs", sizeof(struct srb), 0,
				       SLAB_HWCACHE_ALIGN, NULL);
	if (srb_cachep == NULL) {
		printk(KERN_ERR
		       "%s: Unable to allocate SRB cache..."
		       "Failing load!\n", DRIVER_NAME);
		ret = -ENOMEM;
		goto no_srp_cache;
	}

	
	strcpy(qla4xxx_version_str, QLA4XXX_DRIVER_VERSION);
	if (ql4xextended_error_logging)
		strcat(qla4xxx_version_str, "-debug");

	qla4xxx_scsi_transport =
		iscsi_register_transport(&qla4xxx_iscsi_transport);
	if (!qla4xxx_scsi_transport){
		ret = -ENODEV;
		goto release_srb_cache;
	}

	ret = pci_register_driver(&qla4xxx_pci_driver);
	if (ret)
		goto unregister_transport;

	printk(KERN_INFO "QLogic iSCSI HBA Driver\n");
	return 0;

unregister_transport:
	iscsi_unregister_transport(&qla4xxx_iscsi_transport);
release_srb_cache:
	kmem_cache_destroy(srb_cachep);
no_srp_cache:
	return ret;
}

static void __exit qla4xxx_module_exit(void)
{
	pci_unregister_driver(&qla4xxx_pci_driver);
	iscsi_unregister_transport(&qla4xxx_iscsi_transport);
	kmem_cache_destroy(srb_cachep);
}

module_init(qla4xxx_module_init);
module_exit(qla4xxx_module_exit);

MODULE_AUTHOR("QLogic Corporation");
MODULE_DESCRIPTION("QLogic iSCSI HBA Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(QLA4XXX_DRIVER_VERSION);
