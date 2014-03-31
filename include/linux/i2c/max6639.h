#ifndef _LINUX_MAX6639_H
#define _LINUX_MAX6639_H

#include <linux/types.h>


struct max6639_platform_data {
	bool pwm_polarity;	
	int ppr;		
	int rpm_range;		
};

#endif 
