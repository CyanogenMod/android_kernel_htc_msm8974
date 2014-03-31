/* linux/arch/arm/plat-samsung/include/plat/dma-s3c24xx.h
 *
 * Copyright (C) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Samsung S3C24XX DMA support - per SoC functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <plat/dma-core.h>

extern struct bus_type dma_subsys;
extern struct s3c2410_dma_chan s3c2410_chans[S3C_DMA_CHANNELS];

#define DMA_CH_VALID		(1<<31)
#define DMA_CH_NEVER		(1<<30)


struct s3c24xx_dma_map {
	const char		*name;

	unsigned long		 channels[S3C_DMA_CHANNELS];
	unsigned long		 channels_rx[S3C_DMA_CHANNELS];
};

struct s3c24xx_dma_selection {
	struct s3c24xx_dma_map	*map;
	unsigned long		 map_size;
	unsigned long		 dcon_mask;

	void	(*select)(struct s3c2410_dma_chan *chan,
			  struct s3c24xx_dma_map *map);

	void	(*direction)(struct s3c2410_dma_chan *chan,
			     struct s3c24xx_dma_map *map,
			     enum dma_data_direction dir);
};

extern int s3c24xx_dma_init_map(struct s3c24xx_dma_selection *sel);


struct s3c24xx_dma_order_ch {
	unsigned int	list[S3C_DMA_CHANNELS];	
	unsigned int	flags;				
};


struct s3c24xx_dma_order {
	struct s3c24xx_dma_order_ch	channels[DMACH_MAX];
};

extern int s3c24xx_dma_order_set(struct s3c24xx_dma_order *map);


extern int s3c2410_dma_init(void);

extern int s3c24xx_dma_init(unsigned int channels, unsigned int irq,
			    unsigned int stride);
