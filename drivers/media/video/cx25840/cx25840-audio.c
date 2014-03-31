/* cx25840 audio functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <media/v4l2-common.h>
#include <media/cx25840.h>

#include "cx25840-core.h"


static int cx25840_set_audclk_freq(struct i2c_client *client, u32 freq)
{
	struct cx25840_state *state = to_state(i2c_get_clientdata(client));

	if (state->aud_input != CX25840_AUDIO_SERIAL) {
		switch (freq) {
		case 32000:
			cx25840_write4(client, 0x108, 0x1006040f);


			cx25840_write4(client, 0x110, 0x01bb39ee);

			cx25840_write(client, 0x127, 0x50);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x900, 0x0801f77f);
			cx25840_write4(client, 0x904, 0x0801f77f);
			cx25840_write4(client, 0x90c, 0x0801f77f);
			break;

		case 44100:
			cx25840_write4(client, 0x108, 0x1009040f);


			cx25840_write4(client, 0x110, 0x00ec6bd6);

			cx25840_write(client, 0x127, 0x50);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x900, 0x08016d59);
			cx25840_write4(client, 0x904, 0x08016d59);
			cx25840_write4(client, 0x90c, 0x08016d59);
			break;

		case 48000:
			cx25840_write4(client, 0x108, 0x100a040f);


			cx25840_write4(client, 0x110, 0x0098d6e5);

			cx25840_write(client, 0x127, 0x50);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x900, 0x08014faa);
			cx25840_write4(client, 0x904, 0x08014faa);
			cx25840_write4(client, 0x90c, 0x08014faa);
			break;
		}
	} else {
		switch (freq) {
		case 32000:
			cx25840_write4(client, 0x108, 0x1e08040f);


			cx25840_write4(client, 0x110, 0x012a0869);

			cx25840_write(client, 0x127, 0x54);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x8f8, 0x08010000);

			
			
			cx25840_write4(client, 0x900, 0x08020000);
			cx25840_write4(client, 0x904, 0x08020000);
			cx25840_write4(client, 0x90c, 0x08020000);
			break;

		case 44100:
			cx25840_write4(client, 0x108, 0x1809040f);


			cx25840_write4(client, 0x110, 0x00ec6bd6);

			cx25840_write(client, 0x127, 0x50);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x8f8, 0x080160cd);

			
			
			cx25840_write4(client, 0x900, 0x08017385);
			cx25840_write4(client, 0x904, 0x08017385);
			cx25840_write4(client, 0x90c, 0x08017385);
			break;

		case 48000:
			cx25840_write4(client, 0x108, 0x180a040f);


			cx25840_write4(client, 0x110, 0x0098d6e5);

			cx25840_write(client, 0x127, 0x50);

			if (is_cx2583x(state))
				break;

			
			
			cx25840_write4(client, 0x8f8, 0x08018000);

			
			
			cx25840_write4(client, 0x900, 0x08015555);
			cx25840_write4(client, 0x904, 0x08015555);
			cx25840_write4(client, 0x90c, 0x08015555);
			break;
		}
	}

	state->audclk_freq = freq;

	return 0;
}

static inline int cx25836_set_audclk_freq(struct i2c_client *client, u32 freq)
{
	return cx25840_set_audclk_freq(client, freq);
}

static int cx23885_set_audclk_freq(struct i2c_client *client, u32 freq)
{
	struct cx25840_state *state = to_state(i2c_get_clientdata(client));

	if (state->aud_input != CX25840_AUDIO_SERIAL) {
		switch (freq) {
		case 32000:
		case 44100:
		case 48000:
			
			break;
		}
	} else {
		switch (freq) {
		case 32000:
		case 44100:
			
			break;

		case 48000:
			
			
			cx25840_write4(client, 0x8f8, 0x0801867c);

			
			
			cx25840_write4(client, 0x900, 0x08014faa);
			cx25840_write4(client, 0x904, 0x08014faa);
			cx25840_write4(client, 0x90c, 0x08014faa);
			break;
		}
	}

	state->audclk_freq = freq;

	return 0;
}

static int cx231xx_set_audclk_freq(struct i2c_client *client, u32 freq)
{
	struct cx25840_state *state = to_state(i2c_get_clientdata(client));

	if (state->aud_input != CX25840_AUDIO_SERIAL) {
		switch (freq) {
		case 32000:
			
			
			cx25840_write4(client, 0x900, 0x0801f77f);
			cx25840_write4(client, 0x904, 0x0801f77f);
			cx25840_write4(client, 0x90c, 0x0801f77f);
			break;

		case 44100:
			
			
			cx25840_write4(client, 0x900, 0x08016d59);
			cx25840_write4(client, 0x904, 0x08016d59);
			cx25840_write4(client, 0x90c, 0x08016d59);
			break;

		case 48000:
			
			
			cx25840_write4(client, 0x900, 0x08014faa);
			cx25840_write4(client, 0x904, 0x08014faa);
			cx25840_write4(client, 0x90c, 0x08014faa);
			break;
		}
	} else {
		switch (freq) {
		
		case 32000:
			
			
			cx25840_write4(client, 0x8f8, 0x08010000);

			
			
			cx25840_write4(client, 0x900, 0x08020000);
			cx25840_write4(client, 0x904, 0x08020000);
			cx25840_write4(client, 0x90c, 0x08020000);
			break;

		case 44100:
			
			
			cx25840_write4(client, 0x8f8, 0x080160cd);

			
			
			cx25840_write4(client, 0x900, 0x08017385);
			cx25840_write4(client, 0x904, 0x08017385);
			cx25840_write4(client, 0x90c, 0x08017385);
			break;

		case 48000:
			
			
			cx25840_write4(client, 0x8f8, 0x0801867c);

			
			
			cx25840_write4(client, 0x900, 0x08014faa);
			cx25840_write4(client, 0x904, 0x08014faa);
			cx25840_write4(client, 0x90c, 0x08014faa);
			break;
		}
	}

	state->audclk_freq = freq;

	return 0;
}

static int set_audclk_freq(struct i2c_client *client, u32 freq)
{
	struct cx25840_state *state = to_state(i2c_get_clientdata(client));

	if (freq != 32000 && freq != 44100 && freq != 48000)
		return -EINVAL;

	if (is_cx231xx(state))
		return cx231xx_set_audclk_freq(client, freq);

	if (is_cx2388x(state))
		return cx23885_set_audclk_freq(client, freq);

	if (is_cx2583x(state))
		return cx25836_set_audclk_freq(client, freq);

	return cx25840_set_audclk_freq(client, freq);
}

void cx25840_audio_set_path(struct i2c_client *client)
{
	struct cx25840_state *state = to_state(i2c_get_clientdata(client));

	if (!is_cx2583x(state)) {
		
		cx25840_and_or(client, 0x810, ~0x1, 0x01);

		
		cx25840_and_or(client, 0x803, ~0x10, 0);

		
		cx25840_write(client, 0x8d3, 0x1f);

		if (state->aud_input == CX25840_AUDIO_SERIAL) {
			
			cx25840_write4(client, 0x8d0, 0x01011012);

		} else {
			
			cx25840_write4(client, 0x8d0, 0x1f063870);
		}
	}

	set_audclk_freq(client, state->audclk_freq);

	if (!is_cx2583x(state)) {
		if (state->aud_input != CX25840_AUDIO_SERIAL) {
			cx25840_and_or(client, 0x803, ~0x10, 0x10);
		}

		
		cx25840_and_or(client, 0x810, ~0x1, 0x00);

		
		if (is_cx2388x(state) || is_cx231xx(state))
			cx25840_and_or(client, 0x803, ~0x10, 0x10);
	}
}

static void set_volume(struct i2c_client *client, int volume)
{
	int vol;

	
	vol = volume >> 9;

	if (vol <= 23) {
		vol = 0;
	} else {
		vol -= 23;
	}

	
	cx25840_write(client, 0x8d4, 228 - (vol * 2));
}

static void set_balance(struct i2c_client *client, int balance)
{
	int bal = balance >> 8;
	if (bal > 0x80) {
		
		cx25840_and_or(client, 0x8d5, 0x7f, 0x80);
		
		cx25840_and_or(client, 0x8d5, ~0x7f, bal & 0x7f);
	} else {
		
		cx25840_and_or(client, 0x8d5, 0x7f, 0x00);
		
		cx25840_and_or(client, 0x8d5, ~0x7f, 0x80 - bal);
	}
}

int cx25840_s_clock_freq(struct v4l2_subdev *sd, u32 freq)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct cx25840_state *state = to_state(sd);
	int retval;

	if (!is_cx2583x(state))
		cx25840_and_or(client, 0x810, ~0x1, 1);
	if (state->aud_input != CX25840_AUDIO_SERIAL) {
		cx25840_and_or(client, 0x803, ~0x10, 0);
		cx25840_write(client, 0x8d3, 0x1f);
	}
	retval = set_audclk_freq(client, freq);
	if (state->aud_input != CX25840_AUDIO_SERIAL)
		cx25840_and_or(client, 0x803, ~0x10, 0x10);
	if (!is_cx2583x(state))
		cx25840_and_or(client, 0x810, ~0x1, 0);
	return retval;
}

static int cx25840_audio_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct cx25840_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		if (state->mute->val)
			set_volume(client, 0);
		else
			set_volume(client, state->volume->val);
		break;
	case V4L2_CID_AUDIO_BASS:
		
		cx25840_and_or(client, 0x8d9, ~0x3f,
					48 - (ctrl->val * 48 / 0xffff));
		break;
	case V4L2_CID_AUDIO_TREBLE:
		
		cx25840_and_or(client, 0x8db, ~0x3f,
					48 - (ctrl->val * 48 / 0xffff));
		break;
	case V4L2_CID_AUDIO_BALANCE:
		set_balance(client, ctrl->val);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

const struct v4l2_ctrl_ops cx25840_audio_ctrl_ops = {
	.s_ctrl = cx25840_audio_s_ctrl,
};
