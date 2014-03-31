#ifndef ASMARM_ARCH_MMC_H
#define ASMARM_ARCH_MMC_H

#include <linux/mmc/host.h>
#include <linux/interrupt.h>

struct device;
struct mmc_host;

struct pxamci_platform_data {
	unsigned int ocr_mask;			
	unsigned long detect_delay_ms;		
	int (*init)(struct device *, irq_handler_t , void *);
	int (*get_ro)(struct device *);
	void (*setpower)(struct device *, unsigned int);
	void (*exit)(struct device *, void *);
	int gpio_card_detect;			
	int gpio_card_ro;			
	bool gpio_card_ro_invert;		
	int gpio_power;				
	bool gpio_power_invert;			
};

extern void pxa_set_mci_info(struct pxamci_platform_data *info);
extern void pxa3xx_set_mci2_info(struct pxamci_platform_data *info);
extern void pxa3xx_set_mci3_info(struct pxamci_platform_data *info);

#endif
