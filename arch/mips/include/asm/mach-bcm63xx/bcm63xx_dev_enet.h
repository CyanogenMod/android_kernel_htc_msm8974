#ifndef BCM63XX_DEV_ENET_H_
#define BCM63XX_DEV_ENET_H_

#include <linux/if_ether.h>
#include <linux/init.h>

struct bcm63xx_enet_platform_data {
	char mac_addr[ETH_ALEN];

	int has_phy;

	
	int use_internal_phy;

	
	int phy_id;
	int has_phy_interrupt;
	int phy_interrupt;

	int pause_auto;
	int pause_rx;
	int pause_tx;

	
	int force_speed_100;
	int force_duplex_full;

	int (*mii_config)(struct net_device *dev, int probe,
			  int (*mii_read)(struct net_device *dev,
					  int phy_id, int reg),
			  void (*mii_write)(struct net_device *dev,
					    int phy_id, int reg, int val));
};

int __init bcm63xx_enet_register(int unit,
				 const struct bcm63xx_enet_platform_data *pd);

#endif 
