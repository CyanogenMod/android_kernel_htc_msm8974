/*
 * Intel Langwell USB Device Controller driver
 * Copyright (C) 2008-2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef __LANGWELL_UDC_H
#define __LANGWELL_UDC_H


#define	CAP_REG_OFFSET		0x0
#define	OP_REG_OFFSET		0x28

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

#define	DQH_ALIGNMENT		2048
#define	DTD_ALIGNMENT		64
#define	DMA_BOUNDARY		4096

#define	EP0_MAX_PKT_SIZE	64
#define EP_DIR_IN		1
#define EP_DIR_OUT		0

#define FLUSH_TIMEOUT		1000
#define RESET_TIMEOUT		1000
#define SETUPSTAT_TIMEOUT	100
#define PRIME_TIMEOUT		100



struct langwell_cap_regs {
	
	u8	caplength;	
	u8	_reserved3;
	u16	hciversion;	
	u32	hcsparams;	
	u32	hccparams;	
#define	HCC_LEN	BIT(17)		
	u8	_reserved4[0x20-0xc];
	
	u16	dciversion;	
	u8	_reserved5[0x24-0x22];
	u32	dccparams;	
#define	HOSTCAP	BIT(8)		
#define	DEVCAP	BIT(7)		
#define DEN(d)	\
	(((d)>>0)&0x1f)		
} __attribute__ ((packed));


struct langwell_op_regs {
	
	u32	extsts;
#define	EXTS_TI1	BIT(4)	
#define	EXTS_TI1TI0	BIT(3)	
#define	EXTS_TI1UPI	BIT(2)	
#define	EXTS_TI1UAI	BIT(1)	
#define	EXTS_TI1NAKI	BIT(0)	
	u32	extintr;
#define	EXTI_TIE1	BIT(4)	
#define	EXTI_TIE0	BIT(3)	
#define	EXTI_UPIE	BIT(2)	
#define	EXTI_UAIE	BIT(1)	
#define	EXTI_NAKE	BIT(0)	
	
	u32	usbcmd;
#define	CMD_HIRD(u)	\
	(((u)>>24)&0xf)		
#define	CMD_ITC(u)	\
	(((u)>>16)&0xff)	
#define	CMD_PPE		BIT(15)	
#define	CMD_ATDTW	BIT(14)	
#define	CMD_SUTW	BIT(13)	
#define	CMD_ASPE	BIT(11) 
#define	CMD_FS2		BIT(10)	
#define	CMD_ASP1	BIT(9)	
#define	CMD_ASP0	BIT(8)
#define	CMD_LR		BIT(7)	
#define	CMD_IAA		BIT(6)	
#define	CMD_ASE		BIT(5)	
#define	CMD_PSE		BIT(4)	
#define	CMD_FS1		BIT(3)
#define	CMD_FS0		BIT(2)
#define	CMD_RST		BIT(1)	
#define	CMD_RUNSTOP	BIT(0)	
	u32	usbsts;
#define	STS_PPCI(u)	\
	(((u)>>16)&0xffff)	
#define	STS_AS		BIT(15)	
#define	STS_PS		BIT(14)	
#define	STS_RCL		BIT(13)	
#define	STS_HCH		BIT(12)	
#define	STS_ULPII	BIT(10)	
#define	STS_SLI		BIT(8)	
#define	STS_SRI		BIT(7)	
#define	STS_URI		BIT(6)	
#define	STS_AAI		BIT(5)	
#define	STS_SEI		BIT(4)	
#define	STS_FRI		BIT(3)	
#define	STS_PCI		BIT(2)	
#define	STS_UEI		BIT(1)	
#define	STS_UI		BIT(0)	
	u32	usbintr;
#define	INTR_PPCE(u)	(((u)>>16)&0xffff)
#define	INTR_ULPIE	BIT(10)	
#define	INTR_SLE	BIT(8)	
#define	INTR_SRE	BIT(7)	
#define	INTR_URE	BIT(6)	
#define	INTR_AAE	BIT(5)	
#define	INTR_SEE	BIT(4)	
#define	INTR_FRE	BIT(3)	
#define	INTR_PCE	BIT(2)	
#define	INTR_UEE	BIT(1)	
#define	INTR_UE		BIT(0)	
	u32	frindex;	
#define	FRINDEX_MASK	(0x3fff << 0)
	u32	ctrldssegment;	
	u32	deviceaddr;
#define USBADR_SHIFT	25
#define	USBADR(d)	\
	(((d)>>25)&0x7f)	
#define USBADR_MASK	(0x7f << 25)
#define	USBADRA		BIT(24)	
	u32	endpointlistaddr;
#define	EPBASE(d)	(((d)>>11)&0x1fffff)
#define	ENDPOINTLISTADDR_MASK	(0x1fffff << 11)
	u32	ttctrl;		
	
	u32	burstsize;	
#define	TXPBURST(b)	\
	(((b)>>8)&0xff)		
#define	RXPBURST(b)	\
	(((b)>>0)&0xff)		
	u32	txfilltuning;	
	u32	txttfilltuning;	
	u32	ic_usb;		
	
	u32	ulpi_viewport;	
#define	ULPIWU		BIT(31)	
#define	ULPIRUN		BIT(30)	
#define	ULPIRW		BIT(29)	
#define	ULPISS		BIT(27)	
#define	ULPIPORT(u)	\
	(((u)>>24)&7)		
#define	ULPIADDR(u)	\
	(((u)>>16)&0xff)	
#define	ULPIDATRD(u)	\
	(((u)>>8)&0xff)		
#define	ULPIDATWR(u)	\
	(((u)>>0)&0xff)		
	u8	_reserved6[0x70-0x64];
	
	u32	configflag;	
	u32	portsc1;	
#define	DA(p)	\
	(((p)>>25)&0x7f)	
#define	PORTS_SSTS	(BIT(24) | BIT(23))	
#define	PORTS_WKOC	BIT(22)	
#define	PORTS_WKDS	BIT(21)	
#define	PORTS_WKCN	BIT(20)	
#define	PORTS_PTC(p)	(((p)>>16)&0xf)	
#define	PORTS_PIC	(BIT(15) | BIT(14))	
#define	PORTS_PO	BIT(13)	
#define	PORTS_PP	BIT(12)	
#define	PORTS_LS	(BIT(11) | BIT(10))	
#define	PORTS_SLP	BIT(9)	
#define	PORTS_PR	BIT(8)	
#define	PORTS_SUSP	BIT(7)	
#define	PORTS_FPR	BIT(6)	
#define	PORTS_OCC	BIT(5)	
#define	PORTS_OCA	BIT(4)	
#define	PORTS_PEC	BIT(3)	
#define	PORTS_PE	BIT(2)	
#define	PORTS_CSC	BIT(1)	
#define	PORTS_CCS	BIT(0)	
	u8	_reserved7[0xb4-0x78];
	
	u32	devlc;		
#define	LPM_PTS(d)	(((d)>>29)&7)
#define	LPM_STS		BIT(28)	
#define	LPM_PTW		BIT(27)	
#define	LPM_PSPD(d)	(((d)>>25)&3)	
#define LPM_PSPD_MASK	(BIT(26) | BIT(25))
#define LPM_SPEED_FULL	0
#define LPM_SPEED_LOW	1
#define LPM_SPEED_HIGH	2
#define	LPM_SRT		BIT(24)	
#define	LPM_PFSC	BIT(23)	
#define	LPM_PHCD	BIT(22) 
#define	LPM_STL		BIT(16)	
#define	LPM_BA(d)	\
	(((d)>>1)&0x7ff)	
#define	LPM_NYT_ACK	BIT(0)	
	u8	_reserved8[0xf4-0xb8];
	
	u32	otgsc;		
#define	OTGSC_DPIE	BIT(30)	
#define	OTGSC_MSE	BIT(29)	
#define	OTGSC_BSEIE	BIT(28)	
#define	OTGSC_BSVIE	BIT(27)	
#define	OTGSC_ASVIE	BIT(26)	
#define	OTGSC_AVVIE	BIT(25)	
#define	OTGSC_IDIE	BIT(24)	
#define	OTGSC_DPIS	BIT(22)	
#define	OTGSC_MSS	BIT(21)	
#define	OTGSC_BSEIS	BIT(20)	
#define	OTGSC_BSVIS	BIT(19)	
#define	OTGSC_ASVIS	BIT(18)	
#define	OTGSC_AVVIS	BIT(17)	
#define	OTGSC_IDIS	BIT(16)	
#define	OTGSC_DPS	BIT(14)	
#define	OTGSC_MST	BIT(13)	
#define	OTGSC_BSE	BIT(12)	
#define	OTGSC_BSV	BIT(11)	
#define	OTGSC_ASV	BIT(10)	
#define	OTGSC_AVV	BIT(9)	
#define	OTGSC_USBID	BIT(8)	
#define	OTGSC_HABA	BIT(7)	
#define	OTGSC_HADP	BIT(6)	
#define	OTGSC_IDPU	BIT(5)	
#define	OTGSC_DP	BIT(4)	
#define	OTGSC_OT	BIT(3)	
#define	OTGSC_HAAR	BIT(2)	
#define	OTGSC_VC	BIT(1)	
#define	OTGSC_VD	BIT(0)	
	u32	usbmode;
#define	MODE_VBPS	BIT(5)	
#define	MODE_SDIS	BIT(4)	
#define	MODE_SLOM	BIT(3)	
#define	MODE_ENSE	BIT(2)	
#define	MODE_CM(u)	(((u)>>0)&3)	
#define	MODE_IDLE	0
#define	MODE_DEVICE	2
#define	MODE_HOST	3
	u8	_reserved9[0x100-0xfc];
	
	u32	endptnak;
#define	EPTN(e)		\
	(((e)>>16)&0xffff)	
#define	EPRN(e)		\
	(((e)>>0)&0xffff)	
	u32	endptnaken;
#define	EPTNE(e)	\
	(((e)>>16)&0xffff)	
#define	EPRNE(e)	\
	(((e)>>0)&0xffff)	
	u32	endptsetupstat;
#define	SETUPSTAT_MASK		(0xffff << 0)	
#define EP0SETUPSTAT_MASK	1
	u32	endptprime;
#define	PETB(e)		(((e)>>16)&0xffff)
#define	PERB(e)		(((e)>>0)&0xffff)
	
	u32	endptflush;
#define	FETB(e)		(((e)>>16)&0xffff)
#define	FERB(e)		(((e)>>0)&0xffff)
	u32	endptstat;
#define	ETBR(e)		(((e)>>16)&0xffff)
#define	ERBR(e)		(((e)>>0)&0xffff)
	u32	endptcomplete;
#define	ETCE(e)		(((e)>>16)&0xffff)
#define	ERCE(e)		(((e)>>0)&0xffff)
	
	u32	endptctrl[16];
#define	EPCTRL_TXE	BIT(23)	
#define	EPCTRL_TXR	BIT(22)	
#define	EPCTRL_TXI	BIT(21)	
#define	EPCTRL_TXT(e)	(((e)>>18)&3)	
#define	EPCTRL_TXT_SHIFT	18
#define	EPCTRL_TXD	BIT(17)	
#define	EPCTRL_TXS	BIT(16)	
#define	EPCTRL_RXE	BIT(7)	
#define	EPCTRL_RXR	BIT(6)	
#define	EPCTRL_RXI	BIT(5)	
#define	EPCTRL_RXT(e)	(((e)>>2)&3)	
#define	EPCTRL_RXT_SHIFT	2	
#define	EPCTRL_RXD	BIT(1)	
#define	EPCTRL_RXS	BIT(0)	
} __attribute__ ((packed));

#endif 

