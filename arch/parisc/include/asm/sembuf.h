#ifndef _PARISC_SEMBUF_H
#define _PARISC_SEMBUF_H


struct semid64_ds {
	struct ipc64_perm sem_perm;		
#ifndef CONFIG_64BIT
	unsigned int	__pad1;
#endif
	__kernel_time_t	sem_otime;		
#ifndef CONFIG_64BIT
	unsigned int	__pad2;
#endif
	__kernel_time_t	sem_ctime;		
	unsigned int	sem_nsems;		
	unsigned int	__unused1;
	unsigned int	__unused2;
};

#endif 
