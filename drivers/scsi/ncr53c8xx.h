/******************************************************************************
**  Device driver for the PCI-SCSI NCR538XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**  Copyright (C) 1998-2001  Gerard Roudier <groudier@free.fr>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  This driver has been ported to Linux from the FreeBSD NCR53C8XX driver
**  and is currently maintained by
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**  And has been ported to NetBSD by
**          Charles M. Hannum           <mycroft@gnu.ai.mit.edu>
**
**  NVRAM detection and reading.
**    Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
**
**  Added support for MIPS big endian systems.
**    Carsten Langgaard, carstenl@mips.com
**    Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
**
**  Added support for HP PARISC big endian systems.
**    Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
**
*******************************************************************************
*/

#ifndef NCR53C8XX_H
#define NCR53C8XX_H

#include <scsi/scsi_host.h>


#define SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
#define SCSI_NCR_DEBUG_INFO_SUPPORT

#ifdef	CONFIG_SCSI_NCR53C8XX_INTEGRITY_CHECK
#	define SCSI_NCR_ENABLE_INTEGRITY_CHECK
#endif


#define	SCSI_NCR_SETUP_SPECIAL_FEATURES		(3)

#define SCSI_NCR_MAX_SYNC			(80)

#ifdef	CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#if	CONFIG_SCSI_NCR53C8XX_MAX_TAGS < 2
#define SCSI_NCR_MAX_TAGS	(2)
#elif	CONFIG_SCSI_NCR53C8XX_MAX_TAGS > 256
#define SCSI_NCR_MAX_TAGS	(256)
#else
#define	SCSI_NCR_MAX_TAGS	CONFIG_SCSI_NCR53C8XX_MAX_TAGS
#endif
#else
#define SCSI_NCR_MAX_TAGS	(8)
#endif

#ifdef	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	CONFIG_SCSI_NCR53C8XX_DEFAULT_TAGS
#elif	defined CONFIG_SCSI_NCR53C8XX_TAGGED_QUEUE
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	SCSI_NCR_MAX_TAGS
#else
#define	SCSI_NCR_SETUP_DEFAULT_TAGS	(0)
#endif

#if defined(CONFIG_SCSI_NCR53C8XX_IARB)
#define SCSI_NCR_IARB_SUPPORT
#endif

#ifndef	CONFIG_SCSI_NCR53C8XX_SYNC
#define	CONFIG_SCSI_NCR53C8XX_SYNC	(20)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC > SCSI_NCR_MAX_SYNC
#undef	CONFIG_SCSI_NCR53C8XX_SYNC
#define	CONFIG_SCSI_NCR53C8XX_SYNC	SCSI_NCR_MAX_SYNC
#endif

#if	CONFIG_SCSI_NCR53C8XX_SYNC == 0
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(255)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 5
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(50)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 20
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(250/(CONFIG_SCSI_NCR53C8XX_SYNC))
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 33
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(11)
#elif	CONFIG_SCSI_NCR53C8XX_SYNC <= 40
#define	SCSI_NCR_SETUP_DEFAULT_SYNC	(10)
#else
#define	SCSI_NCR_SETUP_DEFAULT_SYNC 	(9)
#endif

#ifdef CONFIG_SCSI_NCR53C8XX_NO_DISCONNECT
#define SCSI_NCR_SETUP_DISCONNECTION	(0)
#else
#define SCSI_NCR_SETUP_DISCONNECTION	(1)
#endif

#ifdef CONFIG_SCSI_NCR53C8XX_FORCE_SYNC_NEGO
#define SCSI_NCR_SETUP_FORCE_SYNC_NEGO	(1)
#else
#define SCSI_NCR_SETUP_FORCE_SYNC_NEGO	(0)
#endif

#ifdef CONFIG_SCSI_NCR53C8XX_DISABLE_MPARITY_CHECK
#define SCSI_NCR_SETUP_MASTER_PARITY	(0)
#else
#define SCSI_NCR_SETUP_MASTER_PARITY	(1)
#endif

#ifdef CONFIG_SCSI_NCR53C8XX_DISABLE_PARITY_CHECK
#define SCSI_NCR_SETUP_SCSI_PARITY	(0)
#else
#define SCSI_NCR_SETUP_SCSI_PARITY	(1)
#endif

#define SCSI_NCR_SETUP_SETTLE_TIME	(2)

#ifndef	SCSI_NCR_PCIQ_WORK_AROUND_OPT
#define	SCSI_NCR_PCIQ_WORK_AROUND_OPT	1
#endif

#if	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 1
#define	SCSI_NCR_PCIQ_MAY_NOT_FLUSH_PW_UPSTREAM
#define	SCSI_NCR_PCIQ_MAY_REORDER_WRITES
#define	SCSI_NCR_PCIQ_MAY_MISS_COMPLETIONS

#elif	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 2
#define	SCSI_NCR_PCIQ_MAY_NOT_FLUSH_PW_UPSTREAM
#define	SCSI_NCR_PCIQ_MAY_REORDER_WRITES
#define	SCSI_NCR_PCIQ_MAY_MISS_COMPLETIONS
#define	SCSI_NCR_PCIQ_BROKEN_INTR

#elif	SCSI_NCR_PCIQ_WORK_AROUND_OPT == 3
#define	SCSI_NCR_PCIQ_SYNC_ON_INTR
#endif


#define SCSI_NCR_ALWAYS_SIMPLE_TAG
#define SCSI_NCR_MAX_SCATTER	(127)
#define SCSI_NCR_MAX_TARGET	(16)

#define SCSI_NCR_CAN_QUEUE	(8*SCSI_NCR_MAX_TAGS + 2*SCSI_NCR_MAX_TARGET)
#define SCSI_NCR_CMD_PER_LUN	(SCSI_NCR_MAX_TAGS)

#define SCSI_NCR_SG_TABLESIZE	(SCSI_NCR_MAX_SCATTER)
#define SCSI_NCR_TIMER_INTERVAL	(HZ)

#if 1 
#define SCSI_NCR_MAX_LUN	(16)
#else
#define SCSI_NCR_MAX_LUN	(1)
#endif


#ifdef	__BIG_ENDIAN

#define	inw_l2b		inw
#define	inl_l2b		inl
#define	outw_b2l	outw
#define	outl_b2l	outl

#define	readb_raw	readb
#define	writeb_raw	writeb

#if defined(SCSI_NCR_BIG_ENDIAN)
#define	readw_l2b	__raw_readw
#define	readl_l2b	__raw_readl
#define	writew_b2l	__raw_writew
#define	writel_b2l	__raw_writel
#define	readw_raw	__raw_readw
#define	readl_raw	__raw_readl
#define	writew_raw	__raw_writew
#define	writel_raw	__raw_writel
#else	
#define	readw_l2b	readw
#define	readl_l2b	readl
#define	writew_b2l	writew
#define	writel_b2l	writel
#define	readw_raw	readw
#define	readl_raw	readl
#define	writew_raw	writew
#define	writel_raw	writel
#endif

#else	

#define	inw_raw		inw
#define	inl_raw		inl
#define	outw_raw	outw
#define	outl_raw	outl

#define	readb_raw	readb
#define	readw_raw	readw
#define	readl_raw	readl
#define	writeb_raw	writeb
#define	writew_raw	writew
#define	writel_raw	writel

#endif

#if !defined(__hppa__) && !defined(__mips__)
#ifdef	SCSI_NCR_BIG_ENDIAN
#error	"The NCR in BIG ENDIAN addressing mode is not (yet) supported"
#endif
#endif

#define MEMORY_BARRIER()	mb()



#if	defined(SCSI_NCR_BIG_ENDIAN)

#define ncr_offb(o)	(((o)&~3)+((~((o)&3))&3))
#define ncr_offw(o)	(((o)&~3)+((~((o)&3))&2))

#else

#define ncr_offb(o)	(o)
#define ncr_offw(o)	(o)

#endif


#if	defined(__BIG_ENDIAN) && !defined(SCSI_NCR_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_le32(dw)
#define scr_to_cpu(dw)	le32_to_cpu(dw)

#elif	defined(__LITTLE_ENDIAN) && defined(SCSI_NCR_BIG_ENDIAN)

#define cpu_to_scr(dw)	cpu_to_be32(dw)
#define scr_to_cpu(dw)	be32_to_cpu(dw)

#else

#define cpu_to_scr(dw)	(dw)
#define scr_to_cpu(dw)	(dw)

#endif



#define INB_OFF(o)		readb_raw((char __iomem *)np->reg + ncr_offb(o))
#define OUTB_OFF(o, val)	writeb_raw((val), (char __iomem *)np->reg + ncr_offb(o))

#if	defined(__BIG_ENDIAN) && !defined(SCSI_NCR_BIG_ENDIAN)

#define INW_OFF(o)		readw_l2b((char __iomem *)np->reg + ncr_offw(o))
#define INL_OFF(o)		readl_l2b((char __iomem *)np->reg + (o))

#define OUTW_OFF(o, val)	writew_b2l((val), (char __iomem *)np->reg + ncr_offw(o))
#define OUTL_OFF(o, val)	writel_b2l((val), (char __iomem *)np->reg + (o))

#elif	defined(__LITTLE_ENDIAN) && defined(SCSI_NCR_BIG_ENDIAN)

#define INW_OFF(o)		readw_b2l((char __iomem *)np->reg + ncr_offw(o))
#define INL_OFF(o)		readl_b2l((char __iomem *)np->reg + (o))

#define OUTW_OFF(o, val)	writew_l2b((val), (char __iomem *)np->reg + ncr_offw(o))
#define OUTL_OFF(o, val)	writel_l2b((val), (char __iomem *)np->reg + (o))

#else

#ifdef CONFIG_SCSI_NCR53C8XX_NO_WORD_TRANSFERS
#define INW_OFF(o)		(readb((char __iomem *)np->reg + ncr_offw(o)) << 8 | readb((char __iomem *)np->reg + ncr_offw(o) + 1))
#else
#define INW_OFF(o)		readw_raw((char __iomem *)np->reg + ncr_offw(o))
#endif
#define INL_OFF(o)		readl_raw((char __iomem *)np->reg + (o))

#ifdef CONFIG_SCSI_NCR53C8XX_NO_WORD_TRANSFERS
#define OUTW_OFF(o, val)	do { writeb((char)((val) >> 8), (char __iomem *)np->reg + ncr_offw(o)); writeb((char)(val), (char __iomem *)np->reg + ncr_offw(o) + 1); } while (0)
#else
#define OUTW_OFF(o, val)	writew_raw((val), (char __iomem *)np->reg + ncr_offw(o))
#endif
#define OUTL_OFF(o, val)	writel_raw((val), (char __iomem *)np->reg + (o))

#endif

#define INB(r)		INB_OFF (offsetof(struct ncr_reg,r))
#define INW(r)		INW_OFF (offsetof(struct ncr_reg,r))
#define INL(r)		INL_OFF (offsetof(struct ncr_reg,r))

#define OUTB(r, val)	OUTB_OFF (offsetof(struct ncr_reg,r), (val))
#define OUTW(r, val)	OUTW_OFF (offsetof(struct ncr_reg,r), (val))
#define OUTL(r, val)	OUTL_OFF (offsetof(struct ncr_reg,r), (val))


#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

#define OUTL_DSP(v)				\
	do {					\
		MEMORY_BARRIER();		\
		OUTL (nc_dsp, (v));		\
	} while (0)

#define OUTONB_STD()				\
	do {					\
		MEMORY_BARRIER();		\
		OUTONB (nc_dcntl, (STD|NOCOM));	\
	} while (0)


struct ncr_chip {
	unsigned short	revision_id;
	unsigned char	burst_max;	
	unsigned char	offset_max;
	unsigned char	nr_divisor;
	unsigned int	features;
#define FE_LED0		(1<<0)
#define FE_WIDE		(1<<1)    
#define FE_ULTRA	(1<<2)	  
#define FE_DBLR		(1<<4)	  
#define FE_QUAD		(1<<5)	  
#define FE_ERL		(1<<6)    
#define FE_CLSE		(1<<7)    
#define FE_WRIE		(1<<8)    
#define FE_ERMP		(1<<9)    
#define FE_BOF		(1<<10)   
#define FE_DFS		(1<<11)   
#define FE_PFEN		(1<<12)   
#define FE_LDSTR	(1<<13)   
#define FE_RAM		(1<<14)   
#define FE_VARCLK	(1<<15)   
#define FE_RAM8K	(1<<16)   
#define FE_64BIT	(1<<17)   
#define FE_IO256	(1<<18)   
#define FE_NOPM		(1<<19)   
#define FE_LEDC		(1<<20)   
#define FE_DIFF		(1<<21)   
#define FE_66MHZ 	(1<<23)   
#define FE_DAC	 	(1<<24)   
#define FE_ISTAT1 	(1<<25)   
#define FE_DAC_IN_USE	(1<<26)	  
#define FE_EHP		(1<<27)   
#define FE_MUX		(1<<28)   
#define FE_EA		(1<<29)   

#define FE_CACHE_SET	(FE_ERL|FE_CLSE|FE_WRIE|FE_ERMP)
#define FE_SCSI_SET	(FE_WIDE|FE_ULTRA|FE_DBLR|FE_QUAD|F_CLK80)
#define FE_SPECIAL_SET	(FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM)
};


#define SCSI_NCR_MAX_EXCLUDES 8
struct ncr_driver_setup {
	u8	master_parity;
	u8	scsi_parity;
	u8	disconnection;
	u8	special_features;
	u8	force_sync_nego;
	u8	reverse_probe;
	u8	pci_fix_up;
	u8	use_nvram;
	u8	verbose;
	u8	default_tags;
	u16	default_sync;
	u16	debug;
	u8	burst_max;
	u8	led_pin;
	u8	max_wide;
	u8	settle_delay;
	u8	diff_support;
	u8	irqm;
	u8	bus_check;
	u8	optimize;
	u8	recovery;
	u8	host_id;
	u16	iarb;
	u32	excludes[SCSI_NCR_MAX_EXCLUDES];
	char	tag_ctrl[100];
};

#define SCSI_NCR_DRIVER_SETUP			\
{						\
	SCSI_NCR_SETUP_MASTER_PARITY,		\
	SCSI_NCR_SETUP_SCSI_PARITY,		\
	SCSI_NCR_SETUP_DISCONNECTION,		\
	SCSI_NCR_SETUP_SPECIAL_FEATURES,	\
	SCSI_NCR_SETUP_FORCE_SYNC_NEGO,		\
	0,					\
	0,					\
	1,					\
	0,					\
	SCSI_NCR_SETUP_DEFAULT_TAGS,		\
	SCSI_NCR_SETUP_DEFAULT_SYNC,		\
	0x00,					\
	7,					\
	0,					\
	1,					\
	SCSI_NCR_SETUP_SETTLE_TIME,		\
	0,					\
	0,					\
	1,					\
	0,					\
	0,					\
	255,					\
	0x00					\
}

#define SCSI_NCR_DRIVER_SAFE_SETUP		\
{						\
	0,					\
	1,					\
	0,					\
	0,					\
	0,					\
	0,					\
	0,					\
	1,					\
	2,					\
	0,					\
	255,					\
	0x00,					\
	255,					\
	0,					\
	0,					\
	10,					\
	1,					\
	1,					\
	1,					\
	0,					\
	0,					\
	255					\
}



struct ncr_reg {
  u8	nc_scntl0;    

  u8	nc_scntl1;    
        #define   ISCON   0x10  
        #define   CRST    0x08  
        #define   IARB    0x02  

  u8	nc_scntl2;    
	#define   SDU     0x80  
	#define   CHM     0x40  
	#define   WSS     0x08  
	#define   WSR     0x01  

  u8	nc_scntl3;    
	#define   EWS     0x08  
	#define   ULTRA   0x80  
				

  u8	nc_scid;	
	#define   RRE     0x40  
	#define   SRE     0x20  

  u8	nc_sxfer;	
				

  u8	nc_sdid;	

  u8	nc_gpreg;	

  u8	nc_sfbr;	

  u8	nc_socl;
	#define   CREQ	  0x80	
	#define   CACK	  0x40	
	#define   CBSY	  0x20	
	#define   CSEL	  0x10	
	#define   CATN	  0x08	
	#define   CMSG	  0x04	
	#define   CC_D	  0x02	
	#define   CI_O	  0x01	

  u8	nc_ssid;

  u8	nc_sbcl;

  u8	nc_dstat;
        #define   DFE     0x80  
        #define   MDPE    0x40  
        #define   BF      0x20  
        #define   ABRT    0x10  
        #define   SSI     0x08  
        #define   SIR     0x04  
        #define   IID     0x01  

  u8	nc_sstat0;
        #define   ILF     0x80  
        #define   ORF     0x40  
        #define   OLF     0x20  
        #define   AIP     0x10  
        #define   LOA     0x08  
        #define   WOA     0x04  
        #define   IRST    0x02  
        #define   SDP     0x01  

  u8	nc_sstat1;
	#define   FF3210  0xf0	

  u8	nc_sstat2;
        #define   ILF1    0x80  
        #define   ORF1    0x40  
        #define   OLF1    0x20  
        #define   DM      0x04  
        #define   LDSC    0x02  

  u8	nc_dsa;	
  u8	nc_dsa1;
  u8	nc_dsa2;
  u8	nc_dsa3;

  u8	nc_istat;	
        #define   CABRT   0x80  
        #define   SRST    0x40  
        #define   SIGP    0x20  
        #define   SEM     0x10  
        #define   CON     0x08  
        #define   INTF    0x04  
        #define   SIP     0x02  
        #define   DIP     0x01  

  u8	nc_istat1;	
        #define   FLSH    0x04  
        #define   SRUN    0x02  
        #define   SIRQD   0x01  

  u8	nc_mbox0;	
  u8	nc_mbox1;	

	u8	nc_ctest0;
	#define   EHP     0x04	
  u8	nc_ctest1;

  u8	nc_ctest2;
	#define   CSIGP   0x40
				

  u8	nc_ctest3;
	#define   FLF     0x08  
	#define   CLF	  0x04	
	#define   FM      0x02  
	#define   WRIE    0x01  
				

  u32    nc_temp;	

	u8	nc_dfifo;
  u8	nc_ctest4;
	#define   MUX     0x80  
	#define   BDIS    0x80  
	#define   MPEE    0x08  

  u8	nc_ctest5;
	#define   DFS     0x20  
				
  u8	nc_ctest6;

  u32    nc_dbc;	
  u32    nc_dnad;	
  u32    nc_dsp;	
  u32    nc_dsps;	

  u8	nc_scratcha;  
  u8	nc_scratcha1;
  u8	nc_scratcha2;
  u8	nc_scratcha3;

  u8	nc_dmode;
	#define   BL_2    0x80  
	#define   BL_1    0x40  
	#define   ERL     0x08  
	#define   ERMP    0x04  
	#define   BOF     0x02  

  u8	nc_dien;
  u8	nc_sbr;

  u8	nc_dcntl;	
	#define   CLSE    0x80  
	#define   PFF     0x40  
	#define   PFEN    0x20  
	#define   EA      0x20  
	#define   SSM     0x10  
	#define   IRQM    0x08  
	#define   STD     0x04  
	#define   IRQD    0x02  
 	#define	  NOCOM   0x01	
				

  u32	nc_adder;

  u16	nc_sien;	
  u16	nc_sist;	
        #define   SBMC    0x1000
        #define   STO     0x0400
        #define   GEN     0x0200
        #define   HTH     0x0100
        #define   MA      0x80  
        #define   CMP     0x40  
        #define   SEL     0x20  
        #define   RSL     0x10  
        #define   SGE     0x08  
        #define   UDC     0x04  
        #define   RST     0x02  
        #define   PAR     0x01  

  u8	nc_slpar;
  u8	nc_swide;
  u8	nc_macntl;
  u8	nc_gpcntl;
  u8	nc_stime0;    
  u8	nc_stime1;    
  u16   nc_respid;    

  u8	nc_stest0;

  u8	nc_stest1;
	#define   SCLK    0x80	
	#define   DBLEN   0x08	
	#define   DBLSEL  0x04	
  

  u8	nc_stest2;
	#define   ROF     0x40	
	#define   DIF     0x20  
	#define   EXT     0x02  

  u8	nc_stest3;
	#define   TE     0x80	
	#define   HSC    0x20	
	#define   CSF    0x02	

  u16   nc_sidl;	
  u8	nc_stest4;
	#define   SMODE  0xc0	
	#define    SMODE_HVD 0x40	
	#define    SMODE_SE  0x80	
	#define    SMODE_LVD 0xc0	
	#define   LCKFRQ 0x20	
				

  u8	nc_53_;
  u16	nc_sodl;	
	u8	nc_ccntl0;	
	#define   ENPMJ  0x80	
	#define   PMJCTL 0x40	
	#define   ENNDJ  0x20	
	#define   DISFC  0x10	
	#define   DILS   0x02	
	#define   DPR    0x01	

	u8	nc_ccntl1;	
	#define   ZMOD   0x80	
	#define	  DIC	 0x10	
	#define   DDAC   0x08	
	#define   XTIMOD 0x04	
	#define   EXTIBMV 0x02	
	#define   EXDBMV 0x01	

  u16	nc_sbdl;	
  u16	nc_5a_;

  u8	nc_scr0;	
  u8	nc_scr1;	
  u8	nc_scr2;	
  u8	nc_scr3;	

  u8	nc_scrx[64];	
	u32	nc_mmrs;	
	u32	nc_mmws;	
	u32	nc_sfs;		
	u32	nc_drs;		
	u32	nc_sbms;	
	u32	nc_dbms;	
	u32	nc_dnad64;	
	u16	nc_scntl4;	
	#define   U3EN   0x80	
	#define   AIPEN	 0x40   
	#define   XCLKH_DT 0x08 
	#define   XCLKH_ST 0x04 

  u8	nc_aipcntl0;	
  u8	nc_aipcntl1;	

	u32	nc_pmjad1;	
	u32	nc_pmjad2;	
	u8	nc_rbc;		
	u8	nc_rbc1;	
	u8	nc_rbc2;	
	u8	nc_rbc3;	

	u8	nc_ua;		
	u8	nc_ua1;		
	u8	nc_ua2;		
	u8	nc_ua3;		
	u32	nc_esa;		
	u8	nc_ia;		
	u8	nc_ia1;
	u8	nc_ia2;
	u8	nc_ia3;
	u32	nc_sbc;		
	u32	nc_csbc;	

				
  u16	nc_crcpad;	
  u8	nc_crccntl0;	
	#define   SNDCRC  0x10	
  u8	nc_crccntl1;	
  u32	nc_crcdata;	 
  u32	nc_e8_;		
  u32	nc_ec_;		
  u16	nc_dfbc;	 

};


#define REGJ(p,r) (offsetof(struct ncr_reg, p ## r))
#define REG(r) REGJ (nc_, r)

typedef u32 ncrcmd;


#define	SCR_DATA_OUT	0x00000000
#define	SCR_DATA_IN	0x01000000
#define	SCR_COMMAND	0x02000000
#define	SCR_STATUS	0x03000000
#define SCR_DT_DATA_OUT	0x04000000
#define SCR_DT_DATA_IN	0x05000000
#define SCR_MSG_OUT	0x06000000
#define SCR_MSG_IN      0x07000000

#define SCR_ILG_OUT	0x04000000
#define SCR_ILG_IN	0x05000000


#define OPC_MOVE          0x08000000

#define SCR_MOVE_ABS(l) ((0x00000000 | OPC_MOVE) | (l))
#define SCR_MOVE_IND(l) ((0x20000000 | OPC_MOVE) | (l))
#define SCR_MOVE_TBL     (0x10000000 | OPC_MOVE)

#define SCR_CHMOV_ABS(l) ((0x00000000) | (l))
#define SCR_CHMOV_IND(l) ((0x20000000) | (l))
#define SCR_CHMOV_TBL     (0x10000000)

struct scr_tblmove {
        u32  size;
        u32  addr;
};


#define	SCR_SEL_ABS	0x40000000
#define	SCR_SEL_ABS_ATN	0x41000000
#define	SCR_SEL_TBL	0x42000000
#define	SCR_SEL_TBL_ATN	0x43000000


#ifdef SCSI_NCR_BIG_ENDIAN
struct scr_tblsel {
        u8	sel_scntl3;
        u8	sel_id;
        u8	sel_sxfer;
        u8	sel_scntl4;	
};
#else
struct scr_tblsel {
        u8	sel_scntl4;	
        u8	sel_sxfer;
        u8	sel_id;
        u8	sel_scntl3;
};
#endif

#define SCR_JMP_REL     0x04000000
#define SCR_ID(id)	(((u32)(id)) << 16)


#define	SCR_WAIT_DISC	0x48000000
#define SCR_WAIT_RESEL  0x50000000


#define SCR_SET(f)     (0x58000000 | (f))
#define SCR_CLR(f)     (0x60000000 | (f))

#define	SCR_CARRY	0x00000400
#define	SCR_TRG		0x00000200
#define	SCR_ACK		0x00000040
#define	SCR_ATN		0x00000008





#define SCR_NO_FLUSH 0x01000000

#define SCR_COPY(n) (0xc0000000 | SCR_NO_FLUSH | (n))
#define SCR_COPY_F(n) (0xc0000000 | (n))


#define SCR_REG_OFS(ofs) ((((ofs) & 0x7f) << 16ul) + ((ofs) & 0x80)) 

#define SCR_SFBR_REG(reg,op,data) \
        (0x68000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_SFBR(reg,op,data) \
        (0x70000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))

#define SCR_REG_REG(reg,op,data) \
        (0x78000000 | (SCR_REG_OFS(REG(reg))) | (op) | (((data)&0xff)<<8ul))


#define      SCR_LOAD   0x00000000
#define      SCR_SHL    0x01000000
#define      SCR_OR     0x02000000
#define      SCR_XOR    0x03000000
#define      SCR_AND    0x04000000
#define      SCR_SHR    0x05000000
#define      SCR_ADD    0x06000000
#define      SCR_ADDC   0x07000000

#define      SCR_SFBR_DATA   (0x00800000>>8ul)	


#define	SCR_FROM_REG(reg) \
	SCR_REG_SFBR(reg,SCR_OR,0)

#define	SCR_TO_REG(reg) \
	SCR_SFBR_REG(reg,SCR_OR,0)

#define	SCR_LOAD_REG(reg,data) \
	SCR_REG_REG(reg,SCR_LOAD,data)

#define SCR_LOAD_SFBR(data) \
        (SCR_REG_SFBR (gpreg, SCR_LOAD, data))


#define SCR_REG_OFS2(ofs) (((ofs) & 0xff) << 16ul)
#define SCR_NO_FLUSH2	0x02000000
#define SCR_DSA_REL2	0x10000000

#define SCR_LOAD_R(reg, how, n) \
        (0xe1000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_STORE_R(reg, how, n) \
        (0xe0000000 | how | (SCR_REG_OFS2(REG(reg))) | (n))

#define SCR_LOAD_ABS(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2, n)
#define SCR_LOAD_REL(reg, n)	SCR_LOAD_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2, n)
#define SCR_LOAD_ABS_F(reg, n)	SCR_LOAD_R(reg, 0, n)
#define SCR_LOAD_REL_F(reg, n)	SCR_LOAD_R(reg, SCR_DSA_REL2, n)

#define SCR_STORE_ABS(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2, n)
#define SCR_STORE_REL(reg, n)	SCR_STORE_R(reg, SCR_NO_FLUSH2|SCR_DSA_REL2,n)
#define SCR_STORE_ABS_F(reg, n)	SCR_STORE_R(reg, 0, n)
#define SCR_STORE_REL_F(reg, n)	SCR_STORE_R(reg, SCR_DSA_REL2, n)



#define SCR_NO_OP       0x80000000
#define SCR_JUMP        0x80080000
#define SCR_JUMP64      0x80480000
#define SCR_JUMPR       0x80880000
#define SCR_CALL        0x88080000
#define SCR_CALLR       0x88880000
#define SCR_RETURN      0x90080000
#define SCR_INT         0x98080000
#define SCR_INT_FLY     0x98180000

#define IFFALSE(arg)   (0x00080000 | (arg))
#define IFTRUE(arg)    (0x00000000 | (arg))

#define WHEN(phase)    (0x00030000 | (phase))
#define IF(phase)      (0x00020000 | (phase))

#define DATA(D)        (0x00040000 | ((D) & 0xff))
#define MASK(D,M)      (0x00040000 | (((M ^ 0xff) & 0xff) << 8ul)|((D) & 0xff))

#define CARRYSET       (0x00200000)



#define	S_GOOD		(0x00)
#define	S_CHECK_COND	(0x02)
#define	S_COND_MET	(0x04)
#define	S_BUSY		(0x08)
#define	S_INT		(0x10)
#define	S_INT_COND_MET	(0x14)
#define	S_CONFLICT	(0x18)
#define	S_TERMINATED	(0x20)
#define	S_QUEUE_FULL	(0x28)
#define	S_ILLEGAL	(0xff)
#define	S_SENSE		(0x80)



#define ncr_build_sge(np, data, badd, len)	\
do {						\
	(data)->addr = cpu_to_scr(badd);	\
	(data)->size = cpu_to_scr(len);		\
} while (0)

struct ncr_slot {
	u_long	base;
	u_long	base_2;
	u_long	base_c;
	u_long	base_2_c;
	void __iomem *base_v;
	void __iomem *base_2_v;
	int	irq;
	volatile struct ncr_reg	__iomem *reg;
};

struct ncr_device {
	struct device  *dev;
	struct ncr_slot  slot;
	struct ncr_chip  chip;
	u_char host_id;
	u8 differential;
};

extern struct Scsi_Host *ncr_attach(struct scsi_host_template *tpnt, int unit, struct ncr_device *device);
extern void ncr53c8xx_release(struct Scsi_Host *host);
irqreturn_t ncr53c8xx_intr(int irq, void *dev_id);
extern int ncr53c8xx_init(void);
extern void ncr53c8xx_exit(void);

#endif 
