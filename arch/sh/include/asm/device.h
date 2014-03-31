/*
 * Arch specific extensions to struct device
 *
 * This file is released under the GPLv2
 */
#ifndef __ASM_SH_DEVICE_H
#define __ASM_SH_DEVICE_H

#include <asm-generic/device.h>

struct platform_device;
int platform_resource_setup_memory(struct platform_device *pdev,
				   char *name, unsigned long memsize);

void plat_early_device_setup(void);

#endif 
