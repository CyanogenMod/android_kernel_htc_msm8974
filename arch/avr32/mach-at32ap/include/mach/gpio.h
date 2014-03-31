#ifndef __ASM_AVR32_ARCH_GPIO_H
#define __ASM_AVR32_ARCH_GPIO_H

#include <linux/compiler.h>
#include <asm/irq.h>


#define ARCH_NR_GPIOS	(NR_GPIO_IRQS + 2 * 32)


#include <asm-generic/gpio.h>

static inline int gpio_get_value(unsigned int gpio)
{
	return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned int gpio, int value)
{
	__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned int gpio)
{
	return __gpio_cansleep(gpio);
}


static inline int gpio_to_irq(unsigned int gpio)
{
	if (gpio < NR_GPIO_IRQS)
		return gpio + GPIO_IRQ_BASE;
	return -EINVAL;
}

static inline int irq_to_gpio(unsigned int irq)
{
	return irq - GPIO_IRQ_BASE;
}

#endif 
