/*
 * efs_vh.h
 *
 * Copyright (c) 1999 Al Smith
 *
 * Portions derived from IRIX header files (c) 1985 MIPS Computer Systems, Inc.
 */

#ifndef __EFS_VH_H__
#define __EFS_VH_H__

#define VHMAGIC		0xbe5a941	
#define NPARTAB		16		
#define NVDIR		15		
#define BFNAMESIZE	16		
#define VDNAMESIZE	8

struct volume_directory {
	char	vd_name[VDNAMESIZE];	
	__be32	vd_lbn;			
	__be32	vd_nbytes;		
};

struct partition_table {	
	__be32	pt_nblks;	
	__be32	pt_firstlbn;	
	__be32	pt_type;	
};

struct volume_header {
	__be32	vh_magic;			
	__be16	vh_rootpt;			
	__be16	vh_swappt;			
	char	vh_bootfile[BFNAMESIZE];	
	char	pad[48];			
	struct volume_directory vh_vd[NVDIR];	
	struct partition_table  vh_pt[NPARTAB];	
	__be32	vh_csum;			
	__be32	vh_fill;			
};

#define SGI_SYSV	0x05
#define SGI_EFS		0x07
#define IS_EFS(x)	(((x) == SGI_EFS) || ((x) == SGI_SYSV))

struct pt_types {
	int	pt_type;
	char	*pt_name;
};

#endif 

