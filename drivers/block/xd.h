#ifndef _LINUX_XD_H
#define _LINUX_XD_H


#include <linux/interrupt.h>

#define XD_DATA		(xd_iobase + 0x00)	
#define XD_RESET	(xd_iobase + 0x01)	
#define XD_STATUS	(xd_iobase + 0x01)	
#define XD_SELECT	(xd_iobase + 0x02)	
#define XD_JUMPER	(xd_iobase + 0x02)	
#define XD_CONTROL	(xd_iobase + 0x03)	
#define XD_RESERVED	(xd_iobase + 0x03)	

#define CMD_TESTREADY	0x00	
#define CMD_RECALIBRATE	0x01	
#define CMD_SENSE	0x03	
#define CMD_FORMATDRV	0x04	
#define CMD_VERIFY	0x05	
#define CMD_FORMATTRK	0x06	
#define CMD_FORMATBAD	0x07	
#define CMD_READ	0x08	
#define CMD_WRITE	0x0A	
#define CMD_SEEK	0x0B	

#define CMD_DTCSETPARAM	0x0C	
#define CMD_DTCGETECC	0x0D	
#define CMD_DTCREADBUF	0x0E	
#define CMD_DTCWRITEBUF 0x0F	
#define CMD_DTCREMAPTRK	0x11	
#define CMD_DTCGETPARAM	0xFB	
#define CMD_DTCSETSTEP	0xFC	
#define CMD_DTCSETGEOM	0xFE	
#define CMD_DTCGETGEOM	0xFF	
#define CMD_ST11GETGEOM 0xF8	
#define CMD_WDSETPARAM	0x0C	
#define CMD_XBSETPARAM	0x0C	

#define CSB_ERROR	0x02	
#define CSB_LUN		0x20	

#define STAT_READY	0x01	
#define STAT_INPUT	0x02	
#define STAT_COMMAND	0x04	
#define STAT_SELECT	0x08	
#define STAT_REQUEST	0x10	
#define STAT_INTERRUPT	0x20	

#define PIO_MODE	0x00	
#define DMA_MODE	0x03	

#define XD_MAXDRIVES	2	
#define XD_TIMEOUT	HZ	
#define XD_RETRIES	4	

#undef DEBUG			

#ifdef DEBUG
	#define DEBUG_STARTUP	
	#define DEBUG_OVERRIDE	
	#define DEBUG_READWRITE	
	#define DEBUG_OTHER	
	#define DEBUG_COMMAND	
#endif 

typedef struct {
	u_char heads;
	u_short cylinders;
	u_char sectors;
	u_char control;
	int unit;
} XD_INFO;

typedef struct {
	unsigned int offset;
	const char *string;
	void (*init_controller)(unsigned int address);
	void (*init_drive)(u_char drive);
	const char *name;
} XD_SIGNATURE;

#ifndef MODULE
static int xd_manual_geo_init (char *command);
#endif 
static u_char xd_detect (u_char *controller, unsigned int *address);
static u_char xd_initdrives (void (*init_drive)(u_char drive));

static void do_xd_request (struct request_queue * q);
static int xd_ioctl (struct block_device *bdev,fmode_t mode,unsigned int cmd,unsigned long arg);
static int xd_readwrite (u_char operation,XD_INFO *disk,char *buffer,u_int block,u_int count);
static void xd_recalibrate (u_char drive);

static irqreturn_t xd_interrupt_handler(int irq, void *dev_id);
static u_char xd_setup_dma (u_char opcode,u_char *buffer,u_int count);
static u_char *xd_build (u_char *cmdblk,u_char command,u_char drive,u_char head,u_short cylinder,u_char sector,u_char count,u_char control);
static void xd_watchdog (unsigned long unused);
static inline u_char xd_waitport (u_short port,u_char flags,u_char mask,u_long timeout);
static u_int xd_command (u_char *command,u_char mode,u_char *indata,u_char *outdata,u_char *sense,u_long timeout);

static void xd_dtc_init_controller (unsigned int address);
static void xd_dtc5150cx_init_drive (u_char drive);
static void xd_dtc_init_drive (u_char drive);
static void xd_wd_init_controller (unsigned int address);
static void xd_wd_init_drive (u_char drive);
static void xd_seagate_init_controller (unsigned int address);
static void xd_seagate_init_drive (u_char drive);
static void xd_omti_init_controller (unsigned int address);
static void xd_omti_init_drive (u_char drive);
static void xd_xebec_init_controller (unsigned int address);
static void xd_xebec_init_drive (u_char drive);
static void xd_setparam (u_char command,u_char drive,u_char heads,u_short cylinders,u_short rwrite,u_short wprecomp,u_char ecc);
static void xd_override_init_drive (u_char drive);

#endif 
