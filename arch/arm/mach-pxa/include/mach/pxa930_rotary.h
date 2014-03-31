#ifndef __ASM_ARCH_PXA930_ROTARY_H
#define __ASM_ARCH_PXA930_ROTARY_H

struct pxa930_rotary_platform_data {
	int	up_key;
	int	down_key;
	int	rel_code;
};

void __init pxa930_set_rotarykey_info(struct pxa930_rotary_platform_data *info);

#endif 
