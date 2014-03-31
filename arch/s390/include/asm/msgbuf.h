#ifndef _S390_MSGBUF_H
#define _S390_MSGBUF_H


struct msqid64_ds {
	struct ipc64_perm msg_perm;
	__kernel_time_t msg_stime;	
#ifndef __s390x__
	unsigned long	__unused1;
#endif 
	__kernel_time_t msg_rtime;	
#ifndef __s390x__
	unsigned long	__unused2;
#endif 
	__kernel_time_t msg_ctime;	
#ifndef __s390x__
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
