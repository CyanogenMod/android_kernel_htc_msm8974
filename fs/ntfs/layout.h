/*
 * layout.h - All NTFS associated on-disk structures. Part of the Linux-NTFS
 *	      project.
 *
 * Copyright (c) 2001-2005 Anton Altaparmakov
 * Copyright (c) 2002 Richard Russon
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_NTFS_LAYOUT_H
#define _LINUX_NTFS_LAYOUT_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <asm/byteorder.h>

#include "types.h"

#define magicNTFS	cpu_to_le64(0x202020205346544eULL)


typedef struct {
	le16 bytes_per_sector;		
	u8  sectors_per_cluster;	
	le16 reserved_sectors;		
	u8  fats;			
	le16 root_entries;		
	le16 sectors;			
	u8  media_type;			
	le16 sectors_per_fat;		
	le16 sectors_per_track;		
	le16 heads;			
	le32 hidden_sectors;		
	le32 large_sectors;		
} __attribute__ ((__packed__)) BIOS_PARAMETER_BLOCK;

typedef struct {
	u8  jump[3];			
	le64 oem_id;			
	BIOS_PARAMETER_BLOCK bpb;	
	u8  unused[4];			
sle64 number_of_sectors;	
	sle64 mft_lcn;			
	sle64 mftmirr_lcn;		
	s8  clusters_per_mft_record;	
	u8  reserved0[3];		
	s8  clusters_per_index_record;	
	u8  reserved1[3];		
	le64 volume_serial_number;	
	le32 checksum;			
u8  bootstrap[426];		
	le16 end_of_sector_marker;	
} __attribute__ ((__packed__)) NTFS_BOOT_SECTOR;

enum {
	
	magic_FILE = cpu_to_le32(0x454c4946), 
	magic_INDX = cpu_to_le32(0x58444e49), 
	magic_HOLE = cpu_to_le32(0x454c4f48), 

	
	magic_RSTR = cpu_to_le32(0x52545352), 
	magic_RCRD = cpu_to_le32(0x44524352), 

	
	magic_CHKD = cpu_to_le32(0x444b4843), 

	
	magic_BAAD = cpu_to_le32(0x44414142), 
	magic_empty = cpu_to_le32(0xffffffff) 
};

typedef le32 NTFS_RECORD_TYPE;


static inline bool __ntfs_is_magic(le32 x, NTFS_RECORD_TYPE r)
{
	return (x == r);
}
#define ntfs_is_magic(x, m)	__ntfs_is_magic(x, magic_##m)

static inline bool __ntfs_is_magicp(le32 *p, NTFS_RECORD_TYPE r)
{
	return (*p == r);
}
#define ntfs_is_magicp(p, m)	__ntfs_is_magicp(p, magic_##m)

#define ntfs_is_file_record(x)		( ntfs_is_magic (x, FILE) )
#define ntfs_is_file_recordp(p)		( ntfs_is_magicp(p, FILE) )
#define ntfs_is_mft_record(x)		( ntfs_is_file_record (x) )
#define ntfs_is_mft_recordp(p)		( ntfs_is_file_recordp(p) )
#define ntfs_is_indx_record(x)		( ntfs_is_magic (x, INDX) )
#define ntfs_is_indx_recordp(p)		( ntfs_is_magicp(p, INDX) )
#define ntfs_is_hole_record(x)		( ntfs_is_magic (x, HOLE) )
#define ntfs_is_hole_recordp(p)		( ntfs_is_magicp(p, HOLE) )

#define ntfs_is_rstr_record(x)		( ntfs_is_magic (x, RSTR) )
#define ntfs_is_rstr_recordp(p)		( ntfs_is_magicp(p, RSTR) )
#define ntfs_is_rcrd_record(x)		( ntfs_is_magic (x, RCRD) )
#define ntfs_is_rcrd_recordp(p)		( ntfs_is_magicp(p, RCRD) )

#define ntfs_is_chkd_record(x)		( ntfs_is_magic (x, CHKD) )
#define ntfs_is_chkd_recordp(p)		( ntfs_is_magicp(p, CHKD) )

#define ntfs_is_baad_record(x)		( ntfs_is_magic (x, BAAD) )
#define ntfs_is_baad_recordp(p)		( ntfs_is_magicp(p, BAAD) )

#define ntfs_is_empty_record(x)		( ntfs_is_magic (x, empty) )
#define ntfs_is_empty_recordp(p)	( ntfs_is_magicp(p, empty) )

/*
 * The Update Sequence Array (usa) is an array of the le16 values which belong
 * to the end of each sector protected by the update sequence record in which
 * this array is contained. Note that the first entry is the Update Sequence
 * Number (usn), a cyclic counter of how many times the protected record has
 * been written to disk. The values 0 and -1 (ie. 0xffff) are not used. All
 * last le16's of each sector have to be equal to the usn (during reading) or
 * are set to it (during writing). If they are not, an incomplete multi sector
 * transfer has occurred when the data was written.
 * The maximum size for the update sequence array is fixed to:
 *	maximum size = usa_ofs + (usa_count * 2) = 510 bytes
 * The 510 bytes comes from the fact that the last le16 in the array has to
 * (obviously) finish before the last le16 of the first 512-byte sector.
 * This formula can be used as a consistency check in that usa_ofs +
 * (usa_count * 2) has to be less than or equal to 510.
 */
typedef struct {
	NTFS_RECORD_TYPE magic;	
	le16 usa_ofs;		
	le16 usa_count;		
} __attribute__ ((__packed__)) NTFS_RECORD;

typedef enum {
	FILE_MFT       = 0,	
	FILE_MFTMirr   = 1,	
	FILE_LogFile   = 2,	
	FILE_Volume    = 3,	
	FILE_AttrDef   = 4,	
	FILE_root      = 5,	
	FILE_Bitmap    = 6,	
	FILE_Boot      = 7,	
	FILE_BadClus   = 8,	
	FILE_Secure    = 9,	
	FILE_UpCase    = 10,	
	FILE_Extend    = 11,	
	FILE_reserved12 = 12,	
	FILE_reserved13 = 13,
	FILE_reserved14 = 14,
	FILE_reserved15 = 15,
	FILE_first_user = 16,	
} NTFS_SYSTEM_FILES;

enum {
	MFT_RECORD_IN_USE	= cpu_to_le16(0x0001),
	MFT_RECORD_IS_DIRECTORY = cpu_to_le16(0x0002),
} __attribute__ ((__packed__));

typedef le16 MFT_RECORD_FLAGS;


#define MFT_REF_MASK_CPU 0x0000ffffffffffffULL
#define MFT_REF_MASK_LE cpu_to_le64(MFT_REF_MASK_CPU)

typedef u64 MFT_REF;
typedef le64 leMFT_REF;

#define MK_MREF(m, s)	((MFT_REF)(((MFT_REF)(s) << 48) |		\
					((MFT_REF)(m) & MFT_REF_MASK_CPU)))
#define MK_LE_MREF(m, s) cpu_to_le64(MK_MREF(m, s))

#define MREF(x)		((unsigned long)((x) & MFT_REF_MASK_CPU))
#define MSEQNO(x)	((u16)(((x) >> 48) & 0xffff))
#define MREF_LE(x)	((unsigned long)(le64_to_cpu(x) & MFT_REF_MASK_CPU))
#define MSEQNO_LE(x)	((u16)((le64_to_cpu(x) >> 48) & 0xffff))

#define IS_ERR_MREF(x)	(((x) & 0x0000800000000000ULL) ? true : false)
#define ERR_MREF(x)	((u64)((s64)(x)))
#define MREF_ERR(x)	((int)((s64)(x)))

typedef struct {
	NTFS_RECORD_TYPE magic;	
	le16 usa_ofs;		
	le16 usa_count;		

	le64 lsn;		
	le16 sequence_number;	
	le16 link_count;	
	le16 attrs_offset;	
	MFT_RECORD_FLAGS flags;	
	le32 bytes_in_use;	
	le32 bytes_allocated;	
	leMFT_REF base_mft_record;
	le16 next_attr_instance;
 le16 reserved;		
 le32 mft_record_number;	
} __attribute__ ((__packed__)) MFT_RECORD;

typedef struct {
	NTFS_RECORD_TYPE magic;	
	le16 usa_ofs;		
	le16 usa_count;		

	le64 lsn;		
	le16 sequence_number;	
	le16 link_count;	
	le16 attrs_offset;	
	MFT_RECORD_FLAGS flags;	
	le32 bytes_in_use;	
	le32 bytes_allocated;	
	leMFT_REF base_mft_record;
	le16 next_attr_instance;
} __attribute__ ((__packed__)) MFT_RECORD_OLD;

enum {
	AT_UNUSED			= cpu_to_le32(         0),
	AT_STANDARD_INFORMATION		= cpu_to_le32(      0x10),
	AT_ATTRIBUTE_LIST		= cpu_to_le32(      0x20),
	AT_FILE_NAME			= cpu_to_le32(      0x30),
	AT_OBJECT_ID			= cpu_to_le32(      0x40),
	AT_SECURITY_DESCRIPTOR		= cpu_to_le32(      0x50),
	AT_VOLUME_NAME			= cpu_to_le32(      0x60),
	AT_VOLUME_INFORMATION		= cpu_to_le32(      0x70),
	AT_DATA				= cpu_to_le32(      0x80),
	AT_INDEX_ROOT			= cpu_to_le32(      0x90),
	AT_INDEX_ALLOCATION		= cpu_to_le32(      0xa0),
	AT_BITMAP			= cpu_to_le32(      0xb0),
	AT_REPARSE_POINT		= cpu_to_le32(      0xc0),
	AT_EA_INFORMATION		= cpu_to_le32(      0xd0),
	AT_EA				= cpu_to_le32(      0xe0),
	AT_PROPERTY_SET			= cpu_to_le32(      0xf0),
	AT_LOGGED_UTILITY_STREAM	= cpu_to_le32(     0x100),
	AT_FIRST_USER_DEFINED_ATTRIBUTE	= cpu_to_le32(    0x1000),
	AT_END				= cpu_to_le32(0xffffffff)
};

typedef le32 ATTR_TYPE;

enum {
	COLLATION_BINARY		= cpu_to_le32(0x00),
	COLLATION_FILE_NAME		= cpu_to_le32(0x01),
	COLLATION_UNICODE_STRING	= cpu_to_le32(0x02),
	COLLATION_NTOFS_ULONG		= cpu_to_le32(0x10),
	COLLATION_NTOFS_SID		= cpu_to_le32(0x11),
	COLLATION_NTOFS_SECURITY_HASH	= cpu_to_le32(0x12),
	COLLATION_NTOFS_ULONGS		= cpu_to_le32(0x13),
};

typedef le32 COLLATION_RULE;

enum {
	ATTR_DEF_INDEXABLE	= cpu_to_le32(0x02), 
	ATTR_DEF_MULTIPLE	= cpu_to_le32(0x04), 
	ATTR_DEF_NOT_ZERO	= cpu_to_le32(0x08), 
	ATTR_DEF_INDEXED_UNIQUE	= cpu_to_le32(0x10), 
	ATTR_DEF_NAMED_UNIQUE	= cpu_to_le32(0x20), 
	ATTR_DEF_RESIDENT	= cpu_to_le32(0x40), 
	ATTR_DEF_ALWAYS_LOG	= cpu_to_le32(0x80), 
};

typedef le32 ATTR_DEF_FLAGS;

typedef struct {
	ntfschar name[0x40];		
	ATTR_TYPE type;			
	le32 display_rule;		
 COLLATION_RULE collation_rule;	
	ATTR_DEF_FLAGS flags;		
	sle64 min_size;			
	sle64 max_size;			
} __attribute__ ((__packed__)) ATTR_DEF;

enum {
	ATTR_IS_COMPRESSED    = cpu_to_le16(0x0001),
	ATTR_COMPRESSION_MASK = cpu_to_le16(0x00ff), 
	ATTR_IS_ENCRYPTED     = cpu_to_le16(0x4000),
	ATTR_IS_SPARSE	      = cpu_to_le16(0x8000),
} __attribute__ ((__packed__));

typedef le16 ATTR_FLAGS;


enum {
	RESIDENT_ATTR_IS_INDEXED = 0x01, 
} __attribute__ ((__packed__));

typedef u8 RESIDENT_ATTR_FLAGS;

typedef struct {
	ATTR_TYPE type;		
	le32 length;		
	u8 non_resident;	
	u8 name_length;		
	le16 name_offset;	
	ATTR_FLAGS flags;	
	le16 instance;		
	union {
		
		struct {
		le32 value_length;
		le16 value_offset;
		RESIDENT_ATTR_FLAGS flags; 
		s8 reserved;	  
		} __attribute__ ((__packed__)) resident;
		
		struct {
			leVCN lowest_vcn;
			leVCN highest_vcn;
			le16 mapping_pairs_offset; 
			u8 compression_unit; 
			u8 reserved[5];		
			sle64 allocated_size;	
			sle64 data_size;	
			sle64 initialized_size;	
			sle64 compressed_size;	
		} __attribute__ ((__packed__)) non_resident;
	} __attribute__ ((__packed__)) data;
} __attribute__ ((__packed__)) ATTR_RECORD;

typedef ATTR_RECORD ATTR_REC;

enum {
	FILE_ATTR_READONLY		= cpu_to_le32(0x00000001),
	FILE_ATTR_HIDDEN		= cpu_to_le32(0x00000002),
	FILE_ATTR_SYSTEM		= cpu_to_le32(0x00000004),
	

	FILE_ATTR_DIRECTORY		= cpu_to_le32(0x00000010),
	FILE_ATTR_ARCHIVE		= cpu_to_le32(0x00000020),
	FILE_ATTR_DEVICE		= cpu_to_le32(0x00000040),
	FILE_ATTR_NORMAL		= cpu_to_le32(0x00000080),

	FILE_ATTR_TEMPORARY		= cpu_to_le32(0x00000100),
	FILE_ATTR_SPARSE_FILE		= cpu_to_le32(0x00000200),
	FILE_ATTR_REPARSE_POINT		= cpu_to_le32(0x00000400),
	FILE_ATTR_COMPRESSED		= cpu_to_le32(0x00000800),

	FILE_ATTR_OFFLINE		= cpu_to_le32(0x00001000),
	FILE_ATTR_NOT_CONTENT_INDEXED	= cpu_to_le32(0x00002000),
	FILE_ATTR_ENCRYPTED		= cpu_to_le32(0x00004000),

	FILE_ATTR_VALID_FLAGS		= cpu_to_le32(0x00007fb7),
	FILE_ATTR_VALID_SET_FLAGS	= cpu_to_le32(0x000031a7),
	FILE_ATTR_DUP_FILE_NAME_INDEX_PRESENT	= cpu_to_le32(0x10000000),
	FILE_ATTR_DUP_VIEW_INDEX_PRESENT	= cpu_to_le32(0x20000000),
};

typedef le32 FILE_ATTR_FLAGS;


typedef struct {
	sle64 creation_time;		
	sle64 last_data_change_time;	
	sle64 last_mft_change_time;	
	sle64 last_access_time;		
	FILE_ATTR_FLAGS file_attributes; 
	union {
	
		struct {
			u8 reserved12[12];	
		} __attribute__ ((__packed__)) v1;
	
	
		struct {
			le32 maximum_versions;	
			le32 version_number;	
			le32 class_id;		
			le32 owner_id;		
			le32 security_id;	
			le64 quota_charged;	
			leUSN usn;		
		} __attribute__ ((__packed__)) v3;
	
	} __attribute__ ((__packed__)) ver;
} __attribute__ ((__packed__)) STANDARD_INFORMATION;

typedef struct {
	ATTR_TYPE type;		
	le16 length;		
	u8 name_length;		
	u8 name_offset;		
	leVCN lowest_vcn;	
	leMFT_REF mft_reference;
	le16 instance;		
	ntfschar name[0];	
} __attribute__ ((__packed__)) ATTR_LIST_ENTRY;

#define MAXIMUM_FILE_NAME_LENGTH	255

enum {
	FILE_NAME_POSIX		= 0x00,
	FILE_NAME_WIN32		= 0x01,
	FILE_NAME_DOS		= 0x02,
	FILE_NAME_WIN32_AND_DOS	= 0x03,
} __attribute__ ((__packed__));

typedef u8 FILE_NAME_TYPE_FLAGS;

typedef struct {
	leMFT_REF parent_directory;	
	sle64 creation_time;		
	sle64 last_data_change_time;	
	sle64 last_mft_change_time;	
	sle64 last_access_time;		
	sle64 allocated_size;		
	sle64 data_size;		
	FILE_ATTR_FLAGS file_attributes;	
	union {
		struct {
			le16 packed_ea_size;	
			le16 reserved;		
		} __attribute__ ((__packed__)) ea;
		struct {
			le32 reparse_point_tag;	
		} __attribute__ ((__packed__)) rp;
	} __attribute__ ((__packed__)) type;
	u8 file_name_length;			
	FILE_NAME_TYPE_FLAGS file_name_type;	
	ntfschar file_name[0];			
} __attribute__ ((__packed__)) FILE_NAME_ATTR;

typedef struct {
	le32 data1;	
	le16 data2;	
	le16 data3;	
	u8 data4[8];	
} __attribute__ ((__packed__)) GUID;

typedef struct {
	leMFT_REF mft_reference;
	union {
		struct {
			GUID birth_volume_id;
			GUID birth_object_id;
			GUID domain_id;
		} __attribute__ ((__packed__)) origin;
		u8 extended_info[48];
	} __attribute__ ((__packed__)) opt;
} __attribute__ ((__packed__)) OBJ_ID_INDEX_DATA;

typedef struct {
	GUID object_id;				
	union {
		struct {
			GUID birth_volume_id;	
			GUID birth_object_id;	
			GUID domain_id;		
		} __attribute__ ((__packed__)) origin;
		u8 extended_info[48];
	} __attribute__ ((__packed__)) opt;
} __attribute__ ((__packed__)) OBJECT_ID_ATTR;


typedef enum {					
	SECURITY_NULL_RID		  = 0,	
	SECURITY_WORLD_RID		  = 0,	
	SECURITY_LOCAL_RID		  = 0,	

	SECURITY_CREATOR_OWNER_RID	  = 0,	
	SECURITY_CREATOR_GROUP_RID	  = 1,	

	SECURITY_CREATOR_OWNER_SERVER_RID = 2,	
	SECURITY_CREATOR_GROUP_SERVER_RID = 3,	

	SECURITY_DIALUP_RID		  = 1,
	SECURITY_NETWORK_RID		  = 2,
	SECURITY_BATCH_RID		  = 3,
	SECURITY_INTERACTIVE_RID	  = 4,
	SECURITY_SERVICE_RID		  = 6,
	SECURITY_ANONYMOUS_LOGON_RID	  = 7,
	SECURITY_PROXY_RID		  = 8,
	SECURITY_ENTERPRISE_CONTROLLERS_RID=9,
	SECURITY_SERVER_LOGON_RID	  = 9,
	SECURITY_PRINCIPAL_SELF_RID	  = 0xa,
	SECURITY_AUTHENTICATED_USER_RID	  = 0xb,
	SECURITY_RESTRICTED_CODE_RID	  = 0xc,
	SECURITY_TERMINAL_SERVER_RID	  = 0xd,

	SECURITY_LOGON_IDS_RID		  = 5,
	SECURITY_LOGON_IDS_RID_COUNT	  = 3,

	SECURITY_LOCAL_SYSTEM_RID	  = 0x12,

	SECURITY_NT_NON_UNIQUE		  = 0x15,

	SECURITY_BUILTIN_DOMAIN_RID	  = 0x20,


	
	DOMAIN_USER_RID_ADMIN		  = 0x1f4,
	DOMAIN_USER_RID_GUEST		  = 0x1f5,
	DOMAIN_USER_RID_KRBTGT		  = 0x1f6,

	
	DOMAIN_GROUP_RID_ADMINS		  = 0x200,
	DOMAIN_GROUP_RID_USERS		  = 0x201,
	DOMAIN_GROUP_RID_GUESTS		  = 0x202,
	DOMAIN_GROUP_RID_COMPUTERS	  = 0x203,
	DOMAIN_GROUP_RID_CONTROLLERS	  = 0x204,
	DOMAIN_GROUP_RID_CERT_ADMINS	  = 0x205,
	DOMAIN_GROUP_RID_SCHEMA_ADMINS	  = 0x206,
	DOMAIN_GROUP_RID_ENTERPRISE_ADMINS= 0x207,
	DOMAIN_GROUP_RID_POLICY_ADMINS	  = 0x208,

	
	DOMAIN_ALIAS_RID_ADMINS		  = 0x220,
	DOMAIN_ALIAS_RID_USERS		  = 0x221,
	DOMAIN_ALIAS_RID_GUESTS		  = 0x222,
	DOMAIN_ALIAS_RID_POWER_USERS	  = 0x223,

	DOMAIN_ALIAS_RID_ACCOUNT_OPS	  = 0x224,
	DOMAIN_ALIAS_RID_SYSTEM_OPS	  = 0x225,
	DOMAIN_ALIAS_RID_PRINT_OPS	  = 0x226,
	DOMAIN_ALIAS_RID_BACKUP_OPS	  = 0x227,

	DOMAIN_ALIAS_RID_REPLICATOR	  = 0x228,
	DOMAIN_ALIAS_RID_RAS_SERVERS	  = 0x229,
	DOMAIN_ALIAS_RID_PREW2KCOMPACCESS = 0x22a,
} RELATIVE_IDENTIFIERS;


typedef union {
	struct {
		u16 high_part;	
		u32 low_part;	
	} __attribute__ ((__packed__)) parts;
	u8 value[6];		
} __attribute__ ((__packed__)) SID_IDENTIFIER_AUTHORITY;

typedef struct {
	u8 revision;
	u8 sub_authority_count;
	SID_IDENTIFIER_AUTHORITY identifier_authority;
	le32 sub_authority[1];		
} __attribute__ ((__packed__)) SID;

typedef enum {
	SID_REVISION			=  1,	
	SID_MAX_SUB_AUTHORITIES		= 15,	
	SID_RECOMMENDED_SUB_AUTHORITIES	=  1,	
} SID_CONSTANTS;

enum {
	ACCESS_MIN_MS_ACE_TYPE		= 0,
	ACCESS_ALLOWED_ACE_TYPE		= 0,
	ACCESS_DENIED_ACE_TYPE		= 1,
	SYSTEM_AUDIT_ACE_TYPE		= 2,
	SYSTEM_ALARM_ACE_TYPE		= 3, 
	ACCESS_MAX_MS_V2_ACE_TYPE	= 3,

	ACCESS_ALLOWED_COMPOUND_ACE_TYPE= 4,
	ACCESS_MAX_MS_V3_ACE_TYPE	= 4,

	
	ACCESS_MIN_MS_OBJECT_ACE_TYPE	= 5,
	ACCESS_ALLOWED_OBJECT_ACE_TYPE	= 5,
	ACCESS_DENIED_OBJECT_ACE_TYPE	= 6,
	SYSTEM_AUDIT_OBJECT_ACE_TYPE	= 7,
	SYSTEM_ALARM_OBJECT_ACE_TYPE	= 8,
	ACCESS_MAX_MS_OBJECT_ACE_TYPE	= 8,

	ACCESS_MAX_MS_V4_ACE_TYPE	= 8,

	
	ACCESS_MAX_MS_ACE_TYPE		= 8,
} __attribute__ ((__packed__));

typedef u8 ACE_TYPES;

enum {
	
	OBJECT_INHERIT_ACE		= 0x01,
	CONTAINER_INHERIT_ACE		= 0x02,
	NO_PROPAGATE_INHERIT_ACE	= 0x04,
	INHERIT_ONLY_ACE		= 0x08,
	INHERITED_ACE			= 0x10,	
	VALID_INHERIT_FLAGS		= 0x1f,

	
	SUCCESSFUL_ACCESS_ACE_FLAG	= 0x40,
	FAILED_ACCESS_ACE_FLAG		= 0x80,
} __attribute__ ((__packed__));

typedef u8 ACE_FLAGS;

typedef struct {
	ACE_TYPES type;		
	ACE_FLAGS flags;	
	le16 size;		
} __attribute__ ((__packed__)) ACE_HEADER;

enum {
	

	
	FILE_READ_DATA			= cpu_to_le32(0x00000001),
	
	FILE_LIST_DIRECTORY		= cpu_to_le32(0x00000001),

	
	FILE_WRITE_DATA			= cpu_to_le32(0x00000002),
	
	FILE_ADD_FILE			= cpu_to_le32(0x00000002),

	
	FILE_APPEND_DATA		= cpu_to_le32(0x00000004),
	
	FILE_ADD_SUBDIRECTORY		= cpu_to_le32(0x00000004),

	
	FILE_READ_EA			= cpu_to_le32(0x00000008),

	
	FILE_WRITE_EA			= cpu_to_le32(0x00000010),

	
	FILE_EXECUTE			= cpu_to_le32(0x00000020),
	
	FILE_TRAVERSE			= cpu_to_le32(0x00000020),

	FILE_DELETE_CHILD		= cpu_to_le32(0x00000040),

	
	FILE_READ_ATTRIBUTES		= cpu_to_le32(0x00000080),

	
	FILE_WRITE_ATTRIBUTES		= cpu_to_le32(0x00000100),


	
	DELETE				= cpu_to_le32(0x00010000),

	READ_CONTROL			= cpu_to_le32(0x00020000),

	
	WRITE_DAC			= cpu_to_le32(0x00040000),

	
	WRITE_OWNER			= cpu_to_le32(0x00080000),

	SYNCHRONIZE			= cpu_to_le32(0x00100000),


	
	STANDARD_RIGHTS_READ		= cpu_to_le32(0x00020000),
	STANDARD_RIGHTS_WRITE		= cpu_to_le32(0x00020000),
	STANDARD_RIGHTS_EXECUTE		= cpu_to_le32(0x00020000),

	
	STANDARD_RIGHTS_REQUIRED	= cpu_to_le32(0x000f0000),

	STANDARD_RIGHTS_ALL		= cpu_to_le32(0x001f0000),

	ACCESS_SYSTEM_SECURITY		= cpu_to_le32(0x01000000),
	MAXIMUM_ALLOWED			= cpu_to_le32(0x02000000),


	
	GENERIC_ALL			= cpu_to_le32(0x10000000),

	
	GENERIC_EXECUTE			= cpu_to_le32(0x20000000),

	GENERIC_WRITE			= cpu_to_le32(0x40000000),

	GENERIC_READ			= cpu_to_le32(0x80000000),
};

typedef le32 ACCESS_MASK;

typedef struct {
	ACCESS_MASK generic_read;
	ACCESS_MASK generic_write;
	ACCESS_MASK generic_execute;
	ACCESS_MASK generic_all;
} __attribute__ ((__packed__)) GENERIC_MAPPING;


typedef struct {
	ACE_TYPES type;		
	ACE_FLAGS flags;	
	le16 size;		
	ACCESS_MASK mask;	

	SID sid;		
} __attribute__ ((__packed__)) ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE,
			       SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE;

enum {
	ACE_OBJECT_TYPE_PRESENT			= cpu_to_le32(1),
	ACE_INHERITED_OBJECT_TYPE_PRESENT	= cpu_to_le32(2),
};

typedef le32 OBJECT_ACE_FLAGS;

typedef struct {
	ACE_TYPES type;		
	ACE_FLAGS flags;	
	le16 size;		
	ACCESS_MASK mask;	

	OBJECT_ACE_FLAGS object_flags;	
	GUID object_type;
	GUID inherited_object_type;

	SID sid;		
} __attribute__ ((__packed__)) ACCESS_ALLOWED_OBJECT_ACE,
			       ACCESS_DENIED_OBJECT_ACE,
			       SYSTEM_AUDIT_OBJECT_ACE,
			       SYSTEM_ALARM_OBJECT_ACE;

typedef struct {
	u8 revision;	
	u8 alignment1;
	le16 size;	
	le16 ace_count;	
	le16 alignment2;
} __attribute__ ((__packed__)) ACL;

typedef enum {
	
	ACL_REVISION		= 2,
	ACL_REVISION_DS		= 4,

	
	ACL_REVISION1		= 1,
	MIN_ACL_REVISION	= 2,
	ACL_REVISION2		= 2,
	ACL_REVISION3		= 3,
	ACL_REVISION4		= 4,
	MAX_ACL_REVISION	= 4,
} ACL_CONSTANTS;

enum {
	SE_OWNER_DEFAULTED		= cpu_to_le16(0x0001),
	SE_GROUP_DEFAULTED		= cpu_to_le16(0x0002),
	SE_DACL_PRESENT			= cpu_to_le16(0x0004),
	SE_DACL_DEFAULTED		= cpu_to_le16(0x0008),

	SE_SACL_PRESENT			= cpu_to_le16(0x0010),
	SE_SACL_DEFAULTED		= cpu_to_le16(0x0020),

	SE_DACL_AUTO_INHERIT_REQ	= cpu_to_le16(0x0100),
	SE_SACL_AUTO_INHERIT_REQ	= cpu_to_le16(0x0200),
	SE_DACL_AUTO_INHERITED		= cpu_to_le16(0x0400),
	SE_SACL_AUTO_INHERITED		= cpu_to_le16(0x0800),

	SE_DACL_PROTECTED		= cpu_to_le16(0x1000),
	SE_SACL_PROTECTED		= cpu_to_le16(0x2000),
	SE_RM_CONTROL_VALID		= cpu_to_le16(0x4000),
	SE_SELF_RELATIVE		= cpu_to_le16(0x8000)
} __attribute__ ((__packed__));

typedef le16 SECURITY_DESCRIPTOR_CONTROL;

typedef struct {
	u8 revision;	
	u8 alignment;
	SECURITY_DESCRIPTOR_CONTROL control; 
	le32 owner;	
	le32 group;	
	le32 sacl;	
	le32 dacl;	
} __attribute__ ((__packed__)) SECURITY_DESCRIPTOR_RELATIVE;

typedef struct {
	u8 revision;	
	u8 alignment;
	SECURITY_DESCRIPTOR_CONTROL control;	
	SID *owner;	
	SID *group;	
	ACL *sacl;	
	ACL *dacl;	
} __attribute__ ((__packed__)) SECURITY_DESCRIPTOR;

typedef enum {
	
	SECURITY_DESCRIPTOR_REVISION	= 1,
	SECURITY_DESCRIPTOR_REVISION1	= 1,

	SECURITY_DESCRIPTOR_MIN_LENGTH	= sizeof(SECURITY_DESCRIPTOR),
} SECURITY_DESCRIPTOR_CONSTANTS;

typedef SECURITY_DESCRIPTOR_RELATIVE SECURITY_DESCRIPTOR_ATTR;


typedef struct {
	le32 hash;	  
	le32 security_id; 
	le64 offset;	  
	le32 length;	  
} __attribute__ ((__packed__)) SECURITY_DESCRIPTOR_HEADER;

typedef struct {
	le32 hash;	  
	le32 security_id; 
	le64 offset;	  
	le32 length;	  
	SECURITY_DESCRIPTOR_RELATIVE sid; 
} __attribute__ ((__packed__)) SDS_ENTRY;

typedef struct {
	le32 security_id; 
} __attribute__ ((__packed__)) SII_INDEX_KEY;

typedef struct {
	le32 hash;	  
	le32 security_id; 
} __attribute__ ((__packed__)) SDH_INDEX_KEY;

typedef struct {
	ntfschar name[0];	
} __attribute__ ((__packed__)) VOLUME_NAME;

enum {
	VOLUME_IS_DIRTY			= cpu_to_le16(0x0001),
	VOLUME_RESIZE_LOG_FILE		= cpu_to_le16(0x0002),
	VOLUME_UPGRADE_ON_MOUNT		= cpu_to_le16(0x0004),
	VOLUME_MOUNTED_ON_NT4		= cpu_to_le16(0x0008),

	VOLUME_DELETE_USN_UNDERWAY	= cpu_to_le16(0x0010),
	VOLUME_REPAIR_OBJECT_ID		= cpu_to_le16(0x0020),

	VOLUME_CHKDSK_UNDERWAY		= cpu_to_le16(0x4000),
	VOLUME_MODIFIED_BY_CHKDSK	= cpu_to_le16(0x8000),

	VOLUME_FLAGS_MASK		= cpu_to_le16(0xc03f),

	
	VOLUME_MUST_MOUNT_RO_MASK	= cpu_to_le16(0xc027),
} __attribute__ ((__packed__));

typedef le16 VOLUME_FLAGS;

typedef struct {
	le64 reserved;		
	u8 major_ver;		
	u8 minor_ver;		
	VOLUME_FLAGS flags;	
} __attribute__ ((__packed__)) VOLUME_INFORMATION;

typedef struct {
	u8 data[0];		
} __attribute__ ((__packed__)) DATA_ATTR;

enum {
	SMALL_INDEX = 0, 
	LARGE_INDEX = 1, 
	LEAF_NODE  = 0, 
	INDEX_NODE = 1, 
	NODE_MASK  = 1, 
} __attribute__ ((__packed__));

typedef u8 INDEX_HEADER_FLAGS;

typedef struct {
	le32 entries_offset;		
	le32 index_length;		
	le32 allocated_size;		
	INDEX_HEADER_FLAGS flags;	
	u8 reserved[3];			
} __attribute__ ((__packed__)) INDEX_HEADER;

typedef struct {
	ATTR_TYPE type;			
	COLLATION_RULE collation_rule;	
	le32 index_block_size;		
	u8 clusters_per_index_block;	
	u8 reserved[3];			
	INDEX_HEADER index;		
} __attribute__ ((__packed__)) INDEX_ROOT;

typedef struct {
	NTFS_RECORD_TYPE magic;	
	le16 usa_ofs;		
	le16 usa_count;		

	sle64 lsn;		
	leVCN index_block_vcn;	
	INDEX_HEADER index;	
} __attribute__ ((__packed__)) INDEX_BLOCK;

typedef INDEX_BLOCK INDEX_ALLOCATION;

typedef struct {
	le32 reparse_tag;	
	leMFT_REF file_id;	
} __attribute__ ((__packed__)) REPARSE_INDEX_KEY;

enum {
	QUOTA_FLAG_DEFAULT_LIMITS	= cpu_to_le32(0x00000001),
	QUOTA_FLAG_LIMIT_REACHED	= cpu_to_le32(0x00000002),
	QUOTA_FLAG_ID_DELETED		= cpu_to_le32(0x00000004),

	QUOTA_FLAG_USER_MASK		= cpu_to_le32(0x00000007),
	

	QUOTA_FLAG_TRACKING_ENABLED	= cpu_to_le32(0x00000010),
	QUOTA_FLAG_ENFORCEMENT_ENABLED	= cpu_to_le32(0x00000020),
	QUOTA_FLAG_TRACKING_REQUESTED	= cpu_to_le32(0x00000040),
	QUOTA_FLAG_LOG_THRESHOLD	= cpu_to_le32(0x00000080),

	QUOTA_FLAG_LOG_LIMIT		= cpu_to_le32(0x00000100),
	QUOTA_FLAG_OUT_OF_DATE		= cpu_to_le32(0x00000200),
	QUOTA_FLAG_CORRUPT		= cpu_to_le32(0x00000400),
	QUOTA_FLAG_PENDING_DELETES	= cpu_to_le32(0x00000800),
};

typedef le32 QUOTA_FLAGS;

typedef struct {
	le32 version;		
	QUOTA_FLAGS flags;	
	le64 bytes_used;	
	sle64 change_time;	
	sle64 threshold;	
	sle64 limit;		
	sle64 exceeded_time;	
	SID sid;		
} __attribute__ ((__packed__)) QUOTA_CONTROL_ENTRY;

enum {
	QUOTA_INVALID_ID	= cpu_to_le32(0x00000000),
	QUOTA_DEFAULTS_ID	= cpu_to_le32(0x00000001),
	QUOTA_FIRST_USER_ID	= cpu_to_le32(0x00000100),
};

typedef enum {
	
	QUOTA_VERSION	= 2,
} QUOTA_CONTROL_ENTRY_CONSTANTS;

enum {
	INDEX_ENTRY_NODE = cpu_to_le16(1), 
	INDEX_ENTRY_END  = cpu_to_le16(2), 

	INDEX_ENTRY_SPACE_FILLER = cpu_to_le16(0xffff), 
} __attribute__ ((__packed__));

typedef le16 INDEX_ENTRY_FLAGS;

typedef struct {
	union {
		struct { 
			leMFT_REF indexed_file;	
		} __attribute__ ((__packed__)) dir;
		struct { 
			le16 data_offset;	
			le16 data_length;	
			le32 reservedV;		
		} __attribute__ ((__packed__)) vi;
	} __attribute__ ((__packed__)) data;
	le16 length;		 
	le16 key_length;	 
	INDEX_ENTRY_FLAGS flags; 
	le16 reserved;		 
} __attribute__ ((__packed__)) INDEX_ENTRY_HEADER;

typedef struct {
	union {
		struct { 
			leMFT_REF indexed_file;	
		} __attribute__ ((__packed__)) dir;
		struct { 
			le16 data_offset;	
			le16 data_length;	
			le32 reservedV;		
		} __attribute__ ((__packed__)) vi;
	} __attribute__ ((__packed__)) data;
	le16 length;		 
	le16 key_length;	 
	INDEX_ENTRY_FLAGS flags; 
	le16 reserved;		 

	union {		
		FILE_NAME_ATTR file_name;
		SII_INDEX_KEY sii;	
		SDH_INDEX_KEY sdh;	
		GUID object_id;		
		REPARSE_INDEX_KEY reparse;	
		SID sid;		
		le32 owner_id;		
	} __attribute__ ((__packed__)) key;
	
	
	
	
	
	
	
	
	
	
	
	
	
} __attribute__ ((__packed__)) INDEX_ENTRY;

typedef struct {
	u8 bitmap[0];			
} __attribute__ ((__packed__)) BITMAP_ATTR;

enum {
	IO_REPARSE_TAG_IS_ALIAS		= cpu_to_le32(0x20000000),
	IO_REPARSE_TAG_IS_HIGH_LATENCY	= cpu_to_le32(0x40000000),
	IO_REPARSE_TAG_IS_MICROSOFT	= cpu_to_le32(0x80000000),

	IO_REPARSE_TAG_RESERVED_ZERO	= cpu_to_le32(0x00000000),
	IO_REPARSE_TAG_RESERVED_ONE	= cpu_to_le32(0x00000001),
	IO_REPARSE_TAG_RESERVED_RANGE	= cpu_to_le32(0x00000001),

	IO_REPARSE_TAG_NSS		= cpu_to_le32(0x68000005),
	IO_REPARSE_TAG_NSS_RECOVER	= cpu_to_le32(0x68000006),
	IO_REPARSE_TAG_SIS		= cpu_to_le32(0x68000007),
	IO_REPARSE_TAG_DFS		= cpu_to_le32(0x68000008),

	IO_REPARSE_TAG_MOUNT_POINT	= cpu_to_le32(0x88000003),

	IO_REPARSE_TAG_HSM		= cpu_to_le32(0xa8000004),

	IO_REPARSE_TAG_SYMBOLIC_LINK	= cpu_to_le32(0xe8000000),

	IO_REPARSE_TAG_VALID_VALUES	= cpu_to_le32(0xe000ffff),
};

typedef struct {
	le32 reparse_tag;		
	le16 reparse_data_length;	
	le16 reserved;			
	u8 reparse_data[0];		
} __attribute__ ((__packed__)) REPARSE_POINT;

typedef struct {
	le16 ea_length;		
	le16 need_ea_count;	
	le32 ea_query_length;	
} __attribute__ ((__packed__)) EA_INFORMATION;

enum {
	NEED_EA	= 0x80		
} __attribute__ ((__packed__));

typedef u8 EA_FLAGS;

typedef struct {
	le32 next_entry_offset;	
	EA_FLAGS flags;		
	u8 ea_name_length;	
	le16 ea_value_length;	
	u8 ea_name[0];		
	u8 ea_value[0];		
} __attribute__ ((__packed__)) EA_ATTR;

typedef struct {
	
} __attribute__ ((__packed__)) PROPERTY_SET;

typedef struct {
	
	
	
} __attribute__ ((__packed__)) LOGGED_UTILITY_STREAM, EFS_ATTR;

#endif 
