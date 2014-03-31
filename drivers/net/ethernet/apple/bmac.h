/*
 * mace.h - definitions for the registers in the "Big Mac"
 *  Ethernet controller found in PowerMac G3 models.
 *
 * Copyright (C) 1998 Randy Gobbel.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */




#define	XIFC		0x000   
#	define	TxOutputEnable	0x0001 
#	define	XIFLoopback	0x0002 
#	define	MIILoopback	0x0004 
#	define	MIILoopbackBits	0x0006
#	define	MIIBuffDisable	0x0008 
#	define	SQETestEnable	0x0010 
#	define	SQETimeWindow	0x03e0 
#	define	XIFLanceMode	0x0010 
#	define	XIFLanceIPG0	0x03e0 
#define	TXFIFOCSR	0x100   
#	define	TxFIFOEnable	0x0001
#define	TXTH		0x110   
#	define	TxThreshold	0x0004
#define RXFIFOCSR	0x120   
#	define	RxFIFOEnable	0x0001
#define MEMADD		0x130   
#define MEMDATAHI	0x140   
#define MEMDATALO	0x150   
#define XCVRIF		0x160   
#	define	COLActiveLow	0x0002
#	define	SerialMode	0x0004
#	define	ClkBit		0x0008
#	define	LinkStatus	0x0100
#define CHIPID          0x170   
#define	MIFCSR		0x180   
#define	SROMCSR		0x190   
#	define	ChipSelect	0x0001
#	define	Clk		0x0002
#define TXPNTR		0x1a0   
#define	RXPNTR		0x1b0   
#define	STATUS		0x200   
#define	INTDISABLE	0x210   
#	define	FrameReceived	0x00000001 
#	define	RxFrameCntExp	0x00000002 
#	define	RxAlignCntExp	0x00000004 
#	define	RxCRCCntExp	0x00000008 
#	define	RxLenCntExp	0x00000010 
#	define	RxOverFlow	0x00000020 
#	define	RxCodeViolation	0x00000040 
#	define	SQETestError	0x00000080 
#	define	FrameSent	0x00000100 
#	define	TxUnderrun	0x00000200 
#	define	TxMaxSizeError	0x00000400 
#	define	TxNormalCollExp	0x00000800 
#	define	TxExcessCollExp	0x00001000 
#	define	TxLateCollExp	0x00002000 
#	define	TxNetworkCollExp 0x00004000 
#	define	TxDeferTimerExp	0x00008000 
#	define	RxFIFOToHost	0x00010000 
#	define	RxNoDescriptors	0x00020000 
#	define	RxDMAError	0x00040000 
#	define	RxDMALateErr	0x00080000 
#	define	RxParityErr	0x00100000 
#	define	RxTagError	0x00200000 
#	define	TxEOPError	0x00400000 
#	define	MIFIntrEvent	0x00800000 
#	define	TxHostToFIFO	0x01000000 
#	define	TxFIFOAllSent	0x02000000 
#	define	TxDMAError	0x04000000 
#	define	TxDMALateError	0x08000000 
#	define	TxParityError	0x10000000 
#	define	TxTagError	0x20000000 
#	define	PIOError	0x40000000 
#	define	PIOParityError	0x80000000 
#	define	DisableAll	0xffffffff
#	define	EnableAll	0x00000000
#	define	EnableNormal	~(FrameReceived | FrameSent)
#	define	EnableErrors	(FrameReceived | FrameSent)
#	define	RxErrorMask	(RxFrameCntExp | RxAlignCntExp | RxCRCCntExp | \
				 RxLenCntExp | RxOverFlow | RxCodeViolation)
#	define	TxErrorMask	(TxUnderrun | TxMaxSizeError | TxExcessCollExp | \
				 TxLateCollExp | TxNetworkCollExp | TxDeferTimerExp)

#define	TXRST		0x420   
#	define	TxResetBit	0x0001
#define	TXCFG		0x430   
#	define	TxMACEnable	0x0001 
#	define	TxSlowMode	0x0020 
#	define	TxIgnoreColl	0x0040 
#	define	TxNoFCS		0x0080 
#	define	TxNoBackoff	0x0100 
#	define	TxFullDuplex	0x0200 
#	define	TxNeverGiveUp	0x0400 
#define IPG1		0x440   
#define IPG2		0x450   
#define ALIMIT		0x460   
#define SLOT		0x470   
#define PALEN		0x480   
#define PAPAT		0x490   
#define TXSFD		0x4a0   
#define JAM		0x4b0   
#define TXMAX		0x4c0   
#define TXMIN		0x4d0   
#define PAREG		0x4e0   
#define DCNT		0x4f0   
#define NCCNT		0x500   
#define NTCNT		0x510   
#define EXCNT		0x520   
#define LTCNT		0x530   
#define RSEED		0x540   
#define TXSM		0x550   

#define RXRST		0x620   
#	define	RxResetValue	0x0000
#define RXCFG		0x630   
#	define	RxMACEnable	0x0001 
#	define	RxCFGReserved	0x0004
#	define	RxPadStripEnab	0x0020 
#	define	RxPromiscEnable	0x0040 
#	define	RxNoErrCheck	0x0080 
#	define	RxCRCNoStrip	0x0100 
#	define	RxRejectOwnPackets 0x0200 
#	define	RxGrpPromisck	0x0400 
#	define	RxHashFilterEnable 0x0800 
#	define	RxAddrFilterEnable 0x1000 
#define RXMAX		0x640   
#define RXMIN		0x650   
#define MADD2		0x660   
#define MADD1		0x670   
#define MADD0		0x680   
#define FRCNT		0x690   
#define LECNT		0x6a0   
#define AECNT		0x6b0   
#define FECNT		0x6c0   
#define RXSM		0x6d0   
#define RXCV		0x6e0   

#define BHASH3		0x700   
#define BHASH2		0x710   
#define BHASH1		0x720   
#define BHASH0		0x730   

#define AFR2		0x740   
#define AFR1		0x750   
#define AFR0		0x760   
#define AFCR		0x770   
#	define	EnableAllCompares 0x0fff

