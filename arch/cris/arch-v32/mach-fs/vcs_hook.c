
#include "vcs_hook.h"
#include <stdarg.h>
#include <arch-v32/hwregs/reg_map.h>
#include <arch-v32/hwregs/intr_vect_defs.h>

#define HOOK_TRIG_ADDR     0xb7000000	
#define HOOK_MEM_BASE_ADDR 0xa0000000	

#define HOOK_DATA(offset) ((unsigned *)HOOK_MEM_BASE_ADDR)[offset]
#define VHOOK_DATA(offset) ((volatile unsigned *)HOOK_MEM_BASE_ADDR)[offset]
#define HOOK_TRIG(funcid) \
	do { \
		*((unsigned *) HOOK_TRIG_ADDR) = funcid; \
	} while (0)
#define HOOK_DATA_BYTE(offset) ((unsigned char *)HOOK_MEM_BASE_ADDR)[offset]

int hook_call(unsigned id, unsigned pcnt, ...)
{
	va_list ap;
	unsigned i;
	unsigned ret;
#ifdef USING_SOS
	PREEMPT_OFF_SAVE();
#endif

	
	HOOK_DATA(0) = id;


	if (id == hook_print_str) {
		int i;
		char *str;

		HOOK_DATA(1) = pcnt;

		va_start(ap, pcnt);
		str = (char *)va_arg(ap, unsigned);

		for (i = 0; i != pcnt; i++)
			HOOK_DATA_BYTE(8 + i) = str[i];

		HOOK_DATA_BYTE(8 + i) = 0;	
	} else {
		va_start(ap, pcnt);
		for (i = 1; i <= pcnt; i++)
			HOOK_DATA(i) = va_arg(ap, unsigned);
		va_end(ap);
	}

	ret = *((volatile unsigned *)HOOK_MEM_BASE_ADDR);

	
	HOOK_TRIG(id);

	
	while (VHOOK_DATA(0) > 0) ;

	

	ret = VHOOK_DATA(1);

#ifdef USING_SOS
	PREEMPT_RESTORE();
#endif
	return ret;
}

unsigned hook_buf(unsigned i)
{
	return (HOOK_DATA(i));
}

void print_str(const char *str)
{
	int i;
	
	for (i = 1; str[i]; i++) ;
	hook_call(hook_print_str, i, str);
}

void CPU_KICK_DOG(void)
{
	(void)hook_call(hook_kick_dog, 0);
}

void CPU_WATCHDOG_TIMEOUT(unsigned t)
{
	(void)hook_call(hook_dog_timeout, 1, t);
}

