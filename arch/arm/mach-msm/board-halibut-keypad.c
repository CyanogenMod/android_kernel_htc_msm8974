/* linux/arch/arm/mach-msm/board-halibut-keypad.c
 *
 * Copyright (C) 2007 Google, Inc.
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

#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "board_halibut."
static int halibut_ffa;
module_param_named(ffa, halibut_ffa, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define SCAN_FUNCTION_KEYS 0 

static unsigned int halibut_row_gpios[] = {
	31, 32, 33, 34, 35, 41
#if SCAN_FUNCTION_KEYS
	, 42
#endif
};

static unsigned int halibut_col_gpios[] = { 36, 37, 38, 39, 40 };


#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(halibut_col_gpios) + (col))

static const unsigned short halibut_keymap[ARRAY_SIZE(halibut_col_gpios) * ARRAY_SIZE(halibut_row_gpios)] = {
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

static const unsigned short halibut_keymap_ffa[ARRAY_SIZE(halibut_col_gpios) * ARRAY_SIZE(halibut_row_gpios)] = {
	
	
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

static struct gpio_event_matrix_info halibut_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= halibut_keymap,
	.output_gpios	= halibut_row_gpios,
	.input_gpios	= halibut_col_gpios,
	.noutputs	= ARRAY_SIZE(halibut_row_gpios),
	.ninputs	= ARRAY_SIZE(halibut_col_gpios),
	.settle_time.tv.nsec = 0,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE | GPIOKPF_PRINT_UNMAPPED_KEYS 
};

struct gpio_event_info *halibut_keypad_info[] = {
	&halibut_matrix_info.info
};

static struct gpio_event_platform_data halibut_keypad_data = {
	.name		= "halibut_keypad",
	.info		= halibut_keypad_info,
	.info_count	= ARRAY_SIZE(halibut_keypad_info)
};

static struct platform_device halibut_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &halibut_keypad_data,
	},
};

static int __init halibut_init_keypad(void)
{
	if (!machine_is_halibut())
		return 0;
	if (halibut_ffa)
		halibut_matrix_info.keymap = halibut_keymap_ffa;
	return platform_device_register(&halibut_keypad_device);
}

device_initcall(halibut_init_keypad);
