#ifndef LED_H
#define LED_H

#define	LED7		0x80		
#define	LED6		0x40
#define	LED5		0x20
#define	LED4		0x10
#define	LED3		0x08
#define	LED2		0x04
#define	LED1		0x02
#define	LED0		0x01		

#define	LED_LAN_TX	LED0		
#define	LED_LAN_RCV	LED1		
#define	LED_DISK_IO	LED2		
#define	LED_HEARTBEAT	LED3		

#define DISPLAY_MODEL_LCD  0		
#define DISPLAY_MODEL_NONE 1		
#define DISPLAY_MODEL_LASI 2		
#define DISPLAY_MODEL_OLD_ASP 0x7F	

#define LED_CMD_REG_NONE 0		

int __init register_led_driver(int model, unsigned long cmd_reg, unsigned long data_reg);

void __init register_led_regions(void);

#ifdef CONFIG_CHASSIS_LCD_LED
int lcd_print(const char *str);
#else
#define lcd_print(str)
#endif

 
int __init led_init(void);

#endif 
