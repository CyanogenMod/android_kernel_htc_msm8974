#ifndef __ASM_GENERIC_MSGBUF_H
#define __ASM_GENERIC_MSGBUF_H

#include <asm/bitsperlong.h>

struct msqid64_ds {
	struct ipc64_perm msg_perm;
	__kernel_time_t msg_stime;	
#if __BITS_PER_LONG != 64
	unsigned long	__unused1;
#endif
	__kernel_time_t msg_rtime;	
#if __BITS_PER_LONG != 64
	unsigned long	__unused2;
#endif
	__kernel_time_t msg_ctime;	
#if __BITS_PER_LONG != 64
	unsigned long	__unused3;
#endif
	unsigned long  msg_cbytes;	
	unsigned long  msg_qnum;	
	unsigned long  msg_qbytes;	
	__kernel_pid_t msg_lspid;	
	__kernel_pid_t msg_lrpid;	
	unsigned long  __unused4;
	unsigned long  __unused5;
};

#endif 
