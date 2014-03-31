/* AFS File Service definitions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef AFS_FS_H
#define AFS_FS_H

#define AFS_FS_PORT		7000	
#define FS_SERVICE		1	

enum AFS_FS_Operations {
	FSFETCHDATA		= 130,	
	FSFETCHSTATUS		= 132,	
	FSSTOREDATA		= 133,	
	FSSTORESTATUS		= 135,	
	FSREMOVEFILE		= 136,	
	FSCREATEFILE		= 137,	
	FSRENAME		= 138,	
	FSSYMLINK		= 139,	
	FSLINK			= 140,	
	FSMAKEDIR		= 141,	
	FSREMOVEDIR		= 142,	
	FSGIVEUPCALLBACKS	= 147,	
	FSGETVOLUMEINFO		= 148,	
	FSGETVOLUMESTATUS	= 149,	
	FSGETROOTVOLUME		= 151,	
	FSSETLOCK		= 156,	
	FSEXTENDLOCK		= 157,	
	FSRELEASELOCK		= 158,	
	FSLOOKUP		= 161,	
	FSFETCHDATA64		= 65537, 
	FSSTOREDATA64		= 65538, 
};

enum AFS_FS_Errors {
	VSALVAGE	= 101,	
	VNOVNODE	= 102,	
	VNOVOL		= 103,	
	VVOLEXISTS	= 104,	
	VNOSERVICE	= 105,	
	VOFFLINE	= 106,	
	VONLINE		= 107,	
	VDISKFULL	= 108,	
	VOVERQUOTA	= 109,	
	VBUSY		= 110,	
	VMOVED		= 111,	
};

#endif 
