/* arch/arm/mach-zynq/include/mach/uart.h
 *
 *  Copyright (C) 2011 Xilinx
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_UART_H__
#define __MACH_UART_H__

#define UART_CR_OFFSET		0x00  
#define UART_SR_OFFSET		0x2C  
#define UART_FIFO_OFFSET	0x30  

#define UART_SR_TXFULL		0x00000010	
#define UART_SR_TXEMPTY		0x00000008	

#endif
