#ifndef _LINUX_SPI_SPI_OC_TINY_H
#define _LINUX_SPI_SPI_OC_TINY_H

struct tiny_spi_platform_data {
	unsigned int freq;
	unsigned int baudwidth;
	unsigned int gpio_cs_count;
	int *gpio_cs;
};

#endif 
