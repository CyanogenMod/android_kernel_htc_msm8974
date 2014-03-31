#ifndef _ASM_SCORE_USER_H
#define _ASM_SCORE_USER_H

struct user_regs_struct {
	unsigned long regs[32];

	unsigned long cel;
	unsigned long ceh;

	unsigned long sr0;	
	unsigned long sr1;	
	unsigned long sr2;	

	unsigned long cp0_epc;
	unsigned long cp0_ema;
	unsigned long cp0_psr;
	unsigned long cp0_ecr;
	unsigned long cp0_condition;
};

#endif 
