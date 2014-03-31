#ifndef _SPARC_SHMBUF_H
#define _SPARC_SHMBUF_H


#if defined(__sparc__) && defined(__arch64__)
# define PADDING(x)
#else
# define PADDING(x) unsigned int x;
#endif

struct shmid64_ds {
	struct ipc64_perm	shm_perm;	
	PADDING(__pad1)
	__kernel_time_t		shm_atime;	
	PADDING(__pad2)
	__kernel_time_t		shm_dtime;	
	PADDING(__pad3)
	__kernel_time_t		shm_ctime;	
	size_t			shm_segsz;	
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

#undef PADDING

#endif 
