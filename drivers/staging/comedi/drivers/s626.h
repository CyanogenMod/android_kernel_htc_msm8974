/*
  comedi/drivers/s626.h
  Sensoray s626 Comedi driver, header file

  COMEDI - Linux Control and Measurement Device Interface
  Copyright (C) 2000 David A. Schleef <ds@schleef.org>

  Based on Sensoray Model 626 Linux driver Version 0.2
  Copyright (C) 2002-2004 Sensoray Co., Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/


#ifdef _DEBUG_
#define DEBUG(...);        printk(__VA_ARGS__);
#else
#define DEBUG(...)
#endif

#if !defined(TRUE)
#define TRUE    (1)
#endif

#if !defined(FALSE)
#define FALSE   (0)
#endif

#if !defined(INLINE)
#define INLINE static __inline
#endif

#include<linux/slab.h>

#define S626_SIZE 0x0200
#define SIZEOF_ADDRESS_SPACE		0x0200
#define DMABUF_SIZE			4096	

#define S626_ADC_CHANNELS       16
#define S626_DAC_CHANNELS       4
#define S626_ENCODER_CHANNELS   6
#define S626_DIO_CHANNELS       48
#define S626_DIO_BANKS		3	
#define S626_DIO_EXTCHANS	40	
					
					

#define NUM_TRIMDACS	12	

#define INTEL				1	
#define MOTOROLA			2	

#define PLATFORM		INTEL	

#define RANGE_5V                0x10	
#define RANGE_10V               0x00	

#define EOPL			0x80	
#define GSEL_BIPOLAR5V		0x00F0	
#define GSEL_BIPOLAR10V		0x00A0	

#define ERR_ILLEGAL_PARM	0x00010000	
#define ERR_I2C			0x00020000	
#define ERR_COUNTERSETUP	0x00200000	
#define ERR_DEBI_TIMEOUT	0x00400000	

#define ADC_DMABUF_DWORDS	40	
#define DAC_WDMABUF_DWORDS	1	


#define DAC_WDMABUF_OS		ADC_DMABUF_DWORDS

#define IRQ_GPIO3		0x00000040	
#define IRQ_RPS1                0x10000000
#define ISR_AFOU		0x00000800

#define IRQ_COINT1A             0x0400	
#define IRQ_COINT1B             0x0800	
#define IRQ_COINT2A             0x1000	
#define IRQ_COINT2B             0x2000	
#define IRQ_COINT3A             0x4000	
#define IRQ_COINT3B             0x8000	

#define RPS_CLRSIGNAL		0x00000000	
#define RPS_SETSIGNAL		0x10000000	
#define RPS_NOP			0x00000000	
#define RPS_PAUSE		0x20000000	
#define RPS_UPLOAD		0x40000000	
#define RPS_JUMP		0x80000000	
#define RPS_LDREG		0x90000100	
#define RPS_STREG		0xA0000100	
#define RPS_STOP		0x50000000	
#define RPS_IRQ                 0x60000000	

#define RPS_LOGICAL_OR		0x08000000	
#define RPS_INVERT		0x04000000	
#define RPS_DEBI		0x00000002	

#define RPS_SIG0		0x00200000	
#define RPS_SIG1		0x00400000	
#define RPS_SIG2		0x00800000	
#define RPS_GPIO2		0x00080000	
#define RPS_GPIO3		0x00100000	

#define RPS_SIGADC		RPS_SIG0	
#define RPS_SIGDAC		RPS_SIG1	

#define RPSCLK_SCALAR		8	
#define RPSCLK_PER_US		(33 / RPSCLK_SCALAR)	

#define SBA_RPS_A0		0x27	

#define GPIO_BASE		0x10004000	
#define GPIO1_LO		0x00000000	
#define GPIO1_HI		0x00001000	

#define PSR_DEBI_E		0x00040000	
#define PSR_DEBI_S		0x00080000	
#define PSR_A2_IN		0x00008000	
#define PSR_AFOU		0x00000800	
#define PSR_GPIO2		0x00000020	
#define PSR_EC0S		0x00000001	

#define SSR_AF2_OUT		0x00000200	

#define MC1_SOFT_RESET		0x80000000	
#define MC1_SHUTDOWN		0x3FFF0000	

#define MC1_ERPS1		0x2000	
#define MC1_ERPS0		0x1000	
#define MC1_DEBI		0x0800	
#define MC1_AUDIO		0x0200	
#define MC1_I2C			0x0100	
#define MC1_A2OUT		0x0008	
#define MC1_A2IN		0x0004	
#define MC1_A1IN		0x0001	

#define MC2_UPLD_DEBIq		0x00020002	
#define MC2_UPLD_IICq		0x00010001	
#define MC2_RPSSIG2_ONq		0x20002000	
#define MC2_RPSSIG1_ONq		0x10001000	
#define MC2_RPSSIG0_ONq		0x08000800	
#define MC2_UPLD_DEBI_MASKq	0x00000002	
#define MC2_UPLD_IIC_MASKq	0x00000001	
#define MC2_RPSSIG2_MASKq	0x00002000	
#define MC2_RPSSIG1_MASKq	0x00001000	
#define MC2_RPSSIG0_MASKq	0x00000800	

#define MC2_DELAYTRIG_4USq	MC2_RPSSIG1_ON
#define MC2_DELAYBUSY_4USq	MC2_RPSSIG1_MASK

#define	MC2_DELAYTRIG_6USq	MC2_RPSSIG2_ON
#define MC2_DELAYBUSY_6USq	MC2_RPSSIG2_MASK

#define MC2_UPLD_DEBI		0x0002	
#define MC2_UPLD_IIC		0x0001	
#define MC2_RPSSIG2		0x2000	
#define MC2_RPSSIG1		0x1000	
#define MC2_RPSSIG0		0x0800	

#define MC2_ADC_RPS		MC2_RPSSIG0	
#define MC2_DAC_RPS		MC2_RPSSIG1	

#define MC2_UPLD_DEBIQ		0x00020002	
#define MC2_UPLD_IICQ		0x00010001	

#define P_PCI_BT_A		0x004C	
#define P_DEBICFG               0x007C	
#define P_DEBICMD               0x0080	
#define P_DEBIPAGE              0x0084	
#define P_DEBIAD                0x0088	
#define P_I2CCTRL               0x008C	
#define P_I2CSTAT               0x0090	
#define P_BASEA2_IN		0x00AC	
#define P_PROTA2_IN		0x00B0	
#define P_PAGEA2_IN		0x00B4	
#define P_BASEA2_OUT		0x00B8	
#define P_PROTA2_OUT		0x00BC	
#define P_PAGEA2_OUT		0x00C0	
#define P_RPSPAGE0              0x00C4	
#define P_RPSPAGE1              0x00C8	
#define P_RPS0_TOUT		0x00D4	
#define P_RPS1_TOUT		0x00D8	
#define P_IER                   0x00DC	
#define P_GPIO                  0x00E0	
#define P_EC1SSR		0x00E4	
#define P_ECT1R			0x00EC	
#define P_ACON1                 0x00F4	
#define P_ACON2                 0x00F8	
#define P_MC1                   0x00FC	
#define P_MC2                   0x0100	
#define P_RPSADDR0              0x0104	
#define P_RPSADDR1              0x0108	
#define P_ISR                   0x010C	
#define P_PSR                   0x0110	
#define P_SSR                   0x0114	
#define P_EC1R			0x0118	
#define P_ADP4			0x0138	
#define P_FB_BUFFER1            0x0144	
#define P_FB_BUFFER2            0x0148	
#define P_TSL1                  0x0180	
#define P_TSL2                  0x01C0	

#define LP_DACPOL		0x0082	
#define LP_GSEL			0x0084	
#define LP_ISEL			0x0086	
#define LP_WRINTSELA		0x0042	
#define LP_WREDGSELA		0x0044	
#define LP_WRCAPSELA		0x0046	
#define LP_WRDOUTA		0x0048	
#define LP_WRINTSELB		0x0052	
#define LP_WREDGSELB		0x0054	
#define LP_WRCAPSELB		0x0056	
#define LP_WRDOUTB		0x0058	
#define LP_WRINTSELC		0x0062	
#define LP_WREDGSELC		0x0064	
#define LP_WRCAPSELC		0x0066	
#define LP_WRDOUTC		0x0068	

#define LP_RDDINA		0x0040	
#define LP_RDCAPFLGA		0x0048	
#define LP_RDINTSELA		0x004A	
#define LP_RDEDGSELA		0x004C	
#define LP_RDCAPSELA		0x004E	
#define LP_RDDINB		0x0050	
#define LP_RDCAPFLGB		0x0058	
#define LP_RDINTSELB		0x005A	
#define LP_RDEDGSELB		0x005C	
#define LP_RDCAPSELB		0x005E	
#define LP_RDDINC		0x0060	
#define LP_RDCAPFLGC		0x0068	
#define LP_RDINTSELC		0x006A	
#define LP_RDEDGSELC		0x006C	
#define LP_RDCAPSELC		0x006E	

#define LP_CR0A			0x0000	
#define LP_CR0B			0x0002	
#define LP_CR1A			0x0004	
#define LP_CR1B			0x0006	
#define LP_CR2A			0x0008	
#define LP_CR2B			0x000A	

#define	LP_CNTR0ALSW		0x000C	
#define	LP_CNTR0AMSW		0x000E	
#define	LP_CNTR0BLSW		0x0010	
#define	LP_CNTR0BMSW		0x0012	
#define	LP_CNTR1ALSW		0x0014	
#define	LP_CNTR1AMSW		0x0016	
#define	LP_CNTR1BLSW		0x0018	
#define	LP_CNTR1BMSW		0x001A	
#define	LP_CNTR2ALSW		0x001C	
#define	LP_CNTR2AMSW		0x001E	
#define	LP_CNTR2BLSW		0x0020	
#define	LP_CNTR2BMSW		0x0022	

#define LP_MISC1		0x0088	
#define LP_WRMISC2		0x0090	
#define LP_RDMISC2		0x0082	

#define MISC1_WENABLE		0x8000	
#define MISC1_WDISABLE		0x0000	
#define MISC1_EDCAP		0x1000	
#define MISC1_NOEDCAP		0x0000	

#define RDMISC1_WDTIMEOUT	0x4000	

#define WRMISC2_WDCLEAR		0x8000	
#define WRMISC2_CHARGE_ENABLE	0x4000	

#define MISC2_BATT_ENABLE	0x0008	
#define MISC2_WDENABLE		0x0004	
#define MISC2_WDPERIOD_MASK	0x0003	
						

#define A2_RUN			0x40000000	
#define A1_RUN			0x20000000	
#define A1_SWAP			0x00200000	
#define A2_SWAP			0x00100000	
#define WS_MODES		0x00019999	
						
						

#if PLATFORM == INTEL		
#define ACON1_BASE		(WS_MODES | A1_RUN)
#elif PLATFORM == MOTOROLA
#define ACON1_BASE		(WS_MODES | A1_RUN | A1_SWAP | A2_SWAP)
#endif

#define ACON1_ADCSTART		ACON1_BASE	
#define ACON1_DACSTART		(ACON1_BASE | A2_RUN)
#define ACON1_DACSTOP		ACON1_BASE	

#define A1_CLKSRC_BCLK1		0x00000000	
#define A2_CLKSRC_X1		0x00800000	
#define A2_CLKSRC_X2		0x00C00000	
#define A2_CLKSRC_X4		0x01400000	
#define INVERT_BCLK2		0x00100000	
#define BCLK2_OE		0x00040000	
#define ACON2_XORMASK		0x000C0000	
						

#define ACON2_INIT		(ACON2_XORMASK ^ (A1_CLKSRC_BCLK1 | A2_CLKSRC_X2 | INVERT_BCLK2 | BCLK2_OE))

#define WS1		     	0x40000000	
#define WS2		     	0x20000000
#define WS3		     	0x10000000
#define WS4		     	0x08000000
#define RSD1			0x01000000	
#define SDW_A1			0x00800000	
#define SIB_A1			0x00400000	
#define SF_A1			0x00200000	

#define XFIFO_0			0x00000000	
#define XFIFO_1			0x00000010	
#define XFIFO_2			0x00000020	
#define XFIFO_3			0x00000030	
#define XFB0			0x00000040	
#define XFB1			0x00000050	
#define XFB2			0x00000060	
#define XFB3			0x00000070	
#define SIB_A2			0x00000200	
#define SF_A2			0x00000100	
#define LF_A2			0x00000080	
#define XSD2			0x00000008	
#define RSD3			0x00001800	
#define RSD2			0x00001000	
#define LOW_A2			0x00000002	
						
						
#define EOS		     	0x00000001	

#define I2C_CLKSEL		0x0400

#define I2C_BITRATE		68.75

#define I2C_WRTIME		15.0


#define I2C_RETRIES		(I2C_WRTIME * I2C_BITRATE / 9.0)
#define I2C_ERR			0x0002	
						
#define I2C_BUSY		0x0001	
						
#define I2C_ABORT		0x0080	
#define I2C_ATTRSTART		0x3	
#define I2C_ATTRCONT		0x2	
#define I2C_ATTRSTOP		0x1	
#define I2C_ATTRNOP		0x0	

#define I2CR			(devpriv->I2CAdrs | 1)

#define I2CW			(devpriv->I2CAdrs)

#define I2C_B2(ATTR, VAL)	(((ATTR) << 6) | ((VAL) << 24))
#define I2C_B1(ATTR, VAL)	(((ATTR) << 4) | ((VAL) << 16))
#define I2C_B0(ATTR, VAL)	(((ATTR) << 2) | ((VAL) <<  8))

#define P_DEBICFGq              0x007C	
#define P_DEBICMDq              0x0080	
#define P_DEBIPAGEq             0x0084	
#define P_DEBIADq               0x0088	

#define DEBI_CFG_TOQ		0x03C00000	
#define DEBI_CFG_FASTQ		0x10000000	
#define DEBI_CFG_16Q		0x00080000	
#define DEBI_CFG_INCQ		0x00040000	
#define DEBI_CFG_TIMEROFFQ	0x00010000	
#define DEBI_CMD_RDQ		0x00050000	
#define DEBI_CMD_WRQ		0x00040000	
#define DEBI_PAGE_DISABLEQ	0x00000000	

#define DEBI_CMD_SIZE16		(2 << 17)	
						
#define DEBI_CMD_READ		0x00010000	
#define DEBI_CMD_WRITE		0x00000000	

#define DEBI_CMD_RDWORD		(DEBI_CMD_READ  | DEBI_CMD_SIZE16)

#define DEBI_CMD_WRWORD		(DEBI_CMD_WRITE | DEBI_CMD_SIZE16)

#define DEBI_CFG_XIRQ_EN	0x80000000	
						
#define DEBI_CFG_XRESUME	0x40000000	
						
						
#define DEBI_CFG_FAST		0x10000000	

#define DEBI_CFG_TOUT_BIT	22	
					

#define DEBI_CFG_SWAP_NONE	0x00000000	
						
						
#define DEBI_CFG_SWAP_2		0x00100000	
#define DEBI_CFG_SWAP_4		0x00200000	
#define DEBI_CFG_16		0x00080000	
						
						

#define DEBI_CFG_SLAVE16	0x00080000	
						
						
#define DEBI_CFG_INC		0x00040000	
						
						
#define DEBI_CFG_INTEL		0x00020000	
#define DEBI_CFG_TIMEROFF	0x00010000	

#if PLATFORM == INTEL

#define DEBI_TOUT		7	
						
						

#define DEBI_SWAP		DEBI_CFG_SWAP_NONE

#elif PLATFORM == MOTOROLA

#define DEBI_TOUT		15	
					
#define DEBI_SWAP		DEBI_CFG_SWAP_2	

#endif

#define DEBI_PAGE_DISABLE	0x00000000	


#define LOADSRC_INDX		0	
					
#define LOADSRC_OVER		1	
					
#define LOADSRCB_OVERA		2	
					
#define LOADSRC_NONE		3	

#define INTSRC_NONE 		0	
#define INTSRC_OVER 		1	
#define INTSRC_INDX 		2	
#define INTSRC_BOTH 		3	

#define LATCHSRC_AB_READ	0	
#define LATCHSRC_A_INDXA	1	
#define LATCHSRC_B_INDXB	2	
#define LATCHSRC_B_OVERA	3	

#define INDXSRC_HARD		0	
#define INDXSRC_SOFT		1	

#define INDXPOL_POS 		0	
#define INDXPOL_NEG 		1	

#define CLKSRC_COUNTER		0	
#define CLKSRC_TIMER		2	
#define CLKSRC_EXTENDER		3	

#define CLKPOL_POS		0	
					
#define CLKPOL_NEG		1	
					
#define CNTDIR_UP		0	
#define CNTDIR_DOWN 		1	

#define CLKENAB_ALWAYS		0	
#define CLKENAB_INDEX		1	

#define CLKMULT_4X 		0	
#define CLKMULT_2X 		1	
#define CLKMULT_1X 		2	

#define BF_LOADSRC		9	
#define BF_INDXSRC		7	
#define BF_INDXPOL		6	
#define BF_CLKSRC		4	
#define BF_CLKPOL		3	
#define BF_CLKMULT		1	
#define BF_CLKENAB		0	


#define CLKSRC_COUNTER		0	
					
#define CLKSRC_TIMER		2	
					
					
#define CLKSRC_EXTENDER		3	
					


#define MULT_X0			0x0003	
					
					
#define MULT_X1			0x0002	
					
					
#define MULT_X2			0x0001	
					
					
#define MULT_X4			0x0000	
					
					


#define NUM_COUNTERS		6	
					
#define NUM_INTSOURCES		4
#define NUM_LATCHSOURCES	4
#define NUM_CLKMULTS		4
#define NUM_CLKSOURCES		4
#define NUM_CLKPOLS		2
#define NUM_INDEXPOLS		2
#define NUM_INDEXSOURCES	2
#define NUM_LOADTRIGS		4


#define CRABIT_INDXSRC_B	14	
#define CRABIT_CLKSRC_B		12	
#define CRABIT_INDXPOL_A	11	
#define CRABIT_LOADSRC_A	 9	
#define CRABIT_CLKMULT_A	 7	
#define CRABIT_INTSRC_A		 5	
#define CRABIT_CLKPOL_A		 4	
#define CRABIT_INDXSRC_A	 2	
#define CRABIT_CLKSRC_A		 0	

#define CRBBIT_INTRESETCMD	15	
#define CRBBIT_INTRESET_B	14	
#define CRBBIT_INTRESET_A	13	
#define CRBBIT_CLKENAB_A	12	
#define CRBBIT_INTSRC_B		10	
#define CRBBIT_LATCHSRC		 8	
#define CRBBIT_LOADSRC_B	 6	
#define CRBBIT_CLKMULT_B	 3	
#define CRBBIT_CLKENAB_B	 2	
#define CRBBIT_INDXPOL_B	 1	
#define CRBBIT_CLKPOL_B		 0	


#define CRAMSK_INDXSRC_B	((uint16_t)(3 << CRABIT_INDXSRC_B))
#define CRAMSK_CLKSRC_B		((uint16_t)(3 << CRABIT_CLKSRC_B))
#define CRAMSK_INDXPOL_A	((uint16_t)(1 << CRABIT_INDXPOL_A))
#define CRAMSK_LOADSRC_A	((uint16_t)(3 << CRABIT_LOADSRC_A))
#define CRAMSK_CLKMULT_A	((uint16_t)(3 << CRABIT_CLKMULT_A))
#define CRAMSK_INTSRC_A		((uint16_t)(3 << CRABIT_INTSRC_A))
#define CRAMSK_CLKPOL_A		((uint16_t)(3 << CRABIT_CLKPOL_A))
#define CRAMSK_INDXSRC_A	((uint16_t)(3 << CRABIT_INDXSRC_A))
#define CRAMSK_CLKSRC_A		((uint16_t)(3 << CRABIT_CLKSRC_A))

#define CRBMSK_INTRESETCMD	((uint16_t)(1 << CRBBIT_INTRESETCMD))
#define CRBMSK_INTRESET_B	((uint16_t)(1 << CRBBIT_INTRESET_B))
#define CRBMSK_INTRESET_A	((uint16_t)(1 << CRBBIT_INTRESET_A))
#define CRBMSK_CLKENAB_A	((uint16_t)(1 << CRBBIT_CLKENAB_A))
#define CRBMSK_INTSRC_B		((uint16_t)(3 << CRBBIT_INTSRC_B))
#define CRBMSK_LATCHSRC		((uint16_t)(3 << CRBBIT_LATCHSRC))
#define CRBMSK_LOADSRC_B	((uint16_t)(3 << CRBBIT_LOADSRC_B))
#define CRBMSK_CLKMULT_B	((uint16_t)(3 << CRBBIT_CLKMULT_B))
#define CRBMSK_CLKENAB_B	((uint16_t)(1 << CRBBIT_CLKENAB_B))
#define CRBMSK_INDXPOL_B	((uint16_t)(1 << CRBBIT_INDXPOL_B))
#define CRBMSK_CLKPOL_B		((uint16_t)(1 << CRBBIT_CLKPOL_B))

#define CRBMSK_INTCTRL		(CRBMSK_INTRESETCMD | CRBMSK_INTRESET_A | CRBMSK_INTRESET_B)	


#define STDBIT_INTSRC		13
#define STDBIT_LATCHSRC		11
#define STDBIT_LOADSRC		 9
#define STDBIT_INDXSRC		 7
#define STDBIT_INDXPOL		 6
#define STDBIT_CLKSRC		 4
#define STDBIT_CLKPOL		 3
#define STDBIT_CLKMULT		 1
#define STDBIT_CLKENAB		 0


#define STDMSK_INTSRC		((uint16_t)(3 << STDBIT_INTSRC))
#define STDMSK_LATCHSRC		((uint16_t)(3 << STDBIT_LATCHSRC))
#define STDMSK_LOADSRC		((uint16_t)(3 << STDBIT_LOADSRC))
#define STDMSK_INDXSRC		((uint16_t)(1 << STDBIT_INDXSRC))
#define STDMSK_INDXPOL		((uint16_t)(1 << STDBIT_INDXPOL))
#define STDMSK_CLKSRC		((uint16_t)(3 << STDBIT_CLKSRC))
#define STDMSK_CLKPOL		((uint16_t)(1 << STDBIT_CLKPOL))
#define STDMSK_CLKMULT		((uint16_t)(3 << STDBIT_CLKMULT))
#define STDMSK_CLKENAB		((uint16_t)(1 << STDBIT_CLKENAB))

struct bufferDMA {
	dma_addr_t PhysicalBase;
	void *LogicalBase;
	uint32_t DMAHandle;
};
