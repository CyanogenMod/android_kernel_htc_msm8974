/*
 * tvp5150 - Texas Instruments TVP5150A/AM1 video decoder registers
 *
 * Copyright (c) 2005,2006 Mauro Carvalho Chehab (mchehab@infradead.org)
 * This code is placed under the terms of the GNU General Public License v2
 */

#define TVP5150_VD_IN_SRC_SEL_1      0x00 
#define TVP5150_ANAL_CHL_CTL         0x01 
#define TVP5150_OP_MODE_CTL          0x02 
#define TVP5150_MISC_CTL             0x03 
#define TVP5150_AUTOSW_MSK           0x04 


#define TVP5150_COLOR_KIL_THSH_CTL   0x06 
#define TVP5150_LUMA_PROC_CTL_1      0x07 
#define TVP5150_LUMA_PROC_CTL_2      0x08 
#define TVP5150_BRIGHT_CTL           0x09 
#define TVP5150_SATURATION_CTL       0x0a 
#define TVP5150_HUE_CTL              0x0b 
#define TVP5150_CONTRAST_CTL         0x0c 
#define TVP5150_DATA_RATE_SEL        0x0d 
#define TVP5150_LUMA_PROC_CTL_3      0x0e 
#define TVP5150_CONF_SHARED_PIN      0x0f 


#define TVP5150_ACT_VD_CROP_ST_MSB   0x11 
#define TVP5150_ACT_VD_CROP_ST_LSB   0x12 
#define TVP5150_ACT_VD_CROP_STP_MSB  0x13 
#define TVP5150_ACT_VD_CROP_STP_LSB  0x14 
#define TVP5150_GENLOCK              0x15 
#define TVP5150_HORIZ_SYNC_START     0x16 


#define TVP5150_VERT_BLANKING_START 0x18 
#define TVP5150_VERT_BLANKING_STOP  0x19 
#define TVP5150_CHROMA_PROC_CTL_1   0x1a 
#define TVP5150_CHROMA_PROC_CTL_2   0x1b 
#define TVP5150_INT_RESET_REG_B     0x1c 
#define TVP5150_INT_ENABLE_REG_B    0x1d 
#define TVP5150_INTT_CONFIG_REG_B   0x1e 


#define VIDEO_STD_MASK			 (0x07 >> 1)
#define TVP5150_VIDEO_STD                0x28 
#define VIDEO_STD_AUTO_SWITCH_BIT	 0x00
#define VIDEO_STD_NTSC_MJ_BIT		 0x02
#define VIDEO_STD_PAL_BDGHIN_BIT	 0x04
#define VIDEO_STD_PAL_M_BIT		 0x06
#define VIDEO_STD_PAL_COMBINATION_N_BIT	 0x08
#define VIDEO_STD_NTSC_4_43_BIT		 0x0a
#define VIDEO_STD_SECAM_BIT		 0x0c

#define VIDEO_STD_NTSC_MJ_BIT_AS                 0x01
#define VIDEO_STD_PAL_BDGHIN_BIT_AS              0x03
#define VIDEO_STD_PAL_M_BIT_AS		         0x05
#define VIDEO_STD_PAL_COMBINATION_N_BIT_AS	 0x07
#define VIDEO_STD_NTSC_4_43_BIT_AS		 0x09
#define VIDEO_STD_SECAM_BIT_AS		         0x0b


#define TVP5150_CB_GAIN_FACT        0x2c 
#define TVP5150_CR_GAIN_FACTOR      0x2d 
#define TVP5150_MACROVISION_ON_CTR  0x2e 
#define TVP5150_MACROVISION_OFF_CTR 0x2f 
#define TVP5150_REV_SELECT          0x30 


#define TVP5150_MSB_DEV_ID          0x80 
#define TVP5150_LSB_DEV_ID          0x81 
#define TVP5150_ROM_MAJOR_VER       0x82 
#define TVP5150_ROM_MINOR_VER       0x83 
#define TVP5150_VERT_LN_COUNT_MSB   0x84 
#define TVP5150_VERT_LN_COUNT_LSB   0x85 
#define TVP5150_INT_STATUS_REG_B    0x86 
#define TVP5150_INT_ACTIVE_REG_B    0x87 
#define TVP5150_STATUS_REG_1        0x88 
#define TVP5150_STATUS_REG_2        0x89 
#define TVP5150_STATUS_REG_3        0x8a 
#define TVP5150_STATUS_REG_4        0x8b 
#define TVP5150_STATUS_REG_5        0x8c 
 
#define TVP5150_CC_DATA_INI         0x90
#define TVP5150_CC_DATA_END         0x93

 
#define TVP5150_WSS_DATA_INI        0x94
#define TVP5150_WSS_DATA_END        0x99

#define TVP5150_VPS_DATA_INI        0x9a
#define TVP5150_VPS_DATA_END        0xa6

#define TVP5150_VITC_DATA_INI       0xa7
#define TVP5150_VITC_DATA_END       0xaf

#define TVP5150_VBI_FIFO_READ_DATA  0xb0 

#define TVP5150_TELETEXT_FIL1_INI  0xb1
#define TVP5150_TELETEXT_FIL1_END  0xb5

#define TVP5150_TELETEXT_FIL2_INI  0xb6
#define TVP5150_TELETEXT_FIL2_END  0xba

#define TVP5150_TELETEXT_FIL_ENA    0xbb 
#define TVP5150_INT_STATUS_REG_A    0xc0 
#define TVP5150_INT_ENABLE_REG_A    0xc1 
#define TVP5150_INT_CONF            0xc2 
#define TVP5150_VDP_CONF_RAM_DATA   0xc3 
#define TVP5150_CONF_RAM_ADDR_LOW   0xc4 
#define TVP5150_CONF_RAM_ADDR_HIGH  0xc5 
#define TVP5150_VDP_STATUS_REG      0xc6 
#define TVP5150_FIFO_WORD_COUNT     0xc7 
#define TVP5150_FIFO_INT_THRESHOLD  0xc8 
#define TVP5150_FIFO_RESET          0xc9 
#define TVP5150_LINE_NUMBER_INT     0xca 
#define TVP5150_PIX_ALIGN_REG_LOW   0xcb 
#define TVP5150_PIX_ALIGN_REG_HIGH  0xcc 
#define TVP5150_FIFO_OUT_CTRL       0xcd 
#define TVP5150_FULL_FIELD_ENA      0xcf 

#define TVP5150_LINE_MODE_INI       0xd0
#define TVP5150_LINE_MODE_END       0xfb

#define TVP5150_FULL_FIELD_MODE_REG 0xfc 
