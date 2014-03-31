#ifndef __LINUX_MDIO_BITBANG_H
#define __LINUX_MDIO_BITBANG_H

#include <linux/phy.h>

struct module;

struct mdiobb_ctrl;

struct mdiobb_ops {
	struct module *owner;

	void (*set_mdc)(struct mdiobb_ctrl *ctrl, int level);

	void (*set_mdio_dir)(struct mdiobb_ctrl *ctrl, int output);

	void (*set_mdio_data)(struct mdiobb_ctrl *ctrl, int value);

	
	int (*get_mdio_data)(struct mdiobb_ctrl *ctrl);
};

struct mdiobb_ctrl {
	const struct mdiobb_ops *ops;
	
	int (*reset)(struct mii_bus *bus);
};

struct mii_bus *alloc_mdio_bitbang(struct mdiobb_ctrl *ctrl);

void free_mdio_bitbang(struct mii_bus *bus);

#endif
