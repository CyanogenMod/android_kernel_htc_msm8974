/* linux/arch/arm/mach-s3c6400/include/mach/dma.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S3C6400 - DMA support
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H __FILE__

#define S3C_DMA_CHANNELS	(16)


enum dma_ch {
	
	DMACH_UART0 = 0,
	DMACH_UART0_SRC2,
	DMACH_UART1,
	DMACH_UART1_SRC2,
	DMACH_UART2,
	DMACH_UART2_SRC2,
	DMACH_UART3,
	DMACH_UART3_SRC2,
	DMACH_PCM0_TX,
	DMACH_PCM0_RX,
	DMACH_I2S0_OUT,
	DMACH_I2S0_IN,
	DMACH_SPI0_TX,
	DMACH_SPI0_RX,
	DMACH_HSI_I2SV40_TX,
	DMACH_HSI_I2SV40_RX,

	
	DMACH_PCM1_TX = 16,
	DMACH_PCM1_RX,
	DMACH_I2S1_OUT,
	DMACH_I2S1_IN,
	DMACH_SPI1_TX,
	DMACH_SPI1_RX,
	DMACH_AC97_PCMOUT,
	DMACH_AC97_PCMIN,
	DMACH_AC97_MICIN,
	DMACH_PWM,
	DMACH_IRDA,
	DMACH_EXTERNAL,
	DMACH_RES1,
	DMACH_RES2,
	DMACH_SECURITY_RX,	
	DMACH_SECURITY_TX,	
	DMACH_MAX		
};

static inline bool samsung_dma_has_circular(void)
{
	return true;
}

static inline bool samsung_dma_is_dmadev(void)
{
	return false;
}
#define S3C2410_DMAF_CIRCULAR		(1 << 0)

#include <plat/dma.h>

#define DMACH_LOW_LEVEL (1<<28) 

struct s3c64xx_dma_buff;

struct s3c64xx_dma_buff {
	struct s3c64xx_dma_buff *next;

	void			*pw;
	struct pl080s_lli	*lli;
	dma_addr_t		 lli_dma;
};

struct s3c64xx_dmac;

struct s3c2410_dma_chan {
	unsigned char		 number;      
	unsigned char		 in_use;      
	unsigned char		 bit;	      
	unsigned char		 hw_width;
	unsigned char		 peripheral;

	unsigned int		 flags;
	enum dma_data_direction	 source;


	dma_addr_t		dev_addr;

	struct s3c2410_dma_client *client;
	struct s3c64xx_dmac	*dmac;		

	void __iomem		*regs;

	
	s3c2410_dma_cbfn_t	 callback_fn;	
	s3c2410_dma_opfn_t	 op_fn;		

	
	struct s3c64xx_dma_buff	*curr;		
	struct s3c64xx_dma_buff	*next;		
	struct s3c64xx_dma_buff	*end;		

};

#include <plat/dma-core.h>

#endif 
