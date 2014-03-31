#ifndef __ASM_SH_BUGS_H
#define __ASM_SH_BUGS_H



#include <asm/processor.h>

extern void select_idle_routine(void);

static void __init check_bugs(void)
{
	extern unsigned long loops_per_jiffy;
	char *p = &init_utsname()->machine[2]; 

	select_idle_routine();

	current_cpu_data.loops_per_jiffy = loops_per_jiffy;

	switch (current_cpu_data.family) {
	case CPU_FAMILY_SH2:
		*p++ = '2';
		break;
	case CPU_FAMILY_SH2A:
		*p++ = '2';
		*p++ = 'a';
		break;
	case CPU_FAMILY_SH3:
		*p++ = '3';
		break;
	case CPU_FAMILY_SH4:
		*p++ = '4';
		break;
	case CPU_FAMILY_SH4A:
		*p++ = '4';
		*p++ = 'a';
		break;
	case CPU_FAMILY_SH4AL_DSP:
		*p++ = '4';
		*p++ = 'a';
		*p++ = 'l';
		*p++ = '-';
		*p++ = 'd';
		*p++ = 's';
		*p++ = 'p';
		break;
	case CPU_FAMILY_SH5:
		*p++ = '6';
		*p++ = '4';
		break;
	case CPU_FAMILY_UNKNOWN:
		break;
	}

	printk("CPU: %s\n", get_cpu_subtype(&current_cpu_data));

#ifndef __LITTLE_ENDIAN__
	
	*p++ = 'e';
	*p++ = 'b';
#endif
	*p = '\0';
}
#endif 
