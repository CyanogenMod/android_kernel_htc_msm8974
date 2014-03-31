#ifndef _LINUX_NVRAM_H
#define _LINUX_NVRAM_H

#include <linux/ioctl.h>

#define NVRAM_INIT	_IO('p', 0x40) 
#define NVRAM_SETCKS	_IO('p', 0x41) 

#define NVRAM_FIRST_BYTE    14
#define NVRAM_OFFSET(x)   ((x)-NVRAM_FIRST_BYTE)

#ifdef __KERNEL__
extern unsigned char __nvram_read_byte(int i);
extern unsigned char nvram_read_byte(int i);
extern void __nvram_write_byte(unsigned char c, int i);
extern void nvram_write_byte(unsigned char c, int i);
extern int __nvram_check_checksum(void);
extern int nvram_check_checksum(void);
#endif

#endif  
