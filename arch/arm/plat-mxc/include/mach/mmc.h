#ifndef ASMARM_ARCH_MMC_H
#define ASMARM_ARCH_MMC_H

#include <linux/mmc/host.h>

struct device;

struct imxmmc_platform_data {
	int (*get_ro)(struct device *);

	int (*init)(struct device *dev, irq_handler_t handler, void *data);
	void (*exit)(struct device *dev, void *data);

	unsigned int ocr_avail;

	
	void (*setpower)(struct device *, unsigned int vdd);

	
	int dat3_card_detect;
};

#endif
