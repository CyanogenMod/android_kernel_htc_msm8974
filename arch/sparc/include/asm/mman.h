#ifndef __SPARC_MMAN_H__
#define __SPARC_MMAN_H__

#include <asm-generic/mman-common.h>


#define MAP_RENAME      MAP_ANONYMOUS   
#define MAP_NORESERVE   0x40            
#define MAP_INHERIT     0x80            
#define MAP_LOCKED      0x100           
#define _MAP_NEW        0x80000000      

#define MAP_GROWSDOWN	0x0200		
#define MAP_DENYWRITE	0x0800		
#define MAP_EXECUTABLE	0x1000		

#define MCL_CURRENT     0x2000          
#define MCL_FUTURE      0x4000          

#define MAP_POPULATE	0x8000		
#define MAP_NONBLOCK	0x10000		
#define MAP_STACK	0x20000		
#define MAP_HUGETLB	0x40000		

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#define arch_mmap_check(addr,len,flags)	sparc_mmap_check(addr,len)
int sparc_mmap_check(unsigned long addr, unsigned long len);
#endif
#endif

#endif 
