
#ifndef __MACH_SPI_H_
#define __MACH_SPI_H_

struct spi_imx_master {
	int	*chipselect;
	int	num_chipselect;
};

#define MXC_SPI_CS(no)	((no) - 32)

#endif 
