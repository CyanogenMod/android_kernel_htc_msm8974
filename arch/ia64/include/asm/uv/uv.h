#ifndef _ASM_IA64_UV_UV_H
#define _ASM_IA64_UV_UV_H

#include <asm/sn/simulator.h>

static inline int is_uv_system(void)
{
	
	return IS_MEDUSA() || ia64_platform_is("uv");
}

#endif	
