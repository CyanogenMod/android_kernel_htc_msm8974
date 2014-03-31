/*
 * Copyright (C) 2008 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
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

#ifndef __ASM_ARCH_MSM_SERIAL_HS_H
#define __ASM_ARCH_MSM_SERIAL_HS_H

#include <linux/serial_core.h>

extern void msm_hs_request_clock_off(struct uart_port *uport);
extern void msm_hs_request_clock_on(struct uart_port *uport);

struct msm_serial_hs_platform_data {
	int rx_wakeup_irq;
	unsigned char inject_rx_on_wakeup;
	char rx_to_inject;
	void (*exit_lpm_cb)(struct uart_port *);
};

#endif
