#ifndef __LINUX_SPI_XILINX_SPI_H
#define __LINUX_SPI_XILINX_SPI_H

struct xspi_platform_data {
	u16 num_chipselect;
	bool little_endian;
	u8 bits_per_word;
	struct spi_board_info *devices;
	u8 num_devices;
};

#endif 
