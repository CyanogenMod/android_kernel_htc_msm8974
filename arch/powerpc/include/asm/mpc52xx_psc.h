/*
 * include/asm-ppc/mpc52xx_psc.h
 *
 * Definitions of consts/structs to drive the Freescale MPC52xx OnChip
 * PSCs. Theses are shared between multiple drivers since a PSC can be
 * UART, AC97, IR, I2S, ... So this header is in asm-ppc.
 *
 *
 * Maintainer : Sylvain Munaut <tnt@246tNt.com>
 *
 * Based/Extracted from some header of the 2.4 originally written by
 * Dale Farnsworth <dfarnsworth@mvista.com>
 *
 * Copyright (C) 2004 Sylvain Munaut <tnt@246tNt.com>
 * Copyright (C) 2003 MontaVista, Software, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __ASM_MPC52xx_PSC_H__
#define __ASM_MPC52xx_PSC_H__

#include <asm/types.h>

#ifdef CONFIG_PPC_MPC512x
#define MPC52xx_PSC_MAXNUM     12
#else
#define MPC52xx_PSC_MAXNUM	6
#endif

#define MPC52xx_PSC_SR_UNEX_RX	0x0001
#define MPC52xx_PSC_SR_DATA_VAL	0x0002
#define MPC52xx_PSC_SR_DATA_OVR	0x0004
#define MPC52xx_PSC_SR_CMDSEND	0x0008
#define MPC52xx_PSC_SR_CDE	0x0080
#define MPC52xx_PSC_SR_RXRDY	0x0100
#define MPC52xx_PSC_SR_RXFULL	0x0200
#define MPC52xx_PSC_SR_TXRDY	0x0400
#define MPC52xx_PSC_SR_TXEMP	0x0800
#define MPC52xx_PSC_SR_OE	0x1000
#define MPC52xx_PSC_SR_PE	0x2000
#define MPC52xx_PSC_SR_FE	0x4000
#define MPC52xx_PSC_SR_RB	0x8000

#define MPC52xx_PSC_RX_ENABLE		0x0001
#define MPC52xx_PSC_RX_DISABLE		0x0002
#define MPC52xx_PSC_TX_ENABLE		0x0004
#define MPC52xx_PSC_TX_DISABLE		0x0008
#define MPC52xx_PSC_SEL_MODE_REG_1	0x0010
#define MPC52xx_PSC_RST_RX		0x0020
#define MPC52xx_PSC_RST_TX		0x0030
#define MPC52xx_PSC_RST_ERR_STAT	0x0040
#define MPC52xx_PSC_RST_BRK_CHG_INT	0x0050
#define MPC52xx_PSC_START_BRK		0x0060
#define MPC52xx_PSC_STOP_BRK		0x0070

#define MPC52xx_PSC_RXTX_FIFO_ERR	0x0040
#define MPC52xx_PSC_RXTX_FIFO_UF	0x0020
#define MPC52xx_PSC_RXTX_FIFO_OF	0x0010
#define MPC52xx_PSC_RXTX_FIFO_FR	0x0008
#define MPC52xx_PSC_RXTX_FIFO_FULL	0x0004
#define MPC52xx_PSC_RXTX_FIFO_ALARM	0x0002
#define MPC52xx_PSC_RXTX_FIFO_EMPTY	0x0001

#define MPC52xx_PSC_IMR_UNEX_RX_SLOT 0x0001
#define MPC52xx_PSC_IMR_DATA_VALID	0x0002
#define MPC52xx_PSC_IMR_DATA_OVR	0x0004
#define MPC52xx_PSC_IMR_CMD_SEND	0x0008
#define MPC52xx_PSC_IMR_ERROR		0x0040
#define MPC52xx_PSC_IMR_DEOF		0x0080
#define MPC52xx_PSC_IMR_TXRDY		0x0100
#define MPC52xx_PSC_IMR_RXRDY		0x0200
#define MPC52xx_PSC_IMR_DB		0x0400
#define MPC52xx_PSC_IMR_TXEMP		0x0800
#define MPC52xx_PSC_IMR_ORERR		0x1000
#define MPC52xx_PSC_IMR_IPC		0x8000

#define MPC52xx_PSC_CTS			0x01
#define MPC52xx_PSC_DCD			0x02
#define MPC52xx_PSC_D_CTS		0x10
#define MPC52xx_PSC_D_DCD		0x20

#define MPC52xx_PSC_IEC_CTS		0x01
#define MPC52xx_PSC_IEC_DCD		0x02

#define MPC52xx_PSC_OP_RTS		0x01
#define MPC52xx_PSC_OP_RES		0x02

#define MPC52xx_PSC_MODE_5_BITS			0x00
#define MPC52xx_PSC_MODE_6_BITS			0x01
#define MPC52xx_PSC_MODE_7_BITS			0x02
#define MPC52xx_PSC_MODE_8_BITS			0x03
#define MPC52xx_PSC_MODE_BITS_MASK		0x03
#define MPC52xx_PSC_MODE_PAREVEN		0x00
#define MPC52xx_PSC_MODE_PARODD			0x04
#define MPC52xx_PSC_MODE_PARFORCE		0x08
#define MPC52xx_PSC_MODE_PARNONE		0x10
#define MPC52xx_PSC_MODE_ERR			0x20
#define MPC52xx_PSC_MODE_FFULL			0x40
#define MPC52xx_PSC_MODE_RXRTS			0x80

#define MPC52xx_PSC_MODE_ONE_STOP_5_BITS	0x00
#define MPC52xx_PSC_MODE_ONE_STOP		0x07
#define MPC52xx_PSC_MODE_TWO_STOP		0x0f
#define MPC52xx_PSC_MODE_TXCTS			0x10

#define MPC52xx_PSC_RFNUM_MASK	0x01ff

#define MPC52xx_PSC_SICR_DTS1			(1 << 29)
#define MPC52xx_PSC_SICR_SHDR			(1 << 28)
#define MPC52xx_PSC_SICR_SIM_MASK		(0xf << 24)
#define MPC52xx_PSC_SICR_SIM_UART		(0x0 << 24)
#define MPC52xx_PSC_SICR_SIM_UART_DCD		(0x8 << 24)
#define MPC52xx_PSC_SICR_SIM_CODEC_8		(0x1 << 24)
#define MPC52xx_PSC_SICR_SIM_CODEC_16		(0x2 << 24)
#define MPC52xx_PSC_SICR_SIM_AC97		(0x3 << 24)
#define MPC52xx_PSC_SICR_SIM_SIR		(0x8 << 24)
#define MPC52xx_PSC_SICR_SIM_SIR_DCD		(0xc << 24)
#define MPC52xx_PSC_SICR_SIM_MIR		(0x5 << 24)
#define MPC52xx_PSC_SICR_SIM_FIR		(0x6 << 24)
#define MPC52xx_PSC_SICR_SIM_CODEC_24		(0x7 << 24)
#define MPC52xx_PSC_SICR_SIM_CODEC_32		(0xf << 24)
#define MPC52xx_PSC_SICR_ACRB			(0x8 << 24)
#define MPC52xx_PSC_SICR_AWR			(1 << 30)
#define MPC52xx_PSC_SICR_GENCLK			(1 << 23)
#define MPC52xx_PSC_SICR_I2S			(1 << 22)
#define MPC52xx_PSC_SICR_CLKPOL			(1 << 21)
#define MPC52xx_PSC_SICR_SYNCPOL		(1 << 20)
#define MPC52xx_PSC_SICR_CELLSLAVE		(1 << 19)
#define MPC52xx_PSC_SICR_CELL2XCLK		(1 << 18)
#define MPC52xx_PSC_SICR_ESAI			(1 << 17)
#define MPC52xx_PSC_SICR_ENAC97			(1 << 16)
#define MPC52xx_PSC_SICR_SPI			(1 << 15)
#define MPC52xx_PSC_SICR_MSTR			(1 << 14)
#define MPC52xx_PSC_SICR_CPOL			(1 << 13)
#define MPC52xx_PSC_SICR_CPHA			(1 << 12)
#define MPC52xx_PSC_SICR_USEEOF			(1 << 11)
#define MPC52xx_PSC_SICR_DISABLEEOF		(1 << 10)

struct mpc52xx_psc {
	u8		mode;		
	u8		reserved0[3];
	union {				
		u16	status;
		u16	clock_select;
	} sr_csr;
#define mpc52xx_psc_status	sr_csr.status
#define mpc52xx_psc_clock_select sr_csr.clock_select
	u16		reserved1;
	u8		command;	
	u8		reserved2[3];
	union {				
		u8	buffer_8;
		u16	buffer_16;
		u32	buffer_32;
	} buffer;
#define mpc52xx_psc_buffer_8	buffer.buffer_8
#define mpc52xx_psc_buffer_16	buffer.buffer_16
#define mpc52xx_psc_buffer_32	buffer.buffer_32
	union {				
		u8	ipcr;
		u8	acr;
	} ipcr_acr;
#define mpc52xx_psc_ipcr	ipcr_acr.ipcr
#define mpc52xx_psc_acr		ipcr_acr.acr
	u8		reserved3[3];
	union {				
		u16	isr;
		u16	imr;
	} isr_imr;
#define mpc52xx_psc_isr		isr_imr.isr
#define mpc52xx_psc_imr		isr_imr.imr
	u16		reserved4;
	u8		ctur;		
	u8		reserved5[3];
	u8		ctlr;		
	u8		reserved6[3];
	u32		ccr;		
	u32		ac97_slots;	
	u32		ac97_cmd;	
	u32		ac97_data;	
	u8		ivr;		
	u8		reserved8[3];
	u8		ip;		
	u8		reserved9[3];
	u8		op1;		
	u8		reserved10[3];
	u8		op0;		
	u8		reserved11[3];
	u32		sicr;		
	u8		ircr1;		
	u8		reserved13[3];
	u8		ircr2;		
	u8		reserved14[3];
	u8		irsdr;		
	u8		reserved15[3];
	u8		irmdr;		
	u8		reserved16[3];
	u8		irfdr;		
	u8		reserved17[3];
};

struct mpc52xx_psc_fifo {
	u16		rfnum;		
	u16		reserved18;
	u16		tfnum;		
	u16		reserved19;
	u32		rfdata;		
	u16		rfstat;		
	u16		reserved20;
	u8		rfcntl;		
	u8		reserved21[5];
	u16		rfalarm;	
	u16		reserved22;
	u16		rfrptr;		
	u16		reserved23;
	u16		rfwptr;		
	u16		reserved24;
	u16		rflrfptr;	
	u16		reserved25;
	u16		rflwfptr;	
	u32		tfdata;		
	u16		tfstat;		
	u16		reserved26;
	u8		tfcntl;		
	u8		reserved27[5];
	u16		tfalarm;	
	u16		reserved28;
	u16		tfrptr;		
	u16		reserved29;
	u16		tfwptr;		
	u16		reserved30;
	u16		tflrfptr;	
	u16		reserved31;
	u16		tflwfptr;	
};

#define MPC512x_PSC_FIFO_EOF		0x100
#define MPC512x_PSC_FIFO_RESET_SLICE	0x80
#define MPC512x_PSC_FIFO_ENABLE_SLICE	0x01
#define MPC512x_PSC_FIFO_ENABLE_DMA	0x04

#define MPC512x_PSC_FIFO_EMPTY		0x1
#define MPC512x_PSC_FIFO_FULL		0x2
#define MPC512x_PSC_FIFO_ALARM		0x4
#define MPC512x_PSC_FIFO_URERR		0x8
#define MPC512x_PSC_FIFO_ORERR		0x01
#define MPC512x_PSC_FIFO_MEMERROR	0x02

struct mpc512x_psc_fifo {
	u32		reserved1[10];
	u32		txcmd;		
	u32		txalarm;	
	u32		txsr;		
	u32		txisr;		
	u32		tximr;		
	u32		txcnt;		
	u32		txptr;		
	u32		txsz;		
	u32		reserved2[7];
	union {
		u8	txdata_8;
		u16	txdata_16;
		u32	txdata_32;
	} txdata; 			
#define txdata_8 txdata.txdata_8
#define txdata_16 txdata.txdata_16
#define txdata_32 txdata.txdata_32
	u32		rxcmd;		
	u32		rxalarm;	
	u32		rxsr;		
	u32		rxisr;		
	u32		rximr;		
	u32		rxcnt;		
	u32		rxptr;		
	u32		rxsz;		
	u32		reserved3[7];
	union {
		u8	rxdata_8;
		u16	rxdata_16;
		u32	rxdata_32;
	} rxdata; 			
#define rxdata_8 rxdata.rxdata_8
#define rxdata_16 rxdata.rxdata_16
#define rxdata_32 rxdata.rxdata_32
};

#endif  
