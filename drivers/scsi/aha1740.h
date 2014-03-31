#ifndef _AHA1740_H

/* $Id$
 *
 * Header file for the adaptec 1740 driver for Linux
 *
 * With minor revisions 3/31/93
 * Written and (C) 1992,1993 Brad McLean.  See aha1740.c
 * for more info
 *
 */

#include <linux/types.h>

#define SLOTSIZE	0x5c

#define	HID0(base)	(base + 0x0)
#define	HID1(base)	(base + 0x1)
#define HID2(base)	(base + 0x2)
#define	HID3(base)	(base + 0x3)
#define	EBCNTRL(base)	(base + 0x4)
#define	PORTADR(base)	(base + 0x40)
#define BIOSADR(base)	(base + 0x41)
#define INTDEF(base)	(base + 0x42)
#define SCSIDEF(base)	(base + 0x43)
#define BUSDEF(base)	(base + 0x44)
#define	RESV0(base)	(base + 0x45)
#define RESV1(base)	(base + 0x46)
#define	RESV2(base)	(base + 0x47)

#define	HID_MFG	"ADP"
#define	HID_PRD 0
#define HID_REV 2
#define EBCNTRL_VALUE 1
#define PORTADDR_ENH 0x80
#define	G2INTST(base)	(base + 0x56)
#define G2STAT(base)	(base + 0x57)
#define	MBOXIN0(base)	(base + 0x58)
#define	MBOXIN1(base)	(base + 0x59)
#define	MBOXIN2(base)	(base + 0x5a)
#define	MBOXIN3(base)	(base + 0x5b)
#define G2STAT2(base)	(base + 0x5c)

#define G2INTST_MASK		0xf0	
#define	G2INTST_CCBGOOD		0x10	
#define	G2INTST_CCBRETRY	0x50	
#define	G2INTST_HARDFAIL	0x70	
#define	G2INTST_CMDGOOD		0xa0	
#define G2INTST_CCBERROR	0xc0	
#define	G2INTST_ASNEVENT	0xd0	
#define	G2INTST_CMDERROR	0xe0	

#define G2STAT_MBXOUT	4	
#define	G2STAT_INTPEND	2	
#define	G2STAT_BUSY	1	

#define G2STAT2_READY	0	

#define	MBOXOUT0(base)	(base + 0x50)
#define	MBOXOUT1(base)	(base + 0x51)
#define	MBOXOUT2(base)	(base + 0x52)
#define	MBOXOUT3(base)	(base + 0x53)
#define	ATTN(base)	(base + 0x54)
#define G2CNTRL(base)	(base + 0x55)

#define	ATTN_IMMED	0x10	
#define	ATTN_START	0x40	
#define	ATTN_ABORT	0x50	

#define G2CNTRL_HRST	0x80	
#define G2CNTRL_IRST	0x40	
#define G2CNTRL_HRDY	0x20	

struct aha1740_chain {
	u32 dataptr;		
	u32 datalen;		
};

#define any2scsi(up, p)				\
(up)[0] = (((unsigned long)(p)) >> 16)  ;	\
(up)[1] = (((unsigned long)(p)) >> 8);		\
(up)[2] = ((unsigned long)(p));

#define scsi2int(up) ( (((long)*(up)) << 16) + (((long)(up)[1]) << 8) + ((long)(up)[2]) )

#define xany2scsi(up, p)	\
(up)[0] = ((long)(p)) >> 24;	\
(up)[1] = ((long)(p)) >> 16;	\
(up)[2] = ((long)(p)) >> 8;	\
(up)[3] = ((long)(p));

#define xscsi2int(up) ( (((long)(up)[0]) << 24) + (((long)(up)[1]) << 16) \
		      + (((long)(up)[2]) <<  8) +  ((long)(up)[3]) )

#define MAX_CDB 12
#define MAX_SENSE 14
#define MAX_STATUS 32

struct ecb {			
	u16 cmdw;		
	
	u16 cne:1,		
	:6, di:1,		
	:2, ses:1,		
	:1, sg:1,		
	:1, dsb:1,		
	 ars:1;			
	
	u16 lun:3,		
	 tag:1,			
	 tt:2,			
	 nd:1,			
	:1, dat:1,		
	 dir:1,			
	 st:1,			
	 chk:1,			
	:2, rec:1,:1;		
	u16 nil0;		
	u32 dataptr;		
	u32 datalen;		
	u32 statusptr;		
	u32 linkptr;		
	u32 nil1;		
	u32 senseptr;		
	u8 senselen;		
	u8 cdblen;		
	u16 datacheck;		
	u8 cdb[MAX_CDB];	
	u8 sense[MAX_SENSE];	
	u8 status[MAX_STATUS];	
	Scsi_Cmnd *SCpnt;	
	void (*done) (Scsi_Cmnd *);	
};

#define	AHA1740CMD_NOP	 0x00	
#define AHA1740CMD_INIT	 0x01	
#define AHA1740CMD_DIAG	 0x05	
#define AHA1740CMD_SCSI	 0x06	
#define AHA1740CMD_SENSE 0x08	
#define AHA1740CMD_DOWN  0x09	
#define AHA1740CMD_RINQ  0x0a	
#define AHA1740CMD_TARG  0x10	

#define AHA1740_ECBS 32
#define AHA1740_SCATTER 16
#define AHA1740_CMDLUN 1

#endif
