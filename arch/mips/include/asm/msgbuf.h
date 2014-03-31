#ifndef _ASM_MSGBUF_H
#define _ASM_MSGBUF_H



struct msqid64_ds {
	struct ipc64_perm msg_perm;
#if defined(CONFIG_32BIT) && !defined(CONFIG_CPU_LITTLE_ENDIAN)
	unsigned long	__unused1;
#endif
	__kernel_time_t msg_stime;	
#if defined(CONFIG_32BIT) && defined(CONFIG_CPU_LITTLE_ENDIAN)
	unsigned long	__unused1;
#endif
#if defined(CONFIG_32BIT) && !defined(CONFIG_CPU_LITTLE_ENDIAN)
	unsigned long	__unused2;
#endif
	__kernel_time_t msg_rtime;	
#if defined(CONFIG_32BIT) && defined(CONFIG_CPU_LITTLE_ENDIAN)
	unsigned long	__unused2;
#endif
#if defined(CONFIG_32BIT) && !defined(CONFIG_CPU_LITTLE_ENDIAN)
	unsigned long	__unused3;
#endif
	__kernel_time_t msg_ctime;	
#if defined(CONFIG_32BIT) && defined(CONFIG_CPU_LITTLE_ENDIAN)
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
