#ifndef _ASM_POWERPC_SEMBUF_H
#define _ASM_POWERPC_SEMBUF_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


struct semid64_ds {
	struct ipc64_perm sem_perm;	
#ifndef __powerpc64__
	unsigned long	__unused1;
#endif
	__kernel_time_t	sem_otime;	
#ifndef __powerpc64__
	unsigned long	__unused2;
#endif
	__kernel_time_t	sem_ctime;	
	unsigned long	sem_nsems;	
	unsigned long	__unused3;
	unsigned long	__unused4;
};

#endif	
