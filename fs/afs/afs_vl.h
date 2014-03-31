/* AFS Volume Location Service client interface
 *
 * Copyright (C) 2002, 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AFS_VL_H
#define AFS_VL_H

#include "afs.h"

#define AFS_VL_PORT		7003	
#define VL_SERVICE		52	

enum AFSVL_Operations {
	VLGETENTRYBYID		= 503,	
	VLGETENTRYBYNAME	= 504,	
	VLPROBE			= 514,	
};

enum AFSVL_Errors {
	AFSVL_IDEXIST 		= 363520,	
	AFSVL_IO 		= 363521,	
	AFSVL_NAMEEXIST 	= 363522,	
	AFSVL_CREATEFAIL 	= 363523,	
	AFSVL_NOENT 		= 363524,	
	AFSVL_EMPTY 		= 363525,	
	AFSVL_ENTDELETED 	= 363526,	
	AFSVL_BADNAME 		= 363527,	
	AFSVL_BADINDEX 		= 363528,	
	AFSVL_BADVOLTYPE 	= 363529,	
	AFSVL_BADSERVER 	= 363530,	
	AFSVL_BADPARTITION 	= 363531,	
	AFSVL_REPSFULL 		= 363532,	
	AFSVL_NOREPSERVER 	= 363533,	
	AFSVL_DUPREPSERVER 	= 363534,	
	AFSVL_RWNOTFOUND 	= 363535,	
	AFSVL_BADREFCOUNT 	= 363536,	
	AFSVL_SIZEEXCEEDED 	= 363537,	
	AFSVL_BADENTRY 		= 363538,	
	AFSVL_BADVOLIDBUMP 	= 363539,	
	AFSVL_IDALREADYHASHED 	= 363540,	
	AFSVL_ENTRYLOCKED 	= 363541,	
	AFSVL_BADVOLOPER 	= 363542,	
	AFSVL_BADRELLOCKTYPE 	= 363543,	
	AFSVL_RERELEASE 	= 363544,	
	AFSVL_BADSERVERFLAG 	= 363545,	
	AFSVL_PERM 		= 363546,	
	AFSVL_NOMEM 		= 363547,	
};

struct afs_vldbentry {
	char		name[65];		
	afs_voltype_t	type;			
	unsigned	num_servers;		
	unsigned	clone_id;		

	unsigned	flags;
#define AFS_VLF_RWEXISTS	0x1000		
#define AFS_VLF_ROEXISTS	0x2000		
#define AFS_VLF_BACKEXISTS	0x4000		

	afs_volid_t	volume_ids[3];		

	struct {
		struct in_addr	addr;		
		unsigned	partition;	
		unsigned	flags;		
#define AFS_VLSF_NEWREPSITE	0x0001	
#define AFS_VLSF_ROVOL		0x0002	
#define AFS_VLSF_RWVOL		0x0004	
#define AFS_VLSF_BACKVOL	0x0008	
	} servers[8];
};

#endif 
