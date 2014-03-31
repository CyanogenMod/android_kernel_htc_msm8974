#ifndef __ASM_GENERIC_SEMBUF_H
#define __ASM_GENERIC_SEMBUF_H

#include <asm/bitsperlong.h>

struct semid64_ds {
	struct ipc64_perm sem_perm;	
	__kernel_time_t	sem_otime;	
#if __BITS_PER_LONG != 64
	unsigned long	__unused1;
#endif
	__kernel_time_t	sem_ctime;	
#if __BITS_PER_LONG != 64
	unsigned long	__unused2;
#endif
	unsigned long	sem_nsems;	
	unsigned long	__unused3;
	unsigned long	__unused4;
};

#endif 
