
#ifndef __LINUX_USB_SL811_H
#define __LINUX_USB_SL811_H

struct sl811_platform_data {
	unsigned	can_wakeup:1;

	
	u8		potpg;

	
	u8		power;

	
	void		(*port_power)(struct device *dev, int is_on);

	
	void		(*reset)(struct device *dev);

	
	
	
};

#endif 
