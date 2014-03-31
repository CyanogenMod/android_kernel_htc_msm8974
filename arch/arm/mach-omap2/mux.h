/*
 * Copyright (C) 2009 Nokia
 * Copyright (C) 2009-2010 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "mux2420.h"
#include "mux2430.h"
#include "mux34xx.h"
#include "mux44xx.h"

#define OMAP_MUX_TERMINATOR	0xffff

#define OMAP_MUX_MODE0      0
#define OMAP_MUX_MODE1      1
#define OMAP_MUX_MODE2      2
#define OMAP_MUX_MODE3      3
#define OMAP_MUX_MODE4      4
#define OMAP_MUX_MODE5      5
#define OMAP_MUX_MODE6      6
#define OMAP_MUX_MODE7      7

#define OMAP_PULL_ENA			(1 << 3)
#define OMAP_PULL_UP			(1 << 4)
#define OMAP_ALTELECTRICALSEL		(1 << 5)

#define OMAP_INPUT_EN			(1 << 8)
#define OMAP_OFF_EN			(1 << 9)
#define OMAP_OFFOUT_EN			(1 << 10)
#define OMAP_OFFOUT_VAL			(1 << 11)
#define OMAP_OFF_PULL_EN		(1 << 12)
#define OMAP_OFF_PULL_UP		(1 << 13)
#define OMAP_WAKEUP_EN			(1 << 14)

#define OMAP_WAKEUP_EVENT		(1 << 15)

#define OMAP_PIN_OUTPUT			0
#define OMAP_PIN_INPUT			OMAP_INPUT_EN
#define OMAP_PIN_INPUT_PULLUP		(OMAP_PULL_ENA | OMAP_INPUT_EN \
						| OMAP_PULL_UP)
#define OMAP_PIN_INPUT_PULLDOWN		(OMAP_PULL_ENA | OMAP_INPUT_EN)

#define OMAP_PIN_OFF_NONE		0
#define OMAP_PIN_OFF_OUTPUT_HIGH	(OMAP_OFF_EN | OMAP_OFFOUT_EN \
						| OMAP_OFFOUT_VAL)
#define OMAP_PIN_OFF_OUTPUT_LOW		(OMAP_OFF_EN | OMAP_OFFOUT_EN)
#define OMAP_PIN_OFF_INPUT_PULLUP	(OMAP_OFF_EN | OMAP_OFF_PULL_EN \
						| OMAP_OFF_PULL_UP)
#define OMAP_PIN_OFF_INPUT_PULLDOWN	(OMAP_OFF_EN | OMAP_OFF_PULL_EN)
#define OMAP_PIN_OFF_WAKEUPENABLE	OMAP_WAKEUP_EN

#define OMAP_MODE_GPIO(x)	(((x) & OMAP_MUX_MODE7) == OMAP_MUX_MODE4)

#define OMAP_PACKAGE_MASK		0xffff
#define OMAP_PACKAGE_CBS		8		
#define OMAP_PACKAGE_CBL		7		
#define OMAP_PACKAGE_CBP		6		
#define OMAP_PACKAGE_CUS		5		
#define OMAP_PACKAGE_CBB		4		
#define OMAP_PACKAGE_CBC		3		
#define OMAP_PACKAGE_ZAC		2		
#define OMAP_PACKAGE_ZAF		1		


#define OMAP_MUX_NR_MODES		8		
#define OMAP_MUX_NR_SIDES		2		

#define OMAP_MUX_REG_8BIT		(1 << 0)
#define OMAP_MUX_GPIO_IN_MODE3		(1 << 1)

struct omap_board_data {
	int			id;
	u32			flags;
	struct omap_device_pad	*pads;
	int			pads_cnt;
};

struct omap_mux_partition {
	const char		*name;
	u32			flags;
	u32			phys;
	u32			size;
	void __iomem		*base;
	struct list_head	muxmodes;
	struct list_head	node;
};

struct omap_mux {
	u16	reg_offset;
	u16	gpio;
#ifdef CONFIG_OMAP_MUX
	char	*muxnames[OMAP_MUX_NR_MODES];
#ifdef CONFIG_DEBUG_FS
	char	*balls[OMAP_MUX_NR_SIDES];
#endif
#endif
};

struct omap_ball {
	u16	reg_offset;
	char	*balls[OMAP_MUX_NR_SIDES];
};

struct omap_board_mux {
	u16	reg_offset;
	u16	value;
};

#define OMAP_DEVICE_PAD_REMUX		BIT(1)	
#define OMAP_DEVICE_PAD_WAKEUP		BIT(0)	

struct omap_device_pad {
	char				*name;
	u8				flags;
	u16				enable;
	u16				idle;
	u16				off;
	struct omap_mux_partition	*partition;
	struct omap_mux			*mux;
};

struct omap_hwmod_mux_info;

#define OMAP_MUX_STATIC(signal, mode)					\
{									\
	.name	= (signal),						\
	.enable	= (mode),						\
}

#if defined(CONFIG_OMAP_MUX)

int omap_mux_init_gpio(int gpio, int val);

int omap_mux_init_signal(const char *muxname, int val);

extern struct omap_hwmod_mux_info *
omap_hwmod_mux_init(struct omap_device_pad *bpads, int nr_pads);

void omap_hwmod_mux(struct omap_hwmod_mux_info *hmux, u8 state);

#else

static inline int omap_mux_init_gpio(int gpio, int val)
{
	return 0;
}
static inline int omap_mux_init_signal(char *muxname, int val)
{
	return 0;
}

static inline struct omap_hwmod_mux_info *
omap_hwmod_mux_init(struct omap_device_pad *bpads, int nr_pads)
{
	return NULL;
}

static inline void omap_hwmod_mux(struct omap_hwmod_mux_info *hmux, u8 state)
{
}

static struct omap_board_mux *board_mux __maybe_unused;

#endif

u16 omap_mux_get_gpio(int gpio);

void omap_mux_set_gpio(u16 val, int gpio);

struct omap_mux_partition *omap_mux_get(const char *name);

u16 omap_mux_read(struct omap_mux_partition *p, u16 mux_offset);

void omap_mux_write(struct omap_mux_partition *p, u16 val, u16 mux_offset);

void omap_mux_write_array(struct omap_mux_partition *p,
			  struct omap_board_mux *board_mux);

int omap2420_mux_init(struct omap_board_mux *board_mux, int flags);

int omap2430_mux_init(struct omap_board_mux *board_mux, int flags);

int omap3_mux_init(struct omap_board_mux *board_mux, int flags);

int omap4_mux_init(struct omap_board_mux *board_subset,
	struct omap_board_mux *board_wkup_subset, int flags);

int omap_mux_init(const char *name, u32 flags,
		  u32 mux_pbase, u32 mux_size,
		  struct omap_mux *superset,
		  struct omap_mux *package_subset,
		  struct omap_board_mux *board_mux,
		  struct omap_ball *package_balls);

