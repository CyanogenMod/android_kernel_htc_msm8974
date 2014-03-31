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
#ifndef _VXFS_INODE_H_
#define _VXFS_INODE_H_



#define VXFS_ISIZE		0x100		

#define VXFS_NDADDR		10		
#define VXFS_NIADDR		2		
#define VXFS_NIMMED		96		
#define VXFS_NTYPED		6		

#define VXFS_TYPED_OFFSETMASK	(0x00FFFFFFFFFFFFFFULL)
#define VXFS_TYPED_TYPEMASK	(0xFF00000000000000ULL)
#define VXFS_TYPED_TYPESHIFT	56

#define VXFS_TYPED_PER_BLOCK(sbp) \
	((sbp)->s_blocksize / sizeof(struct vxfs_typed))

enum {
	VXFS_TYPED_INDIRECT		= 1,
	VXFS_TYPED_DATA			= 2,
	VXFS_TYPED_INDIRECT_DEV4	= 3,
	VXFS_TYPED_DATA_DEV4		= 4,
};

struct vxfs_immed {
	u_int8_t	vi_immed[VXFS_NIMMED];
};

struct vxfs_ext4 {
	u_int32_t		ve4_spare;		
	u_int32_t		ve4_indsize;		
	vx_daddr_t		ve4_indir[VXFS_NIADDR];	
	struct direct {					
		vx_daddr_t	extent;			
		int32_t		size;			
	} ve4_direct[VXFS_NDADDR];
};

struct vxfs_typed {
	u_int64_t	vt_hdr;		
	vx_daddr_t	vt_block;	
	int32_t		vt_size;	
};

struct vxfs_typed_dev4 {
	u_int64_t	vd4_hdr;	
	u_int64_t	vd4_block;	
	u_int64_t	vd4_size;	
	int32_t		vd4_dev;	
	u_int32_t	__pad1;
};

struct vxfs_dinode {
	int32_t		vdi_mode;
	u_int32_t	vdi_nlink;	
	u_int32_t	vdi_uid;	
	u_int32_t	vdi_gid;	
	u_int64_t	vdi_size;	
	u_int32_t	vdi_atime;	
	u_int32_t	vdi_autime;	
	u_int32_t	vdi_mtime;	
	u_int32_t	vdi_mutime;	
	u_int32_t	vdi_ctime;	
	u_int32_t	vdi_cutime;	
	u_int8_t	vdi_aflags;	
	u_int8_t	vdi_orgtype;	
	u_int16_t	vdi_eopflags;
	u_int32_t	vdi_eopdata;
	union {
		u_int32_t		rdev;
		u_int32_t		dotdot;
		struct {
			u_int32_t	reserved;
			u_int32_t	fixextsize;
		} i_regular;
		struct {
			u_int32_t	matchino;
			u_int32_t	fsetindex;
		} i_vxspec;
		u_int64_t		align;
	} vdi_ftarea;
	u_int32_t	vdi_blocks;	
	u_int32_t	vdi_gen;	
	u_int64_t	vdi_version;	
	union {
		struct vxfs_immed	immed;
		struct vxfs_ext4	ext4;
		struct vxfs_typed	typed[VXFS_NTYPED];
	} vdi_org;
	u_int32_t	vdi_iattrino;
};

#define vdi_rdev	vdi_ftarea.rdev
#define vdi_dotdot	vdi_ftarea.dotdot
#define vdi_fixextsize	vdi_ftarea.regular.fixextsize
#define vdi_matchino	vdi_ftarea.vxspec.matchino
#define vdi_fsetindex	vdi_ftarea.vxspec.fsetindex

#define vdi_immed	vdi_org.immed
#define vdi_ext4	vdi_org.ext4
#define vdi_typed	vdi_org.typed


#define vxfs_inode_info	vxfs_dinode

#define vii_mode	vdi_mode
#define vii_uid		vdi_uid
#define vii_gid		vdi_gid
#define vii_nlink	vdi_nlink
#define vii_size	vdi_size
#define vii_atime	vdi_atime
#define vii_ctime	vdi_ctime
#define vii_mtime	vdi_mtime
#define vii_blocks	vdi_blocks
#define vii_org		vdi_org
#define vii_orgtype	vdi_orgtype
#define vii_gen		vdi_gen

#define vii_rdev	vdi_ftarea.rdev
#define vii_dotdot	vdi_ftarea.dotdot
#define vii_fixextsize	vdi_ftarea.regular.fixextsize
#define vii_matchino	vdi_ftarea.vxspec.matchino
#define vii_fsetindex	vdi_ftarea.vxspec.fsetindex

#define vii_immed	vdi_org.immed
#define vii_ext4	vdi_org.ext4
#define vii_typed	vdi_org.typed

#endif 
