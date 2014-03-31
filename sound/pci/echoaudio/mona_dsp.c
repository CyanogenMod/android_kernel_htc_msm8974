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


static int write_control_reg(struct echoaudio *chip, u32 value, char force);
static int set_input_clock(struct echoaudio *chip, u16 clock);
static int set_professional_spdif(struct echoaudio *chip, char prof);
static int set_digital_mode(struct echoaudio *chip, u8 mode);
static int load_asic_generic(struct echoaudio *chip, u32 cmd, short asic);
static int check_asic_status(struct echoaudio *chip);


static int init_hw(struct echoaudio *chip, u16 device_id, u16 subdevice_id)
{
	int err;

	DE_INIT(("init_hw() - Mona\n"));
	if (snd_BUG_ON((subdevice_id & 0xfff0) != MONA))
		return -ENODEV;

	if ((err = init_dsp_comm_page(chip))) {
		DE_INIT(("init_hw - could not initialize DSP comm page\n"));
		return err;
	}

	chip->device_id = device_id;
	chip->subdevice_id = subdevice_id;
	chip->bad_board = TRUE;
	chip->input_clock_types =
		ECHO_CLOCK_BIT_INTERNAL | ECHO_CLOCK_BIT_SPDIF |
		ECHO_CLOCK_BIT_WORD | ECHO_CLOCK_BIT_ADAT;
	chip->digital_modes =
		ECHOCAPS_HAS_DIGITAL_MODE_SPDIF_RCA |
		ECHOCAPS_HAS_DIGITAL_MODE_SPDIF_OPTICAL |
		ECHOCAPS_HAS_DIGITAL_MODE_ADAT;

	
	if (chip->device_id == DEVICE_ID_56361)
		chip->dsp_code_to_load = FW_MONA_361_DSP;
	else
		chip->dsp_code_to_load = FW_MONA_301_DSP;

	if ((err = load_firmware(chip)) < 0)
		return err;
	chip->bad_board = FALSE;

	DE_INIT(("init_hw done\n"));
	return err;
}



static int set_mixer_defaults(struct echoaudio *chip)
{
	chip->digital_mode = DIGITAL_MODE_SPDIF_RCA;
	chip->professional_spdif = FALSE;
	chip->digital_in_automute = TRUE;
	return init_line_levels(chip);
}



static u32 detect_input_clocks(const struct echoaudio *chip)
{
	u32 clocks_from_dsp, clock_bits;

	clocks_from_dsp = le32_to_cpu(chip->comm_page->status_clocks);

	clock_bits = ECHO_CLOCK_BIT_INTERNAL;

	if (clocks_from_dsp & GML_CLOCK_DETECT_BIT_SPDIF)
		clock_bits |= ECHO_CLOCK_BIT_SPDIF;

	if (clocks_from_dsp & GML_CLOCK_DETECT_BIT_ADAT)
		clock_bits |= ECHO_CLOCK_BIT_ADAT;

	if (clocks_from_dsp & GML_CLOCK_DETECT_BIT_WORD)
		clock_bits |= ECHO_CLOCK_BIT_WORD;

	return clock_bits;
}



static int load_asic(struct echoaudio *chip)
{
	u32 control_reg;
	int err;
	short asic;

	if (chip->asic_loaded)
		return 0;

	mdelay(10);

	if (chip->device_id == DEVICE_ID_56361)
		asic = FW_MONA_361_1_ASIC48;
	else
		asic = FW_MONA_301_1_ASIC48;

	err = load_asic_generic(chip, DSP_FNC_LOAD_MONA_PCI_CARD_ASIC, asic);
	if (err < 0)
		return err;

	chip->asic_code = asic;
	mdelay(10);

	
	err = load_asic_generic(chip, DSP_FNC_LOAD_MONA_EXTERNAL_ASIC,
				FW_MONA_2_ASIC);
	if (err < 0)
		return err;

	mdelay(10);
	err = check_asic_status(chip);

	if (!err) {
		control_reg = GML_CONVERTER_ENABLE | GML_48KHZ;
		err = write_control_reg(chip, control_reg, TRUE);
	}

	return err;
}



static int switch_asic(struct echoaudio *chip, char double_speed)
{
	int err;
	short asic;

	if (chip->device_id == DEVICE_ID_56361) {
		if (double_speed)
			asic = FW_MONA_361_1_ASIC96;
		else
			asic = FW_MONA_361_1_ASIC48;
	} else {
		if (double_speed)
			asic = FW_MONA_301_1_ASIC96;
		else
			asic = FW_MONA_301_1_ASIC48;
	}

	if (asic != chip->asic_code) {
		
		err = load_asic_generic(chip, DSP_FNC_LOAD_MONA_PCI_CARD_ASIC,
					asic);
		if (err < 0)
			return err;
		chip->asic_code = asic;
	}

	return 0;
}



static int set_sample_rate(struct echoaudio *chip, u32 rate)
{
	u32 control_reg, clock;
	short asic;
	char force_write;

	
	if (chip->input_clock != ECHO_CLOCK_INTERNAL) {
		DE_ACT(("set_sample_rate: Cannot set sample rate - "
			"clock not set to CLK_CLOCKININTERNAL\n"));
		
		chip->comm_page->sample_rate = cpu_to_le32(rate);
		chip->sample_rate = rate;
		return 0;
	}

	
	if (rate >= 88200) {
		if (chip->digital_mode == DIGITAL_MODE_ADAT)
			return -EINVAL;
		if (chip->device_id == DEVICE_ID_56361)
			asic = FW_MONA_361_1_ASIC96;
		else
			asic = FW_MONA_301_1_ASIC96;
	} else {
		if (chip->device_id == DEVICE_ID_56361)
			asic = FW_MONA_361_1_ASIC48;
		else
			asic = FW_MONA_301_1_ASIC48;
	}

	force_write = 0;
	if (asic != chip->asic_code) {
		int err;
		
		spin_unlock_irq(&chip->lock);
		err = load_asic_generic(chip, DSP_FNC_LOAD_MONA_PCI_CARD_ASIC,
					asic);
		spin_lock_irq(&chip->lock);

		if (err < 0)
			return err;
		chip->asic_code = asic;
		force_write = 1;
	}

	
	clock = 0;
	control_reg = le32_to_cpu(chip->comm_page->control_register);
	control_reg &= GML_CLOCK_CLEAR_MASK;
	control_reg &= GML_SPDIF_RATE_CLEAR_MASK;

	switch (rate) {
	case 96000:
		clock = GML_96KHZ;
		break;
	case 88200:
		clock = GML_88KHZ;
		break;
	case 48000:
		clock = GML_48KHZ | GML_SPDIF_SAMPLE_RATE1;
		break;
	case 44100:
		clock = GML_44KHZ;
		
		if (control_reg & GML_SPDIF_PRO_MODE)
			clock |= GML_SPDIF_SAMPLE_RATE0;
		break;
	case 32000:
		clock = GML_32KHZ | GML_SPDIF_SAMPLE_RATE0 |
			GML_SPDIF_SAMPLE_RATE1;
		break;
	case 22050:
		clock = GML_22KHZ;
		break;
	case 16000:
		clock = GML_16KHZ;
		break;
	case 11025:
		clock = GML_11KHZ;
		break;
	case 8000:
		clock = GML_8KHZ;
		break;
	default:
		DE_ACT(("set_sample_rate: %d invalid!\n", rate));
		return -EINVAL;
	}

	control_reg |= clock;

	chip->comm_page->sample_rate = cpu_to_le32(rate);	
	chip->sample_rate = rate;
	DE_ACT(("set_sample_rate: %d clock %d\n", rate, clock));

	return write_control_reg(chip, control_reg, force_write);
}



static int set_input_clock(struct echoaudio *chip, u16 clock)
{
	u32 control_reg, clocks_from_dsp;
	int err;

	DE_ACT(("set_input_clock:\n"));

	
	if (atomic_read(&chip->opencount))
		return -EAGAIN;

	
	control_reg = le32_to_cpu(chip->comm_page->control_register) &
		GML_CLOCK_CLEAR_MASK;
	clocks_from_dsp = le32_to_cpu(chip->comm_page->status_clocks);

	switch (clock) {
	case ECHO_CLOCK_INTERNAL:
		DE_ACT(("Set Mona clock to INTERNAL\n"));
		chip->input_clock = ECHO_CLOCK_INTERNAL;
		return set_sample_rate(chip, chip->sample_rate);
	case ECHO_CLOCK_SPDIF:
		if (chip->digital_mode == DIGITAL_MODE_ADAT)
			return -EAGAIN;
		spin_unlock_irq(&chip->lock);
		err = switch_asic(chip, clocks_from_dsp &
				  GML_CLOCK_DETECT_BIT_SPDIF96);
		spin_lock_irq(&chip->lock);
		if (err < 0)
			return err;
		DE_ACT(("Set Mona clock to SPDIF\n"));
		control_reg |= GML_SPDIF_CLOCK;
		if (clocks_from_dsp & GML_CLOCK_DETECT_BIT_SPDIF96)
			control_reg |= GML_DOUBLE_SPEED_MODE;
		else
			control_reg &= ~GML_DOUBLE_SPEED_MODE;
		break;
	case ECHO_CLOCK_WORD:
		DE_ACT(("Set Mona clock to WORD\n"));
		spin_unlock_irq(&chip->lock);
		err = switch_asic(chip, clocks_from_dsp &
				  GML_CLOCK_DETECT_BIT_WORD96);
		spin_lock_irq(&chip->lock);
		if (err < 0)
			return err;
		control_reg |= GML_WORD_CLOCK;
		if (clocks_from_dsp & GML_CLOCK_DETECT_BIT_WORD96)
			control_reg |= GML_DOUBLE_SPEED_MODE;
		else
			control_reg &= ~GML_DOUBLE_SPEED_MODE;
		break;
	case ECHO_CLOCK_ADAT:
		DE_ACT(("Set Mona clock to ADAT\n"));
		if (chip->digital_mode != DIGITAL_MODE_ADAT)
			return -EAGAIN;
		control_reg |= GML_ADAT_CLOCK;
		control_reg &= ~GML_DOUBLE_SPEED_MODE;
		break;
	default:
		DE_ACT(("Input clock 0x%x not supported for Mona\n", clock));
		return -EINVAL;
	}

	chip->input_clock = clock;
	return write_control_reg(chip, control_reg, TRUE);
}



static int dsp_set_digital_mode(struct echoaudio *chip, u8 mode)
{
	u32 control_reg;
	int err, incompatible_clock;

	
	incompatible_clock = FALSE;
	switch (mode) {
	case DIGITAL_MODE_SPDIF_OPTICAL:
	case DIGITAL_MODE_SPDIF_RCA:
		if (chip->input_clock == ECHO_CLOCK_ADAT)
			incompatible_clock = TRUE;
		break;
	case DIGITAL_MODE_ADAT:
		if (chip->input_clock == ECHO_CLOCK_SPDIF)
			incompatible_clock = TRUE;
		break;
	default:
		DE_ACT(("Digital mode not supported: %d\n", mode));
		return -EINVAL;
	}

	spin_lock_irq(&chip->lock);

	if (incompatible_clock) {	
		chip->sample_rate = 48000;
		set_input_clock(chip, ECHO_CLOCK_INTERNAL);
	}

	
	control_reg = le32_to_cpu(chip->comm_page->control_register);
	control_reg &= GML_DIGITAL_MODE_CLEAR_MASK;

	
	switch (mode) {
	case DIGITAL_MODE_SPDIF_OPTICAL:
		control_reg |= GML_SPDIF_OPTICAL_MODE;
		break;
	case DIGITAL_MODE_SPDIF_RCA:
		
		break;
	case DIGITAL_MODE_ADAT:
		if (chip->asic_code == FW_MONA_361_1_ASIC96 ||
		    chip->asic_code == FW_MONA_301_1_ASIC96) {
			set_sample_rate(chip, 48000);
		}
		control_reg |= GML_ADAT_MODE;
		control_reg &= ~GML_DOUBLE_SPEED_MODE;
		break;
	}

	err = write_control_reg(chip, control_reg, FALSE);
	spin_unlock_irq(&chip->lock);
	if (err < 0)
		return err;
	chip->digital_mode = mode;

	DE_ACT(("set_digital_mode to %d\n", mode));
	return incompatible_clock;
}
