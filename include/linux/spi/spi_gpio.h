#ifndef __LINUX_SPI_GPIO_H
#define __LINUX_SPI_GPIO_H


#define SPI_GPIO_NO_CHIPSELECT		((unsigned long)-1l)
#define SPI_GPIO_NO_MISO		((unsigned long)-1l)
#define SPI_GPIO_NO_MOSI		((unsigned long)-1l)

struct spi_gpio_platform_data {
	unsigned	sck;
	unsigned	mosi;
	unsigned	miso;

	u16		num_chipselect;
};

#endif 
