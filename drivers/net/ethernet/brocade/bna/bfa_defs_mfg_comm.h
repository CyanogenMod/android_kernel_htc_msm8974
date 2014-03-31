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
#ifndef __BFA_DEFS_MFG_COMM_H__
#define __BFA_DEFS_MFG_COMM_H__

#include "bfa_defs.h"

#define BFA_MFG_VERSION				3
#define BFA_MFG_VERSION_UNINIT			0xFF

#define BFA_MFG_ENC_VER				2

#define BFA_MFG_VER1_LEN			128

#define BFA_MFG_HDR_LEN				4

#define BFA_MFG_SERIALNUM_SIZE			11
#define STRSZ(_n)				(((_n) + 4) & ~3)

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

enum {
	CB_GPIO_TTV	= (1),		
	CB_GPIO_FC8P2   = (2),		
	CB_GPIO_FC8P1   = (3),		
	CB_GPIO_FC4P2   = (4),		
	CB_GPIO_FC4P1   = (5),		
	CB_GPIO_DFLY    = (6),		
	CB_GPIO_PROTO   = (1 << 7)	
};

#define bfa_mfg_adapter_prop_init_gpio(gpio, card_type, prop)	\
do {								\
	if ((gpio) & CB_GPIO_PROTO) {				\
		(prop) |= BFI_ADAPTER_PROTO;			\
		(gpio) &= ~CB_GPIO_PROTO;			\
	}							\
	switch ((gpio)) {					\
	case CB_GPIO_TTV:					\
		(prop) |= BFI_ADAPTER_TTV;			\
	case CB_GPIO_DFLY:					\
	case CB_GPIO_FC8P2:					\
		(prop) |= BFI_ADAPTER_SETP(NPORTS, 2);		\
		(prop) |= BFI_ADAPTER_SETP(SPEED, 8);		\
		(card_type) = BFA_MFG_TYPE_FC8P2;		\
		break;						\
	case CB_GPIO_FC8P1:					\
		(prop) |= BFI_ADAPTER_SETP(NPORTS, 1);		\
		(prop) |= BFI_ADAPTER_SETP(SPEED, 8);		\
		(card_type) = BFA_MFG_TYPE_FC8P1;		\
		break;						\
	case CB_GPIO_FC4P2:					\
		(prop) |= BFI_ADAPTER_SETP(NPORTS, 2);		\
		(prop) |= BFI_ADAPTER_SETP(SPEED, 4);		\
		(card_type) = BFA_MFG_TYPE_FC4P2;		\
		break;						\
	case CB_GPIO_FC4P1:					\
		(prop) |= BFI_ADAPTER_SETP(NPORTS, 1);		\
		(prop) |= BFI_ADAPTER_SETP(SPEED, 4);		\
		(card_type) = BFA_MFG_TYPE_FC4P1;		\
		break;						\
	default:						\
		(prop) |= BFI_ADAPTER_UNSUPP;			\
		(card_type) = BFA_MFG_TYPE_INVALID;		\
	}							\
} while (0)

#define BFA_MFG_VPD_LEN			512
#define BFA_MFG_VPD_LEN_INVALID		0

#define BFA_MFG_VPD_PCI_HDR_OFF		137
#define BFA_MFG_VPD_PCI_VER_MASK	0x07	
#define BFA_MFG_VPD_PCI_VDR_MASK	0xf8	

enum {
	BFA_MFG_VPD_UNKNOWN	= 0,     
	BFA_MFG_VPD_IBM		= 1,     
	BFA_MFG_VPD_HP		= 2,     
	BFA_MFG_VPD_DELL	= 3,     
	BFA_MFG_VPD_PCI_IBM	= 0x08,  
	BFA_MFG_VPD_PCI_HP	= 0x10,  
	BFA_MFG_VPD_PCI_DELL	= 0x20,  
	BFA_MFG_VPD_PCI_BRCD	= 0xf8,  
};

struct bfa_mfg_vpd {
	u8		version;	
	u8		vpd_sig[3];	
	u8		chksum;		
	u8		vendor;		
	u8	len;		
	u8	rsv;
	u8		data[BFA_MFG_VPD_LEN];	
};

#pragma pack()

#endif 
