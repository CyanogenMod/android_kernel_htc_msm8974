/*
 * BF5XX - NAND flash controller platform_device info
 *
 * Copyright 2007-2008 Analog Devices, Inc.
 *
 * Licensed under the GPL-2
 */


#define NFC_PG_SIZE_OFFSET	9

#define NFC_NWIDTH_8		0
#define NFC_NWIDTH_16		1
#define NFC_NWIDTH_OFFSET	8

#define NFC_RDDLY_OFFSET	4
#define NFC_WRDLY_OFFSET	0

#define NFC_STAT_NBUSY		1

struct bf5xx_nand_platform {
	
	unsigned short		data_width;

	
	unsigned short		rd_dly;
	unsigned short		wr_dly;

	
	int                     nr_partitions;
	struct mtd_partition    *partitions;
};
