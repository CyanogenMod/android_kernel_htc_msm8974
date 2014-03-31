#ifndef _PARISC_SHMBUF_H
#define _PARISC_SHMBUF_H


struct shmid64_ds {
	struct ipc64_perm	shm_perm;	
#ifndef CONFIG_64BIT
	unsigned int		__pad1;
#endif
	__kernel_time_t		shm_atime;	
#ifndef CONFIG_64BIT
	unsigned int		__pad2;
#endif
	__kernel_time_t		shm_dtime;	
#ifndef CONFIG_64BIT
	unsigned int		__pad3;
#endif
	__kernel_time_t		shm_ctime;	
#ifndef CONFIG_64BIT
	unsigned int		__pad4;
#endif
	size_t			shm_segsz;	
	__kernel_pid_t		shm_cpid;	
	__kernel_pid_t		shm_lpid;	
	unsigned int		shm_nattch;	
	unsigned int		__unused1;
	unsigned int		__unused2;
};

#ifdef CONFIG_64BIT
#endif
struct shminfo64 {
	unsigned int	shmmax;
	unsigned int	shmmin;
	unsigned int	shmmni;
	unsigned int	shmseg;
	unsigned int	shmall;
	unsigned int	__unused1;
	unsigned int	__unused2;
	unsigned int	__unused3;
	unsigned int	__unused4;
};

#endif 
