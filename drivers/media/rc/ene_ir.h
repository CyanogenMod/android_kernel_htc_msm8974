/*
 * driver for ENE KB3926 B/C/D/E/F CIR (also known as ENE0XXX)
 *
 * Copyright (C) 2010 Maxim Levitsky <maximlevitsky@gmail.com>
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
#include <linux/spinlock.h>


#define ENE_STATUS		0	
#define ENE_ADDR_HI		1	
#define ENE_ADDR_LO		2	
#define ENE_IO			3	
#define ENE_IO_SIZE		4

#define ENE_FW_SAMPLE_BUFFER	0xF8F0	
#define ENE_FW_SAMPLE_SPACE	0x80	
#define ENE_FW_PACKET_SIZE	4

#define ENE_FW1			0xF8F8  
#define	ENE_FW1_ENABLE		0x01	
#define ENE_FW1_TXIRQ		0x02	
#define ENE_FW1_HAS_EXTRA_BUF	0x04	
#define ENE_FW1_EXTRA_BUF_HND	0x08	
#define ENE_FW1_LED_ON		0x10	

#define ENE_FW1_WPATTERN	0x20	
#define ENE_FW1_WAKE		0x40	
#define ENE_FW1_IRQ		0x80	

#define ENE_FW2			0xF8F9  
#define ENE_FW2_BUF_WPTR	0x01	
#define ENE_FW2_RXIRQ		0x04	
#define ENE_FW2_GP0A		0x08	
#define ENE_FW2_EMMITER1_CONN	0x10	
#define ENE_FW2_EMMITER2_CONN	0x20	

#define ENE_FW2_FAN_INPUT	0x40	
#define ENE_FW2_LEARNING	0x80	

#define ENE_FW_RX_POINTER	0xF8FA

#define ENE_FW_SMPL_BUF_FAN	0xF8FB
#define ENE_FW_SMPL_BUF_FAN_PLS	0x8000	
#define ENE_FW_SMPL_BUF_FAN_MSK	0x0FFF  
#define ENE_FW_SAMPLE_PERIOD_FAN 61	

#define ENE_GPIOFS1		0xFC01
#define ENE_GPIOFS1_GPIO0D	0x20	
#define ENE_GPIOFS8		0xFC08
#define ENE_GPIOFS8_GPIO41	0x02	

#define ENEB_IRQ		0xFD09	
#define ENEB_IRQ_UNK1		0xFD17	
#define ENEB_IRQ_STATUS		0xFD80	
#define ENEB_IRQ_STATUS_IR	0x20	

#define ENE_FAN_AS_IN1		0xFE30  
#define ENE_FAN_AS_IN1_EN	0xCD
#define ENE_FAN_AS_IN2		0xFE31  
#define ENE_FAN_AS_IN2_EN	0x03

#define ENE_IRQ			0xFE9B	
#define ENE_IRQ_MASK		0x0F	
#define ENE_IRQ_UNK_EN		0x10	
#define ENE_IRQ_STATUS		0x20	

#define ENE_CIRCFG		0xFEC0
#define ENE_CIRCFG_RX_EN	0x01	
#define ENE_CIRCFG_RX_IRQ	0x02	
#define ENE_CIRCFG_REV_POL	0x04	
#define ENE_CIRCFG_CARR_DEMOD	0x08	

#define ENE_CIRCFG_TX_EN	0x10	
#define ENE_CIRCFG_TX_IRQ	0x20	
#define ENE_CIRCFG_TX_POL_REV	0x40	
#define ENE_CIRCFG_TX_CARR	0x80	

#define ENE_CIRCFG2		0xFEC1
#define ENE_CIRCFG2_RLC		0x00
#define ENE_CIRCFG2_RC5		0x01
#define ENE_CIRCFG2_RC6		0x02
#define ENE_CIRCFG2_NEC		0x03
#define ENE_CIRCFG2_CARR_DETECT	0x10	
#define ENE_CIRCFG2_GPIO0A	0x20	
#define ENE_CIRCFG2_FAST_SAMPL1	0x40	
#define ENE_CIRCFG2_FAST_SAMPL2	0x80	

#define ENE_CIRPF		0xFEC2
#define ENE_CIRHIGH		0xFEC3
#define ENE_CIRBIT		0xFEC4
#define ENE_CIRSTART		0xFEC5
#define ENE_CIRSTART2		0xFEC6

#define ENE_CIRDAT_IN		0xFEC7


#define ENE_CIRRLC_CFG		0xFEC8
#define ENE_CIRRLC_CFG_OVERFLOW	0x80	
#define ENE_DEFAULT_SAMPLE_PERIOD 50

#define ENE_CIRRLC_OUT0		0xFEC9
#define ENE_CIRRLC_OUT1		0xFECA
#define ENE_CIRRLC_OUT_PULSE	0x80	
#define ENE_CIRRLC_OUT_MASK	0x7F


#define ENE_CIRCAR_PULS		0xFECB

#define ENE_CIRCAR_PRD		0xFECC
#define ENE_CIRCAR_PRD_VALID	0x80	

#define ENE_CIRCAR_HPRD		0xFECD

#define ENE_CIRMOD_PRD		0xFECE
#define ENE_CIRMOD_PRD_POL	0x80	

#define ENE_CIRMOD_PRD_MAX	0x7F	
#define ENE_CIRMOD_PRD_MIN	0x02	

#define ENE_CIRMOD_HPRD		0xFECF

#define ENE_ECHV		0xFF00	
#define ENE_PLLFRH		0xFF16
#define ENE_PLLFRL		0xFF17
#define ENE_DEFAULT_PLL_FREQ	1000

#define ENE_ECSTS		0xFF1D
#define ENE_ECSTS_RSRVD		0x04

#define ENE_ECVER_MAJOR		0xFF1E	
#define ENE_ECVER_MINOR		0xFF1F
#define ENE_HW_VER_OLD		0xFD00


#define ENE_DRIVER_NAME		"ene_ir"

#define ENE_IRQ_RX		1
#define ENE_IRQ_TX		2

#define  ENE_HW_B		1	
#define  ENE_HW_C		2	
#define  ENE_HW_D		3	

#define __dbg(level, format, ...)				\
do {								\
	if (debug >= level)					\
		pr_debug(format "\n", ## __VA_ARGS__);		\
} while (0)

#define dbg(format, ...)		__dbg(1, format, ## __VA_ARGS__)
#define dbg_verbose(format, ...)	__dbg(2, format, ## __VA_ARGS__)
#define dbg_regs(format, ...)		__dbg(3, format, ## __VA_ARGS__)

struct ene_device {
	struct pnp_dev *pnp_dev;
	struct rc_dev *rdev;

	
	long hw_io;
	int irq;
	spinlock_t hw_lock;

	
	int hw_revision;			
	bool hw_use_gpio_0a;			
	bool hw_extra_buffer;			
	bool hw_fan_input;			
	bool hw_learning_and_tx_capable;	
	int  pll_freq;
	int buffer_len;

	
	int extra_buf1_address;
	int extra_buf1_len;
	int extra_buf2_address;
	int extra_buf2_len;

	
	int r_pointer;				
	int w_pointer;				
	bool rx_fan_input_inuse;		
	int tx_reg;				
	u8  saved_conf1;			
	unsigned int tx_sample;			
	bool tx_sample_pulse;			

	
	unsigned *tx_buffer;			
	int tx_pos;				
	int tx_len;				
	int tx_done;				
						
	struct completion tx_complete;		
	struct timer_list tx_sim_timer;

	
	int tx_period;
	int tx_duty_cycle;
	int transmitter_mask;

	
	bool learning_mode_enabled;		
	bool carrier_detect_enabled;		
	int rx_period_adjust;
	bool rx_enabled;
};

static int ene_irq_status(struct ene_device *dev);
static void ene_rx_read_hw_pointer(struct ene_device *dev);
