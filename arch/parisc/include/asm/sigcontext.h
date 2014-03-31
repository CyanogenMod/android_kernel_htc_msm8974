#ifndef _ASMPARISC_SIGCONTEXT_H
#define _ASMPARISC_SIGCONTEXT_H

#define PARISC_SC_FLAG_ONSTACK 1<<0
#define PARISC_SC_FLAG_IN_SYSCALL 1<<1

struct sigcontext {
	unsigned long sc_flags;

	unsigned long sc_gr[32]; 
	unsigned long long sc_fr[32]; 
	unsigned long sc_iasq[2];
	unsigned long sc_iaoq[2];
	unsigned long sc_sar; 
};


#endif
