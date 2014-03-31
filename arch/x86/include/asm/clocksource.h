
#ifndef _ASM_X86_CLOCKSOURCE_H
#define _ASM_X86_CLOCKSOURCE_H

#ifdef CONFIG_X86_64

#define VCLOCK_NONE 0  
#define VCLOCK_TSC  1  
#define VCLOCK_HPET 2  

struct arch_clocksource_data {
	int vclock_mode;
};

#endif 

#endif 
