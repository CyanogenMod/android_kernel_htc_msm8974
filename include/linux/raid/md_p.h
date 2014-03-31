/*
   md_p.h : physical layout of Linux RAID devices
          Copyright (C) 1996-98 Ingo Molnar, Gadi Oxman
	  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#ifndef _MD_P_H
#define _MD_P_H

#include <linux/types.h>


#define MD_RESERVED_BYTES		(64 * 1024)
#define MD_RESERVED_SECTORS		(MD_RESERVED_BYTES / 512)

#define MD_NEW_SIZE_SECTORS(x)		((x & ~(MD_RESERVED_SECTORS - 1)) - MD_RESERVED_SECTORS)

#define MD_SB_BYTES			4096
#define MD_SB_WORDS			(MD_SB_BYTES / 4)
#define MD_SB_SECTORS			(MD_SB_BYTES / 512)

#define	MD_SB_GENERIC_OFFSET		0
#define MD_SB_PERSONALITY_OFFSET	64
#define MD_SB_DISKS_OFFSET		128
#define MD_SB_DESCRIPTOR_OFFSET		992

#define MD_SB_GENERIC_CONSTANT_WORDS	32
#define MD_SB_GENERIC_STATE_WORDS	32
#define MD_SB_GENERIC_WORDS		(MD_SB_GENERIC_CONSTANT_WORDS + MD_SB_GENERIC_STATE_WORDS)
#define MD_SB_PERSONALITY_WORDS		64
#define MD_SB_DESCRIPTOR_WORDS		32
#define MD_SB_DISKS			27
#define MD_SB_DISKS_WORDS		(MD_SB_DISKS*MD_SB_DESCRIPTOR_WORDS)
#define MD_SB_RESERVED_WORDS		(1024 - MD_SB_GENERIC_WORDS - MD_SB_PERSONALITY_WORDS - MD_SB_DISKS_WORDS - MD_SB_DESCRIPTOR_WORDS)
#define MD_SB_EQUAL_WORDS		(MD_SB_GENERIC_WORDS + MD_SB_PERSONALITY_WORDS + MD_SB_DISKS_WORDS)

#define MD_DISK_FAULTY		0 
#define MD_DISK_ACTIVE		1 
#define MD_DISK_SYNC		2 
#define MD_DISK_REMOVED		3 

#define	MD_DISK_WRITEMOSTLY	9 

typedef struct mdp_device_descriptor_s {
	__u32 number;		
	__u32 major;		
	__u32 minor;		
	__u32 raid_disk;	
	__u32 state;		
	__u32 reserved[MD_SB_DESCRIPTOR_WORDS - 5];
} mdp_disk_t;

#define MD_SB_MAGIC		0xa92b4efc

#define MD_SB_CLEAN		0
#define MD_SB_ERRORS		1

#define	MD_SB_BITMAP_PRESENT	8 



typedef struct mdp_superblock_s {
	__u32 md_magic;		
	__u32 major_version;	
	__u32 minor_version;	
	__u32 patch_version;	
	__u32 gvalid_words;	
	__u32 set_uuid0;	
	__u32 ctime;		
	__u32 level;		
	__u32 size;		
	__u32 nr_disks;		
	__u32 raid_disks;	
	__u32 md_minor;		
	__u32 not_persistent;	
	__u32 set_uuid1;	
	__u32 set_uuid2;	
	__u32 set_uuid3;	
	__u32 gstate_creserved[MD_SB_GENERIC_CONSTANT_WORDS - 16];

	__u32 utime;		
	__u32 state;		
	__u32 active_disks;	
	__u32 working_disks;	
	__u32 failed_disks;	
	__u32 spare_disks;	
	__u32 sb_csum;		
#ifdef __BIG_ENDIAN
	__u32 events_hi;	
	__u32 events_lo;	
	__u32 cp_events_hi;	
	__u32 cp_events_lo;	
#else
	__u32 events_lo;	
	__u32 events_hi;	
	__u32 cp_events_lo;	
	__u32 cp_events_hi;	
#endif
	__u32 recovery_cp;	
	
	__u64 reshape_position;	
	__u32 new_level;	
	__u32 delta_disks;	
	__u32 new_layout;	
	__u32 new_chunk;	
	__u32 gstate_sreserved[MD_SB_GENERIC_STATE_WORDS - 18];

	__u32 layout;		
	__u32 chunk_size;	
	__u32 root_pv;		
	__u32 root_block;	
	__u32 pstate_reserved[MD_SB_PERSONALITY_WORDS - 4];

	mdp_disk_t disks[MD_SB_DISKS];

	__u32 reserved[MD_SB_RESERVED_WORDS];

	mdp_disk_t this_disk;

} mdp_super_t;

static inline __u64 md_event(mdp_super_t *sb) {
	__u64 ev = sb->events_hi;
	return (ev<<32)| sb->events_lo;
}

#define MD_SUPERBLOCK_1_TIME_SEC_MASK ((1ULL<<40) - 1)

struct mdp_superblock_1 {
	
	__le32	magic;		
	__le32	major_version;	
	__le32	feature_map;	
	__le32	pad0;		

	__u8	set_uuid[16];	
	char	set_name[32];	

	__le64	ctime;		
	__le32	level;		
	__le32	layout;		
	__le64	size;		

	__le32	chunksize;	
	__le32	raid_disks;
	__le32	bitmap_offset;	

	
	__le32	new_level;	
	__le64	reshape_position;	
	__le32	delta_disks;	
	__le32	new_layout;	
	__le32	new_chunk;	
	__u8	pad1[128-124];	/* set to 0 when written */

	
	__le64	data_offset;	
	__le64	data_size;	
	__le64	super_offset;	
	__le64	recovery_offset;
	__le32	dev_number;	
	__le32	cnt_corrected_read; 
	__u8	device_uuid[16]; 
	__u8	devflags;	
#define	WriteMostly1	1	
	__u8	bblog_shift;	
	__le16	bblog_size;	
	__le32	bblog_offset;	

	
	__le64	utime;		
	__le64	events;		
	__le64	resync_offset;	
	__le32	sb_csum;	
	__le32	max_dev;	
	__u8	pad3[64-32];	

	__le16	dev_roles[0];	
};

#define MD_FEATURE_BITMAP_OFFSET	1
#define	MD_FEATURE_RECOVERY_OFFSET	2 
#define	MD_FEATURE_RESHAPE_ACTIVE	4
#define	MD_FEATURE_BAD_BLOCKS		8 
#define	MD_FEATURE_REPLACEMENT		16 
#define	MD_FEATURE_ALL			(MD_FEATURE_BITMAP_OFFSET	\
					|MD_FEATURE_RECOVERY_OFFSET	\
					|MD_FEATURE_RESHAPE_ACTIVE	\
					|MD_FEATURE_BAD_BLOCKS		\
					|MD_FEATURE_REPLACEMENT)

#endif 
