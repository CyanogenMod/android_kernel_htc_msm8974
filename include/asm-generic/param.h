#ifndef __ASM_GENERIC_PARAM_H
#define __ASM_GENERIC_PARAM_H

#ifndef HZ
#define HZ 100
#endif

#ifndef EXEC_PAGESIZE
#define EXEC_PAGESIZE	4096
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	

#ifdef __KERNEL__
# undef HZ
# define HZ		CONFIG_HZ	
# define USER_HZ	100		
# define CLOCKS_PER_SEC	(USER_HZ)       
#endif

#endif 
