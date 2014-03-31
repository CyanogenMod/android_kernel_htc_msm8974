#ifndef _ASM_POWERPC_SHMBUF_H
#define _ASM_POWERPC_SHMBUF_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


struct shmid64_ds {
	struct ipc64_perm	shm_perm;	
#ifndef __powerpc64__
	unsigned long		__unused1;
#endif
	__kernel_time_t		shm_atime;	
#ifndef __powerpc64__
	unsigned long		__unused2;
#endif
	__kernel_time_t		shm_dtime;	
#ifndef __powerpc64__
	unsigned long		__unused3;
#endif
	__kernel_time_t		shm_ctime;	
#ifndef __powerpc64__
	unsigned long		__unused4;
#endif
	size_t			shm_segsz;	
	__kernel_pid_t		shm_cpid;	
	__kernel_pid_t		shm_lpid;	
	unsigned long		shm_nattch;	
	unsigned long		__unused5;
	unsigned long		__unused6;
};

struct shminfo64 {
	unsigned long	shmmax;
	unsigned long	shmmin;
	unsigned long	shmmni;
	unsigned long	shmseg;
	unsigned long	shmall;
	unsigned long	__unused1;
	unsigned long	__unused2;
	unsigned long	__unused3;
	unsigned long	__unused4;
};

#endif	
