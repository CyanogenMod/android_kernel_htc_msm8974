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

#ifndef __BFA_DEFS_H__
#define __BFA_DEFS_H__

#include "bfa_fc.h"
#include "bfad_drv.h"

#define BFA_MFG_SERIALNUM_SIZE                  11
#define STRSZ(_n)                               (((_n) + 4) & ~3)

enum {
	BFA_MFG_TYPE_CB_MAX  = 825,      
	BFA_MFG_TYPE_FC8P2   = 825,      
	BFA_MFG_TYPE_FC8P1   = 815,      
	BFA_MFG_TYPE_FC4P2   = 425,      
	BFA_MFG_TYPE_FC4P1   = 415,      
	BFA_MFG_TYPE_CNA10P2 = 1020,     
	BFA_MFG_TYPE_CNA10P1 = 1010,     
	BFA_MFG_TYPE_JAYHAWK = 804,      
	BFA_MFG_TYPE_WANCHESE = 1007,    
	BFA_MFG_TYPE_ASTRA    = 807,     
	BFA_MFG_TYPE_LIGHTNING_P0 = 902, 
	BFA_MFG_TYPE_LIGHTNING = 1741,   
	BFA_MFG_TYPE_PROWLER_F = 1560,	 
	BFA_MFG_TYPE_PROWLER_N = 1410,	 
	BFA_MFG_TYPE_PROWLER_C = 1710,   
	BFA_MFG_TYPE_PROWLER_D = 1860,   
	BFA_MFG_TYPE_CHINOOK   = 1867,   
	BFA_MFG_TYPE_INVALID = 0,        
};

#pragma pack(1)

#define bfa_mfg_is_mezz(type) (( \
	(type) == BFA_MFG_TYPE_JAYHAWK || \
	(type) == BFA_MFG_TYPE_WANCHESE || \
	(type) == BFA_MFG_TYPE_ASTRA || \
	(type) == BFA_MFG_TYPE_LIGHTNING_P0 || \
	(type) == BFA_MFG_TYPE_LIGHTNING || \
	(type) == BFA_MFG_TYPE_CHINOOK))

#define bfa_mfg_is_old_wwn_mac_model(type) (( \
	(type) == BFA_MFG_TYPE_FC8P2 || \
	(type) == BFA_MFG_TYPE_FC8P1 || \
	(type) == BFA_MFG_TYPE_FC4P2 || \
	(type) == BFA_MFG_TYPE_FC4P1 || \
	(type) == BFA_MFG_TYPE_CNA10P2 || \
	(type) == BFA_MFG_TYPE_CNA10P1 || \
	(type) == BFA_MFG_TYPE_JAYHAWK || \
	(type) == BFA_MFG_TYPE_WANCHESE))

#define bfa_mfg_increment_wwn_mac(m, i)                         \
do {                                                            \
	u32 t = ((u32)(m)[0] << 16) | ((u32)(m)[1] << 8) | \
		(u32)(m)[2];  \
	t += (i);      \
	(m)[0] = (t >> 16) & 0xFF;                              \
	(m)[1] = (t >> 8) & 0xFF;                               \
	(m)[2] = t & 0xFF;                                      \
} while (0)

#define BFA_MFG_VPD_LEN                 512

enum {
	BFA_MFG_VPD_UNKNOWN     = 0,     
	BFA_MFG_VPD_IBM         = 1,     
	BFA_MFG_VPD_HP          = 2,     
	BFA_MFG_VPD_DELL        = 3,     
	BFA_MFG_VPD_PCI_IBM     = 0x08,  
	BFA_MFG_VPD_PCI_HP      = 0x10,  
	BFA_MFG_VPD_PCI_DELL    = 0x20,  
	BFA_MFG_VPD_PCI_BRCD    = 0xf8,  
};

struct bfa_mfg_vpd_s {
	u8              version;        
	u8              vpd_sig[3];     
	u8              chksum;         
	u8              vendor;         
	u8      len;            
	u8      rsv;
	u8              data[BFA_MFG_VPD_LEN];  
};

#pragma pack()

enum bfa_status {
	BFA_STATUS_OK		= 0,	
	BFA_STATUS_FAILED	= 1,	
	BFA_STATUS_EINVAL	= 2,	
	BFA_STATUS_ENOMEM	= 3,	
	BFA_STATUS_ETIMER	= 5,	
	BFA_STATUS_EPROTOCOL	= 6,	
	BFA_STATUS_SFP_UNSUPP	= 10,	
	BFA_STATUS_UNKNOWN_VFID = 11,	
	BFA_STATUS_DATACORRUPTED = 12,  
	BFA_STATUS_DEVBUSY	= 13,	
	BFA_STATUS_HDMA_FAILED  = 16,   
	BFA_STATUS_FLASH_BAD_LEN = 17,	
	BFA_STATUS_UNKNOWN_LWWN = 18,	
	BFA_STATUS_UNKNOWN_RWWN = 19,	
	BFA_STATUS_VPORT_EXISTS = 21,	
	BFA_STATUS_VPORT_MAX	= 22,	
	BFA_STATUS_UNSUPP_SPEED	= 23,	
	BFA_STATUS_INVLD_DFSZ	= 24,	
	BFA_STATUS_CMD_NOTSUPP  = 26,   
	BFA_STATUS_FABRIC_RJT	= 29,	
	BFA_STATUS_UNKNOWN_VWWN = 30,	
	BFA_STATUS_PORT_OFFLINE = 34,	
	BFA_STATUS_VPORT_WWN_BP	= 46,	
	BFA_STATUS_PORT_NOT_DISABLED = 47, 
	BFA_STATUS_NO_FCPIM_NEXUS = 52,	
	BFA_STATUS_IOC_FAILURE	= 56,	
	BFA_STATUS_INVALID_WWN	= 57,	
	BFA_STATUS_ADAPTER_ENABLED = 60, 
	BFA_STATUS_IOC_NON_OP   = 61,	
	BFA_STATUS_VERSION_FAIL = 70, 
	BFA_STATUS_DIAG_BUSY	= 71,	
	BFA_STATUS_BEACON_ON    = 72,   
	BFA_STATUS_ENOFSAVE	= 78,	
	BFA_STATUS_IOC_DISABLED = 82,   
	BFA_STATUS_NO_SFP_DEV = 89,	
	BFA_STATUS_MEMTEST_FAILED = 90, 
	BFA_STATUS_LEDTEST_OP = 109, 
	BFA_STATUS_INVALID_MAC  = 134, 
	BFA_STATUS_PBC		= 154, 
	BFA_STATUS_BAD_FWCFG = 156,	
	BFA_STATUS_INVALID_VENDOR = 158, 
	BFA_STATUS_SFP_NOT_READY = 159,	
	BFA_STATUS_TRUNK_ENABLED = 164, 
	BFA_STATUS_TRUNK_DISABLED  = 165, 
	BFA_STATUS_IOPROFILE_OFF = 175, 
	BFA_STATUS_PHY_NOT_PRESENT = 183, 
	BFA_STATUS_FEATURE_NOT_SUPPORTED = 192,	
	BFA_STATUS_ENTRY_EXISTS = 193,	
	BFA_STATUS_ENTRY_NOT_EXISTS = 194, 
	BFA_STATUS_NO_CHANGE = 195,	
	BFA_STATUS_FAA_ENABLED = 197,	
	BFA_STATUS_FAA_DISABLED = 198,	
	BFA_STATUS_FAA_ACQUIRED = 199,	
	BFA_STATUS_FAA_ACQ_ADDR = 200,	
	BFA_STATUS_ERROR_TRUNK_ENABLED = 203,	
	BFA_STATUS_MAX_ENTRY_REACHED = 212,	
	BFA_STATUS_MAX_VAL		
};
#define bfa_status_t enum bfa_status

enum bfa_eproto_status {
	BFA_EPROTO_BAD_ACCEPT = 0,
	BFA_EPROTO_UNKNOWN_RSP = 1
};
#define bfa_eproto_status_t enum bfa_eproto_status

enum bfa_boolean {
	BFA_FALSE = 0,
	BFA_TRUE  = 1
};
#define bfa_boolean_t enum bfa_boolean

#define BFA_STRING_32	32
#define BFA_VERSION_LEN 64


enum {
	BFA_ADAPTER_SERIAL_NUM_LEN = STRSZ(BFA_MFG_SERIALNUM_SIZE),
	BFA_ADAPTER_MODEL_NAME_LEN  = 16,  
	BFA_ADAPTER_MODEL_DESCR_LEN = 128, 
	BFA_ADAPTER_MFG_NAME_LEN    = 8,   
	BFA_ADAPTER_SYM_NAME_LEN    = 64,  
	BFA_ADAPTER_OS_TYPE_LEN	    = 64,  
};

struct bfa_adapter_attr_s {
	char		manufacturer[BFA_ADAPTER_MFG_NAME_LEN];
	char		serial_num[BFA_ADAPTER_SERIAL_NUM_LEN];
	u32	card_type;
	char		model[BFA_ADAPTER_MODEL_NAME_LEN];
	char		model_descr[BFA_ADAPTER_MODEL_DESCR_LEN];
	wwn_t		pwwn;
	char		node_symname[FC_SYMNAME_MAX];
	char		hw_ver[BFA_VERSION_LEN];
	char		fw_ver[BFA_VERSION_LEN];
	char		optrom_ver[BFA_VERSION_LEN];
	char		os_type[BFA_ADAPTER_OS_TYPE_LEN];
	struct bfa_mfg_vpd_s	vpd;
	struct mac_s	mac;

	u8		nports;
	u8		max_speed;
	u8		prototype;
	char	        asic_rev;

	u8		pcie_gen;
	u8		pcie_lanes_orig;
	u8		pcie_lanes;
	u8	        cna_capable;

	u8		is_mezz;
	u8		trunk_capable;
};


enum {
	BFA_IOC_DRIVER_LEN	= 16,
	BFA_IOC_CHIP_REV_LEN	= 8,
};

struct bfa_ioc_driver_attr_s {
	char		driver[BFA_IOC_DRIVER_LEN];	
	char		driver_ver[BFA_VERSION_LEN];	
	char		fw_ver[BFA_VERSION_LEN];	
	char		bios_ver[BFA_VERSION_LEN];	
	char		efi_ver[BFA_VERSION_LEN];	
	char		ob_ver[BFA_VERSION_LEN];	
};

struct bfa_ioc_pci_attr_s {
	u16	vendor_id;	
	u16	device_id;	
	u16	ssid;		
	u16	ssvid;		
	u32	pcifn;		
	u32	rsvd;		
	char		chip_rev[BFA_IOC_CHIP_REV_LEN];	 
};

enum bfa_ioc_state {
	BFA_IOC_UNINIT		= 1,	
	BFA_IOC_RESET		= 2,	
	BFA_IOC_SEMWAIT		= 3,	
	BFA_IOC_HWINIT		= 4,	
	BFA_IOC_GETATTR		= 5,	
	BFA_IOC_OPERATIONAL	= 6,	
	BFA_IOC_INITFAIL	= 7,	
	BFA_IOC_FAIL		= 8,	
	BFA_IOC_DISABLING	= 9,	
	BFA_IOC_DISABLED	= 10,	
	BFA_IOC_FWMISMATCH	= 11,	
	BFA_IOC_ENABLING	= 12,	
	BFA_IOC_HWFAIL		= 13,	
	BFA_IOC_ACQ_ADDR	= 14,	
};

struct bfa_fw_ioc_stats_s {
	u32	enable_reqs;
	u32	disable_reqs;
	u32	get_attr_reqs;
	u32	dbg_sync;
	u32	dbg_dump;
	u32	unknown_reqs;
};

struct bfa_ioc_drv_stats_s {
	u32	ioc_isrs;
	u32	ioc_enables;
	u32	ioc_disables;
	u32	ioc_hbfails;
	u32	ioc_boots;
	u32	stats_tmos;
	u32	hb_count;
	u32	disable_reqs;
	u32	enable_reqs;
	u32	disable_replies;
	u32	enable_replies;
	u32	rsvd;
};

struct bfa_ioc_stats_s {
	struct bfa_ioc_drv_stats_s	drv_stats; 
	struct bfa_fw_ioc_stats_s	fw_stats;  
};

enum bfa_ioc_type_e {
	BFA_IOC_TYPE_FC		= 1,
	BFA_IOC_TYPE_FCoE	= 2,
	BFA_IOC_TYPE_LL		= 3,
};

struct bfa_ioc_attr_s {
	enum bfa_ioc_type_e		ioc_type;
	enum bfa_ioc_state		state;		
	struct bfa_adapter_attr_s	adapter_attr;	
	struct bfa_ioc_driver_attr_s	driver_attr;	
	struct bfa_ioc_pci_attr_s	pci_attr;
	u8				port_id;	
	u8				port_mode;	
	u8				cap_bm;		
	u8				port_mode_cfg;	
	u8				rsvd[4];	
};

enum bfa_aen_category {
	BFA_AEN_CAT_ADAPTER	= 1,
	BFA_AEN_CAT_PORT	= 2,
	BFA_AEN_CAT_LPORT	= 3,
	BFA_AEN_CAT_RPORT	= 4,
	BFA_AEN_CAT_ITNIM	= 5,
	BFA_AEN_CAT_AUDIT	= 8,
	BFA_AEN_CAT_IOC		= 9,
};

enum bfa_adapter_aen_event {
	BFA_ADAPTER_AEN_ADD	= 1,	
	BFA_ADAPTER_AEN_REMOVE	= 2,	
};

struct bfa_adapter_aen_data_s {
	char	serial_num[BFA_ADAPTER_SERIAL_NUM_LEN];
	u32	nports; 
	wwn_t	pwwn;   
};

enum bfa_port_aen_event {
	BFA_PORT_AEN_ONLINE	= 1,    
	BFA_PORT_AEN_OFFLINE	= 2,    
	BFA_PORT_AEN_RLIR	= 3,    
	BFA_PORT_AEN_SFP_INSERT	= 4,    
	BFA_PORT_AEN_SFP_REMOVE	= 5,    
	BFA_PORT_AEN_SFP_POM	= 6,    
	BFA_PORT_AEN_ENABLE	= 7,    
	BFA_PORT_AEN_DISABLE	= 8,    
	BFA_PORT_AEN_AUTH_ON	= 9,    
	BFA_PORT_AEN_AUTH_OFF	= 10,   
	BFA_PORT_AEN_DISCONNECT	= 11,   
	BFA_PORT_AEN_QOS_NEG	= 12,   
	BFA_PORT_AEN_FABRIC_NAME_CHANGE	= 13, 
	BFA_PORT_AEN_SFP_ACCESS_ERROR	= 14, 
	BFA_PORT_AEN_SFP_UNSUPPORT	= 15, 
};

enum bfa_port_aen_sfp_pom {
	BFA_PORT_AEN_SFP_POM_GREEN = 1, 
	BFA_PORT_AEN_SFP_POM_AMBER = 2, 
	BFA_PORT_AEN_SFP_POM_RED   = 3, 
	BFA_PORT_AEN_SFP_POM_MAX   = BFA_PORT_AEN_SFP_POM_RED
};

struct bfa_port_aen_data_s {
	wwn_t		pwwn;		
	wwn_t		fwwn;		
	u32		phy_port_num;	
	u16		ioc_type;
	u16		level;		
	mac_t		mac;		
	u16		rsvd;
};

enum bfa_lport_aen_event {
	BFA_LPORT_AEN_NEW	= 1,		
	BFA_LPORT_AEN_DELETE	= 2,		
	BFA_LPORT_AEN_ONLINE	= 3,		
	BFA_LPORT_AEN_OFFLINE	= 4,		
	BFA_LPORT_AEN_DISCONNECT = 5,		
	BFA_LPORT_AEN_NEW_PROP	= 6,		
	BFA_LPORT_AEN_DELETE_PROP = 7,		
	BFA_LPORT_AEN_NEW_STANDARD = 8,		
	BFA_LPORT_AEN_DELETE_STANDARD = 9,	
	BFA_LPORT_AEN_NPIV_DUP_WWN = 10,	
	BFA_LPORT_AEN_NPIV_FABRIC_MAX = 11,	
	BFA_LPORT_AEN_NPIV_UNKNOWN = 12,	
};

struct bfa_lport_aen_data_s {
	u16	vf_id;	
	u16	roles;	
	u32	rsvd;
	wwn_t	ppwwn;	
	wwn_t	lpwwn;	
};

enum bfa_itnim_aen_event {
	BFA_ITNIM_AEN_ONLINE	 = 1,	
	BFA_ITNIM_AEN_OFFLINE	 = 2,	
	BFA_ITNIM_AEN_DISCONNECT = 3,	
};

struct bfa_itnim_aen_data_s {
	u16		vf_id;		
	u16		rsvd[3];
	wwn_t		ppwwn;		
	wwn_t		lpwwn;		
	wwn_t		rpwwn;		
};

enum bfa_audit_aen_event {
	BFA_AUDIT_AEN_AUTH_ENABLE	= 1,
	BFA_AUDIT_AEN_AUTH_DISABLE	= 2,
	BFA_AUDIT_AEN_FLASH_ERASE	= 3,
	BFA_AUDIT_AEN_FLASH_UPDATE	= 4,
};

struct bfa_audit_aen_data_s {
	wwn_t	pwwn;
	int	partition_inst;
	int	partition_type;
};

enum bfa_ioc_aen_event {
	BFA_IOC_AEN_HBGOOD  = 1,	
	BFA_IOC_AEN_HBFAIL  = 2,	
	BFA_IOC_AEN_ENABLE  = 3,	
	BFA_IOC_AEN_DISABLE = 4,	
	BFA_IOC_AEN_FWMISMATCH  = 5,	
	BFA_IOC_AEN_FWCFG_ERROR = 6,	
	BFA_IOC_AEN_INVALID_VENDOR = 7,
	BFA_IOC_AEN_INVALID_NWWN = 8,	
	BFA_IOC_AEN_INVALID_PWWN = 9	
};

struct bfa_ioc_aen_data_s {
	wwn_t	pwwn;
	u16	ioc_type;
	mac_t	mac;
};


#define BFA_MFG_CHKSUM_SIZE			16

#define BFA_MFG_PARTNUM_SIZE			14
#define BFA_MFG_SUPPLIER_ID_SIZE		10
#define BFA_MFG_SUPPLIER_PARTNUM_SIZE		20
#define BFA_MFG_SUPPLIER_SERIALNUM_SIZE		20
#define BFA_MFG_SUPPLIER_REVISION_SIZE		4
#define BFA_MFG_IC_FC	0x01
#define BFA_MFG_IC_ETH	0x02

#define BFA_CM_HBA	0x01
#define BFA_CM_CNA	0x02
#define BFA_CM_NIC	0x04
#define BFA_CM_FC16G	0x08
#define BFA_CM_SRIOV	0x10
#define BFA_CM_MEZZ	0x20

#pragma pack(1)

struct bfa_mfg_block_s {
	u8	version;    
	u8     mfg_sig[3]; 
	u16    mfgsize;    
	u16    u16_chksum; 
	char        brcd_serialnum[STRSZ(BFA_MFG_SERIALNUM_SIZE)];
	char        brcd_partnum[STRSZ(BFA_MFG_PARTNUM_SIZE)];
	u8     mfg_day;    
	u8     mfg_month;  
	u16    mfg_year;   
	wwn_t       mfg_wwn;    
	u8     num_wwn;    
	u8     mfg_speeds; 
	u8     rsv[2];
	char    supplier_id[STRSZ(BFA_MFG_SUPPLIER_ID_SIZE)];
	char    supplier_partnum[STRSZ(BFA_MFG_SUPPLIER_PARTNUM_SIZE)];
	char    supplier_serialnum[STRSZ(BFA_MFG_SUPPLIER_SERIALNUM_SIZE)];
	char    supplier_revision[STRSZ(BFA_MFG_SUPPLIER_REVISION_SIZE)];
	mac_t       mfg_mac;    
	u8     num_mac;    
	u8     rsv2;
	u32    card_type;  
	char        cap_nic;    
	char        cap_cna;    
	char        cap_hba;    
	char        cap_fc16g;  
	char        cap_sriov;  
	char        cap_mezz;   
	u8     rsv3;
	u8     mfg_nports; 
	char        media[8];   
	char        initial_mode[8]; 
	u8     rsv4[84];
	u8     md5_chksum[BFA_MFG_CHKSUM_SIZE]; 
};

#pragma pack()


enum {
	BFA_PCI_VENDOR_ID_BROCADE	= 0x1657,
	BFA_PCI_DEVICE_ID_FC_8G2P	= 0x13,
	BFA_PCI_DEVICE_ID_FC_8G1P	= 0x17,
	BFA_PCI_DEVICE_ID_CT		= 0x14,
	BFA_PCI_DEVICE_ID_CT_FC		= 0x21,
	BFA_PCI_DEVICE_ID_CT2		= 0x22,
};

#define bfa_asic_id_cb(__d)			\
	((__d) == BFA_PCI_DEVICE_ID_FC_8G2P ||	\
	 (__d) == BFA_PCI_DEVICE_ID_FC_8G1P)
#define bfa_asic_id_ct(__d)			\
	((__d) == BFA_PCI_DEVICE_ID_CT ||	\
	 (__d) == BFA_PCI_DEVICE_ID_CT_FC)
#define bfa_asic_id_ct2(__d)	((__d) == BFA_PCI_DEVICE_ID_CT2)
#define bfa_asic_id_ctc(__d)	\
	(bfa_asic_id_ct(__d) || bfa_asic_id_ct2(__d))

enum {
	BFA_PCI_FCOE_SSDEVICE_ID	= 0x14,
	BFA_PCI_CT2_SSID_FCoE		= 0x22,
	BFA_PCI_CT2_SSID_ETH		= 0x23,
	BFA_PCI_CT2_SSID_FC		= 0x24,
};

#define BFA_PCI_ACCESS_RANGES 1

enum bfa_port_speed {
	BFA_PORT_SPEED_UNKNOWN = 0,
	BFA_PORT_SPEED_1GBPS	= 1,
	BFA_PORT_SPEED_2GBPS	= 2,
	BFA_PORT_SPEED_4GBPS	= 4,
	BFA_PORT_SPEED_8GBPS	= 8,
	BFA_PORT_SPEED_10GBPS	= 10,
	BFA_PORT_SPEED_16GBPS	= 16,
	BFA_PORT_SPEED_AUTO	= 0xf,
};
#define bfa_port_speed_t enum bfa_port_speed

enum {
	BFA_BOOT_BOOTLUN_MAX = 4,       
	BFA_PREBOOT_BOOTLUN_MAX = 8,    
};

#define BOOT_CFG_REV1   1
#define BOOT_CFG_VLAN   1

enum bfa_boot_bootopt {
	BFA_BOOT_AUTO_DISCOVER  = 0, 
	BFA_BOOT_STORED_BLUN = 1, 
	BFA_BOOT_FIRST_LUN      = 2, 
	BFA_BOOT_PBC    = 3, 
};

#pragma pack(1)
struct bfa_boot_bootlun_s {
	wwn_t   pwwn;		
	struct scsi_lun   lun;  
};
#pragma pack()

struct bfa_boot_cfg_s {
	u8		version;
	u8		rsvd1;
	u16		chksum;
	u8		enable;		
	u8		speed;          
	u8		topology;       
	u8		bootopt;        
	u32		nbluns;         
	u32		rsvd2;
	struct bfa_boot_bootlun_s blun[BFA_BOOT_BOOTLUN_MAX];
	struct bfa_boot_bootlun_s blun_disc[BFA_BOOT_BOOTLUN_MAX];
};

struct bfa_boot_pbc_s {
	u8              enable;         
	u8              speed;          
	u8              topology;       
	u8              rsvd1;
	u32     nbluns;         
	struct bfa_boot_bootlun_s pblun[BFA_PREBOOT_BOOTLUN_MAX];
};

struct bfa_ethboot_cfg_s {
	u8		version;
	u8		rsvd1;
	u16		chksum;
	u8		enable;	
	u8		rsvd2;
	u16		vlan;
};

#define BFA_ABLK_MAX_PORTS	2
#define BFA_ABLK_MAX_PFS	16
#define BFA_ABLK_MAX		2

#pragma pack(1)
enum bfa_mode_s {
	BFA_MODE_HBA	= 1,
	BFA_MODE_CNA	= 2,
	BFA_MODE_NIC	= 3
};

struct bfa_adapter_cfg_mode_s {
	u16	max_pf;
	u16	max_vf;
	enum bfa_mode_s	mode;
};

struct bfa_ablk_cfg_pf_s {
	u16	pers;
	u8	port_id;
	u8	optrom;
	u8	valid;
	u8	sriov;
	u8	max_vfs;
	u8	rsvd[1];
	u16	num_qpairs;
	u16	num_vectors;
	u32	bw;
};

struct bfa_ablk_cfg_port_s {
	u8	mode;
	u8	type;
	u8	max_pfs;
	u8	rsvd[5];
};

struct bfa_ablk_cfg_inst_s {
	u8	nports;
	u8	max_pfs;
	u8	rsvd[6];
	struct bfa_ablk_cfg_pf_s	pf_cfg[BFA_ABLK_MAX_PFS];
	struct bfa_ablk_cfg_port_s	port_cfg[BFA_ABLK_MAX_PORTS];
};

struct bfa_ablk_cfg_s {
	struct bfa_ablk_cfg_inst_s	inst[BFA_ABLK_MAX];
};


#define SFP_DIAGMON_SIZE	10 

#define BFA_SFP_SCN_REMOVED	0
#define BFA_SFP_SCN_INSERTED	1
#define BFA_SFP_SCN_POM		2
#define BFA_SFP_SCN_FAILED	3
#define BFA_SFP_SCN_UNSUPPORT	4
#define BFA_SFP_SCN_VALID	5

enum bfa_defs_sfp_media_e {
	BFA_SFP_MEDIA_UNKNOWN	= 0x00,
	BFA_SFP_MEDIA_CU	= 0x01,
	BFA_SFP_MEDIA_LW	= 0x02,
	BFA_SFP_MEDIA_SW	= 0x03,
	BFA_SFP_MEDIA_EL	= 0x04,
	BFA_SFP_MEDIA_UNSUPPORT	= 0x05,
};

enum {
	SFP_XMTR_TECH_CU = (1 << 0),	
	SFP_XMTR_TECH_CP = (1 << 1),	
	SFP_XMTR_TECH_CA = (1 << 2),	
	SFP_XMTR_TECH_LL = (1 << 3),	
	SFP_XMTR_TECH_SL = (1 << 4),	
	SFP_XMTR_TECH_SN = (1 << 5),	
	SFP_XMTR_TECH_EL_INTRA = (1 << 6), 
	SFP_XMTR_TECH_EL_INTER = (1 << 7), 
	SFP_XMTR_TECH_LC = (1 << 8),	
	SFP_XMTR_TECH_SA = (1 << 9)
};

struct sfp_srlid_base_s {
	u8	id;		
	u8	extid;		
	u8	connector;	
	u8	xcvr[8];	
	u8	encoding;	
	u8	br_norm;	
	u8	rate_id;	
	u8	len_km;		
	u8	len_100m;	
	u8	len_om2;	
	u8	len_om1;	
	u8	len_cu;		
	u8	len_om3;	
	u8	vendor_name[16];
	u8	unalloc1;
	u8	vendor_oui[3];	
	u8	vendor_pn[16];	
	u8	vendor_rev[4];	
	u8	wavelen[2];	
	u8	unalloc2;
	u8	cc_base;	
};

struct sfp_srlid_ext_s {
	u8	options[2];
	u8	br_max;
	u8	br_min;
	u8	vendor_sn[16];
	u8	date_code[8];
	u8	diag_mon_type;  
	u8	en_options;
	u8	sff_8472;
	u8	cc_ext;
};

struct sfp_diag_base_s {
	u8	temp_high_alarm[2]; 
	u8	temp_low_alarm[2];  
	u8	temp_high_warning[2];   
	u8	temp_low_warning[2];    

	u8	volt_high_alarm[2]; 
	u8	volt_low_alarm[2];  
	u8	volt_high_warning[2];   
	u8	volt_low_warning[2];    

	u8	bias_high_alarm[2]; 
	u8	bias_low_alarm[2];  
	u8	bias_high_warning[2];   
	u8	bias_low_warning[2];    

	u8	tx_pwr_high_alarm[2];   
	u8	tx_pwr_low_alarm[2];    
	u8	tx_pwr_high_warning[2]; 
	u8	tx_pwr_low_warning[2];  

	u8	rx_pwr_high_alarm[2];   
	u8	rx_pwr_low_alarm[2];    
	u8	rx_pwr_high_warning[2]; 
	u8	rx_pwr_low_warning[2];  

	u8	unallocate_1[16];

	u8	rx_pwr[20];
	u8	tx_i[4];
	u8	tx_pwr[4];
	u8	temp[4];
	u8	volt[4];
	u8	unallocate_2[3];
	u8	cc_dmi;
};

struct sfp_diag_ext_s {
	u8	diag[SFP_DIAGMON_SIZE];
	u8	unalloc1[4];
	u8	status_ctl;
	u8	rsvd;
	u8	alarm_flags[2];
	u8	unalloc2[2];
	u8	warning_flags[2];
	u8	ext_status_ctl[2];
};

struct sfp_mem_s {
	struct sfp_srlid_base_s	srlid_base;
	struct sfp_srlid_ext_s	srlid_ext;
	struct sfp_diag_base_s	diag_base;
	struct sfp_diag_ext_s	diag_ext;
};

union sfp_xcvr_e10g_code_u {
	u8		b;
	struct {
#ifdef __BIG_ENDIAN
		u8	e10g_unall:1;   
		u8	e10g_lrm:1;
		u8	e10g_lr:1;
		u8	e10g_sr:1;
		u8	ib_sx:1;    
		u8	ib_lx:1;
		u8	ib_cu_a:1;
		u8	ib_cu_p:1;
#else
		u8	ib_cu_p:1;
		u8	ib_cu_a:1;
		u8	ib_lx:1;
		u8	ib_sx:1;    
		u8	e10g_sr:1;
		u8	e10g_lr:1;
		u8	e10g_lrm:1;
		u8	e10g_unall:1;   
#endif
	} r;
};

union sfp_xcvr_so1_code_u {
	u8		b;
	struct {
		u8	escon:2;    
		u8	oc192_reach:1;  
		u8	so_reach:2;
		u8	oc48_reach:3;
	} r;
};

union sfp_xcvr_so2_code_u {
	u8		b;
	struct {
		u8	reserved:1;
		u8	oc12_reach:3;   
		u8	reserved1:1;
		u8	oc3_reach:3;    
	} r;
};

union sfp_xcvr_eth_code_u {
	u8		b;
	struct {
		u8	base_px:1;
		u8	base_bx10:1;
		u8	e100base_fx:1;
		u8	e100base_lx:1;
		u8	e1000base_t:1;
		u8	e1000base_cx:1;
		u8	e1000base_lx:1;
		u8	e1000base_sx:1;
	} r;
};

struct sfp_xcvr_fc1_code_s {
	u8	link_len:5; 
	u8	xmtr_tech2:3;
	u8	xmtr_tech1:7;   
	u8	reserved1:1;
};

union sfp_xcvr_fc2_code_u {
	u8		b;
	struct {
		u8	tw_media:1; 
		u8	tp_media:1; 
		u8	mi_media:1; 
		u8	tv_media:1; 
		u8	m6_media:1; 
		u8	m5_media:1; 
		u8	reserved:1;
		u8	sm_media:1; 
	} r;
};

union sfp_xcvr_fc3_code_u {
	u8		b;
	struct {
#ifdef __BIG_ENDIAN
		u8	rsv4:1;
		u8	mb800:1;    
		u8	mb1600:1;   
		u8	mb400:1;    
		u8	rsv2:1;
		u8	mb200:1;    
		u8	rsv1:1;
		u8	mb100:1;    
#else
		u8	mb100:1;    
		u8	rsv1:1;
		u8	mb200:1;    
		u8	rsv2:1;
		u8	mb400:1;    
		u8	mb1600:1;   
		u8	mb800:1;    
		u8	rsv4:1;
#endif
	} r;
};

struct sfp_xcvr_s {
	union sfp_xcvr_e10g_code_u	e10g;
	union sfp_xcvr_so1_code_u	so1;
	union sfp_xcvr_so2_code_u	so2;
	union sfp_xcvr_eth_code_u	eth;
	struct sfp_xcvr_fc1_code_s	fc1;
	union sfp_xcvr_fc2_code_u	fc2;
	union sfp_xcvr_fc3_code_u	fc3;
};

#define BFA_FLASH_PART_ENTRY_SIZE	32	
#define BFA_FLASH_PART_MAX		32	

enum bfa_flash_part_type {
	BFA_FLASH_PART_OPTROM   = 1,    
	BFA_FLASH_PART_FWIMG    = 2,    
	BFA_FLASH_PART_FWCFG    = 3,    
	BFA_FLASH_PART_DRV      = 4,    
	BFA_FLASH_PART_BOOT     = 5,    
	BFA_FLASH_PART_ASIC     = 6,    
	BFA_FLASH_PART_MFG      = 7,    
	BFA_FLASH_PART_OPTROM2  = 8,    
	BFA_FLASH_PART_VPD      = 9,    
	BFA_FLASH_PART_PBC      = 10,   
	BFA_FLASH_PART_BOOTOVL  = 11,   
	BFA_FLASH_PART_LOG      = 12,   
	BFA_FLASH_PART_PXECFG   = 13,   
	BFA_FLASH_PART_PXEOVL   = 14,   
	BFA_FLASH_PART_PORTCFG  = 15,   
	BFA_FLASH_PART_ASICBK   = 16,   
};

struct bfa_flash_part_attr_s {
	u32	part_type;      
	u32	part_instance;  
	u32	part_off;       
	u32	part_size;      
	u32	part_len;       
	u32	part_status;    
	char	rsv[BFA_FLASH_PART_ENTRY_SIZE - 24];
};

struct bfa_flash_attr_s {
	u32	status; 
	u32	npart;  
	struct bfa_flash_part_attr_s part[BFA_FLASH_PART_MAX];
};

#define LB_PATTERN_DEFAULT	0xB5B5B5B5
#define QTEST_CNT_DEFAULT	10
#define QTEST_PAT_DEFAULT	LB_PATTERN_DEFAULT

struct bfa_diag_memtest_s {
	u8	algo;
	u8	rsvd[7];
};

struct bfa_diag_memtest_result {
	u32	status;
	u32	addr;
	u32	exp; 
	u32	act; 
	u32	err_status;             
	u32	err_status1;    
	u32	err_addr; 
	u8	algo;
	u8	rsv[3];
};

struct bfa_diag_loopback_result_s {
	u32	numtxmfrm;      
	u32	numosffrm;      
	u32	numrcvfrm;      
	u32	badfrminf;      
	u32	badfrmnum;      
	u8	status;         
	u8	rsvd[3];
};

struct bfa_diag_ledtest_s {
	u32	cmd;    
	u32	color;  
	u16	freq;   
	u8	led;    
	u8	rsvd[5];
};

struct bfa_diag_loopback_s {
	u32	loopcnt;
	u32	pattern;
	u8	lb_mode;    
	u8	speed;      
	u8	rsvd[2];
};

enum bfa_phy_status_e {
	BFA_PHY_STATUS_GOOD	= 0, 
	BFA_PHY_STATUS_NOT_PRESENT	= 1, 
	BFA_PHY_STATUS_BAD	= 2, 
};

struct bfa_phy_attr_s {
	u32	status;         
	u32	length;         
	u32	fw_ver;         
	u32	an_status;      
	u32	pma_pmd_status; 
	u32	pma_pmd_signal; 
	u32	pcs_status;     
};

struct bfa_phy_stats_s {
	u32	status;         
	u32	link_breaks;    
	u32	pma_pmd_fault;  
	u32	pcs_fault;      
	u32	speed_neg;      
	u32	tx_eq_training; 
	u32	tx_eq_timeout;  
	u32	crc_error;      
};

#pragma pack()

#endif 
