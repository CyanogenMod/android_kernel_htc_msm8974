/* arch/arm/mach-s3c2410/include/mach/nand.h
 *
 * Copyright (c) 2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 - NAND device controller platform_device info
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct s3c2410_nand_set {
	unsigned int		disable_ecc:1;
	unsigned int		flash_bbt:1;

	unsigned int		options;
	int			nr_chips;
	int			nr_partitions;
	char			*name;
	int			*nr_map;
	struct mtd_partition	*partitions;
	struct nand_ecclayout	*ecc_layout;
};

struct s3c2410_platform_nand {
	

	int	tacls;	
	int	twrph0;	
	int	twrph1;	

	unsigned int	ignore_unset_ecc:1;

	int			nr_sets;
	struct s3c2410_nand_set *sets;

	void			(*select_chip)(struct s3c2410_nand_set *,
					       int chip);
};

extern void s3c_nand_set_platdata(struct s3c2410_platform_nand *nand);
