#ifndef __PARISC_IPCBUF_H__
#define __PARISC_IPCBUF_H__


struct ipc64_perm
{
	key_t           key;
	uid_t           uid;
	gid_t           gid;
	uid_t           cuid;
	gid_t           cgid;
	unsigned short int	__pad1;
	mode_t          mode;
	unsigned short int	__pad2;
	unsigned short int	seq;
	unsigned int	__pad3;
	unsigned long long int __unused1;
	unsigned long long int __unused2;
};

#endif 
