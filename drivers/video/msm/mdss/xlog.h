/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef XLOH_H
#define XLOH_H

#ifdef CONFIG_MDSS_DUMP_MDP_UNDERRUN

#define XLOG(__func__, d0, d1, d2, d3, d4, d5) xlog(__func__, d0, d1, d2, d3, d4, d5)
#define XLOG_TOUT_HANDLER(__func__, dsi, mdp, bus, dead)	\
		xlog_tout_handler(__func__, dsi, mdp, bus, dead)

void xlog(const char *name, u32 data0, u32 data1, u32 data2, u32 data3, u32 data4, u32 data5);
void xlog_dump(void);
void dsi_dump_reg(void);
void mdp_dump_reg(void);
void mdp_debug_bus(void);
void mdss_dsi_debug_check_te(void);
void xlog_tout_handler(const char *name,  int dump_dsi, int dump_mdp, int dump_mdp_debug_bus, int dead);

#else

#define XLOG(__func__, d0, d1, d2, d3, d4, d5)
#define XLOG_TOUT_HANDLER(__func__, dsi, mdp, bus, dead)

#define xlog(name, data0, data1, data2, data3, data4, data5)
#define xlog_dump()
#define dsi_dump_reg()
#define mdp_dump_reg()
#define mdp_debug_bus()
#define mdss_dsi_debug_check_te()
#define xlog_tout_handler(name, dump_dsi, dump_mdp, dump_mdp_debug_bus, dead)

#endif

#endif
