/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2005 Fen Systems Ltd.
 * Copyright 2006-2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#ifndef EFX_SPI_H
#define EFX_SPI_H

#include "net_driver.h"


#define SPI_WRSR 0x01		
#define SPI_WRITE 0x02		
#define SPI_READ 0x03		
#define SPI_WRDI 0x04		
#define SPI_RDSR 0x05		
#define SPI_WREN 0x06		
#define SPI_SST_EWSR 0x50	

#define SPI_STATUS_WPEN 0x80	
#define SPI_STATUS_BP2 0x10	
#define SPI_STATUS_BP1 0x08	
#define SPI_STATUS_BP0 0x04	
#define SPI_STATUS_WEN 0x02	
#define SPI_STATUS_NRDY 0x01	

struct efx_spi_device {
	int device_id;
	unsigned int size;
	unsigned int addr_len;
	unsigned int munge_address:1;
	u8 erase_command;
	unsigned int erase_size;
	unsigned int block_size;
};

static inline bool efx_spi_present(const struct efx_spi_device *spi)
{
	return spi->size != 0;
}

int falcon_spi_cmd(struct efx_nic *efx,
		   const struct efx_spi_device *spi, unsigned int command,
		   int address, const void *in, void *out, size_t len);
int falcon_spi_wait_write(struct efx_nic *efx,
			  const struct efx_spi_device *spi);
int falcon_spi_read(struct efx_nic *efx,
		    const struct efx_spi_device *spi, loff_t start,
		    size_t len, size_t *retlen, u8 *buffer);
int falcon_spi_write(struct efx_nic *efx,
		     const struct efx_spi_device *spi, loff_t start,
		     size_t len, size_t *retlen, const u8 *buffer);

#define FALCON_NVCONFIG_END 0x400U
#define FALCON_FLASH_BOOTCODE_START 0x8000U
#define EFX_EEPROM_BOOTCONFIG_START 0x800U
#define EFX_EEPROM_BOOTCONFIG_END 0x1800U

#endif 
