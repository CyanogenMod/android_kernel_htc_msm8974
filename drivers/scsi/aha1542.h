#ifndef _AHA1542_H


#include <linux/types.h>

#define STATUS(base) base
#define STST	0x80		
#define DIAGF	0x40		
#define INIT	0x20		
#define IDLE	0x10		
#define CDF	0x08		
#define DF	0x04		
#define INVDCMD	0x01		
#define STATMASK 0xfd		

#define INTRFLAGS(base) (STATUS(base)+2)
#define ANYINTR	0x80		
#define SCRD	0x08		
#define HACC	0x04		
#define MBOA	0x02		
#define MBIF	0x01		
#define INTRMASK 0x8f

#define CONTROL(base) STATUS(base)
#define HRST	0x80		
#define SRST	0x40		
#define IRST	0x20		
#define SCRST	0x10		

#define DATA(base) (STATUS(base)+1)
#define CMD_NOP		0x00	
#define CMD_MBINIT	0x01	
#define CMD_START_SCSI	0x02	
#define CMD_INQUIRY	0x04	
#define CMD_EMBOI	0x05	
#define CMD_BUSON_TIME	0x07	
#define CMD_BUSOFF_TIME	0x08	
#define CMD_DMASPEED	0x09	
#define CMD_RETDEVS	0x0a	
#define CMD_RETCONF	0x0b	
#define CMD_RETSETUP	0x0d	
#define CMD_ECHO	0x1f	

#define CMD_EXTBIOS     0x28    
#define CMD_MBENABLE    0x29    

struct mailbox {
  unchar status;		
  unchar ccbptr[3];		
};

struct chain {
  unchar datalen[3];		
  unchar dataptr[3];		
};

static inline void any2scsi(u8 *p, u32 v)
{
	p[0] = v >> 16;
	p[1] = v >> 8;
	p[2] = v;
}

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

struct ccb {			
  unchar op;			
  unchar idlun;			
				
				
				
  unchar cdblen;		
  unchar rsalen;		
  unchar datalen[3];		
  unchar dataptr[3];		
  unchar linkptr[3];		
  unchar commlinkid;		
  unchar hastat;		
  unchar tarstat;		
  unchar reserved[2];
  unchar cdb[MAX_CDB+MAX_SENSE];
				
};

static int aha1542_detect(struct scsi_host_template *);
static int aha1542_queuecommand(struct Scsi_Host *, struct scsi_cmnd *);
static int aha1542_bus_reset(Scsi_Cmnd * SCpnt);
static int aha1542_dev_reset(Scsi_Cmnd * SCpnt);
static int aha1542_host_reset(Scsi_Cmnd * SCpnt);
#if 0
static int aha1542_old_abort(Scsi_Cmnd * SCpnt);
static int aha1542_old_reset(Scsi_Cmnd *, unsigned int);
#endif
static int aha1542_biosparam(struct scsi_device *, struct block_device *,
		sector_t, int *);

#define AHA1542_MAILBOXES 8
#define AHA1542_SCATTER 16
#define AHA1542_CMDLUN 1

#endif
