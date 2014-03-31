/*
 *  linux/fs/hfs/hfs.h
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 * This file may be distributed under the terms of the GNU General Public License.
 */

#ifndef _HFS_H
#define _HFS_H

#define HFS_DD_BLK		0 
#define HFS_PMAP_BLK		1 
#define HFS_MDB_BLK		2 

#define HFS_DRVR_DESC_MAGIC	0x4552 
#define HFS_OLD_PMAP_MAGIC	0x5453 
#define HFS_NEW_PMAP_MAGIC	0x504D 
#define HFS_SUPER_MAGIC		0x4244 
#define HFS_MFS_SUPER_MAGIC	0xD2D7 

#define HFS_SECTOR_SIZE		512    
#define HFS_SECTOR_SIZE_BITS	9      
#define HFS_NAMELEN		31     
#define HFS_MAX_NAMELEN		128
#define HFS_MAX_VALENCE		32767U

#define HFS_SB_ATTRIB_HLOCK	(1 << 7)
#define HFS_SB_ATTRIB_UNMNT	(1 << 8)
#define HFS_SB_ATTRIB_SPARED	(1 << 9)
#define HFS_SB_ATTRIB_INCNSTNT	(1 << 11)
#define HFS_SB_ATTRIB_SLOCK	(1 << 15)

#define HFS_POR_CNID		1	
#define HFS_ROOT_CNID		2	
#define HFS_EXT_CNID		3	
#define HFS_CAT_CNID		4	
#define HFS_BAD_CNID		5	
#define HFS_ALLOC_CNID		6	
#define HFS_START_CNID		7	
#define HFS_ATTR_CNID		8	
#define HFS_EXCH_CNID		15	
#define HFS_FIRSTUSER_CNID	16

#define HFS_CDR_DIR    0x01    
#define HFS_CDR_FIL    0x02    
#define HFS_CDR_THD    0x03    
#define HFS_CDR_FTH    0x04    

#define HFS_FK_DATA	0x00
#define HFS_FK_RSRC	0xFF

#define HFS_FIL_LOCK	0x01  
#define HFS_FIL_THD	0x02  
#define HFS_FIL_DOPEN   0x04  
#define HFS_FIL_ROPEN   0x08  
#define HFS_FIL_DIR     0x10  
#define HFS_FIL_NOCOPY  0x40  
#define HFS_FIL_USED	0x80  

#define HFS_DIR_LOCK        0x01  
#define HFS_DIR_THD         0x02  
#define HFS_DIR_INEXPFOLDER 0x04  
#define HFS_DIR_MOUNTED     0x08  
#define HFS_DIR_DIR         0x10  
#define HFS_DIR_EXPFOLDER   0x20  

#define HFS_FLG_INITED		0x0100
#define HFS_FLG_LOCKED		0x1000
#define HFS_FLG_INVISIBLE	0x4000


struct hfs_name {
	u8 len;
	u8 name[HFS_NAMELEN];
} __packed;

struct hfs_point {
	__be16 v;
	__be16 h;
} __packed;

struct hfs_rect {
	__be16 top;
	__be16 left;
	__be16 bottom;
	__be16 right;
} __packed;

struct hfs_finfo {
	__be32 fdType;
	__be32 fdCreator;
	__be16 fdFlags;
	struct hfs_point fdLocation;
	__be16 fdFldr;
} __packed;

struct hfs_fxinfo {
	__be16 fdIconID;
	u8 fdUnused[8];
	__be16 fdComment;
	__be32 fdPutAway;
} __packed;

struct hfs_dinfo {
	struct hfs_rect frRect;
	__be16 frFlags;
	struct hfs_point frLocation;
	__be16 frView;
} __packed;

struct hfs_dxinfo {
	struct hfs_point frScroll;
	__be32 frOpenChain;
	__be16 frUnused;
	__be16 frComment;
	__be32 frPutAway;
} __packed;

union hfs_finder_info {
	struct {
		struct hfs_finfo finfo;
		struct hfs_fxinfo fxinfo;
	} file;
	struct {
		struct hfs_dinfo dinfo;
		struct hfs_dxinfo dxinfo;
	} dir;
} __packed;

#define	HFS_BKEY(X)	(((void)((X)->KeyLen)), ((struct hfs_bkey *)(X)))

struct hfs_cat_key {
	u8 key_len;		
	u8 reserved;		
	__be32 ParID;		
	struct hfs_name	CName;	
} __packed;

struct hfs_ext_key {
	u8 key_len;		
	u8 FkType;		
	__be32 FNum;		
	__be16 FABN;		
} __packed;

typedef union hfs_btree_key {
	u8 key_len;			
	struct hfs_cat_key cat;
	struct hfs_ext_key ext;
} hfs_btree_key;

#define HFS_MAX_CAT_KEYLEN	(sizeof(struct hfs_cat_key) - sizeof(u8))
#define HFS_MAX_EXT_KEYLEN	(sizeof(struct hfs_ext_key) - sizeof(u8))

typedef union hfs_btree_key btree_key;

struct hfs_extent {
	__be16 block;
	__be16 count;
};
typedef struct hfs_extent hfs_extent_rec[3];

struct hfs_cat_file {
	s8 type;			
	u8 reserved;
	u8 Flags;			
	s8 Typ;				
	struct hfs_finfo UsrWds;	
	__be32 FlNum;			
	__be16 StBlk;			
	__be32 LgLen;			
	__be32 PyLen;			
	__be16 RStBlk;			
	__be32 RLgLen;			
	__be32 RPyLen;			
	__be32 CrDat;			
	__be32 MdDat;			
	__be32 BkDat;			
	struct hfs_fxinfo FndrInfo;	
	__be16 ClpSize;			
	hfs_extent_rec ExtRec;		
	hfs_extent_rec RExtRec;		
	u32 Resrv;			
} __packed;

struct hfs_cat_dir {
	s8 type;			
	u8 reserved;
	__be16 Flags;			
	__be16 Val;			
	__be32 DirID;			
	__be32 CrDat;			
	__be32 MdDat;			
	__be32 BkDat;			
	struct hfs_dinfo UsrInfo;	
	struct hfs_dxinfo FndrInfo;	
	u8 Resrv[16];			
} __packed;

struct hfs_cat_thread {
	s8 type;			
	u8 reserved[9];			
	__be32 ParID;			
	struct hfs_name CName;		
}  __packed;

typedef union hfs_cat_rec {
	s8 type;			
	struct hfs_cat_file file;
	struct hfs_cat_dir dir;
	struct hfs_cat_thread thread;
} hfs_cat_rec;

struct hfs_mdb {
	__be16 drSigWord;		
	__be32 drCrDate;		
	__be32 drLsMod;			
	__be16 drAtrb;			
	__be16 drNmFls;			
	__be16 drVBMSt;			
	__be16 drAllocPtr;		
	__be16 drNmAlBlks;		
	__be32 drAlBlkSiz;		
	__be32 drClpSiz;		
	__be16 drAlBlSt;		
	__be32 drNxtCNID;		
	__be16 drFreeBks;		
	u8 drVN[28];			
	__be32 drVolBkUp;		
	__be16 drVSeqNum;		
	__be32 drWrCnt;			
	__be32 drXTClpSiz;		
	__be32 drCTClpSiz;		
	__be16 drNmRtDirs;		
	__be32 drFilCnt;		
	__be32 drDirCnt;		
	u8 drFndrInfo[32];		
	__be16 drEmbedSigWord;		
	__be32 drEmbedExtent;		
	__be32 drXTFlSize;		
	hfs_extent_rec drXTExtRec;	
	__be32 drCTFlSize;		
	hfs_extent_rec drCTExtRec;	
} __packed;


struct hfs_readdir_data {
	struct list_head list;
	struct file *file;
	struct hfs_cat_key key;
};

#endif
