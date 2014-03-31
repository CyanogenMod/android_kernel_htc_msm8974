/*****************************************************************************
 *
 * Copyright (C) 2008 Cedric Bregardis <cedric.bregardis@free.fr> and
 * Jean-Christian Hassler <jhassler@free.fr>
 *
 * This file is part of the Audiowerk2 ALSA driver
 *
 * The Audiowerk2 ALSA driver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * The Audiowerk2 ALSA driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Audiowerk2 ALSA driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 *****************************************************************************/

#define AW2_SAA7146_M

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "saa7146.h"
#include "aw2-saa7146.h"

#include "aw2-tsl.c"

#define WRITEREG(value, addr) writel((value), chip->base_addr + (addr))
#define READREG(addr) readl(chip->base_addr + (addr))

static struct snd_aw2_saa7146_cb_param
 arr_substream_it_playback_cb[NB_STREAM_PLAYBACK];
static struct snd_aw2_saa7146_cb_param
 arr_substream_it_capture_cb[NB_STREAM_CAPTURE];

static int snd_aw2_saa7146_get_limit(int size);

int snd_aw2_saa7146_free(struct snd_aw2_saa7146 *chip)
{
	
	WRITEREG(0, IER);

	
	WRITEREG((MRST_N << 16), MC1);

	
	chip->base_addr = NULL;

	return 0;
}

void snd_aw2_saa7146_setup(struct snd_aw2_saa7146 *chip,
			   void __iomem *pci_base_addr)
{

	unsigned int acon2;
	unsigned int acon1 = 0;
	int i;

	
	chip->base_addr = pci_base_addr;

	
	WRITEREG(0, IER);

	
	WRITEREG((MRST_N << 16), MC1);

	
#ifdef __BIG_ENDIAN
	acon1 |= A1_SWAP;
	acon1 |= A2_SWAP;
#endif
	

	
	acon1 |= 0 * WS1_CTRL;
	acon1 |= 0 * WS2_CTRL;

	acon1 |= 3 * WS4_CTRL;

	
	acon1 |= 2 * WS3_CTRL;

	
	acon1 |= 3 * AUDIO_MODE;
	WRITEREG(acon1, ACON1);

	WRITEREG(3 * (BurstA1_in) + 3 * (ThreshA1_in) +
		 3 * (BurstA1_out) + 3 * (ThreshA1_out) +
		 3 * (BurstA2_out) + 3 * (ThreshA2_out), PCI_BT_A);

	
	WRITEREG((EAP << 16) | EAP, MC1);

	
	WRITEREG((EI2C << 16) | EI2C, MC1);
	
	WRITEREG(A1_out | A2_out | A1_in | IIC_S | IIC_E, IER);

	
	acon2 = A2_CLKSRC | BCLK1_OEN;
	WRITEREG(acon2, ACON2);

	
	snd_aw2_saa7146_use_digital_input(chip, 0);

	
	for (i = 0; i < 8; ++i) {
		WRITEREG(tsl1[i], TSL1 + (i * 4));
		WRITEREG(tsl2[i], TSL2 + (i * 4));
	}

}

void snd_aw2_saa7146_pcm_init_playback(struct snd_aw2_saa7146 *chip,
				       int stream_number,
				       unsigned long dma_addr,
				       unsigned long period_size,
				       unsigned long buffer_size)
{
	unsigned long dw_page, dw_limit;


	
	dw_page = (0L << 11);

	dw_limit = snd_aw2_saa7146_get_limit(period_size);
	dw_page |= (dw_limit << 4);

	if (stream_number == 0) {
		WRITEREG(dw_page, PageA2_out);

		
		
		
		WRITEREG(dma_addr, BaseA2_out);

		
		WRITEREG(dma_addr + buffer_size, ProtA2_out);

	} else if (stream_number == 1) {
		WRITEREG(dw_page, PageA1_out);

		
		
		
		WRITEREG(dma_addr, BaseA1_out);

		
		WRITEREG(dma_addr + buffer_size, ProtA1_out);
	} else {
		printk(KERN_ERR
		       "aw2: snd_aw2_saa7146_pcm_init_playback: "
		       "Substream number is not 0 or 1 -> not managed\n");
	}
}

void snd_aw2_saa7146_pcm_init_capture(struct snd_aw2_saa7146 *chip,
				      int stream_number, unsigned long dma_addr,
				      unsigned long period_size,
				      unsigned long buffer_size)
{
	unsigned long dw_page, dw_limit;


	
	dw_page = (0L << 11);

	dw_limit = snd_aw2_saa7146_get_limit(period_size);
	dw_page |= (dw_limit << 4);

	if (stream_number == 0) {
		WRITEREG(dw_page, PageA1_in);

		
		
		
		WRITEREG(dma_addr, BaseA1_in);

		
		WRITEREG(dma_addr + buffer_size, ProtA1_in);
	} else {
		printk(KERN_ERR
		       "aw2: snd_aw2_saa7146_pcm_init_capture: "
		       "Substream number is not 0 -> not managed\n");
	}
}

void snd_aw2_saa7146_define_it_playback_callback(unsigned int stream_number,
						 snd_aw2_saa7146_it_cb
						 p_it_callback,
						 void *p_callback_param)
{
	if (stream_number < NB_STREAM_PLAYBACK) {
		arr_substream_it_playback_cb[stream_number].p_it_callback =
		    (snd_aw2_saa7146_it_cb) p_it_callback;
		arr_substream_it_playback_cb[stream_number].p_callback_param =
		    (void *)p_callback_param;
	}
}

void snd_aw2_saa7146_define_it_capture_callback(unsigned int stream_number,
						snd_aw2_saa7146_it_cb
						p_it_callback,
						void *p_callback_param)
{
	if (stream_number < NB_STREAM_CAPTURE) {
		arr_substream_it_capture_cb[stream_number].p_it_callback =
		    (snd_aw2_saa7146_it_cb) p_it_callback;
		arr_substream_it_capture_cb[stream_number].p_callback_param =
		    (void *)p_callback_param;
	}
}

void snd_aw2_saa7146_pcm_trigger_start_playback(struct snd_aw2_saa7146 *chip,
						int stream_number)
{
	unsigned int acon1 = 0;
	acon1 = READREG(ACON1);
	if (stream_number == 0) {
		WRITEREG((TR_E_A2_OUT << 16) | TR_E_A2_OUT, MC1);

		
		acon1 |= 2 * WS2_CTRL;
		WRITEREG(acon1, ACON1);

	} else if (stream_number == 1) {
		WRITEREG((TR_E_A1_OUT << 16) | TR_E_A1_OUT, MC1);

		
		acon1 |= 1 * WS1_CTRL;
		WRITEREG(acon1, ACON1);
	}
}

void snd_aw2_saa7146_pcm_trigger_stop_playback(struct snd_aw2_saa7146 *chip,
					       int stream_number)
{
	unsigned int acon1 = 0;
	acon1 = READREG(ACON1);
	if (stream_number == 0) {
		
		acon1 &= ~(3 * WS2_CTRL);
		WRITEREG(acon1, ACON1);

		WRITEREG((TR_E_A2_OUT << 16), MC1);
	} else if (stream_number == 1) {
		
		acon1 &= ~(3 * WS1_CTRL);
		WRITEREG(acon1, ACON1);

		WRITEREG((TR_E_A1_OUT << 16), MC1);
	}
}

void snd_aw2_saa7146_pcm_trigger_start_capture(struct snd_aw2_saa7146 *chip,
					       int stream_number)
{
	if (stream_number == 0)
		WRITEREG((TR_E_A1_IN << 16) | TR_E_A1_IN, MC1);
}

void snd_aw2_saa7146_pcm_trigger_stop_capture(struct snd_aw2_saa7146 *chip,
					      int stream_number)
{
	if (stream_number == 0)
		WRITEREG((TR_E_A1_IN << 16), MC1);
}

irqreturn_t snd_aw2_saa7146_interrupt(int irq, void *dev_id)
{
	unsigned int isr;
	unsigned int iicsta;
	struct snd_aw2_saa7146 *chip = dev_id;

	isr = READREG(ISR);
	if (!isr)
		return IRQ_NONE;

	WRITEREG(isr, ISR);

	if (isr & (IIC_S | IIC_E)) {
		iicsta = READREG(IICSTA);
		WRITEREG(0x100, IICSTA);
	}

	if (isr & A1_out) {
		if (arr_substream_it_playback_cb[1].p_it_callback != NULL) {
			arr_substream_it_playback_cb[1].
			    p_it_callback(arr_substream_it_playback_cb[1].
					  p_callback_param);
		}
	}
	if (isr & A2_out) {
		if (arr_substream_it_playback_cb[0].p_it_callback != NULL) {
			arr_substream_it_playback_cb[0].
			    p_it_callback(arr_substream_it_playback_cb[0].
					  p_callback_param);
		}

	}
	if (isr & A1_in) {
		if (arr_substream_it_capture_cb[0].p_it_callback != NULL) {
			arr_substream_it_capture_cb[0].
			    p_it_callback(arr_substream_it_capture_cb[0].
					  p_callback_param);
		}
	}
	return IRQ_HANDLED;
}

unsigned int snd_aw2_saa7146_get_hw_ptr_playback(struct snd_aw2_saa7146 *chip,
						 int stream_number,
						 unsigned char *start_addr,
						 unsigned int buffer_size)
{
	long pci_adp = 0;
	size_t ptr = 0;

	if (stream_number == 0) {
		pci_adp = READREG(PCI_ADP3);
		ptr = pci_adp - (long)start_addr;

		if (ptr == buffer_size)
			ptr = 0;
	}
	if (stream_number == 1) {
		pci_adp = READREG(PCI_ADP1);
		ptr = pci_adp - (size_t) start_addr;

		if (ptr == buffer_size)
			ptr = 0;
	}
	return ptr;
}

unsigned int snd_aw2_saa7146_get_hw_ptr_capture(struct snd_aw2_saa7146 *chip,
						int stream_number,
						unsigned char *start_addr,
						unsigned int buffer_size)
{
	size_t pci_adp = 0;
	size_t ptr = 0;
	if (stream_number == 0) {
		pci_adp = READREG(PCI_ADP2);
		ptr = pci_adp - (size_t) start_addr;

		if (ptr == buffer_size)
			ptr = 0;
	}
	return ptr;
}

void snd_aw2_saa7146_use_digital_input(struct snd_aw2_saa7146 *chip,
				       int use_digital)
{
	if (use_digital)
		WRITEREG(0x40, GPIO_CTRL);
	else
		WRITEREG(0x50, GPIO_CTRL);
}

int snd_aw2_saa7146_is_using_digital_input(struct snd_aw2_saa7146 *chip)
{
	unsigned int reg_val = READREG(GPIO_CTRL);
	if ((reg_val & 0xFF) == 0x40)
		return 1;
	else
		return 0;
}


static int snd_aw2_saa7146_get_limit(int size)
{
	int limitsize = 32;
	int limit = 0;
	while (limitsize < size) {
		limitsize *= 2;
		limit++;
	}
	return limit;
}
