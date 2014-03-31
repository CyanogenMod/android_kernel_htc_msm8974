struct sigscratch {
	unsigned long scratch_unat;	
	unsigned long ar_pfs;		
	struct pt_regs pt;
};

struct sigframe {
	unsigned long arg0;		
	unsigned long arg1;		
	unsigned long arg2;		

	void __user *handler;		
	struct siginfo info;
	struct sigcontext sc;
};

extern void ia64_do_signal (struct sigscratch *, long);
