
#ifndef _ASM_CRIS_TIMEX_H
#define _ASM_CRIS_TIMEX_H

#include <arch/timex.h>


typedef unsigned long long cycles_t;

static inline cycles_t get_cycles(void)
{
        return 0;
}

#endif
