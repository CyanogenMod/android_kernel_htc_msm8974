/*********************************************************************
 *                
 * Filename:      via-ircc.h
 * Version:       1.0
 * Description:   Driver for the VIA VT8231/VT8233 IrDA chipsets
 * Author:        VIA Technologies, inc
 * Date  :	  08/06/2003

Copyright (c) 1998-2003 VIA Technologies, Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTIES OR REPRESENTATIONS; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 * Comment:
 * jul/08/2002 : Rx buffer length should use Rx ring ptr.	
 * Oct/28/2002 : Add SB id for 3147 and 3177.	
 * jul/09/2002 : only implement two kind of dongle currently.
 * Oct/02/2002 : work on VT8231 and VT8233 .
 * Aug/06/2003 : change driver format to pci driver .
 ********************************************************************/
#ifndef via_IRCC_H
#define via_IRCC_H
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <asm/io.h>

#define MAX_TX_WINDOW 7
#define MAX_RX_WINDOW 7

struct st_fifo_entry {
	int status;
	int len;
};

struct st_fifo {
	struct st_fifo_entry entries[MAX_RX_WINDOW + 2];
	int pending_bytes;
	int head;
	int tail;
	int len;
};

struct frame_cb {
	void *start;		
	int len;		
};

struct tx_fifo {
	struct frame_cb queue[MAX_TX_WINDOW + 2];	
	int ptr;		
	int len;		
	int free;		
	void *tail;		
};


struct eventflag		
{
	
	unsigned char TxFIFOUnderRun;
	unsigned char EOMessage;
	unsigned char TxFIFOReady;
	unsigned char EarlyEOM;
	
	unsigned char PHYErr;
	unsigned char CRCErr;
	unsigned char RxFIFOOverRun;
	unsigned char EOPacket;
	unsigned char RxAvail;
	unsigned char TooLargePacket;
	unsigned char SIRBad;
	
	unsigned char Unknown;
	
	unsigned char TimeOut;
	unsigned char RxDMATC;
	unsigned char TxDMATC;
};

struct via_ircc_cb {
	struct st_fifo st_fifo;	
	struct tx_fifo tx_fifo;	

	struct net_device *netdev;	

	struct irlap_cb *irlap;	
	struct qos_info qos;	

	chipio_t io;		
	iobuff_t tx_buff;	
	iobuff_t rx_buff;	
	dma_addr_t tx_buff_dma;
	dma_addr_t rx_buff_dma;

	__u8 ier;		

	struct timeval stamp;
	struct timeval now;

	spinlock_t lock;	

	__u32 flags;		
	__u32 new_speed;
	int index;		

	struct eventflag EventFlag;
	unsigned int chip_id;	
	unsigned int RetryCount;
	unsigned int RxDataReady;
	unsigned int RxLastCount;
};


#define  I_CF_L_0  		0x10
#define  I_CF_H_0		0x11
#define  I_SIR_BOF		0x12
#define  I_SIR_EOF		0x13
#define  I_ST_CT_0		0x15
#define  I_ST_L_1		0x16
#define  I_ST_H_1		0x17
#define  I_CF_L_1		0x18
#define  I_CF_H_1		0x19
#define  I_CF_L_2		0x1a
#define  I_CF_H_2		0x1b
#define  I_CF_3		0x1e
#define  H_CT			0x20
#define  H_ST			0x21
#define  M_CT			0x22
#define  TX_CT_1		0x23
#define  TX_CT_2		0x24
#define  TX_ST			0x25
#define  RX_CT			0x26
#define  RX_ST			0x27
#define  RESET			0x28
#define  P_ADDR		0x29
#define  RX_C_L		0x2a
#define  RX_C_H		0x2b
#define  RX_P_L		0x2c
#define  RX_P_H		0x2d
#define  TX_C_L		0x2e
#define  TX_C_H		0x2f
#define  TIMER         	0x32
#define  I_CF_4         	0x33
#define  I_T_C_L		0x34
#define  I_T_C_H		0x35
#define  VERSION		0x3f
#define StartAddr 	0x10	
#define EndAddr 	0x3f	
#define GetBit(val,bit)  val = (unsigned char) ((val>>bit) & 0x1)
			
#define SetBit(val,bit)  val= (unsigned char ) (val | (0x1 << bit))
			
#define ResetBit(val,bit) val= (unsigned char ) (val & ~(0x1 << bit))
			

#define OFF   0
#define ON   1
#define DMA_TX_MODE   0x08
#define DMA_RX_MODE   0x04

#define DMA1   0
#define DMA2   0xc0
#define MASK1   DMA1+0x0a
#define MASK2   DMA2+0x14

#define Clk_bit 0x40
#define Tx_bit 0x01
#define Rd_Valid 0x08
#define RxBit 0x08

static void DisableDmaChannel(unsigned int channel)
{
	switch (channel) {	
	case 0:
		outb(4, MASK1);	
		break;
	case 1:
		outb(5, MASK1);	
		break;
	case 2:
		outb(6, MASK1);	
		break;
	case 3:
		outb(7, MASK1);	
		break;
	case 5:
		outb(5, MASK2);	
		break;
	case 6:
		outb(6, MASK2);	
		break;
	case 7:
		outb(7, MASK2);	
		break;
	default:
		break;
	}
}

static unsigned char ReadLPCReg(int iRegNum)
{
	unsigned char iVal;

	outb(0x87, 0x2e);
	outb(0x87, 0x2e);
	outb(iRegNum, 0x2e);
	iVal = inb(0x2f);
	outb(0xaa, 0x2e);

	return iVal;
}

static void WriteLPCReg(int iRegNum, unsigned char iVal)
{

	outb(0x87, 0x2e);
	outb(0x87, 0x2e);
	outb(iRegNum, 0x2e);
	outb(iVal, 0x2f);
	outb(0xAA, 0x2e);
}

static __u8 ReadReg(unsigned int BaseAddr, int iRegNum)
{
	return (__u8) inb(BaseAddr + iRegNum);
}

static void WriteReg(unsigned int BaseAddr, int iRegNum, unsigned char iVal)
{
	outb(iVal, BaseAddr + iRegNum);
}

static int WriteRegBit(unsigned int BaseAddr, unsigned char RegNum,
		unsigned char BitPos, unsigned char value)
{
	__u8 Rtemp, Wtemp;

	if (BitPos > 7) {
		return -1;
	}
	if ((RegNum < StartAddr) || (RegNum > EndAddr))
		return -1;
	Rtemp = ReadReg(BaseAddr, RegNum);
	if (value == 0)
		Wtemp = ResetBit(Rtemp, BitPos);
	else {
		if (value == 1)
			Wtemp = SetBit(Rtemp, BitPos);
		else
			return -1;
	}
	WriteReg(BaseAddr, RegNum, Wtemp);
	return 0;
}

static __u8 CheckRegBit(unsigned int BaseAddr, unsigned char RegNum,
		 unsigned char BitPos)
{
	__u8 temp;

	if (BitPos > 7)
		return 0xff;
	if ((RegNum < StartAddr) || (RegNum > EndAddr)) {
	}
	temp = ReadReg(BaseAddr, RegNum);
	return GetBit(temp, BitPos);
}

static void SetMaxRxPacketSize(__u16 iobase, __u16 size)
{
	__u16 low, high;
	if ((size & 0xe000) == 0) {
		low = size & 0x00ff;
		high = (size & 0x1f00) >> 8;
		WriteReg(iobase, I_CF_L_2, low);
		WriteReg(iobase, I_CF_H_2, high);

	}

}


static void SetFIFO(__u16 iobase, __u16 value)
{
	switch (value) {
	case 128:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 1);
		break;
	case 64:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 0);
		break;
	case 32:
		WriteRegBit(iobase, 0x11, 0, 1);
		WriteRegBit(iobase, 0x11, 7, 0);
		break;
	default:
		WriteRegBit(iobase, 0x11, 0, 0);
		WriteRegBit(iobase, 0x11, 7, 0);
	}

}

#define CRC16(BaseAddr,val)         WriteRegBit(BaseAddr,I_CF_L_0,7,val)	
#define SIRFilter(BaseAddr,val)     WriteRegBit(BaseAddr,I_CF_L_0,3,val)
#define Filter(BaseAddr,val)        WriteRegBit(BaseAddr,I_CF_L_0,2,val)
#define InvertTX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_L_0,1,val)
#define InvertRX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_L_0,0,val)
#define EnableTX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_H_0,4,val)
#define EnableRX(BaseAddr,val)      WriteRegBit(BaseAddr,I_CF_H_0,3,val)
#define EnableDMA(BaseAddr,val)     WriteRegBit(BaseAddr,I_CF_H_0,2,val)
#define SIRRecvAny(BaseAddr,val)    WriteRegBit(BaseAddr,I_CF_H_0,1,val)
#define DiableTrans(BaseAddr,val)   WriteRegBit(BaseAddr,I_CF_H_0,0,val)
#define SetSIRBOF(BaseAddr,val)     WriteReg(BaseAddr,I_SIR_BOF,val)
#define SetSIREOF(BaseAddr,val)     WriteReg(BaseAddr,I_SIR_EOF,val)
#define GetSIRBOF(BaseAddr)        ReadReg(BaseAddr,I_SIR_BOF)
#define GetSIREOF(BaseAddr)        ReadReg(BaseAddr,I_SIR_EOF)
#define EnPhys(BaseAddr,val)   WriteRegBit(BaseAddr,I_ST_CT_0,7,val)
#define IsModeError(BaseAddr) CheckRegBit(BaseAddr,I_ST_CT_0,6)	
#define IsVFIROn(BaseAddr)     CheckRegBit(BaseAddr,0x14,0)	
#define IsFIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,5)	
#define IsMIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,4)	
#define IsSIROn(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,3)	
#define IsEnableTX(BaseAddr)  CheckRegBit(BaseAddr,I_ST_CT_0,2)	
#define IsEnableRX(BaseAddr)  CheckRegBit(BaseAddr,I_ST_CT_0,1)	
#define Is16CRC(BaseAddr)     CheckRegBit(BaseAddr,I_ST_CT_0,0)	
#define DisableAdjacentPulseWidth(BaseAddr,val) WriteRegBit(BaseAddr,I_CF_3,5,val)	
#define DisablePulseWidthAdjust(BaseAddr,val)   WriteRegBit(BaseAddr,I_CF_3,4,val)	
#define UseOneRX(BaseAddr,val)                  WriteRegBit(BaseAddr,I_CF_3,1,val)	
#define SlowIRRXLowActive(BaseAddr,val)         WriteRegBit(BaseAddr,I_CF_3,0,val)	
#define EnAllInt(BaseAddr,val)   WriteRegBit(BaseAddr,H_CT,7,val)
#define TXStart(BaseAddr,val)    WriteRegBit(BaseAddr,H_CT,6,val)
#define RXStart(BaseAddr,val)    WriteRegBit(BaseAddr,H_CT,5,val)
#define ClearRXInt(BaseAddr,val)   WriteRegBit(BaseAddr,H_CT,4,val)	
#define IsRXInt(BaseAddr)           CheckRegBit(BaseAddr,H_ST,4)
#define GetIntIndentify(BaseAddr)   ((ReadReg(BaseAddr,H_ST)&0xf1) >>1)
#define IsHostBusy(BaseAddr)        CheckRegBit(BaseAddr,H_ST,0)
#define GetHostStatus(BaseAddr)     ReadReg(BaseAddr,H_ST)	
#define EnTXDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,7,val)
#define EnRXDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,6,val)
#define SwapDMA(BaseAddr,val)         WriteRegBit(BaseAddr,M_CT,5,val)
#define EnInternalLoop(BaseAddr,val)  WriteRegBit(BaseAddr,M_CT,4,val)
#define EnExternalLoop(BaseAddr,val)  WriteRegBit(BaseAddr,M_CT,3,val)
#define EnTXFIFOHalfLevelInt(BaseAddr,val)   WriteRegBit(BaseAddr,TX_CT_1,4,val)	
#define EnTXFIFOUnderrunEOMInt(BaseAddr,val) WriteRegBit(BaseAddr,TX_CT_1,5,val)
#define EnTXFIFOReadyInt(BaseAddr,val)       WriteRegBit(BaseAddr,TX_CT_1,6,val)	
#define ForceUnderrun(BaseAddr,val)   WriteRegBit(BaseAddr,TX_CT_2,7,val)	
#define EnTXCRC(BaseAddr,val)         WriteRegBit(BaseAddr,TX_CT_2,6,val)	
#define ForceBADCRC(BaseAddr,val)     WriteRegBit(BaseAddr,TX_CT_2,5,val)	
#define SendSIP(BaseAddr,val)         WriteRegBit(BaseAddr,TX_CT_2,4,val)	
#define ClearEnTX(BaseAddr,val)       WriteRegBit(BaseAddr,TX_CT_2,3,val)	
#define GetTXStatus(BaseAddr) 	ReadReg(BaseAddr,TX_ST)	
#define EnRXSpecInt(BaseAddr,val)           WriteRegBit(BaseAddr,RX_CT,0,val)
#define EnRXFIFOReadyInt(BaseAddr,val)      WriteRegBit(BaseAddr,RX_CT,1,val)	
#define EnRXFIFOHalfLevelInt(BaseAddr,val)  WriteRegBit(BaseAddr,RX_CT,7,val)	
#define GetRXStatus(BaseAddr) 	ReadReg(BaseAddr,RX_ST)	
#define SetPacketAddr(BaseAddr,addr)        WriteReg(BaseAddr,P_ADDR,addr)
#define EnGPIOtoRX2(BaseAddr,val)	WriteRegBit(BaseAddr,I_CF_4,7,val)
#define EnTimerInt(BaseAddr,val)		WriteRegBit(BaseAddr,I_CF_4,1,val)
#define ClearTimerInt(BaseAddr,val)	WriteRegBit(BaseAddr,I_CF_4,0,val)
#define WriteGIO(BaseAddr,val)	    WriteRegBit(BaseAddr,I_T_C_L,7,val)
#define ReadGIO(BaseAddr)		    CheckRegBit(BaseAddr,I_T_C_L,7)
#define ReadRX(BaseAddr)		    CheckRegBit(BaseAddr,I_T_C_L,3)	
#define WriteTX(BaseAddr,val)		WriteRegBit(BaseAddr,I_T_C_L,0,val)
#define EnRX2(BaseAddr,val)		    WriteRegBit(BaseAddr,I_T_C_H,7,val)
#define ReadRX2(BaseAddr)           CheckRegBit(BaseAddr,I_T_C_H,7)
#define GetFIRVersion(BaseAddr)		ReadReg(BaseAddr,VERSION)


static void SetTimer(__u16 iobase, __u8 count)
{
	EnTimerInt(iobase, OFF);
	WriteReg(iobase, TIMER, count);
	EnTimerInt(iobase, ON);
}


static void SetSendByte(__u16 iobase, __u32 count)
{
	__u32 low, high;

	if ((count & 0xf000) == 0) {
		low = count & 0x00ff;
		high = (count & 0x0f00) >> 8;
		WriteReg(iobase, TX_C_L, low);
		WriteReg(iobase, TX_C_H, high);
	}
}

static void ResetChip(__u16 iobase, __u8 type)
{
	__u8 value;

	value = (type + 2) << 4;
	WriteReg(iobase, RESET, type);
}

static int CkRxRecv(__u16 iobase, struct via_ircc_cb *self)
{
	__u8 low, high;
	__u16 wTmp = 0, wTmp1 = 0, wTmp_new = 0;

	low = ReadReg(iobase, RX_C_L);
	high = ReadReg(iobase, RX_C_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;
	udelay(10);
	low = ReadReg(iobase, RX_C_L);
	high = ReadReg(iobase, RX_C_H);
	wTmp1 = high;
	wTmp_new = (wTmp1 << 8) | low;
	if (wTmp_new != wTmp)
		return 1;
	else
		return 0;

}

static __u16 RxCurCount(__u16 iobase, struct via_ircc_cb * self)
{
	__u8 low, high;
	__u16 wTmp = 0, wTmp1 = 0;

	low = ReadReg(iobase, RX_P_L);
	high = ReadReg(iobase, RX_P_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;
	return wTmp;
}


static __u16 GetRecvByte(__u16 iobase, struct via_ircc_cb * self)
{
	__u8 low, high;
	__u16 wTmp, wTmp1, ret;

	low = ReadReg(iobase, RX_P_L);
	high = ReadReg(iobase, RX_P_H);
	wTmp1 = high;
	wTmp = (wTmp1 << 8) | low;


	if (wTmp >= self->RxLastCount)
		ret = wTmp - self->RxLastCount;
	else
		ret = (0x8000 - self->RxLastCount) + wTmp;
	self->RxLastCount = wTmp;

	return ret;
}

static void Sdelay(__u16 scale)
{
	__u8 bTmp;
	int i, j;

	for (j = 0; j < scale; j++) {
		for (i = 0; i < 0x20; i++) {
			bTmp = inb(0xeb);
			outb(bTmp, 0xeb);
		}
	}
}

static void Tdelay(__u16 scale)
{
	__u8 bTmp;
	int i, j;

	for (j = 0; j < scale; j++) {
		for (i = 0; i < 0x50; i++) {
			bTmp = inb(0xeb);
			outb(bTmp, 0xeb);
		}
	}
}


static void ActClk(__u16 iobase, __u8 value)
{
	__u8 bTmp;
	bTmp = ReadReg(iobase, 0x34);
	if (value)
		WriteReg(iobase, 0x34, bTmp | Clk_bit);
	else
		WriteReg(iobase, 0x34, bTmp & ~Clk_bit);
}

static void ClkTx(__u16 iobase, __u8 Clk, __u8 Tx)
{
	__u8 bTmp;

	bTmp = ReadReg(iobase, 0x34);
	if (Clk == 0)
		bTmp &= ~Clk_bit;
	else {
		if (Clk == 1)
			bTmp |= Clk_bit;
	}
	WriteReg(iobase, 0x34, bTmp);
	Sdelay(1);
	if (Tx == 0)
		bTmp &= ~Tx_bit;
	else {
		if (Tx == 1)
			bTmp |= Tx_bit;
	}
	WriteReg(iobase, 0x34, bTmp);
}

static void Wr_Byte(__u16 iobase, __u8 data)
{
	__u8 bData = data;
	int i;

	ClkTx(iobase, 0, 1);

	Tdelay(2);
	ActClk(iobase, 1);
	Tdelay(1);

	for (i = 0; i < 8; i++) {	

		if ((bData >> i) & 0x01) {
			ClkTx(iobase, 0, 1);	
		} else {
			ClkTx(iobase, 0, 0);	
		}
		Tdelay(2);
		Sdelay(1);
		ActClk(iobase, 1);	
		Tdelay(1);
	}
}

static __u8 Rd_Indx(__u16 iobase, __u8 addr, __u8 index)
{
	__u8 data = 0, bTmp, data_bit;
	int i;

	bTmp = addr | (index << 1) | 0;
	ClkTx(iobase, 0, 0);
	Tdelay(2);
	ActClk(iobase, 1);
	udelay(1);
	Wr_Byte(iobase, bTmp);
	Sdelay(1);
	ClkTx(iobase, 0, 0);
	Tdelay(2);
	for (i = 0; i < 10; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(1);
		ClkTx(iobase, 0, 1);
		Tdelay(1);
		bTmp = ReadReg(iobase, 0x34);
		if (!(bTmp & Rd_Valid))
			break;
	}
	if (!(bTmp & Rd_Valid)) {
		for (i = 0; i < 8; i++) {
			ActClk(iobase, 1);
			Tdelay(1);
			ActClk(iobase, 0);
			bTmp = ReadReg(iobase, 0x34);
			data_bit = 1 << i;
			if (bTmp & RxBit)
				data |= data_bit;
			else
				data &= ~data_bit;
			Tdelay(2);
		}
	} else {
		for (i = 0; i < 2; i++) {
			ActClk(iobase, 1);
			Tdelay(1);
			ActClk(iobase, 0);
			Tdelay(2);
		}
		bTmp = ReadReg(iobase, 0x34);
	}
	for (i = 0; i < 1; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(2);
	}
	ClkTx(iobase, 0, 0);
	Tdelay(1);
	for (i = 0; i < 3; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(2);
	}
	return data;
}

static void Wr_Indx(__u16 iobase, __u8 addr, __u8 index, __u8 data)
{
	int i;
	__u8 bTmp;

	ClkTx(iobase, 0, 0);
	udelay(2);
	ActClk(iobase, 1);
	udelay(1);
	bTmp = addr | (index << 1) | 1;
	Wr_Byte(iobase, bTmp);
	Wr_Byte(iobase, data);
	for (i = 0; i < 2; i++) {
		ClkTx(iobase, 0, 0);
		Tdelay(2);
		ActClk(iobase, 1);
		Tdelay(1);
	}
	ActClk(iobase, 0);
}

static void ResetDongle(__u16 iobase)
{
	int i;
	ClkTx(iobase, 0, 0);
	Tdelay(1);
	for (i = 0; i < 30; i++) {
		ActClk(iobase, 1);
		Tdelay(1);
		ActClk(iobase, 0);
		Tdelay(1);
	}
	ActClk(iobase, 0);
}

static void SetSITmode(__u16 iobase)
{

	__u8 bTmp;

	bTmp = ReadLPCReg(0x28);
	WriteLPCReg(0x28, bTmp | 0x10);	
	bTmp = ReadReg(iobase, 0x35);
	WriteReg(iobase, 0x35, bTmp | 0x40);	
	WriteReg(iobase, 0x28, bTmp | 0x80);	
}

static void SI_SetMode(__u16 iobase, int mode)
{
	
	__u8 bTmp;

	WriteLPCReg(0x28, 0x70);	
	SetSITmode(iobase);
	ResetDongle(iobase);
	udelay(10);
	Wr_Indx(iobase, 0x40, 0x0, 0x17);	
	Wr_Indx(iobase, 0x40, 0x1, mode);	
	Wr_Indx(iobase, 0x40, 0x2, 0xff);	
	bTmp = Rd_Indx(iobase, 0x40, 1);
}

static void InitCard(__u16 iobase)
{
	ResetChip(iobase, 5);
	WriteReg(iobase, I_ST_CT_0, 0x00);	
	SetSIRBOF(iobase, 0xc0);	
	SetSIREOF(iobase, 0xc1);
}

static void CommonInit(__u16 iobase)
{
	SwapDMA(iobase, OFF);
	SetMaxRxPacketSize(iobase, 0x0fff);	
	EnRXFIFOReadyInt(iobase, OFF);
	EnRXFIFOHalfLevelInt(iobase, OFF);
	EnTXFIFOHalfLevelInt(iobase, OFF);
	EnTXFIFOUnderrunEOMInt(iobase, ON);
	InvertTX(iobase, OFF);
	InvertRX(iobase, OFF);
	if (IsSIROn(iobase)) {
		SIRFilter(iobase, ON);
		SIRRecvAny(iobase, ON);
	} else {
		SIRFilter(iobase, OFF);
		SIRRecvAny(iobase, OFF);
	}
	EnRXSpecInt(iobase, ON);
	WriteReg(iobase, I_ST_CT_0, 0x80);
	EnableDMA(iobase, ON);
}

static void SetBaudRate(__u16 iobase, __u32 rate)
{
	__u8 value = 11, temp;

	if (IsSIROn(iobase)) {
		switch (rate) {
		case (__u32) (2400L):
			value = 47;
			break;
		case (__u32) (9600L):
			value = 11;
			break;
		case (__u32) (19200L):
			value = 5;
			break;
		case (__u32) (38400L):
			value = 2;
			break;
		case (__u32) (57600L):
			value = 1;
			break;
		case (__u32) (115200L):
			value = 0;
			break;
		default:
			break;
		}
	} else if (IsMIROn(iobase)) {
		value = 0;	
	} else if (IsFIROn(iobase)) {
		value = 0;	
	}
	temp = (ReadReg(iobase, I_CF_H_1) & 0x03);
	temp |= value << 2;
	WriteReg(iobase, I_CF_H_1, temp);
}

static void SetPulseWidth(__u16 iobase, __u8 width)
{
	__u8 temp, temp1, temp2;

	temp = (ReadReg(iobase, I_CF_L_1) & 0x1f);
	temp1 = (ReadReg(iobase, I_CF_H_1) & 0xfc);
	temp2 = (width & 0x07) << 5;
	temp |= temp2;
	temp2 = (width & 0x18) >> 3;
	temp1 |= temp2;
	WriteReg(iobase, I_CF_L_1, temp);
	WriteReg(iobase, I_CF_H_1, temp1);
}

static void SetSendPreambleCount(__u16 iobase, __u8 count)
{
	__u8 temp;

	temp = ReadReg(iobase, I_CF_L_1) & 0xe0;
	temp |= count;
	WriteReg(iobase, I_CF_L_1, temp);

}

static void SetVFIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_H_0, 5, val);
}

static void SetFIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 6, val);
}

static void SetMIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 5, val);
}

static void SetSIR(__u16 BaseAddr, __u8 val)
{
	__u8 tmp;

	WriteRegBit(BaseAddr, I_CF_H_0, 5, 0);
	tmp = ReadReg(BaseAddr, I_CF_L_0);
	WriteReg(BaseAddr, I_CF_L_0, tmp & 0x8f);
	WriteRegBit(BaseAddr, I_CF_L_0, 4, val);
}

#endif				
