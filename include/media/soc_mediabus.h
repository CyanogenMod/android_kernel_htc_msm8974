/*
 * SoC-camera Media Bus API extensions
 *
 * Copyright (C) 2009, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SOC_MEDIABUS_H
#define SOC_MEDIABUS_H

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>

enum soc_mbus_packing {
	SOC_MBUS_PACKING_NONE,
	SOC_MBUS_PACKING_2X8_PADHI,
	SOC_MBUS_PACKING_2X8_PADLO,
	SOC_MBUS_PACKING_EXTEND16,
	SOC_MBUS_PACKING_VARIABLE,
	SOC_MBUS_PACKING_1_5X8,
};

enum soc_mbus_order {
	SOC_MBUS_ORDER_LE,
	SOC_MBUS_ORDER_BE,
};

struct soc_mbus_pixelfmt {
	const char		*name;
	u32			fourcc;
	enum soc_mbus_packing	packing;
	enum soc_mbus_order	order;
	u8			bits_per_sample;
};

struct soc_mbus_lookup {
	enum v4l2_mbus_pixelcode	code;
	struct soc_mbus_pixelfmt	fmt;
};

const struct soc_mbus_pixelfmt *soc_mbus_find_fmtdesc(
	enum v4l2_mbus_pixelcode code,
	const struct soc_mbus_lookup *lookup,
	int n);
const struct soc_mbus_pixelfmt *soc_mbus_get_fmtdesc(
	enum v4l2_mbus_pixelcode code);
s32 soc_mbus_bytes_per_line(u32 width, const struct soc_mbus_pixelfmt *mf);
int soc_mbus_samples_per_pixel(const struct soc_mbus_pixelfmt *mf,
			unsigned int *numerator, unsigned int *denominator);
unsigned int soc_mbus_config_compatible(const struct v4l2_mbus_config *cfg,
					unsigned int flags);

#endif
