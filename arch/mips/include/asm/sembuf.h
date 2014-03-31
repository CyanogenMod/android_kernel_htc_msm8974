#ifndef _ASM_SEMBUF_H
#define _ASM_SEMBUF_H


struct semid64_ds {
	struct ipc64_perm sem_perm;		
	__kernel_time_t	sem_otime;		
	__kernel_time_t	sem_ctime;		
	unsigned long	sem_nsems;		
	unsigned long	__unused1;
	unsigned long	__unused2;
};

#endif 
