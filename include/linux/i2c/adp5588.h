/*
 * Analog Devices ADP5588 I/O Expander and QWERTY Keypad Controller
 *
 * Copyright 2009-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _ADP5588_H
#define _ADP5588_H

#define DEV_ID 0x00		
#define CFG 0x01		
#define INT_STAT 0x02		
#define KEY_LCK_EC_STAT 0x03	
#define Key_EVENTA 0x04		
#define Key_EVENTB 0x05		
#define Key_EVENTC 0x06		
#define Key_EVENTD 0x07		
#define Key_EVENTE 0x08		
#define Key_EVENTF 0x09		
#define Key_EVENTG 0x0A		
#define Key_EVENTH 0x0B		
#define Key_EVENTI 0x0C		
#define Key_EVENTJ 0x0D		
#define KP_LCK_TMR 0x0E		
#define UNLOCK1 0x0F		
#define UNLOCK2 0x10		
#define GPIO_INT_STAT1 0x11	
#define GPIO_INT_STAT2 0x12	
#define GPIO_INT_STAT3 0x13	
#define GPIO_DAT_STAT1 0x14	
#define GPIO_DAT_STAT2 0x15	
#define GPIO_DAT_STAT3 0x16	
#define GPIO_DAT_OUT1 0x17	
#define GPIO_DAT_OUT2 0x18	
#define GPIO_DAT_OUT3 0x19	
#define GPIO_INT_EN1 0x1A	
#define GPIO_INT_EN2 0x1B	
#define GPIO_INT_EN3 0x1C	
#define KP_GPIO1 0x1D		
#define KP_GPIO2 0x1E		
#define KP_GPIO3 0x1F		
#define GPI_EM1 0x20		
#define GPI_EM2 0x21		
#define GPI_EM3 0x22		
#define GPIO_DIR1 0x23		
#define GPIO_DIR2 0x24		
#define GPIO_DIR3 0x25		
#define GPIO_INT_LVL1 0x26	
#define GPIO_INT_LVL2 0x27	
#define GPIO_INT_LVL3 0x28	
#define Debounce_DIS1 0x29	
#define Debounce_DIS2 0x2A	
#define Debounce_DIS3 0x2B	
#define GPIO_PULL1 0x2C		
#define GPIO_PULL2 0x2D		
#define GPIO_PULL3 0x2E		
#define CMP_CFG_STAT 0x30	
#define CMP_CONFG_SENS1 0x31	
#define CMP_CONFG_SENS2 0x32	
#define CMP1_LVL2_TRIP 0x33	
#define CMP1_LVL2_HYS 0x34	
#define CMP1_LVL3_TRIP 0x35	
#define CMP1_LVL3_HYS 0x36	
#define CMP2_LVL2_TRIP 0x37	
#define CMP2_LVL2_HYS 0x38	
#define CMP2_LVL3_TRIP 0x39	
#define CMP2_LVL3_HYS 0x3A	
#define CMP1_ADC_DAT_R1 0x3B	
#define CMP1_ADC_DAT_R2 0x3C	
#define CMP2_ADC_DAT_R1 0x3D	
#define CMP2_ADC_DAT_R2 0x3E	

#define ADP5588_DEVICE_ID_MASK	0xF

 
#define ADP5588_AUTO_INC	(1 << 7)
#define ADP5588_GPIEM_CFG	(1 << 6)
#define ADP5588_OVR_FLOW_M	(1 << 5)
#define ADP5588_INT_CFG		(1 << 4)
#define ADP5588_OVR_FLOW_IEN	(1 << 3)
#define ADP5588_K_LCK_IM	(1 << 2)
#define ADP5588_GPI_IEN		(1 << 1)
#define ADP5588_KE_IEN		(1 << 0)

#define ADP5588_CMP2_INT	(1 << 5)
#define ADP5588_CMP1_INT	(1 << 4)
#define ADP5588_OVR_FLOW_INT	(1 << 3)
#define ADP5588_K_LCK_INT	(1 << 2)
#define ADP5588_GPI_INT		(1 << 1)
#define ADP5588_KE_INT		(1 << 0)

#define ADP5588_K_LCK_EN	(1 << 6)
#define ADP5588_LCK21		0x30
#define ADP5588_KEC		0xF

#define ADP5588_MAXGPIO		18
#define ADP5588_BANK(offs)	((offs) >> 3)
#define ADP5588_BIT(offs)	(1u << ((offs) & 0x7))


#define ADP5588_KEYMAPSIZE	80

#define GPI_PIN_ROW0 97
#define GPI_PIN_ROW1 98
#define GPI_PIN_ROW2 99
#define GPI_PIN_ROW3 100
#define GPI_PIN_ROW4 101
#define GPI_PIN_ROW5 102
#define GPI_PIN_ROW6 103
#define GPI_PIN_ROW7 104
#define GPI_PIN_COL0 105
#define GPI_PIN_COL1 106
#define GPI_PIN_COL2 107
#define GPI_PIN_COL3 108
#define GPI_PIN_COL4 109
#define GPI_PIN_COL5 110
#define GPI_PIN_COL6 111
#define GPI_PIN_COL7 112
#define GPI_PIN_COL8 113
#define GPI_PIN_COL9 114

#define GPI_PIN_ROW_BASE GPI_PIN_ROW0
#define GPI_PIN_ROW_END GPI_PIN_ROW7
#define GPI_PIN_COL_BASE GPI_PIN_COL0
#define GPI_PIN_COL_END GPI_PIN_COL9

#define GPI_PIN_BASE GPI_PIN_ROW_BASE
#define GPI_PIN_END GPI_PIN_COL_END

#define ADP5588_GPIMAPSIZE_MAX (GPI_PIN_END - GPI_PIN_BASE + 1)

struct adp5588_gpi_map {
	unsigned short pin;
	unsigned short sw_evt;
};

struct adp5588_kpad_platform_data {
	int rows;			
	int cols;			
	const unsigned short *keymap;	
	unsigned short keymapsize;	
	unsigned repeat:1;		
	unsigned en_keylock:1;		
	unsigned short unlock_key1;	
	unsigned short unlock_key2;	
	const struct adp5588_gpi_map *gpimap;
	unsigned short gpimapsize;
	const struct adp5588_gpio_platform_data *gpio_data;
};

struct i2c_client; 

struct adp5588_gpio_platform_data {
	int gpio_start;		
	unsigned irq_base;	
	unsigned pullup_dis_mask; 
	int	(*setup)(struct i2c_client *client,
				int gpio, unsigned ngpio,
				void *context);
	int	(*teardown)(struct i2c_client *client,
				int gpio, unsigned ngpio,
				void *context);
	void	*context;
};

#endif
