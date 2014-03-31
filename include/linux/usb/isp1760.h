
#ifndef __LINUX_USB_ISP1760_H
#define __LINUX_USB_ISP1760_H

struct isp1760_platform_data {
	unsigned is_isp1761:1;			
	unsigned bus_width_16:1;		
	unsigned port1_otg:1;			
	unsigned analog_oc:1;			
	unsigned dack_polarity_high:1;		
	unsigned dreq_polarity_high:1;		
};

#endif 
