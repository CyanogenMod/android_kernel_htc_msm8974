/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 *
 * @File	ctatc.h
 *
 * @Brief
 * This file contains the definition of the device resource management object.
 *
 * @Author	Liu Chun
 * @Date 	Mar 28 2008
 *
 */

#ifndef CTATC_H
#define CTATC_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <sound/core.h>

#include "ctvmem.h"
#include "cthardware.h"
#include "ctresource.h"

enum CTALSADEVS {		
	FRONT,
	SURROUND,
	CLFE,
	SIDE,
	IEC958,
	MIXER,
	NUM_CTALSADEVS		
};

struct ct_atc_chip_sub_details {
	u16 subsys;
	const char *nm_model;
};

struct ct_atc_chip_details {
	u16 vendor;
	u16 device;
	const struct ct_atc_chip_sub_details *sub_details;
	const char *nm_card;
};

struct ct_atc;
struct ct_timer;
struct ct_timer_instance;

struct ct_atc_pcm {
	struct snd_pcm_substream *substream;
	void (*interrupt)(struct ct_atc_pcm *apcm);
	struct ct_timer_instance *timer;
	unsigned int started:1;

	
	struct ct_vm_block *vm_block;
	void *src;		
	void **srccs;		
	void **srcimps;		
	void **amixers;		
	void *mono;		
	unsigned char n_srcc;	
	unsigned char n_srcimp;	
	unsigned char n_amixer;	
};

struct ct_atc {
	struct pci_dev *pci;
	struct snd_card *card;
	unsigned int rsr; 
	unsigned int msr; 
	unsigned int pll_rate; 

	int chip_type;
	int model;
	const char *chip_name;
	const char *model_name;

	struct ct_vm *vm; 
	int (*map_audio_buffer)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	void (*unmap_audio_buffer)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	unsigned long (*get_ptp_phys)(struct ct_atc *atc, int index);

	struct mutex atc_mutex;

	int (*pcm_playback_prepare)(struct ct_atc *atc,
				    struct ct_atc_pcm *apcm);
	int (*pcm_playback_start)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	int (*pcm_playback_stop)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	int (*pcm_playback_position)(struct ct_atc *atc,
				     struct ct_atc_pcm *apcm);
	int (*spdif_passthru_playback_prepare)(struct ct_atc *atc,
					       struct ct_atc_pcm *apcm);
	int (*pcm_capture_prepare)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	int (*pcm_capture_start)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	int (*pcm_capture_stop)(struct ct_atc *atc, struct ct_atc_pcm *apcm);
	int (*pcm_capture_position)(struct ct_atc *atc,
				    struct ct_atc_pcm *apcm);
	int (*pcm_release_resources)(struct ct_atc *atc,
				     struct ct_atc_pcm *apcm);
	int (*select_line_in)(struct ct_atc *atc);
	int (*select_mic_in)(struct ct_atc *atc);
	int (*select_digit_io)(struct ct_atc *atc);
	int (*line_front_unmute)(struct ct_atc *atc, unsigned char state);
	int (*line_surround_unmute)(struct ct_atc *atc, unsigned char state);
	int (*line_clfe_unmute)(struct ct_atc *atc, unsigned char state);
	int (*line_rear_unmute)(struct ct_atc *atc, unsigned char state);
	int (*line_in_unmute)(struct ct_atc *atc, unsigned char state);
	int (*mic_unmute)(struct ct_atc *atc, unsigned char state);
	int (*spdif_out_unmute)(struct ct_atc *atc, unsigned char state);
	int (*spdif_in_unmute)(struct ct_atc *atc, unsigned char state);
	int (*spdif_out_get_status)(struct ct_atc *atc, unsigned int *status);
	int (*spdif_out_set_status)(struct ct_atc *atc, unsigned int status);
	int (*spdif_out_passthru)(struct ct_atc *atc, unsigned char state);
	struct capabilities (*capabilities)(struct ct_atc *atc);
	int (*output_switch_get)(struct ct_atc *atc);
	int (*output_switch_put)(struct ct_atc *atc, int position);
	int (*mic_source_switch_get)(struct ct_atc *atc);
	int (*mic_source_switch_put)(struct ct_atc *atc, int position);

	
	void *rsc_mgrs[NUM_RSCTYP]; 
	void *mixer;		
	void *hw;		
	void **daios;		
	void **pcm;		
	void **srcs;		
	void **srcimps;		
	unsigned char n_daio;
	unsigned char n_src;
	unsigned char n_srcimp;
	unsigned char n_pcm;

	struct ct_timer *timer;

#ifdef CONFIG_PM
	int (*suspend)(struct ct_atc *atc, pm_message_t state);
	int (*resume)(struct ct_atc *atc);
#define NUM_PCMS (NUM_CTALSADEVS - 1)
	struct snd_pcm *pcms[NUM_PCMS];
#endif
};


int __devinit ct_atc_create(struct snd_card *card, struct pci_dev *pci,
			    unsigned int rsr, unsigned int msr, int chip_type,
			    unsigned int subsysid, struct ct_atc **ratc);
int __devinit ct_atc_create_alsa_devs(struct ct_atc *atc);

#endif 
