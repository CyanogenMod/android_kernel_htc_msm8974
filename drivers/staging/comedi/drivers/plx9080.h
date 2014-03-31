/* plx9080.h
 *
 * Copyright (C) 2002,2003 Frank Mori Hess <fmhess@users.sourceforge.net>
 *
 * I modified this file from the plx9060.h header for the
 * wanXL device driver in the linux kernel,
 * for the register offsets and bit definitions.  Made minor modifications,
 * added plx9080 registers and
 * stripped out stuff that was specifically for the wanXL driver.
 * Note: I've only made sure the definitions are correct as far
 * as I make use of them.  There are still various plx9060-isms
 * left in this header file.
 *
 ********************************************************************
 *
 * Copyright (C) 1999 RG Studio s.c.
 * Written by Krzysztof Halasa <khc@rgstudio.com.pl>
 *
 * Portions (C) SBE Inc., used by permission.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __COMEDI_PLX9080_H
#define __COMEDI_PLX9080_H

struct plx_dma_desc {
	volatile uint32_t pci_start_addr;
	volatile uint32_t local_start_addr;
	
	volatile uint32_t transfer_size;
	volatile uint32_t next;
};


#define PLX_LAS0RNG_REG         0x0000	
#define PLX_LAS1RNG_REG         0x00f0	
#define  LRNG_IO           0x00000001	
#define  LRNG_ANY32        0x00000000	
#define  LRNG_LT1MB        0x00000002	
#define  LRNG_ANY64        0x00000004	
#define  LRNG_MEM_MASK     0xfffffff0	
#define  LRNG_IO_MASK     0xfffffffa	

#define PLX_LAS0MAP_REG         0x0004	
#define PLX_LAS1MAP_REG         0x00f4	
#define  LMAP_EN           0x00000001	
#define  LMAP_MEM_MASK     0xfffffff0	
#define  LMAP_IO_MASK     0xfffffffa	

#define PLX_MARB_REG         0x8	
#define PLX_DMAARB_REG      0xac
enum marb_bits {
	MARB_LLT_MASK = 0x000000ff,	
	MARB_LPT_MASK = 0x0000ff00,	
	MARB_LTEN = 0x00010000,	
	MARB_LPEN = 0x00020000,	
	MARB_BREQ = 0x00040000,	
	MARB_DMA_PRIORITY_MASK = 0x00180000,
	MARB_LBDS_GIVE_UP_BUS_MODE = 0x00200000,	
	MARB_DS_LLOCK_ENABLE = 0x00400000,	
	MARB_PCI_REQUEST_MODE = 0x00800000,
	MARB_PCIv21_MODE = 0x01000000,	
	MARB_PCI_READ_NO_WRITE_MODE = 0x02000000,
	MARB_PCI_READ_WITH_WRITE_FLUSH_MODE = 0x04000000,
	MARB_GATE_TIMER_WITH_BREQ = 0x08000000,	
	MARB_PCI_READ_NO_FLUSH_MODE = 0x10000000,
	MARB_USE_SUBSYSTEM_IDS = 0x20000000,
};

#define PLX_BIGEND_REG 0xc
enum bigend_bits {
	BIGEND_CONFIG = 0x1,	
	BIGEND_DIRECT_MASTER = 0x2,
	BIGEND_DIRECT_SLAVE_LOCAL0 = 0x4,
	BIGEND_ROM = 0x8,
	BIGEND_BYTE_LANE = 0x10,	
	BIGEND_DIRECT_SLAVE_LOCAL1 = 0x20,
	BIGEND_DMA1 = 0x40,
	BIGEND_DMA0 = 0x80,
};

#define PLX_ROMRNG_REG         0x0010	
#define PLX_ROMMAP_REG         0x0014	

#define PLX_REGION0_REG         0x0018	
#define  RGN_WIDTH         0x00000002	
#define  RGN_8BITS         0x00000000	
#define  RGN_16BITS        0x00000001	
#define  RGN_32BITS        0x00000002	
#define  RGN_MWS           0x0000003C	
#define  RGN_0MWS          0x00000000
#define  RGN_1MWS          0x00000004
#define  RGN_2MWS          0x00000008
#define  RGN_3MWS          0x0000000C
#define  RGN_4MWS          0x00000010
#define  RGN_6MWS          0x00000018
#define  RGN_8MWS          0x00000020
#define  RGN_MRE           0x00000040	
#define  RGN_MBE           0x00000080	
#define  RGN_READ_PREFETCH_DISABLE 0x00000100
#define  RGN_ROM_PREFETCH_DISABLE 0x00000200
#define  RGN_READ_PREFETCH_COUNT_ENABLE 0x00000400
#define  RGN_RWS           0x003C0000	
#define  RGN_RRE           0x00400000	
#define  RGN_RBE           0x00800000	
#define  RGN_MBEN          0x01000000	
#define  RGN_RBEN          0x04000000	
#define  RGN_THROT         0x08000000	
#define  RGN_TRD           0xF0000000	

#define PLX_REGION1_REG         0x00f8	

#define PLX_DMRNG_REG          0x001C	

#define PLX_LBAPMEM_REG        0x0020	

#define PLX_LBAPIO_REG         0x0024	

#define PLX_DMMAP_REG          0x0028	
#define  DMM_MAE           0x00000001	
#define  DMM_IAE           0x00000002	
#define  DMM_LCK           0x00000004	
#define  DMM_PF4           0x00000008	
#define  DMM_THROT         0x00000010	
#define  DMM_PAF0          0x00000000	
#define  DMM_PAF1          0x00000020	
#define  DMM_PAF2          0x00000040	
#define  DMM_PAF3          0x00000060	
#define  DMM_PAF4          0x00000080	
#define  DMM_PAF5          0x000000A0	
#define  DMM_PAF6          0x000000C0	
#define  DMM_PAF7          0x000000D0	
#define  DMM_MAP           0xFFFF0000	

#define PLX_CAR_REG            0x002C	
#define  CAR_CT0           0x00000000	
#define  CAR_CT1           0x00000001	
#define  CAR_REG           0x000000FC	
#define  CAR_FUN           0x00000700	
#define  CAR_DEV           0x0000F800	
#define  CAR_BUS           0x00FF0000	
#define  CAR_CFG           0x80000000	

#define PLX_DBR_IN_REG         0x0060	

#define PLX_DBR_OUT_REG        0x0064	

#define PLX_INTRCS_REG         0x0068	
#define  ICS_AERR          0x00000001	
#define  ICS_PERR          0x00000002	
#define  ICS_SERR          0x00000004	
#define  ICS_MBIE          0x00000008	
#define  ICS_PIE           0x00000100	
#define  ICS_PDIE          0x00000200	
#define  ICS_PAIE          0x00000400	
#define  ICS_PLIE          0x00000800	
#define  ICS_RAE           0x00001000	
#define  ICS_PDIA          0x00002000	
#define  ICS_PAIA          0x00004000	
#define  ICS_LIA           0x00008000	
#define  ICS_LIE           0x00010000	
#define  ICS_LDIE          0x00020000	
#define  ICS_DMA0_E        0x00040000	
#define  ICS_DMA1_E        0x00080000	
#define  ICS_LDIA          0x00100000	
#define  ICS_DMA0_A        0x00200000	
#define  ICS_DMA1_A        0x00400000	
#define  ICS_BIA           0x00800000	
#define  ICS_TA_DM         0x01000000	
#define  ICS_TA_DMA0       0x02000000	
#define  ICS_TA_DMA1       0x04000000	
#define  ICS_TA_RA         0x08000000	
#define  ICS_MBIA(x)       (0x10000000 << ((x) & 0x3))	

#define PLX_CONTROL_REG        0x006C	
#define  CTL_RDMA          0x0000000E	
#define  CTL_WDMA          0x00000070	
#define  CTL_RMEM          0x00000600	
#define  CTL_WMEM          0x00007000	
#define  CTL_USERO         0x00010000	
#define  CTL_USERI         0x00020000	
#define  CTL_EE_CLK        0x01000000	
#define  CTL_EE_CS         0x02000000	
#define  CTL_EE_W          0x04000000	
#define  CTL_EE_R          0x08000000	
#define  CTL_EECHK         0x10000000	
#define  CTL_EERLD         0x20000000	
#define  CTL_RESET         0x40000000	
#define  CTL_READY         0x80000000	

#define PLX_ID_REG	0x70	

#define PLX_REVISION_REG	0x74	

#define PLX_DMA0_MODE_REG	0x80	
#define PLX_DMA1_MODE_REG	0x94	
#define  PLX_LOCAL_BUS_16_WIDE_BITS	0x1
#define  PLX_LOCAL_BUS_32_WIDE_BITS	0x3
#define  PLX_LOCAL_BUS_WIDTH_MASK	0x3
#define  PLX_DMA_EN_READYIN_BIT	0x40	
#define  PLX_EN_BTERM_BIT	0x80	
#define  PLX_DMA_LOCAL_BURST_EN_BIT	0x100	
#define  PLX_EN_CHAIN_BIT	0x200	
#define  PLX_EN_DMA_DONE_INTR_BIT	0x400	
#define  PLX_LOCAL_ADDR_CONST_BIT	0x800	
#define  PLX_DEMAND_MODE_BIT	0x1000	
#define  PLX_EOT_ENABLE_BIT	0x4000
#define  PLX_STOP_MODE_BIT 0x8000
#define  PLX_DMA_INTR_PCI_BIT	0x20000	

#define PLX_DMA0_PCI_ADDRESS_REG	0x84	
#define PLX_DMA1_PCI_ADDRESS_REG	0x98

#define PLX_DMA0_LOCAL_ADDRESS_REG	0x88	
#define PLX_DMA1_LOCAL_ADDRESS_REG	0x9c

#define PLX_DMA0_TRANSFER_SIZE_REG	0x8c	
#define PLX_DMA1_TRANSFER_SIZE_REG	0xa0

#define PLX_DMA0_DESCRIPTOR_REG	0x90	
#define PLX_DMA1_DESCRIPTOR_REG	0xa4
#define  PLX_DESC_IN_PCI_BIT	0x1	
#define  PLX_END_OF_CHAIN_BIT	0x2	
#define  PLX_INTR_TERM_COUNT	0x4	
#define  PLX_XFER_LOCAL_TO_PCI 0x8	

#define PLX_DMA0_CS_REG	0xa8	
#define PLX_DMA1_CS_REG	0xa9
#define  PLX_DMA_EN_BIT	0x1	
#define  PLX_DMA_START_BIT	0x2	
#define  PLX_DMA_ABORT_BIT	0x4	
#define  PLX_CLEAR_DMA_INTR_BIT	0x8	
#define  PLX_DMA_DONE_BIT	0x10	

#define PLX_DMA0_THRESHOLD_REG	0xb0	


#define PLX_PREFETCH   32



#define MBX_STS_VALID      0x57584744	
#define MBX_STS_DILAV      0x44475857	


#define MBX_STS_MASK       0x000000ff	
#define MBX_STS_TMASK      0x0000000f	

#define MBX_STS_PCIRESET   0x00000100	
#define MBX_STS_BUSY       0x00000080	
#define MBX_STS_ERROR      0x00000040	
#define MBX_STS_RESERVED   0x000000c0	

#define MBX_RESERVED_5     0x00000020	
#define MBX_RESERVED_4     0x00000010	



#define MBX_CMD_MASK       0xffff0000	

#define MBX_CMD_ABORTJ     0x85000000	
#define MBX_CMD_RESETP     0x86000000	
#define MBX_CMD_PAUSE      0x87000000	
#define MBX_CMD_PAUSEC     0x88000000	
#define MBX_CMD_RESUME     0x89000000	
#define MBX_CMD_STEP       0x8a000000	

#define MBX_CMD_BSWAP      0x8c000000	
#define MBX_CMD_BSWAP_0    0x8c000000	
#define MBX_CMD_BSWAP_1    0x8c000001	

#define MBX_CMD_SETHMS     0x8d000000	
#define MBX_CMD_SETHBA     0x8e000000	
#define MBX_CMD_MGO        0x8f000000	
#define MBX_CMD_NOOP       0xFF000000	


#define MBX_MEMSZ_MASK     0xffff0000	

#define MBX_MEMSZ_128KB    0x00020000	
#define MBX_MEMSZ_256KB    0x00040000	
#define MBX_MEMSZ_512KB    0x00080000	
#define MBX_MEMSZ_1MB      0x00100000	
#define MBX_MEMSZ_2MB      0x00200000	
#define MBX_MEMSZ_4MB      0x00400000	
#define MBX_MEMSZ_8MB      0x00800000	
#define MBX_MEMSZ_16MB     0x01000000	


#define MBX_BTYPE_MASK          0x0000ffff	
#define MBX_BTYPE_FAMILY_MASK   0x0000ff00	
#define MBX_BTYPE_SUBTYPE_MASK  0x000000ff	

#define MBX_BTYPE_PLX9060       0x00000100	
#define MBX_BTYPE_PLX9080       0x00000300	

#define MBX_BTYPE_WANXL_4       0x00000104	
#define MBX_BTYPE_WANXL_2       0x00000102	
#define MBX_BTYPE_WANXL_1s      0x00000301	
#define MBX_BTYPE_WANXL_1t      0x00000401	


#define MBX_SMBX_MASK           0x000000ff	


#define MBX_ERR    0
#define MBX_OK     1

#define MBXCHK_STS      0x00	
#define MBXCHK_NOWAIT   0x01	

#define MBX_ADDR_SPACE_360 0x80	
#define MBX_ADDR_MASK_360 (MBX_ADDR_SPACE_360-1)

static inline int plx9080_abort_dma(void __iomem *iobase, unsigned int channel)
{
	void __iomem *dma_cs_addr;
	uint8_t dma_status;
	const int timeout = 10000;
	unsigned int i;

	if (channel)
		dma_cs_addr = iobase + PLX_DMA1_CS_REG;
	else
		dma_cs_addr = iobase + PLX_DMA0_CS_REG;

	
	dma_status = readb(dma_cs_addr);
	if ((dma_status & PLX_DMA_EN_BIT) == 0)
		return 0;

	
	for (i = 0; (dma_status & PLX_DMA_DONE_BIT) && i < timeout; i++) {
		udelay(1);
		dma_status = readb(dma_cs_addr);
	}
	if (i == timeout) {
		printk
		    ("plx9080: cancel() timed out waiting for dma %i done clear\n",
		     channel);
		return -ETIMEDOUT;
	}
	
	writeb(PLX_DMA_ABORT_BIT, dma_cs_addr);
	
	dma_status = readb(dma_cs_addr);
	for (i = 0; (dma_status & PLX_DMA_DONE_BIT) == 0 && i < timeout; i++) {
		udelay(1);
		dma_status = readb(dma_cs_addr);
	}
	if (i == timeout) {
		printk
		    ("plx9080: cancel() timed out waiting for dma %i done set\n",
		     channel);
		return -ETIMEDOUT;
	}

	return 0;
}

#endif 
