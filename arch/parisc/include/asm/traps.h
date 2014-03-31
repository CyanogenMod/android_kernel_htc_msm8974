#ifndef __ASM_TRAPS_H
#define __ASM_TRAPS_H

#ifdef __KERNEL__
struct pt_regs;

void parisc_terminate(char *msg, struct pt_regs *regs,
		int code, unsigned long offset);

void do_page_fault(struct pt_regs *regs, unsigned long code,
		unsigned long address);
#endif

#endif
