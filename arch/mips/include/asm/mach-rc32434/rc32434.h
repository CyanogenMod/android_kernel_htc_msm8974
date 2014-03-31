
#ifndef _ASM_RC32434_RC32434_H_
#define _ASM_RC32434_RC32434_H_

#include <linux/delay.h>
#include <linux/io.h>

#define IDT_CLOCK_MULT		2

static inline void rc32434_sync(void)
{
	__asm__ volatile ("sync");
}

#endif  
