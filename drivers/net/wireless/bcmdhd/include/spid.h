/*
 * SPI device spec header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: spid.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef	_SPI_H
#define	_SPI_H


typedef volatile struct {
	uint8	config;			
	uint8	response_delay;		
	uint8	status_enable;		
	uint8	reset_bp;		
	uint16	intr_reg;		
	uint16	intr_en_reg;		
	uint32	status_reg;		
	uint16	f1_info_reg;		
	uint16	f2_info_reg;		
	uint16	f3_info_reg;		
	uint32	test_read;		
	uint32	test_rw;		
	uint8	resp_delay_f0;		
	uint8	resp_delay_f1;		
	uint8	resp_delay_f2;		
	uint8	resp_delay_f3;		
} spi_regs_t;

#define SPID_CONFIG			0x00
#define SPID_RESPONSE_DELAY		0x01
#define SPID_STATUS_ENABLE		0x02
#define SPID_RESET_BP			0x03	
#define SPID_INTR_REG			0x04	
#define SPID_INTR_EN_REG		0x06	
#define SPID_STATUS_REG			0x08	
#define SPID_F1_INFO_REG		0x0C	
#define SPID_F2_INFO_REG		0x0E	
#define SPID_F3_INFO_REG		0x10	
#define SPID_TEST_READ			0x14	
#define SPID_TEST_RW			0x18	
#define SPID_RESP_DELAY_F0		0x1c	
#define SPID_RESP_DELAY_F1		0x1d	
#define SPID_RESP_DELAY_F2		0x1e	
#define SPID_RESP_DELAY_F3		0x1f	

#define WORD_LENGTH_32	0x1	
#define ENDIAN_BIG	0x2	
#define CLOCK_PHASE	0x4	
#define CLOCK_POLARITY	0x8	
#define HIGH_SPEED_MODE	0x10	
#define INTR_POLARITY	0x20	
#define WAKE_UP		0x80	

#define RESPONSE_DELAY_MASK	0xFF	

#define STATUS_ENABLE		0x1	
#define INTR_WITH_STATUS	0x2	
#define RESP_DELAY_ALL		0x4	
#define DWORD_PKT_LEN_EN	0x8	
#define CMD_ERR_CHK_EN		0x20	
#define DATA_ERR_CHK_EN		0x40	

#define RESET_ON_WLAN_BP_RESET	0x4	
#define RESET_ON_BT_BP_RESET	0x8	
#define RESET_SPI		0x80	

#define DATA_UNAVAILABLE	0x0001	
#define F2_F3_FIFO_RD_UNDERFLOW	0x0002
#define F2_F3_FIFO_WR_OVERFLOW	0x0004
#define COMMAND_ERROR		0x0008	
#define DATA_ERROR		0x0010	
#define F2_PACKET_AVAILABLE	0x0020
#define F3_PACKET_AVAILABLE	0x0040
#define F1_OVERFLOW		0x0080	
#define MISC_INTR0		0x0100
#define MISC_INTR1		0x0200
#define MISC_INTR2		0x0400
#define MISC_INTR3		0x0800
#define MISC_INTR4		0x1000
#define F1_INTR			0x2000
#define F2_INTR			0x4000
#define F3_INTR			0x8000

#define STATUS_DATA_NOT_AVAILABLE	0x00000001
#define STATUS_UNDERFLOW		0x00000002
#define STATUS_OVERFLOW			0x00000004
#define STATUS_F2_INTR			0x00000008
#define STATUS_F3_INTR			0x00000010
#define STATUS_F2_RX_READY		0x00000020
#define STATUS_F3_RX_READY		0x00000040
#define STATUS_HOST_CMD_DATA_ERR	0x00000080
#define STATUS_F2_PKT_AVAILABLE		0x00000100
#define STATUS_F2_PKT_LEN_MASK		0x000FFE00
#define STATUS_F2_PKT_LEN_SHIFT		9
#define STATUS_F3_PKT_AVAILABLE		0x00100000
#define STATUS_F3_PKT_LEN_MASK		0xFFE00000
#define STATUS_F3_PKT_LEN_SHIFT		21

#define F1_ENABLED 			0x0001
#define F1_RDY_FOR_DATA_TRANSFER	0x0002
#define F1_MAX_PKT_SIZE			0x01FC

#define F2_ENABLED 			0x0001
#define F2_RDY_FOR_DATA_TRANSFER	0x0002
#define F2_MAX_PKT_SIZE			0x3FFC

#define F3_ENABLED 			0x0001
#define F3_RDY_FOR_DATA_TRANSFER	0x0002
#define F3_MAX_PKT_SIZE			0x3FFC

#define TEST_RO_DATA_32BIT_LE		0xFEEDBEAD

#define SPI_MAX_IOFUNCS		4

#define SPI_MAX_PKT_LEN		(2048*4)

#define SPI_FUNC_0		0
#define SPI_FUNC_1		1
#define SPI_FUNC_2		2
#define SPI_FUNC_3		3

#define WAIT_F2RXFIFORDY	100
#define WAIT_F2RXFIFORDY_DELAY	20

#endif 
