/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_SPMI_H
#define _LINUX_SPMI_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>

#define SPMI_MAX_SLAVE_ID		16

enum spmi_commands {
	SPMI_CMD_EXT_WRITE = 0x00,
	SPMI_CMD_RESET = 0x10,
	SPMI_CMD_SLEEP = 0x11,
	SPMI_CMD_SHUTDOWN = 0x12,
	SPMI_CMD_WAKEUP = 0x13,
	SPMI_CMD_AUTHENTICATE = 0x14,
	SPMI_CMD_MSTR_READ = 0x15,
	SPMI_CMD_MSTR_WRITE = 0x16,
	SPMI_CMD_TRANSFER_BUS_OWNERSHIP = 0x1A,
	SPMI_CMD_DDB_MASTER_READ = 0x1B,
	SPMI_CMD_DDB_SLAVE_READ = 0x1C,
	SPMI_CMD_EXT_READ = 0x20,
	SPMI_CMD_EXT_WRITEL = 0x30,
	SPMI_CMD_EXT_READL = 0x38,
	SPMI_CMD_WRITE = 0x40,
	SPMI_CMD_READ = 0x60,
	SPMI_CMD_ZERO_WRITE = 0x80,
};

struct spmi_device;

struct spmi_controller {
	struct device		dev;
	unsigned int		nr;
	struct completion	dev_released;
	int		(*cmd)(struct spmi_controller *, u8 opcode, u8 sid);
	int		(*read_cmd)(struct spmi_controller *,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf);
	int		(*write_cmd)(struct spmi_controller *,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf);
};
#define to_spmi_controller(d) container_of(d, struct spmi_controller, dev)

struct spmi_driver {
	int				(*probe)(struct spmi_device *dev);
	int				(*remove)(struct spmi_device *dev);
	void				(*shutdown)(struct spmi_device *dev);
	int				(*suspend)(struct spmi_device *dev,
					pm_message_t pmesg);
	int				(*resume)(struct spmi_device *dev);

	struct device_driver		driver;
	const struct spmi_device_id	*id_table;
};
#define to_spmi_driver(d) container_of(d, struct spmi_driver, driver)

struct spmi_resource {
	struct resource		*resource;
	u32			num_resources;
	struct device_node	*of_node;
	const char		*label;
};

struct spmi_device {
	struct device		dev;
	const char		*name;
	struct spmi_controller	*ctrl;
	struct spmi_resource	res;
	struct spmi_resource	*dev_node;
	u32			num_dev_node;
	u8			sid;
};
#define to_spmi_device(d) container_of(d, struct spmi_device, dev)

struct spmi_boardinfo {
	char			name[SPMI_NAME_SIZE];
	uint8_t			slave_id;
	struct device_node	*of_node;
	struct spmi_resource	res;
	struct spmi_resource	*dev_node;
	u32			num_dev_node;
	const void		*platform_data;
};

extern int spmi_driver_register(struct spmi_driver *drv);

static inline void spmi_driver_unregister(struct spmi_driver *sdrv)
{
	if (sdrv)
		driver_unregister(&sdrv->driver);
}

extern int spmi_add_controller(struct spmi_controller *ctrl);

extern int spmi_del_controller(struct spmi_controller *ctrl);

extern struct spmi_controller *spmi_busnum_to_ctrl(u32 bus_num);

extern struct spmi_device *spmi_alloc_device(struct spmi_controller *ctrl);

extern int spmi_add_device(struct spmi_device *spmi_dev);

extern struct spmi_device *spmi_new_device(struct spmi_controller *ctrl,
					struct spmi_boardinfo const *info);

extern void spmi_remove_device(struct spmi_device *spmi_dev);

#ifdef CONFIG_SPMI
extern int spmi_register_board_info(int busnum,
			struct spmi_boardinfo const *info, unsigned n);
#else
static inline int spmi_register_board_info(int busnum,
			struct spmi_boardinfo const *info, unsigned n)
{
	return 0;
}
#endif

static inline void *spmi_get_ctrldata(const struct spmi_controller *ctrl)
{
	return dev_get_drvdata(&ctrl->dev);
}

static inline void spmi_set_ctrldata(struct spmi_controller *ctrl, void *data)
{
	dev_set_drvdata(&ctrl->dev, data);
}

static inline void *spmi_get_devicedata(const struct spmi_device *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void spmi_set_devicedata(struct spmi_device *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline void spmi_dev_put(struct spmi_device *spmidev)
{
	if (spmidev)
		put_device(&spmidev->dev);
}

extern int spmi_register_read(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf);

extern int spmi_ext_register_read(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf, int len);

extern int spmi_ext_register_readl(struct spmi_controller *ctrl,
					u8 sid, u16 ad, u8 *buf, int len);

extern int spmi_register_write(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf);

/**
 * spmi_register_zero_write() - register zero write
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @data: the data to be written to register 0 (7-bits).
 *
 * Writes data to register 0 of the Slave device.
 */
extern int spmi_register_zero_write(struct spmi_controller *ctrl,
					u8 sid, u8 data);

extern int spmi_ext_register_write(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf, int len);

extern int spmi_ext_register_writel(struct spmi_controller *ctrl,
					u8 sid, u16 ad, u8 *buf, int len);

extern int spmi_command_reset(struct spmi_controller *ctrl, u8 sid);

extern int spmi_command_sleep(struct spmi_controller *ctrl, u8 sid);

extern int spmi_command_wakeup(struct spmi_controller *ctrl, u8 sid);

extern int spmi_command_shutdown(struct spmi_controller *ctrl, u8 sid);

#define spmi_for_each_container_dev(res, spmi_dev)			      \
	for (res = ((spmi_dev)->dev_node ? &(spmi_dev)->dev_node[0] : NULL);  \
	     (res - (spmi_dev)->dev_node) < (spmi_dev)->num_dev_node; res++)

extern struct resource *spmi_get_resource(struct spmi_device *dev,
				      struct spmi_resource *node,
				      unsigned int type, unsigned int res_num);

struct resource *spmi_get_resource_byname(struct spmi_device *dev,
					  struct spmi_resource *node,
					  unsigned int type,
					  const char *name);

extern int spmi_get_irq(struct spmi_device *dev, struct spmi_resource *node,
						 unsigned int res_num);

extern int spmi_get_irq_byname(struct spmi_device *dev,
			       struct spmi_resource *node, const char *name);

static inline const char *spmi_get_primary_dev_name(struct spmi_device *dev)
{
	if (dev->res.label)
		return dev->res.label;
	return NULL;
}

struct spmi_resource *spmi_get_dev_container_byname(struct spmi_device *dev,
						    const char *label);
#endif
