/*
 * Copyright (C) 2006 Freescale Semicondutor, Inc. All rights reserved.
 *
 * Authors: 	Shlomi Gridish <gridish@freescale.com>
 * 		Li Yang <leoli@freescale.com>
 *
 * Description:
 * Internal header file for UCC SLOW unit routines.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __UCC_SLOW_H__
#define __UCC_SLOW_H__

#include <linux/kernel.h>

#include <asm/immap_qe.h>
#include <asm/qe.h>

#include "ucc.h"

#define T_R	0x80000000	
#define T_PAD	0x40000000	
#define T_W	0x20000000	
#define T_I	0x10000000	
#define T_L	0x08000000	

#define T_A	0x04000000	
#define T_TC	0x04000000	
#define T_CM	0x02000000	
#define T_DEF	0x02000000	
#define T_P	0x01000000	
#define T_HB	0x01000000	
#define T_NS	0x00800000	
#define T_LC	0x00800000	
#define T_RL	0x00400000	
#define T_UN	0x00020000	
#define T_CT	0x00010000	
#define T_CSL	0x00010000	
#define T_RC	0x003c0000	

#define R_E	0x80000000	
#define R_W	0x20000000	
#define R_I	0x10000000	
#define R_L	0x08000000	
#define R_C	0x08000000	
#define R_F	0x04000000	
#define R_A	0x04000000	
#define R_CM	0x02000000	
#define R_ID	0x01000000	
#define R_M	0x01000000	
#define R_AM	0x00800000	
#define R_DE	0x00800000	
#define R_LG	0x00200000	
#define R_BR	0x00200000	
#define R_NO	0x00100000	
#define R_FR	0x00100000	
#define R_PR	0x00080000	
#define R_AB	0x00080000	
#define R_SH	0x00080000	
#define R_CR	0x00040000	
#define R_OV	0x00020000	
#define R_CD	0x00010000	
#define R_CL	0x00010000	

#define UCC_SLOW_RX_ALIGN		4
#define UCC_SLOW_MRBLR_ALIGNMENT	4
#define UCC_SLOW_PRAM_SIZE		0x100
#define ALIGNMENT_OF_UCC_SLOW_PRAM	64

enum ucc_slow_channel_protocol_mode {
	UCC_SLOW_CHANNEL_PROTOCOL_MODE_QMC = 0x00000002,
	UCC_SLOW_CHANNEL_PROTOCOL_MODE_UART = 0x00000004,
	UCC_SLOW_CHANNEL_PROTOCOL_MODE_BISYNC = 0x00000008,
};

enum ucc_slow_transparent_tcrc {
	
	UCC_SLOW_TRANSPARENT_TCRC_CCITT_CRC16 = 0x00000000,
	
	UCC_SLOW_TRANSPARENT_TCRC_CRC16 = 0x00004000,
	
	UCC_SLOW_TRANSPARENT_TCRC_CCITT_CRC32 = 0x00008000,
};

enum ucc_slow_tx_oversampling_rate {
	
	UCC_SLOW_OVERSAMPLING_RATE_TX_TDCR_1 = 0x00000000,
	
	UCC_SLOW_OVERSAMPLING_RATE_TX_TDCR_8 = 0x00010000,
	
	UCC_SLOW_OVERSAMPLING_RATE_TX_TDCR_16 = 0x00020000,
	
	UCC_SLOW_OVERSAMPLING_RATE_TX_TDCR_32 = 0x00030000,
};

enum ucc_slow_rx_oversampling_rate {
	
	UCC_SLOW_OVERSAMPLING_RATE_RX_RDCR_1 = 0x00000000,
	
	UCC_SLOW_OVERSAMPLING_RATE_RX_RDCR_8 = 0x00004000,
	
	UCC_SLOW_OVERSAMPLING_RATE_RX_RDCR_16 = 0x00008000,
	
	UCC_SLOW_OVERSAMPLING_RATE_RX_RDCR_32 = 0x0000c000,
};

enum ucc_slow_tx_encoding_method {
	UCC_SLOW_TRANSMITTER_ENCODING_METHOD_TENC_NRZ = 0x00000000,
	UCC_SLOW_TRANSMITTER_ENCODING_METHOD_TENC_NRZI = 0x00000100
};

enum ucc_slow_rx_decoding_method {
	UCC_SLOW_RECEIVER_DECODING_METHOD_RENC_NRZ = 0x00000000,
	UCC_SLOW_RECEIVER_DECODING_METHOD_RENC_NRZI = 0x00000800
};

enum ucc_slow_diag_mode {
	UCC_SLOW_DIAG_MODE_NORMAL = 0x00000000,
	UCC_SLOW_DIAG_MODE_LOOPBACK = 0x00000040,
	UCC_SLOW_DIAG_MODE_ECHO = 0x00000080,
	UCC_SLOW_DIAG_MODE_LOOPBACK_ECHO = 0x000000c0
};

struct ucc_slow_info {
	int ucc_num;
	int protocol;			
	enum qe_clock rx_clock;
	enum qe_clock tx_clock;
	phys_addr_t regs;
	int irq;
	u16 uccm_mask;
	int data_mem_part;
	int init_tx;
	int init_rx;
	u32 tx_bd_ring_len;
	u32 rx_bd_ring_len;
	int rx_interrupts;
	int brkpt_support;
	int grant_support;
	int tsa;
	int cdp;
	int cds;
	int ctsp;
	int ctss;
	int rinv;
	int tinv;
	int rtsm;
	int rfw;
	int tci;
	int tend;
	int tfl;
	int txsy;
	u16 max_rx_buf_length;
	enum ucc_slow_transparent_tcrc tcrc;
	enum ucc_slow_channel_protocol_mode mode;
	enum ucc_slow_diag_mode diag;
	enum ucc_slow_tx_oversampling_rate tdcr;
	enum ucc_slow_rx_oversampling_rate rdcr;
	enum ucc_slow_tx_encoding_method tenc;
	enum ucc_slow_rx_decoding_method renc;
};

struct ucc_slow_private {
	struct ucc_slow_info *us_info;
	struct ucc_slow __iomem *us_regs; 
	struct ucc_slow_pram *us_pram;	
	u32 us_pram_offset;
	int enabled_tx;		
	int enabled_rx;		
	int stopped_tx;		
	int stopped_rx;		
	struct list_head confQ;	
	u32 first_tx_bd_mask;	
	u32 tx_base_offset;	
	u32 rx_base_offset;	
	struct qe_bd *confBd;	
	struct qe_bd *tx_bd;	
	struct qe_bd *rx_bd;	
	void *p_rx_frame;	
	u16 *p_ucce;		
	u16 *p_uccm;		
	u16 saved_uccm;		
#ifdef STATISTICS
	u32 tx_frames;		
	u32 rx_frames;		
	u32 rx_discarded;	
#endif				
};

int ucc_slow_init(struct ucc_slow_info * us_info, struct ucc_slow_private ** uccs_ret);

void ucc_slow_free(struct ucc_slow_private * uccs);

void ucc_slow_enable(struct ucc_slow_private * uccs, enum comm_dir mode);

void ucc_slow_disable(struct ucc_slow_private * uccs, enum comm_dir mode);

void ucc_slow_poll_transmitter_now(struct ucc_slow_private * uccs);

void ucc_slow_graceful_stop_tx(struct ucc_slow_private * uccs);

void ucc_slow_stop_tx(struct ucc_slow_private * uccs);

void ucc_slow_restart_tx(struct ucc_slow_private *uccs);

u32 ucc_slow_get_qe_cr_subblock(int uccs_num);

#endif				
