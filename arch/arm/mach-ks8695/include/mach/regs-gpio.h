/*
 * arch/arm/mach-ks8695/include/mach/regs-gpio.h
 *
 * Copyright (C) 2007 Andrew Victor
 *
 * KS8695 - GPIO control registers and bit definitions.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef KS8695_GPIO_H
#define KS8695_GPIO_H

#define KS8695_GPIO_OFFSET	(0xF0000 + 0xE600)
#define KS8695_GPIO_VA		(KS8695_IO_VA + KS8695_GPIO_OFFSET)
#define KS8695_GPIO_PA		(KS8695_IO_PA + KS8695_GPIO_OFFSET)


#define KS8695_IOPM		(0x00)		
#define KS8695_IOPC		(0x04)		
#define KS8695_IOPD		(0x08)		


#define IOPM(x)			(1 << (x))	

#define IOPC_IOTIM1EN		(1 << 17)	
#define IOPC_IOTIM0EN		(1 << 16)	
#define IOPC_IOEINT3EN		(1 << 15)	
#define IOPC_IOEINT3TM		(7 << 12)	
#define IOPC_IOEINT3_MODE(x)	((x) << 12)
#define IOPC_IOEINT2EN		(1 << 11)	
#define IOPC_IOEINT2TM		(7 << 8)	
#define IOPC_IOEINT2_MODE(x)	((x) << 8)
#define IOPC_IOEINT1EN		(1 << 7)	
#define IOPC_IOEINT1TM		(7 << 4)	
#define IOPC_IOEINT1_MODE(x)	((x) << 4)
#define IOPC_IOEINT0EN		(1 << 3)	
#define IOPC_IOEINT0TM		(7 << 0)	
#define IOPC_IOEINT0_MODE(x)	((x) << 0)

 
#define IOPC_TM_LOW		(0)		
#define IOPC_TM_HIGH		(1)		
#define IOPC_TM_RISING		(2)		
#define IOPC_TM_FALLING		(4)		
#define IOPC_TM_EDGE		(6)		

#define IOPD(x)			(1 << (x))	

#endif
