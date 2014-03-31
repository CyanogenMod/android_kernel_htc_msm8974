/*
 * Internal header file for UCC FAST unit routines.
 *
 * Copyright (C) 2006 Freescale Semicondutor, Inc. All rights reserved.
 *
 * Authors: 	Shlomi Gridish <gridish@freescale.com>
 * 		Li Yang <leoli@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __UCC_FAST_H__
#define __UCC_FAST_H__

#include <linux/kernel.h>

#include <asm/immap_qe.h>
#include <asm/qe.h>

#include "ucc.h"

#define R_E	0x80000000	
#define R_W	0x20000000	
#define R_I	0x10000000	
#define R_L	0x08000000	
#define R_F	0x04000000	

#define T_R	0x80000000	
#define T_W	0x20000000	
#define T_I	0x10000000	
#define T_L	0x08000000	

#define UCC_FAST_RX_ALIGN			4
#define UCC_FAST_MRBLR_ALIGNMENT		4
#define UCC_FAST_VIRT_FIFO_REGS_ALIGNMENT	8

#define UCC_FAST_URFS_MIN_VAL				0x88
#define UCC_FAST_RECEIVE_VIRTUAL_FIFO_SIZE_FUDGE_FACTOR	8

enum ucc_fast_channel_protocol_mode {
	UCC_FAST_PROTOCOL_MODE_HDLC = 0x00000000,
	UCC_FAST_PROTOCOL_MODE_RESERVED01 = 0x00000001,
	UCC_FAST_PROTOCOL_MODE_RESERVED_QMC = 0x00000002,
	UCC_FAST_PROTOCOL_MODE_RESERVED02 = 0x00000003,
	UCC_FAST_PROTOCOL_MODE_RESERVED_UART = 0x00000004,
	UCC_FAST_PROTOCOL_MODE_RESERVED03 = 0x00000005,
	UCC_FAST_PROTOCOL_MODE_RESERVED_EX_MAC_1 = 0x00000006,
	UCC_FAST_PROTOCOL_MODE_RESERVED_EX_MAC_2 = 0x00000007,
	UCC_FAST_PROTOCOL_MODE_RESERVED_BISYNC = 0x00000008,
	UCC_FAST_PROTOCOL_MODE_RESERVED04 = 0x00000009,
	UCC_FAST_PROTOCOL_MODE_ATM = 0x0000000A,
	UCC_FAST_PROTOCOL_MODE_RESERVED05 = 0x0000000B,
	UCC_FAST_PROTOCOL_MODE_ETHERNET = 0x0000000C,
	UCC_FAST_PROTOCOL_MODE_RESERVED06 = 0x0000000D,
	UCC_FAST_PROTOCOL_MODE_POS = 0x0000000E,
	UCC_FAST_PROTOCOL_MODE_RESERVED07 = 0x0000000F
};

enum ucc_fast_transparent_txrx {
	UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_NORMAL = 0x00000000,
	UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_TRANSPARENT = 0x18000000
};

enum ucc_fast_diag_mode {
	UCC_FAST_DIAGNOSTIC_NORMAL = 0x0,
	UCC_FAST_DIAGNOSTIC_LOCAL_LOOP_BACK = 0x40000000,
	UCC_FAST_DIAGNOSTIC_AUTO_ECHO = 0x80000000,
	UCC_FAST_DIAGNOSTIC_LOOP_BACK_AND_ECHO = 0xC0000000
};

enum ucc_fast_sync_len {
	UCC_FAST_SYNC_LEN_NOT_USED = 0x0,
	UCC_FAST_SYNC_LEN_AUTOMATIC = 0x00004000,
	UCC_FAST_SYNC_LEN_8_BIT = 0x00008000,
	UCC_FAST_SYNC_LEN_16_BIT = 0x0000C000
};

enum ucc_fast_ready_to_send {
	UCC_FAST_SEND_IDLES_BETWEEN_FRAMES = 0x00000000,
	UCC_FAST_SEND_FLAGS_BETWEEN_FRAMES = 0x00002000
};

enum ucc_fast_rx_decoding_method {
	UCC_FAST_RX_ENCODING_NRZ = 0x00000000,
	UCC_FAST_RX_ENCODING_NRZI = 0x00000800,
	UCC_FAST_RX_ENCODING_RESERVED0 = 0x00001000,
	UCC_FAST_RX_ENCODING_RESERVED1 = 0x00001800
};

enum ucc_fast_tx_encoding_method {
	UCC_FAST_TX_ENCODING_NRZ = 0x00000000,
	UCC_FAST_TX_ENCODING_NRZI = 0x00000100,
	UCC_FAST_TX_ENCODING_RESERVED0 = 0x00000200,
	UCC_FAST_TX_ENCODING_RESERVED1 = 0x00000300
};

enum ucc_fast_transparent_tcrc {
	UCC_FAST_16_BIT_CRC = 0x00000000,
	UCC_FAST_CRC_RESERVED0 = 0x00000040,
	UCC_FAST_32_BIT_CRC = 0x00000080,
	UCC_FAST_CRC_RESERVED1 = 0x000000C0
};

struct ucc_fast_info {
	int ucc_num;
	enum qe_clock rx_clock;
	enum qe_clock tx_clock;
	u32 regs;
	int irq;
	u32 uccm_mask;
	int bd_mem_part;
	int brkpt_support;
	int grant_support;
	int tsa;
	int cdp;
	int cds;
	int ctsp;
	int ctss;
	int tci;
	int txsy;
	int rtsm;
	int revd;
	int rsyn;
	u16 max_rx_buf_length;
	u16 urfs;
	u16 urfet;
	u16 urfset;
	u16 utfs;
	u16 utfet;
	u16 utftt;
	u16 ufpt;
	enum ucc_fast_channel_protocol_mode mode;
	enum ucc_fast_transparent_txrx ttx_trx;
	enum ucc_fast_tx_encoding_method tenc;
	enum ucc_fast_rx_decoding_method renc;
	enum ucc_fast_transparent_tcrc tcrc;
	enum ucc_fast_sync_len synl;
};

struct ucc_fast_private {
	struct ucc_fast_info *uf_info;
	struct ucc_fast __iomem *uf_regs; 
	u32 __iomem *p_ucce;	
	u32 __iomem *p_uccm;	
#ifdef CONFIG_UGETH_TX_ON_DEMAND
	u16 __iomem *p_utodr;	
#endif
	int enabled_tx;		
	int enabled_rx;		
	int stopped_tx;		
	int stopped_rx;		
	u32 ucc_fast_tx_virtual_fifo_base_offset;
	u32 ucc_fast_rx_virtual_fifo_base_offset;
#ifdef STATISTICS
	u32 tx_frames;		
	u32 rx_frames;		
	u32 tx_discarded;	
	u32 rx_discarded;	
#endif				
	u16 mrblr;		
};

int ucc_fast_init(struct ucc_fast_info * uf_info, struct ucc_fast_private ** uccf_ret);

void ucc_fast_free(struct ucc_fast_private * uccf);

void ucc_fast_enable(struct ucc_fast_private * uccf, enum comm_dir mode);

void ucc_fast_disable(struct ucc_fast_private * uccf, enum comm_dir mode);

void ucc_fast_irq(struct ucc_fast_private * uccf);

void ucc_fast_transmit_on_demand(struct ucc_fast_private * uccf);

u32 ucc_fast_get_qe_cr_subblock(int uccf_num);

void ucc_fast_dump_regs(struct ucc_fast_private * uccf);

#endif				
