
struct eeprom_93xx46_platform_data {
	unsigned char	flags;
#define EE_ADDR8	0x01		
#define EE_ADDR16	0x02		
#define EE_READONLY	0x08		

	void (*prepare)(void *);
	void (*finish)(void *);
};
