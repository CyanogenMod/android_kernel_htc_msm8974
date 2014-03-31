#ifndef _ASM_M32R_SIGCONTEXT_H
#define _ASM_M32R_SIGCONTEXT_H

struct sigcontext {
	
	
	unsigned long sc_r4;
	unsigned long sc_r5;
	unsigned long sc_r6;
	struct pt_regs *sc_pt_regs;
	unsigned long sc_r0;
	unsigned long sc_r1;
	unsigned long sc_r2;
	unsigned long sc_r3;
	unsigned long sc_r7;
	unsigned long sc_r8;
	unsigned long sc_r9;
	unsigned long sc_r10;
	unsigned long sc_r11;
	unsigned long sc_r12;

	
	unsigned long sc_acc0h;
	unsigned long sc_acc0l;
	unsigned long sc_acc1h;	
	unsigned long sc_acc1l;	
	unsigned long sc_psw;
	unsigned long sc_bpc;		
	unsigned long sc_bbpsw;
	unsigned long sc_bbpc;
	unsigned long sc_spu;		
	unsigned long sc_fp;
	unsigned long sc_lr;		
	unsigned long sc_spi;		

	unsigned long	oldmask;
};

#endif  
