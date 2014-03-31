/*
 * ecma_167.h
 *
 * This file is based on ECMA-167 3rd edition (June 1997)
 * http://www.ecma.ch
 *
 * Copyright (c) 2001-2002  Ben Fennema <bfennema@falcon.csc.calpoly.edu>
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
 * GNU Public License ("GPL").
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
 */

#include <linux/types.h>

#ifndef _ECMA_167_H
#define _ECMA_167_H 1

struct charspec {
	uint8_t		charSetType;
	uint8_t		charSetInfo[63];
} __attribute__ ((packed));

#define CHARSPEC_TYPE_CS0		0x00	
#define CHARSPEC_TYPE_CS1		0x01	
#define CHARSPEC_TYPE_CS2		0x02	
#define CHARSPEC_TYPE_CS3		0x03	
#define CHARSPEC_TYPE_CS4		0x04	
#define CHARSPEC_TYPE_CS5		0x05	
#define CHARSPEC_TYPE_CS6		0x06	
#define CHARSPEC_TYPE_CS7		0x07	
#define CHARSPEC_TYPE_CS8		0x08	

typedef uint8_t		dstring;

struct timestamp {
	__le16		typeAndTimezone;
	__le16		year;
	uint8_t		month;
	uint8_t		day;
	uint8_t		hour;
	uint8_t		minute;
	uint8_t		second;
	uint8_t		centiseconds;
	uint8_t		hundredsOfMicroseconds;
	uint8_t		microseconds;
} __attribute__ ((packed));

#define TIMESTAMP_TYPE_MASK		0xF000
#define TIMESTAMP_TYPE_CUT		0x0000
#define TIMESTAMP_TYPE_LOCAL		0x1000
#define TIMESTAMP_TYPE_AGREEMENT	0x2000
#define TIMESTAMP_TIMEZONE_MASK		0x0FFF

struct regid {
	uint8_t		flags;
	uint8_t		ident[23];
	uint8_t		identSuffix[8];
} __attribute__ ((packed));

#define ENTITYID_FLAGS_DIRTY		0x00
#define ENTITYID_FLAGS_PROTECTED	0x01

#define VSD_STD_ID_LEN			5
struct volStructDesc {
	uint8_t		structType;
	uint8_t		stdIdent[VSD_STD_ID_LEN];
	uint8_t		structVersion;
	uint8_t		structData[2041];
} __attribute__ ((packed));

#define VSD_STD_ID_NSR02		"NSR02"	

#define VSD_STD_ID_BEA01		"BEA01"	
#define VSD_STD_ID_BOOT2		"BOOT2"	
#define VSD_STD_ID_CD001		"CD001"	
#define VSD_STD_ID_CDW02		"CDW02"	
#define VSD_STD_ID_NSR03		"NSR03"	
#define VSD_STD_ID_TEA01		"TEA01"	

struct beginningExtendedAreaDesc {
	uint8_t		structType;
	uint8_t		stdIdent[VSD_STD_ID_LEN];
	uint8_t		structVersion;
	uint8_t		structData[2041];
} __attribute__ ((packed));

struct terminatingExtendedAreaDesc {
	uint8_t		structType;
	uint8_t		stdIdent[VSD_STD_ID_LEN];
	uint8_t		structVersion;
	uint8_t		structData[2041];
} __attribute__ ((packed));

struct bootDesc {
	uint8_t			structType;
	uint8_t			stdIdent[VSD_STD_ID_LEN];
	uint8_t			structVersion;
	uint8_t			reserved1;
	struct regid		archType;
	struct regid		bootIdent;
	__le32			bootExtLocation;
	__le32			bootExtLength;
	__le64			loadAddress;
	__le64			startAddress;
	struct timestamp	descCreationDateAndTime;
	__le16			flags;
	uint8_t			reserved2[32];
	uint8_t			bootUse[1906];
} __attribute__ ((packed));

#define BOOT_FLAGS_ERASE		0x01

struct extent_ad {
	__le32		extLength;
	__le32		extLocation;
} __attribute__ ((packed));

struct kernel_extent_ad {
	uint32_t	extLength;
	uint32_t	extLocation;
};

struct tag {
	__le16		tagIdent;
	__le16		descVersion;
	uint8_t		tagChecksum;
	uint8_t		reserved;
	__le16		tagSerialNum;
	__le16		descCRC;
	__le16		descCRCLength;
	__le32		tagLocation;
} __attribute__ ((packed));

#define TAG_IDENT_PVD			0x0001
#define TAG_IDENT_AVDP			0x0002
#define TAG_IDENT_VDP			0x0003
#define TAG_IDENT_IUVD			0x0004
#define TAG_IDENT_PD			0x0005
#define TAG_IDENT_LVD			0x0006
#define TAG_IDENT_USD			0x0007
#define TAG_IDENT_TD			0x0008
#define TAG_IDENT_LVID			0x0009

struct NSRDesc {
	uint8_t		structType;
	uint8_t		stdIdent[VSD_STD_ID_LEN];
	uint8_t		structVersion;
	uint8_t		reserved;
	uint8_t		structData[2040];
} __attribute__ ((packed));

struct primaryVolDesc {
	struct tag		descTag;
	__le32			volDescSeqNum;
	__le32			primaryVolDescNum;
	dstring			volIdent[32];
	__le16			volSeqNum;
	__le16			maxVolSeqNum;
	__le16			interchangeLvl;
	__le16			maxInterchangeLvl;
	__le32			charSetList;
	__le32			maxCharSetList;
	dstring			volSetIdent[128];
	struct charspec		descCharSet;
	struct charspec		explanatoryCharSet;
	struct extent_ad	volAbstract;
	struct extent_ad	volCopyright;
	struct regid		appIdent;
	struct timestamp	recordingDateAndTime;
	struct regid		impIdent;
	uint8_t			impUse[64];
	__le32			predecessorVolDescSeqLocation;
	__le16			flags;
	uint8_t			reserved[22];
} __attribute__ ((packed));

#define PVD_FLAGS_VSID_COMMON		0x0001

struct anchorVolDescPtr {
	struct tag		descTag;
	struct extent_ad	mainVolDescSeqExt;
	struct extent_ad	reserveVolDescSeqExt;
	uint8_t	 		reserved[480];
} __attribute__ ((packed));

struct volDescPtr {
	struct tag		descTag;
	__le32			volDescSeqNum;
	struct extent_ad	nextVolDescSeqExt;
	uint8_t			reserved[484];
} __attribute__ ((packed));

struct impUseVolDesc {
	struct tag	descTag;
	__le32		volDescSeqNum;
	struct regid	impIdent;
	uint8_t		impUse[460];
} __attribute__ ((packed));

struct partitionDesc {
	struct tag descTag;
	__le32 volDescSeqNum;
	__le16 partitionFlags;
	__le16 partitionNumber;
	struct regid partitionContents;
	uint8_t partitionContentsUse[128];
	__le32 accessType;
	__le32 partitionStartingLocation;
	__le32 partitionLength;
	struct regid impIdent;
	uint8_t impUse[128];
	uint8_t reserved[156];
} __attribute__ ((packed));

#define PD_PARTITION_FLAGS_ALLOC	0x0001

#define PD_PARTITION_CONTENTS_NSR02	"+NSR02"

#define PD_PARTITION_CONTENTS_FDC01	"+FDC01"
#define PD_PARTITION_CONTENTS_CD001	"+CD001"
#define PD_PARTITION_CONTENTS_CDW02	"+CDW02"
#define PD_PARTITION_CONTENTS_NSR03	"+NSR03"

#define PD_ACCESS_TYPE_NONE		0x00000000
#define PD_ACCESS_TYPE_READ_ONLY	0x00000001
#define PD_ACCESS_TYPE_WRITE_ONCE	0x00000002
#define PD_ACCESS_TYPE_REWRITABLE	0x00000003
#define PD_ACCESS_TYPE_OVERWRITABLE	0x00000004

struct logicalVolDesc {
	struct tag		descTag;
	__le32			volDescSeqNum;
	struct charspec		descCharSet;
	dstring			logicalVolIdent[128];
	__le32			logicalBlockSize;
	struct regid		domainIdent;
	uint8_t			logicalVolContentsUse[16];
	__le32			mapTableLength;
	__le32			numPartitionMaps;
	struct regid		impIdent;
	uint8_t			impUse[128];
	struct extent_ad	integritySeqExt;
	uint8_t			partitionMaps[0];
} __attribute__ ((packed));

struct genericPartitionMap {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		partitionMapping[0];
} __attribute__ ((packed));

#define GP_PARTITION_MAP_TYPE_UNDEF	0x00
#define GP_PARTIITON_MAP_TYPE_1		0x01
#define GP_PARTITION_MAP_TYPE_2		0x02

struct genericPartitionMap1 {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	__le16		volSeqNum;
	__le16		partitionNum;
} __attribute__ ((packed));

struct genericPartitionMap2 {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		partitionIdent[62];
} __attribute__ ((packed));

struct unallocSpaceDesc {
	struct tag		descTag;
	__le32			volDescSeqNum;
	__le32			numAllocDescs;
	struct extent_ad	allocDescs[0];
} __attribute__ ((packed));

struct terminatingDesc {
	struct tag	descTag;
	uint8_t		reserved[496];
} __attribute__ ((packed));

struct logicalVolIntegrityDesc {
	struct tag		descTag;
	struct timestamp	recordingDateAndTime;
	__le32			integrityType;
	struct extent_ad	nextIntegrityExt;
	uint8_t			logicalVolContentsUse[32];
	__le32			numOfPartitions;
	__le32			lengthOfImpUse;
	__le32			freeSpaceTable[0];
	__le32			sizeTable[0];
	uint8_t			impUse[0];
} __attribute__ ((packed));

#define LVID_INTEGRITY_TYPE_OPEN	0x00000000
#define LVID_INTEGRITY_TYPE_CLOSE	0x00000001

struct lb_addr {
	__le32		logicalBlockNum;
	__le16	 	partitionReferenceNum;
} __attribute__ ((packed));

struct kernel_lb_addr {
	uint32_t		logicalBlockNum;
	uint16_t	 	partitionReferenceNum;
};

struct short_ad {
        __le32		extLength;
        __le32		extPosition;
} __attribute__ ((packed));

struct long_ad {
	__le32		extLength;
	struct lb_addr	extLocation;
	uint8_t		impUse[6];
} __attribute__ ((packed));

struct kernel_long_ad {
	uint32_t		extLength;
	struct kernel_lb_addr	extLocation;
	uint8_t			impUse[6];
};

struct ext_ad {
	__le32		extLength;
	__le32		recordedLength;
	__le32		informationLength;
	struct lb_addr	extLocation;
} __attribute__ ((packed));

struct kernel_ext_ad {
	uint32_t		extLength;
	uint32_t		recordedLength;
	uint32_t		informationLength;
	struct kernel_lb_addr	extLocation;
};


#define TAG_IDENT_FSD			0x0100
#define TAG_IDENT_FID			0x0101
#define TAG_IDENT_AED			0x0102
#define TAG_IDENT_IE			0x0103
#define TAG_IDENT_TE			0x0104
#define TAG_IDENT_FE			0x0105
#define TAG_IDENT_EAHD			0x0106
#define TAG_IDENT_USE			0x0107
#define TAG_IDENT_SBD			0x0108
#define TAG_IDENT_PIE			0x0109
#define TAG_IDENT_EFE			0x010A

struct fileSetDesc {
	struct tag		descTag;
	struct timestamp	recordingDateAndTime;
	__le16			interchangeLvl;
	__le16			maxInterchangeLvl;
	__le32			charSetList;
	__le32			maxCharSetList;
	__le32			fileSetNum;
	__le32			fileSetDescNum;
	struct charspec		logicalVolIdentCharSet;
	dstring			logicalVolIdent[128];
	struct charspec		fileSetCharSet;
	dstring			fileSetIdent[32];
	dstring			copyrightFileIdent[32];
	dstring			abstractFileIdent[32];
	struct long_ad		rootDirectoryICB;
	struct regid		domainIdent;
	struct long_ad		nextExt;
	struct long_ad		streamDirectoryICB;
	uint8_t			reserved[32];
} __attribute__ ((packed));

struct partitionHeaderDesc {
	struct short_ad	unallocSpaceTable;
	struct short_ad	unallocSpaceBitmap;
	struct short_ad	partitionIntegrityTable;
	struct short_ad	freedSpaceTable;
	struct short_ad	freedSpaceBitmap;
	uint8_t		reserved[88];
} __attribute__ ((packed));

struct fileIdentDesc {
	struct tag	descTag;
	__le16		fileVersionNum;
	uint8_t		fileCharacteristics;
	uint8_t		lengthFileIdent;
	struct long_ad	icb;
	__le16		lengthOfImpUse;
	uint8_t		impUse[0];
	uint8_t		fileIdent[0];
	uint8_t		padding[0];
} __attribute__ ((packed));

#define FID_FILE_CHAR_HIDDEN		0x01
#define FID_FILE_CHAR_DIRECTORY		0x02
#define FID_FILE_CHAR_DELETED		0x04
#define FID_FILE_CHAR_PARENT		0x08
#define FID_FILE_CHAR_METADATA		0x10

struct allocExtDesc {
	struct tag	descTag;
	__le32		previousAllocExtLocation;
	__le32		lengthAllocDescs;
} __attribute__ ((packed));

struct icbtag {
	__le32		priorRecordedNumDirectEntries;
	__le16		strategyType;
	__le16		strategyParameter;
	__le16		numEntries;
	uint8_t		reserved;
	uint8_t		fileType;
	struct lb_addr	parentICBLocation;
	__le16		flags;
} __attribute__ ((packed));

#define ICBTAG_STRATEGY_TYPE_UNDEF	0x0000
#define ICBTAG_STRATEGY_TYPE_1		0x0001
#define ICBTAG_STRATEGY_TYPE_2		0x0002
#define ICBTAG_STRATEGY_TYPE_3		0x0003
#define ICBTAG_STRATEGY_TYPE_4		0x0004

#define ICBTAG_FILE_TYPE_UNDEF		0x00
#define ICBTAG_FILE_TYPE_USE		0x01
#define ICBTAG_FILE_TYPE_PIE		0x02
#define ICBTAG_FILE_TYPE_IE		0x03
#define ICBTAG_FILE_TYPE_DIRECTORY	0x04
#define ICBTAG_FILE_TYPE_REGULAR	0x05
#define ICBTAG_FILE_TYPE_BLOCK		0x06
#define ICBTAG_FILE_TYPE_CHAR		0x07
#define ICBTAG_FILE_TYPE_EA		0x08
#define ICBTAG_FILE_TYPE_FIFO		0x09
#define ICBTAG_FILE_TYPE_SOCKET		0x0A
#define ICBTAG_FILE_TYPE_TE		0x0B
#define ICBTAG_FILE_TYPE_SYMLINK	0x0C
#define ICBTAG_FILE_TYPE_STREAMDIR	0x0D

#define ICBTAG_FLAG_AD_MASK		0x0007
#define ICBTAG_FLAG_AD_SHORT		0x0000
#define ICBTAG_FLAG_AD_LONG		0x0001
#define ICBTAG_FLAG_AD_EXTENDED		0x0002
#define ICBTAG_FLAG_AD_IN_ICB		0x0003
#define ICBTAG_FLAG_SORTED		0x0008
#define ICBTAG_FLAG_NONRELOCATABLE	0x0010
#define ICBTAG_FLAG_ARCHIVE		0x0020
#define ICBTAG_FLAG_SETUID		0x0040
#define ICBTAG_FLAG_SETGID		0x0080
#define ICBTAG_FLAG_STICKY		0x0100
#define ICBTAG_FLAG_CONTIGUOUS		0x0200
#define ICBTAG_FLAG_SYSTEM		0x0400
#define ICBTAG_FLAG_TRANSFORMED		0x0800
#define ICBTAG_FLAG_MULTIVERSIONS	0x1000
#define ICBTAG_FLAG_STREAM		0x2000

struct indirectEntry {
	struct tag	descTag;
	struct icbtag	icbTag;
	struct long_ad	indirectICB;
} __attribute__ ((packed));

struct terminalEntry {
	struct tag	descTag;
	struct icbtag	icbTag;
} __attribute__ ((packed));

struct fileEntry {
	struct tag		descTag;
	struct icbtag		icbTag;
	__le32			uid;
	__le32			gid;
	__le32			permissions;
	__le16			fileLinkCount;
	uint8_t			recordFormat;
	uint8_t			recordDisplayAttr;
	__le32			recordLength;
	__le64			informationLength;
	__le64			logicalBlocksRecorded;
	struct timestamp	accessTime;
	struct timestamp	modificationTime;
	struct timestamp	attrTime;
	__le32			checkpoint;
	struct long_ad		extendedAttrICB;
	struct regid		impIdent;
	__le64			uniqueID;
	__le32			lengthExtendedAttr;
	__le32			lengthAllocDescs;
	uint8_t			extendedAttr[0];
	uint8_t			allocDescs[0];
} __attribute__ ((packed));

#define FE_PERM_O_EXEC			0x00000001U
#define FE_PERM_O_WRITE			0x00000002U
#define FE_PERM_O_READ			0x00000004U
#define FE_PERM_O_CHATTR		0x00000008U
#define FE_PERM_O_DELETE		0x00000010U
#define FE_PERM_G_EXEC			0x00000020U
#define FE_PERM_G_WRITE			0x00000040U
#define FE_PERM_G_READ			0x00000080U
#define FE_PERM_G_CHATTR		0x00000100U
#define FE_PERM_G_DELETE		0x00000200U
#define FE_PERM_U_EXEC			0x00000400U
#define FE_PERM_U_WRITE			0x00000800U
#define FE_PERM_U_READ			0x00001000U
#define FE_PERM_U_CHATTR		0x00002000U
#define FE_PERM_U_DELETE		0x00004000U

#define FE_RECORD_FMT_UNDEF		0x00
#define FE_RECORD_FMT_FIXED_PAD		0x01
#define FE_RECORD_FMT_FIXED		0x02
#define FE_RECORD_FMT_VARIABLE8		0x03
#define FE_RECORD_FMT_VARIABLE16	0x04
#define FE_RECORD_FMT_VARIABLE16_MSB	0x05
#define FE_RECORD_FMT_VARIABLE32	0x06
#define FE_RECORD_FMT_PRINT		0x07
#define FE_RECORD_FMT_LF		0x08
#define FE_RECORD_FMT_CR		0x09
#define FE_RECORD_FMT_CRLF		0x0A
#define FE_RECORD_FMT_LFCR		0x0B

#define FE_RECORD_DISPLAY_ATTR_UNDEF	0x00
#define FE_RECORD_DISPLAY_ATTR_1	0x01
#define FE_RECORD_DISPLAY_ATTR_2	0x02
#define FE_RECORD_DISPLAY_ATTR_3	0x03

struct extendedAttrHeaderDesc {
	struct tag	descTag;
	__le32		impAttrLocation;
	__le32		appAttrLocation;
} __attribute__ ((packed));

struct genericFormat {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	uint8_t		attrData[0];
} __attribute__ ((packed));

struct charSetInfo {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		escapeSeqLength;
	uint8_t		charSetType;
	uint8_t		escapeSeq[0];
} __attribute__ ((packed));

struct altPerms {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le16		ownerIdent;
	__le16		groupIdent;
	__le16		permission;
} __attribute__ ((packed));

struct fileTimesExtAttr {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		dataLength;
	__le32		fileTimeExistence;
	uint8_t		fileTimes;
} __attribute__ ((packed));

#define FTE_CREATION			0x00000001
#define FTE_DELETION			0x00000004
#define FTE_EFFECTIVE			0x00000008
#define FTE_BACKUP			0x00000002

struct infoTimesExtAttr {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		dataLength;
	__le32		infoTimeExistence;
	uint8_t		infoTimes[0];
} __attribute__ ((packed));

struct deviceSpec {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		impUseLength;
	__le32		majorDeviceIdent;
	__le32		minorDeviceIdent;
	uint8_t		impUse[0];
} __attribute__ ((packed));

struct impUseExtAttr {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		impUseLength;
	struct regid	impIdent;
	uint8_t		impUse[0];
} __attribute__ ((packed));

struct appUseExtAttr {
	__le32		attrType;
	uint8_t		attrSubtype;
	uint8_t		reserved[3];
	__le32		attrLength;
	__le32		appUseLength;
	struct regid	appIdent;
	uint8_t		appUse[0];
} __attribute__ ((packed));

#define EXTATTR_CHAR_SET		1
#define EXTATTR_ALT_PERMS		3
#define EXTATTR_FILE_TIMES		5
#define EXTATTR_INFO_TIMES		6
#define EXTATTR_DEV_SPEC		12
#define EXTATTR_IMP_USE			2048
#define EXTATTR_APP_USE			65536

struct unallocSpaceEntry {
	struct tag	descTag;
	struct icbtag	icbTag;
	__le32		lengthAllocDescs;
	uint8_t		allocDescs[0];
} __attribute__ ((packed));

struct spaceBitmapDesc {
	struct tag	descTag;
	__le32		numOfBits;
	__le32		numOfBytes;
	uint8_t		bitmap[0];
} __attribute__ ((packed));

struct partitionIntegrityEntry {
	struct tag		descTag;
	struct icbtag		icbTag;
	struct timestamp	recordingDateAndTime;
	uint8_t			integrityType;
	uint8_t			reserved[175];
	struct regid		impIdent;
	uint8_t			impUse[256];
} __attribute__ ((packed));


#define EXT_RECORDED_ALLOCATED		0x00000000
#define EXT_NOT_RECORDED_ALLOCATED	0x40000000
#define EXT_NOT_RECORDED_NOT_ALLOCATED	0x80000000
#define EXT_NEXT_EXTENT_ALLOCDECS	0xC0000000



struct logicalVolHeaderDesc {
	__le64		uniqueID;
	uint8_t		reserved[24];
} __attribute__ ((packed));

struct pathComponent {
	uint8_t		componentType;
	uint8_t		lengthComponentIdent;
	__le16		componentFileVersionNum;
	dstring		componentIdent[0];
} __attribute__ ((packed));

struct extendedFileEntry {
	struct tag		descTag;
	struct icbtag		icbTag;
	__le32			uid;
	__le32			gid;
	__le32			permissions;
	__le16			fileLinkCount;
	uint8_t			recordFormat;
	uint8_t			recordDisplayAttr;
	__le32			recordLength;
	__le64			informationLength;
	__le64			objectSize;
	__le64			logicalBlocksRecorded;
	struct timestamp	accessTime;
	struct timestamp	modificationTime;
	struct timestamp	createTime;
	struct timestamp	attrTime;
	__le32			checkpoint;
	__le32			reserved;
	struct long_ad		extendedAttrICB;
	struct long_ad		streamDirectoryICB;
	struct regid		impIdent;
	__le64			uniqueID;
	__le32			lengthExtendedAttr;
	__le32			lengthAllocDescs;
	uint8_t			extendedAttr[0];
	uint8_t			allocDescs[0];
} __attribute__ ((packed));

#endif 
