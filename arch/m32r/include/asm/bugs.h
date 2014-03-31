#ifndef _ASM_M32R_BUGS_H
#define _ASM_M32R_BUGS_H

#include <asm/processor.h>

static void __init check_bugs(void)
{
	extern unsigned long loops_per_jiffy;

	current_cpu_data.loops_per_jiffy = loops_per_jiffy;
}

#endif  
