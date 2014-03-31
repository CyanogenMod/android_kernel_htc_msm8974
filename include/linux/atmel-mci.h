#ifndef __LINUX_ATMEL_MCI_H
#define __LINUX_ATMEL_MCI_H

#define ATMCI_MAX_NR_SLOTS	2

struct mci_slot_pdata {
	unsigned int		bus_width;
	int			detect_pin;
	int			wp_pin;
	bool			detect_is_active_high;
};

struct mci_platform_data {
	struct mci_dma_data	*dma_slave;
	struct mci_slot_pdata	slot[ATMCI_MAX_NR_SLOTS];
};

#endif 
