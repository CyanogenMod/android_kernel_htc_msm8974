#ifndef _ASM_S390_CIO_H_
#define _ASM_S390_CIO_H_

#include <linux/spinlock.h>
#include <asm/types.h>

#ifdef __KERNEL__

#define LPM_ANYPATH 0xff
#define __MAX_CSSID 0

#include <asm/scsw.h>

struct ccw1 {
	__u8  cmd_code;
	__u8  flags;
	__u16 count;
	__u32 cda;
} __attribute__ ((packed,aligned(8)));

#define CCW_FLAG_DC		0x80
#define CCW_FLAG_CC		0x40
#define CCW_FLAG_SLI		0x20
#define CCW_FLAG_SKIP		0x10
#define CCW_FLAG_PCI		0x08
#define CCW_FLAG_IDA		0x04
#define CCW_FLAG_SUSPEND	0x02

#define CCW_CMD_READ_IPL	0x02
#define CCW_CMD_NOOP		0x03
#define CCW_CMD_BASIC_SENSE	0x04
#define CCW_CMD_TIC		0x08
#define CCW_CMD_STLCK           0x14
#define CCW_CMD_SENSE_PGID	0x34
#define CCW_CMD_SUSPEND_RECONN	0x5B
#define CCW_CMD_RDC		0x64
#define CCW_CMD_RELEASE		0x94
#define CCW_CMD_SET_PGID	0xAF
#define CCW_CMD_SENSE_ID	0xE4
#define CCW_CMD_DCTL		0xF3

#define SENSE_MAX_COUNT		0x20

struct erw {
	__u32 res0  : 3;
	__u32 auth  : 1;
	__u32 pvrf  : 1;
	__u32 cpt   : 1;
	__u32 fsavf : 1;
	__u32 cons  : 1;
	__u32 scavf : 1;
	__u32 fsaf  : 1;
	__u32 scnt  : 6;
	__u32 res16 : 16;
} __attribute__ ((packed));

struct sublog {
	__u32 res0  : 1;
	__u32 esf   : 7;
	__u32 lpum  : 8;
	__u32 arep  : 1;
	__u32 fvf   : 5;
	__u32 sacc  : 2;
	__u32 termc : 2;
	__u32 devsc : 1;
	__u32 serr  : 1;
	__u32 ioerr : 1;
	__u32 seqc  : 3;
} __attribute__ ((packed));

struct esw0 {
	struct sublog sublog;
	struct erw erw;
	__u32  faddr[2];
	__u32  saddr;
} __attribute__ ((packed));

struct esw1 {
	__u8  zero0;
	__u8  lpum;
	__u16 zero16;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));

struct esw2 {
	__u8  zero0;
	__u8  lpum;
	__u16 dcti;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));

struct esw3 {
	__u8  zero0;
	__u8  lpum;
	__u16 res;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));

struct irb {
	union scsw scsw;
	union {
		struct esw0 esw0;
		struct esw1 esw1;
		struct esw2 esw2;
		struct esw3 esw3;
	} esw;
	__u8   ecw[32];
} __attribute__ ((packed,aligned(4)));

struct ciw {
	__u32 et       :  2;
	__u32 reserved :  2;
	__u32 ct       :  4;
	__u32 cmd      :  8;
	__u32 count    : 16;
} __attribute__ ((packed));

#define CIW_TYPE_RCD	0x0    	
#define CIW_TYPE_SII	0x1    	
#define CIW_TYPE_RNI	0x2    	

#define DOIO_ALLOW_SUSPEND	 0x0001 
#define DOIO_DENY_PREFETCH	 0x0002 
#define DOIO_SUPPRESS_INTER	 0x0004 
					
#define CIO_GONE       0x0001
#define CIO_NO_PATH    0x0002
#define CIO_OPER       0x0004
#define CIO_REVALIDATE 0x0008
#define CIO_BOXED      0x0010

struct ccw_dev_id {
	u8 ssid;
	u16 devno;
};

static inline int ccw_dev_id_is_equal(struct ccw_dev_id *dev_id1,
				      struct ccw_dev_id *dev_id2)
{
	if ((dev_id1->ssid == dev_id2->ssid) &&
	    (dev_id1->devno == dev_id2->devno))
		return 1;
	return 0;
}

extern void wait_cons_dev(void);

extern void css_schedule_reprobe(void);

extern void reipl_ccw_dev(struct ccw_dev_id *id);

struct cio_iplinfo {
	u16 devno;
	int is_qdio;
};

extern int cio_get_iplinfo(struct cio_iplinfo *iplinfo);

int chsc_sstpc(void *page, unsigned int op, u16 ctrl);
int chsc_sstpi(void *page, void *result, size_t size);

#endif

#endif
