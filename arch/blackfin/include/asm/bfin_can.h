/*
 * bfin_can.h - interface to Blackfin CANs
 *
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __ASM_BFIN_CAN_H__
#define __ASM_BFIN_CAN_H__

#define TRANSMIT_CHL            24
#define RECEIVE_STD_CHL         0
#define RECEIVE_EXT_CHL         4
#define RECEIVE_RTR_CHL         8
#define RECEIVE_EXT_RTR_CHL     12
#define MAX_CHL_NUMBER          32

#define __BFP(m) u16 m; u16 __pad_##m

struct bfin_can_mask_regs {
	__BFP(aml);
	__BFP(amh);
};

struct bfin_can_channel_regs {
	
	u16 data[8];
	__BFP(dlc);
	__BFP(tsv);
	__BFP(id0);
	__BFP(id1);
};

struct bfin_can_regs {
	__BFP(mc1);           
	__BFP(md1);           
	__BFP(trs1);          
	__BFP(trr1);          
	__BFP(ta1);           
	__BFP(aa1);           
	__BFP(rmp1);          
	__BFP(rml1);          
	__BFP(mbtif1);        
	__BFP(mbrif1);        
	__BFP(mbim1);         
	__BFP(rfh1);          
	__BFP(opss1);         
	u32 __pad1[3];
	__BFP(mc2);           
	__BFP(md2);           
	__BFP(trs2);          
	__BFP(trr2);          
	__BFP(ta2);           
	__BFP(aa2);           
	__BFP(rmp2);          
	__BFP(rml2);          
	__BFP(mbtif2);        
	__BFP(mbrif2);        
	__BFP(mbim2);         
	__BFP(rfh2);          
	__BFP(opss2);         
	u32 __pad2[3];
	__BFP(clock);         
	__BFP(timing);        
	__BFP(debug);         
	__BFP(status);        
	__BFP(cec);           
	__BFP(gis);           
	__BFP(gim);           
	__BFP(gif);           
	__BFP(control);       
	__BFP(intr);          
	__BFP(version);       
	__BFP(mbtd);          
	__BFP(ewr);           
	__BFP(esr);           
	u32 __pad3[2];
	__BFP(ucreg);         
	__BFP(uccnt);         
	__BFP(ucrc);          
	__BFP(uccnf);         
	u32 __pad4[1];
	__BFP(version2);      
	u32 __pad5[10];

	struct bfin_can_mask_regs msk[MAX_CHL_NUMBER];    
	struct bfin_can_channel_regs chl[MAX_CHL_NUMBER]; 
};

#undef __BFP

#define SRS			0x0001	
#define DNM			0x0002	
#define ABO			0x0004	
#define TXPRIO		0x0008	
#define WBA			0x0010	
#define SMR			0x0020	
#define CSR			0x0040	
#define CCR			0x0080	

#define WT			0x0001	
#define WR			0x0002	
#define EP			0x0004	
#define EBO			0x0008	
#define SMA			0x0020	
#define CSA			0x0040	
#define CCA			0x0080	
#define MBPTR		0x1F00	
#define TRM			0x4000	
#define REC			0x8000	

#define BRP			0x03FF	

#define TSEG1		0x000F	
#define TSEG2		0x0070	
#define SAM			0x0080	
#define SJW			0x0300	

#define DEC			0x0001	
#define DRI			0x0002	
#define DTO			0x0004	
#define DIL			0x0008	
#define MAA			0x0010	
#define MRB			0x0020	
#define CDE			0x8000	

#define RXECNT		0x00FF	
#define TXECNT		0xFF00	

#define MBRIRQ	0x0001	
#define MBTIRQ	0x0002	
#define GIRQ		0x0004	
#define SMACK		0x0008	
#define CANTX		0x0040	
#define CANRX		0x0080	

#define DFC			0xFFFF	
#define EXTID_LO	0xFFFF	
#define EXTID_HI	0x0003	
#define BASEID		0x1FFC	
#define IDE			0x2000	
#define RTR			0x4000	
#define AME			0x8000	

#define TSV			0xFFFF	

#define DLC			0x000F	

#define DFM			0xFFFF	
#define EXTID_LO	0xFFFF	
#define EXTID_HI	0x0003	
#define BASEID		0x1FFC	
#define AMIDE		0x2000	
#define FMD			0x4000	
#define FDF			0x8000	

#define MC0			0x0001	
#define MC1			0x0002	
#define MC2			0x0004	
#define MC3			0x0008	
#define MC4			0x0010	
#define MC5			0x0020	
#define MC6			0x0040	
#define MC7			0x0080	
#define MC8			0x0100	
#define MC9			0x0200	
#define MC10		0x0400	
#define MC11		0x0800	
#define MC12		0x1000	
#define MC13		0x2000	
#define MC14		0x4000	
#define MC15		0x8000	

#define MC16		0x0001	
#define MC17		0x0002	
#define MC18		0x0004	
#define MC19		0x0008	
#define MC20		0x0010	
#define MC21		0x0020	
#define MC22		0x0040	
#define MC23		0x0080	
#define MC24		0x0100	
#define MC25		0x0200	
#define MC26		0x0400	
#define MC27		0x0800	
#define MC28		0x1000	
#define MC29		0x2000	
#define MC30		0x4000	
#define MC31		0x8000	

#define MD0			0x0001	
#define MD1			0x0002	
#define MD2			0x0004	
#define MD3			0x0008	
#define MD4			0x0010	
#define MD5			0x0020	
#define MD6			0x0040	
#define MD7			0x0080	
#define MD8			0x0100	
#define MD9			0x0200	
#define MD10		0x0400	
#define MD11		0x0800	
#define MD12		0x1000	
#define MD13		0x2000	
#define MD14		0x4000	
#define MD15		0x8000	

#define MD16		0x0001	
#define MD17		0x0002	
#define MD18		0x0004	
#define MD19		0x0008	
#define MD20		0x0010	
#define MD21		0x0020	
#define MD22		0x0040	
#define MD23		0x0080	
#define MD24		0x0100	
#define MD25		0x0200	
#define MD26		0x0400	
#define MD27		0x0800	
#define MD28		0x1000	
#define MD29		0x2000	
#define MD30		0x4000	
#define MD31		0x8000	

#define RMP0		0x0001	
#define RMP1		0x0002	
#define RMP2		0x0004	
#define RMP3		0x0008	
#define RMP4		0x0010	
#define RMP5		0x0020	
#define RMP6		0x0040	
#define RMP7		0x0080	
#define RMP8		0x0100	
#define RMP9		0x0200	
#define RMP10		0x0400	
#define RMP11		0x0800	
#define RMP12		0x1000	
#define RMP13		0x2000	
#define RMP14		0x4000	
#define RMP15		0x8000	

#define RMP16		0x0001	
#define RMP17		0x0002	
#define RMP18		0x0004	
#define RMP19		0x0008	
#define RMP20		0x0010	
#define RMP21		0x0020	
#define RMP22		0x0040	
#define RMP23		0x0080	
#define RMP24		0x0100	
#define RMP25		0x0200	
#define RMP26		0x0400	
#define RMP27		0x0800	
#define RMP28		0x1000	
#define RMP29		0x2000	
#define RMP30		0x4000	
#define RMP31		0x8000	

#define RML0		0x0001	
#define RML1		0x0002	
#define RML2		0x0004	
#define RML3		0x0008	
#define RML4		0x0010	
#define RML5		0x0020	
#define RML6		0x0040	
#define RML7		0x0080	
#define RML8		0x0100	
#define RML9		0x0200	
#define RML10		0x0400	
#define RML11		0x0800	
#define RML12		0x1000	
#define RML13		0x2000	
#define RML14		0x4000	
#define RML15		0x8000	

#define RML16		0x0001	
#define RML17		0x0002	
#define RML18		0x0004	
#define RML19		0x0008	
#define RML20		0x0010	
#define RML21		0x0020	
#define RML22		0x0040	
#define RML23		0x0080	
#define RML24		0x0100	
#define RML25		0x0200	
#define RML26		0x0400	
#define RML27		0x0800	
#define RML28		0x1000	
#define RML29		0x2000	
#define RML30		0x4000	
#define RML31		0x8000	

#define OPSS0		0x0001	
#define OPSS1		0x0002	
#define OPSS2		0x0004	
#define OPSS3		0x0008	
#define OPSS4		0x0010	
#define OPSS5		0x0020	
#define OPSS6		0x0040	
#define OPSS7		0x0080	
#define OPSS8		0x0100	
#define OPSS9		0x0200	
#define OPSS10		0x0400	
#define OPSS11		0x0800	
#define OPSS12		0x1000	
#define OPSS13		0x2000	
#define OPSS14		0x4000	
#define OPSS15		0x8000	

#define OPSS16		0x0001	
#define OPSS17		0x0002	
#define OPSS18		0x0004	
#define OPSS19		0x0008	
#define OPSS20		0x0010	
#define OPSS21		0x0020	
#define OPSS22		0x0040	
#define OPSS23		0x0080	
#define OPSS24		0x0100	
#define OPSS25		0x0200	
#define OPSS26		0x0400	
#define OPSS27		0x0800	
#define OPSS28		0x1000	
#define OPSS29		0x2000	
#define OPSS30		0x4000	
#define OPSS31		0x8000	

#define TRR0		0x0001	
#define TRR1		0x0002	
#define TRR2		0x0004	
#define TRR3		0x0008	
#define TRR4		0x0010	
#define TRR5		0x0020	
#define TRR6		0x0040	
#define TRR7		0x0080	
#define TRR8		0x0100	
#define TRR9		0x0200	
#define TRR10		0x0400	
#define TRR11		0x0800	
#define TRR12		0x1000	
#define TRR13		0x2000	
#define TRR14		0x4000	
#define TRR15		0x8000	

#define TRR16		0x0001	
#define TRR17		0x0002	
#define TRR18		0x0004	
#define TRR19		0x0008	
#define TRR20		0x0010	
#define TRR21		0x0020	
#define TRR22		0x0040	
#define TRR23		0x0080	
#define TRR24		0x0100	
#define TRR25		0x0200	
#define TRR26		0x0400	
#define TRR27		0x0800	
#define TRR28		0x1000	
#define TRR29		0x2000	
#define TRR30		0x4000	
#define TRR31		0x8000	

#define TRS0		0x0001	
#define TRS1		0x0002	
#define TRS2		0x0004	
#define TRS3		0x0008	
#define TRS4		0x0010	
#define TRS5		0x0020	
#define TRS6		0x0040	
#define TRS7		0x0080	
#define TRS8		0x0100	
#define TRS9		0x0200	
#define TRS10		0x0400	
#define TRS11		0x0800	
#define TRS12		0x1000	
#define TRS13		0x2000	
#define TRS14		0x4000	
#define TRS15		0x8000	

#define TRS16		0x0001	
#define TRS17		0x0002	
#define TRS18		0x0004	
#define TRS19		0x0008	
#define TRS20		0x0010	
#define TRS21		0x0020	
#define TRS22		0x0040	
#define TRS23		0x0080	
#define TRS24		0x0100	
#define TRS25		0x0200	
#define TRS26		0x0400	
#define TRS27		0x0800	
#define TRS28		0x1000	
#define TRS29		0x2000	
#define TRS30		0x4000	
#define TRS31		0x8000	

#define AA0			0x0001	
#define AA1			0x0002	
#define AA2			0x0004	
#define AA3			0x0008	
#define AA4			0x0010	
#define AA5			0x0020	
#define AA6			0x0040	
#define AA7			0x0080	
#define AA8			0x0100	
#define AA9			0x0200	
#define AA10		0x0400	
#define AA11		0x0800	
#define AA12		0x1000	
#define AA13		0x2000	
#define AA14		0x4000	
#define AA15		0x8000	

#define AA16		0x0001	
#define AA17		0x0002	
#define AA18		0x0004	
#define AA19		0x0008	
#define AA20		0x0010	
#define AA21		0x0020	
#define AA22		0x0040	
#define AA23		0x0080	
#define AA24		0x0100	
#define AA25		0x0200	
#define AA26		0x0400	
#define AA27		0x0800	
#define AA28		0x1000	
#define AA29		0x2000	
#define AA30		0x4000	
#define AA31		0x8000	

#define TA0			0x0001	
#define TA1			0x0002	
#define TA2			0x0004	
#define TA3			0x0008	
#define TA4			0x0010	
#define TA5			0x0020	
#define TA6			0x0040	
#define TA7			0x0080	
#define TA8			0x0100	
#define TA9			0x0200	
#define TA10		0x0400	
#define TA11		0x0800	
#define TA12		0x1000	
#define TA13		0x2000	
#define TA14		0x4000	
#define TA15		0x8000	

#define TA16		0x0001	
#define TA17		0x0002	
#define TA18		0x0004	
#define TA19		0x0008	
#define TA20		0x0010	
#define TA21		0x0020	
#define TA22		0x0040	
#define TA23		0x0080	
#define TA24		0x0100	
#define TA25		0x0200	
#define TA26		0x0400	
#define TA27		0x0800	
#define TA28		0x1000	
#define TA29		0x2000	
#define TA30		0x4000	
#define TA31		0x8000	

#define TDPTR		0x001F	
#define TDA			0x0040	
#define TDR			0x0080	

#define RFH0		0x0001	
#define RFH1		0x0002	
#define RFH2		0x0004	
#define RFH3		0x0008	
#define RFH4		0x0010	
#define RFH5		0x0020	
#define RFH6		0x0040	
#define RFH7		0x0080	
#define RFH8		0x0100	
#define RFH9		0x0200	
#define RFH10		0x0400	
#define RFH11		0x0800	
#define RFH12		0x1000	
#define RFH13		0x2000	
#define RFH14		0x4000	
#define RFH15		0x8000	

#define RFH16		0x0001	
#define RFH17		0x0002	
#define RFH18		0x0004	
#define RFH19		0x0008	
#define RFH20		0x0010	
#define RFH21		0x0020	
#define RFH22		0x0040	
#define RFH23		0x0080	
#define RFH24		0x0100	
#define RFH25		0x0200	
#define RFH26		0x0400	
#define RFH27		0x0800	
#define RFH28		0x1000	
#define RFH29		0x2000	
#define RFH30		0x4000	
#define RFH31		0x8000	

#define MBTIF0		0x0001	
#define MBTIF1		0x0002	
#define MBTIF2		0x0004	
#define MBTIF3		0x0008	
#define MBTIF4		0x0010	
#define MBTIF5		0x0020	
#define MBTIF6		0x0040	
#define MBTIF7		0x0080	
#define MBTIF8		0x0100	
#define MBTIF9		0x0200	
#define MBTIF10		0x0400	
#define MBTIF11		0x0800	
#define MBTIF12		0x1000	
#define MBTIF13		0x2000	
#define MBTIF14		0x4000	
#define MBTIF15		0x8000	

#define MBTIF16		0x0001	
#define MBTIF17		0x0002	
#define MBTIF18		0x0004	
#define MBTIF19		0x0008	
#define MBTIF20		0x0010	
#define MBTIF21		0x0020	
#define MBTIF22		0x0040	
#define MBTIF23		0x0080	
#define MBTIF24		0x0100	
#define MBTIF25		0x0200	
#define MBTIF26		0x0400	
#define MBTIF27		0x0800	
#define MBTIF28		0x1000	
#define MBTIF29		0x2000	
#define MBTIF30		0x4000	
#define MBTIF31		0x8000	

#define MBRIF0		0x0001	
#define MBRIF1		0x0002	
#define MBRIF2		0x0004	
#define MBRIF3		0x0008	
#define MBRIF4		0x0010	
#define MBRIF5		0x0020	
#define MBRIF6		0x0040	
#define MBRIF7		0x0080	
#define MBRIF8		0x0100	
#define MBRIF9		0x0200	
#define MBRIF10		0x0400	
#define MBRIF11		0x0800	
#define MBRIF12		0x1000	
#define MBRIF13		0x2000	
#define MBRIF14		0x4000	
#define MBRIF15		0x8000	

#define MBRIF16		0x0001	
#define MBRIF17		0x0002	
#define MBRIF18		0x0004	
#define MBRIF19		0x0008	
#define MBRIF20		0x0010	
#define MBRIF21		0x0020	
#define MBRIF22		0x0040	
#define MBRIF23		0x0080	
#define MBRIF24		0x0100	
#define MBRIF25		0x0200	
#define MBRIF26		0x0400	
#define MBRIF27		0x0800	
#define MBRIF28		0x1000	
#define MBRIF29		0x2000	
#define MBRIF30		0x4000	
#define MBRIF31		0x8000	

#define MBIM0		0x0001	
#define MBIM1		0x0002	
#define MBIM2		0x0004	
#define MBIM3		0x0008	
#define MBIM4		0x0010	
#define MBIM5		0x0020	
#define MBIM6		0x0040	
#define MBIM7		0x0080	
#define MBIM8		0x0100	
#define MBIM9		0x0200	
#define MBIM10		0x0400	
#define MBIM11		0x0800	
#define MBIM12		0x1000	
#define MBIM13		0x2000	
#define MBIM14		0x4000	
#define MBIM15		0x8000	

#define MBIM16		0x0001	
#define MBIM17		0x0002	
#define MBIM18		0x0004	
#define MBIM19		0x0008	
#define MBIM20		0x0010	
#define MBIM21		0x0020	
#define MBIM22		0x0040	
#define MBIM23		0x0080	
#define MBIM24		0x0100	
#define MBIM25		0x0200	
#define MBIM26		0x0400	
#define MBIM27		0x0800	
#define MBIM28		0x1000	
#define MBIM29		0x2000	
#define MBIM30		0x4000	
#define MBIM31		0x8000	

#define EWTIM		0x0001	
#define EWRIM		0x0002	
#define EPIM		0x0004	
#define BOIM		0x0008	
#define WUIM		0x0010	
#define UIAIM		0x0020	
#define AAIM		0x0040	
#define RMLIM		0x0080	
#define UCEIM		0x0100	
#define EXTIM		0x0200	
#define ADIM		0x0400	

#define EWTIS		0x0001	
#define EWRIS		0x0002	
#define EPIS		0x0004	
#define BOIS		0x0008	
#define WUIS		0x0010	
#define UIAIS		0x0020	
#define AAIS		0x0040	
#define RMLIS		0x0080	
#define UCEIS		0x0100	
#define EXTIS		0x0200	
#define ADIS		0x0400	

#define EWTIF		0x0001	
#define EWRIF		0x0002	
#define EPIF		0x0004	
#define BOIF		0x0008	
#define WUIF		0x0010	
#define UIAIF		0x0020	
#define AAIF		0x0040	
#define RMLIF		0x0080	
#define UCEIF		0x0100	
#define EXTIF		0x0200	
#define ADIF		0x0400	

#define UCCNF		0x000F	
#define UC_STAMP	0x0001	
#define UC_WDOG		0x0002	
#define UC_AUTOTX	0x0003	
#define UC_ERROR	0x0006	
#define UC_OVER		0x0007	
#define UC_LOST		0x0008	
#define UC_AA		0x0009	
#define UC_TA		0x000A	
#define UC_REJECT	0x000B	
#define UC_RML		0x000C	
#define UC_RX		0x000D	
#define UC_RMP		0x000E	
#define UC_ALL		0x000F	
#define UCRC		0x0020	
#define UCCT		0x0040	
#define UCE			0x0080	

#define ACKE		0x0004	
#define SER			0x0008	
#define CRCE		0x0010	
#define SA0			0x0020	
#define BEF			0x0040	
#define FER			0x0080	

#define EWLREC		0x00FF	
#define EWLTEC		0xFF00	

#endif
