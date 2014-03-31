#ifndef _INPUT_GPIO_TILT_H
#define _INPUT_GPIO_TILT_H

struct gpio_tilt_axis {
	int axis;
	int min;
	int max;
	int fuzz;
	int flat;
};

struct gpio_tilt_state {
	int gpios;
	int *axes;
};

struct gpio_tilt_platform_data {
	struct gpio *gpios;
	int nr_gpios;

	struct gpio_tilt_axis *axes;
	int nr_axes;

	struct gpio_tilt_state *states;
	int nr_states;

	int debounce_interval;

	unsigned int poll_interval;
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

#endif
