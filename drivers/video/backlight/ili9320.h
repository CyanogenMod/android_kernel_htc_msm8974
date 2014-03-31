/* drivers/video/backlight/ili9320.h
 *
 * ILI9320 LCD controller driver core.
 *
 * Copyright 2007 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct ili9320_reg {
	unsigned short		address;
	unsigned short		value;
};

struct ili9320;

struct ili9320_client {
	const char	*name;
	int	(*init)(struct ili9320 *ili, struct ili9320_platdata *cfg);

};
struct  ili9320_spi {
	struct spi_device	*dev;
	struct spi_message	message;
	struct spi_transfer	xfer[2];

	unsigned char		id;
	unsigned char		buffer_addr[4];
	unsigned char		buffer_data[4];
};

struct ili9320 {
	union {
		struct ili9320_spi	spi;	
	} access;				

	struct device			*dev;
	struct lcd_device		*lcd;	
	struct ili9320_client		*client;
	struct ili9320_platdata		*platdata;

	int				 power; 
	int				 initialised;

	unsigned short			 display1;
	unsigned short			 power1;

	int (*write)(struct ili9320 *ili, unsigned int reg, unsigned int val);
};



extern int ili9320_write(struct ili9320 *ili,
			 unsigned int reg, unsigned int value);

extern int ili9320_write_regs(struct ili9320 *ili,
			      struct ili9320_reg *values,
			      int nr_values);


extern int ili9320_probe_spi(struct spi_device *spi,
			     struct ili9320_client *cli);

extern int ili9320_remove(struct ili9320 *lcd);
extern void ili9320_shutdown(struct ili9320 *lcd);


extern int ili9320_suspend(struct ili9320 *lcd, pm_message_t state);
extern int ili9320_resume(struct ili9320 *lcd);
