#ifndef __LINUX_BQ27X00_BATTERY_H__
#define __LINUX_BQ27X00_BATTERY_H__

struct bq27000_platform_data {
	const char *name;
	int (*read)(struct device *dev, unsigned int);
};

#endif
