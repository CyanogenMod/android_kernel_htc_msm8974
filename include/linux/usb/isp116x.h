
#ifndef __LINUX_USB_ISP116X_H
#define __LINUX_USB_ISP116X_H

struct isp116x_platform_data {
	
	unsigned sel15Kres:1;
	
	unsigned oc_enable:1;
	
	unsigned int_act_high:1;
	
	unsigned int_edge_triggered:1;
	unsigned remote_wakeup_enable:1;
	void (*delay) (struct device *dev, int delay);
};

#endif 
