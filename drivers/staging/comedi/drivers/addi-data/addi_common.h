/*
 *  Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.
 *
 *	ADDI-DATA GmbH
 *	Dieselstrasse 3
 *	D-77833 Ottersweier
 *	Tel: +19(0)7223/9493-0
 *	Fax: +49(0)7223/9493-92
 *	http://www.addi-data.com
 *	info@addi-data.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/kmod.h>
#include <linux/uaccess.h>
#include "../../comedidev.h"
#include "addi_amcc_s5933.h"

#define ERROR	-1
#define SUCCESS	1

#define LOBYTE(W)	(unsigned char)((W) & 0xFF)
#define HIBYTE(W)	(unsigned char)(((W) >> 8) & 0xFF)
#define MAKEWORD(H, L)	(unsigned short)((L) | ((H) << 8))
#define LOWORD(W)	(unsigned short)((W) & 0xFFFF)
#define HIWORD(W)	(unsigned short)(((W) >> 16) & 0xFFFF)
#define MAKEDWORD(H, L)	(unsigned int)((L) | ((H) << 16))

#define ADDI_ENABLE		1
#define ADDI_DISABLE		0
#define APCI1710_SAVE_INTERRUPT	1

#define ADDIDATA_EEPROM		1
#define ADDIDATA_NO_EEPROM	0
#define ADDIDATA_93C76		"93C76"
#define ADDIDATA_S5920		"S5920"
#define ADDIDATA_S5933		"S5933"
#define ADDIDATA_9054		"9054"

#define ADDIDATA_ENABLE		1
#define ADDIDATA_DISABLE	0


struct addi_board {
	const char *pc_DriverName;	
	int i_VendorId;		
	int i_DeviceId;
	int i_IorangeBase0;
	int i_IorangeBase1;
	int i_IorangeBase2;	
	int i_IorangeBase3;	
	int i_PCIEeprom;	
	char *pc_EepromChip;	
	int i_NbrAiChannel;	
	int i_NbrAiChannelDiff;	
	int i_AiChannelList;	
	int i_NbrAoChannel;	
	int i_AiMaxdata;	
	int i_AoMaxdata;	
	const struct comedi_lrange *pr_AiRangelist;	
	const struct comedi_lrange *pr_AoRangelist;	

	int i_NbrDiChannel;	
	int i_NbrDoChannel;	
	int i_DoMaxdata;	

	int i_NbrTTLChannel;	
	const struct comedi_lrange *pr_TTLRangelist;	

	int i_Dma;		
	int i_Timer;		
	unsigned char b_AvailableConvertUnit;
	unsigned int ui_MinAcquisitiontimeNs;	
	unsigned int ui_MinDelaytimeNs;	

	
	void (*v_hwdrv_Interrupt)(int irq, void *d);
	int (*i_hwdrv_Reset)(struct comedi_device *dev);

	

	
	int (*i_hwdrv_InsnConfigAnalogInput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnReadAnalogInput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdrv_InsnWriteAnalogInput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdrv_InsnBitsAnalogInput)(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn,
					   unsigned int *data);
	int (*i_hwdrv_CommandTestAnalogInput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_cmd *cmd);
	int (*i_hwdrv_CommandAnalogInput)(struct comedi_device *dev,
					  struct comedi_subdevice *s);
	int (*i_hwdrv_CancelAnalogInput)(struct comedi_device *dev,
					 struct comedi_subdevice *s);

	
	int (*i_hwdrv_InsnConfigAnalogOutput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnWriteAnalogOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnBitsAnalogOutput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);

	
	int (*i_hwdrv_InsnConfigDigitalInput) (struct comedi_device *dev,
					       struct comedi_subdevice *s,
					       struct comedi_insn *insn,
					       unsigned int *data);
	int (*i_hwdrv_InsnReadDigitalInput) (struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnWriteDigitalInput) (struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnBitsDigitalInput) (struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);

	
	int (*i_hwdrv_InsnConfigDigitalOutput)(struct comedi_device *dev,
					       struct comedi_subdevice *s,
					       struct comedi_insn *insn,
					       unsigned int *data);
	int (*i_hwdrv_InsnWriteDigitalOutput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnBitsDigitalOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnReadDigitalOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);

	
	int (*i_hwdrv_InsnConfigTimer)(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdrv_InsnWriteTimer)(struct comedi_device *dev,
				      struct comedi_subdevice *s, struct comedi_insn *insn,
				      unsigned int *data);
	int (*i_hwdrv_InsnReadTimer)(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdrv_InsnBitsTimer)(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);

	
	int (*i_hwdr_ConfigInitTTLIO)(struct comedi_device *dev,
				      struct comedi_subdevice *s, struct comedi_insn *insn,
				      unsigned int *data);
	int (*i_hwdr_ReadTTLIOBits)(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdr_ReadTTLIOAllPortValue)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdr_WriteTTLIOChlOnOff)(struct comedi_device *dev,
					 struct comedi_subdevice *s,
					 struct comedi_insn *insn, unsigned int *data);
};


union str_ModuleInfo {
	
	struct {
		union {
			struct {
				unsigned char b_ModeRegister1;
				unsigned char b_ModeRegister2;
				unsigned char b_ModeRegister3;
				unsigned char b_ModeRegister4;
			} s_ByteModeRegister;
			unsigned int dw_ModeRegister1_2_3_4;
		} s_ModeRegister;

		struct {
			unsigned int b_IndexInit:1;
			unsigned int b_CounterInit:1;
			unsigned int b_ReferenceInit:1;
			unsigned int b_IndexInterruptOccur:1;
			unsigned int b_CompareLogicInit:1;
			unsigned int b_FrequencyMeasurementInit:1;
			unsigned int b_FrequencyMeasurementEnable:1;
		} s_InitFlag;

	} s_SiemensCounterInfo;

	
	struct {
		unsigned char b_SSIProfile;
		unsigned char b_PositionTurnLength;
		unsigned char b_TurnCptLength;
		unsigned char b_SSIInit;
	} s_SSICounterInfo;

	
	struct {
		unsigned char b_TTLInit;
		unsigned char b_PortConfiguration[4];
	} s_TTLIOInfo;

	
	struct {
		unsigned char b_DigitalInit;
		unsigned char b_ChannelAMode;
		unsigned char b_ChannelBMode;
		unsigned char b_OutputMemoryEnabled;
		unsigned int dw_OutputMemory;
	} s_DigitalIOInfo;

      
	
      

	struct {
		struct {
			unsigned char b_82X54Init;
			unsigned char b_InputClockSelection;
			unsigned char b_InputClockLevel;
			unsigned char b_OutputLevel;
			unsigned char b_HardwareGateLevel;
			unsigned int dw_ConfigurationWord;
		} s_82X54TimerInfo[3];
		unsigned char b_InterruptMask;
	} s_82X54ModuleInfo;

      
	
      

	struct {
		unsigned char b_ChronoInit;
		unsigned char b_InterruptMask;
		unsigned char b_PCIInputClock;
		unsigned char b_TimingUnit;
		unsigned char b_CycleMode;
		double d_TimingInterval;
		unsigned int dw_ConfigReg;
	} s_ChronoModuleInfo;

      
	
      

	struct {
		struct {
			unsigned char b_PulseEncoderInit;
		} s_PulseEncoderInfo[4];
		unsigned int dw_SetRegister;
		unsigned int dw_ControlRegister;
		unsigned int dw_StatusRegister;
	} s_PulseEncoderModuleInfo;

	
	struct {
		struct {
			unsigned char b_TorCounterInit;
			unsigned char b_TimingUnit;
			unsigned char b_InterruptEnable;
			double d_TimingInterval;
			unsigned int ul_RealTimingInterval;
		} s_TorCounterInfo[2];
		unsigned char b_PCIInputClock;
	} s_TorCounterModuleInfo;

	
	struct {
		struct {
			unsigned char b_PWMInit;
			unsigned char b_TimingUnit;
			unsigned char b_InterruptEnable;
			double d_LowTiming;
			double d_HighTiming;
			unsigned int ul_RealLowTiming;
			unsigned int ul_RealHighTiming;
		} s_PWMInfo[2];
		unsigned char b_ClockSelection;
	} s_PWMModuleInfo;

	
	struct {
		struct {
			unsigned char b_ETMEnable;
			unsigned char b_ETMInterrupt;
		} s_ETMInfo[2];
		unsigned char b_ETMInit;
		unsigned char b_TimingUnit;
		unsigned char b_ClockSelection;
		double d_TimingInterval;
		unsigned int ul_Timing;
	} s_ETMModuleInfo;

	
	struct {
		unsigned char b_CDAEnable;
		unsigned char b_CDAInterrupt;
		unsigned char b_CDAInit;
		unsigned char b_FctSelection;
		unsigned char b_CDAReadFIFOOverflow;
	} s_CDAModuleInfo;

};

struct addi_private {

	int iobase;
	int i_IobaseAmcc;	
	int i_IobaseAddon;	
	int i_IobaseReserved;
	void __iomem *dw_AiBase;
	struct pcilst_struct *amcc;	
	unsigned char allocated;		
	unsigned char b_ValidDriver;	
	unsigned char b_AiContinuous;	
	unsigned char b_AiInitialisation;
	unsigned int ui_AiActualScan;	
	unsigned int ui_AiBufferPtr;	
	unsigned int ui_AiNbrofChannels;	
	unsigned int ui_AiScanLength;	
	unsigned int ui_AiActualScanPosition;	
	unsigned int *pui_AiChannelList;	
	unsigned int ui_AiChannelList[32];	
	unsigned char b_AiChannelConfiguration[32];	
	unsigned int ui_AiReadData[32];
	unsigned int dw_AiInitialised;
	unsigned int ui_AiTimer0;	
	unsigned int ui_AiTimer1;	
	unsigned int ui_AiFlags;
	unsigned int ui_AiDataLength;
	short *AiData;	
	unsigned int ui_AiNbrofScans;	
	unsigned short us_UseDma;	
	unsigned char b_DmaDoubleBuffer;	
	unsigned int ui_DmaActualBuffer;	
	
	
	short *ul_DmaBufferVirtual[2];	
	unsigned int ul_DmaBufferHw[2];	
	unsigned int ui_DmaBufferSize[2];	
	unsigned int ui_DmaBufferUsesize[2];	
	unsigned int ui_DmaBufferSamples[2];	
	unsigned int ui_DmaBufferPages[2];	
	unsigned char b_DigitalOutputRegister;	
	unsigned char b_OutputMemoryStatus;
	unsigned char b_AnalogInputChannelNbr;	
	unsigned char b_AnalogOutputChannelNbr;	
	unsigned char b_TimerSelectMode;	/*  Contain data written at iobase + 0C */
	unsigned char b_ModeSelectRegister;	/*  Contain data written at iobase + 0E */
	unsigned short us_OutputRegister;	/*  Contain data written at iobase + 0 */
	unsigned char b_InterruptState;
	unsigned char b_TimerInit;	
	unsigned char b_TimerStarted;	
	unsigned char b_Timer2Mode;	
	unsigned char b_Timer2Interrupt;	
	unsigned char b_AiCyclicAcquisition;	
	unsigned char b_InterruptMode;	
	unsigned char b_EocEosInterrupt;	
	unsigned int ui_EocEosConversionTime;
	unsigned char b_EocEosConversionTimeBase;
	unsigned char b_SingelDiff;
	unsigned char b_ExttrigEnable;	

	
	struct task_struct *tsk_Current;

	
	struct {
		unsigned int ui_Address;	
		unsigned int ui_FlashAddress;
		unsigned char b_InterruptNbr;	
		unsigned char b_SlotNumber;	
		unsigned char b_BoardVersion;
		unsigned int dw_MolduleConfiguration[4];	
	} s_BoardInfos;

	
	struct {
		unsigned int ul_InterruptOccur;	
						
		unsigned int ui_Read;	
		unsigned int ui_Write;	
		struct {
			unsigned char b_OldModuleMask;
			unsigned int ul_OldInterruptMask;	
			unsigned int ul_OldCounterLatchValue;	
		} s_FIFOInterruptParameters[APCI1710_SAVE_INTERRUPT];
	} s_InterruptParameters;

	union str_ModuleInfo s_ModuleInfo[4];
	unsigned int ul_TTLPortConfiguration[10];

	
	struct {
		int i_NbrAiChannel;	
		int i_NbrAoChannel;	
		int i_AiMaxdata;	
		int i_AoMaxdata;	
		int i_NbrDiChannel;	
		int i_NbrDoChannel;	
		int i_DoMaxdata;	
		int i_Dma;		
		int i_Timer;		
		unsigned int ui_MinAcquisitiontimeNs;
					
		unsigned int ui_MinDelaytimeNs;
					
	} s_EeParameters;
};

static unsigned short pci_list_builded;	

static int i_ADDI_Attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int i_ADDI_Detach(struct comedi_device *dev);
static int i_ADDI_Reset(struct comedi_device *dev);

static irqreturn_t v_ADDI_Interrupt(int irq, void *d);
static int i_ADDIDATA_InsnReadEeprom(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);
