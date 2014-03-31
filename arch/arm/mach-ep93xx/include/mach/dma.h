#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#include <linux/types.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#define EP93XX_DMA_I2S1		0
#define EP93XX_DMA_I2S2		1
#define EP93XX_DMA_AAC1		2
#define EP93XX_DMA_AAC2		3
#define EP93XX_DMA_AAC3		4
#define EP93XX_DMA_I2S3		5
#define EP93XX_DMA_UART1	6
#define EP93XX_DMA_UART2	7
#define EP93XX_DMA_UART3	8
#define EP93XX_DMA_IRDA		9
#define EP93XX_DMA_SSP		10
#define EP93XX_DMA_IDE		11

struct ep93xx_dma_data {
	int				port;
	enum dma_transfer_direction	direction;
	const char			*name;
};

struct ep93xx_dma_chan_data {
	const char			*name;
	void __iomem			*base;
	int				irq;
};

struct ep93xx_dma_platform_data {
	struct ep93xx_dma_chan_data	*channels;
	size_t				num_channels;
};

static inline bool ep93xx_dma_chan_is_m2p(struct dma_chan *chan)
{
	return !strcmp(dev_name(chan->device->dev), "ep93xx-dma-m2p");
}

static inline enum dma_transfer_direction
ep93xx_dma_chan_direction(struct dma_chan *chan)
{
	if (!ep93xx_dma_chan_is_m2p(chan))
		return DMA_NONE;

	
	return (chan->chan_id % 2 == 0) ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM;
}

#endif 
