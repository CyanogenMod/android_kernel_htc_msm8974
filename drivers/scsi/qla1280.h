/******************************************************************************
*                  QLOGIC LINUX SOFTWARE
*
* QLogic ISP1280 (Ultra2) /12160 (Ultra3) SCSI driver
* Copyright (C) 2000 Qlogic Corporation
* (www.qlogic.com)
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2, or (at your option) any
* later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
******************************************************************************/

#ifndef	_QLA1280_H
#define	_QLA1280_H

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

#if MEMORY_MAPPED_IO
#define RD_REG_WORD(addr)		readw_relaxed(addr)
#define RD_REG_WORD_dmasync(addr)	readw(addr)
#define WRT_REG_WORD(addr, data)	writew(data, addr)
#else				
#define RD_REG_WORD(addr)		inw((unsigned long)addr)
#define RD_REG_WORD_dmasync(addr)	RD_REG_WORD(addr)
#define WRT_REG_WORD(addr, data)	outw(data, (unsigned long)addr)
#endif				

#define MAX_BUSES	2	
#define MAX_B_BITS	1

#define MAX_TARGETS	16	
#define MAX_T_BITS	4	

#define MAX_LUNS	8	
#define MAX_L_BITS	3	

#define QLA1280_WDG_TIME_QUANTUM	5	

#define COMMAND_RETRY_COUNT		255

#define MAX_OUTSTANDING_COMMANDS	512
#define COMPLETED_HANDLE		((unsigned char *) \
					(MAX_OUTSTANDING_COMMANDS + 2))

#define REQUEST_ENTRY_CNT		255 
#define RESPONSE_ENTRY_CNT		63  

struct srb {
	struct list_head list;		
	struct scsi_cmnd *cmd;	
	struct completion *wait;
	dma_addr_t saved_dma_handle;	
	uint8_t flags;		
	uint8_t dir;		
};

#define SRB_TIMEOUT		(1 << 0)	
#define SRB_SENT		(1 << 1)	
#define SRB_ABORT_PENDING	(1 << 2)	
#define SRB_ABORTED		(1 << 3)	

struct device_reg {
	uint16_t id_l;		
	uint16_t id_h;		
	uint16_t cfg_0;		
#define ISP_CFG0_HWMSK   0x000f	
#define ISP_CFG0_1020    BIT_0	
#define ISP_CFG0_1020A	 BIT_1	
#define ISP_CFG0_1040	 BIT_2	
#define ISP_CFG0_1040A	 BIT_3	
#define ISP_CFG0_1040B	 BIT_4	
#define ISP_CFG0_1040C	 BIT_5	
	uint16_t cfg_1;		
#define ISP_CFG1_F128    BIT_6  
#define ISP_CFG1_F64     BIT_4|BIT_5 
#define ISP_CFG1_F32     BIT_5  
#define ISP_CFG1_F16     BIT_4  
#define ISP_CFG1_BENAB   BIT_2  
#define ISP_CFG1_SXP     BIT_0  
	uint16_t ictrl;		
#define ISP_RESET        BIT_0	
#define ISP_EN_INT       BIT_1	
#define ISP_EN_RISC      BIT_2	
#define ISP_FLASH_ENABLE BIT_8	
#define ISP_FLASH_UPPER  BIT_9	
	uint16_t istatus;	
#define PCI_64BIT_SLOT   BIT_14	
#define RISC_INT         BIT_2	
#define PCI_INT          BIT_1	
	uint16_t semaphore;	
	uint16_t nvram;		
#define NV_DESELECT     0
#define NV_CLOCK        BIT_0
#define NV_SELECT       BIT_1
#define NV_DATA_OUT     BIT_2
#define NV_DATA_IN      BIT_3
	uint16_t flash_data;	
	uint16_t flash_address;	

	uint16_t unused_1[0x06];
	
	
	uint16_t cdma_cfg;
#define CDMA_CONF_SENAB  BIT_3	
#define CDMA_CONF_RIRQ   BIT_2	
#define CDMA_CONF_BENAB  BIT_1	
#define CDMA_CONF_DIR    BIT_0	
	uint16_t cdma_ctrl; 
	uint16_t cdma_status;   
	uint16_t cdma_fifo_status;
	uint16_t cdma_count;
	uint16_t cdma_reserved;
	uint16_t cdma_address_count_0;
	uint16_t cdma_address_count_1;
	uint16_t cdma_address_count_2;
	uint16_t cdma_address_count_3;

	uint16_t unused_2[0x06];

	uint16_t ddma_cfg;
#define DDMA_CONF_SENAB  BIT_3	
#define DDMA_CONF_RIRQ   BIT_2	
#define DDMA_CONF_BENAB  BIT_1	
#define DDMA_CONF_DIR    BIT_0	
	uint16_t ddma_ctrl;
	uint16_t ddma_status; 
	uint16_t ddma_fifo_status;
	uint16_t ddma_xfer_count_low;
	uint16_t ddma_xfer_count_high;
	uint16_t ddma_addr_count_0;
	uint16_t ddma_addr_count_1;
	uint16_t ddma_addr_count_2;
	uint16_t ddma_addr_count_3; 

	uint16_t unused_3[0x0e];

	uint16_t mailbox0;	
	uint16_t mailbox1;	
	uint16_t mailbox2;	
	uint16_t mailbox3;	
	uint16_t mailbox4;	
	uint16_t mailbox5;	
	uint16_t mailbox6;	
	uint16_t mailbox7;	

	uint16_t unused_4[0x20];

	uint16_t host_cmd;	
#define HOST_INT      BIT_7	
#define BIOS_ENABLE   BIT_0

	uint16_t unused_5[0x5];	

	uint16_t gpio_data;
	uint16_t gpio_enable;

	uint16_t unused_6[0x11];	
	uint16_t scsiControlPins;	
};

#define MAILBOX_REGISTER_COUNT	8

#define PROD_ID_1		0x4953
#define PROD_ID_2		0x0000
#define PROD_ID_2a		0x5020
#define PROD_ID_3		0x2020
#define PROD_ID_4		0x1

#define HC_RESET_RISC		0x1000	
#define HC_PAUSE_RISC		0x2000	
#define HC_RELEASE_RISC		0x3000	
#define HC_SET_HOST_INT		0x5000	
#define HC_CLR_HOST_INT		0x6000	
#define HC_CLR_RISC_INT		0x7000	
#define HC_DISABLE_BIOS		0x9000	

#define MBS_FRM_ALIVE		0	
#define MBS_CHKSUM_ERR		1	
#define MBS_SHADOW_LD_ERR	2	
#define MBS_BUSY		4	

#define MBS_CMD_CMP		0x4000	
#define MBS_INV_CMD		0x4001	
#define MBS_HOST_INF_ERR	0x4002	
#define MBS_TEST_FAILED		0x4003	
#define MBS_CMD_ERR		0x4005	
#define MBS_CMD_PARAM_ERR	0x4006	

#define MBA_ASYNC_EVENT		0x8000	
#define MBA_BUS_RESET		0x8001	
#define MBA_SYSTEM_ERR		0x8002	
#define MBA_REQ_TRANSFER_ERR	0x8003	
#define MBA_RSP_TRANSFER_ERR	0x8004	
#define MBA_WAKEUP_THRES	0x8005	
#define MBA_TIMEOUT_RESET	0x8006	
#define MBA_DEVICE_RESET	0x8007	
#define MBA_BUS_MODE_CHANGE	0x800E	
#define MBA_SCSI_COMPLETION	0x8020	

#define MBC_NOP				0	
#define MBC_LOAD_RAM			1	
#define MBC_EXECUTE_FIRMWARE		2	
#define MBC_DUMP_RAM			3	
#define MBC_WRITE_RAM_WORD		4	
#define MBC_READ_RAM_WORD		5	
#define MBC_MAILBOX_REGISTER_TEST	6	
#define MBC_VERIFY_CHECKSUM		7	
#define MBC_ABOUT_FIRMWARE		8	
#define MBC_INIT_REQUEST_QUEUE		0x10	
#define MBC_INIT_RESPONSE_QUEUE		0x11	
#define MBC_EXECUTE_IOCB		0x12	
#define MBC_ABORT_COMMAND		0x15	
#define MBC_ABORT_DEVICE		0x16	
#define MBC_ABORT_TARGET		0x17	
#define MBC_BUS_RESET			0x18	
#define MBC_GET_RETRY_COUNT		0x22	
#define MBC_GET_TARGET_PARAMETERS	0x28	
#define MBC_SET_INITIATOR_ID		0x30	
#define MBC_SET_SELECTION_TIMEOUT	0x31	
#define MBC_SET_RETRY_COUNT		0x32	
#define MBC_SET_TAG_AGE_LIMIT		0x33	
#define MBC_SET_CLOCK_RATE		0x34	
#define MBC_SET_ACTIVE_NEGATION		0x35	
#define MBC_SET_ASYNC_DATA_SETUP	0x36	
#define MBC_SET_PCI_CONTROL		0x37	
#define MBC_SET_TARGET_PARAMETERS	0x38	
#define MBC_SET_DEVICE_QUEUE		0x39	
#define MBC_SET_RESET_DELAY_PARAMETERS	0x3A	
#define MBC_SET_SYSTEM_PARAMETER	0x45	
#define MBC_SET_FIRMWARE_FEATURES	0x4A	
#define MBC_INIT_REQUEST_QUEUE_A64	0x52	
#define MBC_INIT_RESPONSE_QUEUE_A64	0x53	
#define MBC_ENABLE_TARGET_MODE		0x55	
#define MBC_SET_DATA_OVERRUN_RECOVERY	0x5A	

#define TP_PPR			BIT_5	
#define TP_RENEGOTIATE		BIT_8	
#define TP_STOP_QUEUE           BIT_9	
#define TP_AUTO_REQUEST_SENSE   BIT_10	
#define TP_TAGGED_QUEUE         BIT_11	
#define TP_SYNC                 BIT_12	
#define TP_WIDE                 BIT_13	
#define TP_PARITY               BIT_14	
#define TP_DISCONNECT           BIT_15	

#define NV_START_BIT		BIT_2
#define NV_WRITE_OP		(BIT_26 | BIT_24)
#define NV_READ_OP		(BIT_26 | BIT_25)
#define NV_ERASE_OP		(BIT_26 | BIT_25 | BIT_24)
#define NV_MASK_OP		(BIT_26 | BIT_25 | BIT_24)
#define NV_DELAY_COUNT		10

struct nvram {
	uint8_t id0;		
	uint8_t id1;		
	uint8_t id2;		
	uint8_t id3;		
	uint8_t version;	

	struct {
		uint8_t bios_configuration_mode:2;
		uint8_t bios_disable:1;
		uint8_t selectable_scsi_boot_enable:1;
		uint8_t cd_rom_boot_enable:1;
		uint8_t disable_loading_risc_code:1;
		uint8_t enable_64bit_addressing:1;
		uint8_t unused_7:1;
	} cntr_flags_1;		

	struct {
		uint8_t boot_lun_number:5;
		uint8_t scsi_bus_number:1;
		uint8_t unused_6:1;
		uint8_t unused_7:1;
	} cntr_flags_2l;	

	struct {
		uint8_t boot_target_number:4;
		uint8_t unused_12:1;
		uint8_t unused_13:1;
		uint8_t unused_14:1;
		uint8_t unused_15:1;
	} cntr_flags_2h;	

	uint16_t unused_8;	
	uint16_t unused_10;	
	uint16_t unused_12;	
	uint16_t unused_14;	

	struct {
		uint8_t reserved:2;
		uint8_t burst_enable:1;
		uint8_t reserved_1:1;
		uint8_t fifo_threshold:4;
	} isp_config;		

	struct {
		uint8_t scsi_bus_1_control:2;
		uint8_t scsi_bus_0_control:2;
		uint8_t unused_0:1;
		uint8_t unused_1:1;
		uint8_t unused_2:1;
		uint8_t auto_term_support:1;
	} termination;		

	uint16_t isp_parameter;	

	union {
		uint16_t w;
		struct {
			uint16_t enable_fast_posting:1;
			uint16_t report_lvd_bus_transition:1;
			uint16_t unused_2:1;
			uint16_t unused_3:1;
			uint16_t disable_iosbs_with_bus_reset_status:1;
			uint16_t disable_synchronous_backoff:1;
			uint16_t unused_6:1;
			uint16_t synchronous_backoff_reporting:1;
			uint16_t disable_reselection_fairness:1;
			uint16_t unused_9:1;
			uint16_t unused_10:1;
			uint16_t unused_11:1;
			uint16_t unused_12:1;
			uint16_t unused_13:1;
			uint16_t unused_14:1;
			uint16_t unused_15:1;
		} f;
	} firmware_feature;	

	uint16_t unused_22;	

	struct {
		struct {
			uint8_t initiator_id:4;
			uint8_t scsi_reset_disable:1;
			uint8_t scsi_bus_size:1;
			uint8_t scsi_bus_type:1;
			uint8_t unused_7:1;
		} config_1;	

		uint8_t bus_reset_delay;	
		uint8_t retry_count;	
		uint8_t retry_delay;	

		struct {
			uint8_t async_data_setup_time:4;
			uint8_t req_ack_active_negation:1;
			uint8_t data_line_active_negation:1;
			uint8_t unused_6:1;
			uint8_t unused_7:1;
		} config_2;	

		uint8_t unused_29;	

		uint16_t selection_timeout;	
		uint16_t max_queue_depth;	

		uint16_t unused_34;	
		uint16_t unused_36;	
		uint16_t unused_38;	

		struct {
			struct {
				uint8_t renegotiate_on_error:1;
				uint8_t stop_queue_on_check:1;
				uint8_t auto_request_sense:1;
				uint8_t tag_queuing:1;
				uint8_t enable_sync:1;
				uint8_t enable_wide:1;
				uint8_t parity_checking:1;
				uint8_t disconnect_allowed:1;
			} parameter;	

			uint8_t execution_throttle;	
			uint8_t sync_period;	

			union {		
				uint8_t flags_43;
				struct {
					uint8_t sync_offset:4;
					uint8_t device_enable:1;
					uint8_t lun_disable:1;
					uint8_t unused_6:1;
					uint8_t unused_7:1;
				} flags1x80;
				struct {
					uint8_t sync_offset:5;
					uint8_t device_enable:1;
					uint8_t unused_6:1;
					uint8_t unused_7:1;
				} flags1x160;
			} flags;
			union {	
				uint8_t unused_44;
				struct {
					uint8_t ppr_options:4;
					uint8_t ppr_bus_width:2;
					uint8_t unused_8:1;
					uint8_t enable_ppr:1;
				} flags;	
			} ppr_1x160;
			uint8_t unused_45;	
		} target[MAX_TARGETS];
	} bus[MAX_BUSES];

	uint16_t unused_248;	

	uint16_t subsystem_id[2];	

	union {				
		uint8_t unused_254;
		uint8_t system_id_pointer;
	} sysid_1x160;

	uint8_t chksum;		
};

#define MAX_CMDSZ	12		
struct cmd_entry {
	uint8_t entry_type;		
#define COMMAND_TYPE    1		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		
	__le32 handle;			
	uint8_t lun;			
	uint8_t target;			
	__le16 cdb_len;			
	__le16 control_flags;		
	__le16 reserved;
	__le16 timeout;			
	__le16 dseg_count;		
	uint8_t scsi_cdb[MAX_CMDSZ];	
	__le32 dseg_0_address;		
	__le32 dseg_0_length;		
	__le32 dseg_1_address;		
	__le32 dseg_1_length;		
	__le32 dseg_2_address;		
	__le32 dseg_2_length;		
	__le32 dseg_3_address;		
	__le32 dseg_3_length;		
};

struct cont_entry {
	uint8_t entry_type;		
#define CONTINUE_TYPE   2		
	uint8_t entry_count;		
	uint8_t sys_define;		
	uint8_t entry_status;		
	__le32 reserved;		
	__le32 dseg_0_address;		
	__le32 dseg_0_length;		
	__le32 dseg_1_address;		
	__le32 dseg_1_length;		
	__le32 dseg_2_address;		
	__le32 dseg_2_length;		
	__le32 dseg_3_address;		
	__le32 dseg_3_length;		
	__le32 dseg_4_address;		
	__le32 dseg_4_length;		
	__le32 dseg_5_address;		
	__le32 dseg_5_length;		
	__le32 dseg_6_address;		
	__le32 dseg_6_length;		
};

struct response {
	uint8_t entry_type;	
#define STATUS_TYPE     3	
	uint8_t entry_count;	
	uint8_t sys_define;	
	uint8_t entry_status;	
#define RF_CONT         BIT_0	
#define RF_FULL         BIT_1	
#define RF_BAD_HEADER   BIT_2	
#define RF_BAD_PAYLOAD  BIT_3	
	__le32 handle;		
	__le16 scsi_status;	
	__le16 comp_status;	
	__le16 state_flags;	
#define SF_TRANSFER_CMPL	BIT_14	
#define SF_GOT_SENSE	 	BIT_13	
#define SF_GOT_STATUS	 	BIT_12	
#define SF_TRANSFERRED_DATA	BIT_11	
#define SF_SENT_CDB	 	BIT_10	
#define SF_GOT_TARGET	 	BIT_9	
#define SF_GOT_BUS	 	BIT_8	
	__le16 status_flags;	
	__le16 time;		
	__le16 req_sense_length;
	__le32 residual_length;	
	__le16 reserved[4];
	uint8_t req_sense_data[32];	
};

struct mrk_entry {
	uint8_t entry_type;	
#define MARKER_TYPE     4	
	uint8_t entry_count;	
	uint8_t sys_define;	
	uint8_t entry_status;	
	__le32 reserved;
	uint8_t lun;		
	uint8_t target;		
	uint8_t modifier;	
#define MK_SYNC_ID_LUN      0	
#define MK_SYNC_ID          1	
#define MK_SYNC_ALL         2	
	uint8_t reserved_1[53];
};

struct ecmd_entry {
	uint8_t entry_type;	
#define EXTENDED_CMD_TYPE  5	
	uint8_t entry_count;	
	uint8_t sys_define;	
	uint8_t entry_status;	
	uint32_t handle;	
	uint8_t lun;		
	uint8_t target;		
	__le16 cdb_len;		
	__le16 control_flags;	
	__le16 reserved;
	__le16 timeout;		
	__le16 dseg_count;	
	uint8_t scsi_cdb[88];	
};

typedef struct {
	uint8_t entry_type;	
#define COMMAND_A64_TYPE 9	
	uint8_t entry_count;	
	uint8_t sys_define;	
	uint8_t entry_status;	
	__le32 handle;	
	uint8_t lun;		
	uint8_t target;		
	__le16 cdb_len;	
	__le16 control_flags;	
	__le16 reserved;
	__le16 timeout;	
	__le16 dseg_count;	
	uint8_t scsi_cdb[MAX_CMDSZ];	
	__le32 reserved_1[2];	
	__le32 dseg_0_address[2];	
	__le32 dseg_0_length;	
	__le32 dseg_1_address[2];	
	__le32 dseg_1_length;	
} cmd_a64_entry_t, request_t;

struct cont_a64_entry {
	uint8_t entry_type;	
#define CONTINUE_A64_TYPE 0xA	
	uint8_t entry_count;	
	uint8_t sys_define;	
	uint8_t entry_status;	
	__le32 dseg_0_address[2];	
	__le32 dseg_0_length;		
	__le32 dseg_1_address[2];	
	__le32 dseg_1_length;		
	__le32 dseg_2_address[2];	
	__le32 dseg_2_length;		
	__le32 dseg_3_address[2];	
	__le32 dseg_3_length;		
	__le32 dseg_4_address[2];	
	__le32 dseg_4_length;		
};

struct elun_entry {
	uint8_t entry_type;	
#define ENABLE_LUN_TYPE 0xB	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	__le16 lun;		
	__le16 reserved_4;
	__le32 option_flags;
	uint8_t status;
	uint8_t reserved_5;
	uint8_t command_count;	
	uint8_t immed_notify_count;	
	
	uint8_t group_6_length;	
	
	uint8_t group_7_length;	
	
	__le16 timeout;		
	__le16 reserved_6[20];
};

struct modify_lun_entry {
	uint8_t entry_type;	
#define MODIFY_LUN_TYPE 0xC	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;		
	uint8_t reserved_3;
	uint8_t operators;
	uint8_t reserved_4;
	__le32 option_flags;
	uint8_t status;
	uint8_t reserved_5;
	uint8_t command_count;	
	uint8_t immed_notify_count;	
	
	__le16 reserved_6;
	__le16 timeout;		
	__le16 reserved_7[20];
};

struct notify_entry {
	uint8_t entry_type;	
#define IMMED_NOTIFY_TYPE 0xD	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t reserved_4;
	uint8_t tag_value;	
	uint8_t tag_type;	
	
	__le16 seq_id;
	uint8_t scsi_msg[8];	
	__le16 reserved_5[8];
	uint8_t sense_data[18];
};

struct nack_entry {
	uint8_t entry_type;	
#define NOTIFY_ACK_TYPE 0xE	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t event;
	__le16 seq_id;
	__le16 reserved_4[22];
};

struct atio_entry {
	uint8_t entry_type;	
#define ACCEPT_TGT_IO_TYPE 6	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;
	uint8_t initiator_id;
	uint8_t cdb_len;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t scsi_status;
	uint8_t tag_value;	
	uint8_t tag_type;	
	uint8_t cdb[26];
	uint8_t sense_data[18];
};

struct ctio_entry {
	uint8_t entry_type;	
#define CONTINUE_TGT_IO_TYPE 7	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;		
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t scsi_status;
	uint8_t tag_value;	
	uint8_t tag_type;	
	__le32 transfer_length;
	__le32 residual;
	__le16 timeout;		
	__le16 dseg_count;	
	__le32 dseg_0_address;	
	__le32 dseg_0_length;	
	__le32 dseg_1_address;	
	__le32 dseg_1_length;	
	__le32 dseg_2_address;	
	__le32 dseg_2_length;	
	__le32 dseg_3_address;	
	__le32 dseg_3_length;	
};

struct ctio_ret_entry {
	uint8_t entry_type;	
#define CTIO_RET_TYPE   7	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;		
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t scsi_status;
	uint8_t tag_value;	
	uint8_t tag_type;	
	__le32 transfer_length;
	__le32 residual;
	__le16 timeout;		
	__le16 dseg_count;	
	__le32 dseg_0_address;	
	__le32 dseg_0_length;	
	__le32 dseg_1_address;	
	__le16 dseg_1_length;	
	uint8_t sense_data[18];
};

struct ctio_a64_entry {
	uint8_t entry_type;	
#define CTIO_A64_TYPE 0xF	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;		
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t scsi_status;
	uint8_t tag_value;	
	uint8_t tag_type;	
	__le32 transfer_length;
	__le32 residual;
	__le16 timeout;		
	__le16 dseg_count;	
	__le32 reserved_4[2];
	__le32 dseg_0_address[2];
	__le32 dseg_0_length;	
	__le32 dseg_1_address[2];
	__le32 dseg_1_length;	
};

struct ctio_a64_ret_entry {
	uint8_t entry_type;	
#define CTIO_A64_RET_TYPE 0xF	
	uint8_t entry_count;	
	uint8_t reserved_1;
	uint8_t entry_status;	
	__le32 reserved_2;
	uint8_t lun;		
	uint8_t initiator_id;
	uint8_t reserved_3;
	uint8_t target_id;
	__le32 option_flags;
	uint8_t status;
	uint8_t scsi_status;
	uint8_t tag_value;	
	uint8_t tag_type;	
	__le32 transfer_length;
	__le32 residual;
	__le16 timeout;		
	__le16 dseg_count;	
	__le16 reserved_4[7];
	uint8_t sense_data[18];
};

#define RESPONSE_ENTRY_SIZE	(sizeof(struct response))
#define REQUEST_ENTRY_SIZE	(sizeof(request_t))

#define CS_COMPLETE         0x0	
#define CS_INCOMPLETE       0x1	
#define CS_DMA              0x2	
#define CS_TRANSPORT        0x3	
#define CS_RESET            0x4	
#define CS_ABORTED          0x5	
#define CS_TIMEOUT          0x6	
#define CS_DATA_OVERRUN     0x7	
#define CS_COMMAND_OVERRUN  0x8	
#define CS_STATUS_OVERRUN   0x9	
#define CS_BAD_MSG          0xA	
#define CS_NO_MSG_OUT       0xB	
#define CS_EXTENDED_ID      0xC	
#define CS_IDE_MSG          0xD	
#define CS_ABORT_MSG        0xE	
#define CS_REJECT_MSG       0xF	
#define CS_NOP_MSG          0x10	
#define CS_PARITY_MSG       0x11	
#define CS_DEV_RESET_MSG    0x12	
#define CS_ID_MSG           0x13	
#define CS_FREE             0x14	
#define CS_DATA_UNDERRUN    0x15	
#define CS_TRANACTION_1     0x18	
#define CS_TRANACTION_2     0x19	
#define CS_TRANACTION_3     0x1a	
#define CS_INV_ENTRY_TYPE   0x1b	
#define CS_DEV_QUEUE_FULL   0x1c	
#define CS_PHASED_SKIPPED   0x1d	
#define CS_ARS_FAILED       0x1e	
#define CS_LVD_BUS_ERROR    0x21	
#define CS_BAD_PAYLOAD      0x80	
#define CS_UNKNOWN          0x81	
#define CS_RETRY            0x82	

#define OF_ENABLE_TAG       BIT_1	
#define OF_DATA_IN          BIT_6	
					
#define OF_DATA_OUT         BIT_7	
					
#define OF_NO_DATA          (BIT_7 | BIT_6)
#define OF_DISC_DISABLED    BIT_15	
#define OF_DISABLE_SDP      BIT_24	
#define OF_SEND_RDP         BIT_26	
#define OF_FORCE_DISC       BIT_30	
#define OF_SSTS             BIT_31	


struct bus_param {
	uint8_t id;		
	uint8_t bus_reset_delay;	
	uint8_t failed_reset_count;	
	uint8_t unused;
	uint16_t device_enables;	
	uint16_t lun_disables;	
	uint16_t qtag_enables;	
	uint16_t hiwat;		
	uint8_t reset_marker:1;
	uint8_t disable_scsi_reset:1;
	uint8_t scsi_bus_dead:1;	
};


struct qla_driver_setup {
	uint32_t no_sync:1;
	uint32_t no_wide:1;
	uint32_t no_ppr:1;
	uint32_t no_nvram:1;
	uint16_t sync_mask;
	uint16_t wide_mask;
	uint16_t ppr_mask;
};


struct scsi_qla_host {
	
	struct Scsi_Host *host;	
	struct scsi_qla_host *next;
	struct device_reg __iomem *iobase;	

	unsigned char __iomem *mmpbase;	
	unsigned long host_no;
	struct pci_dev *pdev;
	uint8_t devnum;
	uint8_t revision;
	uint8_t ports;

	unsigned long actthreads;
	unsigned long isr_count;	
	unsigned long spurious_int;

	
	struct srb *outstanding_cmds[MAX_OUTSTANDING_COMMANDS];

	
	struct bus_param bus_settings[MAX_BUSES];

	
	volatile uint16_t mailbox_out[MAILBOX_REGISTER_COUNT];

	dma_addr_t request_dma;		
	request_t *request_ring;	
	request_t *request_ring_ptr;	
	uint16_t req_ring_index;	
	uint16_t req_q_cnt;		

	dma_addr_t response_dma;	
	struct response *response_ring;	
	struct response *response_ring_ptr;	
	uint16_t rsp_ring_index;	

	struct list_head done_q;	

	struct completion *mailbox_wait;

	volatile struct {
		uint32_t online:1;			
		uint32_t reset_marker:1;		
		uint32_t disable_host_adapter:1;	
		uint32_t reset_active:1;		
		uint32_t abort_isp_active:1;		
		uint32_t disable_risc_code_load:1;	
#ifdef __ia64__
		uint32_t use_pci_vchannel:1;
#endif
	} flags;

	struct nvram nvram;
	int nvram_valid;

	
	unsigned short fwstart; 
	unsigned char fwver1;   
	unsigned char fwver2;   
	unsigned char fwver3;   
};

#endif 
