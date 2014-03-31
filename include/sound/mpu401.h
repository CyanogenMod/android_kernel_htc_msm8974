#ifndef __SOUND_MPU401_H
#define __SOUND_MPU401_H

/*
 *  Header file for MPU-401 and compatible cards
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "rawmidi.h"
#include <linux/interrupt.h>

#define MPU401_HW_MPU401		1	
#define MPU401_HW_SB			2	
#define MPU401_HW_ES1688		3	
#define MPU401_HW_OPL3SA2		4	
#define MPU401_HW_SONICVIBES		5	
#define MPU401_HW_CS4232		6	
#define MPU401_HW_ES18XX		7	
#define MPU401_HW_FM801			8	
#define MPU401_HW_TRID4DWAVE		9	
#define MPU401_HW_AZT2320		10	
#define MPU401_HW_ALS100		11	
#define MPU401_HW_ICE1712		12	
#define MPU401_HW_VIA686A		13	
#define MPU401_HW_YMFPCI		14	
#define MPU401_HW_CMIPCI		15	
#define MPU401_HW_ALS4000		16	
#define MPU401_HW_INTEL8X0		17	
#define MPU401_HW_PC98II		18	
#define MPU401_HW_AUREAL		19	

#define MPU401_INFO_INPUT	(1 << 0)	
#define MPU401_INFO_OUTPUT	(1 << 1)	
#define MPU401_INFO_INTEGRATED	(1 << 2)	
#define MPU401_INFO_MMIO	(1 << 3)	
#define MPU401_INFO_TX_IRQ	(1 << 4)	
#define MPU401_INFO_IRQ_HOOK	(1 << 5)	
#define MPU401_INFO_NO_ACK	(1 << 6)	
#define MPU401_INFO_USE_TIMER	(1 << 15)	

#define MPU401_MODE_BIT_INPUT		0
#define MPU401_MODE_BIT_OUTPUT		1
#define MPU401_MODE_BIT_INPUT_TRIGGER	2
#define MPU401_MODE_BIT_OUTPUT_TRIGGER	3

#define MPU401_MODE_INPUT		(1<<MPU401_MODE_BIT_INPUT)
#define MPU401_MODE_OUTPUT		(1<<MPU401_MODE_BIT_OUTPUT)
#define MPU401_MODE_INPUT_TRIGGER	(1<<MPU401_MODE_BIT_INPUT_TRIGGER)
#define MPU401_MODE_OUTPUT_TRIGGER	(1<<MPU401_MODE_BIT_OUTPUT_TRIGGER)

#define MPU401_MODE_INPUT_TIMER		(1<<0)
#define MPU401_MODE_OUTPUT_TIMER	(1<<1)

struct snd_mpu401 {
	struct snd_rawmidi *rmidi;

	unsigned short hardware;	
	unsigned int info_flags;	
	unsigned long port;		
	unsigned long cport;		
	struct resource *res;		
	int irq;			

	unsigned long mode;		
	int timer_invoked;

	int (*open_input) (struct snd_mpu401 * mpu);
	void (*close_input) (struct snd_mpu401 * mpu);
	int (*open_output) (struct snd_mpu401 * mpu);
	void (*close_output) (struct snd_mpu401 * mpu);
	void *private_data;

	struct snd_rawmidi_substream *substream_input;
	struct snd_rawmidi_substream *substream_output;

	spinlock_t input_lock;
	spinlock_t output_lock;
	spinlock_t timer_lock;
	
	struct timer_list timer;

	void (*write) (struct snd_mpu401 * mpu, unsigned char data, unsigned long addr);
	unsigned char (*read) (struct snd_mpu401 *mpu, unsigned long addr);
};


#define MPU401C(mpu) (mpu)->cport
#define MPU401D(mpu) (mpu)->port

#define MPU401_RX_EMPTY		0x80
#define MPU401_TX_FULL		0x40

#define MPU401_RESET		0xff
#define MPU401_ENTER_UART	0x3f

#define MPU401_ACK		0xfe



irqreturn_t snd_mpu401_uart_interrupt(int irq, void *dev_id);
irqreturn_t snd_mpu401_uart_interrupt_tx(int irq, void *dev_id);

int snd_mpu401_uart_new(struct snd_card *card,
			int device,
			unsigned short hardware,
			unsigned long port,
			unsigned int info_flags,
			int irq,
			struct snd_rawmidi ** rrawmidi);

#endif 
