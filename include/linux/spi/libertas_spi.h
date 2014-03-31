/*
 * board-specific data for the libertas_spi driver.
 *
 * Copyright 2008 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#ifndef _LIBERTAS_SPI_H_
#define _LIBERTAS_SPI_H_

struct spi_device;

struct libertas_spi_platform_data {
	u16 use_dummy_writes;

	
	int (*setup)(struct spi_device *spi);
	int (*teardown)(struct spi_device *spi);
};
#endif
