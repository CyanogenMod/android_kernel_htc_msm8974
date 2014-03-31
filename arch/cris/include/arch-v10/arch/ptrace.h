#ifndef _CRIS_ARCH_PTRACE_H
#define _CRIS_ARCH_PTRACE_H


#define CRIS_FRAME_NORMAL   0 
#define CRIS_FRAME_BUSFAULT 1 


#define PT_FRAMETYPE 0
#define PT_ORIG_R10  1
#define PT_R13       2
#define PT_R12       3
#define PT_R11       4
#define PT_R10       5
#define PT_R9        6
#define PT_R8        7
#define PT_R7        8
#define PT_R6        9
#define PT_R5        10
#define PT_R4        11
#define PT_R3        12
#define PT_R2        13
#define PT_R1        14
#define PT_R0        15
#define PT_MOF       16
#define PT_DCCR      17
#define PT_SRP       18
#define PT_IRP       19    
#define PT_CSRINSTR  20    
#define PT_CSRADDR   21
#define PT_CSRDATA   22
#define PT_USP       23    
#define PT_MAX       23

#define C_DCCR_BITNR 0
#define V_DCCR_BITNR 1
#define Z_DCCR_BITNR 2
#define N_DCCR_BITNR 3
#define X_DCCR_BITNR 4
#define I_DCCR_BITNR 5
#define B_DCCR_BITNR 6
#define M_DCCR_BITNR 7
#define U_DCCR_BITNR 8
#define P_DCCR_BITNR 9
#define F_DCCR_BITNR 10


struct pt_regs {
	unsigned long frametype;  
	unsigned long orig_r10;
	
	unsigned long r13;
	unsigned long r12;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long r7;
	unsigned long r6;
	unsigned long r5;
	unsigned long r4;
	unsigned long r3;
	unsigned long r2;
	unsigned long r1;
	unsigned long r0;
	unsigned long mof;
	unsigned long dccr;
	unsigned long srp;
	unsigned long irp; 
	unsigned long csrinstr;
	unsigned long csraddr;
	unsigned long csrdata;
};


struct switch_stack {
	unsigned long r9;
	unsigned long r8;
	unsigned long r7;
	unsigned long r6;
	unsigned long r5;
	unsigned long r4;
	unsigned long r3;
	unsigned long r2;
	unsigned long r1;
	unsigned long r0;
	unsigned long return_ip; 
};

#ifdef __KERNEL__

#define user_mode(regs) (((regs)->dccr & 0x100) != 0)
#define instruction_pointer(regs) ((regs)->irp)
#define profile_pc(regs) instruction_pointer(regs)

#endif  

#endif
