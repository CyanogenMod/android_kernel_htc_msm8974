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
#ifndef _VXFS_SUPER_H_
#define _VXFS_SUPER_H_

#include <linux/types.h>


typedef	int32_t		vx_daddr_t;
typedef int32_t		vx_ino_t;

#define VXFS_SUPER_MAGIC	0xa501FCF5

#define VXFS_ROOT_INO		2

#define VXFS_NEFREE		32


struct vxfs_sb {
	u_int32_t	vs_magic;		
	int32_t		vs_version;		
	u_int32_t	vs_ctime;		
	u_int32_t	vs_cutime;		
	int32_t		__unused1;		
	int32_t		__unused2;		
	vx_daddr_t	vs_old_logstart;	
	vx_daddr_t	vs_old_logend;		
	int32_t		vs_bsize;		
	int32_t		vs_size;		
	int32_t		vs_dsize;		
	u_int32_t	vs_old_ninode;		
	int32_t		vs_old_nau;		
	int32_t		__unused3;		
	int32_t		vs_old_defiextsize;	
	int32_t		vs_old_ilbsize;		
	int32_t		vs_immedlen;		
	int32_t		vs_ndaddr;		
	vx_daddr_t	vs_firstau;		
	vx_daddr_t	vs_emap;		
	vx_daddr_t	vs_imap;		
	vx_daddr_t	vs_iextop;		
	vx_daddr_t	vs_istart;		
	vx_daddr_t	vs_bstart;		
	vx_daddr_t	vs_femap;		
	vx_daddr_t	vs_fimap;		
	vx_daddr_t	vs_fiextop;		
	vx_daddr_t	vs_fistart;		
	vx_daddr_t	vs_fbstart;		
	int32_t		vs_nindir;		
	int32_t		vs_aulen;		
	int32_t		vs_auimlen;		
	int32_t		vs_auemlen;		
	int32_t		vs_auilen;		
	int32_t		vs_aupad;		
	int32_t		vs_aublocks;		
	int32_t		vs_maxtier;		
	int32_t		vs_inopb;		
	int32_t		vs_old_inopau;		
	int32_t		vs_old_inopilb;		
	int32_t		vs_old_ndiripau;	
	int32_t		vs_iaddrlen;		
	int32_t		vs_bshift;		
	int32_t		vs_inoshift;		
	int32_t		vs_bmask;		
	int32_t		vs_boffmask;		
	int32_t		vs_old_inomask;		
	int32_t		vs_checksum;		
	
	int32_t		vs_free;		
	int32_t		vs_ifree;		
	int32_t		vs_efree[VXFS_NEFREE];	
	int32_t		vs_flags;		
	u_int8_t	vs_mod;			
	u_int8_t	vs_clean;		
	u_int16_t	__unused4;		
	u_int32_t	vs_firstlogid;		
	u_int32_t	vs_wtime;		/* last time written - sec */
	u_int32_t	vs_wutime;		/* last time written - usec */
	u_int8_t	vs_fname[6];		
	u_int8_t	vs_fpack[6];		
	int32_t		vs_logversion;		
	int32_t		__unused5;		
	
	vx_daddr_t	vs_oltext[2];		
	int32_t		vs_oltsize;		
	int32_t		vs_iauimlen;		
	int32_t		vs_iausize;		
	int32_t		vs_dinosize;		
	int32_t		vs_old_dniaddr;		
	int32_t		vs_checksum2;		

};


struct vxfs_sb_info {
	struct vxfs_sb		*vsi_raw;	
	struct buffer_head	*vsi_bp;	
	struct inode		*vsi_fship;	
	struct inode		*vsi_ilist;	
	struct inode		*vsi_stilist;	
	u_long			vsi_iext;	
	ino_t			vsi_fshino;	
	daddr_t			vsi_oltext;	
	daddr_t			vsi_oltsize;	
};


enum vxfs_mode {
	VXFS_ISUID = 0x00000800,	
	VXFS_ISGID = 0x00000400,	
	VXFS_ISVTX = 0x00000200,	
	VXFS_IREAD = 0x00000100,	
	VXFS_IWRITE = 0x00000080,	
	VXFS_IEXEC = 0x00000040,	

	VXFS_IFIFO = 0x00001000,	
	VXFS_IFCHR = 0x00002000,	
	VXFS_IFDIR = 0x00004000,	
	VXFS_IFNAM = 0x00005000,	
	VXFS_IFBLK = 0x00006000,	
	VXFS_IFREG = 0x00008000,	
	VXFS_IFCMP = 0x00009000,	
	VXFS_IFLNK = 0x0000a000,	
	VXFS_IFSOC = 0x0000c000,	

	
	VXFS_IFFSH = 0x10000000,	
	VXFS_IFILT = 0x20000000,	
	VXFS_IFIAU = 0x30000000,	
	VXFS_IFCUT = 0x40000000,	
	VXFS_IFATT = 0x50000000,	
	VXFS_IFLCT = 0x60000000,	
	VXFS_IFIAT = 0x70000000,	
	VXFS_IFEMR = 0x80000000,	
	VXFS_IFQUO = 0x90000000,	
	VXFS_IFPTI = 0xa0000000,	
	VXFS_IFLAB = 0x11000000,	
	VXFS_IFOLT = 0x12000000,	
	VXFS_IFLOG = 0x13000000,	
	VXFS_IFEMP = 0x14000000,	
	VXFS_IFEAU = 0x15000000,	
	VXFS_IFAUS = 0x16000000,	
	VXFS_IFDEV = 0x17000000,	

};

#define	VXFS_TYPE_MASK		0xfffff000

#define VXFS_IS_TYPE(ip,type)	(((ip)->vii_mode & VXFS_TYPE_MASK) == (type))
#define VXFS_ISFIFO(x)		VXFS_IS_TYPE((x),VXFS_IFIFO)
#define VXFS_ISCHR(x)		VXFS_IS_TYPE((x),VXFS_IFCHR)
#define VXFS_ISDIR(x)		VXFS_IS_TYPE((x),VXFS_IFDIR)
#define VXFS_ISNAM(x)		VXFS_IS_TYPE((x),VXFS_IFNAM)
#define VXFS_ISBLK(x)		VXFS_IS_TYPE((x),VXFS_IFBLK)
#define VXFS_ISLNK(x)		VXFS_IS_TYPE((x),VXFS_IFLNK)
#define VXFS_ISREG(x)		VXFS_IS_TYPE((x),VXFS_IFREG)
#define VXFS_ISCMP(x)		VXFS_IS_TYPE((x),VXFS_IFCMP)
#define VXFS_ISSOC(x)		VXFS_IS_TYPE((x),VXFS_IFSOC)

#define VXFS_ISFSH(x)		VXFS_IS_TYPE((x),VXFS_IFFSH)
#define VXFS_ISILT(x)		VXFS_IS_TYPE((x),VXFS_IFILT)

enum {
	VXFS_ORG_NONE	= 0,	
	VXFS_ORG_EXT4	= 1,	
	VXFS_ORG_IMMED	= 2,	
	VXFS_ORG_TYPED	= 3,	
};

#define VXFS_IS_ORG(ip,org)	((ip)->vii_orgtype == (org))
#define VXFS_ISNONE(ip)		VXFS_IS_ORG((ip), VXFS_ORG_NONE)
#define VXFS_ISEXT4(ip)		VXFS_IS_ORG((ip), VXFS_ORG_EXT4)
#define VXFS_ISIMMED(ip)	VXFS_IS_ORG((ip), VXFS_ORG_IMMED)
#define VXFS_ISTYPED(ip)	VXFS_IS_ORG((ip), VXFS_ORG_TYPED)


#define VXFS_INO(ip) \
	((struct vxfs_inode_info *)(ip)->i_private)

#define VXFS_SBI(sbp) \
	((struct vxfs_sb_info *)(sbp)->s_fs_info)

#endif 
