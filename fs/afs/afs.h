/* AFS common types
 *
 * Copyright (C) 2002, 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AFS_H
#define AFS_H

#include <linux/in.h>

#define AFS_MAXCELLNAME	64		
#define AFS_MAXVOLNAME	64		
#define AFSNAMEMAX	256		
#define AFSPATHMAX	1024		
#define AFSOPAQUEMAX	1024		

typedef unsigned			afs_volid_t;
typedef unsigned			afs_vnodeid_t;
typedef unsigned long long		afs_dataversion_t;

typedef enum {
	AFSVL_RWVOL,			
	AFSVL_ROVOL,			
	AFSVL_BACKVOL,			
} __attribute__((packed)) afs_voltype_t;

typedef enum {
	AFS_FTYPE_INVALID	= 0,
	AFS_FTYPE_FILE		= 1,
	AFS_FTYPE_DIR		= 2,
	AFS_FTYPE_SYMLINK	= 3,
} afs_file_type_t;

typedef enum {
	AFS_LOCK_READ		= 0,	
	AFS_LOCK_WRITE		= 1,	
} afs_lock_type_t;

#define AFS_LOCKWAIT		(5 * 60) 

struct afs_fid {
	afs_volid_t	vid;		
	afs_vnodeid_t	vnode;		
	unsigned	unique;		
};

typedef enum {
	AFSCM_CB_UNTYPED	= 0,	
	AFSCM_CB_EXCLUSIVE	= 1,	
	AFSCM_CB_SHARED		= 2,	
	AFSCM_CB_DROPPED	= 3,	
} afs_callback_type_t;

struct afs_callback {
	struct afs_fid		fid;		
	unsigned		version;	
	unsigned		expiry;		
	afs_callback_type_t	type;		
};

#define AFSCBMAX 50	

struct afs_volume_info {
	afs_volid_t		vid;		
	afs_voltype_t		type;		
	afs_volid_t		type_vids[5];	

	
	size_t			nservers;	
	struct {
		struct in_addr	addr;		
	} servers[8];
};

typedef u32 afs_access_t;
#define AFS_ACE_READ		0x00000001U	
#define AFS_ACE_WRITE		0x00000002U	
#define AFS_ACE_INSERT		0x00000004U	
#define AFS_ACE_LOOKUP		0x00000008U	
#define AFS_ACE_DELETE		0x00000010U	
#define AFS_ACE_LOCK		0x00000020U	
#define AFS_ACE_ADMINISTER	0x00000040U	
#define AFS_ACE_USER_A		0x01000000U	
#define AFS_ACE_USER_B		0x02000000U	
#define AFS_ACE_USER_C		0x04000000U	
#define AFS_ACE_USER_D		0x08000000U	
#define AFS_ACE_USER_E		0x10000000U	
#define AFS_ACE_USER_F		0x20000000U	
#define AFS_ACE_USER_G		0x40000000U	
#define AFS_ACE_USER_H		0x80000000U	

struct afs_file_status {
	unsigned		if_version;	
#define AFS_FSTATUS_VERSION	1

	afs_file_type_t		type;		
	unsigned		nlink;		
	u64			size;		
	afs_dataversion_t	data_version;	
	u32			author;		
	u32			owner;		
	u32			group;		
	afs_access_t		caller_access;	
	afs_access_t		anon_access;	
	umode_t			mode;		
	struct afs_fid		parent;		
	time_t			mtime_client;	
	time_t			mtime_server;	
	s32			lock_count;	
};

struct afs_store_status {
	u32			mask;		
	u32			mtime_client;	
	u32			owner;		
	u32			group;		
	umode_t			mode;		
};

#define AFS_SET_MTIME		0x01		
#define AFS_SET_OWNER		0x02		
#define AFS_SET_GROUP		0x04		
#define AFS_SET_MODE		0x08		
#define AFS_SET_SEG_SIZE	0x10		

struct afs_volsync {
	time_t			creation;	
};

struct afs_volume_status {
	u32			vid;		
	u32			parent_id;	
	u8			online;		
	u8			in_service;	
	u8			blessed;	
	u8			needs_salvage;	
	u32			type;		
	u32			min_quota;	
	u32			max_quota;	
	u32			blocks_in_use;	
	u32			part_blocks_avail; 
	u32			part_max_blocks; 
};

#define AFS_BLOCK_SIZE	1024

#endif 
