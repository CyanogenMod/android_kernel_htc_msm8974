/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <asm/mach-types.h>

#define SCAN_FUNCTION_KEYS 0


static unsigned int keypad_row_gpios[] = {
	31, 32, 33, 34, 35, 41
#if SCAN_FUNCTION_KEYS
	, 42
#endif
};

static unsigned int keypad_col_gpios[] = { 36, 37, 38, 39, 40 };

static unsigned int keypad_row_gpios_8k_ffa[] = {31, 32, 33, 34, 35, 36};
static unsigned int keypad_col_gpios_8k_ffa[] = {38, 39, 40, 41, 42};

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios) + (col))
#define FFA_8K_KEYMAP_INDEX(row, col) ((row)* \
				ARRAY_SIZE(keypad_col_gpios_8k_ffa) + (col))

static const unsigned short keypad_keymap_surf[ARRAY_SIZE(keypad_col_gpios) *
					  ARRAY_SIZE(keypad_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_5,
	[KEYMAP_INDEX(0, 1)] = KEY_9,
	[KEYMAP_INDEX(0, 2)] = 229,            
	[KEYMAP_INDEX(0, 3)] = KEY_6,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_0,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_1,
	[KEYMAP_INDEX(1, 3)] = 228,           
	[KEYMAP_INDEX(1, 4)] = KEY_SEND,

	[KEYMAP_INDEX(2, 0)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(2, 1)] = KEY_HOME,      
	[KEYMAP_INDEX(2, 2)] = KEY_F8,        
	[KEYMAP_INDEX(2, 3)] = KEY_F6,        
	[KEYMAP_INDEX(2, 4)] = KEY_F7,        

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = KEY_CLEAR,
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	[KEYMAP_INDEX(3, 3)] = KEY_MUTE,      
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = 230,           
	[KEYMAP_INDEX(4, 1)] = 232,           
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_BACK,      
	[KEYMAP_INDEX(4, 4)] = KEY_8,

	[KEYMAP_INDEX(5, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(5, 1)] = 227,           
	[KEYMAP_INDEX(5, 2)] = KEY_MAIL,      
	[KEYMAP_INDEX(5, 3)] = KEY_3,
	[KEYMAP_INDEX(5, 4)] = KEY_7,

#if SCAN_FUNCTION_KEYS
	[KEYMAP_INDEX(6, 0)] = KEY_F5,
	[KEYMAP_INDEX(6, 1)] = KEY_F4,
	[KEYMAP_INDEX(6, 2)] = KEY_F3,
	[KEYMAP_INDEX(6, 3)] = KEY_F2,
	[KEYMAP_INDEX(6, 4)] = KEY_F1
#endif
};

static const unsigned short keypad_keymap_ffa[ARRAY_SIZE(keypad_col_gpios) *
					      ARRAY_SIZE(keypad_row_gpios)] = {
	
	
	[KEYMAP_INDEX(0, 2)] = KEY_1,
	[KEYMAP_INDEX(0, 3)] = KEY_SEND,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_3,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEUP,
	
	[KEYMAP_INDEX(1, 4)] = KEY_6,

	[KEYMAP_INDEX(2, 0)] = KEY_HOME,      
	[KEYMAP_INDEX(2, 1)] = KEY_BACK,      
	[KEYMAP_INDEX(2, 2)] = KEY_0,
	[KEYMAP_INDEX(2, 3)] = 228,           
	[KEYMAP_INDEX(2, 4)] = KEY_9,

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = 232,  
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_8,
	[KEYMAP_INDEX(4, 4)] = KEY_5,

	
	[KEYMAP_INDEX(5, 1)] = 227,           
	[KEYMAP_INDEX(5, 2)] = 230,  
	[KEYMAP_INDEX(5, 3)] = KEY_MENU,      
	[KEYMAP_INDEX(5, 4)] = KEY_7,
};

#define QSD8x50_FFA_KEYMAP_SIZE (ARRAY_SIZE(keypad_col_gpios_8k_ffa) * \
			ARRAY_SIZE(keypad_row_gpios_8k_ffa))

static const unsigned short keypad_keymap_8k_ffa[QSD8x50_FFA_KEYMAP_SIZE] = {

	[FFA_8K_KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
	
	[FFA_8K_KEYMAP_INDEX(0, 2)] = KEY_DOWN,
	[FFA_8K_KEYMAP_INDEX(0, 3)] = KEY_8,
	[FFA_8K_KEYMAP_INDEX(0, 4)] = KEY_5,

	[FFA_8K_KEYMAP_INDEX(1, 0)] = KEY_UP,
	[FFA_8K_KEYMAP_INDEX(1, 1)] = KEY_CLEAR,
	[FFA_8K_KEYMAP_INDEX(1, 2)] = KEY_4,
	
	[FFA_8K_KEYMAP_INDEX(1, 4)] = KEY_2,

	[FFA_8K_KEYMAP_INDEX(2, 0)] = KEY_HOME,      
	[FFA_8K_KEYMAP_INDEX(2, 1)] = KEY_BACK,      
	[FFA_8K_KEYMAP_INDEX(2, 2)] = KEY_0,
	[FFA_8K_KEYMAP_INDEX(2, 3)] = 228,           
	[FFA_8K_KEYMAP_INDEX(2, 4)] = KEY_9,

	[FFA_8K_KEYMAP_INDEX(3, 0)] = KEY_3,
	[FFA_8K_KEYMAP_INDEX(3, 1)] = KEY_RIGHT,
	[FFA_8K_KEYMAP_INDEX(3, 2)] = KEY_VOLUMEUP,
	
	[FFA_8K_KEYMAP_INDEX(3, 4)] = KEY_6,

	[FFA_8K_KEYMAP_INDEX(4, 0)] = 232,		
	[FFA_8K_KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[FFA_8K_KEYMAP_INDEX(4, 2)] = KEY_1,
	[FFA_8K_KEYMAP_INDEX(4, 3)] = KEY_SEND,
	[FFA_8K_KEYMAP_INDEX(4, 4)] = KEY_LEFT,

	
	[FFA_8K_KEYMAP_INDEX(5, 1)] = 227,           
	[FFA_8K_KEYMAP_INDEX(5, 2)] = 230,  
	[FFA_8K_KEYMAP_INDEX(5, 3)] = 229,      
	[FFA_8K_KEYMAP_INDEX(5, 4)] = KEY_7,
};

static struct gpio_event_matrix_info surf_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_surf,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv64 = 40 * NSEC_PER_USEC,
	.poll_time.tv64 = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *surf_keypad_info[] = {
	&surf_keypad_matrix_info.info
};

static struct gpio_event_platform_data surf_keypad_data = {
	.name		= "surf_keypad",
	.info		= surf_keypad_info,
	.info_count	= ARRAY_SIZE(surf_keypad_info)
};

struct platform_device keypad_device_surf = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &surf_keypad_data,
	},
};

static struct gpio_event_matrix_info keypad_matrix_info_8k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_8k_ffa,
	.output_gpios	= keypad_row_gpios_8k_ffa,
	.input_gpios	= keypad_col_gpios_8k_ffa,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_8k_ffa),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_8k_ffa),
	.settle_time.tv64 = 40 * NSEC_PER_USEC,
	.poll_time.tv64 = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_8k_ffa[] = {
	&keypad_matrix_info_8k_ffa.info
};

static struct gpio_event_platform_data keypad_data_8k_ffa = {
	.name		= "8k_ffa_keypad",
	.info		= keypad_info_8k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_8k_ffa)
};

struct platform_device keypad_device_8k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_8k_ffa,
	},
};

static struct gpio_event_matrix_info keypad_matrix_info_7k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_ffa,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv64 = 40 * NSEC_PER_USEC,
	.poll_time.tv64 = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_7k_ffa[] = {
	&keypad_matrix_info_7k_ffa.info
};

static struct gpio_event_platform_data keypad_data_7k_ffa = {
	.name		= "7k_ffa_keypad",
	.info		= keypad_info_7k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_7k_ffa)
};

struct platform_device keypad_device_7k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_7k_ffa,
	},
};

