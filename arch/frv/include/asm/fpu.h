#ifndef __ASM_FPU_H
#define __ASM_FPU_H



#define kernel_fpu_end() do { asm volatile("bar":::"memory"); preempt_enable(); } while(0)

#endif 
