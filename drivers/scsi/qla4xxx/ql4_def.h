/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2010 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef __QL4_DEF_H
#define __QL4_DEF_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dmapool.h>
#include <linux/mempool.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/aer.h>
#include <linux/bsg-lib.h>

#include <net/tcp.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_iscsi.h>
#include <scsi/scsi_bsg_iscsi.h>
#include <scsi/scsi_netlink.h>
#include <scsi/libiscsi.h>

#include "ql4_dbg.h"
#include "ql4_nx.h"
#include "ql4_fw.h"
#include "ql4_nvram.h"

#ifndef PCI_DEVICE_ID_QLOGIC_ISP4010
#define PCI_DEVICE_ID_QLOGIC_ISP4010	0x4010
#endif

#ifndef PCI_DEVICE_ID_QLOGIC_ISP4022
#define PCI_DEVICE_ID_QLOGIC_ISP4022	0x4022
#endif

#ifndef PCI_DEVICE_ID_QLOGIC_ISP4032
#define PCI_DEVICE_ID_QLOGIC_ISP4032	0x4032
#endif

#ifndef PCI_DEVICE_ID_QLOGIC_ISP8022
#define PCI_DEVICE_ID_QLOGIC_ISP8022	0x8022
#endif

#define ISP4XXX_PCI_FN_1	0x1
#define ISP4XXX_PCI_FN_2	0x3

#define QLA_SUCCESS			0
#define QLA_ERROR			1

#define BIT_0	0x1
#define BIT_1	0x2
#define BIT_2	0x4
#define BIT_3	0x8
#define BIT_4	0x10
#define BIT_5	0x20
#define BIT_6	0x40
#define BIT_7	0x80
#define BIT_8	0x100
#define BIT_9	0x200
#define BIT_10	0x400
#define BIT_11	0x800
#define BIT_12	0x1000
#define BIT_13	0x2000
#define BIT_14	0x4000
#define BIT_15	0x8000
#define BIT_16	0x10000
#define BIT_17	0x20000
#define BIT_18	0x40000
#define BIT_19	0x80000
#define BIT_20	0x100000
#define BIT_21	0x200000
#define BIT_22	0x400000
#define BIT_23	0x800000
#define BIT_24	0x1000000
#define BIT_25	0x2000000
#define BIT_26	0x4000000
#define BIT_27	0x8000000
#define BIT_28	0x10000000
#define BIT_29	0x20000000
#define BIT_30	0x40000000
#define BIT_31	0x80000000

#define ql4_printk(level, ha, format, arg...) \
	dev_printk(level , &((ha)->pdev->dev) , format , ## arg)


#define MAX_HBAS		16
#define MAX_BUSES		1
#define MAX_TARGETS		MAX_DEV_DB_ENTRIES
#define MAX_LUNS		0xffff
#define MAX_AEN_ENTRIES		MAX_DEV_DB_ENTRIES
#define MAX_DDB_ENTRIES		MAX_DEV_DB_ENTRIES
#define MAX_PDU_ENTRIES		32
#define INVALID_ENTRY		0xFFFF
#define MAX_CMDS_TO_RISC	1024
#define MAX_SRBS		MAX_CMDS_TO_RISC
#define MBOX_AEN_REG_COUNT	8
#define MAX_INIT_RETRIES	5

#define REQUEST_QUEUE_DEPTH		MAX_CMDS_TO_RISC
#define RESPONSE_QUEUE_DEPTH		64
#define QUEUE_SIZE			64
#define DMA_BUFFER_SIZE			512

#define MAC_ADDR_LEN			6	
#define IP_ADDR_LEN			4	
#define IPv6_ADDR_LEN			16	
#define DRIVER_NAME			"qla4xxx"

#define MAX_LINKED_CMDS_PER_LUN		3
#define MAX_REQS_SERVICED_PER_INTR	1

#define ISCSI_IPADDR_SIZE		4	
#define ISCSI_ALIAS_SIZE		32	
#define ISCSI_NAME_SIZE			0xE0	

#define QL4_SESS_RECOVERY_TMO		120	
						

#define LSDW(x) ((u32)((u64)(x)))
#define MSDW(x) ((u32)((((u64)(x)) >> 16) >> 16))

#define MBOX_TOV			60
#define SOFT_RESET_TOV			30
#define RESET_INTR_TOV			3
#define SEMAPHORE_TOV			10
#define ADAPTER_INIT_TOV		30
#define ADAPTER_RESET_TOV		180
#define EXTEND_CMD_TOV			60
#define WAIT_CMD_TOV			30
#define EH_WAIT_CMD_TOV			120
#define FIRMWARE_UP_TOV			60
#define RESET_FIRMWARE_TOV		30
#define LOGOUT_TOV			10
#define IOCB_TOV_MARGIN			10
#define RELOGIN_TOV			18
#define ISNS_DEREG_TOV			5
#define HBA_ONLINE_TOV			30
#define DISABLE_ACB_TOV			30
#define IP_CONFIG_TOV			30
#define LOGIN_TOV			12

#define MAX_RESET_HA_RETRIES		2
#define FW_ALIVE_WAIT_TOV		3

#define CMD_SP(Cmnd)			((Cmnd)->SCp.ptr)

struct srb {
	struct list_head list;	
	struct scsi_qla_host *ha;	
	struct ddb_entry *ddb;
	uint16_t flags;		

#define SRB_DMA_VALID		BIT_3	
#define SRB_GOT_SENSE		BIT_4	
	uint8_t state;		

#define SRB_NO_QUEUE_STATE	 0	
#define SRB_FREE_STATE		 1
#define SRB_ACTIVE_STATE	 3
#define SRB_ACTIVE_TIMEOUT_STATE 4
#define SRB_SUSPENDED_STATE	 7	

	struct scsi_cmnd *cmd;	
	dma_addr_t dma_handle;	
	struct kref srb_ref;	
	uint8_t err_id;		
#define SRB_ERR_PORT	   1	
#define SRB_ERR_LOOP	   2	
#define SRB_ERR_DEVICE	   3	
#define SRB_ERR_OTHER	   4

	uint16_t reserved;
	uint16_t iocb_tov;
	uint16_t iocb_cnt;	
	uint16_t cc_stat;

	
	uint8_t *req_sense_ptr;
	uint16_t req_sense_len;
	uint16_t reserved2;
};

struct mrb {
	struct scsi_qla_host *ha;
	struct mbox_cmd_iocb *mbox;
	uint32_t mbox_cmd;
	uint16_t iocb_cnt;		
	uint32_t pid;
};

struct aen {
        uint32_t mbox_sts[MBOX_AEN_REG_COUNT];
};

struct ql4_aen_log {
        int count;
        struct aen entry[MAX_AEN_ENTRIES];
};

struct ddb_entry {
	struct scsi_qla_host *ha;
	struct iscsi_cls_session *sess;
	struct iscsi_cls_conn *conn;

	uint16_t fw_ddb_index;	
	uint32_t fw_ddb_device_state; 
	uint16_t ddb_type;
#define FLASH_DDB 0x01

	struct dev_db_entry fw_ddb_entry;
	int (*unblock_sess)(struct iscsi_cls_session *cls_session);
	int (*ddb_change)(struct scsi_qla_host *ha, uint32_t fw_ddb_index,
			  struct ddb_entry *ddb_entry, uint32_t state);

	
	unsigned long flags;		  
	uint16_t default_relogin_timeout; 
	atomic_t retry_relogin_timer;	  
	atomic_t relogin_timer;		  
	atomic_t relogin_retry_count;	  
	uint32_t default_time2wait;	  
	uint16_t chap_tbl_idx;
};

struct qla_ddb_index {
	struct list_head list;
	uint16_t fw_ddb_idx;
	struct dev_db_entry fw_ddb;
};

#define DDB_IPADDR_LEN 64

struct ql4_tuple_ddb {
	int port;
	int tpgt;
	char ip_addr[DDB_IPADDR_LEN];
	char iscsi_name[ISCSI_NAME_SIZE];
	uint16_t options;
#define DDB_OPT_IPV6 0x0e0e
#define DDB_OPT_IPV4 0x0f0f
	uint8_t isid[6];
};

#define DDB_STATE_DEAD		0	
#define DDB_STATE_ONLINE	1	
#define DDB_STATE_MISSING	2	

#define DF_RELOGIN		0	
#define DF_ISNS_DISCOVERED	2	
#define DF_FO_MASKED		3

enum qla4_work_type {
	QLA4_EVENT_AEN,
	QLA4_EVENT_PING_STATUS,
};

struct qla4_work_evt {
	struct list_head list;
	enum qla4_work_type type;
	union {
		struct {
			enum iscsi_host_event_code code;
			uint32_t data_size;
			uint8_t data[0];
		} aen;
		struct {
			uint32_t status;
			uint32_t pid;
			uint32_t data_size;
			uint8_t data[0];
		} ping;
	} u;
};

struct ql82xx_hw_data {
	
	uint32_t flash_conf_off;
	uint32_t flash_data_off;

	uint32_t fdt_wrt_disable;
	uint32_t fdt_erase_cmd;
	uint32_t fdt_block_size;
	uint32_t fdt_unprotect_sec_cmd;
	uint32_t fdt_protect_sec_cmd;

	uint32_t flt_region_flt;
	uint32_t flt_region_fdt;
	uint32_t flt_region_boot;
	uint32_t flt_region_bootload;
	uint32_t flt_region_fw;

	uint32_t flt_iscsi_param;
	uint32_t flt_region_chap;
	uint32_t flt_chap_size;
};

struct qla4_8xxx_legacy_intr_set {
	uint32_t int_vec_bit;
	uint32_t tgt_status_reg;
	uint32_t tgt_mask_reg;
	uint32_t pci_int_reg;
};


#define QLA_MSIX_DEFAULT	0x00
#define QLA_MSIX_RSP_Q		0x01

#define QLA_MSIX_ENTRIES	2
#define QLA_MIDX_DEFAULT	0
#define QLA_MIDX_RSP_Q		1

struct ql4_msix_entry {
	int have_irq;
	uint16_t msix_vector;
	uint16_t msix_entry;
};

struct isp_operations {
	int (*iospace_config) (struct scsi_qla_host *ha);
	void (*pci_config) (struct scsi_qla_host *);
	void (*disable_intrs) (struct scsi_qla_host *);
	void (*enable_intrs) (struct scsi_qla_host *);
	int (*start_firmware) (struct scsi_qla_host *);
	irqreturn_t (*intr_handler) (int , void *);
	void (*interrupt_service_routine) (struct scsi_qla_host *, uint32_t);
	int (*reset_chip) (struct scsi_qla_host *);
	int (*reset_firmware) (struct scsi_qla_host *);
	void (*queue_iocb) (struct scsi_qla_host *);
	void (*complete_iocb) (struct scsi_qla_host *);
	uint16_t (*rd_shdw_req_q_out) (struct scsi_qla_host *);
	uint16_t (*rd_shdw_rsp_q_in) (struct scsi_qla_host *);
	int (*get_sys_info) (struct scsi_qla_host *);
};

struct ipaddress_config {
	uint16_t ipv4_options;
	uint16_t tcp_options;
	uint16_t ipv4_vlan_tag;
	uint8_t ipv4_addr_state;
	uint8_t ip_address[IP_ADDR_LEN];
	uint8_t subnet_mask[IP_ADDR_LEN];
	uint8_t gateway[IP_ADDR_LEN];
	uint32_t ipv6_options;
	uint32_t ipv6_addl_options;
	uint8_t ipv6_link_local_state;
	uint8_t ipv6_addr0_state;
	uint8_t ipv6_addr1_state;
	uint8_t ipv6_default_router_state;
	uint16_t ipv6_vlan_tag;
	struct in6_addr ipv6_link_local_addr;
	struct in6_addr ipv6_addr0;
	struct in6_addr ipv6_addr1;
	struct in6_addr ipv6_default_router_addr;
	uint16_t eth_mtu_size;
	uint16_t ipv4_port;
	uint16_t ipv6_port;
};

#define QL4_CHAP_MAX_NAME_LEN 256
#define QL4_CHAP_MAX_SECRET_LEN 100
#define LOCAL_CHAP	0
#define BIDI_CHAP	1

struct ql4_chap_format {
	u8  intr_chap_name[QL4_CHAP_MAX_NAME_LEN];
	u8  intr_secret[QL4_CHAP_MAX_SECRET_LEN];
	u8  target_chap_name[QL4_CHAP_MAX_NAME_LEN];
	u8  target_secret[QL4_CHAP_MAX_SECRET_LEN];
	u16 intr_chap_name_length;
	u16 intr_secret_length;
	u16 target_chap_name_length;
	u16 target_secret_length;
};

struct ip_address_format {
	u8 ip_type;
	u8 ip_address[16];
};

struct	ql4_conn_info {
	u16	dest_port;
	struct	ip_address_format dest_ipaddr;
	struct	ql4_chap_format chap;
};

struct ql4_boot_session_info {
	u8	target_name[224];
	struct	ql4_conn_info conn_list[1];
};

struct ql4_boot_tgt_info {
	struct ql4_boot_session_info boot_pri_sess;
	struct ql4_boot_session_info boot_sec_sess;
};

struct scsi_qla_host {
	
	unsigned long flags;

#define AF_ONLINE			0 
#define AF_INIT_DONE			1 
#define AF_MBOX_COMMAND			2 
#define AF_MBOX_COMMAND_DONE		3 
#define AF_INTERRUPTS_ON		6 
#define AF_GET_CRASH_RECORD		7 
#define AF_LINK_UP			8 
#define AF_IRQ_ATTACHED			10 
#define AF_DISABLE_ACB_COMPLETE		11 
#define AF_HA_REMOVAL			12 
#define AF_INTx_ENABLED			15 
#define AF_MSI_ENABLED			16 
#define AF_MSIX_ENABLED			17 
#define AF_MBOX_COMMAND_NOPOLL		18 
#define AF_FW_RECOVERY			19 
#define AF_EEH_BUSY			20 
#define AF_PCI_CHANNEL_IO_PERM_FAILURE	21 
#define AF_BUILD_DDB_LIST		22 
	unsigned long dpc_flags;

#define DPC_RESET_HA			1 
#define DPC_RETRY_RESET_HA		2 
#define DPC_RELOGIN_DEVICE		3 
#define DPC_RESET_HA_FW_CONTEXT		4 
#define DPC_RESET_HA_INTR		5 
#define DPC_ISNS_RESTART		7 
#define DPC_AEN				9 
#define DPC_GET_DHCP_IP_ADDR		15 
#define DPC_LINK_CHANGED		18 
#define DPC_RESET_ACTIVE		20 
#define DPC_HA_UNRECOVERABLE		21 
#define DPC_HA_NEED_QUIESCENT		22 


	struct Scsi_Host *host; 
	uint32_t tot_ddbs;

	uint16_t iocb_cnt;

	
#define SRB_MIN_REQ	128
	mempool_t *srb_mempool;

	
	struct pci_dev *pdev;

	struct isp_reg __iomem *reg; 
	unsigned long pio_address;
	unsigned long pio_length;
#define MIN_IOBASE_LEN		0x100

	uint16_t req_q_count;

	unsigned long host_no;

	
	struct eeprom_data *nvram;
	spinlock_t hardware_lock ____cacheline_aligned;
	uint32_t eeprom_cmd_data;

	
	uint64_t isr_count;
	uint64_t adapter_error_count;
	uint64_t device_error_count;
	uint64_t total_io_count;
	uint64_t total_mbytes_xferred;
	uint64_t link_failure_count;
	uint64_t invalid_crc_count;
	uint32_t bytes_xfered;
	uint32_t spurious_int_count;
	uint32_t aborted_io_count;
	uint32_t io_timeout_count;
	uint32_t mailbox_timeout_count;
	uint32_t seconds_since_last_intr;
	uint32_t seconds_since_last_heartbeat;
	uint32_t mac_index;

	
	
	uint32_t firmware_version[2];
	uint32_t patch_number;
	uint32_t build_number;
	uint32_t board_id;

	
	
	uint16_t firmware_options;
	uint8_t alias[32];
	uint8_t name_string[256];
	uint8_t heartbeat_interval;

	
	uint8_t my_mac[MAC_ADDR_LEN];
	uint8_t serial_number[16];
	uint16_t port_num;
	
	uint32_t firmware_state;
	uint32_t addl_fw_state;

	
	struct workqueue_struct *dpc_thread;
	struct work_struct dpc_work;

	
	struct timer_list timer;
	uint32_t timer_active;

	
	atomic_t check_relogin_timeouts;
	uint32_t retry_reset_ha_cnt;
	uint32_t isp_reset_timer;	
	uint32_t nic_reset_timer;	
	int eh_start;
	struct list_head free_srb_q;
	uint16_t free_srb_q_count;
	uint16_t num_srbs_allocated;

	
	void *queues;
	dma_addr_t queues_dma;
	unsigned long queues_len;

#define MEM_ALIGN_VALUE \
	    ((max(REQUEST_QUEUE_DEPTH, RESPONSE_QUEUE_DEPTH)) * \
	     sizeof(struct queue_entry))
	
	dma_addr_t request_dma;
	struct queue_entry *request_ring;
	struct queue_entry *request_ptr;
	dma_addr_t response_dma;
	struct queue_entry *response_ring;
	struct queue_entry *response_ptr;
	dma_addr_t shadow_regs_dma;
	struct shadow_regs *shadow_regs;
	uint16_t request_in;	
	uint16_t request_out;
	uint16_t response_in;
	uint16_t response_out;

	
	uint16_t aen_q_count;	
	uint16_t aen_in;	
	uint16_t aen_out;
	struct aen aen_q[MAX_AEN_ENTRIES];

	struct ql4_aen_log aen_log;

	struct mutex  mbox_sem;

	
	volatile uint8_t mbox_status_count;
	volatile uint32_t mbox_status[MBOX_REG_COUNT];

	
	struct ddb_entry *fw_ddb_index_map[MAX_DDB_ENTRIES];

	
	struct srb *status_srb;

	uint8_t acb_version;

	
	struct device_reg_82xx  __iomem *qla4_8xxx_reg; 
	unsigned long nx_pcibase;	
	uint8_t *nx_db_rd_ptr;		
	unsigned long nx_db_wr_ptr;	
	unsigned long first_page_group_start;
	unsigned long first_page_group_end;

	uint32_t crb_win;
	uint32_t curr_window;
	uint32_t ddr_mn_window;
	unsigned long mn_win_crb;
	unsigned long ms_win_crb;
	int qdr_sn_window;
	rwlock_t hw_lock;
	uint16_t func_num;
	int link_width;

	struct qla4_8xxx_legacy_intr_set nx_legacy_intr;
	u32 nx_crb_mask;

	uint8_t revision_id;
	uint32_t fw_heartbeat_counter;

	struct isp_operations *isp_ops;
	struct ql82xx_hw_data hw;

	struct ql4_msix_entry msix_entries[QLA_MSIX_ENTRIES];

	uint32_t nx_dev_init_timeout;
	uint32_t nx_reset_timeout;

	struct completion mbx_intr_comp;

	struct ipaddress_config ip_config;
	struct iscsi_iface *iface_ipv4;
	struct iscsi_iface *iface_ipv6_0;
	struct iscsi_iface *iface_ipv6_1;

	
	uint16_t iscsi_major;
	uint16_t iscsi_minor;
	uint16_t bootload_major;
	uint16_t bootload_minor;
	uint16_t bootload_patch;
	uint16_t bootload_build;
	uint16_t def_timeout; 

	uint32_t flash_state;
#define	QLFLASH_WAITING		0
#define	QLFLASH_READING		1
#define	QLFLASH_WRITING		2
	struct dma_pool *chap_dma_pool;
	uint8_t *chap_list; 
	struct mutex  chap_sem;

#define CHAP_DMA_BLOCK_SIZE    512
	struct workqueue_struct *task_wq;
	unsigned long ddb_idx_map[MAX_DDB_ENTRIES / BITS_PER_LONG];
#define SYSFS_FLAG_FW_SEL_BOOT 2
	struct iscsi_boot_kset *boot_kset;
	struct ql4_boot_tgt_info boot_tgt;
	uint16_t phy_port_num;
	uint16_t phy_port_cnt;
	uint16_t iscsi_pci_func_cnt;
	uint8_t model_name[16];
	struct completion disable_acb_comp;
	struct dma_pool *fw_ddb_dma_pool;
#define DDB_DMA_BLOCK_SIZE 512
	uint16_t pri_ddb_idx;
	uint16_t sec_ddb_idx;
	int is_reset;
	uint16_t temperature;

	
	struct list_head work_list;
	spinlock_t work_lock;

	
#define MAX_MRB		128
	struct mrb *active_mrb_array[MAX_MRB];
	uint32_t mrb_index;
};

struct ql4_task_data {
	struct scsi_qla_host *ha;
	uint8_t iocb_req_cnt;
	dma_addr_t data_dma;
	void *req_buffer;
	dma_addr_t req_dma;
	uint32_t req_len;
	void *resp_buffer;
	dma_addr_t resp_dma;
	uint32_t resp_len;
	struct iscsi_task *task;
	struct passthru_status sts;
	struct work_struct task_work;
};

struct qla_endpoint {
	struct Scsi_Host *host;
	struct sockaddr dst_addr;
};

struct qla_conn {
	struct qla_endpoint *qla_ep;
};

static inline int is_ipv4_enabled(struct scsi_qla_host *ha)
{
	return ((ha->ip_config.ipv4_options & IPOPT_IPV4_PROTOCOL_ENABLE) != 0);
}

static inline int is_ipv6_enabled(struct scsi_qla_host *ha)
{
	return ((ha->ip_config.ipv6_options &
		IPV6_OPT_IPV6_PROTOCOL_ENABLE) != 0);
}

static inline int is_qla4010(struct scsi_qla_host *ha)
{
	return ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP4010;
}

static inline int is_qla4022(struct scsi_qla_host *ha)
{
	return ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP4022;
}

static inline int is_qla4032(struct scsi_qla_host *ha)
{
	return ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP4032;
}

static inline int is_qla40XX(struct scsi_qla_host *ha)
{
	return is_qla4032(ha) || is_qla4022(ha) || is_qla4010(ha);
}

static inline int is_qla8022(struct scsi_qla_host *ha)
{
	return ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8022;
}

static inline int is_aer_supported(struct scsi_qla_host *ha)
{
	return ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP8022;
}

static inline int adapter_up(struct scsi_qla_host *ha)
{
	return (test_bit(AF_ONLINE, &ha->flags) != 0) &&
		(test_bit(AF_LINK_UP, &ha->flags) != 0);
}

static inline struct scsi_qla_host* to_qla_host(struct Scsi_Host *shost)
{
	return (struct scsi_qla_host *)iscsi_host_priv(shost);
}

static inline void __iomem* isp_semaphore(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u1.isp4010.nvram :
		&ha->reg->u1.isp4022.semaphore);
}

static inline void __iomem* isp_nvram(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u1.isp4010.nvram :
		&ha->reg->u1.isp4022.nvram);
}

static inline void __iomem* isp_ext_hw_conf(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u2.isp4010.ext_hw_conf :
		&ha->reg->u2.isp4022.p0.ext_hw_conf);
}

static inline void __iomem* isp_port_status(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u2.isp4010.port_status :
		&ha->reg->u2.isp4022.p0.port_status);
}

static inline void __iomem* isp_port_ctrl(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u2.isp4010.port_ctrl :
		&ha->reg->u2.isp4022.p0.port_ctrl);
}

static inline void __iomem* isp_port_error_status(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u2.isp4010.port_err_status :
		&ha->reg->u2.isp4022.p0.port_err_status);
}

static inline void __iomem * isp_gp_out(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		&ha->reg->u2.isp4010.gp_out :
		&ha->reg->u2.isp4022.p0.gp_out);
}

static inline int eeprom_ext_hw_conf_offset(struct scsi_qla_host *ha)
{
	return (is_qla4010(ha) ?
		offsetof(struct eeprom_data, isp4010.ext_hw_conf) / 2 :
		offsetof(struct eeprom_data, isp4022.ext_hw_conf) / 2);
}

int ql4xxx_sem_spinlock(struct scsi_qla_host * ha, u32 sem_mask, u32 sem_bits);
void ql4xxx_sem_unlock(struct scsi_qla_host * ha, u32 sem_mask);
int ql4xxx_sem_lock(struct scsi_qla_host * ha, u32 sem_mask, u32 sem_bits);

static inline int ql4xxx_lock_flash(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		return ql4xxx_sem_spinlock(a, QL4010_FLASH_SEM_MASK,
					   QL4010_FLASH_SEM_BITS);
	else
		return ql4xxx_sem_spinlock(a, QL4022_FLASH_SEM_MASK,
					   (QL4022_RESOURCE_BITS_BASE_CODE |
					    (a->mac_index)) << 13);
}

static inline void ql4xxx_unlock_flash(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		ql4xxx_sem_unlock(a, QL4010_FLASH_SEM_MASK);
	else
		ql4xxx_sem_unlock(a, QL4022_FLASH_SEM_MASK);
}

static inline int ql4xxx_lock_nvram(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		return ql4xxx_sem_spinlock(a, QL4010_NVRAM_SEM_MASK,
					   QL4010_NVRAM_SEM_BITS);
	else
		return ql4xxx_sem_spinlock(a, QL4022_NVRAM_SEM_MASK,
					   (QL4022_RESOURCE_BITS_BASE_CODE |
					    (a->mac_index)) << 10);
}

static inline void ql4xxx_unlock_nvram(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		ql4xxx_sem_unlock(a, QL4010_NVRAM_SEM_MASK);
	else
		ql4xxx_sem_unlock(a, QL4022_NVRAM_SEM_MASK);
}

static inline int ql4xxx_lock_drvr(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		return ql4xxx_sem_lock(a, QL4010_DRVR_SEM_MASK,
				       QL4010_DRVR_SEM_BITS);
	else
		return ql4xxx_sem_lock(a, QL4022_DRVR_SEM_MASK,
				       (QL4022_RESOURCE_BITS_BASE_CODE |
					(a->mac_index)) << 1);
}

static inline void ql4xxx_unlock_drvr(struct scsi_qla_host *a)
{
	if (is_qla4010(a))
		ql4xxx_sem_unlock(a, QL4010_DRVR_SEM_MASK);
	else
		ql4xxx_sem_unlock(a, QL4022_DRVR_SEM_MASK);
}

static inline int ql4xxx_reset_active(struct scsi_qla_host *ha)
{
	return test_bit(DPC_RESET_ACTIVE, &ha->dpc_flags) ||
	       test_bit(DPC_RESET_HA, &ha->dpc_flags) ||
	       test_bit(DPC_RETRY_RESET_HA, &ha->dpc_flags) ||
	       test_bit(DPC_RESET_HA_INTR, &ha->dpc_flags) ||
	       test_bit(DPC_RESET_HA_FW_CONTEXT, &ha->dpc_flags) ||
	       test_bit(DPC_HA_UNRECOVERABLE, &ha->dpc_flags);

}


#define INIT_ADAPTER    0
#define RESET_ADAPTER   1

#define PRESERVE_DDB_LIST	0
#define REBUILD_DDB_LIST	1

#define PROCESS_ALL_AENS	 0
#define FLUSH_DDB_CHANGED_AENS	 1

#endif	
