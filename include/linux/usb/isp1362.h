
#ifndef __LINUX_USB_ISP1362_H__
#define __LINUX_USB_ISP1362_H__

struct isp1362_platform_data {
	
	unsigned sel15Kres:1;
	
	unsigned clknotstop:1;
	
	unsigned oc_enable:1;
	
	unsigned int_act_high:1;
	
	unsigned int_edge_triggered:1;
	
	unsigned dreq_act_high:1;
	
	unsigned dack_act_high:1;
	
	unsigned remote_wakeup_connected:1;
	
	unsigned no_power_switching:1;
	
	unsigned power_switching_mode:1;
	
	u8 potpg;
	
	void (*reset) (struct device *dev, int set);
	
	void (*clock) (struct device *dev, int start);
	void (*delay) (struct device *dev, unsigned int delay);
};

#endif
