/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2010-2013, The Linux Foundation. All rights reserved.
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

#include<linux/serial_core.h>

struct msm_serial_hs_platform_data {
	int wakeup_irq;  
	unsigned char inject_rx_on_wakeup;
	char rx_to_inject;
	int (*gpio_config)(int);
	int userid;

	int uart_tx_gpio;
	int uart_rx_gpio;
	int uart_cts_gpio;
	int uart_rfr_gpio;
	unsigned bam_tx_ep_pipe_index;
	unsigned bam_rx_ep_pipe_index;
};

unsigned int msm_hs_tx_empty(struct uart_port *uport);
void msm_hs_request_clock_off(struct uart_port *uport);
void msm_hs_request_clock_on(struct uart_port *uport);
struct uart_port *msm_hs_get_uart_port(int port_index);
void msm_hs_set_mctrl(struct uart_port *uport,
				    unsigned int mctrl);
#endif
