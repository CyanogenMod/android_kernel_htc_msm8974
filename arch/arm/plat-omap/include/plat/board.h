/*
 *  arch/arm/plat-omap/include/mach/board.h
 *
 *  Information structures for board-specific data
 *
 *  Copyright (C) 2004	Nokia Corporation
 *  Written by Juha Yrjölä <juha.yrjola@nokia.com>
 */

#ifndef _OMAP_BOARD_H
#define _OMAP_BOARD_H

#include <linux/types.h>

#include <plat/gpio-switch.h>

enum {
	OMAP3EVM_BOARD_GEN_1 = 0,	
	OMAP3EVM_BOARD_GEN_2,		
};

#define OMAP_TAG_CLOCK		0x4f01
#define OMAP_TAG_GPIO_SWITCH	0x4f06
#define OMAP_TAG_STI_CONSOLE	0x4f09
#define OMAP_TAG_CAMERA_SENSOR	0x4f0a

#define OMAP_TAG_BOOT_REASON    0x4f80
#define OMAP_TAG_FLASH_PART	0x4f81
#define OMAP_TAG_VERSION_STR	0x4f82

struct omap_clock_config {
	
	u8 system_clock_type;
};

struct omap_serial_console_config {
	u8 console_uart;
	u32 console_speed;
};

struct omap_sti_console_config {
	unsigned enable:1;
	u8 channel;
};

struct omap_camera_sensor_config {
	u16 reset_gpio;
	int (*power_on)(void * data);
	int (*power_off)(void * data);
};

struct omap_usb_config {
	unsigned	register_host:1;
	unsigned	register_dev:1;
	u8		otg;	

	u8		hmc_mode;

	
	u8		rwc;

	u8		pins[3];

	struct platform_device *udc_device;
	struct platform_device *ohci_device;
	struct platform_device *otg_device;

	u32 (*usb0_init)(unsigned nwires, unsigned is_device);
	u32 (*usb1_init)(unsigned nwires);
	u32 (*usb2_init)(unsigned nwires, unsigned alt_pingroup);
};

struct omap_lcd_config {
	char panel_name[16];
	char ctrl_name[16];
	s16  nreset_gpio;
	u8   data_lines;
};

struct device;
struct fb_info;
struct omap_backlight_config {
	int default_intensity;
	int (*set_power)(struct device *dev, int state);
};

struct omap_fbmem_config {
	u32 start;
	u32 size;
};

struct omap_pwm_led_platform_data {
	const char *name;
	int intensity_timer;
	int blink_timer;
	void (*set_power)(struct omap_pwm_led_platform_data *self, int on_off);
};

struct omap_uart_config {
	
	unsigned int enabled_uarts;
};


struct omap_flash_part_config {
	char part_table[0];
};

struct omap_boot_reason_config {
	char reason_str[12];
};

struct omap_version_config {
	char component[12];
	char version[12];
};

struct omap_board_config_entry {
	u16 tag;
	u16 len;
	u8  data[0];
};

struct omap_board_config_kernel {
	u16 tag;
	const void *data;
};

extern const void *__init __omap_get_config(u16 tag, size_t len, int nr);

#define omap_get_config(tag, type) \
	((const type *) __omap_get_config((tag), sizeof(type), 0))
#define omap_get_nr_config(tag, type, nr) \
	((const type *) __omap_get_config((tag), sizeof(type), (nr)))

extern const void *__init omap_get_var_config(u16 tag, size_t *len);

extern struct omap_board_config_kernel *omap_board_config;
extern int omap_board_config_size;


extern int debug_card_init(u32 addr, unsigned gpio);

#if defined(CONFIG_MACH_OMAP3EVM)
u8 get_omap3_evm_rev(void);
#else
#define get_omap3_evm_rev() (-EINVAL)
#endif
#endif
