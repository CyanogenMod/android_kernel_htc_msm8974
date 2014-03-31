#ifndef _VGATYPES_
#define _VGATYPES_

#include <linux/ioctl.h>
#include <linux/fb.h>	
#include "../../video/sis/vgatypes.h"
#include "../../video/sis/sis.h"		

#ifndef XGI_VB_CHIP_TYPE
enum XGI_VB_CHIP_TYPE {
	VB_CHIP_Legacy = 0,
	VB_CHIP_301,
	VB_CHIP_301B,
	VB_CHIP_301LV,
	VB_CHIP_302,
	VB_CHIP_302B,
	VB_CHIP_302LV,
	VB_CHIP_301C,
	VB_CHIP_302ELV,
	VB_CHIP_UNKNOWN, 
	MAX_VB_CHIP
};
#endif


#define XGI_LCD_TYPE
#ifndef XGI_LCD_TYPE
enum XGI_LCD_TYPE {
	LCD_INVALID = 0,
	LCD_320x480,       
	LCD_640x480,
	LCD_640x480_2,     
	LCD_640x480_3,     
	LCD_800x600,
	LCD_848x480,
	LCD_1024x600,
	LCD_1024x768,
	LCD_1152x768,
	LCD_1152x864,
	LCD_1280x720,
	LCD_1280x768,
	LCD_1280x800,
	LCD_1280x960,
	LCD_1280x1024,
	LCD_1400x1050,
	LCD_1600x1200,
	LCD_1680x1050,
	LCD_1920x1440,
	LCD_2048x1536,
	LCD_CUSTOM,
	LCD_UNKNOWN
};
#endif

struct xgi_hw_device_info {
	unsigned long ulExternalChip; 
				      

	void __iomem *pjVideoMemoryAddress;
					    

	unsigned long ulVideoMemorySize; 

	unsigned char *pjIOAddress; 

	unsigned char jChipType; 
				 
				 

	unsigned char jChipRevision; 

	unsigned char ujVBChipID; 
				  
				  

	unsigned long ulCRT2LCDType; 
};

#endif

