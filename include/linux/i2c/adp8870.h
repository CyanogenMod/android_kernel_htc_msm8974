/*
 * Definitions and platform data for Analog Devices
 * Backlight drivers ADP8870
 *
 * Copyright 2009-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __LINUX_I2C_ADP8870_H
#define __LINUX_I2C_ADP8870_H

#define ID_ADP8870		8870

#define ADP8870_MAX_BRIGHTNESS	0x7F
#define FLAG_OFFT_SHIFT 8


#define ADP8870_LED_DIS_BLINK	(0 << FLAG_OFFT_SHIFT)
#define ADP8870_LED_OFFT_600ms	(1 << FLAG_OFFT_SHIFT)
#define ADP8870_LED_OFFT_1200ms	(2 << FLAG_OFFT_SHIFT)
#define ADP8870_LED_OFFT_1800ms	(3 << FLAG_OFFT_SHIFT)

#define ADP8870_LED_ONT_200ms	0
#define ADP8870_LED_ONT_600ms	1
#define ADP8870_LED_ONT_800ms	2
#define ADP8870_LED_ONT_1200ms	3

#define ADP8870_LED_D7		(7)
#define ADP8870_LED_D6		(6)
#define ADP8870_LED_D5		(5)
#define ADP8870_LED_D4		(4)
#define ADP8870_LED_D3		(3)
#define ADP8870_LED_D2		(2)
#define ADP8870_LED_D1		(1)


#define ADP8870_BL_D7		(1 << 6)
#define ADP8870_BL_D6		(1 << 5)
#define ADP8870_BL_D5		(1 << 4)
#define ADP8870_BL_D4		(1 << 3)
#define ADP8870_BL_D3		(1 << 2)
#define ADP8870_BL_D2		(1 << 1)
#define ADP8870_BL_D1		(1 << 0)

#define ADP8870_FADE_T_DIS	0	
#define ADP8870_FADE_T_300ms	1	
#define ADP8870_FADE_T_600ms	2
#define ADP8870_FADE_T_900ms	3
#define ADP8870_FADE_T_1200ms	4
#define ADP8870_FADE_T_1500ms	5
#define ADP8870_FADE_T_1800ms	6
#define ADP8870_FADE_T_2100ms	7
#define ADP8870_FADE_T_2400ms	8
#define ADP8870_FADE_T_2700ms	9
#define ADP8870_FADE_T_3000ms	10
#define ADP8870_FADE_T_3500ms	11
#define ADP8870_FADE_T_4000ms	12
#define ADP8870_FADE_T_4500ms	13
#define ADP8870_FADE_T_5000ms	14
#define ADP8870_FADE_T_5500ms	15	

#define ADP8870_FADE_LAW_LINEAR	0
#define ADP8870_FADE_LAW_SQUARE	1
#define ADP8870_FADE_LAW_CUBIC1	2
#define ADP8870_FADE_LAW_CUBIC2	3

#define ADP8870_BL_AMBL_FILT_80ms	0	
#define ADP8870_BL_AMBL_FILT_160ms	1
#define ADP8870_BL_AMBL_FILT_320ms	2
#define ADP8870_BL_AMBL_FILT_640ms	3
#define ADP8870_BL_AMBL_FILT_1280ms	4
#define ADP8870_BL_AMBL_FILT_2560ms	5
#define ADP8870_BL_AMBL_FILT_5120ms	6
#define ADP8870_BL_AMBL_FILT_10240ms	7	

#define ADP8870_BL_CUR_mA(I)		((I * 127) / 30)

#define ADP8870_L2_COMP_CURR_uA(I)	((I * 255) / 1106)

#define ADP8870_L3_COMP_CURR_uA(I)	((I * 255) / 551)

#define ADP8870_L4_COMP_CURR_uA(I)	((I * 255) / 275)

#define ADP8870_L5_COMP_CURR_uA(I)	((I * 255) / 138)

struct adp8870_backlight_platform_data {
	u8 bl_led_assign;	
	u8 pwm_assign;		

	u8 bl_fade_in;		
	u8 bl_fade_out;		
	u8 bl_fade_law;		

	u8 en_ambl_sens;	
	u8 abml_filt;		

	u8 l1_daylight_max;	
	u8 l1_daylight_dim;	
	u8 l2_bright_max;	
	u8 l2_bright_dim;	
	u8 l3_office_max;	
	u8 l3_office_dim;	
	u8 l4_indoor_max;	
	u8 l4_indor_dim;	
	u8 l5_dark_max;		
	u8 l5_dark_dim;		

	u8 l2_trip;		
	u8 l2_hyst;		
	u8 l3_trip;		
	u8 l3_hyst;		
	u8 l4_trip;		
	u8 l4_hyst;		
	u8 l5_trip;		
	u8 l5_hyst;		


	int num_leds;
	struct led_info	*leds;
	u8 led_fade_in;		
	u8 led_fade_out;	
	u8 led_fade_law;	
	u8 led_on_time;
};

#endif 
