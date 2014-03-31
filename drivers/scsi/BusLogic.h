/*

  Linux Driver for BusLogic MultiMaster and FlashPoint SCSI Host Adapters

  Copyright 1995-1998 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that any modifications to this software be
  sent directly to him for evaluation and testing.

  Special thanks to Wayne Yen, Jin-Lon Hon, and Alex Win of BusLogic, whose
  advice has been invaluable, to David Gentzel, for writing the original Linux
  BusLogic driver, and to Paul Gortmaker, for being such a dedicated test site.

  Finally, special thanks to Mylex/BusLogic for making the FlashPoint SCCB
  Manager available as freely redistributable source code.

*/

#ifndef _BUSLOGIC_H
#define _BUSLOGIC_H


#ifndef PACKED
#define PACKED __attribute__((packed))
#endif


#define BusLogic_MaxHostAdapters		16



#define BusLogic_MaxTargetDevices		16



#define BusLogic_ScatterGatherLimit		128



#define BusLogic_MaxTaggedQueueDepth		64
#define BusLogic_MaxAutomaticTaggedQueueDepth	28
#define BusLogic_MinAutomaticTaggedQueueDepth	7
#define BusLogic_TaggedQueueDepthBB		3
#define BusLogic_UntaggedQueueDepth		3
#define BusLogic_UntaggedQueueDepthBB		2



#define BusLogic_DefaultBusSettleTime		2



#define BusLogic_MaxMailboxes			211



#define BusLogic_CCB_AllocationGroupSize	7



#define BusLogic_LineBufferSize			100
#define BusLogic_MessageBufferSize		9700



enum BusLogic_MessageLevel {
	BusLogic_AnnounceLevel = 0,
	BusLogic_InfoLevel = 1,
	BusLogic_NoticeLevel = 2,
	BusLogic_WarningLevel = 3,
	BusLogic_ErrorLevel = 4
};

static char *BusLogic_MessageLevelMap[] = { KERN_NOTICE, KERN_NOTICE, KERN_NOTICE, KERN_WARNING, KERN_ERR };



#define BusLogic_Announce(Format, Arguments...) \
  BusLogic_Message(BusLogic_AnnounceLevel, Format, ##Arguments)

#define BusLogic_Info(Format, Arguments...) \
  BusLogic_Message(BusLogic_InfoLevel, Format, ##Arguments)

#define BusLogic_Notice(Format, Arguments...) \
  BusLogic_Message(BusLogic_NoticeLevel, Format, ##Arguments)

#define BusLogic_Warning(Format, Arguments...) \
  BusLogic_Message(BusLogic_WarningLevel, Format, ##Arguments)

#define BusLogic_Error(Format, Arguments...) \
  BusLogic_Message(BusLogic_ErrorLevel, Format, ##Arguments)



enum BusLogic_HostAdapterType {
	BusLogic_MultiMaster = 1,
	BusLogic_FlashPoint = 2
} PACKED;

#define BusLogic_MultiMasterAddressCount	4
#define BusLogic_FlashPointAddressCount		256

static int BusLogic_HostAdapterAddressCount[3] = { 0, BusLogic_MultiMasterAddressCount, BusLogic_FlashPointAddressCount };



#ifdef CONFIG_SCSI_FLASHPOINT

#define BusLogic_MultiMasterHostAdapterP(HostAdapter) \
  (HostAdapter->HostAdapterType == BusLogic_MultiMaster)

#define BusLogic_FlashPointHostAdapterP(HostAdapter) \
  (HostAdapter->HostAdapterType == BusLogic_FlashPoint)

#else

#define BusLogic_MultiMasterHostAdapterP(HostAdapter) \
  (true)

#define BusLogic_FlashPointHostAdapterP(HostAdapter) \
  (false)

#endif



enum BusLogic_HostAdapterBusType {
	BusLogic_Unknown_Bus = 0,
	BusLogic_ISA_Bus = 1,
	BusLogic_EISA_Bus = 2,
	BusLogic_PCI_Bus = 3,
	BusLogic_VESA_Bus = 4,
	BusLogic_MCA_Bus = 5
} PACKED;

static char *BusLogic_HostAdapterBusNames[] = { "Unknown", "ISA", "EISA", "PCI", "VESA", "MCA" };

static enum BusLogic_HostAdapterBusType BusLogic_HostAdapterBusTypes[] = {
	BusLogic_VESA_Bus,	
	BusLogic_ISA_Bus,	
	BusLogic_MCA_Bus,	
	BusLogic_EISA_Bus,	
	BusLogic_Unknown_Bus,	
	BusLogic_PCI_Bus	
};


enum BusLogic_BIOS_DiskGeometryTranslation {
	BusLogic_BIOS_Disk_Not_Installed = 0,
	BusLogic_BIOS_Disk_Installed_64x32 = 1,
	BusLogic_BIOS_Disk_Installed_128x32 = 2,
	BusLogic_BIOS_Disk_Installed_255x63 = 3
} PACKED;



struct BusLogic_ByteCounter {
	unsigned int Units;
	unsigned int Billions;
};



struct BusLogic_ProbeInfo {
	enum BusLogic_HostAdapterType HostAdapterType;
	enum BusLogic_HostAdapterBusType HostAdapterBusType;
	unsigned long IO_Address;
	unsigned long PCI_Address;
	struct pci_dev *PCI_Device;
	unsigned char Bus;
	unsigned char Device;
	unsigned char IRQ_Channel;
};


struct BusLogic_ProbeOptions {
	bool NoProbe:1;		
	bool NoProbeISA:1;	
	bool NoProbePCI:1;	
	bool NoSortPCI:1;	
	bool MultiMasterFirst:1;
	bool FlashPointFirst:1;	
	bool LimitedProbeISA:1;	
	bool Probe330:1;	
	bool Probe334:1;	
	bool Probe230:1;	
	bool Probe234:1;	
	bool Probe130:1;	
	bool Probe134:1;	
};


struct BusLogic_GlobalOptions {
	bool TraceProbe:1;	
	bool TraceHardwareReset:1;	
	bool TraceConfiguration:1;	
	bool TraceErrors:1;	
};


struct BusLogic_LocalOptions {
	bool InhibitTargetInquiry:1;	
};


#define BusLogic_ControlRegisterOffset		0	
#define BusLogic_StatusRegisterOffset		0	
#define BusLogic_CommandParameterRegisterOffset	1	
#define BusLogic_DataInRegisterOffset		1	
#define BusLogic_InterruptRegisterOffset	2	
#define BusLogic_GeometryRegisterOffset		3	


union BusLogic_ControlRegister {
	unsigned char All;
	struct {
		unsigned char:4;	
		bool SCSIBusReset:1;	
		bool InterruptReset:1;	
		bool SoftReset:1;	
		bool HardReset:1;	
	} cr;
};


union BusLogic_StatusRegister {
	unsigned char All;
	struct {
		bool CommandInvalid:1;		
		bool Reserved:1;		
		bool DataInRegisterReady:1;	
		bool CommandParameterRegisterBusy:1;	
		bool HostAdapterReady:1;	
		bool InitializationRequired:1;	
		bool DiagnosticFailure:1;	
		bool DiagnosticActive:1;	
	} sr;
};


union BusLogic_InterruptRegister {
	unsigned char All;
	struct {
		bool IncomingMailboxLoaded:1;	
		bool OutgoingMailboxAvailable:1;
		bool CommandComplete:1;		
		bool ExternalBusReset:1;	
		unsigned char Reserved:3;	
		bool InterruptValid:1;		
	} ir;
};


union BusLogic_GeometryRegister {
	unsigned char All;
	struct {
		enum BusLogic_BIOS_DiskGeometryTranslation Drive0Geometry:2;	
		enum BusLogic_BIOS_DiskGeometryTranslation Drive1Geometry:2;	
		unsigned char:3;	
		bool ExtendedTranslationEnabled:1;	
	} gr;
};


enum BusLogic_OperationCode {
	BusLogic_TestCommandCompleteInterrupt = 0x00,
	BusLogic_InitializeMailbox = 0x01,
	BusLogic_ExecuteMailboxCommand = 0x02,
	BusLogic_ExecuteBIOSCommand = 0x03,
	BusLogic_InquireBoardID = 0x04,
	BusLogic_EnableOutgoingMailboxAvailableInt = 0x05,
	BusLogic_SetSCSISelectionTimeout = 0x06,
	BusLogic_SetPreemptTimeOnBus = 0x07,
	BusLogic_SetTimeOffBus = 0x08,
	BusLogic_SetBusTransferRate = 0x09,
	BusLogic_InquireInstalledDevicesID0to7 = 0x0A,
	BusLogic_InquireConfiguration = 0x0B,
	BusLogic_EnableTargetMode = 0x0C,
	BusLogic_InquireSetupInformation = 0x0D,
	BusLogic_WriteAdapterLocalRAM = 0x1A,
	BusLogic_ReadAdapterLocalRAM = 0x1B,
	BusLogic_WriteBusMasterChipFIFO = 0x1C,
	BusLogic_ReadBusMasterChipFIFO = 0x1D,
	BusLogic_EchoCommandData = 0x1F,
	BusLogic_HostAdapterDiagnostic = 0x20,
	BusLogic_SetAdapterOptions = 0x21,
	BusLogic_InquireInstalledDevicesID8to15 = 0x23,
	BusLogic_InquireTargetDevices = 0x24,
	BusLogic_DisableHostAdapterInterrupt = 0x25,
	BusLogic_InitializeExtendedMailbox = 0x81,
	BusLogic_ExecuteSCSICommand = 0x83,
	BusLogic_InquireFirmwareVersion3rdDigit = 0x84,
	BusLogic_InquireFirmwareVersionLetter = 0x85,
	BusLogic_InquirePCIHostAdapterInformation = 0x86,
	BusLogic_InquireHostAdapterModelNumber = 0x8B,
	BusLogic_InquireSynchronousPeriod = 0x8C,
	BusLogic_InquireExtendedSetupInformation = 0x8D,
	BusLogic_EnableStrictRoundRobinMode = 0x8F,
	BusLogic_StoreHostAdapterLocalRAM = 0x90,
	BusLogic_FetchHostAdapterLocalRAM = 0x91,
	BusLogic_StoreLocalDataInEEPROM = 0x92,
	BusLogic_UploadAutoSCSICode = 0x94,
	BusLogic_ModifyIOAddress = 0x95,
	BusLogic_SetCCBFormat = 0x96,
	BusLogic_WriteInquiryBuffer = 0x9A,
	BusLogic_ReadInquiryBuffer = 0x9B,
	BusLogic_FlashROMUploadDownload = 0xA7,
	BusLogic_ReadSCAMData = 0xA8,
	BusLogic_WriteSCAMData = 0xA9
};


struct BusLogic_BoardID {
	unsigned char BoardType;	
	unsigned char CustomFeatures;	
	unsigned char FirmwareVersion1stDigit;	
	unsigned char FirmwareVersion2ndDigit;	
};


struct BusLogic_Configuration {
	unsigned char:5;	
	bool DMA_Channel5:1;	
	bool DMA_Channel6:1;	
	bool DMA_Channel7:1;	
	bool IRQ_Channel9:1;	
	bool IRQ_Channel10:1;	
	bool IRQ_Channel11:1;	
	bool IRQ_Channel12:1;	
	unsigned char:1;	
	bool IRQ_Channel14:1;	
	bool IRQ_Channel15:1;	
	unsigned char:1;	
	unsigned char HostAdapterID:4;	
	unsigned char:4;	
};


struct BusLogic_SynchronousValue {
	unsigned char Offset:4;	
	unsigned char TransferPeriod:3;	
	bool Synchronous:1;	
};

struct BusLogic_SetupInformation {
	bool SynchronousInitiationEnabled:1;	
	bool ParityCheckingEnabled:1;		
	unsigned char:6;	
	unsigned char BusTransferRate;	
	unsigned char PreemptTimeOnBus;	
	unsigned char TimeOffBus;	
	unsigned char MailboxCount;	
	unsigned char MailboxAddress[3];	
	struct BusLogic_SynchronousValue SynchronousValuesID0to7[8];	
	unsigned char DisconnectPermittedID0to7;	
	unsigned char Signature;	
	unsigned char CharacterD;	
	unsigned char HostBusType;	
	unsigned char WideTransfersPermittedID0to7;	
	unsigned char WideTransfersActiveID0to7;	
	struct BusLogic_SynchronousValue SynchronousValuesID8to15[8];	
	unsigned char DisconnectPermittedID8to15;	
	unsigned char:8;	
	unsigned char WideTransfersPermittedID8to15;	
	unsigned char WideTransfersActiveID8to15;	
};


struct BusLogic_ExtendedMailboxRequest {
	unsigned char MailboxCount;	
	u32 BaseMailboxAddress;	
} PACKED;



enum BusLogic_ISACompatibleIOPort {
	BusLogic_IO_330 = 0,
	BusLogic_IO_334 = 1,
	BusLogic_IO_230 = 2,
	BusLogic_IO_234 = 3,
	BusLogic_IO_130 = 4,
	BusLogic_IO_134 = 5,
	BusLogic_IO_Disable = 6,
	BusLogic_IO_Disable2 = 7
} PACKED;

struct BusLogic_PCIHostAdapterInformation {
	enum BusLogic_ISACompatibleIOPort ISACompatibleIOPort;	
	unsigned char PCIAssignedIRQChannel;	
	bool LowByteTerminated:1;	
	bool HighByteTerminated:1;	
	unsigned char:2;	
	bool JP1:1;		
	bool JP2:1;		
	bool JP3:1;		
	bool GenericInfoValid:1;
	unsigned char:8;	
};


struct BusLogic_ExtendedSetupInformation {
	unsigned char BusType;	
	unsigned char BIOS_Address;	
	unsigned short ScatterGatherLimit;	
	unsigned char MailboxCount;	
	u32 BaseMailboxAddress;	
	struct {
		unsigned char:2;	
		bool FastOnEISA:1;	
		unsigned char:3;	
		bool LevelSensitiveInterrupt:1;	
		unsigned char:1;	
	} Misc;
	unsigned char FirmwareRevision[3];	
	bool HostWideSCSI:1;		
	bool HostDifferentialSCSI:1;	
	bool HostSupportsSCAM:1;	
	bool HostUltraSCSI:1;		
	bool HostSmartTermination:1;	
	unsigned char:3;	
} PACKED;


enum BusLogic_RoundRobinModeRequest {
	BusLogic_AggressiveRoundRobinMode = 0,
	BusLogic_StrictRoundRobinMode = 1
} PACKED;



#define BusLogic_BIOS_BaseOffset		0
#define BusLogic_AutoSCSI_BaseOffset		64

struct BusLogic_FetchHostAdapterLocalRAMRequest {
	unsigned char ByteOffset;	
	unsigned char ByteCount;	
};


struct BusLogic_AutoSCSIData {
	unsigned char InternalFactorySignature[2];	
	unsigned char InformationByteCount;	
	unsigned char HostAdapterType[6];	
	unsigned char:8;	
	bool FloppyEnabled:1;		
	bool FloppySecondary:1;		
	bool LevelSensitiveInterrupt:1;	
	unsigned char:2;	
	unsigned char SystemRAMAreaForBIOS:3;	
	unsigned char DMA_Channel:7;	
	bool DMA_AutoConfiguration:1;	
	unsigned char IRQ_Channel:7;	
	bool IRQ_AutoConfiguration:1;	
	unsigned char DMA_TransferRate;	
	unsigned char SCSI_ID;	
	bool LowByteTerminated:1;	
	bool ParityCheckingEnabled:1;	
	bool HighByteTerminated:1;	
	bool NoisyCablingEnvironment:1;	
	bool FastSynchronousNegotiation:1;	
	bool BusResetEnabled:1;		
	 bool:1;		
	bool ActiveNegationEnabled:1;	
	unsigned char BusOnDelay;	
	unsigned char BusOffDelay;	
	bool HostAdapterBIOSEnabled:1;		
	bool BIOSRedirectionOfINT19Enabled:1;	
	bool ExtendedTranslationEnabled:1;	
	bool MapRemovableAsFixedEnabled:1;	
	 bool:1;		
	bool BIOSSupportsMoreThan2DrivesEnabled:1;	
	bool BIOSInterruptModeEnabled:1;	
	bool FlopticalSupportEnabled:1;		
	unsigned short DeviceEnabled;	
	unsigned short WidePermitted;	
	unsigned short FastPermitted;	
	unsigned short SynchronousPermitted;	
	unsigned short DisconnectPermitted;	
	unsigned short SendStartUnitCommand;	
	unsigned short IgnoreInBIOSScan;	
	unsigned char PCIInterruptPin:2;	
	unsigned char HostAdapterIOPortAddress:2;	
	bool StrictRoundRobinModeEnabled:1;	
	bool VESABusSpeedGreaterThan33MHz:1;	
	bool VESABurstWriteEnabled:1;	
	bool VESABurstReadEnabled:1;	
	unsigned short UltraPermitted;	
	unsigned int:32;	
	unsigned char:8;	
	unsigned char AutoSCSIMaximumLUN;	
	 bool:1;		
	bool SCAM_Dominant:1;	
	bool SCAM_Enabled:1;	
	bool SCAM_Level2:1;	
	unsigned char:4;	
	bool INT13ExtensionEnabled:1;	
	 bool:1;		
	bool CDROMBootEnabled:1;	
	unsigned char:5;	
	unsigned char BootTargetID:4;	
	unsigned char BootChannel:4;	
	unsigned char ForceBusDeviceScanningOrder:1;	
	unsigned char:7;	
	unsigned short NonTaggedToAlternateLUNPermitted;	
	unsigned short RenegotiateSyncAfterCheckCondition;	
	unsigned char Reserved[10];	
	unsigned char ManufacturingDiagnostic[2];	
	unsigned short Checksum;	
} PACKED;


struct BusLogic_AutoSCSIByte45 {
	unsigned char ForceBusDeviceScanningOrder:1;	
	unsigned char:7;	
};


#define BusLogic_BIOS_DriveMapOffset		17

struct BusLogic_BIOSDriveMapByte {
	unsigned char TargetIDBit3:1;	
	unsigned char:2;	
	enum BusLogic_BIOS_DiskGeometryTranslation DiskGeometry:2;	
	unsigned char TargetID:3;	
};


enum BusLogic_SetCCBFormatRequest {
	BusLogic_LegacyLUNFormatCCB = 0,
	BusLogic_ExtendedLUNFormatCCB = 1
} PACKED;


enum BusLogic_ActionCode {
	BusLogic_OutgoingMailboxFree = 0x00,
	BusLogic_MailboxStartCommand = 0x01,
	BusLogic_MailboxAbortCommand = 0x02
} PACKED;



enum BusLogic_CompletionCode {
	BusLogic_IncomingMailboxFree = 0x00,
	BusLogic_CommandCompletedWithoutError = 0x01,
	BusLogic_CommandAbortedAtHostRequest = 0x02,
	BusLogic_AbortedCommandNotFound = 0x03,
	BusLogic_CommandCompletedWithError = 0x04,
	BusLogic_InvalidCCB = 0x05
} PACKED;


enum BusLogic_CCB_Opcode {
	BusLogic_InitiatorCCB = 0x00,
	BusLogic_TargetCCB = 0x01,
	BusLogic_InitiatorCCB_ScatterGather = 0x02,
	BusLogic_InitiatorCCB_ResidualDataLength = 0x03,
	BusLogic_InitiatorCCB_ScatterGatherResidual = 0x04,
	BusLogic_BusDeviceReset = 0x81
} PACKED;



enum BusLogic_DataDirection {
	BusLogic_UncheckedDataTransfer = 0,
	BusLogic_DataInLengthChecked = 1,
	BusLogic_DataOutLengthChecked = 2,
	BusLogic_NoDataTransfer = 3
};



enum BusLogic_HostAdapterStatus {
	BusLogic_CommandCompletedNormally = 0x00,
	BusLogic_LinkedCommandCompleted = 0x0A,
	BusLogic_LinkedCommandCompletedWithFlag = 0x0B,
	BusLogic_DataUnderRun = 0x0C,
	BusLogic_SCSISelectionTimeout = 0x11,
	BusLogic_DataOverRun = 0x12,
	BusLogic_UnexpectedBusFree = 0x13,
	BusLogic_InvalidBusPhaseRequested = 0x14,
	BusLogic_InvalidOutgoingMailboxActionCode = 0x15,
	BusLogic_InvalidCommandOperationCode = 0x16,
	BusLogic_LinkedCCBhasInvalidLUN = 0x17,
	BusLogic_InvalidCommandParameter = 0x1A,
	BusLogic_AutoRequestSenseFailed = 0x1B,
	BusLogic_TaggedQueuingMessageRejected = 0x1C,
	BusLogic_UnsupportedMessageReceived = 0x1D,
	BusLogic_HostAdapterHardwareFailed = 0x20,
	BusLogic_TargetFailedResponseToATN = 0x21,
	BusLogic_HostAdapterAssertedRST = 0x22,
	BusLogic_OtherDeviceAssertedRST = 0x23,
	BusLogic_TargetDeviceReconnectedImproperly = 0x24,
	BusLogic_HostAdapterAssertedBusDeviceReset = 0x25,
	BusLogic_AbortQueueGenerated = 0x26,
	BusLogic_HostAdapterSoftwareError = 0x27,
	BusLogic_HostAdapterHardwareTimeoutError = 0x30,
	BusLogic_SCSIParityErrorDetected = 0x34
} PACKED;



enum BusLogic_TargetDeviceStatus {
	BusLogic_OperationGood = 0x00,
	BusLogic_CheckCondition = 0x02,
	BusLogic_DeviceBusy = 0x08
} PACKED;


enum BusLogic_QueueTag {
	BusLogic_SimpleQueueTag = 0,
	BusLogic_HeadOfQueueTag = 1,
	BusLogic_OrderedQueueTag = 2,
	BusLogic_ReservedQT = 3
};


#define BusLogic_CDB_MaxLength			12

typedef unsigned char SCSI_CDB_T[BusLogic_CDB_MaxLength];



struct BusLogic_ScatterGatherSegment {
	u32 SegmentByteCount;	
	u32 SegmentDataPointer;	
};


enum BusLogic_CCB_Status {
	BusLogic_CCB_Free = 0,
	BusLogic_CCB_Active = 1,
	BusLogic_CCB_Completed = 2,
	BusLogic_CCB_Reset = 3
} PACKED;



struct BusLogic_CCB {
	enum BusLogic_CCB_Opcode Opcode;	
	unsigned char:3;	
	enum BusLogic_DataDirection DataDirection:2;	
	bool TagEnable:1;	
	enum BusLogic_QueueTag QueueTag:2;	
	unsigned char CDB_Length;	
	unsigned char SenseDataLength;	
	u32 DataLength;		
	u32 DataPointer;	
	unsigned char:8;	
	unsigned char:8;	
	enum BusLogic_HostAdapterStatus HostAdapterStatus;	
	enum BusLogic_TargetDeviceStatus TargetDeviceStatus;	
	unsigned char TargetID;	
	unsigned char LogicalUnit:5;	
	bool LegacyTagEnable:1;	
	enum BusLogic_QueueTag LegacyQueueTag:2;	
	SCSI_CDB_T CDB;		
	unsigned char:8;	
	unsigned char:8;	
	unsigned int:32;	
	u32 SenseDataPointer;	
	void (*CallbackFunction) (struct BusLogic_CCB *);	
	u32 BaseAddress;	
	enum BusLogic_CompletionCode CompletionCode;	
#ifdef CONFIG_SCSI_FLASHPOINT
	unsigned char:8;	
	unsigned short OS_Flags;	
	unsigned char Private[48];	
#endif
	dma_addr_t AllocationGroupHead;
	unsigned int AllocationGroupSize;
	u32 DMA_Handle;
	enum BusLogic_CCB_Status Status;
	unsigned long SerialNumber;
	struct scsi_cmnd *Command;
	struct BusLogic_HostAdapter *HostAdapter;
	struct BusLogic_CCB *Next;
	struct BusLogic_CCB *NextAll;
	struct BusLogic_ScatterGatherSegment
	 ScatterGatherList[BusLogic_ScatterGatherLimit];
};


struct BusLogic_OutgoingMailbox {
	u32 CCB;		
	unsigned int:24;	
	enum BusLogic_ActionCode ActionCode;	
};


struct BusLogic_IncomingMailbox {
	u32 CCB;		
	enum BusLogic_HostAdapterStatus HostAdapterStatus;	
	enum BusLogic_TargetDeviceStatus TargetDeviceStatus;	
	unsigned char:8;	
	enum BusLogic_CompletionCode CompletionCode;	
};



struct BusLogic_DriverOptions {
	unsigned short TaggedQueuingPermitted;
	unsigned short TaggedQueuingPermittedMask;
	unsigned short BusSettleTime;
	struct BusLogic_LocalOptions LocalOptions;
	unsigned char CommonQueueDepth;
	unsigned char QueueDepth[BusLogic_MaxTargetDevices];
};


struct BusLogic_TargetFlags {
	bool TargetExists:1;
	bool TaggedQueuingSupported:1;
	bool WideTransfersSupported:1;
	bool TaggedQueuingActive:1;
	bool WideTransfersActive:1;
	bool CommandSuccessfulFlag:1;
	bool TargetInfoReported:1;
};


#define BusLogic_SizeBuckets			10

typedef unsigned int BusLogic_CommandSizeBuckets_T[BusLogic_SizeBuckets];

struct BusLogic_TargetStatistics {
	unsigned int CommandsAttempted;
	unsigned int CommandsCompleted;
	unsigned int ReadCommands;
	unsigned int WriteCommands;
	struct BusLogic_ByteCounter TotalBytesRead;
	struct BusLogic_ByteCounter TotalBytesWritten;
	BusLogic_CommandSizeBuckets_T ReadCommandSizeBuckets;
	BusLogic_CommandSizeBuckets_T WriteCommandSizeBuckets;
	unsigned short CommandAbortsRequested;
	unsigned short CommandAbortsAttempted;
	unsigned short CommandAbortsCompleted;
	unsigned short BusDeviceResetsRequested;
	unsigned short BusDeviceResetsAttempted;
	unsigned short BusDeviceResetsCompleted;
	unsigned short HostAdapterResetsRequested;
	unsigned short HostAdapterResetsAttempted;
	unsigned short HostAdapterResetsCompleted;
};


#define FlashPoint_BadCardHandle		0xFFFFFFFF

typedef unsigned int FlashPoint_CardHandle_T;



struct FlashPoint_Info {
	u32 BaseAddress;	
	bool Present;		
	unsigned char IRQ_Channel;	
	unsigned char SCSI_ID;	
	unsigned char SCSI_LUN;	
	unsigned short FirmwareRevision;	
	unsigned short SynchronousPermitted;	
	unsigned short FastPermitted;	
	unsigned short UltraPermitted;	
	unsigned short DisconnectPermitted;	
	unsigned short WidePermitted;	
	bool ParityCheckingEnabled:1;	
	bool HostWideSCSI:1;		
	bool HostSoftReset:1;		
	bool ExtendedTranslationEnabled:1;	
	bool LowByteTerminated:1;	
	bool HighByteTerminated:1;	
	bool ReportDataUnderrun:1;	
	bool SCAM_Enabled:1;	
	bool SCAM_Level2:1;	
	unsigned char:7;	
	unsigned char Family;	
	unsigned char BusType;	
	unsigned char ModelNumber[3];	
	unsigned char RelativeCardNumber;	
	unsigned char Reserved[4];	
	unsigned int OS_Reserved;	
	unsigned char TranslationInfo[4];	
	unsigned int Reserved2[5];	
	unsigned int SecondaryRange;	
};


struct BusLogic_HostAdapter {
	struct Scsi_Host *SCSI_Host;
	struct pci_dev *PCI_Device;
	enum BusLogic_HostAdapterType HostAdapterType;
	enum BusLogic_HostAdapterBusType HostAdapterBusType;
	unsigned long IO_Address;
	unsigned long PCI_Address;
	unsigned short AddressCount;
	unsigned char HostNumber;
	unsigned char ModelName[9];
	unsigned char FirmwareVersion[6];
	unsigned char FullModelName[18];
	unsigned char Bus;
	unsigned char Device;
	unsigned char IRQ_Channel;
	unsigned char DMA_Channel;
	unsigned char SCSI_ID;
	bool IRQ_ChannelAcquired:1;
	bool DMA_ChannelAcquired:1;
	bool ExtendedTranslationEnabled:1;
	bool ParityCheckingEnabled:1;
	bool BusResetEnabled:1;
	bool LevelSensitiveInterrupt:1;
	bool HostWideSCSI:1;
	bool HostDifferentialSCSI:1;
	bool HostSupportsSCAM:1;
	bool HostUltraSCSI:1;
	bool ExtendedLUNSupport:1;
	bool TerminationInfoValid:1;
	bool LowByteTerminated:1;
	bool HighByteTerminated:1;
	bool BounceBuffersRequired:1;
	bool StrictRoundRobinModeSupport:1;
	bool SCAM_Enabled:1;
	bool SCAM_Level2:1;
	bool HostAdapterInitialized:1;
	bool HostAdapterExternalReset:1;
	bool HostAdapterInternalError:1;
	bool ProcessCompletedCCBsActive;
	volatile bool HostAdapterCommandCompleted;
	unsigned short HostAdapterScatterGatherLimit;
	unsigned short DriverScatterGatherLimit;
	unsigned short MaxTargetDevices;
	unsigned short MaxLogicalUnits;
	unsigned short MailboxCount;
	unsigned short InitialCCBs;
	unsigned short IncrementalCCBs;
	unsigned short AllocatedCCBs;
	unsigned short DriverQueueDepth;
	unsigned short HostAdapterQueueDepth;
	unsigned short UntaggedQueueDepth;
	unsigned short CommonQueueDepth;
	unsigned short BusSettleTime;
	unsigned short SynchronousPermitted;
	unsigned short FastPermitted;
	unsigned short UltraPermitted;
	unsigned short WidePermitted;
	unsigned short DisconnectPermitted;
	unsigned short TaggedQueuingPermitted;
	unsigned short ExternalHostAdapterResets;
	unsigned short HostAdapterInternalErrors;
	unsigned short TargetDeviceCount;
	unsigned short MessageBufferLength;
	u32 BIOS_Address;
	struct BusLogic_DriverOptions *DriverOptions;
	struct FlashPoint_Info FlashPointInfo;
	FlashPoint_CardHandle_T CardHandle;
	struct list_head host_list;
	struct BusLogic_CCB *All_CCBs;
	struct BusLogic_CCB *Free_CCBs;
	struct BusLogic_CCB *FirstCompletedCCB;
	struct BusLogic_CCB *LastCompletedCCB;
	struct BusLogic_CCB *BusDeviceResetPendingCCB[BusLogic_MaxTargetDevices];
	struct BusLogic_TargetFlags TargetFlags[BusLogic_MaxTargetDevices];
	unsigned char QueueDepth[BusLogic_MaxTargetDevices];
	unsigned char SynchronousPeriod[BusLogic_MaxTargetDevices];
	unsigned char SynchronousOffset[BusLogic_MaxTargetDevices];
	unsigned char ActiveCommands[BusLogic_MaxTargetDevices];
	unsigned int CommandsSinceReset[BusLogic_MaxTargetDevices];
	unsigned long LastSequencePoint[BusLogic_MaxTargetDevices];
	unsigned long LastResetAttempted[BusLogic_MaxTargetDevices];
	unsigned long LastResetCompleted[BusLogic_MaxTargetDevices];
	struct BusLogic_OutgoingMailbox *FirstOutgoingMailbox;
	struct BusLogic_OutgoingMailbox *LastOutgoingMailbox;
	struct BusLogic_OutgoingMailbox *NextOutgoingMailbox;
	struct BusLogic_IncomingMailbox *FirstIncomingMailbox;
	struct BusLogic_IncomingMailbox *LastIncomingMailbox;
	struct BusLogic_IncomingMailbox *NextIncomingMailbox;
	struct BusLogic_TargetStatistics TargetStatistics[BusLogic_MaxTargetDevices];
	unsigned char *MailboxSpace;
	dma_addr_t MailboxSpaceHandle;
	unsigned int MailboxSize;
	unsigned long CCB_Offset;
	char MessageBuffer[BusLogic_MessageBufferSize];
};


struct BIOS_DiskParameters {
	int Heads;
	int Sectors;
	int Cylinders;
};


struct SCSI_Inquiry {
	unsigned char PeripheralDeviceType:5;	
	unsigned char PeripheralQualifier:3;	
	unsigned char DeviceTypeModifier:7;	
	bool RMB:1;		
	unsigned char ANSI_ApprovedVersion:3;	
	unsigned char ECMA_Version:3;	
	unsigned char ISO_Version:2;	
	unsigned char ResponseDataFormat:4;	
	unsigned char:2;	
	bool TrmIOP:1;		
	bool AENC:1;		
	unsigned char AdditionalLength;	
	unsigned char:8;	
	unsigned char:8;	
	bool SftRe:1;		
	bool CmdQue:1;		
	 bool:1;		
	bool Linked:1;		
	bool Sync:1;		
	bool WBus16:1;		
	bool WBus32:1;		
	bool RelAdr:1;		
	unsigned char VendorIdentification[8];	
	unsigned char ProductIdentification[16];	
	unsigned char ProductRevisionLevel[4];	
};



static inline void BusLogic_SCSIBusReset(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_ControlRegister ControlRegister;
	ControlRegister.All = 0;
	ControlRegister.cr.SCSIBusReset = true;
	outb(ControlRegister.All, HostAdapter->IO_Address + BusLogic_ControlRegisterOffset);
}

static inline void BusLogic_InterruptReset(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_ControlRegister ControlRegister;
	ControlRegister.All = 0;
	ControlRegister.cr.InterruptReset = true;
	outb(ControlRegister.All, HostAdapter->IO_Address + BusLogic_ControlRegisterOffset);
}

static inline void BusLogic_SoftReset(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_ControlRegister ControlRegister;
	ControlRegister.All = 0;
	ControlRegister.cr.SoftReset = true;
	outb(ControlRegister.All, HostAdapter->IO_Address + BusLogic_ControlRegisterOffset);
}

static inline void BusLogic_HardReset(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_ControlRegister ControlRegister;
	ControlRegister.All = 0;
	ControlRegister.cr.HardReset = true;
	outb(ControlRegister.All, HostAdapter->IO_Address + BusLogic_ControlRegisterOffset);
}

static inline unsigned char BusLogic_ReadStatusRegister(struct BusLogic_HostAdapter *HostAdapter)
{
	return inb(HostAdapter->IO_Address + BusLogic_StatusRegisterOffset);
}

static inline void BusLogic_WriteCommandParameterRegister(struct BusLogic_HostAdapter
							  *HostAdapter, unsigned char Value)
{
	outb(Value, HostAdapter->IO_Address + BusLogic_CommandParameterRegisterOffset);
}

static inline unsigned char BusLogic_ReadDataInRegister(struct BusLogic_HostAdapter *HostAdapter)
{
	return inb(HostAdapter->IO_Address + BusLogic_DataInRegisterOffset);
}

static inline unsigned char BusLogic_ReadInterruptRegister(struct BusLogic_HostAdapter *HostAdapter)
{
	return inb(HostAdapter->IO_Address + BusLogic_InterruptRegisterOffset);
}

static inline unsigned char BusLogic_ReadGeometryRegister(struct BusLogic_HostAdapter *HostAdapter)
{
	return inb(HostAdapter->IO_Address + BusLogic_GeometryRegisterOffset);
}


static inline void BusLogic_StartMailboxCommand(struct BusLogic_HostAdapter *HostAdapter)
{
	BusLogic_WriteCommandParameterRegister(HostAdapter, BusLogic_ExecuteMailboxCommand);
}


static inline void BusLogic_Delay(int Seconds)
{
	mdelay(1000 * Seconds);
}


static inline u32 Virtual_to_Bus(void *VirtualAddress)
{
	return (u32) virt_to_bus(VirtualAddress);
}

static inline void *Bus_to_Virtual(u32 BusAddress)
{
	return (void *) bus_to_virt(BusAddress);
}


static inline u32 Virtual_to_32Bit_Virtual(void *VirtualAddress)
{
	return (u32) (unsigned long) VirtualAddress;
}


static inline void BusLogic_IncrementErrorCounter(unsigned short *ErrorCounter)
{
	if (*ErrorCounter < 65535)
		(*ErrorCounter)++;
}


static inline void BusLogic_IncrementByteCounter(struct BusLogic_ByteCounter
						 *ByteCounter, unsigned int Amount)
{
	ByteCounter->Units += Amount;
	if (ByteCounter->Units > 999999999) {
		ByteCounter->Units -= 1000000000;
		ByteCounter->Billions++;
	}
}


static inline void BusLogic_IncrementSizeBucket(BusLogic_CommandSizeBuckets_T CommandSizeBuckets, unsigned int Amount)
{
	int Index = 0;
	if (Amount < 8 * 1024) {
		if (Amount < 2 * 1024)
			Index = (Amount < 1 * 1024 ? 0 : 1);
		else
			Index = (Amount < 4 * 1024 ? 2 : 3);
	} else if (Amount < 128 * 1024) {
		if (Amount < 32 * 1024)
			Index = (Amount < 16 * 1024 ? 4 : 5);
		else
			Index = (Amount < 64 * 1024 ? 6 : 7);
	} else
		Index = (Amount < 256 * 1024 ? 8 : 9);
	CommandSizeBuckets[Index]++;
}


#define FlashPoint_FirmwareVersion		"5.02"


#define FlashPoint_NormalInterrupt		0x00
#define FlashPoint_InternalError		0xFE
#define FlashPoint_ExternalBusReset		0xFF


static const char *BusLogic_DriverInfo(struct Scsi_Host *);
static int BusLogic_QueueCommand(struct Scsi_Host *h, struct scsi_cmnd *);
static int BusLogic_BIOSDiskParameters(struct scsi_device *, struct block_device *, sector_t, int *);
static int BusLogic_ProcDirectoryInfo(struct Scsi_Host *, char *, char **, off_t, int, int);
static int BusLogic_SlaveConfigure(struct scsi_device *);
static void BusLogic_QueueCompletedCCB(struct BusLogic_CCB *);
static irqreturn_t BusLogic_InterruptHandler(int, void *);
static int BusLogic_ResetHostAdapter(struct BusLogic_HostAdapter *, bool HardReset);
static void BusLogic_Message(enum BusLogic_MessageLevel, char *, struct BusLogic_HostAdapter *, ...);
static int __init BusLogic_Setup(char *);

#endif				
