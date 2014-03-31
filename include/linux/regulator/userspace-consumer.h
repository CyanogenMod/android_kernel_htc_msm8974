#ifndef __REGULATOR_PLATFORM_CONSUMER_H_
#define __REGULATOR_PLATFORM_CONSUMER_H_

struct regulator_consumer_supply;

struct regulator_userspace_consumer_data {
	const char *name;

	int num_supplies;
	struct regulator_bulk_data *supplies;

	bool init_on;
};

#endif 
