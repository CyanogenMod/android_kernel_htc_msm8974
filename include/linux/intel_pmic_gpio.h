#ifndef LINUX_INTEL_PMIC_H
#define LINUX_INTEL_PMIC_H

struct intel_pmic_gpio_platform_data {
	
	unsigned	irq_base;
	
	unsigned	gpio_base;
	unsigned	gpiointr;
};

#endif
