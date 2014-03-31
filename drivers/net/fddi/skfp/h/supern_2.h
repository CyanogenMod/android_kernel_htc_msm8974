/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/


#ifndef	_SUPERNET_
#define _SUPERNET_

#ifdef	PCI
#ifndef	SUPERNET_3
#define	SUPERNET_3
#endif
#define TAG
#endif

#define	MB	0xff
#define	MW	0xffff
#define	MD	0xffffffff

#define	FS_EI		(1<<2)
#define	FS_AI		(1<<1)
#define	FS_CI		(1<<0)

#define FS_MSVALID	(1<<15)		
#define FS_MSRABT	(1<<14)		
#define FS_SSRCRTG	(1<<12)		
#define FS_SEAC2	(FS_EI<<9)	
#define FS_SEAC1	(FS_AI<<9)	
#define FS_SEAC0	(FS_CI<<9)	
#define FS_SFRMERR	(1<<8)		
#define FS_SADRRG	(1<<7)		
#define FS_SFRMTY2	(1<<6)		
#define FS_SFRMTY1	(1<<5)		
#define FS_SFRMTY0	(1<<4)		
#define FS_ERFBB1	(1<<1)		
#define FS_ERFBB0	(1<<0)		

#define	FRM_SMT		(0)	
#define	FRM_LLCA	(1)
#define	FRM_IMPA	(2)	
#define	FRM_MAC		(4)	
#define	FRM_LLCS	(5)
#define	FRM_IMPS	(6)

#define RX_MSVALID	((long)1<<31)	
#define RX_MSRABT	((long)1<<30)	
#define RX_FS_E		((long)FS_SEAC2<<16)	
#define RX_FS_A		((long)FS_SEAC1<<16)	
#define RX_FS_C		((long)FS_SEAC0<<16)	
#define RX_FS_CRC	((long)FS_SFRMERR<<16)
#define RX_FS_ADDRESS	((long)FS_SADRRG<<16)	
#define RX_FS_MAC	((long)FS_SFRMTY2<<16)
#define RX_FS_SMT	((long)0<<16)		
#define RX_FS_IMPL	((long)FS_SFRMTY1<<16)
#define RX_FS_LLC	((long)FS_SFRMTY0<<16)

union rx_descr {
	struct {
#ifdef	LITTLE_ENDIAN
	unsigned	rx_length :16 ;	
	unsigned	rx_erfbb  :2 ;	
	unsigned	rx_reserv2:2 ;		
	unsigned	rx_sfrmty :3 ;	
	unsigned	rx_sadrrg :1 ;	
	unsigned	rx_sfrmerr:1 ;	
	unsigned	rx_seac0  :1 ;	
	unsigned	rx_seac1  :1 ;	
	unsigned	rx_seac2  :1 ;	
	unsigned	rx_ssrcrtg:1 ;	
	unsigned	rx_reserv1:1 ;		
	unsigned	rx_msrabt :1 ;	
	unsigned	rx_msvalid:1 ;	
#else
	unsigned	rx_msvalid:1 ;	
	unsigned	rx_msrabt :1 ;	
	unsigned	rx_reserv1:1 ;		
	unsigned	rx_ssrcrtg:1 ;	
	unsigned	rx_seac2  :1 ;	
	unsigned	rx_seac1  :1 ;	
	unsigned	rx_seac0  :1 ;	
	unsigned	rx_sfrmerr:1 ;	
	unsigned	rx_sadrrg :1 ;	
	unsigned	rx_sfrmty :3 ;	
	unsigned	rx_erfbb  :2 ;	
	unsigned	rx_reserv2:2 ;		
	unsigned	rx_length :16 ;	
#endif
	} r ;
	long	i ;
} ;

#define RD_S_ERFBB	0x00030000L	
#define RD_S_RES2	0x000c0000L	
#define RD_S_SFRMTY	0x00700000L	
#define RD_S_SADRRG	0x00800000L	
#define RD_S_SFRMERR	0x01000000L	
#define	RD_S_SEAC	0x0e000000L	
#define RD_S_SEAC0	0x02000000L	
#define RD_S_SEAC1	0x04000000L	
#define RD_S_SEAC2	0x08000000L	
#define RD_S_SSRCRTG	0x10000000L	
#define RD_S_RES1	0x20000000L	
#define RD_S_MSRABT	0x40000000L	
#define RD_S_MSVALID	0x80000000L	

#define	RD_STATUS	0xffff0000L
#define	RD_LENGTH	0x0000ffffL

#define RD_FRM_SMT	(unsigned long)(0<<20)     
#define RD_FRM_LLCA	(unsigned long)(1<<20)
#define RD_FRM_IMPA	(unsigned long)(2<<20)
#define RD_FRM_MAC	(unsigned long)(4<<20)     
#define RD_FRM_LLCS	(unsigned long)(5<<20)
#define RD_FRM_IMPS	(unsigned long)(6<<20)

#define TX_DESCRIPTOR	0x40000000L
#define TX_OFFSET_3	0x18000000L

#define TXP1	2

union tx_descr {
	struct {
#ifdef	LITTLE_ENDIAN
	unsigned	tx_length:16 ;	
	unsigned	tx_res	 :8 ;	
	unsigned	tx_xmtabt:1 ;	
	unsigned	tx_nfcs  :1 ;	
	unsigned	tx_xdone :1 ;	
	unsigned	tx_rpxm  :2 ;	
	unsigned	tx_pat1  :2 ;	
	unsigned	tx_more	 :1 ;	
#else
	unsigned	tx_more	 :1 ;	
	unsigned	tx_pat1  :2 ;	
	unsigned	tx_rpxm  :2 ;	
	unsigned	tx_xdone :1 ;	
	unsigned	tx_nfcs  :1 ;	
	unsigned	tx_xmtabt:1 ;	
	unsigned	tx_res	 :8 ;	
	unsigned	tx_length:16 ;	
#endif
	} t ;
	long	i ;
} ;

#define	TD_C_MORE	0x80000000L	
#define	TD_C_DESCR	0x60000000L	
#define	TD_C_TXFBB	0x18000000L	
#define	TD_C_XDONE	0x04000000L	
#define TD_C_NFCS	0x02000000L	
#define TD_C_XMTABT	0x01000000L	

#define	TD_C_LNCNU	0x0000ff00L	
#define TD_C_LNCNL	0x000000ffL
#define TD_C_LNCN	0x0000ffffL	
 
union tx_pointer {
	struct t {
#ifdef	LITTLE_ENDIAN
	unsigned	tp_pointer:16 ;	
	unsigned	tp_res	  :8 ;	
	unsigned	tp_pattern:8 ;	
#else
	unsigned	tp_pattern:8 ;	
	unsigned	tp_res	  :8 ;	
	unsigned	tp_pointer:16 ;	
#endif
	} t ;
	long	i ;
} ;

#define	TD_P_CNTRL	0xff000000L
#define TD_P_RPXU	0x0000ff00L
#define TD_P_RPXL	0x000000ffL
#define TD_P_RPX	0x0000ffffL


#define TX_PATTERN	0xa0
#define TX_POINTER_END	0xa0000000L
#define TX_INT_PATTERN	0xa0000000L

struct tx_queue {
	struct tx_queue *tq_next ;
	u_short tq_pack_offset ;	
	u_char  tq_pad[2] ;
} ;


#define FM_CMDREG1	0x00		
#define FM_CMDREG2	0x01		
#define FM_ST1U		0x00		
#define FM_ST1L		0x01		
#define FM_ST2U		0x02		
#define FM_ST2L		0x03		
#define FM_IMSK1U	0x04		
#define FM_IMSK1L	0x05		
#define FM_IMSK2U	0x06		
#define FM_IMSK2L	0x07		
#define FM_SAID		0x08		
#define FM_LAIM		0x09		
#define FM_LAIC		0x0a		
#define FM_LAIL		0x0b		
#define FM_SAGP		0x0c		
#define FM_LAGM		0x0d		
#define FM_LAGC		0x0e		
#define FM_LAGL		0x0f		
#define FM_MDREG1	0x10		
#define FM_STMCHN	0x11		
#define FM_MIR1		0x12		
#define FM_MIR0		0x13		
#define FM_TMAX		0x14		
#define FM_TVX		0x15		
#define FM_TRT		0x16		
#define FM_THT		0x17		
#define FM_TNEG		0x18		
#define FM_TMRS		0x19		
#define FM_TREQ0	0x1a		
#define FM_TREQ1	0x1b		
#define FM_PRI0		0x1c		
#define FM_PRI1		0x1d		
#define FM_PRI2		0x1e		
#define FM_TSYNC	0x1f		
#define FM_MDREG2	0x20		
#define FM_FRMTHR	0x21		
#define FM_EACB		0x22		
#define FM_EARV		0x23		
#define	FM_EARV1	FM_EARV

#define FM_EAS		0x24		
#define FM_EAA0		0x25		
#define FM_EAA1		0x26		
#define FM_EAA2		0x27		
#define FM_SACL		0x28		
#define FM_SABC		0x29		
#define FM_WPXSF	0x2a		
#define FM_RPXSF	0x2b		
#define FM_RPR		0x2d		
#define FM_WPR		0x2e		
#define FM_SWPR		0x2f		
 
#define FM_RPR1         FM_RPR   
#define FM_WPR1         FM_WPR 
#define FM_SWPR1        FM_SWPR

#define FM_WPXS		0x30		
#define FM_WPXA0	0x31		
#define FM_WPXA1	0x32		
#define FM_WPXA2	0x33		
#define FM_SWPXS	0x34		
#define FM_SWPXA0	0x35		
#define FM_SWPXA1	0x36		
#define FM_SWPXA2	0x37		
#define FM_RPXS		0x38		
#define FM_RPXA0	0x39		
#define FM_RPXA1	0x3a		
#define FM_RPXA2	0x3b		
#define FM_MARR		0x3c		
#define FM_MARW		0x3d		
#define FM_MDRU		0x3e		
#define FM_MDRL		0x3f		

#define FM_TMSYNC	0x40		
#define FM_FCNTR	0x41		
#define FM_LCNTR	0x42		
#define FM_ECNTR	0x43		

#define	FM_FSCNTR	0x44		
#define	FM_FRSELREG	0x45		

#define	FM_MDREG3	0x60		
#define	FM_ST3U		0x61		
#define	FM_ST3L		0x62		
#define	FM_IMSK3U	0x63		
#define	FM_IMSK3L	0x64		
#define	FM_IVR		0x65		
#define	FM_IMR		0x66		
#define	FM_RPR2		0x68		
#define	FM_WPR2		0x69		
#define	FM_SWPR2	0x6a		
#define	FM_EARV2	0x6b		
#define	FM_UNLCKDLY	0x6c		
					
					
#define	FM_LTDPA1	0x79		

#define	FM_AFCMD	0xb0		
#define	FM_AFSTAT	0xb2		
#define	FM_AFBIST	0xb4		
#define	FM_AFCOMP2	0xb6		
#define	FM_AFCOMP1	0xb8		
#define	FM_AFCOMP0	0xba		
#define	FM_AFMASK2	0xbc		
#define	FM_AFMASK1	0xbe		
#define	FM_AFMASK0	0xc0		
#define	FM_AFPERS	0xc2		

#define	FM_ORBIST	0xd0		
#define	FM_ORSTAT	0xd2		


#define FM_RES0		0x0001		
					
#define	FM_XMTINH_HOLD	0x0002		
					
#define	FM_HOFLXI	0x0003		
#define	FM_FULL_HALF	0x0004		
#define	FM_LOCKTX	0x0008		
#define FM_EXGPA0	0x0010		
#define FM_EXGPA1	0x0020		
#define FM_DISCRY	0x0040		
					
#define FM_SELRA	0x0080		

#define FM_ADDET	0x0700		
#define FM_MDAMA	(0<<8)		
#define FM_MDASAMA	(1<<8)		
#define	FM_MRNNSAFNMA	(2<<8)		
#define	FM_MRNNSAF	(3<<8)		
#define	FM_MDISRCV	(4<<8)		
#define	FM_MRES0	(5<<8)		
#define	FM_MLIMPROM	(6<<8)		
#define FM_MPROMISCOUS	(7<<8)		

#define FM_SELSA	0x0800		

#define FM_MMODE	0x7000		
#define FM_MINIT	(0<<12)		
#define FM_MMEMACT	(1<<12)		
#define FM_MONLINESP	(2<<12)		
#define FM_MONLINE	(3<<12)		
#define FM_MILOOP	(4<<12)		
#define FM_MRES1	(5<<12)		
#define FM_MRES2	(6<<12)		
#define FM_MELOOP	(7<<12)		

#define	FM_SNGLFRM	0x8000		
					

#define	MDR1INIT	(FM_MINIT | FM_MDAMA)

#define	FM_AFULL	0x000f		
#define	FM_RCVERR	0x0010		
#define	FM_SYMCTL	0x0020		
					
#define	FM_SYNPRQ	0x0040		
#define	FM_ENNPRQ	0x0080		
#define	FM_ENHSRQ	0x0100		
#define	FM_RXFBB01	0x0600		
#define	FM_LSB		0x0800		
#define	FM_PARITY	0x1000		
#define	FM_CHKPAR	0x2000		
#define	FM_STRPFCS	0x4000		
#define	FM_BMMODE	0x8000		
					

#define FM_STEFRMS	0x0001		
#define FM_STEFRMA0	0x0002		
#define FM_STEFRMA1	0x0004		
#define FM_STEFRMA2	0x0008		
					
#define FM_STECFRMS	0x0010		
					
#define FM_STECFRMA0	0x0020		
					
#define FM_STECFRMA1	0x0040		
					
#define FM_STECMDA1	0x0040		
#define FM_STECFRMA2	0x0080		
					
#define	FM_STEXDONS	0x0100		
#define	FM_STBFLA	0x0200		
#define	FM_STBFLS	0x0400		
#define	FM_STXABRS	0x0800		
#define	FM_STXABRA0	0x1000		
#define	FM_STXABRA1	0x2000		
#define	FM_STXABRA2	0x4000		
					
#define	FM_SXMTABT	0x8000		

#define FM_SQLCKS	0x0001		
#define FM_SQLCKA0	0x0002		
#define FM_SQLCKA1	0x0004		
#define FM_SQLCKA2	0x0008		
					
#define FM_STXINFLS	0x0010		
					
#define FM_STXINFLA0	0x0020		
					
#define FM_STXINFLA1	0x0040		
					
#define FM_STXINFLA2	0x0080		
					
#define FM_SPCEPDS	0x0100		
#define FM_SPCEPDA0	0x0200		
#define FM_SPCEPDA1	0x0400		
#define FM_SPCEPDA2	0x0800		
					
#define FM_STBURS	0x1000		
#define FM_STBURA0	0x2000		
#define FM_STBURA1	0x4000		
#define FM_STBURA2	0x8000		
					

#define FM_SOTRBEC	0x0001		
#define FM_SMYBEC	0x0002		
#define FM_SBEC		0x0004		
#define FM_SLOCLM	0x0008		
#define FM_SHICLM	0x0010		
#define FM_SMYCLM	0x0020		
#define FM_SCLM		0x0040		
#define FM_SERRSF	0x0080		
#define FM_SNFSLD	0x0100		
#define FM_SRFRCTOV	0x0200		
					
#define FM_SRCVFRM	0x0400		
					
#define FM_SRCVOVR	0x0800		
#define FM_SRBFL	0x1000		
#define FM_SRABT	0x2000		
#define FM_SRBMT	0x4000		
#define FM_SRCOMP	0x8000		

#define FM_SRES0	0x0001		
#define FM_SESTRIPTK	0x0001		
#define FM_STRTEXR	0x0002		
#define FM_SDUPCLM	0x0004		
#define FM_SSIFG	0x0008		
#define FM_SFRMCTR	0x0010		
#define FM_SERRCTR	0x0020		
#define FM_SLSTCTR	0x0040		
#define FM_SPHINV	0x0080		
#define FM_SADET	0x0100		
#define FM_SMISFRM	0x0200		
#define FM_STRTEXP	0x0400		
#define FM_STVXEXP	0x0800		
#define FM_STKISS	0x1000		
#define FM_STKERR	0x2000		
#define FM_SMULTDA	0x4000		
#define FM_SRNGOP	0x8000		

#define	FM_SRQUNLCK1	0x0001		
#define	FM_SRQUNLCK2	0x0002		
#define	FM_SRPERRQ1	0x0004		
#define	FM_SRPERRQ2	0x0008		
					
#define	FM_SRCVOVR2	0x0800		
#define	FM_SRBFL2	0x1000		
#define	FM_SRABT2	0x2000		
#define	FM_SRBMT2	0x4000		
#define	FM_SRCOMP2	0x8000		

#define	FM_AF_BIST_DONE		0x0001	
#define	FM_PLC_BIST_DONE	0x0002	
#define	FM_PDX_BIST_DONE	0x0004	
					
#define	FM_SICAMDAMAT		0x0010	
#define	FM_SICAMDAXACT		0x0020	
#define	FM_SICAMSAMAT		0x0040	
#define	FM_SICAMSAXACT		0x0080	

#define	FM_MDRTAG	0x0004		
#define	FM_SNPPND	0x0008		
#define	FM_TXSTAT	0x0070		
#define	FM_RCSTAT	0x0380		
#define	FM_TM01		0x0c00		
#define	FM_SIM		0x1000		
#define	FM_REV		0xe000		

#define	FM_MENRS	0x0001		
#define	FM_MENXS	0x0002		
#define	FM_MENXCT	0x0004		
#define	FM_MENAFULL	0x0008		
#define	FM_MEIND	0x0030		
#define	FM_MENQCTRL	0x0040		
#define	FM_MENRQAUNLCK	0x0080		
#define	FM_MENDAS	0x0100		
#define	FM_MENPLCCST	0x0200		
#define	FM_MENSGLINT	0x0400		
#define	FM_MENDRCV	0x0800		
#define	FM_MENFCLOC	0x3000		
#define	FM_MENTRCMD	0x4000		
#define	FM_MENTDLPBK	0x8000		

#define	FM_RECV1	0x000f		
#define	FM_RCV1_ALL	(0<<0)		
#define	FM_RCV1_LLC	(1<<0)		
#define	FM_RCV1_SMT	(2<<0)		
#define	FM_RCV1_NSMT	(3<<0)		
#define	FM_RCV1_IMP	(4<<0)		
#define	FM_RCV1_MAC	(5<<0)		
#define	FM_RCV1_SLLC	(6<<0)		
#define	FM_RCV1_ALLC	(7<<0)		
#define	FM_RCV1_VOID	(8<<0)		
#define	FM_RCV1_ALSMT	(9<<0)		
#define	FM_RECV2	0x00f0		
#define	FM_RCV2_ALL	(0<<4)		
#define	FM_RCV2_LLC	(1<<4)		
#define	FM_RCV2_SMT	(2<<4)		
#define	FM_RCV2_NSMT	(3<<4)		
#define	FM_RCV2_IMP	(4<<4)		
#define	FM_RCV2_MAC	(5<<4)		
#define	FM_RCV2_SLLC	(6<<4)		
#define	FM_RCV2_ALLC	(7<<4)		
#define	FM_RCV2_VOID	(8<<4)		
#define	FM_RCV2_ALSMT	(9<<4)		
#define	FM_ENXMTADSWAP	0x4000		
#define	FM_ENRCVADSWAP	0x8000		

#define	FM_INST		0x0007		
#define FM_IINV_CAM	(0<<0)		
#define FM_IWRITE_CAM	(1<<0)		
#define FM_IREAD_CAM	(2<<0)		
#define FM_IRUN_BIST	(3<<0)		
#define FM_IFIND	(4<<0)		
#define FM_IINV		(5<<0)		
#define FM_ISKIP	(6<<0)		
#define FM_ICL_SKIP	(7<<0)		

					
#define	FM_REV_NO	0x00e0		
#define	FM_BIST_DONE	0x0100		
#define	FM_EMPTY	0x0200		
#define	FM_ERROR	0x0400		
#define	FM_MULT		0x0800		
#define	FM_EXACT	0x1000		
#define	FM_FOUND	0x2000		
#define	FM_FULL		0x4000		
#define	FM_DONE		0x8000		

#define	AF_BIST_SIGNAT	0x0553		

#define	FM_VALID	0x0001		
#define	FM_DA		0x0002		
#define	FM_DAX		0x0004		
#define	FM_SA		0x0008		
#define	FM_SAX		0x0010		
#define	FM_SKIP		0x0020		

#define FM_IRESET	0x01		
#define FM_IRMEMWI	0x02		
#define FM_IRMEMWO	0x03		
#define FM_IIL		0x04		
#define FM_ICL		0x05		
#define FM_IBL		0x06		
#define FM_ILTVX	0x07		
#define FM_INRTM	0x08		
#define FM_IENTM	0x09		
#define FM_IERTM	0x0a		
#define FM_IRTM		0x0b		
#define FM_ISURT	0x0c		
#define FM_ISRT		0x0d		
#define FM_ISIM		0x0e		
#define FM_IESIM	0x0f		
#define FM_ICLLS	0x11		
#define FM_ICLLA0	0x12		
#define FM_ICLLA1	0x14		
#define FM_ICLLA2	0x18		
					
#define FM_ICLLR	0x20		
#define FM_ICLLR2	0x21		
#define FM_ITRXBUS	0x22		
#define FM_IDRXBUS	0x23		
#define FM_ICLLAL	0x3f		

#define FM_ITRS		0x01		
					
#define FM_ITRA0	0x02		
					
#define FM_ITRA1	0x04		
					
#define FM_ITRA2	0x08		
					
#define FM_IACTR	0x10		
#define FM_IRSTQ	0x20		
#define FM_ISTTB	0x30		
#define FM_IERSF	0x40		
					
#define	FM_ITR		0x50		



#define PL_CNTRL_A	0x00		
#define PL_CNTRL_B	0x01		
#define PL_INTR_MASK	0x02		
#define PL_XMIT_VECTOR	0x03		
#define PL_VECTOR_LEN	0x04		
#define PL_LE_THRESHOLD	0x05		
#define PL_C_MIN	0x06		
#define PL_TL_MIN	0x07		
#define PL_TB_MIN	0x08		
#define PL_T_OUT	0x09		
#define PL_CNTRL_C	0x0a		
#define PL_LC_LENGTH	0x0b		
#define PL_T_SCRUB	0x0c		
#define PL_NS_MAX	0x0d		
#define PL_TPC_LOAD_V	0x0e		
#define PL_TNE_LOAD_V	0x0f		
#define PL_STATUS_A	0x10		
#define PL_STATUS_B	0x11		
#define PL_TPC		0x12		
#define PL_TNE		0x13		
#define PL_CLK_DIV	0x14		
#define PL_BIST_SIGNAT	0x15		
#define PL_RCV_VECTOR	0x16		
#define PL_INTR_EVENT	0x17		
#define PL_VIOL_SYM_CTR	0x18		
#define PL_MIN_IDLE_CTR	0x19		
#define PL_LINK_ERR_CTR	0x1a		
#ifdef	MOT_ELM
#define	PL_T_FOT_ASS	0x1e		
#define	PL_T_FOT_DEASS	0x1f		
#endif	

#ifdef	MOT_ELM
#define	QELM_XBAR_W	0x80		
#define	QELM_XBAR_X	0x81		
#define	QELM_XBAR_Y	0x82		
#define	QELM_XBAR_Z	0x83		
#define	QELM_XBAR_P	0x84		
#define	QELM_XBAR_S	0x85		
#define	QELM_XBAR_R	0x86		
#define	QELM_WR_XBAR	0x87		
#define	QELM_CTR_W	0x88		
#define	QELM_CTR_X	0x89		
#define	QELM_CTR_Y	0x8a		
#define	QELM_CTR_Z	0x8b		
#define	QELM_INT_MASK	0x8c		
#define	QELM_INT_DATA	0x8d		
#define	QELM_ELMB	0x00		
#define	QELM_ELM_SIZE	0x20		
#endif	
#define	PL_RUN_BIST	0x0001		
#define	PL_RF_DISABLE	0x0002		
#define	PL_SC_REM_LOOP	0x0004		
#define	PL_SC_BYPASS	0x0008		
#define	PL_LM_LOC_LOOP	0x0010		
#define	PL_EB_LOC_LOOP	0x0020		
#define	PL_FOT_OFF	0x0040		
#define	PL_LOOPBACK	0x0080		
#define	PL_MINI_CTR_INT 0x0100		
#define	PL_VSYM_CTR_INT	0x0200		
#define	PL_ENA_PAR_CHK	0x0400		
#define	PL_REQ_SCRUB	0x0800		
#define	PL_TPC_16BIT	0x1000		
#define	PL_TNE_16BIT	0x2000		
#define	PL_NOISE_TIMER	0x4000		

#define	PL_PCM_CNTRL	0x0003		
#define	PL_PCM_NAF	(0)		
#define	PL_PCM_START	(1)		
#define	PL_PCM_TRACE	(2)		
#define	PL_PCM_STOP	(3)		

#define	PL_MAINT	0x0004		
#define	PL_LONG		0x0008		
#define	PL_PC_JOIN	0x0010		

#define	PL_PC_LOOP	0x0060		
#define	PL_NOLCT	(0<<5)		
#define	PL_TPDR		(1<<5)		
#define	PL_TIDLE	(2<<5)		
#define	PL_RLBP		(3<<5)		

#define	PL_CLASS_S	0x0080		

#define	PL_MAINT_LS	0x0700		
#define	PL_M_QUI0	(0<<8)		
#define	PL_M_IDLE	(1<<8)		
#define	PL_M_HALT	(2<<8)		
#define	PL_M_MASTR	(3<<8)		
#define	PL_M_QUI1	(4<<8)		
#define	PL_M_QUI2	(5<<8)		
#define	PL_M_TPDR	(6<<8)		
#define	PL_M_QUI3	(7<<8)		

#define	PL_MATCH_LS	0x7800		
#define	PL_I_ANY	(0<<11)		
#define	PL_I_IDLE	(1<<11)		
#define	PL_I_HALT	(2<<11)		
#define	PL_I_MASTR	(4<<11)		
#define	PL_I_QUIET	(8<<11)		

#define	PL_CONFIG_CNTRL	0x8000		

#define PL_C_CIPHER_ENABLE	(1<<0)	
#define PL_C_CIPHER_LPBCK	(1<<1)	
#define PL_C_SDOFF_ENABLE	(1<<6)	
#define PL_C_SDON_ENABLE	(1<<7)	
#ifdef	MOT_ELM
#define PL_C_FOTOFF_CTRL	(3<<2)	
#define PL_C_FOTOFF_TIM		(0<<2)	
#define PL_C_FOTOFF_INA		(2<<2)	
#define PL_C_FOTOFF_ACT		(3<<2)	
#define PL_C_FOTOFF_SRCE	(1<<4)	
#define	PL_C_RXDATA_EN		(1<<5)	
#define	PL_C_SDNRZEN		(1<<8)	
#else	
#define PL_C_FOTOFF_CTRL	(3<<8)	
#define PL_C_FOTOFF_0		(0<<8)	
#define PL_C_FOTOFF_30		(1<<8)	
#define PL_C_FOTOFF_50		(2<<8)	
#define PL_C_FOTOFF_NEVER	(3<<8)	
#define PL_C_SDON_TIMER		(3<<10)	
#define PL_C_SDON_084		(0<<10)	
#define PL_C_SDON_132		(1<<10)	
#define PL_C_SDON_252		(2<<10)	
#define PL_C_SDON_512		(3<<10)	
#define PL_C_SOFF_TIMER		(3<<12)	
#define PL_C_SOFF_076		(0<<12)	
#define PL_C_SOFF_132		(1<<12)	
#define PL_C_SOFF_252		(2<<12)	
#define PL_C_SOFF_512		(3<<12)	
#define PL_C_TSEL		(3<<14)	
#endif	

#ifdef	MOT_ELM
#define PLC_INT_MASK	0xc000		
#define PLC_INT_C	0x0000		
#define PLC_INT_CAMEL	0x4000		
#define PLC_INT_QE	0x8000		
#define PLC_REV_MASK	0x3800		
#define PLC_REVISION_B	0x0000		
#define PLC_REVISION_QA	0x0800		
#else	
#define PLC_REV_MASK	0xf800		
#define PLC_REVISION_A	0x0000		
#define PLC_REVISION_S	0xf800		
#define PLC_REV_SN3	0x7800		
#endif	
#define	PL_SYM_PR_CTR	0x0007		
#define	PL_UNKN_LINE_ST	0x0008		
#define	PL_LSM_STATE	0x0010		

#define	PL_LINE_ST	0x00e0		
#define	PL_L_NLS	(0<<5)		
#define	PL_L_ALS	(1<<5)		
#define	PL_L_UND	(2<<5)		
#define	PL_L_ILS4	(3<<5)		
#define	PL_L_QLS	(4<<5)		
#define	PL_L_MLS	(5<<5)		
#define	PL_L_HLS	(6<<5)		
#define	PL_L_ILS16	(7<<5)		

#define	PL_PREV_LINE_ST	0x0300		
#define	PL_P_QLS	(0<<8)		
#define	PL_P_MLS	(1<<8)		
#define	PL_P_HLS	(2<<8)		
#define	PL_P_ILS16	(3<<8)		

#define	PL_SIGNAL_DET	0x0400		


#define	PL_BREAK_REASON	0x0007		
#define	PL_B_NOT	(0)		
#define	PL_B_PCS	(1)		
#define	PL_B_TPC	(2)		
#define	PL_B_TNE	(3)		
#define	PL_B_QLS	(4)		
#define	PL_B_ILS	(5)		
#define	PL_B_HLS	(6)		

#define	PL_TCF		0x0008		
#define	PL_RCF		0x0010		
#define	PL_LSF		0x0020		
#define	PL_PCM_SIGNAL	0x0040		/* indic. that XMIT_VECTOR hb.written*/

#define	PL_PCM_STATE	0x0780		
#define	PL_PC0		(0<<7)		
#define	PL_PC1		(1<<7)		
#define	PL_PC2		(2<<7)		
#define	PL_PC3		(3<<7)		
#define	PL_PC4		(4<<7)		
#define	PL_PC5		(5<<7)		
#define	PL_PC6		(6<<7)		
#define	PL_PC7		(7<<7)		
#define	PL_PC8		(8<<7)		
#define	PL_PC9		(9<<7)		

#define	PL_PCI_SCRUB	0x0800		

#define	PL_PCI_STATE	0x3000		
#define	PL_CI_REMV	(0<<12)		
#define	PL_CI_ISCR	(1<<12)		
#define	PL_CI_RSCR	(2<<12)		
#define	PL_CI_INS	(3<<12)		

#define	PL_RF_STATE	0xc000		
#define	PL_RF_REPT	(0<<14)		
#define	PL_RF_IDLE	(1<<14)		
#define	PL_RF_HALT1	(2<<14)		
#define	PL_RF_HALT2	(3<<14)		


#define	PL_PARITY_ERR	0x0001		
#define	PL_LS_MATCH	0x0002		
#define	PL_PCM_CODE	0x0004		
#define	PL_TRACE_PROP	0x0008		
#define	PL_SELF_TEST	0x0010		
#define	PL_PCM_BREAK	0x0020		
#define	PL_PCM_ENABLED	0x0040		
#define	PL_TPC_EXPIRED	0x0080		
#define	PL_TNE_EXPIRED	0x0100		
#define	PL_EBUF_ERR	0x0200		
#define	PL_PHYINV	0x0400		
#define	PL_VSYM_CTR	0x0800		
#define	PL_MINI_CTR	0x1000		
#define	PL_LE_CTR	0x2000		
#define	PL_LSDO		0x4000		
#define	PL_NP_ERR	0x8000		


#ifdef	MOT_ELM
#define	QELM_XOUT_IDLE	0x0000		
#define	QELM_XOUT_P	0x0001		
#define	QELM_XOUT_S	0x0002		
#define	QELM_XOUT_R	0x0003		
#define	QELM_XOUT_W	0x0004		
#define	QELM_XOUT_X	0x0005		
#define	QELM_XOUT_Y	0x0006		
#define	QELM_XOUT_Z	0x0007		

#define	QELM_NP_ERR	(1<<15)		
#define	QELM_COUNT_Z	(1<<7)		
#define	QELM_COUNT_Y	(1<<6)		
#define	QELM_COUNT_X	(1<<5)		
#define	QELM_COUNT_W	(1<<4)		
#define	QELM_ELM_Z	(1<<3)		
#define	QELM_ELM_Y	(1<<2)		
#define	QELM_ELM_X	(1<<1)		
#define	QELM_ELM_W	(1<<0)		
#endif	
#define	TP_C_MIN	0xff9c	
#define	TP_TL_MIN	0xfff0	
#define	TP_TB_MIN	0xff10	
#define	TP_T_OUT	0xd9db	
#define	TP_LC_LENGTH	0xf676	
#define	TP_LC_LONGLN	0xa0a2	
#define	TP_T_SCRUB	0xff6d	
#define	TP_NS_MAX	0xf021	

#define PLC_BIST	0x6ecd		
#define PLCS_BIST	0x5b6b 		
#define	PLC_ELM_B_BIST	0x6ecd		
#define	PLC_ELM_D_BIST	0x5b6b		
#define	PLC_CAM_A_BIST	0x9e75		
#define	PLC_CAM_B_BIST	0x5b6b		
#define	PLC_IFD_A_BIST	0x9e75		
#define	PLC_IFD_B_BIST	0x5b6b		
#define	PLC_QELM_A_BIST	0x5b6b		


#define	RQ_NOT		0		
#define	RQ_RES		1		
#define	RQ_SFW		2		
#define	RQ_RRQ		3		
#define	RQ_WSQ		4		
#define	RQ_WA0		5		
#define	RQ_WA1		6		
#define	RQ_WA2		7		

#define	SZ_LONG		(sizeof(long))

#define	COMPLREF	((u_long)32*256*256)	
#define MSTOBCLK(x)	((u_long)(x)*12500L)
#define MSTOTVX(x)	(((u_long)(x)*1000L)/80/255)

#endif	
