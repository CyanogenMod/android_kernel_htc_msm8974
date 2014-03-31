/*
 *  linux/drivers/message/fusion/mptbase.h
 *      High performance SCSI + LAN / Fibre Channel device drivers.
 *      For use with PCI chip/adapter(s):
 *          LSIFC9xx/LSI409xx Fibre Channel
 *      running LSI Fusion MPT (Message Passing Technology) firmware.
 *
 *  Copyright (c) 1999-2008 LSI Corporation
 *  (mailto:DL-MPTFusionLinux@lsi.com)
 *
 */
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MPTBASE_H_INCLUDED
#define MPTBASE_H_INCLUDED

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/mutex.h>

#include "lsi/mpi_type.h"
#include "lsi/mpi.h"		
#include "lsi/mpi_ioc.h"	
#include "lsi/mpi_cnfg.h"	
#include "lsi/mpi_init.h"	
#include "lsi/mpi_lan.h"	
#include "lsi/mpi_raid.h"	

#include "lsi/mpi_fc.h"		
#include "lsi/mpi_targ.h"	
#include "lsi/mpi_tool.h"	
#include "lsi/mpi_sas.h"	


#ifndef MODULEAUTHOR
#define MODULEAUTHOR	"LSI Corporation"
#endif

#ifndef COPYRIGHT
#define COPYRIGHT	"Copyright (c) 1999-2008 " MODULEAUTHOR
#endif

#define MPT_LINUX_VERSION_COMMON	"3.04.20"
#define MPT_LINUX_PACKAGE_NAME		"@(#)mptlinux-3.04.20"
#define WHAT_MAGIC_STRING		"@" "(" "#" ")"

#define show_mptmod_ver(s,ver)  \
	printk(KERN_INFO "%s %s\n", s, ver);

#define MPT_MAX_ADAPTERS		18
#define MPT_MAX_PROTOCOL_DRIVERS	16
#define MPT_MAX_CALLBACKNAME_LEN	49
#define MPT_MAX_BUS			1	
#define MPT_MAX_FC_DEVICES		255
#define MPT_MAX_SCSI_DEVICES		16
#define MPT_LAST_LUN			255
#define MPT_SENSE_BUFFER_ALLOC		64
	
#if MPT_SENSE_BUFFER_ALLOC >= 256
#	undef MPT_SENSE_BUFFER_ALLOC
#	define MPT_SENSE_BUFFER_ALLOC	256
#	define MPT_SENSE_BUFFER_SIZE	255
#else
#	define MPT_SENSE_BUFFER_SIZE	MPT_SENSE_BUFFER_ALLOC
#endif

#define MPT_NAME_LENGTH			32
#define MPT_KOBJ_NAME_LEN		20

#define MPT_PROCFS_MPTBASEDIR		"mpt"
						
#define MPT_PROCFS_SUMMARY_ALL_NODE		MPT_PROCFS_MPTBASEDIR "/summary"
#define MPT_PROCFS_SUMMARY_ALL_PATHNAME		"/proc/" MPT_PROCFS_SUMMARY_ALL_NODE
#define MPT_FW_REV_MAGIC_ID_STRING		"FwRev="

#define  MPT_MAX_REQ_DEPTH		1023
#define  MPT_DEFAULT_REQ_DEPTH		256
#define  MPT_MIN_REQ_DEPTH		128

#define  MPT_MAX_REPLY_DEPTH		MPT_MAX_REQ_DEPTH
#define  MPT_DEFAULT_REPLY_DEPTH	128
#define  MPT_MIN_REPLY_DEPTH		8
#define  MPT_MAX_REPLIES_PER_ISR	32

#define  MPT_MAX_FRAME_SIZE		128
#define  MPT_DEFAULT_FRAME_SIZE		128

#define  MPT_REPLY_FRAME_SIZE		0x50  

#define  MPT_SG_REQ_128_SCALE		1
#define  MPT_SG_REQ_96_SCALE		2
#define  MPT_SG_REQ_64_SCALE		4

#define	 CAN_SLEEP			1
#define  NO_SLEEP			0

#define MPT_COALESCING_TIMEOUT		0x10


#define MPT_ULTRA320			0x08
#define MPT_ULTRA160			0x09
#define MPT_ULTRA2			0x0A
#define MPT_ULTRA			0x0C
#define MPT_FAST			0x19
#define MPT_SCSI			0x32
#define MPT_ASYNC			0xFF

#define MPT_NARROW			0
#define MPT_WIDE			1

#define C0_1030				0x08
#define XL_929				0x01


#define MPT_FC_CAN_QUEUE	1024
#define MPT_SCSI_CAN_QUEUE	127
#define MPT_SAS_CAN_QUEUE	127

#ifdef CONFIG_FUSION_MAX_SGE
#if CONFIG_FUSION_MAX_SGE  < 16
#define MPT_SCSI_SG_DEPTH	16
#elif CONFIG_FUSION_MAX_SGE  > 128
#define MPT_SCSI_SG_DEPTH	128
#else
#define MPT_SCSI_SG_DEPTH	CONFIG_FUSION_MAX_SGE
#endif
#else
#define MPT_SCSI_SG_DEPTH	40
#endif

#ifdef CONFIG_FUSION_MAX_FC_SGE
#if CONFIG_FUSION_MAX_FC_SGE  < 16
#define MPT_SCSI_FC_SG_DEPTH	16
#elif CONFIG_FUSION_MAX_FC_SGE  > 256
#define MPT_SCSI_FC_SG_DEPTH	256
#else
#define MPT_SCSI_FC_SG_DEPTH	CONFIG_FUSION_MAX_FC_SGE
#endif
#else
#define MPT_SCSI_FC_SG_DEPTH	40
#endif

# define EVENT_DESCR_STR_SZ             100

#define MPT_POLLING_INTERVAL		1000	

#ifdef __KERNEL__	

#include <linux/proc_fs.h>

#define MYIOC_s_FMT			MYNAM ": %s: "
#define MYIOC_s_DEBUG_FMT		KERN_DEBUG MYNAM ": %s: "
#define MYIOC_s_INFO_FMT		KERN_INFO MYNAM ": %s: "
#define MYIOC_s_NOTE_FMT		KERN_NOTICE MYNAM ": %s: "
#define MYIOC_s_WARN_FMT		KERN_WARNING MYNAM ": %s: WARNING - "
#define MYIOC_s_ERR_FMT			KERN_ERR MYNAM ": %s: ERROR - "

#define ATTOFLAG_DISC     0x0001
#define ATTOFLAG_TAGGED   0x0002
#define ATTOFLAG_WIDE_ENB 0x0008
#define ATTOFLAG_ID_ENB   0x0010
#define ATTOFLAG_LUN_ENB  0x0060

typedef struct _ATTO_DEVICE_INFO
{
	u8	Offset;					
	u8	Period;					
	u16	ATTOFlags;				
} ATTO_DEVICE_INFO, MPI_POINTER PTR_ATTO_DEVICE_INFO,
  ATTODeviceInfo_t, MPI_POINTER pATTODeviceInfo_t;

typedef struct _ATTO_CONFIG_PAGE_SCSI_PORT_2
{
	CONFIG_PAGE_HEADER	Header;			
	u16			PortFlags;		
	u16			Unused1;		
	u32			Unused2;		
	ATTO_DEVICE_INFO	DeviceSettings[16];	
} fATTO_CONFIG_PAGE_SCSI_PORT_2, MPI_POINTER PTR_ATTO_CONFIG_PAGE_SCSI_PORT_2,
  ATTO_SCSIPortPage2_t, MPI_POINTER pATTO_SCSIPortPage2_t;


typedef enum {
	MPTBASE_DRIVER,		
	MPTCTL_DRIVER,		
	MPTSPI_DRIVER,		
	MPTFC_DRIVER,		
	MPTSAS_DRIVER,		
	MPTLAN_DRIVER,		
	MPTSTM_DRIVER,		
	MPTUNKNOWN_DRIVER
} MPT_DRIVER_CLASS;

struct mpt_pci_driver{
	int  (*probe) (struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove) (struct pci_dev *dev);
};


typedef union _MPT_FRAME_TRACKER {
	struct {
		struct list_head	list;
		u32			 arg1;
		u32			 pad;
		void			*argp1;
	} linkage;
	struct {
		u32 __hdr[2];
		union {
			u32		 MsgContext;
			struct {
				u16	 req_idx;	
				u8	 cb_idx;	
				u8	 rsvd;
			} fld;
		} msgctxu;
	} hwhdr;
} MPT_FRAME_TRACKER;

typedef struct _MPT_FRAME_HDR {
	union {
		MPIHeader_t		hdr;
		SCSIIORequest_t		scsireq;
		SCSIIOReply_t		sreply;
		ConfigReply_t		configreply;
		MPIDefaultReply_t	reply;
		MPT_FRAME_TRACKER	frame;
	} u;
} MPT_FRAME_HDR;

#define MPT_REQ_MSGFLAGS_DROPME		0x80

typedef struct _MPT_SGL_HDR {
	SGESimple32_t	 sge[1];
} MPT_SGL_HDR;

typedef struct _MPT_SGL64_HDR {
	SGESimple64_t	 sge[1];
} MPT_SGL64_HDR;


typedef struct _SYSIF_REGS
{
	u32	Doorbell;	
	u32	WriteSequence;	
	u32	Diagnostic;	
	u32	TestBase;	
	u32	DiagRwData;	
	u32	DiagRwAddress;	
	u32	Reserved1[6];	
	u32	IntStatus;	
	u32	IntMask;	
	u32	Reserved2[2];	
	u32	RequestFifo;	
	u32	ReplyFifo;	
	u32	RequestHiPriFifo; 
	u32	Reserved3;	
	u32	HostIndex;	
	u32	Reserved4[15];	
	u32	Fubar;		
	u32	Reserved5[1050];
	u32	Reset_1078;	
} SYSIF_REGS;




#define MPT_TARGET_NO_NEGO_WIDE		0x01
#define MPT_TARGET_NO_NEGO_SYNC		0x02
#define MPT_TARGET_NO_NEGO_QAS		0x04
#define MPT_TAPE_NEGO_IDP     		0x08

typedef struct _VirtTarget {
	struct scsi_target	*starget;
	u8			 tflags;
	u8			 ioc_id;
	u8			 id;
	u8			 channel;
	u8			 minSyncFactor;	
	u8			 maxOffset;	
	u8			 maxWidth;	
	u8			 negoFlags;	
	u8			 raidVolume;	
	u8			 type;		
	u8			 deleted;	
	u8			 inDMD;		
	u32			 num_luns;
} VirtTarget;

typedef struct _VirtDevice {
	VirtTarget		*vtarget;
	u8			 configured_lun;
	int			 lun;
} VirtDevice;

#define MPT_TARGET_DEFAULT_DV_STATUS	0x00
#define MPT_TARGET_FLAGS_VALID_NEGO	0x01
#define MPT_TARGET_FLAGS_VALID_INQUIRY	0x02
#define MPT_TARGET_FLAGS_Q_YES		0x08
#define MPT_TARGET_FLAGS_VALID_56	0x10
#define MPT_TARGET_FLAGS_SAF_TE_ISSUED	0x20
#define MPT_TARGET_FLAGS_RAID_COMPONENT	0x40
#define MPT_TARGET_FLAGS_LED_ON		0x80


#define MPTCTL_RESET_OK			0x01	

#define MPT_MGMT_STATUS_RF_VALID	0x01	
#define MPT_MGMT_STATUS_COMMAND_GOOD	0x02	
#define MPT_MGMT_STATUS_PENDING		0x04	
#define MPT_MGMT_STATUS_DID_IOCRESET	0x08	
#define MPT_MGMT_STATUS_SENSE_VALID	0x10	
#define MPT_MGMT_STATUS_TIMER_ACTIVE	0x20	
#define MPT_MGMT_STATUS_FREE_MF		0x40	

#define INITIALIZE_MGMT_STATUS(status) \
	status = MPT_MGMT_STATUS_PENDING;
#define CLEAR_MGMT_STATUS(status) \
	status = 0;
#define CLEAR_MGMT_PENDING_STATUS(status) \
	status &= ~MPT_MGMT_STATUS_PENDING;
#define SET_MGMT_MSG_CONTEXT(msg_context, value) \
	msg_context = value;

typedef struct _MPT_MGMT {
	struct mutex		 mutex;
	struct completion	 done;
	u8			 reply[MPT_DEFAULT_FRAME_SIZE]; 
	u8			 sense[MPT_SENSE_BUFFER_ALLOC];
	u8			 status;	
	int			 completion_code;
	u32			 msg_context;
} MPT_MGMT;

#define MPTCTL_EVENT_LOG_SIZE		(0x000000032)
typedef struct _mpt_ioctl_events {
	u32	event;		
	u32	eventContext;	
	u32	data[2];	
} MPT_IOCTL_EVENTS;

#define MPT_CONFIG_GOOD		MPI_IOCSTATUS_SUCCESS
#define MPT_CONFIG_ERROR	0x002F

						
#define MPT_SCSICFG_USE_NVRAM		0x01	
#define MPT_SCSICFG_ALL_IDS		0x02	

typedef	struct _SpiCfgData {
	u32		 PortFlags;
	int		*nvram;			
	IOCPage4_t	*pIocPg4;		
	dma_addr_t	 IocPg4_dma;		
	int		 IocPg4Sz;		
	u8		 minSyncFactor;		
	u8		 maxSyncOffset;		
	u8		 maxBusWidth;		
	u8		 busType;		
	u8		 sdp1version;		
	u8		 sdp1length;		
	u8		 sdp0version;		
	u8		 sdp0length;		
	u8		 dvScheduled;		
	u8		 noQas;			
	u8		 Saf_Te;		
	u8		 bus_reset;		
	u8		 rsvd[1];
}SpiCfgData;

typedef	struct _SasCfgData {
	u8		 ptClear;		
}SasCfgData;

struct inactive_raid_component_info {
	struct 	 list_head list;
	u8		 volumeID;		
	u8		 volumeBus;		
	IOC_3_PHYS_DISK	 d;			
};

typedef	struct _RaidCfgData {
	IOCPage2_t	*pIocPg2;		
	IOCPage3_t	*pIocPg3;		
	struct mutex	inactive_list_mutex;
	struct list_head	inactive_list; 
}RaidCfgData;

typedef struct _FcCfgData {
	
	struct {
		FCPortPage1_t	*data;
		dma_addr_t	 dma;
		int		 pg_sz;
	}			 fc_port_page1[2];
} FcCfgData;

#define MPT_RPORT_INFO_FLAGS_REGISTERED	0x01	
#define MPT_RPORT_INFO_FLAGS_MISSING	0x02	

struct mptfc_rport_info
{
	struct list_head list;
	struct fc_rport *rport;
	struct scsi_target *starget;
	FCDevicePage0_t pg0;
	u8		flags;
};



#define MPT_HOST_BUS_UNKNOWN		(0xFF)
#define MPT_HOST_TOO_MANY_TM		(0x05)
#define MPT_HOST_NVRAM_INVALID		(0xFFFFFFFF)
#define MPT_HOST_NO_CHAIN		(0xFFFFFFFF)
#define MPT_NVRAM_MASK_TIMEOUT		(0x000000FF)
#define MPT_NVRAM_SYNC_MASK		(0x0000FF00)
#define MPT_NVRAM_SYNC_SHIFT		(8)
#define MPT_NVRAM_DISCONNECT_ENABLE	(0x00010000)
#define MPT_NVRAM_ID_SCAN_ENABLE	(0x00020000)
#define MPT_NVRAM_LUN_SCAN_ENABLE	(0x00040000)
#define MPT_NVRAM_TAG_QUEUE_ENABLE	(0x00080000)
#define MPT_NVRAM_WIDE_DISABLE		(0x00100000)
#define MPT_NVRAM_BOOT_CHOICE		(0x00200000)

typedef enum {
	FC,
	SPI,
	SAS
} BUS_TYPE;

typedef struct _MPT_SCSI_HOST {
	struct _MPT_ADAPTER		 *ioc;
	ushort			  sel_timeout[MPT_MAX_FC_DEVICES];
	char			  *info_kbuf;
	long			  last_queue_full;
	u16			  spi_pending;
	struct list_head	  target_reset_list;
} MPT_SCSI_HOST;

typedef void (*MPT_ADD_SGE)(void *pAddr, u32 flagslength, dma_addr_t dma_addr);
typedef void (*MPT_ADD_CHAIN)(void *pAddr, u8 next, u16 length,
		dma_addr_t dma_addr);
typedef void (*MPT_SCHEDULE_TARGET_RESET)(void *ioc);
typedef void (*MPT_FLUSH_RUNNING_CMDS)(MPT_SCSI_HOST *hd);

typedef struct _MPT_ADAPTER
{
	int			 id;		
	int			 pci_irq;	
	char			 name[MPT_NAME_LENGTH];	
	char			 prod_name[MPT_NAME_LENGTH];	
#ifdef CONFIG_FUSION_LOGGING
	
	char			 evStr[EVENT_DESCR_STR_SZ];
#endif
	char			 board_name[16];
	char			 board_assembly[16];
	char			 board_tracer[16];
	u16			 nvdata_version_persistent;
	u16			 nvdata_version_default;
	int			 debug_level;
	u8			 io_missing_delay;
	u16			 device_missing_delay;
	SYSIF_REGS __iomem	*chip;		
	SYSIF_REGS __iomem	*pio_chip;	
	u8			 bus_type;
	u32			 mem_phys;	
	u32			 pio_mem_phys;	
	int			 mem_size;	
	int			 number_of_buses;
	int			 devices_per_bus;
	int			 alloc_total;
	u32			 last_state;
	int			 active;
	u8			*alloc;		
	dma_addr_t		 alloc_dma;
	u32			 alloc_sz;
	MPT_FRAME_HDR		*reply_frames;	
	u32			 reply_frames_low_dma;
	int			 reply_depth;	
	int			 reply_sz;	
	int			 num_chain;	
	MPT_ADD_SGE              add_sge;       
	MPT_ADD_CHAIN		 add_chain;	
	int			*ReqToChain;
	int			*RequestNB;
	int			*ChainToChain;
	u8			*ChainBuffer;
	dma_addr_t		 ChainBufferDMA;
	struct list_head	 FreeChainQ;
	spinlock_t		 FreeChainQlock;
		
	dma_addr_t		 req_frames_dma;
	MPT_FRAME_HDR		*req_frames;	
	u32			 req_frames_low_dma;
	int			 req_depth;	
	int			 req_sz;	
	spinlock_t		 FreeQlock;
	struct list_head	 FreeQ;
	u8			*sense_buf_pool;
	dma_addr_t		 sense_buf_pool_dma;
	u32			 sense_buf_low_dma;
	u8			*HostPageBuffer; 
	u32			HostPageBuffer_sz;
	dma_addr_t		HostPageBuffer_dma;
	int			 mtrr_reg;
	struct pci_dev		*pcidev;	
	int			bars;		
	int			msi_enable;
	u8			__iomem *memmap;	
	struct Scsi_Host	*sh;		
	SpiCfgData		spi_data;	
	RaidCfgData		raid_data;	
	SasCfgData		sas_data;	
	FcCfgData		fc_data;	
	struct proc_dir_entry	*ioc_dentry;
	struct _MPT_ADAPTER	*alt_ioc;	
	u32			 biosVersion;	
	int			 eventTypes;	
	int			 eventContext;	
	int			 eventLogSize;	
	struct _mpt_ioctl_events *events;	
	u8			*cached_fw;	
	dma_addr_t	 	cached_fw_dma;
	int			 hs_reply_idx;
#ifndef MFCNT
	u32			 pad0;
#else
	u32			 mfcnt;
#endif
	u32			 NB_for_64_byte_frame;
	u32			 hs_req[MPT_MAX_FRAME_SIZE/sizeof(u32)];
	u16			 hs_reply[MPT_MAX_FRAME_SIZE/sizeof(u16)];
	IOCFactsReply_t		 facts;
	PortFactsReply_t	 pfacts[2];
	FCPortPage0_t		 fc_port_page0[2];
	LANPage0_t		 lan_cnfg_page0;
	LANPage1_t		 lan_cnfg_page1;

	u8			 ir_firmware; 
	int			 errata_flag_1064;
	int			 aen_event_read_flag; 
	u8			 FirstWhoInit;
	u8			 upload_fw;	
	u8			 NBShiftFactor;  
	u8			 pad1[4];
	u8			 DoneCtx;
	u8			 TaskCtx;
	u8			 InternalCtx;
	struct list_head	 list;
	struct net_device	*netdev;
	struct list_head	 sas_topology;
	struct mutex		 sas_topology_mutex;

	struct workqueue_struct	*fw_event_q;
	struct list_head	 fw_event_list;
	spinlock_t		 fw_event_lock;
	u8			 fw_events_off; 
	char 			 fw_event_q_name[MPT_KOBJ_NAME_LEN];

	struct mutex		 sas_discovery_mutex;
	u8			 sas_discovery_runtime;
	u8			 sas_discovery_ignore_events;

	
	struct mptsas_portinfo	*hba_port_info;
	u64			 hba_port_sas_addr;
	u16			 hba_port_num_phy;
	struct list_head	 sas_device_info_list;
	struct mutex		 sas_device_info_mutex;
	u8			 old_sas_discovery_protocal;
	u8			 sas_discovery_quiesce_io;
	int			 sas_index; 
	MPT_MGMT		 sas_mgmt;
	MPT_MGMT		 mptbase_cmds; 
	MPT_MGMT		 internal_cmds;
	MPT_MGMT		 taskmgmt_cmds;
	MPT_MGMT		 ioctl_cmds;
	spinlock_t		 taskmgmt_lock; 
	int			 taskmgmt_in_progress;
	u8			 taskmgmt_quiesce_io;
	u8			 ioc_reset_in_progress;
	u8			 reset_status;
	u8			 wait_on_reset_completion;
	MPT_SCHEDULE_TARGET_RESET schedule_target_reset;
	MPT_FLUSH_RUNNING_CMDS schedule_dead_ioc_flush_running_cmds;
	struct work_struct	 sas_persist_task;

	struct work_struct	 fc_setup_reset_work;
	struct list_head	 fc_rports;
	struct work_struct	 fc_lsc_work;
	u8			 fc_link_speed[2];
	spinlock_t		 fc_rescan_work_lock;
	struct work_struct	 fc_rescan_work;
	char			 fc_rescan_work_q_name[MPT_KOBJ_NAME_LEN];
	struct workqueue_struct *fc_rescan_work_q;

	
	unsigned long		  hard_resets;
	
	unsigned long		  soft_resets;
	
	unsigned long		  timeouts;

	struct scsi_cmnd	**ScsiLookup;
	spinlock_t		  scsi_lookup_lock;
	u64			dma_mask;
	u32			  broadcast_aen_busy;
	char			 reset_work_q_name[MPT_KOBJ_NAME_LEN];
	struct workqueue_struct *reset_work_q;
	struct delayed_work	 fault_reset_work;

	u8			sg_addr_size;
	u8			in_rescan;
	u8			SGE_size;

} MPT_ADAPTER;

typedef int (*MPT_CALLBACK)(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req, MPT_FRAME_HDR *reply);
typedef int (*MPT_EVHANDLER)(MPT_ADAPTER *ioc, EventNotificationReply_t *evReply);
typedef int (*MPT_RESETHANDLER)(MPT_ADAPTER *ioc, int reset_phase);
#define MPT_IOC_PRE_RESET		0
#define MPT_IOC_POST_RESET		1
#define MPT_IOC_SETUP_RESET		2

typedef struct _MPT_HOST_EVENT {
	EventNotificationReply_t	 MpiEvent;	
	u32				 pad[6];
	void				*next;
} MPT_HOST_EVENT;

#define MPT_HOSTEVENT_IOC_BRINGUP	0x91
#define MPT_HOSTEVENT_IOC_RECOVER	0x92

typedef struct _mpt_sge {
	u32		FlagsLength;
	dma_addr_t	Address;
} MptSge_t;


#define mpt_msg_flags(ioc) \
	(ioc->sg_addr_size == sizeof(u64)) ?		\
	MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_64 : 		\
	MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_32

#define MPT_SGE_FLAGS_64_BIT_ADDRESSING \
	(MPI_SGE_FLAGS_64_BIT_ADDRESSING << MPI_SGE_FLAGS_SHIFT)

#include "mptdebug.h"

#define MPT_INDEX_2_MFPTR(ioc,idx) \
	(MPT_FRAME_HDR*)( (u8*)(ioc)->req_frames + (ioc)->req_sz * (idx) )

#define MFPTR_2_MPT_INDEX(ioc,mf) \
	(int)( ((u8*)mf - (u8*)(ioc)->req_frames) / (ioc)->req_sz )

#define MPT_INDEX_2_RFPTR(ioc,idx) \
	(MPT_FRAME_HDR*)( (u8*)(ioc)->reply_frames + (ioc)->req_sz * (idx) )


#define SCSI_STD_SENSE_BYTES    18
#define SCSI_STD_INQUIRY_BYTES  36
#define SCSI_MAX_INQUIRY_BYTES  96

typedef struct _MPT_LOCAL_REPLY {
	ConfigPageHeader_t header;
	int	completion;
	u8	sense[SCSI_STD_SENSE_BYTES];
	u8	scsiStatus;
	u8	skip;
	u32	pad;
} MPT_LOCAL_REPLY;


#define TM_STATE_NONE          (0)
#define	TM_STATE_IN_PROGRESS   (1)
#define	TM_STATE_ERROR	       (2)


struct scsi_cmnd;

typedef struct _x_config_parms {
	union {
		ConfigExtendedPageHeader_t	*ehdr;
		ConfigPageHeader_t	*hdr;
	} cfghdr;
	dma_addr_t		 physAddr;
	u32			 pageAddr;	
	u16			 status;
	u8			 action;
	u8			 dir;
	u8			 timeout;	
} CONFIGPARMS;

extern int	 mpt_attach(struct pci_dev *pdev, const struct pci_device_id *id);
extern void	 mpt_detach(struct pci_dev *pdev);
#ifdef CONFIG_PM
extern int	 mpt_suspend(struct pci_dev *pdev, pm_message_t state);
extern int	 mpt_resume(struct pci_dev *pdev);
#endif
extern u8	 mpt_register(MPT_CALLBACK cbfunc, MPT_DRIVER_CLASS dclass,
		char *func_name);
extern void	 mpt_deregister(u8 cb_idx);
extern int	 mpt_event_register(u8 cb_idx, MPT_EVHANDLER ev_cbfunc);
extern void	 mpt_event_deregister(u8 cb_idx);
extern int	 mpt_reset_register(u8 cb_idx, MPT_RESETHANDLER reset_func);
extern void	 mpt_reset_deregister(u8 cb_idx);
extern int	 mpt_device_driver_register(struct mpt_pci_driver * dd_cbfunc, u8 cb_idx);
extern void	 mpt_device_driver_deregister(u8 cb_idx);
extern MPT_FRAME_HDR	*mpt_get_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc);
extern void	 mpt_free_msg_frame(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf);
extern void	 mpt_put_msg_frame(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf);
extern void	 mpt_put_msg_frame_hi_pri(u8 cb_idx, MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf);

extern int	 mpt_send_handshake_request(u8 cb_idx, MPT_ADAPTER *ioc, int reqBytes, u32 *req, int sleepFlag);
extern int	 mpt_verify_adapter(int iocid, MPT_ADAPTER **iocpp);
extern u32	 mpt_GetIocState(MPT_ADAPTER *ioc, int cooked);
extern void	 mpt_print_ioc_summary(MPT_ADAPTER *ioc, char *buf, int *size, int len, int showlan);
extern int	 mpt_HardResetHandler(MPT_ADAPTER *ioc, int sleepFlag);
extern int	 mpt_Soft_Hard_ResetHandler(MPT_ADAPTER *ioc, int sleepFlag);
extern int	 mpt_config(MPT_ADAPTER *ioc, CONFIGPARMS *cfg);
extern int	 mpt_alloc_fw_memory(MPT_ADAPTER *ioc, int size);
extern void	 mpt_free_fw_memory(MPT_ADAPTER *ioc);
extern int	 mpt_findImVolumes(MPT_ADAPTER *ioc);
extern int	 mptbase_sas_persist_operation(MPT_ADAPTER *ioc, u8 persist_opcode);
extern int	 mpt_raid_phys_disk_pg0(MPT_ADAPTER *ioc, u8 phys_disk_num, pRaidPhysDiskPage0_t phys_disk);
extern int	mpt_raid_phys_disk_pg1(MPT_ADAPTER *ioc, u8 phys_disk_num,
		pRaidPhysDiskPage1_t phys_disk);
extern int	mpt_raid_phys_disk_get_num_paths(MPT_ADAPTER *ioc,
		u8 phys_disk_num);
extern int	 mpt_set_taskmgmt_in_progress_flag(MPT_ADAPTER *ioc);
extern void	 mpt_clear_taskmgmt_in_progress_flag(MPT_ADAPTER *ioc);
extern void     mpt_halt_firmware(MPT_ADAPTER *ioc);


extern struct list_head	  ioc_list;
extern int mpt_fwfault_debug;

#endif		

#ifdef CONFIG_64BIT
#define CAST_U32_TO_PTR(x)	((void *)(u64)x)
#define CAST_PTR_TO_U32(x)	((u32)(u64)x)
#else
#define CAST_U32_TO_PTR(x)	((void *)x)
#define CAST_PTR_TO_U32(x)	((u32)x)
#endif

#define MPT_PROTOCOL_FLAGS_c_c_c_c(pflags) \
	((pflags) & MPI_PORTFACTS_PROTOCOL_INITIATOR)	? 'I' : 'i',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_TARGET)	? 'T' : 't',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_LAN)		? 'L' : 'l',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_LOGBUSADDR)	? 'B' : 'b'

#define MPT_TRANSFER_IOC_TO_HOST		(0x00000000)
#define MPT_TRANSFER_HOST_TO_IOC		(0x04000000)
#define MPT_SGE_FLAGS_LAST_ELEMENT		(0x80000000)
#define MPT_SGE_FLAGS_END_OF_BUFFER		(0x40000000)
#define MPT_SGE_FLAGS_LOCAL_ADDRESS		(0x08000000)
#define MPT_SGE_FLAGS_DIRECTION			(0x04000000)
#define MPT_SGE_FLAGS_END_OF_LIST		(0x01000000)

#define MPT_SGE_FLAGS_TRANSACTION_ELEMENT	(0x00000000)
#define MPT_SGE_FLAGS_SIMPLE_ELEMENT		(0x10000000)
#define MPT_SGE_FLAGS_CHAIN_ELEMENT		(0x30000000)
#define MPT_SGE_FLAGS_ELEMENT_MASK		(0x30000000)

#define MPT_SGE_FLAGS_SSIMPLE_READ \
	(MPT_SGE_FLAGS_LAST_ELEMENT |	\
	 MPT_SGE_FLAGS_END_OF_BUFFER |	\
	 MPT_SGE_FLAGS_END_OF_LIST |	\
	 MPT_SGE_FLAGS_SIMPLE_ELEMENT |	\
	 MPT_TRANSFER_IOC_TO_HOST)
#define MPT_SGE_FLAGS_SSIMPLE_WRITE \
	(MPT_SGE_FLAGS_LAST_ELEMENT |	\
	 MPT_SGE_FLAGS_END_OF_BUFFER |	\
	 MPT_SGE_FLAGS_END_OF_LIST |	\
	 MPT_SGE_FLAGS_SIMPLE_ELEMENT |	\
	 MPT_TRANSFER_HOST_TO_IOC)

#endif

