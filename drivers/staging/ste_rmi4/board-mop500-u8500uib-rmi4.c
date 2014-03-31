#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include "synaptics_i2c_rmi4.h"


static struct synaptics_rmi4_platform_data rmi4_i2c_dev_platformdata = {
	.irq_number     = NOMADIK_GPIO_TO_IRQ(84),
	.irq_type       = (IRQF_TRIGGER_FALLING | IRQF_SHARED),
	.x_flip		= false,
	.y_flip		= true,
};

struct i2c_board_info __initdata mop500_i2c3_devices_u8500[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x4B),
		.platform_data = &rmi4_i2c_dev_platformdata,
	},
};
