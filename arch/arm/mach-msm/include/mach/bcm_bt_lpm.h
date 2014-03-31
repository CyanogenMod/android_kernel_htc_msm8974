/*
 * Copyright (C) 2009 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_BCM_BT_LPM_H
#define __ASM_ARCH_BCM_BT_LPM_H

#include <linux/serial_core.h>

extern void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

struct bcm_bt_lpm_platform_data {
	unsigned int gpio_wake;   
	unsigned int gpio_host_wake;  

	void (*request_clock_off_locked)(struct uart_port *uport);
	void (*request_clock_on_locked)(struct uart_port *uport);
};

#endif
