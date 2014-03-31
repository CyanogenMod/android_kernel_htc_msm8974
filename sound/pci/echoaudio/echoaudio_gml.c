/****************************************************************************

   Copyright Echo Digital Audio Corporation (c) 1998 - 2004
   All rights reserved
   www.echoaudio.com

   This file is part of Echo Digital Audio's generic driver library.

   Echo Digital Audio's generic driver library is free software;
   you can redistribute it and/or modify it under the terms of
   the GNU General Public License as published by the Free Software
   Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA  02111-1307, USA.

   *************************************************************************

 Translation from C++ and adaptation for use in ALSA-Driver
 were made by Giuliano Pochini <pochini@shiny.it>

****************************************************************************/




static int check_asic_status(struct echoaudio *chip)
{
	u32 asic_status;

	send_vector(chip, DSP_VC_TEST_ASIC);

	if (read_dsp(chip, &asic_status) < 0) {
		DE_INIT(("check_asic_status: failed on read_dsp\n"));
		chip->asic_loaded = FALSE;
		return -EIO;
	}

	chip->asic_loaded = (asic_status == ASIC_ALREADY_LOADED);
	return chip->asic_loaded ? 0 : -EIO;
}



static int write_control_reg(struct echoaudio *chip, u32 value, char force)
{
	
	if (chip->digital_in_automute)
		value |= GML_DIGITAL_IN_AUTO_MUTE;
	else
		value &= ~GML_DIGITAL_IN_AUTO_MUTE;

	DE_ACT(("write_control_reg: 0x%x\n", value));

	
	value = cpu_to_le32(value);
	if (value != chip->comm_page->control_register || force) {
		if (wait_handshake(chip))
			return -EIO;
		chip->comm_page->control_register = value;
		clear_handshake(chip);
		return send_vector(chip, DSP_VC_WRITE_CONTROL_REG);
	}
	return 0;
}



static int set_input_auto_mute(struct echoaudio *chip, int automute)
{
	DE_ACT(("set_input_auto_mute %d\n", automute));

	chip->digital_in_automute = automute;

	return set_input_clock(chip, chip->input_clock);
}



static int set_digital_mode(struct echoaudio *chip, u8 mode)
{
	u8 previous_mode;
	int err, i, o;

	if (chip->bad_board)
		return -EIO;

	
	if (snd_BUG_ON(chip->pipe_alloc_mask))
		return -EAGAIN;

	if (snd_BUG_ON(!(chip->digital_modes & (1 << mode))))
		return -EINVAL;

	previous_mode = chip->digital_mode;
	err = dsp_set_digital_mode(chip, mode);

	if (err >= 0 && previous_mode != mode &&
	    (previous_mode == DIGITAL_MODE_ADAT || mode == DIGITAL_MODE_ADAT)) {
		spin_lock_irq(&chip->lock);
		for (o = 0; o < num_busses_out(chip); o++)
			for (i = 0; i < num_busses_in(chip); i++)
				set_monitor_gain(chip, o, i,
						 chip->monitor_gain[o][i]);

#ifdef ECHOCARD_HAS_INPUT_GAIN
		for (i = 0; i < num_busses_in(chip); i++)
			set_input_gain(chip, i, chip->input_gain[i]);
		update_input_line_level(chip);
#endif

		for (o = 0; o < num_busses_out(chip); o++)
			set_output_gain(chip, o, chip->output_gain[o]);
		update_output_line_level(chip);
		spin_unlock_irq(&chip->lock);
	}

	return err;
}



static int set_professional_spdif(struct echoaudio *chip, char prof)
{
	u32 control_reg;
	int err;

	
	control_reg = le32_to_cpu(chip->comm_page->control_register);
	control_reg &= GML_SPDIF_FORMAT_CLEAR_MASK;

	
	control_reg |= GML_SPDIF_TWO_CHANNEL | GML_SPDIF_24_BIT |
		GML_SPDIF_COPY_PERMIT;
	if (prof) {
		
		control_reg |= GML_SPDIF_PRO_MODE;

		switch (chip->sample_rate) {
		case 32000:
			control_reg |= GML_SPDIF_SAMPLE_RATE0 |
				GML_SPDIF_SAMPLE_RATE1;
			break;
		case 44100:
			control_reg |= GML_SPDIF_SAMPLE_RATE0;
			break;
		case 48000:
			control_reg |= GML_SPDIF_SAMPLE_RATE1;
			break;
		}
	} else {
		
		switch (chip->sample_rate) {
		case 32000:
			control_reg |= GML_SPDIF_SAMPLE_RATE0 |
				GML_SPDIF_SAMPLE_RATE1;
			break;
		case 48000:
			control_reg |= GML_SPDIF_SAMPLE_RATE1;
			break;
		}
	}

	if ((err = write_control_reg(chip, control_reg, FALSE)))
		return err;
	chip->professional_spdif = prof;
	DE_ACT(("set_professional_spdif to %s\n",
		prof ? "Professional" : "Consumer"));
	return 0;
}
