/*
   cx231xx_avcore.c - driver for Conexant Cx23100/101/102
		      USB video capture devices

   Copyright (C) 2008 <srinivasa.deevi at conexant dot com>

   This program contains the specific code to control the avdecoder chip and
   other related usb control functions for cx231xx based chipset.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/usb.h>
#include <linux/i2c.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <media/tuner.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-chip-ident.h>

#include "cx231xx.h"
#include "cx231xx-dif.h"

#define TUNER_MODE_FM_RADIO 0
static int verve_write_byte(struct cx231xx *dev, u8 saddr, u8 data)
{
	return cx231xx_write_i2c_data(dev, VERVE_I2C_ADDRESS,
					saddr, 1, data, 1);
}

static int verve_read_byte(struct cx231xx *dev, u8 saddr, u8 *data)
{
	int status;
	u32 temp = 0;

	status = cx231xx_read_i2c_data(dev, VERVE_I2C_ADDRESS,
					saddr, 1, &temp, 1);
	*data = (u8) temp;
	return status;
}
void initGPIO(struct cx231xx *dev)
{
	u32 _gpio_direction = 0;
	u32 value = 0;
	u8 val = 0;

	_gpio_direction = _gpio_direction & 0xFC0003FF;
	_gpio_direction = _gpio_direction | 0x03FDFC00;
	cx231xx_send_gpio_cmd(dev, _gpio_direction, (u8 *)&value, 4, 0, 0);

	verve_read_byte(dev, 0x07, &val);
	cx231xx_info(" verve_read_byte address0x07=0x%x\n", val);
	verve_write_byte(dev, 0x07, 0xF4);
	verve_read_byte(dev, 0x07, &val);
	cx231xx_info(" verve_read_byte address0x07=0x%x\n", val);

	cx231xx_capture_start(dev, 1, 2);

	cx231xx_mode_register(dev, EP_MODE_SET, 0x0500FE00);
	cx231xx_mode_register(dev, GBULK_BIT_EN, 0xFFFDFFFF);

}
void uninitGPIO(struct cx231xx *dev)
{
	u8 value[4] = { 0, 0, 0, 0 };

	cx231xx_capture_start(dev, 0, 2);
	verve_write_byte(dev, 0x07, 0x14);
	cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
			0x68, value, 4);
}

static int afe_write_byte(struct cx231xx *dev, u16 saddr, u8 data)
{
	return cx231xx_write_i2c_data(dev, AFE_DEVICE_ADDRESS,
					saddr, 2, data, 1);
}

static int afe_read_byte(struct cx231xx *dev, u16 saddr, u8 *data)
{
	int status;
	u32 temp = 0;

	status = cx231xx_read_i2c_data(dev, AFE_DEVICE_ADDRESS,
					saddr, 2, &temp, 1);
	*data = (u8) temp;
	return status;
}

int cx231xx_afe_init_super_block(struct cx231xx *dev, u32 ref_count)
{
	int status = 0;
	u8 temp = 0;
	u8 afe_power_status = 0;
	int i = 0;

	
	temp = (u8) (ref_count & 0xff);
	status = afe_write_byte(dev, SUP_BLK_TUNE2, temp);
	if (status < 0)
		return status;

	status = afe_read_byte(dev, SUP_BLK_TUNE2, &afe_power_status);
	if (status < 0)
		return status;

	temp = (u8) ((ref_count & 0x300) >> 8);
	temp |= 0x40;
	status = afe_write_byte(dev, SUP_BLK_TUNE1, temp);
	if (status < 0)
		return status;

	status = afe_write_byte(dev, SUP_BLK_PLL2, 0x0f);
	if (status < 0)
		return status;

	
	while (afe_power_status != 0x18) {
		status = afe_write_byte(dev, SUP_BLK_PWRDN, 0x18);
		if (status < 0) {
			cx231xx_info(
			": Init Super Block failed in send cmd\n");
			break;
		}

		status = afe_read_byte(dev, SUP_BLK_PWRDN, &afe_power_status);
		afe_power_status &= 0xff;
		if (status < 0) {
			cx231xx_info(
			": Init Super Block failed in receive cmd\n");
			break;
		}
		i++;
		if (i == 10) {
			cx231xx_info(
			": Init Super Block force break in loop !!!!\n");
			status = -1;
			break;
		}
	}

	if (status < 0)
		return status;

	
	status = afe_write_byte(dev, SUP_BLK_TUNE3, 0x40);
	if (status < 0)
		return status;

	msleep(5);

	
	status = afe_write_byte(dev, SUP_BLK_TUNE3, 0x00);

	return status;
}

int cx231xx_afe_init_channels(struct cx231xx *dev)
{
	int status = 0;

	
	status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1, 0x00);
	status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2, 0x00);
	status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3, 0x00);

	
	status = afe_write_byte(dev, ADC_COM_QUANT, 0x02);

	
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH1, 0x17);
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH2, 0x17);
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH3, 0x17);

	
	status = afe_write_byte(dev, ADC_CAL_ATEST_CH1, 0x10);
	status = afe_write_byte(dev, ADC_CAL_ATEST_CH2, 0x10);
	status = afe_write_byte(dev, ADC_CAL_ATEST_CH3, 0x10);
	msleep(5);

	
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH1, 0x07);
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH2, 0x07);
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH3, 0x07);

	
	status = afe_write_byte(dev, ADC_NTF_PRECLMP_EN_CH1, 0xf0);
	status = afe_write_byte(dev, ADC_NTF_PRECLMP_EN_CH2, 0xf0);
	status = afe_write_byte(dev, ADC_NTF_PRECLMP_EN_CH3, 0xf0);

	
	status = cx231xx_reg_mask_write(dev, AFE_DEVICE_ADDRESS, 8,
				   ADC_QGAIN_RES_TRM_CH1, 3, 7, 0x00);
	status = cx231xx_reg_mask_write(dev, AFE_DEVICE_ADDRESS, 8,
				   ADC_QGAIN_RES_TRM_CH2, 3, 7, 0x00);
	status = cx231xx_reg_mask_write(dev, AFE_DEVICE_ADDRESS, 8,
				   ADC_QGAIN_RES_TRM_CH3, 3, 7, 0x00);

	
	status = afe_write_byte(dev, ADC_DCSERVO_DEM_CH1, 0x03);
	status = afe_write_byte(dev, ADC_DCSERVO_DEM_CH2, 0x03);
	status = afe_write_byte(dev, ADC_DCSERVO_DEM_CH3, 0x03);

	return status;
}

int cx231xx_afe_setup_AFE_for_baseband(struct cx231xx *dev)
{
	u8 c_value = 0;
	int status = 0;

	status = afe_read_byte(dev, ADC_PWRDN_CLAMP_CH2, &c_value);
	c_value &= (~(0x50));
	status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2, c_value);

	return status;
}

int cx231xx_afe_set_input_mux(struct cx231xx *dev, u32 input_mux)
{
	u8 ch1_setting = (u8) input_mux;
	u8 ch2_setting = (u8) (input_mux >> 8);
	u8 ch3_setting = (u8) (input_mux >> 16);
	int status = 0;
	u8 value = 0;

	if (ch1_setting != 0) {
		status = afe_read_byte(dev, ADC_INPUT_CH1, &value);
		value &= ~INPUT_SEL_MASK;
		value |= (ch1_setting - 1) << 4;
		value &= 0xff;
		status = afe_write_byte(dev, ADC_INPUT_CH1, value);
	}

	if (ch2_setting != 0) {
		status = afe_read_byte(dev, ADC_INPUT_CH2, &value);
		value &= ~INPUT_SEL_MASK;
		value |= (ch2_setting - 1) << 4;
		value &= 0xff;
		status = afe_write_byte(dev, ADC_INPUT_CH2, value);
	}

	if (ch3_setting != 0) {
		status = afe_read_byte(dev, ADC_INPUT_CH3, &value);
		value &= ~INPUT_SEL_MASK;
		value |= (ch3_setting - 1) << 4;
		value &= 0xff;
		status = afe_write_byte(dev, ADC_INPUT_CH3, value);
	}

	return status;
}

int cx231xx_afe_set_mode(struct cx231xx *dev, enum AFE_MODE mode)
{
	int status = 0;


	switch (mode) {
	case AFE_MODE_LOW_IF:
		cx231xx_Setup_AFE_for_LowIF(dev);
		break;
	case AFE_MODE_BASEBAND:
		status = cx231xx_afe_setup_AFE_for_baseband(dev);
		break;
	case AFE_MODE_EU_HI_IF:
		
		break;
	case AFE_MODE_US_HI_IF:
		
		break;
	case AFE_MODE_JAPAN_HI_IF:
		
		break;
	}

	if ((mode != dev->afe_mode) &&
		(dev->video_input == CX231XX_VMUX_TELEVISION))
		status = cx231xx_afe_adjust_ref_count(dev,
						     CX231XX_VMUX_TELEVISION);

	dev->afe_mode = mode;

	return status;
}

int cx231xx_afe_update_power_control(struct cx231xx *dev,
					enum AV_MODE avmode)
{
	u8 afe_power_status = 0;
	int status = 0;

	switch (dev->model) {
	case CX231XX_BOARD_CNXT_CARRAERA:
	case CX231XX_BOARD_CNXT_RDE_250:
	case CX231XX_BOARD_CNXT_SHELBY:
	case CX231XX_BOARD_CNXT_RDU_250:
	case CX231XX_BOARD_CNXT_RDE_253S:
	case CX231XX_BOARD_CNXT_RDU_253S:
	case CX231XX_BOARD_CNXT_VIDEO_GRABBER:
	case CX231XX_BOARD_HAUPPAUGE_EXETER:
	case CX231XX_BOARD_HAUPPAUGE_USBLIVE2:
	case CX231XX_BOARD_PV_PLAYTV_USB_HYBRID:
	case CX231XX_BOARD_HAUPPAUGE_USB2_FM_PAL:
	case CX231XX_BOARD_HAUPPAUGE_USB2_FM_NTSC:
		if (avmode == POLARIS_AVMODE_ANALOGT_TV) {
			while (afe_power_status != (FLD_PWRDN_TUNING_BIAS |
						FLD_PWRDN_ENABLE_PLL)) {
				status = afe_write_byte(dev, SUP_BLK_PWRDN,
							FLD_PWRDN_TUNING_BIAS |
							FLD_PWRDN_ENABLE_PLL);
				status |= afe_read_byte(dev, SUP_BLK_PWRDN,
							&afe_power_status);
				if (status < 0)
					break;
			}

			status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
							0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
							0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
							0x00);
		} else if (avmode == POLARIS_AVMODE_DIGITAL) {
			status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
							0x70);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
							0x70);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
							0x70);

			status |= afe_read_byte(dev, SUP_BLK_PWRDN,
						  &afe_power_status);
			afe_power_status |= FLD_PWRDN_PD_BANDGAP |
						FLD_PWRDN_PD_BIAS |
						FLD_PWRDN_PD_TUNECK;
			status |= afe_write_byte(dev, SUP_BLK_PWRDN,
						   afe_power_status);
		} else if (avmode == POLARIS_AVMODE_ENXTERNAL_AV) {
			while (afe_power_status != (FLD_PWRDN_TUNING_BIAS |
						FLD_PWRDN_ENABLE_PLL)) {
				status = afe_write_byte(dev, SUP_BLK_PWRDN,
							FLD_PWRDN_TUNING_BIAS |
							FLD_PWRDN_ENABLE_PLL);
				status |= afe_read_byte(dev, SUP_BLK_PWRDN,
							&afe_power_status);
				if (status < 0)
					break;
			}

			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
						0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
						0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
						0x00);
		} else {
			cx231xx_info("Invalid AV mode input\n");
			status = -1;
		}
		break;
	default:
		if (avmode == POLARIS_AVMODE_ANALOGT_TV) {
			while (afe_power_status != (FLD_PWRDN_TUNING_BIAS |
						FLD_PWRDN_ENABLE_PLL)) {
				status = afe_write_byte(dev, SUP_BLK_PWRDN,
							FLD_PWRDN_TUNING_BIAS |
							FLD_PWRDN_ENABLE_PLL);
				status |= afe_read_byte(dev, SUP_BLK_PWRDN,
							&afe_power_status);
				if (status < 0)
					break;
			}

			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
							0x40);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
							0x40);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
							0x00);
		} else if (avmode == POLARIS_AVMODE_DIGITAL) {
			status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
							0x70);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
							0x70);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
							0x70);

			status |= afe_read_byte(dev, SUP_BLK_PWRDN,
						       &afe_power_status);
			afe_power_status |= FLD_PWRDN_PD_BANDGAP |
						FLD_PWRDN_PD_BIAS |
						FLD_PWRDN_PD_TUNECK;
			status |= afe_write_byte(dev, SUP_BLK_PWRDN,
							afe_power_status);
		} else if (avmode == POLARIS_AVMODE_ENXTERNAL_AV) {
			while (afe_power_status != (FLD_PWRDN_TUNING_BIAS |
						FLD_PWRDN_ENABLE_PLL)) {
				status = afe_write_byte(dev, SUP_BLK_PWRDN,
							FLD_PWRDN_TUNING_BIAS |
							FLD_PWRDN_ENABLE_PLL);
				status |= afe_read_byte(dev, SUP_BLK_PWRDN,
							&afe_power_status);
				if (status < 0)
					break;
			}

			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH1,
							0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH2,
							0x00);
			status |= afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3,
							0x40);
		} else {
			cx231xx_info("Invalid AV mode input\n");
			status = -1;
		}
	}			

	return status;
}

int cx231xx_afe_adjust_ref_count(struct cx231xx *dev, u32 video_input)
{
	u8 input_mode = 0;
	u8 ntf_mode = 0;
	int status = 0;

	dev->video_input = video_input;

	if (video_input == CX231XX_VMUX_TELEVISION) {
		status = afe_read_byte(dev, ADC_INPUT_CH3, &input_mode);
		status = afe_read_byte(dev, ADC_NTF_PRECLMP_EN_CH3,
					&ntf_mode);
	} else {
		status = afe_read_byte(dev, ADC_INPUT_CH1, &input_mode);
		status = afe_read_byte(dev, ADC_NTF_PRECLMP_EN_CH1,
					&ntf_mode);
	}

	input_mode = (ntf_mode & 0x3) | ((input_mode & 0x6) << 1);

	switch (input_mode) {
	case SINGLE_ENDED:
		dev->afe_ref_count = 0x23C;
		break;
	case LOW_IF:
		dev->afe_ref_count = 0x24C;
		break;
	case EU_IF:
		dev->afe_ref_count = 0x258;
		break;
	case US_IF:
		dev->afe_ref_count = 0x260;
		break;
	default:
		break;
	}

	status = cx231xx_afe_init_super_block(dev, dev->afe_ref_count);

	return status;
}

static int vid_blk_write_byte(struct cx231xx *dev, u16 saddr, u8 data)
{
	return cx231xx_write_i2c_data(dev, VID_BLK_I2C_ADDRESS,
					saddr, 2, data, 1);
}

static int vid_blk_read_byte(struct cx231xx *dev, u16 saddr, u8 *data)
{
	int status;
	u32 temp = 0;

	status = cx231xx_read_i2c_data(dev, VID_BLK_I2C_ADDRESS,
					saddr, 2, &temp, 1);
	*data = (u8) temp;
	return status;
}

static int vid_blk_write_word(struct cx231xx *dev, u16 saddr, u32 data)
{
	return cx231xx_write_i2c_data(dev, VID_BLK_I2C_ADDRESS,
					saddr, 2, data, 4);
}

static int vid_blk_read_word(struct cx231xx *dev, u16 saddr, u32 *data)
{
	return cx231xx_read_i2c_data(dev, VID_BLK_I2C_ADDRESS,
					saddr, 2, data, 4);
}
int cx231xx_check_fw(struct cx231xx *dev)
{
	u8 temp = 0;
	int status = 0;
	status = vid_blk_read_byte(dev, DL_CTL_ADDRESS_LOW, &temp);
	if (status < 0)
		return status;
	else
		return temp;

}

int cx231xx_set_video_input_mux(struct cx231xx *dev, u8 input)
{
	int status = 0;

	switch (INPUT(input)->type) {
	case CX231XX_VMUX_COMPOSITE1:
	case CX231XX_VMUX_SVIDEO:
		if ((dev->current_pcb_config.type == USB_BUS_POWER) &&
		    (dev->power_mode != POLARIS_AVMODE_ENXTERNAL_AV)) {
			
			status = cx231xx_set_power_mode(dev,
					POLARIS_AVMODE_ENXTERNAL_AV);
			if (status < 0) {
				cx231xx_errdev("%s: set_power_mode : Failed to"
						" set Power - errCode [%d]!\n",
						__func__, status);
				return status;
			}
		}
		status = cx231xx_set_decoder_video_input(dev,
							 INPUT(input)->type,
							 INPUT(input)->vmux);
		break;
	case CX231XX_VMUX_TELEVISION:
	case CX231XX_VMUX_CABLE:
		if ((dev->current_pcb_config.type == USB_BUS_POWER) &&
		    (dev->power_mode != POLARIS_AVMODE_ANALOGT_TV)) {
			
			status = cx231xx_set_power_mode(dev,
						POLARIS_AVMODE_ANALOGT_TV);
			if (status < 0) {
				cx231xx_errdev("%s: set_power_mode:Failed"
					" to set Power - errCode [%d]!\n",
					__func__, status);
				return status;
			}
		}
		if (dev->tuner_type == TUNER_NXP_TDA18271)
			status = cx231xx_set_decoder_video_input(dev,
							CX231XX_VMUX_TELEVISION,
							INPUT(input)->vmux);
		else
			status = cx231xx_set_decoder_video_input(dev,
							CX231XX_VMUX_COMPOSITE1,
							INPUT(input)->vmux);

		break;
	default:
		cx231xx_errdev("%s: set_power_mode : Unknown Input %d !\n",
		     __func__, INPUT(input)->type);
		break;
	}

	
	dev->video_input = input;

	return status;
}

int cx231xx_set_decoder_video_input(struct cx231xx *dev,
				u8 pin_type, u8 input)
{
	int status = 0;
	u32 value = 0;

	if (pin_type != dev->video_input) {
		status = cx231xx_afe_adjust_ref_count(dev, pin_type);
		if (status < 0) {
			cx231xx_errdev("%s: adjust_ref_count :Failed to set"
				"AFE input mux - errCode [%d]!\n",
				__func__, status);
			return status;
		}
	}

	
	status = cx231xx_afe_set_input_mux(dev, input);
	if (status < 0) {
		cx231xx_errdev("%s: set_input_mux :Failed to set"
				" AFE input mux - errCode [%d]!\n",
				__func__, status);
		return status;
	}

	switch (pin_type) {
	case CX231XX_VMUX_COMPOSITE1:
		status = vid_blk_read_word(dev, AFE_CTRL, &value);
		value |= (0 << 13) | (1 << 4);
		value &= ~(1 << 5);

		
		value &= (~(0x1ff8000));
		
		value |= 0x1000000;
		status = vid_blk_write_word(dev, AFE_CTRL, value);

		status = vid_blk_read_word(dev, OUT_CTRL1, &value);
		value |= (1 << 7);
		status = vid_blk_write_word(dev, OUT_CTRL1, value);

		
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							OUT_CTRL1,
							FLD_OUT_MODE,
							dev->board.output_mode);

		
		status = cx231xx_dif_set_standard(dev, DIF_USE_BASEBAND);
		if (status < 0) {
			cx231xx_errdev("%s: cx231xx_dif set to By pass"
						   " mode- errCode [%d]!\n",
				__func__, status);
			return status;
		}

		
		status = vid_blk_read_word(dev, DFE_CTRL1, &value);

		
		value |= FLD_VBI_GATE_EN;

		
		value |= FLD_VGA_AUTO_EN;

		
		status = vid_blk_write_word(dev, DFE_CTRL1, value);

		
		status = cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					MODE_CTRL, FLD_ACFG_DIS,
					cx231xx_set_field(FLD_ACFG_DIS, 1));

		
		status = cx231xx_read_modify_write_i2c_dword(dev,
			VID_BLK_I2C_ADDRESS,
			MODE_CTRL, FLD_INPUT_MODE,
			cx231xx_set_field(FLD_INPUT_MODE, INPUT_MODE_CVBS_0));
		break;
	case CX231XX_VMUX_SVIDEO:
		

		status = vid_blk_read_word(dev, AFE_CTRL, &value);

		
		value &= (~(0x1ff8000));
		value |= 0x1000010;
		status = vid_blk_write_word(dev, AFE_CTRL, value);

		
		status = cx231xx_dif_set_standard(dev, DIF_USE_BASEBAND);
		if (status < 0) {
			cx231xx_errdev("%s: cx231xx_dif set to By pass"
						   " mode- errCode [%d]!\n",
				__func__, status);
			return status;
		}

		
		status = vid_blk_read_word(dev, DFE_CTRL1, &value);

		
		value |= FLD_VBI_GATE_EN;

		
		value |= FLD_VGA_AUTO_EN;

		
		status = vid_blk_write_word(dev, DFE_CTRL1, value);

		
		status =  cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					MODE_CTRL, FLD_ACFG_DIS,
					cx231xx_set_field(FLD_ACFG_DIS, 1));

		
		status = cx231xx_read_modify_write_i2c_dword(dev,
			VID_BLK_I2C_ADDRESS,
			MODE_CTRL,
			FLD_INPUT_MODE,
			cx231xx_set_field(FLD_INPUT_MODE, INPUT_MODE_YC_1));

		
		status = vid_blk_read_word(dev, AFE_CTRL, &value);
		value |= FLD_CHROMA_IN_SEL;	

		value &= ~(FLD_VGA_SEL_CH2 | FLD_VGA_SEL_CH3);

		status = vid_blk_write_word(dev, AFE_CTRL, value);

		status = cx231xx_afe_set_mode(dev, AFE_MODE_BASEBAND);
		break;
	case CX231XX_VMUX_TELEVISION:
	case CX231XX_VMUX_CABLE:
	default:
		
		if (dev->board.tuner_type == TUNER_XC5000) {
			

			status = vid_blk_read_word(dev, AFE_CTRL, &value);
			value |= (0 << 13) | (1 << 4);
			value &= ~(1 << 5);

			
			value &= (~(0x1FF8000));
			
			value |= 0x1000000;
			status = vid_blk_write_word(dev, AFE_CTRL, value);

			status = vid_blk_read_word(dev, OUT_CTRL1, &value);
			value |= (1 << 7);
			status = vid_blk_write_word(dev, OUT_CTRL1, value);

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							OUT_CTRL1, FLD_OUT_MODE,
							dev->board.output_mode);

			
			status = cx231xx_dif_set_standard(dev,
							  DIF_USE_BASEBAND);
			if (status < 0) {
				cx231xx_errdev("%s: cx231xx_dif set to By pass"
						" mode- errCode [%d]!\n",
						__func__, status);
				return status;
			}

			
			status = vid_blk_read_word(dev, DFE_CTRL1, &value);

			
			value |= FLD_VBI_GATE_EN;

			
			value |= FLD_VGA_AUTO_EN;

			
			status = vid_blk_write_word(dev, DFE_CTRL1, value);

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					MODE_CTRL, FLD_ACFG_DIS,
					cx231xx_set_field(FLD_ACFG_DIS, 1));

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
				VID_BLK_I2C_ADDRESS,
				MODE_CTRL, FLD_INPUT_MODE,
				cx231xx_set_field(FLD_INPUT_MODE,
						INPUT_MODE_CVBS_0));
		} else {
			

			
			status = cx231xx_dif_set_standard(dev, dev->norm);
			if (status < 0) {
				cx231xx_errdev("%s: cx231xx_dif set to By pass"
						" mode- errCode [%d]!\n",
						__func__, status);
				return status;
			}

			
			status = vid_blk_read_word(dev, DIF_MISC_CTRL, &value);

			
			value &= ~FLD_DIF_DIF_BYPASS;

			
			status = vid_blk_write_word(dev, DIF_MISC_CTRL, value);

			
			status = vid_blk_read_word(dev, DFE_CTRL1, &value);

			
			value &= ~FLD_VBI_GATE_EN;

			value |= FLD_VGA_AUTO_EN | FLD_AGC_AUTO_EN | 0x00200000;

			
			status = vid_blk_write_word(dev, DFE_CTRL1, value);

			
			msleep(1);

			
			value &= ~(FLD_VGA_AUTO_EN);

			
			status = vid_blk_write_word(dev, DFE_CTRL1, value);

			
			status = vid_blk_read_word(dev, PIN_CTRL, &value);
			value |= (FLD_OEF_AGC_RF) |
				 (FLD_OEF_AGC_IFVGA) |
				 (FLD_OEF_AGC_IF);
			status = vid_blk_write_word(dev, PIN_CTRL, value);

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
						VID_BLK_I2C_ADDRESS,
						OUT_CTRL1, FLD_OUT_MODE,
						dev->board.output_mode);

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					MODE_CTRL, FLD_ACFG_DIS,
					cx231xx_set_field(FLD_ACFG_DIS, 1));

			
			status = cx231xx_read_modify_write_i2c_dword(dev,
				VID_BLK_I2C_ADDRESS,
				MODE_CTRL, FLD_INPUT_MODE,
				cx231xx_set_field(FLD_INPUT_MODE,
						INPUT_MODE_CVBS_0));

			
			
			
			status = vid_blk_read_word(dev, AFE_CTRL, &value);

			
			value &= (~(FLD_FUNC_MODE));
			value |= 0x800000;

			value |= FLD_VGA_SEL_CH3 | FLD_VGA_SEL_CH2;

			status = vid_blk_write_word(dev, AFE_CTRL, value);

			if (dev->tuner_type == TUNER_NXP_TDA18271) {
				status = vid_blk_read_word(dev, PIN_CTRL,
				 &value);
				status = vid_blk_write_word(dev, PIN_CTRL,
				 (value & 0xFFFFFFEF));
			}

			break;

		}
		break;
	}

	
	status = cx231xx_read_modify_write_i2c_dword(dev,
				VID_BLK_I2C_ADDRESS,
				OUT_CTRL1, FLD_VBIHACTRAW_EN,
				cx231xx_set_field(FLD_VBIHACTRAW_EN, 1));

	status = vid_blk_read_word(dev, OUT_CTRL1, &value);
	if (value & 0x02) {
		value |= (1 << 19);
		status = vid_blk_write_word(dev, OUT_CTRL1, value);
	}

	return status;
}

void cx231xx_enable656(struct cx231xx *dev)
{
	u8 temp = 0;
	int status;
	

	status = vid_blk_write_byte(dev, TS1_PIN_CTL0, 0xFF);

	

	status = vid_blk_read_byte(dev, TS1_PIN_CTL1, &temp);
	temp = temp|0x04;

	status = vid_blk_write_byte(dev, TS1_PIN_CTL1, temp);

}
EXPORT_SYMBOL_GPL(cx231xx_enable656);

void cx231xx_disable656(struct cx231xx *dev)
{
	u8 temp = 0;
	int status;


	status = vid_blk_write_byte(dev, TS1_PIN_CTL0, 0x00);

	status = vid_blk_read_byte(dev, TS1_PIN_CTL1, &temp);
	temp = temp&0xFB;

	status = vid_blk_write_byte(dev, TS1_PIN_CTL1, temp);
}
EXPORT_SYMBOL_GPL(cx231xx_disable656);

int cx231xx_do_mode_ctrl_overrides(struct cx231xx *dev)
{
	int status = 0;

	cx231xx_info("do_mode_ctrl_overrides : 0x%x\n",
		     (unsigned int)dev->norm);

	
	status = vid_blk_write_word(dev, DFE_CTRL3, 0xCD3F0280);

	if (dev->norm & (V4L2_STD_NTSC | V4L2_STD_PAL_M)) {
		cx231xx_info("do_mode_ctrl_overrides NTSC\n");

		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VBLANK_CNT, 0x18);
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VACTIVE_CNT,
							0x1E7000);
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_V656BLANK_CNT,
							0x1C000000);

		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							HORIZ_TIM_CTRL,
							FLD_HBLANK_CNT,
							cx231xx_set_field
							(FLD_HBLANK_CNT, 0x79));

	} else if (dev->norm & V4L2_STD_SECAM) {
		cx231xx_info("do_mode_ctrl_overrides SECAM\n");
		status =  cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VBLANK_CNT, 0x20);
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VACTIVE_CNT,
							cx231xx_set_field
							(FLD_VACTIVE_CNT,
							 0x244));
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_V656BLANK_CNT,
							cx231xx_set_field
							(FLD_V656BLANK_CNT,
							0x24));
		
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							HORIZ_TIM_CTRL,
							FLD_HBLANK_CNT,
							cx231xx_set_field
							(FLD_HBLANK_CNT, 0x85));
	} else {
		cx231xx_info("do_mode_ctrl_overrides PAL\n");
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VBLANK_CNT, 0x20);
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_VACTIVE_CNT,
							cx231xx_set_field
							(FLD_VACTIVE_CNT,
							 0x244));
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							VERT_TIM_CTRL,
							FLD_V656BLANK_CNT,
							cx231xx_set_field
							(FLD_V656BLANK_CNT,
							0x24));
		
		status = cx231xx_read_modify_write_i2c_dword(dev,
							VID_BLK_I2C_ADDRESS,
							HORIZ_TIM_CTRL,
							FLD_HBLANK_CNT,
							cx231xx_set_field
							(FLD_HBLANK_CNT, 0x85));

	}

	return status;
}

int cx231xx_unmute_audio(struct cx231xx *dev)
{
	return vid_blk_write_byte(dev, PATH1_VOL_CTL, 0x24);
}
EXPORT_SYMBOL_GPL(cx231xx_unmute_audio);

int stopAudioFirmware(struct cx231xx *dev)
{
	return vid_blk_write_byte(dev, DL_CTL_CONTROL, 0x03);
}

int restartAudioFirmware(struct cx231xx *dev)
{
	return vid_blk_write_byte(dev, DL_CTL_CONTROL, 0x13);
}

int cx231xx_set_audio_input(struct cx231xx *dev, u8 input)
{
	int status = 0;
	enum AUDIO_INPUT ainput = AUDIO_INPUT_LINE;

	switch (INPUT(input)->amux) {
	case CX231XX_AMUX_VIDEO:
		ainput = AUDIO_INPUT_TUNER_TV;
		break;
	case CX231XX_AMUX_LINE_IN:
		status = cx231xx_i2s_blk_set_audio_input(dev, input);
		ainput = AUDIO_INPUT_LINE;
		break;
	default:
		break;
	}

	status = cx231xx_set_audio_decoder_input(dev, ainput);

	return status;
}

int cx231xx_set_audio_decoder_input(struct cx231xx *dev,
				    enum AUDIO_INPUT audio_input)
{
	u32 dwval;
	int status;
	u8 gen_ctrl;
	u32 value = 0;

	
	status = vid_blk_read_byte(dev, GENERAL_CTL, &gen_ctrl);
	gen_ctrl |= 1;
	status = vid_blk_write_byte(dev, GENERAL_CTL, gen_ctrl);

	switch (audio_input) {
	case AUDIO_INPUT_LINE:
		
		value = cx231xx_set_field(FLD_AUD_CHAN1_SRC,
					  AUD_CHAN_SRC_PARALLEL);
		status = vid_blk_write_word(dev, AUD_IO_CTRL, value);

		status = vid_blk_read_word(dev, AC97_CTL, &dwval);

		status = vid_blk_write_word(dev, AC97_CTL,
					   (dwval | FLD_AC97_UP2X_BYPASS));

		
		status = vid_blk_write_word(dev, BAND_OUT_SEL,
				cx231xx_set_field(FLD_SRC3_IN_SEL, 0x0) |
				cx231xx_set_field(FLD_SRC3_CLK_SEL, 0x0) |
				cx231xx_set_field(FLD_PARALLEL1_SRC_SEL, 0x0));

		status = vid_blk_write_word(dev, DL_CTL, 0x3000001);
		status = vid_blk_write_word(dev, PATH1_CTL1, 0x00063073);

		
		status = vid_blk_read_word(dev, PATH1_VOL_CTL, &dwval);
		status = vid_blk_write_word(dev, PATH1_VOL_CTL,
					   (dwval | FLD_PATH1_AVC_THRESHOLD));

		
		status = vid_blk_read_word(dev, PATH1_SC_CTL, &dwval);
		status = vid_blk_write_word(dev, PATH1_SC_CTL,
					   (dwval | FLD_PATH1_SC_THRESHOLD));
		break;

	case AUDIO_INPUT_TUNER_TV:
	default:
		status = stopAudioFirmware(dev);
		
		status = vid_blk_write_word(dev, BAND_OUT_SEL,
			cx231xx_set_field(FLD_SRC6_IN_SEL, 0x00)         |
			cx231xx_set_field(FLD_SRC6_CLK_SEL, 0x01)        |
			cx231xx_set_field(FLD_SRC5_IN_SEL, 0x00)         |
			cx231xx_set_field(FLD_SRC5_CLK_SEL, 0x02)        |
			cx231xx_set_field(FLD_SRC4_IN_SEL, 0x02)         |
			cx231xx_set_field(FLD_SRC4_CLK_SEL, 0x03)        |
			cx231xx_set_field(FLD_SRC3_IN_SEL, 0x00)         |
			cx231xx_set_field(FLD_SRC3_CLK_SEL, 0x00)        |
			cx231xx_set_field(FLD_BASEBAND_BYPASS_CTL, 0x00) |
			cx231xx_set_field(FLD_AC97_SRC_SEL, 0x03)        |
			cx231xx_set_field(FLD_I2S_SRC_SEL, 0x00)         |
			cx231xx_set_field(FLD_PARALLEL2_SRC_SEL, 0x02)   |
			cx231xx_set_field(FLD_PARALLEL1_SRC_SEL, 0x01));

		
		status = vid_blk_write_word(dev, AUD_IO_CTRL,
			cx231xx_set_field(FLD_I2S_PORT_DIR, 0x00)  |
			cx231xx_set_field(FLD_I2S_OUT_SRC, 0x00)   |
			cx231xx_set_field(FLD_AUD_CHAN3_SRC, 0x00) |
			cx231xx_set_field(FLD_AUD_CHAN2_SRC, 0x00) |
			cx231xx_set_field(FLD_AUD_CHAN1_SRC, 0x03));

		status = vid_blk_write_word(dev, PATH1_CTL1, 0x1F063870);

		
		status = vid_blk_write_word(dev, PATH1_CTL1, 0x00063870);

		status = restartAudioFirmware(dev);

		switch (dev->board.tuner_type) {
		case TUNER_XC5000:
			
			status = cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					CHIP_CTRL,
					FLD_SIF_EN,
					cx231xx_set_field(FLD_SIF_EN, 1));
			break;
		case TUNER_NXP_TDA18271:
			
			status = cx231xx_read_modify_write_i2c_dword(dev,
					VID_BLK_I2C_ADDRESS,
					CHIP_CTRL,
					FLD_SIF_EN,
					cx231xx_set_field(FLD_SIF_EN, 0));
			break;
		default:
			printk(KERN_INFO "Unknown tuner type configuring SIF");
			break;
		}
		break;

	case AUDIO_INPUT_TUNER_FM:
		break;

	case AUDIO_INPUT_MUTE:
		status = vid_blk_write_word(dev, PATH1_CTL1, 0x1F011012);
		break;
	}

	
	status = vid_blk_read_byte(dev, GENERAL_CTL, &gen_ctrl);
	gen_ctrl &= ~1;
	status = vid_blk_write_byte(dev, GENERAL_CTL, gen_ctrl);

	return status;
}

int cx231xx_init_ctrl_pin_status(struct cx231xx *dev)
{
	u32 value;
	int status = 0;

	status = vid_blk_read_word(dev, PIN_CTRL, &value);
	value |= (~dev->board.ctl_pin_status_mask);
	status = vid_blk_write_word(dev, PIN_CTRL, value);

	return status;
}

int cx231xx_set_agc_analog_digital_mux_select(struct cx231xx *dev,
					      u8 analog_or_digital)
{
	int status = 0;

	
	status = cx231xx_set_gpio_direction(dev,
					    dev->board.
					    agc_analog_digital_select_gpio, 1);

	
	status = cx231xx_set_gpio_value(dev,
				   dev->board.agc_analog_digital_select_gpio,
				   analog_or_digital);

	return status;
}

int cx231xx_enable_i2c_port_3(struct cx231xx *dev, bool is_port_3)
{
	u8 value[4] = { 0, 0, 0, 0 };
	int status = 0;
	bool current_is_port_3;

	if (dev->board.dont_use_port_3)
		is_port_3 = false;
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER,
				       PWR_CTL_EN, value, 4);
	if (status < 0)
		return status;

	current_is_port_3 = value[0] & I2C_DEMOD_EN ? true : false;

	
	if (current_is_port_3 == is_port_3)
		return 0;

	if (is_port_3)
		value[0] |= I2C_DEMOD_EN;
	else
		value[0] &= ~I2C_DEMOD_EN;

	cx231xx_info("Changing the i2c master port to %d\n",
		     is_port_3 ?  3 : 1);

	status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
					PWR_CTL_EN, value, 4);

	return status;

}
EXPORT_SYMBOL_GPL(cx231xx_enable_i2c_port_3);

void update_HH_register_after_set_DIF(struct cx231xx *dev)
{
}

void cx231xx_dump_HH_reg(struct cx231xx *dev)
{
	u8 status = 0;
	u32 value = 0;
	u16  i = 0;

	value = 0x45005390;
	status = vid_blk_write_word(dev, 0x104, value);

	for (i = 0x100; i < 0x140; i++) {
		status = vid_blk_read_word(dev, i, &value);
		cx231xx_info("reg0x%x=0x%x\n", i, value);
		i = i+3;
	}

	for (i = 0x300; i < 0x400; i++) {
		status = vid_blk_read_word(dev, i, &value);
		cx231xx_info("reg0x%x=0x%x\n", i, value);
		i = i+3;
	}

	for (i = 0x400; i < 0x440; i++) {
		status = vid_blk_read_word(dev, i,  &value);
		cx231xx_info("reg0x%x=0x%x\n", i, value);
		i = i+3;
	}

	status = vid_blk_read_word(dev, AFE_CTRL_C2HH_SRC_CTRL, &value);
	cx231xx_info("AFE_CTRL_C2HH_SRC_CTRL=0x%x\n", value);
	vid_blk_write_word(dev, AFE_CTRL_C2HH_SRC_CTRL, 0x4485D390);
	status = vid_blk_read_word(dev, AFE_CTRL_C2HH_SRC_CTRL, &value);
	cx231xx_info("AFE_CTRL_C2HH_SRC_CTRL=0x%x\n", value);
}

void cx231xx_dump_SC_reg(struct cx231xx *dev)
{
	u8 value[4] = { 0, 0, 0, 0 };
	int status = 0;
	cx231xx_info("cx231xx_dump_SC_reg!\n");

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, BOARD_CFG_STAT,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", BOARD_CFG_STAT, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, TS_MODE_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", TS_MODE_REG, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, TS1_CFG_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", TS1_CFG_REG, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, TS1_LENGTH_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", TS1_LENGTH_REG, value[0],
				 value[1], value[2], value[3]);

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, TS2_CFG_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", TS2_CFG_REG, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, TS2_LENGTH_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", TS2_LENGTH_REG, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, EP_MODE_SET,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", EP_MODE_SET, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_PTN1,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_PTN1, value[0],
				 value[1], value[2], value[3]);

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_PTN2,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_PTN2, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_PTN3,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_PTN3, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_MASK0,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_MASK0, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_MASK1,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_MASK1, value[0],
				 value[1], value[2], value[3]);

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_PWR_MASK2,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_PWR_MASK2, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_GAIN,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_GAIN, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_CAR_REG,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_CAR_REG, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_OT_CFG1,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_OT_CFG1, value[0],
				 value[1], value[2], value[3]);

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, CIR_OT_CFG2,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", CIR_OT_CFG2, value[0],
				 value[1], value[2], value[3]);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, PWR_CTL_EN,
				 value, 4);
	cx231xx_info("reg0x%x=0x%x 0x%x 0x%x 0x%x\n", PWR_CTL_EN, value[0],
				 value[1], value[2], value[3]);


}

void cx231xx_Setup_AFE_for_LowIF(struct cx231xx *dev)

{
	u8 status = 0;
	u8 value = 0;



	status = afe_read_byte(dev, ADC_STATUS2_CH3, &value);
	value = (value & 0xFE)|0x01;
	status = afe_write_byte(dev, ADC_STATUS2_CH3, value);

	status = afe_read_byte(dev, ADC_STATUS2_CH3, &value);
	value = (value & 0xFE)|0x00;
	status = afe_write_byte(dev, ADC_STATUS2_CH3, value);



	status = afe_read_byte(dev, ADC_NTF_PRECLMP_EN_CH3, &value);
	value = (value & 0xFC)|0x00;
	status = afe_write_byte(dev, ADC_NTF_PRECLMP_EN_CH3, value);

	status = afe_read_byte(dev, ADC_INPUT_CH3, &value);
	value = (value & 0xF9)|0x02;
	status = afe_write_byte(dev, ADC_INPUT_CH3, value);

	status = afe_read_byte(dev, ADC_FB_FRCRST_CH3, &value);
	value = (value & 0xFB)|0x04;
	status = afe_write_byte(dev, ADC_FB_FRCRST_CH3, value);

	status = afe_read_byte(dev, ADC_DCSERVO_DEM_CH3, &value);
	value = (value & 0xFC)|0x03;
	status = afe_write_byte(dev, ADC_DCSERVO_DEM_CH3, value);

	status = afe_read_byte(dev, ADC_CTRL_DAC1_CH3, &value);
	value = (value & 0xFB)|0x04;
	status = afe_write_byte(dev, ADC_CTRL_DAC1_CH3, value);

	status = afe_read_byte(dev, ADC_CTRL_DAC23_CH3, &value);
	value = (value & 0xF8)|0x06;
	status = afe_write_byte(dev, ADC_CTRL_DAC23_CH3, value);

	status = afe_read_byte(dev, ADC_CTRL_DAC23_CH3, &value);
	value = (value & 0x8F)|0x40;
	status = afe_write_byte(dev, ADC_CTRL_DAC23_CH3, value);

	status = afe_read_byte(dev, ADC_PWRDN_CLAMP_CH3, &value);
	value = (value & 0xDF)|0x20;
	status = afe_write_byte(dev, ADC_PWRDN_CLAMP_CH3, value);
}

void cx231xx_set_Colibri_For_LowIF(struct cx231xx *dev, u32 if_freq,
		 u8 spectral_invert, u32 mode)
{
	u32 colibri_carrier_offset = 0;
	u8 status = 0;
	u32 func_mode = 0x01; 
	u32 standard = 0;
	u8 value[4] = { 0, 0, 0, 0 };

	cx231xx_info("Enter cx231xx_set_Colibri_For_LowIF()\n");
	value[0] = (u8) 0x6F;
	value[1] = (u8) 0x6F;
	value[2] = (u8) 0x6F;
	value[3] = (u8) 0x6F;
	status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
					PWR_CTL_EN, value, 4);

	
	status = cx231xx_afe_set_mode(dev, AFE_MODE_LOW_IF);

	
	standard = dev->norm;
	status = cx231xx_dif_configure_C2HH_for_low_IF(dev, dev->active_mode,
						       func_mode, standard);

	
	colibri_carrier_offset = cx231xx_Get_Colibri_CarrierOffset(mode,
								   standard);

	cx231xx_info("colibri_carrier_offset=%d, standard=0x%x\n",
		     colibri_carrier_offset, standard);

	
	cx231xx_set_DIF_bandpass(dev, (if_freq+colibri_carrier_offset),
				 spectral_invert, mode);
}

u32 cx231xx_Get_Colibri_CarrierOffset(u32 mode, u32 standerd)
{
	u32 colibri_carrier_offset = 0;

	if (mode == TUNER_MODE_FM_RADIO) {
		colibri_carrier_offset = 1100000;
	} else if (standerd & (V4L2_STD_MN | V4L2_STD_NTSC_M_JP)) {
		colibri_carrier_offset = 4832000;  
	} else if (standerd & (V4L2_STD_PAL_B | V4L2_STD_PAL_G)) {
		colibri_carrier_offset = 2700000;  
	} else if (standerd & (V4L2_STD_PAL_D | V4L2_STD_PAL_I
			| V4L2_STD_SECAM)) {
		colibri_carrier_offset = 2100000;  
	}

	return colibri_carrier_offset;
}

void cx231xx_set_DIF_bandpass(struct cx231xx *dev, u32 if_freq,
		 u8 spectral_invert, u32 mode)
{
	unsigned long pll_freq_word;
	int status = 0;
	u32 dif_misc_ctrl_value = 0;
	u64 pll_freq_u64 = 0;
	u32 i = 0;

	cx231xx_info("if_freq=%d;spectral_invert=0x%x;mode=0x%x\n",
			 if_freq, spectral_invert, mode);


	if (mode == TUNER_MODE_FM_RADIO) {
		pll_freq_word = 0x905A1CAC;
		status = vid_blk_write_word(dev, DIF_PLL_FREQ_WORD,  pll_freq_word);

	} else {
		
		pll_freq_word = if_freq;
		pll_freq_u64 = (u64)pll_freq_word << 28L;
		do_div(pll_freq_u64, 50000000);
		pll_freq_word = (u32)pll_freq_u64;
		
		status = vid_blk_write_word(dev, DIF_PLL_FREQ_WORD,  pll_freq_word);

	if (spectral_invert) {
		if_freq -= 400000;
		
		status = vid_blk_read_word(dev, DIF_MISC_CTRL,
					&dif_misc_ctrl_value);
		dif_misc_ctrl_value = dif_misc_ctrl_value | 0x00200000;
		status = vid_blk_write_word(dev, DIF_MISC_CTRL,
					dif_misc_ctrl_value);
	} else {
		if_freq += 400000;
		
		status = vid_blk_read_word(dev, DIF_MISC_CTRL,
					&dif_misc_ctrl_value);
		dif_misc_ctrl_value = dif_misc_ctrl_value & 0xFFDFFFFF;
		status = vid_blk_write_word(dev, DIF_MISC_CTRL,
					dif_misc_ctrl_value);
	}

	if_freq = (if_freq/100000)*100000;

	if (if_freq < 3000000)
		if_freq = 3000000;

	if (if_freq > 16000000)
		if_freq = 16000000;
	}

	cx231xx_info("Enter IF=%zd\n",
			sizeof(Dif_set_array)/sizeof(struct dif_settings));
	for (i = 0; i < sizeof(Dif_set_array)/sizeof(struct dif_settings); i++) {
		if (Dif_set_array[i].if_freq == if_freq) {
			status = vid_blk_write_word(dev,
			Dif_set_array[i].register_address, Dif_set_array[i].value);
		}
	}
}

int cx231xx_dif_configure_C2HH_for_low_IF(struct cx231xx *dev, u32 mode,
					  u32 function_mode, u32 standard)
{
	int status = 0;


	if (mode == V4L2_TUNER_RADIO) {
		
		
		status = cx231xx_reg_mask_write(dev,
				VID_BLK_I2C_ADDRESS, 32,
				AFE_CTRL_C2HH_SRC_CTRL, 30, 31, 0x1);
		
		status = cx231xx_reg_mask_write(dev,
				VID_BLK_I2C_ADDRESS, 32,
				AFE_CTRL_C2HH_SRC_CTRL, 23, 24, function_mode);
		
		status = cx231xx_reg_mask_write(dev,
				VID_BLK_I2C_ADDRESS, 32,
				AFE_CTRL_C2HH_SRC_CTRL, 15, 22, 0xFF);
		
		status = cx231xx_reg_mask_write(dev,
				VID_BLK_I2C_ADDRESS, 32,
				AFE_CTRL_C2HH_SRC_CTRL, 9, 9, 0x1);
	} else if (standard != DIF_USE_BASEBAND) {
		if (standard & V4L2_STD_MN) {
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 30, 31, 0x1);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 23, 24,
					function_mode);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 15, 22, 0xb);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 9, 9, 0x1);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AUD_IO_CTRL, 0, 31, 0x00000003);
		} else if ((standard == V4L2_STD_PAL_I) |
			(standard & V4L2_STD_PAL_D) |
			(standard & V4L2_STD_SECAM)) {
			
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 30, 31, 0x1);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 23, 24,
					function_mode);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 15, 22, 0xF);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 9, 9, 0x1);
		} else {
			
			
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 30, 31, 0x1);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 23, 24,
					function_mode);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 15, 22, 0xE);
			
			status = cx231xx_reg_mask_write(dev,
					VID_BLK_I2C_ADDRESS, 32,
					AFE_CTRL_C2HH_SRC_CTRL, 9, 9, 0x1);
		}
	}

	return status;
}

int cx231xx_dif_set_standard(struct cx231xx *dev, u32 standard)
{
	int status = 0;
	u32 dif_misc_ctrl_value = 0;
	u32 func_mode = 0;

	cx231xx_info("%s: setStandard to %x\n", __func__, standard);

	status = vid_blk_read_word(dev, DIF_MISC_CTRL, &dif_misc_ctrl_value);
	if (standard != DIF_USE_BASEBAND)
		dev->norm = standard;

	switch (dev->model) {
	case CX231XX_BOARD_CNXT_CARRAERA:
	case CX231XX_BOARD_CNXT_RDE_250:
	case CX231XX_BOARD_CNXT_SHELBY:
	case CX231XX_BOARD_CNXT_RDU_250:
	case CX231XX_BOARD_CNXT_VIDEO_GRABBER:
	case CX231XX_BOARD_HAUPPAUGE_EXETER:
		func_mode = 0x03;
		break;
	case CX231XX_BOARD_CNXT_RDE_253S:
	case CX231XX_BOARD_CNXT_RDU_253S:
	case CX231XX_BOARD_HAUPPAUGE_USB2_FM_PAL:
	case CX231XX_BOARD_HAUPPAUGE_USB2_FM_NTSC:
		func_mode = 0x01;
		break;
	default:
		func_mode = 0x01;
	}

	status = cx231xx_dif_configure_C2HH_for_low_IF(dev, dev->active_mode,
						  func_mode, standard);

	if (standard == DIF_USE_BASEBAND) {	
		status = vid_blk_write_word(dev, DIF_SRC_PHASE_INC, 0xDF7DF83);
		status = vid_blk_read_word(dev, DIF_MISC_CTRL,
						&dif_misc_ctrl_value);
		dif_misc_ctrl_value |= FLD_DIF_DIF_BYPASS;
		status = vid_blk_write_word(dev, DIF_MISC_CTRL,
						dif_misc_ctrl_value);
	} else if (standard & V4L2_STD_PAL_D) {
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL, 0, 31, 0x6503bc0c);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL1, 0, 31, 0xbd038c85);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL2, 0, 31, 0x1db4640a);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL3, 0, 31, 0x00008800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_REF, 0, 31, 0x444C1380);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_IF, 0, 31, 0xDA302600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_INT, 0, 31, 0xDA261700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_RF, 0, 31, 0xDA262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_INT_CURRENT, 0, 31,
					   0x26001700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_RF_CURRENT, 0, 31,
					   0x00002660);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VIDEO_AGC_CTRL, 0, 31,
					   0x72500800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VID_AUD_OVERRIDE, 0, 31,
					   0x27000100);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AV_SEP_CTRL, 0, 31, 0x3F3934EA);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_COMP_FLT_CTRL, 0, 31,
					   0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_PHASE_INC, 0, 31,
					   0x1befbf06);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_GAIN_CONTROL, 0, 31,
					   0x000035e8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_RPT_VARIANCE, 0, 31, 0x00000000);
		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a023F11;
	} else if (standard & V4L2_STD_PAL_I) {
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL, 0, 31, 0x6503bc0c);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL1, 0, 31, 0xbd038c85);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL2, 0, 31, 0x1db4640a);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL3, 0, 31, 0x00008800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_REF, 0, 31, 0x444C1380);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_IF, 0, 31, 0xDA302600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_INT, 0, 31, 0xDA261700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_RF, 0, 31, 0xDA262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_INT_CURRENT, 0, 31,
					   0x26001700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_RF_CURRENT, 0, 31,
					   0x00002660);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VIDEO_AGC_CTRL, 0, 31,
					   0x72500800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VID_AUD_OVERRIDE, 0, 31,
					   0x27000100);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AV_SEP_CTRL, 0, 31, 0x5F39A934);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_COMP_FLT_CTRL, 0, 31,
					   0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_PHASE_INC, 0, 31,
					   0x1befbf06);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_GAIN_CONTROL, 0, 31,
					   0x000035e8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_RPT_VARIANCE, 0, 31, 0x00000000);
		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a033F11;
	} else if (standard & V4L2_STD_PAL_M) {
		
		status = vid_blk_write_word(dev, DIF_PLL_CTRL, 0xFF01FF0C);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL1, 0xbd038c85);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL2, 0x1db4640a);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL3, 0x00008800);
		status = vid_blk_write_word(dev, DIF_AGC_IF_REF, 0x444C1380);
		status = vid_blk_write_word(dev, DIF_AGC_IF_INT_CURRENT,
						0x26001700);
		status = vid_blk_write_word(dev, DIF_AGC_RF_CURRENT,
						0x00002660);
		status = vid_blk_write_word(dev, DIF_VIDEO_AGC_CTRL,
						0x72500800);
		status = vid_blk_write_word(dev, DIF_VID_AUD_OVERRIDE,
						0x27000100);
		status = vid_blk_write_word(dev, DIF_AV_SEP_CTRL, 0x012c405d);
		status = vid_blk_write_word(dev, DIF_COMP_FLT_CTRL,
						0x009f50c1);
		status = vid_blk_write_word(dev, DIF_SRC_PHASE_INC,
						0x1befbf06);
		status = vid_blk_write_word(dev, DIF_SRC_GAIN_CONTROL,
						0x000035e8);
		status = vid_blk_write_word(dev, DIF_SOFT_RST_CTRL_REVB,
						0x00000000);
		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3A0A3F10;
	} else if (standard & (V4L2_STD_PAL_N | V4L2_STD_PAL_Nc)) {
		
		status = vid_blk_write_word(dev, DIF_PLL_CTRL, 0xFF01FF0C);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL1, 0xbd038c85);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL2, 0x1db4640a);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL3, 0x00008800);
		status = vid_blk_write_word(dev, DIF_AGC_IF_REF, 0x444C1380);
		status = vid_blk_write_word(dev, DIF_AGC_IF_INT_CURRENT,
						0x26001700);
		status = vid_blk_write_word(dev, DIF_AGC_RF_CURRENT,
						0x00002660);
		status = vid_blk_write_word(dev, DIF_VIDEO_AGC_CTRL,
						0x72500800);
		status = vid_blk_write_word(dev, DIF_VID_AUD_OVERRIDE,
						0x27000100);
		status = vid_blk_write_word(dev, DIF_AV_SEP_CTRL,
						0x012c405d);
		status = vid_blk_write_word(dev, DIF_COMP_FLT_CTRL,
						0x009f50c1);
		status = vid_blk_write_word(dev, DIF_SRC_PHASE_INC,
						0x1befbf06);
		status = vid_blk_write_word(dev, DIF_SRC_GAIN_CONTROL,
						0x000035e8);
		status = vid_blk_write_word(dev, DIF_SOFT_RST_CTRL_REVB,
						0x00000000);
		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value = 0x3A093F10;
	} else if (standard &
		  (V4L2_STD_SECAM_B | V4L2_STD_SECAM_D | V4L2_STD_SECAM_G |
		   V4L2_STD_SECAM_K | V4L2_STD_SECAM_K1)) {

		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL, 0, 31, 0x6503bc0c);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL1, 0, 31, 0xbd038c85);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL2, 0, 31, 0x1db4640a);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL3, 0, 31, 0x00008800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_REF, 0, 31, 0x888C0380);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_IF, 0, 31, 0xe0262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_INT, 0, 31, 0xc2171700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_RF, 0, 31, 0xc2262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_INT_CURRENT, 0, 31,
					   0x26001700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_RF_CURRENT, 0, 31,
					   0x00002660);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VID_AUD_OVERRIDE, 0, 31,
					   0x27000100);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AV_SEP_CTRL, 0, 31, 0x3F3530ec);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_COMP_FLT_CTRL, 0, 31,
					   0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_PHASE_INC, 0, 31,
					   0x1befbf06);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_GAIN_CONTROL, 0, 31,
					   0x000035e8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_RPT_VARIANCE, 0, 31, 0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VIDEO_AGC_CTRL, 0, 31,
					   0xf4000000);

		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a023F11;
	} else if (standard & (V4L2_STD_SECAM_L | V4L2_STD_SECAM_LC)) {
		
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL, 0, 31, 0x6503bc0c);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL1, 0, 31, 0xbd038c85);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL2, 0, 31, 0x1db4640a);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL3, 0, 31, 0x00008800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_REF, 0, 31, 0x888C0380);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_IF, 0, 31, 0xe0262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_INT, 0, 31, 0xc2171700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_RF, 0, 31, 0xc2262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_INT_CURRENT, 0, 31,
					   0x26001700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_RF_CURRENT, 0, 31,
					   0x00002660);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VID_AUD_OVERRIDE, 0, 31,
					   0x27000100);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AV_SEP_CTRL, 0, 31, 0x3F3530ec);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_COMP_FLT_CTRL, 0, 31,
					   0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_PHASE_INC, 0, 31,
					   0x1befbf06);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_GAIN_CONTROL, 0, 31,
					   0x000035e8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_RPT_VARIANCE, 0, 31, 0x00000000);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VIDEO_AGC_CTRL, 0, 31,
					   0xf2560000);

		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a023F11;

	} else if (standard & V4L2_STD_NTSC_M) {


		status = vid_blk_write_word(dev, DIF_PLL_CTRL, 0x6503BC0C);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL1, 0xBD038C85);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL2, 0x1DB4640A);
		status = vid_blk_write_word(dev, DIF_PLL_CTRL3, 0x00008800);
		status = vid_blk_write_word(dev, DIF_AGC_IF_REF, 0x444C0380);
		status = vid_blk_write_word(dev, DIF_AGC_IF_INT_CURRENT,
						0x26001700);
		status = vid_blk_write_word(dev, DIF_AGC_RF_CURRENT,
						0x00002660);
		status = vid_blk_write_word(dev, DIF_VIDEO_AGC_CTRL,
						0x04000800);
		status = vid_blk_write_word(dev, DIF_VID_AUD_OVERRIDE,
						0x27000100);
		status = vid_blk_write_word(dev, DIF_AV_SEP_CTRL, 0x01296e1f);

		status = vid_blk_write_word(dev, DIF_COMP_FLT_CTRL,
						0x009f50c1);
		status = vid_blk_write_word(dev, DIF_SRC_PHASE_INC,
						0x1befbf06);
		status = vid_blk_write_word(dev, DIF_SRC_GAIN_CONTROL,
						0x000035e8);

		status = vid_blk_write_word(dev, DIF_AGC_CTRL_IF, 0xC2262600);
		status = vid_blk_write_word(dev, DIF_AGC_CTRL_INT,
						0xC2262600);
		status = vid_blk_write_word(dev, DIF_AGC_CTRL_RF, 0xC2262600);

		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a003F10;
	} else {
		
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL, 0, 31, 0x6503bc0c);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL1, 0, 31, 0xbd038c85);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL2, 0, 31, 0x1db4640a);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_PLL_CTRL3, 0, 31, 0x00008800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_REF, 0, 31, 0x444C1380);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_IF, 0, 31, 0xDA302600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_INT, 0, 31, 0xDA261700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_CTRL_RF, 0, 31, 0xDA262600);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_IF_INT_CURRENT, 0, 31,
					   0x26001700);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AGC_RF_CURRENT, 0, 31,
					   0x00002660);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VIDEO_AGC_CTRL, 0, 31,
					   0x72500800);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_VID_AUD_OVERRIDE, 0, 31,
					   0x27000100);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_AV_SEP_CTRL, 0, 31, 0x3F3530EC);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_COMP_FLT_CTRL, 0, 31,
					   0x00A653A8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_PHASE_INC, 0, 31,
					   0x1befbf06);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_SRC_GAIN_CONTROL, 0, 31,
					   0x000035e8);
		status = cx231xx_reg_mask_write(dev, VID_BLK_I2C_ADDRESS, 32,
					   DIF_RPT_VARIANCE, 0, 31, 0x00000000);
		
		dif_misc_ctrl_value &= FLD_DIF_SPEC_INV;
		dif_misc_ctrl_value |= 0x3a013F11;
	}

	dif_misc_ctrl_value &= ~FLD_DIF_AUD_SRC_SEL;

	if (dev->active_mode == V4L2_TUNER_RADIO)
		dif_misc_ctrl_value = 0x7a080000;

	
	status = vid_blk_write_word(dev, DIF_MISC_CTRL, dif_misc_ctrl_value);

	return status;
}

int cx231xx_tuner_pre_channel_change(struct cx231xx *dev)
{
	int status = 0;
	u32 dwval;

	
	status = vid_blk_read_word(dev, DIF_AGC_IF_REF, &dwval);
	dwval &= ~(FLD_DIF_K_AGC_RF | FLD_DIF_K_AGC_IF);
	dwval |= 0x33000000;

	status = vid_blk_write_word(dev, DIF_AGC_IF_REF, dwval);

	return status;
}

int cx231xx_tuner_post_channel_change(struct cx231xx *dev)
{
	int status = 0;
	u32 dwval;
	cx231xx_info("cx231xx_tuner_post_channel_change  dev->tuner_type =0%d\n",
		     dev->tuner_type);
	status = vid_blk_read_word(dev, DIF_AGC_IF_REF, &dwval);
	dwval &= ~(FLD_DIF_K_AGC_RF | FLD_DIF_K_AGC_IF);

	if (dev->norm & (V4L2_STD_SECAM_L | V4L2_STD_SECAM_B |
			 V4L2_STD_SECAM_D)) {
			if (dev->tuner_type == TUNER_NXP_TDA18271) {
				dwval &= ~FLD_DIF_IF_REF;
				dwval |= 0x88000300;
			} else
				dwval |= 0x88000000;
		} else {
			if (dev->tuner_type == TUNER_NXP_TDA18271) {
				dwval &= ~FLD_DIF_IF_REF;
				dwval |= 0xCC000300;
			} else
				dwval |= 0x44000000;
		}

	status = vid_blk_write_word(dev, DIF_AGC_IF_REF, dwval);

	return status;
}

int cx231xx_i2s_blk_initialize(struct cx231xx *dev)
{
	int status = 0;
	u32 value;

	status = cx231xx_read_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
				       CH_PWR_CTRL1, 1, &value, 1);
	
	value |= 0x80;
	status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
					CH_PWR_CTRL1, 1, value, 1);
	
	status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
					CH_PWR_CTRL2, 1, 0x00, 1);

	return status;
}

int cx231xx_i2s_blk_update_power_control(struct cx231xx *dev,
					enum AV_MODE avmode)
{
	int status = 0;
	u32 value = 0;

	if (avmode != POLARIS_AVMODE_ENXTERNAL_AV) {
		status = cx231xx_read_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
					  CH_PWR_CTRL2, 1, &value, 1);
		value |= 0xfe;
		status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
						CH_PWR_CTRL2, 1, value, 1);
	} else {
		status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
						CH_PWR_CTRL2, 1, 0x00, 1);
	}

	return status;
}

int cx231xx_i2s_blk_set_audio_input(struct cx231xx *dev, u8 audio_input)
{
	int status = 0;

	switch (audio_input) {
	case CX231XX_AMUX_LINE_IN:
		status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
						CH_PWR_CTRL2, 1, 0x00, 1);
		status = cx231xx_write_i2c_data(dev, I2S_BLK_DEVICE_ADDRESS,
						CH_PWR_CTRL1, 1, 0x80, 1);
		break;
	case CX231XX_AMUX_VIDEO:
	default:
		break;
	}

	dev->ctl_ainput = audio_input;

	return status;
}

int cx231xx_set_power_mode(struct cx231xx *dev, enum AV_MODE mode)
{
	u8 value[4] = { 0, 0, 0, 0 };
	u32 tmp = 0;
	int status = 0;

	if (dev->power_mode != mode)
		dev->power_mode = mode;
	else {
		cx231xx_info(" setPowerMode::mode = %d, No Change req.\n",
			     mode);
		return 0;
	}

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, PWR_CTL_EN, value,
				       4);
	if (status < 0)
		return status;

	tmp = *((u32 *) value);

	switch (mode) {
	case POLARIS_AVMODE_ENXTERNAL_AV:

		tmp &= (~PWR_MODE_MASK);

		tmp |= PWR_AV_EN;
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
						PWR_CTL_EN, value, 4);
		msleep(PWR_SLEEP_INTERVAL);

		tmp |= PWR_ISO_EN;
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status =
		    cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER, PWR_CTL_EN,
					   value, 4);
		msleep(PWR_SLEEP_INTERVAL);

		tmp |= POLARIS_AVMODE_ENXTERNAL_AV;
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
						PWR_CTL_EN, value, 4);

		
		dev->xc_fw_load_done = 0;
		break;

	case POLARIS_AVMODE_ANALOGT_TV:

		tmp |= PWR_DEMOD_EN;
		tmp |= (I2C_DEMOD_EN);
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
						PWR_CTL_EN, value, 4);
		msleep(PWR_SLEEP_INTERVAL);

		if (!(tmp & PWR_TUNER_EN)) {
			tmp |= (PWR_TUNER_EN);
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}

		if (!(tmp & PWR_AV_EN)) {
			tmp |= PWR_AV_EN;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}
		if (!(tmp & PWR_ISO_EN)) {
			tmp |= PWR_ISO_EN;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}

		if (!(tmp & POLARIS_AVMODE_ANALOGT_TV)) {
			tmp |= POLARIS_AVMODE_ANALOGT_TV;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}

		if (dev->board.tuner_type != TUNER_ABSENT) {
			
			cx231xx_enable_i2c_port_3(dev, true);

			
			if (dev->board.tuner_gpio)
				cx231xx_gpio_set(dev, dev->board.tuner_gpio);

			if (dev->cx231xx_reset_analog_tuner)
				dev->cx231xx_reset_analog_tuner(dev);
		}

		break;

	case POLARIS_AVMODE_DIGITAL:
		if (!(tmp & PWR_TUNER_EN)) {
			tmp |= (PWR_TUNER_EN);
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}
		if (!(tmp & PWR_AV_EN)) {
			tmp |= PWR_AV_EN;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}
		if (!(tmp & PWR_ISO_EN)) {
			tmp |= PWR_ISO_EN;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}

		tmp &= (~PWR_AV_MODE);
		tmp |= POLARIS_AVMODE_DIGITAL | I2C_DEMOD_EN;
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
						PWR_CTL_EN, value, 4);
		msleep(PWR_SLEEP_INTERVAL);

		if (!(tmp & PWR_DEMOD_EN)) {
			tmp |= PWR_DEMOD_EN;
			value[0] = (u8) tmp;
			value[1] = (u8) (tmp >> 8);
			value[2] = (u8) (tmp >> 16);
			value[3] = (u8) (tmp >> 24);
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
							PWR_CTL_EN, value, 4);
			msleep(PWR_SLEEP_INTERVAL);
		}

		if (dev->board.tuner_type != TUNER_ABSENT) {
			if (dev->model == CX231XX_BOARD_HAUPPAUGE_EXETER)
				cx231xx_enable_i2c_port_3(dev, false);
			else
				cx231xx_enable_i2c_port_3(dev, true);

			
			if (dev->board.tuner_gpio)
				cx231xx_gpio_set(dev, dev->board.tuner_gpio);

			if (dev->cx231xx_reset_analog_tuner)
				dev->cx231xx_reset_analog_tuner(dev);
		}
		break;

	default:
		break;
	}

	msleep(PWR_SLEEP_INTERVAL);

	if (mode == POLARIS_AVMODE_DIGITAL) {
		tmp |= PWR_RESETOUT_EN;
		value[0] = (u8) tmp;
		value[1] = (u8) (tmp >> 8);
		value[2] = (u8) (tmp >> 16);
		value[3] = (u8) (tmp >> 24);
		status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
						PWR_CTL_EN, value, 4);
		msleep(PWR_SLEEP_INTERVAL);
	}

	
	status = cx231xx_afe_update_power_control(dev, mode);

	
	status = cx231xx_i2s_blk_update_power_control(dev, mode);

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, PWR_CTL_EN, value,
				       4);

	return status;
}

int cx231xx_power_suspend(struct cx231xx *dev)
{
	u8 value[4] = { 0, 0, 0, 0 };
	u32 tmp = 0;
	int status = 0;

	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, PWR_CTL_EN,
				       value, 4);
	if (status > 0)
		return status;

	tmp = *((u32 *) value);
	tmp &= (~PWR_MODE_MASK);

	value[0] = (u8) tmp;
	value[1] = (u8) (tmp >> 8);
	value[2] = (u8) (tmp >> 16);
	value[3] = (u8) (tmp >> 24);
	status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER, PWR_CTL_EN,
					value, 4);

	return status;
}

int cx231xx_start_stream(struct cx231xx *dev, u32 ep_mask)
{
	u8 value[4] = { 0x0, 0x0, 0x0, 0x0 };
	u32 tmp = 0;
	int status = 0;

	cx231xx_info("cx231xx_start_stream():: ep_mask = %x\n", ep_mask);
	status = cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, EP_MODE_SET,
				       value, 4);
	if (status < 0)
		return status;

	tmp = *((u32 *) value);
	tmp |= ep_mask;
	value[0] = (u8) tmp;
	value[1] = (u8) (tmp >> 8);
	value[2] = (u8) (tmp >> 16);
	value[3] = (u8) (tmp >> 24);

	status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER, EP_MODE_SET,
					value, 4);

	return status;
}

int cx231xx_stop_stream(struct cx231xx *dev, u32 ep_mask)
{
	u8 value[4] = { 0x0, 0x0, 0x0, 0x0 };
	u32 tmp = 0;
	int status = 0;

	cx231xx_info("cx231xx_stop_stream():: ep_mask = %x\n", ep_mask);
	status =
	    cx231xx_read_ctrl_reg(dev, VRT_GET_REGISTER, EP_MODE_SET, value, 4);
	if (status < 0)
		return status;

	tmp = *((u32 *) value);
	tmp &= (~ep_mask);
	value[0] = (u8) tmp;
	value[1] = (u8) (tmp >> 8);
	value[2] = (u8) (tmp >> 16);
	value[3] = (u8) (tmp >> 24);

	status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER, EP_MODE_SET,
					value, 4);

	return status;
}

int cx231xx_initialize_stream_xfer(struct cx231xx *dev, u32 media_type)
{
	int status = 0;
	u32 value = 0;
	u8 val[4] = { 0, 0, 0, 0 };

	if (dev->udev->speed == USB_SPEED_HIGH) {
		switch (media_type) {
		case 81: 
			cx231xx_info("%s: Audio enter HANC\n", __func__);
			status =
			    cx231xx_mode_register(dev, TS_MODE_REG, 0x9300);
			break;

		case 2:	
			cx231xx_info("%s: set vanc registers\n", __func__);
			status = cx231xx_mode_register(dev, TS_MODE_REG, 0x300);
			break;

		case 3:	
			cx231xx_info("%s: set hanc registers\n", __func__);
			status =
			    cx231xx_mode_register(dev, TS_MODE_REG, 0x1300);
			break;

		case 0:	
			cx231xx_info("%s: set video registers\n", __func__);
			status = cx231xx_mode_register(dev, TS_MODE_REG, 0x100);
			break;

		case 4:	
			cx231xx_info("%s: set ts1 registers", __func__);

		if (dev->board.has_417) {
			cx231xx_info(" MPEG\n");
			value &= 0xFFFFFFFC;
			value |= 0x3;

			status = cx231xx_mode_register(dev, TS_MODE_REG, value);

			val[0] = 0x04;
			val[1] = 0xA3;
			val[2] = 0x3B;
			val[3] = 0x00;
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
				 TS1_CFG_REG, val, 4);

			val[0] = 0x00;
			val[1] = 0x08;
			val[2] = 0x00;
			val[3] = 0x08;
			status = cx231xx_write_ctrl_reg(dev, VRT_SET_REGISTER,
				 TS1_LENGTH_REG, val, 4);

		} else {
			cx231xx_info(" BDA\n");
			status = cx231xx_mode_register(dev, TS_MODE_REG, 0x101);
			status = cx231xx_mode_register(dev, TS1_CFG_REG, 0x010);
		}
			break;

		case 6:	
			cx231xx_info("%s: set ts1 parallel mode registers\n",
				     __func__);
			status = cx231xx_mode_register(dev, TS_MODE_REG, 0x100);
			status = cx231xx_mode_register(dev, TS1_CFG_REG, 0x400);
			break;
		}
	} else {
		status = cx231xx_mode_register(dev, TS_MODE_REG, 0x101);
	}

	return status;
}

int cx231xx_capture_start(struct cx231xx *dev, int start, u8 media_type)
{
	int rc = -1;
	u32 ep_mask = -1;
	struct pcb_config *pcb_config;

	
	pcb_config = (struct pcb_config *)&dev->current_pcb_config;

	if (pcb_config->config_num == 1) {
		switch (media_type) {
		case 0:	
			ep_mask = ENABLE_EP4;	
			break;
		case 1:	
			ep_mask = ENABLE_EP3;	
			break;
		case 2:	
			ep_mask = ENABLE_EP5;	
			break;
		case 3:	
			ep_mask = ENABLE_EP6;	
			break;
		case 4:	
		case 6:	
			ep_mask = ENABLE_EP1;	
			break;
		case 5:	
			ep_mask = ENABLE_EP2;	
			break;
		}

	} else if (pcb_config->config_num > 1) {
		switch (media_type) {
		case 0:	
			ep_mask = ENABLE_EP4;	
			break;
		case 1:	
			ep_mask = ENABLE_EP3;	
			break;
		case 2:	
			ep_mask = ENABLE_EP5;	
			break;
		case 3:	
			ep_mask = ENABLE_EP6;	
			break;
		case 4:	
		case 6:	
			ep_mask = ENABLE_EP1;	
			break;
		case 5:	
			ep_mask = ENABLE_EP2;	
			break;
		}

	}

	if (start) {
		rc = cx231xx_initialize_stream_xfer(dev, media_type);

		if (rc < 0)
			return rc;

		
		if (ep_mask > 0)
			rc = cx231xx_start_stream(dev, ep_mask);
	} else {
		
		if (ep_mask > 0)
			rc = cx231xx_stop_stream(dev, ep_mask);
	}

	if (dev->mode == CX231XX_ANALOG_MODE)
		;
	else
		;

	return rc;
}
EXPORT_SYMBOL_GPL(cx231xx_capture_start);

int cx231xx_set_gpio_bit(struct cx231xx *dev, u32 gpio_bit, u8 *gpio_val)
{
	int status = 0;

	status = cx231xx_send_gpio_cmd(dev, gpio_bit, gpio_val, 4, 0, 0);

	return status;
}

int cx231xx_get_gpio_bit(struct cx231xx *dev, u32 gpio_bit, u8 *gpio_val)
{
	int status = 0;

	status = cx231xx_send_gpio_cmd(dev, gpio_bit, gpio_val, 4, 0, 1);

	return status;
}

int cx231xx_set_gpio_direction(struct cx231xx *dev,
			       int pin_number, int pin_value)
{
	int status = 0;
	u32 value = 0;

	
	if (pin_number >= 32)
		return -EINVAL;

	
	if (pin_value == 0)
		value = dev->gpio_dir & (~(1 << pin_number));	
	else
		value = dev->gpio_dir | (1 << pin_number);

	status = cx231xx_set_gpio_bit(dev, value, (u8 *) &dev->gpio_val);

	
	dev->gpio_dir = value;

	return status;
}

int cx231xx_set_gpio_value(struct cx231xx *dev, int pin_number, int pin_value)
{
	int status = 0;
	u32 value = 0;

	
	if (pin_number >= 32)
		return -EINVAL;

	
	if ((dev->gpio_dir & (1 << pin_number)) == 0x00) {
		
		value = dev->gpio_dir | (1 << pin_number);
		dev->gpio_dir = value;
		status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
					      (u8 *) &dev->gpio_val);
		value = 0;
	}

	if (pin_value == 0)
		value = dev->gpio_val & (~(1 << pin_number));
	else
		value = dev->gpio_val | (1 << pin_number);

	
	dev->gpio_val = value;

	
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	return status;
}

int cx231xx_gpio_i2c_start(struct cx231xx *dev)
{
	int status = 0;

	
	dev->gpio_dir |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_dir |= 1 << dev->board.tuner_sda_gpio;
	dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_val |= 1 << dev->board.tuner_sda_gpio;

	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	
	dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);

	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	
	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);

	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	return status;
}

int cx231xx_gpio_i2c_end(struct cx231xx *dev)
{
	int status = 0;

	
	dev->gpio_dir |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_dir |= 1 << dev->board.tuner_sda_gpio;

	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);

	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	
	dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);

	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	dev->gpio_dir &= ~(1 << dev->board.tuner_scl_gpio);
	dev->gpio_dir &= ~(1 << dev->board.tuner_sda_gpio);

	status =
	    cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);
	if (status < 0)
		return -EINVAL;

	return status;
}

int cx231xx_gpio_i2c_write_byte(struct cx231xx *dev, u8 data)
{
	int status = 0;
	u8 i;

	
	dev->gpio_dir |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_dir |= 1 << dev->board.tuner_sda_gpio;

	for (i = 0; i < 8; i++) {
		if (((data << i) & 0x80) == 0) {
			
			dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
			dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);

			
			dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);

			
			dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);
		} else {
			
			dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
			dev->gpio_val |= 1 << dev->board.tuner_sda_gpio;
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);

			
			dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);

			
			dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
			status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
						      (u8 *)&dev->gpio_val);
		}
	}
	return status;
}

int cx231xx_gpio_i2c_read_byte(struct cx231xx *dev, u8 *buf)
{
	u8 value = 0;
	int status = 0;
	u32 gpio_logic_value = 0;
	u8 i;

	
	for (i = 0; i < 8; i++) {	

		
		dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
		status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
					      (u8 *)&dev->gpio_val);

		
		dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
		status = cx231xx_set_gpio_bit(dev, dev->gpio_dir,
					      (u8 *)&dev->gpio_val);

		
		gpio_logic_value = dev->gpio_val;
		status = cx231xx_get_gpio_bit(dev, dev->gpio_dir,
					      (u8 *)&dev->gpio_val);
		if ((dev->gpio_val & (1 << dev->board.tuner_sda_gpio)) != 0)
			value |= (1 << (8 - i - 1));

		dev->gpio_val = gpio_logic_value;
	}

	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	*buf = value & 0xff;

	return status;
}

int cx231xx_gpio_i2c_read_ack(struct cx231xx *dev)
{
	int status = 0;
	u32 gpio_logic_value = 0;
	int nCnt = 10;
	int nInit = nCnt;

	dev->gpio_dir &= ~(1 << dev->board.tuner_sda_gpio);
	dev->gpio_dir &= ~(1 << dev->board.tuner_scl_gpio);

	gpio_logic_value = dev->gpio_val;
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	do {
		msleep(2);
		status = cx231xx_get_gpio_bit(dev, dev->gpio_dir,
					      (u8 *)&dev->gpio_val);
		nCnt--;
	} while (((dev->gpio_val &
			  (1 << dev->board.tuner_scl_gpio)) == 0) &&
			 (nCnt > 0));

	if (nCnt == 0)
		cx231xx_info("No ACK after %d msec -GPIO I2C failed!",
			     nInit * 10);

	status = cx231xx_get_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	if ((dev->gpio_val & 1 << dev->board.tuner_sda_gpio) == 0) {
		dev->gpio_val = gpio_logic_value;
		dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);
		status = 0;
	} else {
		dev->gpio_val = gpio_logic_value;
		dev->gpio_val |= (1 << dev->board.tuner_sda_gpio);
	}

	dev->gpio_val = gpio_logic_value;
	dev->gpio_dir |= (1 << dev->board.tuner_scl_gpio);
	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	return status;
}

int cx231xx_gpio_i2c_write_ack(struct cx231xx *dev)
{
	int status = 0;

	
	dev->gpio_dir |= 1 << dev->board.tuner_sda_gpio;
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_val &= ~(1 << dev->board.tuner_sda_gpio);
	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_dir &= ~(1 << dev->board.tuner_sda_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	return status;
}

int cx231xx_gpio_i2c_write_nak(struct cx231xx *dev)
{
	int status = 0;

	
	dev->gpio_dir |= 1 << dev->board.tuner_scl_gpio;
	dev->gpio_dir &= ~(1 << dev->board.tuner_sda_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_val &= ~(1 << dev->board.tuner_scl_gpio);
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	
	dev->gpio_val |= 1 << dev->board.tuner_scl_gpio;
	status = cx231xx_set_gpio_bit(dev, dev->gpio_dir, (u8 *)&dev->gpio_val);

	return status;
}

int cx231xx_gpio_i2c_read(struct cx231xx *dev, u8 dev_addr, u8 *buf, u8 len)
{
	int status = 0;
	int i = 0;

	
	mutex_lock(&dev->gpio_i2c_lock);

	
	status = cx231xx_gpio_i2c_start(dev);

	
	status = cx231xx_gpio_i2c_write_byte(dev, (dev_addr << 1) + 1);

	
	status = cx231xx_gpio_i2c_read_ack(dev);

	
	for (i = 0; i < len; i++) {
		
		buf[i] = 0;
		status = cx231xx_gpio_i2c_read_byte(dev, &buf[i]);

		if ((i + 1) != len) {
			
			status = cx231xx_gpio_i2c_write_ack(dev);
		}
	}

	
	status = cx231xx_gpio_i2c_write_nak(dev);

	
	status = cx231xx_gpio_i2c_end(dev);

	
	mutex_unlock(&dev->gpio_i2c_lock);

	return status;
}

int cx231xx_gpio_i2c_write(struct cx231xx *dev, u8 dev_addr, u8 *buf, u8 len)
{
	int status = 0;
	int i = 0;

	
	mutex_lock(&dev->gpio_i2c_lock);

	
	status = cx231xx_gpio_i2c_start(dev);

	
	status = cx231xx_gpio_i2c_write_byte(dev, dev_addr << 1);

	
	status = cx231xx_gpio_i2c_read_ack(dev);

	for (i = 0; i < len; i++) {
		
		status = cx231xx_gpio_i2c_write_byte(dev, buf[i]);

		
		status = cx231xx_gpio_i2c_read_ack(dev);
	}

	
	status = cx231xx_gpio_i2c_end(dev);

	
	mutex_unlock(&dev->gpio_i2c_lock);

	return 0;
}
