#ifndef __SOUND_EMUX_LEGACY_H
#define __SOUND_EMUX_LEGACY_H

/*
 *  Copyright (c) 1999-2000 Takashi Iwai <tiwai@suse.de>
 *
 *  Definitions of OSS compatible headers for Emu8000 device informations
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

#include "seq_oss_legacy.h"


#define _EMUX_OSS_DEBUG_MODE		0x00
#define _EMUX_OSS_REVERB_MODE		0x01
#define _EMUX_OSS_CHORUS_MODE		0x02
#define _EMUX_OSS_REMOVE_LAST_SAMPLES	0x03
#define _EMUX_OSS_INITIALIZE_CHIP	0x04
#define _EMUX_OSS_SEND_EFFECT		0x05
#define _EMUX_OSS_TERMINATE_CHANNEL	0x06
#define _EMUX_OSS_TERMINATE_ALL		0x07
#define _EMUX_OSS_INITIAL_VOLUME	0x08
#define _EMUX_OSS_INITIAL_ATTEN	_EMUX_OSS_INITIAL_VOLUME
#define _EMUX_OSS_RESET_CHANNEL		0x09
#define _EMUX_OSS_CHANNEL_MODE		0x0a
#define _EMUX_OSS_DRUM_CHANNELS		0x0b
#define _EMUX_OSS_MISC_MODE		0x0c
#define _EMUX_OSS_RELEASE_ALL		0x0d
#define _EMUX_OSS_NOTEOFF_ALL		0x0e
#define _EMUX_OSS_CHN_PRESSURE		0x0f
#define _EMUX_OSS_EQUALIZER		0x11

#define _EMUX_OSS_MODE_FLAG		0x80
#define _EMUX_OSS_COOKED_FLAG		0x40	
#define _EMUX_OSS_MODE_VALUE_MASK	0x3F


enum {
	EMUX_MD_EXCLUSIVE_OFF,	
	EMUX_MD_EXCLUSIVE_ON,	
	EMUX_MD_VERSION,		
	EMUX_MD_EXCLUSIVE_SOUND,	
	EMUX_MD_REALTIME_PAN,	
	EMUX_MD_GUS_BANK,	
	EMUX_MD_KEEP_EFFECT,	
	EMUX_MD_ZERO_ATTEN,	
	EMUX_MD_CHN_PRIOR,	
	EMUX_MD_MOD_SENSE,	
	EMUX_MD_DEF_PRESET,	
	EMUX_MD_DEF_BANK,	
	EMUX_MD_DEF_DRUM,	
	EMUX_MD_TOGGLE_DRUM_BANK, 
	EMUX_MD_NEW_VOLUME_CALC,	
	EMUX_MD_CHORUS_MODE,	
	EMUX_MD_REVERB_MODE,	
	EMUX_MD_BASS_LEVEL,	
	EMUX_MD_TREBLE_LEVEL,	
	EMUX_MD_DEBUG_MODE,	
	EMUX_MD_PAN_EXCHANGE,	
	EMUX_MD_END,
};


enum {

	EMUX_FX_ENV1_DELAY,	
	EMUX_FX_ENV1_ATTACK,	
	EMUX_FX_ENV1_HOLD,	
	EMUX_FX_ENV1_DECAY,	
	EMUX_FX_ENV1_RELEASE,	
	EMUX_FX_ENV1_SUSTAIN,	
	EMUX_FX_ENV1_PITCH,	
	EMUX_FX_ENV1_CUTOFF,	

	EMUX_FX_ENV2_DELAY,	
	EMUX_FX_ENV2_ATTACK,	
	EMUX_FX_ENV2_HOLD,	
	EMUX_FX_ENV2_DECAY,	
	EMUX_FX_ENV2_RELEASE,	
	EMUX_FX_ENV2_SUSTAIN,	
	
	EMUX_FX_LFO1_DELAY,	
	EMUX_FX_LFO1_FREQ,	
	EMUX_FX_LFO1_VOLUME,	
	EMUX_FX_LFO1_PITCH,	
	EMUX_FX_LFO1_CUTOFF,	

	EMUX_FX_LFO2_DELAY,	
	EMUX_FX_LFO2_FREQ,	
	EMUX_FX_LFO2_PITCH,	

	EMUX_FX_INIT_PITCH,	
	EMUX_FX_CHORUS,		
	EMUX_FX_REVERB,		
	EMUX_FX_CUTOFF,		
	EMUX_FX_FILTERQ,		

	EMUX_FX_SAMPLE_START,	
	EMUX_FX_LOOP_START,	
	EMUX_FX_LOOP_END,	
	EMUX_FX_COARSE_SAMPLE_START,	
	EMUX_FX_COARSE_LOOP_START,	
	EMUX_FX_COARSE_LOOP_END,		
	EMUX_FX_ATTEN,		

	EMUX_FX_END,
};
#define EMUX_NUM_EFFECTS  EMUX_FX_END

#define EMUX_FX_FLAG_OFF	0
#define EMUX_FX_FLAG_SET	1
#define EMUX_FX_FLAG_ADD	2


#endif 
