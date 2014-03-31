#ifndef _ASM_CRIS_ARCH_USER_H
#define _ASM_CRIS_ARCH_USER_H


struct user_regs_struct {
	unsigned long r0;	
	unsigned long r1;
	unsigned long r2;
	unsigned long r3;
	unsigned long r4;
	unsigned long r5;
	unsigned long r6;
	unsigned long r7;
	unsigned long r8;
	unsigned long r9;
	unsigned long r10;
	unsigned long r11;
	unsigned long r12;
	unsigned long r13;
	unsigned long sp;	
	unsigned long acr;	
	unsigned long bz;	
	unsigned long vr;	
	unsigned long pid;	
	unsigned long srs;	
	unsigned long wz;	
	unsigned long exs;	
	unsigned long eda;	
	unsigned long mof;	
	unsigned long dz;	
	unsigned long ebp;	
	unsigned long erp;	
	unsigned long srp;	
	unsigned long nrp;	
	unsigned long ccs;	
	unsigned long usp;	
	unsigned long spc;	
};

#endif 
