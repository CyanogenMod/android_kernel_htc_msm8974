/*
 * Functions for auto gain.
 *
 * Copyright (C) 2010-2011 Hans de Goede <hdegoede@redhat.com>
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

static inline int auto_gain_n_exposure(
			struct gspca_dev *gspca_dev,
			int avg_lum,
			int desired_avg_lum,
			int deadzone,
			int gain_knee,
			int exposure_knee)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i, steps, gain, orig_gain, exposure, orig_exposure;
	int retval = 0;

	orig_gain = gain = sd->ctrls[GAIN].val;
	orig_exposure = exposure = sd->ctrls[EXPOSURE].val;

	steps = abs(desired_avg_lum - avg_lum) / deadzone;

	PDEBUG(D_FRAM, "autogain: lum: %d, desired: %d, steps: %d",
		avg_lum, desired_avg_lum, steps);

	for (i = 0; i < steps; i++) {
		if (avg_lum > desired_avg_lum) {
			if (gain > gain_knee)
				gain--;
			else if (exposure > exposure_knee)
				exposure--;
			else if (gain > sd->ctrls[GAIN].def)
				gain--;
			else if (exposure > sd->ctrls[EXPOSURE].min)
				exposure--;
			else if (gain > sd->ctrls[GAIN].min)
				gain--;
			else
				break;
		} else {
			if (gain < sd->ctrls[GAIN].def)
				gain++;
			else if (exposure < exposure_knee)
				exposure++;
			else if (gain < gain_knee)
				gain++;
			else if (exposure < sd->ctrls[EXPOSURE].max)
				exposure++;
			else if (gain < sd->ctrls[GAIN].max)
				gain++;
			else
				break;
		}
	}

	if (gain != orig_gain) {
		sd->ctrls[GAIN].val = gain;
		setgain(gspca_dev);
		retval = 1;
	}
	if (exposure != orig_exposure) {
		sd->ctrls[EXPOSURE].val = exposure;
		setexposure(gspca_dev);
		retval = 1;
	}

	if (retval)
		PDEBUG(D_FRAM, "autogain: changed gain: %d, expo: %d",
			gain, exposure);
	return retval;
}

static inline int coarse_grained_expo_autogain(
			struct gspca_dev *gspca_dev,
			int avg_lum,
			int desired_avg_lum,
			int deadzone)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int steps, gain, orig_gain, exposure, orig_exposure;
	int gain_low, gain_high;
	int retval = 0;

	orig_gain = gain = sd->ctrls[GAIN].val;
	orig_exposure = exposure = sd->ctrls[EXPOSURE].val;

	gain_low = (sd->ctrls[GAIN].max - sd->ctrls[GAIN].min) / 5 * 2;
	gain_low += sd->ctrls[GAIN].min;
	gain_high = (sd->ctrls[GAIN].max - sd->ctrls[GAIN].min) / 5 * 4;
	gain_high += sd->ctrls[GAIN].min;

	steps = (desired_avg_lum - avg_lum) / deadzone;

	PDEBUG(D_FRAM, "autogain: lum: %d, desired: %d, steps: %d",
		avg_lum, desired_avg_lum, steps);

	if ((gain + steps) > gain_high &&
	    exposure < sd->ctrls[EXPOSURE].max) {
		gain = gain_high;
		sd->exp_too_low_cnt++;
		sd->exp_too_high_cnt = 0;
	} else if ((gain + steps) < gain_low &&
		   exposure > sd->ctrls[EXPOSURE].min) {
		gain = gain_low;
		sd->exp_too_high_cnt++;
		sd->exp_too_low_cnt = 0;
	} else {
		gain += steps;
		if (gain > sd->ctrls[GAIN].max)
			gain = sd->ctrls[GAIN].max;
		else if (gain < sd->ctrls[GAIN].min)
			gain = sd->ctrls[GAIN].min;
		sd->exp_too_high_cnt = 0;
		sd->exp_too_low_cnt = 0;
	}

	if (sd->exp_too_high_cnt > 3) {
		exposure--;
		sd->exp_too_high_cnt = 0;
	} else if (sd->exp_too_low_cnt > 3) {
		exposure++;
		sd->exp_too_low_cnt = 0;
	}

	if (gain != orig_gain) {
		sd->ctrls[GAIN].val = gain;
		setgain(gspca_dev);
		retval = 1;
	}
	if (exposure != orig_exposure) {
		sd->ctrls[EXPOSURE].val = exposure;
		setexposure(gspca_dev);
		retval = 1;
	}

	if (retval)
		PDEBUG(D_FRAM, "autogain: changed gain: %d, expo: %d",
			gain, exposure);
	return retval;
}
