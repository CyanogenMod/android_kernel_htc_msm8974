/*
 * Aic94xx SAS/SATA driver SAS definitions and hardware interface header file.
 *
 * Copyright (C) 2005 Adaptec, Inc.  All rights reserved.
 * Copyright (C) 2005 Luben Tuikov <luben_tuikov@adaptec.com>
 *
 * This file is licensed under GPLv2.
 *
 * This file is part of the aic94xx driver.
 *
 * The aic94xx driver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * The aic94xx driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the aic94xx driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _AIC94XX_SAS_H_
#define _AIC94XX_SAS_H_

#include <scsi/libsas.h>

#define ASD_MAX_DDBS	128

struct asd_ddb_ssp_smp_target_port {
	u8     conn_type;	  
#define DDB_TP_CONN_TYPE 0x81	  

	u8     conn_rate;
	__be16 init_conn_tag;
	u8     dest_sas_addr[8];  

	__le16 send_queue_head;
	u8     sq_suspended;
	u8     ddb_type;	  
#define DDB_TYPE_UNUSED    0xFF
#define DDB_TYPE_TARGET    0xFE
#define DDB_TYPE_INITIATOR 0xFD
#define DDB_TYPE_PM_PORT   0xFC

	__le16 _r_a;
	__be16 awt_def;

	u8     compat_features;	  
	u8     pathway_blocked_count;
	__be16 arb_wait_time;
	__be32 more_compat_features; 

	u8     conn_mask;
	u8     flags;	  
#define CONCURRENT_CONN_SUPP 0x04
#define OPEN_REQUIRED        0x01

	u16    _r_b;
	__le16 exec_queue_tail;
	__le16 send_queue_tail;
	__le16 sister_ddb;

	__le16 _r_c;

	u8     max_concurrent_conn;
	u8     num_concurrent_conn;
	u8     num_contexts;

	u8     _r_d;

	__le16 active_task_count;

	u8     _r_e[9];

	u8     itnl_reason;	  

	__le16 _r_f;

	__le16 itnl_timeout;
#define ITNL_TIMEOUT_CONST 0x7D0 

	__le32 itnl_timestamp;
} __attribute__ ((packed));

struct asd_ddb_stp_sata_target_port {
	u8     conn_type;	  
	u8     conn_rate;
	__be16 init_conn_tag;
	u8     dest_sas_addr[8];  

	__le16 send_queue_head;
	u8     sq_suspended;
	u8     ddb_type;	  

	__le16 _r_a;

	__be16 awt_def;
	u8     compat_features;	  
	u8     pathway_blocked_count;
	__be16 arb_wait_time;
	__be32 more_compat_features; 

	u8     conn_mask;
	u8     flags;	  
#define SATA_MULTIPORT     0x80
#define SUPPORTS_AFFIL     0x40
#define STP_AFFIL_POL      0x20

	u8     _r_b;
	u8     flags2;		  
#define STP_CL_POL_NO_TX    0x00
#define STP_CL_POL_BTW_CMDS 0x01

	__le16 exec_queue_tail;
	__le16 send_queue_tail;
	__le16 sister_ddb;
	__le16 ata_cmd_scbptr;
	__le32 sata_tag_alloc_mask;
	__le16 active_task_count;
	__le16 _r_c;
	__le32 sata_sactive;
	u8     num_sata_tags;
	u8     sata_status;
	u8     sata_ending_status;
	u8     itnl_reason;	  
	__le16 ncq_data_scb_ptr;
	__le16 itnl_timeout;
	__le32 itnl_timestamp;
} __attribute__ ((packed));

struct asd_ddb_init_port {
	u8     conn_type;	  
	u8     conn_rate;
	__be16 init_conn_tag;     
	u8     dest_sas_addr[8];
	__le16 send_queue_head;   
	u8     sq_suspended;
	u8     ddb_type;	  
	__le16 _r_a;
	__be16 awt_def;		  
	u8     compat_features;
	u8     pathway_blocked_count;
	__be16 arb_wait_time;	  
	__be32 more_compat_features; 
	u8     conn_mask;
	u8     flags;		  
	u16    _r_b;
	__le16 exec_queue_tail;	  
	__le16 send_queue_tail;
	__le16 sister_ddb;
	__le16 init_resp_timeout; 
	__le32 _r_c;
	__le16 active_tasks;	  
	__le16 init_list;	  
	__le32 _r_d;
	u8     max_conn_to[3]; 
	u8     itnl_reason;	  
	__le16 bus_inact_to; 
	__le16 itnl_to;		  
	__le32 itnl_timestamp;
} __attribute__ ((packed));

struct asd_ddb_sata_tag {
	__le16 scb_pointer[32];
} __attribute__ ((packed));

struct asd_ddb_sata_pm_table {
	__le16 ddb_pointer[16];
	__le16 _r_a[16];
} __attribute__ ((packed));

struct asd_ddb_sata_pm_port {
	u8     _r_a[15];
	u8     ddb_type;
	u8     _r_b[13];
	u8     pm_port_flags;
#define PM_PORT_MASK  0xF0
#define PM_PORT_SET   0x02
	u8     _r_c[6];
	__le16 sister_ddb;
	__le16 ata_cmd_scbptr;
	__le32 sata_tag_alloc_mask;
	__le16 active_task_count;
	__le16 parent_ddb;
	__le32 sata_sactive;
	u8     num_sata_tags;
	u8     sata_status;
	u8     sata_ending_status;
	u8     _r_d[9];
} __attribute__ ((packed));

struct asd_ddb_seq_shared {
	__le16 q_free_ddb_head;
	__le16 q_free_ddb_tail;
	__le16 q_free_ddb_cnt;
	__le16 q_used_ddb_head;
	__le16 q_used_ddb_tail;
	__le16 shared_mem_lock;
	__le16 smp_conn_tag;
	__le16 est_nexus_buf_cnt;
	__le16 est_nexus_buf_thresh;
	u32    _r_a;
	u8     settable_max_contexts;
	u8     _r_b[23];
	u8     conn_not_active;
	u8     phy_is_up;
	u8     _r_c[8];
	u8     port_map_by_links[8];
} __attribute__ ((packed));


struct sg_el {
	__le64 bus_addr;
	__le32 size;
	__le16 _r;
	u8     next_sg_offs;
	u8     flags;
#define ASD_SG_EL_DS_MASK   0x30
#define ASD_SG_EL_DS_OCM    0x10
#define ASD_SG_EL_DS_HM     0x00
#define ASD_SG_EL_LIST_MASK 0xC0
#define ASD_SG_EL_LIST_EOL  0x40
#define ASD_SG_EL_LIST_EOS  0x80
} __attribute__ ((packed));



struct scb_header {
	__le64 next_scb;
	__le16 index;		  
	u8     opcode;
} __attribute__ ((packed));

#define INITIATE_SSP_TASK       0x00
#define INITIATE_LONG_SSP_TASK  0x01
#define INITIATE_BIDIR_SSP_TASK 0x02
#define SCB_ABORT_TASK          0x03
#define INITIATE_SSP_TMF        0x04
#define SSP_TARG_GET_DATA       0x05
#define SSP_TARG_GET_DATA_GOOD  0x06
#define SSP_TARG_SEND_RESP      0x07
#define QUERY_SSP_TASK          0x08
#define INITIATE_ATA_TASK       0x09
#define INITIATE_ATAPI_TASK     0x0a
#define CONTROL_ATA_DEV         0x0b
#define INITIATE_SMP_TASK       0x0c
#define SMP_TARG_SEND_RESP      0x0f

#define SSP_TARG_SEND_DATA      0x40
#define SSP_TARG_SEND_DATA_GOOD 0x41

#define CONTROL_PHY             0x80
#define SEND_PRIMITIVE          0x81
#define INITIATE_LINK_ADM_TASK  0x82

#define EMPTY_SCB               0xc0
#define INITIATE_SEQ_ADM_TASK   0xc1
#define EST_ICL_TARG_WINDOW     0xc2
#define COPY_MEM                0xc3
#define CLEAR_NEXUS             0xc4
#define INITIATE_DDB_ADM_TASK   0xc6
#define ESTABLISH_NEXUS_ESCB    0xd0

#define LUN_SIZE                8

struct ssp_task_iu {
	u8     lun[LUN_SIZE];	  
	u16    _r_a;
	u8     tmf;
	u8     _r_b;
	__be16 tag;		  
	u8     _r_c[14];
} __attribute__ ((packed));

struct ssp_command_iu {
	u8     lun[LUN_SIZE];
	u8     _r_a;
	u8     efb_prio_attr;	  
#define EFB_MASK        0x80
#define TASK_PRIO_MASK	0x78
#define TASK_ATTR_MASK  0x07

	u8    _r_b;
	u8     add_cdb_len;	  
	union {
		u8     cdb[16];
		struct {
			__le64 long_cdb_addr;	  
			__le32 long_cdb_size;	  
			u8     _r_c[3];
			u8     eol_ds;		  
		} long_cdb;	  
	};
} __attribute__ ((packed));

struct xfer_rdy_iu {
	__be32 requested_offset;  
	__be32 write_data_len;	  
	__be32 _r_a;
} __attribute__ ((packed));


struct initiate_ssp_task {
	u8     proto_conn_rate;	  
	__le32 total_xfer_len;
	struct ssp_frame_hdr  ssp_frame;
	struct ssp_command_iu ssp_cmd;
	__le16 sister_scb;	  
	__le16 conn_handle;	  
	u8     data_dir;	  
#define DATA_DIR_NONE   0x00
#define DATA_DIR_IN     0x01
#define DATA_DIR_OUT    0x02
#define DATA_DIR_BYRECIPIENT 0x03

	u8     _r_a;
	u8     retry_count;
	u8     _r_b[5];
	struct sg_el sg_element[3]; 
} __attribute__ ((packed));

struct initiate_ata_task {
	u8     proto_conn_rate;
	__le32 total_xfer_len;
	struct host_to_dev_fis fis;
	__le32 data_offs;
	u8     atapi_packet[16];
	u8     _r_a[12];
	__le16 sister_scb;
	__le16 conn_handle;
	u8     ata_flags;	  
#define CSMI_TASK           0x40
#define DATA_XFER_MODE_DMA  0x10
#define ATA_Q_TYPE_MASK     0x08
#define	ATA_Q_TYPE_UNTAGGED 0x00
#define ATA_Q_TYPE_NCQ      0x08

	u8     _r_b;
	u8     retry_count;
	u8     _r_c;
	u8     flags;
#define STP_AFFIL_POLICY   0x20
#define SET_AFFIL_POLICY   0x10
#define RET_PARTIAL_SGLIST 0x02

	u8     _r_d[3];
	struct sg_el sg_element[3];
} __attribute__ ((packed));

struct initiate_smp_task {
	u8     proto_conn_rate;
	u8     _r_a[40];
	struct sg_el smp_req;
	__le16 sister_scb;
	__le16 conn_handle;
	u8     _r_c[8];
	struct sg_el smp_resp;
	u8     _r_d[32];
} __attribute__ ((packed));

struct control_phy {
	u8     phy_id;
	u8     sub_func;
#define DISABLE_PHY            0x00
#define ENABLE_PHY             0x01
#define RELEASE_SPINUP_HOLD    0x02
#define ENABLE_PHY_NO_SAS_OOB  0x03
#define ENABLE_PHY_NO_SATA_OOB 0x04
#define PHY_NO_OP              0x05
#define EXECUTE_HARD_RESET     0x81

	u8     func_mask;
	u8     speed_mask;
	u8     hot_plug_delay;
	u8     port_type;
	u8     flags;
#define DEV_PRES_TIMER_OVERRIDE_ENABLE 0x01
#define DISABLE_PHY_IF_OOB_FAILS       0x02

	__le32 timeout_override;
	u8     link_reset_retries;
	u8     _r_a[47];
	__le16 conn_handle;
	u8     _r_b[56];
} __attribute__ ((packed));

struct control_ata_dev {
	u8     proto_conn_rate;
	__le32 _r_a;
	struct host_to_dev_fis fis;
	u8     _r_b[32];
	__le16 sister_scb;
	__le16 conn_handle;
	u8     ata_flags;	  
	u8     _r_c[55];
} __attribute__ ((packed));

struct empty_scb {
	u8     num_valid;
	__le32 _r_a;
#define ASD_EDBS_PER_SCB 7
#define ASD_EDB_SIZE (24+1024+4+16)
	struct sg_el eb[ASD_EDBS_PER_SCB];
#define ELEMENT_NOT_VALID  0xC0
} __attribute__ ((packed));

struct initiate_link_adm {
	u8     phy_id;
	u8     sub_func;
#define GET_LINK_ERROR_COUNT      0x00
#define RESET_LINK_ERROR_COUNT    0x01
#define ENABLE_NOTIFY_SPINUP_INTS 0x02

	u8     _r_a[57];
	__le16 conn_handle;
	u8     _r_b[56];
} __attribute__ ((packed));

struct copy_memory {
	u8     _r_a;
	__le16 xfer_len;
	__le16 _r_b;
	__le64 src_busaddr;
	u8     src_ds;		  
	u8     _r_c[45];
	__le16 conn_handle;
	__le64 _r_d;
	__le64 dest_busaddr;
	u8     dest_ds;		  
	u8     _r_e[39];
} __attribute__ ((packed));

struct abort_task {
	u8     proto_conn_rate;
	__le32 _r_a;
	struct ssp_frame_hdr ssp_frame;
	struct ssp_task_iu ssp_task;
	__le16 sister_scb;
	__le16 conn_handle;
	u8     flags;	  
#define SUSPEND_DATA_TRANS 0x04

	u8     _r_b;
	u8     retry_count;
	u8     _r_c[5];
	__le16 index;  
	__le16 itnl_to;
	u8     _r_d[44];
} __attribute__ ((packed));

struct clear_nexus {
	u8     nexus;
#define NEXUS_ADAPTER  0x00
#define NEXUS_PORT     0x01
#define NEXUS_I_T      0x02
#define NEXUS_I_T_L    0x03
#define NEXUS_TAG      0x04
#define NEXUS_TRANS_CX 0x05
#define NEXUS_SATA_TAG 0x06
#define NEXUS_T_L      0x07
#define NEXUS_L        0x08
#define NEXUS_T_TAG    0x09

	__le32 _r_a;
	u8     flags;
#define SUSPEND_TX     0x80
#define RESUME_TX      0x40
#define SEND_Q         0x04
#define EXEC_Q         0x02
#define NOTINQ         0x01

	u8     _r_b[3];
	u8     conn_mask;
	u8     _r_c[19];
	struct ssp_task_iu ssp_task; 
	__le16 _r_d;
	__le16 conn_handle;
	__le64 _r_e;
	__le16 index;  
	__le16 context;		  
	u8     _r_f[44];
} __attribute__ ((packed));

struct initiate_ssp_tmf {
	u8     proto_conn_rate;
	__le32 _r_a;
	struct ssp_frame_hdr ssp_frame;
	struct ssp_task_iu ssp_task;
	__le16 sister_scb;
	__le16 conn_handle;
	u8     flags;	  
#define OVERRIDE_ITNL_TIMER  8

	u8     _r_b;
	u8     retry_count;
	u8     _r_c[5];
	__le16 index;  
	__le16 itnl_to;
	u8     _r_d[44];
} __attribute__ ((packed));

struct send_prim {
	u8     phy_id;
	u8     wait_transmit; 	  
	u8     xmit_flags;
#define XMTPSIZE_MASK      0xF0
#define XMTPSIZE_SINGLE    0x10
#define XMTPSIZE_REPEATED  0x20
#define XMTPSIZE_CONT      0x20
#define XMTPSIZE_TRIPLE    0x30
#define XMTPSIZE_REDUNDANT 0x60
#define XMTPSIZE_INF       0

#define XMTCONTEN          0x04
#define XMTPFRM            0x02	  
#define XMTPIMM            0x01	  

	__le16 _r_a;
	u8     prim[4];		  
	u8     _r_b[50];
	__le16 conn_handle;
	u8     _r_c[56];
} __attribute__ ((packed));

struct ssp_targ_get_data {
	u8     proto_conn_rate;
	__le32 total_xfer_len;
	struct ssp_frame_hdr ssp_frame;
	struct xfer_rdy_iu  xfer_rdy;
	u8     lun[LUN_SIZE];
	__le64 _r_a;
	__le16 sister_scb;
	__le16 conn_handle;
	u8     data_dir;	  
	u8     _r_b;
	u8     retry_count;
	u8     _r_c[5];
	struct sg_el sg_element[3];
} __attribute__ ((packed));


struct scb {
	struct scb_header header;
	union {
		struct initiate_ssp_task ssp_task;
		struct initiate_ata_task ata_task;
		struct initiate_smp_task smp_task;
		struct control_phy       control_phy;
		struct control_ata_dev   control_ata_dev;
		struct empty_scb         escb;
		struct initiate_link_adm link_adm;
		struct copy_memory       cp_mem;
		struct abort_task        abort_task;
		struct clear_nexus       clear_nexus;
		struct initiate_ssp_tmf  ssp_tmf;
	};
} __attribute__ ((packed));

#define TC_NO_ERROR             0x00
#define TC_UNDERRUN             0x01
#define TC_OVERRUN              0x02
#define TF_OPEN_TO              0x03
#define TF_OPEN_REJECT          0x04
#define TI_BREAK                0x05
#define TI_PROTO_ERR            0x06
#define TC_SSP_RESP             0x07
#define TI_PHY_DOWN             0x08
#define TF_PHY_DOWN             0x09
#define TC_LINK_ADM_RESP        0x0a
#define TC_CSMI                 0x0b
#define TC_ATA_RESP             0x0c
#define TU_PHY_DOWN             0x0d
#define TU_BREAK                0x0e
#define TI_SATA_TO              0x0f
#define TI_NAK                  0x10
#define TC_CONTROL_PHY          0x11
#define TF_BREAK                0x12
#define TC_RESUME               0x13
#define TI_ACK_NAK_TO           0x14
#define TF_SMPRSP_TO            0x15
#define TF_SMP_XMIT_RCV_ERR     0x16
#define TC_PARTIAL_SG_LIST      0x17
#define TU_ACK_NAK_TO           0x18
#define TU_SATA_TO              0x19
#define TF_NAK_RECV             0x1a
#define TA_I_T_NEXUS_LOSS       0x1b
#define TC_ATA_R_ERR_RECV       0x1c
#define TF_TMF_NO_CTX           0x1d
#define TA_ON_REQ               0x1e
#define TF_TMF_NO_TAG           0x1f
#define TF_TMF_TAG_FREE         0x20
#define TF_TMF_TASK_DONE        0x21
#define TF_TMF_NO_CONN_HANDLE   0x22
#define TC_TASK_CLEARED         0x23
#define TI_SYNCS_RECV           0x24
#define TU_SYNCS_RECV           0x25
#define TF_IRTT_TO              0x26
#define TF_NO_SMP_CONN          0x27
#define TF_IU_SHORT             0x28
#define TF_DATA_OFFS_ERR        0x29
#define TF_INV_CONN_HANDLE      0x2a
#define TF_REQUESTED_N_PENDING  0x2b

#define ESCB_RECVD              0xC0


struct done_list_struct {
	__le16 index;		  
	u8     opcode;
	u8     status_block[4];
	u8     toggle;		  
#define DL_TOGGLE_MASK     0x01
} __attribute__ ((packed));


struct asd_phy {
	struct asd_sas_phy        sas_phy;
	struct asd_phy_desc   *phy_desc; 

	struct sas_identify_frame *identify_frame;
	struct asd_dma_tok  *id_frm_tok;
	struct asd_port     *asd_port;

	u8         frame_rcvd[ASD_EDB_SIZE];
};


#define ASD_SCB_SIZE sizeof(struct scb)
#define ASD_DDB_SIZE sizeof(struct asd_ddb_ssp_smp_target_port)

#define ASD_NOTIFY_ENABLE_SPINUP  0x10

#define ASD_NOTIFY_TIMEOUT        2500

#define ASD_NOTIFY_DOWN_COUNT     0

#define ASD_DEV_PRESENT_TIMEOUT   0x2710

#define ASD_SATA_INTERLOCK_TIMEOUT 0

#define ASD_STP_SHUTDOWN_TIMEOUT  0x0

#define ASD_SRST_ASSERT_TIMEOUT   0x05

#define ASD_RCV_FIS_TIMEOUT       0x01D905C0

#define ASD_ONE_MILLISEC_TIMEOUT  0x03e8

#define ASD_TEN_MILLISEC_TIMEOUT  0x2710
#define ASD_COMINIT_TIMEOUT ASD_TEN_MILLISEC_TIMEOUT

#define ASD_SMP_RCV_TIMEOUT       0x000F4240

#endif
