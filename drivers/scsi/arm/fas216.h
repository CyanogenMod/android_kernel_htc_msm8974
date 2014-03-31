/*
 *  linux/drivers/acorn/scsi/fas216.h
 *
 *  Copyright (C) 1997-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  FAS216 generic driver
 */
#ifndef FAS216_H
#define FAS216_H

#include <scsi/scsi_eh.h>

#include "queue.h"
#include "msgqueue.h"


#define REG_CTCL		(0)
#define REG_STCL		(0)

#define REG_CTCM		(1)
#define REG_STCM		(1)

#define REG_FF			(2)

#define REG_CMD			(3)
#define CMD_NOP			0x00
#define CMD_FLUSHFIFO		0x01
#define CMD_RESETCHIP		0x02
#define CMD_RESETSCSI		0x03

#define CMD_TRANSFERINFO	0x10
#define CMD_INITCMDCOMPLETE	0x11
#define CMD_MSGACCEPTED		0x12
#define CMD_PADBYTES		0x18
#define CMD_SETATN		0x1a
#define CMD_RSETATN		0x1b

#define CMD_SELECTWOATN		0x41
#define CMD_SELECTATN		0x42
#define CMD_SELECTATNSTOP	0x43
#define CMD_ENABLESEL		0x44
#define CMD_DISABLESEL		0x45
#define CMD_SELECTATN3		0x46
#define CMD_RESEL3		0x47

#define CMD_WITHDMA		0x80

#define REG_STAT		(4)
#define STAT_IO			(1 << 0)			
#define STAT_CD			(1 << 1)			
#define STAT_MSG		(1 << 2)			
#define STAT_TRANSFERDONE	(1 << 3)			
#define STAT_TRANSFERCNTZ	(1 << 4)			
#define STAT_PARITYERROR	(1 << 5)			
#define STAT_REALBAD		(1 << 6)			
#define STAT_INT		(1 << 7)			

#define STAT_BUSMASK		(STAT_MSG|STAT_CD|STAT_IO)
#define STAT_DATAOUT		(0)				
#define STAT_DATAIN		(STAT_IO)			
#define STAT_COMMAND		(STAT_CD)			
#define STAT_STATUS		(STAT_CD|STAT_IO)		
#define STAT_MESGOUT		(STAT_MSG|STAT_CD)		
#define STAT_MESGIN		(STAT_MSG|STAT_CD|STAT_IO)	

#define REG_SDID		(4)
#define BUSID(target)		((target) & 7)

#define REG_INST		(5)
#define INST_SELWOATN		(1 << 0)			
#define INST_SELATN		(1 << 1)			
#define INST_RESELECTED		(1 << 2)			
#define INST_FUNCDONE		(1 << 3)			
#define INST_BUSSERVICE		(1 << 4)			
#define INST_DISCONNECT		(1 << 5)			
#define INST_ILLEGALCMD		(1 << 6)			
#define INST_BUSRESET		(1 << 7)			

#define REG_STIM		(5)

#define REG_IS			(6)
#define IS_BITS			0x07
#define IS_SELARB		0x00				
#define IS_MSGBYTESENT		0x01				
#define IS_NOTCOMMAND		0x02				
#define IS_EARLYPHASE		0x03				
#define IS_COMPLETE		0x04				
#define IS_SOF			0x08				

#define REG_STP			(6)

#define REG_SOF			(7)

#define REG_CFIS		(7)
#define CFIS_CF			0x1f				
#define CFIS_IS			0xe0				

#define REG_CNTL1		(8)
#define CNTL1_CID		(7 << 0)			
#define CNTL1_STE		(1 << 3)			
#define CNTL1_PERE		(1 << 4)			
#define CNTL1_PTE		(1 << 5)			
#define CNTL1_DISR		(1 << 6)			
#define CNTL1_ETM		(1 << 7)			

#define REG_CLKF		(9)
#define CLKF_F37MHZ		0x00				
#define CLKF_F10MHZ		0x02				
#define CLKF_F12MHZ		0x03				
#define CLKF_F17MHZ		0x04				
#define CLKF_F22MHZ		0x05				
#define CLKF_F27MHZ		0x06				
#define CLKF_F32MHZ		0x07				

#define REG_FTM			(10)
#define TEST_FTM		0x01				
#define TEST_FIM		0x02				
#define TEST_FHI		0x04				

#define REG_CNTL2		(11)
#define CNTL2_PGDP		(1 << 0)			
#define CNTL2_PGRP		(1 << 1)			
#define CNTL2_ACDPE		(1 << 2)			
#define CNTL2_S2FE		(1 << 3)			
#define CNTL2_TSDR		(1 << 4)			
#define CNTL2_SBO		(1 << 5)			
#define CNTL2_ENF		(1 << 6)			
#define CNTL2_DAE		(1 << 7)			

#define REG_CNTL3		(12)
#define CNTL3_BS8		(1 << 0)			
#define CNTL3_MDM		(1 << 1)			
#define CNTL3_LBTM		(1 << 2)			
#define CNTL3_FASTCLK		(1 << 3)			
#define CNTL3_FASTSCSI		(1 << 4)			
#define CNTL3_G2CB		(1 << 5)			
#define CNTL3_QTAG		(1 << 6)			
#define CNTL3_ADIDCHK		(1 << 7)			

#define REG_CTCH		(14)
#define REG_STCH		(14)

#define REG_ID			(14)

#define REG_DAL			(15)

typedef enum {
	PHASE_IDLE,					
	PHASE_SELECTION,				
	PHASE_SELSTEPS,					
	PHASE_COMMAND,					
	PHASE_MESSAGESENT,				
	PHASE_DATAOUT,					
	PHASE_DATAIN,					
	PHASE_MSGIN,					
	PHASE_MSGIN_DISCONNECT,				
	PHASE_MSGOUT,					
	PHASE_MSGOUT_EXPECT,				
	PHASE_STATUS,					
	PHASE_DONE					
} phase_t;

typedef enum {
	DMA_OUT,					
	DMA_IN						
} fasdmadir_t;

typedef enum {
	fasdma_none,					
	fasdma_pio,					
	fasdma_pseudo,					
	fasdma_real_block,				
	fasdma_real_all					
} fasdmatype_t;

typedef enum {
	neg_wait,					
	neg_inprogress,					
	neg_complete,					
	neg_targcomplete,				
	neg_invalid					
} neg_t;

#define MAGIC	0x441296bdUL
#define NR_MSGS	8

#define FASCAP_DMA		(1 << 0)
#define FASCAP_PSEUDODMA	(1 << 1)

typedef struct {
	unsigned long		magic_start;
	spinlock_t		host_lock;
	struct Scsi_Host	*host;			
	struct scsi_cmnd	*SCpnt;			
	struct scsi_cmnd	*origSCpnt;		
	struct scsi_cmnd	*reqSCpnt;		
	struct scsi_cmnd	*rstSCpnt;		
	struct scsi_cmnd	*pending_SCpnt[8];	
	int			next_pending;		

	wait_queue_head_t	eh_wait;
	struct timer_list	eh_timer;
	unsigned int		rst_dev_status;
	unsigned int		rst_bus_status;

	
	struct {
		phase_t		phase;			
		void __iomem	*io_base;		
		unsigned int	io_shift;		
		unsigned char	cfg[4];			
		const char	*type;			
		unsigned int	irq;			
		int		dma;			

		struct scsi_pointer	SCp;			

		MsgQueue_t	msgs;			

		unsigned int	async_stp;		
		unsigned char	msgin_fifo;		
		unsigned char	message[256];		

		unsigned char	disconnectable:1;	
		unsigned char	aborting:1;		
	} scsi;

	
	struct {
		unsigned int	queues;
		unsigned int	removes;
		unsigned int	fins;
		unsigned int	reads;
		unsigned int	writes;
		unsigned int	miscs;
		unsigned int	disconnects;
		unsigned int	aborts;
		unsigned int	bus_resets;
		unsigned int	host_resets;
	} stats;

	
	struct {
		unsigned char	clockrate;		
		unsigned char	select_timeout;		
		unsigned char	sync_max_depth;		
		unsigned char	wide_max_size;		
		unsigned char	cntl3;			
		unsigned int	asyncperiod;		
		unsigned int	capabilities;		
		unsigned int	disconnect_ok:1;	
	} ifcfg;

	
	struct {
	    	Queue_t		issue;			
    		Queue_t		disconnected;		
	} queues;

	
	struct fas216_device {
		unsigned char	disconnect_ok:1;	
		unsigned char	parity_enabled:1;	
		unsigned char	parity_check:1;		
		unsigned char	period;			
		unsigned char	stp;			
		unsigned char	sof;			
		unsigned char	wide_xfer;		
		neg_t		sync_state;		
		neg_t		wide_state;		
	} device[8];
	unsigned long	busyluns[64/sizeof(unsigned long)];

	
	struct {
		fasdmatype_t	transfer_type;		
		fasdmatype_t	(*setup) (struct Scsi_Host *host, struct scsi_pointer *SCp, fasdmadir_t direction, fasdmatype_t min_dma);
		void		(*pseudo)(struct Scsi_Host *host, struct scsi_pointer *SCp, fasdmadir_t direction, int transfer);
		void		(*stop)  (struct Scsi_Host *host, struct scsi_pointer *SCp);
	} dma;

	
	int			internal_done;		
	struct scsi_eh_save	ses;		
	unsigned long		magic_end;
} FAS216_Info;

extern int fas216_init (struct Scsi_Host *instance);

extern int fas216_add (struct Scsi_Host *instance, struct device *dev);

extern int fas216_queue_command(struct Scsi_Host *h, struct scsi_cmnd *SCpnt);

extern int fas216_noqueue_command(struct Scsi_Host *, struct scsi_cmnd *);

extern irqreturn_t fas216_intr (FAS216_Info *info);

extern void fas216_remove (struct Scsi_Host *instance);

extern void fas216_release (struct Scsi_Host *instance);

extern int fas216_print_host(FAS216_Info *info, char *buffer);
extern int fas216_print_stats(FAS216_Info *info, char *buffer);
extern int fas216_print_devices(FAS216_Info *info, char *buffer);

extern int fas216_eh_abort(struct scsi_cmnd *SCpnt);

extern int fas216_eh_device_reset(struct scsi_cmnd *SCpnt);

extern int fas216_eh_bus_reset(struct scsi_cmnd *SCpnt);

extern int fas216_eh_host_reset(struct scsi_cmnd *SCpnt);

#endif 
