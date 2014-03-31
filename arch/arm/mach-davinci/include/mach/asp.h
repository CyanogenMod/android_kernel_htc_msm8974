#ifndef __ASM_ARCH_DAVINCI_ASP_H
#define __ASM_ARCH_DAVINCI_ASP_H

#include <mach/irqs.h>
#include <mach/edma.h>

#define DAVINCI_ASP0_BASE	0x01E02000
#define DAVINCI_ASP1_BASE	0x01E04000

#define DAVINCI_DM365_ASP0_BASE	0x01D02000

#define	DAVINCI_DM646X_MCASP0_REG_BASE		0x01D01000
#define DAVINCI_DM646X_MCASP1_REG_BASE		0x01D01800

#define DAVINCI_DA8XX_MCASP0_REG_BASE	0x01D00000

#define DAVINCI_DA830_MCASP1_REG_BASE	0x01D04000

#define DAVINCI_DMA_ASP0_TX	2
#define DAVINCI_DMA_ASP0_RX	3
#define DAVINCI_DMA_ASP1_TX	8
#define DAVINCI_DMA_ASP1_RX	9

#define	DAVINCI_DM646X_DMA_MCASP0_AXEVT0	6
#define	DAVINCI_DM646X_DMA_MCASP0_AREVT0	9
#define	DAVINCI_DM646X_DMA_MCASP1_AXEVT1	12

#define	DAVINCI_DA8XX_DMA_MCASP0_AREVT	0
#define	DAVINCI_DA8XX_DMA_MCASP0_AXEVT	1

#define	DAVINCI_DA830_DMA_MCASP1_AREVT	2
#define	DAVINCI_DA830_DMA_MCASP1_AXEVT	3

#define DAVINCI_ASP0_RX_INT	IRQ_MBRINT
#define DAVINCI_ASP0_TX_INT	IRQ_MBXINT
#define DAVINCI_ASP1_RX_INT	IRQ_MBRINT
#define DAVINCI_ASP1_TX_INT	IRQ_MBXINT

struct snd_platform_data {
	u32 tx_dma_offset;
	u32 rx_dma_offset;
	enum dma_event_q asp_chan_q;	
	enum dma_event_q ram_chan_q;	
	unsigned int codec_fmt;
	unsigned enable_channel_combine:1;
	unsigned sram_size_playback;
	unsigned sram_size_capture;

	int clk_input_pin;

	bool i2s_accurate_sck;

	
	int tdm_slots;
	u8 op_mode;
	u8 num_serializer;
	u8 *serial_dir;
	u8 version;
	u8 txnumevt;
	u8 rxnumevt;
};

enum {
	MCASP_VERSION_1 = 0,	
	MCASP_VERSION_2,	
};

enum dm365_clk_input_pin {
	MCBSP_CLKR = 0,		
	MCBSP_CLKS,
};

#define INACTIVE_MODE	0
#define TX_MODE		1
#define RX_MODE		2

#define DAVINCI_MCASP_IIS_MODE	0
#define DAVINCI_MCASP_DIT_MODE	1

#endif 
