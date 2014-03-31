/*
 * Tehuti Networks(R) Network Driver
 * Copyright (C) 2007 Tehuti Networks Ltd. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _TEHUTI_H
#define _TEHUTI_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/uaccess.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <asm/byteorder.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#define BDX_TSO
#define BDX_LLTX
#define BDX_DELAY_WPTR

#if !defined CONFIG_PCI_MSI
#   undef BDX_MSI
#endif

#define BDX_DEF_MSG_ENABLE	(NETIF_MSG_DRV          | \
				NETIF_MSG_PROBE        | \
				NETIF_MSG_LINK)

#define BDX_OP_READ  1
#define BDX_OP_WRITE 2

#define BDX_COPYBREAK    257

#define DRIVER_AUTHOR     "Tehuti Networks(R)"
#define BDX_DRV_DESC      "Tehuti Networks(R) Network Driver"
#define BDX_DRV_NAME      "tehuti"
#define BDX_NIC_NAME      "Tehuti 10 Giga TOE SmartNIC"
#define BDX_NIC2PORT_NAME "Tehuti 2-Port 10 Giga TOE SmartNIC"
#define BDX_DRV_VERSION   "7.29.3"

#ifdef BDX_MSI
#    define BDX_MSI_STRING "msi "
#else
#    define BDX_MSI_STRING ""
#endif

#define BDX_NDEV_TXQ_LEN 3000

#define FIFO_SIZE  4096
#define FIFO_EXTRA_SPACE            1024

#if BITS_PER_LONG == 64
#    define H32_64(x)  (u32) ((u64)(x) >> 32)
#    define L32_64(x)  (u32) ((u64)(x) & 0xffffffff)
#elif BITS_PER_LONG == 32
#    define H32_64(x)  0
#    define L32_64(x)  ((u32) (x))
#else				
#    error BITS_PER_LONG is undefined. Must be 64 or 32
#endif				

#ifdef __BIG_ENDIAN
#   define CPU_CHIP_SWAP32(x) swab32(x)
#   define CPU_CHIP_SWAP16(x) swab16(x)
#else
#   define CPU_CHIP_SWAP32(x) (x)
#   define CPU_CHIP_SWAP16(x) (x)
#endif

#define READ_REG(pp, reg)         readl(pp->pBdxRegs + reg)
#define WRITE_REG(pp, reg, val)   writel(val, pp->pBdxRegs + reg)

#ifndef NET_IP_ALIGN
#   define NET_IP_ALIGN 2
#endif

#ifndef NETDEV_TX_OK
#   define NETDEV_TX_OK 0
#endif

#define LUXOR_MAX_PORT     2
#define BDX_MAX_RX_DONE    150
#define BDX_TXF_DESC_SZ    16
#define BDX_MAX_TX_LEVEL   (priv->txd_fifo0.m.memsz - 16)
#define BDX_MIN_TX_LEVEL   256
#define BDX_NO_UPD_PACKETS 40

struct pci_nic {
	int port_num;
	void __iomem *regs;
	int irq_type;
	struct bdx_priv *priv[LUXOR_MAX_PORT];
};

enum { IRQ_INTX, IRQ_MSI, IRQ_MSIX };

#define PCK_TH_MULT   128
#define INT_COAL_MULT 2

#define BITS_MASK(nbits)			((1<<nbits)-1)
#define GET_BITS_SHIFT(x, nbits, nshift)	(((x)>>nshift)&BITS_MASK(nbits))
#define BITS_SHIFT_MASK(nbits, nshift)		(BITS_MASK(nbits)<<nshift)
#define BITS_SHIFT_VAL(x, nbits, nshift)	(((x)&BITS_MASK(nbits))<<nshift)
#define BITS_SHIFT_CLEAR(x, nbits, nshift)	\
	((x)&(~BITS_SHIFT_MASK(nbits, nshift)))

#define GET_INT_COAL(x)				GET_BITS_SHIFT(x, 15, 0)
#define GET_INT_COAL_RC(x)			GET_BITS_SHIFT(x, 1, 15)
#define GET_RXF_TH(x)				GET_BITS_SHIFT(x, 4, 16)
#define GET_PCK_TH(x)				GET_BITS_SHIFT(x, 4, 20)

#define INT_REG_VAL(coal, coal_rc, rxf_th, pck_th)	\
	((coal)|((coal_rc)<<15)|((rxf_th)<<16)|((pck_th)<<20))

struct fifo {
	dma_addr_t da;		
	char *va;		
	u32 rptr, wptr;		
	u16 reg_CFG0, reg_CFG1;
	u16 reg_RPTR, reg_WPTR;
	u16 memsz;		
	u16 size_mask;
	u16 pktsz;		
	u16 rcvno;		
};

struct txf_fifo {
	struct fifo m;		
};

struct txd_fifo {
	struct fifo m;		
};

struct rxf_fifo {
	struct fifo m;		
};

struct rxd_fifo {
	struct fifo m;		
};

struct rx_map {
	u64 dma;
	struct sk_buff *skb;
};

struct rxdb {
	int *stack;
	struct rx_map *elems;
	int nelem;
	int top;
};

union bdx_dma_addr {
	dma_addr_t dma;
	struct sk_buff *skb;
};

struct tx_map {
	union bdx_dma_addr addr;
	int len;
};

struct txdb {
	struct tx_map *start;	
	struct tx_map *end;	
	struct tx_map *rptr;	
	struct tx_map *wptr;	
	int size;		
};

struct bdx_stats {
	u64 InUCast;			
	u64 InMCast;			
	u64 InBCast;			
	u64 InPkts;			
	u64 InErrors;			
	u64 InDropped;			
	u64 FrameTooLong;		
	u64 FrameSequenceErrors;	
	u64 InVLAN;			
	u64 InDroppedDFE;		
	u64 InDroppedIntFull;		
	u64 InFrameAlignErrors;		

	

	u64 OutUCast;			
	u64 OutMCast;			
	u64 OutBCast;			
	u64 OutPkts;			

	

	u64 OutVLAN;			
	u64 InUCastOctects;		
	u64 OutUCastOctects;		

	

	u64 InBCastOctects;		
	u64 OutBCastOctects;		
	u64 InOctects;			
	u64 OutOctects;			
};

struct bdx_priv {
	void __iomem *pBdxRegs;
	struct net_device *ndev;

	struct napi_struct napi;

	
	struct rxd_fifo rxd_fifo0;
	struct rxf_fifo rxf_fifo0;
	struct rxdb *rxdb;	
	int napi_stop;

	
	struct txd_fifo txd_fifo0;
	struct txf_fifo txf_fifo0;

	struct txdb txdb;
	int tx_level;
#ifdef BDX_DELAY_WPTR
	int tx_update_mark;
	int tx_noupd;
#endif
	spinlock_t tx_lock;	

	
	u8 port;
	u32 msg_enable;
	int stats_flag;
	struct bdx_stats hw_stats;
	struct pci_dev *pdev;

	struct pci_nic *nic;

	u8 txd_size;
	u8 txf_size;
	u8 rxd_size;
	u8 rxf_size;
	u32 rdintcm;
	u32 tdintcm;
};

struct rxf_desc {
	u32 info;		
	u32 va_lo;		
	u32 va_hi;		
	u32 pa_lo;		
	u32 pa_hi;		
	u32 len;		
};

#define GET_RXD_BC(x)			GET_BITS_SHIFT((x), 5, 0)
#define GET_RXD_RXFQ(x)			GET_BITS_SHIFT((x), 2, 8)
#define GET_RXD_TO(x)			GET_BITS_SHIFT((x), 1, 15)
#define GET_RXD_TYPE(x)			GET_BITS_SHIFT((x), 4, 16)
#define GET_RXD_ERR(x)			GET_BITS_SHIFT((x), 6, 21)
#define GET_RXD_RXP(x)			GET_BITS_SHIFT((x), 1, 27)
#define GET_RXD_PKT_ID(x)		GET_BITS_SHIFT((x), 3, 28)
#define GET_RXD_VTAG(x)			GET_BITS_SHIFT((x), 1, 31)
#define GET_RXD_VLAN_ID(x)		GET_BITS_SHIFT((x), 12, 0)
#define GET_RXD_VLAN_TCI(x)		GET_BITS_SHIFT((x), 16, 0)
#define GET_RXD_CFI(x)			GET_BITS_SHIFT((x), 1, 12)
#define GET_RXD_PRIO(x)			GET_BITS_SHIFT((x), 3, 13)

struct rxd_desc {
	u32 rxd_val1;
	u16 len;
	u16 rxd_vlan;
	u32 va_lo;
	u32 va_hi;
};

struct pbl {
	u32 pa_lo;
	u32 pa_hi;
	u32 len;
};

#define TXD_W1_VAL(bc, checksum, vtag, lgsnd, vlan_id)	\
	((bc) | ((checksum)<<5) | ((vtag)<<8) | \
	((lgsnd)<<9) | (0x30000) | ((vlan_id)<<20))

struct txd_desc {
	u32 txd_val1;
	u16 mss;
	u16 length;
	u32 va_lo;
	u32 va_hi;
	struct pbl pbl[0];	
} __packed;

#define BDX_REGS_SIZE	  0x1000

#define regTXD_CFG1_0   0x4000
#define regRXF_CFG1_0   0x4010
#define regRXD_CFG1_0   0x4020
#define regTXF_CFG1_0   0x4030
#define regTXD_CFG0_0   0x4040
#define regRXF_CFG0_0   0x4050
#define regRXD_CFG0_0   0x4060
#define regTXF_CFG0_0   0x4070
#define regTXD_WPTR_0   0x4080
#define regRXF_WPTR_0   0x4090
#define regRXD_WPTR_0   0x40A0
#define regTXF_WPTR_0   0x40B0
#define regTXD_RPTR_0   0x40C0
#define regRXF_RPTR_0   0x40D0
#define regRXD_RPTR_0   0x40E0
#define regTXF_RPTR_0   0x40F0
#define regTXF_RPTR_3   0x40FC

#define  FW_VER         0x5010
#define  SROM_VER       0x5020
#define  FPGA_VER       0x5030
#define  FPGA_SEED      0x5040

#define regISR regISR0
#define regISR0          0x5100

#define regIMR regIMR0
#define regIMR0          0x5110

#define regRDINTCM0      0x5120
#define regRDINTCM2      0x5128

#define regTDINTCM0      0x5130

#define regISR_MSK0      0x5140

#define regINIT_SEMAPHORE 0x5170
#define regINIT_STATUS    0x5180

#define regMAC_LNK_STAT  0x0200
#define MAC_LINK_STAT    0x4	

#define regGMAC_RXF_A   0x1240

#define regUNC_MAC0_A   0x1250
#define regUNC_MAC1_A   0x1260
#define regUNC_MAC2_A   0x1270

#define regVLAN_0       0x1800

#define regMAX_FRAME_A  0x12C0

#define regRX_MAC_MCST0    0x1A80
#define regRX_MAC_MCST1    0x1A84
#define MAC_MCST_NUM       15
#define regRX_MCST_HASH0   0x1A00
#define MAC_MCST_HASH_NUM  8

#define regVPC                  0x2300
#define regVIC                  0x2320
#define regVGLB                 0x2340

#define regCLKPLL               0x5000

#define regREVISION        0x6000
#define regSCRATCH         0x6004
#define regCTRLST          0x6008
#define regMAC_ADDR_0      0x600C
#define regMAC_ADDR_1      0x6010
#define regFRM_LENGTH      0x6014
#define regPAUSE_QUANT     0x6018
#define regRX_FIFO_SECTION 0x601C
#define regTX_FIFO_SECTION 0x6020
#define regRX_FULLNESS     0x6024
#define regTX_FULLNESS     0x6028
#define regHASHTABLE       0x602C
#define regMDIO_ST         0x6030
#define regMDIO_CTL        0x6034
#define regMDIO_DATA       0x6038
#define regMDIO_ADDR       0x603C

#define regRST_PORT        0x7000
#define regDIS_PORT        0x7010
#define regRST_QU          0x7020
#define regDIS_QU          0x7030

#define regCTRLST_TX_ENA   0x0001
#define regCTRLST_RX_ENA   0x0002
#define regCTRLST_PRM_ENA  0x0010
#define regCTRLST_PAD_ENA  0x0020

#define regCTRLST_BASE     (regCTRLST_PAD_ENA|regCTRLST_PRM_ENA)

#define regRX_FLT   0x1400

#define  TX_RX_CFG1_BASE          0xffffffff	
#define  TX_RX_CFG0_BASE          0xfffff000	
#define  TX_RX_CFG0_RSVD          0x0ffc	
#define  TX_RX_CFG0_SIZE          0x0003	

#define  TXF_WPTR_WR_PTR        0x7ff8	

#define  TXF_RPTR_RD_PTR        0x7ff8	

#define TXF_WPTR_MASK 0x7ff0	

#define  IMR_INPROG   0x80000000	
#define  IR_LNKCHG1   0x10000000	
#define  IR_LNKCHG0   0x08000000	
#define  IR_GPIO      0x04000000	
#define  IR_RFRSH     0x02000000	
#define  IR_RSVD      0x01000000	
#define  IR_SWI       0x00800000	
#define  IR_RX_FREE_3 0x00400000	
#define  IR_RX_FREE_2 0x00200000	
#define  IR_RX_FREE_1 0x00100000	
#define  IR_RX_FREE_0 0x00080000	
#define  IR_TX_FREE_3 0x00040000	
#define  IR_TX_FREE_2 0x00020000	
#define  IR_TX_FREE_1 0x00010000	
#define  IR_TX_FREE_0 0x00008000	
#define  IR_RX_DESC_3 0x00004000	
#define  IR_RX_DESC_2 0x00002000	
#define  IR_RX_DESC_1 0x00001000	
#define  IR_RX_DESC_0 0x00000800	
#define  IR_PSE       0x00000400	
#define  IR_TMR3      0x00000200	
#define  IR_TMR2      0x00000100	
#define  IR_TMR1      0x00000080	
#define  IR_TMR0      0x00000040	
#define  IR_VNT       0x00000020	
#define  IR_RxFL      0x00000010	
#define  IR_SDPERR    0x00000008	
#define  IR_TR        0x00000004	
#define  IR_PCIE_LINK 0x00000002	
#define  IR_PCIE_TOUT 0x00000001	

#define  IR_EXTRA (IR_RX_FREE_0 | IR_LNKCHG0 | IR_PSE | \
    IR_TMR0 | IR_PCIE_LINK | IR_PCIE_TOUT)
#define  IR_RUN (IR_EXTRA | IR_RX_DESC_0 | IR_TX_FREE_0)
#define  IR_ALL 0xfdfffff7

#define  IR_LNKCHG0_ofst        27

#define  GMAC_RX_FILTER_OSEN  0x1000	
#define  GMAC_RX_FILTER_TXFC  0x0400	
#define  GMAC_RX_FILTER_RSV0  0x0200	
#define  GMAC_RX_FILTER_FDA   0x0100	
#define  GMAC_RX_FILTER_AOF   0x0080	
#define  GMAC_RX_FILTER_ACF   0x0040	
#define  GMAC_RX_FILTER_ARUNT 0x0020	
#define  GMAC_RX_FILTER_ACRC  0x0010	
#define  GMAC_RX_FILTER_AM    0x0008	
#define  GMAC_RX_FILTER_AB    0x0004	
#define  GMAC_RX_FILTER_PRM   0x0001	

#define  MAX_FRAME_AB_VAL       0x3fff	

#define  CLKPLL_PLLLKD          0x0200	
#define  CLKPLL_RSTEND          0x0100	
#define  CLKPLL_SFTRST          0x0001	

#define  CLKPLL_LKD             (CLKPLL_PLLLKD|CLKPLL_RSTEND)

#define PCI_DEV_CTRL_REG 0x88
#define GET_DEV_CTRL_MAXPL(x)           GET_BITS_SHIFT(x, 3, 5)
#define GET_DEV_CTRL_MRRS(x)            GET_BITS_SHIFT(x, 3, 12)

#define PCI_LINK_STATUS_REG 0x92
#define GET_LINK_STATUS_LANES(x)		GET_BITS_SHIFT(x, 6, 4)


#define DBG2(fmt, args...)					\
	pr_err("%s:%-5d: " fmt, __func__, __LINE__, ## args)

#define BDX_ASSERT(x) BUG_ON(x)

#ifdef DEBUG

#define ENTER						\
do {							\
	pr_err("%s:%-5d: ENTER\n", __func__, __LINE__); \
} while (0)

#define RET(args...)					 \
do {							 \
	pr_err("%s:%-5d: RETURN\n", __func__, __LINE__); \
	return args;					 \
} while (0)

#define DBG(fmt, args...)					\
	pr_err("%s:%-5d: " fmt, __func__, __LINE__, ## args)
#else
#define ENTER do {  } while (0)
#define RET(args...)   return args
#define DBG(fmt, args...)			\
do {						\
	if (0)					\
		pr_err(fmt, ##args);		\
} while (0)
#endif

#endif 
