#ifndef __HID_ROCCAT_KONEPLUS_H
#define __HID_ROCCAT_KONEPLUS_H

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

struct koneplus_talk {
	uint8_t command; 
	uint8_t size; 
	uint8_t data[14];
} __packed;

struct koneplus_control {
	uint8_t command; 
	uint8_t value;
	uint8_t request;
} __attribute__ ((__packed__));

enum koneplus_control_requests {
	KONEPLUS_CONTROL_REQUEST_STATUS = 0x00,
	KONEPLUS_CONTROL_REQUEST_PROFILE_SETTINGS = 0x80,
	KONEPLUS_CONTROL_REQUEST_PROFILE_BUTTONS = 0x90,
};

enum koneplus_control_values {
	KONEPLUS_CONTROL_REQUEST_STATUS_OVERLOAD = 0,
	KONEPLUS_CONTROL_REQUEST_STATUS_OK = 1,
	KONEPLUS_CONTROL_REQUEST_STATUS_WAIT = 3,
};

struct koneplus_actual_profile {
	uint8_t command; 
	uint8_t size; 
	uint8_t actual_profile; 
} __attribute__ ((__packed__));

struct koneplus_profile_settings {
	uint8_t command; 
	uint8_t size; 
	uint8_t number; 
	uint8_t advanced_sensitivity;
	uint8_t sensitivity_x;
	uint8_t sensitivity_y;
	uint8_t cpi_levels_enabled;
	uint8_t cpi_levels_x[5];
	uint8_t cpi_startup_level; 
	uint8_t cpi_levels_y[5]; 
	uint8_t unknown1;
	uint8_t polling_rate;
	uint8_t lights_enabled;
	uint8_t light_effect_mode;
	uint8_t color_flow_effect;
	uint8_t light_effect_type;
	uint8_t light_effect_speed;
	uint8_t lights[16];
	uint16_t checksum;
} __attribute__ ((__packed__));

struct koneplus_profile_buttons {
	uint8_t command; 
	uint8_t size; 
	uint8_t number; 
	uint8_t data[72];
	uint16_t checksum;
} __attribute__ ((__packed__));

struct koneplus_macro {
	uint8_t command; 
	uint16_t size; 
	uint8_t profile; 
	uint8_t button; 
	uint8_t data[2075];
	uint16_t checksum;
} __attribute__ ((__packed__));

struct koneplus_info {
	uint8_t command; 
	uint8_t size; 
	uint8_t firmware_version;
	uint8_t unknown[3];
} __attribute__ ((__packed__));

struct koneplus_e {
	uint8_t command; 
	uint8_t size; 
	uint8_t unknown; 
} __attribute__ ((__packed__));

struct koneplus_sensor {
	uint8_t command;  
	uint8_t size; 
	uint8_t data[4];
} __attribute__ ((__packed__));

struct koneplus_firmware_write {
	uint8_t command; 
	uint8_t unknown[1025];
} __attribute__ ((__packed__));

struct koneplus_firmware_write_control {
	uint8_t command; 
	uint8_t value;
	uint8_t unknown; 
} __attribute__ ((__packed__));

struct koneplus_tcu {
	uint16_t usb_command; 
	uint8_t data[2];
} __attribute__ ((__packed__));

struct koneplus_tcu_image {
	uint16_t usb_command; 
	uint8_t data[1024];
	uint16_t checksum;
} __attribute__ ((__packed__));

enum koneplus_commands {
	KONEPLUS_COMMAND_CONTROL = 0x4,
	KONEPLUS_COMMAND_ACTUAL_PROFILE = 0x5,
	KONEPLUS_COMMAND_PROFILE_SETTINGS = 0x6,
	KONEPLUS_COMMAND_PROFILE_BUTTONS = 0x7,
	KONEPLUS_COMMAND_MACRO = 0x8,
	KONEPLUS_COMMAND_INFO = 0x9,
	KONEPLUS_COMMAND_TCU = 0xc,
	KONEPLUS_COMMAND_E = 0xe,
	KONEPLUS_COMMAND_SENSOR = 0xf,
	KONEPLUS_COMMAND_TALK = 0x10,
	KONEPLUS_COMMAND_FIRMWARE_WRITE = 0x1b,
	KONEPLUS_COMMAND_FIRMWARE_WRITE_CONTROL = 0x1c,
};

enum koneplus_mouse_report_numbers {
	KONEPLUS_MOUSE_REPORT_NUMBER_HID = 1,
	KONEPLUS_MOUSE_REPORT_NUMBER_AUDIO = 2,
	KONEPLUS_MOUSE_REPORT_NUMBER_BUTTON = 3,
};

struct koneplus_mouse_report_button {
	uint8_t report_number; 
	uint8_t zero1;
	uint8_t type;
	uint8_t data1;
	uint8_t data2;
	uint8_t zero2;
	uint8_t unknown[2];
} __attribute__ ((__packed__));

enum koneplus_mouse_report_button_types {
	
	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_PROFILE = 0x20,

	
	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_QUICKLAUNCH = 0x60,

	
	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_TIMER = 0x80,

	
	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_CPI = 0xb0,

	
	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_SENSITIVITY = 0xc0,

	KONEPLUS_MOUSE_REPORT_BUTTON_TYPE_MULTIMEDIA = 0xf0,
	KONEPLUS_MOUSE_REPORT_TALK = 0xff,
};

enum koneplus_mouse_report_button_action {
	KONEPLUS_MOUSE_REPORT_BUTTON_ACTION_PRESS = 0,
	KONEPLUS_MOUSE_REPORT_BUTTON_ACTION_RELEASE = 1,
};

struct koneplus_roccat_report {
	uint8_t type;
	uint8_t data1;
	uint8_t data2;
	uint8_t profile;
} __attribute__ ((__packed__));

struct koneplus_device {
	int actual_profile;

	int roccat_claimed;
	int chrdev_minor;

	struct mutex koneplus_lock;

	struct koneplus_info info;
	struct koneplus_profile_settings profile_settings[5];
	struct koneplus_profile_buttons profile_buttons[5];
};

#endif
