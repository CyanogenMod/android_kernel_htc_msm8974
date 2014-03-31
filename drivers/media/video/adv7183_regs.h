/*
 * adv7183 - Analog Devices ADV7183 video decoder registers
 *
 * Copyright (c) 2011 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ADV7183_REGS_H_
#define _ADV7183_REGS_H_

#define ADV7183_IN_CTRL            0x00 
#define ADV7183_VD_SEL             0x01 
#define ADV7183_OUT_CTRL           0x03 
#define ADV7183_EXT_OUT_CTRL       0x04 
#define ADV7183_AUTO_DET_EN        0x07 
#define ADV7183_CONTRAST           0x08 
#define ADV7183_BRIGHTNESS         0x0A 
#define ADV7183_HUE                0x0B 
#define ADV7183_DEF_Y              0x0C 
#define ADV7183_DEF_C              0x0D 
#define ADV7183_ADI_CTRL           0x0E 
#define ADV7183_POW_MANAGE         0x0F 
#define ADV7183_STATUS_1           0x10 
#define ADV7183_IDENT              0x11 
#define ADV7183_STATUS_2           0x12 
#define ADV7183_STATUS_3           0x13 
#define ADV7183_ANAL_CLAMP_CTRL    0x14 
#define ADV7183_DIGI_CLAMP_CTRL_1  0x15 
#define ADV7183_SHAP_FILT_CTRL     0x17 
#define ADV7183_SHAP_FILT_CTRL_2   0x18 
#define ADV7183_COMB_FILT_CTRL     0x19 
#define ADV7183_ADI_CTRL_2         0x1D 
#define ADV7183_PIX_DELAY_CTRL     0x27 
#define ADV7183_MISC_GAIN_CTRL     0x2B 
#define ADV7183_AGC_MODE_CTRL      0x2C 
#define ADV7183_CHRO_GAIN_CTRL_1   0x2D 
#define ADV7183_CHRO_GAIN_CTRL_2   0x2E 
#define ADV7183_LUMA_GAIN_CTRL_1   0x2F 
#define ADV7183_LUMA_GAIN_CTRL_2   0x30 
#define ADV7183_VS_FIELD_CTRL_1    0x31 
#define ADV7183_VS_FIELD_CTRL_2    0x32 
#define ADV7183_VS_FIELD_CTRL_3    0x33 
#define ADV7183_HS_POS_CTRL_1      0x34 
#define ADV7183_HS_POS_CTRL_2      0x35 
#define ADV7183_HS_POS_CTRL_3      0x36 
#define ADV7183_POLARITY           0x37 
#define ADV7183_NTSC_COMB_CTRL     0x38 
#define ADV7183_PAL_COMB_CTRL      0x39 
#define ADV7183_ADC_CTRL           0x3A 
#define ADV7183_MAN_WIN_CTRL       0x3D 
#define ADV7183_RESAMPLE_CTRL      0x41 
#define ADV7183_GEMSTAR_CTRL_1     0x48 
#define ADV7183_GEMSTAR_CTRL_2     0x49 
#define ADV7183_GEMSTAR_CTRL_3     0x4A 
#define ADV7183_GEMSTAR_CTRL_4     0x4B 
#define ADV7183_GEMSTAR_CTRL_5     0x4C 
#define ADV7183_CTI_DNR_CTRL_1     0x4D 
#define ADV7183_CTI_DNR_CTRL_2     0x4E 
#define ADV7183_CTI_DNR_CTRL_4     0x50 
#define ADV7183_LOCK_CNT           0x51 
#define ADV7183_FREE_LINE_LEN      0x8F 
#define ADV7183_VBI_INFO           0x90 
#define ADV7183_WSS_1              0x91 
#define ADV7183_WSS_2              0x92 
#define ADV7183_EDTV_1             0x93 
#define ADV7183_EDTV_2             0x94 
#define ADV7183_EDTV_3             0x95 
#define ADV7183_CGMS_1             0x96 
#define ADV7183_CGMS_2             0x97 
#define ADV7183_CGMS_3             0x98 
#define ADV7183_CCAP_1             0x99 
#define ADV7183_CCAP_2             0x9A 
#define ADV7183_LETTERBOX_1        0x9B 
#define ADV7183_LETTERBOX_2        0x9C 
#define ADV7183_LETTERBOX_3        0x9D 
#define ADV7183_CRC_EN             0xB2 
#define ADV7183_ADC_SWITCH_1       0xC3 
#define ADV7183_ADC_SWITCH_2       0xC4 
#define ADV7183_LETTERBOX_CTRL_1   0xDC 
#define ADV7183_LETTERBOX_CTRL_2   0xDD 
#define ADV7183_SD_OFFSET_CB       0xE1 
#define ADV7183_SD_OFFSET_CR       0xE2 
#define ADV7183_SD_SATURATION_CB   0xE3 
#define ADV7183_SD_SATURATION_CR   0xE4 
#define ADV7183_NTSC_V_BEGIN       0xE5 
#define ADV7183_NTSC_V_END         0xE6 
#define ADV7183_NTSC_F_TOGGLE      0xE7 
#define ADV7183_PAL_V_BEGIN        0xE8 
#define ADV7183_PAL_V_END          0xE9 
#define ADV7183_PAL_F_TOGGLE       0xEA 
#define ADV7183_DRIVE_STR          0xF4 
#define ADV7183_IF_COMP_CTRL       0xF8 
#define ADV7183_VS_MODE_CTRL       0xF9 

#endif
