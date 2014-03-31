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

#if PAGE_SIZE < 4096
#error PAGE_SIZE is < 4k
#endif

static int restore_dsp_rettings(struct echoaudio *chip);


static int wait_handshake(struct echoaudio *chip)
{
	int i;

	
	for (i = 0; i < HANDSHAKE_TIMEOUT; i++) {
		
		barrier();
		if (chip->comm_page->handshake) {
			return 0;
		}
		udelay(1);
	}

	snd_printk(KERN_ERR "wait_handshake(): Timeout waiting for DSP\n");
	return -EBUSY;
}



static int send_vector(struct echoaudio *chip, u32 command)
{
	int i;

	wmb();	

	
	for (i = 0; i < VECTOR_BUSY_TIMEOUT; i++) {
		if (!(get_dsp_register(chip, CHI32_VECTOR_REG) &
		      CHI32_VECTOR_BUSY)) {
			set_dsp_register(chip, CHI32_VECTOR_REG, command);
			
			return 0;
		}
		udelay(1);
	}

	DE_ACT((KERN_ERR "timeout on send_vector\n"));
	return -EBUSY;
}



static int write_dsp(struct echoaudio *chip, u32 data)
{
	u32 status, i;

	for (i = 0; i < 10000000; i++) {	
		status = get_dsp_register(chip, CHI32_STATUS_REG);
		if ((status & CHI32_STATUS_HOST_WRITE_EMPTY) != 0) {
			set_dsp_register(chip, CHI32_DATA_REG, data);
			wmb();			
			return 0;
		}
		udelay(1);
		cond_resched();
	}

	chip->bad_board = TRUE;		
	DE_ACT((KERN_ERR "write_dsp: Set bad_board to TRUE\n"));
	return -EIO;
}



static int read_dsp(struct echoaudio *chip, u32 *data)
{
	u32 status, i;

	for (i = 0; i < READ_DSP_TIMEOUT; i++) {
		status = get_dsp_register(chip, CHI32_STATUS_REG);
		if ((status & CHI32_STATUS_HOST_READ_FULL) != 0) {
			*data = get_dsp_register(chip, CHI32_DATA_REG);
			return 0;
		}
		udelay(1);
		cond_resched();
	}

	chip->bad_board = TRUE;		
	DE_INIT((KERN_ERR "read_dsp: Set bad_board to TRUE\n"));
	return -EIO;
}




static int read_sn(struct echoaudio *chip)
{
	int i;
	u32 sn[6];

	for (i = 0; i < 5; i++) {
		if (read_dsp(chip, &sn[i])) {
			snd_printk(KERN_ERR "Failed to read serial number\n");
			return -EIO;
		}
	}
	DE_INIT(("Read serial number %08x %08x %08x %08x %08x\n",
		 sn[0], sn[1], sn[2], sn[3], sn[4]));
	return 0;
}



#ifndef ECHOCARD_HAS_ASIC
static inline int check_asic_status(struct echoaudio *chip)
{
	chip->asic_loaded = TRUE;
	return 0;
}

#endif 



#ifdef ECHOCARD_HAS_ASIC

static int load_asic_generic(struct echoaudio *chip, u32 cmd, short asic)
{
	const struct firmware *fw;
	int err;
	u32 i, size;
	u8 *code;

	err = get_firmware(&fw, chip, asic);
	if (err < 0) {
		snd_printk(KERN_WARNING "Firmware not found !\n");
		return err;
	}

	code = (u8 *)fw->data;
	size = fw->size;

	
	if (write_dsp(chip, cmd) < 0)
		goto la_error;

	
	if (write_dsp(chip, size) < 0)
		goto la_error;

	for (i = 0; i < size; i++) {
		if (write_dsp(chip, code[i]) < 0)
			goto la_error;
	}

	DE_INIT(("ASIC loaded\n"));
	free_firmware(fw);
	return 0;

la_error:
	DE_INIT(("failed on write_dsp\n"));
	free_firmware(fw);
	return -EIO;
}

#endif 



#ifdef DSP_56361

static int install_resident_loader(struct echoaudio *chip)
{
	u32 address;
	int index, words, i;
	u16 *code;
	u32 status;
	const struct firmware *fw;

	if (chip->device_id != DEVICE_ID_56361)
		return 0;

	status = get_dsp_register(chip, CHI32_STATUS_REG);
	if (status & CHI32_STATUS_REG_HF5) {
		DE_INIT(("Resident loader already installed; status is 0x%x\n",
			 status));
		return 0;
	}

	i = get_firmware(&fw, chip, FW_361_LOADER);
	if (i < 0) {
		snd_printk(KERN_WARNING "Firmware not found !\n");
		return i;
	}


	
	set_dsp_register(chip, CHI32_CONTROL_REG,
			 get_dsp_register(chip, CHI32_CONTROL_REG) | 0x900);

	code = (u16 *)fw->data;

	index = code[0];

	
	index += 3;

	
	words = code[index++];

	
	address = ((u32)code[index] << 16) + code[index + 1];
	index += 2;

	
	if (write_dsp(chip, words)) {
		DE_INIT(("install_resident_loader: Failed to write word count!\n"));
		goto irl_error;
	}
	
	if (write_dsp(chip, address)) {
		DE_INIT(("install_resident_loader: Failed to write DSP address!\n"));
		goto irl_error;
	}
	
	for (i = 0; i < words; i++) {
		u32 data;

		data = ((u32)code[index] << 16) + code[index + 1];
		if (write_dsp(chip, data)) {
			DE_INIT(("install_resident_loader: Failed to write DSP code\n"));
			goto irl_error;
		}
		index += 2;
	}

	
	for (i = 0; i < 200; i++) {	
		udelay(50);
		status = get_dsp_register(chip, CHI32_STATUS_REG);
		if (status & CHI32_STATUS_REG_HF5)
			break;
	}

	if (i == 200) {
		DE_INIT(("Resident loader failed to set HF5\n"));
		goto irl_error;
	}

	DE_INIT(("Resident loader successfully installed\n"));
	free_firmware(fw);
	return 0;

irl_error:
	free_firmware(fw);
	return -EIO;
}

#endif 


static int load_dsp(struct echoaudio *chip, u16 *code)
{
	u32 address, data;
	int index, words, i;

	if (chip->dsp_code == code) {
		DE_INIT(("DSP is already loaded!\n"));
		return 0;
	}
	chip->bad_board = TRUE;		
	chip->dsp_code = NULL;		
	chip->asic_loaded = FALSE;	

	DE_INIT(("load_dsp: Set bad_board to TRUE\n"));

	
#ifdef DSP_56361
	if ((i = install_resident_loader(chip)) < 0)
		return i;
#endif

	
	if (send_vector(chip, DSP_VC_RESET) < 0) {
		DE_INIT(("LoadDsp: send_vector DSP_VC_RESET failed, Critical Failure\n"));
		return -EIO;
	}
	
	udelay(10);

	
	for (i = 0; i < 1000; i++) {	
		if (get_dsp_register(chip, CHI32_STATUS_REG) &
		    CHI32_STATUS_REG_HF3)
			break;
		udelay(10);
	}

	if (i == 1000) {
		DE_INIT(("load_dsp: Timeout waiting for CHI32_STATUS_REG_HF3\n"));
		return -EIO;
	}

	
	set_dsp_register(chip, CHI32_CONTROL_REG,
			 get_dsp_register(chip, CHI32_CONTROL_REG) | 0x900);

	

	index = code[0];
	for (;;) {
		int block_type, mem_type;

		
		index++;

		
		block_type = code[index];
		if (block_type == 4)	
			break;

		index++;

		
		mem_type = code[index++];

		
		words = code[index++];
		if (words == 0)		
			break;

		
		address = ((u32)code[index] << 16) + code[index + 1];
		index += 2;

		if (write_dsp(chip, words) < 0) {
			DE_INIT(("load_dsp: failed to write number of DSP words\n"));
			return -EIO;
		}
		if (write_dsp(chip, address) < 0) {
			DE_INIT(("load_dsp: failed to write DSP address\n"));
			return -EIO;
		}
		if (write_dsp(chip, mem_type) < 0) {
			DE_INIT(("load_dsp: failed to write DSP memory type\n"));
			return -EIO;
		}
		
		for (i = 0; i < words; i++, index+=2) {
			data = ((u32)code[index] << 16) + code[index + 1];
			if (write_dsp(chip, data) < 0) {
				DE_INIT(("load_dsp: failed to write DSP data\n"));
				return -EIO;
			}
		}
	}

	if (write_dsp(chip, 0) < 0) {	
		DE_INIT(("load_dsp: Failed to write final zero\n"));
		return -EIO;
	}
	udelay(10);

	for (i = 0; i < 5000; i++) {	
		
		if (get_dsp_register(chip, CHI32_STATUS_REG) &
		    CHI32_STATUS_REG_HF4) {
			set_dsp_register(chip, CHI32_CONTROL_REG,
					 get_dsp_register(chip, CHI32_CONTROL_REG) & ~0x1b00);

			if (write_dsp(chip, DSP_FNC_SET_COMMPAGE_ADDR) < 0) {
				DE_INIT(("load_dsp: Failed to write DSP_FNC_SET_COMMPAGE_ADDR\n"));
				return -EIO;
			}

			if (write_dsp(chip, chip->comm_page_phys) < 0) {
				DE_INIT(("load_dsp: Failed to write comm page address\n"));
				return -EIO;
			}

			if (read_sn(chip) < 0) {
				DE_INIT(("load_dsp: Failed to read serial number\n"));
				return -EIO;
			}

			chip->dsp_code = code;		
			chip->bad_board = FALSE;	
			DE_INIT(("load_dsp: OK!\n"));
			return 0;
		}
		udelay(100);
	}

	DE_INIT(("load_dsp: DSP load timed out waiting for HF4\n"));
	return -EIO;
}



static int load_firmware(struct echoaudio *chip)
{
	const struct firmware *fw;
	int box_type, err;

	if (snd_BUG_ON(!chip->comm_page))
		return -EPERM;

	
	if (chip->dsp_code) {
		if ((box_type = check_asic_status(chip)) >= 0)
			return box_type;
		
		chip->dsp_code = NULL;
	}

	err = get_firmware(&fw, chip, chip->dsp_code_to_load);
	if (err < 0)
		return err;
	err = load_dsp(chip, (u16 *)fw->data);
	free_firmware(fw);
	if (err < 0)
		return err;

	if ((box_type = load_asic(chip)) < 0)
		return box_type;	

	return box_type;
}




#if defined(ECHOCARD_HAS_INPUT_NOMINAL_LEVEL) || \
	defined(ECHOCARD_HAS_OUTPUT_NOMINAL_LEVEL)

static int set_nominal_level(struct echoaudio *chip, u16 index, char consumer)
{
	if (snd_BUG_ON(index >= num_busses_out(chip) + num_busses_in(chip)))
		return -EINVAL;

	
	if (wait_handshake(chip))
		return -EIO;

	chip->nominal_level[index] = consumer;

	if (consumer)
		chip->comm_page->nominal_level_mask |= cpu_to_le32(1 << index);
	else
		chip->comm_page->nominal_level_mask &= ~cpu_to_le32(1 << index);

	return 0;
}

#endif 



static int set_output_gain(struct echoaudio *chip, u16 channel, s8 gain)
{
	if (snd_BUG_ON(channel >= num_busses_out(chip)))
		return -EINVAL;

	if (wait_handshake(chip))
		return -EIO;

	
	chip->output_gain[channel] = gain;
	chip->comm_page->line_out_level[channel] = gain;
	return 0;
}



#ifdef ECHOCARD_HAS_MONITOR
static int set_monitor_gain(struct echoaudio *chip, u16 output, u16 input,
			    s8 gain)
{
	if (snd_BUG_ON(output >= num_busses_out(chip) ||
		    input >= num_busses_in(chip)))
		return -EINVAL;

	if (wait_handshake(chip))
		return -EIO;

	chip->monitor_gain[output][input] = gain;
	chip->comm_page->monitors[monitor_index(chip, output, input)] = gain;
	return 0;
}
#endif 


static int update_output_line_level(struct echoaudio *chip)
{
	if (wait_handshake(chip))
		return -EIO;
	clear_handshake(chip);
	return send_vector(chip, DSP_VC_UPDATE_OUTVOL);
}



static int update_input_line_level(struct echoaudio *chip)
{
	if (wait_handshake(chip))
		return -EIO;
	clear_handshake(chip);
	return send_vector(chip, DSP_VC_UPDATE_INGAIN);
}



static void set_meters_on(struct echoaudio *chip, char on)
{
	if (on && !chip->meters_enabled) {
		send_vector(chip, DSP_VC_METERS_ON);
		chip->meters_enabled = 1;
	} else if (!on && chip->meters_enabled) {
		send_vector(chip, DSP_VC_METERS_OFF);
		chip->meters_enabled = 0;
		memset((s8 *)chip->comm_page->vu_meter, ECHOGAIN_MUTED,
		       DSP_MAXPIPES);
		memset((s8 *)chip->comm_page->peak_meter, ECHOGAIN_MUTED,
		       DSP_MAXPIPES);
	}
}



/* Fill out an the given array using the current values in the comm page.
Meters are written in the comm page by the DSP in this order:
 Output busses
 Input busses
 Output pipes (vmixer cards only)

This function assumes there are no more than 16 in/out busses or pipes
Meters is an array [3][16][2] of long. */
static void get_audio_meters(struct echoaudio *chip, long *meters)
{
	int i, m, n;

	m = 0;
	n = 0;
	for (i = 0; i < num_busses_out(chip); i++, m++) {
		meters[n++] = chip->comm_page->vu_meter[m];
		meters[n++] = chip->comm_page->peak_meter[m];
	}
	for (; n < 32; n++)
		meters[n] = 0;

#ifdef ECHOCARD_ECHO3G
	m = E3G_MAX_OUTPUTS;	
#endif

	for (i = 0; i < num_busses_in(chip); i++, m++) {
		meters[n++] = chip->comm_page->vu_meter[m];
		meters[n++] = chip->comm_page->peak_meter[m];
	}
	for (; n < 64; n++)
		meters[n] = 0;

#ifdef ECHOCARD_HAS_VMIXER
	for (i = 0; i < num_pipes_out(chip); i++, m++) {
		meters[n++] = chip->comm_page->vu_meter[m];
		meters[n++] = chip->comm_page->peak_meter[m];
	}
#endif
	for (; n < 96; n++)
		meters[n] = 0;
}



static int restore_dsp_rettings(struct echoaudio *chip)
{
	int i, o, err;
	DE_INIT(("restore_dsp_settings\n"));

	if ((err = check_asic_status(chip)) < 0)
		return err;

	
	chip->comm_page->gd_clock_state = GD_CLOCK_UNDEF;
	chip->comm_page->gd_spdif_status = GD_SPDIF_STATUS_UNDEF;
	chip->comm_page->handshake = 0xffffffff;

	
	for (i = 0; i < num_busses_out(chip); i++) {
		err = set_output_gain(chip, i, chip->output_gain[i]);
		if (err < 0)
			return err;
	}

#ifdef ECHOCARD_HAS_VMIXER
	for (i = 0; i < num_pipes_out(chip); i++)
		for (o = 0; o < num_busses_out(chip); o++) {
			err = set_vmixer_gain(chip, o, i,
						chip->vmixer_gain[o][i]);
			if (err < 0)
				return err;
		}
	if (update_vmixer_level(chip) < 0)
		return -EIO;
#endif 

#ifdef ECHOCARD_HAS_MONITOR
	for (o = 0; o < num_busses_out(chip); o++)
		for (i = 0; i < num_busses_in(chip); i++) {
			err = set_monitor_gain(chip, o, i,
						chip->monitor_gain[o][i]);
			if (err < 0)
				return err;
		}
#endif 

#ifdef ECHOCARD_HAS_INPUT_GAIN
	for (i = 0; i < num_busses_in(chip); i++) {
		err = set_input_gain(chip, i, chip->input_gain[i]);
		if (err < 0)
			return err;
	}
#endif 

	err = update_output_line_level(chip);
	if (err < 0)
		return err;

	err = update_input_line_level(chip);
	if (err < 0)
		return err;

	err = set_sample_rate(chip, chip->sample_rate);
	if (err < 0)
		return err;

	if (chip->meters_enabled) {
		err = send_vector(chip, DSP_VC_METERS_ON);
		if (err < 0)
			return err;
	}

#ifdef ECHOCARD_HAS_DIGITAL_MODE_SWITCH
	if (set_digital_mode(chip, chip->digital_mode) < 0)
		return -EIO;
#endif

#ifdef ECHOCARD_HAS_DIGITAL_IO
	if (set_professional_spdif(chip, chip->professional_spdif) < 0)
		return -EIO;
#endif

#ifdef ECHOCARD_HAS_PHANTOM_POWER
	if (set_phantom_power(chip, chip->phantom_power) < 0)
		return -EIO;
#endif

#ifdef ECHOCARD_HAS_EXTERNAL_CLOCK
	
	if (set_input_clock(chip, chip->input_clock) < 0)
		return -EIO;
#endif

#ifdef ECHOCARD_HAS_OUTPUT_CLOCK_SWITCH
	if (set_output_clock(chip, chip->output_clock) < 0)
		return -EIO;
#endif

	if (wait_handshake(chip) < 0)
		return -EIO;
	clear_handshake(chip);
	if (send_vector(chip, DSP_VC_UPDATE_FLAGS) < 0)
		return -EIO;

	DE_INIT(("restore_dsp_rettings done\n"));
	return 0;
}




static void set_audio_format(struct echoaudio *chip, u16 pipe_index,
			     const struct audioformat *format)
{
	u16 dsp_format;

	dsp_format = DSP_AUDIOFORM_SS_16LE;

	
	if (format->interleave > 2) {
		switch (format->bits_per_sample) {
		case 16:
			dsp_format = DSP_AUDIOFORM_SUPER_INTERLEAVE_16LE;
			break;
		case 24:
			dsp_format = DSP_AUDIOFORM_SUPER_INTERLEAVE_24LE;
			break;
		case 32:
			dsp_format = DSP_AUDIOFORM_SUPER_INTERLEAVE_32LE;
			break;
		}
		dsp_format |= format->interleave;
	} else if (format->data_are_bigendian) {
		
		switch (format->interleave) {
		case 1:
			dsp_format = DSP_AUDIOFORM_MM_32BE;
			break;
#ifdef ECHOCARD_HAS_STEREO_BIG_ENDIAN32
		case 2:
			dsp_format = DSP_AUDIOFORM_SS_32BE;
			break;
#endif
		}
	} else if (format->interleave == 1 &&
		   format->bits_per_sample == 32 && !format->mono_to_stereo) {
		
		dsp_format = DSP_AUDIOFORM_MM_32LE;
	} else {
		
		switch (format->bits_per_sample) {
		case 8:
			if (format->interleave == 2)
				dsp_format = DSP_AUDIOFORM_SS_8;
			else
				dsp_format = DSP_AUDIOFORM_MS_8;
			break;
		default:
		case 16:
			if (format->interleave == 2)
				dsp_format = DSP_AUDIOFORM_SS_16LE;
			else
				dsp_format = DSP_AUDIOFORM_MS_16LE;
			break;
		case 24:
			if (format->interleave == 2)
				dsp_format = DSP_AUDIOFORM_SS_24LE;
			else
				dsp_format = DSP_AUDIOFORM_MS_24LE;
			break;
		case 32:
			if (format->interleave == 2)
				dsp_format = DSP_AUDIOFORM_SS_32LE;
			else
				dsp_format = DSP_AUDIOFORM_MS_32LE;
			break;
		}
	}
	DE_ACT(("set_audio_format[%d] = %x\n", pipe_index, dsp_format));
	chip->comm_page->audio_format[pipe_index] = cpu_to_le16(dsp_format);
}



static int start_transport(struct echoaudio *chip, u32 channel_mask,
			   u32 cyclic_mask)
{
	DE_ACT(("start_transport %x\n", channel_mask));

	if (wait_handshake(chip))
		return -EIO;

	chip->comm_page->cmd_start |= cpu_to_le32(channel_mask);

	if (chip->comm_page->cmd_start) {
		clear_handshake(chip);
		send_vector(chip, DSP_VC_START_TRANSFER);
		if (wait_handshake(chip))
			return -EIO;
		
		chip->active_mask |= channel_mask;
		chip->comm_page->cmd_start = 0;
		return 0;
	}

	DE_ACT(("start_transport: No pipes to start!\n"));
	return -EINVAL;
}



static int pause_transport(struct echoaudio *chip, u32 channel_mask)
{
	DE_ACT(("pause_transport %x\n", channel_mask));

	if (wait_handshake(chip))
		return -EIO;

	chip->comm_page->cmd_stop |= cpu_to_le32(channel_mask);
	chip->comm_page->cmd_reset = 0;
	if (chip->comm_page->cmd_stop) {
		clear_handshake(chip);
		send_vector(chip, DSP_VC_STOP_TRANSFER);
		if (wait_handshake(chip))
			return -EIO;
		
		chip->active_mask &= ~channel_mask;
		chip->comm_page->cmd_stop = 0;
		chip->comm_page->cmd_reset = 0;
		return 0;
	}

	DE_ACT(("pause_transport: No pipes to stop!\n"));
	return 0;
}



static int stop_transport(struct echoaudio *chip, u32 channel_mask)
{
	DE_ACT(("stop_transport %x\n", channel_mask));

	if (wait_handshake(chip))
		return -EIO;

	chip->comm_page->cmd_stop |= cpu_to_le32(channel_mask);
	chip->comm_page->cmd_reset |= cpu_to_le32(channel_mask);
	if (chip->comm_page->cmd_reset) {
		clear_handshake(chip);
		send_vector(chip, DSP_VC_STOP_TRANSFER);
		if (wait_handshake(chip))
			return -EIO;
		
		chip->active_mask &= ~channel_mask;
		chip->comm_page->cmd_stop = 0;
		chip->comm_page->cmd_reset = 0;
		return 0;
	}

	DE_ACT(("stop_transport: No pipes to stop!\n"));
	return 0;
}



static inline int is_pipe_allocated(struct echoaudio *chip, u16 pipe_index)
{
	return (chip->pipe_alloc_mask & (1 << pipe_index));
}



static int rest_in_peace(struct echoaudio *chip)
{
	DE_ACT(("rest_in_peace() open=%x\n", chip->pipe_alloc_mask));

	
	stop_transport(chip, chip->active_mask);

	set_meters_on(chip, FALSE);

#ifdef ECHOCARD_HAS_MIDI
	enable_midi_input(chip, FALSE);
#endif

	
	if (chip->dsp_code) {
		
		chip->dsp_code = NULL;
		
		return send_vector(chip, DSP_VC_GO_COMATOSE);
	}
	return 0;
}



static int init_dsp_comm_page(struct echoaudio *chip)
{
	
	if (offsetof(struct comm_page, midi_output) != 0xbe0) {
		DE_INIT(("init_dsp_comm_page() - Invalid struct comm_page structure\n"));
		return -EPERM;
	}

	
	chip->card_name = ECHOCARD_NAME;
	chip->bad_board = TRUE;	
	chip->dsp_code = NULL;	
	chip->asic_loaded = FALSE;
	memset(chip->comm_page, 0, sizeof(struct comm_page));

	
	chip->comm_page->comm_size =
		cpu_to_le32(sizeof(struct comm_page));
	chip->comm_page->handshake = 0xffffffff;
	chip->comm_page->midi_out_free_count =
		cpu_to_le32(DSP_MIDI_OUT_FIFO_SIZE);
	chip->comm_page->sample_rate = cpu_to_le32(44100);

	
	memset(chip->comm_page->monitors, ECHOGAIN_MUTED, MONITOR_ARRAY_SIZE);
	memset(chip->comm_page->vmixer, ECHOGAIN_MUTED, VMIXER_ARRAY_SIZE);

	return 0;
}



static int init_line_levels(struct echoaudio *chip)
{
	DE_INIT(("init_line_levels\n"));
	memset(chip->output_gain, ECHOGAIN_MUTED, sizeof(chip->output_gain));
	memset(chip->input_gain, ECHOGAIN_MUTED, sizeof(chip->input_gain));
	memset(chip->monitor_gain, ECHOGAIN_MUTED, sizeof(chip->monitor_gain));
	memset(chip->vmixer_gain, ECHOGAIN_MUTED, sizeof(chip->vmixer_gain));
	chip->input_clock = ECHO_CLOCK_INTERNAL;
	chip->output_clock = ECHO_CLOCK_WORD;
	chip->sample_rate = 44100;
	return restore_dsp_rettings(chip);
}



static int service_irq(struct echoaudio *chip)
{
	int st;

	
	if (get_dsp_register(chip, CHI32_STATUS_REG) & CHI32_STATUS_IRQ) {
		st = 0;
#ifdef ECHOCARD_HAS_MIDI
		
		if (chip->comm_page->midi_input[0])	
			st = midi_service_irq(chip);	
#endif
		
		chip->comm_page->midi_input[0] = 0;
		send_vector(chip, DSP_VC_ACK_INT);
		return st;
	}
	return -1;
}





static int allocate_pipes(struct echoaudio *chip, struct audiopipe *pipe,
			  int pipe_index, int interleave)
{
	int i;
	u32 channel_mask;
	char is_cyclic;

	DE_ACT(("allocate_pipes: ch=%d int=%d\n", pipe_index, interleave));

	if (chip->bad_board)
		return -EIO;

	is_cyclic = 1;	

	for (channel_mask = i = 0; i < interleave; i++)
		channel_mask |= 1 << (pipe_index + i);
	if (chip->pipe_alloc_mask & channel_mask) {
		DE_ACT(("allocate_pipes: channel already open\n"));
		return -EAGAIN;
	}

	chip->comm_page->position[pipe_index] = 0;
	chip->pipe_alloc_mask |= channel_mask;
	if (is_cyclic)
		chip->pipe_cyclic_mask |= channel_mask;
	pipe->index = pipe_index;
	pipe->interleave = interleave;
	pipe->state = PIPE_STATE_STOPPED;

	pipe->dma_counter = &chip->comm_page->position[pipe_index];
	*pipe->dma_counter = 0;
	DE_ACT(("allocate_pipes: ok\n"));
	return pipe_index;
}



static int free_pipes(struct echoaudio *chip, struct audiopipe *pipe)
{
	u32 channel_mask;
	int i;

	DE_ACT(("free_pipes: Pipe %d\n", pipe->index));
	if (snd_BUG_ON(!is_pipe_allocated(chip, pipe->index)))
		return -EINVAL;
	if (snd_BUG_ON(pipe->state != PIPE_STATE_STOPPED))
		return -EINVAL;

	for (channel_mask = i = 0; i < pipe->interleave; i++)
		channel_mask |= 1 << (pipe->index + i);

	chip->pipe_alloc_mask &= ~channel_mask;
	chip->pipe_cyclic_mask &= ~channel_mask;
	return 0;
}




static int sglist_init(struct echoaudio *chip, struct audiopipe *pipe)
{
	pipe->sglist_head = 0;
	memset(pipe->sgpage.area, 0, PAGE_SIZE);
	chip->comm_page->sglist_addr[pipe->index].addr =
		cpu_to_le32(pipe->sgpage.addr);
	return 0;
}



static int sglist_add_mapping(struct echoaudio *chip, struct audiopipe *pipe,
				dma_addr_t address, size_t length)
{
	int head = pipe->sglist_head;
	struct sg_entry *list = (struct sg_entry *)pipe->sgpage.area;

	if (head < MAX_SGLIST_ENTRIES - 1) {
		list[head].addr = cpu_to_le32(address);
		list[head].size = cpu_to_le32(length);
		pipe->sglist_head++;
	} else {
		DE_ACT(("SGlist: too many fragments\n"));
		return -ENOMEM;
	}
	return 0;
}



static inline int sglist_add_irq(struct echoaudio *chip, struct audiopipe *pipe)
{
	return sglist_add_mapping(chip, pipe, 0, 0);
}



static inline int sglist_wrap(struct echoaudio *chip, struct audiopipe *pipe)
{
	return sglist_add_mapping(chip, pipe, pipe->sgpage.addr, 0);
}
