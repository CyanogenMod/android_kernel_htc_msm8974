/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2010 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef _QLA4X_FW_H
#define _QLA4X_FW_H


#define MAX_PRST_DEV_DB_ENTRIES		64
#define MIN_DISC_DEV_DB_ENTRY		MAX_PRST_DEV_DB_ENTRIES
#define MAX_DEV_DB_ENTRIES		512
#define MAX_DEV_DB_ENTRIES_40XX		256


struct port_ctrl_stat_regs {
	__le32 ext_hw_conf;	
	__le32 rsrvd0;		
	__le32 port_ctrl;	
	__le32 port_status;	
	__le32 rsrvd1[32];	
	__le32 gp_out;		
	__le32 gp_in;		
	__le32 rsrvd2[5];	
	__le32 port_err_status; 
};

struct host_mem_cfg_regs {
	__le32 rsrvd0[12];	
	__le32 req_q_out;	
	__le32 rsrvd1[31];	
};

struct device_reg_82xx {
	__le32 req_q_out;	
	__le32 reserve1[63];	
	__le32 rsp_q_in;	
	__le32 reserve2[63];	
	__le32 rsp_q_out;	
	__le32 reserve3[63];	

	__le32 mailbox_in[8];	
	__le32 reserve4[24];
	__le32 hint;		
#define HINT_MBX_INT_PENDING	BIT_0
	__le32 reserve5[31];
	__le32 mailbox_out[8];	
	__le32 reserve6[56];

	__le32 host_status;	
#define HSRX_RISC_MB_INT	BIT_0  
#define HSRX_RISC_IOCB_INT	BIT_1  

	__le32 host_int;	
#define ISRX_82XX_RISC_INT	BIT_0 
};

struct isp_reg {
#define MBOX_REG_COUNT 8
	__le32 mailbox[MBOX_REG_COUNT];

	__le32 flash_address;	
	__le32 flash_data;
	__le32 ctrl_status;

	union {
		struct {
			__le32 nvram;
			__le32 reserved1[2]; 
		} __attribute__ ((packed)) isp4010;
		struct {
			__le32 intr_mask;
			__le32 nvram; 
			__le32 semaphore;
		} __attribute__ ((packed)) isp4022;
	} u1;

	__le32 req_q_in;    
	__le32 rsp_q_out;   

	__le32 reserved2[4];	

	union {
		struct {
			__le32 ext_hw_conf; 
			__le32 flow_ctrl;
			__le32 port_ctrl;
			__le32 port_status;

			__le32 reserved3[8]; 

			__le32 req_q_out; 

			__le32 reserved4[23]; 

			__le32 gp_out; 
			__le32 gp_in;

			__le32 reserved5[5];

			__le32 port_err_status; 
		} __attribute__ ((packed)) isp4010;
		struct {
			union {
				struct port_ctrl_stat_regs p0;
				struct host_mem_cfg_regs p1;
			};
		} __attribute__ ((packed)) isp4022;
	} u2;
};				


#define QL4010_DRVR_SEM_BITS	0x00000030
#define QL4010_GPIO_SEM_BITS	0x000000c0
#define QL4010_SDRAM_SEM_BITS	0x00000300
#define QL4010_PHY_SEM_BITS	0x00000c00
#define QL4010_NVRAM_SEM_BITS	0x00003000
#define QL4010_FLASH_SEM_BITS	0x0000c000

#define QL4010_DRVR_SEM_MASK	0x00300000
#define QL4010_GPIO_SEM_MASK	0x00c00000
#define QL4010_SDRAM_SEM_MASK	0x03000000
#define QL4010_PHY_SEM_MASK	0x0c000000
#define QL4010_NVRAM_SEM_MASK	0x30000000
#define QL4010_FLASH_SEM_MASK	0xc0000000

#define QL4022_RESOURCE_MASK_BASE_CODE 0x7
#define QL4022_RESOURCE_BITS_BASE_CODE 0x4


#define QL4022_DRVR_SEM_MASK	(QL4022_RESOURCE_MASK_BASE_CODE << (1+16))
#define QL4022_DDR_RAM_SEM_MASK (QL4022_RESOURCE_MASK_BASE_CODE << (4+16))
#define QL4022_PHY_GIO_SEM_MASK (QL4022_RESOURCE_MASK_BASE_CODE << (7+16))
#define QL4022_NVRAM_SEM_MASK	(QL4022_RESOURCE_MASK_BASE_CODE << (10+16))
#define QL4022_FLASH_SEM_MASK	(QL4022_RESOURCE_MASK_BASE_CODE << (13+16))

#define NVRAM_PORT0_BOOT_MODE		0x03b1
#define NVRAM_PORT0_BOOT_PRI_TGT	0x03b2
#define NVRAM_PORT0_BOOT_SEC_TGT	0x03bb
#define NVRAM_PORT1_BOOT_MODE		0x07b1
#define NVRAM_PORT1_BOOT_PRI_TGT	0x07b2
#define NVRAM_PORT1_BOOT_SEC_TGT	0x07bb


#define PORT_CTRL_STAT_PAGE			0	
#define HOST_MEM_CFG_PAGE			1	
#define LOCAL_RAM_CFG_PAGE			2	
#define PROT_STAT_PAGE				3	

static inline uint32_t set_rmask(uint32_t val)
{
	return (val & 0xffff) | (val << 16);
}


static inline uint32_t clr_rmask(uint32_t val)
{
	return 0 | (val << 16);
}

#define CSR_SCSI_PAGE_SELECT			0x00000003
#define CSR_SCSI_INTR_ENABLE			0x00000004	
#define CSR_SCSI_RESET_INTR			0x00000008
#define CSR_SCSI_COMPLETION_INTR		0x00000010
#define CSR_SCSI_PROCESSOR_INTR			0x00000020
#define CSR_INTR_RISC				0x00000040
#define CSR_BOOT_ENABLE				0x00000080
#define CSR_NET_PAGE_SELECT			0x00000300	
#define CSR_FUNC_NUM				0x00000700	
#define CSR_NET_RESET_INTR			0x00000800	
#define CSR_FORCE_SOFT_RESET			0x00002000	
#define CSR_FATAL_ERROR				0x00004000
#define CSR_SOFT_RESET				0x00008000
#define ISP_CONTROL_FN_MASK			CSR_FUNC_NUM
#define ISP_CONTROL_FN0_SCSI			0x0500
#define ISP_CONTROL_FN1_SCSI			0x0700

#define INTR_PENDING				(CSR_SCSI_COMPLETION_INTR |\
						 CSR_SCSI_PROCESSOR_INTR |\
						 CSR_SCSI_RESET_INTR)

#define IMR_SCSI_INTR_ENABLE			0x00000004	

#define NVR_WRITE_ENABLE			0x00000010	

#define QL4010_NVRAM_SIZE			0x200
#define QL40X2_NVRAM_SIZE			0x800



#define GPOR_TOPCAT_RESET			0x00000004

struct shadow_regs {
	
	__le32 req_q_out;	

	
	__le32 rsp_q_in;	
};		  


union external_hw_config_reg {
	struct {
		__le32 bReserved0:1;
		__le32 bSDRAMProtectionMethod:2;
		__le32 bSDRAMBanks:1;
		__le32 bSDRAMChipWidth:1;
		__le32 bSDRAMChipSize:2;
		__le32 bParityDisable:1;
		__le32 bExternalMemoryType:1;
		__le32 bFlashBIOSWriteEnable:1;
		__le32 bFlashUpperBankSelect:1;
		__le32 bWriteBurst:2;
		__le32 bReserved1:3;
		__le32 bMask:16;
	};
	uint32_t Asuint32_t;
};

#define FA_FLASH_LAYOUT_ADDR_82		0xFC400
#define FA_FLASH_DESCR_ADDR_82		0xFC000
#define FA_BOOT_LOAD_ADDR_82		0x04000
#define FA_BOOT_CODE_ADDR_82		0x20000
#define FA_RISC_CODE_ADDR_82		0x40000
#define FA_GOLD_RISC_CODE_ADDR_82	0x80000
#define FA_FLASH_ISCSI_CHAP		0x540000
#define FA_FLASH_CHAP_SIZE		0xC0000

struct qla_fdt_layout {
	uint8_t sig[4];
	uint16_t version;
	uint16_t len;
	uint16_t checksum;
	uint8_t unused1[2];
	uint8_t model[16];
	uint16_t man_id;
	uint16_t id;
	uint8_t flags;
	uint8_t erase_cmd;
	uint8_t alt_erase_cmd;
	uint8_t wrt_enable_cmd;
	uint8_t wrt_enable_bits;
	uint8_t wrt_sts_reg_cmd;
	uint8_t unprotect_sec_cmd;
	uint8_t read_man_id_cmd;
	uint32_t block_size;
	uint32_t alt_block_size;
	uint32_t flash_size;
	uint32_t wrt_enable_data;
	uint8_t read_id_addr_len;
	uint8_t wrt_disable_bits;
	uint8_t read_dev_id_len;
	uint8_t chip_erase_cmd;
	uint16_t read_timeout;
	uint8_t protect_sec_cmd;
	uint8_t unused2[65];
};


struct qla_flt_location {
	uint8_t sig[4];
	uint16_t start_lo;
	uint16_t start_hi;
	uint8_t version;
	uint8_t unused[5];
	uint16_t checksum;
};

struct qla_flt_header {
	uint16_t version;
	uint16_t length;
	uint16_t checksum;
	uint16_t unused;
};

#define FLT_REG_FDT		0x1a
#define FLT_REG_FLT		0x1c
#define FLT_REG_BOOTLOAD_82	0x72
#define FLT_REG_FW_82		0x74
#define FLT_REG_FW_82_1		0x97
#define FLT_REG_GOLD_FW_82	0x75
#define FLT_REG_BOOT_CODE_82	0x78
#define FLT_REG_ISCSI_PARAM	0x65
#define FLT_REG_ISCSI_CHAP	0x63

struct qla_flt_region {
	uint32_t code;
	uint32_t size;
	uint32_t start;
	uint32_t end;
};


#define MBOX_CMD_ABOUT_FW			0x0009
#define MBOX_CMD_PING				0x000B
#define PING_IPV6_PROTOCOL_ENABLE		0x1
#define PING_IPV6_LINKLOCAL_ADDR		0x4
#define PING_IPV6_ADDR0				0x8
#define PING_IPV6_ADDR1				0xC
#define MBOX_CMD_ENABLE_INTRS			0x0010
#define INTR_DISABLE				0
#define INTR_ENABLE				1
#define MBOX_CMD_STOP_FW			0x0014
#define MBOX_CMD_ABORT_TASK			0x0015
#define MBOX_CMD_LUN_RESET			0x0016
#define MBOX_CMD_TARGET_WARM_RESET		0x0017
#define MBOX_CMD_GET_MANAGEMENT_DATA		0x001E
#define MBOX_CMD_GET_FW_STATUS			0x001F
#define MBOX_CMD_SET_ISNS_SERVICE		0x0021
#define ISNS_DISABLE				0
#define ISNS_ENABLE				1
#define MBOX_CMD_COPY_FLASH			0x0024
#define MBOX_CMD_WRITE_FLASH			0x0025
#define MBOX_CMD_READ_FLASH			0x0026
#define MBOX_CMD_CLEAR_DATABASE_ENTRY		0x0031
#define MBOX_CMD_CONN_OPEN			0x0074
#define MBOX_CMD_CONN_CLOSE_SESS_LOGOUT		0x0056
#define LOGOUT_OPTION_CLOSE_SESSION		0x0002
#define LOGOUT_OPTION_RELOGIN			0x0004
#define LOGOUT_OPTION_FREE_DDB			0x0008
#define MBOX_CMD_EXECUTE_IOCB_A64		0x005A
#define MBOX_CMD_INITIALIZE_FIRMWARE		0x0060
#define MBOX_CMD_GET_INIT_FW_CTRL_BLOCK		0x0061
#define MBOX_CMD_REQUEST_DATABASE_ENTRY		0x0062
#define MBOX_CMD_SET_DATABASE_ENTRY		0x0063
#define MBOX_CMD_GET_DATABASE_ENTRY		0x0064
#define DDB_DS_UNASSIGNED			0x00
#define DDB_DS_NO_CONNECTION_ACTIVE		0x01
#define DDB_DS_DISCOVERY			0x02
#define DDB_DS_SESSION_ACTIVE			0x04
#define DDB_DS_SESSION_FAILED			0x06
#define DDB_DS_LOGIN_IN_PROCESS			0x07
#define MBOX_CMD_GET_FW_STATE			0x0069
#define MBOX_CMD_GET_INIT_FW_CTRL_BLOCK_DEFAULTS 0x006A
#define MBOX_CMD_GET_SYS_INFO			0x0078
#define MBOX_CMD_GET_NVRAM			0x0078	
#define MBOX_CMD_SET_NVRAM			0x0079	
#define MBOX_CMD_RESTORE_FACTORY_DEFAULTS	0x0087
#define MBOX_CMD_SET_ACB			0x0088
#define MBOX_CMD_GET_ACB			0x0089
#define MBOX_CMD_DISABLE_ACB			0x008A
#define MBOX_CMD_GET_IPV6_NEIGHBOR_CACHE	0x008B
#define MBOX_CMD_GET_IPV6_DEST_CACHE		0x008C
#define MBOX_CMD_GET_IPV6_DEF_ROUTER_LIST	0x008D
#define MBOX_CMD_GET_IPV6_LCL_PREFIX_LIST	0x008E
#define MBOX_CMD_SET_IPV6_NEIGHBOR_CACHE	0x0090
#define MBOX_CMD_GET_IP_ADDR_STATE		0x0091
#define MBOX_CMD_SEND_IPV6_ROUTER_SOL		0x0092
#define MBOX_CMD_GET_DB_ENTRY_CURRENT_IP_ADDR	0x0093

#define FW_STATE_READY				0x0000
#define FW_STATE_CONFIG_WAIT			0x0001
#define FW_STATE_WAIT_AUTOCONNECT		0x0002
#define FW_STATE_ERROR				0x0004
#define FW_STATE_CONFIGURING_IP			0x0008

#define FW_ADDSTATE_OPTICAL_MEDIA		0x0001
#define FW_ADDSTATE_DHCPv4_ENABLED		0x0002
#define FW_ADDSTATE_DHCPv4_LEASE_ACQUIRED	0x0004
#define FW_ADDSTATE_DHCPv4_LEASE_EXPIRED	0x0008
#define FW_ADDSTATE_LINK_UP			0x0010
#define FW_ADDSTATE_ISNS_SVC_ENABLED		0x0020
#define FW_ADDSTATE_LINK_SPEED_10MBPS		0x0100
#define FW_ADDSTATE_LINK_SPEED_100MBPS		0x0200
#define FW_ADDSTATE_LINK_SPEED_1GBPS		0x0400
#define FW_ADDSTATE_LINK_SPEED_10GBPS		0x0800

#define MBOX_CMD_GET_DATABASE_ENTRY_DEFAULTS	0x006B
#define IPV6_DEFAULT_DDB_ENTRY			0x0001

#define MBOX_CMD_CONN_OPEN_SESS_LOGIN		0x0074
#define MBOX_CMD_GET_CRASH_RECORD		0x0076	
#define MBOX_CMD_GET_CONN_EVENT_LOG		0x0077

#define MBOX_COMPLETION_STATUS			4
#define MBOX_STS_BUSY				0x0007
#define MBOX_STS_INTERMEDIATE_COMPLETION	0x1000
#define MBOX_STS_COMMAND_COMPLETE		0x4000
#define MBOX_STS_COMMAND_ERROR			0x4005

#define MBOX_ASYNC_EVENT_STATUS			8
#define MBOX_ASTS_SYSTEM_ERROR			0x8002
#define MBOX_ASTS_REQUEST_TRANSFER_ERROR	0x8003
#define MBOX_ASTS_RESPONSE_TRANSFER_ERROR	0x8004
#define MBOX_ASTS_PROTOCOL_STATISTIC_ALARM	0x8005
#define MBOX_ASTS_SCSI_COMMAND_PDU_REJECTED	0x8006
#define MBOX_ASTS_LINK_UP			0x8010
#define MBOX_ASTS_LINK_DOWN			0x8011
#define MBOX_ASTS_DATABASE_CHANGED		0x8014
#define MBOX_ASTS_UNSOLICITED_PDU_RECEIVED	0x8015
#define MBOX_ASTS_SELF_TEST_FAILED		0x8016
#define MBOX_ASTS_LOGIN_FAILED			0x8017
#define MBOX_ASTS_DNS				0x8018
#define MBOX_ASTS_HEARTBEAT			0x8019
#define MBOX_ASTS_NVRAM_INVALID			0x801A
#define MBOX_ASTS_MAC_ADDRESS_CHANGED		0x801B
#define MBOX_ASTS_IP_ADDRESS_CHANGED		0x801C
#define MBOX_ASTS_DHCP_LEASE_EXPIRED		0x801D
#define MBOX_ASTS_DHCP_LEASE_ACQUIRED		0x801F
#define MBOX_ASTS_ISNS_UNSOLICITED_PDU_RECEIVED 0x8021
#define MBOX_ASTS_DUPLICATE_IP			0x8025
#define MBOX_ASTS_ARP_COMPLETE			0x8026
#define MBOX_ASTS_SUBNET_STATE_CHANGE		0x8027
#define MBOX_ASTS_RESPONSE_QUEUE_FULL		0x8028
#define MBOX_ASTS_IP_ADDR_STATE_CHANGED		0x8029
#define MBOX_ASTS_IPV6_PREFIX_EXPIRED		0x802B
#define MBOX_ASTS_IPV6_ND_PREFIX_IGNORED	0x802C
#define MBOX_ASTS_IPV6_LCL_PREFIX_IGNORED	0x802D
#define MBOX_ASTS_ICMPV6_ERROR_MSG_RCVD		0x802E
#define MBOX_ASTS_TXSCVR_INSERTED		0x8130
#define MBOX_ASTS_TXSCVR_REMOVED		0x8131

#define ISNS_EVENT_DATA_RECEIVED		0x0000
#define ISNS_EVENT_CONNECTION_OPENED		0x0001
#define ISNS_EVENT_CONNECTION_FAILED		0x0002
#define MBOX_ASTS_IPSEC_SYSTEM_FATAL_ERROR	0x8022
#define MBOX_ASTS_SUBNET_STATE_CHANGE		0x8027

#define ACB_STATE_UNCONFIGURED	0x00
#define ACB_STATE_INVALID	0x01
#define ACB_STATE_ACQUIRING	0x02
#define ACB_STATE_TENTATIVE	0x03
#define ACB_STATE_DEPRICATED	0x04
#define ACB_STATE_VALID		0x05
#define ACB_STATE_DISABLING	0x06

#define FLASH_SEGMENT_IFCB	0x04000000

#define FLASH_OPT_RMW_HOLD	0
#define FLASH_OPT_RMW_INIT	1
#define FLASH_OPT_COMMIT	2
#define FLASH_OPT_RMW_COMMIT	3


struct addr_ctrl_blk {
	uint8_t version;	
#define  IFCB_VER_MIN			0x01
#define  IFCB_VER_MAX			0x02
	uint8_t control;	

	uint16_t fw_options;	
#define	 FWOPT_HEARTBEAT_ENABLE		  0x1000
#define	 FWOPT_SESSION_MODE		  0x0040
#define	 FWOPT_INITIATOR_MODE		  0x0020
#define	 FWOPT_TARGET_MODE		  0x0010
#define	 FWOPT_ENABLE_CRBDB		  0x8000

	uint16_t exec_throttle;	
	uint8_t zio_count;	
	uint8_t res0;	
	uint16_t eth_mtu_size;	
	uint16_t add_fw_options;	
#define ADFWOPT_SERIALIZE_TASK_MGMT	0x0400
#define ADFWOPT_AUTOCONN_DISABLE	0x0002

	uint8_t hb_interval;	
	uint8_t inst_num; 
	uint16_t res1;		
	uint16_t rqq_consumer_idx;	
	uint16_t compq_producer_idx;	
	uint16_t rqq_len;	
	uint16_t compq_len;	
	uint32_t rqq_addr_lo;	
	uint32_t rqq_addr_hi;	
	uint32_t compq_addr_lo;	
	uint32_t compq_addr_hi;	
	uint32_t shdwreg_addr_lo;	
	uint32_t shdwreg_addr_hi;	

	uint16_t iscsi_opts;	
	uint16_t ipv4_tcp_opts;	
#define TCPOPT_DHCP_ENABLE		0x0200
	uint16_t ipv4_ip_opts;	
#define IPOPT_IPV4_PROTOCOL_ENABLE	0x8000
#define IPOPT_VLAN_TAGGING_ENABLE	0x2000

	uint16_t iscsi_max_pdu_size;	
	uint8_t ipv4_tos;	
	uint8_t ipv4_ttl;	
	uint8_t acb_version;	
#define ACB_NOT_SUPPORTED		0x00
#define ACB_SUPPORTED			0x02 

	uint8_t res2;	
	uint16_t def_timeout;	
	uint16_t iscsi_fburst_len;	
	uint16_t iscsi_def_time2wait;	
	uint16_t iscsi_def_time2retain;	
	uint16_t iscsi_max_outstnd_r2t;	
	uint16_t conn_ka_timeout;	
	uint16_t ipv4_port;	
	uint16_t iscsi_max_burst_len;	
	uint32_t res5;		
	uint8_t ipv4_addr[4];	
	uint16_t ipv4_vlan_tag;	
	uint8_t ipv4_addr_state;	
	uint8_t ipv4_cacheid;	
	uint8_t res6[8];	
	uint8_t ipv4_subnet[4];	
	uint8_t res7[12];	
	uint8_t ipv4_gw_addr[4];	
	uint8_t res8[0xc];	
	uint8_t pri_dns_srvr_ip[4];
	uint8_t sec_dns_srvr_ip[4];
	uint16_t min_eph_port;	
	uint16_t max_eph_port;	
	uint8_t res9[4];	
	uint8_t iscsi_alias[32];
	uint8_t res9_1[0x16];	
	uint16_t tgt_portal_grp;
	uint8_t abort_timer;	
	uint8_t ipv4_tcp_wsf;	
	uint8_t res10[6];	
	uint8_t ipv4_sec_ip_addr[4];	
	uint8_t ipv4_dhcp_vid_len;	
	uint8_t ipv4_dhcp_vid[11];	
	uint8_t res11[20];	
	uint8_t ipv4_dhcp_alt_cid_len;	
	uint8_t ipv4_dhcp_alt_cid[11];	
	uint8_t iscsi_name[224];	
	uint8_t res12[32];	
	uint32_t cookie;	
	uint16_t ipv6_port;	
	uint16_t ipv6_opts;	
#define IPV6_OPT_IPV6_PROTOCOL_ENABLE	0x8000
#define IPV6_OPT_VLAN_TAGGING_ENABLE	0x2000

	uint16_t ipv6_addtl_opts;	
#define IPV6_ADDOPT_NEIGHBOR_DISCOVERY_ADDR_ENABLE	0x0002 
#define IPV6_ADDOPT_AUTOCONFIG_LINK_LOCAL_ADDR		0x0001

	uint16_t ipv6_tcp_opts;	
	uint8_t ipv6_tcp_wsf;	
	uint16_t ipv6_flow_lbl;	
	uint8_t ipv6_dflt_rtr_addr[16]; 
	uint16_t ipv6_vlan_tag;	
	uint8_t ipv6_lnk_lcl_addr_state;
	uint8_t ipv6_addr0_state;	
	uint8_t ipv6_addr1_state;	
#define IP_ADDRSTATE_UNCONFIGURED	0
#define IP_ADDRSTATE_INVALID		1
#define IP_ADDRSTATE_ACQUIRING		2
#define IP_ADDRSTATE_TENTATIVE		3
#define IP_ADDRSTATE_DEPRICATED		4
#define IP_ADDRSTATE_PREFERRED		5
#define IP_ADDRSTATE_DISABLING		6

	uint8_t ipv6_dflt_rtr_state;    
#define IPV6_RTRSTATE_UNKNOWN                   0
#define IPV6_RTRSTATE_MANUAL                    1
#define IPV6_RTRSTATE_ADVERTISED                3
#define IPV6_RTRSTATE_STALE                     4

	uint8_t ipv6_traffic_class;	
	uint8_t ipv6_hop_limit;	
	uint8_t ipv6_if_id[8];	
	uint8_t ipv6_addr0[16];	
	uint8_t ipv6_addr1[16];	
	uint32_t ipv6_nd_reach_time;	
	uint32_t ipv6_nd_rexmit_timer;	
	uint32_t ipv6_nd_stale_timeout;	
	uint8_t ipv6_dup_addr_detect_count;	
	uint8_t ipv6_cache_id;	
	uint8_t res13[18];	
	uint32_t ipv6_gw_advrt_mtu;	
	uint8_t res14[140];	
};

#define IP_ADDR_COUNT	4 

#define IP_STATE_MASK	0x0F000000
#define IP_STATE_SHIFT	24

struct init_fw_ctrl_blk {
	struct addr_ctrl_blk pri;
};

#define PRIMARI_ACB		0
#define SECONDARY_ACB		1

struct addr_ctrl_blk_def {
	uint8_t reserved1[1];	
	uint8_t control;	
	uint8_t reserved2[11];	
	uint8_t inst_num;	
	uint8_t reserved3[34];	
	uint16_t iscsi_opts;	
	uint16_t ipv4_tcp_opts;	
	uint16_t ipv4_ip_opts;	
	uint16_t iscsi_max_pdu_size;	
	uint8_t ipv4_tos;	
	uint8_t ipv4_ttl;	
	uint8_t reserved4[2];	
	uint16_t def_timeout;	
	uint16_t iscsi_fburst_len;	
	uint8_t reserved5[4];	
	uint16_t iscsi_max_outstnd_r2t;	
	uint8_t reserved6[2];	
	uint16_t ipv4_port;	
	uint16_t iscsi_max_burst_len;	
	uint8_t reserved7[4];	
	uint8_t ipv4_addr[4];	
	uint16_t ipv4_vlan_tag;	
	uint8_t ipv4_addr_state;	
	uint8_t ipv4_cacheid;	
	uint8_t reserved8[8];	
	uint8_t ipv4_subnet[4];	
	uint8_t reserved9[12];	
	uint8_t ipv4_gw_addr[4];	
	uint8_t reserved10[84];	
	uint8_t abort_timer;	
	uint8_t ipv4_tcp_wsf;	
	uint8_t reserved11[10];	
	uint8_t ipv4_dhcp_vid_len;	
	uint8_t ipv4_dhcp_vid[11];	
	uint8_t reserved12[20];	
	uint8_t ipv4_dhcp_alt_cid_len;	
	uint8_t ipv4_dhcp_alt_cid[11];	
	uint8_t iscsi_name[224];	
	uint8_t reserved13[32];	
	uint32_t cookie;	
	uint16_t ipv6_port;	
	uint16_t ipv6_opts;	
	uint16_t ipv6_addtl_opts;	
	uint16_t ipv6_tcp_opts;		
	uint8_t ipv6_tcp_wsf;		
	uint16_t ipv6_flow_lbl;		
	uint8_t ipv6_dflt_rtr_addr[16];	
	uint16_t ipv6_vlan_tag;		
	uint8_t ipv6_lnk_lcl_addr_state;	
	uint8_t ipv6_addr0_state;	
	uint8_t ipv6_addr1_state;	
	uint8_t ipv6_dflt_rtr_state;	
	uint8_t ipv6_traffic_class;	
	uint8_t ipv6_hop_limit;		
	uint8_t ipv6_if_id[8];		
	uint8_t ipv6_addr0[16];		
	uint8_t ipv6_addr1[16];		
	uint32_t ipv6_nd_reach_time;	
	uint32_t ipv6_nd_rexmit_timer;	
	uint32_t ipv6_nd_stale_timeout;	
	uint8_t ipv6_dup_addr_detect_count;	
	uint8_t ipv6_cache_id;		
	uint8_t reserved14[18];		
	uint32_t ipv6_gw_advrt_mtu;	
	uint8_t reserved15[140];	
};


#define MAX_CHAP_ENTRIES_40XX	128
#define MAX_CHAP_ENTRIES_82XX	1024
#define MAX_RESRV_CHAP_IDX	3
#define FLASH_CHAP_OFFSET	0x06000000

struct ql4_chap_table {
	uint16_t link;
	uint8_t flags;
	uint8_t secret_len;
#define MIN_CHAP_SECRET_LEN	12
#define MAX_CHAP_SECRET_LEN	100
	uint8_t secret[MAX_CHAP_SECRET_LEN];
#define MAX_CHAP_NAME_LEN	256
	uint8_t name[MAX_CHAP_NAME_LEN];
	uint16_t reserved;
#define CHAP_VALID_COOKIE	0x4092
#define CHAP_INVALID_COOKIE	0xFFEE
	uint16_t cookie;
};

struct dev_db_entry {
	uint16_t options;	
#define DDB_OPT_DISC_SESSION  0x10
#define DDB_OPT_TARGET	      0x02 
#define DDB_OPT_IPV6_DEVICE	0x100
#define DDB_OPT_AUTO_SENDTGTS_DISABLE		0x40
#define DDB_OPT_IPV6_NULL_LINK_LOCAL		0x800 
#define DDB_OPT_IPV6_FW_DEFINED_LINK_LOCAL	0x800 

	uint16_t exec_throttle;	
	uint16_t exec_count;	
	uint16_t res0;	
	uint16_t iscsi_options;	
	uint16_t tcp_options;	
	uint16_t ip_options;	
	uint16_t iscsi_max_rcv_data_seg_len;	
#define BYTE_UNITS	512
	uint32_t res1;	
	uint16_t iscsi_max_snd_data_seg_len;	
	uint16_t iscsi_first_burst_len;	
	uint16_t iscsi_def_time2wait;	
	uint16_t iscsi_def_time2retain;	
	uint16_t iscsi_max_outsnd_r2t;	
	uint16_t ka_timeout;	
	uint8_t isid[6];	
	uint16_t tsid;		
	uint16_t port;	
	uint16_t iscsi_max_burst_len;	
	uint16_t def_timeout;	
	uint16_t res2;	
	uint8_t ip_addr[0x10];	
	uint8_t iscsi_alias[0x20];	
	uint8_t tgt_addr[0x20];	
	uint16_t mss;	
	uint16_t res3;	
	uint16_t lcl_port;	
	uint8_t ipv4_tos;	
	uint16_t ipv6_flow_lbl;	
	uint8_t res4[0x36];	
	uint8_t iscsi_name[0xE0];	
	uint8_t link_local_ipv6_addr[0x10]; 
	uint8_t res5[0x10];	
	uint16_t ddb_link;	
	uint16_t chap_tbl_idx;	
	uint16_t tgt_portal_grp; 
	uint8_t tcp_xmt_wsf;	
	uint8_t tcp_rcv_wsf;	
	uint32_t stat_sn;	
	uint32_t exp_stat_sn;	
	uint8_t res6[0x2b];	
#define DDB_VALID_COOKIE	0x9034
	uint16_t cookie;	
	uint16_t len;		
};



#define FLASH_OFFSET_SYS_INFO	0x02000000
#define FLASH_DEFAULTBLOCKSIZE	0x20000
#define FLASH_EOF_OFFSET	(FLASH_DEFAULTBLOCKSIZE-8) 
#define FLASH_RAW_ACCESS_ADDR	0x8e000000

#define BOOT_PARAM_OFFSET_PORT0 0x3b0
#define BOOT_PARAM_OFFSET_PORT1 0x7b0

#define FLASH_OFFSET_DB_INFO	0x05000000
#define FLASH_OFFSET_DB_END	(FLASH_OFFSET_DB_INFO + 0x7fff)


struct sys_info_phys_addr {
	uint8_t address[6];	
	uint8_t filler[2];	
};

struct flash_sys_info {
	uint32_t cookie;	
	uint32_t physAddrCount; 
	struct sys_info_phys_addr physAddr[4]; 
	uint8_t vendorId[128];	
	uint8_t productId[128]; 
	uint32_t serialNumber;	

	
	uint32_t pciDeviceVendor;	
	uint32_t pciDeviceId;	
	uint32_t pciSubsysVendor;	
	uint32_t pciSubsysId;	

	
	uint32_t crumbs;	

	uint32_t enterpriseNumber;	

	uint32_t mtu;		
	uint32_t reserved0;	
	uint32_t crumbs2;	
	uint8_t acSerialNumber[16];	
	uint32_t crumbs3;	

	uint32_t reserved1[39]; 
};	

struct mbx_sys_info {
	uint8_t board_id_str[16];   
				
	uint16_t board_id;	
	uint16_t phys_port_cnt;	
	uint16_t port_num;	
				
	uint8_t mac_addr[6];	
	uint32_t iscsi_pci_func_cnt;  
	uint32_t pci_func;	      
	unsigned char serial_number[16];  
	uint8_t reserved[12];		  
};

struct about_fw_info {
	uint16_t fw_major;		
	uint16_t fw_minor;		
	uint16_t fw_patch;		
	uint16_t fw_build;		
	uint8_t fw_build_date[16];	
	uint8_t fw_build_time[16];	
	uint8_t fw_build_user[16];	
	uint16_t fw_load_source;	
	uint8_t reserved1[6];		
	uint16_t iscsi_major;		
	uint16_t iscsi_minor;		
	uint16_t bootload_major;	
	uint16_t bootload_minor;	
	uint16_t bootload_patch;	
	uint16_t bootload_build;	
	uint8_t reserved2[180];		
};

struct crash_record {
	uint16_t fw_major_version;	
	uint16_t fw_minor_version;	
	uint16_t fw_patch_version;	
	uint16_t fw_build_version;	

	uint8_t build_date[16]; 
	uint8_t build_time[16]; 
	uint8_t build_user[16]; 
	uint8_t card_serial_num[16];	

	uint32_t time_of_crash_in_secs; 
	uint32_t time_of_crash_in_ms;	

	uint16_t out_RISC_sd_num_frames;	
	uint16_t OAP_sd_num_words;	
	uint16_t IAP_sd_num_frames;	
	uint16_t in_RISC_sd_num_words;	

	uint8_t reserved1[28];	

	uint8_t out_RISC_reg_dump[256]; 
	uint8_t in_RISC_reg_dump[256];	
	uint8_t in_out_RISC_stack_dump[0];	
};

struct conn_event_log_entry {
#define MAX_CONN_EVENT_LOG_ENTRIES	100
	uint32_t timestamp_sec; 
	uint32_t timestamp_ms;	
	uint16_t device_index;	
	uint16_t fw_conn_state; 
	uint8_t event_type;	
	uint8_t error_code;	
	uint16_t error_code_detail;	
	uint8_t num_consecutive_events; 
	uint8_t rsvd[3];	
};

#define IOCB_MAX_CDB_LEN	    16	
#define IOCB_MAX_SENSEDATA_LEN	    32	
#define IOCB_MAX_EXT_SENSEDATA_LEN  60  

struct qla4_header {
	uint8_t entryType;
#define ET_STATUS		 0x03
#define ET_MARKER		 0x04
#define ET_CONT_T1		 0x0A
#define ET_STATUS_CONTINUATION	 0x10
#define ET_CMND_T3		 0x19
#define ET_PASSTHRU0		 0x3A
#define ET_PASSTHRU_STATUS	 0x3C
#define ET_MBOX_CMD		0x38
#define ET_MBOX_STATUS		0x39

	uint8_t entryStatus;
	uint8_t systemDefined;
#define SD_ISCSI_PDU	0x01
	uint8_t entryCount;

	
};

struct queue_entry {
	uint8_t data[60];
	uint32_t signature;

};


#define COMMAND_SEG_A64	  1
#define CONTINUE_SEG_A64  5


struct data_seg_a64 {
	struct {
		uint32_t addrLow;
		uint32_t addrHigh;

	} base;

	uint32_t count;

};


struct command_t3_entry {
	struct qla4_header hdr;	

	uint32_t handle;	
	uint16_t target;	
	uint16_t connection_id; 

	uint8_t control_flags;	

	
#define CF_WRITE		0x20
#define CF_READ			0x40
#define CF_NO_DATA		0x00

	
#define CF_HEAD_TAG		0x03
#define CF_ORDERED_TAG		0x02
#define CF_SIMPLE_TAG		0x01

	uint8_t state_flags;	
	uint8_t cmdRefNum;	
	uint8_t reserved1;	
	uint8_t cdb[IOCB_MAX_CDB_LEN];	
	struct scsi_lun lun;	
	uint32_t cmdSeqNum;	
	uint16_t timeout;	
	uint16_t dataSegCnt;	
	uint32_t ttlByteCnt;	
	struct data_seg_a64 dataseg[COMMAND_SEG_A64];	

};


struct continuation_t1_entry {
	struct qla4_header hdr;

	struct data_seg_a64 dataseg[CONTINUE_SEG_A64];

};

#define COMMAND_SEG	COMMAND_SEG_A64
#define CONTINUE_SEG	CONTINUE_SEG_A64

#define ET_COMMAND	ET_CMND_T3
#define ET_CONTINUE	ET_CONT_T1

struct qla4_marker_entry {
	struct qla4_header hdr;	

	uint32_t system_defined; 
	uint16_t target;	
	uint16_t modifier;	
#define MM_LUN_RESET		0
#define MM_TGT_WARM_RESET	1

	uint16_t flags;		
	uint16_t reserved1;	
	struct scsi_lun lun;	
	uint64_t reserved2;	
	uint64_t reserved3;	
	uint64_t reserved4;	
	uint64_t reserved5;	
	uint64_t reserved6;	
};

struct status_entry {
	struct qla4_header hdr;	

	uint32_t handle;	

	uint8_t scsiStatus;	
#define SCSI_CHECK_CONDITION		  0x02

	uint8_t iscsiFlags;	
#define ISCSI_FLAG_RESIDUAL_UNDER	  0x02
#define ISCSI_FLAG_RESIDUAL_OVER	  0x04

	uint8_t iscsiResponse;	

	uint8_t completionStatus;	
#define SCS_COMPLETE			  0x00
#define SCS_INCOMPLETE			  0x01
#define SCS_RESET_OCCURRED		  0x04
#define SCS_ABORTED			  0x05
#define SCS_TIMEOUT			  0x06
#define SCS_DATA_OVERRUN		  0x07
#define SCS_DATA_UNDERRUN		  0x15
#define SCS_QUEUE_FULL			  0x1C
#define SCS_DEVICE_UNAVAILABLE		  0x28
#define SCS_DEVICE_LOGGED_OUT		  0x29

	uint8_t reserved1;	

	uint8_t state_flags;	

	uint16_t senseDataByteCnt;	
	uint32_t residualByteCnt;	
	uint32_t bidiResidualByteCnt;	
	uint32_t expSeqNum;	
	uint32_t maxCmdSeqNum;	
	uint8_t senseData[IOCB_MAX_SENSEDATA_LEN];	

};

struct status_cont_entry {
       struct qla4_header hdr; 
       uint8_t ext_sense_data[IOCB_MAX_EXT_SENSEDATA_LEN]; 
};

struct passthru0 {
	struct qla4_header hdr;		       
	uint32_t handle;	
	uint16_t target;	
	uint16_t connection_id;	
#define ISNS_DEFAULT_SERVER_CONN_ID	((uint16_t)0x8000)

	uint16_t control_flags;	
#define PT_FLAG_ETHERNET_FRAME		0x8000
#define PT_FLAG_ISNS_PDU		0x8000
#define PT_FLAG_SEND_BUFFER		0x0200
#define PT_FLAG_WAIT_4_RESPONSE		0x0100
#define PT_FLAG_ISCSI_PDU		0x1000

	uint16_t timeout;	
#define PT_DEFAULT_TIMEOUT		30 

	struct data_seg_a64 out_dsd;    
	uint32_t res1;		
	struct data_seg_a64 in_dsd;     
	uint8_t res2[20];	
};

struct passthru_status {
	struct qla4_header hdr;		       
	uint32_t handle;	
	uint16_t target;	
	uint16_t connectionID;	

	uint8_t completionStatus;	
#define PASSTHRU_STATUS_COMPLETE		0x01

	uint8_t residualFlags;	

	uint16_t timeout;	
	uint16_t portNumber;	
	uint8_t res1[10];	
	uint32_t outResidual;	
	uint8_t res2[12];	
	uint32_t inResidual;	
	uint8_t res4[16];	
};

struct mbox_cmd_iocb {
	struct qla4_header hdr;	
	uint32_t handle;	
	uint32_t in_mbox[8];	
	uint32_t res1[6];	
};

struct mbox_status_iocb {
	struct qla4_header hdr;	
	uint32_t handle;	
	uint32_t out_mbox[8];	
	uint32_t res1[6];	
};

struct response {
	uint8_t data[60];
	uint32_t signature;
#define RESPONSE_PROCESSED	0xDEADDEAD	
};

struct ql_iscsi_stats {
	uint8_t reserved1[656]; 
	uint32_t tx_cmd_pdu; 
	uint32_t tx_resp_pdu; 
	uint32_t rx_cmd_pdu; 
	uint32_t rx_resp_pdu; 

	uint64_t tx_data_octets; 
	uint64_t rx_data_octets; 

	uint32_t hdr_digest_err; 
	uint32_t data_digest_err; 
	uint32_t conn_timeout_err; 
	uint32_t framing_err; 

	uint32_t tx_nopout_pdus; 
	uint32_t tx_scsi_cmd_pdus;  
	uint32_t tx_tmf_cmd_pdus; 
	uint32_t tx_login_cmd_pdus; 
	uint32_t tx_text_cmd_pdus; 
	uint32_t tx_scsi_write_pdus; 
	uint32_t tx_logout_cmd_pdus; 
	uint32_t tx_snack_req_pdus; 

	uint32_t rx_nopin_pdus; 
	uint32_t rx_scsi_resp_pdus; 
	uint32_t rx_tmf_resp_pdus; 
	uint32_t rx_login_resp_pdus; 
	uint32_t rx_text_resp_pdus; 
	uint32_t rx_scsi_read_pdus; 
	uint32_t rx_logout_resp_pdus; 

	uint32_t rx_r2t_pdus; 
	uint32_t rx_async_pdus; 
	uint32_t rx_reject_pdus; 

	uint8_t reserved2[264]; 
};

#endif 
