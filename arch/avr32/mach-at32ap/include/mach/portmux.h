/*
 * AT32 portmux interface.
 *
 * Copyright (C) 2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_PORTMUX_H__
#define __ASM_ARCH_PORTMUX_H__

#define AT32_GPIOF_PULLUP	0x00000001	
#define AT32_GPIOF_OUTPUT	0x00000002	
#define AT32_GPIOF_HIGH		0x00000004	
#define AT32_GPIOF_DEGLITCH	0x00000008	
#define AT32_GPIOF_MULTIDRV	0x00000010	

void at32_select_periph(unsigned int port, unsigned int pin,
			unsigned int periph, unsigned long flags);
void at32_select_gpio(unsigned int pin, unsigned long flags);
void at32_deselect_pin(unsigned int pin);
void at32_reserve_pin(unsigned int port, u32 pin_mask);

#endif 
