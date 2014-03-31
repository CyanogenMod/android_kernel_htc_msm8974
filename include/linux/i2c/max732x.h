#ifndef __LINUX_I2C_MAX732X_H
#define __LINUX_I2C_MAX732X_H


struct max732x_platform_data {
	
	unsigned	gpio_base;

	
	int		irq_base;

	void		*context;	

	int		(*setup)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	int		(*teardown)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
};
#endif 
