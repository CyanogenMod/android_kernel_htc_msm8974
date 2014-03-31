
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

#define BusLogic_DriverVersion		"2.1.16"
#define BusLogic_DriverDate		"18 July 2002"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <scsi/scsicam.h>

#include <asm/dma.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include "BusLogic.h"
#include "FlashPoint.c"

#ifndef FAILURE
#define FAILURE (-1)
#endif

static struct scsi_host_template Bus_Logic_template;


static int BusLogic_DriverOptionsCount;



static struct BusLogic_DriverOptions BusLogic_DriverOptions[BusLogic_MaxHostAdapters];



MODULE_LICENSE("GPL");
#ifdef MODULE
static char *BusLogic;
module_param(BusLogic, charp, 0);
#endif



static struct BusLogic_ProbeOptions BusLogic_ProbeOptions;



static struct BusLogic_GlobalOptions BusLogic_GlobalOptions;

static LIST_HEAD(BusLogic_host_list);


static int BusLogic_ProbeInfoCount;



static struct BusLogic_ProbeInfo *BusLogic_ProbeInfoList;



static char *BusLogic_CommandFailureReason;

/*
  BusLogic_AnnounceDriver announces the Driver Version and Date, Author's
  Name, Copyright Notice, and Electronic Mail Address.
*/

static void BusLogic_AnnounceDriver(struct BusLogic_HostAdapter *HostAdapter)
{
	BusLogic_Announce("***** BusLogic SCSI Driver Version " BusLogic_DriverVersion " of " BusLogic_DriverDate " *****\n", HostAdapter);
	BusLogic_Announce("Copyright 1995-1998 by Leonard N. Zubkoff " "<lnz@dandelion.com>\n", HostAdapter);
}



static const char *BusLogic_DriverInfo(struct Scsi_Host *Host)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Host->hostdata;
	return HostAdapter->FullModelName;
}


static void BusLogic_InitializeCCBs(struct BusLogic_HostAdapter *HostAdapter, void *BlockPointer, int BlockSize, dma_addr_t BlockPointerHandle)
{
	struct BusLogic_CCB *CCB = (struct BusLogic_CCB *) BlockPointer;
	unsigned int offset = 0;
	memset(BlockPointer, 0, BlockSize);
	CCB->AllocationGroupHead = BlockPointerHandle;
	CCB->AllocationGroupSize = BlockSize;
	while ((BlockSize -= sizeof(struct BusLogic_CCB)) >= 0) {
		CCB->Status = BusLogic_CCB_Free;
		CCB->HostAdapter = HostAdapter;
		CCB->DMA_Handle = (u32) BlockPointerHandle + offset;
		if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
			CCB->CallbackFunction = BusLogic_QueueCompletedCCB;
			CCB->BaseAddress = HostAdapter->FlashPointInfo.BaseAddress;
		}
		CCB->Next = HostAdapter->Free_CCBs;
		CCB->NextAll = HostAdapter->All_CCBs;
		HostAdapter->Free_CCBs = CCB;
		HostAdapter->All_CCBs = CCB;
		HostAdapter->AllocatedCCBs++;
		CCB++;
		offset += sizeof(struct BusLogic_CCB);
	}
}



static bool __init BusLogic_CreateInitialCCBs(struct BusLogic_HostAdapter *HostAdapter)
{
	int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(struct BusLogic_CCB);
	void *BlockPointer;
	dma_addr_t BlockPointerHandle;
	while (HostAdapter->AllocatedCCBs < HostAdapter->InitialCCBs) {
		BlockPointer = pci_alloc_consistent(HostAdapter->PCI_Device, BlockSize, &BlockPointerHandle);
		if (BlockPointer == NULL) {
			BusLogic_Error("UNABLE TO ALLOCATE CCB GROUP - DETACHING\n", HostAdapter);
			return false;
		}
		BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize, BlockPointerHandle);
	}
	return true;
}



static void BusLogic_DestroyCCBs(struct BusLogic_HostAdapter *HostAdapter)
{
	struct BusLogic_CCB *NextCCB = HostAdapter->All_CCBs, *CCB, *Last_CCB = NULL;
	HostAdapter->All_CCBs = NULL;
	HostAdapter->Free_CCBs = NULL;
	while ((CCB = NextCCB) != NULL) {
		NextCCB = CCB->NextAll;
		if (CCB->AllocationGroupHead) {
			if (Last_CCB)
				pci_free_consistent(HostAdapter->PCI_Device, Last_CCB->AllocationGroupSize, Last_CCB, Last_CCB->AllocationGroupHead);
			Last_CCB = CCB;
		}
	}
	if (Last_CCB)
		pci_free_consistent(HostAdapter->PCI_Device, Last_CCB->AllocationGroupSize, Last_CCB, Last_CCB->AllocationGroupHead);
}



static void BusLogic_CreateAdditionalCCBs(struct BusLogic_HostAdapter *HostAdapter, int AdditionalCCBs, bool SuccessMessageP)
{
	int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(struct BusLogic_CCB);
	int PreviouslyAllocated = HostAdapter->AllocatedCCBs;
	void *BlockPointer;
	dma_addr_t BlockPointerHandle;
	if (AdditionalCCBs <= 0)
		return;
	while (HostAdapter->AllocatedCCBs - PreviouslyAllocated < AdditionalCCBs) {
		BlockPointer = pci_alloc_consistent(HostAdapter->PCI_Device, BlockSize, &BlockPointerHandle);
		if (BlockPointer == NULL)
			break;
		BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize, BlockPointerHandle);
	}
	if (HostAdapter->AllocatedCCBs > PreviouslyAllocated) {
		if (SuccessMessageP)
			BusLogic_Notice("Allocated %d additional CCBs (total now %d)\n", HostAdapter, HostAdapter->AllocatedCCBs - PreviouslyAllocated, HostAdapter->AllocatedCCBs);
		return;
	}
	BusLogic_Notice("Failed to allocate additional CCBs\n", HostAdapter);
	if (HostAdapter->DriverQueueDepth > HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount) {
		HostAdapter->DriverQueueDepth = HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount;
		HostAdapter->SCSI_Host->can_queue = HostAdapter->DriverQueueDepth;
	}
}


static struct BusLogic_CCB *BusLogic_AllocateCCB(struct BusLogic_HostAdapter
						 *HostAdapter)
{
	static unsigned long SerialNumber = 0;
	struct BusLogic_CCB *CCB;
	CCB = HostAdapter->Free_CCBs;
	if (CCB != NULL) {
		CCB->SerialNumber = ++SerialNumber;
		HostAdapter->Free_CCBs = CCB->Next;
		CCB->Next = NULL;
		if (HostAdapter->Free_CCBs == NULL)
			BusLogic_CreateAdditionalCCBs(HostAdapter, HostAdapter->IncrementalCCBs, true);
		return CCB;
	}
	BusLogic_CreateAdditionalCCBs(HostAdapter, HostAdapter->IncrementalCCBs, true);
	CCB = HostAdapter->Free_CCBs;
	if (CCB == NULL)
		return NULL;
	CCB->SerialNumber = ++SerialNumber;
	HostAdapter->Free_CCBs = CCB->Next;
	CCB->Next = NULL;
	return CCB;
}



static void BusLogic_DeallocateCCB(struct BusLogic_CCB *CCB)
{
	struct BusLogic_HostAdapter *HostAdapter = CCB->HostAdapter;

	scsi_dma_unmap(CCB->Command);
	pci_unmap_single(HostAdapter->PCI_Device, CCB->SenseDataPointer,
			 CCB->SenseDataLength, PCI_DMA_FROMDEVICE);

	CCB->Command = NULL;
	CCB->Status = BusLogic_CCB_Free;
	CCB->Next = HostAdapter->Free_CCBs;
	HostAdapter->Free_CCBs = CCB;
}



static int BusLogic_Command(struct BusLogic_HostAdapter *HostAdapter, enum BusLogic_OperationCode OperationCode, void *ParameterData, int ParameterLength, void *ReplyData, int ReplyLength)
{
	unsigned char *ParameterPointer = (unsigned char *) ParameterData;
	unsigned char *ReplyPointer = (unsigned char *) ReplyData;
	union BusLogic_StatusRegister StatusRegister;
	union BusLogic_InterruptRegister InterruptRegister;
	unsigned long ProcessorFlags = 0;
	int ReplyBytes = 0, Result;
	long TimeoutCounter;
	if (ReplyLength > 0)
		memset(ReplyData, 0, ReplyLength);
	if (!HostAdapter->IRQ_ChannelAcquired)
		local_irq_save(ProcessorFlags);
	TimeoutCounter = 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.HostAdapterReady && !StatusRegister.sr.CommandParameterRegisterBusy)
			break;
		udelay(100);
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Host Adapter Ready";
		Result = -2;
		goto Done;
	}
	HostAdapter->HostAdapterCommandCompleted = false;
	BusLogic_WriteCommandParameterRegister(HostAdapter, OperationCode);
	TimeoutCounter = 10000;
	while (ParameterLength > 0 && --TimeoutCounter >= 0) {
		/*
		   Wait 100 microseconds to give the Host Adapter enough time to determine
		   whether the last value written to the Command/Parameter Register was
		   valid or not.  If the Command Complete bit is set in the Interrupt
		   Register, then the Command Invalid bit in the Status Register will be
		   reset if the Operation Code or Parameter was valid and the command
		   has completed, or set if the Operation Code or Parameter was invalid.
		   If the Data In Register Ready bit is set in the Status Register, then
		   the Operation Code was valid, and data is waiting to be read back
		   from the Host Adapter.  Otherwise, wait for the Command/Parameter
		   Register Busy bit in the Status Register to be reset.
		 */
		udelay(100);
		InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (InterruptRegister.ir.CommandComplete)
			break;
		if (HostAdapter->HostAdapterCommandCompleted)
			break;
		if (StatusRegister.sr.DataInRegisterReady)
			break;
		if (StatusRegister.sr.CommandParameterRegisterBusy)
			continue;
		BusLogic_WriteCommandParameterRegister(HostAdapter, *ParameterPointer++);
		ParameterLength--;
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Parameter Acceptance";
		Result = -2;
		goto Done;
	}
	if (OperationCode == BusLogic_ModifyIOAddress) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.CommandInvalid) {
			BusLogic_CommandFailureReason = "Modify I/O Address Invalid";
			Result = -1;
			goto Done;
		}
		if (BusLogic_GlobalOptions.TraceConfiguration)
			BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: " "(Modify I/O Address)\n", HostAdapter, OperationCode, StatusRegister.All);
		Result = 0;
		goto Done;
	}
	switch (OperationCode) {
	case BusLogic_InquireInstalledDevicesID0to7:
	case BusLogic_InquireInstalledDevicesID8to15:
	case BusLogic_InquireTargetDevices:
		
		TimeoutCounter = 60 * 10000;
		break;
	default:
		
		TimeoutCounter = 10000;
		break;
	}
	while (--TimeoutCounter >= 0) {
		InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (InterruptRegister.ir.CommandComplete)
			break;
		if (HostAdapter->HostAdapterCommandCompleted)
			break;
		if (StatusRegister.sr.DataInRegisterReady) {
			if (++ReplyBytes <= ReplyLength)
				*ReplyPointer++ = BusLogic_ReadDataInRegister(HostAdapter);
			else
				BusLogic_ReadDataInRegister(HostAdapter);
		}
		if (OperationCode == BusLogic_FetchHostAdapterLocalRAM && StatusRegister.sr.HostAdapterReady)
			break;
		udelay(100);
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Command Complete";
		Result = -2;
		goto Done;
	}
	BusLogic_InterruptReset(HostAdapter);
	if (BusLogic_GlobalOptions.TraceConfiguration) {
		int i;
		BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: %2d ==> %2d:", HostAdapter, OperationCode, StatusRegister.All, ReplyLength, ReplyBytes);
		if (ReplyLength > ReplyBytes)
			ReplyLength = ReplyBytes;
		for (i = 0; i < ReplyLength; i++)
			BusLogic_Notice(" %02X", HostAdapter, ((unsigned char *) ReplyData)[i]);
		BusLogic_Notice("\n", HostAdapter);
	}
	if (StatusRegister.sr.CommandInvalid) {
		udelay(1000);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.CommandInvalid ||
		    StatusRegister.sr.Reserved ||
		    StatusRegister.sr.DataInRegisterReady ||
		    StatusRegister.sr.CommandParameterRegisterBusy || !StatusRegister.sr.HostAdapterReady || !StatusRegister.sr.InitializationRequired || StatusRegister.sr.DiagnosticActive || StatusRegister.sr.DiagnosticFailure) {
			BusLogic_SoftReset(HostAdapter);
			udelay(1000);
		}
		BusLogic_CommandFailureReason = "Command Invalid";
		Result = -1;
		goto Done;
	}
	if (ParameterLength > 0) {
		BusLogic_CommandFailureReason = "Excess Parameters Supplied";
		Result = -1;
		goto Done;
	}
	BusLogic_CommandFailureReason = NULL;
	Result = ReplyBytes;
      Done:
	if (!HostAdapter->IRQ_ChannelAcquired)
		local_irq_restore(ProcessorFlags);
	return Result;
}



static void __init BusLogic_AppendProbeAddressISA(unsigned long IO_Address)
{
	struct BusLogic_ProbeInfo *ProbeInfo;
	if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters)
		return;
	ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
	ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
	ProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
	ProbeInfo->IO_Address = IO_Address;
	ProbeInfo->PCI_Device = NULL;
}



static void __init BusLogic_InitializeProbeInfoListISA(struct BusLogic_HostAdapter
						       *PrototypeHostAdapter)
{
	if (BusLogic_ProbeOptions.NoProbeISA)
		return;
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe330)
		BusLogic_AppendProbeAddressISA(0x330);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe334)
		BusLogic_AppendProbeAddressISA(0x334);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe230)
		BusLogic_AppendProbeAddressISA(0x230);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe234)
		BusLogic_AppendProbeAddressISA(0x234);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe130)
		BusLogic_AppendProbeAddressISA(0x130);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe134)
		BusLogic_AppendProbeAddressISA(0x134);
}


#ifdef CONFIG_PCI



static void __init BusLogic_SortProbeInfo(struct BusLogic_ProbeInfo *ProbeInfoList, int ProbeInfoCount)
{
	int LastInterchange = ProbeInfoCount - 1, Bound, j;
	while (LastInterchange > 0) {
		Bound = LastInterchange;
		LastInterchange = 0;
		for (j = 0; j < Bound; j++) {
			struct BusLogic_ProbeInfo *ProbeInfo1 = &ProbeInfoList[j];
			struct BusLogic_ProbeInfo *ProbeInfo2 = &ProbeInfoList[j + 1];
			if (ProbeInfo1->Bus > ProbeInfo2->Bus || (ProbeInfo1->Bus == ProbeInfo2->Bus && (ProbeInfo1->Device > ProbeInfo2->Device))) {
				struct BusLogic_ProbeInfo TempProbeInfo;
				memcpy(&TempProbeInfo, ProbeInfo1, sizeof(struct BusLogic_ProbeInfo));
				memcpy(ProbeInfo1, ProbeInfo2, sizeof(struct BusLogic_ProbeInfo));
				memcpy(ProbeInfo2, &TempProbeInfo, sizeof(struct BusLogic_ProbeInfo));
				LastInterchange = j;
			}
		}
	}
}



static int __init BusLogic_InitializeMultiMasterProbeInfo(struct BusLogic_HostAdapter
							  *PrototypeHostAdapter)
{
	struct BusLogic_ProbeInfo *PrimaryProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount];
	int NonPrimaryPCIMultiMasterIndex = BusLogic_ProbeInfoCount + 1;
	int NonPrimaryPCIMultiMasterCount = 0, PCIMultiMasterCount = 0;
	bool ForceBusDeviceScanningOrder = false;
	bool ForceBusDeviceScanningOrderChecked = false;
	bool StandardAddressSeen[6];
	struct pci_dev *PCI_Device = NULL;
	int i;
	if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters)
		return 0;
	BusLogic_ProbeInfoCount++;
	for (i = 0; i < 6; i++)
		StandardAddressSeen[i] = false;
	PrimaryProbeInfo->IO_Address = 0;
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER, PCI_Device)) != NULL) {
		struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
		struct BusLogic_PCIHostAdapterInformation PCIHostAdapterInformation;
		enum BusLogic_ISACompatibleIOPort ModifyIOAddressRequest;
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long BaseAddress0;
		unsigned long BaseAddress1;
		unsigned long IO_Address;
		unsigned long PCI_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32) ))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = BaseAddress0 = pci_resource_start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);

		if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for " "MultiMaster Host Adapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (pci_resource_flags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for " "MultiMaster Host Adapter\n", NULL, BaseAddress1);
			BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogic_Error("BusLogic: IRQ Channel %d invalid for " "MultiMaster Host Adapter\n", NULL, IRQ_Channel);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (BusLogic_GlobalOptions.TraceProbe) {
			BusLogic_Notice("BusLogic: PCI MultiMaster Host Adapter " "detected at\n", NULL);
			BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address " "0x%X PCI Address 0x%X\n", NULL, Bus, Device, IO_Address, PCI_Address);
		}
		HostAdapter->IO_Address = IO_Address;
		BusLogic_InterruptReset(HostAdapter);
		if (BusLogic_Command(HostAdapter, BusLogic_InquirePCIHostAdapterInformation, NULL, 0, &PCIHostAdapterInformation, sizeof(PCIHostAdapterInformation))
		    == sizeof(PCIHostAdapterInformation)) {
			if (PCIHostAdapterInformation.ISACompatibleIOPort < 6)
				StandardAddressSeen[PCIHostAdapterInformation.ISACompatibleIOPort] = true;
		} else
			PCIHostAdapterInformation.ISACompatibleIOPort = BusLogic_IO_Disable;
		ModifyIOAddressRequest = BusLogic_IO_Disable;
		BusLogic_Command(HostAdapter, BusLogic_ModifyIOAddress, &ModifyIOAddressRequest, sizeof(ModifyIOAddressRequest), NULL, 0);
		if (!ForceBusDeviceScanningOrderChecked) {
			struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
			struct BusLogic_AutoSCSIByte45 AutoSCSIByte45;
			struct BusLogic_BoardID BoardID;
			FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_AutoSCSI_BaseOffset + 45;
			FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(AutoSCSIByte45);
			BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &AutoSCSIByte45, sizeof(AutoSCSIByte45));
			BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0, &BoardID, sizeof(BoardID));
			if (BoardID.FirmwareVersion1stDigit == '5')
				ForceBusDeviceScanningOrder = AutoSCSIByte45.ForceBusDeviceScanningOrder;
			ForceBusDeviceScanningOrderChecked = true;
		}
		if (PCIHostAdapterInformation.ISACompatibleIOPort == BusLogic_IO_330) {
			PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			PrimaryProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			PrimaryProbeInfo->IO_Address = IO_Address;
			PrimaryProbeInfo->PCI_Address = PCI_Address;
			PrimaryProbeInfo->Bus = Bus;
			PrimaryProbeInfo->Device = Device;
			PrimaryProbeInfo->IRQ_Channel = IRQ_Channel;
			PrimaryProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			PCIMultiMasterCount++;
		} else if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
			ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			ProbeInfo->IO_Address = IO_Address;
			ProbeInfo->PCI_Address = PCI_Address;
			ProbeInfo->Bus = Bus;
			ProbeInfo->Device = Device;
			ProbeInfo->IRQ_Channel = IRQ_Channel;
			ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			NonPrimaryPCIMultiMasterCount++;
			PCIMultiMasterCount++;
		} else
			BusLogic_Warning("BusLogic: Too many Host Adapters " "detected\n", NULL);
	}
	if (ForceBusDeviceScanningOrder)
		BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[NonPrimaryPCIMultiMasterIndex], NonPrimaryPCIMultiMasterCount);
	if (!BusLogic_ProbeOptions.NoProbeISA)
		if (PrimaryProbeInfo->IO_Address == 0 &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe330)) {
			PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			PrimaryProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
			PrimaryProbeInfo->IO_Address = 0x330;
		}
	if (!BusLogic_ProbeOptions.NoProbeISA) {
		if (!StandardAddressSeen[1] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe334))
			BusLogic_AppendProbeAddressISA(0x334);
		if (!StandardAddressSeen[2] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe230))
			BusLogic_AppendProbeAddressISA(0x230);
		if (!StandardAddressSeen[3] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe234))
			BusLogic_AppendProbeAddressISA(0x234);
		if (!StandardAddressSeen[4] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe130))
			BusLogic_AppendProbeAddressISA(0x130);
		if (!StandardAddressSeen[5] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe134))
			BusLogic_AppendProbeAddressISA(0x134);
	}
	PCI_Device = NULL;
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long IO_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32)))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = pci_resource_start(PCI_Device, 0);

		if (IO_Address == 0 || IRQ_Channel == 0)
			continue;
		for (i = 0; i < BusLogic_ProbeInfoCount; i++) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[i];
			if (ProbeInfo->IO_Address == IO_Address && ProbeInfo->HostAdapterType == BusLogic_MultiMaster) {
				ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
				ProbeInfo->PCI_Address = 0;
				ProbeInfo->Bus = Bus;
				ProbeInfo->Device = Device;
				ProbeInfo->IRQ_Channel = IRQ_Channel;
				ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
				break;
			}
		}
	}
	return PCIMultiMasterCount;
}



static int __init BusLogic_InitializeFlashPointProbeInfo(struct BusLogic_HostAdapter
							 *PrototypeHostAdapter)
{
	int FlashPointIndex = BusLogic_ProbeInfoCount, FlashPointCount = 0;
	struct pci_dev *PCI_Device = NULL;
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long BaseAddress0;
		unsigned long BaseAddress1;
		unsigned long IO_Address;
		unsigned long PCI_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32)))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = BaseAddress0 = pci_resource_start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);
#ifdef CONFIG_SCSI_FLASHPOINT
		if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for " "FlashPoint Host Adapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (pci_resource_flags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for " "FlashPoint Host Adapter\n", NULL, BaseAddress1);
			BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogic_Error("BusLogic: IRQ Channel %d invalid for " "FlashPoint Host Adapter\n", NULL, IRQ_Channel);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (BusLogic_GlobalOptions.TraceProbe) {
			BusLogic_Notice("BusLogic: FlashPoint Host Adapter " "detected at\n", NULL);
			BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address " "0x%X PCI Address 0x%X\n", NULL, Bus, Device, IO_Address, PCI_Address);
		}
		if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
			ProbeInfo->HostAdapterType = BusLogic_FlashPoint;
			ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			ProbeInfo->IO_Address = IO_Address;
			ProbeInfo->PCI_Address = PCI_Address;
			ProbeInfo->Bus = Bus;
			ProbeInfo->Device = Device;
			ProbeInfo->IRQ_Channel = IRQ_Channel;
			ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			FlashPointCount++;
		} else
			BusLogic_Warning("BusLogic: Too many Host Adapters " "detected\n", NULL);
#else
		BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at " "PCI Bus %d Device %d\n", NULL, Bus, Device);
		BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, irq %d, " "but FlashPoint\n", NULL, IO_Address, PCI_Address, IRQ_Channel);
		BusLogic_Error("BusLogic: support was omitted in this kernel " "configuration.\n", NULL);
#endif
	}
	BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[FlashPointIndex], FlashPointCount);
	return FlashPointCount;
}



static void __init BusLogic_InitializeProbeInfoList(struct BusLogic_HostAdapter
						    *PrototypeHostAdapter)
{
	if (!BusLogic_ProbeOptions.NoProbePCI) {
		if (BusLogic_ProbeOptions.MultiMasterFirst) {
			BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
			BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
		} else if (BusLogic_ProbeOptions.FlashPointFirst) {
			BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
			BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
		} else {
			int FlashPointCount = BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
			int PCIMultiMasterCount = BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
			if (FlashPointCount > 0 && PCIMultiMasterCount > 0) {
				struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[FlashPointCount];
				struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
				struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
				struct BusLogic_BIOSDriveMapByte Drive0MapByte;
				while (ProbeInfo->HostAdapterBusType != BusLogic_PCI_Bus)
					ProbeInfo++;
				HostAdapter->IO_Address = ProbeInfo->IO_Address;
				FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_BIOS_BaseOffset + BusLogic_BIOS_DriveMapOffset + 0;
				FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(Drive0MapByte);
				BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &Drive0MapByte, sizeof(Drive0MapByte));
				if (Drive0MapByte.DiskGeometry != BusLogic_BIOS_Disk_Not_Installed) {
					struct BusLogic_ProbeInfo SavedProbeInfo[BusLogic_MaxHostAdapters];
					int MultiMasterCount = BusLogic_ProbeInfoCount - FlashPointCount;
					memcpy(SavedProbeInfo, BusLogic_ProbeInfoList, BusLogic_ProbeInfoCount * sizeof(struct BusLogic_ProbeInfo));
					memcpy(&BusLogic_ProbeInfoList[0], &SavedProbeInfo[FlashPointCount], MultiMasterCount * sizeof(struct BusLogic_ProbeInfo));
					memcpy(&BusLogic_ProbeInfoList[MultiMasterCount], &SavedProbeInfo[0], FlashPointCount * sizeof(struct BusLogic_ProbeInfo));
				}
			}
		}
	} else
		BusLogic_InitializeProbeInfoListISA(PrototypeHostAdapter);
}


#else
#define BusLogic_InitializeProbeInfoList(adapter) \
		BusLogic_InitializeProbeInfoListISA(adapter)
#endif				



static bool BusLogic_Failure(struct BusLogic_HostAdapter *HostAdapter, char *ErrorMessage)
{
	BusLogic_AnnounceDriver(HostAdapter);
	if (HostAdapter->HostAdapterBusType == BusLogic_PCI_Bus) {
		BusLogic_Error("While configuring BusLogic PCI Host Adapter at\n", HostAdapter);
		BusLogic_Error("Bus %d Device %d I/O Address 0x%X PCI Address 0x%X:\n", HostAdapter, HostAdapter->Bus, HostAdapter->Device, HostAdapter->IO_Address, HostAdapter->PCI_Address);
	} else
		BusLogic_Error("While configuring BusLogic Host Adapter at " "I/O Address 0x%X:\n", HostAdapter, HostAdapter->IO_Address);
	BusLogic_Error("%s FAILED - DETACHING\n", HostAdapter, ErrorMessage);
	if (BusLogic_CommandFailureReason != NULL)
		BusLogic_Error("ADDITIONAL FAILURE INFO - %s\n", HostAdapter, BusLogic_CommandFailureReason);
	return false;
}



static bool __init BusLogic_ProbeHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_StatusRegister StatusRegister;
	union BusLogic_InterruptRegister InterruptRegister;
	union BusLogic_GeometryRegister GeometryRegister;
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		FlashPointInfo->BaseAddress = (u32) HostAdapter->IO_Address;
		FlashPointInfo->IRQ_Channel = HostAdapter->IRQ_Channel;
		FlashPointInfo->Present = false;
		if (!(FlashPoint_ProbeHostAdapter(FlashPointInfo) == 0 && FlashPointInfo->Present)) {
			BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at " "PCI Bus %d Device %d\n", HostAdapter, HostAdapter->Bus, HostAdapter->Device);
			BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, " "but FlashPoint\n", HostAdapter, HostAdapter->IO_Address, HostAdapter->PCI_Address);
			BusLogic_Error("BusLogic: Probe Function failed to validate it.\n", HostAdapter);
			return false;
		}
		if (BusLogic_GlobalOptions.TraceProbe)
			BusLogic_Notice("BusLogic_Probe(0x%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_Address);
		return true;
	}
	StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
	InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
	GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
	if (BusLogic_GlobalOptions.TraceProbe)
		BusLogic_Notice("BusLogic_Probe(0x%X): Status 0x%02X, Interrupt 0x%02X, " "Geometry 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All, InterruptRegister.All, GeometryRegister.All);
	if (StatusRegister.All == 0 || StatusRegister.sr.DiagnosticActive || StatusRegister.sr.CommandParameterRegisterBusy || StatusRegister.sr.Reserved || StatusRegister.sr.CommandInvalid || InterruptRegister.ir.Reserved != 0)
		return false;
	if (GeometryRegister.All == 0xFF)
		return false;
	return true;
}



static bool BusLogic_HardwareResetHostAdapter(struct BusLogic_HostAdapter
						 *HostAdapter, bool HardReset)
{
	union BusLogic_StatusRegister StatusRegister;
	int TimeoutCounter;
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		FlashPointInfo->HostSoftReset = !HardReset;
		FlashPointInfo->ReportDataUnderrun = true;
		HostAdapter->CardHandle = FlashPoint_HardwareResetHostAdapter(FlashPointInfo);
		if (HostAdapter->CardHandle == FlashPoint_BadCardHandle)
			return false;
		return true;
	}
	if (HardReset)
		BusLogic_HardReset(HostAdapter);
	else
		BusLogic_SoftReset(HostAdapter);
	TimeoutCounter = 5 * 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.DiagnosticActive)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Active, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	udelay(100);
	TimeoutCounter = 10 * 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (!StatusRegister.sr.DiagnosticActive)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Completed, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	TimeoutCounter = 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.DiagnosticFailure || StatusRegister.sr.HostAdapterReady || StatusRegister.sr.DataInRegisterReady)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Host Adapter Ready, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	if (StatusRegister.sr.DiagnosticFailure || !StatusRegister.sr.HostAdapterReady) {
		BusLogic_CommandFailureReason = NULL;
		BusLogic_Failure(HostAdapter, "HARD RESET DIAGNOSTICS");
		BusLogic_Error("HOST ADAPTER STATUS REGISTER = %02X\n", HostAdapter, StatusRegister.All);
		if (StatusRegister.sr.DataInRegisterReady) {
			unsigned char ErrorCode = BusLogic_ReadDataInRegister(HostAdapter);
			BusLogic_Error("HOST ADAPTER ERROR CODE = %d\n", HostAdapter, ErrorCode);
		}
		return false;
	}
	return true;
}



static bool __init BusLogic_CheckHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	struct BusLogic_ExtendedSetupInformation ExtendedSetupInformation;
	unsigned char RequestedReplyLength;
	bool Result = true;
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		return true;
	RequestedReplyLength = sizeof(ExtendedSetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &ExtendedSetupInformation, sizeof(ExtendedSetupInformation))
	    != sizeof(ExtendedSetupInformation))
		Result = false;
	if (BusLogic_GlobalOptions.TraceProbe)
		BusLogic_Notice("BusLogic_Check(0x%X): MultiMaster %s\n", HostAdapter, HostAdapter->IO_Address, (Result ? "Found" : "Not Found"));
	return Result;
}



static bool __init BusLogic_ReadHostAdapterConfiguration(struct BusLogic_HostAdapter
							    *HostAdapter)
{
	struct BusLogic_BoardID BoardID;
	struct BusLogic_Configuration Configuration;
	struct BusLogic_SetupInformation SetupInformation;
	struct BusLogic_ExtendedSetupInformation ExtendedSetupInformation;
	unsigned char HostAdapterModelNumber[5];
	unsigned char FirmwareVersion3rdDigit;
	unsigned char FirmwareVersionLetter;
	struct BusLogic_PCIHostAdapterInformation PCIHostAdapterInformation;
	struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
	struct BusLogic_AutoSCSIData AutoSCSIData;
	union BusLogic_GeometryRegister GeometryRegister;
	unsigned char RequestedReplyLength;
	unsigned char *TargetPointer, Character;
	int TargetID, i;
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		TargetPointer = HostAdapter->ModelName;
		*TargetPointer++ = 'B';
		*TargetPointer++ = 'T';
		*TargetPointer++ = '-';
		for (i = 0; i < sizeof(FlashPointInfo->ModelNumber); i++)
			*TargetPointer++ = FlashPointInfo->ModelNumber[i];
		*TargetPointer++ = '\0';
		strcpy(HostAdapter->FirmwareVersion, FlashPoint_FirmwareVersion);
		HostAdapter->SCSI_ID = FlashPointInfo->SCSI_ID;
		HostAdapter->ExtendedTranslationEnabled = FlashPointInfo->ExtendedTranslationEnabled;
		HostAdapter->ParityCheckingEnabled = FlashPointInfo->ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = !FlashPointInfo->HostSoftReset;
		HostAdapter->LevelSensitiveInterrupt = true;
		HostAdapter->HostWideSCSI = FlashPointInfo->HostWideSCSI;
		HostAdapter->HostDifferentialSCSI = false;
		HostAdapter->HostSupportsSCAM = true;
		HostAdapter->HostUltraSCSI = true;
		HostAdapter->ExtendedLUNSupport = true;
		HostAdapter->TerminationInfoValid = true;
		HostAdapter->LowByteTerminated = FlashPointInfo->LowByteTerminated;
		HostAdapter->HighByteTerminated = FlashPointInfo->HighByteTerminated;
		HostAdapter->SCAM_Enabled = FlashPointInfo->SCAM_Enabled;
		HostAdapter->SCAM_Level2 = FlashPointInfo->SCAM_Level2;
		HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
		HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
		HostAdapter->MaxLogicalUnits = 32;
		HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
		HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
		HostAdapter->DriverQueueDepth = 255;
		HostAdapter->HostAdapterQueueDepth = HostAdapter->DriverQueueDepth;
		HostAdapter->SynchronousPermitted = FlashPointInfo->SynchronousPermitted;
		HostAdapter->FastPermitted = FlashPointInfo->FastPermitted;
		HostAdapter->UltraPermitted = FlashPointInfo->UltraPermitted;
		HostAdapter->WidePermitted = FlashPointInfo->WidePermitted;
		HostAdapter->DisconnectPermitted = FlashPointInfo->DisconnectPermitted;
		HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		goto Common;
	}
	if (BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0, &BoardID, sizeof(BoardID)) != sizeof(BoardID))
		return BusLogic_Failure(HostAdapter, "INQUIRE BOARD ID");
	if (BusLogic_Command(HostAdapter, BusLogic_InquireConfiguration, NULL, 0, &Configuration, sizeof(Configuration))
	    != sizeof(Configuration))
		return BusLogic_Failure(HostAdapter, "INQUIRE CONFIGURATION");
	RequestedReplyLength = sizeof(SetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &SetupInformation, sizeof(SetupInformation))
	    != sizeof(SetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
	RequestedReplyLength = sizeof(ExtendedSetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &ExtendedSetupInformation, sizeof(ExtendedSetupInformation))
	    != sizeof(ExtendedSetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE EXTENDED SETUP INFORMATION");
	FirmwareVersion3rdDigit = '\0';
	if (BoardID.FirmwareVersion1stDigit > '0')
		if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersion3rdDigit, NULL, 0, &FirmwareVersion3rdDigit, sizeof(FirmwareVersion3rdDigit))
		    != sizeof(FirmwareVersion3rdDigit))
			return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE 3RD DIGIT");
	if (ExtendedSetupInformation.BusType == 'A' && BoardID.FirmwareVersion1stDigit == '2')
		
		strcpy(HostAdapterModelNumber, "542B");
	else if (ExtendedSetupInformation.BusType == 'E' && BoardID.FirmwareVersion1stDigit == '2' && (BoardID.FirmwareVersion2ndDigit <= '1' || (BoardID.FirmwareVersion2ndDigit == '2' && FirmwareVersion3rdDigit == '0')))
		
		strcpy(HostAdapterModelNumber, "742A");
	else if (ExtendedSetupInformation.BusType == 'E' && BoardID.FirmwareVersion1stDigit == '0')
		
		strcpy(HostAdapterModelNumber, "747A");
	else {
		RequestedReplyLength = sizeof(HostAdapterModelNumber);
		if (BusLogic_Command(HostAdapter, BusLogic_InquireHostAdapterModelNumber, &RequestedReplyLength, sizeof(RequestedReplyLength), &HostAdapterModelNumber, sizeof(HostAdapterModelNumber))
		    != sizeof(HostAdapterModelNumber))
			return BusLogic_Failure(HostAdapter, "INQUIRE HOST ADAPTER MODEL NUMBER");
	}
	TargetPointer = HostAdapter->ModelName;
	*TargetPointer++ = 'B';
	*TargetPointer++ = 'T';
	*TargetPointer++ = '-';
	for (i = 0; i < sizeof(HostAdapterModelNumber); i++) {
		Character = HostAdapterModelNumber[i];
		if (Character == ' ' || Character == '\0')
			break;
		*TargetPointer++ = Character;
	}
	*TargetPointer++ = '\0';
	TargetPointer = HostAdapter->FirmwareVersion;
	*TargetPointer++ = BoardID.FirmwareVersion1stDigit;
	*TargetPointer++ = '.';
	*TargetPointer++ = BoardID.FirmwareVersion2ndDigit;
	if (FirmwareVersion3rdDigit != ' ' && FirmwareVersion3rdDigit != '\0')
		*TargetPointer++ = FirmwareVersion3rdDigit;
	*TargetPointer = '\0';
	if (strcmp(HostAdapter->FirmwareVersion, "3.3") >= 0) {
		if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersionLetter, NULL, 0, &FirmwareVersionLetter, sizeof(FirmwareVersionLetter))
		    != sizeof(FirmwareVersionLetter))
			return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE VERSION LETTER");
		if (FirmwareVersionLetter != ' ' && FirmwareVersionLetter != '\0')
			*TargetPointer++ = FirmwareVersionLetter;
		*TargetPointer = '\0';
	}
	HostAdapter->SCSI_ID = Configuration.HostAdapterID;
	HostAdapter->HostAdapterBusType = BusLogic_HostAdapterBusTypes[HostAdapter->ModelName[3] - '4'];
	if (HostAdapter->IRQ_Channel == 0) {
		if (Configuration.IRQ_Channel9)
			HostAdapter->IRQ_Channel = 9;
		else if (Configuration.IRQ_Channel10)
			HostAdapter->IRQ_Channel = 10;
		else if (Configuration.IRQ_Channel11)
			HostAdapter->IRQ_Channel = 11;
		else if (Configuration.IRQ_Channel12)
			HostAdapter->IRQ_Channel = 12;
		else if (Configuration.IRQ_Channel14)
			HostAdapter->IRQ_Channel = 14;
		else if (Configuration.IRQ_Channel15)
			HostAdapter->IRQ_Channel = 15;
	}
	if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus) {
		if (Configuration.DMA_Channel5)
			HostAdapter->DMA_Channel = 5;
		else if (Configuration.DMA_Channel6)
			HostAdapter->DMA_Channel = 6;
		else if (Configuration.DMA_Channel7)
			HostAdapter->DMA_Channel = 7;
	}
	GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
	HostAdapter->ExtendedTranslationEnabled = GeometryRegister.gr.ExtendedTranslationEnabled;
	HostAdapter->HostAdapterScatterGatherLimit = ExtendedSetupInformation.ScatterGatherLimit;
	HostAdapter->DriverScatterGatherLimit = HostAdapter->HostAdapterScatterGatherLimit;
	if (HostAdapter->HostAdapterScatterGatherLimit > BusLogic_ScatterGatherLimit)
		HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
	if (ExtendedSetupInformation.Misc.LevelSensitiveInterrupt)
		HostAdapter->LevelSensitiveInterrupt = true;
	HostAdapter->HostWideSCSI = ExtendedSetupInformation.HostWideSCSI;
	HostAdapter->HostDifferentialSCSI = ExtendedSetupInformation.HostDifferentialSCSI;
	HostAdapter->HostSupportsSCAM = ExtendedSetupInformation.HostSupportsSCAM;
	HostAdapter->HostUltraSCSI = ExtendedSetupInformation.HostUltraSCSI;
	if (HostAdapter->FirmwareVersion[0] == '5' || (HostAdapter->FirmwareVersion[0] == '4' && HostAdapter->HostWideSCSI))
		HostAdapter->ExtendedLUNSupport = true;
	if (HostAdapter->FirmwareVersion[0] == '5') {
		if (BusLogic_Command(HostAdapter, BusLogic_InquirePCIHostAdapterInformation, NULL, 0, &PCIHostAdapterInformation, sizeof(PCIHostAdapterInformation))
		    != sizeof(PCIHostAdapterInformation))
			return BusLogic_Failure(HostAdapter, "INQUIRE PCI HOST ADAPTER INFORMATION");
		if (PCIHostAdapterInformation.GenericInfoValid) {
			HostAdapter->TerminationInfoValid = true;
			HostAdapter->LowByteTerminated = PCIHostAdapterInformation.LowByteTerminated;
			HostAdapter->HighByteTerminated = PCIHostAdapterInformation.HighByteTerminated;
		}
	}
	if (HostAdapter->FirmwareVersion[0] >= '4') {
		FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_AutoSCSI_BaseOffset;
		FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(AutoSCSIData);
		if (BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &AutoSCSIData, sizeof(AutoSCSIData))
		    != sizeof(AutoSCSIData))
			return BusLogic_Failure(HostAdapter, "FETCH HOST ADAPTER LOCAL RAM");
		HostAdapter->ParityCheckingEnabled = AutoSCSIData.ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = AutoSCSIData.BusResetEnabled;
		if (HostAdapter->FirmwareVersion[0] == '4') {
			HostAdapter->TerminationInfoValid = true;
			HostAdapter->LowByteTerminated = AutoSCSIData.LowByteTerminated;
			HostAdapter->HighByteTerminated = AutoSCSIData.HighByteTerminated;
		}
		HostAdapter->WidePermitted = AutoSCSIData.WidePermitted;
		HostAdapter->FastPermitted = AutoSCSIData.FastPermitted;
		HostAdapter->SynchronousPermitted = AutoSCSIData.SynchronousPermitted;
		HostAdapter->DisconnectPermitted = AutoSCSIData.DisconnectPermitted;
		if (HostAdapter->HostUltraSCSI)
			HostAdapter->UltraPermitted = AutoSCSIData.UltraPermitted;
		if (HostAdapter->HostSupportsSCAM) {
			HostAdapter->SCAM_Enabled = AutoSCSIData.SCAM_Enabled;
			HostAdapter->SCAM_Level2 = AutoSCSIData.SCAM_Level2;
		}
	}
	if (HostAdapter->FirmwareVersion[0] < '4') {
		if (SetupInformation.SynchronousInitiationEnabled) {
			HostAdapter->SynchronousPermitted = 0xFF;
			if (HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus) {
				if (ExtendedSetupInformation.Misc.FastOnEISA)
					HostAdapter->FastPermitted = 0xFF;
				if (strcmp(HostAdapter->ModelName, "BT-757") == 0)
					HostAdapter->WidePermitted = 0xFF;
			}
		}
		HostAdapter->DisconnectPermitted = 0xFF;
		HostAdapter->ParityCheckingEnabled = SetupInformation.ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = true;
	}
	HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
	HostAdapter->MaxLogicalUnits = (HostAdapter->ExtendedLUNSupport ? 32 : 8);
	if (HostAdapter->FirmwareVersion[0] == '5')
		HostAdapter->HostAdapterQueueDepth = 192;
	else if (HostAdapter->FirmwareVersion[0] == '4')
		HostAdapter->HostAdapterQueueDepth = (HostAdapter->HostAdapterBusType != BusLogic_ISA_Bus ? 100 : 50);
	else
		HostAdapter->HostAdapterQueueDepth = 30;
	if (strcmp(HostAdapter->FirmwareVersion, "3.31") >= 0) {
		HostAdapter->StrictRoundRobinModeSupport = true;
		HostAdapter->MailboxCount = BusLogic_MaxMailboxes;
	} else {
		HostAdapter->StrictRoundRobinModeSupport = false;
		HostAdapter->MailboxCount = 32;
	}
	HostAdapter->DriverQueueDepth = HostAdapter->MailboxCount;
	HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
	HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
	HostAdapter->TaggedQueuingPermitted = 0;
	switch (HostAdapter->FirmwareVersion[0]) {
	case '5':
		HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	case '4':
		if (strcmp(HostAdapter->FirmwareVersion, "4.22") >= 0)
			HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	case '3':
		if (strcmp(HostAdapter->FirmwareVersion, "3.35") >= 0)
			HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	}
	HostAdapter->BIOS_Address = ExtendedSetupInformation.BIOS_Address << 12;
	if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus && (void *) high_memory > (void *) MAX_DMA_ADDRESS)
		HostAdapter->BounceBuffersRequired = true;
	if (HostAdapter->BIOS_Address > 0 && strcmp(HostAdapter->ModelName, "BT-445S") == 0 && strcmp(HostAdapter->FirmwareVersion, "3.37") < 0 && (void *) high_memory > (void *) MAX_DMA_ADDRESS)
		HostAdapter->BounceBuffersRequired = true;
      Common:
	strcpy(HostAdapter->FullModelName, "BusLogic ");
	strcat(HostAdapter->FullModelName, HostAdapter->ModelName);
	for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++) {
		unsigned char QueueDepth = 0;
		if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->QueueDepth[TargetID] > 0)
			QueueDepth = HostAdapter->DriverOptions->QueueDepth[TargetID];
		else if (HostAdapter->BounceBuffersRequired)
			QueueDepth = BusLogic_TaggedQueueDepthBB;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
	}
	if (HostAdapter->BounceBuffersRequired)
		HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepthBB;
	else
		HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepth;
	if (HostAdapter->DriverOptions != NULL)
		HostAdapter->CommonQueueDepth = HostAdapter->DriverOptions->CommonQueueDepth;
	if (HostAdapter->CommonQueueDepth > 0 && HostAdapter->CommonQueueDepth < HostAdapter->UntaggedQueueDepth)
		HostAdapter->UntaggedQueueDepth = HostAdapter->CommonQueueDepth;
	HostAdapter->TaggedQueuingPermitted &= HostAdapter->DisconnectPermitted;
	if (HostAdapter->DriverOptions != NULL)
		HostAdapter->TaggedQueuingPermitted =
		    (HostAdapter->DriverOptions->TaggedQueuingPermitted & HostAdapter->DriverOptions->TaggedQueuingPermittedMask) | (HostAdapter->TaggedQueuingPermitted & ~HostAdapter->DriverOptions->TaggedQueuingPermittedMask);

	if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->BusSettleTime > 0)
		HostAdapter->BusSettleTime = HostAdapter->DriverOptions->BusSettleTime;
	else
		HostAdapter->BusSettleTime = BusLogic_DefaultBusSettleTime;
	return true;
}



static bool __init BusLogic_ReportHostAdapterConfiguration(struct BusLogic_HostAdapter
							      *HostAdapter)
{
	unsigned short AllTargetsMask = (1 << HostAdapter->MaxTargetDevices) - 1;
	unsigned short SynchronousPermitted, FastPermitted;
	unsigned short UltraPermitted, WidePermitted;
	unsigned short DisconnectPermitted, TaggedQueuingPermitted;
	bool CommonSynchronousNegotiation, CommonTaggedQueueDepth;
	char SynchronousString[BusLogic_MaxTargetDevices + 1];
	char WideString[BusLogic_MaxTargetDevices + 1];
	char DisconnectString[BusLogic_MaxTargetDevices + 1];
	char TaggedQueuingString[BusLogic_MaxTargetDevices + 1];
	char *SynchronousMessage = SynchronousString;
	char *WideMessage = WideString;
	char *DisconnectMessage = DisconnectString;
	char *TaggedQueuingMessage = TaggedQueuingString;
	int TargetID;
	BusLogic_Info("Configuring BusLogic Model %s %s%s%s%s SCSI Host Adapter\n",
		      HostAdapter, HostAdapter->ModelName,
		      BusLogic_HostAdapterBusNames[HostAdapter->HostAdapterBusType], (HostAdapter->HostWideSCSI ? " Wide" : ""), (HostAdapter->HostDifferentialSCSI ? " Differential" : ""), (HostAdapter->HostUltraSCSI ? " Ultra" : ""));
	BusLogic_Info("  Firmware Version: %s, I/O Address: 0x%X, " "IRQ Channel: %d/%s\n", HostAdapter, HostAdapter->FirmwareVersion, HostAdapter->IO_Address, HostAdapter->IRQ_Channel, (HostAdapter->LevelSensitiveInterrupt ? "Level" : "Edge"));
	if (HostAdapter->HostAdapterBusType != BusLogic_PCI_Bus) {
		BusLogic_Info("  DMA Channel: ", HostAdapter);
		if (HostAdapter->DMA_Channel > 0)
			BusLogic_Info("%d, ", HostAdapter, HostAdapter->DMA_Channel);
		else
			BusLogic_Info("None, ", HostAdapter);
		if (HostAdapter->BIOS_Address > 0)
			BusLogic_Info("BIOS Address: 0x%X, ", HostAdapter, HostAdapter->BIOS_Address);
		else
			BusLogic_Info("BIOS Address: None, ", HostAdapter);
	} else {
		BusLogic_Info("  PCI Bus: %d, Device: %d, Address: ", HostAdapter, HostAdapter->Bus, HostAdapter->Device);
		if (HostAdapter->PCI_Address > 0)
			BusLogic_Info("0x%X, ", HostAdapter, HostAdapter->PCI_Address);
		else
			BusLogic_Info("Unassigned, ", HostAdapter);
	}
	BusLogic_Info("Host Adapter SCSI ID: %d\n", HostAdapter, HostAdapter->SCSI_ID);
	BusLogic_Info("  Parity Checking: %s, Extended Translation: %s\n", HostAdapter, (HostAdapter->ParityCheckingEnabled ? "Enabled" : "Disabled"), (HostAdapter->ExtendedTranslationEnabled ? "Enabled" : "Disabled"));
	AllTargetsMask &= ~(1 << HostAdapter->SCSI_ID);
	SynchronousPermitted = HostAdapter->SynchronousPermitted & AllTargetsMask;
	FastPermitted = HostAdapter->FastPermitted & AllTargetsMask;
	UltraPermitted = HostAdapter->UltraPermitted & AllTargetsMask;
	if ((BusLogic_MultiMasterHostAdapterP(HostAdapter) && (HostAdapter->FirmwareVersion[0] >= '4' || HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus)) || BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		CommonSynchronousNegotiation = false;
		if (SynchronousPermitted == 0) {
			SynchronousMessage = "Disabled";
			CommonSynchronousNegotiation = true;
		} else if (SynchronousPermitted == AllTargetsMask) {
			if (FastPermitted == 0) {
				SynchronousMessage = "Slow";
				CommonSynchronousNegotiation = true;
			} else if (FastPermitted == AllTargetsMask) {
				if (UltraPermitted == 0) {
					SynchronousMessage = "Fast";
					CommonSynchronousNegotiation = true;
				} else if (UltraPermitted == AllTargetsMask) {
					SynchronousMessage = "Ultra";
					CommonSynchronousNegotiation = true;
				}
			}
		}
		if (!CommonSynchronousNegotiation) {
			for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
				SynchronousString[TargetID] = ((!(SynchronousPermitted & (1 << TargetID))) ? 'N' : (!(FastPermitted & (1 << TargetID)) ? 'S' : (!(UltraPermitted & (1 << TargetID)) ? 'F' : 'U')));
			SynchronousString[HostAdapter->SCSI_ID] = '#';
			SynchronousString[HostAdapter->MaxTargetDevices] = '\0';
		}
	} else
		SynchronousMessage = (SynchronousPermitted == 0 ? "Disabled" : "Enabled");
	WidePermitted = HostAdapter->WidePermitted & AllTargetsMask;
	if (WidePermitted == 0)
		WideMessage = "Disabled";
	else if (WidePermitted == AllTargetsMask)
		WideMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			WideString[TargetID] = ((WidePermitted & (1 << TargetID)) ? 'Y' : 'N');
		WideString[HostAdapter->SCSI_ID] = '#';
		WideString[HostAdapter->MaxTargetDevices] = '\0';
	}
	DisconnectPermitted = HostAdapter->DisconnectPermitted & AllTargetsMask;
	if (DisconnectPermitted == 0)
		DisconnectMessage = "Disabled";
	else if (DisconnectPermitted == AllTargetsMask)
		DisconnectMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			DisconnectString[TargetID] = ((DisconnectPermitted & (1 << TargetID)) ? 'Y' : 'N');
		DisconnectString[HostAdapter->SCSI_ID] = '#';
		DisconnectString[HostAdapter->MaxTargetDevices] = '\0';
	}
	TaggedQueuingPermitted = HostAdapter->TaggedQueuingPermitted & AllTargetsMask;
	if (TaggedQueuingPermitted == 0)
		TaggedQueuingMessage = "Disabled";
	else if (TaggedQueuingPermitted == AllTargetsMask)
		TaggedQueuingMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			TaggedQueuingString[TargetID] = ((TaggedQueuingPermitted & (1 << TargetID)) ? 'Y' : 'N');
		TaggedQueuingString[HostAdapter->SCSI_ID] = '#';
		TaggedQueuingString[HostAdapter->MaxTargetDevices] = '\0';
	}
	BusLogic_Info("  Synchronous Negotiation: %s, Wide Negotiation: %s\n", HostAdapter, SynchronousMessage, WideMessage);
	BusLogic_Info("  Disconnect/Reconnect: %s, Tagged Queuing: %s\n", HostAdapter, DisconnectMessage, TaggedQueuingMessage);
	if (BusLogic_MultiMasterHostAdapterP(HostAdapter)) {
		BusLogic_Info("  Scatter/Gather Limit: %d of %d segments, " "Mailboxes: %d\n", HostAdapter, HostAdapter->DriverScatterGatherLimit, HostAdapter->HostAdapterScatterGatherLimit, HostAdapter->MailboxCount);
		BusLogic_Info("  Driver Queue Depth: %d, " "Host Adapter Queue Depth: %d\n", HostAdapter, HostAdapter->DriverQueueDepth, HostAdapter->HostAdapterQueueDepth);
	} else
		BusLogic_Info("  Driver Queue Depth: %d, " "Scatter/Gather Limit: %d segments\n", HostAdapter, HostAdapter->DriverQueueDepth, HostAdapter->DriverScatterGatherLimit);
	BusLogic_Info("  Tagged Queue Depth: ", HostAdapter);
	CommonTaggedQueueDepth = true;
	for (TargetID = 1; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		if (HostAdapter->QueueDepth[TargetID] != HostAdapter->QueueDepth[0]) {
			CommonTaggedQueueDepth = false;
			break;
		}
	if (CommonTaggedQueueDepth) {
		if (HostAdapter->QueueDepth[0] > 0)
			BusLogic_Info("%d", HostAdapter, HostAdapter->QueueDepth[0]);
		else
			BusLogic_Info("Automatic", HostAdapter);
	} else
		BusLogic_Info("Individual", HostAdapter);
	BusLogic_Info(", Untagged Queue Depth: %d\n", HostAdapter, HostAdapter->UntaggedQueueDepth);
	if (HostAdapter->TerminationInfoValid) {
		if (HostAdapter->HostWideSCSI)
			BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter, (HostAdapter->LowByteTerminated ? (HostAdapter->HighByteTerminated ? "Both Enabled" : "Low Enabled")
										  : (HostAdapter->HighByteTerminated ? "High Enabled" : "Both Disabled")));
		else
			BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter, (HostAdapter->LowByteTerminated ? "Enabled" : "Disabled"));
		if (HostAdapter->HostSupportsSCAM)
			BusLogic_Info(", SCAM: %s", HostAdapter, (HostAdapter->SCAM_Enabled ? (HostAdapter->SCAM_Level2 ? "Enabled, Level 2" : "Enabled, Level 1")
								  : "Disabled"));
		BusLogic_Info("\n", HostAdapter);
	}
	return true;
}



static bool __init BusLogic_AcquireResources(struct BusLogic_HostAdapter *HostAdapter)
{
	if (HostAdapter->IRQ_Channel == 0) {
		BusLogic_Error("NO LEGAL INTERRUPT CHANNEL ASSIGNED - DETACHING\n", HostAdapter);
		return false;
	}
	if (request_irq(HostAdapter->IRQ_Channel, BusLogic_InterruptHandler, IRQF_SHARED, HostAdapter->FullModelName, HostAdapter) < 0) {
		BusLogic_Error("UNABLE TO ACQUIRE IRQ CHANNEL %d - DETACHING\n", HostAdapter, HostAdapter->IRQ_Channel);
		return false;
	}
	HostAdapter->IRQ_ChannelAcquired = true;
	if (HostAdapter->DMA_Channel > 0) {
		if (request_dma(HostAdapter->DMA_Channel, HostAdapter->FullModelName) < 0) {
			BusLogic_Error("UNABLE TO ACQUIRE DMA CHANNEL %d - DETACHING\n", HostAdapter, HostAdapter->DMA_Channel);
			return false;
		}
		set_dma_mode(HostAdapter->DMA_Channel, DMA_MODE_CASCADE);
		enable_dma(HostAdapter->DMA_Channel);
		HostAdapter->DMA_ChannelAcquired = true;
	}
	return true;
}



static void BusLogic_ReleaseResources(struct BusLogic_HostAdapter *HostAdapter)
{
	if (HostAdapter->IRQ_ChannelAcquired)
		free_irq(HostAdapter->IRQ_Channel, HostAdapter);
	if (HostAdapter->DMA_ChannelAcquired)
		free_dma(HostAdapter->DMA_Channel);
	if (HostAdapter->MailboxSpace)
		pci_free_consistent(HostAdapter->PCI_Device, HostAdapter->MailboxSize, HostAdapter->MailboxSpace, HostAdapter->MailboxSpaceHandle);
	pci_dev_put(HostAdapter->PCI_Device);
	HostAdapter->MailboxSpace = NULL;
	HostAdapter->MailboxSpaceHandle = 0;
	HostAdapter->MailboxSize = 0;
}



static bool BusLogic_InitializeHostAdapter(struct BusLogic_HostAdapter
					      *HostAdapter)
{
	struct BusLogic_ExtendedMailboxRequest ExtendedMailboxRequest;
	enum BusLogic_RoundRobinModeRequest RoundRobinModeRequest;
	enum BusLogic_SetCCBFormatRequest SetCCBFormatRequest;
	int TargetID;
	HostAdapter->FirstCompletedCCB = NULL;
	HostAdapter->LastCompletedCCB = NULL;
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		HostAdapter->BusDeviceResetPendingCCB[TargetID] = NULL;
		HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
		HostAdapter->TargetFlags[TargetID].CommandSuccessfulFlag = false;
		HostAdapter->ActiveCommands[TargetID] = 0;
		HostAdapter->CommandsSinceReset[TargetID] = 0;
	}
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		goto Done;
	HostAdapter->MailboxSize = HostAdapter->MailboxCount * (sizeof(struct BusLogic_OutgoingMailbox) + sizeof(struct BusLogic_IncomingMailbox));
	HostAdapter->MailboxSpace = pci_alloc_consistent(HostAdapter->PCI_Device, HostAdapter->MailboxSize, &HostAdapter->MailboxSpaceHandle);
	if (HostAdapter->MailboxSpace == NULL)
		return BusLogic_Failure(HostAdapter, "MAILBOX ALLOCATION");
	HostAdapter->FirstOutgoingMailbox = (struct BusLogic_OutgoingMailbox *) HostAdapter->MailboxSpace;
	HostAdapter->LastOutgoingMailbox = HostAdapter->FirstOutgoingMailbox + HostAdapter->MailboxCount - 1;
	HostAdapter->NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
	HostAdapter->FirstIncomingMailbox = (struct BusLogic_IncomingMailbox *) (HostAdapter->LastOutgoingMailbox + 1);
	HostAdapter->LastIncomingMailbox = HostAdapter->FirstIncomingMailbox + HostAdapter->MailboxCount - 1;
	HostAdapter->NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;

	memset(HostAdapter->FirstOutgoingMailbox, 0, HostAdapter->MailboxCount * sizeof(struct BusLogic_OutgoingMailbox));
	memset(HostAdapter->FirstIncomingMailbox, 0, HostAdapter->MailboxCount * sizeof(struct BusLogic_IncomingMailbox));
	ExtendedMailboxRequest.MailboxCount = HostAdapter->MailboxCount;
	ExtendedMailboxRequest.BaseMailboxAddress = (u32) HostAdapter->MailboxSpaceHandle;
	if (BusLogic_Command(HostAdapter, BusLogic_InitializeExtendedMailbox, &ExtendedMailboxRequest, sizeof(ExtendedMailboxRequest), NULL, 0) < 0)
		return BusLogic_Failure(HostAdapter, "MAILBOX INITIALIZATION");
	if (HostAdapter->StrictRoundRobinModeSupport) {
		RoundRobinModeRequest = BusLogic_StrictRoundRobinMode;
		if (BusLogic_Command(HostAdapter, BusLogic_EnableStrictRoundRobinMode, &RoundRobinModeRequest, sizeof(RoundRobinModeRequest), NULL, 0) < 0)
			return BusLogic_Failure(HostAdapter, "ENABLE STRICT ROUND ROBIN MODE");
	}
	if (HostAdapter->ExtendedLUNSupport) {
		SetCCBFormatRequest = BusLogic_ExtendedLUNFormatCCB;
		if (BusLogic_Command(HostAdapter, BusLogic_SetCCBFormat, &SetCCBFormatRequest, sizeof(SetCCBFormatRequest), NULL, 0) < 0)
			return BusLogic_Failure(HostAdapter, "SET CCB FORMAT");
	}
      Done:
	if (!HostAdapter->HostAdapterInitialized) {
		BusLogic_Info("*** %s Initialized Successfully ***\n", HostAdapter, HostAdapter->FullModelName);
		BusLogic_Info("\n", HostAdapter);
	} else
		BusLogic_Warning("*** %s Initialized Successfully ***\n", HostAdapter, HostAdapter->FullModelName);
	HostAdapter->HostAdapterInitialized = true;
	return true;
}



static bool __init BusLogic_TargetDeviceInquiry(struct BusLogic_HostAdapter
						   *HostAdapter)
{
	u16 InstalledDevices;
	u8 InstalledDevicesID0to7[8];
	struct BusLogic_SetupInformation SetupInformation;
	u8 SynchronousPeriod[BusLogic_MaxTargetDevices];
	unsigned char RequestedReplyLength;
	int TargetID;
	BusLogic_Delay(HostAdapter->BusSettleTime);
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		return true;
	if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->LocalOptions.InhibitTargetInquiry)
		return true;
	if (strcmp(HostAdapter->FirmwareVersion, "4.25") >= 0) {


		if (BusLogic_Command(HostAdapter, BusLogic_InquireTargetDevices, NULL, 0, &InstalledDevices, sizeof(InstalledDevices))
		    != sizeof(InstalledDevices))
			return BusLogic_Failure(HostAdapter, "INQUIRE TARGET DEVICES");
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->TargetFlags[TargetID].TargetExists = (InstalledDevices & (1 << TargetID) ? true : false);
	} else {


		if (BusLogic_Command(HostAdapter, BusLogic_InquireInstalledDevicesID0to7, NULL, 0, &InstalledDevicesID0to7, sizeof(InstalledDevicesID0to7))
		    != sizeof(InstalledDevicesID0to7))
			return BusLogic_Failure(HostAdapter, "INQUIRE INSTALLED DEVICES ID 0 TO 7");
		for (TargetID = 0; TargetID < 8; TargetID++)
			HostAdapter->TargetFlags[TargetID].TargetExists = (InstalledDevicesID0to7[TargetID] != 0 ? true : false);
	}
	RequestedReplyLength = sizeof(SetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &SetupInformation, sizeof(SetupInformation))
	    != sizeof(SetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		HostAdapter->SynchronousOffset[TargetID] = (TargetID < 8 ? SetupInformation.SynchronousValuesID0to7[TargetID].Offset : SetupInformation.SynchronousValuesID8to15[TargetID - 8].Offset);
	if (strcmp(HostAdapter->FirmwareVersion, "5.06L") >= 0)
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->TargetFlags[TargetID].WideTransfersActive = (TargetID < 8 ? (SetupInformation.WideTransfersActiveID0to7 & (1 << TargetID)
												  ? true : false)
										  : (SetupInformation.WideTransfersActiveID8to15 & (1 << (TargetID - 8))
										     ? true : false));
	if (HostAdapter->FirmwareVersion[0] >= '3') {


		RequestedReplyLength = sizeof(SynchronousPeriod);
		if (BusLogic_Command(HostAdapter, BusLogic_InquireSynchronousPeriod, &RequestedReplyLength, sizeof(RequestedReplyLength), &SynchronousPeriod, sizeof(SynchronousPeriod))
		    != sizeof(SynchronousPeriod))
			return BusLogic_Failure(HostAdapter, "INQUIRE SYNCHRONOUS PERIOD");
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->SynchronousPeriod[TargetID] = SynchronousPeriod[TargetID];
	} else
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			if (SetupInformation.SynchronousValuesID0to7[TargetID].Offset > 0)
				HostAdapter->SynchronousPeriod[TargetID] = 20 + 5 * SetupInformation.SynchronousValuesID0to7[TargetID]
				    .TransferPeriod;
	return true;
}


static void __init BusLogic_InitializeHostStructure(struct BusLogic_HostAdapter
						    *HostAdapter, struct Scsi_Host *Host)
{
	Host->max_id = HostAdapter->MaxTargetDevices;
	Host->max_lun = HostAdapter->MaxLogicalUnits;
	Host->max_channel = 0;
	Host->unique_id = HostAdapter->IO_Address;
	Host->this_id = HostAdapter->SCSI_ID;
	Host->can_queue = HostAdapter->DriverQueueDepth;
	Host->sg_tablesize = HostAdapter->DriverScatterGatherLimit;
	Host->unchecked_isa_dma = HostAdapter->BounceBuffersRequired;
	Host->cmd_per_lun = HostAdapter->UntaggedQueueDepth;
}

static int BusLogic_SlaveConfigure(struct scsi_device *Device)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Device->host->hostdata;
	int TargetID = Device->id;
	int QueueDepth = HostAdapter->QueueDepth[TargetID];

	if (HostAdapter->TargetFlags[TargetID].TaggedQueuingSupported && (HostAdapter->TaggedQueuingPermitted & (1 << TargetID))) {
		if (QueueDepth == 0)
			QueueDepth = BusLogic_MaxAutomaticTaggedQueueDepth;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
		scsi_adjust_queue_depth(Device, MSG_SIMPLE_TAG, QueueDepth);
	} else {
		HostAdapter->TaggedQueuingPermitted &= ~(1 << TargetID);
		QueueDepth = HostAdapter->UntaggedQueueDepth;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
		scsi_adjust_queue_depth(Device, 0, QueueDepth);
	}
	QueueDepth = 0;
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		if (HostAdapter->TargetFlags[TargetID].TargetExists) {
			QueueDepth += HostAdapter->QueueDepth[TargetID];
		}
	if (QueueDepth > HostAdapter->AllocatedCCBs)
		BusLogic_CreateAdditionalCCBs(HostAdapter, QueueDepth - HostAdapter->AllocatedCCBs, false);
	return 0;
}


static int __init BusLogic_init(void)
{
	int BusLogicHostAdapterCount = 0, DriverOptionsIndex = 0, ProbeIndex;
	struct BusLogic_HostAdapter *PrototypeHostAdapter;
	int ret = 0;

#ifdef MODULE
	if (BusLogic)
		BusLogic_Setup(BusLogic);
#endif

	if (BusLogic_ProbeOptions.NoProbe)
		return -ENODEV;
	BusLogic_ProbeInfoList =
	    kzalloc(BusLogic_MaxHostAdapters * sizeof(struct BusLogic_ProbeInfo), GFP_KERNEL);
	if (BusLogic_ProbeInfoList == NULL) {
		BusLogic_Error("BusLogic: Unable to allocate Probe Info List\n", NULL);
		return -ENOMEM;
	}

	PrototypeHostAdapter =
	    kzalloc(sizeof(struct BusLogic_HostAdapter), GFP_KERNEL);
	if (PrototypeHostAdapter == NULL) {
		kfree(BusLogic_ProbeInfoList);
		BusLogic_Error("BusLogic: Unable to allocate Prototype " "Host Adapter\n", NULL);
		return -ENOMEM;
	}

#ifdef MODULE
	if (BusLogic != NULL)
		BusLogic_Setup(BusLogic);
#endif
	BusLogic_InitializeProbeInfoList(PrototypeHostAdapter);
	for (ProbeIndex = 0; ProbeIndex < BusLogic_ProbeInfoCount; ProbeIndex++) {
		struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[ProbeIndex];
		struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
		struct Scsi_Host *Host;
		if (ProbeInfo->IO_Address == 0)
			continue;
		memset(HostAdapter, 0, sizeof(struct BusLogic_HostAdapter));
		HostAdapter->HostAdapterType = ProbeInfo->HostAdapterType;
		HostAdapter->HostAdapterBusType = ProbeInfo->HostAdapterBusType;
		HostAdapter->IO_Address = ProbeInfo->IO_Address;
		HostAdapter->PCI_Address = ProbeInfo->PCI_Address;
		HostAdapter->Bus = ProbeInfo->Bus;
		HostAdapter->Device = ProbeInfo->Device;
		HostAdapter->PCI_Device = ProbeInfo->PCI_Device;
		HostAdapter->IRQ_Channel = ProbeInfo->IRQ_Channel;
		HostAdapter->AddressCount = BusLogic_HostAdapterAddressCount[HostAdapter->HostAdapterType];

		if (!request_region(HostAdapter->IO_Address, HostAdapter->AddressCount,
					"BusLogic"))
			continue;
		if (!BusLogic_ProbeHostAdapter(HostAdapter)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		if (!BusLogic_HardwareResetHostAdapter(HostAdapter, true)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		if (!BusLogic_CheckHostAdapter(HostAdapter)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		if (DriverOptionsIndex < BusLogic_DriverOptionsCount)
			HostAdapter->DriverOptions = &BusLogic_DriverOptions[DriverOptionsIndex++];
		/*
		   Announce the Driver Version and Date, Author's Name, Copyright Notice,
		   and Electronic Mail Address.
		 */
		BusLogic_AnnounceDriver(HostAdapter);

		Host = scsi_host_alloc(&Bus_Logic_template, sizeof(struct BusLogic_HostAdapter));
		if (Host == NULL) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		HostAdapter = (struct BusLogic_HostAdapter *) Host->hostdata;
		memcpy(HostAdapter, PrototypeHostAdapter, sizeof(struct BusLogic_HostAdapter));
		HostAdapter->SCSI_Host = Host;
		HostAdapter->HostNumber = Host->host_no;
		list_add_tail(&HostAdapter->host_list, &BusLogic_host_list);

		if (BusLogic_ReadHostAdapterConfiguration(HostAdapter) &&
		    BusLogic_ReportHostAdapterConfiguration(HostAdapter) &&
		    BusLogic_AcquireResources(HostAdapter) &&
		    BusLogic_CreateInitialCCBs(HostAdapter) &&
		    BusLogic_InitializeHostAdapter(HostAdapter) &&
		    BusLogic_TargetDeviceInquiry(HostAdapter)) {
			release_region(HostAdapter->IO_Address,
				       HostAdapter->AddressCount);
			if (!request_region(HostAdapter->IO_Address,
					    HostAdapter->AddressCount,
					    HostAdapter->FullModelName)) {
				printk(KERN_WARNING
					"BusLogic: Release and re-register of "
					"port 0x%04lx failed \n",
					(unsigned long)HostAdapter->IO_Address);
				BusLogic_DestroyCCBs(HostAdapter);
				BusLogic_ReleaseResources(HostAdapter);
				list_del(&HostAdapter->host_list);
				scsi_host_put(Host);
				ret = -ENOMEM;
			} else {
				BusLogic_InitializeHostStructure(HostAdapter,
								 Host);
				if (scsi_add_host(Host, HostAdapter->PCI_Device
						? &HostAdapter->PCI_Device->dev
						  : NULL)) {
					printk(KERN_WARNING
					       "BusLogic: scsi_add_host()"
					       "failed!\n");
					BusLogic_DestroyCCBs(HostAdapter);
					BusLogic_ReleaseResources(HostAdapter);
					list_del(&HostAdapter->host_list);
					scsi_host_put(Host);
					ret = -ENODEV;
				} else {
					scsi_scan_host(Host);
					BusLogicHostAdapterCount++;
				}
			}
		} else {
			BusLogic_DestroyCCBs(HostAdapter);
			BusLogic_ReleaseResources(HostAdapter);
			list_del(&HostAdapter->host_list);
			scsi_host_put(Host);
			ret = -ENODEV;
		}
	}
	kfree(PrototypeHostAdapter);
	kfree(BusLogic_ProbeInfoList);
	BusLogic_ProbeInfoList = NULL;
	return ret;
}



static int __exit BusLogic_ReleaseHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	struct Scsi_Host *Host = HostAdapter->SCSI_Host;

	scsi_remove_host(Host);

	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		FlashPoint_ReleaseHostAdapter(HostAdapter->CardHandle);
	BusLogic_DestroyCCBs(HostAdapter);
	BusLogic_ReleaseResources(HostAdapter);
	release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
	list_del(&HostAdapter->host_list);

	scsi_host_put(Host);
	return 0;
}



static void BusLogic_QueueCompletedCCB(struct BusLogic_CCB *CCB)
{
	struct BusLogic_HostAdapter *HostAdapter = CCB->HostAdapter;
	CCB->Status = BusLogic_CCB_Completed;
	CCB->Next = NULL;
	if (HostAdapter->FirstCompletedCCB == NULL) {
		HostAdapter->FirstCompletedCCB = CCB;
		HostAdapter->LastCompletedCCB = CCB;
	} else {
		HostAdapter->LastCompletedCCB->Next = CCB;
		HostAdapter->LastCompletedCCB = CCB;
	}
	HostAdapter->ActiveCommands[CCB->TargetID]--;
}



static int BusLogic_ComputeResultCode(struct BusLogic_HostAdapter *HostAdapter, enum BusLogic_HostAdapterStatus HostAdapterStatus, enum BusLogic_TargetDeviceStatus TargetDeviceStatus)
{
	int HostStatus;
	switch (HostAdapterStatus) {
	case BusLogic_CommandCompletedNormally:
	case BusLogic_LinkedCommandCompleted:
	case BusLogic_LinkedCommandCompletedWithFlag:
		HostStatus = DID_OK;
		break;
	case BusLogic_SCSISelectionTimeout:
		HostStatus = DID_TIME_OUT;
		break;
	case BusLogic_InvalidOutgoingMailboxActionCode:
	case BusLogic_InvalidCommandOperationCode:
	case BusLogic_InvalidCommandParameter:
		BusLogic_Warning("BusLogic Driver Protocol Error 0x%02X\n", HostAdapter, HostAdapterStatus);
	case BusLogic_DataUnderRun:
	case BusLogic_DataOverRun:
	case BusLogic_UnexpectedBusFree:
	case BusLogic_LinkedCCBhasInvalidLUN:
	case BusLogic_AutoRequestSenseFailed:
	case BusLogic_TaggedQueuingMessageRejected:
	case BusLogic_UnsupportedMessageReceived:
	case BusLogic_HostAdapterHardwareFailed:
	case BusLogic_TargetDeviceReconnectedImproperly:
	case BusLogic_AbortQueueGenerated:
	case BusLogic_HostAdapterSoftwareError:
	case BusLogic_HostAdapterHardwareTimeoutError:
	case BusLogic_SCSIParityErrorDetected:
		HostStatus = DID_ERROR;
		break;
	case BusLogic_InvalidBusPhaseRequested:
	case BusLogic_TargetFailedResponseToATN:
	case BusLogic_HostAdapterAssertedRST:
	case BusLogic_OtherDeviceAssertedRST:
	case BusLogic_HostAdapterAssertedBusDeviceReset:
		HostStatus = DID_RESET;
		break;
	default:
		BusLogic_Warning("Unknown Host Adapter Status 0x%02X\n", HostAdapter, HostAdapterStatus);
		HostStatus = DID_ERROR;
		break;
	}
	return (HostStatus << 16) | TargetDeviceStatus;
}



static void BusLogic_ScanIncomingMailboxes(struct BusLogic_HostAdapter *HostAdapter)
{
	struct BusLogic_IncomingMailbox *NextIncomingMailbox = HostAdapter->NextIncomingMailbox;
	enum BusLogic_CompletionCode CompletionCode;
	while ((CompletionCode = NextIncomingMailbox->CompletionCode) != BusLogic_IncomingMailboxFree) {
		struct BusLogic_CCB *CCB = (struct BusLogic_CCB *) Bus_to_Virtual(NextIncomingMailbox->CCB);
		if (CompletionCode != BusLogic_AbortedCommandNotFound) {
			if (CCB->Status == BusLogic_CCB_Active || CCB->Status == BusLogic_CCB_Reset) {
				CCB->CompletionCode = CompletionCode;
				BusLogic_QueueCompletedCCB(CCB);
			} else {
				BusLogic_Warning("Illegal CCB #%ld status %d in " "Incoming Mailbox\n", HostAdapter, CCB->SerialNumber, CCB->Status);
			}
		}
		NextIncomingMailbox->CompletionCode = BusLogic_IncomingMailboxFree;
		if (++NextIncomingMailbox > HostAdapter->LastIncomingMailbox)
			NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;
	}
	HostAdapter->NextIncomingMailbox = NextIncomingMailbox;
}



static void BusLogic_ProcessCompletedCCBs(struct BusLogic_HostAdapter *HostAdapter)
{
	if (HostAdapter->ProcessCompletedCCBsActive)
		return;
	HostAdapter->ProcessCompletedCCBsActive = true;
	while (HostAdapter->FirstCompletedCCB != NULL) {
		struct BusLogic_CCB *CCB = HostAdapter->FirstCompletedCCB;
		struct scsi_cmnd *Command = CCB->Command;
		HostAdapter->FirstCompletedCCB = CCB->Next;
		if (HostAdapter->FirstCompletedCCB == NULL)
			HostAdapter->LastCompletedCCB = NULL;
		if (CCB->Opcode == BusLogic_BusDeviceReset) {
			int TargetID = CCB->TargetID;
			BusLogic_Warning("Bus Device Reset CCB #%ld to Target " "%d Completed\n", HostAdapter, CCB->SerialNumber, TargetID);
			BusLogic_IncrementErrorCounter(&HostAdapter->TargetStatistics[TargetID].BusDeviceResetsCompleted);
			HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
			HostAdapter->CommandsSinceReset[TargetID] = 0;
			HostAdapter->LastResetCompleted[TargetID] = jiffies;
			BusLogic_DeallocateCCB(CCB);
#if 0				
			while (Command != NULL) {
				struct scsi_cmnd *NextCommand = Command->reset_chain;
				Command->reset_chain = NULL;
				Command->result = DID_RESET << 16;
				Command->scsi_done(Command);
				Command = NextCommand;
			}
#endif
			for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
				if (CCB->Status == BusLogic_CCB_Reset && CCB->TargetID == TargetID) {
					Command = CCB->Command;
					BusLogic_DeallocateCCB(CCB);
					HostAdapter->ActiveCommands[TargetID]--;
					Command->result = DID_RESET << 16;
					Command->scsi_done(Command);
				}
			HostAdapter->BusDeviceResetPendingCCB[TargetID] = NULL;
		} else {
			switch (CCB->CompletionCode) {
			case BusLogic_IncomingMailboxFree:
			case BusLogic_AbortedCommandNotFound:
			case BusLogic_InvalidCCB:
				BusLogic_Warning("CCB #%ld to Target %d Impossible State\n", HostAdapter, CCB->SerialNumber, CCB->TargetID);
				break;
			case BusLogic_CommandCompletedWithoutError:
				HostAdapter->TargetStatistics[CCB->TargetID]
				    .CommandsCompleted++;
				HostAdapter->TargetFlags[CCB->TargetID]
				    .CommandSuccessfulFlag = true;
				Command->result = DID_OK << 16;
				break;
			case BusLogic_CommandAbortedAtHostRequest:
				BusLogic_Warning("CCB #%ld to Target %d Aborted\n", HostAdapter, CCB->SerialNumber, CCB->TargetID);
				BusLogic_IncrementErrorCounter(&HostAdapter->TargetStatistics[CCB->TargetID]
							       .CommandAbortsCompleted);
				Command->result = DID_ABORT << 16;
				break;
			case BusLogic_CommandCompletedWithError:
				Command->result = BusLogic_ComputeResultCode(HostAdapter, CCB->HostAdapterStatus, CCB->TargetDeviceStatus);
				if (CCB->HostAdapterStatus != BusLogic_SCSISelectionTimeout) {
					HostAdapter->TargetStatistics[CCB->TargetID]
					    .CommandsCompleted++;
					if (BusLogic_GlobalOptions.TraceErrors) {
						int i;
						BusLogic_Notice("CCB #%ld Target %d: Result %X Host "
								"Adapter Status %02X " "Target Status %02X\n", HostAdapter, CCB->SerialNumber, CCB->TargetID, Command->result, CCB->HostAdapterStatus, CCB->TargetDeviceStatus);
						BusLogic_Notice("CDB   ", HostAdapter);
						for (i = 0; i < CCB->CDB_Length; i++)
							BusLogic_Notice(" %02X", HostAdapter, CCB->CDB[i]);
						BusLogic_Notice("\n", HostAdapter);
						BusLogic_Notice("Sense ", HostAdapter);
						for (i = 0; i < CCB->SenseDataLength; i++)
							BusLogic_Notice(" %02X", HostAdapter, Command->sense_buffer[i]);
						BusLogic_Notice("\n", HostAdapter);
					}
				}
				break;
			}
			if (CCB->CDB[0] == INQUIRY && CCB->CDB[1] == 0 && CCB->HostAdapterStatus == BusLogic_CommandCompletedNormally) {
				struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[CCB->TargetID];
				struct SCSI_Inquiry *InquiryResult =
					(struct SCSI_Inquiry *) scsi_sglist(Command);
				TargetFlags->TargetExists = true;
				TargetFlags->TaggedQueuingSupported = InquiryResult->CmdQue;
				TargetFlags->WideTransfersSupported = InquiryResult->WBus16;
			}
			BusLogic_DeallocateCCB(CCB);
			Command->scsi_done(Command);
		}
	}
	HostAdapter->ProcessCompletedCCBsActive = false;
}



static irqreturn_t BusLogic_InterruptHandler(int IRQ_Channel, void *DeviceIdentifier)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) DeviceIdentifier;
	unsigned long ProcessorFlags;
	spin_lock_irqsave(HostAdapter->SCSI_Host->host_lock, ProcessorFlags);
	if (BusLogic_MultiMasterHostAdapterP(HostAdapter)) {
		union BusLogic_InterruptRegister InterruptRegister;
		InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
		if (InterruptRegister.ir.InterruptValid) {
			BusLogic_InterruptReset(HostAdapter);
			if (InterruptRegister.ir.ExternalBusReset)
				HostAdapter->HostAdapterExternalReset = true;
			else if (InterruptRegister.ir.IncomingMailboxLoaded)
				BusLogic_ScanIncomingMailboxes(HostAdapter);
			else if (InterruptRegister.ir.CommandComplete)
				HostAdapter->HostAdapterCommandCompleted = true;
		}
	} else {
		if (FlashPoint_InterruptPending(HostAdapter->CardHandle))
			switch (FlashPoint_HandleInterrupt(HostAdapter->CardHandle)) {
			case FlashPoint_NormalInterrupt:
				break;
			case FlashPoint_ExternalBusReset:
				HostAdapter->HostAdapterExternalReset = true;
				break;
			case FlashPoint_InternalError:
				BusLogic_Warning("Internal FlashPoint Error detected" " - Resetting Host Adapter\n", HostAdapter);
				HostAdapter->HostAdapterInternalError = true;
				break;
			}
	}
	if (HostAdapter->FirstCompletedCCB != NULL)
		BusLogic_ProcessCompletedCCBs(HostAdapter);
	if (HostAdapter->HostAdapterExternalReset) {
		BusLogic_Warning("Resetting %s due to External SCSI Bus Reset\n", HostAdapter, HostAdapter->FullModelName);
		BusLogic_IncrementErrorCounter(&HostAdapter->ExternalHostAdapterResets);
		BusLogic_ResetHostAdapter(HostAdapter, false);
		HostAdapter->HostAdapterExternalReset = false;
	} else if (HostAdapter->HostAdapterInternalError) {
		BusLogic_Warning("Resetting %s due to Host Adapter Internal Error\n", HostAdapter, HostAdapter->FullModelName);
		BusLogic_IncrementErrorCounter(&HostAdapter->HostAdapterInternalErrors);
		BusLogic_ResetHostAdapter(HostAdapter, true);
		HostAdapter->HostAdapterInternalError = false;
	}
	spin_unlock_irqrestore(HostAdapter->SCSI_Host->host_lock, ProcessorFlags);
	return IRQ_HANDLED;
}



static bool BusLogic_WriteOutgoingMailbox(struct BusLogic_HostAdapter
					     *HostAdapter, enum BusLogic_ActionCode ActionCode, struct BusLogic_CCB *CCB)
{
	struct BusLogic_OutgoingMailbox *NextOutgoingMailbox;
	NextOutgoingMailbox = HostAdapter->NextOutgoingMailbox;
	if (NextOutgoingMailbox->ActionCode == BusLogic_OutgoingMailboxFree) {
		CCB->Status = BusLogic_CCB_Active;
		/*
		   The CCB field must be written before the Action Code field since
		   the Host Adapter is operating asynchronously and the locking code
		   does not protect against simultaneous access by the Host Adapter.
		 */
		NextOutgoingMailbox->CCB = CCB->DMA_Handle;
		NextOutgoingMailbox->ActionCode = ActionCode;
		BusLogic_StartMailboxCommand(HostAdapter);
		if (++NextOutgoingMailbox > HostAdapter->LastOutgoingMailbox)
			NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
		HostAdapter->NextOutgoingMailbox = NextOutgoingMailbox;
		if (ActionCode == BusLogic_MailboxStartCommand) {
			HostAdapter->ActiveCommands[CCB->TargetID]++;
			if (CCB->Opcode != BusLogic_BusDeviceReset)
				HostAdapter->TargetStatistics[CCB->TargetID].CommandsAttempted++;
		}
		return true;
	}
	return false;
}


static int BusLogic_host_reset(struct scsi_cmnd * SCpnt)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) SCpnt->device->host->hostdata;

	unsigned int id = SCpnt->device->id;
	struct BusLogic_TargetStatistics *stats = &HostAdapter->TargetStatistics[id];
	int rc;

	spin_lock_irq(SCpnt->device->host->host_lock);

	BusLogic_IncrementErrorCounter(&stats->HostAdapterResetsRequested);

	rc = BusLogic_ResetHostAdapter(HostAdapter, false);
	spin_unlock_irq(SCpnt->device->host->host_lock);
	return rc;
}


static int BusLogic_QueueCommand_lck(struct scsi_cmnd *Command, void (*CompletionRoutine) (struct scsi_cmnd *))
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Command->device->host->hostdata;
	struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[Command->device->id];
	struct BusLogic_TargetStatistics *TargetStatistics = HostAdapter->TargetStatistics;
	unsigned char *CDB = Command->cmnd;
	int CDB_Length = Command->cmd_len;
	int TargetID = Command->device->id;
	int LogicalUnit = Command->device->lun;
	int BufferLength = scsi_bufflen(Command);
	int Count;
	struct BusLogic_CCB *CCB;
	if (CDB[0] == REQUEST_SENSE && Command->sense_buffer[0] != 0) {
		Command->result = DID_OK << 16;
		CompletionRoutine(Command);
		return 0;
	}
	CCB = BusLogic_AllocateCCB(HostAdapter);
	if (CCB == NULL) {
		spin_unlock_irq(HostAdapter->SCSI_Host->host_lock);
		BusLogic_Delay(1);
		spin_lock_irq(HostAdapter->SCSI_Host->host_lock);
		CCB = BusLogic_AllocateCCB(HostAdapter);
		if (CCB == NULL) {
			Command->result = DID_ERROR << 16;
			CompletionRoutine(Command);
			return 0;
		}
	}

	Count = scsi_dma_map(Command);
	BUG_ON(Count < 0);
	if (Count) {
		struct scatterlist *sg;
		int i;

		CCB->Opcode = BusLogic_InitiatorCCB_ScatterGather;
		CCB->DataLength = Count * sizeof(struct BusLogic_ScatterGatherSegment);
		if (BusLogic_MultiMasterHostAdapterP(HostAdapter))
			CCB->DataPointer = (unsigned int) CCB->DMA_Handle + ((unsigned long) &CCB->ScatterGatherList - (unsigned long) CCB);
		else
			CCB->DataPointer = Virtual_to_32Bit_Virtual(CCB->ScatterGatherList);

		scsi_for_each_sg(Command, sg, Count, i) {
			CCB->ScatterGatherList[i].SegmentByteCount =
				sg_dma_len(sg);
			CCB->ScatterGatherList[i].SegmentDataPointer =
				sg_dma_address(sg);
		}
	} else if (!Count) {
		CCB->Opcode = BusLogic_InitiatorCCB;
		CCB->DataLength = BufferLength;
		CCB->DataPointer = 0;
	}

	switch (CDB[0]) {
	case READ_6:
	case READ_10:
		CCB->DataDirection = BusLogic_DataInLengthChecked;
		TargetStatistics[TargetID].ReadCommands++;
		BusLogic_IncrementByteCounter(&TargetStatistics[TargetID].TotalBytesRead, BufferLength);
		BusLogic_IncrementSizeBucket(TargetStatistics[TargetID].ReadCommandSizeBuckets, BufferLength);
		break;
	case WRITE_6:
	case WRITE_10:
		CCB->DataDirection = BusLogic_DataOutLengthChecked;
		TargetStatistics[TargetID].WriteCommands++;
		BusLogic_IncrementByteCounter(&TargetStatistics[TargetID].TotalBytesWritten, BufferLength);
		BusLogic_IncrementSizeBucket(TargetStatistics[TargetID].WriteCommandSizeBuckets, BufferLength);
		break;
	default:
		CCB->DataDirection = BusLogic_UncheckedDataTransfer;
		break;
	}
	CCB->CDB_Length = CDB_Length;
	CCB->HostAdapterStatus = 0;
	CCB->TargetDeviceStatus = 0;
	CCB->TargetID = TargetID;
	CCB->LogicalUnit = LogicalUnit;
	CCB->TagEnable = false;
	CCB->LegacyTagEnable = false;
	if (HostAdapter->CommandsSinceReset[TargetID]++ >=
	    BusLogic_MaxTaggedQueueDepth && !TargetFlags->TaggedQueuingActive && HostAdapter->ActiveCommands[TargetID] == 0 && TargetFlags->TaggedQueuingSupported && (HostAdapter->TaggedQueuingPermitted & (1 << TargetID))) {
		TargetFlags->TaggedQueuingActive = true;
		BusLogic_Notice("Tagged Queuing now active for Target %d\n", HostAdapter, TargetID);
	}
	if (TargetFlags->TaggedQueuingActive) {
		enum BusLogic_QueueTag QueueTag = BusLogic_SimpleQueueTag;
		if (HostAdapter->ActiveCommands[TargetID] == 0)
			HostAdapter->LastSequencePoint[TargetID] = jiffies;
		else if (time_after(jiffies, HostAdapter->LastSequencePoint[TargetID] + 4 * HZ)) {
			HostAdapter->LastSequencePoint[TargetID] = jiffies;
			QueueTag = BusLogic_OrderedQueueTag;
		}
		if (HostAdapter->ExtendedLUNSupport) {
			CCB->TagEnable = true;
			CCB->QueueTag = QueueTag;
		} else {
			CCB->LegacyTagEnable = true;
			CCB->LegacyQueueTag = QueueTag;
		}
	}
	memcpy(CCB->CDB, CDB, CDB_Length);
	CCB->SenseDataLength = SCSI_SENSE_BUFFERSIZE;
	CCB->SenseDataPointer = pci_map_single(HostAdapter->PCI_Device, Command->sense_buffer, CCB->SenseDataLength, PCI_DMA_FROMDEVICE);
	CCB->Command = Command;
	Command->scsi_done = CompletionRoutine;
	if (BusLogic_MultiMasterHostAdapterP(HostAdapter)) {
		if (!BusLogic_WriteOutgoingMailbox(HostAdapter, BusLogic_MailboxStartCommand, CCB)) {
			spin_unlock_irq(HostAdapter->SCSI_Host->host_lock);
			BusLogic_Warning("Unable to write Outgoing Mailbox - " "Pausing for 1 second\n", HostAdapter);
			BusLogic_Delay(1);
			spin_lock_irq(HostAdapter->SCSI_Host->host_lock);
			if (!BusLogic_WriteOutgoingMailbox(HostAdapter, BusLogic_MailboxStartCommand, CCB)) {
				BusLogic_Warning("Still unable to write Outgoing Mailbox - " "Host Adapter Dead?\n", HostAdapter);
				BusLogic_DeallocateCCB(CCB);
				Command->result = DID_ERROR << 16;
				Command->scsi_done(Command);
			}
		}
	} else {
		CCB->Status = BusLogic_CCB_Active;
		HostAdapter->ActiveCommands[TargetID]++;
		TargetStatistics[TargetID].CommandsAttempted++;
		FlashPoint_StartCCB(HostAdapter->CardHandle, CCB);
		if (CCB->Status == BusLogic_CCB_Completed)
			BusLogic_ProcessCompletedCCBs(HostAdapter);
	}
	return 0;
}

static DEF_SCSI_QCMD(BusLogic_QueueCommand)

#if 0

static int BusLogic_AbortCommand(struct scsi_cmnd *Command)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Command->device->host->hostdata;

	int TargetID = Command->device->id;
	struct BusLogic_CCB *CCB;
	BusLogic_IncrementErrorCounter(&HostAdapter->TargetStatistics[TargetID].CommandAbortsRequested);
	for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
		if (CCB->Command == Command)
			break;
	if (CCB == NULL) {
		BusLogic_Warning("Unable to Abort Command to Target %d - " "No CCB Found\n", HostAdapter, TargetID);
		return SUCCESS;
	} else if (CCB->Status == BusLogic_CCB_Completed) {
		BusLogic_Warning("Unable to Abort Command to Target %d - " "CCB Completed\n", HostAdapter, TargetID);
		return SUCCESS;
	} else if (CCB->Status == BusLogic_CCB_Reset) {
		BusLogic_Warning("Unable to Abort Command to Target %d - " "CCB Reset\n", HostAdapter, TargetID);
		return SUCCESS;
	}
	if (BusLogic_MultiMasterHostAdapterP(HostAdapter)) {
		if (HostAdapter->TargetFlags[TargetID].TaggedQueuingActive && HostAdapter->FirmwareVersion[0] < '5') {
			BusLogic_Warning("Unable to Abort CCB #%ld to Target %d - " "Abort Tag Not Supported\n", HostAdapter, CCB->SerialNumber, TargetID);
			return FAILURE;
		} else if (BusLogic_WriteOutgoingMailbox(HostAdapter, BusLogic_MailboxAbortCommand, CCB)) {
			BusLogic_Warning("Aborting CCB #%ld to Target %d\n", HostAdapter, CCB->SerialNumber, TargetID);
			BusLogic_IncrementErrorCounter(&HostAdapter->TargetStatistics[TargetID].CommandAbortsAttempted);
			return SUCCESS;
		} else {
			BusLogic_Warning("Unable to Abort CCB #%ld to Target %d - " "No Outgoing Mailboxes\n", HostAdapter, CCB->SerialNumber, TargetID);
			return FAILURE;
		}
	} else {
		BusLogic_Warning("Aborting CCB #%ld to Target %d\n", HostAdapter, CCB->SerialNumber, TargetID);
		BusLogic_IncrementErrorCounter(&HostAdapter->TargetStatistics[TargetID].CommandAbortsAttempted);
		FlashPoint_AbortCCB(HostAdapter->CardHandle, CCB);
		if (CCB->Status == BusLogic_CCB_Completed) {
			BusLogic_ProcessCompletedCCBs(HostAdapter);
		}
		return SUCCESS;
	}
	return SUCCESS;
}

#endif

static int BusLogic_ResetHostAdapter(struct BusLogic_HostAdapter *HostAdapter, bool HardReset)
{
	struct BusLogic_CCB *CCB;
	int TargetID;


	if (!(BusLogic_HardwareResetHostAdapter(HostAdapter, HardReset) && BusLogic_InitializeHostAdapter(HostAdapter))) {
		BusLogic_Error("Resetting %s Failed\n", HostAdapter, HostAdapter->FullModelName);
		return FAILURE;
	}


	for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
		if (CCB->Status == BusLogic_CCB_Active)
			BusLogic_DeallocateCCB(CCB);

	if (HardReset) {
		spin_unlock_irq(HostAdapter->SCSI_Host->host_lock);
		BusLogic_Delay(HostAdapter->BusSettleTime);
		spin_lock_irq(HostAdapter->SCSI_Host->host_lock);
	}

	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		HostAdapter->LastResetAttempted[TargetID] = jiffies;
		HostAdapter->LastResetCompleted[TargetID] = jiffies;
	}
	return SUCCESS;
}


static int BusLogic_BIOSDiskParameters(struct scsi_device *sdev, struct block_device *Device, sector_t capacity, int *Parameters)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) sdev->host->hostdata;
	struct BIOS_DiskParameters *DiskParameters = (struct BIOS_DiskParameters *) Parameters;
	unsigned char *buf;
	if (HostAdapter->ExtendedTranslationEnabled && capacity >= 2 * 1024 * 1024  ) {
		if (capacity >= 4 * 1024 * 1024  ) {
			DiskParameters->Heads = 255;
			DiskParameters->Sectors = 63;
		} else {
			DiskParameters->Heads = 128;
			DiskParameters->Sectors = 32;
		}
	} else {
		DiskParameters->Heads = 64;
		DiskParameters->Sectors = 32;
	}
	DiskParameters->Cylinders = (unsigned long) capacity / (DiskParameters->Heads * DiskParameters->Sectors);
	buf = scsi_bios_ptable(Device);
	if (buf == NULL)
		return 0;
	if (*(unsigned short *) (buf + 64) == 0xAA55) {
		struct partition *FirstPartitionEntry = (struct partition *) buf;
		struct partition *PartitionEntry = FirstPartitionEntry;
		int SavedCylinders = DiskParameters->Cylinders, PartitionNumber;
		unsigned char PartitionEntryEndHead = 0, PartitionEntryEndSector = 0;
		for (PartitionNumber = 0; PartitionNumber < 4; PartitionNumber++) {
			PartitionEntryEndHead = PartitionEntry->end_head;
			PartitionEntryEndSector = PartitionEntry->end_sector & 0x3F;
			if (PartitionEntryEndHead == 64 - 1) {
				DiskParameters->Heads = 64;
				DiskParameters->Sectors = 32;
				break;
			} else if (PartitionEntryEndHead == 128 - 1) {
				DiskParameters->Heads = 128;
				DiskParameters->Sectors = 32;
				break;
			} else if (PartitionEntryEndHead == 255 - 1) {
				DiskParameters->Heads = 255;
				DiskParameters->Sectors = 63;
				break;
			}
			PartitionEntry++;
		}
		if (PartitionNumber == 4) {
			PartitionEntryEndHead = FirstPartitionEntry->end_head;
			PartitionEntryEndSector = FirstPartitionEntry->end_sector & 0x3F;
		}
		DiskParameters->Cylinders = (unsigned long) capacity / (DiskParameters->Heads * DiskParameters->Sectors);
		if (PartitionNumber < 4 && PartitionEntryEndSector == DiskParameters->Sectors) {
			if (DiskParameters->Cylinders != SavedCylinders)
				BusLogic_Warning("Adopting Geometry %d/%d from Partition Table\n", HostAdapter, DiskParameters->Heads, DiskParameters->Sectors);
		} else if (PartitionEntryEndHead > 0 || PartitionEntryEndSector > 0) {
			BusLogic_Warning("Warning: Partition Table appears to " "have Geometry %d/%d which is\n", HostAdapter, PartitionEntryEndHead + 1, PartitionEntryEndSector);
			BusLogic_Warning("not compatible with current BusLogic " "Host Adapter Geometry %d/%d\n", HostAdapter, DiskParameters->Heads, DiskParameters->Sectors);
		}
	}
	kfree(buf);
	return 0;
}



static int BusLogic_ProcDirectoryInfo(struct Scsi_Host *shost, char *ProcBuffer, char **StartPointer, off_t Offset, int BytesAvailable, int WriteFlag)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) shost->hostdata;
	struct BusLogic_TargetStatistics *TargetStatistics;
	int TargetID, Length;
	char *Buffer;

	TargetStatistics = HostAdapter->TargetStatistics;
	if (WriteFlag) {
		HostAdapter->ExternalHostAdapterResets = 0;
		HostAdapter->HostAdapterInternalErrors = 0;
		memset(TargetStatistics, 0, BusLogic_MaxTargetDevices * sizeof(struct BusLogic_TargetStatistics));
		return 0;
	}
	Buffer = HostAdapter->MessageBuffer;
	Length = HostAdapter->MessageBufferLength;
	Length += sprintf(&Buffer[Length], "\n\
Current Driver Queue Depth:	%d\n\
Currently Allocated CCBs:	%d\n", HostAdapter->DriverQueueDepth, HostAdapter->AllocatedCCBs);
	Length += sprintf(&Buffer[Length], "\n\n\
			   DATA TRANSFER STATISTICS\n\
\n\
Target	Tagged Queuing	Queue Depth  Active  Attempted	Completed\n\
======	==============	===========  ======  =========	=========\n");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[TargetID];
		if (!TargetFlags->TargetExists)
			continue;
		Length += sprintf(&Buffer[Length], "  %2d	%s", TargetID, (TargetFlags->TaggedQueuingSupported ? (TargetFlags->TaggedQueuingActive ? "    Active" : (HostAdapter->TaggedQueuingPermitted & (1 << TargetID)
																				    ? "  Permitted" : "   Disabled"))
									  : "Not Supported"));
		Length += sprintf(&Buffer[Length],
				  "	    %3d       %3u    %9u	%9u\n", HostAdapter->QueueDepth[TargetID], HostAdapter->ActiveCommands[TargetID], TargetStatistics[TargetID].CommandsAttempted, TargetStatistics[TargetID].CommandsCompleted);
	}
	Length += sprintf(&Buffer[Length], "\n\
Target  Read Commands  Write Commands   Total Bytes Read    Total Bytes Written\n\
======  =============  ==============  ===================  ===================\n");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[TargetID];
		if (!TargetFlags->TargetExists)
			continue;
		Length += sprintf(&Buffer[Length], "  %2d	  %9u	 %9u", TargetID, TargetStatistics[TargetID].ReadCommands, TargetStatistics[TargetID].WriteCommands);
		if (TargetStatistics[TargetID].TotalBytesRead.Billions > 0)
			Length += sprintf(&Buffer[Length], "     %9u%09u", TargetStatistics[TargetID].TotalBytesRead.Billions, TargetStatistics[TargetID].TotalBytesRead.Units);
		else
			Length += sprintf(&Buffer[Length], "		%9u", TargetStatistics[TargetID].TotalBytesRead.Units);
		if (TargetStatistics[TargetID].TotalBytesWritten.Billions > 0)
			Length += sprintf(&Buffer[Length], "   %9u%09u\n", TargetStatistics[TargetID].TotalBytesWritten.Billions, TargetStatistics[TargetID].TotalBytesWritten.Units);
		else
			Length += sprintf(&Buffer[Length], "	     %9u\n", TargetStatistics[TargetID].TotalBytesWritten.Units);
	}
	Length += sprintf(&Buffer[Length], "\n\
Target  Command    0-1KB      1-2KB      2-4KB      4-8KB     8-16KB\n\
======  =======  =========  =========  =========  =========  =========\n");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[TargetID];
		if (!TargetFlags->TargetExists)
			continue;
		Length +=
		    sprintf(&Buffer[Length],
			    "  %2d	 Read	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
			    TargetStatistics[TargetID].ReadCommandSizeBuckets[0],
			    TargetStatistics[TargetID].ReadCommandSizeBuckets[1], TargetStatistics[TargetID].ReadCommandSizeBuckets[2], TargetStatistics[TargetID].ReadCommandSizeBuckets[3], TargetStatistics[TargetID].ReadCommandSizeBuckets[4]);
		Length +=
		    sprintf(&Buffer[Length],
			    "  %2d	 Write	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
			    TargetStatistics[TargetID].WriteCommandSizeBuckets[0],
			    TargetStatistics[TargetID].WriteCommandSizeBuckets[1], TargetStatistics[TargetID].WriteCommandSizeBuckets[2], TargetStatistics[TargetID].WriteCommandSizeBuckets[3], TargetStatistics[TargetID].WriteCommandSizeBuckets[4]);
	}
	Length += sprintf(&Buffer[Length], "\n\
Target  Command   16-32KB    32-64KB   64-128KB   128-256KB   256KB+\n\
======  =======  =========  =========  =========  =========  =========\n");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[TargetID];
		if (!TargetFlags->TargetExists)
			continue;
		Length +=
		    sprintf(&Buffer[Length],
			    "  %2d	 Read	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
			    TargetStatistics[TargetID].ReadCommandSizeBuckets[5],
			    TargetStatistics[TargetID].ReadCommandSizeBuckets[6], TargetStatistics[TargetID].ReadCommandSizeBuckets[7], TargetStatistics[TargetID].ReadCommandSizeBuckets[8], TargetStatistics[TargetID].ReadCommandSizeBuckets[9]);
		Length +=
		    sprintf(&Buffer[Length],
			    "  %2d	 Write	 %9u  %9u  %9u  %9u  %9u\n", TargetID,
			    TargetStatistics[TargetID].WriteCommandSizeBuckets[5],
			    TargetStatistics[TargetID].WriteCommandSizeBuckets[6], TargetStatistics[TargetID].WriteCommandSizeBuckets[7], TargetStatistics[TargetID].WriteCommandSizeBuckets[8], TargetStatistics[TargetID].WriteCommandSizeBuckets[9]);
	}
	Length += sprintf(&Buffer[Length], "\n\n\
			   ERROR RECOVERY STATISTICS\n\
\n\
	  Command Aborts      Bus Device Resets	  Host Adapter Resets\n\
Target	Requested Completed  Requested Completed  Requested Completed\n\
  ID	\\\\\\\\ Attempted ////  \\\\\\\\ Attempted ////  \\\\\\\\ Attempted ////\n\
======	 ===== ===== =====    ===== ===== =====	   ===== ===== =====\n");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		struct BusLogic_TargetFlags *TargetFlags = &HostAdapter->TargetFlags[TargetID];
		if (!TargetFlags->TargetExists)
			continue;
		Length += sprintf(&Buffer[Length], "\
  %2d	 %5d %5d %5d    %5d %5d %5d	   %5d %5d %5d\n", TargetID, TargetStatistics[TargetID].CommandAbortsRequested, TargetStatistics[TargetID].CommandAbortsAttempted, TargetStatistics[TargetID].CommandAbortsCompleted, TargetStatistics[TargetID].BusDeviceResetsRequested, TargetStatistics[TargetID].BusDeviceResetsAttempted, TargetStatistics[TargetID].BusDeviceResetsCompleted, TargetStatistics[TargetID].HostAdapterResetsRequested, TargetStatistics[TargetID].HostAdapterResetsAttempted, TargetStatistics[TargetID].HostAdapterResetsCompleted);
	}
	Length += sprintf(&Buffer[Length], "\nExternal Host Adapter Resets: %d\n", HostAdapter->ExternalHostAdapterResets);
	Length += sprintf(&Buffer[Length], "Host Adapter Internal Errors: %d\n", HostAdapter->HostAdapterInternalErrors);
	if (Length >= BusLogic_MessageBufferSize)
		BusLogic_Error("Message Buffer length %d exceeds size %d\n", HostAdapter, Length, BusLogic_MessageBufferSize);
	if ((Length -= Offset) <= 0)
		return 0;
	if (Length >= BytesAvailable)
		Length = BytesAvailable;
	memcpy(ProcBuffer, HostAdapter->MessageBuffer + Offset, Length);
	*StartPointer = ProcBuffer;
	return Length;
}



static void BusLogic_Message(enum BusLogic_MessageLevel MessageLevel, char *Format, struct BusLogic_HostAdapter *HostAdapter, ...)
{
	static char Buffer[BusLogic_LineBufferSize];
	static bool BeginningOfLine = true;
	va_list Arguments;
	int Length = 0;
	va_start(Arguments, HostAdapter);
	Length = vsprintf(Buffer, Format, Arguments);
	va_end(Arguments);
	if (MessageLevel == BusLogic_AnnounceLevel) {
		static int AnnouncementLines = 0;
		strcpy(&HostAdapter->MessageBuffer[HostAdapter->MessageBufferLength], Buffer);
		HostAdapter->MessageBufferLength += Length;
		if (++AnnouncementLines <= 2)
			printk("%sscsi: %s", BusLogic_MessageLevelMap[MessageLevel], Buffer);
	} else if (MessageLevel == BusLogic_InfoLevel) {
		strcpy(&HostAdapter->MessageBuffer[HostAdapter->MessageBufferLength], Buffer);
		HostAdapter->MessageBufferLength += Length;
		if (BeginningOfLine) {
			if (Buffer[0] != '\n' || Length > 1)
				printk("%sscsi%d: %s", BusLogic_MessageLevelMap[MessageLevel], HostAdapter->HostNumber, Buffer);
		} else
			printk("%s", Buffer);
	} else {
		if (BeginningOfLine) {
			if (HostAdapter != NULL && HostAdapter->HostAdapterInitialized)
				printk("%sscsi%d: %s", BusLogic_MessageLevelMap[MessageLevel], HostAdapter->HostNumber, Buffer);
			else
				printk("%s%s", BusLogic_MessageLevelMap[MessageLevel], Buffer);
		} else
			printk("%s", Buffer);
	}
	BeginningOfLine = (Buffer[Length - 1] == '\n');
}



static bool __init BusLogic_ParseKeyword(char **StringPointer, char *Keyword)
{
	char *Pointer = *StringPointer;
	while (*Keyword != '\0') {
		char StringChar = *Pointer++;
		char KeywordChar = *Keyword++;
		if (StringChar >= 'A' && StringChar <= 'Z')
			StringChar += 'a' - 'Z';
		if (KeywordChar >= 'A' && KeywordChar <= 'Z')
			KeywordChar += 'a' - 'Z';
		if (StringChar != KeywordChar)
			return false;
	}
	*StringPointer = Pointer;
	return true;
}



static int __init BusLogic_ParseDriverOptions(char *OptionsString)
{
	while (true) {
		struct BusLogic_DriverOptions *DriverOptions = &BusLogic_DriverOptions[BusLogic_DriverOptionsCount++];
		int TargetID;
		memset(DriverOptions, 0, sizeof(struct BusLogic_DriverOptions));
		while (*OptionsString != '\0' && *OptionsString != ';') {
			
			if (BusLogic_ParseKeyword(&OptionsString, "IO:")) {
				unsigned long IO_Address = simple_strtoul(OptionsString, &OptionsString, 0);
				BusLogic_ProbeOptions.LimitedProbeISA = true;
				switch (IO_Address) {
				case 0x330:
					BusLogic_ProbeOptions.Probe330 = true;
					break;
				case 0x334:
					BusLogic_ProbeOptions.Probe334 = true;
					break;
				case 0x230:
					BusLogic_ProbeOptions.Probe230 = true;
					break;
				case 0x234:
					BusLogic_ProbeOptions.Probe234 = true;
					break;
				case 0x130:
					BusLogic_ProbeOptions.Probe130 = true;
					break;
				case 0x134:
					BusLogic_ProbeOptions.Probe134 = true;
					break;
				default:
					BusLogic_Error("BusLogic: Invalid Driver Options " "(invalid I/O Address 0x%X)\n", NULL, IO_Address);
					return 0;
				}
			} else if (BusLogic_ParseKeyword(&OptionsString, "NoProbeISA"))
				BusLogic_ProbeOptions.NoProbeISA = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "NoProbePCI"))
				BusLogic_ProbeOptions.NoProbePCI = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "NoProbe"))
				BusLogic_ProbeOptions.NoProbe = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "NoSortPCI"))
				BusLogic_ProbeOptions.NoSortPCI = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "MultiMasterFirst"))
				BusLogic_ProbeOptions.MultiMasterFirst = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "FlashPointFirst"))
				BusLogic_ProbeOptions.FlashPointFirst = true;
			
			else if (BusLogic_ParseKeyword(&OptionsString, "QueueDepth:[") || BusLogic_ParseKeyword(&OptionsString, "QD:[")) {
				for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++) {
					unsigned short QueueDepth = simple_strtoul(OptionsString, &OptionsString, 0);
					if (QueueDepth > BusLogic_MaxTaggedQueueDepth) {
						BusLogic_Error("BusLogic: Invalid Driver Options " "(invalid Queue Depth %d)\n", NULL, QueueDepth);
						return 0;
					}
					DriverOptions->QueueDepth[TargetID] = QueueDepth;
					if (*OptionsString == ',')
						OptionsString++;
					else if (*OptionsString == ']')
						break;
					else {
						BusLogic_Error("BusLogic: Invalid Driver Options " "(',' or ']' expected at '%s')\n", NULL, OptionsString);
						return 0;
					}
				}
				if (*OptionsString != ']') {
					BusLogic_Error("BusLogic: Invalid Driver Options " "(']' expected at '%s')\n", NULL, OptionsString);
					return 0;
				} else
					OptionsString++;
			} else if (BusLogic_ParseKeyword(&OptionsString, "QueueDepth:") || BusLogic_ParseKeyword(&OptionsString, "QD:")) {
				unsigned short QueueDepth = simple_strtoul(OptionsString, &OptionsString, 0);
				if (QueueDepth == 0 || QueueDepth > BusLogic_MaxTaggedQueueDepth) {
					BusLogic_Error("BusLogic: Invalid Driver Options " "(invalid Queue Depth %d)\n", NULL, QueueDepth);
					return 0;
				}
				DriverOptions->CommonQueueDepth = QueueDepth;
				for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
					DriverOptions->QueueDepth[TargetID] = QueueDepth;
			} else if (BusLogic_ParseKeyword(&OptionsString, "TaggedQueuing:") || BusLogic_ParseKeyword(&OptionsString, "TQ:")) {
				if (BusLogic_ParseKeyword(&OptionsString, "Default")) {
					DriverOptions->TaggedQueuingPermitted = 0x0000;
					DriverOptions->TaggedQueuingPermittedMask = 0x0000;
				} else if (BusLogic_ParseKeyword(&OptionsString, "Enable")) {
					DriverOptions->TaggedQueuingPermitted = 0xFFFF;
					DriverOptions->TaggedQueuingPermittedMask = 0xFFFF;
				} else if (BusLogic_ParseKeyword(&OptionsString, "Disable")) {
					DriverOptions->TaggedQueuingPermitted = 0x0000;
					DriverOptions->TaggedQueuingPermittedMask = 0xFFFF;
				} else {
					unsigned short TargetBit;
					for (TargetID = 0, TargetBit = 1; TargetID < BusLogic_MaxTargetDevices; TargetID++, TargetBit <<= 1)
						switch (*OptionsString++) {
						case 'Y':
							DriverOptions->TaggedQueuingPermitted |= TargetBit;
							DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
							break;
						case 'N':
							DriverOptions->TaggedQueuingPermitted &= ~TargetBit;
							DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
							break;
						case 'X':
							break;
						default:
							OptionsString--;
							TargetID = BusLogic_MaxTargetDevices;
							break;
						}
				}
			}
			
			else if (BusLogic_ParseKeyword(&OptionsString, "BusSettleTime:") || BusLogic_ParseKeyword(&OptionsString, "BST:")) {
				unsigned short BusSettleTime = simple_strtoul(OptionsString, &OptionsString, 0);
				if (BusSettleTime > 5 * 60) {
					BusLogic_Error("BusLogic: Invalid Driver Options " "(invalid Bus Settle Time %d)\n", NULL, BusSettleTime);
					return 0;
				}
				DriverOptions->BusSettleTime = BusSettleTime;
			} else if (BusLogic_ParseKeyword(&OptionsString, "InhibitTargetInquiry"))
				DriverOptions->LocalOptions.InhibitTargetInquiry = true;
			
			else if (BusLogic_ParseKeyword(&OptionsString, "TraceProbe"))
				BusLogic_GlobalOptions.TraceProbe = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "TraceHardwareReset"))
				BusLogic_GlobalOptions.TraceHardwareReset = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "TraceConfiguration"))
				BusLogic_GlobalOptions.TraceConfiguration = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "TraceErrors"))
				BusLogic_GlobalOptions.TraceErrors = true;
			else if (BusLogic_ParseKeyword(&OptionsString, "Debug")) {
				BusLogic_GlobalOptions.TraceProbe = true;
				BusLogic_GlobalOptions.TraceHardwareReset = true;
				BusLogic_GlobalOptions.TraceConfiguration = true;
				BusLogic_GlobalOptions.TraceErrors = true;
			}
			if (*OptionsString == ',')
				OptionsString++;
			else if (*OptionsString != ';' && *OptionsString != '\0') {
				BusLogic_Error("BusLogic: Unexpected Driver Option '%s' " "ignored\n", NULL, OptionsString);
				*OptionsString = '\0';
			}
		}
		if (!(BusLogic_DriverOptionsCount == 0 || BusLogic_ProbeInfoCount == 0 || BusLogic_DriverOptionsCount == BusLogic_ProbeInfoCount)) {
			BusLogic_Error("BusLogic: Invalid Driver Options " "(all or no I/O Addresses must be specified)\n", NULL);
			return 0;
		}
		for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++)
			if (DriverOptions->QueueDepth[TargetID] == 1) {
				unsigned short TargetBit = 1 << TargetID;
				DriverOptions->TaggedQueuingPermitted &= ~TargetBit;
				DriverOptions->TaggedQueuingPermittedMask |= TargetBit;
			}
		if (*OptionsString == ';')
			OptionsString++;
		if (*OptionsString == '\0')
			return 0;
	}
	return 1;
}


static struct scsi_host_template Bus_Logic_template = {
	.module = THIS_MODULE,
	.proc_name = "BusLogic",
	.proc_info = BusLogic_ProcDirectoryInfo,
	.name = "BusLogic",
	.info = BusLogic_DriverInfo,
	.queuecommand = BusLogic_QueueCommand,
	.slave_configure = BusLogic_SlaveConfigure,
	.bios_param = BusLogic_BIOSDiskParameters,
	.eh_host_reset_handler = BusLogic_host_reset,
#if 0
	.eh_abort_handler = BusLogic_AbortCommand,
#endif
	.unchecked_isa_dma = 1,
	.max_sectors = 128,
	.use_clustering = ENABLE_CLUSTERING,
};


static int __init BusLogic_Setup(char *str)
{
	int ints[3];

	(void) get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] != 0) {
		BusLogic_Error("BusLogic: Obsolete Command Line Entry " "Format Ignored\n", NULL);
		return 0;
	}
	if (str == NULL || *str == '\0')
		return 0;
	return BusLogic_ParseDriverOptions(str);
}


static void __exit BusLogic_exit(void)
{
	struct BusLogic_HostAdapter *ha, *next;

	list_for_each_entry_safe(ha, next, &BusLogic_host_list, host_list)
		BusLogic_ReleaseHostAdapter(ha);
}

__setup("BusLogic=", BusLogic_Setup);

#ifdef MODULE
static struct pci_device_id BusLogic_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{ }
};
#endif
MODULE_DEVICE_TABLE(pci, BusLogic_pci_tbl);

module_init(BusLogic_init);
module_exit(BusLogic_exit);
