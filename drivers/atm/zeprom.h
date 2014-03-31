
/* Written 1995,1996 by Werner Almesberger, EPFL LRC */


#ifndef DRIVER_ATM_ZEPROM_H
#define DRIVER_ATM_ZEPROM_H


#define ZEPROM_V1_REG	PCI_VENDOR_ID	
#define ZEPROM_V2_REG	0x40


#define ZEPROM_SK	0x80000000	
#define ZEPROM_CS	0x40000000	
#define ZEPROM_DI	0x20000000	
#define ZEPROM_DO	0x10000000	

#define ZEPROM_SIZE	32		
#define ZEPROM_V1_ESI_OFF 24		
#define ZEPROM_V2_ESI_OFF 4		

#define ZEPROM_CMD_LEN	3		
#define ZEPROM_ADDR_LEN	6		


#define ZEPROM_CMD_READ	6


#endif
