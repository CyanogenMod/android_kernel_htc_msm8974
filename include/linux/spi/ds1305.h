#ifndef __LINUX_SPI_DS1305_H
#define __LINUX_SPI_DS1305_H

struct ds1305_platform_data {

#define DS1305_TRICKLE_MAGIC	0xa0
#define DS1305_TRICKLE_DS2	0x08	
#define DS1305_TRICKLE_DS1	0x04	
#define DS1305_TRICKLE_2K	0x01	
#define DS1305_TRICKLE_4K	0x02	
#define DS1305_TRICKLE_8K	0x03	
	u8	trickle;

	
	bool	is_ds1306;

	
	bool	en_1hz;

};

#endif 
