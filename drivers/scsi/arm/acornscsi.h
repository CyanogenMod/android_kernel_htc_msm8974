/*
 *  linux/drivers/acorn/scsi/acornscsi.h
 *
 *  Copyright (C) 1997 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Acorn SCSI driver
 */
#ifndef ACORNSCSI_H
#define ACORNSCSI_H

#define SBIC_OWNID		0
#define OWNID_FS1		(1<<7)
#define OWNID_FS2		(1<<6)
#define OWNID_EHP		(1<<4)
#define OWNID_EAF		(1<<3)

#define SBIC_CTRL		1
#define CTRL_DMAMODE		(1<<7)
#define CTRL_DMADBAMODE		(1<<6)
#define CTRL_DMABURST		(1<<5)
#define CTRL_DMAPOLLED		0
#define CTRL_HHP		(1<<4)
#define CTRL_EDI		(1<<3)
#define CTRL_IDI		(1<<2)
#define CTRL_HA			(1<<1)
#define CTRL_HSP		(1<<0)

#define SBIC_TIMEOUT		2
#define SBIC_TOTSECTS		3
#define SBIC_TOTHEADS		4
#define SBIC_TOTCYLH		5
#define SBIC_TOTCYLL		6
#define SBIC_LOGADDRH		7
#define SBIC_LOGADDRM2		8
#define SBIC_LOGADDRM1		9
#define SBIC_LOGADDRL		10
#define SBIC_SECTORNUM		11
#define SBIC_HEADNUM		12
#define SBIC_CYLH		13
#define SBIC_CYLL		14
#define SBIC_TARGETLUN		15
#define TARGETLUN_TLV		(1<<7)
#define TARGETLUN_DOK		(1<<6)

#define SBIC_CMNDPHASE		16
#define SBIC_SYNCHTRANSFER	17
#define SYNCHTRANSFER_OF0	0x00
#define SYNCHTRANSFER_OF1	0x01
#define SYNCHTRANSFER_OF2	0x02
#define SYNCHTRANSFER_OF3	0x03
#define SYNCHTRANSFER_OF4	0x04
#define SYNCHTRANSFER_OF5	0x05
#define SYNCHTRANSFER_OF6	0x06
#define SYNCHTRANSFER_OF7	0x07
#define SYNCHTRANSFER_OF8	0x08
#define SYNCHTRANSFER_OF9	0x09
#define SYNCHTRANSFER_OF10	0x0A
#define SYNCHTRANSFER_OF11	0x0B
#define SYNCHTRANSFER_OF12	0x0C
#define SYNCHTRANSFER_8DBA	0x00
#define SYNCHTRANSFER_2DBA	0x20
#define SYNCHTRANSFER_3DBA	0x30
#define SYNCHTRANSFER_4DBA	0x40
#define SYNCHTRANSFER_5DBA	0x50
#define SYNCHTRANSFER_6DBA	0x60
#define SYNCHTRANSFER_7DBA	0x70

#define SBIC_TRANSCNTH		18
#define SBIC_TRANSCNTM		19
#define SBIC_TRANSCNTL		20
#define SBIC_DESTID		21
#define DESTID_SCC		(1<<7)
#define DESTID_DPD		(1<<6)

#define SBIC_SOURCEID		22
#define SOURCEID_ER		(1<<7)
#define SOURCEID_ES		(1<<6)
#define SOURCEID_DSP		(1<<5)
#define SOURCEID_SIV		(1<<4)

#define SBIC_SSR		23
#define SBIC_CMND		24
#define CMND_RESET		0x00
#define CMND_ABORT		0x01
#define CMND_ASSERTATN		0x02
#define CMND_NEGATEACK		0x03
#define CMND_DISCONNECT		0x04
#define CMND_RESELECT		0x05
#define CMND_SELWITHATN		0x06
#define CMND_SELECT		0x07
#define CMND_SELECTATNTRANSFER	0x08
#define CMND_SELECTTRANSFER	0x09
#define CMND_RESELECTRXDATA	0x0A
#define CMND_RESELECTTXDATA	0x0B
#define CMND_WAITFORSELRECV	0x0C
#define CMND_SENDSTATCMD	0x0D
#define CMND_SENDDISCONNECT	0x0E
#define CMND_SETIDI		0x0F
#define CMND_RECEIVECMD		0x10
#define CMND_RECEIVEDTA		0x11
#define CMND_RECEIVEMSG		0x12
#define CMND_RECEIVEUSP		0x13
#define CMND_SENDCMD		0x14
#define CMND_SENDDATA		0x15
#define CMND_SENDMSG		0x16
#define CMND_SENDUSP		0x17
#define CMND_TRANSLATEADDR	0x18
#define CMND_XFERINFO		0x20
#define CMND_SBT		(1<<7)

#define SBIC_DATA		25
#define SBIC_ASR		26
#define ASR_INT			(1<<7)
#define ASR_LCI			(1<<6)
#define ASR_BSY			(1<<5)
#define ASR_CIP			(1<<4)
#define ASR_PE			(1<<1)
#define ASR_DBR			(1<<0)

#define DMAC_INIT		0x00
#define INIT_8BIT		(1)

#define DMAC_CHANNEL		0x80
#define CHANNEL_0		0x00
#define CHANNEL_1		0x01
#define CHANNEL_2		0x02
#define CHANNEL_3		0x03

#define DMAC_TXCNTLO		0x01
#define DMAC_TXCNTHI		0x81
#define DMAC_TXADRLO		0x02
#define DMAC_TXADRMD		0x82
#define DMAC_TXADRHI		0x03

#define DMAC_DEVCON0		0x04
#define DEVCON0_AKL		(1<<7)
#define DEVCON0_RQL		(1<<6)
#define DEVCON0_EXW		(1<<5)
#define DEVCON0_ROT		(1<<4)
#define DEVCON0_CMP		(1<<3)
#define DEVCON0_DDMA		(1<<2)
#define DEVCON0_AHLD		(1<<1)
#define DEVCON0_MTM		(1<<0)

#define DMAC_DEVCON1		0x84
#define DEVCON1_WEV		(1<<1)
#define DEVCON1_BHLD		(1<<0)

#define DMAC_MODECON		0x05
#define MODECON_WOED		0x01
#define MODECON_VERIFY		0x00
#define MODECON_READ		0x04
#define MODECON_WRITE		0x08
#define MODECON_AUTOINIT	0x10
#define MODECON_ADDRDIR		0x20
#define MODECON_DEMAND		0x00
#define MODECON_SINGLE		0x40
#define MODECON_BLOCK		0x80
#define MODECON_CASCADE		0xC0

#define DMAC_STATUS		0x85
#define STATUS_TC0		(1<<0)
#define STATUS_RQ0		(1<<4)

#define DMAC_TEMPLO		0x06
#define DMAC_TEMPHI		0x86
#define DMAC_REQREG		0x07
#define DMAC_MASKREG		0x87
#define MASKREG_M0		0x01
#define MASKREG_M1		0x02
#define MASKREG_M2		0x04
#define MASKREG_M3		0x08


#define MASK_ON		(MASKREG_M3|MASKREG_M2|MASKREG_M1|MASKREG_M0)
#define MASK_OFF	(MASKREG_M3|MASKREG_M2|MASKREG_M1)

typedef enum {
    PHASE_IDLE,					
    PHASE_CONNECTING,				
    PHASE_CONNECTED,				
    PHASE_MSGOUT,				
    PHASE_RECONNECTED,				
    PHASE_COMMANDPAUSED,			
    PHASE_COMMAND,				
    PHASE_DATAOUT,				
    PHASE_DATAIN,				
    PHASE_STATUSIN,				
    PHASE_MSGIN,				
    PHASE_DONE,					
    PHASE_ABORTED,				
    PHASE_DISCONNECT,				
} phase_t;

typedef enum {
    INTR_IDLE,					
    INTR_NEXT_COMMAND,				
    INTR_PROCESSING,				
} intr_ret_t;

typedef enum {
    DMA_OUT,					
    DMA_IN					
} dmadir_t;

typedef enum {					
    SYNC_ASYNCHRONOUS,				
    SYNC_NEGOCIATE,				
    SYNC_SENT_REQUEST,				
    SYNC_COMPLETED,				
} syncxfer_t;

typedef enum {					
    CMD_READ,					
    CMD_WRITE,					
    CMD_MISC,					
} cmdtype_t;

typedef enum {					
    DATADIR_IN,					
    DATADIR_OUT					
} datadir_t;

#include "queue.h"
#include "msgqueue.h"

#define STATUS_BUFFER_SIZE	32
struct status_entry {
	unsigned long	when;
	unsigned char	ssr;
	unsigned char	ph;
	unsigned char	irq;
	unsigned char	unused;
};

#define ADD_STATUS(_q,_ssr,_ph,_irq) \
({									\
	host->status[(_q)][host->status_ptr[(_q)]].when = jiffies;	\
	host->status[(_q)][host->status_ptr[(_q)]].ssr  = (_ssr);	\
	host->status[(_q)][host->status_ptr[(_q)]].ph   = (_ph);	\
	host->status[(_q)][host->status_ptr[(_q)]].irq  = (_irq);	\
	host->status_ptr[(_q)] = (host->status_ptr[(_q)] + 1) & (STATUS_BUFFER_SIZE - 1); \
})

typedef struct acornscsi_hostdata {
    
    struct Scsi_Host	*host;			
    struct scsi_cmnd	*SCpnt;			
    struct scsi_cmnd	*origSCpnt;		
    void __iomem	*base;			
    void __iomem	*fast;			

    
    struct {
	unsigned int	irq;			
	phase_t		phase;			

	struct {
	    unsigned char	target;		
	    unsigned char	lun;		
	    unsigned char	tag;		
	} reconnected;

	struct scsi_pointer	SCp;			

	MsgQueue_t	msgs;

	unsigned short	last_message;		
	unsigned char	disconnectable:1;	
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
	unsigned int	resets;
    } stats;

    
    struct {
	Queue_t		issue;			
	Queue_t		disconnected;		
    } queues;

    
    struct {
	unsigned char	sync_xfer;		
	syncxfer_t	sync_state;		
	unsigned char	disconnect_ok:1;	
    } device[8];
    unsigned long	busyluns[64 / sizeof(unsigned long)];

    
    struct {
	unsigned int	free_addr;		
	unsigned int	start_addr;		
	dmadir_t	direction;		
	unsigned int	transferred;		
	unsigned int	xfer_start;		
	unsigned int	xfer_length;		
	char		*xfer_ptr;		
	unsigned char	xfer_required:1;	
	unsigned char	xfer_setup:1;		
	unsigned char	xfer_done:1;		
    } dma;

    
    struct {
	unsigned char	page_reg;		
    } card;

    unsigned char status_ptr[9];
    struct status_entry status[9][STATUS_BUFFER_SIZE];
} AS_Host;

#endif 
