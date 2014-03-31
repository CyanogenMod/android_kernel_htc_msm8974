#ifndef __ASM_OFFSETS_H__
#define __ASM_OFFSETS_H__

#define PT_orig_r10 0 
#define PT_r13 56 
#define PT_r12 52 
#define PT_r11 48 
#define PT_r10 44 
#define PT_r9 40 
#define PT_acr 60 
#define PT_srs 64 
#define PT_mof 68 
#define PT_ccs 76 
#define PT_srp 80 

#define TI_task 0 
#define TI_flags 8 
#define TI_preempt_count 16 

#define THREAD_ksp 0 
#define THREAD_usp 4 
#define THREAD_ccs 8 

#define TASK_pid 151 

#define LCLONE_VM 256 
#define LCLONE_UNTRACED 8388608 

#endif
