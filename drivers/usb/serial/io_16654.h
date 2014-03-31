/************************************************************************
 *
 *	16654.H		Definitions for 16C654 UART used on EdgePorts
 *
 *	Copyright (C) 1998 Inside Out Networks, Inc.
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 ************************************************************************/

#if !defined(_16654_H)
#define	_16654_H


	
	
	
	
	
	
	
	
	


#define THR			0	
#define RDR			0	
#define IER			1	
#define FCR			2	
#define ISR			2	
#define LCR			3	
#define MCR			4	
#define LSR			5	
#define MSR			6	
#define SPR			7	
#define DLL			8	
#define DLM			9	
#define EFR			10	
#define XON1			12	
#define XON2			13	
#define XOFF1			14	
#define XOFF2			15	

#define	NUM_16654_REGS		16

#define IS_REG_2ND_BANK(x)	((x) >= 8)

	
	
	

#define IER_RX			0x01	
#define IER_TX			0x02	
#define IER_RXS			0x04	
#define IER_MDM			0x08	
#define IER_SLEEP		0x10	
#define IER_XOFF		0x20	
#define IER_RTS			0x40	
#define IER_CTS			0x80	
#define IER_ENABLE_ALL		0xFF	


#define FCR_FIFO_EN		0x01	
#define FCR_RXCLR		0x02	
#define FCR_TXCLR		0x04	
#define FCR_DMA_BLK		0x08	
#define FCR_TX_LEVEL_MASK	0x30	
#define FCR_TX_LEVEL_8		0x00	
#define FCR_TX_LEVEL_16		0x10	
#define FCR_TX_LEVEL_32		0x20	
#define FCR_TX_LEVEL_56		0x30	
#define FCR_RX_LEVEL_MASK	0xC0	
#define FCR_RX_LEVEL_8		0x00	
#define FCR_RX_LEVEL_16		0x40	
#define FCR_RX_LEVEL_56		0x80	
#define FCR_RX_LEVEL_60		0xC0	


#define ISR_INT_MDM_STATUS	0x00	
#define ISR_INT_NONE		0x01	
#define ISR_INT_TXRDY		0x02	
#define ISR_INT_RXRDY		0x04	
#define ISR_INT_LINE_STATUS	0x06	
#define ISR_INT_RX_TIMEOUT	0x0C	
#define ISR_INT_RX_XOFF		0x10	
#define ISR_INT_RTS_CTS		0x20	
#define ISR_FIFO_ENABLED	0xC0	
#define ISR_INT_BITS_MASK	0x3E	


#define LCR_BITS_5		0x00	
#define LCR_BITS_6		0x01	
#define LCR_BITS_7		0x02	
#define LCR_BITS_8		0x03	
#define LCR_BITS_MASK		0x03	

#define LCR_STOP_1		0x00	
#define LCR_STOP_1_5		0x04	
#define LCR_STOP_2		0x04	
#define LCR_STOP_MASK		0x04	

#define LCR_PAR_NONE		0x00	
#define LCR_PAR_ODD		0x08	
#define LCR_PAR_EVEN		0x18	
#define LCR_PAR_MARK		0x28	
#define LCR_PAR_SPACE		0x38	
#define LCR_PAR_MASK		0x38	

#define LCR_SET_BREAK		0x40	
#define LCR_DL_ENABLE		0x80	

#define LCR_ACCESS_EFR		0xBF	
					
					


#define MCR_DTR			0x01	
#define MCR_RTS			0x02	
#define MCR_OUT1		0x04	
#define MCR_MASTER_IE		0x08	
#define MCR_LOOPBACK		0x10	
#define MCR_XON_ANY		0x20	
#define MCR_IR_ENABLE		0x40	
#define MCR_BRG_DIV_4		0x80	


#define LSR_RX_AVAIL		0x01	
#define LSR_OVER_ERR		0x02	
#define LSR_PAR_ERR		0x04	
#define LSR_FRM_ERR		0x08	
#define LSR_BREAK		0x10	
#define LSR_TX_EMPTY		0x20	
#define LSR_TX_ALL_EMPTY	0x40	
#define LSR_FIFO_ERR		0x80	


#define EDGEPORT_MSR_DELTA_CTS	0x01	
#define EDGEPORT_MSR_DELTA_DSR	0x02	
#define EDGEPORT_MSR_DELTA_RI	0x04	
#define EDGEPORT_MSR_DELTA_CD	0x08	
#define EDGEPORT_MSR_CTS	0x10	
#define EDGEPORT_MSR_DSR	0x20	
#define EDGEPORT_MSR_RI		0x40	
#define EDGEPORT_MSR_CD		0x80	



					
					
#define EFR_SWFC_NONE		0x00	
#define EFR_SWFC_RX1		0x02 	
#define EFR_SWFC_RX2		0x01 	
#define EFR_SWFC_RX12		0x03 	
#define EFR_SWFC_TX1		0x08 	
#define EFR_SWFC_TX1_RX1	0x0a 	
#define EFR_SWFC_TX1_RX2	0x09 	
#define EFR_SWFC_TX1_RX12	0x0b 	
#define EFR_SWFC_TX2		0x04 	
#define EFR_SWFC_TX2_RX1	0x06 	
#define EFR_SWFC_TX2_RX2	0x05 	
#define EFR_SWFC_TX2_RX12	0x07 	
#define EFR_SWFC_TX12		0x0c 	
#define EFR_SWFC_TX12_RX1	0x0e 	
#define EFR_SWFC_TX12_RX2	0x0d 	
#define EFR_SWFC_TX12_RX12	0x0f 	

#define EFR_TX_FC_MASK		0x0c	
#define EFR_TX_FC_NONE		0x00	
#define EFR_TX_FC_X1		0x08	
#define EFR_TX_FC_X2		0x04	
#define EFR_TX_FC_X1_2		0x0c	

#define EFR_RX_FC_MASK		0x03	
#define EFR_RX_FC_NONE		0x00	
#define EFR_RX_FC_X1		0x02	
#define EFR_RX_FC_X2		0x01	
#define EFR_RX_FC_X1_2		0x03	


#define EFR_SWFC_MASK		0x0F	
#define EFR_ENABLE_16654	0x10	
#define EFR_SPEC_DETECT		0x20	
#define EFR_AUTO_RTS		0x40	
#define EFR_AUTO_CTS		0x80	

#endif	

