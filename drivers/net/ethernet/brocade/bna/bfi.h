/*
 * Linux network driver for Brocade Converged Network Adapter.
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
/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 */
#ifndef __BFI_H__
#define __BFI_H__

#include "bfa_defs.h"

#pragma pack(1)

#define	BFI_FLASH_CHUNK_SZ			256	
#define	BFI_FLASH_CHUNK_SZ_WORDS	(BFI_FLASH_CHUNK_SZ/sizeof(u32))

struct bfi_mhdr {
	u8		msg_class;	
	u8		msg_id;		
	union {
		struct {
			u8	qid;
			u8	fn_lpu;	
		} h2i;
		u16	i2htok;	
	} mtag;
};

#define bfi_fn_lpu(__fn, __lpu)	((__fn) << 1 | (__lpu))
#define bfi_mhdr_2_fn(_mh)	((_mh)->mtag.h2i.fn_lpu >> 1)
#define bfi_mhdr_2_qid(_mh)	((_mh)->mtag.h2i.qid)

#define bfi_h2i_set(_mh, _mc, _op, _fn_lpu) do {		\
	(_mh).msg_class			= (_mc);		\
	(_mh).msg_id			= (_op);		\
	(_mh).mtag.h2i.fn_lpu	= (_fn_lpu);			\
} while (0)

#define bfi_i2h_set(_mh, _mc, _op, _i2htok) do {		\
	(_mh).msg_class			= (_mc);		\
	(_mh).msg_id			= (_op);		\
	(_mh).mtag.i2htok		= (_i2htok);		\
} while (0)

#define BFI_I2H_OPCODE_BASE	128
#define BFA_I2HM(_x)			((_x) + BFI_I2H_OPCODE_BASE)


union bfi_addr_u {
	struct {
		u32	addr_lo;
		u32	addr_hi;
	} a32;
};

struct bfi_alen {
	union bfi_addr_u	al_addr;	
	u32			al_len;		
};

#define BFI_LMSG_SZ		128
#define BFI_LMSG_PL_WSZ	\
			((BFI_LMSG_SZ - sizeof(struct bfi_mhdr)) / 4)

#define BFI_MBMSG_SZ		7
struct bfi_mbmsg {
	struct bfi_mhdr mh;
	u32		pl[BFI_MBMSG_SZ];
};

enum bfi_pcifn_class {
	BFI_PCIFN_CLASS_FC	= 0x0c04,
	BFI_PCIFN_CLASS_ETH	= 0x0200,
};

enum bfi_mclass {
	BFI_MC_IOC		= 1,	
	BFI_MC_DIAG		= 2,	
	BFI_MC_FLASH		= 3,	
	BFI_MC_CEE		= 4,	
	BFI_MC_FCPORT		= 5,	
	BFI_MC_IOCFC		= 6,	
	BFI_MC_LL		= 7,	
	BFI_MC_UF		= 8,	
	BFI_MC_FCXP		= 9,	
	BFI_MC_LPS		= 10,	
	BFI_MC_RPORT		= 11,	
	BFI_MC_ITNIM		= 12,	
	BFI_MC_IOIM_READ	= 13,	
	BFI_MC_IOIM_WRITE	= 14,	
	BFI_MC_IOIM_IO		= 15,	
	BFI_MC_IOIM		= 16,	
	BFI_MC_IOIM_IOCOM	= 17,	
	BFI_MC_TSKIM		= 18,	
	BFI_MC_SBOOT		= 19,	
	BFI_MC_IPFC		= 20,	
	BFI_MC_PORT		= 21,	
	BFI_MC_SFP		= 22,	
	BFI_MC_MSGQ		= 23,	
	BFI_MC_ENET		= 24,	
	BFI_MC_PHY		= 25,	
	BFI_MC_NBOOT		= 26,	
	BFI_MC_TIO_READ		= 27,	
	BFI_MC_TIO_WRITE	= 28,	
	BFI_MC_TIO_DATA_XFERED	= 29,	
	BFI_MC_TIO_IO		= 30,	
	BFI_MC_TIO		= 31,	
	BFI_MC_MFG		= 32,	
	BFI_MC_EDMA		= 33,	
	BFI_MC_MAX		= 34
};

#define BFI_IOC_MSGLEN_MAX	32	

#define BFI_FWBOOT_ENV_OS		0


enum bfi_asic_gen {
	BFI_ASIC_GEN_CB		= 1,
	BFI_ASIC_GEN_CT		= 2,
	BFI_ASIC_GEN_CT2	= 3,
};

enum bfi_asic_mode {
	BFI_ASIC_MODE_FC	= 1,	
	BFI_ASIC_MODE_FC16	= 2,	
	BFI_ASIC_MODE_ETH	= 3,	
	BFI_ASIC_MODE_COMBO	= 4,	
};

enum bfi_ioc_h2i_msgs {
	BFI_IOC_H2I_ENABLE_REQ		= 1,
	BFI_IOC_H2I_DISABLE_REQ		= 2,
	BFI_IOC_H2I_GETATTR_REQ		= 3,
	BFI_IOC_H2I_DBG_SYNC		= 4,
	BFI_IOC_H2I_DBG_DUMP		= 5,
};

enum bfi_ioc_i2h_msgs {
	BFI_IOC_I2H_ENABLE_REPLY	= BFA_I2HM(1),
	BFI_IOC_I2H_DISABLE_REPLY	= BFA_I2HM(2),
	BFI_IOC_I2H_GETATTR_REPLY	= BFA_I2HM(3),
	BFI_IOC_I2H_HBEAT		= BFA_I2HM(4),
};

struct bfi_ioc_getattr_req {
	struct bfi_mhdr mh;
	union bfi_addr_u	attr_addr;
};

struct bfi_ioc_attr {
	u64		mfg_pwwn;	
	u64		mfg_nwwn;	
	mac_t		mfg_mac;	
	u8		port_mode;	
	u8		rsvd_a;
	u64		pwwn;
	u64		nwwn;
	mac_t		mac;		
	u16	rsvd_b;
	mac_t		fcoe_mac;
	u16	rsvd_c;
	char		brcd_serialnum[STRSZ(BFA_MFG_SERIALNUM_SIZE)];
	u8		pcie_gen;
	u8		pcie_lanes_orig;
	u8		pcie_lanes;
	u8		rx_bbcredit;	
	u32	adapter_prop;	
	u16	maxfrsize;	
	char		asic_rev;
	u8		rsvd_d;
	char		fw_version[BFA_VERSION_LEN];
	char		optrom_version[BFA_VERSION_LEN];
	struct bfa_mfg_vpd vpd;
	u32	card_type;	
};

struct bfi_ioc_getattr_reply {
	struct bfi_mhdr mh;	
	u8			status;	
	u8			rsvd[3];
};

#define BFI_IOC_SMEM_PG0_CB	(0x40)
#define BFI_IOC_SMEM_PG0_CT	(0x180)

#define BFI_IOC_FWSTATS_OFF	(0x6B40)
#define BFI_IOC_FWSTATS_SZ	(4096)

#define BFI_IOC_TRC_OFF		(0x4b00)
#define BFI_IOC_TRC_ENTS	256
#define BFI_IOC_TRC_ENT_SZ	16
#define BFI_IOC_TRC_HDR_SZ	32

#define BFI_IOC_FW_SIGNATURE	(0xbfadbfad)
#define BFI_IOC_MD5SUM_SZ	4
struct bfi_ioc_image_hdr {
	u32	signature;	
	u8	asic_gen;	
	u8	asic_mode;
	u8	port0_mode;	
	u8	port1_mode;	
	u32	exec;		
	u32	bootenv;	
	u32	rsvd_b[4];
	u32	md5sum[BFI_IOC_MD5SUM_SZ];
};

#define BFI_FWBOOT_DEVMODE_OFF		4
#define BFI_FWBOOT_TYPE_OFF		8
#define BFI_FWBOOT_ENV_OFF		12
#define BFI_FWBOOT_DEVMODE(__asic_gen, __asic_mode, __p0_mode, __p1_mode) \
	(((u32)(__asic_gen)) << 24 |	\
	 ((u32)(__asic_mode)) << 16 |	\
	 ((u32)(__p0_mode)) << 8 |	\
	 ((u32)(__p1_mode)))

enum bfi_fwboot_type {
	BFI_FWBOOT_TYPE_NORMAL	= 0,
	BFI_FWBOOT_TYPE_FLASH	= 1,
	BFI_FWBOOT_TYPE_MEMTEST	= 2,
};

enum bfi_port_mode {
	BFI_PORT_MODE_FC	= 1,
	BFI_PORT_MODE_ETH	= 2,
};

struct bfi_ioc_hbeat {
	struct bfi_mhdr mh;		
	u32	   hb_count;	
};

enum bfi_ioc_state {
	BFI_IOC_UNINIT		= 0,	
	BFI_IOC_INITING		= 1,	
	BFI_IOC_HWINIT		= 2,	
	BFI_IOC_CFG		= 3,	
	BFI_IOC_OP		= 4,	
	BFI_IOC_DISABLING	= 5,	
	BFI_IOC_DISABLED	= 6,	
	BFI_IOC_CFG_DISABLED	= 7,	
	BFI_IOC_FAIL		= 8,	
	BFI_IOC_MEMTEST		= 9,	
};

#define BFI_IOC_ENDIAN_SIG  0x12345678

enum {
	BFI_ADAPTER_TYPE_FC	= 0x01,		
	BFI_ADAPTER_TYPE_MK	= 0x0f0000,	
	BFI_ADAPTER_TYPE_SH	= 16,	        
	BFI_ADAPTER_NPORTS_MK	= 0xff00,	
	BFI_ADAPTER_NPORTS_SH	= 8,	        
	BFI_ADAPTER_SPEED_MK	= 0xff,		
	BFI_ADAPTER_SPEED_SH	= 0,	        
	BFI_ADAPTER_PROTO	= 0x100000,	
	BFI_ADAPTER_TTV		= 0x200000,	
	BFI_ADAPTER_UNSUPP	= 0x400000,	
};

#define BFI_ADAPTER_GETP(__prop, __adap_prop)			\
	(((__adap_prop) & BFI_ADAPTER_ ## __prop ## _MK) >>	\
		BFI_ADAPTER_ ## __prop ## _SH)
#define BFI_ADAPTER_SETP(__prop, __val)				\
	((__val) << BFI_ADAPTER_ ## __prop ## _SH)
#define BFI_ADAPTER_IS_PROTO(__adap_type)			\
	((__adap_type) & BFI_ADAPTER_PROTO)
#define BFI_ADAPTER_IS_TTV(__adap_type)				\
	((__adap_type) & BFI_ADAPTER_TTV)
#define BFI_ADAPTER_IS_UNSUPP(__adap_type)			\
	((__adap_type) & BFI_ADAPTER_UNSUPP)
#define BFI_ADAPTER_IS_SPECIAL(__adap_type)			\
	((__adap_type) & (BFI_ADAPTER_TTV | BFI_ADAPTER_PROTO |	\
			BFI_ADAPTER_UNSUPP))

struct bfi_ioc_ctrl_req {
	struct bfi_mhdr mh;
	u16			clscode;
	u16			rsvd;
	u32		tv_sec;
};

struct bfi_ioc_ctrl_reply {
	struct bfi_mhdr mh;			
	u8			status;		
	u8			port_mode;	
	u8			cap_bm;		
	u8			rsvd;
};

#define BFI_IOC_MSGSZ   8
union bfi_ioc_h2i_msg_u {
	struct bfi_mhdr mh;
	struct bfi_ioc_ctrl_req enable_req;
	struct bfi_ioc_ctrl_req disable_req;
	struct bfi_ioc_getattr_req getattr_req;
	u32			mboxmsg[BFI_IOC_MSGSZ];
};

union bfi_ioc_i2h_msg_u {
	struct bfi_mhdr mh;
	struct bfi_ioc_ctrl_reply fw_event;
	u32			mboxmsg[BFI_IOC_MSGSZ];
};


enum bfi_msgq_h2i_msgs {
	BFI_MSGQ_H2I_INIT_REQ	   = 1,
	BFI_MSGQ_H2I_DOORBELL_PI	= 2,
	BFI_MSGQ_H2I_DOORBELL_CI	= 3,
	BFI_MSGQ_H2I_CMDQ_COPY_RSP      = 4,
};

enum bfi_msgq_i2h_msgs {
	BFI_MSGQ_I2H_INIT_RSP	   = BFA_I2HM(BFI_MSGQ_H2I_INIT_REQ),
	BFI_MSGQ_I2H_DOORBELL_PI	= BFA_I2HM(BFI_MSGQ_H2I_DOORBELL_PI),
	BFI_MSGQ_I2H_DOORBELL_CI	= BFA_I2HM(BFI_MSGQ_H2I_DOORBELL_CI),
	BFI_MSGQ_I2H_CMDQ_COPY_REQ      = BFA_I2HM(BFI_MSGQ_H2I_CMDQ_COPY_RSP),
};

struct bfi_msgq_mhdr {
	u8	msg_class;
	u8	msg_id;
	u16	msg_token;
	u16	num_entries;
	u8	enet_id;
	u8	rsvd[1];
};

#define bfi_msgq_mhdr_set(_mh, _mc, _mid, _tok, _enet_id) do {	\
	(_mh).msg_class	 = (_mc);	\
	(_mh).msg_id	    = (_mid);       \
	(_mh).msg_token	 = (_tok);       \
	(_mh).enet_id	   = (_enet_id);   \
} while (0)

#define BFI_MSGQ_CMD_ENTRY_SIZE	 (64)    
#define BFI_MSGQ_RSP_ENTRY_SIZE	 (64)    

#define bfi_msgq_num_cmd_entries(_size)				 \
	(((_size) + BFI_MSGQ_CMD_ENTRY_SIZE - 1) / BFI_MSGQ_CMD_ENTRY_SIZE)

struct bfi_msgq {
	union bfi_addr_u addr;
	u16 q_depth;     
	u8 rsvd[2];
};

struct bfi_msgq_cfg_req {
	struct bfi_mhdr mh;
	struct bfi_msgq cmdq;
	struct bfi_msgq rspq;
};

struct bfi_msgq_cfg_rsp {
	struct bfi_mhdr mh;
	u8 cmd_status;
	u8 rsvd[3];
};

struct bfi_msgq_h2i_db {
	struct bfi_mhdr mh;
	union {
		u16 cmdq_pi;
		u16 rspq_ci;
	} idx;
};

struct bfi_msgq_i2h_db {
	struct bfi_mhdr mh;
	union {
		u16 rspq_pi;
		u16 cmdq_ci;
	} idx;
};

#define BFI_CMD_COPY_SZ 28

struct bfi_msgq_h2i_cmdq_copy_rsp {
	struct bfi_mhdr mh;
	u8	      data[BFI_CMD_COPY_SZ];
};

struct bfi_msgq_i2h_cmdq_copy_req {
	struct bfi_mhdr mh;
	u16     offset;
	u16     len;
};

enum bfi_flash_h2i_msgs {
	BFI_FLASH_H2I_QUERY_REQ = 1,
	BFI_FLASH_H2I_ERASE_REQ = 2,
	BFI_FLASH_H2I_WRITE_REQ = 3,
	BFI_FLASH_H2I_READ_REQ = 4,
	BFI_FLASH_H2I_BOOT_VER_REQ = 5,
};

enum bfi_flash_i2h_msgs {
	BFI_FLASH_I2H_QUERY_RSP = BFA_I2HM(1),
	BFI_FLASH_I2H_ERASE_RSP = BFA_I2HM(2),
	BFI_FLASH_I2H_WRITE_RSP = BFA_I2HM(3),
	BFI_FLASH_I2H_READ_RSP = BFA_I2HM(4),
	BFI_FLASH_I2H_BOOT_VER_RSP = BFA_I2HM(5),
	BFI_FLASH_I2H_EVENT = BFA_I2HM(127),
};

struct bfi_flash_query_req {
	struct bfi_mhdr mh;   
	struct bfi_alen alen;
};

struct bfi_flash_write_req {
	struct bfi_mhdr mh;	
	struct bfi_alen alen;
	u32	type;   
	u8	instance; 
	u8	last;
	u8	rsv[2];
	u32	offset;
	u32	length;
};

struct bfi_flash_read_req {
	struct bfi_mhdr mh;	
	u32	type;		
	u8	instance;	
	u8	rsv[3];
	u32	offset;
	u32	length;
	struct bfi_alen alen;
};

struct bfi_flash_query_rsp {
	struct bfi_mhdr mh;	
	u32	status;
};

struct bfi_flash_read_rsp {
	struct bfi_mhdr mh;	
	u32	type;		
	u8	instance;	
	u8	rsv[3];
	u32	status;
	u32	length;
};

struct bfi_flash_write_rsp {
	struct bfi_mhdr mh;	
	u32	type;		
	u8	instance;	
	u8	rsv[3];
	u32	status;
	u32	length;
};

#pragma pack()

#endif 
