/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SMB350_H__
#define __SMB350_H__

#define SMB350_NAME		"smb350"

struct smb350_platform_data {
	int chg_en_n_gpio;
	int chg_susp_n_gpio;
	int chg_current_ma;
	int term_current_ma;
	int stat_gpio;
};

#endif
