#ifndef _SPARC_MSGBUF_H
#define _SPARC_MSGBUF_H


#if defined(__sparc__) && defined(__arch64__)
# define PADDING(x)
#else
# define PADDING(x) unsigned int x;
#endif


struct msqid64_ds {
	struct ipc64_perm msg_perm;
	PADDING(__pad1)
	__kernel_time_t msg_stime;	
	PADDING(__pad2)
	__kernel_time_t msg_rtime;	
	PADDING(__pad3)
	__kernel_time_t msg_ctime;	
	unsigned long  msg_cbytes;	
	unsigned long  msg_qnum;	
	unsigned long  msg_qbytes;	
	__kernel_pid_t msg_lspid;	
	__kernel_pid_t msg_lrpid;	
	unsigned long  __unused1;
	unsigned long  __unused2;
};
#undef PADDING
#endif 
