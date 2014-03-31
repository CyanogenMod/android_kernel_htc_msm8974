/*
 * exynos4_tmu.h - Samsung EXYNOS4 TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_EXYNOS4_TMU_H
#define _LINUX_EXYNOS4_TMU_H

enum calibration_type {
	TYPE_ONE_POINT_TRIMMING,
	TYPE_TWO_POINT_TRIMMING,
	TYPE_NONE,
};

struct exynos4_tmu_platform_data {
	u8 threshold;
	u8 trigger_levels[4];
	bool trigger_level0_en;
	bool trigger_level1_en;
	bool trigger_level2_en;
	bool trigger_level3_en;

	u8 gain;
	u8 reference_voltage;

	enum calibration_type cal_type;
};
#endif 
