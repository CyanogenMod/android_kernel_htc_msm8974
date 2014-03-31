#ifndef __ASM_SH_AUXVEC_H
#define __ASM_SH_AUXVEC_H


#define AT_FPUCW		18	

#if defined(CONFIG_VSYSCALL) || !defined(__KERNEL__)
#define AT_SYSINFO_EHDR		33
#endif

#define AT_L1I_CACHESHAPE	34
#define AT_L1D_CACHESHAPE	35
#define AT_L2_CACHESHAPE	36

#define AT_VECTOR_SIZE_ARCH 5 

#endif 
