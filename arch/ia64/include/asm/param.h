#ifndef _ASM_IA64_PARAM_H
#define _ASM_IA64_PARAM_H


#define EXEC_PAGESIZE	65536

#ifndef NOGROUP
# define NOGROUP	(-1)
#endif

#define MAXHOSTNAMELEN	64	

#ifdef __KERNEL__
# define HZ		CONFIG_HZ
# define USER_HZ	HZ
# define CLOCKS_PER_SEC	HZ	
#else
# define HZ 1024
#endif

#endif 
