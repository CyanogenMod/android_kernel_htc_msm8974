/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *                   Creative Labs, Inc.
 *                   Lee Revell <rlrevell@joe-job.com>
 *  Routines for control of EMU10K1 chips - voice manager
 *
 *  Rewrote voice allocator for multichannel support - rlrevell 12/2004
 * 
 *  BUGS:
 *    --
 *
 *  TODO:
 *    --
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

#include <linux/time.h>
#include <linux/export.h>
#include <sound/core.h>
#include <sound/emu10k1.h>


static int voice_alloc(struct snd_emu10k1 *emu, int type, int number,
		       struct snd_emu10k1_voice **rvoice)
{
	struct snd_emu10k1_voice *voice;
	int i, j, k, first_voice, last_voice, skip;

	*rvoice = NULL;
	first_voice = last_voice = 0;
	for (i = emu->next_free_voice, j = 0; j < NUM_G ; i += number, j += number) {
		i %= NUM_G;

		
		if ((number == 2) && (i % 2)) {
			i++;
			continue;
		}
			
		skip = 0;
		for (k = 0; k < number; k++) {
			voice = &emu->voices[(i+k) % NUM_G];
			if (voice->use) {
				skip = 1;
				break;
			}
		}
		if (!skip) {
			
			first_voice = i;
			last_voice = (i + number) % NUM_G;
			emu->next_free_voice = last_voice;
			break;
		}
	}
	
	if (first_voice == last_voice)
		return -ENOMEM;
	
	for (i = 0; i < number; i++) {
		voice = &emu->voices[(first_voice + i) % NUM_G];
		voice->use = 1;
		switch (type) {
		case EMU10K1_PCM:
			voice->pcm = 1;
			break;
		case EMU10K1_SYNTH:
			voice->synth = 1;
			break;
		case EMU10K1_MIDI:
			voice->midi = 1;
			break;
		case EMU10K1_EFX:
			voice->efx = 1;
			break;
		}
	}
	*rvoice = &emu->voices[first_voice];
	return 0;
}

int snd_emu10k1_voice_alloc(struct snd_emu10k1 *emu, int type, int number,
			    struct snd_emu10k1_voice **rvoice)
{
	unsigned long flags;
	int result;

	if (snd_BUG_ON(!rvoice))
		return -EINVAL;
	if (snd_BUG_ON(!number))
		return -EINVAL;

	spin_lock_irqsave(&emu->voice_lock, flags);
	for (;;) {
		result = voice_alloc(emu, type, number, rvoice);
		if (result == 0 || type == EMU10K1_SYNTH || type == EMU10K1_MIDI)
			break;

		
		if (emu->get_synth_voice) {
			result = emu->get_synth_voice(emu);
			if (result >= 0) {
				struct snd_emu10k1_voice *pvoice = &emu->voices[result];
				pvoice->interrupt = NULL;
				pvoice->use = pvoice->pcm = pvoice->synth = pvoice->midi = pvoice->efx = 0;
				pvoice->epcm = NULL;
			}
		}
		if (result < 0)
			break;
	}
	spin_unlock_irqrestore(&emu->voice_lock, flags);

	return result;
}

EXPORT_SYMBOL(snd_emu10k1_voice_alloc);

int snd_emu10k1_voice_free(struct snd_emu10k1 *emu,
			   struct snd_emu10k1_voice *pvoice)
{
	unsigned long flags;

	if (snd_BUG_ON(!pvoice))
		return -EINVAL;
	spin_lock_irqsave(&emu->voice_lock, flags);
	pvoice->interrupt = NULL;
	pvoice->use = pvoice->pcm = pvoice->synth = pvoice->midi = pvoice->efx = 0;
	pvoice->epcm = NULL;
	snd_emu10k1_voice_init(emu, pvoice->number);
	spin_unlock_irqrestore(&emu->voice_lock, flags);
	return 0;
}

EXPORT_SYMBOL(snd_emu10k1_voice_free);
