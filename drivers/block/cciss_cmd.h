#ifndef CCISS_CMD_H
#define CCISS_CMD_H

#include <linux/cciss_defs.h>

#define CISS_VERSION "1.00"

#define MAXSGENTRIES            32
#define CCISS_SG_CHAIN          0x80000000
#define MAXREPLYQS              256

#define POWER_OR_RESET			0x29
#define STATE_CHANGED			0x2a
#define UNIT_ATTENTION_CLEARED		0x2f
#define LUN_FAILED			0x3e
#define REPORT_LUNS_CHANGED		0x3f


	
#define POWER_ON_RESET			0x00
#define POWER_ON_REBOOT			0x01
#define SCSI_BUS_RESET			0x02
#define MSA_TARGET_RESET		0x03
#define CONTROLLER_FAILOVER		0x04
#define TRANSCEIVER_SE			0x05
#define TRANSCEIVER_LVD			0x06

	
#define RESERVATION_PREEMPTED		0x03
#define ASYM_ACCESS_CHANGED		0x06
#define LUN_CAPACITY_CHANGED		0x09

#define CFG_VENDORID            0x00
#define CFG_DEVICEID            0x02
#define CFG_I2OBAR              0x10
#define CFG_MEM1BAR             0x14

#define I2O_IBDB_SET            0x20
#define I2O_IBDB_CLEAR          0x70
#define I2O_INT_STATUS          0x30
#define I2O_INT_MASK            0x34
#define I2O_IBPOST_Q            0x40
#define I2O_OBPOST_Q            0x44
#define I2O_DMA1_CFG		0x214

#define CFGTBL_ChangeReq        0x00000001l
#define CFGTBL_AccCmds          0x00000001l
#define DOORBELL_CTLR_RESET     0x00000004l
#define DOORBELL_CTLR_RESET2    0x00000020l

#define CFGTBL_Trans_Simple     0x00000002l
#define CFGTBL_Trans_Performant 0x00000004l
#define CFGTBL_Trans_use_short_tags 0x20000000l

#define CFGTBL_BusType_Ultra2   0x00000001l
#define CFGTBL_BusType_Ultra3   0x00000002l
#define CFGTBL_BusType_Fibre1G  0x00000100l
#define CFGTBL_BusType_Fibre2G  0x00000200l
typedef struct _vals32
{
        __u32   lower;
        __u32   upper;
} vals32;

typedef union _u64bit
{
   vals32	val32;
   __u64	val;
} u64bit;

#define QWORD vals32 

#define CISS_MAX_PHYS_LUN	1024

#pragma pack(1)	

#define CISS_INQUIRY 0x12
typedef struct _InquiryData_struct
{
  BYTE data_byte[36];
} InquiryData_struct;

#define CISS_REPORT_LOG 0xc2    
#define CISS_REPORT_PHYS 0xc3   
typedef struct _ReportLUNdata_struct
{
  BYTE LUNListLength[4];
  DWORD reserved;
  BYTE LUN[CISS_MAX_LUN][8];
} ReportLunData_struct;

#define CCISS_READ_CAPACITY 0x25  
typedef struct _ReadCapdata_struct
{
  BYTE total_size[4];	
  BYTE block_size[4];	
} ReadCapdata_struct;

#define CCISS_READ_CAPACITY_16 0x9e 


#define CCISS_READ_CAPACITY_16_SERVICE_ACT 0x10

typedef struct _ReadCapdata_struct_16
{
	BYTE total_size[8];   
	BYTE block_size[4];   
	BYTE prot_en:1;       
	BYTE rto_en:1;        
	BYTE reserved:6;      
	BYTE reserved2[18];   
} ReadCapdata_struct_16;


#define CCISS_READ_10   0x28    
#define CCISS_WRITE_10  0x2a    
#define CCISS_READ_16   0x88    
#define CCISS_WRITE_16  0x8a    


#define CDB_LEN10	10
#define CDB_LEN16	16

#define BMIC_READ 0x26
#define BMIC_WRITE 0x27
#define BMIC_CACHE_FLUSH 0xc2
#define CCISS_CACHE_FLUSH 0x01	

#define CCISS_ABORT_MSG 0x00
#define CCISS_RESET_MSG 0x01
#define CCISS_RESET_TYPE_CONTROLLER 0x00
#define CCISS_RESET_TYPE_BUS 0x01
#define CCISS_RESET_TYPE_TARGET 0x03
#define CCISS_RESET_TYPE_LUN 0x04
#define CCISS_NOOP_MSG 0x03

#define CTLR_LUNID "\0\0\0\0\0\0\0\0"

typedef struct _CommandListHeader_struct {
  BYTE              ReplyQueue;
  BYTE              SGList;
  HWORD             SGTotal;
  QWORD             Tag;
  LUNAddr_struct    LUN;
} CommandListHeader_struct;
typedef struct _ErrDescriptor_struct {
  QWORD  Addr;
  DWORD  Len;
} ErrDescriptor_struct;
typedef struct _SGDescriptor_struct {
  QWORD  Addr;
  DWORD  Len;
  DWORD  Ext;
} SGDescriptor_struct;

#define CMD_RWREQ       0x00
#define CMD_IOCTL_PEND  0x01
#define CMD_SCSI	0x03
#define CMD_MSG_DONE	0x04
#define CMD_MSG_TIMEOUT 0x05
#define CMD_MSG_STALE	0xff

#define COMMANDLIST_ALIGNMENT (32)
#define IS_64_BIT ((sizeof(long) - 4)/4)
#define IS_32_BIT (!IS_64_BIT)
#define PAD_32 (0)
#define PAD_64 (4)
#define PADSIZE (IS_32_BIT * PAD_32 + IS_64_BIT * PAD_64)
#define DIRECT_LOOKUP_BIT 0x10
#define DIRECT_LOOKUP_SHIFT 5

typedef struct _CommandList_struct {
  CommandListHeader_struct Header;
  RequestBlock_struct      Request;
  ErrDescriptor_struct     ErrDesc;
  SGDescriptor_struct      SG[MAXSGENTRIES];
	 
  __u32			   busaddr; 
  ErrorInfo_struct * 	   err_info;  
  int			   ctlr;
  int			   cmd_type; 
  long			   cmdindex;
  struct list_head list;
  struct request *	   rq;
  struct completion *waiting;
  int	 retry_count;
  void * scsi_cmd;
  char pad[PADSIZE];
} CommandList_struct;

typedef struct _HostWrite_struct {
  DWORD TransportRequest;
  DWORD Reserved;
  DWORD CoalIntDelay;
  DWORD CoalIntCount;
} HostWrite_struct;

typedef struct _CfgTable_struct {
  BYTE             Signature[4];
  DWORD            SpecValence;
#define SIMPLE_MODE	0x02
#define PERFORMANT_MODE	0x04
#define MEMQ_MODE	0x08
  DWORD            TransportSupport;
  DWORD            TransportActive;
  HostWrite_struct HostWrite;
  DWORD            CmdsOutMax;
  DWORD            BusTypes;
  DWORD            TransMethodOffset;
  BYTE             ServerName[16];
  DWORD            HeartBeat;
  DWORD            SCSI_Prefetch;
  DWORD            MaxSGElements;
  DWORD            MaxLogicalUnits;
  DWORD            MaxPhysicalDrives;
  DWORD            MaxPhysicalDrivesPerLogicalUnit;
  DWORD            MaxPerformantModeCommands;
  u8		   reserved[0x78 - 0x58];
  u32		   misc_fw_support; 
#define MISC_FW_DOORBELL_RESET (0x02)
#define MISC_FW_DOORBELL_RESET2 (0x10)
	u8	   driver_version[32];
} CfgTable_struct;

struct TransTable_struct {
  u32 BlockFetch0;
  u32 BlockFetch1;
  u32 BlockFetch2;
  u32 BlockFetch3;
  u32 BlockFetch4;
  u32 BlockFetch5;
  u32 BlockFetch6;
  u32 BlockFetch7;
  u32 RepQSize;
  u32 RepQCount;
  u32 RepQCtrAddrLow32;
  u32 RepQCtrAddrHigh32;
  u32 RepQAddr0Low32;
  u32 RepQAddr0High32;
};

#pragma pack()	 
#endif 
