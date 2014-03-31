/*
 * Upcall description for nfsdcld communication
 *
 * Copyright (c) 2012 Red Hat, Inc.
 * Author(s): Jeff Layton <jlayton@redhat.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _NFSD_CLD_H
#define _NFSD_CLD_H

#define CLD_UPCALL_VERSION 1

#define NFS4_OPAQUE_LIMIT 1024

enum cld_command {
	Cld_Create,		
	Cld_Remove,		
	Cld_Check,		
	Cld_GraceDone,		
};

struct cld_name {
	uint16_t	cn_len;				
	unsigned char	cn_id[NFS4_OPAQUE_LIMIT];	
} __attribute__((packed));

struct cld_msg {
	uint8_t		cm_vers;		
	uint8_t		cm_cmd;			
	int16_t		cm_status;		
	uint32_t	cm_xid;			
	union {
		int64_t		cm_gracetime;	
		struct cld_name	cm_name;
	} __attribute__((packed)) cm_u;
} __attribute__((packed));

#endif 
