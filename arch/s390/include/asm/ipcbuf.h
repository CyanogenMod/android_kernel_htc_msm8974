#ifndef __S390_IPCBUF_H__
#define __S390_IPCBUF_H__


struct ipc64_perm
{
	__kernel_key_t		key;
	__kernel_uid32_t	uid;
	__kernel_gid32_t	gid;
	__kernel_uid32_t	cuid;
	__kernel_gid32_t	cgid;
	__kernel_mode_t		mode;
	unsigned short		__pad1;
	unsigned short		seq;
#ifndef __s390x__
	unsigned short		__pad2;
#endif 
	unsigned long		__unused1;
	unsigned long		__unused2;
};

#endif 
