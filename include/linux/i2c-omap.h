#ifndef __I2C_OMAP_H__
#define __I2C_OMAP_H__

#include <linux/platform_device.h>


#define OMAP_I2C_IP_VERSION_1 1
#define OMAP_I2C_IP_VERSION_2 2


#define OMAP_I2C_FLAG_NO_FIFO			BIT(0)
#define OMAP_I2C_FLAG_SIMPLE_CLOCK		BIT(1)
#define OMAP_I2C_FLAG_16BIT_DATA_REG		BIT(2)
#define OMAP_I2C_FLAG_RESET_REGS_POSTIDLE	BIT(3)
#define OMAP_I2C_FLAG_APPLY_ERRATA_I207	BIT(4)
#define OMAP_I2C_FLAG_ALWAYS_ARMXOR_CLK	BIT(5)
#define OMAP_I2C_FLAG_FORCE_19200_INT_CLK	BIT(6)
#define OMAP_I2C_FLAG_BUS_SHIFT_NONE 0
#define OMAP_I2C_FLAG_BUS_SHIFT_1		BIT(7)
#define OMAP_I2C_FLAG_BUS_SHIFT_2		BIT(8)
#define OMAP_I2C_FLAG_BUS_SHIFT__SHIFT 7

struct omap_i2c_bus_platform_data {
	u32		clkrate;
	u32		rev;
	u32		flags;
	void		(*set_mpu_wkup_lat)(struct device *dev, long set);
};

#endif
