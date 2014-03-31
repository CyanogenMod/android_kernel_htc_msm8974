/*
 *  Amiga Linux/m68k Ariadne Ethernet Driver
 *
 *  © Copyright 1995 by Geert Uytterhoeven (geert@linux-m68k.org)
 *			Peter De Schrijver
 *		       (Peter.DeSchrijver@linux.cc.kuleuven.ac.be)
 *
 *  ----------------------------------------------------------------------------------
 *
 *  This program is based on
 *
 *	lance.c:	An AMD LANCE ethernet driver for linux.
 *			Written 1993-94 by Donald Becker.
 *
 *	Am79C960:	PCnet(tm)-ISA Single-Chip Ethernet Controller
 *			Advanced Micro Devices
 *			Publication #16907, Rev. B, Amendment/0, May 1994
 *
 *	MC68230:	Parallel Interface/Timer (PI/T)
 *			Motorola Semiconductors, December, 1983
 *
 *  ----------------------------------------------------------------------------------
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 *
 *  ----------------------------------------------------------------------------------
 *
 *  The Ariadne is a Zorro-II board made by Village Tronic. It contains:
 *
 *	- an Am79C960 PCnet-ISA Single-Chip Ethernet Controller with both
 *	  10BASE-2 (thin coax) and 10BASE-T (UTP) connectors
 *
 *	- an MC68230 Parallel Interface/Timer configured as 2 parallel ports
 */



struct Am79C960 {
    volatile u_short AddressPROM[8];
				
    volatile u_short RDP;	
    volatile u_short RAP;	
    volatile u_short Reset;	
    volatile u_short IDP;	
};



#define CSR0		0x0000	
#define CSR1		0x0100	
#define CSR2		0x0200	
#define CSR3		0x0300	
#define CSR4		0x0400	
#define CSR6		0x0600	
#define CSR8		0x0800	
#define CSR9		0x0900	
#define CSR10		0x0a00	
#define CSR11		0x0b00	
#define CSR12		0x0c00	
#define CSR13		0x0d00	
#define CSR14		0x0e00	
#define CSR15		0x0f00	
#define CSR16		0x1000	
#define CSR17		0x1100	
#define CSR18		0x1200	
#define CSR19		0x1300	
#define CSR20		0x1400	
#define CSR21		0x1500	
#define CSR22		0x1600	
#define CSR23		0x1700	
#define CSR24		0x1800	
#define CSR25		0x1900	
#define CSR26		0x1a00	
#define CSR27		0x1b00	
#define CSR28		0x1c00	
#define CSR29		0x1d00	
#define CSR30		0x1e00	
#define CSR31		0x1f00	
#define CSR32		0x2000	
#define CSR33		0x2100	
#define CSR34		0x2200	
#define CSR35		0x2300	
#define CSR36		0x2400	
#define CSR37		0x2500	
#define CSR38		0x2600	
#define CSR39		0x2700	
#define CSR40		0x2800	
#define CSR41		0x2900	
#define CSR42		0x2a00	
#define CSR43		0x2b00	
#define CSR44		0x2c00	
#define CSR45		0x2d00	
#define CSR46		0x2e00	
#define CSR47		0x2f00	
#define CSR48		0x3000	
#define CSR49		0x3100	
#define CSR50		0x3200	
#define CSR51		0x3300	
#define CSR52		0x3400	
#define CSR53		0x3500	
#define CSR54		0x3600	
#define CSR55		0x3700	
#define CSR56		0x3800	
#define CSR57		0x3900	
#define CSR58		0x3a00	
#define CSR59		0x3b00	
#define CSR60		0x3c00	
#define CSR61		0x3d00	
#define CSR62		0x3e00	
#define CSR63		0x3f00	
#define CSR64		0x4000	
#define CSR65		0x4100	
#define CSR66		0x4200	
#define CSR67		0x4300	
#define CSR68		0x4400	
#define CSR69		0x4500	
#define CSR70		0x4600	
#define CSR71		0x4700	
#define CSR72		0x4800	
#define CSR74		0x4a00	
#define CSR76		0x4c00	
#define CSR78		0x4e00	
#define CSR80		0x5000	
#define CSR82		0x5200	
#define CSR84		0x5400	
#define CSR85		0x5500	
#define CSR86		0x5600	
#define CSR88		0x5800	
#define CSR89		0x5900	
#define CSR92		0x5c00	
#define CSR94		0x5e00	
#define CSR96		0x6000	
#define CSR97		0x6100	
#define CSR98		0x6200	
#define CSR99		0x6300	
#define CSR104		0x6800	
#define CSR105		0x6900	
#define CSR108		0x6c00	
#define CSR109		0x6d00	
#define CSR112		0x7000	
#define CSR114		0x7200	
#define CSR124		0x7c00	



#define ISACSR0		0x0000	
#define ISACSR1		0x0100	
#define ISACSR2		0x0200	
#define ISACSR4		0x0400	
#define ISACSR5		0x0500	
#define ISACSR6		0x0600	
#define ISACSR7		0x0700	



#define ERR		0x0080	
#define BABL		0x0040	
#define CERR		0x0020	
#define MISS		0x0010	
#define MERR		0x0008	
#define RINT		0x0004	
#define TINT		0x0002	
#define IDON		0x0001	
#define INTR		0x8000	
#define INEA		0x4000	
#define RXON		0x2000	
#define TXON		0x1000	
#define TDMD		0x0800	
#define STOP		0x0400	
#define STRT		0x0200	
#define INIT		0x0100	



#define BABLM		0x0040	
#define MISSM		0x0010	
#define MERRM		0x0008	
#define RINTM		0x0004	
#define TINTM		0x0002	
#define IDONM		0x0001	
#define DXMT2PD		0x1000	
#define EMBA		0x0800	



#define ENTST		0x0080	
#define DMAPLUS		0x0040	
#define TIMER		0x0020	
#define DPOLL		0x0010	
#define APAD_XMT	0x0008	
#define ASTRP_RCV	0x0004	
#define MFCO		0x0002	
#define MFCOM		0x0001	
#define RCVCCO		0x2000	
#define RCVCCOM		0x1000	
#define TXSTRT		0x0800	
#define TXSTRTM		0x0400	
#define JAB		0x0200	
#define JABM		0x0100	



#define PROM		0x0080	
#define DRCVBC		0x0040	
#define DRCVPA		0x0020	
#define DLNKTST		0x0010	
#define DAPC		0x0008	
#define MENDECL		0x0004	
#define LRTTSEL		0x0002	
#define PORTSEL1	0x0001	
#define PORTSEL2	0x8000	
#define INTL		0x4000	
#define DRTY		0x2000	
#define FCOLL		0x1000	
#define DXMTFCS		0x0800	
#define LOOP		0x0400	
#define DTX		0x0200	
#define DRX		0x0100	



#define ASEL		0x0200	



#define LEDOUT		0x0080	
#define PSE		0x8000	
#define XMTE		0x1000	
#define RVPOLE		0x0800	
#define RCVE		0x0400	
#define JABE		0x0200	
#define COLE		0x0100	



struct RDRE {
    volatile u_short RMD0;	
    volatile u_short RMD1;	
    volatile u_short RMD2;	
    volatile u_short RMD3;	
};



struct TDRE {
    volatile u_short TMD0;	
    volatile u_short TMD1;	
    volatile u_short TMD2;	
    volatile u_short TMD3;	
};



#define RF_OWN		0x0080	
#define RF_ERR		0x0040	
#define RF_FRAM		0x0020	
#define RF_OFLO		0x0010	
#define RF_CRC		0x0008	
#define RF_BUFF		0x0004	
#define RF_STP		0x0002	
#define RF_ENP		0x0001	



#define TF_OWN		0x0080	
#define TF_ERR		0x0040	
#define TF_ADD_FCS	0x0020	
#define TF_MORE		0x0010	
#define TF_ONE		0x0008	
#define TF_DEF		0x0004	
#define TF_STP		0x0002	
#define TF_ENP		0x0001	



#define EF_BUFF		0x0080	
#define EF_UFLO		0x0040	
#define EF_LCOL		0x0010	
#define EF_LCAR		0x0008	
#define EF_RTRY		0x0004	
#define EF_TDR		0xff03	




struct MC68230 {
    volatile u_char PGCR;	
    u_char Pad1[1];
    volatile u_char PSRR;	
    u_char Pad2[1];
    volatile u_char PADDR;	
    u_char Pad3[1];
    volatile u_char PBDDR;	
    u_char Pad4[1];
    volatile u_char PCDDR;	
    u_char Pad5[1];
    volatile u_char PIVR;	
    u_char Pad6[1];
    volatile u_char PACR;	
    u_char Pad7[1];
    volatile u_char PBCR;	
    u_char Pad8[1];
    volatile u_char PADR;	
    u_char Pad9[1];
    volatile u_char PBDR;	
    u_char Pad10[1];
    volatile u_char PAAR;	
    u_char Pad11[1];
    volatile u_char PBAR;	
    u_char Pad12[1];
    volatile u_char PCDR;	
    u_char Pad13[1];
    volatile u_char PSR;	
    u_char Pad14[5];
    volatile u_char TCR;	
    u_char Pad15[1];
    volatile u_char TIVR;	
    u_char Pad16[3];
    volatile u_char CPRH;	
    u_char Pad17[1];
    volatile u_char CPRM;	
    u_char Pad18[1];
    volatile u_char CPRL;	
    u_char Pad19[3];
    volatile u_char CNTRH;	
    u_char Pad20[1];
    volatile u_char CNTRM;	
    u_char Pad21[1];
    volatile u_char CNTRL;	
    u_char Pad22[1];
    volatile u_char TSR;	
    u_char Pad23[11];
};



#define ARIADNE_LANCE		0x360

#define ARIADNE_PIT		0x1000

#define ARIADNE_BOOTPROM	0x4000	
#define ARIADNE_BOOTPROM_SIZE	0x4000

#define ARIADNE_RAM		0x8000	
#define ARIADNE_RAM_SIZE	0x8000

