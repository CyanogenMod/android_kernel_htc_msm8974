#ifndef __CAN_PLATFORM_MCP251X_H__
#define __CAN_PLATFORM_MCP251X_H__


#include <linux/spi/spi.h>


struct mcp251x_platform_data {
	unsigned long oscillator_frequency;
	unsigned long irq_flags;
	int (*board_specific_setup)(struct spi_device *spi);
	int (*transceiver_enable)(int enable);
	int (*power_enable) (int enable);
};

#endif 
