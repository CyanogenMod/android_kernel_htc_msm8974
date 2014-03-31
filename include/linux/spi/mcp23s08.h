

struct mcp23s08_chip_info {
	bool		is_present;	
	unsigned	pullups;	
};

struct mcp23s08_platform_data {
	struct mcp23s08_chip_info	chip[8];

	unsigned	base;
};
