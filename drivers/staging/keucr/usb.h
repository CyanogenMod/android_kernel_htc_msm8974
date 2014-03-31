
#ifndef _USB_H_
#define _USB_H_

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <scsi/scsi_host.h>
#include "common.h"

struct us_data;
struct scsi_cmnd;


struct us_unusual_dev {
	const char* vendorName;
	const char* productName;
	__u8  useProtocol;
	__u8  useTransport;
	int (*initFunction)(struct us_data *);
};

#define REG_CARD_STATUS     0xFF83
#define REG_HW_TRAP1        0xFF89

#define SS_SUCCESS                  0x00      
#define SS_NOT_READY                0x02
#define SS_MEDIUM_ERR               0x03
#define SS_HW_ERR                   0x04
#define SS_ILLEGAL_REQUEST          0x05
#define SS_UNIT_ATTENTION           0x06

#define SD_INIT1_PATTERN   1
#define SD_INIT2_PATTERN   2
#define SD_RW_PATTERN      3
#define MS_INIT_PATTERN    4
#define MSP_RW_PATTERN     5
#define MS_RW_PATTERN      6
#define SM_INIT_PATTERN    7
#define SM_RW_PATTERN      8

#define FDIR_WRITE        0
#define FDIR_READ         1

typedef struct _SD_STATUS {
    BYTE    Insert:1;
    BYTE    Ready:1;
    BYTE    MediaChange:1;
    BYTE    IsMMC:1;
    BYTE    HiCapacity:1;
    BYTE    HiSpeed:1;
    BYTE    WtP:1;
    BYTE    Reserved:1;
} SD_STATUS, *PSD_STATUS;

typedef struct _MS_STATUS {
    BYTE    Insert:1;
    BYTE    Ready:1;
    BYTE    MediaChange:1;
    BYTE    IsMSPro:1;
    BYTE    IsMSPHG:1;
    BYTE    Reserved1:1;
    BYTE    WtP:1;
    BYTE    Reserved2:1;
} MS_STATUS, *PMS_STATUS;

typedef struct _SM_STATUS {
    BYTE    Insert:1;
    BYTE    Ready:1;
    BYTE    MediaChange:1;
    BYTE    Reserved:3;
    BYTE    WtP:1;
    BYTE    IsMS:1;
} SM_STATUS, *PSM_STATUS;

#define SD_BLOCK_LEN                            9       

#define US_FLIDX_URB_ACTIVE	0	
#define US_FLIDX_SG_ACTIVE	1	
#define US_FLIDX_ABORTING	2	
#define US_FLIDX_DISCONNECTING	3	
#define US_FLIDX_RESETTING	4	
#define US_FLIDX_TIMED_OUT	5	
#define US_FLIDX_DONT_SCAN	6	


#define USB_STOR_STRING_LEN 32


#define US_IOBUF_SIZE		64	
#define US_SENSE_SIZE		18	

typedef int (*trans_cmnd)(struct scsi_cmnd *, struct us_data*);
typedef int (*trans_reset)(struct us_data*);
typedef void (*proto_cmnd)(struct scsi_cmnd*, struct us_data*);
typedef void (*extra_data_destructor)(void *);	
typedef void (*pm_hook)(struct us_data *, int);	

#define US_SUSPEND	0
#define US_RESUME	1

struct us_data {
	struct mutex		dev_mutex;	 
	struct usb_device	*pusb_dev;	 
	struct usb_interface	*pusb_intf;	 
	struct us_unusual_dev   *unusual_dev;	 
	unsigned long		fflags;		 
	unsigned long		dflags;		 
	unsigned int		send_bulk_pipe;	 
	unsigned int		recv_bulk_pipe;
	unsigned int		send_ctrl_pipe;
	unsigned int		recv_ctrl_pipe;
	unsigned int		recv_intr_pipe;

	
	char			*transport_name;
	char			*protocol_name;
	__le32			bcs_signature;
	u8			subclass;
	u8			protocol;
	u8			max_lun;

	u8			ifnum;		 
	u8			ep_bInterval;	 

	
	trans_cmnd		transport;	 
	trans_reset		transport_reset; 
	proto_cmnd		proto_handler;	 

	
	struct scsi_cmnd	*srb;		 
	unsigned int		tag;		 

	
	struct urb		*current_urb;	 
	struct usb_ctrlrequest	*cr;		 
	struct usb_sg_request	current_sg;	 
	unsigned char		*iobuf;		 
	unsigned char		*sensebuf;	 
	dma_addr_t		cr_dma;		 
	dma_addr_t		iobuf_dma;
	struct task_struct	*ctl_thread;	 

	
	struct completion	cmnd_ready;	 
	struct completion	notify;		 
	wait_queue_head_t	delay_wait;	 
	struct completion	scanning_done;	 

	
	void			*extra;		 
	extra_data_destructor	extra_destructor;
#ifdef CONFIG_PM
	pm_hook			suspend_resume_hook;
#endif
	
	SD_STATUS   SD_Status;
	MS_STATUS   MS_Status;
	SM_STATUS   SM_Status;

	
	
	WORD        SD_Block_Mult;
	BYTE        SD_READ_BL_LEN;
	WORD        SD_C_SIZE;
	BYTE        SD_C_SIZE_MULT;

	
	BYTE        SD_SPEC_VER;
	BYTE        SD_CSD_VER;
	BYTE        SD20_HIGH_CAPACITY;
	DWORD       HC_C_SIZE;
	BYTE        MMC_SPEC_VER;
	BYTE        MMC_BusWidth;
	BYTE        MMC_HIGH_CAPACITY;
	
	
	BOOLEAN             MS_SWWP;
	DWORD               MSP_TotalBlock;
	
	BOOLEAN             MS_IsRWPage;
	WORD                MS_Model;

	
	BYTE		SM_DeviceID;
	BYTE		SM_CardID;

	PBYTE		testbuf;
	BYTE		BIN_FLAG;
	DWORD		bl_num;
	int		SrbStatus;
	
	
	BOOLEAN         Power_IsResum;	
};

static inline struct Scsi_Host *us_to_host(struct us_data *us) {
	return container_of((void *) us, struct Scsi_Host, hostdata);
}
static inline struct us_data *host_to_us(struct Scsi_Host *host) {
	return (struct us_data *) host->hostdata;
}

extern void fill_inquiry_response(struct us_data *us,
	unsigned char *data, unsigned int data_len);

#define scsi_unlock(host)	spin_unlock_irq(host->host_lock)
#define scsi_lock(host)		spin_lock_irq(host->host_lock)

#endif
