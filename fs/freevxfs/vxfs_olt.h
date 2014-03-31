/*
 * Copyright (c) 2000-2001 Christoph Hellwig.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _VXFS_OLT_H_
#define _VXFS_OLT_H_



#define VXFS_OLT_MAGIC		0xa504FCF5

enum {
	VXFS_OLT_FREE	= 1,
	VXFS_OLT_FSHEAD	= 2,
	VXFS_OLT_CUT	= 3,
	VXFS_OLT_ILIST	= 4,
	VXFS_OLT_DEV	= 5,
	VXFS_OLT_SB	= 6
};

struct vxfs_olt {
	u_int32_t	olt_magic;	
	u_int32_t	olt_size;	
	u_int32_t	olt_checksum;	
	u_int32_t	__unused1;	
	u_int32_t	olt_mtime;	
	u_int32_t	olt_mutime;	
	u_int32_t	olt_totfree;	
	vx_daddr_t	olt_extents[2];	
	u_int32_t	olt_esize;	
	vx_daddr_t	olt_next[2];    
	u_int32_t	olt_nsize;	
	u_int32_t	__unused2;	
};

struct vxfs_oltcommon {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
};

struct vxfs_oltfree {
	u_int32_t	olt_type;	
	u_int32_t	olt_fsize;	
};

struct vxfs_oltilist {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
	vx_ino_t	olt_iext[2];	
};

struct vxfs_oltcut {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
	vx_ino_t	olt_cutino;	
	u_int32_t	__pad;		
};

struct vxfs_oltsb {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
	vx_ino_t	olt_sbino;	
	u_int32_t	__unused1;	
	vx_ino_t	olt_logino[2];	
	vx_ino_t	olt_oltino[2];	
};

struct vxfs_oltdev {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
	vx_ino_t	olt_devino[2];	
};

struct vxfs_oltfshead {
	u_int32_t	olt_type;	
	u_int32_t	olt_size;	
	vx_ino_t	olt_fsino[2];   
};

#endif 
