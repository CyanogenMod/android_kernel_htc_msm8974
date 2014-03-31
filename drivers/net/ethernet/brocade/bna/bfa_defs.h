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

#ifndef __BFA_DEFS_H__
#define __BFA_DEFS_H__

#include "cna.h"
#include "bfa_defs_status.h"
#include "bfa_defs_mfg_comm.h"

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

struct bfa_adapter_attr {
	char		manufacturer[BFA_ADAPTER_MFG_NAME_LEN];
	char		serial_num[BFA_ADAPTER_SERIAL_NUM_LEN];
	u32	card_type;
	char		model[BFA_ADAPTER_MODEL_NAME_LEN];
	char		model_descr[BFA_ADAPTER_MODEL_DESCR_LEN];
	u64		pwwn;
	char		node_symname[FC_SYMNAME_MAX];
	char		hw_ver[BFA_VERSION_LEN];
	char		fw_ver[BFA_VERSION_LEN];
	char		optrom_ver[BFA_VERSION_LEN];
	char		os_type[BFA_ADAPTER_OS_TYPE_LEN];
	struct bfa_mfg_vpd vpd;
	struct mac mac;

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

struct bfa_ioc_driver_attr {
	char		driver[BFA_IOC_DRIVER_LEN];	
	char		driver_ver[BFA_VERSION_LEN];	
	char		fw_ver[BFA_VERSION_LEN];	
	char		bios_ver[BFA_VERSION_LEN];	
	char		efi_ver[BFA_VERSION_LEN];	
	char		ob_ver[BFA_VERSION_LEN];	
};

struct bfa_ioc_pci_attr {
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
};

struct bfa_fw_ioc_stats {
	u32	enable_reqs;
	u32	disable_reqs;
	u32	get_attr_reqs;
	u32	dbg_sync;
	u32	dbg_dump;
	u32	unknown_reqs;
};

struct bfa_ioc_drv_stats {
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

struct bfa_ioc_stats {
	struct bfa_ioc_drv_stats drv_stats; 
	struct bfa_fw_ioc_stats fw_stats;  
};

enum bfa_ioc_type {
	BFA_IOC_TYPE_FC		= 1,
	BFA_IOC_TYPE_FCoE	= 2,
	BFA_IOC_TYPE_LL		= 3,
};

struct bfa_ioc_attr {
	enum bfa_ioc_type ioc_type;
	enum bfa_ioc_state		state;		
	struct bfa_adapter_attr adapter_attr;	
	struct bfa_ioc_driver_attr driver_attr;	
	struct bfa_ioc_pci_attr pci_attr;
	u8				port_id;	
	u8				port_mode;	
	u8				cap_bm;		
	u8				port_mode_cfg;	
	u8				rsvd[4];	
};

enum {
	BFA_CM_HBA	=	0x01,
	BFA_CM_CNA	=	0x02,
	BFA_CM_NIC	=	0x04,
};


#define BFA_MFG_CHKSUM_SIZE			16

#define BFA_MFG_PARTNUM_SIZE			14
#define BFA_MFG_SUPPLIER_ID_SIZE		10
#define BFA_MFG_SUPPLIER_PARTNUM_SIZE		20
#define BFA_MFG_SUPPLIER_SERIALNUM_SIZE		20
#define BFA_MFG_SUPPLIER_REVISION_SIZE		4

#pragma pack(1)

struct bfa_mfg_block {
	u8	version;	
	u8	mfg_sig[3];	
	u16	mfgsize;	
	u16	u16_chksum;	
	char	brcd_serialnum[STRSZ(BFA_MFG_SERIALNUM_SIZE)];
	char	brcd_partnum[STRSZ(BFA_MFG_PARTNUM_SIZE)];
	u8	mfg_day;	
	u8	mfg_month;	
	u16	mfg_year;	
	u64	mfg_wwn;	
	u8	num_wwn;	
	u8	mfg_speeds;	
	u8	rsv[2];
	char	supplier_id[STRSZ(BFA_MFG_SUPPLIER_ID_SIZE)];
	char	supplier_partnum[STRSZ(BFA_MFG_SUPPLIER_PARTNUM_SIZE)];
	char	supplier_serialnum[STRSZ(BFA_MFG_SUPPLIER_SERIALNUM_SIZE)];
	char	supplier_revision[STRSZ(BFA_MFG_SUPPLIER_REVISION_SIZE)];
	mac_t	mfg_mac;	
	u8	num_mac;	
	u8	rsv2;
	u32	card_type;	
	char	cap_nic;	
	char	cap_cna;	
	char	cap_hba;	
	char	cap_fc16g;	
	char	cap_sriov;	
	char	cap_mezz;	
	u8	rsv3;
	u8	mfg_nports;	
	char	media[8];	
	char	initial_mode[8]; 
	u8	rsv4[84];
	u8	md5_chksum[BFA_MFG_CHKSUM_SIZE]; 
};

#pragma pack()


enum {
	BFA_PCI_DEVICE_ID_CT2		= 0x22,
};

#define bfa_asic_id_ct(device)			\
	((device) == PCI_DEVICE_ID_BROCADE_CT ||	\
	 (device) == PCI_DEVICE_ID_BROCADE_CT_FC)
#define bfa_asic_id_ct2(device)			\
	((device) == BFA_PCI_DEVICE_ID_CT2)
#define bfa_asic_id_ctc(device)			\
	(bfa_asic_id_ct(device) || bfa_asic_id_ct2(device))

enum {
	BFA_PCI_FCOE_SSDEVICE_ID	= 0x14,
	BFA_PCI_CT2_SSID_FCoE		= 0x22,
	BFA_PCI_CT2_SSID_ETH		= 0x23,
	BFA_PCI_CT2_SSID_FC		= 0x24,
};

enum bfa_mode {
	BFA_MODE_HBA		= 1,
	BFA_MODE_CNA		= 2,
	BFA_MODE_NIC		= 3
};

#define BFA_FLASH_PART_ENTRY_SIZE	32	
#define BFA_FLASH_PART_MAX		32	
#define BFA_TOTAL_FLASH_SIZE		0x400000
#define BFA_FLASH_PART_FWIMG		2
#define BFA_FLASH_PART_MFG		7

struct bfa_flash_part_attr {
	u32	part_type;	
	u32	part_instance;	
	u32	part_off;	
	u32	part_size;	
	u32	part_len;	
	u32	part_status;	
	char	rsv[BFA_FLASH_PART_ENTRY_SIZE - 24];
};

struct bfa_flash_attr {
	u32	status;	
	u32	npart;  
	struct bfa_flash_part_attr part[BFA_FLASH_PART_MAX];
};

#endif 
