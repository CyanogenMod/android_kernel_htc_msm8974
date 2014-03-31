/*
 * IXP2400 MSF network device driver for the Radisys ENP2611
 * Copyright (C) 2004, 2005 Lennert Buytenhek <buytenh@wantstofly.org>
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <asm/hardware/uengine.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include "ixpdev.h"
#include "caleb.h"
#include "ixp2400-msf.h"
#include "pm3386.h"

static struct ixp2400_msf_parameters enp2611_msf_parameters =
{
	.rx_mode =		IXP2400_RX_MODE_UTOPIA_POS |
				IXP2400_RX_MODE_1x32 |
				IXP2400_RX_MODE_MPHY |
				IXP2400_RX_MODE_MPHY_32 |
				IXP2400_RX_MODE_MPHY_POLLED_STATUS |
				IXP2400_RX_MODE_MPHY_LEVEL3 |
				IXP2400_RX_MODE_RBUF_SIZE_64,

	.rxclk01_multiplier =	IXP2400_PLL_MULTIPLIER_16,

	.rx_poll_ports =	3,

	.rx_channel_mode = {
		IXP2400_PORT_RX_MODE_MASTER |
		IXP2400_PORT_RX_MODE_POS_PHY |
		IXP2400_PORT_RX_MODE_POS_PHY_L3 |
		IXP2400_PORT_RX_MODE_ODD_PARITY |
		IXP2400_PORT_RX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_RX_MODE_MASTER |
		IXP2400_PORT_RX_MODE_POS_PHY |
		IXP2400_PORT_RX_MODE_POS_PHY_L3 |
		IXP2400_PORT_RX_MODE_ODD_PARITY |
		IXP2400_PORT_RX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_RX_MODE_MASTER |
		IXP2400_PORT_RX_MODE_POS_PHY |
		IXP2400_PORT_RX_MODE_POS_PHY_L3 |
		IXP2400_PORT_RX_MODE_ODD_PARITY |
		IXP2400_PORT_RX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_RX_MODE_MASTER |
		IXP2400_PORT_RX_MODE_POS_PHY |
		IXP2400_PORT_RX_MODE_POS_PHY_L3 |
		IXP2400_PORT_RX_MODE_ODD_PARITY |
		IXP2400_PORT_RX_MODE_2_CYCLE_DECODE
	},

	.tx_mode =		IXP2400_TX_MODE_UTOPIA_POS |
				IXP2400_TX_MODE_1x32 |
				IXP2400_TX_MODE_MPHY |
				IXP2400_TX_MODE_MPHY_32 |
				IXP2400_TX_MODE_MPHY_POLLED_STATUS |
				IXP2400_TX_MODE_MPHY_LEVEL3 |
				IXP2400_TX_MODE_TBUF_SIZE_64,

	.txclk01_multiplier =	IXP2400_PLL_MULTIPLIER_16,

	.tx_poll_ports =	3,

	.tx_channel_mode = {
		IXP2400_PORT_TX_MODE_MASTER |
		IXP2400_PORT_TX_MODE_POS_PHY |
		IXP2400_PORT_TX_MODE_ODD_PARITY |
		IXP2400_PORT_TX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_TX_MODE_MASTER |
		IXP2400_PORT_TX_MODE_POS_PHY |
		IXP2400_PORT_TX_MODE_ODD_PARITY |
		IXP2400_PORT_TX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_TX_MODE_MASTER |
		IXP2400_PORT_TX_MODE_POS_PHY |
		IXP2400_PORT_TX_MODE_ODD_PARITY |
		IXP2400_PORT_TX_MODE_2_CYCLE_DECODE,

		IXP2400_PORT_TX_MODE_MASTER |
		IXP2400_PORT_TX_MODE_POS_PHY |
		IXP2400_PORT_TX_MODE_ODD_PARITY |
		IXP2400_PORT_TX_MODE_2_CYCLE_DECODE
	}
};

static struct net_device *nds[3];
static struct timer_list link_check_timer;

static void enp2611_check_link_status(unsigned long __dummy)
{
	int i;

	for (i = 0; i < 3; i++) {
		struct net_device *dev;
		int status;

		dev = nds[i];
		if (dev == NULL)
			continue;

		status = pm3386_is_link_up(i);
		if (status && !netif_carrier_ok(dev)) {
			
			printk(KERN_INFO "%s: NIC Link is Up\n", dev->name);

			pm3386_enable_tx(i);
			caleb_enable_tx(i);
			netif_carrier_on(dev);
		} else if (!status && netif_carrier_ok(dev)) {
			printk(KERN_INFO "%s: NIC Link is Down\n", dev->name);

			netif_carrier_off(dev);
			caleb_disable_tx(i);
			pm3386_disable_tx(i);
		}
	}

	link_check_timer.expires = jiffies + HZ / 10;
	add_timer(&link_check_timer);
}

static void enp2611_set_port_admin_status(int port, int up)
{
	if (up) {
		caleb_enable_rx(port);

		pm3386_set_carrier(port, 1);
		pm3386_enable_rx(port);
	} else {
		caleb_disable_tx(port);
		pm3386_disable_tx(port);
		
		pm3386_set_carrier(port, 0);

		pm3386_disable_rx(port);
		caleb_disable_rx(port);
	}
}

static int __init enp2611_init_module(void)
{ 
	int ports;
	int i;

	if (!machine_is_enp2611())
		return -ENODEV;

	caleb_reset();
	pm3386_reset();

	ports = pm3386_port_count();
	for (i = 0; i < ports; i++) {
		nds[i] = ixpdev_alloc(i, sizeof(struct ixpdev_priv));
		if (nds[i] == NULL) {
			while (--i >= 0)
				free_netdev(nds[i]);
			return -ENOMEM;
		}

		pm3386_init_port(i);
		pm3386_get_mac(i, nds[i]->dev_addr);
	}

	ixp2400_msf_init(&enp2611_msf_parameters);

	if (ixpdev_init(ports, nds, enp2611_set_port_admin_status)) {
		for (i = 0; i < ports; i++)
			if (nds[i])
				free_netdev(nds[i]);
		return -EINVAL;
	}

	init_timer(&link_check_timer);
	link_check_timer.function = enp2611_check_link_status;
	link_check_timer.expires = jiffies;
	add_timer(&link_check_timer);

	return 0;
}

static void __exit enp2611_cleanup_module(void)
{
	int i;

	del_timer_sync(&link_check_timer);

	ixpdev_deinit();
	for (i = 0; i < 3; i++)
		free_netdev(nds[i]);
}

module_init(enp2611_init_module);
module_exit(enp2611_cleanup_module);
MODULE_LICENSE("GPL");
