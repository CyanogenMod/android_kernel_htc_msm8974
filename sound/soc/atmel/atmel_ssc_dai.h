/*
 * atmel_ssc_dai.h - ALSA SSC interface for the Atmel  SoC
 *
 * Copyright (C) 2005 SAN People
 * Copyright (C) 2008 Atmel
 *
 * Author: Sedji Gaouaou <sedji.gaouaou@atmel.com>
 *         ATMEL CORP.
 *
 * Based on at91-ssc.c by
 * Frank Mandarino <fmandarino@endrelia.com>
 * Based on pxa2xx Platform drivers by
 * Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _ATMEL_SSC_DAI_H
#define _ATMEL_SSC_DAI_H

#include <linux/types.h>
#include <linux/atmel-ssc.h>

#include "atmel-pcm.h"

#define ATMEL_SYSCLK_MCK	0 

#define ATMEL_SSC_CMR_DIV	0 
#define ATMEL_SSC_TCMR_PERIOD	1 
#define ATMEL_SSC_RCMR_PERIOD	2 
#define SSC_DIR_MASK_UNUSED	0
#define SSC_DIR_MASK_PLAYBACK	1
#define SSC_DIR_MASK_CAPTURE	2

#define SSC_START_CONTINUOUS	0
#define SSC_START_TX_RX		1
#define SSC_START_LOW_RF	2
#define SSC_START_HIGH_RF	3
#define SSC_START_FALLING_RF	4
#define SSC_START_RISING_RF	5
#define SSC_START_LEVEL_RF	6
#define SSC_START_EDGE_RF	7
#define SSS_START_COMPARE_0	8

#define SSC_CKI_FALLING		0
#define SSC_CKI_RISING		1

#define SSC_CKO_NONE		0
#define SSC_CKO_CONTINUOUS	1
#define SSC_CKO_TRANSFER	2

#define SSC_CKS_DIV		0
#define SSC_CKS_CLOCK		1
#define SSC_CKS_PIN		2

#define SSC_FSEDGE_POSITIVE	0
#define SSC_FSEDGE_NEGATIVE	1

#define SSC_FSOS_NONE		0
#define SSC_FSOS_NEGATIVE	1
#define SSC_FSOS_POSITIVE	2
#define SSC_FSOS_LOW		3
#define SSC_FSOS_HIGH		4
#define SSC_FSOS_TOGGLE		5

#define START_DELAY		1

struct atmel_ssc_state {
	u32 ssc_cmr;
	u32 ssc_rcmr;
	u32 ssc_rfmr;
	u32 ssc_tcmr;
	u32 ssc_tfmr;
	u32 ssc_sr;
	u32 ssc_imr;
};


struct atmel_ssc_info {
	char *name;
	struct ssc_device *ssc;
	spinlock_t lock;	
	unsigned short dir_mask;	
	unsigned short initialized;	
	unsigned short daifmt;
	unsigned short cmr_div;
	unsigned short tcmr_period;
	unsigned short rcmr_period;
	struct atmel_pcm_dma_params *dma_params[2];
	struct atmel_ssc_state ssc_state;
};

int atmel_ssc_set_audio(int ssc);

#endif 
