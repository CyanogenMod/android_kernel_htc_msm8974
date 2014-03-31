/*
 * R8A66597 driver platform data
 *
 * Copyright (C) 2009  Renesas Solutions Corp.
 *
 * Author : Yoshihiro Shimoda <yoshihiro.shimoda.uh@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __LINUX_USB_R8A66597_H
#define __LINUX_USB_R8A66597_H

#define R8A66597_PLATDATA_XTAL_12MHZ	0x01
#define R8A66597_PLATDATA_XTAL_24MHZ	0x02
#define R8A66597_PLATDATA_XTAL_48MHZ	0x03

struct r8a66597_platdata {
	
	void (*port_power)(int port, int power);

	
	u16		buswait;

	
	unsigned	on_chip:1;

	
	unsigned	xtal:2;

	
	unsigned	vif:1;

	
	unsigned	endian:1;

	
	unsigned	wr0_shorted_to_wr1:1;

	
	unsigned	sudmac:1;
};

#define SYSCFG0		0x00
#define SYSCFG1		0x02
#define SYSSTS0		0x04
#define SYSSTS1		0x06
#define DVSTCTR0	0x08
#define DVSTCTR1	0x0A
#define TESTMODE	0x0C
#define PINCFG		0x0E
#define DMA0CFG		0x10
#define DMA1CFG		0x12
#define CFIFO		0x14
#define D0FIFO		0x18
#define D1FIFO		0x1C
#define CFIFOSEL	0x20
#define CFIFOCTR	0x22
#define CFIFOSIE	0x24
#define D0FIFOSEL	0x28
#define D0FIFOCTR	0x2A
#define D1FIFOSEL	0x2C
#define D1FIFOCTR	0x2E
#define INTENB0		0x30
#define INTENB1		0x32
#define INTENB2		0x34
#define BRDYENB		0x36
#define NRDYENB		0x38
#define BEMPENB		0x3A
#define SOFCFG		0x3C
#define INTSTS0		0x40
#define INTSTS1		0x42
#define INTSTS2		0x44
#define BRDYSTS		0x46
#define NRDYSTS		0x48
#define BEMPSTS		0x4A
#define FRMNUM		0x4C
#define UFRMNUM		0x4E
#define USBADDR		0x50
#define USBREQ		0x54
#define USBVAL		0x56
#define USBINDX		0x58
#define USBLENG		0x5A
#define DCPCFG		0x5C
#define DCPMAXP		0x5E
#define DCPCTR		0x60
#define PIPESEL		0x64
#define PIPECFG		0x68
#define PIPEBUF		0x6A
#define PIPEMAXP	0x6C
#define PIPEPERI	0x6E
#define PIPE1CTR	0x70
#define PIPE2CTR	0x72
#define PIPE3CTR	0x74
#define PIPE4CTR	0x76
#define PIPE5CTR	0x78
#define PIPE6CTR	0x7A
#define PIPE7CTR	0x7C
#define PIPE8CTR	0x7E
#define PIPE9CTR	0x80
#define PIPE1TRE	0x90
#define PIPE1TRN	0x92
#define PIPE2TRE	0x94
#define PIPE2TRN	0x96
#define PIPE3TRE	0x98
#define PIPE3TRN	0x9A
#define PIPE4TRE	0x9C
#define	PIPE4TRN	0x9E
#define	PIPE5TRE	0xA0
#define	PIPE5TRN	0xA2
#define DEVADD0		0xD0
#define DEVADD1		0xD2
#define DEVADD2		0xD4
#define DEVADD3		0xD6
#define DEVADD4		0xD8
#define DEVADD5		0xDA
#define DEVADD6		0xDC
#define DEVADD7		0xDE
#define DEVADD8		0xE0
#define DEVADD9		0xE2
#define DEVADDA		0xE4

#define	XTAL		0xC000	
#define	  XTAL48	 0x8000	  
#define	  XTAL24	 0x4000	  
#define	  XTAL12	 0x0000	  
#define	XCKE		0x2000	
#define	PLLC		0x0800	
#define	SCKE		0x0400	
#define	PCSDIS		0x0200	
#define	LPSME		0x0100	
#define	HSE		0x0080	
#define	DCFM		0x0040	
#define	DRPD		0x0020	
#define	DPRPU		0x0010	
#define	USBE		0x0001	

#define	OVCBIT		0x8000	
#define	OVCMON		0xC000	
#define	SOFEA		0x0020	
#define	IDMON		0x0004	
#define	LNST		0x0003	
#define	  SE1		 0x0003	  
#define	  FS_KSTS	 0x0002	  
#define	  FS_JSTS	 0x0001	  
#define	  LS_JSTS	 0x0002	  
#define	  LS_KSTS	 0x0001	  
#define	  SE0		 0x0000	  

#define	EXTLP0		0x0400	
#define	VBOUT		0x0200	
#define	WKUP		0x0100	
#define	RWUPE		0x0080	
#define	USBRST		0x0040	
#define	RESUME		0x0020	
#define	UACT		0x0010	
#define	RHST		0x0007	
#define	  HSPROC	 0x0004	  
#define	  HSMODE	 0x0003	  
#define	  FSMODE	 0x0002	  
#define	  LSMODE	 0x0001	  
#define	  UNDECID	 0x0000	  

#define	UTST			0x000F	
#define	  H_TST_PACKET		 0x000C	  
#define	  H_TST_SE0_NAK		 0x000B	  
#define	  H_TST_K		 0x000A	  
#define	  H_TST_J		 0x0009	  
#define	  H_TST_NORMAL		 0x0000	  
#define	  P_TST_PACKET		 0x0004	  
#define	  P_TST_SE0_NAK		 0x0003	  
#define	  P_TST_K		 0x0002	  
#define	  P_TST_J		 0x0001	  
#define	  P_TST_NORMAL		 0x0000	  

#define	LDRV			0x8000	
#define	  VIF1			  0x0000		
#define	  VIF3			  0x8000		
#define	INTA			0x0001	

#define	DREQA			0x4000	
#define	BURST			0x2000	
#define	DACKA			0x0400	
#define	DFORM			0x0380	
#define	  CPU_ADR_RD_WR		 0x0000	  
#define	  CPU_DACK_RD_WR	 0x0100	  
#define	  CPU_DACK_ONLY		 0x0180	  
#define	  SPLIT_DACK_ONLY	 0x0200	  
#define	DENDA			0x0040	
#define	PKTM			0x0020	
#define	DENDE			0x0010	
#define	OBUS			0x0004	

#define	RCNT		0x8000	
#define	REW		0x4000	
#define	DCLRM		0x2000	
#define	DREQE		0x1000	
#define	  MBW_8		 0x0000	  
#define	  MBW_16	 0x0400	  
#define	  MBW_32	 0x0800   
#define	BIGEND		0x0100	
#define	  BYTE_LITTLE	 0x0000		
#define	  BYTE_BIG	 0x0100		
#define	ISEL		0x0020	
#define	CURPIPE		0x000F	

#define	BVAL		0x8000	
#define	BCLR		0x4000	
#define	FRDY		0x2000	
#define	DTLN		0x0FFF	

#define	VBSE	0x8000	
#define	RSME	0x4000	
#define	SOFE	0x2000	
#define	DVSE	0x1000	
#define	CTRE	0x0800	
#define	BEMPE	0x0400	
#define	NRDYE	0x0200	
#define	BRDYE	0x0100	

#define	OVRCRE		0x8000	
#define	BCHGE		0x4000	
#define	DTCHE		0x1000	
#define	ATTCHE		0x0800	
#define	EOFERRE		0x0040	
#define	SIGNE		0x0020	
#define	SACKE		0x0010	

#define	BRDY9		0x0200	
#define	BRDY8		0x0100	
#define	BRDY7		0x0080	
#define	BRDY6		0x0040	
#define	BRDY5		0x0020	
#define	BRDY4		0x0010	
#define	BRDY3		0x0008	
#define	BRDY2		0x0004	
#define	BRDY1		0x0002	
#define	BRDY0		0x0001	

#define	NRDY9		0x0200	
#define	NRDY8		0x0100	
#define	NRDY7		0x0080	
#define	NRDY6		0x0040	
#define	NRDY5		0x0020	
#define	NRDY4		0x0010	
#define	NRDY3		0x0008	
#define	NRDY2		0x0004	
#define	NRDY1		0x0002	
#define	NRDY0		0x0001	

#define	BEMP9		0x0200	
#define	BEMP8		0x0100	
#define	BEMP7		0x0080	
#define	BEMP6		0x0040	
#define	BEMP5		0x0020	
#define	BEMP4		0x0010	
#define	BEMP3		0x0008	
#define	BEMP2		0x0004	
#define	BEMP1		0x0002	
#define	BEMP0		0x0001	

#define	TRNENSEL	0x0100	
#define	BRDYM		0x0040	
#define	INTL		0x0020	
#define	EDGESTS		0x0010	
#define	SOFMODE		0x000C	
#define	  SOF_125US	 0x0008	  
#define	  SOF_1MS	 0x0004	  
#define	  SOF_DISABLE	 0x0000	  

#define	VBINT	0x8000	
#define	RESM	0x4000	
#define	SOFR	0x2000	
#define	DVST	0x1000	
#define	CTRT	0x0800	
#define	BEMP	0x0400	
#define	NRDY	0x0200	
#define	BRDY	0x0100	
#define	VBSTS	0x0080	
#define	DVSQ	0x0070	
#define	  DS_SPD_CNFG	 0x0070	  
#define	  DS_SPD_ADDR	 0x0060	  
#define	  DS_SPD_DFLT	 0x0050	  
#define	  DS_SPD_POWR	 0x0040	  
#define	  DS_SUSP	 0x0040	  
#define	  DS_CNFG	 0x0030	  
#define	  DS_ADDS	 0x0020	  
#define	  DS_DFLT	 0x0010	  
#define	  DS_POWR	 0x0000	  
#define	DVSQS		0x0030	
#define	VALID		0x0008	
#define	CTSQ		0x0007	
#define	  CS_SQER	 0x0006	  
#define	  CS_WRND	 0x0005	  
#define	  CS_WRSS	 0x0004	  
#define	  CS_WRDS	 0x0003	  
#define	  CS_RDSS	 0x0002	  
#define	  CS_RDDS	 0x0001	  
#define	  CS_IDST	 0x0000	  

#define	OVRCR		0x8000	
#define	BCHG		0x4000	
#define	DTCH		0x1000	
#define	ATTCH		0x0800	
#define	EOFERR		0x0040	
#define	SIGN		0x0020	
#define	SACK		0x0010	

#define	OVRN		0x8000	
#define	CRCE		0x4000	
#define	FRNM		0x07FF	

#define	UFRNM		0x0007	

#define	DEVSEL	0xF000	
#define	MAXP	0x007F	

#define	BSTS		0x8000	
#define	SUREQ		0x4000	
#define	CSCLR		0x2000	
#define	CSSTS		0x1000	
#define	SUREQCLR	0x0800	
#define	SQCLR		0x0100	
#define	SQSET		0x0080	
#define	SQMON		0x0040	
#define	PBUSY		0x0020	
#define	PINGE		0x0010	
#define	CCPL		0x0004	
#define	PID		0x0003	
#define	  PID_STALL11	 0x0003	  
#define	  PID_STALL	 0x0002	  
#define	  PID_BUF	 0x0001	  
#define	  PID_NAK	 0x0000	  

#define	PIPENM		0x0007	

#define	R8A66597_TYP	0xC000	
#define	  R8A66597_ISO	 0xC000		  
#define	  R8A66597_INT	 0x8000		  
#define	  R8A66597_BULK	 0x4000		  
#define	R8A66597_BFRE	0x0400	
#define	R8A66597_DBLB	0x0200	
#define	R8A66597_CNTMD	0x0100	
#define	R8A66597_SHTNAK	0x0080	
#define	R8A66597_DIR	0x0010	
#define	R8A66597_EPNUM	0x000F	

#define	BUFSIZE		0x7C00	
#define	BUFNMB		0x007F	
#define	PIPE0BUF	256
#define	PIPExBUF	64

#define	MXPS		0x07FF	

#define	IFIS	0x1000	
#define	IITV	0x0007	

#define	BSTS	0x8000	
#define	INBUFM	0x4000	
#define	CSCLR	0x2000	
#define	CSSTS	0x1000	
#define	ATREPM	0x0400	
#define	ACLRM	0x0200	
#define	SQCLR	0x0100	
#define	SQSET	0x0080	
#define	SQMON	0x0040	
#define	PBUSY	0x0020	
#define	PID	0x0003	

#define	TRENB		0x0200	
#define	TRCLR		0x0100	

#define	TRNCNT		0xFFFF	

#define	UPPHUB		0x7800
#define	HUBPORT		0x0700
#define	USBSPD		0x00C0
#define	RTPORT		0x0001

#define CH0CFG		0x00
#define CH1CFG		0x04
#define CH0BA		0x10
#define CH1BA		0x14
#define CH0BBC		0x18
#define CH1BBC		0x1C
#define CH0CA		0x20
#define CH1CA		0x24
#define CH0CBC		0x28
#define CH1CBC		0x2C
#define CH0DEN		0x30
#define CH1DEN		0x34
#define DSTSCLR		0x38
#define DBUFCTRL	0x3C
#define DINTCTRL	0x40
#define DINTSTS		0x44
#define DINTSTSCLR	0x48
#define CH0SHCTRL	0x50
#define CH1SHCTRL	0x54

#define SENDBUFM	0x1000 
#define RCVENDM		0x0100 
#define LBA_WAIT	0x0030 

#define DEN		0x0001 

#define CH1STCLR	0x0002 
#define CH0STCLR	0x0001 

#define CH1BUFW		0x0200 
#define CH0BUFW		0x0100 
#define CH1BUFS		0x0002 
#define CH0BUFS		0x0001 

#define CH1ERRE		0x0200 
#define CH0ERRE		0x0100 
#define CH1ENDE		0x0002 
#define CH0ENDE		0x0001 

#define CH1ERRS		0x0200 
#define CH0ERRS		0x0100 
#define CH1ENDS		0x0002 
#define CH0ENDS		0x0001 

#define CH1ERRC		0x0200 
#define CH0ERRC		0x0100 
#define CH1ENDC		0x0002 
#define CH0ENDC		0x0001 

#endif 

