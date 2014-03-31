#ifndef _SPARC_SEMBUF_H
#define _SPARC_SEMBUF_H

#if defined(__sparc__) && defined(__arch64__)
# define PADDING(x)
#else
# define PADDING(x) unsigned int x;
#endif

struct semid64_ds {
	struct ipc64_perm sem_perm;		
	PADDING(__pad1)
	__kernel_time_t	sem_otime;		
	PADDING(__pad2)
	__kernel_time_t	sem_ctime;		
	unsigned long	sem_nsems;		
	unsigned long	__unused1;
	unsigned long	__unused2;
};
#undef PADDING

#endif 
