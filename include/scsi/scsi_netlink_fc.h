/*
 *  FC Transport Netlink Interface
 *
 *  Copyright (C) 2006   James Smart, Emulex Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef SCSI_NETLINK_FC_H
#define SCSI_NETLINK_FC_H

#include <scsi/scsi_netlink.h>


	
#define FC_NL_ASYNC_EVENT			0x0100
	



#define FC_NL_MSGALIGN(len)		(((len) + 7) & ~7)


struct fc_nl_event {
	struct scsi_nl_hdr snlh;		
	uint64_t seconds;
	uint64_t vendor_id;
	uint16_t host_no;
	uint16_t event_datalen;
	uint32_t event_num;
	uint32_t event_code;
	uint32_t event_data;
} __attribute__((aligned(sizeof(uint64_t))));


#endif 

