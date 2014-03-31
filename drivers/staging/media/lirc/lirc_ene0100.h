/*
 * driver for ENE KB3926 B/C/D CIR (also known as ENE0100)
 *
 * Copyright (C) 2009 Maxim Levitsky <maximlevitsky@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <media/lirc.h>
#include <media/lirc_dev.h>

#define ENE_STATUS		0	 
#define ENE_ADDR_HI		1	 
#define ENE_ADDR_LO		2	 
#define ENE_IO			3	 
#define ENE_MAX_IO		4

#define ENE_SAMPLE_BUFFER	0xF8F0	 
#define ENE_SAMPLE_SPC_MASK	(1 << 7) 
#define ENE_SAMPLE_VALUE_MASK	0x7F
#define ENE_SAMPLE_OVERFLOW	0x7F
#define ENE_SAMPLES_SIZE	4

#define ENE_SAMPLE_BUFFER_FAN	0xF8FB	 
					 

#define ENE_FAN_SMPL_PULS_MSK	0x8000	 
					 
#define ENE_FAN_VALUE_MASK	0x0FFF   

#define ENE_FW1			0xF8F8
#define	ENE_FW1_ENABLE		(1 << 0) 
#define ENE_FW1_TXIRQ		(1 << 1) 
#define ENE_FW1_WAKE		(1 << 6) 
#define ENE_FW1_IRQ		(1 << 7) 

#define ENE_FW2			0xF8F9
#define ENE_FW2_BUF_HIGH	(1 << 0) 
#define ENE_FW2_IRQ_CLR		(1 << 2) 
#define ENE_FW2_GP40_AS_LEARN	(1 << 4) 
					 
#define ENE_FW2_FAN_AS_NRML_IN	(1 << 6) 
#define ENE_FW2_LEARNING	(1 << 7) 

#define ENE_FAN_AS_IN1		0xFE30   
#define ENE_FAN_AS_IN1_EN	0xCD
#define ENE_FAN_AS_IN2		0xFE31   
#define ENE_FAN_AS_IN2_EN	0x03
#define ENE_SAMPLE_PERIOD_FAN   61	 

#define ENEB_IRQ		0xFD09	 
#define ENEB_IRQ_UNK1		0xFD17	 
#define ENEB_IRQ_STATUS		0xFD80	 
#define ENEB_IRQ_STATUS_IR	(1 << 5) 

#define ENEC_IRQ		0xFE9B	 
#define ENEC_IRQ_MASK		0x0F	 
#define ENEC_IRQ_UNK_EN		(1 << 4) 
#define ENEC_IRQ_STATUS		(1 << 5) 

#define ENE_CIR_CONF1		0xFEC0
#define ENE_CIR_CONF1_ADC_ON	0x7	 
#define ENE_CIR_CONF1_LEARN1	(1 << 3) 
#define ENE_CIR_CONF1_TX_ON	0x30	 
#define ENE_CIR_CONF1_TX_CARR	(1 << 7) 

#define ENE_CIR_CONF2		0xFEC1	 
#define ENE_CIR_CONF2_LEARN2	(1 << 4) 
#define ENE_CIR_CONF2_GPIO40DIS	(1 << 5) 

#define ENE_CIR_SAMPLE_PERIOD	0xFEC8	 
#define ENE_CIR_SAMPLE_OVERFLOW	(1 << 7) 


/* transmission is very similar to receiving, a byte is written to */


#define ENE_TX_PORT1		0xFC01	 
#define ENE_TX_PORT1_EN		(1 << 5) 
#define ENE_TX_PORT2		0xFC08
#define ENE_TX_PORT2_EN		(1 << 1)

#define ENE_TX_INPUT		0xFEC9	 
#define ENE_TX_SPC_MASK		(1 << 7) 
#define ENE_TX_UNK1		0xFECB	 
#define ENE_TX_SMPL_PERIOD	50	 


#define ENE_TX_CARRIER		0xFECE	 
#define ENE_TX_CARRIER_UNKBIT	0x80	 
#define ENE_TX_CARRIER_LOW	0xFECF	 

#define ENE_HW_VERSION		0xFF00	 
#define ENE_HW_UNK		0xFF1D
#define ENE_HW_UNK_CLR		(1 << 2)
#define ENE_HW_VER_MAJOR	0xFF1E	 
#define ENE_HW_VER_MINOR	0xFF1F
#define ENE_HW_VER_OLD		0xFD00

#define same_sign(a, b) ((((a) > 0) && (b) > 0) || ((a) < 0 && (b) < 0))

#define ENE_DRIVER_NAME		"enecir"
#define ENE_MAXGAP		250000	 

#define space(len)	       (-(len))	 

#define ENE_IRQ_RX		1
#define ENE_IRQ_TX		2

#define  ENE_HW_B		1	
#define  ENE_HW_C		2	
#define  ENE_HW_D		3	

#define ene_printk(level, text, ...) \
	printk(level ENE_DRIVER_NAME ": " text, ## __VA_ARGS__)

struct ene_device {
	struct pnp_dev *pnp_dev;
	struct lirc_driver *lirc_driver;

	
	unsigned long hw_io;
	int irq;

	int hw_revision;			
	int hw_learning_and_tx_capable;		
	int hw_gpio40_learning;			
	int hw_fan_as_normal_input;	

	
	int idle;
	int fan_input_inuse;

	int sample;
	int in_use;

	struct timeval gap_start;
};
