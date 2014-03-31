#ifndef _LINUX_LGUEST_H
#define _LINUX_LGUEST_H

#ifndef __ASSEMBLY__
#include <linux/time.h>
#include <asm/irq.h>
#include <asm/lguest_hcall.h>

#define LG_CLOCK_MIN_DELTA	100UL
#define LG_CLOCK_MAX_DELTA	ULONG_MAX

struct lguest_data {
	unsigned int irq_enabled;
	
	DECLARE_BITMAP(blocked_interrupts, LGUEST_IRQS);

	unsigned long cr2;

	
	struct timespec time;

	int irq_pending;


	
	u8 hcall_status[LHCALL_RING_SIZE];
	
	struct hcall_args hcalls[LHCALL_RING_SIZE];

	
	unsigned long reserve_mem;
	
	u32 tsc_khz;

	
	unsigned long noirq_start, noirq_end;
	
	unsigned long kernel_address;
	
	unsigned int syscall_vec;
};
extern struct lguest_data lguest_data;
#endif 
#endif	
