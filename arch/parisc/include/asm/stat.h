#ifndef _PARISC_STAT_H
#define _PARISC_STAT_H

#include <linux/types.h>

struct stat {
	unsigned int	st_dev;		
	ino_t		st_ino;		
	mode_t		st_mode;	
	nlink_t		st_nlink;	
	unsigned short	st_reserved1;	
	unsigned short	st_reserved2;	
	unsigned int	st_rdev;
	off_t		st_size;
	time_t		st_atime;
	unsigned int	st_atime_nsec;
	time_t		st_mtime;
	unsigned int	st_mtime_nsec;
	time_t		st_ctime;
	unsigned int	st_ctime_nsec;
	int		st_blksize;
	int		st_blocks;
	unsigned int	__unused1;	
	unsigned int	__unused2;	
	ino_t		__unused3;	
	unsigned int	__unused4;	
	unsigned short	__unused5;	
	short		st_fstype;
	unsigned int	st_realdev;
	unsigned short	st_basemode;
	unsigned short	st_spareshort;
	uid_t		st_uid;
	gid_t		st_gid;
	unsigned int	st_spare4[3];
};

#define STAT_HAVE_NSEC

typedef __kernel_off64_t	off64_t;

struct hpux_stat64 {
	unsigned int	st_dev;		
	ino_t           st_ino;         
	mode_t		st_mode;	
	nlink_t		st_nlink;	
	unsigned short	st_reserved1;	
	unsigned short	st_reserved2;	
	unsigned int	st_rdev;
	off64_t		st_size;
	time_t		st_atime;
	unsigned int	st_spare1;
	time_t		st_mtime;
	unsigned int	st_spare2;
	time_t		st_ctime;
	unsigned int	st_spare3;
	int		st_blksize;
	__u64		st_blocks;
	unsigned int	__unused1;	
	unsigned int	__unused2;	
	ino_t           __unused3;      
	unsigned int	__unused4;	
	unsigned short	__unused5;	
	short		st_fstype;
	unsigned int	st_realdev;
	unsigned short	st_basemode;
	unsigned short	st_spareshort;
	uid_t		st_uid;
	gid_t		st_gid;
	unsigned int	st_spare4[3];
};

struct stat64 {
	unsigned long long	st_dev;
	unsigned int		__pad1;

	unsigned int		__st_ino;	
	unsigned int		st_mode;
	unsigned int		st_nlink;
	unsigned int		st_uid;
	unsigned int		st_gid;
	unsigned long long	st_rdev;
	unsigned int		__pad2;
	signed long long	st_size;
	signed int		st_blksize;

	signed long long	st_blocks;
	signed int		st_atime;
	unsigned int		st_atime_nsec;
	signed int		st_mtime;
	unsigned int		st_mtime_nsec;
	signed int		st_ctime;
	unsigned int		st_ctime_nsec;
	unsigned long long	st_ino;
};

#endif
