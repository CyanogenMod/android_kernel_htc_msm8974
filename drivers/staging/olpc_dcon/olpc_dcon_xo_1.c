/*
 * Mainly by David Woodhouse, somewhat modified by Jordan Crouse
 *
 * Copyright © 2006-2007  Red Hat, Inc.
 * Copyright © 2006-2007  Advanced Micro Devices, Inc.
 * Copyright © 2009       VIA Technology, Inc.
 * Copyright (c) 2010  Andres Salomon <dilinger@queued.net>
 *
 * This program is free software.  You can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
#include <linux/cs5535.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/olpc.h>

#include "olpc_dcon.h"

static int dcon_init_xo_1(struct dcon_priv *dcon)
{
	unsigned char lob;

	if (gpio_request(OLPC_GPIO_DCON_STAT0, "OLPC-DCON")) {
		printk(KERN_ERR "olpc-dcon: failed to request STAT0 GPIO\n");
		return -EIO;
	}
	if (gpio_request(OLPC_GPIO_DCON_STAT1, "OLPC-DCON")) {
		printk(KERN_ERR "olpc-dcon: failed to request STAT1 GPIO\n");
		goto err_gp_stat1;
	}
	if (gpio_request(OLPC_GPIO_DCON_IRQ, "OLPC-DCON")) {
		printk(KERN_ERR "olpc-dcon: failed to request IRQ GPIO\n");
		goto err_gp_irq;
	}
	if (gpio_request(OLPC_GPIO_DCON_LOAD, "OLPC-DCON")) {
		printk(KERN_ERR "olpc-dcon: failed to request LOAD GPIO\n");
		goto err_gp_load;
	}
	if (gpio_request(OLPC_GPIO_DCON_BLANK, "OLPC-DCON")) {
		printk(KERN_ERR "olpc-dcon: failed to request BLANK GPIO\n");
		goto err_gp_blank;
	}

	
	cs5535_gpio_clear(OLPC_GPIO_DCON_IRQ, GPIO_EVENTS_ENABLE);

	dcon->curr_src = cs5535_gpio_isset(OLPC_GPIO_DCON_LOAD, GPIO_OUTPUT_VAL)
		? DCON_SOURCE_CPU
		: DCON_SOURCE_DCON;
	dcon->pending_src = dcon->curr_src;

	
	gpio_direction_input(OLPC_GPIO_DCON_STAT0);
	gpio_direction_input(OLPC_GPIO_DCON_STAT1);
	gpio_direction_input(OLPC_GPIO_DCON_IRQ);
	gpio_direction_input(OLPC_GPIO_DCON_BLANK);
	gpio_direction_output(OLPC_GPIO_DCON_LOAD,
			dcon->curr_src == DCON_SOURCE_CPU);

	

	
	cs5535_gpio_setup_event(OLPC_GPIO_DCON_IRQ, 2, 0);

	
	cs5535_gpio_set_irq(2, DCON_IRQ);

	
	lob = inb(0x4d0);
	lob &= ~(1 << DCON_IRQ);
	outb(lob, 0x4d0);

	
	if (request_irq(DCON_IRQ, &dcon_interrupt, 0, "DCON", dcon)) {
		printk(KERN_ERR "olpc-dcon: failed to request DCON's irq\n");
		goto err_req_irq;
	}

	
	cs5535_gpio_clear(OLPC_GPIO_DCON_IRQ, GPIO_INPUT_INVERT);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_BLANK, GPIO_INPUT_FILTER);

	
	cs5535_gpio_clear(OLPC_GPIO_DCON_IRQ, GPIO_INPUT_FILTER);

	
	cs5535_gpio_clear(OLPC_GPIO_DCON_IRQ, GPIO_INPUT_EVENT_COUNT);
	cs5535_gpio_clear(OLPC_GPIO_DCON_BLANK, GPIO_INPUT_EVENT_COUNT);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_BLANK, GPIO_FE7_SEL);

	
	cs5535_gpio_clear(OLPC_GPIO_DCON_BLANK, GPIO_NEGATIVE_EDGE_EN);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_IRQ, GPIO_NEGATIVE_EDGE_EN);

	
	cs5535_gpio_set(0, GPIO_FLTR7_AMOUNT);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_IRQ, GPIO_NEGATIVE_EDGE_STS);
	cs5535_gpio_set(OLPC_GPIO_DCON_BLANK, GPIO_NEGATIVE_EDGE_STS);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_IRQ, GPIO_POSITIVE_EDGE_STS);
	cs5535_gpio_set(OLPC_GPIO_DCON_BLANK, GPIO_POSITIVE_EDGE_STS);

	
	cs5535_gpio_set(OLPC_GPIO_DCON_IRQ, GPIO_EVENTS_ENABLE);
	cs5535_gpio_set(OLPC_GPIO_DCON_BLANK, GPIO_EVENTS_ENABLE);

	return 0;

err_req_irq:
	gpio_free(OLPC_GPIO_DCON_BLANK);
err_gp_blank:
	gpio_free(OLPC_GPIO_DCON_LOAD);
err_gp_load:
	gpio_free(OLPC_GPIO_DCON_IRQ);
err_gp_irq:
	gpio_free(OLPC_GPIO_DCON_STAT1);
err_gp_stat1:
	gpio_free(OLPC_GPIO_DCON_STAT0);
	return -EIO;
}

static void dcon_wiggle_xo_1(void)
{
	int x;

	cs5535_gpio_set(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_VAL);
	cs5535_gpio_set(OLPC_GPIO_SMB_DATA, GPIO_OUTPUT_VAL);
	cs5535_gpio_set(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_ENABLE);
	cs5535_gpio_set(OLPC_GPIO_SMB_DATA, GPIO_OUTPUT_ENABLE);
	cs5535_gpio_clear(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_AUX1);
	cs5535_gpio_clear(OLPC_GPIO_SMB_DATA, GPIO_OUTPUT_AUX1);
	cs5535_gpio_clear(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_AUX2);
	cs5535_gpio_clear(OLPC_GPIO_SMB_DATA, GPIO_OUTPUT_AUX2);
	cs5535_gpio_clear(OLPC_GPIO_SMB_CLK, GPIO_INPUT_AUX1);
	cs5535_gpio_clear(OLPC_GPIO_SMB_DATA, GPIO_INPUT_AUX1);

	for (x = 0; x < 16; x++) {
		udelay(5);
		cs5535_gpio_clear(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_VAL);
		udelay(5);
		cs5535_gpio_set(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_VAL);
	}
	udelay(5);
	cs5535_gpio_set(OLPC_GPIO_SMB_CLK, GPIO_OUTPUT_AUX1);
	cs5535_gpio_set(OLPC_GPIO_SMB_DATA, GPIO_OUTPUT_AUX1);
	cs5535_gpio_set(OLPC_GPIO_SMB_CLK, GPIO_INPUT_AUX1);
	cs5535_gpio_set(OLPC_GPIO_SMB_DATA, GPIO_INPUT_AUX1);
}

static void dcon_set_dconload_1(int val)
{
	gpio_set_value(OLPC_GPIO_DCON_LOAD, val);
}

static int dcon_read_status_xo_1(u8 *status)
{
	*status = gpio_get_value(OLPC_GPIO_DCON_STAT0);
	*status |= gpio_get_value(OLPC_GPIO_DCON_STAT1) << 1;

	
	cs5535_gpio_set(OLPC_GPIO_DCON_IRQ, GPIO_NEGATIVE_EDGE_STS);

	return 0;
}

struct dcon_platform_data dcon_pdata_xo_1 = {
	.init = dcon_init_xo_1,
	.bus_stabilize_wiggle = dcon_wiggle_xo_1,
	.set_dconload = dcon_set_dconload_1,
	.read_status = dcon_read_status_xo_1,
};
