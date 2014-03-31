
#ifndef _DAVINCI_MMC_H
#define _DAVINCI_MMC_H

#include <linux/types.h>
#include <linux/mmc/host.h>

struct davinci_mmc_config {
	
	int	(*get_cd)(int module);
	int	(*get_ro)(int module);

	void	(*set_power)(int module, bool on);

	
	u8	wires;

	u32     max_freq;

	
	u32     caps;

	
	u8	version;

	
	u8	nr_sg;
};
void davinci_setup_mmc(int module, struct davinci_mmc_config *config);

enum {
	MMC_CTLR_VERSION_1 = 0,	
	MMC_CTLR_VERSION_2,	
};

#endif
