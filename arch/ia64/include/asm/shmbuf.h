#ifndef _ASM_IA64_SHMBUF_H
#define _ASM_IA64_SHMBUF_H


struct shmid64_ds {
	struct ipc64_perm	shm_perm;	
	size_t			shm_segsz;	
	__kernel_time_t		shm_atime;	
	__kernel_time_t		shm_dtime;	
	__kernel_time_t		shm_ctime;	
	__kernel_pid_t		shm_cpid;	
	__kernel_pid_t		shm_lpid;	
	unsigned long		shm_nattch;	
	unsigned long		__unused1;
	unsigned long		__unused2;
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
