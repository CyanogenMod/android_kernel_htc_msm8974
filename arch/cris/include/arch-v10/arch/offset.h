#ifndef __ASM_OFFSETS_H__
#define __ASM_OFFSETS_H__

#define PT_orig_r10 4 
#define PT_r13 8 
#define PT_r12 12 
#define PT_r11 16 
#define PT_r10 20 
#define PT_r9 24 
#define PT_mof 64 
#define PT_dccr 68 
#define PT_srp 72 

#define TI_task 0 
#define TI_flags 8 
#define TI_preempt_count 16 

#define THREAD_ksp 0 
#define THREAD_usp 4 
#define THREAD_dccr 8 

#define TASK_pid 141 

#define LCLONE_VM 256 
#define LCLONE_UNTRACED 8388608 

#endif
