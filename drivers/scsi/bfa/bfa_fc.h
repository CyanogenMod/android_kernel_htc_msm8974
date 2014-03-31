/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __BFA_FC_H__
#define __BFA_FC_H__

#include "bfad_drv.h"

typedef u64 wwn_t;

#define WWN_NULL	(0)
#define FC_SYMNAME_MAX	256	

#pragma pack(1)

#define MAC_ADDRLEN	(6)
struct mac_s { u8 mac[MAC_ADDRLEN]; };
#define mac_t struct mac_s

#define SCSI_MAX_CDBLEN     16
struct scsi_cdb_s {
	u8         scsi_cdb[SCSI_MAX_CDBLEN];
};

#define SCSI_STATUS_GOOD                   0x00
#define SCSI_STATUS_CHECK_CONDITION        0x02
#define SCSI_STATUS_CONDITION_MET          0x04
#define SCSI_STATUS_BUSY                   0x08
#define SCSI_STATUS_INTERMEDIATE           0x10
#define SCSI_STATUS_ICM                    0x14 
#define SCSI_STATUS_RESERVATION_CONFLICT   0x18
#define SCSI_STATUS_COMMAND_TERMINATED     0x22
#define SCSI_STATUS_QUEUE_FULL             0x28
#define SCSI_STATUS_ACA_ACTIVE             0x30

#define SCSI_MAX_ALLOC_LEN      0xFF    

struct fchs_s {
#ifdef __BIG_ENDIAN
	u32        routing:4;	
	u32        cat_info:4;	
#else
	u32        cat_info:4;	
	u32        routing:4;	
#endif
	u32        d_id:24;	

	u32        cs_ctl:8;	
	u32        s_id:24;	

	u32        type:8;	
	u32        f_ctl:24;	

	u8         seq_id;	
	u8         df_ctl;	
	u16        seq_cnt;	

	__be16     ox_id;	
	u16        rx_id;	

	u32        ro;		
};

enum {
	FC_RTG_FC4_DEV_DATA	= 0x0,	
	FC_RTG_EXT_LINK		= 0x2,	
	FC_RTG_FC4_LINK_DATA	= 0x3,	
	FC_RTG_VIDEO_DATA	= 0x4,	
	FC_RTG_EXT_HDR		= 0x5,	
	FC_RTG_BASIC_LINK	= 0x8,	
	FC_RTG_LINK_CTRL	= 0xC,	
};

enum {
	FC_CAT_LD_REQUEST	= 0x2,	
	FC_CAT_LD_REPLY		= 0x3,	
	FC_CAT_LD_DIAG		= 0xF,	
};

enum {
	FC_CAT_VFT_HDR = 0x0,	
	FC_CAT_IFR_HDR = 0x1,	
	FC_CAT_ENC_HDR = 0x2,	
};

enum {
	FC_CAT_UNCATEG_INFO	= 0x0,	
	FC_CAT_SOLICIT_DATA	= 0x1,	
	FC_CAT_UNSOLICIT_CTRL	= 0x2,	
	FC_CAT_SOLICIT_CTRL	= 0x3,	
	FC_CAT_UNSOLICIT_DATA	= 0x4,	
	FC_CAT_DATA_DESC	= 0x5,	
	FC_CAT_UNSOLICIT_CMD	= 0x6,	
	FC_CAT_CMD_STATUS	= 0x7,	
};

enum {
	FC_TYPE_BLS		= 0x0,	
	FC_TYPE_ELS		= 0x1,	
	FC_TYPE_IP		= 0x5,	
	FC_TYPE_FCP		= 0x8,	
	FC_TYPE_GPP		= 0x9,	
	FC_TYPE_SERVICES	= 0x20,	
	FC_TYPE_FC_FSS		= 0x22,	
	FC_TYPE_FC_AL		= 0x23,	
	FC_TYPE_FC_SNMP		= 0x24,	
	FC_TYPE_FC_SPINFAB	= 0xEE,	
	FC_TYPE_FC_DIAG		= 0xEF,	
	FC_TYPE_MAX		= 256,	
};

enum {
	FCTL_EC_ORIG = 0x000000,	
	FCTL_EC_RESP = 0x800000,	
	FCTL_SEQ_INI = 0x000000,	
	FCTL_SEQ_REC = 0x400000,	
	FCTL_FS_EXCH = 0x200000,	
	FCTL_LS_EXCH = 0x100000,	
	FCTL_END_SEQ = 0x080000,	
	FCTL_SI_XFER = 0x010000,	
	FCTL_RO_PRESENT = 0x000008,	
	FCTL_FILLBYTE_MASK = 0x000003	
};

enum {
	FC_MIN_WELL_KNOWN_ADDR		= 0xFFFFF0,
	FC_DOMAIN_CONTROLLER_MASK	= 0xFFFC00,
	FC_ALIAS_SERVER			= 0xFFFFF8,
	FC_MGMT_SERVER			= 0xFFFFFA,
	FC_TIME_SERVER			= 0xFFFFFB,
	FC_NAME_SERVER			= 0xFFFFFC,
	FC_FABRIC_CONTROLLER		= 0xFFFFFD,
	FC_FABRIC_PORT			= 0xFFFFFE,
	FC_BROADCAST_SERVER		= 0xFFFFFF
};

#define FC_DOMAIN_MASK  0xFF0000
#define FC_DOMAIN_SHIFT 16
#define FC_AREA_MASK    0x00FF00
#define FC_AREA_SHIFT   8
#define FC_PORT_MASK    0x0000FF
#define FC_PORT_SHIFT   0

#define FC_GET_DOMAIN(p)	(((p) & FC_DOMAIN_MASK) >> FC_DOMAIN_SHIFT)
#define FC_GET_AREA(p)		(((p) & FC_AREA_MASK) >> FC_AREA_SHIFT)
#define FC_GET_PORT(p)		(((p) & FC_PORT_MASK) >> FC_PORT_SHIFT)

#define FC_DOMAIN_CTRLR(p)	(FC_DOMAIN_CONTROLLER_MASK | (FC_GET_DOMAIN(p)))

enum {
	FC_RXID_ANY = 0xFFFFU,
};

struct fc_els_cmd_s {
	u32        els_code:8;	
	u32        reserved:24;
};

enum {
	FC_ELS_LS_RJT = 0x1,	
	FC_ELS_ACC = 0x02,	
	FC_ELS_PLOGI = 0x03,	
	FC_ELS_FLOGI = 0x04,	
	FC_ELS_LOGO = 0x05,	
	FC_ELS_ABTX = 0x06,	
	FC_ELS_RES = 0x08,	
	FC_ELS_RSS = 0x09,	
	FC_ELS_RSI = 0x0A,	
	FC_ELS_ESTC = 0x0C,	
	FC_ELS_RTV = 0x0E,	
	FC_ELS_RLS = 0x0F,	
	FC_ELS_ECHO = 0x10,	
	FC_ELS_TEST = 0x11,	
	FC_ELS_RRQ = 0x12,	
	FC_ELS_REC = 0x13,	
	FC_ELS_PRLI = 0x20,	
	FC_ELS_PRLO = 0x21,	
	FC_ELS_SCN = 0x22,	
	FC_ELS_TPRLO = 0x24,	
	FC_ELS_PDISC = 0x50,	
	FC_ELS_FDISC = 0x51,	
	FC_ELS_ADISC = 0x52,	
	FC_ELS_FARP_REQ = 0x54,	
	FC_ELS_FARP_REP = 0x55,	
	FC_ELS_FAN = 0x60,	
	FC_ELS_RSCN = 0x61,	
	FC_ELS_SCR = 0x62,	
	FC_ELS_RTIN = 0x77,	
	FC_ELS_RNID = 0x78,	
	FC_ELS_RLIR = 0x79,	

	FC_ELS_RPSC = 0x7D,	
	FC_ELS_QSA = 0x7E,	
	FC_ELS_E2E_LBEACON = 0x81,
				
	FC_ELS_AUTH = 0x90,	
	FC_ELS_RFCN = 0x97,	
};

enum {
	FC_PH_VER_4_3 = 0x09,
	FC_PH_VER_PH_3 = 0x20,
};

enum {
	FC_MIN_PDUSZ = 512,
	FC_MAX_PDUSZ = 2112,
};

struct fc_plogi_csp_s {
	u8		verhi;		
	u8		verlo;		
	__be16		bbcred;		

#ifdef __BIG_ENDIAN
	u8		ciro:1,		
			rro:1,		
			npiv_supp:1,	
			port_type:1,	
			altbbcred:1,	
			resolution:1,	
			vvl_info:1,	
			reserved1:1;

	u8		hg_supp:1,
			query_dbc:1,
			security:1,
			sync_cap:1,
			r_t_tov:1,
			dh_dup_supp:1,
			cisc:1,		
			payload:1;
#else
	u8		reserved2:2,
			resolution:1,	
			altbbcred:1,	
			port_type:1,	
			npiv_supp:1,	
			rro:1,		
			ciro:1;		

	u8		payload:1,
			cisc:1,		
			dh_dup_supp:1,
			r_t_tov:1,
			sync_cap:1,
			security:1,
			query_dbc:1,
			hg_supp:1;
#endif
	__be16		rxsz;		
	__be16		conseq;
	__be16		ro_bitmap;
	__be32		e_d_tov;
};

struct fc_plogi_clp_s {
#ifdef __BIG_ENDIAN
	u32        class_valid:1;
	u32        intermix:1;	
	u32        reserved1:2;
	u32        sequential:1;
	u32        reserved2:3;
#else
	u32        reserved2:3;
	u32        sequential:1;
	u32        reserved1:2;
	u32        intermix:1;	
	u32        class_valid:1;
#endif
	u32        reserved3:24;

	u32        reserved4:16;
	u32        rxsz:16;	

	u32        reserved5:8;
	u32        conseq:8;
	u32        e2e_credit:16; 

	u32        reserved7:8;
	u32        ospx:8;
	u32        reserved8:16;
};

#define FLOGI_VVL_BRCD    0x42524344

struct fc_logi_s {
	struct fc_els_cmd_s	els_cmd;	
	struct fc_plogi_csp_s	csp;		
	wwn_t			port_name;
	wwn_t			node_name;
	struct fc_plogi_clp_s	class1;		
	struct fc_plogi_clp_s	class2;		
	struct fc_plogi_clp_s	class3;		
	struct fc_plogi_clp_s	class4;		
	u8			vvl[16];	
};

struct fc_logo_s {
	struct fc_els_cmd_s	els_cmd;	
	u32			res1:8;
	u32		nport_id:24;	
	wwn_t           orig_port_name;	
};

struct fc_adisc_s {
	struct fc_els_cmd_s els_cmd;	
	u32		res1:8;
	u32		orig_HA:24;	
	wwn_t		orig_port_name;	
	wwn_t		orig_node_name;	
	u32		res2:8;
	u32		nport_id:24;	
};

struct fc_exch_status_blk_s {
	u32        oxid:16;
	u32        rxid:16;
	u32        res1:8;
	u32        orig_np:24;	
	u32        res2:8;
	u32        resp_np:24;	
	u32        es_bits;
	u32        res3;
};

struct fc_res_s {
	struct fc_els_cmd_s els_cmd;	
	u32        res1:8;
	u32        nport_id:24;		
	u32        oxid:16;
	u32        rxid:16;
	u8         assoc_hdr[32];
};

struct fc_res_acc_s {
	struct fc_els_cmd_s		els_cmd;	
	struct fc_exch_status_blk_s	fc_exch_blk; 
};

struct fc_rec_s {
	struct fc_els_cmd_s els_cmd;	
	u32        res1:8;
	u32        nport_id:24;	
	u32        oxid:16;
	u32        rxid:16;
};

#define FC_REC_ESB_OWN_RSP	0x80000000	
#define FC_REC_ESB_SI		0x40000000	
#define FC_REC_ESB_COMP		0x20000000	
#define FC_REC_ESB_ENDCOND_ABN	0x10000000	
#define FC_REC_ESB_RQACT	0x04000000	
#define FC_REC_ESB_ERRP_MSK	0x03000000
#define FC_REC_ESB_OXID_INV	0x00800000	
#define FC_REC_ESB_RXID_INV	0x00400000	
#define FC_REC_ESB_PRIO_INUSE	0x00200000

struct fc_rec_acc_s {
	struct fc_els_cmd_s els_cmd;	
	u32        oxid:16;
	u32        rxid:16;
	u32        res1:8;
	u32        orig_id:24;	
	u32        res2:8;
	u32        resp_id:24;	
	u32        count;	
	u32        e_stat;	
};

struct fc_rsi_s {
	struct fc_els_cmd_s els_cmd;
	u32        res1:8;
	u32        orig_sid:24;
	u32        oxid:16;
	u32        rxid:16;
};

struct fc_prli_params_s {
	u32        reserved:16;
#ifdef __BIG_ENDIAN
	u32        reserved1:5;
	u32        rec_support:1;
	u32        task_retry_id:1;
	u32        retry:1;

	u32        confirm:1;
	u32        doverlay:1;
	u32        initiator:1;
	u32        target:1;
	u32        cdmix:1;
	u32        drmix:1;
	u32        rxrdisab:1;
	u32        wxrdisab:1;
#else
	u32        retry:1;
	u32        task_retry_id:1;
	u32        rec_support:1;
	u32        reserved1:5;

	u32        wxrdisab:1;
	u32        rxrdisab:1;
	u32        drmix:1;
	u32        cdmix:1;
	u32        target:1;
	u32        initiator:1;
	u32        doverlay:1;
	u32        confirm:1;
#endif
};

enum {
	FC_PRLI_ACC_XQTD = 0x1,		
	FC_PRLI_ACC_PREDEF_IMG = 0x5,	
};

struct fc_prli_params_page_s {
	u32        type:8;
	u32        codext:8;
#ifdef __BIG_ENDIAN
	u32        origprocasv:1;
	u32        rsppav:1;
	u32        imagepair:1;
	u32        reserved1:1;
	u32        rspcode:4;
#else
	u32        rspcode:4;
	u32        reserved1:1;
	u32        imagepair:1;
	u32        rsppav:1;
	u32        origprocasv:1;
#endif
	u32        reserved2:8;

	u32        origprocas;
	u32        rspprocas;
	struct fc_prli_params_s servparams;
};

struct fc_prli_s {
	u32        command:8;
	u32        pglen:8;
	u32        pagebytes:16;
	struct fc_prli_params_page_s parampage;
};

struct fc_prlo_params_page_s {
	u32        type:8;
	u32        type_ext:8;
#ifdef __BIG_ENDIAN
	u32        opa_valid:1;	
	u32        rpa_valid:1;	
	u32        res1:14;
#else
	u32        res1:14;
	u32        rpa_valid:1;	
	u32        opa_valid:1;	
#endif
	u32        orig_process_assc;
	u32        resp_process_assc;

	u32        res2;
};

struct fc_prlo_s {
	u32	command:8;
	u32	page_len:8;
	u32	payload_len:16;
	struct fc_prlo_params_page_s	prlo_params[1];
};

struct fc_prlo_acc_params_page_s {
	u32        type:8;
	u32        type_ext:8;

#ifdef __BIG_ENDIAN
	u32        opa_valid:1;	
	u32        rpa_valid:1;	
	u32        res1:14;
#else
	u32        res1:14;
	u32        rpa_valid:1;	
	u32        opa_valid:1;	
#endif
	u32        orig_process_assc;
	u32        resp_process_assc;

	u32        fc4type_csp;
};

struct fc_prlo_acc_s {
	u32        command:8;
	u32        page_len:8;
	u32        payload_len:16;
	struct fc_prlo_acc_params_page_s prlo_acc_params[1];
};

enum {
	FC_SCR_REG_FUNC_FABRIC_DETECTED = 0x01,
	FC_SCR_REG_FUNC_N_PORT_DETECTED = 0x02,
	FC_SCR_REG_FUNC_FULL = 0x03,
	FC_SCR_REG_FUNC_CLEAR_REG = 0xFF,
};

enum {
	FC_VU_SCR_REG_FUNC_FABRIC_NAME_CHANGE = 0x01
};

struct fc_scr_s {
	u32 command:8;
	u32 res:24;
	u32 vu_reg_func:8; 
	u32 res1:16;
	u32 reg_func:8;
};

enum {
	FC_CAT_NOP	= 0x0,
	FC_CAT_ABTS	= 0x1,
	FC_CAT_RMC	= 0x2,
	FC_CAT_BA_ACC	= 0x4,
	FC_CAT_BA_RJT	= 0x5,
	FC_CAT_PRMT	= 0x6,
};

struct fc_ls_rjt_s {
	struct fc_els_cmd_s els_cmd;	
	u32        res1:8;
	u32        reason_code:8;	
	u32        reason_code_expl:8;	
	u32        vendor_unique:8;	
};

enum {
	FC_LS_RJT_RSN_INV_CMD_CODE	= 0x01,
	FC_LS_RJT_RSN_LOGICAL_ERROR	= 0x03,
	FC_LS_RJT_RSN_LOGICAL_BUSY	= 0x05,
	FC_LS_RJT_RSN_PROTOCOL_ERROR	= 0x07,
	FC_LS_RJT_RSN_UNABLE_TO_PERF_CMD = 0x09,
	FC_LS_RJT_RSN_CMD_NOT_SUPP	= 0x0B,
};

enum {
	FC_LS_RJT_EXP_NO_ADDL_INFO		= 0x00,
	FC_LS_RJT_EXP_SPARMS_ERR_OPTIONS	= 0x01,
	FC_LS_RJT_EXP_SPARMS_ERR_INI_CTL	= 0x03,
	FC_LS_RJT_EXP_SPARMS_ERR_REC_CTL	= 0x05,
	FC_LS_RJT_EXP_SPARMS_ERR_RXSZ		= 0x07,
	FC_LS_RJT_EXP_SPARMS_ERR_CONSEQ		= 0x09,
	FC_LS_RJT_EXP_SPARMS_ERR_CREDIT		= 0x0B,
	FC_LS_RJT_EXP_INV_PORT_NAME		= 0x0D,
	FC_LS_RJT_EXP_INV_NODE_FABRIC_NAME	= 0x0E,
	FC_LS_RJT_EXP_INV_CSP			= 0x0F,
	FC_LS_RJT_EXP_INV_ASSOC_HDR		= 0x11,
	FC_LS_RJT_EXP_ASSOC_HDR_REQD		= 0x13,
	FC_LS_RJT_EXP_INV_ORIG_S_ID		= 0x15,
	FC_LS_RJT_EXP_INV_OXID_RXID_COMB	= 0x17,
	FC_LS_RJT_EXP_CMD_ALREADY_IN_PROG	= 0x19,
	FC_LS_RJT_EXP_LOGIN_REQUIRED		= 0x1E,
	FC_LS_RJT_EXP_INVALID_NPORT_ID		= 0x1F,
	FC_LS_RJT_EXP_INSUFF_RES		= 0x29,
	FC_LS_RJT_EXP_CMD_NOT_SUPP		= 0x2C,
	FC_LS_RJT_EXP_INV_PAYLOAD_LEN		= 0x2D,
};

struct fc_rrq_s {
	struct fc_els_cmd_s els_cmd;	
	u32        res1:8;
	u32        s_id:24;	

	u32        ox_id:16;	
	u32        rx_id:16;	

	u32        res2[8];	
};

struct fc_ba_acc_s {
	u32        seq_id_valid:8;	
	u32        seq_id:8;		
	u32        res2:16;
	u32        ox_id:16;		
	u32        rx_id:16;		
	u32        low_seq_cnt:16;	
	u32        high_seq_cnt:16;	
};

struct fc_ba_rjt_s {
	u32        res1:8;		
	u32        reason_code:8;	
	u32        reason_expl:8;	
	u32        vendor_unique:8; 
};

struct fc_tprlo_params_page_s {
	u32        type:8;
	u32        type_ext:8;

#ifdef __BIG_ENDIAN
	u32        opa_valid:1;
	u32        rpa_valid:1;
	u32        tpo_nport_valid:1;
	u32        global_process_logout:1;
	u32        res1:12;
#else
	u32        res1:12;
	u32        global_process_logout:1;
	u32        tpo_nport_valid:1;
	u32        rpa_valid:1;
	u32        opa_valid:1;
#endif

	u32        orig_process_assc;
	u32        resp_process_assc;

	u32        res2:8;
	u32        tpo_nport_id;
};

struct fc_tprlo_s {
	u32        command:8;
	u32        page_len:8;
	u32        payload_len:16;

	struct fc_tprlo_params_page_s tprlo_params[1];
};

enum fc_tprlo_type {
	FC_GLOBAL_LOGO = 1,
	FC_TPR_LOGO
};

struct fc_tprlo_acc_s {
	u32	command:8;
	u32	page_len:8;
	u32	payload_len:16;
	struct fc_prlo_acc_params_page_s tprlo_acc_params[1];
};

#define FC_RSCN_PGLEN	0x4

enum fc_rscn_format {
	FC_RSCN_FORMAT_PORTID	= 0x0,
	FC_RSCN_FORMAT_AREA	= 0x1,
	FC_RSCN_FORMAT_DOMAIN	= 0x2,
	FC_RSCN_FORMAT_FABRIC	= 0x3,
};

struct fc_rscn_event_s {
	u32	format:2;
	u32	qualifier:4;
	u32	resvd:2;
	u32	portid:24;
};

struct fc_rscn_pl_s {
	u8	command;
	u8	pagelen;
	__be16	payldlen;
	struct fc_rscn_event_s event[1];
};

struct fc_echo_s {
	struct fc_els_cmd_s els_cmd;
};

#define RNID_NODEID_DATA_FORMAT_COMMON			0x00
#define RNID_NODEID_DATA_FORMAT_FCP3			0x08
#define RNID_NODEID_DATA_FORMAT_DISCOVERY		0xDF

#define RNID_ASSOCIATED_TYPE_UNKNOWN			0x00000001
#define RNID_ASSOCIATED_TYPE_OTHER                      0x00000002
#define RNID_ASSOCIATED_TYPE_HUB                        0x00000003
#define RNID_ASSOCIATED_TYPE_SWITCH                     0x00000004
#define RNID_ASSOCIATED_TYPE_GATEWAY                    0x00000005
#define RNID_ASSOCIATED_TYPE_STORAGE_DEVICE             0x00000009
#define RNID_ASSOCIATED_TYPE_HOST                       0x0000000A
#define RNID_ASSOCIATED_TYPE_STORAGE_SUBSYSTEM          0x0000000B
#define RNID_ASSOCIATED_TYPE_STORAGE_ACCESS_DEVICE      0x0000000E
#define RNID_ASSOCIATED_TYPE_NAS_SERVER                 0x00000011
#define RNID_ASSOCIATED_TYPE_BRIDGE                     0x00000002
#define RNID_ASSOCIATED_TYPE_VIRTUALIZATION_DEVICE      0x00000003
#define RNID_ASSOCIATED_TYPE_MULTI_FUNCTION_DEVICE      0x000000FF

struct fc_rnid_cmd_s {
	struct fc_els_cmd_s els_cmd;
	u32        node_id_data_format:8;
	u32        reserved:24;
};


struct fc_rnid_common_id_data_s {
	wwn_t		port_name;
	wwn_t           node_name;
};

struct fc_rnid_general_topology_data_s {
	u32        vendor_unique[4];
	__be32     asso_type;
	u32        phy_port_num;
	__be32     num_attached_nodes;
	u32        node_mgmt:8;
	u32        ip_version:8;
	u32        udp_tcp_port_num:16;
	u32        ip_address[4];
	u32        reserved:16;
	u32        vendor_specific:16;
};

struct fc_rnid_acc_s {
	struct fc_els_cmd_s els_cmd;
	u32        node_id_data_format:8;
	u32        common_id_data_length:8;
	u32        reserved:8;
	u32        specific_id_data_length:8;
	struct fc_rnid_common_id_data_s common_id_data;
	struct fc_rnid_general_topology_data_s gen_topology_data;
};

#define RNID_ASSOCIATED_TYPE_UNKNOWN                    0x00000001
#define RNID_ASSOCIATED_TYPE_OTHER                      0x00000002
#define RNID_ASSOCIATED_TYPE_HUB                        0x00000003
#define RNID_ASSOCIATED_TYPE_SWITCH                     0x00000004
#define RNID_ASSOCIATED_TYPE_GATEWAY                    0x00000005
#define RNID_ASSOCIATED_TYPE_STORAGE_DEVICE             0x00000009
#define RNID_ASSOCIATED_TYPE_HOST                       0x0000000A
#define RNID_ASSOCIATED_TYPE_STORAGE_SUBSYSTEM          0x0000000B
#define RNID_ASSOCIATED_TYPE_STORAGE_ACCESS_DEVICE      0x0000000E
#define RNID_ASSOCIATED_TYPE_NAS_SERVER                 0x00000011
#define RNID_ASSOCIATED_TYPE_BRIDGE                     0x00000002
#define RNID_ASSOCIATED_TYPE_VIRTUALIZATION_DEVICE      0x00000003
#define RNID_ASSOCIATED_TYPE_MULTI_FUNCTION_DEVICE      0x000000FF

enum fc_rpsc_speed_cap {
	RPSC_SPEED_CAP_1G = 0x8000,
	RPSC_SPEED_CAP_2G = 0x4000,
	RPSC_SPEED_CAP_4G = 0x2000,
	RPSC_SPEED_CAP_10G = 0x1000,
	RPSC_SPEED_CAP_8G = 0x0800,
	RPSC_SPEED_CAP_16G = 0x0400,

	RPSC_SPEED_CAP_UNKNOWN = 0x0001,
};

enum fc_rpsc_op_speed {
	RPSC_OP_SPEED_1G = 0x8000,
	RPSC_OP_SPEED_2G = 0x4000,
	RPSC_OP_SPEED_4G = 0x2000,
	RPSC_OP_SPEED_10G = 0x1000,
	RPSC_OP_SPEED_8G = 0x0800,
	RPSC_OP_SPEED_16G = 0x0400,

	RPSC_OP_SPEED_NOT_EST = 0x0001,	
};

struct fc_rpsc_speed_info_s {
	__be16        port_speed_cap;	
	__be16        port_op_speed;	
};

struct fc_rpsc_cmd_s {
	struct fc_els_cmd_s els_cmd;
};

struct fc_rpsc_acc_s {
	u32        command:8;
	u32        rsvd:8;
	u32        num_entries:16;

	struct fc_rpsc_speed_info_s speed_info[1];
};

#define FC_BRCD_TOKEN    0x42524344

struct fc_rpsc2_cmd_s {
	struct fc_els_cmd_s els_cmd;
	__be32	token;
	u16	resvd;
	__be16	num_pids;		
	struct  {
		u32	rsvd1:8;
		u32	pid:24;		
	} pid_list[1];
};

enum fc_rpsc2_port_type {
	RPSC2_PORT_TYPE_UNKNOWN = 0,
	RPSC2_PORT_TYPE_NPORT   = 1,
	RPSC2_PORT_TYPE_NLPORT  = 2,
	RPSC2_PORT_TYPE_NPIV_PORT  = 0x5f,
	RPSC2_PORT_TYPE_NPORT_TRUNK  = 0x6f,
};

struct fc_rpsc2_port_info_s {
	__be32	pid;		
	u16	resvd1;
	__be16	index;		
	u8	resvd2;
	u8	type;		
	__be16	speed;		
};

struct fc_rpsc2_acc_s {
	u8        els_cmd;
	u8        resvd;
	__be16    num_pids; 
	struct fc_rpsc2_port_info_s port_info[1]; 
};

enum fc_cos {
	FC_CLASS_2	= 0x04,
	FC_CLASS_3	= 0x08,
	FC_CLASS_2_3	= 0x0C,
};

struct fc_symname_s {
	u8         symname[FC_SYMNAME_MAX];
};

#define FC_ED_TOV	2
#define FC_REC_TOV	(FC_ED_TOV + 1)
#define FC_RA_TOV	10
#define FC_ELS_TOV	((2 * FC_RA_TOV) + 1)
#define FC_FCCT_TOV	(3 * FC_RA_TOV)

#define FC_VF_ID_NULL    0	
#define FC_VF_ID_MIN     1
#define FC_VF_ID_MAX     0xEFF
#define FC_VF_ID_CTL     0xFEF	

struct fc_vft_s {
	u32        r_ctl:8;
	u32        ver:2;
	u32        type:4;
	u32        res_a:2;
	u32        priority:3;
	u32        vf_id:12;
	u32        res_b:1;
	u32        hopct:8;
	u32        res_c:24;
};

#define FCP_CMND_CDB_LEN    16
#define FCP_CMND_LUN_LEN    8

struct fcp_cmnd_s {
	struct scsi_lun	lun;		
	u8		crn;		
#ifdef __BIG_ENDIAN
	u8		resvd:1,
			priority:4,	
			taskattr:3;	
#else
	u8		taskattr:3,	
			priority:4,	
			resvd:1;
#endif
	u8		tm_flags;	
#ifdef __BIG_ENDIAN
	u8		addl_cdb_len:6,	
			iodir:2;	
#else
	u8		iodir:2,	
			addl_cdb_len:6;	
#endif
	struct scsi_cdb_s      cdb;

	__be32        fcp_dl;	
};

#define fcp_cmnd_cdb_len(_cmnd) ((_cmnd)->addl_cdb_len * 4 + FCP_CMND_CDB_LEN)
#define fcp_cmnd_fcpdl(_cmnd)	((&(_cmnd)->fcp_dl)[(_cmnd)->addl_cdb_len])

enum fcp_iodir {
	FCP_IODIR_NONE  = 0,
	FCP_IODIR_WRITE = 1,
	FCP_IODIR_READ  = 2,
	FCP_IODIR_RW    = 3,
};

enum fcp_tm_cmnd {
	FCP_TM_ABORT_TASK_SET	= BIT(1),
	FCP_TM_CLEAR_TASK_SET	= BIT(2),
	FCP_TM_LUN_RESET	= BIT(4),
	FCP_TM_TARGET_RESET	= BIT(5),	
	FCP_TM_CLEAR_ACA	= BIT(6),
};

enum fcp_residue {
	FCP_NO_RESIDUE = 0,     
	FCP_RESID_OVER = 1,     
	FCP_RESID_UNDER = 2,    
};

struct fcp_rspinfo_s {
	u32        res0:24;
	u32        rsp_code:8;		
	u32        res1;
};

struct fcp_resp_s {
	u32        reserved[2];		
	u16        reserved2;
#ifdef __BIG_ENDIAN
	u8         reserved3:3;
	u8         fcp_conf_req:1;	
	u8         resid_flags:2;	
	u8         sns_len_valid:1;	
	u8         rsp_len_valid:1;	
#else
	u8         rsp_len_valid:1;	
	u8         sns_len_valid:1;	
	u8         resid_flags:2;	
	u8         fcp_conf_req:1;	
	u8         reserved3:3;
#endif
	u8         scsi_status;		
	u32        residue;		
	u32        sns_len;		
	u32        rsp_len;		
};

#define fcp_snslen(__fcprsp)	((__fcprsp)->sns_len_valid ?		\
					(__fcprsp)->sns_len : 0)
#define fcp_rsplen(__fcprsp)	((__fcprsp)->rsp_len_valid ?		\
					(__fcprsp)->rsp_len : 0)
#define fcp_rspinfo(__fcprsp)	((struct fcp_rspinfo_s *)((__fcprsp) + 1))
#define fcp_snsinfo(__fcprsp)	(((u8 *)fcp_rspinfo(__fcprsp)) +	\
						fcp_rsplen(__fcprsp))
struct ct_hdr_s {
	u32	rev_id:8;	
	u32	in_id:24;	
	u32	gs_type:8;	
	u32	gs_sub_type:8;	
	u32	options:8;	
	u32	rsvrd:8;	
	u32	cmd_rsp_code:16;
	u32	max_res_size:16;
	u32	frag_id:8;	
	u32	reason_code:8;	
	u32	exp_code:8;	
	u32	vendor_unq:8;	
};

enum {
	CT_GS3_REVISION = 0x01,
};

enum {
	CT_GSTYPE_KEYSERVICE	= 0xF7,
	CT_GSTYPE_ALIASSERVICE	= 0xF8,
	CT_GSTYPE_MGMTSERVICE	= 0xFA,
	CT_GSTYPE_TIMESERVICE	= 0xFB,
	CT_GSTYPE_DIRSERVICE	= 0xFC,
};

enum {
	CT_GSSUBTYPE_NAMESERVER = 0x02,
};

enum {
	CT_GSSUBTYPE_CFGSERVER	= 0x01,
	CT_GSSUBTYPE_UNZONED_NS = 0x02,
	CT_GSSUBTYPE_ZONESERVER = 0x03,
	CT_GSSUBTYPE_LOCKSERVER = 0x04,
	CT_GSSUBTYPE_HBA_MGMTSERVER = 0x10,	
};

enum {
	CT_RSP_REJECT = 0x8001,
	CT_RSP_ACCEPT = 0x8002,
};

enum {
	CT_RSN_INV_CMD		= 0x01,
	CT_RSN_INV_VER		= 0x02,
	CT_RSN_LOGIC_ERR	= 0x03,
	CT_RSN_INV_SIZE		= 0x04,
	CT_RSN_LOGICAL_BUSY	= 0x05,
	CT_RSN_PROTO_ERR	= 0x07,
	CT_RSN_UNABLE_TO_PERF	= 0x09,
	CT_RSN_NOT_SUPP		= 0x0B,
	CT_RSN_SERVER_NOT_AVBL  = 0x0D,
	CT_RSN_SESSION_COULD_NOT_BE_ESTBD = 0x0E,
	CT_RSN_VENDOR_SPECIFIC  = 0xFF,

};

enum {
	CT_NS_EXP_NOADDITIONAL	= 0x00,
	CT_NS_EXP_ID_NOT_REG	= 0x01,
	CT_NS_EXP_PN_NOT_REG	= 0x02,
	CT_NS_EXP_NN_NOT_REG	= 0x03,
	CT_NS_EXP_CS_NOT_REG	= 0x04,
	CT_NS_EXP_IPN_NOT_REG	= 0x05,
	CT_NS_EXP_IPA_NOT_REG	= 0x06,
	CT_NS_EXP_FT_NOT_REG	= 0x07,
	CT_NS_EXP_SPN_NOT_REG	= 0x08,
	CT_NS_EXP_SNN_NOT_REG	= 0x09,
	CT_NS_EXP_PT_NOT_REG	= 0x0A,
	CT_NS_EXP_IPP_NOT_REG	= 0x0B,
	CT_NS_EXP_FPN_NOT_REG	= 0x0C,
	CT_NS_EXP_HA_NOT_REG	= 0x0D,
	CT_NS_EXP_FD_NOT_REG	= 0x0E,
	CT_NS_EXP_FF_NOT_REG	= 0x0F,
	CT_NS_EXP_ACCESSDENIED	= 0x10,
	CT_NS_EXP_UNACCEPTABLE_ID = 0x11,
	CT_NS_EXP_DATABASEEMPTY		= 0x12,
	CT_NS_EXP_NOT_REG_IN_SCOPE	= 0x13,
	CT_NS_EXP_DOM_ID_NOT_PRESENT	= 0x14,
	CT_NS_EXP_PORT_NUM_NOT_PRESENT	= 0x15,
	CT_NS_EXP_NO_DEVICE_ATTACHED	= 0x16
};

enum {
	CT_EXP_AUTH_EXCEPTION		= 0xF1,
	CT_EXP_DB_FULL			= 0xF2,
	CT_EXP_DB_EMPTY			= 0xF3,
	CT_EXP_PROCESSING_REQ		= 0xF4,
	CT_EXP_UNABLE_TO_VERIFY_CONN	= 0xF5,
	CT_EXP_DEVICES_NOT_IN_CMN_ZONE  = 0xF6
};

enum {
	GS_GID_PN	= 0x0121,	
	GS_GPN_ID	= 0x0112,	
	GS_GNN_ID	= 0x0113,	
	GS_GID_FT	= 0x0171,	
	GS_GSPN_ID	= 0x0118,	
	GS_RFT_ID	= 0x0217,	
	GS_RSPN_ID	= 0x0218,	
	GS_RPN_ID	= 0x0212,	
	GS_RNN_ID	= 0x0213,	
	GS_RCS_ID	= 0x0214,	
	GS_RPT_ID	= 0x021A,	
	GS_GA_NXT	= 0x0100,	
	GS_RFF_ID	= 0x021F,	
};

struct fcgs_id_req_s {
	u32 rsvd:8;
	u32 dap:24; 
};
#define fcgs_gpnid_req_t struct fcgs_id_req_s
#define fcgs_gnnid_req_t struct fcgs_id_req_s
#define fcgs_gspnid_req_t struct fcgs_id_req_s

struct fcgs_gidpn_req_s {
	wwn_t	port_name;	
};

struct fcgs_gidpn_resp_s {
	u32	rsvd:8;
	u32	dap:24;		
};

struct fcgs_rftid_req_s {
	u32	rsvd:8;
	u32	dap:24;		
	__be32	fc4_type[8];	
};

#define FC_GS_FCP_FC4_FEATURE_INITIATOR  0x02
#define FC_GS_FCP_FC4_FEATURE_TARGET	 0x01

struct fcgs_rffid_req_s {
	u32	rsvd:8;
	u32	dap:24;		
	u32	rsvd1:16;
	u32	fc4ftr_bits:8;	
	u32	fc4_type:8;		
};

struct fcgs_gidft_req_s {
	u8	reserved;
	u8	domain_id;	
	u8	area_id;	
	u8	fc4_type;	
};

struct fcgs_gidft_resp_s {
	u8	last:1;		
	u8	reserved:7;
	u32	pid:24;		
};

struct fcgs_rspnid_req_s {
	u32	rsvd:8;
	u32	dap:24;		
	u8	spn_len;	
	u8	spn[256];	
};

struct fcgs_rpnid_req_s {
	u32	rsvd:8;
	u32	port_id:24;
	wwn_t	port_name;
};

struct fcgs_rnnid_req_s {
	u32	rsvd:8;
	u32	port_id:24;
	wwn_t	node_name;
};

struct fcgs_rcsid_req_s {
	u32	rsvd:8;
	u32	port_id:24;
	u32	cos;
};

struct fcgs_rptid_req_s {
	u32	rsvd:8;
	u32	port_id:24;
	u32	port_type:8;
	u32	rsvd1:24;
};

struct fcgs_ganxt_req_s {
	u32	rsvd:8;
	u32	port_id:24;
};

struct fcgs_ganxt_rsp_s {
	u32		port_type:8;	
	u32		port_id:24;	
	wwn_t		port_name;	
	u8		spn_len;	
	char		spn[255];	
	wwn_t		node_name;	
	u8		snn_len;	
	char		snn[255];	
	u8		ipa[8];		
	u8		ip[16];		
	u32		cos;		
	u32		fc4types[8];	
	wwn_t		fabric_port_name; 
	u32		rsvd:8;		
	u32		hard_addr:24;	
};

enum {
	GS_FC_GFN_CMD	= 0x0114,	
	GS_FC_GMAL_CMD	= 0x0116,	
	GS_FC_TRACE_CMD	= 0x0400,	
	GS_FC_PING_CMD	= 0x0401,	
};

#define CT_GMAL_RESP_PREFIX_TELNET	 "telnet://"
#define CT_GMAL_RESP_PREFIX_HTTP	 "http://"

struct fcgs_req_s {
	wwn_t    wwn;   
};

#define fcgs_gmal_req_t struct fcgs_req_s
#define fcgs_gfn_req_t struct fcgs_req_s

struct fcgs_gmal_resp_s {
	__be32	ms_len;   
	u8	ms_ma[256];
};

struct fcgs_gmal_entry_s {
	u8  len;
	u8  prefix[7]; 
	u8  ip_addr[248];
};

#define	FDMI_GRHL		0x0100
#define	FDMI_GHAT		0x0101
#define	FDMI_GRPL		0x0102
#define	FDMI_GPAT		0x0110
#define	FDMI_RHBA		0x0200
#define	FDMI_RHAT		0x0201
#define	FDMI_RPRT		0x0210
#define	FDMI_RPA		0x0211
#define	FDMI_DHBA		0x0300
#define	FDMI_DPRT		0x0310

#define	FDMI_NO_ADDITIONAL_EXP		0x00
#define	FDMI_HBA_ALREADY_REG		0x10
#define	FDMI_HBA_ATTRIB_NOT_REG		0x11
#define	FDMI_HBA_ATTRIB_MULTIPLE	0x12
#define	FDMI_HBA_ATTRIB_LENGTH_INVALID	0x13
#define	FDMI_HBA_ATTRIB_NOT_PRESENT	0x14
#define	FDMI_PORT_ORIG_NOT_IN_LIST	0x15
#define	FDMI_PORT_HBA_NOT_IN_LIST	0x16
#define	FDMI_PORT_ATTRIB_NOT_REG	0x20
#define	FDMI_PORT_NOT_REG		0x21
#define	FDMI_PORT_ATTRIB_MULTIPLE	0x22
#define	FDMI_PORT_ATTRIB_LENGTH_INVALID	0x23
#define	FDMI_PORT_ALREADY_REGISTEREED	0x24

#define	FDMI_TRANS_SPEED_1G		0x00000001
#define	FDMI_TRANS_SPEED_2G		0x00000002
#define	FDMI_TRANS_SPEED_10G		0x00000004
#define	FDMI_TRANS_SPEED_4G		0x00000008
#define	FDMI_TRANS_SPEED_8G		0x00000010
#define	FDMI_TRANS_SPEED_16G		0x00000020
#define	FDMI_TRANS_SPEED_UNKNOWN	0x00008000

enum fdmi_hba_attribute_type {
	FDMI_HBA_ATTRIB_NODENAME = 1,	
	FDMI_HBA_ATTRIB_MANUFACTURER,	
	FDMI_HBA_ATTRIB_SERIALNUM,	
	FDMI_HBA_ATTRIB_MODEL,		
	FDMI_HBA_ATTRIB_MODEL_DESC,	
	FDMI_HBA_ATTRIB_HW_VERSION,	
	FDMI_HBA_ATTRIB_DRIVER_VERSION,	
	FDMI_HBA_ATTRIB_ROM_VERSION,	
	FDMI_HBA_ATTRIB_FW_VERSION,	
	FDMI_HBA_ATTRIB_OS_NAME,	
	FDMI_HBA_ATTRIB_MAX_CT,		

	FDMI_HBA_ATTRIB_MAX_TYPE
};

enum fdmi_port_attribute_type {
	FDMI_PORT_ATTRIB_FC4_TYPES = 1,	
	FDMI_PORT_ATTRIB_SUPP_SPEED,	
	FDMI_PORT_ATTRIB_PORT_SPEED,	
	FDMI_PORT_ATTRIB_FRAME_SIZE,	
	FDMI_PORT_ATTRIB_DEV_NAME,	
	FDMI_PORT_ATTRIB_HOST_NAME,	

	FDMI_PORT_ATTR_MAX_TYPE
};

struct fdmi_attr_s {
	__be16        type;
	__be16        len;
	u8         value[1];
};

struct fdmi_hba_attr_s {
	__be32        attr_count;	
	struct fdmi_attr_s hba_attr;	
};

struct fdmi_port_list_s {
	__be32		num_ports;	
	wwn_t		port_entry;	
};

struct fdmi_port_attr_s {
	__be32        attr_count;	
	struct fdmi_attr_s port_attr;	
};

struct fdmi_rhba_s {
	wwn_t			hba_id;		
	struct fdmi_port_list_s port_list;	
	struct fdmi_hba_attr_s hba_attr_blk;	
};

struct fdmi_rprt_s {
	wwn_t			hba_id;		
	wwn_t			port_name;	
	struct fdmi_port_attr_s port_attr_blk;	
};

struct fdmi_rpa_s {
	wwn_t			port_name;	
	struct fdmi_port_attr_s port_attr_blk;	
};

#pragma pack()

#endif	
