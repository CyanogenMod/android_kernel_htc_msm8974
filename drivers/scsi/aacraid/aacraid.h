#ifndef dprintk
# define dprintk(x)
#endif
#define _nblank(x) #x
#define nblank(x) _nblank(x)[0]

#include <linux/interrupt.h>


#ifndef AAC_DRIVER_BUILD
# define AAC_DRIVER_BUILD 28900
# define AAC_DRIVER_BRANCH "-ms"
#endif
#define MAXIMUM_NUM_CONTAINERS	32

#define AAC_NUM_MGT_FIB         8
#define AAC_NUM_IO_FIB		(512 - AAC_NUM_MGT_FIB)
#define AAC_NUM_FIB		(AAC_NUM_IO_FIB + AAC_NUM_MGT_FIB)

#define AAC_MAX_LUN		(8)

#define AAC_MAX_HOSTPHYSMEMPAGES (0xfffff)
#define AAC_MAX_32BIT_SGBCOUNT	((unsigned short)256)

#define AAC_DEBUG_INSTRUMENT_AIF_DELETE

#define CONTAINER_CHANNEL		(0)
#define CONTAINER_TO_CHANNEL(cont)	(CONTAINER_CHANNEL)
#define CONTAINER_TO_ID(cont)		(cont)
#define CONTAINER_TO_LUN(cont)		(0)

#define aac_phys_to_logical(x)  ((x)+1)
#define aac_logical_to_phys(x)  ((x)?(x)-1:0)


struct diskparm
{
	int heads;
	int sectors;
	int cylinders;
};



#define		CT_NONE			0
#define		CT_OK			218
#define		FT_FILESYS	8	
#define		FT_DRIVE	9	

struct sgentry {
	__le32	addr;	
	__le32	count;	
};

struct user_sgentry {
	u32	addr;	
	u32	count;	
};

struct sgentry64 {
	__le32	addr[2];	
	__le32	count;	
};

struct user_sgentry64 {
	u32	addr[2];	
	u32	count;	
};

struct sgentryraw {
	__le32		next;	
	__le32		prev;	
	__le32		addr[2];
	__le32		count;
	__le32		flags;	
};

struct user_sgentryraw {
	u32		next;	
	u32		prev;	
	u32		addr[2];
	u32		count;
	u32		flags;	
};


struct sgmap {
	__le32		count;
	struct sgentry	sg[1];
};

struct user_sgmap {
	u32		count;
	struct user_sgentry	sg[1];
};

struct sgmap64 {
	__le32		count;
	struct sgentry64 sg[1];
};

struct user_sgmap64 {
	u32		count;
	struct user_sgentry64 sg[1];
};

struct sgmapraw {
	__le32		  count;
	struct sgentryraw sg[1];
};

struct user_sgmapraw {
	u32		  count;
	struct user_sgentryraw sg[1];
};

struct creation_info
{
	u8		buildnum;		
	u8		usec;			
	u8		via;			
	u8		year;			
	__le32		date;			
	__le32		serial[2];			
};




#define NUMBER_OF_COMM_QUEUES  8   
#define HOST_HIGH_CMD_ENTRIES  4
#define HOST_NORM_CMD_ENTRIES  8
#define ADAP_HIGH_CMD_ENTRIES  4
#define ADAP_NORM_CMD_ENTRIES  512
#define HOST_HIGH_RESP_ENTRIES 4
#define HOST_NORM_RESP_ENTRIES 512
#define ADAP_HIGH_RESP_ENTRIES 4
#define ADAP_NORM_RESP_ENTRIES 8

#define TOTAL_QUEUE_ENTRIES  \
    (HOST_NORM_CMD_ENTRIES + HOST_HIGH_CMD_ENTRIES + ADAP_NORM_CMD_ENTRIES + ADAP_HIGH_CMD_ENTRIES + \
	    HOST_NORM_RESP_ENTRIES + HOST_HIGH_RESP_ENTRIES + ADAP_NORM_RESP_ENTRIES + ADAP_HIGH_RESP_ENTRIES)



#define QUEUE_ALIGNMENT		16


struct aac_entry {
	__le32 size; 
	__le32 addr; 
};


struct aac_qhdr {
	__le64 header_addr;
	__le32 *producer; 
	__le32 *consumer; 
};


#define		HostNormCmdQue		1	
#define		HostHighCmdQue		2	
#define		HostNormRespQue		3	
#define		HostHighRespQue		4	
#define		AdapNormRespNotFull	5
#define		AdapHighRespNotFull	6
#define		AdapNormCmdNotFull	7
#define		AdapHighCmdNotFull	8
#define		SynchCommandComplete	9
#define		AdapInternalError	0xfe    


#define		AdapNormCmdQue		2
#define		AdapHighCmdQue		3
#define		AdapNormRespQue		6
#define		AdapHighRespQue		7
#define		HostShutdown		8
#define		HostPowerFail		9
#define		FatalCommError		10
#define		HostNormRespNotFull	11
#define		HostHighRespNotFull	12
#define		HostNormCmdNotFull	13
#define		HostHighCmdNotFull	14
#define		FastIo			15
#define		AdapPrintfDone		16


enum aac_queue_types {
        HostNormCmdQueue = 0,	
        HostHighCmdQueue,	
        AdapNormCmdQueue,	
        AdapHighCmdQueue,	
        HostNormRespQueue,	
        HostHighRespQueue,	
        AdapNormRespQueue,	
        AdapHighRespQueue	
};


#define		FIB_MAGIC	0x0001


#define		FsaNormal	1

struct aac_fib_xporthdr {
	u64	HostAddress;	
	u32	Size;		
	u32	Handle;		
	u64	Reserved[2];
};

#define		ALIGN32		32


struct aac_fibhdr {
	__le32 XferState;	
	__le16 Command;		
	u8 StructType;		
	u8 Flags;		
	__le16 Size;		
	__le16 SenderSize;	
	__le32 SenderFibAddress;  
	__le32 ReceiverFibAddress;
	u32 SenderData;		
	union {
		struct {
		    __le32 _ReceiverTimeStart;	
		    __le32 _ReceiverTimeDone;	
		} _s;
	} _u;
};

struct hw_fib {
	struct aac_fibhdr header;
	u8 data[512-sizeof(struct aac_fibhdr)];	
};


#define		TestCommandResponse		1
#define		TestAdapterCommand		2
#define		LastTestCommand			100
#define		ReinitHostNormCommandQueue	101
#define		ReinitHostHighCommandQueue	102
#define		ReinitHostHighRespQueue		103
#define		ReinitHostNormRespQueue		104
#define		ReinitAdapNormCommandQueue	105
#define		ReinitAdapHighCommandQueue	107
#define		ReinitAdapHighRespQueue		108
#define		ReinitAdapNormRespQueue		109
#define		InterfaceShutdown		110
#define		DmaCommandFib			120
#define		StartProfile			121
#define		TermProfile			122
#define		SpeedTest			123
#define		TakeABreakPt			124
#define		RequestPerfData			125
#define		SetInterruptDefTimer		126
#define		SetInterruptDefCount		127
#define		GetInterruptDefStatus		128
#define		LastCommCommand			129
#define		NuFileSystem			300
#define		UFS				301
#define		HostFileSystem			302
#define		LastFileSystemCommand		303
#define		ContainerCommand		500
#define		ContainerCommand64		501
#define		ContainerRawIo			502
#define		ScsiPortCommand			600
#define		ScsiPortCommand64		601
#define		AifRequest			700
#define		CheckRevision			701
#define		FsaHostShutdown			702
#define		RequestAdapterInfo		703
#define		IsAdapterPaused			704
#define		SendHostTime			705
#define		RequestSupplementAdapterInfo	706
#define		LastMiscCommand			707


enum fib_xfer_state {
	HostOwned			= (1<<0),
	AdapterOwned			= (1<<1),
	FibInitialized			= (1<<2),
	FibEmpty			= (1<<3),
	AllocatedFromPool		= (1<<4),
	SentFromHost			= (1<<5),
	SentFromAdapter			= (1<<6),
	ResponseExpected		= (1<<7),
	NoResponseExpected		= (1<<8),
	AdapterProcessed		= (1<<9),
	HostProcessed			= (1<<10),
	HighPriority			= (1<<11),
	NormalPriority			= (1<<12),
	Async				= (1<<13),
	AsyncIo				= (1<<13),	
	PageFileIo			= (1<<14),	
	ShutdownRequest			= (1<<15),
	LazyWrite			= (1<<16),	
	AdapterMicroFib			= (1<<17),
	BIOSFibPath			= (1<<18),
	FastResponseCapable		= (1<<19),
	ApiFib				= (1<<20),	
	
	NoMoreAifDataAvailable		= (1<<21)
};


#define ADAPTER_INIT_STRUCT_REVISION		3
#define ADAPTER_INIT_STRUCT_REVISION_4		4 
#define ADAPTER_INIT_STRUCT_REVISION_6		6 

struct aac_init
{
	__le32	InitStructRevision;
	__le32	MiniPortRevision;
	__le32	fsrev;
	__le32	CommHeaderAddress;
	__le32	FastIoCommAreaAddress;
	__le32	AdapterFibsPhysicalAddress;
	__le32	AdapterFibsVirtualAddress;
	__le32	AdapterFibsSize;
	__le32	AdapterFibAlign;
	__le32	printfbuf;
	__le32	printfbufsiz;
	__le32	HostPhysMemPages;   
	__le32	HostElapsedSeconds; 
	__le32	InitFlags;	
#define INITFLAGS_NEW_COMM_SUPPORTED	0x00000001
#define INITFLAGS_DRIVER_USES_UTC_TIME	0x00000010
#define INITFLAGS_DRIVER_SUPPORTS_PM	0x00000020
#define INITFLAGS_NEW_COMM_TYPE1_SUPPORTED	0x00000041
	__le32	MaxIoCommands;	
	__le32	MaxIoSize;	
	__le32	MaxFibSize;	
	
	__le32	MaxNumAif;	
	
	__le32	HostRRQ_AddrLow;
	__le32	HostRRQ_AddrHigh;	
};

enum aac_log_level {
	LOG_AAC_INIT			= 10,
	LOG_AAC_INFORMATIONAL		= 20,
	LOG_AAC_WARNING			= 30,
	LOG_AAC_LOW_ERROR		= 40,
	LOG_AAC_MEDIUM_ERROR		= 50,
	LOG_AAC_HIGH_ERROR		= 60,
	LOG_AAC_PANIC			= 70,
	LOG_AAC_DEBUG			= 80,
	LOG_AAC_WINDBG_PRINT		= 90
};

#define FSAFS_NTC_GET_ADAPTER_FIB_CONTEXT	0x030b
#define FSAFS_NTC_FIB_CONTEXT			0x030c

struct aac_dev;
struct fib;
struct scsi_cmnd;

struct adapter_ops
{
	
	void (*adapter_interrupt)(struct aac_dev *dev);
	void (*adapter_notify)(struct aac_dev *dev, u32 event);
	void (*adapter_disable_int)(struct aac_dev *dev);
	void (*adapter_enable_int)(struct aac_dev *dev);
	int  (*adapter_sync_cmd)(struct aac_dev *dev, u32 command, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, u32 *status, u32 *r1, u32 *r2, u32 *r3, u32 *r4);
	int  (*adapter_check_health)(struct aac_dev *dev);
	int  (*adapter_restart)(struct aac_dev *dev, int bled);
	
	int  (*adapter_ioremap)(struct aac_dev * dev, u32 size);
	irq_handler_t adapter_intr;
	
	int  (*adapter_deliver)(struct fib * fib);
	int  (*adapter_bounds)(struct aac_dev * dev, struct scsi_cmnd * cmd, u64 lba);
	int  (*adapter_read)(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count);
	int  (*adapter_write)(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count, int fua);
	int  (*adapter_scsi)(struct fib * fib, struct scsi_cmnd * cmd);
	
	int  (*adapter_comm)(struct aac_dev * dev, int comm);
};


struct aac_driver_ident
{
	int	(*init)(struct aac_dev *dev);
	char *	name;
	char *	vname;
	char *	model;
	u16	channels;
	int	quirks;
};
#define AAC_QUIRK_31BIT	0x0001

#define AAC_QUIRK_34SG	0x0002

#define AAC_QUIRK_SLAVE 0x0004

#define AAC_QUIRK_MASTER 0x0008

#define AAC_QUIRK_17SG	0x0010

#define AAC_QUIRK_SCSI_32	0x0020


struct aac_queue {
	u64			logical;	
	struct aac_entry	*base;		
	struct aac_qhdr		headers;	
	u32			entries;	
	wait_queue_head_t	qfull;		
	wait_queue_head_t	cmdready;	
		
	spinlock_t		*lock;		
	spinlock_t		lockdata;	
	struct list_head	cmdq;		
						
	u32			numpending;	
	struct aac_dev *	dev;		
};


struct aac_queue_block
{
	struct aac_queue queue[8];
};


struct sa_drawbridge_CSR {
				
	__le32	reserved[10];	
	u8	LUT_Offset;	
	u8	reserved1[3];	
	__le32	LUT_Data;	
	__le32	reserved2[26];	
	__le16	PRICLEARIRQ;	
	__le16	SECCLEARIRQ;	
	__le16	PRISETIRQ;	
	__le16	SECSETIRQ;	
	__le16	PRICLEARIRQMASK;
	__le16	SECCLEARIRQMASK;
	__le16	PRISETIRQMASK;	
	__le16	SECSETIRQMASK;	
	__le32	MAILBOX0;	
	__le32	MAILBOX1;	
	__le32	MAILBOX2;	
	__le32	MAILBOX3;	
	__le32	MAILBOX4;	
	__le32	MAILBOX5;	
	__le32	MAILBOX6;	
	__le32	MAILBOX7;	
	__le32	ROM_Setup_Data;	
	__le32	ROM_Control_Addr;
	__le32	reserved3[12];	
	__le32	LUT[64];	
};

#define Mailbox0	SaDbCSR.MAILBOX0
#define Mailbox1	SaDbCSR.MAILBOX1
#define Mailbox2	SaDbCSR.MAILBOX2
#define Mailbox3	SaDbCSR.MAILBOX3
#define Mailbox4	SaDbCSR.MAILBOX4
#define Mailbox5	SaDbCSR.MAILBOX5
#define Mailbox6	SaDbCSR.MAILBOX6
#define Mailbox7	SaDbCSR.MAILBOX7

#define DoorbellReg_p SaDbCSR.PRISETIRQ
#define DoorbellReg_s SaDbCSR.SECSETIRQ
#define DoorbellClrReg_p SaDbCSR.PRICLEARIRQ


#define	DOORBELL_0	0x0001
#define DOORBELL_1	0x0002
#define DOORBELL_2	0x0004
#define DOORBELL_3	0x0008
#define DOORBELL_4	0x0010
#define DOORBELL_5	0x0020
#define DOORBELL_6	0x0040


#define PrintfReady	DOORBELL_5
#define PrintfDone	DOORBELL_5

struct sa_registers {
	struct sa_drawbridge_CSR	SaDbCSR;			
};


#define Sa_MINIPORT_REVISION			1

#define sa_readw(AEP, CSR)		readl(&((AEP)->regs.sa->CSR))
#define sa_readl(AEP, CSR)		readl(&((AEP)->regs.sa->CSR))
#define sa_writew(AEP, CSR, value)	writew(value, &((AEP)->regs.sa->CSR))
#define sa_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.sa->CSR))


struct rx_mu_registers {
			    
	__le32	ARSR;	    
	__le32	reserved0;  
	__le32	AWR;	    
	__le32	reserved1;  
	__le32	IMRx[2];    
	__le32	OMRx[2];    
	__le32	IDR;	    
	__le32	IISR;	    
	__le32	IIMR;	    
	__le32	ODR;	    
	__le32	OISR;	    
	__le32	OIMR;	    
	__le32	reserved2;  
	__le32	reserved3;  
	__le32	InboundQueue;
	__le32	OutboundQueue;
};

struct rx_inbound {
	__le32	Mailbox[8];
};

#define	INBOUNDDOORBELL_0	0x00000001
#define INBOUNDDOORBELL_1	0x00000002
#define INBOUNDDOORBELL_2	0x00000004
#define INBOUNDDOORBELL_3	0x00000008
#define INBOUNDDOORBELL_4	0x00000010
#define INBOUNDDOORBELL_5	0x00000020
#define INBOUNDDOORBELL_6	0x00000040

#define	OUTBOUNDDOORBELL_0	0x00000001
#define OUTBOUNDDOORBELL_1	0x00000002
#define OUTBOUNDDOORBELL_2	0x00000004
#define OUTBOUNDDOORBELL_3	0x00000008
#define OUTBOUNDDOORBELL_4	0x00000010

#define InboundDoorbellReg	MUnit.IDR
#define OutboundDoorbellReg	MUnit.ODR

struct rx_registers {
	struct rx_mu_registers		MUnit;		
	__le32				reserved1[2];	
	struct rx_inbound		IndexRegs;
};

#define rx_readb(AEP, CSR)		readb(&((AEP)->regs.rx->CSR))
#define rx_readl(AEP, CSR)		readl(&((AEP)->regs.rx->CSR))
#define rx_writeb(AEP, CSR, value)	writeb(value, &((AEP)->regs.rx->CSR))
#define rx_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.rx->CSR))


#define rkt_mu_registers rx_mu_registers
#define rkt_inbound rx_inbound

struct rkt_registers {
	struct rkt_mu_registers		MUnit;		 
	__le32				reserved1[1006]; 
	struct rkt_inbound		IndexRegs;	 
};

#define rkt_readb(AEP, CSR)		readb(&((AEP)->regs.rkt->CSR))
#define rkt_readl(AEP, CSR)		readl(&((AEP)->regs.rkt->CSR))
#define rkt_writeb(AEP, CSR, value)	writeb(value, &((AEP)->regs.rkt->CSR))
#define rkt_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.rkt->CSR))


#define src_inbound rx_inbound

struct src_mu_registers {
				
	__le32	reserved0[8];	
	__le32	IDR;		
	__le32	IISR;		
	__le32	reserved1[3];	
	__le32	OIMR;		
	__le32	reserved2[25];	
	__le32	ODR_R;		
	__le32	ODR_C;		
	__le32	reserved3[6];	
	__le32	OMR;		
	__le32	IQ_L;		
	__le32	IQ_H;		
};

struct src_registers {
	struct src_mu_registers MUnit;	
	union {
		struct {
			__le32 reserved1[130790];	
			struct src_inbound IndexRegs;	
		} tupelo;
		struct {
			__le32 reserved1[974];		
			struct src_inbound IndexRegs;	
		} denali;
	} u;
};

#define src_readb(AEP, CSR)		readb(&((AEP)->regs.src.bar0->CSR))
#define src_readl(AEP, CSR)		readl(&((AEP)->regs.src.bar0->CSR))
#define src_writeb(AEP, CSR, value)	writeb(value, \
						&((AEP)->regs.src.bar0->CSR))
#define src_writel(AEP, CSR, value)	writel(value, \
						&((AEP)->regs.src.bar0->CSR))

#define SRC_ODR_SHIFT		12
#define SRC_IDR_SHIFT		9

typedef void (*fib_callback)(void *ctxt, struct fib *fibctx);

struct aac_fib_context {
	s16			type;		
	s16			size;
	u32			unique;		
	ulong			jiffies;	
	struct list_head	next;		
	struct semaphore	wait_sem;	
	int			wait;		
	unsigned long		count;		
	struct list_head	fib_list;	
};

struct sense_data {
	u8 error_code;		
	u8 valid:1;		
	u8 segment_number;	
	u8 sense_key:4;		
	u8 reserved:1;
	u8 ILI:1;		
	u8 EOM:1;		
	u8 filemark:1;		

	u8 information[4];	
	u8 add_sense_len;	
	u8 cmnd_info[4];	
	u8 ASC;			
	u8 ASCQ;		
	u8 FRUC;		
	u8 bit_ptr:3;		
	u8 BPV:1;		
	u8 reserved2:2;
	u8 CD:1;		
	u8 SKSV:1;
	u8 field_ptr[2];	
};

struct fsa_dev_info {
	u64		last;
	u64		size;
	u32		type;
	u32		config_waiting_on;
	unsigned long	config_waiting_stamp;
	u16		queue_depth;
	u8		config_needed;
	u8		valid;
	u8		ro;
	u8		locked;
	u8		deleted;
	char		devname[8];
	struct sense_data sense_data;
};

struct fib {
	void			*next;	
	s16			type;
	s16			size;
	struct aac_dev		*dev;
	struct semaphore	event_wait;
	spinlock_t		event_lock;

	u32			done;	
	fib_callback		callback;
	void			*callback_data;
	u32			flags; 
	struct list_head	fiblink;
	void			*data;
	struct hw_fib		*hw_fib_va;		
	dma_addr_t		hw_fib_pa;		
};


struct aac_adapter_info
{
	__le32	platform;
	__le32	cpu;
	__le32	subcpu;
	__le32	clock;
	__le32	execmem;
	__le32	buffermem;
	__le32	totalmem;
	__le32	kernelrev;
	__le32	kernelbuild;
	__le32	monitorrev;
	__le32	monitorbuild;
	__le32	hwrev;
	__le32	hwbuild;
	__le32	biosrev;
	__le32	biosbuild;
	__le32	cluster;
	__le32	clusterchannelmask;
	__le32	serial[2];
	__le32	battery;
	__le32	options;
	__le32	OEM;
};

struct aac_supplement_adapter_info
{
	u8	AdapterTypeText[17+1];
	u8	Pad[2];
	__le32	FlashMemoryByteSize;
	__le32	FlashImageId;
	__le32	MaxNumberPorts;
	__le32	Version;
	__le32	FeatureBits;
	u8	SlotNumber;
	u8	ReservedPad0[3];
	u8	BuildDate[12];
	__le32	CurrentNumberPorts;
	struct {
		u8	AssemblyPn[8];
		u8	FruPn[8];
		u8	BatteryFruPn[8];
		u8	EcVersionString[8];
		u8	Tsid[12];
	}	VpdInfo;
	__le32	FlashFirmwareRevision;
	__le32	FlashFirmwareBuild;
	__le32	RaidTypeMorphOptions;
	__le32	FlashFirmwareBootRevision;
	__le32	FlashFirmwareBootBuild;
	u8	MfgPcbaSerialNo[12];
	u8	MfgWWNName[8];
	__le32	SupportedOptions2;
	__le32	StructExpansion;
	
	__le32	FeatureBits3;
	__le32	SupportedPerformanceModes;
	__le32	ReservedForFutureGrowth[80];
};
#define AAC_FEATURE_FALCON	cpu_to_le32(0x00000010)
#define AAC_FEATURE_JBOD	cpu_to_le32(0x08000000)
#define AAC_OPTION_MU_RESET		cpu_to_le32(0x00000001)
#define AAC_OPTION_IGNORE_RESET		cpu_to_le32(0x00000002)
#define AAC_OPTION_POWER_MANAGEMENT	cpu_to_le32(0x00000004)
#define AAC_OPTION_DOORBELL_RESET	cpu_to_le32(0x00004000)
#define AAC_SIS_VERSION_V3	3
#define AAC_SIS_SLOT_UNKNOWN	0xFF

#define GetBusInfo 0x00000009
struct aac_bus_info {
	__le32	Command;	
	__le32	ObjType;	
	__le32	MethodId;	
	__le32	ObjectId;	
	__le32	CtlCmd;		
};

struct aac_bus_info_response {
	__le32	Status;		
	__le32	ObjType;
	__le32	MethodId;	
	__le32	ObjectId;	
	__le32	CtlCmd;		
	__le32	ProbeComplete;
	__le32	BusCount;
	__le32	TargetsPerBus;
	u8	InitiatorBusId[10];
	u8	BusValid[10];
};

#define AAC_BAT_REQ_PRESENT	(1)
#define AAC_BAT_REQ_NOTPRESENT	(2)
#define AAC_BAT_OPT_PRESENT	(3)
#define AAC_BAT_OPT_NOTPRESENT	(4)
#define AAC_BAT_NOT_SUPPORTED	(5)
#define AAC_CPU_SIMULATOR	(1)
#define AAC_CPU_I960		(2)
#define AAC_CPU_STRONGARM	(3)

#define AAC_OPT_SNAPSHOT		cpu_to_le32(1)
#define AAC_OPT_CLUSTERS		cpu_to_le32(1<<1)
#define AAC_OPT_WRITE_CACHE		cpu_to_le32(1<<2)
#define AAC_OPT_64BIT_DATA		cpu_to_le32(1<<3)
#define AAC_OPT_HOST_TIME_FIB		cpu_to_le32(1<<4)
#define AAC_OPT_RAID50			cpu_to_le32(1<<5)
#define AAC_OPT_4GB_WINDOW		cpu_to_le32(1<<6)
#define AAC_OPT_SCSI_UPGRADEABLE	cpu_to_le32(1<<7)
#define AAC_OPT_SOFT_ERR_REPORT		cpu_to_le32(1<<8)
#define AAC_OPT_SUPPORTED_RECONDITION	cpu_to_le32(1<<9)
#define AAC_OPT_SGMAP_HOST64		cpu_to_le32(1<<10)
#define AAC_OPT_ALARM			cpu_to_le32(1<<11)
#define AAC_OPT_NONDASD			cpu_to_le32(1<<12)
#define AAC_OPT_SCSI_MANAGED		cpu_to_le32(1<<13)
#define AAC_OPT_RAID_SCSI_MODE		cpu_to_le32(1<<14)
#define AAC_OPT_SUPPLEMENT_ADAPTER_INFO	cpu_to_le32(1<<16)
#define AAC_OPT_NEW_COMM		cpu_to_le32(1<<17)
#define AAC_OPT_NEW_COMM_64		cpu_to_le32(1<<18)
#define AAC_OPT_NEW_COMM_TYPE1		cpu_to_le32(1<<28)
#define AAC_OPT_NEW_COMM_TYPE2		cpu_to_le32(1<<29)
#define AAC_OPT_NEW_COMM_TYPE3		cpu_to_le32(1<<30)
#define AAC_OPT_NEW_COMM_TYPE4		cpu_to_le32(1<<31)


struct aac_dev
{
	struct list_head	entry;
	const char		*name;
	int			id;

	unsigned		max_fib_size;
	unsigned		sg_tablesize;
	unsigned		max_num_aif;

	dma_addr_t		hw_fib_pa;
	struct hw_fib		*hw_fib_va;
	struct hw_fib		*aif_base_va;
	struct fib              *fibs;

	struct fib		*free_fib;
	spinlock_t		fib_lock;

	struct aac_queue_block *queues;
	struct list_head	fib_list;

	struct adapter_ops	a_ops;
	unsigned long		fsrev;		

	unsigned long		dbg_base;	

	unsigned		base_size, dbg_size;	

	struct aac_init		*init;		
	dma_addr_t		init_pa;	

	u32			*host_rrq;	

	dma_addr_t		host_rrq_pa;	
	u32			host_rrq_idx;	

	struct pci_dev		*pdev;		
	void *			printfbuf;	
	void *			comm_addr;	
	dma_addr_t		comm_phys;	
	size_t			comm_size;

	struct Scsi_Host	*scsi_host_ptr;
	int			maximum_num_containers;
	int			maximum_num_physicals;
	int			maximum_num_channels;
	struct fsa_dev_info	*fsa_dev;
	struct task_struct	*thread;
	int			cardtype;

#ifndef AAC_MIN_FOOTPRINT_SIZE
#	define AAC_MIN_FOOTPRINT_SIZE 8192
#	define AAC_MIN_SRC_BAR0_SIZE 0x400000
#	define AAC_MIN_SRC_BAR1_SIZE 0x800
#	define AAC_MIN_SRCV_BAR0_SIZE 0x100000
#	define AAC_MIN_SRCV_BAR1_SIZE 0x400
#endif
	union
	{
		struct sa_registers __iomem *sa;
		struct rx_registers __iomem *rx;
		struct rkt_registers __iomem *rkt;
		struct {
			struct src_registers __iomem *bar0;
			char __iomem *bar1;
		} src;
	} regs;
	volatile void __iomem *base, *dbg_base_mapped;
	volatile struct rx_inbound __iomem *IndexRegs;
	u32			OIMR; 
	u32			aif_thread;
	struct aac_adapter_info adapter_info;
	struct aac_supplement_adapter_info supplement_adapter_info;
	u8			nondasd_support;
	u8			jbod;
	u8			cache_protected;
	u8			dac_support;
	u8			needs_dac;
	u8			raid_scsi_mode;
	u8			comm_interface;
#	define AAC_COMM_PRODUCER 0
#	define AAC_COMM_MESSAGE  1
#	define AAC_COMM_MESSAGE_TYPE1	3
	u8			raw_io_interface;
	u8			raw_io_64;
	u8			printf_enabled;
	u8			in_reset;
	u8			msi;
	int			management_fib_count;
	spinlock_t		manage_lock;
	spinlock_t		sync_lock;
	int			sync_mode;
	struct fib		*sync_fib;
	struct list_head	sync_fib_list;
};

#define aac_adapter_interrupt(dev) \
	(dev)->a_ops.adapter_interrupt(dev)

#define aac_adapter_notify(dev, event) \
	(dev)->a_ops.adapter_notify(dev, event)

#define aac_adapter_disable_int(dev) \
	(dev)->a_ops.adapter_disable_int(dev)

#define aac_adapter_enable_int(dev) \
	(dev)->a_ops.adapter_enable_int(dev)

#define aac_adapter_sync_cmd(dev, command, p1, p2, p3, p4, p5, p6, status, r1, r2, r3, r4) \
	(dev)->a_ops.adapter_sync_cmd(dev, command, p1, p2, p3, p4, p5, p6, status, r1, r2, r3, r4)

#define aac_adapter_check_health(dev) \
	(dev)->a_ops.adapter_check_health(dev)

#define aac_adapter_restart(dev,bled) \
	(dev)->a_ops.adapter_restart(dev,bled)

#define aac_adapter_ioremap(dev, size) \
	(dev)->a_ops.adapter_ioremap(dev, size)

#define aac_adapter_deliver(fib) \
	((fib)->dev)->a_ops.adapter_deliver(fib)

#define aac_adapter_bounds(dev,cmd,lba) \
	dev->a_ops.adapter_bounds(dev,cmd,lba)

#define aac_adapter_read(fib,cmd,lba,count) \
	((fib)->dev)->a_ops.adapter_read(fib,cmd,lba,count)

#define aac_adapter_write(fib,cmd,lba,count,fua) \
	((fib)->dev)->a_ops.adapter_write(fib,cmd,lba,count,fua)

#define aac_adapter_scsi(fib,cmd) \
	((fib)->dev)->a_ops.adapter_scsi(fib,cmd)

#define aac_adapter_comm(dev,comm) \
	(dev)->a_ops.adapter_comm(dev, comm)

#define FIB_CONTEXT_FLAG_TIMED_OUT		(0x00000001)
#define FIB_CONTEXT_FLAG			(0x00000002)
#define FIB_CONTEXT_FLAG_WAIT			(0x00000004)


#define		Null			0
#define		GetAttributes		1
#define		SetAttributes		2
#define		Lookup			3
#define		ReadLink		4
#define		Read			5
#define		Write			6
#define		Create			7
#define		MakeDirectory		8
#define		SymbolicLink		9
#define		MakeNode		10
#define		Removex			11
#define		RemoveDirectoryx	12
#define		Rename			13
#define		Link			14
#define		ReadDirectory		15
#define		ReadDirectoryPlus	16
#define		FileSystemStatus	17
#define		FileSystemInfo		18
#define		PathConfigure		19
#define		Commit			20
#define		Mount			21
#define		UnMount			22
#define		Newfs			23
#define		FsCheck			24
#define		FsSync			25
#define		SimReadWrite		26
#define		SetFileSystemStatus	27
#define		BlockRead		28
#define		BlockWrite		29
#define		NvramIoctl		30
#define		FsSyncWait		31
#define		ClearArchiveBit		32
#define		SetAcl			33
#define		GetAcl			34
#define		AssignAcl		35
#define		FaultInsertion		36	
#define		CrazyCache		37	

#define		MAX_FSACOMMAND_NUM	38



#define		ST_OK		0
#define		ST_PERM		1
#define		ST_NOENT	2
#define		ST_IO		5
#define		ST_NXIO		6
#define		ST_E2BIG	7
#define		ST_ACCES	13
#define		ST_EXIST	17
#define		ST_XDEV		18
#define		ST_NODEV	19
#define		ST_NOTDIR	20
#define		ST_ISDIR	21
#define		ST_INVAL	22
#define		ST_FBIG		27
#define		ST_NOSPC	28
#define		ST_ROFS		30
#define		ST_MLINK	31
#define		ST_WOULDBLOCK	35
#define		ST_NAMETOOLONG	63
#define		ST_NOTEMPTY	66
#define		ST_DQUOT	69
#define		ST_STALE	70
#define		ST_REMOTE	71
#define		ST_NOT_READY	72
#define		ST_BADHANDLE	10001
#define		ST_NOT_SYNC	10002
#define		ST_BAD_COOKIE	10003
#define		ST_NOTSUPP	10004
#define		ST_TOOSMALL	10005
#define		ST_SERVERFAULT	10006
#define		ST_BADTYPE	10007
#define		ST_JUKEBOX	10008
#define		ST_NOTMOUNTED	10009
#define		ST_MAINTMODE	10010
#define		ST_STALEACL	10011

/*
 *	On writes how does the client want the data written.
 */

#define	CACHE_CSTABLE		1
#define CACHE_UNSTABLE		2


#define	CMFILE_SYNCH_NVRAM	1
#define	CMDATA_SYNCH_NVRAM	2
#define	CMFILE_SYNCH		3
#define CMDATA_SYNCH		4
#define CMUNSTABLE		5

struct aac_read
{
	__le32		command;
	__le32		cid;
	__le32		block;
	__le32		count;
	struct sgmap	sg;	
};

struct aac_read64
{
	__le32		command;
	__le16		cid;
	__le16		sector_count;
	__le32		block;
	__le16		pad;
	__le16		flags;
	struct sgmap64	sg;	
};

struct aac_read_reply
{
	__le32		status;
	__le32		count;
};

struct aac_write
{
	__le32		command;
	__le32		cid;
	__le32		block;
	__le32		count;
	__le32		stable;	
	struct sgmap	sg;	
};

struct aac_write64
{
	__le32		command;
	__le16		cid;
	__le16		sector_count;
	__le32		block;
	__le16		pad;
	__le16		flags;
#define	IO_TYPE_WRITE 0x00000000
#define	IO_TYPE_READ  0x00000001
#define	IO_SUREWRITE  0x00000008
	struct sgmap64	sg;	
};
struct aac_write_reply
{
	__le32		status;
	__le32		count;
	__le32		committed;
};

struct aac_raw_io
{
	__le32		block[2];
	__le32		count;
	__le16		cid;
	__le16		flags;		
	__le16		bpTotal;	
	__le16		bpComplete;	
	struct sgmapraw	sg;
};

#define CT_FLUSH_CACHE 129
struct aac_synchronize {
	__le32		command;	
	__le32		type;		
	__le32		cid;
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	
};

struct aac_synchronize_reply {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	u8		data[16];
};

#define CT_POWER_MANAGEMENT	245
#define CT_PM_START_UNIT	2
#define CT_PM_STOP_UNIT		3
#define CT_PM_UNIT_IMMEDIATE	1
struct aac_power_management {
	__le32		command;	
	__le32		type;		
	__le32		sub;		
	__le32		cid;
	__le32		parm;		
};

#define CT_PAUSE_IO    65
#define CT_RELEASE_IO  66
struct aac_pause {
	__le32		command;	
	__le32		type;		
	__le32		timeout;	
	__le32		min;
	__le32		noRescan;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	
};

struct aac_srb
{
	__le32		function;
	__le32		channel;
	__le32		id;
	__le32		lun;
	__le32		timeout;
	__le32		flags;
	__le32		count;		
	__le32		retry_limit;
	__le32		cdb_size;
	u8		cdb[16];
	struct	sgmap	sg;
};

struct user_aac_srb
{
	u32		function;
	u32		channel;
	u32		id;
	u32		lun;
	u32		timeout;
	u32		flags;
	u32		count;		
	u32		retry_limit;
	u32		cdb_size;
	u8		cdb[16];
	struct	user_sgmap	sg;
};

#define		AAC_SENSE_BUFFERSIZE	 30

struct aac_srb_reply
{
	__le32		status;
	__le32		srb_status;
	__le32		scsi_status;
	__le32		data_xfer_length;
	__le32		sense_data_size;
	u8		sense_data[AAC_SENSE_BUFFERSIZE]; 
};
#define		SRB_NoDataXfer		 0x0000
#define		SRB_DisableDisconnect	 0x0004
#define		SRB_DisableSynchTransfer 0x0008
#define		SRB_BypassFrozenQueue	 0x0010
#define		SRB_DisableAutosense	 0x0020
#define		SRB_DataIn		 0x0040
#define		SRB_DataOut		 0x0080

#define	SRBF_ExecuteScsi	0x0000
#define	SRBF_ClaimDevice	0x0001
#define	SRBF_IO_Control		0x0002
#define	SRBF_ReceiveEvent	0x0003
#define	SRBF_ReleaseQueue	0x0004
#define	SRBF_AttachDevice	0x0005
#define	SRBF_ReleaseDevice	0x0006
#define	SRBF_Shutdown		0x0007
#define	SRBF_Flush		0x0008
#define	SRBF_AbortCommand	0x0010
#define	SRBF_ReleaseRecovery	0x0011
#define	SRBF_ResetBus		0x0012
#define	SRBF_ResetDevice	0x0013
#define	SRBF_TerminateIO	0x0014
#define	SRBF_FlushQueue		0x0015
#define	SRBF_RemoveDevice	0x0016
#define	SRBF_DomainValidation	0x0017

#define SRB_STATUS_PENDING                  0x00
#define SRB_STATUS_SUCCESS                  0x01
#define SRB_STATUS_ABORTED                  0x02
#define SRB_STATUS_ABORT_FAILED             0x03
#define SRB_STATUS_ERROR                    0x04
#define SRB_STATUS_BUSY                     0x05
#define SRB_STATUS_INVALID_REQUEST          0x06
#define SRB_STATUS_INVALID_PATH_ID          0x07
#define SRB_STATUS_NO_DEVICE                0x08
#define SRB_STATUS_TIMEOUT                  0x09
#define SRB_STATUS_SELECTION_TIMEOUT        0x0A
#define SRB_STATUS_COMMAND_TIMEOUT          0x0B
#define SRB_STATUS_MESSAGE_REJECTED         0x0D
#define SRB_STATUS_BUS_RESET                0x0E
#define SRB_STATUS_PARITY_ERROR             0x0F
#define SRB_STATUS_REQUEST_SENSE_FAILED     0x10
#define SRB_STATUS_NO_HBA                   0x11
#define SRB_STATUS_DATA_OVERRUN             0x12
#define SRB_STATUS_UNEXPECTED_BUS_FREE      0x13
#define SRB_STATUS_PHASE_SEQUENCE_FAILURE   0x14
#define SRB_STATUS_BAD_SRB_BLOCK_LENGTH     0x15
#define SRB_STATUS_REQUEST_FLUSHED          0x16
#define SRB_STATUS_DELAYED_RETRY	    0x17
#define SRB_STATUS_INVALID_LUN              0x20
#define SRB_STATUS_INVALID_TARGET_ID        0x21
#define SRB_STATUS_BAD_FUNCTION             0x22
#define SRB_STATUS_ERROR_RECOVERY           0x23
#define SRB_STATUS_NOT_STARTED		    0x24
#define SRB_STATUS_NOT_IN_USE		    0x30
#define SRB_STATUS_FORCE_ABORT		    0x31
#define SRB_STATUS_DOMAIN_VALIDATION_FAIL   0x32


#define		VM_Null			0
#define		VM_NameServe		1
#define		VM_ContainerConfig	2
#define		VM_Ioctl		3
#define		VM_FilesystemIoctl	4
#define		VM_CloseAll		5
#define		VM_CtBlockRead		6
#define		VM_CtBlockWrite		7
#define		VM_SliceBlockRead	8	
#define		VM_SliceBlockWrite	9
#define		VM_DriveBlockRead	10	
#define		VM_DriveBlockWrite	11
#define		VM_EnclosureMgt		12	
#define		VM_Unused		13	
#define		VM_CtBlockVerify	14
#define		VM_CtPerf		15	
#define		VM_CtBlockRead64	16
#define		VM_CtBlockWrite64	17
#define		VM_CtBlockVerify64	18
#define		VM_CtHostRead64		19
#define		VM_CtHostWrite64	20
#define		VM_DrvErrTblLog		21
#define		VM_NameServe64		22

#define		MAX_VMCOMMAND_NUM	23	


struct aac_fsinfo {
	__le32  fsTotalSize;	
	__le32  fsBlockSize;
	__le32  fsFragSize;
	__le32  fsMaxExtendSize;
	__le32  fsSpaceUnits;
	__le32  fsMaxNumFiles;
	__le32  fsNumFreeFiles;
	__le32  fsInodeDensity;
};	

union aac_contentinfo {
	struct aac_fsinfo filesys;	
};


#define CT_GET_CONFIG_STATUS 147
struct aac_get_config_status {
	__le32		command;	
	__le32		type;		
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	__le32		count;	
};

#define CFACT_CONTINUE 0
#define CFACT_PAUSE    1
#define CFACT_ABORT    2
struct aac_get_config_status_resp {
	__le32		response; 
	__le32		dummy0;
	__le32		status;	
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	struct {
		__le32	action; 
		__le16	flags;
		__le16	count;
	}		data;
};


#define CT_COMMIT_CONFIG 152

struct aac_commit_config {
	__le32		command;	
	__le32		type;		
};


#define CT_GET_CONTAINER_COUNT 4
struct aac_get_container_count {
	__le32		command;	
	__le32		type;		
};

struct aac_get_container_count_resp {
	__le32		response; 
	__le32		dummy0;
	__le32		MaxContainers;
	__le32		ContainerSwitchEntries;
	__le32		MaxPartitions;
};



struct aac_mntent {
	__le32			oid;
	u8			name[16];	
	struct creation_info	create_info;	
	__le32			capacity;
	__le32			vol;		
	__le32			obj;		
	__le32			state;		
	union aac_contentinfo	fileinfo;	
	__le32			altoid;		
	__le32			capacityhigh;
};

#define FSCS_NOTCLEAN	0x0001  
#define FSCS_READONLY	0x0002	
#define FSCS_HIDDEN	0x0004	
#define FSCS_NOT_READY	0x0008	

struct aac_query_mount {
	__le32		command;
	__le32		type;
	__le32		count;
};

struct aac_mount {
	__le32		status;
	__le32		type;           
	__le32		count;
	struct aac_mntent mnt[1];
};

#define CT_READ_NAME 130
struct aac_get_name {
	__le32		command;	
	__le32		type;		
	__le32		cid;
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	
};

struct aac_get_name_resp {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	u8		data[16];
};

#define CT_CID_TO_32BITS_UID 165
struct aac_get_serial {
	__le32		command;	
	__le32		type;		
	__le32		cid;
};

struct aac_get_serial_resp {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	
	__le32		uid;
};


struct aac_close {
	__le32	command;
	__le32	cid;
};

struct aac_query_disk
{
	s32	cnum;
	s32	bus;
	s32	id;
	s32	lun;
	u32	valid;
	u32	locked;
	u32	deleted;
	s32	instance;
	s8	name[10];
	u32	unmapped;
};

struct aac_delete_disk {
	u32	disknum;
	u32	cnum;
};

struct fib_ioctl
{
	u32	fibctx;
	s32	wait;
	char	__user *fib;
};

struct revision
{
	u32 compat;
	__le32 version;
	__le32 build;
};



#define CTL_CODE(function, method) (                 \
    (4<< 16) | ((function) << 2) | (method) \
)


#define METHOD_BUFFERED                 0
#define METHOD_NEITHER                  3


#define FSACTL_SENDFIB				CTL_CODE(2050, METHOD_BUFFERED)
#define FSACTL_SEND_RAW_SRB			CTL_CODE(2067, METHOD_BUFFERED)
#define FSACTL_DELETE_DISK			0x163
#define FSACTL_QUERY_DISK			0x173
#define FSACTL_OPEN_GET_ADAPTER_FIB		CTL_CODE(2100, METHOD_BUFFERED)
#define FSACTL_GET_NEXT_ADAPTER_FIB		CTL_CODE(2101, METHOD_BUFFERED)
#define FSACTL_CLOSE_GET_ADAPTER_FIB		CTL_CODE(2102, METHOD_BUFFERED)
#define FSACTL_MINIPORT_REV_CHECK               CTL_CODE(2107, METHOD_BUFFERED)
#define FSACTL_GET_PCI_INFO			CTL_CODE(2119, METHOD_BUFFERED)
#define FSACTL_FORCE_DELETE_DISK		CTL_CODE(2120, METHOD_NEITHER)
#define FSACTL_GET_CONTAINERS			2131
#define FSACTL_SEND_LARGE_FIB			CTL_CODE(2138, METHOD_BUFFERED)


struct aac_common
{
	u32 irq_mod;
	u32 peak_fibs;
	u32 zero_fibs;
	u32 fib_timeouts;
#ifdef DBG
	u32 FibsSent;
	u32 FibRecved;
	u32 NoResponseSent;
	u32 NoResponseRecved;
	u32 AsyncSent;
	u32 AsyncRecved;
	u32 NormalSent;
	u32 NormalRecved;
#endif
};

extern struct aac_common aac_config;



#ifdef DBG
#define	FIB_COUNTER_INCREMENT(counter)		(counter)++
#else
#define	FIB_COUNTER_INCREMENT(counter)
#endif


#define	BREAKPOINT_REQUEST		0x00000004
#define	INIT_STRUCT_BASE_ADDRESS	0x00000005
#define READ_PERMANENT_PARAMETERS	0x0000000a
#define WRITE_PERMANENT_PARAMETERS	0x0000000b
#define HOST_CRASHING			0x0000000d
#define	SEND_SYNCHRONOUS_FIB		0x0000000c
#define COMMAND_POST_RESULTS		0x00000014
#define GET_ADAPTER_PROPERTIES		0x00000019
#define GET_DRIVER_BUFFER_PROPERTIES	0x00000023
#define RCV_TEMP_READINGS		0x00000025
#define GET_COMM_PREFERRED_SETTINGS	0x00000026
#define IOP_RESET			0x00001000
#define IOP_RESET_ALWAYS		0x00001001
#define RE_INIT_ADAPTER			0x000000ee


#define	SELF_TEST_FAILED		0x00000004
#define	MONITOR_PANIC			0x00000020
#define	KERNEL_UP_AND_RUNNING		0x00000080
#define	KERNEL_PANIC			0x00000100


#define DoorBellSyncCmdAvailable	(1<<0)	
#define DoorBellPrintfDone		(1<<5)	
#define DoorBellAdapterNormCmdReady	(1<<1)	
#define DoorBellAdapterNormRespReady	(1<<2)	
#define DoorBellAdapterNormCmdNotFull	(1<<3)	
#define DoorBellAdapterNormRespNotFull	(1<<4)	
#define DoorBellPrintfReady		(1<<5)	
#define DoorBellAifPending		(1<<6)	

#define PmDoorBellResponseSent		(1<<1)	


#define		AifCmdEventNotify	1	
#define			AifEnConfigChange	3	
#define			AifEnContainerChange	4	
#define			AifEnDeviceFailure	5	
#define			AifEnEnclosureManagement 13	
#define				EM_DRIVE_INSERTION	31
#define				EM_DRIVE_REMOVAL	32
#define			AifEnBatteryEvent	14	
#define			AifEnAddContainer	15	
#define			AifEnDeleteContainer	16	
#define			AifEnExpEvent		23	
#define			AifExeFirmwarePanic	3	
#define			AifHighPriority		3	
#define			AifEnAddJBOD		30	
#define			AifEnDeleteJBOD		31	

#define		AifCmdJobProgress	2	
#define			AifJobCtrZero	101	
#define			AifJobStsSuccess 1	
#define			AifJobStsRunning 102	
#define		AifCmdAPIReport		3	
#define		AifCmdDriverNotify	4	
#define			AifDenMorphComplete 200	
#define			AifDenVolumeExtendComplete 201 
#define		AifReqJobList		100	
#define		AifReqJobsForCtr	101	
#define		AifReqJobsForScsi	102	
#define		AifReqJobReport		103	
#define		AifReqTerminateJob	104	
#define		AifReqSuspendJob	105	
#define		AifReqResumeJob		106	
#define		AifReqSendAPIReport	107	
#define		AifReqAPIJobStart	108	
#define		AifReqAPIJobUpdate	109	
#define		AifReqAPIJobFinish	110	

#define		AifReqEvent		200


struct aac_aifcmd {
	__le32 command;		
	__le32 seqnum;		
	u8 data[1];		
};

static inline unsigned int cap_to_cyls(sector_t capacity, unsigned divisor)
{
	sector_div(capacity, divisor);
	return capacity;
}

#define AAC_OWNER_MIDLEVEL	0x101
#define AAC_OWNER_LOWLEVEL	0x102
#define AAC_OWNER_ERROR_HANDLER	0x103
#define AAC_OWNER_FIRMWARE	0x106

const char *aac_driverinfo(struct Scsi_Host *);
struct fib *aac_fib_alloc(struct aac_dev *dev);
int aac_fib_setup(struct aac_dev *dev);
void aac_fib_map_free(struct aac_dev *dev);
void aac_fib_free(struct fib * context);
void aac_fib_init(struct fib * context);
void aac_printf(struct aac_dev *dev, u32 val);
int aac_fib_send(u16 command, struct fib * context, unsigned long size, int priority, int wait, int reply, fib_callback callback, void *ctxt);
int aac_consumer_get(struct aac_dev * dev, struct aac_queue * q, struct aac_entry **entry);
void aac_consumer_free(struct aac_dev * dev, struct aac_queue * q, u32 qnum);
int aac_fib_complete(struct fib * context);
#define fib_data(fibctx) ((void *)(fibctx)->hw_fib_va->data)
struct aac_dev *aac_init_adapter(struct aac_dev *dev);
int aac_get_config_status(struct aac_dev *dev, int commit_flag);
int aac_get_containers(struct aac_dev *dev);
int aac_scsi_cmd(struct scsi_cmnd *cmd);
int aac_dev_ioctl(struct aac_dev *dev, int cmd, void __user *arg);
#ifndef shost_to_class
#define shost_to_class(shost) &shost->shost_dev
#endif
ssize_t aac_get_serial_number(struct device *dev, char *buf);
int aac_do_ioctl(struct aac_dev * dev, int cmd, void __user *arg);
int aac_rx_init(struct aac_dev *dev);
int aac_rkt_init(struct aac_dev *dev);
int aac_nark_init(struct aac_dev *dev);
int aac_sa_init(struct aac_dev *dev);
int aac_src_init(struct aac_dev *dev);
int aac_srcv_init(struct aac_dev *dev);
int aac_queue_get(struct aac_dev * dev, u32 * index, u32 qid, struct hw_fib * hw_fib, int wait, struct fib * fibptr, unsigned long *nonotify);
unsigned int aac_response_normal(struct aac_queue * q);
unsigned int aac_command_normal(struct aac_queue * q);
unsigned int aac_intr_normal(struct aac_dev *dev, u32 Index,
			int isAif, int isFastResponse,
			struct hw_fib *aif_fib);
int aac_reset_adapter(struct aac_dev * dev, int forced);
int aac_check_health(struct aac_dev * dev);
int aac_command_thread(void *data);
int aac_close_fib_context(struct aac_dev * dev, struct aac_fib_context *fibctx);
int aac_fib_adapter_complete(struct fib * fibptr, unsigned short size);
struct aac_driver_ident* aac_get_driver_ident(int devtype);
int aac_get_adapter_info(struct aac_dev* dev);
int aac_send_shutdown(struct aac_dev *dev);
int aac_probe_container(struct aac_dev *dev, int cid);
int _aac_rx_init(struct aac_dev *dev);
int aac_rx_select_comm(struct aac_dev *dev, int comm);
int aac_rx_deliver_producer(struct fib * fib);
char * get_container_type(unsigned type);
extern int numacb;
extern int acbsize;
extern char aac_driver_version[];
extern int startup_timeout;
extern int aif_timeout;
extern int expose_physicals;
extern int aac_reset_devices;
extern int aac_msi;
extern int aac_commit;
extern int update_interval;
extern int check_interval;
extern int aac_check_reset;
