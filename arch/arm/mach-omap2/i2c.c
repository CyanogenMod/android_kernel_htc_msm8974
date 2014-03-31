/*
 * Helper module for board specific I2C bus registration
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <plat/cpu.h>
#include <plat/i2c.h>
#include "common.h"
#include <plat/omap_hwmod.h>

#include "mux.h"

#define I2C_EN					BIT(15)
#define OMAP2_I2C_CON_OFFSET			0x24
#define OMAP4_I2C_CON_OFFSET			0xA4

#define MAX_MODULE_SOFTRESET_WAIT	10000

void __init omap2_i2c_mux_pins(int bus_id)
{
	char mux_name[sizeof("i2c2_scl.i2c2_scl")];

	
	if (bus_id == 1)
		return;

	sprintf(mux_name, "i2c%i_scl.i2c%i_scl", bus_id, bus_id);
	omap_mux_init_signal(mux_name, OMAP_PIN_INPUT);
	sprintf(mux_name, "i2c%i_sda.i2c%i_sda", bus_id, bus_id);
	omap_mux_init_signal(mux_name, OMAP_PIN_INPUT);
}

int omap_i2c_reset(struct omap_hwmod *oh)
{
	u32 v;
	u16 i2c_con;
	int c = 0;

	if (oh->class->rev == OMAP_I2C_IP_VERSION_2) {
		i2c_con = OMAP4_I2C_CON_OFFSET;
	} else if (oh->class->rev == OMAP_I2C_IP_VERSION_1) {
		i2c_con = OMAP2_I2C_CON_OFFSET;
	} else {
		WARN(1, "Cannot reset I2C block %s: unsupported revision\n",
		     oh->name);
		return -EINVAL;
	}

	
	v = omap_hwmod_read(oh, i2c_con);
	v &= ~I2C_EN;
	omap_hwmod_write(v, oh, i2c_con);

	
	omap_hwmod_softreset(oh);

	
	v = omap_hwmod_read(oh, i2c_con);
	v |= I2C_EN;
	omap_hwmod_write(v, oh, i2c_con);

	
	omap_test_timeout((omap_hwmod_read(oh,
				oh->class->sysc->syss_offs)
				& SYSS_RESETDONE_MASK),
				MAX_MODULE_SOFTRESET_WAIT, c);

	if (c == MAX_MODULE_SOFTRESET_WAIT)
		pr_warning("%s: %s: softreset failed (waited %d usec)\n",
			__func__, oh->name, MAX_MODULE_SOFTRESET_WAIT);
	else
		pr_debug("%s: %s: softreset in %d usec\n", __func__,
			oh->name, c);

	return 0;
}
