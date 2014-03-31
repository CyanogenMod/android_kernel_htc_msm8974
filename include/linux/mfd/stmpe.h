/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License, version 2
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 */

#ifndef __LINUX_MFD_STMPE_H
#define __LINUX_MFD_STMPE_H

#include <linux/mutex.h>

struct device;

enum stmpe_block {
	STMPE_BLOCK_GPIO	= 1 << 0,
	STMPE_BLOCK_KEYPAD	= 1 << 1,
	STMPE_BLOCK_TOUCHSCREEN	= 1 << 2,
	STMPE_BLOCK_ADC		= 1 << 3,
	STMPE_BLOCK_PWM		= 1 << 4,
	STMPE_BLOCK_ROTATOR	= 1 << 5,
};

enum stmpe_partnum {
	STMPE610,
	STMPE801,
	STMPE811,
	STMPE1601,
	STMPE2401,
	STMPE2403,
	STMPE_NBR_PARTS
};

enum {
	STMPE_IDX_CHIP_ID,
	STMPE_IDX_ICR_LSB,
	STMPE_IDX_IER_LSB,
	STMPE_IDX_ISR_MSB,
	STMPE_IDX_GPMR_LSB,
	STMPE_IDX_GPSR_LSB,
	STMPE_IDX_GPCR_LSB,
	STMPE_IDX_GPDR_LSB,
	STMPE_IDX_GPEDR_MSB,
	STMPE_IDX_GPRER_LSB,
	STMPE_IDX_GPFER_LSB,
	STMPE_IDX_GPAFR_U_MSB,
	STMPE_IDX_IEGPIOR_LSB,
	STMPE_IDX_ISGPIOR_MSB,
	STMPE_IDX_MAX,
};


struct stmpe_variant_info;
struct stmpe_client_info;

struct stmpe {
	struct mutex lock;
	struct mutex irq_lock;
	struct device *dev;
	void *client;
	struct stmpe_client_info *ci;
	enum stmpe_partnum partnum;
	struct stmpe_variant_info *variant;
	const u8 *regs;

	int irq;
	int irq_base;
	int num_gpios;
	u8 ier[2];
	u8 oldier[2];
	struct stmpe_platform_data *pdata;
};

extern int stmpe_reg_write(struct stmpe *stmpe, u8 reg, u8 data);
extern int stmpe_reg_read(struct stmpe *stmpe, u8 reg);
extern int stmpe_block_read(struct stmpe *stmpe, u8 reg, u8 length,
			    u8 *values);
extern int stmpe_block_write(struct stmpe *stmpe, u8 reg, u8 length,
			     const u8 *values);
extern int stmpe_set_bits(struct stmpe *stmpe, u8 reg, u8 mask, u8 val);
extern int stmpe_set_altfunc(struct stmpe *stmpe, u32 pins,
			     enum stmpe_block block);
extern int stmpe_enable(struct stmpe *stmpe, unsigned int blocks);
extern int stmpe_disable(struct stmpe *stmpe, unsigned int blocks);

struct matrix_keymap_data;

struct stmpe_keypad_platform_data {
	struct matrix_keymap_data *keymap_data;
	unsigned int debounce_ms;
	unsigned int scan_count;
	bool no_autorepeat;
};

#define STMPE_GPIO_NOREQ_811_TOUCH	(0xf0)

struct stmpe_gpio_platform_data {
	int gpio_base;
	unsigned norequest_mask;
	void (*setup)(struct stmpe *stmpe, unsigned gpio_base);
	void (*remove)(struct stmpe *stmpe, unsigned gpio_base);
};

struct stmpe_ts_platform_data {
       u8 sample_time;
       u8 mod_12b;
       u8 ref_sel;
       u8 adc_freq;
       u8 ave_ctrl;
       u8 touch_det_delay;
       u8 settling;
       u8 fraction_z;
       u8 i_drive;
};

struct stmpe_platform_data {
	int id;
	unsigned int blocks;
	int irq_base;
	unsigned int irq_trigger;
	bool irq_invert_polarity;
	bool autosleep;
	bool irq_over_gpio;
	int irq_gpio;
	int autosleep_timeout;

	struct stmpe_gpio_platform_data *gpio;
	struct stmpe_keypad_platform_data *keypad;
	struct stmpe_ts_platform_data *ts;
};

#define STMPE_NR_INTERNAL_IRQS	9
#define STMPE_INT_GPIO(x)	(STMPE_NR_INTERNAL_IRQS + (x))

#define STMPE_NR_GPIOS		24
#define STMPE_NR_IRQS		STMPE_INT_GPIO(STMPE_NR_GPIOS)

#endif
