#ifndef __LINUX_USB_IOWARRIOR_H
#define __LINUX_USB_IOWARRIOR_H

#define CODEMERCS_MAGIC_NUMBER	0xC0	

#define IOW_WRITE	_IOW(CODEMERCS_MAGIC_NUMBER, 1, __u8 *)
#define IOW_READ	_IOW(CODEMERCS_MAGIC_NUMBER, 2, __u8 *)

struct iowarrior_info {
	
	__u32 vendor;
	
	__u32 product;
	__u8 serial[9];
	
	__u32 revision;
	
	__u32 speed;
	
	__u32 power;
	
	__u32 if_num;
	
	__u32 report_size;
};

#define IOW_GETINFO _IOR(CODEMERCS_MAGIC_NUMBER, 3, struct iowarrior_info)

#endif 
