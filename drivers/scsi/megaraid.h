#ifndef __MEGARAID_H__
#define __MEGARAID_H__

#include <linux/spinlock.h>
#include <linux/mutex.h>

#define MEGARAID_VERSION	\
	"v2.00.4 (Release Date: Thu Feb 9 08:51:30 EST 2006)\n"


#define MEGA_HAVE_COALESCING	0

#define MEGA_HAVE_CLUSTERING	1

#define MEGA_HAVE_STATS		0

#define MEGA_HAVE_ENH_PROC	1

#define MAX_DEV_TYPE	32

#ifndef PCI_VENDOR_ID_LSI_LOGIC
#define PCI_VENDOR_ID_LSI_LOGIC		0x1000
#endif

#ifndef PCI_VENDOR_ID_AMI
#define PCI_VENDOR_ID_AMI		0x101E
#endif

#ifndef PCI_VENDOR_ID_DELL
#define PCI_VENDOR_ID_DELL		0x1028
#endif

#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL		0x8086
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID
#define PCI_DEVICE_ID_AMI_MEGARAID	0x9010
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID2
#define PCI_DEVICE_ID_AMI_MEGARAID2	0x9060
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID3
#define PCI_DEVICE_ID_AMI_MEGARAID3	0x1960
#endif

#define PCI_DEVICE_ID_DISCOVERY		0x000E
#define PCI_DEVICE_ID_PERC4_DI		0x000F
#define PCI_DEVICE_ID_PERC4_QC_VERDE	0x0407

#define	AMI_SUBSYS_VID			0x101E
#define DELL_SUBSYS_VID			0x1028
#define	HP_SUBSYS_VID			0x103C
#define LSI_SUBSYS_VID			0x1000
#define INTEL_SUBSYS_VID		0x8086

#define HBA_SIGNATURE	      		0x3344
#define HBA_SIGNATURE_471	  	0xCCCC
#define HBA_SIGNATURE_64BIT		0x0299

#define MBOX_BUSY_WAIT			10	
#define DEFAULT_INITIATOR_ID	7

#define MAX_SGLIST		64	
#define MIN_SGLIST		26	
#define MAX_COMMANDS		126
#define CMDID_INT_CMDS		MAX_COMMANDS+1	

#define MAX_CDB_LEN	     	10
#define MAX_EXT_CDB_LEN		16	

#define DEF_CMD_PER_LUN		63
#define MAX_CMD_PER_LUN		MAX_COMMANDS
#define MAX_FIRMWARE_STATUS	46
#define MAX_XFER_PER_CMD	(64*1024)
#define MAX_SECTORS_PER_IO	128

#define MAX_LOGICAL_DRIVES_40LD		40
#define FC_MAX_PHYSICAL_DEVICES		256
#define MAX_LOGICAL_DRIVES_8LD		8
#define MAX_CHANNELS			5
#define MAX_TARGET			15
#define MAX_PHYSICAL_DRIVES		MAX_CHANNELS*MAX_TARGET
#define MAX_ROW_SIZE_40LD		32
#define MAX_ROW_SIZE_8LD		8
#define MAX_SPAN_DEPTH			8

#define NVIRT_CHAN		4	
struct mbox_out {
	 u8 cmd;
	 u8 cmdid;
	 u16 numsectors;
	 u32 lba;
	 u32 xferaddr;
	 u8 logdrv;
	 u8 numsgelements;
	 u8 resvd;
} __attribute__ ((packed));

struct mbox_in {
	 volatile u8 busy;
	 volatile u8 numstatus;
	 volatile u8 status;
	 volatile u8 completed[MAX_FIRMWARE_STATUS];
	volatile u8 poll;
	volatile u8 ack;
} __attribute__ ((packed));

typedef struct {
	struct mbox_out	m_out;
	struct mbox_in	m_in;
} __attribute__ ((packed)) mbox_t;

typedef struct {
	u32 xfer_segment_lo;
	u32 xfer_segment_hi;
	mbox_t mbox;
} __attribute__ ((packed)) mbox64_t;


#define MAX_REQ_SENSE_LEN       0x20

typedef struct {
	u8 timeout:3;		
	u8 ars:1;
	u8 reserved:3;
	u8 islogical:1;
	u8 logdrv;		
	u8 channel;		
	u8 target;		
	u8 queuetag;		
	u8 queueaction;		
	u8 cdb[MAX_CDB_LEN];
	u8 cdblen;
	u8 reqsenselen;
	u8 reqsensearea[MAX_REQ_SENSE_LEN];
	u8 numsgelements;
	u8 scsistatus;
	u32 dataxferaddr;
	u32 dataxferlen;
} __attribute__ ((packed)) mega_passthru;


typedef struct {
	u8 timeout:3;		
	u8 ars:1;
	u8 rsvd1:1;
	u8 cd_rom:1;
	u8 rsvd2:1;
	u8 islogical:1;
	u8 logdrv;		
	u8 channel;		
	u8 target;		
	u8 queuetag;		
	u8 queueaction;		
	u8 cdblen;
	u8 rsvd3;
	u8 cdb[MAX_EXT_CDB_LEN];
	u8 numsgelements;
	u8 status;
	u8 reqsenselen;
	u8 reqsensearea[MAX_REQ_SENSE_LEN];
	u8 rsvd4;
	u32 dataxferaddr;
	u32 dataxferlen;
} __attribute__ ((packed)) mega_ext_passthru;

typedef struct {
	u64 address;
	u32 length;
} __attribute__ ((packed)) mega_sgl64;

typedef struct {
	u32 address;
	u32 length;
} __attribute__ ((packed)) mega_sglist;


typedef struct {
	int	idx;
	u32	state;
	struct list_head	list;
	u8	raw_mbox[66];
	u32	dma_type;
	u32	dma_direction;

	Scsi_Cmnd	*cmd;
	dma_addr_t	dma_h_bulkdata;
	dma_addr_t	dma_h_sgdata;

	mega_sglist	*sgl;
	mega_sgl64	*sgl64;
	dma_addr_t	sgl_dma_addr;

	mega_passthru		*pthru;
	dma_addr_t		pthru_dma_addr;
	mega_ext_passthru	*epthru;
	dma_addr_t		epthru_dma_addr;
} scb_t;

#define SCB_FREE	0x0000	
#define SCB_ACTIVE	0x0001	
#define SCB_PENDQ	0x0002	
#define SCB_ISSUED	0x0004	
#define SCB_ABORT	0x0008	
#define SCB_RESET	0x0010	

typedef struct {
	u32	data_size; 

	u32	config_signature;

	u8	fw_version[16];		
	u8	bios_version[16];	
	u8	product_name[80];	

	u8	max_commands;		
	u8	nchannels;		
	u8	fc_loop_present;	
	u8	mem_type;		

	u32	signature;
	u16	dram_size;		
	u16	subsysid;

	u16	subsysvid;
	u8	notify_counters;
	u8	pad1k[889];		
} __attribute__ ((packed)) mega_product_info;

struct notify {
	u32 global_counter;	

	u8 param_counter;	
	u8 param_id;		
	u16 param_val;		

	u8 write_config_counter;	
	u8 write_config_rsvd[3];

	u8 ldrv_op_counter;	
	u8 ldrv_opid;		
	u8 ldrv_opcmd;		
	u8 ldrv_opstatus;	

	u8 ldrv_state_counter;	
	u8 ldrv_state_id;		
	u8 ldrv_state_new;	
	u8 ldrv_state_old;	

	u8 pdrv_state_counter;	
	u8 pdrv_state_id;		
	u8 pdrv_state_new;	
	u8 pdrv_state_old;	

	u8 pdrv_fmt_counter;	
	u8 pdrv_fmt_id;		
	u8 pdrv_fmt_val;		
	u8 pdrv_fmt_rsvd;

	u8 targ_xfer_counter;	
	u8 targ_xfer_id;	
	u8 targ_xfer_val;		
	u8 targ_xfer_rsvd;

	u8 fcloop_id_chg_counter;	
	u8 fcloopid_pdrvid;		
	u8 fcloop_id0;			
	u8 fcloop_id1;			

	u8 fcloop_state_counter;	
	u8 fcloop_state0;		
	u8 fcloop_state1;		
	u8 fcloop_state_rsvd;
} __attribute__ ((packed));

#define MAX_NOTIFY_SIZE     0x80
#define CUR_NOTIFY_SIZE     sizeof(struct notify)

typedef struct {
	u32	data_size; 

	struct notify notify;

	u8	notify_rsvd[MAX_NOTIFY_SIZE - CUR_NOTIFY_SIZE];

	u8	rebuild_rate;		
	u8	cache_flush_interval;	
	u8	sense_alert;
	u8	drive_insert_count;	

	u8	battery_status;
	u8	num_ldrv;		
	u8	recon_state[MAX_LOGICAL_DRIVES_40LD / 8];	
	u16	ldrv_op_status[MAX_LOGICAL_DRIVES_40LD / 8]; 

	u32	ldrv_size[MAX_LOGICAL_DRIVES_40LD];
	u8	ldrv_prop[MAX_LOGICAL_DRIVES_40LD];
	u8	ldrv_state[MAX_LOGICAL_DRIVES_40LD];
	u8	pdrv_state[FC_MAX_PHYSICAL_DEVICES];
	u16	pdrv_format[FC_MAX_PHYSICAL_DEVICES / 16];

	u8	targ_xfer[80];	
	u8	pad1k[263];	
} __attribute__ ((packed)) mega_inquiry3;


typedef struct {
	u8	max_commands;	
	u8	rebuild_rate;	
	u8	max_targ_per_chan;	
	u8	nchannels;	
	u8	fw_version[4];	
	u16	age_of_flash;	
	u8	chip_set_value;	
	u8	dram_size;	
	u8	cache_flush_interval;	
	u8	bios_version[4];
	u8	board_type;
	u8	sense_alert;
	u8	write_config_count;	
	u8	drive_inserted_count;	
	u8	inserted_drive;	
	u8	battery_status;	
	u8	dec_fault_bus_info;
} __attribute__ ((packed)) mega_adp_info;


typedef struct {
	u8	num_ldrv;	
	u8	rsvd[3];
	u32	ldrv_size[MAX_LOGICAL_DRIVES_8LD];
	u8	ldrv_prop[MAX_LOGICAL_DRIVES_8LD];
	u8	ldrv_state[MAX_LOGICAL_DRIVES_8LD];
} __attribute__ ((packed)) mega_ldrv_info;

typedef struct {
	u8	pdrv_state[MAX_PHYSICAL_DRIVES];
	u8	rsvd;
} __attribute__ ((packed)) mega_pdrv_info;

typedef struct {
	mega_adp_info	adapter_info;
	mega_ldrv_info	logdrv_info;
	mega_pdrv_info	pdrv_info;
} __attribute__ ((packed)) mraid_inquiry;


typedef struct {
	mraid_inquiry	raid_inq;
	u16	phys_drv_format[MAX_CHANNELS];
	u8	stack_attn;
	u8	modem_status;
	u8	rsvd[2];
} __attribute__ ((packed)) mraid_ext_inquiry;


typedef struct {
	u8	channel;
	u8	target;
}__attribute__ ((packed)) adp_device;

typedef struct {
	u32		start_blk;	
	u32		num_blks;	
	adp_device	device[MAX_ROW_SIZE_40LD];
}__attribute__ ((packed)) adp_span_40ld;

typedef struct {
	u32		start_blk;	
	u32		num_blks;	
	adp_device	device[MAX_ROW_SIZE_8LD];
}__attribute__ ((packed)) adp_span_8ld;

typedef struct {
	u8	span_depth;	
	u8	level;		
	u8	read_ahead;	
	u8	stripe_sz;	
	u8	status;		
	u8	write_mode;	
	u8	direct_io;	
	u8	row_size;	
} __attribute__ ((packed)) logdrv_param;

typedef struct {
	logdrv_param	lparam;
	adp_span_40ld	span[MAX_SPAN_DEPTH];
}__attribute__ ((packed)) logdrv_40ld;

typedef struct {
	logdrv_param	lparam;
	adp_span_8ld	span[MAX_SPAN_DEPTH];
}__attribute__ ((packed)) logdrv_8ld;

typedef struct {
	u8	type;		
	u8	cur_status;	
	u8	tag_depth;	
	u8	sync_neg;	
	u32	size;		
}__attribute__ ((packed)) phys_drv;

typedef struct {
	u8		nlog_drives;		
	u8		resvd[3];
	logdrv_40ld	ldrv[MAX_LOGICAL_DRIVES_40LD];
	phys_drv	pdrv[MAX_PHYSICAL_DRIVES];
}__attribute__ ((packed)) disk_array_40ld;

typedef struct {
	u8		nlog_drives;	
	u8		resvd[3];
	logdrv_8ld	ldrv[MAX_LOGICAL_DRIVES_8LD];
	phys_drv	pdrv[MAX_PHYSICAL_DRIVES];
}__attribute__ ((packed)) disk_array_8ld;


#define IOCTL_MAX_DATALEN       4096

struct uioctl_t {
	u32 inlen;
	u32 outlen;
	union {
		u8 fca[16];
		struct {
			u8 opcode;
			u8 subopcode;
			u16 adapno;
#if BITS_PER_LONG == 32
			u8 *buffer;
			u8 pad[4];
#endif
#if BITS_PER_LONG == 64
			u8 *buffer;
#endif
			u32 length;
		} __attribute__ ((packed)) fcs;
	} __attribute__ ((packed)) ui;
	u8 mbox[18];		
	mega_passthru pthru;
#if BITS_PER_LONG == 32
	char __user *data;		
	char pad[4];
#endif
#if BITS_PER_LONG == 64
	char __user *data;
#endif
} __attribute__ ((packed));

#define MAX_CONTROLLERS 32

struct mcontroller {
	u64 base;
	u8 irq;
	u8 numldrv;
	u8 pcibus;
	u16 pcidev;
	u8 pcifun;
	u16 pciid;
	u16 pcivendor;
	u8 pcislot;
	u32 uid;
};

typedef struct {
	u8	cmd;
	u8	cmdid;
	u8	opcode;
	u8	subopcode;
	u32	lba;
	u32	xferaddr;
	u8	logdrv;
	u8	rsvd[3];
	u8	numstatus;
	u8	status;
} __attribute__ ((packed)) megacmd_t;

#define MEGAIOC_MAGIC  	'm'

#define MEGAIOC_QNADAP		'm'	
#define MEGAIOC_QDRVRVER	'e'	
#define MEGAIOC_QADAPINFO   	'g'	
#define MKADAP(adapno)	  	(MEGAIOC_MAGIC << 8 | (adapno) )
#define GETADAP(mkadap)	 	( (mkadap) ^ MEGAIOC_MAGIC << 8 )


#define VENDOR_SPECIFIC_COMMANDS	0xE0
#define MEGA_INTERNAL_CMD		VENDOR_SPECIFIC_COMMANDS + 0x01

#define USCSICMD	VENDOR_SPECIFIC_COMMANDS

#define UIOC_RD		0x00001
#define UIOC_WR		0x00002

#define MBOX_CMD	0x00000	
#define GET_DRIVER_VER	0x10000	
#define GET_N_ADAP	0x20000	
#define GET_ADAP_INFO	0x30000	
#define GET_CAP		0x40000	
#define GET_STATS	0x50000	


typedef struct {
	char		signature[8];	
	u32		opcode;		
	u32		adapno;		
	union {
		u8	__raw_mbox[18];
		void __user *__uaddr; 
	}__ua;

#define uioc_rmbox	__ua.__raw_mbox
#define MBOX(uioc)	((megacmd_t *)&((uioc).__ua.__raw_mbox[0]))
#define MBOX_P(uioc)	((megacmd_t __user *)&((uioc)->__ua.__raw_mbox[0]))
#define uioc_uaddr	__ua.__uaddr

	u32		xferlen;	
	u32		flags;		
}nitioctl_t;


typedef struct {
	int	num_ldrv;	
	u32	nreads[MAX_LOGICAL_DRIVES_40LD];	
	u32	nreadblocks[MAX_LOGICAL_DRIVES_40LD];	
	u32	nwrites[MAX_LOGICAL_DRIVES_40LD];	
	u32	nwriteblocks[MAX_LOGICAL_DRIVES_40LD];	
	u32	rd_errors[MAX_LOGICAL_DRIVES_40LD];	
	u32	wr_errors[MAX_LOGICAL_DRIVES_40LD];	
}megastat_t;


struct private_bios_data {
	u8	geometry:4;	
	u8	unused:4;	
	u8	boot_drv;	
	u8	rsvd[12];
	u16	cksum;	
} __attribute__ ((packed));





#define MEGA_MBOXCMD_LREAD	0x01
#define MEGA_MBOXCMD_LWRITE	0x02
#define MEGA_MBOXCMD_PASSTHRU	0x03
#define MEGA_MBOXCMD_ADPEXTINQ	0x04
#define MEGA_MBOXCMD_ADAPTERINQ	0x05
#define MEGA_MBOXCMD_LREAD64	0xA7
#define MEGA_MBOXCMD_LWRITE64	0xA8
#define MEGA_MBOXCMD_PASSTHRU64	0xC3
#define MEGA_MBOXCMD_EXTPTHRU	0xE3

#define MAIN_MISC_OPCODE	0xA4	
#define GET_MAX_SG_SUPPORT	0x01	

#define FC_NEW_CONFIG		0xA1
#define NC_SUBOP_PRODUCT_INFO	0x0E
#define NC_SUBOP_ENQUIRY3	0x0F
#define ENQ3_GET_SOLICITED_FULL	0x02
#define OP_DCMD_READ_CONFIG	0x04
#define NEW_READ_CONFIG_8LD	0x67
#define READ_CONFIG_8LD		0x07
#define FLUSH_ADAPTER		0x0A
#define FLUSH_SYSTEM		0xFE

#define	FC_DEL_LOGDRV		0xA4	
#define	OP_SUP_DEL_LOGDRV	0x2A	
#define OP_GET_LDID_MAP		0x18	
#define OP_DEL_LOGDRV		0x1C	

#define IS_BIOS_ENABLED		0x62
#define GET_BIOS		0x01
#define CHNL_CLASS		0xA9
#define GET_CHNL_CLASS		0x00
#define SET_CHNL_CLASS		0x01
#define CH_RAID			0x01
#define CH_SCSI			0x00
#define BIOS_PVT_DATA		0x40
#define GET_BIOS_PVT_DATA	0x00


#define MEGA_GET_TARGET_ID	0x7D
#define MEGA_CLUSTER_OP		0x70
#define MEGA_GET_CLUSTER_MODE	0x02
#define MEGA_CLUSTER_CMD	0x6E
#define MEGA_RESERVE_LD		0x01
#define MEGA_RELEASE_LD		0x02
#define MEGA_RESET_RESERVATIONS	0x03
#define MEGA_RESERVATION_STATUS	0x04
#define MEGA_RESERVE_PD		0x05
#define MEGA_RELEASE_PD		0x06


#define MEGA_BATT_MODULE_MISSING	0x01
#define MEGA_BATT_LOW_VOLTAGE		0x02
#define MEGA_BATT_TEMP_HIGH		0x04
#define MEGA_BATT_PACK_MISSING		0x08
#define MEGA_BATT_CHARGE_MASK		0x30
#define MEGA_BATT_CHARGE_DONE		0x00
#define MEGA_BATT_CHARGE_INPROG		0x10
#define MEGA_BATT_CHARGE_FAIL		0x20
#define MEGA_BATT_CYCLES_EXCEEDED	0x40

#define PDRV_UNCNF	0
#define PDRV_ONLINE	3
#define PDRV_FAILED	4
#define PDRV_RBLD	5
#define PDRV_HOTSPARE	6


#define RDRV_OFFLINE	0
#define RDRV_DEGRADED	1
#define RDRV_OPTIMAL	2
#define RDRV_DELETED	3

#define NO_READ_AHEAD		0
#define READ_AHEAD		1
#define ADAP_READ_AHEAD		2
#define WRMODE_WRITE_THRU	0
#define WRMODE_WRITE_BACK	1
#define CACHED_IO		0
#define DIRECT_IO		1


#define SCSI_LIST(scp) ((struct list_head *)(&(scp)->SCp))

typedef struct {
	int	this_id;	
	u32	flag;

	unsigned long		base;
	void __iomem		*mmio_base;

	
	mbox64_t	*una_mbox64;
	dma_addr_t	una_mbox64_dma;

	volatile mbox64_t	*mbox64;
	volatile mbox_t		*mbox;	
	dma_addr_t		mbox_dma;

	struct pci_dev	*dev;

	struct list_head	free_list;
	struct list_head	pending_list;
	struct list_head	completed_list;

	struct Scsi_Host	*host;

#define MEGA_BUFFER_SIZE (2*1024)
	u8		*mega_buffer;
	dma_addr_t	buf_dma_handle;

	mega_product_info	product_info;

	u8		max_cmds;
	scb_t		*scb_list;

	atomic_t	pend_cmds;	

#if MEGA_HAVE_STATS
	u32	nreads[MAX_LOGICAL_DRIVES_40LD];
	u32	nreadblocks[MAX_LOGICAL_DRIVES_40LD];
	u32	nwrites[MAX_LOGICAL_DRIVES_40LD];
	u32	nwriteblocks[MAX_LOGICAL_DRIVES_40LD];
	u32	rd_errors[MAX_LOGICAL_DRIVES_40LD];
	u32	wr_errors[MAX_LOGICAL_DRIVES_40LD];
#endif

	
	u8	numldrv;
	u8	fw_version[7];
	u8	bios_version[7];

#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*controller_proc_dir_entry;
	struct proc_dir_entry	*proc_read;
	struct proc_dir_entry	*proc_stat;
	struct proc_dir_entry	*proc_mbox;

#if MEGA_HAVE_ENH_PROC
	struct proc_dir_entry	*proc_rr;
	struct proc_dir_entry	*proc_battery;
#define MAX_PROC_CHANNELS	4
	struct proc_dir_entry	*proc_pdrvstat[MAX_PROC_CHANNELS];
	struct proc_dir_entry	*proc_rdrvstat[MAX_PROC_CHANNELS];
#endif

#endif

	int	has_64bit_addr;		
	int	support_ext_cdb;
	int	boot_ldrv_enabled;
	int	boot_ldrv;
	int	boot_pdrv_enabled;	
	int	boot_pdrv_ch;		
	int	boot_pdrv_tgt;		


	int	support_random_del;	
	int	read_ldidmap;	
	atomic_t	quiescent;	
	spinlock_t	lock;

	u8	logdrv_chan[MAX_CHANNELS+NVIRT_CHAN]; 
	int	mega_ch_class;

	u8	sglen;	

	unsigned char int_cdb[MAX_COMMAND_SIZE];
	scb_t			int_scb;
	struct mutex		int_mtx;	
	struct completion	int_waitq;	

	int	has_cluster;	
}adapter_t;


struct mega_hbas {
	int is_bios_enabled;
	adapter_t *hostdata_addr;
};


#define IN_ABORT	0x80000000L
#define IN_RESET	0x40000000L
#define BOARD_MEMMAP	0x20000000L
#define BOARD_IOMAP	0x10000000L
#define BOARD_40LD   	0x08000000L
#define BOARD_64BIT	0x04000000L

#define INTR_VALID			0x40

#define PCI_CONF_AMISIG			0xa0
#define PCI_CONF_AMISIG64		0xa4


#define MEGA_DMA_TYPE_NONE		0xFFFF
#define MEGA_BULK_DATA			0x0001
#define MEGA_SGLIST			0x0002


#define CMD_PORT	 	0x00
#define ACK_PORT	 	0x00
#define TOGGLE_PORT		0x01
#define INTR_PORT	  	0x0a

#define MBOX_BUSY_PORT     	0x00
#define MBOX_PORT0	 	0x04
#define MBOX_PORT1	 	0x05
#define MBOX_PORT2	 	0x06
#define MBOX_PORT3	 	0x07
#define ENABLE_MBOX_REGION 	0x0B

#define ISSUE_BYTE	 	0x10
#define ACK_BYTE	   	0x08
#define ENABLE_INTR_BYTE   	0xc0
#define DISABLE_INTR_BYTE  	0x00
#define VALID_INTR_BYTE    	0x40
#define MBOX_BUSY_BYTE     	0x10
#define ENABLE_MBOX_BYTE   	0x00


#define issue_command(adapter)	\
		outb_p(ISSUE_BYTE, (adapter)->base + CMD_PORT)

#define irq_state(adapter)	inb_p((adapter)->base + INTR_PORT)

#define set_irq_state(adapter, value)	\
		outb_p((value), (adapter)->base + INTR_PORT)

#define irq_ack(adapter)	\
		outb_p(ACK_BYTE, (adapter)->base + ACK_PORT)

#define irq_enable(adapter)	\
	outb_p(ENABLE_INTR_BYTE, (adapter)->base + TOGGLE_PORT)

#define irq_disable(adapter)	\
	outb_p(DISABLE_INTR_BYTE, (adapter)->base + TOGGLE_PORT)




const char *megaraid_info (struct Scsi_Host *);

static int mega_query_adapter(adapter_t *);
static int issue_scb(adapter_t *, scb_t *);
static int mega_setup_mailbox(adapter_t *);

static int megaraid_queue (struct Scsi_Host *, struct scsi_cmnd *);
static scb_t * mega_build_cmd(adapter_t *, Scsi_Cmnd *, int *);
static void __mega_runpendq(adapter_t *);
static int issue_scb_block(adapter_t *, u_char *);

static irqreturn_t megaraid_isr_memmapped(int, void *);
static irqreturn_t megaraid_isr_iomapped(int, void *);

static void mega_free_scb(adapter_t *, scb_t *);

static int megaraid_abort(Scsi_Cmnd *);
static int megaraid_reset(Scsi_Cmnd *);
static int megaraid_abort_and_reset(adapter_t *, Scsi_Cmnd *, int);
static int megaraid_biosparam(struct scsi_device *, struct block_device *,
		sector_t, int []);

static int mega_build_sglist (adapter_t *adapter, scb_t *scb,
			      u32 *buffer, u32 *length);
static int __mega_busywait_mbox (adapter_t *);
static void mega_rundoneq (adapter_t *);
static void mega_cmd_done(adapter_t *, u8 [], int, int);
static inline void mega_free_sgl (adapter_t *adapter);
static void mega_8_to_40ld (mraid_inquiry *inquiry,
		mega_inquiry3 *enquiry3, mega_product_info *);

static int megadev_open (struct inode *, struct file *);
static int megadev_ioctl (struct file *, unsigned int, unsigned long);
static int mega_m_to_n(void __user *, nitioctl_t *);
static int mega_n_to_m(void __user *, megacmd_t *);

static int mega_init_scb (adapter_t *);

static int mega_is_bios_enabled (adapter_t *);

#ifdef CONFIG_PROC_FS
static int mega_print_inquiry(char *, char *);
static void mega_create_proc_entry(int, struct proc_dir_entry *);
static int proc_read_config(char *, char **, off_t, int, int *, void *);
static int proc_read_stat(char *, char **, off_t, int, int *, void *);
static int proc_read_mbox(char *, char **, off_t, int, int *, void *);
static int proc_rebuild_rate(char *, char **, off_t, int, int *, void *);
static int proc_battery(char *, char **, off_t, int, int *, void *);
static int proc_pdrv_ch0(char *, char **, off_t, int, int *, void *);
static int proc_pdrv_ch1(char *, char **, off_t, int, int *, void *);
static int proc_pdrv_ch2(char *, char **, off_t, int, int *, void *);
static int proc_pdrv_ch3(char *, char **, off_t, int, int *, void *);
static int proc_pdrv(adapter_t *, char *, int);
static int proc_rdrv_10(char *, char **, off_t, int, int *, void *);
static int proc_rdrv_20(char *, char **, off_t, int, int *, void *);
static int proc_rdrv_30(char *, char **, off_t, int, int *, void *);
static int proc_rdrv_40(char *, char **, off_t, int, int *, void *);
static int proc_rdrv(adapter_t *, char *, int, int);

static int mega_adapinq(adapter_t *, dma_addr_t);
static int mega_internal_dev_inquiry(adapter_t *, u8, u8, dma_addr_t);
#endif

static int mega_support_ext_cdb(adapter_t *);
static mega_passthru* mega_prepare_passthru(adapter_t *, scb_t *,
		Scsi_Cmnd *, int, int);
static mega_ext_passthru* mega_prepare_extpassthru(adapter_t *,
		scb_t *, Scsi_Cmnd *, int, int);
static void mega_enum_raid_scsi(adapter_t *);
static void mega_get_boot_drv(adapter_t *);
static int mega_support_random_del(adapter_t *);
static int mega_del_logdrv(adapter_t *, int);
static int mega_do_del_logdrv(adapter_t *, int);
static void mega_get_max_sgl(adapter_t *);
static int mega_internal_command(adapter_t *, megacmd_t *, mega_passthru *);
static void mega_internal_done(Scsi_Cmnd *);
static int mega_support_cluster(adapter_t *);
#endif

