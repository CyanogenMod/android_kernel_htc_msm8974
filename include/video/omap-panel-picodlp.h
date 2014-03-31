/*
 * panel data for picodlp panel
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Mayuresh Janorkar <mayur@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __PANEL_PICODLP_H
#define __PANEL_PICODLP_H
struct picodlp_panel_data {
	int picodlp_adapter_id;
	int emu_done_gpio;
	int pwrgood_gpio;
};
#endif 
