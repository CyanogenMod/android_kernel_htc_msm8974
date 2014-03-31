/*
 *  Copyright (c) by Uros Bizjak <uros@kss-loka.si>
 *
 *  Midi synth routines for OPL2/OPL3/OPL4 FM
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

#undef DEBUG_ALLOC
#undef DEBUG_MIDI

#include "opl3_voice.h"
#include <sound/asoundef.h>

extern char snd_opl3_regmap[MAX_OPL2_VOICES][4];

extern bool use_internal_drums;

static void snd_opl3_note_off_unsafe(void *p, int note, int vel,
				     struct snd_midi_channel *chan);

static char opl3_volume_table[128] =
{
	-63, -48, -40, -35, -32, -29, -27, -26,
	-24, -23, -21, -20, -19, -18, -18, -17,
	-16, -15, -15, -14, -13, -13, -12, -12,
	-11, -11, -10, -10, -10, -9, -9, -8,
	-8, -8, -7, -7, -7, -6, -6, -6,
	-5, -5, -5, -5, -4, -4, -4, -4,
	-3, -3, -3, -3, -2, -2, -2, -2,
	-2, -1, -1, -1, -1, 0, 0, 0,
	0, 0, 0, 1, 1, 1, 1, 1,
	1, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 4,
	4, 4, 4, 4, 4, 4, 4, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 8, 8, 8, 8, 8
};

void snd_opl3_calc_volume(unsigned char *volbyte, int vel,
			  struct snd_midi_channel *chan)
{
	int oldvol, newvol, n;
	int volume;

	volume = (vel * chan->gm_volume * chan->gm_expression) / (127*127);
	if (volume > 127)
		volume = 127;

	oldvol = OPL3_TOTAL_LEVEL_MASK - (*volbyte & OPL3_TOTAL_LEVEL_MASK);

	newvol = opl3_volume_table[volume] + oldvol;
	if (newvol > OPL3_TOTAL_LEVEL_MASK)
		newvol = OPL3_TOTAL_LEVEL_MASK;
	else if (newvol < 0)
		newvol = 0;

	n = OPL3_TOTAL_LEVEL_MASK - (newvol & OPL3_TOTAL_LEVEL_MASK);

	*volbyte = (*volbyte & OPL3_KSL_MASK) | (n & OPL3_TOTAL_LEVEL_MASK);
}

static short opl3_note_table[16] =
{
	305, 323,	
	343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647,
	686, 726	
};

static void snd_opl3_calc_pitch(unsigned char *fnum, unsigned char *blocknum,
				int note, struct snd_midi_channel *chan)
{
	int block = ((note / 12) & 0x07) - 1;
	int idx = (note % 12) + 2;
	int freq;

	if (chan->midi_pitchbend) {
		int pitchbend = chan->midi_pitchbend;
		int segment;

		if (pitchbend > 0x1FFF)
			pitchbend = 0x1FFF;

		segment = pitchbend / 0x1000;
		freq = opl3_note_table[idx+segment];
		freq += ((opl3_note_table[idx+segment+1] - freq) *
			 (pitchbend % 0x1000)) / 0x1000;
	} else {
		freq = opl3_note_table[idx];
	}

	*fnum = (unsigned char) freq;
	*blocknum = ((freq >> 8) & OPL3_FNUM_HIGH_MASK) |
		((block << 2) & OPL3_BLOCKNUM_MASK);
}


#ifdef DEBUG_ALLOC
static void debug_alloc(struct snd_opl3 *opl3, char *s, int voice) {
	int i;
	char *str = "x.24";

	printk(KERN_DEBUG "time %.5i: %s [%.2i]: ", opl3->use_time, s, voice);
	for (i = 0; i < opl3->max_voices; i++)
		printk("%c", *(str + opl3->voices[i].state + 1));
	printk("\n");
}
#endif

static int opl3_get_voice(struct snd_opl3 *opl3, int instr_4op,
			  struct snd_midi_channel *chan) {
	int chan_4op_1;		
	int chan_4op_2;		

	struct snd_opl3_voice *vp, *vp2;
	unsigned int voice_time;
	int i;

#ifdef DEBUG_ALLOC
	char *alloc_type[3] = { "FREE     ", "CHEAP    ", "EXPENSIVE" };
#endif

	
	enum {
		FREE = 0, CHEAP, EXPENSIVE, END
	};

	
	struct best {
		unsigned int time;
		int voice;
	} best[END];
	struct best *bp;

	for (i = 0; i < END; i++) {
		best[i].time = (unsigned int)(-1); ;
		best[i].voice = -1;
	}

	
	for (i = 0; i < opl3->max_voices; i++) {
		vp = &opl3->voices[i];

		if (vp->state == SNDRV_OPL3_ST_NOT_AVAIL)
			continue;

		voice_time = vp->time;
		bp = best;

		chan_4op_1 = ((i < 3) || (i > 8 && i < 12));
		chan_4op_2 = ((i > 2 && i < 6) || (i > 11 && i < 15));
		if (instr_4op) {
			
			
			if (!chan_4op_1)
				continue;

			if (vp->state)
				
				bp++;
			vp2 = &opl3->voices[i + 3];
			if (vp2->state == SNDRV_OPL3_ST_ON_2OP) {
				
				bp++;
				voice_time = (voice_time > vp->time) ?
					voice_time : vp->time;
			}
		} else {
			
			if ((chan_4op_1) || (chan_4op_2))
				
				bp++;
			else if (vp->state)
				
				bp++;
			
			if (vp->state)
				bp++;
		}
		if (voice_time < bp->time) {
			bp->time = voice_time;
			bp->voice = i;
		}
	}

	for (i = 0; i < END; i++) {
		if (best[i].voice >= 0) {
#ifdef DEBUG_ALLOC
			printk(KERN_DEBUG "%s %iop allocation on voice %i\n",
			       alloc_type[i], instr_4op ? 4 : 2,
			       best[i].voice);
#endif
			return best[i].voice;
		}
	}
	
	return -1;
}


void snd_opl3_timer_func(unsigned long data)
{

	struct snd_opl3 *opl3 = (struct snd_opl3 *)data;
	unsigned long flags;
	int again = 0;
	int i;

	spin_lock_irqsave(&opl3->voice_lock, flags);
	for (i = 0; i < opl3->max_voices; i++) {
		struct snd_opl3_voice *vp = &opl3->voices[i];
		if (vp->state > 0 && vp->note_off_check) {
			if (vp->note_off == jiffies)
				snd_opl3_note_off_unsafe(opl3, vp->note, 0,
							 vp->chan);
			else
				again++;
		}
	}
	spin_unlock_irqrestore(&opl3->voice_lock, flags);

	spin_lock_irqsave(&opl3->sys_timer_lock, flags);
	if (again) {
		opl3->tlist.expires = jiffies + 1;	
		add_timer(&opl3->tlist);
	} else {
		opl3->sys_timer_status = 0;
	}
	spin_unlock_irqrestore(&opl3->sys_timer_lock, flags);
}

static void snd_opl3_start_timer(struct snd_opl3 *opl3)
{
	unsigned long flags;
	spin_lock_irqsave(&opl3->sys_timer_lock, flags);
	if (! opl3->sys_timer_status) {
		opl3->tlist.expires = jiffies + 1;
		add_timer(&opl3->tlist);
		opl3->sys_timer_status = 1;
	}
	spin_unlock_irqrestore(&opl3->sys_timer_lock, flags);
}



static int snd_opl3_oss_map[MAX_OPL3_VOICES] = {
	0, 1, 2, 9, 10, 11, 6, 7, 8, 15, 16, 17, 3, 4 ,5, 12, 13, 14
};

void snd_opl3_note_on(void *p, int note, int vel, struct snd_midi_channel *chan)
{
	struct snd_opl3 *opl3;
	int instr_4op;

	int voice;
	struct snd_opl3_voice *vp, *vp2;
	unsigned short connect_mask;
	unsigned char connection;
	unsigned char vol_op[4];

	int extra_prg = 0;

	unsigned short reg_side;
	unsigned char op_offset;
	unsigned char voice_offset;
	unsigned short opl3_reg;
	unsigned char reg_val;
	unsigned char prg, bank;

	int key = note;
	unsigned char fnum, blocknum;
	int i;

	struct fm_patch *patch;
	struct fm_instrument *fm;
	unsigned long flags;

	opl3 = p;

#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "Note on, ch %i, inst %i, note %i, vel %i\n",
		   chan->number, chan->midi_program, note, vel);
#endif

	
	
	if (opl3->synth_mode == SNDRV_OPL3_MODE_SEQ) {
		if (chan->drum_channel) {
			
			bank = 128;
			prg = note;
		} else {
			bank = chan->gm_bank_select;
			prg = chan->midi_program;
		}
	} else {
		
		if (chan->number >= MAX_OPL3_VOICES)
			return;

		
		bank = 127;
		prg = chan->midi_program;
	}

	spin_lock_irqsave(&opl3->voice_lock, flags);

	if (use_internal_drums) {
		snd_opl3_drum_switch(opl3, note, vel, 1, chan);
		spin_unlock_irqrestore(&opl3->voice_lock, flags);
		return;
	}

 __extra_prg:
	patch = snd_opl3_find_patch(opl3, prg, bank, 0);
	if (!patch) {
		spin_unlock_irqrestore(&opl3->voice_lock, flags);
		return;
	}

	fm = &patch->inst;
	switch (patch->type) {
	case FM_PATCH_OPL2:
		instr_4op = 0;
		break;
	case FM_PATCH_OPL3:
		if (opl3->hardware >= OPL3_HW_OPL3) {
			instr_4op = 1;
			break;
		}
	default:
		spin_unlock_irqrestore(&opl3->voice_lock, flags);
		return;
	}
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "  --> OPL%i instrument: %s\n",
		   instr_4op ? 3 : 2, patch->name);
#endif
	
	
	if (opl3->synth_mode == SNDRV_OPL3_MODE_SEQ) {
		voice = opl3_get_voice(opl3, instr_4op, chan);
	} else {
		
		voice = snd_opl3_oss_map[chan->number];		
	}

	if (voice < MAX_OPL2_VOICES) {
		
		reg_side = OPL3_LEFT;
		voice_offset = voice;
		connect_mask = (OPL3_LEFT_4OP_0 << voice_offset) & 0x07;
	} else {
		
		reg_side = OPL3_RIGHT;
		voice_offset = voice - MAX_OPL2_VOICES;
		connect_mask = (OPL3_RIGHT_4OP_0 << voice_offset) & 0x38;
	}

	
	vp = &opl3->voices[voice];
	if (vp->state > 0) {
		opl3_reg = reg_side | (OPL3_REG_KEYON_BLOCK + voice_offset);
		reg_val = vp->keyon_reg & ~OPL3_KEYON_BIT;
		opl3->command(opl3, opl3_reg, reg_val);
	}
	if (instr_4op) {
		vp2 = &opl3->voices[voice + 3];
		if (vp->state > 0) {
			opl3_reg = reg_side | (OPL3_REG_KEYON_BLOCK +
					       voice_offset + 3);
			reg_val = vp->keyon_reg & ~OPL3_KEYON_BIT;
			opl3->command(opl3, opl3_reg, reg_val);
		}
	}

	
	if (instr_4op) {
		if ((opl3->connection_reg ^ connect_mask) & connect_mask) {
			opl3->connection_reg |= connect_mask;
			
			opl3_reg = OPL3_RIGHT | OPL3_REG_CONNECTION_SELECT;
			opl3->command(opl3, opl3_reg, opl3->connection_reg);
		}
	} else {
		if ((opl3->connection_reg ^ ~connect_mask) & connect_mask) {
			opl3->connection_reg &= ~connect_mask;
			
			opl3_reg = OPL3_RIGHT | OPL3_REG_CONNECTION_SELECT;
			opl3->command(opl3, opl3_reg, opl3->connection_reg);
		}
	}

#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "  --> setting OPL3 connection: 0x%x\n",
		   opl3->connection_reg);
#endif
	for (i = 0; i < (instr_4op ? 4 : 2); i++)
		vol_op[i] = fm->op[i].ksl_level;

	connection = fm->feedback_connection[0] & 0x01;
	if (instr_4op) {
		connection <<= 1;
		connection |= fm->feedback_connection[1] & 0x01;

		snd_opl3_calc_volume(&vol_op[3], vel, chan);
		switch (connection) {
		case 0x03:
			snd_opl3_calc_volume(&vol_op[2], vel, chan);
			
		case 0x02:
			snd_opl3_calc_volume(&vol_op[0], vel, chan);
			break;
		case 0x01:
			snd_opl3_calc_volume(&vol_op[1], vel, chan);
		}
	} else {
		snd_opl3_calc_volume(&vol_op[1], vel, chan);
		if (connection)
			snd_opl3_calc_volume(&vol_op[0], vel, chan);
	}

	
	for (i = 0; i < (instr_4op ? 4 : 2); i++) {
#ifdef DEBUG_MIDI
		snd_printk(KERN_DEBUG "  --> programming operator %i\n", i);
#endif
		op_offset = snd_opl3_regmap[voice_offset][i];

		 
		reg_val = fm->op[i].am_vib;
		opl3_reg = reg_side | (OPL3_REG_AM_VIB + op_offset);
		opl3->command(opl3, opl3_reg, reg_val);

		 
		reg_val = vol_op[i];
		opl3_reg = reg_side | (OPL3_REG_KSL_LEVEL + op_offset);
		opl3->command(opl3, opl3_reg, reg_val);

		 
		reg_val = fm->op[i].attack_decay;
		opl3_reg = reg_side | (OPL3_REG_ATTACK_DECAY + op_offset);
		opl3->command(opl3, opl3_reg, reg_val);

		 
		reg_val = fm->op[i].sustain_release;
		opl3_reg = reg_side | (OPL3_REG_SUSTAIN_RELEASE + op_offset);
		opl3->command(opl3, opl3_reg, reg_val);

		
		reg_val = fm->op[i].wave_select;
		opl3_reg = reg_side | (OPL3_REG_WAVE_SELECT + op_offset);
		opl3->command(opl3, opl3_reg, reg_val);
	}

	
	reg_val = fm->feedback_connection[0];
	
	reg_val |= OPL3_STEREO_BITS;
	if (chan->gm_pan < 43)
		reg_val &= ~OPL3_VOICE_TO_RIGHT;
	if (chan->gm_pan > 85)
		reg_val &= ~OPL3_VOICE_TO_LEFT;
	opl3_reg = reg_side | (OPL3_REG_FEEDBACK_CONNECTION + voice_offset);
	opl3->command(opl3, opl3_reg, reg_val);

	if (instr_4op) {
		
		reg_val = fm->feedback_connection[1] & OPL3_CONNECTION_BIT;
		
		reg_val |= OPL3_STEREO_BITS;
		if (chan->gm_pan < 43)
			reg_val &= ~OPL3_VOICE_TO_RIGHT;
		if (chan->gm_pan > 85)
			reg_val &= ~OPL3_VOICE_TO_LEFT;
		opl3_reg = reg_side | (OPL3_REG_FEEDBACK_CONNECTION +
				       voice_offset + 3);
		opl3->command(opl3, opl3_reg, reg_val);
	}

	if (fm->fix_key)
		note = fm->fix_key;
	if (fm->trnsps)
		note += (fm->trnsps - 64);

	snd_opl3_calc_pitch(&fnum, &blocknum, note, chan);

	
	opl3_reg = reg_side | (OPL3_REG_FNUM_LOW + voice_offset);
	opl3->command(opl3, opl3_reg, fnum);

	opl3->voices[voice].keyon_reg = blocknum;

	
	blocknum |= OPL3_KEYON_BIT;

#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "  --> trigger voice %i\n", voice);
#endif
	 
	opl3_reg = reg_side | (OPL3_REG_KEYON_BLOCK + voice_offset);
	opl3->command(opl3, opl3_reg, blocknum);

	
	if (fm->fix_dur) {
		opl3->voices[voice].note_off = jiffies +
			(fm->fix_dur * HZ) / 100;
		snd_opl3_start_timer(opl3);
		opl3->voices[voice].note_off_check = 1;
	} else
		opl3->voices[voice].note_off_check = 0;

	
	extra_prg = (extra_prg) ? 0 : fm->modes;

	
	vp->time = opl3->use_time++;
	vp->note = key;
	vp->chan = chan;

	if (instr_4op) {
		vp->state = SNDRV_OPL3_ST_ON_4OP;

		vp2 = &opl3->voices[voice + 3];
		vp2->time = opl3->use_time++;
		vp2->note = key;
		vp2->chan = chan;
		vp2->state = SNDRV_OPL3_ST_NOT_AVAIL;
	} else {
		if (vp->state == SNDRV_OPL3_ST_ON_4OP) {
			
			vp2 = &opl3->voices[voice + 3];
			vp2->time = opl3->use_time++;
			vp2->state = SNDRV_OPL3_ST_OFF;
		}
		vp->state = SNDRV_OPL3_ST_ON_2OP;
	}

#ifdef DEBUG_ALLOC
	debug_alloc(opl3, "note on ", voice);
#endif

	
	if (extra_prg) {
		if (extra_prg > 128) {
			bank = 128;
			
			prg = extra_prg - 128 + 35 - 1;
		} else {
			bank = 0;
			prg = extra_prg - 1;
		}
#ifdef DEBUG_MIDI
		snd_printk(KERN_DEBUG " *** allocating extra program\n");
#endif
		goto __extra_prg;
	}
	spin_unlock_irqrestore(&opl3->voice_lock, flags);
}

static void snd_opl3_kill_voice(struct snd_opl3 *opl3, int voice)
{
	unsigned short reg_side;
	unsigned char voice_offset;
	unsigned short opl3_reg;

	struct snd_opl3_voice *vp, *vp2;

	if (snd_BUG_ON(voice >= MAX_OPL3_VOICES))
		return;

	vp = &opl3->voices[voice];
	if (voice < MAX_OPL2_VOICES) {
		
		reg_side = OPL3_LEFT;
		voice_offset = voice;
	} else {
		
		reg_side = OPL3_RIGHT;
		voice_offset = voice - MAX_OPL2_VOICES;
	}

	
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "  --> kill voice %i\n", voice);
#endif
	opl3_reg = reg_side | (OPL3_REG_KEYON_BLOCK + voice_offset);
	
	opl3->command(opl3, opl3_reg, vp->keyon_reg);

	
	vp->time = opl3->use_time++;

	if (vp->state == SNDRV_OPL3_ST_ON_4OP) {
		vp2 = &opl3->voices[voice + 3];

		vp2->time = opl3->use_time++;
		vp2->state = SNDRV_OPL3_ST_OFF;
	}
	vp->state = SNDRV_OPL3_ST_OFF;
#ifdef DEBUG_ALLOC
	debug_alloc(opl3, "note off", voice);
#endif

}

static void snd_opl3_note_off_unsafe(void *p, int note, int vel,
				     struct snd_midi_channel *chan)
{
  	struct snd_opl3 *opl3;

	int voice;
	struct snd_opl3_voice *vp;

	opl3 = p;

#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "Note off, ch %i, inst %i, note %i\n",
		   chan->number, chan->midi_program, note);
#endif

	if (opl3->synth_mode == SNDRV_OPL3_MODE_SEQ) {
		if (chan->drum_channel && use_internal_drums) {
			snd_opl3_drum_switch(opl3, note, vel, 0, chan);
			return;
		}
		for (voice = 0; voice < opl3->max_voices; voice++) {
			vp = &opl3->voices[voice];
			if (vp->state > 0 && vp->chan == chan && vp->note == note) {
				snd_opl3_kill_voice(opl3, voice);
			}
		}
	} else {
		
		if (chan->number < MAX_OPL3_VOICES) {
			voice = snd_opl3_oss_map[chan->number];		
			snd_opl3_kill_voice(opl3, voice);
		}
	}
}

void snd_opl3_note_off(void *p, int note, int vel,
		       struct snd_midi_channel *chan)
{
	struct snd_opl3 *opl3 = p;
	unsigned long flags;

	spin_lock_irqsave(&opl3->voice_lock, flags);
	snd_opl3_note_off_unsafe(p, note, vel, chan);
	spin_unlock_irqrestore(&opl3->voice_lock, flags);
}

void snd_opl3_key_press(void *p, int note, int vel, struct snd_midi_channel *chan)
{
  	struct snd_opl3 *opl3;

	opl3 = p;
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "Key pressure, ch#: %i, inst#: %i\n",
		   chan->number, chan->midi_program);
#endif
}

void snd_opl3_terminate_note(void *p, int note, struct snd_midi_channel *chan)
{
  	struct snd_opl3 *opl3;

	opl3 = p;
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "Terminate note, ch#: %i, inst#: %i\n",
		   chan->number, chan->midi_program);
#endif
}

static void snd_opl3_update_pitch(struct snd_opl3 *opl3, int voice)
{
	unsigned short reg_side;
	unsigned char voice_offset;
	unsigned short opl3_reg;

	unsigned char fnum, blocknum;

	struct snd_opl3_voice *vp;

	if (snd_BUG_ON(voice >= MAX_OPL3_VOICES))
		return;

	vp = &opl3->voices[voice];
	if (vp->chan == NULL)
		return; 

	if (voice < MAX_OPL2_VOICES) {
		
		reg_side = OPL3_LEFT;
		voice_offset = voice;
	} else {
		
		reg_side = OPL3_RIGHT;
		voice_offset = voice - MAX_OPL2_VOICES;
	}

	snd_opl3_calc_pitch(&fnum, &blocknum, vp->note, vp->chan);

	
	opl3_reg = reg_side | (OPL3_REG_FNUM_LOW + voice_offset);
	opl3->command(opl3, opl3_reg, fnum);

	vp->keyon_reg = blocknum;

	
	blocknum |= OPL3_KEYON_BIT;

	 
	opl3_reg = reg_side | (OPL3_REG_KEYON_BLOCK + voice_offset);
	opl3->command(opl3, opl3_reg, blocknum);

	vp->time = opl3->use_time++;
}

static void snd_opl3_pitch_ctrl(struct snd_opl3 *opl3, struct snd_midi_channel *chan)
{
	int voice;
	struct snd_opl3_voice *vp;

	unsigned long flags;

	spin_lock_irqsave(&opl3->voice_lock, flags);

	if (opl3->synth_mode == SNDRV_OPL3_MODE_SEQ) {
		for (voice = 0; voice < opl3->max_voices; voice++) {
			vp = &opl3->voices[voice];
			if (vp->state > 0 && vp->chan == chan) {
				snd_opl3_update_pitch(opl3, voice);
			}
		}
	} else {
		
		if (chan->number < MAX_OPL3_VOICES) {
			voice = snd_opl3_oss_map[chan->number];		
			snd_opl3_update_pitch(opl3, voice);
		}
	}
	spin_unlock_irqrestore(&opl3->voice_lock, flags);
}

void snd_opl3_control(void *p, int type, struct snd_midi_channel *chan)
{
  	struct snd_opl3 *opl3;

	opl3 = p;
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "Controller, TYPE = %i, ch#: %i, inst#: %i\n",
		   type, chan->number, chan->midi_program);
#endif

	switch (type) {
	case MIDI_CTL_MSB_MODWHEEL:
		if (chan->control[MIDI_CTL_MSB_MODWHEEL] > 63)
			opl3->drum_reg |= OPL3_VIBRATO_DEPTH;
		else 
			opl3->drum_reg &= ~OPL3_VIBRATO_DEPTH;
		opl3->command(opl3, OPL3_LEFT | OPL3_REG_PERCUSSION,
				 opl3->drum_reg);
		break;
	case MIDI_CTL_E2_TREMOLO_DEPTH:
		if (chan->control[MIDI_CTL_E2_TREMOLO_DEPTH] > 63)
			opl3->drum_reg |= OPL3_TREMOLO_DEPTH;
		else 
			opl3->drum_reg &= ~OPL3_TREMOLO_DEPTH;
		opl3->command(opl3, OPL3_LEFT | OPL3_REG_PERCUSSION,
				 opl3->drum_reg);
		break;
	case MIDI_CTL_PITCHBEND:
		snd_opl3_pitch_ctrl(opl3, chan);
		break;
	}
}

void snd_opl3_nrpn(void *p, struct snd_midi_channel *chan,
		   struct snd_midi_channel_set *chset)
{
  	struct snd_opl3 *opl3;

	opl3 = p;
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "NRPN, ch#: %i, inst#: %i\n",
		   chan->number, chan->midi_program);
#endif
}

void snd_opl3_sysex(void *p, unsigned char *buf, int len,
		    int parsed, struct snd_midi_channel_set *chset)
{
  	struct snd_opl3 *opl3;

	opl3 = p;
#ifdef DEBUG_MIDI
	snd_printk(KERN_DEBUG "SYSEX\n");
#endif
}
