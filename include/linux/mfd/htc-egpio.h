
#ifndef __HTC_EGPIO_H__
#define __HTC_EGPIO_H__

#include <linux/gpio.h>

#define HTC_EGPIO_OUTPUT (~0)
#define HTC_EGPIO_INPUT  0

struct htc_egpio_chip {
	int           reg_start;
	int           gpio_base;
	int           num_gpios;
	unsigned long direction;
	unsigned long initial_values;
};

struct htc_egpio_platform_data {
	int                   bus_width;
	int                   reg_width;

	int                   irq_base;
	int                   num_irqs;
	int                   invert_acks;
	int                   ack_register;

	struct htc_egpio_chip *chip;
	int                   num_chips;
};

extern int htc_egpio_get_wakeup_irq(struct device *dev);

#endif
