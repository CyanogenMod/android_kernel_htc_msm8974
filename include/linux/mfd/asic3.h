/*
 * include/linux/mfd/asic3.h
 *
 * Compaq ASIC3 headers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright 2001 Compaq Computer Corporation.
 * Copyright 2007-2008 OpenedHand Ltd.
 */

#ifndef __ASIC3_H__
#define __ASIC3_H__

#include <linux/types.h>

struct led_classdev;
struct asic3_led {
	const char	*name;
	const char	*default_trigger;
	struct led_classdev *cdev;
};

struct asic3_platform_data {
	u16 *gpio_config;
	unsigned int gpio_config_num;

	unsigned int irq_base;

	unsigned int gpio_base;

	struct asic3_led *leds;
};

#define ASIC3_NUM_GPIO_BANKS	4
#define ASIC3_GPIOS_PER_BANK	16
#define ASIC3_NUM_GPIOS		64
#define ASIC3_NR_IRQS		ASIC3_NUM_GPIOS + 6

#define ASIC3_IRQ_LED0		64
#define ASIC3_IRQ_LED1		65
#define ASIC3_IRQ_LED2		66
#define ASIC3_IRQ_SPI		67
#define ASIC3_IRQ_SMBUS		68
#define ASIC3_IRQ_OWM		69

#define ASIC3_TO_GPIO(gpio) (NR_BUILTIN_GPIO + (gpio))

#define ASIC3_GPIO_BANK_A	0
#define ASIC3_GPIO_BANK_B	1
#define ASIC3_GPIO_BANK_C	2
#define ASIC3_GPIO_BANK_D	3

#define ASIC3_GPIO(bank, gpio) \
	((ASIC3_GPIOS_PER_BANK * ASIC3_GPIO_BANK_##bank) + (gpio))
#define ASIC3_GPIO_bit(gpio) (1 << (gpio & 0xf))
#define ASIC3_DEFAULT_ADDR_SHIFT 2

#define ASIC3_OFFSET(base, reg) (ASIC3_##base##_BASE + ASIC3_##base##_##reg)
#define ASIC3_GPIO_OFFSET(base, reg) \
	(ASIC3_GPIO_##base##_BASE + ASIC3_GPIO_##reg)

#define ASIC3_GPIO_A_BASE      0x0000
#define ASIC3_GPIO_B_BASE      0x0100
#define ASIC3_GPIO_C_BASE      0x0200
#define ASIC3_GPIO_D_BASE      0x0300

#define ASIC3_GPIO_TO_BANK(gpio) ((gpio) >> 4)
#define ASIC3_GPIO_TO_BIT(gpio)  ((gpio) - \
				  (ASIC3_GPIOS_PER_BANK * ((gpio) >> 4)))
#define ASIC3_GPIO_TO_MASK(gpio) (1 << ASIC3_GPIO_TO_BIT(gpio))
#define ASIC3_GPIO_TO_BASE(gpio) (ASIC3_GPIO_A_BASE + (((gpio) >> 4) * 0x0100))
#define ASIC3_BANK_TO_BASE(bank) (ASIC3_GPIO_A_BASE + ((bank) * 0x100))

#define ASIC3_GPIO_MASK          0x00    
#define ASIC3_GPIO_DIRECTION     0x04    
#define ASIC3_GPIO_OUT           0x08    
#define ASIC3_GPIO_TRIGGER_TYPE  0x0c    
#define ASIC3_GPIO_EDGE_TRIGGER  0x10    
#define ASIC3_GPIO_LEVEL_TRIGGER 0x14    
#define ASIC3_GPIO_SLEEP_MASK    0x18    
#define ASIC3_GPIO_SLEEP_OUT     0x1c    
#define ASIC3_GPIO_BAT_FAULT_OUT 0x20    
#define ASIC3_GPIO_INT_STATUS    0x24    
#define ASIC3_GPIO_ALT_FUNCTION  0x28	 
#define ASIC3_GPIO_SLEEP_CONF    0x2c    
#define ASIC3_GPIO_STATUS        0x30    

#define ASIC3_CONFIG_GPIO_PIN(config) ((config) & 0x7f)
#define ASIC3_CONFIG_GPIO_ALT(config)  (((config) & (0x7f << 7)) >> 7)
#define ASIC3_CONFIG_GPIO_DIR(config)  ((config & (1 << 14)) >> 14)
#define ASIC3_CONFIG_GPIO_INIT(config) ((config & (1 << 15)) >> 15)
#define ASIC3_CONFIG_GPIO(gpio, alt, dir, init) (((gpio) & 0x7f) \
	| (((alt) & 0x7f) << 7) | (((dir) & 0x1) << 14) \
	| (((init) & 0x1) << 15))
#define ASIC3_CONFIG_GPIO_DEFAULT(gpio, dir, init) \
	ASIC3_CONFIG_GPIO((gpio), 0, (dir), (init))
#define ASIC3_CONFIG_GPIO_DEFAULT_OUT(gpio, init) \
	ASIC3_CONFIG_GPIO((gpio), 0, 1, (init))

#define ASIC3_GPIOA11_PWM0		ASIC3_CONFIG_GPIO(11, 1, 1, 0)
#define ASIC3_GPIOA12_PWM1		ASIC3_CONFIG_GPIO(12, 1, 1, 0)
#define ASIC3_GPIOA15_CONTROL_CX	ASIC3_CONFIG_GPIO(15, 1, 1, 0)
#define ASIC3_GPIOC0_LED0		ASIC3_CONFIG_GPIO(32, 1, 0, 0)
#define ASIC3_GPIOC1_LED1		ASIC3_CONFIG_GPIO(33, 1, 0, 0)
#define ASIC3_GPIOC2_LED2		ASIC3_CONFIG_GPIO(34, 1, 0, 0)
#define ASIC3_GPIOC3_SPI_RXD		ASIC3_CONFIG_GPIO(35, 1, 0, 0)
#define ASIC3_GPIOC4_CF_nCD		ASIC3_CONFIG_GPIO(36, 1, 0, 0)
#define ASIC3_GPIOC4_SPI_TXD		ASIC3_CONFIG_GPIO(36, 1, 1, 0)
#define ASIC3_GPIOC5_SPI_CLK		ASIC3_CONFIG_GPIO(37, 1, 1, 0)
#define ASIC3_GPIOC5_nCIOW		ASIC3_CONFIG_GPIO(37, 1, 1, 0)
#define ASIC3_GPIOC6_nCIOR		ASIC3_CONFIG_GPIO(38, 1, 1, 0)
#define ASIC3_GPIOC7_nPCE_1		ASIC3_CONFIG_GPIO(39, 1, 0, 0)
#define ASIC3_GPIOC8_nPCE_2		ASIC3_CONFIG_GPIO(40, 1, 0, 0)
#define ASIC3_GPIOC9_nPOE		ASIC3_CONFIG_GPIO(41, 1, 0, 0)
#define ASIC3_GPIOC10_nPWE		ASIC3_CONFIG_GPIO(42, 1, 0, 0)
#define ASIC3_GPIOC11_PSKTSEL		ASIC3_CONFIG_GPIO(43, 1, 0, 0)
#define ASIC3_GPIOC12_nPREG		ASIC3_CONFIG_GPIO(44, 1, 0, 0)
#define ASIC3_GPIOC13_nPWAIT		ASIC3_CONFIG_GPIO(45, 1, 1, 0)
#define ASIC3_GPIOC14_nPIOIS16		ASIC3_CONFIG_GPIO(46, 1, 1, 0)
#define ASIC3_GPIOC15_nPIOR		ASIC3_CONFIG_GPIO(47, 1, 0, 0)
#define ASIC3_GPIOD11_nCIOIS16		ASIC3_CONFIG_GPIO(59, 1, 0, 0)
#define ASIC3_GPIOD12_nCWAIT		ASIC3_CONFIG_GPIO(60, 1, 0, 0)
#define ASIC3_GPIOD15_nPIOW		ASIC3_CONFIG_GPIO(63, 1, 0, 0)


#define ASIC3_SPI_Base		      0x0400
#define ASIC3_SPI_Control               0x0000
#define ASIC3_SPI_TxData                0x0004
#define ASIC3_SPI_RxData                0x0008
#define ASIC3_SPI_Int                   0x000c
#define ASIC3_SPI_Status                0x0010

#define SPI_CONTROL_SPR(clk)      ((clk) & 0x0f)  

#define ASIC3_PWM_0_Base                0x0500
#define ASIC3_PWM_1_Base                0x0600
#define ASIC3_PWM_TimeBase              0x0000
#define ASIC3_PWM_PeriodTime            0x0004
#define ASIC3_PWM_DutyTime              0x0008

#define PWM_TIMEBASE_VALUE(x)    ((x)&0xf)   
#define PWM_TIMEBASE_ENABLE     (1 << 4)   

#define ASIC3_NUM_LEDS                  3
#define ASIC3_LED_0_Base                0x0700
#define ASIC3_LED_1_Base                0x0800
#define ASIC3_LED_2_Base 		      0x0900
#define ASIC3_LED_TimeBase              0x0000    
#define ASIC3_LED_PeriodTime            0x0004    
#define ASIC3_LED_DutyTime              0x0008    
#define ASIC3_LED_AutoStopCount         0x000c    

#define LED_TBS		0x0f 
			     
			     
			     
			     

#define LED_EN		(1 << 4) 
#define LED_AUTOSTOP	(1 << 5) 
#define LED_ALWAYS	(1 << 6) 

#define ASIC3_CLOCK_BASE	   0x0A00
#define ASIC3_CLOCK_CDEX           0x00
#define ASIC3_CLOCK_SEL            0x04

#define CLOCK_CDEX_SOURCE       (1 << 0)  
#define CLOCK_CDEX_SOURCE0      (1 << 0)
#define CLOCK_CDEX_SOURCE1      (1 << 1)
#define CLOCK_CDEX_SPI          (1 << 2)
#define CLOCK_CDEX_OWM          (1 << 3)
#define CLOCK_CDEX_PWM0         (1 << 4)
#define CLOCK_CDEX_PWM1         (1 << 5)
#define CLOCK_CDEX_LED0         (1 << 6)
#define CLOCK_CDEX_LED1         (1 << 7)
#define CLOCK_CDEX_LED2         (1 << 8)

#define CLOCK_CDEX_SD_HOST      (1 << 9)   
#define CLOCK_CDEX_SD_BUS       (1 << 10)  
#define CLOCK_CDEX_SMBUS        (1 << 11)
#define CLOCK_CDEX_CONTROL_CX   (1 << 12)

#define CLOCK_CDEX_EX0          (1 << 13)  
#define CLOCK_CDEX_EX1          (1 << 14)  

#define CLOCK_SEL_SD_HCLK_SEL   (1 << 0)   
#define CLOCK_SEL_SD_BCLK_SEL   (1 << 1)   

#define CLOCK_SEL_CX            (1 << 2)


#define ASIC3_INTR_BASE		0x0B00

#define ASIC3_INTR_INT_MASK       0x00  
#define ASIC3_INTR_P_INT_STAT     0x04  
#define ASIC3_INTR_INT_CPS        0x08  
#define ASIC3_INTR_INT_TBS        0x0c  

#define ASIC3_INTMASK_GINTMASK    (1 << 0)  
#define ASIC3_INTMASK_GINTEL      (1 << 1)  
#define ASIC3_INTMASK_MASK0       (1 << 2)
#define ASIC3_INTMASK_MASK1       (1 << 3)
#define ASIC3_INTMASK_MASK2       (1 << 4)
#define ASIC3_INTMASK_MASK3       (1 << 5)
#define ASIC3_INTMASK_MASK4       (1 << 6)
#define ASIC3_INTMASK_MASK5       (1 << 7)

#define ASIC3_INTR_PERIPHERAL_A   (1 << 0)
#define ASIC3_INTR_PERIPHERAL_B   (1 << 1)
#define ASIC3_INTR_PERIPHERAL_C   (1 << 2)
#define ASIC3_INTR_PERIPHERAL_D   (1 << 3)
#define ASIC3_INTR_LED0           (1 << 4)
#define ASIC3_INTR_LED1           (1 << 5)
#define ASIC3_INTR_LED2           (1 << 6)
#define ASIC3_INTR_SPI            (1 << 7)
#define ASIC3_INTR_SMBUS          (1 << 8)
#define ASIC3_INTR_OWM            (1 << 9)

#define ASIC3_INTR_CPS(x)         ((x)&0x0f)    
#define ASIC3_INTR_CPS_SET        (1 << 4)    


#define ASIC3_SDHWCTRL_BASE     0x0E00
#define ASIC3_SDHWCTRL_SDCONF     0x00

#define ASIC3_SDHWCTRL_SUSPEND    (1 << 0)  
#define ASIC3_SDHWCTRL_CLKSEL     (1 << 1)  
#define ASIC3_SDHWCTRL_PCLR       (1 << 2)  
#define ASIC3_SDHWCTRL_LEVCD      (1 << 3)  

#define ASIC3_SDHWCTRL_LEVWP      (1 << 4)
#define ASIC3_SDHWCTRL_SDLED      (1 << 5)  

#define ASIC3_SDHWCTRL_SDPWR      (1 << 6)

#define ASIC3_EXTCF_BASE        0x1100

#define ASIC3_EXTCF_SELECT        0x00
#define ASIC3_EXTCF_RESET         0x04

#define ASIC3_EXTCF_SMOD0	         (1 << 0)  
#define ASIC3_EXTCF_SMOD1	         (1 << 1)  
#define ASIC3_EXTCF_SMOD2	         (1 << 2)  
#define ASIC3_EXTCF_OWM_EN	         (1 << 4)  
#define ASIC3_EXTCF_OWM_SMB	         (1 << 5)  
#define ASIC3_EXTCF_OWM_RESET            (1 << 6)  
#define ASIC3_EXTCF_CF0_SLEEP_MODE       (1 << 7)  
#define ASIC3_EXTCF_CF1_SLEEP_MODE       (1 << 8)  
#define ASIC3_EXTCF_CF0_PWAIT_EN         (1 << 10) 
#define ASIC3_EXTCF_CF1_PWAIT_EN         (1 << 11) 
#define ASIC3_EXTCF_CF0_BUF_EN           (1 << 12) 
#define ASIC3_EXTCF_CF1_BUF_EN           (1 << 13) 
#define ASIC3_EXTCF_SD_MEM_ENABLE        (1 << 14)
#define ASIC3_EXTCF_CF_SLEEP             (1 << 15) 


#define ASIC3_OWM_BASE		0xC00

#define ASIC3_SD_CONFIG_BASE	0x0400 
#define ASIC3_SD_CONFIG_SIZE	0x0200 
#define ASIC3_SD_CTRL_BASE	0x1000
#define ASIC3_SDIO_CTRL_BASE	0x1200

#define ASIC3_MAP_SIZE_32BIT	0x2000
#define ASIC3_MAP_SIZE_16BIT	0x1000


struct asic3;
extern void asic3_write_register(struct asic3 *asic, unsigned int reg, u32 val);
extern u32 asic3_read_register(struct asic3 *asic, unsigned int reg);

#endif 
