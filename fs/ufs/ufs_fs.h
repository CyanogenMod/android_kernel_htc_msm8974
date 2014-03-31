/*
 *  linux/include/linux/ufs_fs.h
 *
 * Copyright (C) 1996
 * Adrian Rodriguez (adrian@franklins-tower.rutgers.edu)
 * Laboratory for Computer Science Research Computing Facility
 * Rutgers, The State University of New Jersey
 *
 * Clean swab support by Fare <fare@tunes.org>
 * just hope no one is using NNUUXXI on __?64 structure elements
 * 64-bit clean thanks to Maciej W. Rozycki <macro@ds2.pg.gda.pl>
 *
 * 4.4BSD (FreeBSD) support added on February 1st 1998 by
 * Niels Kristian Bech Jensen <nkbj@image.dk> partially based
 * on code by Martin von Loewis <martin@mira.isdn.cs.tu-berlin.de>.
 *
 * NeXTstep support added on February 5th 1998 by
 * Niels Kristian Bech Jensen <nkbj@image.dk>.
 *
 * Write support by Daniel Pirkl <daniel.pirkl@email.cz>
 *
 * HP/UX hfs filesystem support added by
 * Martin K. Petersen <mkp@mkp.net>, August 1999
 *
 * UFS2 (of FreeBSD 5.x) support added by
 * Niraj Kumar <niraj17@iitbombay.org>  , Jan 2004
 *
 */

#ifndef __LINUX_UFS_FS_H
#define __LINUX_UFS_FS_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>

#include <asm/div64.h>
typedef __u64 __bitwise __fs64;
typedef __u32 __bitwise __fs32;
typedef __u16 __bitwise __fs16;

#define UFS_BBLOCK 0
#define UFS_BBSIZE 8192
#define UFS_SBLOCK 8192
#define UFS_SBSIZE 8192

#define UFS_SECTOR_SIZE 512
#define UFS_SECTOR_BITS 9
#define UFS_MAGIC  0x00011954
#define UFS_MAGIC_BW 0x0f242697
#define UFS2_MAGIC 0x19540119
#define UFS_CIGAM  0x54190100 

#define SBLOCK_FLOPPY        0
#define SBLOCK_UFS1       8192
#define SBLOCK_UFS2      65536
#define SBLOCK_PIGGY    262144
#define SBLOCKSIZE        8192
#define SBLOCKSEARCH \
        { SBLOCK_UFS2, SBLOCK_UFS1, SBLOCK_FLOPPY, SBLOCK_PIGGY, -1 }



#define UFS_MAGIC_LFN   0x00095014 
#define UFS_CIGAM_LFN   0x14500900 

#define UFS_MAGIC_SEC   0x00612195 
#define UFS_CIGAM_SEC   0x95216100

#define UFS_MAGIC_FEA   0x00195612 
#define UFS_CIGAM_FEA   0x12561900

#define UFS_MAGIC_4GB   0x05231994 
#define UFS_CIGAM_4GB   0x94192305

#define UFS_FSF_LFN     0x00000001 
#define UFS_FSF_B1      0x00000002 
#define UFS_FSF_LFS     0x00000002 
#define UFS_FSF_LUID    0x00000004 



#define UFS_BSIZE	8192
#define UFS_MINBSIZE	4096
#define UFS_FSIZE	1024
#define UFS_MAXFRAG	(UFS_BSIZE / UFS_FSIZE)

#define UFS_NDADDR 12
#define UFS_NINDIR 3

#define UFS_IND_BLOCK	(UFS_NDADDR + 0)
#define UFS_DIND_BLOCK	(UFS_NDADDR + 1)
#define UFS_TIND_BLOCK	(UFS_NDADDR + 2)

#define UFS_NDIR_FRAGMENT (UFS_NDADDR << uspi->s_fpbshift)
#define UFS_IND_FRAGMENT (UFS_IND_BLOCK << uspi->s_fpbshift)
#define UFS_DIND_FRAGMENT (UFS_DIND_BLOCK << uspi->s_fpbshift)
#define UFS_TIND_FRAGMENT (UFS_TIND_BLOCK << uspi->s_fpbshift)

#define UFS_ROOTINO 2
#define UFS_FIRST_INO (UFS_ROOTINO + 1)

#define UFS_USEEFT  ((__u16)65535)

#define UFS_FSOK      0x7c269d38
#define UFS_FSACTIVE  ((__s8)0x00)
#define UFS_FSCLEAN   ((__s8)0x01)
#define UFS_FSSTABLE  ((__s8)0x02)
#define UFS_FSOSF1    ((__s8)0x03)	
#define UFS_FSBAD     ((__s8)0xff)

#define UFS_FSSUSPEND ((__s8)0xfe)	
#define UFS_FSLOG     ((__s8)0xfd)	
#define UFS_FSFIX     ((__s8)0xfc)	

#define UFS_DE_MASK		0x00000010	
#define UFS_DE_OLD		0x00000000
#define UFS_DE_44BSD		0x00000010
#define UFS_UID_MASK		0x00000060	
#define UFS_UID_OLD		0x00000000
#define UFS_UID_44BSD		0x00000020
#define UFS_UID_EFT		0x00000040
#define UFS_ST_MASK		0x00000700	
#define UFS_ST_OLD		0x00000000
#define UFS_ST_44BSD		0x00000100
#define UFS_ST_SUN		0x00000200 
#define UFS_ST_SUNOS		0x00000300
#define UFS_ST_SUNx86		0x00000400 
#define UFS_CG_MASK		0x00003000	
#define UFS_CG_OLD		0x00000000
#define UFS_CG_44BSD		0x00002000
#define UFS_CG_SUN		0x00001000
#define UFS_TYPE_MASK		0x00010000	
#define UFS_TYPE_UFS1		0x00000000
#define UFS_TYPE_UFS2		0x00010000


#define UFS_42INODEFMT	-1
#define UFS_44INODEFMT	2

#define UFS_MINFREE         5
#define UFS_DEFAULTOPT      UFS_OPTTIME

#define ufs_fsbtodb(uspi, b)	((b) << (uspi)->s_fsbtodb)
#define	ufs_dbtofsb(uspi, b)	((b) >> (uspi)->s_fsbtodb)

#define	ufs_cgbase(c)	(uspi->s_fpg * (c))
#define ufs_cgstart(c)	((uspi)->fs_magic == UFS2_MAGIC ?  ufs_cgbase(c) : \
	(ufs_cgbase(c)  + uspi->s_cgoffset * ((c) & ~uspi->s_cgmask)))
#define	ufs_cgsblock(c)	(ufs_cgstart(c) + uspi->s_sblkno)	
#define	ufs_cgcmin(c)	(ufs_cgstart(c) + uspi->s_cblkno)	
#define	ufs_cgimin(c)	(ufs_cgstart(c) + uspi->s_iblkno)	
#define	ufs_cgdmin(c)	(ufs_cgstart(c) + uspi->s_dblkno)	

#define	ufs_inotocg(x)		((x) / uspi->s_ipg)
#define	ufs_inotocgoff(x)	((x) % uspi->s_ipg)
#define	ufs_inotofsba(x)	(((u64)ufs_cgimin(ufs_inotocg(x))) + ufs_inotocgoff(x) / uspi->s_inopf)
#define	ufs_inotofsbo(x)	((x) % uspi->s_inopf)

#define ufs_cbtocylno(bno) \
	((bno) * uspi->s_nspf / uspi->s_spc)
#define ufs_cbtorpos(bno)				      \
	((UFS_SB(sb)->s_flags & UFS_CG_SUN) ?		      \
	 (((((bno) * uspi->s_nspf % uspi->s_spc) %	      \
	    uspi->s_nsect) *				      \
	   uspi->s_nrpos) / uspi->s_nsect)		      \
	 :						      \
	((((bno) * uspi->s_nspf % uspi->s_spc / uspi->s_nsect \
	* uspi->s_trackskew + (bno) * uspi->s_nspf % uspi->s_spc \
	% uspi->s_nsect * uspi->s_interleave) % uspi->s_nsect \
	  * uspi->s_nrpos) / uspi->s_npsect))

#define ufs_blkoff(loc)		((loc) & uspi->s_qbmask)
#define ufs_fragoff(loc)	((loc) & uspi->s_qfmask)
#define ufs_lblktosize(blk)	((blk) << uspi->s_bshift)
#define ufs_lblkno(loc)		((loc) >> uspi->s_bshift)
#define ufs_numfrags(loc)	((loc) >> uspi->s_fshift)
#define ufs_blkroundup(size)	(((size) + uspi->s_qbmask) & uspi->s_bmask)
#define ufs_fragroundup(size)	(((size) + uspi->s_qfmask) & uspi->s_fmask)
#define ufs_fragstoblks(frags)	((frags) >> uspi->s_fpbshift)
#define ufs_blkstofrags(blks)	((blks) << uspi->s_fpbshift)
#define ufs_fragnum(fsb)	((fsb) & uspi->s_fpbmask)
#define ufs_blknum(fsb)		((fsb) & ~uspi->s_fpbmask)

#define	UFS_MAXNAMLEN 255
#define UFS_MAXMNTLEN 512
#define UFS2_MAXMNTLEN 468
#define UFS2_MAXVOLLEN 32
#define UFS_MAXCSBUFS 31
#define UFS_LINK_MAX 32000
#define	UFS2_NOCSPTRS	28

#define UFS_DIR_PAD			4
#define UFS_DIR_ROUND			(UFS_DIR_PAD - 1)
#define UFS_DIR_REC_LEN(name_len)	(((name_len) + 1 + 8 + UFS_DIR_ROUND) & ~UFS_DIR_ROUND)

struct ufs_timeval {
	__fs32	tv_sec;
	__fs32	tv_usec;
};

struct ufs_dir_entry {
	__fs32  d_ino;			
	__fs16  d_reclen;		
	union {
		__fs16	d_namlen;		
		struct {
			__u8	d_type;		
			__u8	d_namlen;	
		} d_44;
	} d_u;
	__u8	d_name[UFS_MAXNAMLEN + 1];	
};

struct ufs_csum {
	__fs32	cs_ndir;	
	__fs32	cs_nbfree;	
	__fs32	cs_nifree;	
	__fs32	cs_nffree;	
};
struct ufs2_csum_total {
	__fs64	cs_ndir;	
	__fs64	cs_nbfree;	
	__fs64	cs_nifree;	
	__fs64	cs_nffree;	
	__fs64   cs_numclusters;	
	__fs64   cs_spare[3];	
};

struct ufs_csum_core {
	__u64	cs_ndir;	
	__u64	cs_nbfree;	
	__u64	cs_nifree;	
	__u64	cs_nffree;	
	__u64   cs_numclusters;	
};

#define UFS_UNCLEAN      0x01    
#define UFS_DOSOFTDEP    0x02    
#define UFS_NEEDSFSCK    0x04    
#define UFS_INDEXDIRS    0x08    
#define UFS_ACLS         0x10    
#define UFS_MULTILABEL   0x20    
#define UFS_FLAGS_UPDATED 0x80   

#if 0
struct ufs_super_block {
	union {
		struct {
			__fs32	fs_link;	
		} fs_42;
		struct {
			__fs32	fs_state;	
		} fs_sun;
	} fs_u0;
	__fs32	fs_rlink;	
	__fs32	fs_sblkno;	
	__fs32	fs_cblkno;	
	__fs32	fs_iblkno;	
	__fs32	fs_dblkno;	
	__fs32	fs_cgoffset;	
	__fs32	fs_cgmask;	
	__fs32	fs_time;	/* last time written -- time_t */
	__fs32	fs_size;	
	__fs32	fs_dsize;	
	__fs32	fs_ncg;		
	__fs32	fs_bsize;	
	__fs32	fs_fsize;	
	__fs32	fs_frag;	
	__fs32	fs_minfree;	
	__fs32	fs_rotdelay;	
	__fs32	fs_rps;		
	__fs32	fs_bmask;	
	__fs32	fs_fmask;	
	__fs32	fs_bshift;	
	__fs32	fs_fshift;	
	__fs32	fs_maxcontig;	
	__fs32	fs_maxbpg;	
	__fs32	fs_fragshift;	
	__fs32	fs_fsbtodb;	
	__fs32	fs_sbsize;	
	__fs32	fs_csmask;	
	__fs32	fs_csshift;	
	__fs32	fs_nindir;	
	__fs32	fs_inopb;	
	__fs32	fs_nspf;	
	__fs32	fs_optim;	
	union {
		struct {
			__fs32	fs_npsect;	
		} fs_sun;
		struct {
			__fs32	fs_state;	
		} fs_sunx86;
	} fs_u1;
	__fs32	fs_interleave;	
	__fs32	fs_trackskew;	
	__fs32	fs_id[2];	
	__fs32	fs_csaddr;	
	__fs32	fs_cssize;	
	__fs32	fs_cgsize;	
	__fs32	fs_ntrak;	
	__fs32	fs_nsect;	
	__fs32	fs_spc;		
	__fs32	fs_ncyl;	
	__fs32	fs_cpg;		
	__fs32	fs_ipg;		
	__fs32	fs_fpg;		
	struct ufs_csum fs_cstotal;	
	__s8	fs_fmod;	
	__s8	fs_clean;	
	__s8	fs_ronly;	
	__s8	fs_flags;
	union {
		struct {
			__s8	fs_fsmnt[UFS_MAXMNTLEN];
			__fs32	fs_cgrotor;	
			__fs32	fs_csp[UFS_MAXCSBUFS];
			__fs32	fs_maxcluster;
			__fs32	fs_cpc;		
			__fs16	fs_opostbl[16][8]; 
		} fs_u1;
		struct {
			__s8  fs_fsmnt[UFS2_MAXMNTLEN];	
			__u8   fs_volname[UFS2_MAXVOLLEN]; 
			__fs64  fs_swuid;		
			__fs32  fs_pad;	
			__fs32   fs_cgrotor;     
			__fs32   fs_ocsp[UFS2_NOCSPTRS]; 
			__fs32   fs_contigdirs;
			__fs32   fs_csp;	
			__fs32   fs_maxcluster;
			__fs32   fs_active;
			__fs32   fs_old_cpc;	
			__fs32   fs_maxbsize;
			__fs64   fs_sparecon64[17];
			__fs64   fs_sblockloc; 
			struct  ufs2_csum_total fs_cstotal;
			struct  ufs_timeval    fs_time;		/* last time written */
			__fs64    fs_size;		
			__fs64    fs_dsize;	
			__fs64   fs_csaddr;	
			__fs64    fs_pendingblocks;
			__fs32    fs_pendinginodes;
		} fs_u2;
	}  fs_u11;
	union {
		struct {
			__fs32	fs_sparecon[53];
			__fs32	fs_reclaim;
			__fs32	fs_sparecon2[1];
			__fs32	fs_state;	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
		} fs_sun;
		struct {
			__fs32	fs_sparecon[53];
			__fs32	fs_reclaim;
			__fs32	fs_sparecon2[1];
			__fs32	fs_npsect;	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
		} fs_sunx86;
		struct {
			__fs32	fs_sparecon[50];
			__fs32	fs_contigsumsize;
			__fs32	fs_maxsymlinklen;
			__fs32	fs_inodefmt;	
			__fs32	fs_maxfilesize[2];	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
			__fs32	fs_state;	
		} fs_44;
	} fs_u2;
	__fs32	fs_postblformat;	
	__fs32	fs_nrpos;		
	__fs32	fs_postbloff;		
	__fs32	fs_rotbloff;		
	__fs32	fs_magic;		
	__u8	fs_space[1];		
};
#endif

#define UFS_OPTTIME	0	
#define UFS_OPTSPACE	1	

#define UFS_42POSTBLFMT		-1	
#define UFS_DYNAMICPOSTBLFMT	1	

#define fs_cs(indx) s_csp[(indx)]

#define	CG_MAGIC	0x090255
#define ufs_cg_chkmagic(sb, ucg) \
	(fs32_to_cpu((sb), (ucg)->cg_magic) == CG_MAGIC)
#define ufs_ocg_blktot(sb, ucg)      fs32_to_cpu((sb), ((struct ufs_old_cylinder_group *)(ucg))->cg_btot)
#define ufs_ocg_blks(sb, ucg, cylno) fs32_to_cpu((sb), ((struct ufs_old_cylinder_group *)(ucg))->cg_b[cylno])
#define ufs_ocg_inosused(sb, ucg)    fs32_to_cpu((sb), ((struct ufs_old_cylinder_group *)(ucg))->cg_iused)
#define ufs_ocg_blksfree(sb, ucg)    fs32_to_cpu((sb), ((struct ufs_old_cylinder_group *)(ucg))->cg_free)
#define ufs_ocg_chkmagic(sb, ucg) \
	(fs32_to_cpu((sb), ((struct ufs_old_cylinder_group *)(ucg))->cg_magic) == CG_MAGIC)

struct	ufs_cylinder_group {
	__fs32	cg_link;		
	__fs32	cg_magic;		
	__fs32	cg_time;		/* time last written */
	__fs32	cg_cgx;			
	__fs16	cg_ncyl;		
	__fs16	cg_niblk;		
	__fs32	cg_ndblk;		
	struct	ufs_csum cg_cs;		
	__fs32	cg_rotor;		
	__fs32	cg_frotor;		
	__fs32	cg_irotor;		
	__fs32	cg_frsum[UFS_MAXFRAG];	
	__fs32	cg_btotoff;		
	__fs32	cg_boff;		
	__fs32	cg_iusedoff;		
	__fs32	cg_freeoff;		
	__fs32	cg_nextfreeoff;		
	union {
		struct {
			__fs32	cg_clustersumoff;	
			__fs32	cg_clusteroff;		
			__fs32	cg_nclusterblks;	
			__fs32	cg_sparecon[13];	
		} cg_44;
		struct {
			__fs32	cg_clustersumoff;
			__fs32	cg_clusteroff;	
			__fs32	cg_nclusterblks;
			__fs32   cg_niblk; 
			__fs32   cg_initediblk;	
			__fs32   cg_sparecon32[3];
			__fs64   cg_time;	/* time last written */
			__fs64	cg_sparecon[3];	
		} cg_u2;
		__fs32	cg_sparecon[16];	
	} cg_u;
	__u8	cg_space[1];		
};

struct ufs_old_cylinder_group {
	__fs32	cg_link;		
	__fs32	cg_rlink;		
	__fs32	cg_time;		/* time last written */
	__fs32	cg_cgx;			
	__fs16	cg_ncyl;		
	__fs16	cg_niblk;		
	__fs32	cg_ndblk;		
	struct	ufs_csum cg_cs;		
	__fs32	cg_rotor;		
	__fs32	cg_frotor;		
	__fs32	cg_irotor;		
	__fs32	cg_frsum[8];		
	__fs32	cg_btot[32];		
	__fs16	cg_b[32][8];		
	__u8	cg_iused[256];		
	__fs32	cg_magic;		
	__u8	cg_free[1];		
};

struct ufs_inode {
	__fs16	ui_mode;		
	__fs16	ui_nlink;		
	union {
		struct {
			__fs16	ui_suid;	
			__fs16	ui_sgid;	
		} oldids;
		__fs32	ui_inumber;		
		__fs32	ui_author;		
	} ui_u1;
	__fs64	ui_size;		
	struct ufs_timeval ui_atime;	
	struct ufs_timeval ui_mtime;	
	struct ufs_timeval ui_ctime;	
	union {
		struct {
			__fs32	ui_db[UFS_NDADDR];
			__fs32	ui_ib[UFS_NINDIR];
		} ui_addr;
		__u8	ui_symlink[4*(UFS_NDADDR+UFS_NINDIR)];
	} ui_u2;
	__fs32	ui_flags;		
	__fs32	ui_blocks;		
	__fs32	ui_gen;			
	union {
		struct {
			__fs32	ui_shadow;	
			__fs32	ui_uid;		
			__fs32	ui_gid;		
			__fs32	ui_oeftflag;	
		} ui_sun;
		struct {
			__fs32	ui_uid;		
			__fs32	ui_gid;		
			__fs32	ui_spare[2];	
		} ui_44;
		struct {
			__fs32	ui_uid;		
			__fs32	ui_gid;		
			__fs16	ui_modeh;	
			__fs16	ui_spare;	
			__fs32	ui_trans;	
		} ui_hurd;
	} ui_u3;
};

#define UFS_NXADDR  2            
struct ufs2_inode {
	__fs16     ui_mode;        
	__fs16     ui_nlink;       
	__fs32     ui_uid;         
	__fs32     ui_gid;         
	__fs32     ui_blksize;     
	__fs64     ui_size;        
	__fs64     ui_blocks;      
	__fs64   ui_atime;       
	__fs64   ui_mtime;       
	__fs64   ui_ctime;       
	__fs64   ui_birthtime;   
	__fs32     ui_mtimensec;   
	__fs32     ui_atimensec;   
	__fs32     ui_ctimensec;   
	__fs32     ui_birthnsec;   
	__fs32     ui_gen;         
	__fs32     ui_kernflags;   
	__fs32     ui_flags;       
	__fs32     ui_extsize;     
	__fs64     ui_extb[UFS_NXADDR];
	union {
		struct {
			__fs64     ui_db[UFS_NDADDR]; 
			__fs64     ui_ib[UFS_NINDIR];
		} ui_addr;
	__u8	ui_symlink[2*4*(UFS_NDADDR+UFS_NINDIR)];
	} ui_u2;
	__fs64     ui_spare[3];    
};


#define UFS_UF_SETTABLE   0x0000ffff
#define UFS_UF_NODUMP     0x00000001  
#define UFS_UF_IMMUTABLE  0x00000002  
#define UFS_UF_APPEND     0x00000004  
#define UFS_UF_OPAQUE     0x00000008  
#define UFS_UF_NOUNLINK   0x00000010  
#define UFS_SF_SETTABLE   0xffff0000
#define UFS_SF_ARCHIVED   0x00010000  
#define UFS_SF_IMMUTABLE  0x00020000  
#define UFS_SF_APPEND     0x00040000  
#define UFS_SF_NOUNLINK   0x00100000  

struct ufs_buffer_head {
	__u64 fragment;			
	__u64 count;				
	struct buffer_head * bh[UFS_MAXFRAG];	
};

struct ufs_cg_private_info {
	struct ufs_buffer_head c_ubh;
	__u32	c_cgx;		
	__u16	c_ncyl;		
	__u16	c_niblk;	
	__u32	c_ndblk;	
	__u32	c_rotor;	
	__u32	c_frotor;	
	__u32	c_irotor;	
	__u32	c_btotoff;	
	__u32	c_boff;		
	__u32	c_iusedoff;	
	__u32	c_freeoff;	
	__u32	c_nextfreeoff;	
	__u32	c_clustersumoff;
	__u32	c_clusteroff;	
	__u32	c_nclusterblks;	
};


struct ufs_sb_private_info {
	struct ufs_buffer_head s_ubh; 
	struct ufs_csum_core cs_total;
	__u32	s_sblkno;	
	__u32	s_cblkno;	
	__u32	s_iblkno;	
	__u32	s_dblkno;	
	__u32	s_cgoffset;	
	__u32	s_cgmask;	
	__u32	s_size;		
	__u32	s_dsize;	
	__u64	s_u2_size;	
	__u64	s_u2_dsize;	
	__u32	s_ncg;		
	__u32	s_bsize;	
	__u32	s_fsize;	
	__u32	s_fpb;		
	__u32	s_minfree;	
	__u32	s_bmask;	
	__u32	s_fmask;	
	__u32	s_bshift;	
	__u32   s_fshift;	
	__u32	s_fpbshift;	
	__u32	s_fsbtodb;	
	__u32	s_sbsize;	
	__u32   s_csmask;	
	__u32	s_csshift;	
	__u32	s_nindir;	
	__u32	s_inopb;	
	__u32	s_nspf;		
	__u32	s_npsect;	
	__u32	s_interleave;	
	__u32	s_trackskew;	
	__u64	s_csaddr;	
	__u32	s_cssize;	
	__u32	s_cgsize;	
	__u32	s_ntrak;	
	__u32	s_nsect;	
	__u32	s_spc;		
	__u32	s_ipg;		
	__u32	s_fpg;		
	__u32	s_cpc;		
	__s32	s_contigsumsize;
	__s64	s_qbmask;	
	__s64	s_qfmask;	
	__s32	s_postblformat;	
	__s32	s_nrpos;	
        __s32	s_postbloff;	
	__s32	s_rotbloff;	

	__u32	s_fpbmask;	
	__u32	s_apb;		
	__u32	s_2apb;		
	__u32	s_3apb;		
	__u32	s_apbmask;	
	__u32	s_apbshift;	
	__u32	s_2apbshift;	
	__u32	s_3apbshift;	
	__u32	s_nspfshift;	
	__u32	s_nspb;		
	__u32	s_inopf;	
	__u32	s_sbbase;	
	__u32	s_bpf;		
	__u32	s_bpfshift;	
	__u32	s_bpfmask;	

	__u32	s_maxsymlinklen;
	__s32	fs_magic;       
	unsigned int s_dirblksize;
};

struct ufs_super_block_first {
	union {
		struct {
			__fs32	fs_link;	
		} fs_42;
		struct {
			__fs32	fs_state;	
		} fs_sun;
	} fs_u0;
	__fs32	fs_rlink;
	__fs32	fs_sblkno;
	__fs32	fs_cblkno;
	__fs32	fs_iblkno;
	__fs32	fs_dblkno;
	__fs32	fs_cgoffset;
	__fs32	fs_cgmask;
	__fs32	fs_time;
	__fs32	fs_size;
	__fs32	fs_dsize;
	__fs32	fs_ncg;
	__fs32	fs_bsize;
	__fs32	fs_fsize;
	__fs32	fs_frag;
	__fs32	fs_minfree;
	__fs32	fs_rotdelay;
	__fs32	fs_rps;
	__fs32	fs_bmask;
	__fs32	fs_fmask;
	__fs32	fs_bshift;
	__fs32	fs_fshift;
	__fs32	fs_maxcontig;
	__fs32	fs_maxbpg;
	__fs32	fs_fragshift;
	__fs32	fs_fsbtodb;
	__fs32	fs_sbsize;
	__fs32	fs_csmask;
	__fs32	fs_csshift;
	__fs32	fs_nindir;
	__fs32	fs_inopb;
	__fs32	fs_nspf;
	__fs32	fs_optim;
	union {
		struct {
			__fs32	fs_npsect;
		} fs_sun;
		struct {
			__fs32	fs_state;
		} fs_sunx86;
	} fs_u1;
	__fs32	fs_interleave;
	__fs32	fs_trackskew;
	__fs32	fs_id[2];
	__fs32	fs_csaddr;
	__fs32	fs_cssize;
	__fs32	fs_cgsize;
	__fs32	fs_ntrak;
	__fs32	fs_nsect;
	__fs32	fs_spc;
	__fs32	fs_ncyl;
	__fs32	fs_cpg;
	__fs32	fs_ipg;
	__fs32	fs_fpg;
	struct ufs_csum fs_cstotal;
	__s8	fs_fmod;
	__s8	fs_clean;
	__s8	fs_ronly;
	__s8	fs_flags;
	__s8	fs_fsmnt[UFS_MAXMNTLEN - 212];

};

struct ufs_super_block_second {
	union {
		struct {
			__s8	fs_fsmnt[212];
			__fs32	fs_cgrotor;
			__fs32	fs_csp[UFS_MAXCSBUFS];
			__fs32	fs_maxcluster;
			__fs32	fs_cpc;
			__fs16	fs_opostbl[82];
		} fs_u1;
		struct {
			__s8  fs_fsmnt[UFS2_MAXMNTLEN - UFS_MAXMNTLEN + 212];
			__u8   fs_volname[UFS2_MAXVOLLEN];
			__fs64  fs_swuid;
			__fs32  fs_pad;
			__fs32   fs_cgrotor;
			__fs32   fs_ocsp[UFS2_NOCSPTRS];
			__fs32   fs_contigdirs;
			__fs32   fs_csp;
			__fs32   fs_maxcluster;
			__fs32   fs_active;
			__fs32   fs_old_cpc;
			__fs32   fs_maxbsize;
			__fs64   fs_sparecon64[17];
			__fs64   fs_sblockloc;
			__fs64	cs_ndir;
			__fs64	cs_nbfree;
		} fs_u2;
	} fs_un;
};

struct ufs_super_block_third {
	union {
		struct {
			__fs16	fs_opostbl[46];
		} fs_u1;
		struct {
			__fs64	cs_nifree;	
			__fs64	cs_nffree;	
			__fs64   cs_numclusters;	
			__fs64   cs_spare[3];	
			struct  ufs_timeval    fs_time;		/* last time written */
			__fs64    fs_size;		
			__fs64    fs_dsize;	
			__fs64   fs_csaddr;	
			__fs64    fs_pendingblocks;
			__fs32    fs_pendinginodes;
		} __attribute__ ((packed)) fs_u2;
	} fs_un1;
	union {
		struct {
			__fs32	fs_sparecon[53];
			__fs32	fs_reclaim;
			__fs32	fs_sparecon2[1];
			__fs32	fs_state;	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
		} fs_sun;
		struct {
			__fs32	fs_sparecon[53];
			__fs32	fs_reclaim;
			__fs32	fs_sparecon2[1];
			__fs32	fs_npsect;	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
		} fs_sunx86;
		struct {
			__fs32	fs_sparecon[50];
			__fs32	fs_contigsumsize;
			__fs32	fs_maxsymlinklen;
			__fs32	fs_inodefmt;	
			__fs32	fs_maxfilesize[2];	
			__fs32	fs_qbmask[2];	
			__fs32	fs_qfmask[2];	
			__fs32	fs_state;	
		} fs_44;
	} fs_un2;
	__fs32	fs_postblformat;
	__fs32	fs_nrpos;
	__fs32	fs_postbloff;
	__fs32	fs_rotbloff;
	__fs32	fs_magic;
	__u8	fs_space[1];
};

#endif 
