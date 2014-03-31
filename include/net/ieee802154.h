/*
 * IEEE802.15.4-2003 specification
 *
 * Copyright (C) 2007, 2008 Siemens AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by:
 * Pavel Smolenskiy <pavel.smolenskiy@gmail.com>
 * Maxim Gorbachyov <maxim.gorbachev@siemens.com>
 * Maxim Osipov <maxim.osipov@siemens.com>
 * Dmitry Eremin-Solenikov <dbaryshkov@gmail.com>
 * Alexander Smirnov <alex.bluesman.smirnov@gmail.com>
 */

#ifndef NET_IEEE802154_H
#define NET_IEEE802154_H

#define IEEE802154_MTU			127

#define IEEE802154_FC_TYPE_BEACON	0x0	
#define	IEEE802154_FC_TYPE_DATA		0x1	
#define IEEE802154_FC_TYPE_ACK		0x2	
#define IEEE802154_FC_TYPE_MAC_CMD	0x3	

#define IEEE802154_FC_TYPE_SHIFT		0
#define IEEE802154_FC_TYPE_MASK		((1 << 3) - 1)
#define IEEE802154_FC_TYPE(x)		((x & IEEE802154_FC_TYPE_MASK) >> IEEE802154_FC_TYPE_SHIFT)
#define IEEE802154_FC_SET_TYPE(v, x)	do {	\
	v = (((v) & ~IEEE802154_FC_TYPE_MASK) | \
	    (((x) << IEEE802154_FC_TYPE_SHIFT) & IEEE802154_FC_TYPE_MASK)); \
	} while (0)

#define IEEE802154_FC_SECEN		(1 << 3)
#define IEEE802154_FC_FRPEND		(1 << 4)
#define IEEE802154_FC_ACK_REQ		(1 << 5)
#define IEEE802154_FC_INTRA_PAN		(1 << 6)

#define IEEE802154_FC_SAMODE_SHIFT	14
#define IEEE802154_FC_SAMODE_MASK	(3 << IEEE802154_FC_SAMODE_SHIFT)
#define IEEE802154_FC_DAMODE_SHIFT	10
#define IEEE802154_FC_DAMODE_MASK	(3 << IEEE802154_FC_DAMODE_SHIFT)

#define IEEE802154_FC_SAMODE(x)		\
	(((x) & IEEE802154_FC_SAMODE_MASK) >> IEEE802154_FC_SAMODE_SHIFT)

#define IEEE802154_FC_DAMODE(x)		\
	(((x) & IEEE802154_FC_DAMODE_MASK) >> IEEE802154_FC_DAMODE_SHIFT)


#define IEEE802154_MFR_SIZE	2 

#define IEEE802154_CMD_ASSOCIATION_REQ		0x01
#define IEEE802154_CMD_ASSOCIATION_RESP		0x02
#define IEEE802154_CMD_DISASSOCIATION_NOTIFY	0x03
#define IEEE802154_CMD_DATA_REQ			0x04
#define IEEE802154_CMD_PANID_CONFLICT_NOTIFY	0x05
#define IEEE802154_CMD_ORPHAN_NOTIFY		0x06
#define IEEE802154_CMD_BEACON_REQ		0x07
#define IEEE802154_CMD_COORD_REALIGN_NOTIFY	0x08
#define IEEE802154_CMD_GTS_REQ			0x09

enum {
	IEEE802154_SUCCESS = 0x0,

	
	IEEE802154_BEACON_LOSS = 0xe0,
	IEEE802154_CHNL_ACCESS_FAIL = 0xe1,
	
	IEEE802154_DENINED = 0xe2,
	
	IEEE802154_DISABLE_TRX_FAIL = 0xe3,
	IEEE802154_FAILED_SECURITY_CHECK = 0xe4,
	IEEE802154_FRAME_TOO_LONG = 0xe5,
	IEEE802154_INVALID_GTS = 0xe6,
	IEEE802154_INVALID_HANDLE = 0xe7,
	
	IEEE802154_INVALID_PARAMETER = 0xe8,
	
	IEEE802154_NO_ACK = 0xe9,
	
	IEEE802154_NO_BEACON = 0xea,
	
	IEEE802154_NO_DATA = 0xeb,
	
	IEEE802154_NO_SHORT_ADDRESS = 0xec,
	IEEE802154_OUT_OF_CAP = 0xed,
	IEEE802154_PANID_CONFLICT = 0xee,
	
	IEEE802154_REALIGMENT = 0xef,
	
	IEEE802154_TRANSACTION_EXPIRED = 0xf0,
	
	IEEE802154_TRANSACTION_OVERFLOW = 0xf1,
	IEEE802154_TX_ACTIVE = 0xf2,
	
	IEEE802154_UNAVAILABLE_KEY = 0xf3,
	IEEE802154_UNSUPPORTED_ATTR = 0xf4,
	IEEE802154_SCAN_IN_PROGRESS = 0xfc,
};


#endif


