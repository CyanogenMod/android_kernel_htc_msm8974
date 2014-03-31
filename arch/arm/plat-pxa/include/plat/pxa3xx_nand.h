#ifndef __ASM_ARCH_PXA3XX_NAND_H
#define __ASM_ARCH_PXA3XX_NAND_H

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

struct pxa3xx_nand_timing {
	unsigned int	tCH;  
	unsigned int	tCS;  
	unsigned int	tWH;  
	unsigned int	tWP;  
	unsigned int	tRH;  
	unsigned int	tRP;  
	unsigned int	tR;   
	unsigned int	tWHR; 
	unsigned int	tAR;  
};

struct pxa3xx_nand_cmdset {
	uint16_t	read1;
	uint16_t	read2;
	uint16_t	program;
	uint16_t	read_status;
	uint16_t	read_id;
	uint16_t	erase;
	uint16_t	reset;
	uint16_t	lock;
	uint16_t	unlock;
	uint16_t	lock_status;
};

struct pxa3xx_nand_flash {
	char		*name;
	uint32_t	chip_id;
	unsigned int	page_per_block; 
	unsigned int	page_size;	
	unsigned int	flash_width;	
	unsigned int	dfc_width;	
	unsigned int	num_blocks;	

	struct pxa3xx_nand_timing *timing;	
};


#define NUM_CHIP_SELECT		(2)
struct pxa3xx_nand_platform_data {

	int	enable_arbiter;

	
	int	keep_config;

	
	int	num_cs;

	const struct mtd_partition		*parts[NUM_CHIP_SELECT];
	unsigned int				nr_parts[NUM_CHIP_SELECT];

	const struct pxa3xx_nand_flash * 	flash;
	size_t					num_flash;
};

extern void pxa3xx_set_nand_info(struct pxa3xx_nand_platform_data *info);
#endif 
