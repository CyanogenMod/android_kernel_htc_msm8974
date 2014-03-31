/*
 *  cx18 ADEC audio functions
 *
 *  Derived from cx25840-audio.c
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *  Copyright (C) 2008  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "cx18-driver.h"

static int set_audclk_freq(struct cx18 *cx, u32 freq)
{
	struct cx18_av_state *state = &cx->av_state;

	if (freq != 32000 && freq != 44100 && freq != 48000)
		return -EINVAL;


	if (state->aud_input > CX18_AV_AUDIO_SERIAL2) {
		switch (freq) {
		case 32000:
			cx18_av_write4(cx, 0x108, 0x200d040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x0176740c);

			
			
			cx18_av_write4(cx, 0x900, 0x0801f77f);
			cx18_av_write4(cx, 0x904, 0x0801f77f);
			cx18_av_write4(cx, 0x90c, 0x0801f77f);

			
			cx18_av_write(cx, 0x127, 0x60);

			
			cx18_av_write4(cx, 0x12c, 0x11202fff);

			cx18_av_write4(cx, 0x128, 0xa00d2ef8);
			break;

		case 44100:
			cx18_av_write4(cx, 0x108, 0x180e040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x0062a1f2);

			
			
			cx18_av_write4(cx, 0x900, 0x08016d59);
			cx18_av_write4(cx, 0x904, 0x08016d59);
			cx18_av_write4(cx, 0x90c, 0x08016d59);

			
			cx18_av_write(cx, 0x127, 0x58);

			
			cx18_av_write4(cx, 0x12c, 0x112092ff);

			cx18_av_write4(cx, 0x128, 0xa01d4bf8);
			break;

		case 48000:
			cx18_av_write4(cx, 0x108, 0x160e040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x005227ad);

			
			
			cx18_av_write4(cx, 0x900, 0x08014faa);
			cx18_av_write4(cx, 0x904, 0x08014faa);
			cx18_av_write4(cx, 0x90c, 0x08014faa);

			
			cx18_av_write(cx, 0x127, 0x56);

			
			cx18_av_write4(cx, 0x12c, 0x11205fff);

			cx18_av_write4(cx, 0x128, 0xa01193f8);
			break;
		}
	} else {
		switch (freq) {
		case 32000:
			cx18_av_write4(cx, 0x108, 0x300d040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x0176740c);

			
			
			cx18_av_write4(cx, 0x8f8, 0x08010000);

			
			
			cx18_av_write4(cx, 0x900, 0x08020000);
			cx18_av_write4(cx, 0x904, 0x08020000);
			cx18_av_write4(cx, 0x90c, 0x08020000);

			
			cx18_av_write(cx, 0x127, 0x70);

			
			cx18_av_write4(cx, 0x12c, 0x11201fff);

			cx18_av_write4(cx, 0x128, 0xa00d2ef8);
			break;

		case 44100:
			cx18_av_write4(cx, 0x108, 0x240e040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x0062a1f2);

			
			
			cx18_av_write4(cx, 0x8f8, 0x080160cd);

			
			
			cx18_av_write4(cx, 0x900, 0x08017385);
			cx18_av_write4(cx, 0x904, 0x08017385);
			cx18_av_write4(cx, 0x90c, 0x08017385);

			
			cx18_av_write(cx, 0x127, 0x64);

			
			cx18_av_write4(cx, 0x12c, 0x112061ff);

			cx18_av_write4(cx, 0x128, 0xa01d4bf8);
			break;

		case 48000:
			cx18_av_write4(cx, 0x108, 0x200d040f);

			
			
			cx18_av_write4(cx, 0x10c, 0x002be2fe);

			
			
			cx18_av_write4(cx, 0x110, 0x0176740c);

			
			
			cx18_av_write4(cx, 0x8f8, 0x08018000);

			
			
			cx18_av_write4(cx, 0x900, 0x08015555);
			cx18_av_write4(cx, 0x904, 0x08015555);
			cx18_av_write4(cx, 0x90c, 0x08015555);

			
			cx18_av_write(cx, 0x127, 0x60);

			
			cx18_av_write4(cx, 0x12c, 0x11203fff);

			cx18_av_write4(cx, 0x128, 0xa01193f8);
			break;
		}
	}

	state->audclk_freq = freq;

	return 0;
}

void cx18_av_audio_set_path(struct cx18 *cx)
{
	struct cx18_av_state *state = &cx->av_state;
	u8 v;

	
	v = cx18_av_read(cx, 0x803) & ~0x10;
	cx18_av_write_expect(cx, 0x803, v, v, 0x1f);

	
	v = cx18_av_read(cx, 0x810) | 0x01;
	cx18_av_write_expect(cx, 0x810, v, v, 0x0f);

	
	cx18_av_write(cx, 0x8d3, 0x1f);

	if (state->aud_input <= CX18_AV_AUDIO_SERIAL2) {
		
		cx18_av_write4(cx, 0x8d0, 0x01011012);

	} else {
		
		cx18_av_write4(cx, 0x8d0, 0x1f063870);
	}

	set_audclk_freq(cx, state->audclk_freq);

	
	v = cx18_av_read(cx, 0x810) & ~0x01;
	cx18_av_write_expect(cx, 0x810, v, v, 0x0f);

	if (state->aud_input > CX18_AV_AUDIO_SERIAL2) {
		v = cx18_av_read(cx, 0x803) | 0x10;
		cx18_av_write_expect(cx, 0x803, v, v, 0x1f);
	}
}

static void set_volume(struct cx18 *cx, int volume)
{
	
	int vol = volume >> 9;
	if (vol <= 23)
		vol = 0;
	else
		vol -= 23;

	
	cx18_av_write(cx, 0x8d4, 228 - (vol * 2));
}

static void set_bass(struct cx18 *cx, int bass)
{
	
	cx18_av_and_or(cx, 0x8d9, ~0x3f, 48 - (bass * 48 / 0xffff));
}

static void set_treble(struct cx18 *cx, int treble)
{
	
	cx18_av_and_or(cx, 0x8db, ~0x3f, 48 - (treble * 48 / 0xffff));
}

static void set_balance(struct cx18 *cx, int balance)
{
	int bal = balance >> 8;
	if (bal > 0x80) {
		
		cx18_av_and_or(cx, 0x8d5, 0x7f, 0x80);
		
		cx18_av_and_or(cx, 0x8d5, ~0x7f, bal & 0x7f);
	} else {
		
		cx18_av_and_or(cx, 0x8d5, 0x7f, 0x00);
		
		cx18_av_and_or(cx, 0x8d5, ~0x7f, 0x80 - bal);
	}
}

static void set_mute(struct cx18 *cx, int mute)
{
	struct cx18_av_state *state = &cx->av_state;
	u8 v;

	if (state->aud_input > CX18_AV_AUDIO_SERIAL2) {
		v = cx18_av_read(cx, 0x803);
		if (mute) {
			
			v &= ~0x10;
			cx18_av_write_expect(cx, 0x803, v, v, 0x1f);
			cx18_av_write(cx, 0x8d3, 0x1f);
		} else {
			
			v |= 0x10;
			cx18_av_write_expect(cx, 0x803, v, v, 0x1f);
		}
	} else {
		
		cx18_av_and_or(cx, 0x8d3, ~0x2, mute ? 0x02 : 0x00);
	}
}

int cx18_av_s_clock_freq(struct v4l2_subdev *sd, u32 freq)
{
	struct cx18 *cx = v4l2_get_subdevdata(sd);
	struct cx18_av_state *state = &cx->av_state;
	int retval;
	u8 v;

	if (state->aud_input > CX18_AV_AUDIO_SERIAL2) {
		v = cx18_av_read(cx, 0x803) & ~0x10;
		cx18_av_write_expect(cx, 0x803, v, v, 0x1f);
		cx18_av_write(cx, 0x8d3, 0x1f);
	}
	v = cx18_av_read(cx, 0x810) | 0x1;
	cx18_av_write_expect(cx, 0x810, v, v, 0x0f);

	retval = set_audclk_freq(cx, freq);

	v = cx18_av_read(cx, 0x810) & ~0x1;
	cx18_av_write_expect(cx, 0x810, v, v, 0x0f);
	if (state->aud_input > CX18_AV_AUDIO_SERIAL2) {
		v = cx18_av_read(cx, 0x803) | 0x10;
		cx18_av_write_expect(cx, 0x803, v, v, 0x1f);
	}
	return retval;
}

static int cx18_av_audio_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct cx18 *cx = v4l2_get_subdevdata(sd);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		set_volume(cx, ctrl->val);
		break;
	case V4L2_CID_AUDIO_BASS:
		set_bass(cx, ctrl->val);
		break;
	case V4L2_CID_AUDIO_TREBLE:
		set_treble(cx, ctrl->val);
		break;
	case V4L2_CID_AUDIO_BALANCE:
		set_balance(cx, ctrl->val);
		break;
	case V4L2_CID_AUDIO_MUTE:
		set_mute(cx, ctrl->val);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

const struct v4l2_ctrl_ops cx18_av_audio_ctrl_ops = {
	.s_ctrl = cx18_av_audio_s_ctrl,
};
