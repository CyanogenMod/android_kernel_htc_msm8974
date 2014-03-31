
#ifndef _AU1550_SPI_H_
#define _AU1550_SPI_H_

struct au1550_spi_info {
	u32 mainclk_hz;		
	u16 num_chipselect;	
	void (*activate_cs)(struct au1550_spi_info *spi, int cs, int polarity);
	void (*deactivate_cs)(struct au1550_spi_info *spi, int cs, int polarity);
};

#endif
