#ifndef _S390_SEMBUF_H
#define _S390_SEMBUF_H


struct semid64_ds {
	struct ipc64_perm sem_perm;		
	__kernel_time_t	sem_otime;		
#ifndef __s390x__
	unsigned long	__unused1;
#endif 
	__kernel_time_t	sem_ctime;		
#ifndef __s390x__
	unsigned long	__unused2;
#endif 
	unsigned long	sem_nsems;		
	unsigned long	__unused3;
	unsigned long	__unused4;
};

#endif 
