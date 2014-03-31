/*

  Linux Driver for Mylex DAC960/AcceleRAID/eXtremeRAID PCI RAID Controllers

  Copyright 1998-2001 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that any modifications to this software be
  sent directly to him for evaluation and testing.

*/



#define DAC960_MaxControllers			8



#define DAC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4



#define DAC960_V1_MaxTargets			16
#define DAC960_V2_MaxTargets			128



#define DAC960_MaxLogicalDrives			32



#define DAC960_V1_MaxPhysicalDevices		45
#define DAC960_V2_MaxPhysicalDevices		272


typedef unsigned long DAC960_IO_Address_T;



typedef unsigned long DAC960_PCI_Address_T;



typedef unsigned int DAC960_BusAddress32_T;



typedef unsigned long long DAC960_BusAddress64_T;



typedef unsigned int DAC960_ByteCount32_T;



typedef unsigned long long DAC960_ByteCount64_T;



struct dma_loaf {
	void	*cpu_base;
	dma_addr_t dma_base;
	size_t  length;
	void	*cpu_free;
	dma_addr_t dma_free;
};


typedef struct DAC960_SCSI_Inquiry
{
  unsigned char PeripheralDeviceType:5;			
  unsigned char PeripheralQualifier:3;			
  unsigned char DeviceTypeModifier:7;			
  bool RMB:1;						
  unsigned char ANSI_ApprovedVersion:3;			
  unsigned char ECMA_Version:3;				
  unsigned char ISO_Version:2;				
  unsigned char ResponseDataFormat:4;			
  unsigned char :2;					
  bool TrmIOP:1;					
  bool AENC:1;						
  unsigned char AdditionalLength;			
  unsigned char :8;					
  unsigned char :8;					
  bool SftRe:1;						
  bool CmdQue:1;					
  bool :1;						
  bool Linked:1;					
  bool Sync:1;						
  bool WBus16:1;					
  bool WBus32:1;					
  bool RelAdr:1;					
  unsigned char VendorIdentification[8];		
  unsigned char ProductIdentification[16];		
  unsigned char ProductRevisionLevel[4];		
}
DAC960_SCSI_Inquiry_T;



typedef struct DAC960_SCSI_Inquiry_UnitSerialNumber
{
  unsigned char PeripheralDeviceType:5;			
  unsigned char PeripheralQualifier:3;			
  unsigned char PageCode;				
  unsigned char :8;					
  unsigned char PageLength;				
  unsigned char ProductSerialNumber[28];		
}
DAC960_SCSI_Inquiry_UnitSerialNumber_T;



typedef enum
{
  DAC960_SenseKey_NoSense =			0x0,
  DAC960_SenseKey_RecoveredError =		0x1,
  DAC960_SenseKey_NotReady =			0x2,
  DAC960_SenseKey_MediumError =			0x3,
  DAC960_SenseKey_HardwareError =		0x4,
  DAC960_SenseKey_IllegalRequest =		0x5,
  DAC960_SenseKey_UnitAttention =		0x6,
  DAC960_SenseKey_DataProtect =			0x7,
  DAC960_SenseKey_BlankCheck =			0x8,
  DAC960_SenseKey_VendorSpecific =		0x9,
  DAC960_SenseKey_CopyAborted =			0xA,
  DAC960_SenseKey_AbortedCommand =		0xB,
  DAC960_SenseKey_Equal =			0xC,
  DAC960_SenseKey_VolumeOverflow =		0xD,
  DAC960_SenseKey_Miscompare =			0xE,
  DAC960_SenseKey_Reserved =			0xF
}
__attribute__ ((packed))
DAC960_SCSI_RequestSenseKey_T;



typedef struct DAC960_SCSI_RequestSense
{
  unsigned char ErrorCode:7;				
  bool Valid:1;						
  unsigned char SegmentNumber;				
  DAC960_SCSI_RequestSenseKey_T SenseKey:4;		
  unsigned char :1;					
  bool ILI:1;						
  bool EOM:1;						
  bool Filemark:1;					
  unsigned char Information[4];				
  unsigned char AdditionalSenseLength;			
  unsigned char CommandSpecificInformation[4];		
  unsigned char AdditionalSenseCode;			
  unsigned char AdditionalSenseCodeQualifier;		
}
DAC960_SCSI_RequestSense_T;



typedef enum
{
  
  DAC960_V1_ReadExtended =			0x33,
  DAC960_V1_WriteExtended =			0x34,
  DAC960_V1_ReadAheadExtended =			0x35,
  DAC960_V1_ReadExtendedWithScatterGather =	0xB3,
  DAC960_V1_WriteExtendedWithScatterGather =	0xB4,
  DAC960_V1_Read =				0x36,
  DAC960_V1_ReadWithScatterGather =		0xB6,
  DAC960_V1_Write =				0x37,
  DAC960_V1_WriteWithScatterGather =		0xB7,
  DAC960_V1_DCDB =				0x04,
  DAC960_V1_DCDBWithScatterGather =		0x84,
  DAC960_V1_Flush =				0x0A,
  
  DAC960_V1_Enquiry =				0x53,
  DAC960_V1_Enquiry2 =				0x1C,
  DAC960_V1_GetLogicalDriveElement =		0x55,
  DAC960_V1_GetLogicalDriveInformation =	0x19,
  DAC960_V1_IOPortRead =			0x39,
  DAC960_V1_IOPortWrite =			0x3A,
  DAC960_V1_GetSDStats =			0x3E,
  DAC960_V1_GetPDStats =			0x3F,
  DAC960_V1_PerformEventLogOperation =		0x72,
  
  DAC960_V1_StartDevice =			0x10,
  DAC960_V1_GetDeviceState =			0x50,
  DAC960_V1_StopChannel =			0x13,
  DAC960_V1_StartChannel =			0x12,
  DAC960_V1_ResetChannel =			0x1A,
  
  DAC960_V1_Rebuild =				0x09,
  DAC960_V1_RebuildAsync =			0x16,
  DAC960_V1_CheckConsistency =			0x0F,
  DAC960_V1_CheckConsistencyAsync =		0x1E,
  DAC960_V1_RebuildStat =			0x0C,
  DAC960_V1_GetRebuildProgress =		0x27,
  DAC960_V1_RebuildControl =			0x1F,
  DAC960_V1_ReadBadBlockTable =			0x0B,
  DAC960_V1_ReadBadDataTable =			0x25,
  DAC960_V1_ClearBadDataTable =			0x26,
  DAC960_V1_GetErrorTable =			0x17,
  DAC960_V1_AddCapacityAsync =			0x2A,
  DAC960_V1_BackgroundInitializationControl =	0x2B,
  
  DAC960_V1_ReadConfig2 =			0x3D,
  DAC960_V1_WriteConfig2 =			0x3C,
  DAC960_V1_ReadConfigurationOnDisk =		0x4A,
  DAC960_V1_WriteConfigurationOnDisk =		0x4B,
  DAC960_V1_ReadConfiguration =			0x4E,
  DAC960_V1_ReadBackupConfiguration =		0x4D,
  DAC960_V1_WriteConfiguration =		0x4F,
  DAC960_V1_AddConfiguration =			0x4C,
  DAC960_V1_ReadConfigurationLabel =		0x48,
  DAC960_V1_WriteConfigurationLabel =		0x49,
  
  DAC960_V1_LoadImage =				0x20,
  DAC960_V1_StoreImage =			0x21,
  DAC960_V1_ProgramImage =			0x22,
  
  DAC960_V1_SetDiagnosticMode =			0x31,
  DAC960_V1_RunDiagnostic =			0x32,
  
  DAC960_V1_GetSubsystemData =			0x70,
  DAC960_V1_SetSubsystemParameters =		0x71,
  
  DAC960_V1_Enquiry_Old =			0x05,
  DAC960_V1_GetDeviceState_Old =		0x14,
  DAC960_V1_Read_Old =				0x02,
  DAC960_V1_Write_Old =				0x03,
  DAC960_V1_ReadWithScatterGather_Old =		0x82,
  DAC960_V1_WriteWithScatterGather_Old =	0x83
}
__attribute__ ((packed))
DAC960_V1_CommandOpcode_T;



typedef unsigned char DAC960_V1_CommandIdentifier_T;



#define DAC960_V1_NormalCompletion		0x0000	
#define DAC960_V1_CheckConditionReceived	0x0002	
#define DAC960_V1_NoDeviceAtAddress		0x0102	
#define DAC960_V1_InvalidDeviceAddress		0x0105	
#define DAC960_V1_InvalidParameter		0x0105	
#define DAC960_V1_IrrecoverableDataError	0x0001	
#define DAC960_V1_LogicalDriveNonexistentOrOffline 0x0002 
#define DAC960_V1_AccessBeyondEndOfLogicalDrive	0x0105	
#define DAC960_V1_BadDataEncountered		0x010C	
#define DAC960_V1_DeviceBusy			0x0008	
#define DAC960_V1_DeviceNonresponsive		0x000E	
#define DAC960_V1_CommandTerminatedAbnormally	0x000F	
#define DAC960_V1_UnableToStartDevice		0x0002	
#define DAC960_V1_InvalidChannelOrTargetOrModifier 0x0105 
#define DAC960_V1_ChannelBusy			0x0106	
#define DAC960_V1_ChannelNotStopped		0x0002	
#define DAC960_V1_AttemptToRebuildOnlineDrive	0x0002	
#define DAC960_V1_RebuildBadBlocksEncountered	0x0003	
#define DAC960_V1_NewDiskFailedDuringRebuild	0x0004	
#define DAC960_V1_RebuildOrCheckAlreadyInProgress 0x0106 
#define DAC960_V1_DependentDiskIsDead		0x0002	
#define DAC960_V1_InconsistentBlocksFound	0x0003	
#define DAC960_V1_InvalidOrNonredundantLogicalDrive 0x0105 
#define DAC960_V1_NoRebuildOrCheckInProgress	0x0105	
#define DAC960_V1_RebuildInProgress_DataValid	0x0000	
#define DAC960_V1_RebuildFailed_LogicalDriveFailure 0x0002 
#define DAC960_V1_RebuildFailed_BadBlocksOnOther 0x0003	
#define DAC960_V1_RebuildFailed_NewDriveFailed	0x0004	
#define DAC960_V1_RebuildSuccessful		0x0100	
#define DAC960_V1_RebuildSuccessfullyTerminated	0x0107	
#define DAC960_V1_BackgroundInitSuccessful	0x0100	
#define DAC960_V1_BackgroundInitAborted		0x0005	
#define DAC960_V1_NoBackgroundInitInProgress	0x0105	
#define DAC960_V1_AddCapacityInProgress		0x0004	
#define DAC960_V1_AddCapacityFailedOrSuspended	0x00F4	
#define DAC960_V1_Config2ChecksumError		0x0002	
#define DAC960_V1_ConfigurationSuspended	0x0106	
#define DAC960_V1_FailedToConfigureNVRAM	0x0105	
#define DAC960_V1_ConfigurationNotSavedStateChange 0x0106 
#define DAC960_V1_SubsystemNotInstalled		0x0001	
#define DAC960_V1_SubsystemFailed		0x0002	
#define DAC960_V1_SubsystemBusy			0x0106	

typedef unsigned short DAC960_V1_CommandStatus_T;



typedef struct DAC960_V1_Enquiry
{
  unsigned char NumberOfLogicalDrives;			
  unsigned int :24;					
  unsigned int LogicalDriveSizes[32];			
  unsigned short FlashAge;				
  struct {
    bool DeferredWriteError:1;				
    bool BatteryLow:1;					
    unsigned char :6;					
  } StatusFlags;
  unsigned char :8;					
  unsigned char MinorFirmwareVersion;			
  unsigned char MajorFirmwareVersion;			
  enum {
    DAC960_V1_NoStandbyRebuildOrCheckInProgress =		    0x00,
    DAC960_V1_StandbyRebuildInProgress =			    0x01,
    DAC960_V1_BackgroundRebuildInProgress =			    0x02,
    DAC960_V1_BackgroundCheckInProgress =			    0x03,
    DAC960_V1_StandbyRebuildCompletedWithError =		    0xFF,
    DAC960_V1_BackgroundRebuildOrCheckFailed_DriveFailed =	    0xF0,
    DAC960_V1_BackgroundRebuildOrCheckFailed_LogicalDriveFailed =   0xF1,
    DAC960_V1_BackgroundRebuildOrCheckFailed_OtherCauses =	    0xF2,
    DAC960_V1_BackgroundRebuildOrCheckSuccessfullyTerminated =	    0xF3
  } __attribute__ ((packed)) RebuildFlag;		
  unsigned char MaxCommands;				
  unsigned char OfflineLogicalDriveCount;		
  unsigned char :8;					
  unsigned short EventLogSequenceNumber;		
  unsigned char CriticalLogicalDriveCount;		
  unsigned int :24;					
  unsigned char DeadDriveCount;				
  unsigned char :8;					
  unsigned char RebuildCount;				
  struct {
    unsigned char :3;					
    bool BatteryBackupUnitPresent:1;			
    unsigned char :3;					
    unsigned char :1;					
  } MiscFlags;
  struct {
    unsigned char TargetID;
    unsigned char Channel;
  } DeadDrives[21];					
  unsigned char Reserved[62];				
}
__attribute__ ((packed))
DAC960_V1_Enquiry_T;



typedef struct DAC960_V1_Enquiry2
{
  struct {
    enum {
      DAC960_V1_P_PD_PU =			0x01,
      DAC960_V1_PL =				0x02,
      DAC960_V1_PG =				0x10,
      DAC960_V1_PJ =				0x11,
      DAC960_V1_PR =				0x12,
      DAC960_V1_PT =				0x13,
      DAC960_V1_PTL0 =				0x14,
      DAC960_V1_PRL =				0x15,
      DAC960_V1_PTL1 =				0x16,
      DAC960_V1_1164P =				0x20
    } __attribute__ ((packed)) SubModel;		
    unsigned char ActualChannels;			
    enum {
      DAC960_V1_FiveChannelBoard =		0x01,
      DAC960_V1_ThreeChannelBoard =		0x02,
      DAC960_V1_TwoChannelBoard =		0x03,
      DAC960_V1_ThreeChannelASIC_DAC =		0x04
    } __attribute__ ((packed)) Model;			
    enum {
      DAC960_V1_EISA_Controller =		0x01,
      DAC960_V1_MicroChannel_Controller =	0x02,
      DAC960_V1_PCI_Controller =		0x03,
      DAC960_V1_SCSItoSCSI_Controller =		0x08
    } __attribute__ ((packed)) ProductFamily;		
  } HardwareID;						
  
  struct {
    unsigned char MajorVersion;				
    unsigned char MinorVersion;				
    unsigned char TurnID;				
    char FirmwareType;					
  } FirmwareID;						
  unsigned char :8;					
  unsigned int :24;					
  unsigned char ConfiguredChannels;			
  unsigned char ActualChannels;				
  unsigned char MaxTargets;				
  unsigned char MaxTags;				
  unsigned char MaxLogicalDrives;			
  unsigned char MaxArms;				
  unsigned char MaxSpans;				
  unsigned char :8;					
  unsigned int :32;					
  unsigned int MemorySize;				
  unsigned int CacheSize;				
  unsigned int FlashMemorySize;				
  unsigned int NonVolatileMemorySize;			
  struct {
    enum {
      DAC960_V1_RamType_DRAM =			0x0,
      DAC960_V1_RamType_EDO =			0x1,
      DAC960_V1_RamType_SDRAM =			0x2,
      DAC960_V1_RamType_Last =			0x7
    } __attribute__ ((packed)) RamType:3;		
    enum {
      DAC960_V1_ErrorCorrection_None =		0x0,
      DAC960_V1_ErrorCorrection_Parity =	0x1,
      DAC960_V1_ErrorCorrection_ECC =		0x2,
      DAC960_V1_ErrorCorrection_Last =		0x7
    } __attribute__ ((packed)) ErrorCorrection:3;	
    bool FastPageMode:1;				
    bool LowPowerMemory:1;				
    unsigned char :8;					
  } MemoryType;
  unsigned short ClockSpeed;				
  unsigned short MemorySpeed;				
  unsigned short HardwareSpeed;				
  unsigned int :32;					
  unsigned int :32;					
  unsigned char :8;					
  unsigned char :8;					
  unsigned short :16;					
  unsigned short MaxCommands;				
  unsigned short MaxScatterGatherEntries;		
  unsigned short MaxDriveCommands;			
  unsigned short MaxIODescriptors;			
  unsigned short MaxCombinedSectors;			
  unsigned char Latency;				
  unsigned char :8;					
  unsigned char SCSITimeout;				
  unsigned char :8;					
  unsigned short MinFreeLines;				
  unsigned int :32;					
  unsigned int :32;					
  unsigned char RebuildRateConstant;			
  unsigned char :8;					
  unsigned char :8;					
  unsigned char :8;					
  unsigned int :32;					
  unsigned int :32;					
  unsigned short PhysicalDriveBlockSize;		
  unsigned short LogicalDriveBlockSize;			
  unsigned short MaxBlocksPerCommand;			
  unsigned short BlockFactor;				
  unsigned short CacheLineSize;				
  struct {
    enum {
      DAC960_V1_Narrow_8bit =			0x0,
      DAC960_V1_Wide_16bit =			0x1,
      DAC960_V1_Wide_32bit =			0x2
    } __attribute__ ((packed)) BusWidth:2;		
    enum {
      DAC960_V1_Fast =				0x0,
      DAC960_V1_Ultra =				0x1,
      DAC960_V1_Ultra2 =			0x2
    } __attribute__ ((packed)) BusSpeed:2;		
    bool Differential:1;				
    unsigned char :3;					
  } SCSICapability;
  unsigned char :8;					
  unsigned int :32;					
  unsigned short FirmwareBuildNumber;			
  enum {
    DAC960_V1_AEMI =				0x01,
    DAC960_V1_OEM1 =				0x02,
    DAC960_V1_OEM2 =				0x04,
    DAC960_V1_OEM3 =				0x08,
    DAC960_V1_Conner =				0x10,
    DAC960_V1_SAFTE =				0x20
  } __attribute__ ((packed)) FaultManagementType;	
  unsigned char :8;					
  struct {
    bool Clustering:1;					
    bool MylexOnlineRAIDExpansion:1;			
    bool ReadAhead:1;					
    bool BackgroundInitialization:1;			
    unsigned int :28;					
  } FirmwareFeatures;
  unsigned int :32;					
  unsigned int :32;					
}
DAC960_V1_Enquiry2_T;



typedef enum
{
  DAC960_V1_LogicalDrive_Online =		0x03,
  DAC960_V1_LogicalDrive_Critical =		0x04,
  DAC960_V1_LogicalDrive_Offline =		0xFF
}
__attribute__ ((packed))
DAC960_V1_LogicalDriveState_T;



typedef struct DAC960_V1_LogicalDriveInformation
{
  unsigned int LogicalDriveSize;			
  DAC960_V1_LogicalDriveState_T LogicalDriveState;	
  unsigned char RAIDLevel:7;				
  bool WriteBack:1;					
  unsigned short :16;					
}
DAC960_V1_LogicalDriveInformation_T;



typedef DAC960_V1_LogicalDriveInformation_T
	DAC960_V1_LogicalDriveInformationArray_T[DAC960_MaxLogicalDrives];



typedef enum
{
  DAC960_V1_GetEventLogEntry =			0x00
}
__attribute__ ((packed))
DAC960_V1_PerformEventLogOpType_T;



typedef struct DAC960_V1_EventLogEntry
{
  unsigned char MessageType;				
  unsigned char MessageLength;				
  unsigned char TargetID:5;				
  unsigned char Channel:3;				
  unsigned char LogicalUnit:6;				
  unsigned char :2;					
  unsigned short SequenceNumber;			
  unsigned char ErrorCode:7;				
  bool Valid:1;						
  unsigned char SegmentNumber;				
  DAC960_SCSI_RequestSenseKey_T SenseKey:4;		
  unsigned char :1;					
  bool ILI:1;						
  bool EOM:1;						
  bool Filemark:1;					
  unsigned char Information[4];				
  unsigned char AdditionalSenseLength;			
  unsigned char CommandSpecificInformation[4];		
  unsigned char AdditionalSenseCode;			
  unsigned char AdditionalSenseCodeQualifier;		
  unsigned char Dummy[12];				
}
DAC960_V1_EventLogEntry_T;



typedef enum
{
    DAC960_V1_Device_Dead =			0x00,
    DAC960_V1_Device_WriteOnly =		0x02,
    DAC960_V1_Device_Online =			0x03,
    DAC960_V1_Device_Standby =			0x10
}
__attribute__ ((packed))
DAC960_V1_PhysicalDeviceState_T;



typedef struct DAC960_V1_DeviceState
{
  bool Present:1;					
  unsigned char :7;					
  enum {
    DAC960_V1_OtherType =			0x0,
    DAC960_V1_DiskType =			0x1,
    DAC960_V1_SequentialType =			0x2,
    DAC960_V1_CDROM_or_WORM_Type =		0x3
    } __attribute__ ((packed)) DeviceType:2;		
  bool :1;						
  bool Fast20:1;					
  bool Sync:1;						
  bool Fast:1;						
  bool Wide:1;						
  bool TaggedQueuingSupported:1;			
  DAC960_V1_PhysicalDeviceState_T DeviceState;		
  unsigned char :8;					
  unsigned char SynchronousMultiplier;			
  unsigned char SynchronousOffset:5;			
  unsigned char :3;					
  unsigned int DiskSize __attribute__ ((packed));	
  unsigned short :16;					
}
DAC960_V1_DeviceState_T;



typedef struct DAC960_V1_RebuildProgress
{
  unsigned int LogicalDriveNumber;			
  unsigned int LogicalDriveSize;			
  unsigned int RemainingBlocks;				
}
DAC960_V1_RebuildProgress_T;



typedef struct DAC960_V1_BackgroundInitializationStatus
{
  unsigned int LogicalDriveSize;			
  unsigned int BlocksCompleted;				
  unsigned char Reserved1[12];				
  unsigned int LogicalDriveNumber;			
  unsigned char RAIDLevel;				
  enum {
    DAC960_V1_BackgroundInitializationInvalid =	    0x00,
    DAC960_V1_BackgroundInitializationStarted =	    0x02,
    DAC960_V1_BackgroundInitializationInProgress =  0x04,
    DAC960_V1_BackgroundInitializationSuspended =   0x05,
    DAC960_V1_BackgroundInitializationCancelled =   0x06
  } __attribute__ ((packed)) Status;			
  unsigned char Reserved2[6];				
}
DAC960_V1_BackgroundInitializationStatus_T;



typedef struct DAC960_V1_ErrorTableEntry
{
  unsigned char ParityErrorCount;			
  unsigned char SoftErrorCount;				
  unsigned char HardErrorCount;				
  unsigned char MiscErrorCount;				
}
DAC960_V1_ErrorTableEntry_T;



typedef struct DAC960_V1_ErrorTable
{
  DAC960_V1_ErrorTableEntry_T
    ErrorTableEntries[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
}
DAC960_V1_ErrorTable_T;



typedef struct DAC960_V1_Config2
{
  unsigned char :1;					
  bool ActiveNegationEnabled:1;				
  unsigned char :5;					
  bool NoRescanIfResetReceivedDuringScan:1;		
  bool StorageWorksSupportEnabled:1;			
  bool HewlettPackardSupportEnabled:1;			
  bool NoDisconnectOnFirstCommand:1;			
  unsigned char :2;					
  bool AEMI_ARM:1;					
  bool AEMI_OFM:1;					
  unsigned char :1;					
  enum {
    DAC960_V1_OEMID_Mylex =			0x00,
    DAC960_V1_OEMID_IBM =			0x08,
    DAC960_V1_OEMID_HP =			0x0A,
    DAC960_V1_OEMID_DEC =			0x0C,
    DAC960_V1_OEMID_Siemens =			0x10,
    DAC960_V1_OEMID_Intel =			0x12
  } __attribute__ ((packed)) OEMID;			
  unsigned char OEMModelNumber;				
  unsigned char PhysicalSector;				
  unsigned char LogicalSector;				
  unsigned char BlockFactor;				
  bool ReadAheadEnabled:1;				
  bool LowBIOSDelay:1;					
  unsigned char :2;					
  bool ReassignRestrictedToOneSector:1;			
  unsigned char :1;					
  bool ForceUnitAccessDuringWriteRecovery:1;		
  bool EnableLeftSymmetricRAID5Algorithm:1;		
  unsigned char DefaultRebuildRate;			
  unsigned char :8;					
  unsigned char BlocksPerCacheLine;			
  unsigned char BlocksPerStripe;			
  struct {
    enum {
      DAC960_V1_Async =				0x0,
      DAC960_V1_Sync_8MHz =			0x1,
      DAC960_V1_Sync_5MHz =			0x2,
      DAC960_V1_Sync_10or20MHz =		0x3	
    } __attribute__ ((packed)) Speed:2;
    bool Force8Bit:1;					
    bool DisableFast20:1;				
    unsigned char :3;					
    bool EnableTaggedQueuing:1;				
  } __attribute__ ((packed)) ChannelParameters[6];	
  unsigned char SCSIInitiatorID;			
  unsigned char :8;					
  enum {
    DAC960_V1_StartupMode_ControllerSpinUp =	0x00,
    DAC960_V1_StartupMode_PowerOnSpinUp =	0x01
  } __attribute__ ((packed)) StartupMode;		
  unsigned char SimultaneousDeviceSpinUpCount;		
  unsigned char SecondsDelayBetweenSpinUps;		
  unsigned char Reserved1[29];				
  bool BIOSDisabled:1;					
  bool CDROMBootEnabled:1;				
  unsigned char :3;					
  enum {
    DAC960_V1_Geometry_128_32 =			0x0,
    DAC960_V1_Geometry_255_63 =			0x1,
    DAC960_V1_Geometry_Reserved1 =		0x2,
    DAC960_V1_Geometry_Reserved2 =		0x3
  } __attribute__ ((packed)) DriveGeometry:2;		
  unsigned char :1;					
  unsigned char Reserved2[9];				
  unsigned short Checksum;				
}
DAC960_V1_Config2_T;



typedef struct DAC960_V1_DCDB
{
  unsigned char TargetID:4;				 
  unsigned char Channel:4;				 
  enum {
    DAC960_V1_DCDB_NoDataTransfer =		0,
    DAC960_V1_DCDB_DataTransferDeviceToSystem = 1,
    DAC960_V1_DCDB_DataTransferSystemToDevice = 2,
    DAC960_V1_DCDB_IllegalDataTransfer =	3
  } __attribute__ ((packed)) Direction:2;		 
  bool EarlyStatus:1;					 
  unsigned char :1;					 
  enum {
    DAC960_V1_DCDB_Timeout_24_hours =		0,
    DAC960_V1_DCDB_Timeout_10_seconds =		1,
    DAC960_V1_DCDB_Timeout_60_seconds =		2,
    DAC960_V1_DCDB_Timeout_10_minutes =		3
  } __attribute__ ((packed)) Timeout:2;			 
  bool NoAutomaticRequestSense:1;			 
  bool DisconnectPermitted:1;				 
  unsigned short TransferLength;			 
  DAC960_BusAddress32_T BusAddress;			 
  unsigned char CDBLength:4;				 
  unsigned char TransferLengthHigh4:4;			 
  unsigned char SenseLength;				 
  unsigned char CDB[12];				 
  unsigned char SenseData[64];				 
  unsigned char Status;					 
  unsigned char :8;					 
}
DAC960_V1_DCDB_T;



typedef struct DAC960_V1_ScatterGatherSegment
{
  DAC960_BusAddress32_T SegmentDataPointer;		
  DAC960_ByteCount32_T SegmentByteCount;		
}
DAC960_V1_ScatterGatherSegment_T;



typedef union DAC960_V1_CommandMailbox
{
  unsigned int Words[4];				
  unsigned char Bytes[16];				
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char Dummy[14];				
  } __attribute__ ((packed)) Common;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char Dummy1[6];				
    DAC960_BusAddress32_T BusAddress;			
    unsigned char Dummy2[4];				
  } __attribute__ ((packed)) Type3;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char CommandOpcode2;			
    unsigned char Dummy1[5];				
    DAC960_BusAddress32_T BusAddress;			
    unsigned char Dummy2[4];				
  } __attribute__ ((packed)) Type3B;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char Dummy1[5];				
    unsigned char LogicalDriveNumber:6;			
    bool AutoRestore:1;					
    unsigned char Dummy2[8];				
  } __attribute__ ((packed)) Type3C;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char Channel;				
    unsigned char TargetID;				
    DAC960_V1_PhysicalDeviceState_T DeviceState:5;	
    unsigned char Modifier:3;				
    unsigned char Dummy1[3];				
    DAC960_BusAddress32_T BusAddress;			
    unsigned char Dummy2[4];				
  } __attribute__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    DAC960_V1_PerformEventLogOpType_T OperationType;	
    unsigned char OperationQualifier;			
    unsigned short SequenceNumber;			
    unsigned char Dummy1[2];				
    DAC960_BusAddress32_T BusAddress;			
    unsigned char Dummy2[4];				
  } __attribute__ ((packed)) Type3E;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char Dummy1[2];				
    unsigned char RebuildRateConstant;			
    unsigned char Dummy2[3];				
    DAC960_BusAddress32_T BusAddress;			
    unsigned char Dummy3[4];				
  } __attribute__ ((packed)) Type3R;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned short TransferLength;			
    unsigned int LogicalBlockAddress;			
    DAC960_BusAddress32_T BusAddress;			
    unsigned char LogicalDriveNumber;			
    unsigned char Dummy[3];				
  } __attribute__ ((packed)) Type4;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    struct {
      unsigned short TransferLength:11;			
      unsigned char LogicalDriveNumber:5;		
    } __attribute__ ((packed)) LD;
    unsigned int LogicalBlockAddress;			
    DAC960_BusAddress32_T BusAddress;			
    unsigned char ScatterGatherCount:6;			
    enum {
      DAC960_V1_ScatterGather_32BitAddress_32BitByteCount = 0x0,
      DAC960_V1_ScatterGather_32BitAddress_16BitByteCount = 0x1,
      DAC960_V1_ScatterGather_32BitByteCount_32BitAddress = 0x2,
      DAC960_V1_ScatterGather_16BitByteCount_32BitAddress = 0x3
    } __attribute__ ((packed)) ScatterGatherType:2;	
    unsigned char Dummy[3];				
  } __attribute__ ((packed)) Type5;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char CommandOpcode2;			
    unsigned char :8;					
    DAC960_BusAddress32_T CommandMailboxesBusAddress;	
    DAC960_BusAddress32_T StatusMailboxesBusAddress;	
    unsigned char Dummy[4];				
  } __attribute__ ((packed)) TypeX;
}
DAC960_V1_CommandMailbox_T;



typedef enum
{
  DAC960_V2_MemCopy =				0x01,
  DAC960_V2_SCSI_10_Passthru =			0x02,
  DAC960_V2_SCSI_255_Passthru =			0x03,
  DAC960_V2_SCSI_10 =				0x04,
  DAC960_V2_SCSI_256 =				0x05,
  DAC960_V2_IOCTL =				0x20
}
__attribute__ ((packed))
DAC960_V2_CommandOpcode_T;



typedef enum
{
  DAC960_V2_GetControllerInfo =			0x01,
  DAC960_V2_GetLogicalDeviceInfoValid =		0x03,
  DAC960_V2_GetPhysicalDeviceInfoValid =	0x05,
  DAC960_V2_GetHealthStatus =			0x11,
  DAC960_V2_GetEvent =				0x15,
  DAC960_V2_StartDiscovery =			0x81,
  DAC960_V2_SetDeviceState =			0x82,
  DAC960_V2_RebuildDeviceStart =		0x88,
  DAC960_V2_RebuildDeviceStop =			0x89,
  DAC960_V2_ConsistencyCheckStart =		0x8C,
  DAC960_V2_ConsistencyCheckStop =		0x8D,
  DAC960_V2_SetMemoryMailbox =			0x8E,
  DAC960_V2_PauseDevice =			0x92,
  DAC960_V2_TranslatePhysicalToLogicalDevice =	0xC5
}
__attribute__ ((packed))
DAC960_V2_IOCTL_Opcode_T;



typedef unsigned short DAC960_V2_CommandIdentifier_T;



#define DAC960_V2_NormalCompletion		0x00
#define DAC960_V2_AbormalCompletion		0x02
#define DAC960_V2_DeviceBusy			0x08
#define DAC960_V2_DeviceNonresponsive		0x0E
#define DAC960_V2_DeviceNonresponsive2		0x0F
#define DAC960_V2_DeviceRevervationConflict	0x18

typedef unsigned char DAC960_V2_CommandStatus_T;



typedef struct DAC960_V2_MemoryType
{
  enum {
    DAC960_V2_MemoryType_Reserved =		0x00,
    DAC960_V2_MemoryType_DRAM =			0x01,
    DAC960_V2_MemoryType_EDRAM =		0x02,
    DAC960_V2_MemoryType_EDO =			0x03,
    DAC960_V2_MemoryType_SDRAM =		0x04,
    DAC960_V2_MemoryType_Last =			0x1F
  } __attribute__ ((packed)) MemoryType:5;		
  bool :1;						
  bool MemoryParity:1;					
  bool MemoryECC:1;					
}
DAC960_V2_MemoryType_T;



typedef enum
{
  DAC960_V2_ProcessorType_i960CA =		0x01,
  DAC960_V2_ProcessorType_i960RD =		0x02,
  DAC960_V2_ProcessorType_i960RN =		0x03,
  DAC960_V2_ProcessorType_i960RP =		0x04,
  DAC960_V2_ProcessorType_NorthBay =		0x05,
  DAC960_V2_ProcessorType_StrongArm =		0x06,
  DAC960_V2_ProcessorType_i960RM =		0x07
}
__attribute__ ((packed))
DAC960_V2_ProcessorType_T;



typedef struct DAC960_V2_ControllerInfo
{
  unsigned char :8;					
  enum {
    DAC960_V2_SCSI_Bus =			0x00,
    DAC960_V2_Fibre_Bus =			0x01,
    DAC960_V2_PCI_Bus =				0x03
  } __attribute__ ((packed)) BusInterfaceType;		
  enum {
    DAC960_V2_DAC960E =				0x01,
    DAC960_V2_DAC960M =				0x08,
    DAC960_V2_DAC960PD =			0x10,
    DAC960_V2_DAC960PL =			0x11,
    DAC960_V2_DAC960PU =			0x12,
    DAC960_V2_DAC960PE =			0x13,
    DAC960_V2_DAC960PG =			0x14,
    DAC960_V2_DAC960PJ =			0x15,
    DAC960_V2_DAC960PTL0 =			0x16,
    DAC960_V2_DAC960PR =			0x17,
    DAC960_V2_DAC960PRL =			0x18,
    DAC960_V2_DAC960PT =			0x19,
    DAC960_V2_DAC1164P =			0x1A,
    DAC960_V2_DAC960PTL1 =			0x1B,
    DAC960_V2_EXR2000P =			0x1C,
    DAC960_V2_EXR3000P =			0x1D,
    DAC960_V2_AcceleRAID352 =			0x1E,
    DAC960_V2_AcceleRAID170 =			0x1F,
    DAC960_V2_AcceleRAID160 =			0x20,
    DAC960_V2_DAC960S =				0x60,
    DAC960_V2_DAC960SU =			0x61,
    DAC960_V2_DAC960SX =			0x62,
    DAC960_V2_DAC960SF =			0x63,
    DAC960_V2_DAC960SS =			0x64,
    DAC960_V2_DAC960FL =			0x65,
    DAC960_V2_DAC960LL =			0x66,
    DAC960_V2_DAC960FF =			0x67,
    DAC960_V2_DAC960HP =			0x68,
    DAC960_V2_RAIDBRICK =			0x69,
    DAC960_V2_METEOR_FL =			0x6A,
    DAC960_V2_METEOR_FF =			0x6B
  } __attribute__ ((packed)) ControllerType;		
  unsigned char :8;					
  unsigned short BusInterfaceSpeedMHz;			
  unsigned char BusWidthBits;				
  unsigned char FlashCodeTypeOrProductID;		
  unsigned char NumberOfHostPortsPresent;		
  unsigned char Reserved1[7];				
  unsigned char BusInterfaceName[16];			
  unsigned char ControllerName[16];			
  unsigned char Reserved2[16];				
  
  unsigned char FirmwareMajorVersion;			
  unsigned char FirmwareMinorVersion;			
  unsigned char FirmwareTurnNumber;			
  unsigned char FirmwareBuildNumber;			
  unsigned char FirmwareReleaseDay;			
  unsigned char FirmwareReleaseMonth;			
  unsigned char FirmwareReleaseYearHigh2Digits;		
  unsigned char FirmwareReleaseYearLow2Digits;		
  
  unsigned char HardwareRevision;			
  unsigned int :24;					
  unsigned char HardwareReleaseDay;			
  unsigned char HardwareReleaseMonth;			
  unsigned char HardwareReleaseYearHigh2Digits;		
  unsigned char HardwareReleaseYearLow2Digits;		
  
  unsigned char ManufacturingBatchNumber;		
  unsigned char :8;					
  unsigned char ManufacturingPlantNumber;		
  unsigned char :8;					
  unsigned char HardwareManufacturingDay;		
  unsigned char HardwareManufacturingMonth;		
  unsigned char HardwareManufacturingYearHigh2Digits;	
  unsigned char HardwareManufacturingYearLow2Digits;	
  unsigned char MaximumNumberOfPDDperXLD;		
  unsigned char MaximumNumberOfILDperXLD;		
  unsigned short NonvolatileMemorySizeKB;		
  unsigned char MaximumNumberOfXLD;			
  unsigned int :24;					
  
  unsigned char ControllerSerialNumber[16];		
  unsigned char Reserved3[16];				
  
  unsigned int :24;					
  unsigned char OEM_Code;				
  unsigned char VendorName[16];				
  
  bool BBU_Present:1;					
  bool ActiveActiveClusteringMode:1;			
  unsigned char :6;					
  unsigned char :8;					
  unsigned short :16;					
  
  bool PhysicalScanActive:1;				
  unsigned char :7;					
  unsigned char PhysicalDeviceChannelNumber;		
  unsigned char PhysicalDeviceTargetID;			
  unsigned char PhysicalDeviceLogicalUnit;		
  
  unsigned short MaximumDataTransferSizeInBlocks;	
  unsigned short MaximumScatterGatherEntries;		
  
  unsigned short LogicalDevicesPresent;			
  unsigned short LogicalDevicesCritical;		
  unsigned short LogicalDevicesOffline;			
  unsigned short PhysicalDevicesPresent;		
  unsigned short PhysicalDisksPresent;			
  unsigned short PhysicalDisksCritical;			
  unsigned short PhysicalDisksOffline;			
  unsigned short MaximumParallelCommands;		
  
  unsigned char NumberOfPhysicalChannelsPresent;	
  unsigned char NumberOfVirtualChannelsPresent;		
  unsigned char NumberOfPhysicalChannelsPossible;	
  unsigned char NumberOfVirtualChannelsPossible;	
  unsigned char MaximumTargetsPerChannel[16];		
  unsigned char Reserved4[12];				
  
  unsigned short MemorySizeMB;				
  unsigned short CacheSizeMB;				
  unsigned int ValidCacheSizeInBytes;			
  unsigned int DirtyCacheSizeInBytes;			
  unsigned short MemorySpeedMHz;			
  unsigned char MemoryDataWidthBits;			
  DAC960_V2_MemoryType_T MemoryType;			
  unsigned char CacheMemoryTypeName[16];		
  
  unsigned short ExecutionMemorySizeMB;			
  unsigned short ExecutionL2CacheSizeMB;		
  unsigned char Reserved5[8];				
  unsigned short ExecutionMemorySpeedMHz;		
  unsigned char ExecutionMemoryDataWidthBits;		
  DAC960_V2_MemoryType_T ExecutionMemoryType;		
  unsigned char ExecutionMemoryTypeName[16];		
  
  unsigned short FirstProcessorSpeedMHz;		
  DAC960_V2_ProcessorType_T FirstProcessorType;		
  unsigned char FirstProcessorCount;			
  unsigned char Reserved6[12];				
  unsigned char FirstProcessorName[16];			
  
  unsigned short SecondProcessorSpeedMHz;		
  DAC960_V2_ProcessorType_T SecondProcessorType;	
  unsigned char SecondProcessorCount;			
  unsigned char Reserved7[12];				
  unsigned char SecondProcessorName[16];		
  
  unsigned short CurrentProfilingDataPageNumber;	
  unsigned short ProgramsAwaitingProfilingData;		
  unsigned short CurrentCommandTimeTraceDataPageNumber;	
  unsigned short ProgramsAwaitingCommandTimeTraceData;	
  unsigned char Reserved8[8];				
  
  unsigned short PhysicalDeviceBusResets;		
  unsigned short PhysicalDeviceParityErrors;		
  unsigned short PhysicalDeviceSoftErrors;		
  unsigned short PhysicalDeviceCommandsFailed;		
  unsigned short PhysicalDeviceMiscellaneousErrors;	
  unsigned short PhysicalDeviceCommandTimeouts;		
  unsigned short PhysicalDeviceSelectionTimeouts;	
  unsigned short PhysicalDeviceRetriesDone;		
  unsigned short PhysicalDeviceAbortsDone;		
  unsigned short PhysicalDeviceHostCommandAbortsDone;	
  unsigned short PhysicalDevicePredictedFailuresDetected; 
  unsigned short PhysicalDeviceHostCommandsFailed;	
  unsigned short PhysicalDeviceHardErrors;		
  unsigned char Reserved9[6];				
  
  unsigned short LogicalDeviceSoftErrors;		
  unsigned short LogicalDeviceCommandsFailed;		
  unsigned short LogicalDeviceHostCommandAbortsDone;	
  unsigned short :16;					
  
  unsigned short ControllerMemoryErrors;		
  unsigned short ControllerHostCommandAbortsDone;	
  unsigned int :32;					
  
  unsigned short BackgroundInitializationsActive;	
  unsigned short LogicalDeviceInitializationsActive;	
  unsigned short PhysicalDeviceInitializationsActive;	
  unsigned short ConsistencyChecksActive;		
  unsigned short RebuildsActive;			
  unsigned short OnlineExpansionsActive;		
  unsigned short PatrolActivitiesActive;		
  unsigned short :16;					
  
  unsigned char FlashType;				
  unsigned char :8;					
  unsigned short FlashSizeMB;				
  unsigned int FlashLimit;				
  unsigned int FlashCount;				
  unsigned int :32;					
  unsigned char FlashTypeName[16];			
  
  unsigned char RebuildRate;				
  unsigned char BackgroundInitializationRate;		
  unsigned char ForegroundInitializationRate;		
  unsigned char ConsistencyCheckRate;			
  unsigned int :32;					
  unsigned int MaximumDP;				
  unsigned int FreeDP;					
  unsigned int MaximumIOP;				
  unsigned int FreeIOP;					
  unsigned short MaximumCombLengthInBlocks;		
  unsigned short NumberOfConfigurationGroups;		
  bool InstallationAbortStatus:1;			
  bool MaintenanceModeStatus:1;				
  unsigned int :24;					
  unsigned char Reserved10[32];				
  unsigned char Reserved11[512];			
}
DAC960_V2_ControllerInfo_T;



typedef enum
{
  DAC960_V2_LogicalDevice_Online =		0x01,
  DAC960_V2_LogicalDevice_Offline =		0x08,
  DAC960_V2_LogicalDevice_Critical =		0x09
}
__attribute__ ((packed))
DAC960_V2_LogicalDeviceState_T;



typedef struct DAC960_V2_LogicalDeviceInfo
{
  unsigned char :8;					
  unsigned char Channel;				
  unsigned char TargetID;				
  unsigned char LogicalUnit;				
  DAC960_V2_LogicalDeviceState_T LogicalDeviceState;	
  unsigned char RAIDLevel;				
  unsigned char StripeSize;				
  unsigned char CacheLineSize;				
  struct {
    enum {
      DAC960_V2_ReadCacheDisabled =		0x0,
      DAC960_V2_ReadCacheEnabled =		0x1,
      DAC960_V2_ReadAheadEnabled =		0x2,
      DAC960_V2_IntelligentReadAheadEnabled =	0x3,
      DAC960_V2_ReadCache_Last =		0x7
    } __attribute__ ((packed)) ReadCache:3;		
    enum {
      DAC960_V2_WriteCacheDisabled =		0x0,
      DAC960_V2_LogicalDeviceReadOnly =		0x1,
      DAC960_V2_WriteCacheEnabled =		0x2,
      DAC960_V2_IntelligentWriteCacheEnabled =	0x3,
      DAC960_V2_WriteCache_Last =		0x7
    } __attribute__ ((packed)) WriteCache:3;		
    bool :1;						
    bool LogicalDeviceInitialized:1;			
  } LogicalDeviceControl;				
  
  bool ConsistencyCheckInProgress:1;			
  bool RebuildInProgress:1;				
  bool BackgroundInitializationInProgress:1;		
  bool ForegroundInitializationInProgress:1;		
  bool DataMigrationInProgress:1;			
  bool PatrolOperationInProgress:1;			
  unsigned char :2;					
  unsigned char RAID5WriteUpdate;			
  unsigned char RAID5Algorithm;				
  unsigned short LogicalDeviceNumber;			
  
  bool BIOSDisabled:1;					
  bool CDROMBootEnabled:1;				
  bool DriveCoercionEnabled:1;				
  bool WriteSameDisabled:1;				
  bool HBA_ModeEnabled:1;				
  enum {
    DAC960_V2_Geometry_128_32 =			0x0,
    DAC960_V2_Geometry_255_63 =			0x1,
    DAC960_V2_Geometry_Reserved1 =		0x2,
    DAC960_V2_Geometry_Reserved2 =		0x3
  } __attribute__ ((packed)) DriveGeometry:2;		
  bool SuperReadAheadEnabled:1;				
  unsigned char :8;					
  
  unsigned short SoftErrors;				
  unsigned short CommandsFailed;			
  unsigned short HostCommandAbortsDone;			
  unsigned short DeferredWriteErrors;			
  unsigned int :32;					
  unsigned int :32;					
  
  unsigned short :16;					
  unsigned short DeviceBlockSizeInBytes;		
  unsigned int OriginalDeviceSize;			
  unsigned int ConfigurableDeviceSize;			
  unsigned int :32;					
  unsigned char LogicalDeviceName[32];			
  unsigned char SCSI_InquiryData[36];			
  unsigned char Reserved1[12];				
  DAC960_ByteCount64_T LastReadBlockNumber;		
  DAC960_ByteCount64_T LastWrittenBlockNumber;		
  DAC960_ByteCount64_T ConsistencyCheckBlockNumber;	
  DAC960_ByteCount64_T RebuildBlockNumber;		
  DAC960_ByteCount64_T BackgroundInitializationBlockNumber; 
  DAC960_ByteCount64_T ForegroundInitializationBlockNumber; 
  DAC960_ByteCount64_T DataMigrationBlockNumber;	
  DAC960_ByteCount64_T PatrolOperationBlockNumber;	
  unsigned char Reserved2[64];				
}
DAC960_V2_LogicalDeviceInfo_T;



typedef enum
{
    DAC960_V2_Device_Unconfigured =		0x00,
    DAC960_V2_Device_Online =			0x01,
    DAC960_V2_Device_Rebuild =			0x03,
    DAC960_V2_Device_Missing =			0x04,
    DAC960_V2_Device_Critical =			0x05,
    DAC960_V2_Device_Dead =			0x08,
    DAC960_V2_Device_SuspectedDead =		0x0C,
    DAC960_V2_Device_CommandedOffline =		0x10,
    DAC960_V2_Device_Standby =			0x21,
    DAC960_V2_Device_InvalidState =		0xFF
}
__attribute__ ((packed))
DAC960_V2_PhysicalDeviceState_T;



typedef struct DAC960_V2_PhysicalDeviceInfo
{
  unsigned char :8;					
  unsigned char Channel;				
  unsigned char TargetID;				
  unsigned char LogicalUnit;				
  
  bool PhysicalDeviceFaultTolerant:1;			
  bool PhysicalDeviceConnected:1;			
  bool PhysicalDeviceLocalToController:1;		
  unsigned char :5;					
  
  bool RemoteHostSystemDead:1;				
  bool RemoteControllerDead:1;				
  unsigned char :6;					
  DAC960_V2_PhysicalDeviceState_T PhysicalDeviceState;	
  unsigned char NegotiatedDataWidthBits;		
  unsigned short NegotiatedSynchronousMegaTransfers;	
  
  unsigned char NumberOfPortConnections;		
  unsigned char DriveAccessibilityBitmap;		
  unsigned int :32;					
  unsigned char NetworkAddress[16];			
  unsigned short MaximumTags;				
  
  bool ConsistencyCheckInProgress:1;			
  bool RebuildInProgress:1;				
  bool MakingDataConsistentInProgress:1;		
  bool PhysicalDeviceInitializationInProgress:1;	
  bool DataMigrationInProgress:1;			
  bool PatrolOperationInProgress:1;			
  unsigned char :2;					
  unsigned char LongOperationStatus;			
  unsigned char ParityErrors;				
  unsigned char SoftErrors;				
  unsigned char HardErrors;				
  unsigned char MiscellaneousErrors;			
  unsigned char CommandTimeouts;			
  unsigned char Retries;				
  unsigned char Aborts;					
  unsigned char PredictedFailuresDetected;		
  unsigned int :32;					
  unsigned short :16;					
  unsigned short DeviceBlockSizeInBytes;		
  unsigned int OriginalDeviceSize;			
  unsigned int ConfigurableDeviceSize;			
  unsigned int :32;					
  unsigned char PhysicalDeviceName[16];			
  unsigned char Reserved1[16];				
  unsigned char Reserved2[32];				
  unsigned char SCSI_InquiryData[36];			
  unsigned char Reserved3[20];				
  unsigned char Reserved4[8];				
  DAC960_ByteCount64_T LastReadBlockNumber;		
  DAC960_ByteCount64_T LastWrittenBlockNumber;		
  DAC960_ByteCount64_T ConsistencyCheckBlockNumber;	
  DAC960_ByteCount64_T RebuildBlockNumber;		
  DAC960_ByteCount64_T MakingDataConsistentBlockNumber;	
  DAC960_ByteCount64_T DeviceInitializationBlockNumber; 
  DAC960_ByteCount64_T DataMigrationBlockNumber;	
  DAC960_ByteCount64_T PatrolOperationBlockNumber;	
  unsigned char Reserved5[256];				
}
DAC960_V2_PhysicalDeviceInfo_T;



typedef struct DAC960_V2_HealthStatusBuffer
{
  unsigned int MicrosecondsFromControllerStartTime;	
  unsigned int MillisecondsFromControllerStartTime;	
  unsigned int SecondsFrom1January1970;			
  unsigned int :32;					
  unsigned int StatusChangeCounter;			
  unsigned int :32;					
  unsigned int DebugOutputMessageBufferIndex;		
  unsigned int CodedMessageBufferIndex;			
  unsigned int CurrentTimeTracePageNumber;		
  unsigned int CurrentProfilerPageNumber;		
  unsigned int NextEventSequenceNumber;			
  unsigned int :32;					
  unsigned char Reserved1[16];				
  unsigned char Reserved2[64];				
}
DAC960_V2_HealthStatusBuffer_T;



typedef struct DAC960_V2_Event
{
  unsigned int EventSequenceNumber;			
  unsigned int EventTime;				
  unsigned int EventCode;				
  unsigned char :8;					
  unsigned char Channel;				
  unsigned char TargetID;				
  unsigned char LogicalUnit;				
  unsigned int :32;					
  unsigned int EventSpecificParameter;			
  unsigned char RequestSenseData[40];			
}
DAC960_V2_Event_T;



typedef struct DAC960_V2_CommandControlBits
{
  bool ForceUnitAccess:1;				
  bool DisablePageOut:1;				
  bool :1;						
  bool AdditionalScatterGatherListMemory:1;		
  bool DataTransferControllerToHost:1;			
  bool :1;						
  bool NoAutoRequestSense:1;				
  bool DisconnectProhibited:1;				
}
DAC960_V2_CommandControlBits_T;



typedef struct DAC960_V2_CommandTimeout
{
  unsigned char TimeoutValue:6;				
  enum {
    DAC960_V2_TimeoutScale_Seconds =		0,
    DAC960_V2_TimeoutScale_Minutes =		1,
    DAC960_V2_TimeoutScale_Hours =		2,
    DAC960_V2_TimeoutScale_Reserved =		3
  } __attribute__ ((packed)) TimeoutScale:2;		
}
DAC960_V2_CommandTimeout_T;



typedef struct DAC960_V2_PhysicalDevice
{
  unsigned char LogicalUnit;				
  unsigned char TargetID;				
  unsigned char Channel:3;				
  unsigned char Controller:5;				
}
__attribute__ ((packed))
DAC960_V2_PhysicalDevice_T;



typedef struct DAC960_V2_LogicalDevice
{
  unsigned short LogicalDeviceNumber;			
  unsigned char :3;					
  unsigned char Controller:5;				
}
__attribute__ ((packed))
DAC960_V2_LogicalDevice_T;



typedef enum
{
  DAC960_V2_Physical_Device =			0x00,
  DAC960_V2_RAID_Device =			0x01,
  DAC960_V2_Physical_Channel =			0x02,
  DAC960_V2_RAID_Channel =			0x03,
  DAC960_V2_Physical_Controller =		0x04,
  DAC960_V2_RAID_Controller =			0x05,
  DAC960_V2_Configuration_Group =		0x10,
  DAC960_V2_Enclosure =				0x11
}
__attribute__ ((packed))
DAC960_V2_OperationDevice_T;



typedef struct DAC960_V2_PhysicalToLogicalDevice
{
  unsigned short LogicalDeviceNumber;			
  unsigned short :16;					
  unsigned char PreviousBootController;			
  unsigned char PreviousBootChannel;			
  unsigned char PreviousBootTargetID;			
  unsigned char PreviousBootLogicalUnit;		
}
DAC960_V2_PhysicalToLogicalDevice_T;




typedef struct DAC960_V2_ScatterGatherSegment
{
  DAC960_BusAddress64_T SegmentDataPointer;		
  DAC960_ByteCount64_T SegmentByteCount;		
}
DAC960_V2_ScatterGatherSegment_T;



typedef union DAC960_V2_DataTransferMemoryAddress
{
  DAC960_V2_ScatterGatherSegment_T ScatterGatherSegments[2]; 
  struct {
    unsigned short ScatterGatherList0Length;		
    unsigned short ScatterGatherList1Length;		
    unsigned short ScatterGatherList2Length;		
    unsigned short :16;					
    DAC960_BusAddress64_T ScatterGatherList0Address;	
    DAC960_BusAddress64_T ScatterGatherList1Address;	
    DAC960_BusAddress64_T ScatterGatherList2Address;	
  } ExtendedScatterGather;
}
DAC960_V2_DataTransferMemoryAddress_T;



typedef union DAC960_V2_CommandMailbox
{
  unsigned int Words[16];				
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    unsigned int :24;					
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned char Reserved[10];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } Common;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char CDBLength;				
    unsigned char SCSI_CDB[10];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } SCSI_10;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char CDBLength;				
    unsigned short :16;					
    DAC960_BusAddress64_T SCSI_CDB_BusAddress;		
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } SCSI_255;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    unsigned short :16;					
    unsigned char ControllerNumber;			
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned char Reserved[10];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } ControllerInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_LogicalDevice_T LogicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned char Reserved[10];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } LogicalDeviceInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned char Reserved[10];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } PhysicalDeviceInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    unsigned short EventSequenceNumberHigh16;		
    unsigned char ControllerNumber;			
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned short EventSequenceNumberLow16;		
    unsigned char Reserved[8];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } GetEvent;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_LogicalDevice_T LogicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    union {
      DAC960_V2_LogicalDeviceState_T LogicalDeviceState;
      DAC960_V2_PhysicalDeviceState_T PhysicalDeviceState;
    } DeviceState;					
    unsigned char Reserved[9];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } SetDeviceState;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_LogicalDevice_T LogicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    bool RestoreConsistency:1;				
    bool InitializedAreaOnly:1;				
    unsigned char :6;					
    unsigned char Reserved[9];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } ConsistencyCheck;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    unsigned char FirstCommandMailboxSizeKB;		
    unsigned char FirstStatusMailboxSizeKB;		
    unsigned char SecondCommandMailboxSizeKB;		
    unsigned char SecondStatusMailboxSizeKB;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    unsigned int :24;					
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    unsigned char HealthStatusBufferSizeKB;		
    unsigned char :8;					
    DAC960_BusAddress64_T HealthStatusBufferBusAddress; 
    DAC960_BusAddress64_T FirstCommandMailboxBusAddress; 
    DAC960_BusAddress64_T FirstStatusMailboxBusAddress; 
    DAC960_BusAddress64_T SecondCommandMailboxBusAddress; 
    DAC960_BusAddress64_T SecondStatusMailboxBusAddress; 
  } SetMemoryMailbox;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandOpcode_T CommandOpcode;		
    DAC960_V2_CommandControlBits_T CommandControlBits;	
    DAC960_ByteCount32_T DataTransferSize:24;		
    unsigned char DataTransferPageNumber;		
    DAC960_BusAddress64_T RequestSenseBusAddress;	
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		
    DAC960_V2_CommandTimeout_T CommandTimeout;		
    unsigned char RequestSenseSize;			
    unsigned char IOCTL_Opcode;				
    DAC960_V2_OperationDevice_T OperationDevice;	
    unsigned char Reserved[9];				
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			
  } DeviceOperation;
}
DAC960_V2_CommandMailbox_T;



#define DAC960_IOCTL_GET_CONTROLLER_COUNT	0xDAC001
#define DAC960_IOCTL_GET_CONTROLLER_INFO	0xDAC002
#define DAC960_IOCTL_V1_EXECUTE_COMMAND		0xDAC003
#define DAC960_IOCTL_V2_EXECUTE_COMMAND		0xDAC004
#define DAC960_IOCTL_V2_GET_HEALTH_STATUS	0xDAC005



typedef struct DAC960_ControllerInfo
{
  unsigned char ControllerNumber;
  unsigned char FirmwareType;
  unsigned char Channels;
  unsigned char Targets;
  unsigned char PCI_Bus;
  unsigned char PCI_Device;
  unsigned char PCI_Function;
  unsigned char IRQ_Channel;
  DAC960_PCI_Address_T PCI_Address;
  unsigned char ModelName[20];
  unsigned char FirmwareVersion[12];
}
DAC960_ControllerInfo_T;



typedef struct DAC960_V1_UserCommand
{
  unsigned char ControllerNumber;
  DAC960_V1_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  void __user *DataTransferBuffer;
  DAC960_V1_DCDB_T __user *DCDB;
}
DAC960_V1_UserCommand_T;



typedef struct DAC960_V1_KernelCommand
{
  unsigned char ControllerNumber;
  DAC960_V1_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  void *DataTransferBuffer;
  DAC960_V1_DCDB_T *DCDB;
  DAC960_V1_CommandStatus_T CommandStatus;
  void (*CompletionFunction)(struct DAC960_V1_KernelCommand *);
  void *CompletionData;
}
DAC960_V1_KernelCommand_T;



typedef struct DAC960_V2_UserCommand
{
  unsigned char ControllerNumber;
  DAC960_V2_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  int RequestSenseLength;
  void __user *DataTransferBuffer;
  void __user *RequestSenseBuffer;
}
DAC960_V2_UserCommand_T;



typedef struct DAC960_V2_KernelCommand
{
  unsigned char ControllerNumber;
  DAC960_V2_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  int RequestSenseLength;
  void *DataTransferBuffer;
  void *RequestSenseBuffer;
  DAC960_V2_CommandStatus_T CommandStatus;
  void (*CompletionFunction)(struct DAC960_V2_KernelCommand *);
  void *CompletionData;
}
DAC960_V2_KernelCommand_T;



typedef struct DAC960_V2_GetHealthStatus
{
  unsigned char ControllerNumber;
  DAC960_V2_HealthStatusBuffer_T __user *HealthStatusBuffer;
}
DAC960_V2_GetHealthStatus_T;



extern int DAC960_KernelIOCTL(unsigned int Request, void *Argument);



#ifdef DAC960_DriverVersion



#define DAC960_MaxDriverQueueDepth		511
#define DAC960_MaxControllerQueueDepth		512



#define DAC960_V1_ScatterGatherLimit		33
#define DAC960_V2_ScatterGatherLimit		128



#define DAC960_V1_CommandMailboxCount		256
#define DAC960_V1_StatusMailboxCount		1024
#define DAC960_V2_CommandMailboxCount		512
#define DAC960_V2_StatusMailboxCount		512



#define DAC960_MonitoringTimerInterval		(10 * HZ)



#define DAC960_SecondaryMonitoringInterval	(60 * HZ)



#define DAC960_HealthStatusMonitoringInterval	(1 * HZ)



#define DAC960_ProgressReportingInterval	(60 * HZ)



#define DAC960_MaxPartitions			8
#define DAC960_MaxPartitionsBits		3


#define DAC960_BlockSize			512
#define DAC960_BlockSizeBits			9



#define DAC960_V1_CommandAllocationGroupSize	11
#define DAC960_V2_CommandAllocationGroupSize	29



#define DAC960_LineBufferSize			100
#define DAC960_ProgressBufferSize		200
#define DAC960_UserMessageSize			200
#define DAC960_InitialStatusBufferSize		(8192-32)



typedef enum
{
  DAC960_V1_Controller =			1,
  DAC960_V2_Controller =			2
}
DAC960_FirmwareType_T;



typedef enum
{
  DAC960_BA_Controller =			1,	
  DAC960_LP_Controller =			2,	
  DAC960_LA_Controller =			3,	
  DAC960_PG_Controller =			4,	
  DAC960_PD_Controller =			5,	
  DAC960_P_Controller =				6,	
  DAC960_GEM_Controller =			7,	
}
DAC960_HardwareType_T;



typedef enum DAC960_MessageLevel
{
  DAC960_AnnounceLevel =			0,
  DAC960_InfoLevel =				1,
  DAC960_NoticeLevel =				2,
  DAC960_WarningLevel =				3,
  DAC960_ErrorLevel =				4,
  DAC960_ProgressLevel =			5,
  DAC960_CriticalLevel =			6,
  DAC960_UserCriticalLevel =			7
}
DAC960_MessageLevel_T;

static char
  *DAC960_MessageLevelMap[] =
    { KERN_NOTICE, KERN_NOTICE, KERN_NOTICE, KERN_WARNING,
      KERN_ERR, KERN_CRIT, KERN_CRIT, KERN_CRIT };



#define DAC960_Announce(Format, Arguments...) \
  DAC960_Message(DAC960_AnnounceLevel, Format, ##Arguments)

#define DAC960_Info(Format, Arguments...) \
  DAC960_Message(DAC960_InfoLevel, Format, ##Arguments)

#define DAC960_Notice(Format, Arguments...) \
  DAC960_Message(DAC960_NoticeLevel, Format, ##Arguments)

#define DAC960_Warning(Format, Arguments...) \
  DAC960_Message(DAC960_WarningLevel, Format, ##Arguments)

#define DAC960_Error(Format, Arguments...) \
  DAC960_Message(DAC960_ErrorLevel, Format, ##Arguments)

#define DAC960_Progress(Format, Arguments...) \
  DAC960_Message(DAC960_ProgressLevel, Format, ##Arguments)

#define DAC960_Critical(Format, Arguments...) \
  DAC960_Message(DAC960_CriticalLevel, Format, ##Arguments)

#define DAC960_UserCritical(Format, Arguments...) \
  DAC960_Message(DAC960_UserCriticalLevel, Format, ##Arguments)


struct DAC960_privdata {
	DAC960_HardwareType_T	HardwareType;
	DAC960_FirmwareType_T	FirmwareType;
	irq_handler_t		InterruptHandler;
	unsigned int		MemoryWindowSize;
};



typedef union DAC960_V1_StatusMailbox
{
  unsigned int Word;					
  struct {
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	
    unsigned char :7;					
    bool Valid:1;					
    DAC960_V1_CommandStatus_T CommandStatus;		
  } Fields;
}
DAC960_V1_StatusMailbox_T;



typedef union DAC960_V2_StatusMailbox
{
  unsigned int Words[2];				
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	
    DAC960_V2_CommandStatus_T CommandStatus;		
    unsigned char RequestSenseLength;			
    int DataTransferResidue;				
  } Fields;
}
DAC960_V2_StatusMailbox_T;



typedef enum
{
  DAC960_ReadCommand =				1,
  DAC960_WriteCommand =				2,
  DAC960_ReadRetryCommand =			3,
  DAC960_WriteRetryCommand =			4,
  DAC960_MonitoringCommand =			5,
  DAC960_ImmediateCommand =			6,
  DAC960_QueuedCommand =			7
}
DAC960_CommandType_T;



typedef struct DAC960_Command
{
  int CommandIdentifier;
  DAC960_CommandType_T CommandType;
  struct DAC960_Controller *Controller;
  struct DAC960_Command *Next;
  struct completion *Completion;
  unsigned int LogicalDriveNumber;
  unsigned int BlockNumber;
  unsigned int BlockCount;
  unsigned int SegmentCount;
  int	DmaDirection;
  struct scatterlist *cmd_sglist;
  struct request *Request;
  union {
    struct {
      DAC960_V1_CommandMailbox_T CommandMailbox;
      DAC960_V1_KernelCommand_T *KernelCommand;
      DAC960_V1_CommandStatus_T CommandStatus;
      DAC960_V1_ScatterGatherSegment_T *ScatterGatherList;
      dma_addr_t ScatterGatherListDMA;
      struct scatterlist ScatterList[DAC960_V1_ScatterGatherLimit];
      unsigned int EndMarker[0];
    } V1;
    struct {
      DAC960_V2_CommandMailbox_T CommandMailbox;
      DAC960_V2_KernelCommand_T *KernelCommand;
      DAC960_V2_CommandStatus_T CommandStatus;
      unsigned char RequestSenseLength;
      int DataTransferResidue;
      DAC960_V2_ScatterGatherSegment_T *ScatterGatherList;
      dma_addr_t ScatterGatherListDMA;
      DAC960_SCSI_RequestSense_T *RequestSense;
      dma_addr_t RequestSenseDMA;
      struct scatterlist ScatterList[DAC960_V2_ScatterGatherLimit];
      unsigned int EndMarker[0];
    } V2;
  } FW;
}
DAC960_Command_T;



typedef struct DAC960_Controller
{
  void __iomem *BaseAddress;
  void __iomem *MemoryMappedAddress;
  DAC960_FirmwareType_T FirmwareType;
  DAC960_HardwareType_T HardwareType;
  DAC960_IO_Address_T IO_Address;
  DAC960_PCI_Address_T PCI_Address;
  struct pci_dev *PCIDevice;
  unsigned char ControllerNumber;
  unsigned char ControllerName[4];
  unsigned char ModelName[20];
  unsigned char FullModelName[28];
  unsigned char FirmwareVersion[12];
  unsigned char Bus;
  unsigned char Device;
  unsigned char Function;
  unsigned char IRQ_Channel;
  unsigned char Channels;
  unsigned char Targets;
  unsigned char MemorySize;
  unsigned char LogicalDriveCount;
  unsigned short CommandAllocationGroupSize;
  unsigned short ControllerQueueDepth;
  unsigned short DriverQueueDepth;
  unsigned short MaxBlocksPerCommand;
  unsigned short ControllerScatterGatherLimit;
  unsigned short DriverScatterGatherLimit;
  u64		BounceBufferLimit;
  unsigned int CombinedStatusBufferLength;
  unsigned int InitialStatusLength;
  unsigned int CurrentStatusLength;
  unsigned int ProgressBufferLength;
  unsigned int UserStatusLength;
  struct dma_loaf DmaPages;
  unsigned long MonitoringTimerCount;
  unsigned long PrimaryMonitoringTime;
  unsigned long SecondaryMonitoringTime;
  unsigned long ShutdownMonitoringTimer;
  unsigned long LastProgressReportTime;
  unsigned long LastCurrentStatusTime;
  bool ControllerInitialized;
  bool MonitoringCommandDeferred;
  bool EphemeralProgressMessage;
  bool DriveSpinUpMessageDisplayed;
  bool MonitoringAlertMode;
  bool SuppressEnclosureMessages;
  struct timer_list MonitoringTimer;
  struct gendisk *disks[DAC960_MaxLogicalDrives];
  struct pci_pool *ScatterGatherPool;
  DAC960_Command_T *FreeCommands;
  unsigned char *CombinedStatusBuffer;
  unsigned char *CurrentStatusBuffer;
  struct request_queue *RequestQueue[DAC960_MaxLogicalDrives];
  int req_q_index;
  spinlock_t queue_lock;
  wait_queue_head_t CommandWaitQueue;
  wait_queue_head_t HealthStatusWaitQueue;
  DAC960_Command_T InitialCommand;
  DAC960_Command_T *Commands[DAC960_MaxDriverQueueDepth];
  struct proc_dir_entry *ControllerProcEntry;
  bool LogicalDriveInitiallyAccessible[DAC960_MaxLogicalDrives];
  void (*QueueCommand)(DAC960_Command_T *Command);
  bool (*ReadControllerConfiguration)(struct DAC960_Controller *);
  bool (*ReadDeviceConfiguration)(struct DAC960_Controller *);
  bool (*ReportDeviceConfiguration)(struct DAC960_Controller *);
  void (*QueueReadWriteCommand)(DAC960_Command_T *Command);
  union {
    struct {
      unsigned char GeometryTranslationHeads;
      unsigned char GeometryTranslationSectors;
      unsigned char PendingRebuildFlag;
      unsigned short StripeSize;
      unsigned short SegmentSize;
      unsigned short NewEventLogSequenceNumber;
      unsigned short OldEventLogSequenceNumber;
      unsigned short DeviceStateChannel;
      unsigned short DeviceStateTargetID;
      bool DualModeMemoryMailboxInterface;
      bool BackgroundInitializationStatusSupported;
      bool SAFTE_EnclosureManagementEnabled;
      bool NeedLogicalDriveInformation;
      bool NeedErrorTableInformation;
      bool NeedDeviceStateInformation;
      bool NeedDeviceInquiryInformation;
      bool NeedDeviceSerialNumberInformation;
      bool NeedRebuildProgress;
      bool NeedConsistencyCheckProgress;
      bool NeedBackgroundInitializationStatus;
      bool StartDeviceStateScan;
      bool RebuildProgressFirst;
      bool RebuildFlagPending;
      bool RebuildStatusPending;

      dma_addr_t	FirstCommandMailboxDMA;
      DAC960_V1_CommandMailbox_T *FirstCommandMailbox;
      DAC960_V1_CommandMailbox_T *LastCommandMailbox;
      DAC960_V1_CommandMailbox_T *NextCommandMailbox;
      DAC960_V1_CommandMailbox_T *PreviousCommandMailbox1;
      DAC960_V1_CommandMailbox_T *PreviousCommandMailbox2;

      dma_addr_t	FirstStatusMailboxDMA;
      DAC960_V1_StatusMailbox_T *FirstStatusMailbox;
      DAC960_V1_StatusMailbox_T *LastStatusMailbox;
      DAC960_V1_StatusMailbox_T *NextStatusMailbox;

      DAC960_V1_DCDB_T *MonitoringDCDB;
      dma_addr_t MonitoringDCDB_DMA;

      DAC960_V1_Enquiry_T Enquiry;
      DAC960_V1_Enquiry_T *NewEnquiry;
      dma_addr_t NewEnquiryDMA;

      DAC960_V1_ErrorTable_T ErrorTable;
      DAC960_V1_ErrorTable_T *NewErrorTable;
      dma_addr_t NewErrorTableDMA;

      DAC960_V1_EventLogEntry_T *EventLogEntry;
      dma_addr_t EventLogEntryDMA;

      DAC960_V1_RebuildProgress_T *RebuildProgress;
      dma_addr_t RebuildProgressDMA;
      DAC960_V1_CommandStatus_T LastRebuildStatus;
      DAC960_V1_CommandStatus_T PendingRebuildStatus;

      DAC960_V1_LogicalDriveInformationArray_T LogicalDriveInformation;
      DAC960_V1_LogicalDriveInformationArray_T *NewLogicalDriveInformation;
      dma_addr_t NewLogicalDriveInformationDMA;

      DAC960_V1_BackgroundInitializationStatus_T
        	*BackgroundInitializationStatus;
      dma_addr_t BackgroundInitializationStatusDMA;
      DAC960_V1_BackgroundInitializationStatus_T
        	LastBackgroundInitializationStatus;

      DAC960_V1_DeviceState_T
	DeviceState[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_V1_DeviceState_T *NewDeviceState;
      dma_addr_t	NewDeviceStateDMA;

      DAC960_SCSI_Inquiry_T
	InquiryStandardData[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_SCSI_Inquiry_T *NewInquiryStandardData;
      dma_addr_t NewInquiryStandardDataDMA;

      DAC960_SCSI_Inquiry_UnitSerialNumber_T
	InquiryUnitSerialNumber[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber;
      dma_addr_t NewInquiryUnitSerialNumberDMA;

      int DeviceResetCount[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      bool DirectCommandActive[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
    } V1;
    struct {
      unsigned int StatusChangeCounter;
      unsigned int NextEventSequenceNumber;
      unsigned int PhysicalDeviceIndex;
      bool NeedLogicalDeviceInformation;
      bool NeedPhysicalDeviceInformation;
      bool NeedDeviceSerialNumberInformation;
      bool StartLogicalDeviceInformationScan;
      bool StartPhysicalDeviceInformationScan;
      struct pci_pool *RequestSensePool;

      dma_addr_t	FirstCommandMailboxDMA;
      DAC960_V2_CommandMailbox_T *FirstCommandMailbox;
      DAC960_V2_CommandMailbox_T *LastCommandMailbox;
      DAC960_V2_CommandMailbox_T *NextCommandMailbox;
      DAC960_V2_CommandMailbox_T *PreviousCommandMailbox1;
      DAC960_V2_CommandMailbox_T *PreviousCommandMailbox2;

      dma_addr_t	FirstStatusMailboxDMA;
      DAC960_V2_StatusMailbox_T *FirstStatusMailbox;
      DAC960_V2_StatusMailbox_T *LastStatusMailbox;
      DAC960_V2_StatusMailbox_T *NextStatusMailbox;

      dma_addr_t	HealthStatusBufferDMA;
      DAC960_V2_HealthStatusBuffer_T *HealthStatusBuffer;

      DAC960_V2_ControllerInfo_T ControllerInformation;
      DAC960_V2_ControllerInfo_T *NewControllerInformation;
      dma_addr_t	NewControllerInformationDMA;

      DAC960_V2_LogicalDeviceInfo_T
	*LogicalDeviceInformation[DAC960_MaxLogicalDrives];
      DAC960_V2_LogicalDeviceInfo_T *NewLogicalDeviceInformation;
      dma_addr_t	 NewLogicalDeviceInformationDMA;

      DAC960_V2_PhysicalDeviceInfo_T
	*PhysicalDeviceInformation[DAC960_V2_MaxPhysicalDevices];
      DAC960_V2_PhysicalDeviceInfo_T *NewPhysicalDeviceInformation;
      dma_addr_t	NewPhysicalDeviceInformationDMA;

      DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber;
      dma_addr_t	NewInquiryUnitSerialNumberDMA;
      DAC960_SCSI_Inquiry_UnitSerialNumber_T
	*InquiryUnitSerialNumber[DAC960_V2_MaxPhysicalDevices];

      DAC960_V2_Event_T *Event;
      dma_addr_t EventDMA;

      DAC960_V2_PhysicalToLogicalDevice_T *PhysicalToLogicalDevice;
      dma_addr_t PhysicalToLogicalDeviceDMA;

      DAC960_V2_PhysicalDevice_T
	LogicalDriveToVirtualDevice[DAC960_MaxLogicalDrives];
      bool LogicalDriveFoundDuringScan[DAC960_MaxLogicalDrives];
    } V2;
  } FW;
  unsigned char ProgressBuffer[DAC960_ProgressBufferSize];
  unsigned char UserStatusBuffer[DAC960_UserMessageSize];
}
DAC960_Controller_T;



#define V1				FW.V1
#define V2				FW.V2
#define DAC960_QueueCommand(Command) \
  (Controller->QueueCommand)(Command)
#define DAC960_ReadControllerConfiguration(Controller) \
  (Controller->ReadControllerConfiguration)(Controller)
#define DAC960_ReadDeviceConfiguration(Controller) \
  (Controller->ReadDeviceConfiguration)(Controller)
#define DAC960_ReportDeviceConfiguration(Controller) \
  (Controller->ReportDeviceConfiguration)(Controller)
#define DAC960_QueueReadWriteCommand(Command) \
  (Controller->QueueReadWriteCommand)(Command)

/*
 * dma_addr_writeql is provided to write dma_addr_t types
 * to a 64-bit pci address space register.  The controller
 * will accept having the register written as two 32-bit
 * values.
 *
 * In HIGHMEM kernels, dma_addr_t is a 64-bit value.
 * without HIGHMEM,  dma_addr_t is a 32-bit value.
 *
 * The compiler should always fix up the assignment
 * to u.wq appropriately, depending upon the size of
 * dma_addr_t.
 */
static inline
void dma_addr_writeql(dma_addr_t addr, void __iomem *write_address)
{
	union {
		u64 wq;
		uint wl[2];
	} u;

	u.wq = addr;

	writel(u.wl[0], write_address);
	writel(u.wl[1], write_address + 4);
}


#define DAC960_GEM_RegisterWindowSize	0x600

typedef enum
{
  DAC960_GEM_InboundDoorBellRegisterReadSetOffset   =   0x214,
  DAC960_GEM_InboundDoorBellRegisterClearOffset     =   0x218,
  DAC960_GEM_OutboundDoorBellRegisterReadSetOffset  =   0x224,
  DAC960_GEM_OutboundDoorBellRegisterClearOffset    =   0x228,
  DAC960_GEM_InterruptStatusRegisterOffset          =   0x208,
  DAC960_GEM_InterruptMaskRegisterReadSetOffset     =   0x22C,
  DAC960_GEM_InterruptMaskRegisterClearOffset       =   0x230,
  DAC960_GEM_CommandMailboxBusAddressOffset         =   0x510,
  DAC960_GEM_CommandStatusOffset                    =   0x518,
  DAC960_GEM_ErrorStatusRegisterReadSetOffset       =   0x224,
  DAC960_GEM_ErrorStatusRegisterClearOffset         =   0x228,
}
DAC960_GEM_RegisterOffsets_T;


typedef union DAC960_GEM_InboundDoorBellRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    bool HardwareMailboxNewCommand:1;
    bool AcknowledgeHardwareMailboxStatus:1;
    bool GenerateInterrupt:1;
    bool ControllerReset:1;
    bool MemoryMailboxNewCommand:1;
    unsigned int :3;
  } Write;
  struct {
    unsigned int :24;
    bool HardwareMailboxFull:1;
    bool InitializationInProgress:1;
    unsigned int :6;
  } Read;
}
DAC960_GEM_InboundDoorBellRegister_T;

typedef union DAC960_GEM_OutboundDoorBellRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    bool AcknowledgeHardwareMailboxInterrupt:1;
    bool AcknowledgeMemoryMailboxInterrupt:1;
    unsigned int :6;
  } Write;
  struct {
    unsigned int :24;
    bool HardwareMailboxStatusAvailable:1;
    bool MemoryMailboxStatusAvailable:1;
    unsigned int :6;
  } Read;
}
DAC960_GEM_OutboundDoorBellRegister_T;

typedef union DAC960_GEM_InterruptMaskRegister
{
  unsigned int All;
  struct {
    unsigned int :16;
    unsigned int :8;
    unsigned int HardwareMailboxInterrupt:1;
    unsigned int MemoryMailboxInterrupt:1;
    unsigned int :6;
  } Bits;
}
DAC960_GEM_InterruptMaskRegister_T;


typedef union DAC960_GEM_ErrorStatusRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    unsigned int :5;
    bool ErrorStatusPending:1;
    unsigned int :2;
  } Bits;
}
DAC960_GEM_ErrorStatusRegister_T;


static inline
void DAC960_GEM_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
bool DAC960_GEM_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
  return InboundDoorBellRegister.Read.HardwareMailboxFull;
}

static inline
bool DAC960_GEM_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
  return InboundDoorBellRegister.Read.InitializationInProgress;
}

static inline
void DAC960_GEM_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
bool DAC960_GEM_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_OutboundDoorBellRegisterReadSetOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_GEM_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_OutboundDoorBellRegisterReadSetOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_GEM_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.HardwareMailboxInterrupt = true;
  InterruptMaskRegister.Bits.MemoryMailboxInterrupt = true;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InterruptMaskRegisterClearOffset);
}

static inline
void DAC960_GEM_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.HardwareMailboxInterrupt = true;
  InterruptMaskRegister.Bits.MemoryMailboxInterrupt = true;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InterruptMaskRegisterReadSetOffset);
}

static inline
bool DAC960_GEM_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InterruptMaskRegisterReadSetOffset);
  return !(InterruptMaskRegister.Bits.HardwareMailboxInterrupt ||
           InterruptMaskRegister.Bits.MemoryMailboxInterrupt);
}

static inline
void DAC960_GEM_WriteCommandMailbox(DAC960_V2_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V2_CommandMailbox_T
				     *CommandMailbox)
{
  memcpy(&MemoryCommandMailbox->Words[1], &CommandMailbox->Words[1],
	 sizeof(DAC960_V2_CommandMailbox_T) - sizeof(unsigned int));
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}

static inline
void DAC960_GEM_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    dma_addr_t CommandMailboxDMA)
{
	dma_addr_writeql(CommandMailboxDMA,
		ControllerBaseAddress +
		DAC960_GEM_CommandMailboxBusAddressOffset);
}

static inline DAC960_V2_CommandIdentifier_T
DAC960_GEM_ReadCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_GEM_CommandStatusOffset);
}

static inline DAC960_V2_CommandStatus_T
DAC960_GEM_ReadCommandStatus(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_GEM_CommandStatusOffset + 2);
}

static inline bool
DAC960_GEM_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_GEM_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readl(ControllerBaseAddress + DAC960_GEM_ErrorStatusRegisterReadSetOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_GEM_CommandMailboxBusAddressOffset + 0);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_GEM_CommandMailboxBusAddressOffset + 1);
  writel(0x03000000, ControllerBaseAddress +
         DAC960_GEM_ErrorStatusRegisterClearOffset);
  return true;
}


#define DAC960_BA_RegisterWindowSize		0x80

typedef enum
{
  DAC960_BA_InboundDoorBellRegisterOffset =	0x60,
  DAC960_BA_OutboundDoorBellRegisterOffset =	0x61,
  DAC960_BA_InterruptStatusRegisterOffset =	0x30,
  DAC960_BA_InterruptMaskRegisterOffset =	0x34,
  DAC960_BA_CommandMailboxBusAddressOffset =	0x50,
  DAC960_BA_CommandStatusOffset =		0x58,
  DAC960_BA_ErrorStatusRegisterOffset =		0x63
}
DAC960_BA_RegisterOffsets_T;



typedef union DAC960_BA_InboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool HardwareMailboxNewCommand:1;			
    bool AcknowledgeHardwareMailboxStatus:1;		
    bool GenerateInterrupt:1;				
    bool ControllerReset:1;				
    bool MemoryMailboxNewCommand:1;			
    unsigned char :3;					
  } Write;
  struct {
    bool HardwareMailboxEmpty:1;			
    bool InitializationNotInProgress:1;			
    unsigned char :6;					
  } Read;
}
DAC960_BA_InboundDoorBellRegister_T;



typedef union DAC960_BA_OutboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool AcknowledgeHardwareMailboxInterrupt:1;		
    bool AcknowledgeMemoryMailboxInterrupt:1;		
    unsigned char :6;					
  } Write;
  struct {
    bool HardwareMailboxStatusAvailable:1;		
    bool MemoryMailboxStatusAvailable:1;		
    unsigned char :6;					
  } Read;
}
DAC960_BA_OutboundDoorBellRegister_T;



typedef union DAC960_BA_InterruptMaskRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool DisableInterrupts:1;				
    bool DisableInterruptsI2O:1;			
    unsigned int :4;					
  } Bits;
}
DAC960_BA_InterruptMaskRegister_T;



typedef union DAC960_BA_ErrorStatusRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool ErrorStatusPending:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_BA_ErrorStatusRegister_T;



static inline
void DAC960_BA_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
}

static inline
bool DAC960_BA_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
  return !InboundDoorBellRegister.Read.HardwareMailboxEmpty;
}

static inline
bool DAC960_BA_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_InboundDoorBellRegisterOffset);
  return !InboundDoorBellRegister.Read.InitializationNotInProgress;
}

static inline
void DAC960_BA_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_BA_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_BA_OutboundDoorBellRegisterOffset);
}

static inline
bool DAC960_BA_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_BA_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_BA_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = false;
  InterruptMaskRegister.Bits.DisableInterruptsI2O = true;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_BA_InterruptMaskRegisterOffset);
}

static inline
void DAC960_BA_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = true;
  InterruptMaskRegister.Bits.DisableInterruptsI2O = true;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_BA_InterruptMaskRegisterOffset);
}

static inline
bool DAC960_BA_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_BA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_InterruptMaskRegisterOffset);
  return !InterruptMaskRegister.Bits.DisableInterrupts;
}

static inline
void DAC960_BA_WriteCommandMailbox(DAC960_V2_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V2_CommandMailbox_T
				     *CommandMailbox)
{
  memcpy(&MemoryCommandMailbox->Words[1], &CommandMailbox->Words[1],
	 sizeof(DAC960_V2_CommandMailbox_T) - sizeof(unsigned int));
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}


static inline
void DAC960_BA_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    dma_addr_t CommandMailboxDMA)
{
	dma_addr_writeql(CommandMailboxDMA,
		ControllerBaseAddress +
		DAC960_BA_CommandMailboxBusAddressOffset);
}

static inline DAC960_V2_CommandIdentifier_T
DAC960_BA_ReadCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_BA_CommandStatusOffset);
}

static inline DAC960_V2_CommandStatus_T
DAC960_BA_ReadCommandStatus(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_BA_CommandStatusOffset + 2);
}

static inline bool
DAC960_BA_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_BA_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readb(ControllerBaseAddress + DAC960_BA_ErrorStatusRegisterOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_BA_CommandMailboxBusAddressOffset + 0);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_BA_CommandMailboxBusAddressOffset + 1);
  writeb(0xFF, ControllerBaseAddress + DAC960_BA_ErrorStatusRegisterOffset);
  return true;
}



#define DAC960_LP_RegisterWindowSize		0x80

typedef enum
{
  DAC960_LP_InboundDoorBellRegisterOffset =	0x20,
  DAC960_LP_OutboundDoorBellRegisterOffset =	0x2C,
  DAC960_LP_InterruptStatusRegisterOffset =	0x30,
  DAC960_LP_InterruptMaskRegisterOffset =	0x34,
  DAC960_LP_CommandMailboxBusAddressOffset =	0x10,
  DAC960_LP_CommandStatusOffset =		0x18,
  DAC960_LP_ErrorStatusRegisterOffset =		0x2E
}
DAC960_LP_RegisterOffsets_T;



typedef union DAC960_LP_InboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool HardwareMailboxNewCommand:1;			
    bool AcknowledgeHardwareMailboxStatus:1;		
    bool GenerateInterrupt:1;				
    bool ControllerReset:1;				
    bool MemoryMailboxNewCommand:1;			
    unsigned char :3;					
  } Write;
  struct {
    bool HardwareMailboxFull:1;				
    bool InitializationInProgress:1;			
    unsigned char :6;					
  } Read;
}
DAC960_LP_InboundDoorBellRegister_T;



typedef union DAC960_LP_OutboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool AcknowledgeHardwareMailboxInterrupt:1;		
    bool AcknowledgeMemoryMailboxInterrupt:1;		
    unsigned char :6;					
  } Write;
  struct {
    bool HardwareMailboxStatusAvailable:1;		
    bool MemoryMailboxStatusAvailable:1;		
    unsigned char :6;					
  } Read;
}
DAC960_LP_OutboundDoorBellRegister_T;



typedef union DAC960_LP_InterruptMaskRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool DisableInterrupts:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_LP_InterruptMaskRegister_T;



typedef union DAC960_LP_ErrorStatusRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool ErrorStatusPending:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_LP_ErrorStatusRegister_T;



static inline
void DAC960_LP_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
}

static inline
bool DAC960_LP_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.HardwareMailboxFull;
}

static inline
bool DAC960_LP_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.InitializationInProgress;
}

static inline
void DAC960_LP_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_LP_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LP_OutboundDoorBellRegisterOffset);
}

static inline
bool DAC960_LP_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_LP_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_LP_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = false;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_LP_InterruptMaskRegisterOffset);
}

static inline
void DAC960_LP_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = true;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_LP_InterruptMaskRegisterOffset);
}

static inline
bool DAC960_LP_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_LP_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_InterruptMaskRegisterOffset);
  return !InterruptMaskRegister.Bits.DisableInterrupts;
}

static inline
void DAC960_LP_WriteCommandMailbox(DAC960_V2_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V2_CommandMailbox_T
				     *CommandMailbox)
{
  memcpy(&MemoryCommandMailbox->Words[1], &CommandMailbox->Words[1],
	 sizeof(DAC960_V2_CommandMailbox_T) - sizeof(unsigned int));
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}

static inline
void DAC960_LP_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    dma_addr_t CommandMailboxDMA)
{
	dma_addr_writeql(CommandMailboxDMA,
		ControllerBaseAddress +
		DAC960_LP_CommandMailboxBusAddressOffset);
}

static inline DAC960_V2_CommandIdentifier_T
DAC960_LP_ReadCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_LP_CommandStatusOffset);
}

static inline DAC960_V2_CommandStatus_T
DAC960_LP_ReadCommandStatus(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_LP_CommandStatusOffset + 2);
}

static inline bool
DAC960_LP_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_LP_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readb(ControllerBaseAddress + DAC960_LP_ErrorStatusRegisterOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_LP_CommandMailboxBusAddressOffset + 0);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_LP_CommandMailboxBusAddressOffset + 1);
  writeb(0xFF, ControllerBaseAddress + DAC960_LP_ErrorStatusRegisterOffset);
  return true;
}



#define DAC960_LA_RegisterWindowSize		0x80

typedef enum
{
  DAC960_LA_InboundDoorBellRegisterOffset =	0x60,
  DAC960_LA_OutboundDoorBellRegisterOffset =	0x61,
  DAC960_LA_InterruptMaskRegisterOffset =	0x34,
  DAC960_LA_CommandOpcodeRegisterOffset =	0x50,
  DAC960_LA_CommandIdentifierRegisterOffset =	0x51,
  DAC960_LA_MailboxRegister2Offset =		0x52,
  DAC960_LA_MailboxRegister3Offset =		0x53,
  DAC960_LA_MailboxRegister4Offset =		0x54,
  DAC960_LA_MailboxRegister5Offset =		0x55,
  DAC960_LA_MailboxRegister6Offset =		0x56,
  DAC960_LA_MailboxRegister7Offset =		0x57,
  DAC960_LA_MailboxRegister8Offset =		0x58,
  DAC960_LA_MailboxRegister9Offset =		0x59,
  DAC960_LA_MailboxRegister10Offset =		0x5A,
  DAC960_LA_MailboxRegister11Offset =		0x5B,
  DAC960_LA_MailboxRegister12Offset =		0x5C,
  DAC960_LA_StatusCommandIdentifierRegOffset =	0x5D,
  DAC960_LA_StatusRegisterOffset =		0x5E,
  DAC960_LA_ErrorStatusRegisterOffset =		0x63
}
DAC960_LA_RegisterOffsets_T;



typedef union DAC960_LA_InboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool HardwareMailboxNewCommand:1;			
    bool AcknowledgeHardwareMailboxStatus:1;		
    bool GenerateInterrupt:1;				
    bool ControllerReset:1;				
    bool MemoryMailboxNewCommand:1;			
    unsigned char :3;					
  } Write;
  struct {
    bool HardwareMailboxEmpty:1;			
    bool InitializationNotInProgress:1;		
    unsigned char :6;					
  } Read;
}
DAC960_LA_InboundDoorBellRegister_T;



typedef union DAC960_LA_OutboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool AcknowledgeHardwareMailboxInterrupt:1;		
    bool AcknowledgeMemoryMailboxInterrupt:1;		
    unsigned char :6;					
  } Write;
  struct {
    bool HardwareMailboxStatusAvailable:1;		
    bool MemoryMailboxStatusAvailable:1;		
    unsigned char :6;					
  } Read;
}
DAC960_LA_OutboundDoorBellRegister_T;



typedef union DAC960_LA_InterruptMaskRegister
{
  unsigned char All;
  struct {
    unsigned char :2;					
    bool DisableInterrupts:1;				
    unsigned char :5;					
  } Bits;
}
DAC960_LA_InterruptMaskRegister_T;



typedef union DAC960_LA_ErrorStatusRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool ErrorStatusPending:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_LA_ErrorStatusRegister_T;



static inline
void DAC960_LA_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
}

static inline
bool DAC960_LA_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
  return !InboundDoorBellRegister.Read.HardwareMailboxEmpty;
}

static inline
bool DAC960_LA_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_InboundDoorBellRegisterOffset);
  return !InboundDoorBellRegister.Read.InitializationNotInProgress;
}

static inline
void DAC960_LA_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_LA_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_LA_OutboundDoorBellRegisterOffset);
}

static inline
bool DAC960_LA_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_LA_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_LA_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = false;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_LA_InterruptMaskRegisterOffset);
}

static inline
void DAC960_LA_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0xFF;
  InterruptMaskRegister.Bits.DisableInterrupts = true;
  writeb(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_LA_InterruptMaskRegisterOffset);
}

static inline
bool DAC960_LA_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_LA_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_InterruptMaskRegisterOffset);
  return !InterruptMaskRegister.Bits.DisableInterrupts;
}

static inline
void DAC960_LA_WriteCommandMailbox(DAC960_V1_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V1_CommandMailbox_T
				     *CommandMailbox)
{
  MemoryCommandMailbox->Words[1] = CommandMailbox->Words[1];
  MemoryCommandMailbox->Words[2] = CommandMailbox->Words[2];
  MemoryCommandMailbox->Words[3] = CommandMailbox->Words[3];
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}

static inline
void DAC960_LA_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    DAC960_V1_CommandMailbox_T *CommandMailbox)
{
  writel(CommandMailbox->Words[0],
	 ControllerBaseAddress + DAC960_LA_CommandOpcodeRegisterOffset);
  writel(CommandMailbox->Words[1],
	 ControllerBaseAddress + DAC960_LA_MailboxRegister4Offset);
  writel(CommandMailbox->Words[2],
	 ControllerBaseAddress + DAC960_LA_MailboxRegister8Offset);
  writeb(CommandMailbox->Bytes[12],
	 ControllerBaseAddress + DAC960_LA_MailboxRegister12Offset);
}

static inline DAC960_V1_CommandIdentifier_T
DAC960_LA_ReadStatusCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readb(ControllerBaseAddress
	       + DAC960_LA_StatusCommandIdentifierRegOffset);
}

static inline DAC960_V1_CommandStatus_T
DAC960_LA_ReadStatusRegister(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_LA_StatusRegisterOffset);
}

static inline bool
DAC960_LA_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_LA_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readb(ControllerBaseAddress + DAC960_LA_ErrorStatusRegisterOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_LA_CommandOpcodeRegisterOffset);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_LA_CommandIdentifierRegisterOffset);
  writeb(0xFF, ControllerBaseAddress + DAC960_LA_ErrorStatusRegisterOffset);
  return true;
}


#define DAC960_PG_RegisterWindowSize		0x2000

typedef enum
{
  DAC960_PG_InboundDoorBellRegisterOffset =	0x0020,
  DAC960_PG_OutboundDoorBellRegisterOffset =	0x002C,
  DAC960_PG_InterruptMaskRegisterOffset =	0x0034,
  DAC960_PG_CommandOpcodeRegisterOffset =	0x1000,
  DAC960_PG_CommandIdentifierRegisterOffset =	0x1001,
  DAC960_PG_MailboxRegister2Offset =		0x1002,
  DAC960_PG_MailboxRegister3Offset =		0x1003,
  DAC960_PG_MailboxRegister4Offset =		0x1004,
  DAC960_PG_MailboxRegister5Offset =		0x1005,
  DAC960_PG_MailboxRegister6Offset =		0x1006,
  DAC960_PG_MailboxRegister7Offset =		0x1007,
  DAC960_PG_MailboxRegister8Offset =		0x1008,
  DAC960_PG_MailboxRegister9Offset =		0x1009,
  DAC960_PG_MailboxRegister10Offset =		0x100A,
  DAC960_PG_MailboxRegister11Offset =		0x100B,
  DAC960_PG_MailboxRegister12Offset =		0x100C,
  DAC960_PG_StatusCommandIdentifierRegOffset =	0x1018,
  DAC960_PG_StatusRegisterOffset =		0x101A,
  DAC960_PG_ErrorStatusRegisterOffset =		0x103F
}
DAC960_PG_RegisterOffsets_T;



typedef union DAC960_PG_InboundDoorBellRegister
{
  unsigned int All;
  struct {
    bool HardwareMailboxNewCommand:1;			
    bool AcknowledgeHardwareMailboxStatus:1;		
    bool GenerateInterrupt:1;				
    bool ControllerReset:1;				
    bool MemoryMailboxNewCommand:1;			
    unsigned int :27;					
  } Write;
  struct {
    bool HardwareMailboxFull:1;				
    bool InitializationInProgress:1;			
    unsigned int :30;					
  } Read;
}
DAC960_PG_InboundDoorBellRegister_T;



typedef union DAC960_PG_OutboundDoorBellRegister
{
  unsigned int All;
  struct {
    bool AcknowledgeHardwareMailboxInterrupt:1;		
    bool AcknowledgeMemoryMailboxInterrupt:1;		
    unsigned int :30;					
  } Write;
  struct {
    bool HardwareMailboxStatusAvailable:1;		
    bool MemoryMailboxStatusAvailable:1;		
    unsigned int :30;					
  } Read;
}
DAC960_PG_OutboundDoorBellRegister_T;



typedef union DAC960_PG_InterruptMaskRegister
{
  unsigned int All;
  struct {
    unsigned int MessageUnitInterruptMask1:2;		
    bool DisableInterrupts:1;				
    unsigned int MessageUnitInterruptMask2:5;		
    unsigned int Reserved0:24;				
  } Bits;
}
DAC960_PG_InterruptMaskRegister_T;



typedef union DAC960_PG_ErrorStatusRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool ErrorStatusPending:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_PG_ErrorStatusRegister_T;



static inline
void DAC960_PG_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
}

static inline
bool DAC960_PG_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.HardwareMailboxFull;
}

static inline
bool DAC960_PG_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress + DAC960_PG_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.InitializationInProgress;
}

static inline
void DAC960_PG_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_OutboundDoorBellRegisterOffset);
}

static inline
void DAC960_PG_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PG_OutboundDoorBellRegisterOffset);
}

static inline
bool DAC960_PG_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress + DAC960_PG_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_PG_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress + DAC960_PG_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_PG_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.MessageUnitInterruptMask1 = 0x3;
  InterruptMaskRegister.Bits.DisableInterrupts = false;
  InterruptMaskRegister.Bits.MessageUnitInterruptMask2 = 0x1F;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_PG_InterruptMaskRegisterOffset);
}

static inline
void DAC960_PG_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.MessageUnitInterruptMask1 = 0x3;
  InterruptMaskRegister.Bits.DisableInterrupts = true;
  InterruptMaskRegister.Bits.MessageUnitInterruptMask2 = 0x1F;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_PG_InterruptMaskRegisterOffset);
}

static inline
bool DAC960_PG_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_PG_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readl(ControllerBaseAddress + DAC960_PG_InterruptMaskRegisterOffset);
  return !InterruptMaskRegister.Bits.DisableInterrupts;
}

static inline
void DAC960_PG_WriteCommandMailbox(DAC960_V1_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V1_CommandMailbox_T
				     *CommandMailbox)
{
  MemoryCommandMailbox->Words[1] = CommandMailbox->Words[1];
  MemoryCommandMailbox->Words[2] = CommandMailbox->Words[2];
  MemoryCommandMailbox->Words[3] = CommandMailbox->Words[3];
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}

static inline
void DAC960_PG_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    DAC960_V1_CommandMailbox_T *CommandMailbox)
{
  writel(CommandMailbox->Words[0],
	 ControllerBaseAddress + DAC960_PG_CommandOpcodeRegisterOffset);
  writel(CommandMailbox->Words[1],
	 ControllerBaseAddress + DAC960_PG_MailboxRegister4Offset);
  writel(CommandMailbox->Words[2],
	 ControllerBaseAddress + DAC960_PG_MailboxRegister8Offset);
  writeb(CommandMailbox->Bytes[12],
	 ControllerBaseAddress + DAC960_PG_MailboxRegister12Offset);
}

static inline DAC960_V1_CommandIdentifier_T
DAC960_PG_ReadStatusCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readb(ControllerBaseAddress
	       + DAC960_PG_StatusCommandIdentifierRegOffset);
}

static inline DAC960_V1_CommandStatus_T
DAC960_PG_ReadStatusRegister(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_PG_StatusRegisterOffset);
}

static inline bool
DAC960_PG_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_PG_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readb(ControllerBaseAddress + DAC960_PG_ErrorStatusRegisterOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_PG_CommandOpcodeRegisterOffset);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_PG_CommandIdentifierRegisterOffset);
  writeb(0, ControllerBaseAddress + DAC960_PG_ErrorStatusRegisterOffset);
  return true;
}


#define DAC960_PD_RegisterWindowSize		0x80

typedef enum
{
  DAC960_PD_CommandOpcodeRegisterOffset =	0x00,
  DAC960_PD_CommandIdentifierRegisterOffset =	0x01,
  DAC960_PD_MailboxRegister2Offset =		0x02,
  DAC960_PD_MailboxRegister3Offset =		0x03,
  DAC960_PD_MailboxRegister4Offset =		0x04,
  DAC960_PD_MailboxRegister5Offset =		0x05,
  DAC960_PD_MailboxRegister6Offset =		0x06,
  DAC960_PD_MailboxRegister7Offset =		0x07,
  DAC960_PD_MailboxRegister8Offset =		0x08,
  DAC960_PD_MailboxRegister9Offset =		0x09,
  DAC960_PD_MailboxRegister10Offset =		0x0A,
  DAC960_PD_MailboxRegister11Offset =		0x0B,
  DAC960_PD_MailboxRegister12Offset =		0x0C,
  DAC960_PD_StatusCommandIdentifierRegOffset =	0x0D,
  DAC960_PD_StatusRegisterOffset =		0x0E,
  DAC960_PD_ErrorStatusRegisterOffset =		0x3F,
  DAC960_PD_InboundDoorBellRegisterOffset =	0x40,
  DAC960_PD_OutboundDoorBellRegisterOffset =	0x41,
  DAC960_PD_InterruptEnableRegisterOffset =	0x43
}
DAC960_PD_RegisterOffsets_T;



typedef union DAC960_PD_InboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool NewCommand:1;					
    bool AcknowledgeStatus:1;				
    bool GenerateInterrupt:1;				
    bool ControllerReset:1;				
    unsigned char :4;					
  } Write;
  struct {
    bool MailboxFull:1;					
    bool InitializationInProgress:1;			
    unsigned char :6;					
  } Read;
}
DAC960_PD_InboundDoorBellRegister_T;



typedef union DAC960_PD_OutboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool AcknowledgeInterrupt:1;			
    unsigned char :7;					
  } Write;
  struct {
    bool StatusAvailable:1;				
    unsigned char :7;					
  } Read;
}
DAC960_PD_OutboundDoorBellRegister_T;



typedef union DAC960_PD_InterruptEnableRegister
{
  unsigned char All;
  struct {
    bool EnableInterrupts:1;				
    unsigned char :7;					
  } Bits;
}
DAC960_PD_InterruptEnableRegister_T;



typedef union DAC960_PD_ErrorStatusRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					
    bool ErrorStatusPending:1;				
    unsigned int :5;					
  } Bits;
}
DAC960_PD_ErrorStatusRegister_T;



static inline
void DAC960_PD_NewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.NewCommand = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PD_AcknowledgeStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeStatus = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PD_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
}

static inline
void DAC960_PD_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writeb(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
}

static inline
bool DAC960_PD_MailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.MailboxFull;
}

static inline
bool DAC960_PD_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_PD_InboundDoorBellRegisterOffset);
  return InboundDoorBellRegister.Read.InitializationInProgress;
}

static inline
void DAC960_PD_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeInterrupt = true;
  writeb(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_PD_OutboundDoorBellRegisterOffset);
}

static inline
bool DAC960_PD_StatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readb(ControllerBaseAddress + DAC960_PD_OutboundDoorBellRegisterOffset);
  return OutboundDoorBellRegister.Read.StatusAvailable;
}

static inline
void DAC960_PD_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InterruptEnableRegister_T InterruptEnableRegister;
  InterruptEnableRegister.All = 0;
  InterruptEnableRegister.Bits.EnableInterrupts = true;
  writeb(InterruptEnableRegister.All,
	 ControllerBaseAddress + DAC960_PD_InterruptEnableRegisterOffset);
}

static inline
void DAC960_PD_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InterruptEnableRegister_T InterruptEnableRegister;
  InterruptEnableRegister.All = 0;
  InterruptEnableRegister.Bits.EnableInterrupts = false;
  writeb(InterruptEnableRegister.All,
	 ControllerBaseAddress + DAC960_PD_InterruptEnableRegisterOffset);
}

static inline
bool DAC960_PD_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_PD_InterruptEnableRegister_T InterruptEnableRegister;
  InterruptEnableRegister.All =
    readb(ControllerBaseAddress + DAC960_PD_InterruptEnableRegisterOffset);
  return InterruptEnableRegister.Bits.EnableInterrupts;
}

static inline
void DAC960_PD_WriteCommandMailbox(void __iomem *ControllerBaseAddress,
				   DAC960_V1_CommandMailbox_T *CommandMailbox)
{
  writel(CommandMailbox->Words[0],
	 ControllerBaseAddress + DAC960_PD_CommandOpcodeRegisterOffset);
  writel(CommandMailbox->Words[1],
	 ControllerBaseAddress + DAC960_PD_MailboxRegister4Offset);
  writel(CommandMailbox->Words[2],
	 ControllerBaseAddress + DAC960_PD_MailboxRegister8Offset);
  writeb(CommandMailbox->Bytes[12],
	 ControllerBaseAddress + DAC960_PD_MailboxRegister12Offset);
}

static inline DAC960_V1_CommandIdentifier_T
DAC960_PD_ReadStatusCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readb(ControllerBaseAddress
	       + DAC960_PD_StatusCommandIdentifierRegOffset);
}

static inline DAC960_V1_CommandStatus_T
DAC960_PD_ReadStatusRegister(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_PD_StatusRegisterOffset);
}

static inline bool
DAC960_PD_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_PD_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readb(ControllerBaseAddress + DAC960_PD_ErrorStatusRegisterOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_PD_CommandOpcodeRegisterOffset);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_PD_CommandIdentifierRegisterOffset);
  writeb(0, ControllerBaseAddress + DAC960_PD_ErrorStatusRegisterOffset);
  return true;
}

static inline void DAC960_P_To_PD_TranslateEnquiry(void *Enquiry)
{
  memcpy(Enquiry + 132, Enquiry + 36, 64);
  memset(Enquiry + 36, 0, 96);
}

static inline void DAC960_P_To_PD_TranslateDeviceState(void *DeviceState)
{
  memcpy(DeviceState + 2, DeviceState + 3, 1);
  memmove(DeviceState + 4, DeviceState + 5, 2);
  memmove(DeviceState + 6, DeviceState + 8, 4);
}

static inline
void DAC960_PD_To_P_TranslateReadWriteCommand(DAC960_V1_CommandMailbox_T
					      *CommandMailbox)
{
  int LogicalDriveNumber = CommandMailbox->Type5.LD.LogicalDriveNumber;
  CommandMailbox->Bytes[3] &= 0x7;
  CommandMailbox->Bytes[3] |= CommandMailbox->Bytes[7] << 6;
  CommandMailbox->Bytes[7] = LogicalDriveNumber;
}

static inline
void DAC960_P_To_PD_TranslateReadWriteCommand(DAC960_V1_CommandMailbox_T
					      *CommandMailbox)
{
  int LogicalDriveNumber = CommandMailbox->Bytes[7];
  CommandMailbox->Bytes[7] = CommandMailbox->Bytes[3] >> 6;
  CommandMailbox->Bytes[3] &= 0x7;
  CommandMailbox->Bytes[3] |= LogicalDriveNumber << 3;
}



static void DAC960_FinalizeController(DAC960_Controller_T *);
static void DAC960_V1_QueueReadWriteCommand(DAC960_Command_T *);
static void DAC960_V2_QueueReadWriteCommand(DAC960_Command_T *); 
static void DAC960_RequestFunction(struct request_queue *);
static irqreturn_t DAC960_BA_InterruptHandler(int, void *);
static irqreturn_t DAC960_LP_InterruptHandler(int, void *);
static irqreturn_t DAC960_LA_InterruptHandler(int, void *);
static irqreturn_t DAC960_PG_InterruptHandler(int, void *);
static irqreturn_t DAC960_PD_InterruptHandler(int, void *);
static irqreturn_t DAC960_P_InterruptHandler(int, void *);
static void DAC960_V1_QueueMonitoringCommand(DAC960_Command_T *);
static void DAC960_V2_QueueMonitoringCommand(DAC960_Command_T *);
static void DAC960_MonitoringTimerFunction(unsigned long);
static void DAC960_Message(DAC960_MessageLevel_T, unsigned char *,
			   DAC960_Controller_T *, ...);
static void DAC960_CreateProcEntries(DAC960_Controller_T *);
static void DAC960_DestroyProcEntries(DAC960_Controller_T *);

#endif 
