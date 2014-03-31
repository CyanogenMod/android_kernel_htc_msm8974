#include <linux/types.h>


struct pl061_platform_data {
	
	unsigned	gpio_base;

	unsigned	irq_base;

	u8		directions;	
	u8		values;		
};
