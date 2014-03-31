#ifndef _ASM_IA64_MSGBUF_H
#define _ASM_IA64_MSGBUF_H


struct msqid64_ds {
	struct ipc64_perm msg_perm;
	__kernel_time_t msg_stime;	
	__kernel_time_t msg_rtime;	
	__kernel_time_t msg_ctime;	
	unsigned long  msg_cbytes;	
	unsigned long  msg_qnum;	
	unsigned long  msg_qbytes;	
	__kernel_pid_t msg_lspid;	
	__kernel_pid_t msg_lrpid;	
	unsigned long  __unused1;
	unsigned long  __unused2;
};

#endif 
