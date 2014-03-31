
#include <linux/platform_device.h>

#ifndef _SOFTING_DEVICE_H_
#define _SOFTING_DEVICE_H_

#define fw_dir "softing-4.6/"

struct softing_platform_data {
	unsigned int manf;
	unsigned int prod;
	int generation;
	int nbus; 
	unsigned int freq; 
	unsigned int max_brp;
	unsigned int max_sjw;
	unsigned long dpram_size;
	const char *name;
	struct {
		unsigned long offs;
		unsigned long addr;
		const char *fw;
	} boot, load, app;
	int (*reset)(struct platform_device *pdev, int value);
	int (*enable_irq)(struct platform_device *pdev, int value);
};

#endif
