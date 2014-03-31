#ifndef __PARISC_MMAN_H__
#define __PARISC_MMAN_H__

#define PROT_READ	0x1		
#define PROT_WRITE	0x2		/* page can be written */
#define PROT_EXEC	0x4		
#define PROT_SEM	0x8		
#define PROT_NONE	0x0		
#define PROT_GROWSDOWN	0x01000000	
#define PROT_GROWSUP	0x02000000	

#define MAP_SHARED	0x01		
#define MAP_PRIVATE	0x02		
#define MAP_TYPE	0x03		
#define MAP_FIXED	0x04		
#define MAP_ANONYMOUS	0x10		

#define MAP_DENYWRITE	0x0800		
#define MAP_EXECUTABLE	0x1000		
#define MAP_LOCKED	0x2000		
#define MAP_NORESERVE	0x4000		
#define MAP_GROWSDOWN	0x8000		
#define MAP_POPULATE	0x10000		
#define MAP_NONBLOCK	0x20000		
#define MAP_STACK	0x40000		
#define MAP_HUGETLB	0x80000		

#define MS_SYNC		1		
#define MS_ASYNC	2		
#define MS_INVALIDATE	4		

#define MCL_CURRENT	1		
#define MCL_FUTURE	2		

#define MADV_NORMAL     0               
#define MADV_RANDOM     1               
#define MADV_SEQUENTIAL 2               
#define MADV_WILLNEED   3               
#define MADV_DONTNEED   4               
#define MADV_SPACEAVAIL 5               
#define MADV_VPS_PURGE  6               
#define MADV_VPS_INHERIT 7              

#define MADV_REMOVE	9		
#define MADV_DONTFORK	10		
#define MADV_DOFORK	11		

#define MADV_4K_PAGES   12              
#define MADV_16K_PAGES  14              
#define MADV_64K_PAGES  16              
#define MADV_256K_PAGES 18              
#define MADV_1M_PAGES   20              
#define MADV_4M_PAGES   22              
#define MADV_16M_PAGES  24              
#define MADV_64M_PAGES  26              

#define MADV_MERGEABLE   65		
#define MADV_UNMERGEABLE 66		

#define MADV_HUGEPAGE	67		
#define MADV_NOHUGEPAGE	68		

#define MADV_DONTDUMP   69		
#define MADV_DODUMP	70		

#define MAP_FILE	0
#define MAP_VARIABLE	0

#endif 
