#ifndef _PTRACE32_H
#define _PTRACE32_H

#include <asm/ptrace.h>    
#include "compat_linux.h"  

struct compat_per_struct_kernel {
	__u32 cr9;		
	__u32 cr10;		
	__u32 cr11;		
	__u32 bits;		
	__u32 starting_addr;	
	__u32 ending_addr;	
	__u16 perc_atmid;	
	__u32 address;		
	__u8  access_id;	
};

struct compat_user_regs_struct
{
	psw_compat_t psw;
	u32 gprs[NUM_GPRS];
	u32 acrs[NUM_ACRS];
	u32 orig_gpr2;
	
	s390_fp_regs fp_regs;
	struct compat_per_struct_kernel per_info;
	u32  ieee_instruction_pointer;	
};

struct compat_user {
	struct compat_user_regs_struct regs;
	
	u32 u_tsize;		
	u32 u_dsize;	        
	u32 u_ssize;	        
	u32 start_code;         
	u32 start_stack;	
	s32 signal;     	 
	u32 u_ar0;               
	                         
	u32 magic;		 
	char u_comm[32];	 
};

typedef struct
{
	__u32   len;
	__u32   kernel_addr;
	__u32   process_addr;
} compat_ptrace_area;

#endif 
