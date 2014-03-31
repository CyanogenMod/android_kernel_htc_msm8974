#ifndef __ASM_MACH_EP93XX_SPI_H
#define __ASM_MACH_EP93XX_SPI_H

struct spi_device;

struct ep93xx_spi_info {
	int	num_chipselect;
	bool	use_dma;
};

struct ep93xx_spi_chip_ops {
	int	(*setup)(struct spi_device *spi);
	void	(*cleanup)(struct spi_device *spi);
	void	(*cs_control)(struct spi_device *spi, int value);
};

#endif 
