#ifndef _PARISC_MSGBUF_H
#define _PARISC_MSGBUF_H


struct msqid64_ds {
	struct ipc64_perm msg_perm;
#ifndef CONFIG_64BIT
	unsigned int   __pad1;
#endif
	__kernel_time_t msg_stime;	
#ifndef CONFIG_64BIT
	unsigned int   __pad2;
#endif
	__kernel_time_t msg_rtime;	
#ifndef CONFIG_64BIT
	unsigned int   __pad3;
#endif
	__kernel_time_t msg_ctime;	
	unsigned int  msg_cbytes;	
	unsigned int  msg_qnum;	
	unsigned int  msg_qbytes;	
	__kernel_pid_t msg_lspid;	
	__kernel_pid_t msg_lrpid;	
	unsigned int  __unused1;
	unsigned int  __unused2;
};

#endif 
