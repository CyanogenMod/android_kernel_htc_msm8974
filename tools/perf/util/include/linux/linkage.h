
#ifndef PERF_LINUX_LINKAGE_H_
#define PERF_LINUX_LINKAGE_H_


#define ENTRY(name)				\
	.globl name;				\
	name:

#define ENDPROC(name)

#endif	
