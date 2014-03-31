#ifndef _ASM_POWERPC_MSGBUF_H
#define _ASM_POWERPC_MSGBUF_H


struct msqid64_ds {
	struct ipc64_perm msg_perm;
#ifndef __powerpc64__
	unsigned int	__unused1;
#endif
	__kernel_time_t msg_stime;	
#ifndef __powerpc64__
	unsigned int	__unused2;
#endif
	__kernel_time_t msg_rtime;	
#ifndef __powerpc64__
	unsigned int	__unused3;
#endif
	__kernel_time_t msg_ctime;	
	unsigned long  msg_cbytes;	
	unsigned long  msg_qnum;	
	unsigned long  msg_qbytes;	
	__kernel_pid_t msg_lspid;	
	__kernel_pid_t msg_lrpid;	
	unsigned long  __unused4;
	unsigned long  __unused5;
};

#endif	
