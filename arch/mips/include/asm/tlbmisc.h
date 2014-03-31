#ifndef __ASM_TLBMISC_H
#define __ASM_TLBMISC_H

extern void add_wired_entry(unsigned long entrylo0, unsigned long entrylo1,
	unsigned long entryhi, unsigned long pagemask);

#endif 
