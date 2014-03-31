/*
 * gpio-au1300.h -- GPIO control for Au1300 GPIC and compatibles.
 *
 * Copyright (c) 2009-2011 Manuel Lauss <manuel.lauss@googlemail.com>
 */

#ifndef _GPIO_AU1300_H_
#define _GPIO_AU1300_H_

#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/mach-au1x00/au1000.h>

struct gpio;
struct gpio_chip;

#define AU1300_GPIO_BASE	0
#define AU1300_GPIO_NUM		75
#define AU1300_GPIO_MAX		(AU1300_GPIO_BASE + AU1300_GPIO_NUM - 1)

#define AU1300_GPIC_ADDR	\
	(void __iomem *)KSEG1ADDR(AU1300_GPIC_PHYS_ADDR)

static inline int au1300_gpio_get_value(unsigned int gpio)
{
	void __iomem *roff = AU1300_GPIC_ADDR;
	int bit;

	gpio -= AU1300_GPIO_BASE;
	roff += GPIC_GPIO_BANKOFF(gpio);
	bit = GPIC_GPIO_TO_BIT(gpio);
	return __raw_readl(roff + AU1300_GPIC_PINVAL) & bit;
}

static inline int au1300_gpio_direction_input(unsigned int gpio)
{
	void __iomem *roff = AU1300_GPIC_ADDR;
	unsigned long bit;

	gpio -= AU1300_GPIO_BASE;

	roff += GPIC_GPIO_BANKOFF(gpio);
	bit = GPIC_GPIO_TO_BIT(gpio);
	__raw_writel(bit, roff + AU1300_GPIC_DEVCLR);
	wmb();

	return 0;
}

static inline int au1300_gpio_set_value(unsigned int gpio, int v)
{
	void __iomem *roff = AU1300_GPIC_ADDR;
	unsigned long bit;

	gpio -= AU1300_GPIO_BASE;

	roff += GPIC_GPIO_BANKOFF(gpio);
	bit = GPIC_GPIO_TO_BIT(gpio);
	__raw_writel(bit, roff + (v ? AU1300_GPIC_PINVAL
				    : AU1300_GPIC_PINVALCLR));
	wmb();

	return 0;
}

static inline int au1300_gpio_direction_output(unsigned int gpio, int v)
{
	
	return au1300_gpio_set_value(gpio, v);
}

static inline int au1300_gpio_to_irq(unsigned int gpio)
{
	return AU1300_FIRST_INT + (gpio - AU1300_GPIO_BASE);
}

static inline int au1300_irq_to_gpio(unsigned int irq)
{
	return (irq - AU1300_FIRST_INT) + AU1300_GPIO_BASE;
}

static inline int au1300_gpio_is_valid(unsigned int gpio)
{
	int ret;

	switch (alchemy_get_cputype()) {
	case ALCHEMY_CPU_AU1300:
		ret = ((gpio >= AU1300_GPIO_BASE) && (gpio <= AU1300_GPIO_MAX));
		break;
	default:
		ret = 0;
	}
	return ret;
}

static inline int au1300_gpio_cansleep(unsigned int gpio)
{
	return 0;
}

static inline int au1300_gpio_getinitlvl(unsigned int gpio)
{
	void __iomem *roff = AU1300_GPIC_ADDR;
	unsigned long v;

	if (unlikely(gpio > 63))
		return 0;
	else if (gpio > 31) {
		gpio -= 32;
		roff += 4;
	}

	v = __raw_readl(roff + AU1300_GPIC_RSTVAL);
	return (v >> gpio) & 1;
}



#ifndef CONFIG_GPIOLIB

#ifdef CONFIG_ALCHEMY_GPIOINT_AU1300

#ifndef CONFIG_ALCHEMY_GPIO_INDIRECT	

static inline int gpio_direction_input(unsigned int gpio)
{
	return au1300_gpio_direction_input(gpio);
}

static inline int gpio_direction_output(unsigned int gpio, int v)
{
	return au1300_gpio_direction_output(gpio, v);
}

static inline int gpio_get_value(unsigned int gpio)
{
	return au1300_gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned int gpio, int v)
{
	au1300_gpio_set_value(gpio, v);
}

static inline int gpio_get_value_cansleep(unsigned gpio)
{
	return gpio_get_value(gpio);
}

static inline void gpio_set_value_cansleep(unsigned gpio, int value)
{
	gpio_set_value(gpio, value);
}

static inline int gpio_is_valid(unsigned int gpio)
{
	return au1300_gpio_is_valid(gpio);
}

static inline int gpio_cansleep(unsigned int gpio)
{
	return au1300_gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned int gpio)
{
	return au1300_gpio_to_irq(gpio);
}

static inline int irq_to_gpio(unsigned int irq)
{
	return au1300_irq_to_gpio(irq);
}

static inline int gpio_request(unsigned int gpio, const char *label)
{
	return 0;
}

static inline int gpio_request_one(unsigned gpio,
					unsigned long flags, const char *label)
{
	return 0;
}

static inline int gpio_request_array(struct gpio *array, size_t num)
{
	return 0;
}

static inline void gpio_free(unsigned gpio)
{
}

static inline void gpio_free_array(struct gpio *array, size_t num)
{
}

static inline int gpio_set_debounce(unsigned gpio, unsigned debounce)
{
	return -ENOSYS;
}

static inline void gpio_unexport(unsigned gpio)
{
}

static inline int gpio_export(unsigned gpio, bool direction_may_change)
{
	return -ENOSYS;
}

static inline int gpio_sysfs_set_active_low(unsigned gpio, int value)
{
	return -ENOSYS;
}

static inline int gpio_export_link(struct device *dev, const char *name,
				   unsigned gpio)
{
	return -ENOSYS;
}

#endif	

#endif	

#endif	

#endif 
