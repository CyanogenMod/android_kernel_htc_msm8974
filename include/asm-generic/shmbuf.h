#ifndef __ASM_GENERIC_SHMBUF_H
#define __ASM_GENERIC_SHMBUF_H

#include <asm/bitsperlong.h>


struct shmid64_ds {
	struct ipc64_perm	shm_perm;	
	size_t			shm_segsz;	
	__kernel_time_t		shm_atime;	
#if __BITS_PER_LONG != 64
	unsigned long		__unused1;
#endif
	__kernel_time_t		shm_dtime;	
#if __BITS_PER_LONG != 64
	unsigned long		__unused2;
#endif
	__kernel_time_t		shm_ctime;	
#if __BITS_PER_LONG != 64
	unsigned long		__unused3;
#endif
	__kernel_pid_t		shm_cpid;	
	__kernel_pid_t		shm_lpid;	
	unsigned long		shm_nattch;	
	unsigned long		__unused4;
	unsigned long		__unused5;
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
