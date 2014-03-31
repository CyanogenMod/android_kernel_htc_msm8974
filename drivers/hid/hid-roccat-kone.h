#ifndef __HID_ROCCAT_KONE_H
#define __HID_ROCCAT_KONE_H

/*
 * Copyright (c) 2010 Stefan Achatz <erazor_de@users.sourceforge.net>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/types.h>

struct kone_keystroke {
	uint8_t key;
	uint8_t action;
	uint16_t period; 
} __attribute__ ((__packed__));

enum kone_keystroke_buttons {
	kone_keystroke_button_1 = 0xf0, 
	kone_keystroke_button_2 = 0xf1, 
	kone_keystroke_button_3 = 0xf2, 
	kone_keystroke_button_9 = 0xf3, 
	kone_keystroke_button_8 = 0xf4 
};

enum kone_keystroke_actions {
	kone_keystroke_action_press = 0,
	kone_keystroke_action_release = 1
};

struct kone_button_info {
	uint8_t number; 
	uint8_t type;
	uint8_t macro_type; 
	uint8_t macro_set_name[16]; 
	uint8_t macro_name[16]; 
	uint8_t count;
	struct kone_keystroke keystrokes[20];
} __attribute__ ((__packed__));

enum kone_button_info_types {
	
	kone_button_info_type_button_1 = 0x1, 
	kone_button_info_type_button_2 = 0x2, 
	kone_button_info_type_button_3 = 0x3, 
	kone_button_info_type_double_click = 0x4,
	kone_button_info_type_key = 0x5,
	kone_button_info_type_macro = 0x6,
	kone_button_info_type_off = 0x7,
	
	kone_button_info_type_osd_xy_prescaling = 0x8,
	kone_button_info_type_osd_dpi = 0x9,
	kone_button_info_type_osd_profile = 0xa,
	kone_button_info_type_button_9 = 0xb, 
	kone_button_info_type_button_8 = 0xc, 
	kone_button_info_type_dpi_up = 0xd, 
	kone_button_info_type_dpi_down = 0xe, 
	kone_button_info_type_button_7 = 0xf, 
	kone_button_info_type_button_6 = 0x10, 
	kone_button_info_type_profile_up = 0x11, 
	kone_button_info_type_profile_down = 0x12, 
	
	kone_button_info_type_multimedia_open_player = 0x20,
	kone_button_info_type_multimedia_next_track = 0x21,
	kone_button_info_type_multimedia_prev_track = 0x22,
	kone_button_info_type_multimedia_play_pause = 0x23,
	kone_button_info_type_multimedia_stop = 0x24,
	kone_button_info_type_multimedia_mute = 0x25,
	kone_button_info_type_multimedia_volume_up = 0x26,
	kone_button_info_type_multimedia_volume_down = 0x27
};

enum kone_button_info_numbers {
	kone_button_top = 1,
	kone_button_wheel_tilt_left = 2,
	kone_button_wheel_tilt_right = 3,
	kone_button_forward = 4,
	kone_button_backward = 5,
	kone_button_middle = 6,
	kone_button_plus = 7,
	kone_button_minus = 8,
};

struct kone_light_info {
	uint8_t number; 
	uint8_t mod;   
	uint8_t red;   
	uint8_t green; 
	uint8_t blue;  
} __attribute__ ((__packed__));

struct kone_profile {
	uint16_t size; 
	uint16_t unused; 

	uint8_t profile; 

	uint16_t main_sensitivity; 
	uint8_t xy_sensitivity_enabled; 
	uint16_t x_sensitivity; 
	uint16_t y_sensitivity; 
	uint8_t dpi_rate; 
	uint8_t startup_dpi; 
	uint8_t polling_rate; 
	uint8_t dcu_flag;
	uint8_t light_effect_1; 
	uint8_t light_effect_2; 
	uint8_t light_effect_3; 
	uint8_t light_effect_speed; 

	struct kone_light_info light_infos[5];
	
	struct kone_button_info button_infos[8];

	uint16_t checksum; 
} __attribute__ ((__packed__));

enum kone_polling_rates {
	kone_polling_rate_125 = 1,
	kone_polling_rate_500 = 2,
	kone_polling_rate_1000 = 3
};

struct kone_settings {
	uint16_t size; 
	uint8_t  startup_profile; 
	uint8_t	 unknown1;
	uint8_t  tcu; 
	uint8_t  unknown2[23];
	uint8_t  calibration_data[4];
	uint8_t  unknown3[2];
	uint16_t checksum;
} __attribute__ ((__packed__));

struct kone_mouse_event {
	uint8_t report_number; 
	uint8_t button;
	uint16_t x;
	uint16_t y;
	uint8_t wheel; 
	uint8_t tilt; 
	uint8_t unknown;
	uint8_t event;
	uint8_t value; 
	uint8_t macro_key; 
} __attribute__ ((__packed__));

enum kone_mouse_events {
	
	kone_mouse_event_osd_dpi = 0xa0,
	kone_mouse_event_osd_profile = 0xb0,
	
	kone_mouse_event_calibration = 0xc0,
	kone_mouse_event_call_overlong_macro = 0xe0,
	
	kone_mouse_event_switch_dpi = 0xf0,
	kone_mouse_event_switch_profile = 0xf1
};

enum kone_commands {
	kone_command_profile = 0x5a,
	kone_command_settings = 0x15a,
	kone_command_firmware_version = 0x25a,
	kone_command_weight = 0x45a,
	kone_command_calibrate = 0x55a,
	kone_command_confirm_write = 0x65a,
	kone_command_firmware = 0xe5a
};

struct kone_roccat_report {
	uint8_t event;
	uint8_t value; 
	uint8_t key; 
} __attribute__ ((__packed__));

struct kone_device {
	int actual_profile, actual_dpi;
	
	struct kone_mouse_event last_mouse_event;

	struct mutex kone_lock;

	struct kone_profile profiles[5];
	struct kone_settings settings;

	int firmware_version;

	int roccat_claimed;
	int chrdev_minor;
};

#endif
