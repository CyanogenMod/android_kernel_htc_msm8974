/*
 * Definitions for SMTC /proc entries
 * Copyright(C) 2005 MIPS Technologies Inc.
 */
#ifndef __ASM_SMTC_PROC_H
#define __ASM_SMTC_PROC_H


struct smtc_cpu_proc {
	unsigned long timerints;
	unsigned long selfipis;
};

extern struct smtc_cpu_proc smtc_cpu_stats[NR_CPUS];


extern atomic_t smtc_fpu_recoveries;

#endif 
