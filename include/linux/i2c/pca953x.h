#ifndef _LINUX_PCA953X_H
#define _LINUX_PCA953X_H

#include <linux/types.h>
#include <linux/i2c.h>


struct pca953x_platform_data {
	
	unsigned	gpio_base;

	
	uint16_t	invert;

	
	int		irq_base;

	void		*context;	

	int		(*setup)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	int		(*teardown)(struct i2c_client *client,
				unsigned gpio, unsigned ngpio,
				void *context);
	const char	*const *names;
};

#endif 
