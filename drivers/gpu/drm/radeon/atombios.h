/*
 * Copyright 2006-2007 Advanced Micro Devices, Inc.  
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


	


#ifndef _ATOMBIOS_H
#define _ATOMBIOS_H

#define ATOM_VERSION_MAJOR                   0x00020000
#define ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | ATOM_VERSION_MINOR)

#ifndef ATOM_BIG_ENDIAN
#error Endian not specified
#endif

#ifdef _H2INC
  #ifndef ULONG 
    typedef unsigned long ULONG;
  #endif

  #ifndef UCHAR
    typedef unsigned char UCHAR;
  #endif

  #ifndef USHORT 
    typedef unsigned short USHORT;
  #endif
#endif
      
#define ATOM_DAC_A            0 
#define ATOM_DAC_B            1
#define ATOM_EXT_DAC          2

#define ATOM_CRTC1            0
#define ATOM_CRTC2            1
#define ATOM_CRTC3            2
#define ATOM_CRTC4            3
#define ATOM_CRTC5            4
#define ATOM_CRTC6            5
#define ATOM_CRTC_INVALID     0xFF

#define ATOM_DIGA             0
#define ATOM_DIGB             1

#define ATOM_PPLL1            0
#define ATOM_PPLL2            1
#define ATOM_DCPLL            2
#define ATOM_PPLL0            2
#define ATOM_EXT_PLL1         8
#define ATOM_EXT_PLL2         9
#define ATOM_EXT_CLOCK        10
#define ATOM_PPLL_INVALID     0xFF

#define ENCODER_REFCLK_SRC_P1PLL       0       
#define ENCODER_REFCLK_SRC_P2PLL       1
#define ENCODER_REFCLK_SRC_DCPLL       2
#define ENCODER_REFCLK_SRC_EXTCLK      3
#define ENCODER_REFCLK_SRC_INVALID     0xFF

#define ATOM_SCALER1          0
#define ATOM_SCALER2          1

#define ATOM_SCALER_DISABLE   0   
#define ATOM_SCALER_CENTER    1   
#define ATOM_SCALER_EXPANSION 2   
#define ATOM_SCALER_MULTI_EX  3   

#define ATOM_DISABLE          0
#define ATOM_ENABLE           1
#define ATOM_LCD_BLOFF                          (ATOM_DISABLE+2)
#define ATOM_LCD_BLON                           (ATOM_ENABLE+2)
#define ATOM_LCD_BL_BRIGHTNESS_CONTROL          (ATOM_ENABLE+3)
#define ATOM_LCD_SELFTEST_START									(ATOM_DISABLE+5)
#define ATOM_LCD_SELFTEST_STOP									(ATOM_ENABLE+5)
#define ATOM_ENCODER_INIT			                  (ATOM_DISABLE+7)
#define ATOM_INIT			                          (ATOM_DISABLE+7)
#define ATOM_GET_STATUS                         (ATOM_DISABLE+8)

#define ATOM_BLANKING         1
#define ATOM_BLANKING_OFF     0

#define ATOM_CURSOR1          0
#define ATOM_CURSOR2          1

#define ATOM_ICON1            0
#define ATOM_ICON2            1

#define ATOM_CRT1             0
#define ATOM_CRT2             1

#define ATOM_TV_NTSC          1
#define ATOM_TV_NTSCJ         2
#define ATOM_TV_PAL           3
#define ATOM_TV_PALM          4
#define ATOM_TV_PALCN         5
#define ATOM_TV_PALN          6
#define ATOM_TV_PAL60         7
#define ATOM_TV_SECAM         8
#define ATOM_TV_CV            16

#define ATOM_DAC1_PS2         1
#define ATOM_DAC1_CV          2
#define ATOM_DAC1_NTSC        3
#define ATOM_DAC1_PAL         4

#define ATOM_DAC2_PS2         ATOM_DAC1_PS2
#define ATOM_DAC2_CV          ATOM_DAC1_CV
#define ATOM_DAC2_NTSC        ATOM_DAC1_NTSC
#define ATOM_DAC2_PAL         ATOM_DAC1_PAL
 
#define ATOM_PM_ON            0
#define ATOM_PM_STANDBY       1
#define ATOM_PM_SUSPEND       2
#define ATOM_PM_OFF           3


#define ATOM_PANEL_MISC_DUAL               0x00000001
#define ATOM_PANEL_MISC_888RGB             0x00000002
#define ATOM_PANEL_MISC_GREY_LEVEL         0x0000000C
#define ATOM_PANEL_MISC_FPDI               0x00000010
#define ATOM_PANEL_MISC_GREY_LEVEL_SHIFT   2
#define ATOM_PANEL_MISC_SPATIAL            0x00000020
#define ATOM_PANEL_MISC_TEMPORAL           0x00000040
#define ATOM_PANEL_MISC_API_ENABLED        0x00000080


#define MEMTYPE_DDR1              "DDR1"
#define MEMTYPE_DDR2              "DDR2"
#define MEMTYPE_DDR3              "DDR3"
#define MEMTYPE_DDR4              "DDR4"

#define ASIC_BUS_TYPE_PCI         "PCI"
#define ASIC_BUS_TYPE_AGP         "AGP"
#define ASIC_BUS_TYPE_PCIE        "PCI_EXPRESS"


#define ATOM_FIREGL_FLAG_STRING     "FGL"             
#define ATOM_MAX_SIZE_OF_FIREGL_FLAG_STRING  3        

#define ATOM_FAKE_DESKTOP_STRING    "DSK"             
#define ATOM_MAX_SIZE_OF_FAKE_DESKTOP_STRING  ATOM_MAX_SIZE_OF_FIREGL_FLAG_STRING 

#define ATOM_M54T_FLAG_STRING       "M54T"            
#define ATOM_MAX_SIZE_OF_M54T_FLAG_STRING    4        

#define HW_ASSISTED_I2C_STATUS_FAILURE          2
#define HW_ASSISTED_I2C_STATUS_SUCCESS          1

#pragma pack(1)                                       


#define OFFSET_TO_POINTER_TO_ATOM_ROM_HEADER		0x00000048L
#define OFFSET_TO_ATOM_ROM_IMAGE_SIZE				    0x00000002L

#define OFFSET_TO_ATOMBIOS_ASIC_BUS_MEM_TYPE    0x94
#define MAXSIZE_OF_ATOMBIOS_ASIC_BUS_MEM_TYPE   20    
#define	OFFSET_TO_GET_ATOMBIOS_STRINGS_NUMBER		0x002f
#define	OFFSET_TO_GET_ATOMBIOS_STRINGS_START		0x006e


typedef struct _ATOM_COMMON_TABLE_HEADER
{
  USHORT usStructureSize;
  UCHAR  ucTableFormatRevision;   
  UCHAR  ucTableContentRevision;  
                                  
}ATOM_COMMON_TABLE_HEADER;

	
	
typedef struct _ATOM_ROM_HEADER
{
  ATOM_COMMON_TABLE_HEADER		sHeader;
  UCHAR	 uaFirmWareSignature[4];    
  USHORT usBiosRuntimeSegmentAddress;
  USHORT usProtectedModeInfoOffset;
  USHORT usConfigFilenameOffset;
  USHORT usCRC_BlockOffset;
  USHORT usBIOS_BootupMessageOffset;
  USHORT usInt10Offset;
  USHORT usPciBusDevInitCode;
  USHORT usIoBaseAddress;
  USHORT usSubsystemVendorID;
  USHORT usSubsystemID;
  USHORT usPCI_InfoOffset; 
  USHORT usMasterCommandTableOffset; 
  USHORT usMasterDataTableOffset;   
  UCHAR  ucExtendedFunctionCode;
  UCHAR  ucReserved;
}ATOM_ROM_HEADER;


#ifdef	UEFI_BUILD
	#define	UTEMP	USHORT
	#define	USHORT	void*
#endif

	
	
typedef struct _ATOM_MASTER_LIST_OF_COMMAND_TABLES{
  USHORT ASIC_Init;                              
  USHORT GetDisplaySurfaceSize;                  
  USHORT ASIC_RegistersInit;                     
  USHORT VRAM_BlockVenderDetection;              
  USHORT DIGxEncoderControl;										 
  USHORT MemoryControllerInit;                   
  USHORT EnableCRTCMemReq;                       
  USHORT MemoryParamAdjust; 										 
  USHORT DVOEncoderControl;                      
  USHORT GPIOPinControl;												 
  USHORT SetEngineClock;                         
  USHORT SetMemoryClock;                         
  USHORT SetPixelClock;                          
  USHORT EnableDispPowerGating;                  
  USHORT ResetMemoryDLL;                         
  USHORT ResetMemoryDevice;                      
  USHORT MemoryPLLInit;                          
  USHORT AdjustDisplayPll;											 
  USHORT AdjustMemoryController;                 
  USHORT EnableASIC_StaticPwrMgt;                
  USHORT ASIC_StaticPwrMgtStatusChange;          
  USHORT DAC_LoadDetection;                      
  USHORT LVTMAEncoderControl;                    
  USHORT HW_Misc_Operation;                      
  USHORT DAC1EncoderControl;                     
  USHORT DAC2EncoderControl;                     
  USHORT DVOOutputControl;                       
  USHORT CV1OutputControl;                       
  USHORT GetConditionalGoldenSetting;            
  USHORT TVEncoderControl;                       
  USHORT PatchMCSetting;                         
  USHORT MC_SEQ_Control;                         
  USHORT TV1OutputControl;                       
  USHORT EnableScaler;                           
  USHORT BlankCRTC;                              
  USHORT EnableCRTC;                             
  USHORT GetPixelClock;                          
  USHORT EnableVGA_Render;                       
  USHORT GetSCLKOverMCLKRatio;                   
  USHORT SetCRTC_Timing;                         
  USHORT SetCRTC_OverScan;                       
  USHORT SetCRTC_Replication;                    
  USHORT SelectCRTC_Source;                      
  USHORT EnableGraphSurfaces;                    
  USHORT UpdateCRTC_DoubleBufferRegisters;			 
  USHORT LUT_AutoFill;                           
  USHORT EnableHW_IconCursor;                    
  USHORT GetMemoryClock;                         
  USHORT GetEngineClock;                         
  USHORT SetCRTC_UsingDTDTiming;                 
  USHORT ExternalEncoderControl;                 
  USHORT LVTMAOutputControl;                     
  USHORT VRAM_BlockDetectionByStrap;             
  USHORT MemoryCleanUp;                          
  USHORT ProcessI2cChannelTransaction;           
  USHORT WriteOneByteToHWAssistedI2C;            
  USHORT ReadHWAssistedI2CStatus;                
  USHORT SpeedFanControl;                        
  USHORT PowerConnectorDetection;                
  USHORT MC_Synchronization;                     
  USHORT ComputeMemoryEnginePLL;                 
  USHORT MemoryRefreshConversion;                
  USHORT VRAM_GetCurrentInfoBlock;               
  USHORT DynamicMemorySettings;                  
  USHORT MemoryTraining;                         
  USHORT EnableSpreadSpectrumOnPPLL;             
  USHORT TMDSAOutputControl;                     
  USHORT SetVoltage;                             
  USHORT DAC1OutputControl;                      
  USHORT DAC2OutputControl;                      
  USHORT ComputeMemoryClockParam;                
  USHORT ClockSource;                            
  USHORT MemoryDeviceInit;                       
  USHORT GetDispObjectInfo;                      
  USHORT DIG1EncoderControl;                     
  USHORT DIG2EncoderControl;                     
  USHORT DIG1TransmitterControl;                 
  USHORT DIG2TransmitterControl;	               
  USHORT ProcessAuxChannelTransaction;					 
  USHORT DPEncoderService;											 
  USHORT GetVoltageInfo;                         
}ATOM_MASTER_LIST_OF_COMMAND_TABLES;   

#define ReadEDIDFromHWAssistedI2C                ProcessI2cChannelTransaction
#define DPTranslatorControl                      DIG2EncoderControl
#define UNIPHYTransmitterControl			     DIG1TransmitterControl
#define LVTMATransmitterControl				     DIG2TransmitterControl
#define SetCRTC_DPM_State                        GetConditionalGoldenSetting
#define SetUniphyInstance                        ASIC_StaticPwrMgtStatusChange
#define HPDInterruptService                      ReadHWAssistedI2CStatus
#define EnableVGA_Access                         GetSCLKOverMCLKRatio
#define EnableYUV                                GetDispObjectInfo                         
#define DynamicClockGating                       EnableDispPowerGating
#define SetupHWAssistedI2CStatus                 ComputeMemoryClockParam

#define TMDSAEncoderControl                      PatchMCSetting
#define LVDSEncoderControl                       MC_SEQ_Control
#define LCD1OutputControl                        HW_Misc_Operation


typedef struct _ATOM_MASTER_COMMAND_TABLE
{
  ATOM_COMMON_TABLE_HEADER           sHeader;
  ATOM_MASTER_LIST_OF_COMMAND_TABLES ListOfCommandTables;
}ATOM_MASTER_COMMAND_TABLE;

	
	
typedef struct _ATOM_TABLE_ATTRIBUTE
{
#if ATOM_BIG_ENDIAN
  USHORT  UpdatedByUtility:1;         
  USHORT  PS_SizeInBytes:7;           
  USHORT  WS_SizeInBytes:8;           
#else
  USHORT  WS_SizeInBytes:8;           
  USHORT  PS_SizeInBytes:7;           
  USHORT  UpdatedByUtility:1;         
#endif
}ATOM_TABLE_ATTRIBUTE;

typedef union _ATOM_TABLE_ATTRIBUTE_ACCESS
{
  ATOM_TABLE_ATTRIBUTE sbfAccess;
  USHORT               susAccess;
}ATOM_TABLE_ATTRIBUTE_ACCESS;

	
	
typedef struct _ATOM_COMMON_ROM_COMMAND_TABLE_HEADER
{
  ATOM_COMMON_TABLE_HEADER CommonHeader;
  ATOM_TABLE_ATTRIBUTE     TableAttribute;	
}ATOM_COMMON_ROM_COMMAND_TABLE_HEADER;

	
	
#define COMPUTE_MEMORY_PLL_PARAM        1
#define COMPUTE_ENGINE_PLL_PARAM        2
#define ADJUST_MC_SETTING_PARAM         3

	
	
typedef struct _ATOM_ADJUST_MEMORY_CLOCK_FREQ
{
#if ATOM_BIG_ENDIAN
  ULONG ulPointerReturnFlag:1;      
  ULONG ulMemoryModuleNumber:7;     
  ULONG ulClockFreq:24;
#else
  ULONG ulClockFreq:24;
  ULONG ulMemoryModuleNumber:7;     
  ULONG ulPointerReturnFlag:1;      
#endif
}ATOM_ADJUST_MEMORY_CLOCK_FREQ;
#define POINTER_RETURN_FLAG             0x80

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS
{
  ULONG   ulClock;        
  UCHAR   ucAction;       
  UCHAR   ucReserved;     
  UCHAR   ucFbDiv;        
  UCHAR   ucPostDiv;      
}COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V2
{
  ULONG   ulClock;        
  UCHAR   ucAction;       //0:reserved;COMPUTE_MEMORY_PLL_PARAM:Memory;COMPUTE_ENGINE_PLL_PARAM:Engine. it return ref_div to be written to register
  USHORT  usFbDiv;		    //return Feedback value to be written to register
  UCHAR   ucPostDiv;      //return post div to be written to register
}COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V2;
#define COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS


#define SET_CLOCK_FREQ_MASK                     0x00FFFFFF  
#define USE_NON_BUS_CLOCK_MASK                  0x01000000  
#define USE_MEMORY_SELF_REFRESH_MASK            0x02000000	
#define SKIP_INTERNAL_MEMORY_PARAMETER_CHANGE   0x04000000  
#define FIRST_TIME_CHANGE_CLOCK									0x08000000	
#define SKIP_SW_PROGRAM_PLL											0x10000000	
#define USE_SS_ENABLED_PIXEL_CLOCK  USE_NON_BUS_CLOCK_MASK

#define b3USE_NON_BUS_CLOCK_MASK                  0x01       
#define b3USE_MEMORY_SELF_REFRESH                 0x02	     
#define b3SKIP_INTERNAL_MEMORY_PARAMETER_CHANGE   0x04       
#define b3FIRST_TIME_CHANGE_CLOCK									0x08       
#define b3SKIP_SW_PROGRAM_PLL											0x10			 

typedef struct _ATOM_COMPUTE_CLOCK_FREQ
{
#if ATOM_BIG_ENDIAN
  ULONG ulComputeClockFlag:8;                 
  ULONG ulClockFreq:24;                       
#else
  ULONG ulClockFreq:24;                       
  ULONG ulComputeClockFlag:8;                 
#endif
}ATOM_COMPUTE_CLOCK_FREQ;

typedef struct _ATOM_S_MPLL_FB_DIVIDER
{
  USHORT usFbDivFrac;  
  USHORT usFbDiv;  
}ATOM_S_MPLL_FB_DIVIDER;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V3
{
  union
  {
    ATOM_COMPUTE_CLOCK_FREQ  ulClock;         
    ATOM_S_MPLL_FB_DIVIDER   ulFbDiv;         
  };
  UCHAR   ucRefDiv;                           
  UCHAR   ucPostDiv;                          
  UCHAR   ucCntlFlag;                         
  UCHAR   ucReserved;
}COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V3;

#define ATOM_PLL_CNTL_FLAG_PLL_POST_DIV_EN          1
#define ATOM_PLL_CNTL_FLAG_MPLL_VCO_MODE            2
#define ATOM_PLL_CNTL_FLAG_FRACTION_DISABLE         4
#define ATOM_PLL_CNTL_FLAG_SPLL_ISPARE_9						8


typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V4
{
#if ATOM_BIG_ENDIAN
  ULONG  ucPostDiv;          
  ULONG  ulClock:24;         
#else
  ULONG  ulClock:24;         
  ULONG  ucPostDiv;          
#endif
}COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V4;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V5
{
  union
  {
    ATOM_COMPUTE_CLOCK_FREQ  ulClock;         
    ATOM_S_MPLL_FB_DIVIDER   ulFbDiv;         
  };
  UCHAR   ucRefDiv;                           
  UCHAR   ucPostDiv;                          
  union
  {
    UCHAR   ucCntlFlag;                       
    UCHAR   ucInputFlag;                      
  };
  UCHAR   ucReserved;                       
}COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V5;

#define ATOM_PLL_INPUT_FLAG_PLL_STROBE_MODE_EN  1   

typedef struct _COMPUTE_MEMORY_CLOCK_PARAM_PARAMETERS_V2_1
{
  union
  {
    ULONG  ulClock;         
    ATOM_S_MPLL_FB_DIVIDER   ulFbDiv;         
  };
  UCHAR   ucDllSpeed;                         
  UCHAR   ucPostDiv;                          
  union{
    UCHAR   ucInputFlag;                      
    UCHAR   ucPllCntlFlag;                    
  };
  UCHAR   ucBWCntl;                       
}COMPUTE_MEMORY_CLOCK_PARAM_PARAMETERS_V2_1;

#define MPLL_INPUT_FLAG_STROBE_MODE_EN          0x01
#define MPLL_CNTL_FLAG_VCO_MODE_MASK            0x03 
#define MPLL_CNTL_FLAG_BYPASS_DQ_PLL            0x04
#define MPLL_CNTL_FLAG_QDR_ENABLE               0x08
#define MPLL_CNTL_FLAG_AD_HALF_RATE             0x10

#define MPLL_CNTL_FLAG_BYPASS_AD_PLL            0x04

typedef struct _DYNAMICE_MEMORY_SETTINGS_PARAMETER
{
  ATOM_COMPUTE_CLOCK_FREQ ulClock;
  ULONG ulReserved[2];
}DYNAMICE_MEMORY_SETTINGS_PARAMETER;

typedef struct _DYNAMICE_ENGINE_SETTINGS_PARAMETER
{
  ATOM_COMPUTE_CLOCK_FREQ ulClock;
  ULONG ulMemoryClock;
  ULONG ulReserved;
}DYNAMICE_ENGINE_SETTINGS_PARAMETER;

	
	
typedef struct _SET_ENGINE_CLOCK_PARAMETERS
{
  ULONG ulTargetEngineClock;          
}SET_ENGINE_CLOCK_PARAMETERS;

typedef struct _SET_ENGINE_CLOCK_PS_ALLOCATION
{
  ULONG ulTargetEngineClock;          
  COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION sReserved;
}SET_ENGINE_CLOCK_PS_ALLOCATION;

	
	
typedef struct _SET_MEMORY_CLOCK_PARAMETERS
{
  ULONG ulTargetMemoryClock;          
}SET_MEMORY_CLOCK_PARAMETERS;

typedef struct _SET_MEMORY_CLOCK_PS_ALLOCATION
{
  ULONG ulTargetMemoryClock;          
  COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION sReserved;
}SET_MEMORY_CLOCK_PS_ALLOCATION;

	
	
typedef struct _ASIC_INIT_PARAMETERS
{
  ULONG ulDefaultEngineClock;         
  ULONG ulDefaultMemoryClock;         
}ASIC_INIT_PARAMETERS;

typedef struct _ASIC_INIT_PS_ALLOCATION
{
  ASIC_INIT_PARAMETERS sASICInitClocks;
  SET_ENGINE_CLOCK_PS_ALLOCATION sReserved; 
}ASIC_INIT_PS_ALLOCATION;

	
	
typedef struct _DYNAMIC_CLOCK_GATING_PARAMETERS 
{
  UCHAR ucEnable;                     
  UCHAR ucPadding[3];
}DYNAMIC_CLOCK_GATING_PARAMETERS;
#define  DYNAMIC_CLOCK_GATING_PS_ALLOCATION  DYNAMIC_CLOCK_GATING_PARAMETERS

	
	
typedef struct _ENABLE_DISP_POWER_GATING_PARAMETERS_V2_1 
{
  UCHAR ucDispPipeId;                 
  UCHAR ucEnable;                     
  UCHAR ucPadding[2];
}ENABLE_DISP_POWER_GATING_PARAMETERS_V2_1;

	
	
typedef struct _ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS
{
  UCHAR ucEnable;                     
  UCHAR ucPadding[3];
}ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS;
#define ENABLE_ASIC_STATIC_PWR_MGT_PS_ALLOCATION  ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS

	
	
typedef struct _DAC_LOAD_DETECTION_PARAMETERS
{
  USHORT usDeviceID;                  
  UCHAR  ucDacType;                   
  UCHAR  ucMisc;											
}DAC_LOAD_DETECTION_PARAMETERS;

#define DAC_LOAD_MISC_YPrPb						0x01

typedef struct _DAC_LOAD_DETECTION_PS_ALLOCATION
{
  DAC_LOAD_DETECTION_PARAMETERS            sDacload;
  ULONG                                    Reserved[2];
}DAC_LOAD_DETECTION_PS_ALLOCATION;

	
	
typedef struct _DAC_ENCODER_CONTROL_PARAMETERS 
{
  USHORT usPixelClock;                
  UCHAR  ucDacStandard;               
  UCHAR  ucAction;                    
                                      
                                      
}DAC_ENCODER_CONTROL_PARAMETERS;

#define DAC_ENCODER_CONTROL_PS_ALLOCATION  DAC_ENCODER_CONTROL_PARAMETERS

	
	
typedef struct _DIG_ENCODER_CONTROL_PARAMETERS
{
  USHORT usPixelClock;		
  UCHAR  ucConfig;		  
                            
                            
                            
                            
                            
                            
                            
  UCHAR ucAction;           
                            
  UCHAR ucEncoderMode;
                            
                            
                            
                            
                            
  UCHAR ucLaneNum;          
  UCHAR ucReserved[2];
}DIG_ENCODER_CONTROL_PARAMETERS;
#define DIG_ENCODER_CONTROL_PS_ALLOCATION			  DIG_ENCODER_CONTROL_PARAMETERS
#define EXTERNAL_ENCODER_CONTROL_PARAMETER			DIG_ENCODER_CONTROL_PARAMETERS

#define ATOM_ENCODER_CONFIG_DPLINKRATE_MASK				0x01
#define ATOM_ENCODER_CONFIG_DPLINKRATE_1_62GHZ		0x00
#define ATOM_ENCODER_CONFIG_DPLINKRATE_2_70GHZ		0x01
#define ATOM_ENCODER_CONFIG_DPLINKRATE_5_40GHZ		0x02
#define ATOM_ENCODER_CONFIG_LINK_SEL_MASK				  0x04
#define ATOM_ENCODER_CONFIG_LINKA								  0x00
#define ATOM_ENCODER_CONFIG_LINKB								  0x04
#define ATOM_ENCODER_CONFIG_LINKA_B							  ATOM_TRANSMITTER_CONFIG_LINKA
#define ATOM_ENCODER_CONFIG_LINKB_A							  ATOM_ENCODER_CONFIG_LINKB
#define ATOM_ENCODER_CONFIG_TRANSMITTER_SEL_MASK	0x08
#define ATOM_ENCODER_CONFIG_UNIPHY							  0x00
#define ATOM_ENCODER_CONFIG_LVTMA								  0x08
#define ATOM_ENCODER_CONFIG_TRANSMITTER1				  0x00
#define ATOM_ENCODER_CONFIG_TRANSMITTER2				  0x08
#define ATOM_ENCODER_CONFIG_DIGB								  0x80			

#define ATOM_ENCODER_MODE_DP											0
#define ATOM_ENCODER_MODE_LVDS										1
#define ATOM_ENCODER_MODE_DVI											2
#define ATOM_ENCODER_MODE_HDMI										3
#define ATOM_ENCODER_MODE_SDVO										4
#define ATOM_ENCODER_MODE_DP_AUDIO                5
#define ATOM_ENCODER_MODE_TV											13
#define ATOM_ENCODER_MODE_CV											14
#define ATOM_ENCODER_MODE_CRT											15
#define ATOM_ENCODER_MODE_DVO											16
#define ATOM_ENCODER_MODE_DP_SST                  ATOM_ENCODER_MODE_DP    
#define ATOM_ENCODER_MODE_DP_MST                  5                       

typedef struct _ATOM_DIG_ENCODER_CONFIG_V2
{
#if ATOM_BIG_ENDIAN
    UCHAR ucReserved1:2;
    UCHAR ucTransmitterSel:2;     
    UCHAR ucLinkSel:1;            
    UCHAR ucReserved:1;
    UCHAR ucDPLinkRate:1;         
#else
    UCHAR ucDPLinkRate:1;         
    UCHAR ucReserved:1;
    UCHAR ucLinkSel:1;            
    UCHAR ucTransmitterSel:2;     
    UCHAR ucReserved1:2;
#endif
}ATOM_DIG_ENCODER_CONFIG_V2;


typedef struct _DIG_ENCODER_CONTROL_PARAMETERS_V2
{
  USHORT usPixelClock;      
  ATOM_DIG_ENCODER_CONFIG_V2 acConfig;
  UCHAR ucAction;                                       
  UCHAR ucEncoderMode;
                            
                            
                            
                            
                            
  UCHAR ucLaneNum;          
  UCHAR ucStatus;           
  UCHAR ucReserved;
}DIG_ENCODER_CONTROL_PARAMETERS_V2;

#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_MASK				0x01
#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_1_62GHZ		  0x00
#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_2_70GHZ		  0x01
#define ATOM_ENCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#define ATOM_ENCODER_CONFIG_V2_LINKA								  0x00
#define ATOM_ENCODER_CONFIG_V2_LINKB								  0x04
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER_SEL_MASK	  0x18
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER1				    0x00
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER2				    0x08
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER3				    0x10

#define ATOM_ENCODER_CMD_DP_LINK_TRAINING_START       0x08
#define ATOM_ENCODER_CMD_DP_LINK_TRAINING_PATTERN1    0x09
#define ATOM_ENCODER_CMD_DP_LINK_TRAINING_PATTERN2    0x0a
#define ATOM_ENCODER_CMD_DP_LINK_TRAINING_PATTERN3    0x13
#define ATOM_ENCODER_CMD_DP_LINK_TRAINING_COMPLETE    0x0b
#define ATOM_ENCODER_CMD_DP_VIDEO_OFF                 0x0c
#define ATOM_ENCODER_CMD_DP_VIDEO_ON                  0x0d
#define ATOM_ENCODER_CMD_QUERY_DP_LINK_TRAINING_STATUS    0x0e
#define ATOM_ENCODER_CMD_SETUP                        0x0f
#define ATOM_ENCODER_CMD_SETUP_PANEL_MODE             0x10

#define ATOM_ENCODER_STATUS_LINK_TRAINING_COMPLETE    0x10
#define ATOM_ENCODER_STATUS_LINK_TRAINING_INCOMPLETE  0x00

typedef struct _ATOM_DIG_ENCODER_CONFIG_V3
{
#if ATOM_BIG_ENDIAN
    UCHAR ucReserved1:1;
    UCHAR ucDigSel:3;             
    UCHAR ucReserved:3;
    UCHAR ucDPLinkRate:1;         
#else
    UCHAR ucDPLinkRate:1;         
    UCHAR ucReserved:3;
    UCHAR ucDigSel:3;             
    UCHAR ucReserved1:1;
#endif
}ATOM_DIG_ENCODER_CONFIG_V3;

#define ATOM_ENCODER_CONFIG_V3_DPLINKRATE_MASK				0x03
#define ATOM_ENCODER_CONFIG_V3_DPLINKRATE_1_62GHZ		  0x00
#define ATOM_ENCODER_CONFIG_V3_DPLINKRATE_2_70GHZ		  0x01
#define ATOM_ENCODER_CONFIG_V3_ENCODER_SEL					  0x70
#define ATOM_ENCODER_CONFIG_V3_DIG0_ENCODER					  0x00
#define ATOM_ENCODER_CONFIG_V3_DIG1_ENCODER					  0x10
#define ATOM_ENCODER_CONFIG_V3_DIG2_ENCODER					  0x20
#define ATOM_ENCODER_CONFIG_V3_DIG3_ENCODER					  0x30
#define ATOM_ENCODER_CONFIG_V3_DIG4_ENCODER					  0x40
#define ATOM_ENCODER_CONFIG_V3_DIG5_ENCODER					  0x50

typedef struct _DIG_ENCODER_CONTROL_PARAMETERS_V3
{
  USHORT usPixelClock;      
  ATOM_DIG_ENCODER_CONFIG_V3 acConfig;
  UCHAR ucAction;                              
  union {
    UCHAR ucEncoderMode;
                            
                            
                            
                            
                            
                            
    UCHAR ucPanelMode;      
	                    
	                    
	                    
  };
  UCHAR ucLaneNum;          
  UCHAR ucBitPerColor;      
  UCHAR ucReserved;
}DIG_ENCODER_CONTROL_PARAMETERS_V3;

typedef struct _ATOM_DIG_ENCODER_CONFIG_V4
{
#if ATOM_BIG_ENDIAN
    UCHAR ucReserved1:1;
    UCHAR ucDigSel:3;             
    UCHAR ucReserved:2;
    UCHAR ucDPLinkRate:2;         
#else
    UCHAR ucDPLinkRate:2;         
    UCHAR ucReserved:2;
    UCHAR ucDigSel:3;             
    UCHAR ucReserved1:1;
#endif
}ATOM_DIG_ENCODER_CONFIG_V4;

#define ATOM_ENCODER_CONFIG_V4_DPLINKRATE_MASK				0x03
#define ATOM_ENCODER_CONFIG_V4_DPLINKRATE_1_62GHZ		  0x00
#define ATOM_ENCODER_CONFIG_V4_DPLINKRATE_2_70GHZ		  0x01
#define ATOM_ENCODER_CONFIG_V4_DPLINKRATE_5_40GHZ		  0x02
#define ATOM_ENCODER_CONFIG_V4_DPLINKRATE_3_24GHZ		  0x03
#define ATOM_ENCODER_CONFIG_V4_ENCODER_SEL					  0x70
#define ATOM_ENCODER_CONFIG_V4_DIG0_ENCODER					  0x00
#define ATOM_ENCODER_CONFIG_V4_DIG1_ENCODER					  0x10
#define ATOM_ENCODER_CONFIG_V4_DIG2_ENCODER					  0x20
#define ATOM_ENCODER_CONFIG_V4_DIG3_ENCODER					  0x30
#define ATOM_ENCODER_CONFIG_V4_DIG4_ENCODER					  0x40
#define ATOM_ENCODER_CONFIG_V4_DIG5_ENCODER					  0x50
#define ATOM_ENCODER_CONFIG_V4_DIG6_ENCODER					  0x60

typedef struct _DIG_ENCODER_CONTROL_PARAMETERS_V4
{
  USHORT usPixelClock;      
  union{
  ATOM_DIG_ENCODER_CONFIG_V4 acConfig;
  UCHAR ucConfig;
  };
  UCHAR ucAction;                              
  union {
    UCHAR ucEncoderMode;
                            
                            
                            
                            
                            
                            
    UCHAR ucPanelMode;      
	                    
	                    
	                    
  };
  UCHAR ucLaneNum;          
  UCHAR ucBitPerColor;      
  UCHAR ucHPD_ID;           
}DIG_ENCODER_CONTROL_PARAMETERS_V4;

#define PANEL_BPC_UNDEFINE                               0x00
#define PANEL_6BIT_PER_COLOR                             0x01 
#define PANEL_8BIT_PER_COLOR                             0x02
#define PANEL_10BIT_PER_COLOR                            0x03
#define PANEL_12BIT_PER_COLOR                            0x04
#define PANEL_16BIT_PER_COLOR                            0x05

#define DP_PANEL_MODE_EXTERNAL_DP_MODE                   0x00
#define DP_PANEL_MODE_INTERNAL_DP2_MODE                  0x01
#define DP_PANEL_MODE_INTERNAL_DP1_MODE                  0x11

	
	
typedef struct _ATOM_DP_VS_MODE
{
  UCHAR ucLaneSel;
  UCHAR ucLaneSet;
}ATOM_DP_VS_MODE;

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS
{
	union
	{
  USHORT usPixelClock;		
	USHORT usInitInfo;			
  ATOM_DP_VS_MODE asMode; 
	};
  UCHAR ucConfig;
													
													
                          
													
													
  												
													
		  										
                          
                          
                          
                          
                          
	UCHAR ucAction;				  
	                        
  UCHAR ucReserved[4];
}DIG_TRANSMITTER_CONTROL_PARAMETERS;

#define DIG_TRANSMITTER_CONTROL_PS_ALLOCATION		DIG_TRANSMITTER_CONTROL_PARAMETERS					

#define ATOM_TRAMITTER_INITINFO_CONNECTOR_MASK	0x00ff			

#define ATOM_TRANSMITTER_CONFIG_8LANE_LINK			0x01
#define ATOM_TRANSMITTER_CONFIG_COHERENT				0x02
#define ATOM_TRANSMITTER_CONFIG_LINK_SEL_MASK		0x04
#define ATOM_TRANSMITTER_CONFIG_LINKA						0x00
#define ATOM_TRANSMITTER_CONFIG_LINKB						0x04
#define ATOM_TRANSMITTER_CONFIG_LINKA_B					0x00			
#define ATOM_TRANSMITTER_CONFIG_LINKB_A					0x04

#define ATOM_TRANSMITTER_CONFIG_ENCODER_SEL_MASK	0x08			
#define ATOM_TRANSMITTER_CONFIG_DIG1_ENCODER		0x00				
#define ATOM_TRANSMITTER_CONFIG_DIG2_ENCODER		0x08				

#define ATOM_TRANSMITTER_CONFIG_CLKSRC_MASK			0x30
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_PPLL			0x00
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_PCIE			0x20
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_XTALIN		0x30
#define ATOM_TRANSMITTER_CONFIG_LANE_SEL_MASK		0xc0
#define ATOM_TRANSMITTER_CONFIG_LANE_0_3				0x00
#define ATOM_TRANSMITTER_CONFIG_LANE_0_7				0x00
#define ATOM_TRANSMITTER_CONFIG_LANE_4_7				0x40
#define ATOM_TRANSMITTER_CONFIG_LANE_8_11				0x80
#define ATOM_TRANSMITTER_CONFIG_LANE_8_15				0x80
#define ATOM_TRANSMITTER_CONFIG_LANE_12_15			0xc0

#define ATOM_TRANSMITTER_ACTION_DISABLE					       0
#define ATOM_TRANSMITTER_ACTION_ENABLE					       1
#define ATOM_TRANSMITTER_ACTION_LCD_BLOFF				       2
#define ATOM_TRANSMITTER_ACTION_LCD_BLON				       3
#define ATOM_TRANSMITTER_ACTION_BL_BRIGHTNESS_CONTROL  4
#define ATOM_TRANSMITTER_ACTION_LCD_SELFTEST_START		 5
#define ATOM_TRANSMITTER_ACTION_LCD_SELFTEST_STOP			 6
#define ATOM_TRANSMITTER_ACTION_INIT						       7
#define ATOM_TRANSMITTER_ACTION_DISABLE_OUTPUT	       8
#define ATOM_TRANSMITTER_ACTION_ENABLE_OUTPUT		       9
#define ATOM_TRANSMITTER_ACTION_SETUP						       10
#define ATOM_TRANSMITTER_ACTION_SETUP_VSEMPH           11
#define ATOM_TRANSMITTER_ACTION_POWER_ON               12
#define ATOM_TRANSMITTER_ACTION_POWER_OFF              13

typedef struct _ATOM_DIG_TRANSMITTER_CONFIG_V2
{
#if ATOM_BIG_ENDIAN
  UCHAR ucTransmitterSel:2;         
                                    
                                    
  UCHAR ucReserved:1;               
  UCHAR fDPConnector:1;             
  UCHAR ucEncoderSel:1;             
  UCHAR ucLinkSel:1;                
                                    

  UCHAR fCoherentMode:1;            
  UCHAR fDualLinkConnector:1;       
#else
  UCHAR fDualLinkConnector:1;       
  UCHAR fCoherentMode:1;            
  UCHAR ucLinkSel:1;                
                                    
  UCHAR ucEncoderSel:1;             
  UCHAR fDPConnector:1;             
  UCHAR ucReserved:1;               
  UCHAR ucTransmitterSel:2;         
                                    
                                    
#endif
}ATOM_DIG_TRANSMITTER_CONFIG_V2;

#define ATOM_TRANSMITTER_CONFIG_V2_DUAL_LINK_CONNECTOR			0x01

#define ATOM_TRANSMITTER_CONFIG_V2_COHERENT				          0x02

#define ATOM_TRANSMITTER_CONFIG_V2_LINK_SEL_MASK		        0x04
#define ATOM_TRANSMITTER_CONFIG_V2_LINKA  			            0x00
#define ATOM_TRANSMITTER_CONFIG_V2_LINKB				            0x04

#define ATOM_TRANSMITTER_CONFIG_V2_ENCODER_SEL_MASK	        0x08
#define ATOM_TRANSMITTER_CONFIG_V2_DIG1_ENCODER		          0x00				
#define ATOM_TRANSMITTER_CONFIG_V2_DIG2_ENCODER		          0x08				

#define ATOM_TRASMITTER_CONFIG_V2_DP_CONNECTOR			        0x10

#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER_SEL_MASK     0xC0
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER1           	0x00	
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER2           	0x40	
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER3           	0x80	

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS_V2
{
	union
	{
  USHORT usPixelClock;		
	USHORT usInitInfo;			
  ATOM_DP_VS_MODE asMode; 
	};
  ATOM_DIG_TRANSMITTER_CONFIG_V2 acConfig;
	UCHAR ucAction;				  
  UCHAR ucReserved[4];
}DIG_TRANSMITTER_CONTROL_PARAMETERS_V2;

typedef struct _ATOM_DIG_TRANSMITTER_CONFIG_V3
{
#if ATOM_BIG_ENDIAN
  UCHAR ucTransmitterSel:2;         
                                    
                                    
  UCHAR ucRefClkSource:2;           
  UCHAR ucEncoderSel:1;             
  UCHAR ucLinkSel:1;                
                                    
  UCHAR fCoherentMode:1;            
  UCHAR fDualLinkConnector:1;       
#else
  UCHAR fDualLinkConnector:1;       
  UCHAR fCoherentMode:1;            
  UCHAR ucLinkSel:1;                
                                    
  UCHAR ucEncoderSel:1;             
  UCHAR ucRefClkSource:2;           
  UCHAR ucTransmitterSel:2;         
                                    
                                    
#endif
}ATOM_DIG_TRANSMITTER_CONFIG_V3;


typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS_V3
{
	union
	{
    USHORT usPixelClock;		
	  USHORT usInitInfo;			
    ATOM_DP_VS_MODE asMode; 
	};
  ATOM_DIG_TRANSMITTER_CONFIG_V3 acConfig;
	UCHAR ucAction;				    
  UCHAR ucLaneNum;
  UCHAR ucReserved[3];
}DIG_TRANSMITTER_CONTROL_PARAMETERS_V3;

#define ATOM_TRANSMITTER_CONFIG_V3_DUAL_LINK_CONNECTOR			0x01

#define ATOM_TRANSMITTER_CONFIG_V3_COHERENT				          0x02

#define ATOM_TRANSMITTER_CONFIG_V3_LINK_SEL_MASK		        0x04
#define ATOM_TRANSMITTER_CONFIG_V3_LINKA  			            0x00
#define ATOM_TRANSMITTER_CONFIG_V3_LINKB				            0x04

#define ATOM_TRANSMITTER_CONFIG_V3_ENCODER_SEL_MASK	        0x08
#define ATOM_TRANSMITTER_CONFIG_V3_DIG1_ENCODER		          0x00
#define ATOM_TRANSMITTER_CONFIG_V3_DIG2_ENCODER		          0x08

#define ATOM_TRASMITTER_CONFIG_V3_REFCLK_SEL_MASK 	        0x30
#define ATOM_TRASMITTER_CONFIG_V3_P1PLL          		        0x00
#define ATOM_TRASMITTER_CONFIG_V3_P2PLL		                  0x10
#define ATOM_TRASMITTER_CONFIG_V3_REFCLK_SRC_EXT            0x20

#define ATOM_TRANSMITTER_CONFIG_V3_TRANSMITTER_SEL_MASK     0xC0
#define ATOM_TRANSMITTER_CONFIG_V3_TRANSMITTER1           	0x00	
#define ATOM_TRANSMITTER_CONFIG_V3_TRANSMITTER2           	0x40	
#define ATOM_TRANSMITTER_CONFIG_V3_TRANSMITTER3           	0x80	


	
	
typedef struct _ATOM_DP_VS_MODE_V4
{
  UCHAR ucLaneSel;
 	union
 	{  
 	  UCHAR ucLaneSet;
 	  struct {
#if ATOM_BIG_ENDIAN
 		  UCHAR ucPOST_CURSOR2:2;         
 		  UCHAR ucPRE_EMPHASIS:3;         
 		  UCHAR ucVOLTAGE_SWING:3;        
#else
 		  UCHAR ucVOLTAGE_SWING:3;        
 		  UCHAR ucPRE_EMPHASIS:3;         
 		  UCHAR ucPOST_CURSOR2:2;         
#endif
 		};
 	}; 
}ATOM_DP_VS_MODE_V4;
 
typedef struct _ATOM_DIG_TRANSMITTER_CONFIG_V4
{
#if ATOM_BIG_ENDIAN
  UCHAR ucTransmitterSel:2;         
                                    
                                    
  UCHAR ucRefClkSource:2;           
  UCHAR ucEncoderSel:1;             
  UCHAR ucLinkSel:1;                
                                    
  UCHAR fCoherentMode:1;            
  UCHAR fDualLinkConnector:1;       
#else
  UCHAR fDualLinkConnector:1;       
  UCHAR fCoherentMode:1;            
  UCHAR ucLinkSel:1;                
                                    
  UCHAR ucEncoderSel:1;             
  UCHAR ucRefClkSource:2;           
  UCHAR ucTransmitterSel:2;         
                                    
                                    
#endif
}ATOM_DIG_TRANSMITTER_CONFIG_V4;

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS_V4
{
  union
  {
    USHORT usPixelClock;		
    USHORT usInitInfo;			
    ATOM_DP_VS_MODE_V4 asMode; 
  };
  union
  {
  ATOM_DIG_TRANSMITTER_CONFIG_V4 acConfig;
  UCHAR ucConfig;
  };
  UCHAR ucAction;				    
  UCHAR ucLaneNum;
  UCHAR ucReserved[3];
}DIG_TRANSMITTER_CONTROL_PARAMETERS_V4;

#define ATOM_TRANSMITTER_CONFIG_V4_DUAL_LINK_CONNECTOR			0x01
#define ATOM_TRANSMITTER_CONFIG_V4_COHERENT				          0x02
#define ATOM_TRANSMITTER_CONFIG_V4_LINK_SEL_MASK		        0x04
#define ATOM_TRANSMITTER_CONFIG_V4_LINKA  			            0x00			
#define ATOM_TRANSMITTER_CONFIG_V4_LINKB				            0x04
#define ATOM_TRANSMITTER_CONFIG_V4_ENCODER_SEL_MASK	        0x08
#define ATOM_TRANSMITTER_CONFIG_V4_DIG1_ENCODER		          0x00				 
#define ATOM_TRANSMITTER_CONFIG_V4_DIG2_ENCODER		          0x08				
#define ATOM_TRANSMITTER_CONFIG_V4_REFCLK_SEL_MASK 	        0x30
#define ATOM_TRANSMITTER_CONFIG_V4_P1PLL         		        0x00
#define ATOM_TRANSMITTER_CONFIG_V4_P2PLL		                0x10
#define ATOM_TRANSMITTER_CONFIG_V4_DCPLL		                0x20   
#define ATOM_TRANSMITTER_CONFIG_V4_REFCLK_SRC_EXT           0x30   
#define ATOM_TRANSMITTER_CONFIG_V4_TRANSMITTER_SEL_MASK     0xC0
#define ATOM_TRANSMITTER_CONFIG_V4_TRANSMITTER1           	0x00	
#define ATOM_TRANSMITTER_CONFIG_V4_TRANSMITTER2           	0x40	
#define ATOM_TRANSMITTER_CONFIG_V4_TRANSMITTER3           	0x80	


typedef struct _ATOM_DIG_TRANSMITTER_CONFIG_V5
{
#if ATOM_BIG_ENDIAN
  UCHAR ucReservd1:1;
  UCHAR ucHPDSel:3;
  UCHAR ucPhyClkSrcId:2;            
  UCHAR ucCoherentMode:1;            
  UCHAR ucReserved:1;
#else
  UCHAR ucReserved:1;
  UCHAR ucCoherentMode:1;            
  UCHAR ucPhyClkSrcId:2;            
  UCHAR ucHPDSel:3;
  UCHAR ucReservd1:1;
#endif
}ATOM_DIG_TRANSMITTER_CONFIG_V5;

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS_V1_5
{
  USHORT usSymClock;		        
  UCHAR  ucPhyId;                   
  UCHAR  ucAction;				    
  UCHAR  ucLaneNum;                 
  UCHAR  ucConnObjId;               
  UCHAR  ucDigMode;                 
  union{
  ATOM_DIG_TRANSMITTER_CONFIG_V5 asConfig;
  UCHAR ucConfig;
  };
  UCHAR  ucDigEncoderSel;           
  UCHAR  ucDPLaneSet;
  UCHAR  ucReserved;
  UCHAR  ucReserved1;
}DIG_TRANSMITTER_CONTROL_PARAMETERS_V1_5;

#define ATOM_PHY_ID_UNIPHYA                                 0  
#define ATOM_PHY_ID_UNIPHYB                                 1
#define ATOM_PHY_ID_UNIPHYC                                 2
#define ATOM_PHY_ID_UNIPHYD                                 3
#define ATOM_PHY_ID_UNIPHYE                                 4
#define ATOM_PHY_ID_UNIPHYF                                 5
#define ATOM_PHY_ID_UNIPHYG                                 6

#define ATOM_TRANMSITTER_V5__DIGA_SEL                       0x01
#define ATOM_TRANMSITTER_V5__DIGB_SEL                       0x02
#define ATOM_TRANMSITTER_V5__DIGC_SEL                       0x04
#define ATOM_TRANMSITTER_V5__DIGD_SEL                       0x08
#define ATOM_TRANMSITTER_V5__DIGE_SEL                       0x10
#define ATOM_TRANMSITTER_V5__DIGF_SEL                       0x20
#define ATOM_TRANMSITTER_V5__DIGG_SEL                       0x40

#define ATOM_TRANSMITTER_DIGMODE_V5_DP                      0
#define ATOM_TRANSMITTER_DIGMODE_V5_LVDS                    1
#define ATOM_TRANSMITTER_DIGMODE_V5_DVI                     2
#define ATOM_TRANSMITTER_DIGMODE_V5_HDMI                    3
#define ATOM_TRANSMITTER_DIGMODE_V5_SDVO                    4
#define ATOM_TRANSMITTER_DIGMODE_V5_DP_MST                  5

#define DP_LANE_SET__0DB_0_4V                               0x00
#define DP_LANE_SET__0DB_0_6V                               0x01
#define DP_LANE_SET__0DB_0_8V                               0x02
#define DP_LANE_SET__0DB_1_2V                               0x03
#define DP_LANE_SET__3_5DB_0_4V                             0x08  
#define DP_LANE_SET__3_5DB_0_6V                             0x09
#define DP_LANE_SET__3_5DB_0_8V                             0x0a
#define DP_LANE_SET__6DB_0_4V                               0x10
#define DP_LANE_SET__6DB_0_6V                               0x11
#define DP_LANE_SET__9_5DB_0_4V                             0x18  

#define ATOM_TRANSMITTER_CONFIG_V5_COHERENT				          0x02

#define ATOM_TRANSMITTER_CONFIG_V5_REFCLK_SEL_MASK 	        0x0c
#define ATOM_TRANSMITTER_CONFIG_V5_REFCLK_SEL_SHIFT		    0x02

#define ATOM_TRANSMITTER_CONFIG_V5_P1PLL         		        0x00
#define ATOM_TRANSMITTER_CONFIG_V5_P2PLL		                0x04
#define ATOM_TRANSMITTER_CONFIG_V5_P0PLL		                0x08   
#define ATOM_TRANSMITTER_CONFIG_V5_REFCLK_SRC_EXT           0x0c
#define ATOM_TRANSMITTER_CONFIG_V5_HPD_SEL_MASK		          0x70
#define ATOM_TRANSMITTER_CONFIG_V5_HPD_SEL_SHIFT		      0x04

#define ATOM_TRANSMITTER_CONFIG_V5_NO_HPD_SEL				        0x00
#define ATOM_TRANSMITTER_CONFIG_V5_HPD1_SEL				          0x10
#define ATOM_TRANSMITTER_CONFIG_V5_HPD2_SEL				          0x20
#define ATOM_TRANSMITTER_CONFIG_V5_HPD3_SEL				          0x30
#define ATOM_TRANSMITTER_CONFIG_V5_HPD4_SEL				          0x40
#define ATOM_TRANSMITTER_CONFIG_V5_HPD5_SEL				          0x50
#define ATOM_TRANSMITTER_CONFIG_V5_HPD6_SEL				          0x60

#define DIG_TRANSMITTER_CONTROL_PS_ALLOCATION_V1_5            DIG_TRANSMITTER_CONTROL_PARAMETERS_V1_5


	
	

typedef struct _EXTERNAL_ENCODER_CONTROL_PARAMETERS_V3
{
  union{
  USHORT usPixelClock;      
  USHORT usConnectorId;     
  };
  UCHAR  ucConfig;          
  UCHAR  ucAction;          
  UCHAR  ucEncoderMode;     
  UCHAR  ucLaneNum;         
  UCHAR  ucBitPerColor;     
  UCHAR  ucReserved;        
}EXTERNAL_ENCODER_CONTROL_PARAMETERS_V3;

#define EXTERNAL_ENCODER_ACTION_V3_DISABLE_OUTPUT         0x00
#define EXTERNAL_ENCODER_ACTION_V3_ENABLE_OUTPUT          0x01
#define EXTERNAL_ENCODER_ACTION_V3_ENCODER_INIT           0x07
#define EXTERNAL_ENCODER_ACTION_V3_ENCODER_SETUP          0x0f
#define EXTERNAL_ENCODER_ACTION_V3_ENCODER_BLANKING_OFF   0x10
#define EXTERNAL_ENCODER_ACTION_V3_ENCODER_BLANKING       0x11
#define EXTERNAL_ENCODER_ACTION_V3_DACLOAD_DETECTION      0x12
#define EXTERNAL_ENCODER_ACTION_V3_DDC_SETUP              0x14

#define EXTERNAL_ENCODER_CONFIG_V3_DPLINKRATE_MASK				0x03
#define EXTERNAL_ENCODER_CONFIG_V3_DPLINKRATE_1_62GHZ		  0x00
#define EXTERNAL_ENCODER_CONFIG_V3_DPLINKRATE_2_70GHZ		  0x01
#define EXTERNAL_ENCODER_CONFIG_V3_DPLINKRATE_5_40GHZ		  0x02
#define EXTERNAL_ENCODER_CONFIG_V3_ENCODER_SEL_MASK		    0x70
#define EXTERNAL_ENCODER_CONFIG_V3_ENCODER1		            0x00
#define EXTERNAL_ENCODER_CONFIG_V3_ENCODER2		            0x10
#define EXTERNAL_ENCODER_CONFIG_V3_ENCODER3		            0x20

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION_V3
{
  EXTERNAL_ENCODER_CONTROL_PARAMETERS_V3 sExtEncoder;
  ULONG ulReserved[2];
}EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION_V3;


	
	
typedef struct _DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
{
  UCHAR  ucAction;                    
                                      
                                      
                                      
                                      
  UCHAR  aucPadding[3];               
}DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS;

#define DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS


#define CRT1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS 
#define CRT1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define CRT2_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS 
#define CRT2_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define CV1_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define CV1_OUTPUT_CONTROL_PS_ALLOCATION   DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define TV1_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define TV1_OUTPUT_CONTROL_PS_ALLOCATION   DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DFP1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DFP1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DFP2_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DFP2_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define LCD1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define LCD1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DVO_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DVO_OUTPUT_CONTROL_PS_ALLOCATION   DIG_TRANSMITTER_CONTROL_PS_ALLOCATION
#define DVO_OUTPUT_CONTROL_PARAMETERS_V3	 DIG_TRANSMITTER_CONTROL_PARAMETERS

	
	
typedef struct _BLANK_CRTC_PARAMETERS
{
  UCHAR  ucCRTC;                    	
  UCHAR  ucBlanking;                  
  USHORT usBlackColorRCr;
  USHORT usBlackColorGY;
  USHORT usBlackColorBCb;
}BLANK_CRTC_PARAMETERS;
#define BLANK_CRTC_PS_ALLOCATION    BLANK_CRTC_PARAMETERS

	
	
typedef struct _ENABLE_CRTC_PARAMETERS
{
  UCHAR ucCRTC;                    	  
  UCHAR ucEnable;                     
  UCHAR ucPadding[2];
}ENABLE_CRTC_PARAMETERS;
#define ENABLE_CRTC_PS_ALLOCATION   ENABLE_CRTC_PARAMETERS

	
	
typedef struct _SET_CRTC_OVERSCAN_PARAMETERS
{
  USHORT usOverscanRight;             
  USHORT usOverscanLeft;              
  USHORT usOverscanBottom;            
  USHORT usOverscanTop;               
  UCHAR  ucCRTC;                      
  UCHAR  ucPadding[3];
}SET_CRTC_OVERSCAN_PARAMETERS;
#define SET_CRTC_OVERSCAN_PS_ALLOCATION  SET_CRTC_OVERSCAN_PARAMETERS

	
	
typedef struct _SET_CRTC_REPLICATION_PARAMETERS
{
  UCHAR ucH_Replication;              
  UCHAR ucV_Replication;              
  UCHAR usCRTC;                       
  UCHAR ucPadding;
}SET_CRTC_REPLICATION_PARAMETERS;
#define SET_CRTC_REPLICATION_PS_ALLOCATION  SET_CRTC_REPLICATION_PARAMETERS

	
	
typedef struct _SELECT_CRTC_SOURCE_PARAMETERS
{
  UCHAR ucCRTC;                    	  
  UCHAR ucDevice;                     
  UCHAR ucPadding[2];
}SELECT_CRTC_SOURCE_PARAMETERS;
#define SELECT_CRTC_SOURCE_PS_ALLOCATION  SELECT_CRTC_SOURCE_PARAMETERS

typedef struct _SELECT_CRTC_SOURCE_PARAMETERS_V2
{
  UCHAR ucCRTC;                    	  
  UCHAR ucEncoderID;                  
  UCHAR ucEncodeMode;									
  UCHAR ucPadding;
}SELECT_CRTC_SOURCE_PARAMETERS_V2;



	
	
typedef struct _PIXEL_CLOCK_PARAMETERS
{
  USHORT usPixelClock;                
                                      
  USHORT usRefDiv;                    
  USHORT usFbDiv;                     
  UCHAR  ucPostDiv;                   
  UCHAR  ucFracFbDiv;                 
  UCHAR  ucPpll;                      
  UCHAR  ucRefDivSrc;                 
  UCHAR  ucCRTC;                      
  UCHAR  ucPadding;
}PIXEL_CLOCK_PARAMETERS;

#define MISC_FORCE_REPROG_PIXEL_CLOCK 0x1
#define MISC_DEVICE_INDEX_MASK        0xF0
#define MISC_DEVICE_INDEX_SHIFT       4

typedef struct _PIXEL_CLOCK_PARAMETERS_V2
{
  USHORT usPixelClock;                
                                      
  USHORT usRefDiv;                    
  USHORT usFbDiv;                     
  UCHAR  ucPostDiv;                   
  UCHAR  ucFracFbDiv;                 
  UCHAR  ucPpll;                      
  UCHAR  ucRefDivSrc;                 
  UCHAR  ucCRTC;                      
  UCHAR  ucMiscInfo;                  
}PIXEL_CLOCK_PARAMETERS_V2;



#define PIXEL_CLOCK_MISC_FORCE_PROG_PPLL						0x01
#define PIXEL_CLOCK_MISC_VGA_MODE										0x02
#define PIXEL_CLOCK_MISC_CRTC_SEL_MASK							0x04
#define PIXEL_CLOCK_MISC_CRTC_SEL_CRTC1							0x00
#define PIXEL_CLOCK_MISC_CRTC_SEL_CRTC2							0x04
#define PIXEL_CLOCK_MISC_USE_ENGINE_FOR_DISPCLK			0x08
#define PIXEL_CLOCK_MISC_REF_DIV_SRC                    0x10
#define PIXEL_CLOCK_V4_MISC_SS_ENABLE               0x10
#define PIXEL_CLOCK_V4_MISC_COHERENT_MODE           0x20


typedef struct _PIXEL_CLOCK_PARAMETERS_V3
{
  USHORT usPixelClock;                
                                      
  USHORT usRefDiv;                    
  USHORT usFbDiv;                     
  UCHAR  ucPostDiv;                   
  UCHAR  ucFracFbDiv;                 
  UCHAR  ucPpll;                      
  UCHAR  ucTransmitterId;             
	union
	{
  UCHAR  ucEncoderMode;               
	UCHAR  ucDVOConfig;									
	};
  UCHAR  ucMiscInfo;                  
                                      
                                      
}PIXEL_CLOCK_PARAMETERS_V3;

#define PIXEL_CLOCK_PARAMETERS_LAST			PIXEL_CLOCK_PARAMETERS_V2
#define GET_PIXEL_CLOCK_PS_ALLOCATION		PIXEL_CLOCK_PARAMETERS_LAST

typedef struct _PIXEL_CLOCK_PARAMETERS_V5
{
  UCHAR  ucCRTC;             
                             
  union{
  UCHAR  ucReserved;
  UCHAR  ucFracFbDiv;        
  };
  USHORT usPixelClock;       
                             
  USHORT usFbDiv;            
  UCHAR  ucPostDiv;          
  UCHAR  ucRefDiv;           
  UCHAR  ucPpll;             
  UCHAR  ucTransmitterID;    
                             
  UCHAR  ucEncoderMode;      
  UCHAR  ucMiscInfo;         
                             
                             
                             
                             
	                           
                             
                             
  ULONG  ulFbDivDecFrac;     

}PIXEL_CLOCK_PARAMETERS_V5;

#define PIXEL_CLOCK_V5_MISC_FORCE_PROG_PPLL					0x01
#define PIXEL_CLOCK_V5_MISC_VGA_MODE								0x02
#define PIXEL_CLOCK_V5_MISC_HDMI_BPP_MASK           0x0c
#define PIXEL_CLOCK_V5_MISC_HDMI_24BPP              0x00
#define PIXEL_CLOCK_V5_MISC_HDMI_30BPP              0x04
#define PIXEL_CLOCK_V5_MISC_HDMI_32BPP              0x08
#define PIXEL_CLOCK_V5_MISC_REF_DIV_SRC             0x10

typedef struct _CRTC_PIXEL_CLOCK_FREQ
{
#if ATOM_BIG_ENDIAN
  ULONG  ucCRTC:8;            
                              
  ULONG  ulPixelClock:24;     
                              
#else
  ULONG  ulPixelClock:24;     
                              
  ULONG  ucCRTC:8;            
                              
#endif
}CRTC_PIXEL_CLOCK_FREQ;

typedef struct _PIXEL_CLOCK_PARAMETERS_V6
{
  union{
    CRTC_PIXEL_CLOCK_FREQ ulCrtcPclkFreq;    
    ULONG ulDispEngClkFreq;                  
  };
  USHORT usFbDiv;            
  UCHAR  ucPostDiv;          
  UCHAR  ucRefDiv;           
  UCHAR  ucPpll;             
  UCHAR  ucTransmitterID;    
                             
  UCHAR  ucEncoderMode;      
  UCHAR  ucMiscInfo;         
                             
                             
                             
                             
	                           
                             
                             
  ULONG  ulFbDivDecFrac;     

}PIXEL_CLOCK_PARAMETERS_V6;

#define PIXEL_CLOCK_V6_MISC_FORCE_PROG_PPLL					0x01
#define PIXEL_CLOCK_V6_MISC_VGA_MODE								0x02
#define PIXEL_CLOCK_V6_MISC_HDMI_BPP_MASK           0x0c
#define PIXEL_CLOCK_V6_MISC_HDMI_24BPP              0x00
#define PIXEL_CLOCK_V6_MISC_HDMI_36BPP              0x04
#define PIXEL_CLOCK_V6_MISC_HDMI_30BPP              0x08
#define PIXEL_CLOCK_V6_MISC_HDMI_48BPP              0x0c
#define PIXEL_CLOCK_V6_MISC_REF_DIV_SRC             0x10

typedef struct _GET_DISP_PLL_STATUS_INPUT_PARAMETERS_V2
{
  PIXEL_CLOCK_PARAMETERS_V3 sDispClkInput;
}GET_DISP_PLL_STATUS_INPUT_PARAMETERS_V2;

typedef struct _GET_DISP_PLL_STATUS_OUTPUT_PARAMETERS_V2
{
  UCHAR  ucStatus;
  UCHAR  ucRefDivSrc;                 
  UCHAR  ucReserved[2];
}GET_DISP_PLL_STATUS_OUTPUT_PARAMETERS_V2;

typedef struct _GET_DISP_PLL_STATUS_INPUT_PARAMETERS_V3
{
  PIXEL_CLOCK_PARAMETERS_V5 sDispClkInput;
}GET_DISP_PLL_STATUS_INPUT_PARAMETERS_V3;

	
	
typedef struct _ADJUST_DISPLAY_PLL_PARAMETERS
{
	USHORT usPixelClock;
	UCHAR ucTransmitterID;
	UCHAR ucEncodeMode;
	union
	{
		UCHAR ucDVOConfig;									
		UCHAR ucConfig;											
	};
	UCHAR ucReserved[3];
}ADJUST_DISPLAY_PLL_PARAMETERS;

#define ADJUST_DISPLAY_CONFIG_SS_ENABLE       0x10
#define ADJUST_DISPLAY_PLL_PS_ALLOCATION			ADJUST_DISPLAY_PLL_PARAMETERS

typedef struct _ADJUST_DISPLAY_PLL_INPUT_PARAMETERS_V3
{
	USHORT usPixelClock;                    
	UCHAR ucTransmitterID;                  
	UCHAR ucEncodeMode;                     
  UCHAR ucDispPllConfig;                 
  UCHAR ucExtTransmitterID;               
	UCHAR ucReserved[2];
}ADJUST_DISPLAY_PLL_INPUT_PARAMETERS_V3;

#define DISPPLL_CONFIG_DVO_RATE_SEL                0x0001     
#define DISPPLL_CONFIG_DVO_DDR_SPEED               0x0000     
#define DISPPLL_CONFIG_DVO_SDR_SPEED               0x0001     
#define DISPPLL_CONFIG_DVO_OUTPUT_SEL              0x000c     
#define DISPPLL_CONFIG_DVO_LOW12BIT                0x0000     
#define DISPPLL_CONFIG_DVO_UPPER12BIT              0x0004     
#define DISPPLL_CONFIG_DVO_24BIT                   0x0008     
#define DISPPLL_CONFIG_SS_ENABLE                   0x0010     
#define DISPPLL_CONFIG_COHERENT_MODE               0x0020     
#define DISPPLL_CONFIG_DUAL_LINK                   0x0040     


typedef struct _ADJUST_DISPLAY_PLL_OUTPUT_PARAMETERS_V3
{
  ULONG ulDispPllFreq;                 
  UCHAR ucRefDiv;                      
  UCHAR ucPostDiv;                     
  UCHAR ucReserved[2];  
}ADJUST_DISPLAY_PLL_OUTPUT_PARAMETERS_V3;

typedef struct _ADJUST_DISPLAY_PLL_PS_ALLOCATION_V3
{
  union 
  {
    ADJUST_DISPLAY_PLL_INPUT_PARAMETERS_V3  sInput;
    ADJUST_DISPLAY_PLL_OUTPUT_PARAMETERS_V3 sOutput;
  };
} ADJUST_DISPLAY_PLL_PS_ALLOCATION_V3;

	
	
typedef struct _ENABLE_YUV_PARAMETERS
{
  UCHAR ucEnable;                     
  UCHAR ucCRTC;                       
  UCHAR ucPadding[2];
}ENABLE_YUV_PARAMETERS;
#define ENABLE_YUV_PS_ALLOCATION ENABLE_YUV_PARAMETERS

	
	
typedef struct _GET_MEMORY_CLOCK_PARAMETERS
{
  ULONG ulReturnMemoryClock;          
} GET_MEMORY_CLOCK_PARAMETERS;
#define GET_MEMORY_CLOCK_PS_ALLOCATION  GET_MEMORY_CLOCK_PARAMETERS

	
	
typedef struct _GET_ENGINE_CLOCK_PARAMETERS
{
  ULONG ulReturnEngineClock;          
} GET_ENGINE_CLOCK_PARAMETERS;
#define GET_ENGINE_CLOCK_PS_ALLOCATION  GET_ENGINE_CLOCK_PARAMETERS

	
	
typedef struct _READ_EDID_FROM_HW_I2C_DATA_PARAMETERS
{
  USHORT    usPrescale;         
  USHORT    usVRAMAddress;      
  USHORT    usStatus;           
                                
  UCHAR     ucSlaveAddr;        
  UCHAR     ucLineNumber;       
}READ_EDID_FROM_HW_I2C_DATA_PARAMETERS;
#define READ_EDID_FROM_HW_I2C_DATA_PS_ALLOCATION  READ_EDID_FROM_HW_I2C_DATA_PARAMETERS


#define  ATOM_WRITE_I2C_FORMAT_PSOFFSET_PSDATABYTE                  0
#define  ATOM_WRITE_I2C_FORMAT_PSOFFSET_PSTWODATABYTES              1
#define  ATOM_WRITE_I2C_FORMAT_PSCOUNTER_PSOFFSET_IDDATABLOCK       2
#define  ATOM_WRITE_I2C_FORMAT_PSCOUNTER_IDOFFSET_PLUS_IDDATABLOCK  3
#define  ATOM_WRITE_I2C_FORMAT_IDCOUNTER_IDOFFSET_IDDATABLOCK       4

typedef struct _WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS
{
  USHORT    usPrescale;         
  USHORT    usByteOffset;       
                                
                                
                                
                                
                                
                                
  UCHAR     ucData;             
  UCHAR     ucStatus;           
  UCHAR     ucSlaveAddr;        
  UCHAR     ucLineNumber;       
}WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS;

#define WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION  WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS

typedef struct _SET_UP_HW_I2C_DATA_PARAMETERS
{
  USHORT    usPrescale;         
  UCHAR     ucSlaveAddr;        
  UCHAR     ucLineNumber;       
}SET_UP_HW_I2C_DATA_PARAMETERS;


#define SPEED_FAN_CONTROL_PS_ALLOCATION   WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS


	
	
typedef struct	_POWER_CONNECTOR_DETECTION_PARAMETERS
{
  UCHAR   ucPowerConnectorStatus;      
	UCHAR   ucPwrBehaviorId;							
	USHORT	usPwrBudget;								 
}POWER_CONNECTOR_DETECTION_PARAMETERS;

typedef struct POWER_CONNECTOR_DETECTION_PS_ALLOCATION
{                               
  UCHAR   ucPowerConnectorStatus;      
	UCHAR   ucReserved;
	USHORT	usPwrBudget;								 
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION    sReserved;
}POWER_CONNECTOR_DETECTION_PS_ALLOCATION;


	
	
typedef struct	_ENABLE_LVDS_SS_PARAMETERS
{
  USHORT  usSpreadSpectrumPercentage;       
  UCHAR   ucSpreadSpectrumType;           
  UCHAR   ucSpreadSpectrumStepSize_Delay; 
  UCHAR   ucEnable;                       
  UCHAR   ucPadding[3];
}ENABLE_LVDS_SS_PARAMETERS;

typedef struct	_ENABLE_LVDS_SS_PARAMETERS_V2
{
  USHORT  usSpreadSpectrumPercentage;       
  UCHAR   ucSpreadSpectrumType;           
  UCHAR   ucSpreadSpectrumStep;           
  UCHAR   ucEnable;                       
  UCHAR   ucSpreadSpectrumDelay;
  UCHAR   ucSpreadSpectrumRange;
  UCHAR   ucPadding;
}ENABLE_LVDS_SS_PARAMETERS_V2;

typedef struct	_ENABLE_SPREAD_SPECTRUM_ON_PPLL
{
  USHORT  usSpreadSpectrumPercentage;
  UCHAR   ucSpreadSpectrumType;           
  UCHAR   ucSpreadSpectrumStep;           
  UCHAR   ucEnable;                       
  UCHAR   ucSpreadSpectrumDelay;
  UCHAR   ucSpreadSpectrumRange;
  UCHAR   ucPpll;												  
}ENABLE_SPREAD_SPECTRUM_ON_PPLL;

typedef struct _ENABLE_SPREAD_SPECTRUM_ON_PPLL_V2
{
  USHORT  usSpreadSpectrumPercentage;
  UCHAR   ucSpreadSpectrumType;	        
                                        
                                        
                                        
  UCHAR   ucEnable;	                    
  USHORT  usSpreadSpectrumAmount;      	
  USHORT  usSpreadSpectrumStep;	        
}ENABLE_SPREAD_SPECTRUM_ON_PPLL_V2;

#define ATOM_PPLL_SS_TYPE_V2_DOWN_SPREAD      0x00
#define ATOM_PPLL_SS_TYPE_V2_CENTRE_SPREAD    0x01
#define ATOM_PPLL_SS_TYPE_V2_EXT_SPREAD       0x02
#define ATOM_PPLL_SS_TYPE_V2_PPLL_SEL_MASK    0x0c
#define ATOM_PPLL_SS_TYPE_V2_P1PLL            0x00
#define ATOM_PPLL_SS_TYPE_V2_P2PLL            0x04
#define ATOM_PPLL_SS_TYPE_V2_DCPLL            0x08
#define ATOM_PPLL_SS_AMOUNT_V2_FBDIV_MASK     0x00FF
#define ATOM_PPLL_SS_AMOUNT_V2_FBDIV_SHIFT    0
#define ATOM_PPLL_SS_AMOUNT_V2_NFRAC_MASK     0x0F00
#define ATOM_PPLL_SS_AMOUNT_V2_NFRAC_SHIFT    8

 typedef struct _ENABLE_SPREAD_SPECTRUM_ON_PPLL_V3
{
  USHORT  usSpreadSpectrumAmountFrac;   
  UCHAR   ucSpreadSpectrumType;	        
                                        
                                        
                                        
  UCHAR   ucEnable;	                    
  USHORT  usSpreadSpectrumAmount;      	
  USHORT  usSpreadSpectrumStep;	        
}ENABLE_SPREAD_SPECTRUM_ON_PPLL_V3;
    
#define ATOM_PPLL_SS_TYPE_V3_DOWN_SPREAD      0x00
#define ATOM_PPLL_SS_TYPE_V3_CENTRE_SPREAD    0x01
#define ATOM_PPLL_SS_TYPE_V3_EXT_SPREAD       0x02
#define ATOM_PPLL_SS_TYPE_V3_PPLL_SEL_MASK    0x0c
#define ATOM_PPLL_SS_TYPE_V3_P1PLL            0x00
#define ATOM_PPLL_SS_TYPE_V3_P2PLL            0x04
#define ATOM_PPLL_SS_TYPE_V3_DCPLL            0x08
#define ATOM_PPLL_SS_TYPE_V3_P0PLL            ATOM_PPLL_SS_TYPE_V3_DCPLL
#define ATOM_PPLL_SS_AMOUNT_V3_FBDIV_MASK     0x00FF
#define ATOM_PPLL_SS_AMOUNT_V3_FBDIV_SHIFT    0
#define ATOM_PPLL_SS_AMOUNT_V3_NFRAC_MASK     0x0F00
#define ATOM_PPLL_SS_AMOUNT_V3_NFRAC_SHIFT    8

#define ENABLE_SPREAD_SPECTRUM_ON_PPLL_PS_ALLOCATION  ENABLE_SPREAD_SPECTRUM_ON_PPLL


typedef struct _SET_PIXEL_CLOCK_PS_ALLOCATION
{
  PIXEL_CLOCK_PARAMETERS sPCLKInput;
  ENABLE_SPREAD_SPECTRUM_ON_PPLL sReserved;
}SET_PIXEL_CLOCK_PS_ALLOCATION;

#define ENABLE_VGA_RENDER_PS_ALLOCATION   SET_PIXEL_CLOCK_PS_ALLOCATION

	
	
typedef struct	_MEMORY_TRAINING_PARAMETERS
{
  ULONG ulTargetMemoryClock;          
}MEMORY_TRAINING_PARAMETERS;
#define MEMORY_TRAINING_PS_ALLOCATION MEMORY_TRAINING_PARAMETERS




	
	
typedef struct _LVDS_ENCODER_CONTROL_PARAMETERS
{
  USHORT usPixelClock;  
  UCHAR  ucMisc;        
                        
                        
                        
  UCHAR  ucAction;      
                        
}LVDS_ENCODER_CONTROL_PARAMETERS;

#define LVDS_ENCODER_CONTROL_PS_ALLOCATION  LVDS_ENCODER_CONTROL_PARAMETERS
   
#define TMDS1_ENCODER_CONTROL_PARAMETERS    LVDS_ENCODER_CONTROL_PARAMETERS
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION TMDS1_ENCODER_CONTROL_PARAMETERS

#define TMDS2_ENCODER_CONTROL_PARAMETERS    TMDS1_ENCODER_CONTROL_PARAMETERS
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION TMDS2_ENCODER_CONTROL_PARAMETERS


typedef struct _LVDS_ENCODER_CONTROL_PARAMETERS_V2
{
  USHORT usPixelClock;  
  UCHAR  ucMisc;        
  UCHAR  ucAction;      
                        
  UCHAR  ucTruncate;    
                        
                        
                        
  UCHAR  ucSpatial;     
                        
                        
                        
  UCHAR  ucTemporal;    
                        
                        
                        
                        
                        
  UCHAR  ucFRC;         
                        
                        
                        
                        
                        
                        
                        
}LVDS_ENCODER_CONTROL_PARAMETERS_V2;

#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_V2  LVDS_ENCODER_CONTROL_PARAMETERS_V2
   
#define TMDS1_ENCODER_CONTROL_PARAMETERS_V2    LVDS_ENCODER_CONTROL_PARAMETERS_V2
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_V2 TMDS1_ENCODER_CONTROL_PARAMETERS_V2
  
#define TMDS2_ENCODER_CONTROL_PARAMETERS_V2    TMDS1_ENCODER_CONTROL_PARAMETERS_V2
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_V2 TMDS2_ENCODER_CONTROL_PARAMETERS_V2

#define LVDS_ENCODER_CONTROL_PARAMETERS_V3     LVDS_ENCODER_CONTROL_PARAMETERS_V2
#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_V3  LVDS_ENCODER_CONTROL_PARAMETERS_V3

#define TMDS1_ENCODER_CONTROL_PARAMETERS_V3    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_V3 TMDS1_ENCODER_CONTROL_PARAMETERS_V3

#define TMDS2_ENCODER_CONTROL_PARAMETERS_V3    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_V3 TMDS2_ENCODER_CONTROL_PARAMETERS_V3

	
	
typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS
{                               
  UCHAR    ucEnable;            
  UCHAR    ucMisc;              
  UCHAR    ucPadding[2];
}ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS;

typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION
{                               
  ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS    sXTmdsEncoder;
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION   sReserved;     
}ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION;

#define ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS_V2  LVDS_ENCODER_CONTROL_PARAMETERS_V2

typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION_V2
{                               
  ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS_V2    sXTmdsEncoder;
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION      sReserved;     
}ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION_V2;

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION
{
  DIG_ENCODER_CONTROL_PARAMETERS            sDigEncoder;
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;
}EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION;

	
	

#define DVO_ENCODER_CONFIG_RATE_SEL							0x01
#define DVO_ENCODER_CONFIG_DDR_SPEED						0x00
#define DVO_ENCODER_CONFIG_SDR_SPEED						0x01
#define DVO_ENCODER_CONFIG_OUTPUT_SEL						0x0c
#define DVO_ENCODER_CONFIG_LOW12BIT							0x00
#define DVO_ENCODER_CONFIG_UPPER12BIT						0x04
#define DVO_ENCODER_CONFIG_24BIT								0x08

typedef struct _DVO_ENCODER_CONTROL_PARAMETERS_V3
{
  USHORT usPixelClock; 
  UCHAR  ucDVOConfig;
  UCHAR  ucAction;														
  UCHAR  ucReseved[4];
}DVO_ENCODER_CONTROL_PARAMETERS_V3;
#define DVO_ENCODER_CONTROL_PS_ALLOCATION_V3	DVO_ENCODER_CONTROL_PARAMETERS_V3


#define LVDS_ENCODER_CONTROL_PARAMETERS_LAST     LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_LAST  LVDS_ENCODER_CONTROL_PARAMETERS_LAST

#define TMDS1_ENCODER_CONTROL_PARAMETERS_LAST    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS1_ENCODER_CONTROL_PARAMETERS_LAST

#define TMDS2_ENCODER_CONTROL_PARAMETERS_LAST    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS2_ENCODER_CONTROL_PARAMETERS_LAST

#define DVO_ENCODER_CONTROL_PARAMETERS_LAST      DVO_ENCODER_CONTROL_PARAMETERS
#define DVO_ENCODER_CONTROL_PS_ALLOCATION_LAST   DVO_ENCODER_CONTROL_PS_ALLOCATION

#define PANEL_ENCODER_MISC_DUAL                0x01
#define PANEL_ENCODER_MISC_COHERENT            0x02
#define	PANEL_ENCODER_MISC_TMDS_LINKB					 0x04
#define	PANEL_ENCODER_MISC_HDMI_TYPE					 0x08

#define PANEL_ENCODER_ACTION_DISABLE           ATOM_DISABLE
#define PANEL_ENCODER_ACTION_ENABLE            ATOM_ENABLE
#define PANEL_ENCODER_ACTION_COHERENTSEQ       (ATOM_ENABLE+1)

#define PANEL_ENCODER_TRUNCATE_EN              0x01
#define PANEL_ENCODER_TRUNCATE_DEPTH           0x10
#define PANEL_ENCODER_SPATIAL_DITHER_EN        0x01
#define PANEL_ENCODER_SPATIAL_DITHER_DEPTH     0x10
#define PANEL_ENCODER_TEMPORAL_DITHER_EN       0x01
#define PANEL_ENCODER_TEMPORAL_DITHER_DEPTH    0x10
#define PANEL_ENCODER_TEMPORAL_LEVEL_4         0x20
#define PANEL_ENCODER_25FRC_MASK               0x10
#define PANEL_ENCODER_25FRC_E                  0x00
#define PANEL_ENCODER_25FRC_F                  0x10
#define PANEL_ENCODER_50FRC_MASK               0x60
#define PANEL_ENCODER_50FRC_A                  0x00
#define PANEL_ENCODER_50FRC_B                  0x20
#define PANEL_ENCODER_50FRC_C                  0x40
#define PANEL_ENCODER_50FRC_D                  0x60
#define PANEL_ENCODER_75FRC_MASK               0x80
#define PANEL_ENCODER_75FRC_E                  0x00
#define PANEL_ENCODER_75FRC_F                  0x80

	
	
#define SET_VOLTAGE_TYPE_ASIC_VDDC             1
#define SET_VOLTAGE_TYPE_ASIC_MVDDC            2
#define SET_VOLTAGE_TYPE_ASIC_MVDDQ            3
#define SET_VOLTAGE_TYPE_ASIC_VDDCI            4
#define SET_VOLTAGE_INIT_MODE                  5
#define SET_VOLTAGE_GET_MAX_VOLTAGE            6					

#define SET_ASIC_VOLTAGE_MODE_ALL_SOURCE       0x1
#define SET_ASIC_VOLTAGE_MODE_SOURCE_A         0x2
#define SET_ASIC_VOLTAGE_MODE_SOURCE_B         0x4

#define	SET_ASIC_VOLTAGE_MODE_SET_VOLTAGE      0x0
#define	SET_ASIC_VOLTAGE_MODE_GET_GPIOVAL      0x1	
#define	SET_ASIC_VOLTAGE_MODE_GET_GPIOMASK     0x2

typedef struct	_SET_VOLTAGE_PARAMETERS
{
  UCHAR    ucVoltageType;               
  UCHAR    ucVoltageMode;               
  UCHAR    ucVoltageIndex;              
  UCHAR    ucReserved;          
}SET_VOLTAGE_PARAMETERS;

typedef struct	_SET_VOLTAGE_PARAMETERS_V2
{
  UCHAR    ucVoltageType;               
  UCHAR    ucVoltageMode;               
  USHORT   usVoltageLevel;              
}SET_VOLTAGE_PARAMETERS_V2;


typedef struct	_SET_VOLTAGE_PARAMETERS_V1_3
{
  UCHAR    ucVoltageType;               
  UCHAR    ucVoltageMode;               
  USHORT   usVoltageLevel;              
}SET_VOLTAGE_PARAMETERS_V1_3;

#define VOLTAGE_TYPE_VDDC                    1
#define VOLTAGE_TYPE_MVDDC                   2
#define VOLTAGE_TYPE_MVDDQ                   3
#define VOLTAGE_TYPE_VDDCI                   4

#define ATOM_SET_VOLTAGE                     0        
#define ATOM_INIT_VOLTAGE_REGULATOR          3        
#define ATOM_SET_VOLTAGE_PHASE               4        
#define ATOM_GET_MAX_VOLTAGE                 6        
#define ATOM_GET_VOLTAGE_LEVEL               6        

#define ATOM_VIRTUAL_VOLTAGE_ID0             0xff01
#define ATOM_VIRTUAL_VOLTAGE_ID1             0xff02
#define ATOM_VIRTUAL_VOLTAGE_ID2             0xff03
#define ATOM_VIRTUAL_VOLTAGE_ID3             0xff04

typedef struct _SET_VOLTAGE_PS_ALLOCATION
{
  SET_VOLTAGE_PARAMETERS sASICSetVoltage;
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;
}SET_VOLTAGE_PS_ALLOCATION;

typedef struct  _GET_VOLTAGE_INFO_INPUT_PARAMETER_V1_1
{
  UCHAR    ucVoltageType;               
  UCHAR    ucVoltageMode;               
  USHORT   usVoltageLevel;              
  ULONG    ulReserved;
}GET_VOLTAGE_INFO_INPUT_PARAMETER_V1_1;

typedef struct  _GET_VOLTAGE_INFO_OUTPUT_PARAMETER_V1_1
{
  ULONG    ulVotlageGpioState;
  ULONG    ulVoltageGPioMask;
}GET_VOLTAGE_INFO_OUTPUT_PARAMETER_V1_1;

typedef struct  _GET_LEAKAGE_VOLTAGE_INFO_OUTPUT_PARAMETER_V1_1
{
  USHORT   usVoltageLevel;
  USHORT   usVoltageId;                                  
  ULONG    ulReseved;
}GET_LEAKAGE_VOLTAGE_INFO_OUTPUT_PARAMETER_V1_1;


#define	ATOM_GET_VOLTAGE_VID                0x00
#define ATOM_GET_VOTLAGE_INIT_SEQ           0x03
#define ATOM_GET_VOLTTAGE_PHASE_PHASE_VID   0x04
#define	ATOM_GET_VOLTAGE_STATE0_LEAKAGE_VID 0x10

#define	ATOM_GET_VOLTAGE_STATE1_LEAKAGE_VID 0x11
#define	ATOM_GET_VOLTAGE_STATE2_LEAKAGE_VID 0x12
#define	ATOM_GET_VOLTAGE_STATE3_LEAKAGE_VID 0x13

	
	
typedef struct _TV_ENCODER_CONTROL_PARAMETERS
{
  USHORT usPixelClock;                
  UCHAR  ucTvStandard;                
  UCHAR  ucAction;                    
                                      
}TV_ENCODER_CONTROL_PARAMETERS;

typedef struct _TV_ENCODER_CONTROL_PS_ALLOCATION
{
  TV_ENCODER_CONTROL_PARAMETERS sTVEncoder;          
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION    sReserved; 
}TV_ENCODER_CONTROL_PS_ALLOCATION;


	
	
typedef struct _ATOM_MASTER_LIST_OF_DATA_TABLES
{
  USHORT        UtilityPipeLine;	        
  USHORT        MultimediaCapabilityInfo; 
  USHORT        MultimediaConfigInfo;     
  USHORT        StandardVESA_Timing;      
  USHORT        FirmwareInfo;             
  USHORT        PaletteData;              
  USHORT        LCD_Info;                 
  USHORT        DIGTransmitterInfo;       
  USHORT        AnalogTV_Info;            
  USHORT        SupportedDevicesInfo;     
  USHORT        GPIO_I2C_Info;            
  USHORT        VRAM_UsageByFirmware;     
  USHORT        GPIO_Pin_LUT;             
  USHORT        VESA_ToInternalModeLUT;   
  USHORT        ComponentVideoInfo;       
  USHORT        PowerPlayInfo;            
  USHORT        CompassionateData;        
  USHORT        SaveRestoreInfo;          
  USHORT        PPLL_SS_Info;             
  USHORT        OemInfo;                  
  USHORT        XTMDS_Info;               
  USHORT        MclkSS_Info;              
  USHORT        Object_Header;            
  USHORT        IndirectIOAccess;         
  USHORT        MC_InitParameter;         
  USHORT        ASIC_VDDC_Info;						
  USHORT        ASIC_InternalSS_Info;			
  USHORT        TV_VideoMode;							
  USHORT        VRAM_Info;								
  USHORT        MemoryTrainingInfo;				
  USHORT        IntegratedSystemInfo;			
  USHORT        ASIC_ProfilingInfo;				
  USHORT        VoltageObjectInfo;				
	USHORT				PowerSourceInfo;					
}ATOM_MASTER_LIST_OF_DATA_TABLES;

typedef struct _ATOM_MASTER_DATA_TABLE
{ 
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_MASTER_LIST_OF_DATA_TABLES   ListOfDataTables;
}ATOM_MASTER_DATA_TABLE;

#define LVDS_Info                LCD_Info
#define DAC_Info                 PaletteData
#define TMDS_Info                DIGTransmitterInfo

	
	
typedef struct _ATOM_MULTIMEDIA_CAPABILITY_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ULONG                    ulSignature;      
  UCHAR                    ucI2C_Type;       
  UCHAR                    ucTV_OutInfo;     
  UCHAR                    ucVideoPortInfo;  
  UCHAR                    ucHostPortInfo;   
}ATOM_MULTIMEDIA_CAPABILITY_INFO;

	
	
typedef struct _ATOM_MULTIMEDIA_CONFIG_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;
  ULONG                    ulSignature;      
  UCHAR                    ucTunerInfo;      
  UCHAR                    ucAudioChipInfo;  
  UCHAR                    ucProductID;      
  UCHAR                    ucMiscInfo1;      
  UCHAR                    ucMiscInfo2;      
  UCHAR                    ucMiscInfo3;      
  UCHAR                    ucMiscInfo4;      
  UCHAR                    ucVideoInput0Info;
  UCHAR                    ucVideoInput1Info;
  UCHAR                    ucVideoInput2Info;
  UCHAR                    ucVideoInput3Info;
  UCHAR                    ucVideoInput4Info;
}ATOM_MULTIMEDIA_CONFIG_INFO;


	
	

#define ATOM_BIOS_INFO_ATOM_FIRMWARE_POSTED         0x0001
#define ATOM_BIOS_INFO_DUAL_CRTC_SUPPORT            0x0002
#define ATOM_BIOS_INFO_EXTENDED_DESKTOP_SUPPORT     0x0004
#define ATOM_BIOS_INFO_MEMORY_CLOCK_SS_SUPPORT      0x0008		
#define ATOM_BIOS_INFO_ENGINE_CLOCK_SS_SUPPORT      0x0010		
#define ATOM_BIOS_INFO_BL_CONTROLLED_BY_GPU         0x0020
#define ATOM_BIOS_INFO_WMI_SUPPORT                  0x0040
#define ATOM_BIOS_INFO_PPMODE_ASSIGNGED_BY_SYSTEM   0x0080
#define ATOM_BIOS_INFO_HYPERMEMORY_SUPPORT          0x0100
#define ATOM_BIOS_INFO_HYPERMEMORY_SIZE_MASK        0x1E00
#define ATOM_BIOS_INFO_VPOST_WITHOUT_FIRST_MODE_SET 0x2000
#define ATOM_BIOS_INFO_BIOS_SCRATCH6_SCL2_REDEFINE  0x4000
#define ATOM_BIOS_INFO_MEMORY_CLOCK_EXT_SS_SUPPORT  0x0008		
#define ATOM_BIOS_INFO_ENGINE_CLOCK_EXT_SS_SUPPORT  0x0010		

#ifndef _H2INC

typedef struct _ATOM_FIRMWARE_CAPABILITY
{
#if ATOM_BIG_ENDIAN
  USHORT Reserved:1;
  USHORT SCL2Redefined:1;
  USHORT PostWithoutModeSet:1;
  USHORT HyperMemory_Size:4;
  USHORT HyperMemory_Support:1;
  USHORT PPMode_Assigned:1;
  USHORT WMI_SUPPORT:1;
  USHORT GPUControlsBL:1;
  USHORT EngineClockSS_Support:1;
  USHORT MemoryClockSS_Support:1;
  USHORT ExtendedDesktopSupport:1;
  USHORT DualCRTC_Support:1;
  USHORT FirmwarePosted:1;
#else
  USHORT FirmwarePosted:1;
  USHORT DualCRTC_Support:1;
  USHORT ExtendedDesktopSupport:1;
  USHORT MemoryClockSS_Support:1;
  USHORT EngineClockSS_Support:1;
  USHORT GPUControlsBL:1;
  USHORT WMI_SUPPORT:1;
  USHORT PPMode_Assigned:1;
  USHORT HyperMemory_Support:1;
  USHORT HyperMemory_Size:4;
  USHORT PostWithoutModeSet:1;
  USHORT SCL2Redefined:1;
  USHORT Reserved:1;
#endif
}ATOM_FIRMWARE_CAPABILITY;

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS
{
  ATOM_FIRMWARE_CAPABILITY sbfAccess;
  USHORT                   susAccess;
}ATOM_FIRMWARE_CAPABILITY_ACCESS;

#else

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS
{
  USHORT                   susAccess;
}ATOM_FIRMWARE_CAPABILITY_ACCESS;

#endif

typedef struct _ATOM_FIRMWARE_INFO
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulDriverTargetEngineClock;  
  ULONG                           ulDriverTargetMemoryClock;  
  ULONG                           ulMaxEngineClockPLL_Output; 
  ULONG                           ulMaxMemoryClockPLL_Output; 
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulASICMaxEngineClock;       
  ULONG                           ulASICMaxMemoryClock;       
  UCHAR                           ucASICMaxTemperature;
  UCHAR                           ucPadding[3];               
  ULONG                           aulReservedForBIOS[3];      
  USHORT                          usMinEngineClockPLL_Input;  
  USHORT                          usMaxEngineClockPLL_Input;  
  USHORT                          usMinEngineClockPLL_Output; 
  USHORT                          usMinMemoryClockPLL_Input;  
  USHORT                          usMaxMemoryClockPLL_Input;  
  USHORT                          usMinMemoryClockPLL_Output; 
  USHORT                          usMaxPixelClock;            
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usMinPixelClockPLL_Output;  
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usReferenceClock;           
  USHORT                          usPM_RTS_Location;          
  UCHAR                           ucPM_RTS_StreamSize;        
  UCHAR                           ucDesign_ID;                
  UCHAR                           ucMemoryModule_ID;          
}ATOM_FIRMWARE_INFO;

typedef struct _ATOM_FIRMWARE_INFO_V1_2
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulDriverTargetEngineClock;  
  ULONG                           ulDriverTargetMemoryClock;  
  ULONG                           ulMaxEngineClockPLL_Output; 
  ULONG                           ulMaxMemoryClockPLL_Output; 
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulASICMaxEngineClock;       
  ULONG                           ulASICMaxMemoryClock;       
  UCHAR                           ucASICMaxTemperature;
  UCHAR                           ucMinAllowedBL_Level;
  UCHAR                           ucPadding[2];               
  ULONG                           aulReservedForBIOS[2];      
  ULONG                           ulMinPixelClockPLL_Output;  
  USHORT                          usMinEngineClockPLL_Input;  
  USHORT                          usMaxEngineClockPLL_Input;  
  USHORT                          usMinEngineClockPLL_Output; 
  USHORT                          usMinMemoryClockPLL_Input;  
  USHORT                          usMaxMemoryClockPLL_Input;  
  USHORT                          usMinMemoryClockPLL_Output; 
  USHORT                          usMaxPixelClock;            
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usMinPixelClockPLL_Output;  
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usReferenceClock;           
  USHORT                          usPM_RTS_Location;          
  UCHAR                           ucPM_RTS_StreamSize;        
  UCHAR                           ucDesign_ID;                
  UCHAR                           ucMemoryModule_ID;          
}ATOM_FIRMWARE_INFO_V1_2;

typedef struct _ATOM_FIRMWARE_INFO_V1_3
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulDriverTargetEngineClock;  
  ULONG                           ulDriverTargetMemoryClock;  
  ULONG                           ulMaxEngineClockPLL_Output; 
  ULONG                           ulMaxMemoryClockPLL_Output; 
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulASICMaxEngineClock;       
  ULONG                           ulASICMaxMemoryClock;       
  UCHAR                           ucASICMaxTemperature;
  UCHAR                           ucMinAllowedBL_Level;
  UCHAR                           ucPadding[2];               
  ULONG                           aulReservedForBIOS;         
  ULONG                           ul3DAccelerationEngineClock;
  ULONG                           ulMinPixelClockPLL_Output;  
  USHORT                          usMinEngineClockPLL_Input;  
  USHORT                          usMaxEngineClockPLL_Input;  
  USHORT                          usMinEngineClockPLL_Output; 
  USHORT                          usMinMemoryClockPLL_Input;  
  USHORT                          usMaxMemoryClockPLL_Input;  
  USHORT                          usMinMemoryClockPLL_Output; 
  USHORT                          usMaxPixelClock;            
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usMinPixelClockPLL_Output;  
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usReferenceClock;           
  USHORT                          usPM_RTS_Location;          
  UCHAR                           ucPM_RTS_StreamSize;        
  UCHAR                           ucDesign_ID;                
  UCHAR                           ucMemoryModule_ID;          
}ATOM_FIRMWARE_INFO_V1_3;

typedef struct _ATOM_FIRMWARE_INFO_V1_4
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulDriverTargetEngineClock;  
  ULONG                           ulDriverTargetMemoryClock;  
  ULONG                           ulMaxEngineClockPLL_Output; 
  ULONG                           ulMaxMemoryClockPLL_Output; 
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulASICMaxEngineClock;       
  ULONG                           ulASICMaxMemoryClock;       
  UCHAR                           ucASICMaxTemperature;
  UCHAR                           ucMinAllowedBL_Level;
  USHORT                          usBootUpVDDCVoltage;        
  USHORT                          usLcdMinPixelClockPLL_Output; 
  USHORT                          usLcdMaxPixelClockPLL_Output; 
  ULONG                           ul3DAccelerationEngineClock;
  ULONG                           ulMinPixelClockPLL_Output;  
  USHORT                          usMinEngineClockPLL_Input;  
  USHORT                          usMaxEngineClockPLL_Input;  
  USHORT                          usMinEngineClockPLL_Output; 
  USHORT                          usMinMemoryClockPLL_Input;  
  USHORT                          usMaxMemoryClockPLL_Input;  
  USHORT                          usMinMemoryClockPLL_Output; 
  USHORT                          usMaxPixelClock;            
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usMinPixelClockPLL_Output;  
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usReferenceClock;           
  USHORT                          usPM_RTS_Location;          
  UCHAR                           ucPM_RTS_StreamSize;        
  UCHAR                           ucDesign_ID;                
  UCHAR                           ucMemoryModule_ID;          
}ATOM_FIRMWARE_INFO_V1_4;

typedef struct _ATOM_FIRMWARE_INFO_V2_1
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulReserved1;
  ULONG                           ulReserved2;
  ULONG                           ulMaxEngineClockPLL_Output; 
  ULONG                           ulMaxMemoryClockPLL_Output; 
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulBinaryAlteredInfo;        
  ULONG                           ulDefaultDispEngineClkFreq; 
  UCHAR                           ucReserved1;                
  UCHAR                           ucMinAllowedBL_Level;
  USHORT                          usBootUpVDDCVoltage;        
  USHORT                          usLcdMinPixelClockPLL_Output; 
  USHORT                          usLcdMaxPixelClockPLL_Output; 
  ULONG                           ulReserved4;                
  ULONG                           ulMinPixelClockPLL_Output;  
  USHORT                          usMinEngineClockPLL_Input;  
  USHORT                          usMaxEngineClockPLL_Input;  
  USHORT                          usMinEngineClockPLL_Output; 
  USHORT                          usMinMemoryClockPLL_Input;  
  USHORT                          usMaxMemoryClockPLL_Input;  
  USHORT                          usMinMemoryClockPLL_Output; 
  USHORT                          usMaxPixelClock;            
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usMinPixelClockPLL_Output;  
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usCoreReferenceClock;       
  USHORT                          usMemoryReferenceClock;     
  USHORT                          usUniphyDPModeExtClkFreq;   
  UCHAR                           ucMemoryModule_ID;          
  UCHAR                           ucReserved4[3];
}ATOM_FIRMWARE_INFO_V2_1;

typedef struct _ATOM_FIRMWARE_INFO_V2_2
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG                           ulFirmwareRevision;
  ULONG                           ulDefaultEngineClock;       
  ULONG                           ulDefaultMemoryClock;       
  ULONG                           ulReserved[2];
  ULONG                           ulReserved1;                
  ULONG                           ulReserved2;                
  ULONG                           ulMaxPixelClockPLL_Output;  
  ULONG                           ulBinaryAlteredInfo;        
  ULONG                           ulDefaultDispEngineClkFreq; 
  UCHAR                           ucReserved3;                
  UCHAR                           ucMinAllowedBL_Level;
  USHORT                          usBootUpVDDCVoltage;        
  USHORT                          usLcdMinPixelClockPLL_Output; 
  USHORT                          usLcdMaxPixelClockPLL_Output; 
  ULONG                           ulReserved4;                
  ULONG                           ulMinPixelClockPLL_Output;  
  UCHAR                           ucRemoteDisplayConfig;
  UCHAR                           ucReserved5[3];             
  ULONG                           ulReserved6;                
  ULONG                           ulReserved7;                
  USHORT                          usReserved11;               
  USHORT                          usMinPixelClockPLL_Input;   
  USHORT                          usMaxPixelClockPLL_Input;   
  USHORT                          usBootUpVDDCIVoltage;       
  ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
  USHORT                          usCoreReferenceClock;       
  USHORT                          usMemoryReferenceClock;     
  USHORT                          usUniphyDPModeExtClkFreq;   
  UCHAR                           ucMemoryModule_ID;          
  UCHAR                           ucReserved9[3];
  USHORT                          usBootUpMVDDCVoltage;       
  USHORT                          usReserved12;
  ULONG                           ulReserved10[3];            
}ATOM_FIRMWARE_INFO_V2_2;

#define ATOM_FIRMWARE_INFO_LAST  ATOM_FIRMWARE_INFO_V2_2


#define REMOTE_DISPLAY_DISABLE                   0x00
#define REMOTE_DISPLAY_ENABLE                    0x01

	
	
#define IGP_CAP_FLAG_DYNAMIC_CLOCK_EN      0x2
#define IGP_CAP_FLAG_AC_CARD               0x4
#define IGP_CAP_FLAG_SDVO_CARD             0x8
#define IGP_CAP_FLAG_POSTDIV_BY_2_MODE     0x10

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO
{
  ATOM_COMMON_TABLE_HEADER        sHeader; 
  ULONG	                          ulBootUpEngineClock;		    
  ULONG	                          ulBootUpMemoryClock;		    
  ULONG	                          ulMaxSystemMemoryClock;	    
  ULONG	                          ulMinSystemMemoryClock;	    
  UCHAR                           ucNumberOfCyclesInPeriodHi;
  UCHAR                           ucLCDTimingSel;             
  USHORT                          usReserved1;
  USHORT                          usInterNBVoltageLow;        
  USHORT                          usInterNBVoltageHigh;       
  ULONG	                          ulReserved[2];

  USHORT	                        usFSBClock;			            
  USHORT                          usCapabilityFlag;		        
																                              
                                                              
  USHORT	                        usPCIENBCfgReg7;				    
  USHORT	                        usK8MemoryClock;            
  USHORT	                        usK8SyncStartDelay;         
  USHORT	                        usK8DataReturnTime;         
  UCHAR                           ucMaxNBVoltage;
  UCHAR                           ucMinNBVoltage;
  UCHAR                           ucMemoryType;					      
  UCHAR                           ucNumberOfCyclesInPeriod;		
  UCHAR                           ucStartingPWM_HighTime;     
  UCHAR                           ucHTLinkWidth;              
  UCHAR                           ucMaxNBVoltageHigh;    
  UCHAR                           ucMinNBVoltageHigh;
}ATOM_INTEGRATED_SYSTEM_INFO;





typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_V2
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  ULONG	                     ulBootUpEngineClock;       
  ULONG			     ulReserved1[2];            
  ULONG	                     ulBootUpUMAClock;          
  ULONG	                     ulBootUpSidePortClock;     
  ULONG	                     ulMinSidePortClock;        
  ULONG			     ulReserved2[6];            
  ULONG                      ulSystemConfig;            
  ULONG                      ulBootUpReqDisplayVector;
  ULONG                      ulOtherDisplayMisc;
  ULONG                      ulDDISlot1Config;
  ULONG                      ulDDISlot2Config;
  UCHAR                      ucMemoryType;              
  UCHAR                      ucUMAChannelNumber;
  UCHAR                      ucDockingPinBit;
  UCHAR                      ucDockingPinPolarity;
  ULONG                      ulDockingPinCFGInfo;
  ULONG                      ulCPUCapInfo;
  USHORT                     usNumberOfCyclesInPeriod;
  USHORT                     usMaxNBVoltage;
  USHORT                     usMinNBVoltage;
  USHORT                     usBootUpNBVoltage;
  ULONG                      ulHTLinkFreq;              
  USHORT                     usMinHTLinkWidth;
  USHORT                     usMaxHTLinkWidth;
  USHORT                     usUMASyncStartDelay;
  USHORT                     usUMADataReturnTime;
  USHORT                     usLinkStatusZeroTime;
  USHORT                     usDACEfuse;				
  ULONG                      ulHighVoltageHTLinkFreq;     
  ULONG                      ulLowVoltageHTLinkFreq;      
  USHORT                     usMaxUpStreamHTLinkWidth;
  USHORT                     usMaxDownStreamHTLinkWidth;
  USHORT                     usMinUpStreamHTLinkWidth;
  USHORT                     usMinDownStreamHTLinkWidth;
  USHORT                     usFirmwareVersion;         
  USHORT                     usFullT0Time;             
  ULONG                      ulReserved3[96];          
}ATOM_INTEGRATED_SYSTEM_INFO_V2;   


#define    INTEGRATED_SYSTEM_INFO__UNKNOWN_CPU             0
#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__GRIFFIN        1
#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__GREYHOUND      2
#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__K8             3
#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__PHARAOH        4
#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__OROCHI         5

#define    INTEGRATED_SYSTEM_INFO__AMD_CPU__MAX_CODE       INTEGRATED_SYSTEM_INFO__AMD_CPU__OROCHI    

#define SYSTEM_CONFIG_POWEREXPRESS_ENABLE                 0x00000001
#define SYSTEM_CONFIG_RUN_AT_OVERDRIVE_ENGINE             0x00000002
#define SYSTEM_CONFIG_USE_PWM_ON_VOLTAGE                  0x00000004 
#define SYSTEM_CONFIG_PERFORMANCE_POWERSTATE_ONLY         0x00000008
#define SYSTEM_CONFIG_CLMC_ENABLED                        0x00000010
#define SYSTEM_CONFIG_CDLW_ENABLED                        0x00000020
#define SYSTEM_CONFIG_HIGH_VOLTAGE_REQUESTED              0x00000040
#define SYSTEM_CONFIG_CLMC_HYBRID_MODE_ENABLED            0x00000080
#define SYSTEM_CONFIG_CDLF_ENABLED                        0x00000100
#define SYSTEM_CONFIG_DLL_SHUTDOWN_ENABLED                0x00000200

#define IGP_DDI_SLOT_LANE_CONFIG_MASK                     0x000000FF

#define b0IGP_DDI_SLOT_LANE_MAP_MASK                      0x0F
#define b0IGP_DDI_SLOT_DOCKING_LANE_MAP_MASK              0xF0
#define b0IGP_DDI_SLOT_CONFIG_LANE_0_3                    0x01
#define b0IGP_DDI_SLOT_CONFIG_LANE_4_7                    0x02
#define b0IGP_DDI_SLOT_CONFIG_LANE_8_11                   0x04
#define b0IGP_DDI_SLOT_CONFIG_LANE_12_15                  0x08

#define IGP_DDI_SLOT_ATTRIBUTE_MASK                       0x0000FF00
#define IGP_DDI_SLOT_CONFIG_REVERSED                      0x00000100
#define b1IGP_DDI_SLOT_CONFIG_REVERSED                    0x01

#define IGP_DDI_SLOT_CONNECTOR_TYPE_MASK                  0x00FF0000

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_V5
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  ULONG	                     ulBootUpEngineClock;       
  ULONG                      ulDentistVCOFreq;          
  ULONG                      ulLClockFreq;              
  ULONG	                     ulBootUpUMAClock;          
  ULONG                      ulReserved1[8];            
  ULONG                      ulBootUpReqDisplayVector;
  ULONG                      ulOtherDisplayMisc;
  ULONG                      ulReserved2[4];            
  ULONG                      ulSystemConfig;            
  ULONG                      ulCPUCapInfo;              
  USHORT                     usMaxNBVoltage;            
  USHORT                     usMinNBVoltage;            
  USHORT                     usBootUpNBVoltage;         
  UCHAR                      ucHtcTmpLmt;               
  UCHAR                      ucTjOffset;                
  ULONG                      ulReserved3[4];            
  ULONG                      ulDDISlot1Config;          
  ULONG                      ulDDISlot2Config;
  ULONG                      ulDDISlot3Config;
  ULONG                      ulDDISlot4Config;
  ULONG                      ulReserved4[4];            
  UCHAR                      ucMemoryType;              
  UCHAR                      ucUMAChannelNumber;
  USHORT                     usReserved;
  ULONG                      ulReserved5[4];            
  ULONG                      ulCSR_M3_ARB_CNTL_DEFAULT[10];
  ULONG                      ulCSR_M3_ARB_CNTL_UVD[10]; 
  ULONG                      ulCSR_M3_ARB_CNTL_FS3D[10];
  ULONG                      ulReserved6[61];           
}ATOM_INTEGRATED_SYSTEM_INFO_V5;   

#define ATOM_CRT_INT_ENCODER1_INDEX                       0x00000000
#define ATOM_LCD_INT_ENCODER1_INDEX                       0x00000001
#define ATOM_TV_INT_ENCODER1_INDEX                        0x00000002
#define ATOM_DFP_INT_ENCODER1_INDEX                       0x00000003
#define ATOM_CRT_INT_ENCODER2_INDEX                       0x00000004
#define ATOM_LCD_EXT_ENCODER1_INDEX                       0x00000005
#define ATOM_TV_EXT_ENCODER1_INDEX                        0x00000006
#define ATOM_DFP_EXT_ENCODER1_INDEX                       0x00000007
#define ATOM_CV_INT_ENCODER1_INDEX                        0x00000008
#define ATOM_DFP_INT_ENCODER2_INDEX                       0x00000009
#define ATOM_CRT_EXT_ENCODER1_INDEX                       0x0000000A
#define ATOM_CV_EXT_ENCODER1_INDEX                        0x0000000B
#define ATOM_DFP_INT_ENCODER3_INDEX                       0x0000000C
#define ATOM_DFP_INT_ENCODER4_INDEX                       0x0000000D

#define ASIC_INT_DAC1_ENCODER_ID    											0x00 
#define ASIC_INT_TV_ENCODER_ID														0x02
#define ASIC_INT_DIG1_ENCODER_ID													0x03
#define ASIC_INT_DAC2_ENCODER_ID													0x04
#define ASIC_EXT_TV_ENCODER_ID														0x06
#define ASIC_INT_DVO_ENCODER_ID														0x07
#define ASIC_INT_DIG2_ENCODER_ID													0x09
#define ASIC_EXT_DIG_ENCODER_ID														0x05
#define ASIC_EXT_DIG2_ENCODER_ID													0x08
#define ASIC_INT_DIG3_ENCODER_ID													0x0a
#define ASIC_INT_DIG4_ENCODER_ID													0x0b
#define ASIC_INT_DIG5_ENCODER_ID													0x0c
#define ASIC_INT_DIG6_ENCODER_ID													0x0d
#define ASIC_INT_DIG7_ENCODER_ID													0x0e

#define ATOM_ANALOG_ENCODER																0
#define ATOM_DIGITAL_ENCODER															1		
#define ATOM_DP_ENCODER															      2		

#define ATOM_ENCODER_ENUM_MASK                            0x70
#define ATOM_ENCODER_ENUM_ID1                             0x00
#define ATOM_ENCODER_ENUM_ID2                             0x10
#define ATOM_ENCODER_ENUM_ID3                             0x20
#define ATOM_ENCODER_ENUM_ID4                             0x30
#define ATOM_ENCODER_ENUM_ID5                             0x40 
#define ATOM_ENCODER_ENUM_ID6                             0x50

#define ATOM_DEVICE_CRT1_INDEX                            0x00000000
#define ATOM_DEVICE_LCD1_INDEX                            0x00000001
#define ATOM_DEVICE_TV1_INDEX                             0x00000002
#define ATOM_DEVICE_DFP1_INDEX                            0x00000003
#define ATOM_DEVICE_CRT2_INDEX                            0x00000004
#define ATOM_DEVICE_LCD2_INDEX                            0x00000005
#define ATOM_DEVICE_DFP6_INDEX                            0x00000006
#define ATOM_DEVICE_DFP2_INDEX                            0x00000007
#define ATOM_DEVICE_CV_INDEX                              0x00000008
#define ATOM_DEVICE_DFP3_INDEX                            0x00000009
#define ATOM_DEVICE_DFP4_INDEX                            0x0000000A
#define ATOM_DEVICE_DFP5_INDEX                            0x0000000B

#define ATOM_DEVICE_RESERVEDC_INDEX                       0x0000000C
#define ATOM_DEVICE_RESERVEDD_INDEX                       0x0000000D
#define ATOM_DEVICE_RESERVEDE_INDEX                       0x0000000E
#define ATOM_DEVICE_RESERVEDF_INDEX                       0x0000000F
#define ATOM_MAX_SUPPORTED_DEVICE_INFO                    (ATOM_DEVICE_DFP3_INDEX+1)
#define ATOM_MAX_SUPPORTED_DEVICE_INFO_2                  ATOM_MAX_SUPPORTED_DEVICE_INFO
#define ATOM_MAX_SUPPORTED_DEVICE_INFO_3                  (ATOM_DEVICE_DFP5_INDEX + 1 )

#define ATOM_MAX_SUPPORTED_DEVICE                         (ATOM_DEVICE_RESERVEDF_INDEX+1)

#define ATOM_DEVICE_CRT1_SUPPORT                          (0x1L << ATOM_DEVICE_CRT1_INDEX )
#define ATOM_DEVICE_LCD1_SUPPORT                          (0x1L << ATOM_DEVICE_LCD1_INDEX )
#define ATOM_DEVICE_TV1_SUPPORT                           (0x1L << ATOM_DEVICE_TV1_INDEX  )
#define ATOM_DEVICE_DFP1_SUPPORT                          (0x1L << ATOM_DEVICE_DFP1_INDEX )
#define ATOM_DEVICE_CRT2_SUPPORT                          (0x1L << ATOM_DEVICE_CRT2_INDEX )
#define ATOM_DEVICE_LCD2_SUPPORT                          (0x1L << ATOM_DEVICE_LCD2_INDEX )
#define ATOM_DEVICE_DFP6_SUPPORT                          (0x1L << ATOM_DEVICE_DFP6_INDEX )
#define ATOM_DEVICE_DFP2_SUPPORT                          (0x1L << ATOM_DEVICE_DFP2_INDEX )
#define ATOM_DEVICE_CV_SUPPORT                            (0x1L << ATOM_DEVICE_CV_INDEX   )
#define ATOM_DEVICE_DFP3_SUPPORT                          (0x1L << ATOM_DEVICE_DFP3_INDEX )
#define ATOM_DEVICE_DFP4_SUPPORT                          (0x1L << ATOM_DEVICE_DFP4_INDEX )
#define ATOM_DEVICE_DFP5_SUPPORT                          (0x1L << ATOM_DEVICE_DFP5_INDEX )

#define ATOM_DEVICE_CRT_SUPPORT                           (ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_CRT2_SUPPORT)
#define ATOM_DEVICE_DFP_SUPPORT                           (ATOM_DEVICE_DFP1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT |  ATOM_DEVICE_DFP3_SUPPORT | ATOM_DEVICE_DFP4_SUPPORT | ATOM_DEVICE_DFP5_SUPPORT | ATOM_DEVICE_DFP6_SUPPORT)
#define ATOM_DEVICE_TV_SUPPORT                            (ATOM_DEVICE_TV1_SUPPORT)
#define ATOM_DEVICE_LCD_SUPPORT                           (ATOM_DEVICE_LCD1_SUPPORT | ATOM_DEVICE_LCD2_SUPPORT)

#define ATOM_DEVICE_CONNECTOR_TYPE_MASK                   0x000000F0
#define ATOM_DEVICE_CONNECTOR_TYPE_SHIFT                  0x00000004
#define ATOM_DEVICE_CONNECTOR_VGA                         0x00000001
#define ATOM_DEVICE_CONNECTOR_DVI_I                       0x00000002
#define ATOM_DEVICE_CONNECTOR_DVI_D                       0x00000003
#define ATOM_DEVICE_CONNECTOR_DVI_A                       0x00000004
#define ATOM_DEVICE_CONNECTOR_SVIDEO                      0x00000005
#define ATOM_DEVICE_CONNECTOR_COMPOSITE                   0x00000006
#define ATOM_DEVICE_CONNECTOR_LVDS                        0x00000007
#define ATOM_DEVICE_CONNECTOR_DIGI_LINK                   0x00000008
#define ATOM_DEVICE_CONNECTOR_SCART                       0x00000009
#define ATOM_DEVICE_CONNECTOR_HDMI_TYPE_A                 0x0000000A
#define ATOM_DEVICE_CONNECTOR_HDMI_TYPE_B                 0x0000000B
#define ATOM_DEVICE_CONNECTOR_CASE_1                      0x0000000E
#define ATOM_DEVICE_CONNECTOR_DISPLAYPORT                 0x0000000F


#define ATOM_DEVICE_DAC_INFO_MASK                         0x0000000F
#define ATOM_DEVICE_DAC_INFO_SHIFT                        0x00000000
#define ATOM_DEVICE_DAC_INFO_NODAC                        0x00000000
#define ATOM_DEVICE_DAC_INFO_DACA                         0x00000001
#define ATOM_DEVICE_DAC_INFO_DACB                         0x00000002
#define ATOM_DEVICE_DAC_INFO_EXDAC                        0x00000003

#define ATOM_DEVICE_I2C_ID_NOI2C                          0x00000000

#define ATOM_DEVICE_I2C_LINEMUX_MASK                      0x0000000F
#define ATOM_DEVICE_I2C_LINEMUX_SHIFT                     0x00000000

#define ATOM_DEVICE_I2C_ID_MASK                           0x00000070
#define ATOM_DEVICE_I2C_ID_SHIFT                          0x00000004
#define ATOM_DEVICE_I2C_ID_IS_FOR_NON_MM_USE              0x00000001
#define ATOM_DEVICE_I2C_ID_IS_FOR_MM_USE                  0x00000002
#define ATOM_DEVICE_I2C_ID_IS_FOR_SDVO_USE                0x00000003    
#define ATOM_DEVICE_I2C_ID_IS_FOR_DAC_SCL                 0x00000004    

#define ATOM_DEVICE_I2C_HARDWARE_CAP_MASK                 0x00000080
#define ATOM_DEVICE_I2C_HARDWARE_CAP_SHIFT                0x00000007
#define	ATOM_DEVICE_USES_SOFTWARE_ASSISTED_I2C            0x00000000
#define	ATOM_DEVICE_USES_HARDWARE_ASSISTED_I2C            0x00000001



typedef struct _ATOM_I2C_ID_CONFIG
{
#if ATOM_BIG_ENDIAN
  UCHAR   bfHW_Capable:1;
  UCHAR   bfHW_EngineID:3;
  UCHAR   bfI2C_LineMux:4;
#else
  UCHAR   bfI2C_LineMux:4;
  UCHAR   bfHW_EngineID:3;
  UCHAR   bfHW_Capable:1;
#endif
}ATOM_I2C_ID_CONFIG;

typedef union _ATOM_I2C_ID_CONFIG_ACCESS
{
  ATOM_I2C_ID_CONFIG sbfAccess;
  UCHAR              ucAccess;
}ATOM_I2C_ID_CONFIG_ACCESS;
   

	
	
typedef struct _ATOM_GPIO_I2C_ASSIGMENT
{
  USHORT                    usClkMaskRegisterIndex;
  USHORT                    usClkEnRegisterIndex;
  USHORT                    usClkY_RegisterIndex;
  USHORT                    usClkA_RegisterIndex;
  USHORT                    usDataMaskRegisterIndex;
  USHORT                    usDataEnRegisterIndex;
  USHORT                    usDataY_RegisterIndex;
  USHORT                    usDataA_RegisterIndex;
  ATOM_I2C_ID_CONFIG_ACCESS sucI2cId;
  UCHAR                     ucClkMaskShift;
  UCHAR                     ucClkEnShift;
  UCHAR                     ucClkY_Shift;
  UCHAR                     ucClkA_Shift;
  UCHAR                     ucDataMaskShift;
  UCHAR                     ucDataEnShift;
  UCHAR                     ucDataY_Shift;
  UCHAR                     ucDataA_Shift;
  UCHAR                     ucReserved1;
  UCHAR                     ucReserved2;
}ATOM_GPIO_I2C_ASSIGMENT;

typedef struct _ATOM_GPIO_I2C_INFO
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  ATOM_GPIO_I2C_ASSIGMENT   asGPIO_Info[ATOM_MAX_SUPPORTED_DEVICE];
}ATOM_GPIO_I2C_INFO;

	
	

#ifndef _H2INC
  
typedef struct _ATOM_MODE_MISC_INFO
{ 
#if ATOM_BIG_ENDIAN
  USHORT Reserved:6;
  USHORT RGB888:1;
  USHORT DoubleClock:1;
  USHORT Interlace:1;
  USHORT CompositeSync:1;
  USHORT V_ReplicationBy2:1;
  USHORT H_ReplicationBy2:1;
  USHORT VerticalCutOff:1;
  USHORT VSyncPolarity:1;      
  USHORT HSyncPolarity:1;      
  USHORT HorizontalCutOff:1;
#else
  USHORT HorizontalCutOff:1;
  USHORT HSyncPolarity:1;      
  USHORT VSyncPolarity:1;      
  USHORT VerticalCutOff:1;
  USHORT H_ReplicationBy2:1;
  USHORT V_ReplicationBy2:1;
  USHORT CompositeSync:1;
  USHORT Interlace:1;
  USHORT DoubleClock:1;
  USHORT RGB888:1;
  USHORT Reserved:6;           
#endif
}ATOM_MODE_MISC_INFO;
  
typedef union _ATOM_MODE_MISC_INFO_ACCESS
{ 
  ATOM_MODE_MISC_INFO sbfAccess;
  USHORT              usAccess;
}ATOM_MODE_MISC_INFO_ACCESS;
  
#else
  
typedef union _ATOM_MODE_MISC_INFO_ACCESS
{ 
  USHORT              usAccess;
}ATOM_MODE_MISC_INFO_ACCESS;
   
#endif

#define ATOM_H_CUTOFF           0x01
#define ATOM_HSYNC_POLARITY     0x02             
#define ATOM_VSYNC_POLARITY     0x04             
#define ATOM_V_CUTOFF           0x08
#define ATOM_H_REPLICATIONBY2   0x10
#define ATOM_V_REPLICATIONBY2   0x20
#define ATOM_COMPOSITESYNC      0x40
#define ATOM_INTERLACE          0x80
#define ATOM_DOUBLE_CLOCK_MODE  0x100
#define ATOM_RGB888_MODE        0x200

#define ATOM_REFRESH_43         43
#define ATOM_REFRESH_47         47
#define ATOM_REFRESH_56         56	
#define ATOM_REFRESH_60         60
#define ATOM_REFRESH_65         65
#define ATOM_REFRESH_70         70
#define ATOM_REFRESH_72         72
#define ATOM_REFRESH_75         75
#define ATOM_REFRESH_85         85


	
	
typedef struct _SET_CRTC_USING_DTD_TIMING_PARAMETERS
{
  USHORT  usH_Size;
  USHORT  usH_Blanking_Time;
  USHORT  usV_Size;
  USHORT  usV_Blanking_Time;			
  USHORT  usH_SyncOffset;
  USHORT  usH_SyncWidth;
  USHORT  usV_SyncOffset;
  USHORT  usV_SyncWidth;
  ATOM_MODE_MISC_INFO_ACCESS  susModeMiscInfo;  
  UCHAR   ucH_Border;         
  UCHAR   ucV_Border;
  UCHAR   ucCRTC;             
  UCHAR   ucPadding[3];
}SET_CRTC_USING_DTD_TIMING_PARAMETERS;

	
	
typedef struct _SET_CRTC_TIMING_PARAMETERS
{
  USHORT                      usH_Total;        
  USHORT                      usH_Disp;         
  USHORT                      usH_SyncStart;    
  USHORT                      usH_SyncWidth;    
  USHORT                      usV_Total;        
  USHORT                      usV_Disp;         
  USHORT                      usV_SyncStart;    
  USHORT                      usV_SyncWidth;    
  ATOM_MODE_MISC_INFO_ACCESS  susModeMiscInfo;
  UCHAR                       ucCRTC;           
  UCHAR                       ucOverscanRight;  
  UCHAR                       ucOverscanLeft;   
  UCHAR                       ucOverscanBottom; 
  UCHAR                       ucOverscanTop;    
  UCHAR                       ucReserved;
}SET_CRTC_TIMING_PARAMETERS;
#define SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION SET_CRTC_TIMING_PARAMETERS

	
	
typedef struct _ATOM_MODE_TIMING
{
  USHORT  usCRTC_H_Total;
  USHORT  usCRTC_H_Disp;
  USHORT  usCRTC_H_SyncStart;
  USHORT  usCRTC_H_SyncWidth;
  USHORT  usCRTC_V_Total;
  USHORT  usCRTC_V_Disp;
  USHORT  usCRTC_V_SyncStart;
  USHORT  usCRTC_V_SyncWidth;
  USHORT  usPixelClock;					                 
  ATOM_MODE_MISC_INFO_ACCESS  susModeMiscInfo;
  USHORT  usCRTC_OverscanRight;
  USHORT  usCRTC_OverscanLeft;
  USHORT  usCRTC_OverscanBottom;
  USHORT  usCRTC_OverscanTop;
  USHORT  usReserve;
  UCHAR   ucInternalModeNumber;
  UCHAR   ucRefreshRate;
}ATOM_MODE_TIMING;

typedef struct _ATOM_DTD_FORMAT
{
  USHORT  usPixClk;
  USHORT  usHActive;
  USHORT  usHBlanking_Time;
  USHORT  usVActive;
  USHORT  usVBlanking_Time;			
  USHORT  usHSyncOffset;
  USHORT  usHSyncWidth;
  USHORT  usVSyncOffset;
  USHORT  usVSyncWidth;
  USHORT  usImageHSize;
  USHORT  usImageVSize;
  UCHAR   ucHBorder;
  UCHAR   ucVBorder;
  ATOM_MODE_MISC_INFO_ACCESS susModeMiscInfo;
  UCHAR   ucInternalModeNumber;
  UCHAR   ucRefreshRate;
}ATOM_DTD_FORMAT;

	
	
#define SUPPORTED_LCD_REFRESHRATE_30Hz          0x0004
#define SUPPORTED_LCD_REFRESHRATE_40Hz          0x0008
#define SUPPORTED_LCD_REFRESHRATE_50Hz          0x0010
#define SUPPORTED_LCD_REFRESHRATE_60Hz          0x0020

typedef struct _ATOM_LVDS_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_DTD_FORMAT     sLCDTiming;
  USHORT              usModePatchTableOffset;
  USHORT              usSupportedRefreshRate;     
  USHORT              usOffDelayInMs;
  UCHAR               ucPowerSequenceDigOntoDEin10Ms;
  UCHAR               ucPowerSequenceDEtoBLOnin10Ms;
  UCHAR               ucLVDS_Misc;               
                                                 
                                                 
                                                 
  UCHAR               ucPanelDefaultRefreshRate;
  UCHAR               ucPanelIdentification;
  UCHAR               ucSS_Id;
}ATOM_LVDS_INFO;

typedef struct _ATOM_LVDS_INFO_V12
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_DTD_FORMAT     sLCDTiming;
  USHORT              usExtInfoTableOffset;
  USHORT              usSupportedRefreshRate;     
  USHORT              usOffDelayInMs;
  UCHAR               ucPowerSequenceDigOntoDEin10Ms;
  UCHAR               ucPowerSequenceDEtoBLOnin10Ms;
  UCHAR               ucLVDS_Misc;               
                                                 
                                                 
                                                 
  UCHAR               ucPanelDefaultRefreshRate;
  UCHAR               ucPanelIdentification;
  UCHAR               ucSS_Id;
  USHORT              usLCDVenderID;
  USHORT              usLCDProductID;
  UCHAR               ucLCDPanel_SpecialHandlingCap; 
	UCHAR								ucPanelInfoSize;					
  UCHAR               ucReserved[2];
}ATOM_LVDS_INFO_V12;


#define	LCDPANEL_CAP_READ_EDID                  0x1

#define	LCDPANEL_CAP_DRR_SUPPORTED              0x2

#define	LCDPANEL_CAP_eDP                        0x4


                              
                              
                              
                              
                              
                              
                              
                              

#define PANEL_COLOR_BIT_DEPTH_MASK    0x70

#define PANEL_RANDOM_DITHER   0x80
#define PANEL_RANDOM_DITHER_MASK   0x80

#define ATOM_LVDS_INFO_LAST  ATOM_LVDS_INFO_V12   

	
	
typedef struct _ATOM_LCD_INFO_V13
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_DTD_FORMAT     sLCDTiming;
  USHORT              usExtInfoTableOffset;
  USHORT              usSupportedRefreshRate;     
  ULONG               ulReserved0;
  UCHAR               ucLCD_Misc;                
                                                 
                                                 
                                                 
                                                 
                                                 
  UCHAR               ucPanelDefaultRefreshRate;
  UCHAR               ucPanelIdentification;
  UCHAR               ucSS_Id;
  USHORT              usLCDVenderID;
  USHORT              usLCDProductID;
  UCHAR               ucLCDPanel_SpecialHandlingCap;  
                                                 
                                                 
                                                 
                                                 
  UCHAR               ucPanelInfoSize;					 
  USHORT              usBacklightPWM;            

  UCHAR               ucPowerSequenceDIGONtoDE_in4Ms;
  UCHAR               ucPowerSequenceDEtoVARY_BL_in4Ms;
  UCHAR               ucPowerSequenceVARY_BLtoDE_in4Ms;
  UCHAR               ucPowerSequenceDEtoDIGON_in4Ms;

  UCHAR               ucOffDelay_in4Ms;
  UCHAR               ucPowerSequenceVARY_BLtoBLON_in4Ms;
  UCHAR               ucPowerSequenceBLONtoVARY_BL_in4Ms;
  UCHAR               ucReserved1;

  UCHAR               ucDPCD_eDP_CONFIGURATION_CAP;     
  UCHAR               ucDPCD_MAX_LINK_RATE;             
  UCHAR               ucDPCD_MAX_LANE_COUNT;            
  UCHAR               ucDPCD_MAX_DOWNSPREAD;            

  USHORT              usMaxPclkFreqInSingleLink;        
  UCHAR               uceDPToLVDSRxId;
  UCHAR               ucLcdReservd;
  ULONG               ulReserved[2];
}ATOM_LCD_INFO_V13;  

#define ATOM_LCD_INFO_LAST  ATOM_LCD_INFO_V13    

#define ATOM_PANEL_MISC_V13_DUAL                   0x00000001
#define ATOM_PANEL_MISC_V13_FPDI                   0x00000002
#define ATOM_PANEL_MISC_V13_GREY_LEVEL             0x0000000C
#define ATOM_PANEL_MISC_V13_GREY_LEVEL_SHIFT       2
#define ATOM_PANEL_MISC_V13_COLOR_BIT_DEPTH_MASK   0x70
#define ATOM_PANEL_MISC_V13_6BIT_PER_COLOR         0x10
#define ATOM_PANEL_MISC_V13_8BIT_PER_COLOR         0x20

                              
                              
                              
                              
                              
                              
                              
                              
 

#define	LCDPANEL_CAP_V13_READ_EDID              0x1        

#define	LCDPANEL_CAP_V13_DRR_SUPPORTED          0x2        

#define	LCDPANEL_CAP_V13_eDP                    0x4        

#define eDP_TO_LVDS_RX_DISABLE                  0x00       
#define eDP_TO_LVDS_COMMON_ID                   0x01       
#define eDP_TO_LVDS_RT_ID                       0x02       

typedef struct  _ATOM_PATCH_RECORD_MODE
{
  UCHAR     ucRecordType;
  USHORT    usHDisp;
  USHORT    usVDisp;
}ATOM_PATCH_RECORD_MODE;

typedef struct  _ATOM_LCD_RTS_RECORD
{
  UCHAR     ucRecordType;
  UCHAR     ucRTSValue;
}ATOM_LCD_RTS_RECORD;

typedef struct  _ATOM_LCD_MODE_CONTROL_CAP
{
  UCHAR     ucRecordType;
  USHORT    usLCDCap;
}ATOM_LCD_MODE_CONTROL_CAP;

#define LCD_MODE_CAP_BL_OFF                   1
#define LCD_MODE_CAP_CRTC_OFF                 2
#define LCD_MODE_CAP_PANEL_OFF                4

typedef struct _ATOM_FAKE_EDID_PATCH_RECORD
{
  UCHAR ucRecordType;
  UCHAR ucFakeEDIDLength;
  UCHAR ucFakeEDIDString[1];    
} ATOM_FAKE_EDID_PATCH_RECORD;

typedef struct  _ATOM_PANEL_RESOLUTION_PATCH_RECORD
{
   UCHAR    ucRecordType;
   USHORT		usHSize;
   USHORT		usVSize;
}ATOM_PANEL_RESOLUTION_PATCH_RECORD;

#define LCD_MODE_PATCH_RECORD_MODE_TYPE       1
#define LCD_RTS_RECORD_TYPE                   2
#define LCD_CAP_RECORD_TYPE                   3
#define LCD_FAKE_EDID_PATCH_RECORD_TYPE       4
#define LCD_PANEL_RESOLUTION_RECORD_TYPE      5
#define LCD_EDID_OFFSET_PATCH_RECORD_TYPE     6
#define ATOM_RECORD_END_TYPE                  0xFF


typedef struct _ATOM_SPREAD_SPECTRUM_ASSIGNMENT
{
  USHORT              usSpreadSpectrumPercentage; 
  UCHAR               ucSpreadSpectrumType;	    
  UCHAR               ucSS_Step;
  UCHAR               ucSS_Delay;
  UCHAR               ucSS_Id;
  UCHAR               ucRecommendedRef_Div;
  UCHAR               ucSS_Range;               
}ATOM_SPREAD_SPECTRUM_ASSIGNMENT;

#define ATOM_MAX_SS_ENTRY                      16
#define ATOM_DP_SS_ID1												 0x0f1			
#define ATOM_DP_SS_ID2												 0x0f2			
#define ATOM_LVLINK_2700MHz_SS_ID              0x0f3      
#define ATOM_LVLINK_1620MHz_SS_ID              0x0f4      


#define ATOM_SS_DOWN_SPREAD_MODE_MASK          0x00000000
#define ATOM_SS_DOWN_SPREAD_MODE               0x00000000
#define ATOM_SS_CENTRE_SPREAD_MODE_MASK        0x00000001
#define ATOM_SS_CENTRE_SPREAD_MODE             0x00000001
#define ATOM_INTERNAL_SS_MASK                  0x00000000
#define ATOM_EXTERNAL_SS_MASK                  0x00000002
#define EXEC_SS_STEP_SIZE_SHIFT                2
#define EXEC_SS_DELAY_SHIFT                    4    
#define ACTIVEDATA_TO_BLON_DELAY_SHIFT         4

typedef struct _ATOM_SPREAD_SPECTRUM_INFO
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  ATOM_SPREAD_SPECTRUM_ASSIGNMENT   asSS_Info[ATOM_MAX_SS_ENTRY];
}ATOM_SPREAD_SPECTRUM_INFO;

	
	


#define NTSC_SUPPORT          0x1
#define NTSCJ_SUPPORT         0x2

#define PAL_SUPPORT           0x4
#define PALM_SUPPORT          0x8
#define PALCN_SUPPORT         0x10
#define PALN_SUPPORT          0x20
#define PAL60_SUPPORT         0x40
#define SECAM_SUPPORT         0x80

#define MAX_SUPPORTED_TV_TIMING    2

typedef struct _ATOM_ANALOG_TV_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  UCHAR                    ucTV_SupportedStandard;
  UCHAR                    ucTV_BootUpDefaultStandard; 
  UCHAR                    ucExt_TV_ASIC_ID;
  UCHAR                    ucExt_TV_ASIC_SlaveAddr;
  
  ATOM_MODE_TIMING         aModeTimings[MAX_SUPPORTED_TV_TIMING];
}ATOM_ANALOG_TV_INFO;

#define MAX_SUPPORTED_TV_TIMING_V1_2    3

typedef struct _ATOM_ANALOG_TV_INFO_V1_2
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  UCHAR                    ucTV_SupportedStandard;
  UCHAR                    ucTV_BootUpDefaultStandard; 
  UCHAR                    ucExt_TV_ASIC_ID;
  UCHAR                    ucExt_TV_ASIC_SlaveAddr;
  ATOM_DTD_FORMAT          aModeTimings[MAX_SUPPORTED_TV_TIMING_V1_2];
}ATOM_ANALOG_TV_INFO_V1_2;

typedef struct _ATOM_DPCD_INFO
{
  UCHAR   ucRevisionNumber;        
  UCHAR   ucMaxLinkRate;           
  UCHAR   ucMaxLane;               
  UCHAR   ucMaxDownSpread;         
}ATOM_DPCD_INFO;

#define ATOM_DPCD_MAX_LANE_MASK    0x1F



#ifndef VESA_MEMORY_IN_64K_BLOCK
#define VESA_MEMORY_IN_64K_BLOCK        0x100       
#endif

#define ATOM_EDID_RAW_DATASIZE          256         
#define ATOM_HWICON_SURFACE_SIZE        4096        
#define ATOM_HWICON_INFOTABLE_SIZE      32
#define MAX_DTD_MODE_IN_VRAM            6
#define ATOM_DTD_MODE_SUPPORT_TBL_SIZE  (MAX_DTD_MODE_IN_VRAM*28)    
#define ATOM_STD_MODE_SUPPORT_TBL_SIZE  32*8                         
#define DFP_ENCODER_TYPE_OFFSET         (ATOM_EDID_RAW_DATASIZE + ATOM_DTD_MODE_SUPPORT_TBL_SIZE + ATOM_STD_MODE_SUPPORT_TBL_SIZE - 20)    
#define ATOM_DP_DPCD_OFFSET             (DFP_ENCODER_TYPE_OFFSET + 4 )        

#define ATOM_HWICON1_SURFACE_ADDR       0
#define ATOM_HWICON2_SURFACE_ADDR       (ATOM_HWICON1_SURFACE_ADDR + ATOM_HWICON_SURFACE_SIZE)
#define ATOM_HWICON_INFOTABLE_ADDR      (ATOM_HWICON2_SURFACE_ADDR + ATOM_HWICON_SURFACE_SIZE)
#define ATOM_CRT1_EDID_ADDR             (ATOM_HWICON_INFOTABLE_ADDR + ATOM_HWICON_INFOTABLE_SIZE)
#define ATOM_CRT1_DTD_MODE_TBL_ADDR     (ATOM_CRT1_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_CRT1_STD_MODE_TBL_ADDR	    (ATOM_CRT1_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_LCD1_EDID_ADDR             (ATOM_CRT1_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_LCD1_DTD_MODE_TBL_ADDR     (ATOM_LCD1_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_LCD1_STD_MODE_TBL_ADDR   	(ATOM_LCD1_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_TV1_DTD_MODE_TBL_ADDR      (ATOM_LCD1_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP1_EDID_ADDR             (ATOM_TV1_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP1_DTD_MODE_TBL_ADDR     (ATOM_DFP1_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP1_STD_MODE_TBL_ADDR	    (ATOM_DFP1_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_CRT2_EDID_ADDR             (ATOM_DFP1_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_CRT2_DTD_MODE_TBL_ADDR     (ATOM_CRT2_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_CRT2_STD_MODE_TBL_ADDR	    (ATOM_CRT2_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_LCD2_EDID_ADDR             (ATOM_CRT2_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_LCD2_DTD_MODE_TBL_ADDR     (ATOM_LCD2_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_LCD2_STD_MODE_TBL_ADDR   	(ATOM_LCD2_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP6_EDID_ADDR             (ATOM_LCD2_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP6_DTD_MODE_TBL_ADDR     (ATOM_DFP6_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP6_STD_MODE_TBL_ADDR     (ATOM_DFP6_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP2_EDID_ADDR             (ATOM_DFP6_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP2_DTD_MODE_TBL_ADDR     (ATOM_DFP2_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP2_STD_MODE_TBL_ADDR     (ATOM_DFP2_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_CV_EDID_ADDR               (ATOM_DFP2_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_CV_DTD_MODE_TBL_ADDR       (ATOM_CV_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_CV_STD_MODE_TBL_ADDR       (ATOM_CV_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP3_EDID_ADDR             (ATOM_CV_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP3_DTD_MODE_TBL_ADDR     (ATOM_DFP3_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP3_STD_MODE_TBL_ADDR     (ATOM_DFP3_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP4_EDID_ADDR             (ATOM_DFP3_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP4_DTD_MODE_TBL_ADDR     (ATOM_DFP4_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP4_STD_MODE_TBL_ADDR     (ATOM_DFP4_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DFP5_EDID_ADDR             (ATOM_DFP4_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)
#define ATOM_DFP5_DTD_MODE_TBL_ADDR     (ATOM_DFP5_EDID_ADDR + ATOM_EDID_RAW_DATASIZE)
#define ATOM_DFP5_STD_MODE_TBL_ADDR     (ATOM_DFP5_DTD_MODE_TBL_ADDR + ATOM_DTD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_DP_TRAINING_TBL_ADDR       (ATOM_DFP5_STD_MODE_TBL_ADDR + ATOM_STD_MODE_SUPPORT_TBL_SIZE)

#define ATOM_STACK_STORAGE_START        (ATOM_DP_TRAINING_TBL_ADDR + 1024)       
#define ATOM_STACK_STORAGE_END          ATOM_STACK_STORAGE_START + 512        

#define ATOM_VRAM_RESERVE_SIZE         ((((ATOM_STACK_STORAGE_END - ATOM_HWICON1_SURFACE_ADDR)>>10)+4)&0xFFFC)
   
#define ATOM_VRAM_RESERVE_V2_SIZE      32

#define	ATOM_VRAM_OPERATION_FLAGS_MASK         0xC0000000L
#define ATOM_VRAM_OPERATION_FLAGS_SHIFT        30
#define	ATOM_VRAM_BLOCK_NEEDS_NO_RESERVATION   0x1
#define	ATOM_VRAM_BLOCK_NEEDS_RESERVATION      0x0

	
	

	
#define ATOM_MAX_FIRMWARE_VRAM_USAGE_INFO			1

typedef struct _ATOM_FIRMWARE_VRAM_RESERVE_INFO
{
  ULONG   ulStartAddrUsedByFirmware;
  USHORT  usFirmwareUseInKb;
  USHORT  usReserved;
}ATOM_FIRMWARE_VRAM_RESERVE_INFO;

typedef struct _ATOM_VRAM_USAGE_BY_FIRMWARE
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_FIRMWARE_VRAM_RESERVE_INFO	asFirmwareVramReserveInfo[ATOM_MAX_FIRMWARE_VRAM_USAGE_INFO];
}ATOM_VRAM_USAGE_BY_FIRMWARE;

typedef struct _ATOM_FIRMWARE_VRAM_RESERVE_INFO_V1_5
{
  ULONG   ulStartAddrUsedByFirmware;
  USHORT  usFirmwareUseInKb;
  USHORT  usFBUsedByDrvInKb;
}ATOM_FIRMWARE_VRAM_RESERVE_INFO_V1_5;

typedef struct _ATOM_VRAM_USAGE_BY_FIRMWARE_V1_5
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_FIRMWARE_VRAM_RESERVE_INFO_V1_5	asFirmwareVramReserveInfo[ATOM_MAX_FIRMWARE_VRAM_USAGE_INFO];
}ATOM_VRAM_USAGE_BY_FIRMWARE_V1_5;

	
	
typedef struct _ATOM_GPIO_PIN_ASSIGNMENT
{
  USHORT                   usGpioPin_AIndex;
  UCHAR                    ucGpioPinBitShift;
  UCHAR                    ucGPIO_ID;
}ATOM_GPIO_PIN_ASSIGNMENT;

typedef struct _ATOM_GPIO_PIN_LUT
{
  ATOM_COMMON_TABLE_HEADER  sHeader;
  ATOM_GPIO_PIN_ASSIGNMENT	asGPIO_Pin[1];
}ATOM_GPIO_PIN_LUT;

	
	
#define GPIO_PIN_ACTIVE_HIGH          0x1

#define MAX_SUPPORTED_CV_STANDARDS    5

#define ATOM_GPIO_SETTINGS_BITSHIFT_MASK  0x1F    
#define ATOM_GPIO_SETTINGS_RESERVED_MASK  0x60    
#define ATOM_GPIO_SETTINGS_ACTIVE_MASK    0x80    

typedef struct _ATOM_GPIO_INFO
{
  USHORT  usAOffset;
  UCHAR   ucSettings;
  UCHAR   ucReserved;
}ATOM_GPIO_INFO;

#define ATOM_CV_RESTRICT_FORMAT_SELECTION           0x2

#define ATOM_GPIO_DEFAULT_MODE_EN                   0x80 
#define ATOM_GPIO_SETTING_PERMODE_MASK              0x7F 

#define ATOM_CV_LINE3_ASPECTRATIO_16_9_GPIO_A       0x01     
#define ATOM_CV_LINE3_ASPECTRATIO_16_9_GPIO_B       0x02     
#define ATOM_CV_LINE3_ASPECTRATIO_16_9_GPIO_SHIFT   0x0   

#define ATOM_CV_LINE3_ASPECTRATIO_4_3_LETBOX_GPIO_A 0x04     
#define ATOM_CV_LINE3_ASPECTRATIO_4_3_LETBOX_GPIO_B 0x08     
#define ATOM_CV_LINE3_ASPECTRATIO_4_3_LETBOX_GPIO_SHIFT 0x2     

#define ATOM_CV_LINE3_ASPECTRATIO_4_3_GPIO_A        0x10     
#define ATOM_CV_LINE3_ASPECTRATIO_4_3_GPIO_B        0x20     
#define ATOM_CV_LINE3_ASPECTRATIO_4_3_GPIO_SHIFT    0x4 

#define ATOM_CV_LINE3_ASPECTRATIO_MASK              0x3F     

#define ATOM_CV_LINE3_ASPECTRATIO_EXIST             0x80     

#define ATOM_GPIO_INDEX_LINE3_ASPECRATIO_GPIO_A   3   
#define ATOM_GPIO_INDEX_LINE3_ASPECRATIO_GPIO_B   4   


typedef struct _ATOM_COMPONENT_VIDEO_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;
  USHORT             usMask_PinRegisterIndex;
  USHORT             usEN_PinRegisterIndex;
  USHORT             usY_PinRegisterIndex;
  USHORT             usA_PinRegisterIndex;
  UCHAR              ucBitShift;
  UCHAR              ucPinActiveState;  
  ATOM_DTD_FORMAT    sReserved;         
  UCHAR              ucMiscInfo;
  UCHAR              uc480i;
  UCHAR              uc480p;
  UCHAR              uc720p;
  UCHAR              uc1080i;
  UCHAR              ucLetterBoxMode;
  UCHAR              ucReserved[3];
  UCHAR              ucNumOfWbGpioBlocks; 
  ATOM_GPIO_INFO     aWbGpioStateBlock[MAX_SUPPORTED_CV_STANDARDS];
  ATOM_DTD_FORMAT    aModeTimings[MAX_SUPPORTED_CV_STANDARDS];
}ATOM_COMPONENT_VIDEO_INFO;

typedef struct _ATOM_COMPONENT_VIDEO_INFO_V21
{
  ATOM_COMMON_TABLE_HEADER sHeader;
  UCHAR              ucMiscInfo;
  UCHAR              uc480i;
  UCHAR              uc480p;
  UCHAR              uc720p;
  UCHAR              uc1080i;
  UCHAR              ucReserved;
  UCHAR              ucLetterBoxMode;
  UCHAR              ucNumOfWbGpioBlocks; 
  ATOM_GPIO_INFO     aWbGpioStateBlock[MAX_SUPPORTED_CV_STANDARDS];
  ATOM_DTD_FORMAT    aModeTimings[MAX_SUPPORTED_CV_STANDARDS];
}ATOM_COMPONENT_VIDEO_INFO_V21;

#define ATOM_COMPONENT_VIDEO_INFO_LAST  ATOM_COMPONENT_VIDEO_INFO_V21

	
	
typedef struct _ATOM_OBJECT_HEADER
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  USHORT                    usDeviceSupport;
  USHORT                    usConnectorObjectTableOffset;
  USHORT                    usRouterObjectTableOffset;
  USHORT                    usEncoderObjectTableOffset;
  USHORT                    usProtectionObjectTableOffset; 
  USHORT                    usDisplayPathTableOffset;
}ATOM_OBJECT_HEADER;

typedef struct _ATOM_OBJECT_HEADER_V3
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  USHORT                    usDeviceSupport;
  USHORT                    usConnectorObjectTableOffset;
  USHORT                    usRouterObjectTableOffset;
  USHORT                    usEncoderObjectTableOffset;
  USHORT                    usProtectionObjectTableOffset; 
  USHORT                    usDisplayPathTableOffset;
  USHORT                    usMiscObjectTableOffset;
}ATOM_OBJECT_HEADER_V3;

typedef struct  _ATOM_DISPLAY_OBJECT_PATH
{
  USHORT    usDeviceTag;                                   
  USHORT    usSize;                                        
  USHORT    usConnObjectId;                                
  USHORT    usGPUObjectId;                                 
  USHORT    usGraphicObjIds[1];                             
}ATOM_DISPLAY_OBJECT_PATH;

typedef struct  _ATOM_DISPLAY_EXTERNAL_OBJECT_PATH
{
  USHORT    usDeviceTag;                                   
  USHORT    usSize;                                        
  USHORT    usConnObjectId;                                
  USHORT    usGPUObjectId;                                 
  USHORT    usGraphicObjIds[2];                            
}ATOM_DISPLAY_EXTERNAL_OBJECT_PATH;

typedef struct _ATOM_DISPLAY_OBJECT_PATH_TABLE
{
  UCHAR                           ucNumOfDispPath;
  UCHAR                           ucVersion;
  UCHAR                           ucPadding[2];
  ATOM_DISPLAY_OBJECT_PATH        asDispPath[1];
}ATOM_DISPLAY_OBJECT_PATH_TABLE;


typedef struct _ATOM_OBJECT                                
{
  USHORT              usObjectID;
  USHORT              usSrcDstTableOffset;
  USHORT              usRecordOffset;                     
  USHORT              usReserved;
}ATOM_OBJECT;

typedef struct _ATOM_OBJECT_TABLE                         
{
  UCHAR               ucNumberOfObjects;
  UCHAR               ucPadding[3];
  ATOM_OBJECT         asObjects[1];
}ATOM_OBJECT_TABLE;

typedef struct _ATOM_SRC_DST_TABLE_FOR_ONE_OBJECT         
{
  UCHAR               ucNumberOfSrc;
  USHORT              usSrcObjectID[1];
  UCHAR               ucNumberOfDst;
  USHORT              usDstObjectID[1];
}ATOM_SRC_DST_TABLE_FOR_ONE_OBJECT;



#define EXT_HPDPIN_LUTINDEX_0                   0
#define EXT_HPDPIN_LUTINDEX_1                   1
#define EXT_HPDPIN_LUTINDEX_2                   2
#define EXT_HPDPIN_LUTINDEX_3                   3
#define EXT_HPDPIN_LUTINDEX_4                   4
#define EXT_HPDPIN_LUTINDEX_5                   5
#define EXT_HPDPIN_LUTINDEX_6                   6
#define EXT_HPDPIN_LUTINDEX_7                   7
#define MAX_NUMBER_OF_EXT_HPDPIN_LUT_ENTRIES   (EXT_HPDPIN_LUTINDEX_7+1)

#define EXT_AUXDDC_LUTINDEX_0                   0
#define EXT_AUXDDC_LUTINDEX_1                   1
#define EXT_AUXDDC_LUTINDEX_2                   2
#define EXT_AUXDDC_LUTINDEX_3                   3
#define EXT_AUXDDC_LUTINDEX_4                   4
#define EXT_AUXDDC_LUTINDEX_5                   5
#define EXT_AUXDDC_LUTINDEX_6                   6
#define EXT_AUXDDC_LUTINDEX_7                   7
#define MAX_NUMBER_OF_EXT_AUXDDC_LUT_ENTRIES   (EXT_AUXDDC_LUTINDEX_7+1)

typedef struct _ATOM_DP_CONN_CHANNEL_MAPPING
{
#if ATOM_BIG_ENDIAN
  UCHAR ucDP_Lane3_Source:2;
  UCHAR ucDP_Lane2_Source:2;
  UCHAR ucDP_Lane1_Source:2;
  UCHAR ucDP_Lane0_Source:2;
#else
  UCHAR ucDP_Lane0_Source:2;
  UCHAR ucDP_Lane1_Source:2;
  UCHAR ucDP_Lane2_Source:2;
  UCHAR ucDP_Lane3_Source:2;
#endif
}ATOM_DP_CONN_CHANNEL_MAPPING;

typedef struct _ATOM_DVI_CONN_CHANNEL_MAPPING
{
#if ATOM_BIG_ENDIAN
  UCHAR ucDVI_CLK_Source:2;
  UCHAR ucDVI_DATA0_Source:2;
  UCHAR ucDVI_DATA1_Source:2;
  UCHAR ucDVI_DATA2_Source:2;
#else
  UCHAR ucDVI_DATA2_Source:2;
  UCHAR ucDVI_DATA1_Source:2;
  UCHAR ucDVI_DATA0_Source:2;
  UCHAR ucDVI_CLK_Source:2;
#endif
}ATOM_DVI_CONN_CHANNEL_MAPPING;

typedef struct _EXT_DISPLAY_PATH
{
  USHORT  usDeviceTag;                    
  USHORT  usDeviceACPIEnum;               
  USHORT  usDeviceConnector;              
  UCHAR   ucExtAUXDDCLutIndex;            
  UCHAR   ucExtHPDPINLutIndex;            
  USHORT  usExtEncoderObjId;              
  union{
    UCHAR   ucChannelMapping;                  
    ATOM_DP_CONN_CHANNEL_MAPPING asDPMapping;
    ATOM_DVI_CONN_CHANNEL_MAPPING asDVIMapping;
  };
  UCHAR   ucChPNInvert;                   
  USHORT  usCaps;
  USHORT  usReserved; 
}EXT_DISPLAY_PATH;
   
#define NUMBER_OF_UCHAR_FOR_GUID          16
#define MAX_NUMBER_OF_EXT_DISPLAY_PATH    7

#define  EXT_DISPLAY_PATH_CAPS__HBR2_DISABLE          0x01

typedef  struct _ATOM_EXTERNAL_DISPLAY_CONNECTION_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;
  UCHAR                    ucGuid [NUMBER_OF_UCHAR_FOR_GUID];     
  EXT_DISPLAY_PATH         sPath[MAX_NUMBER_OF_EXT_DISPLAY_PATH]; 
  UCHAR                    ucChecksum;                            
  UCHAR                    uc3DStereoPinId;                       
  UCHAR                    ucRemoteDisplayConfig;
  UCHAR                    uceDPToLVDSRxId;
  UCHAR                    Reserved[4];                           
}ATOM_EXTERNAL_DISPLAY_CONNECTION_INFO;

typedef struct _ATOM_COMMON_RECORD_HEADER
{
  UCHAR               ucRecordType;                      
  UCHAR               ucRecordSize;                      
}ATOM_COMMON_RECORD_HEADER;


#define ATOM_I2C_RECORD_TYPE                           1         
#define ATOM_HPD_INT_RECORD_TYPE                       2
#define ATOM_OUTPUT_PROTECTION_RECORD_TYPE             3
#define ATOM_CONNECTOR_DEVICE_TAG_RECORD_TYPE          4
#define	ATOM_CONNECTOR_DVI_EXT_INPUT_RECORD_TYPE	     5 
#define ATOM_ENCODER_FPGA_CONTROL_RECORD_TYPE          6 
#define ATOM_CONNECTOR_CVTV_SHARE_DIN_RECORD_TYPE      7
#define ATOM_JTAG_RECORD_TYPE                          8 
#define ATOM_OBJECT_GPIO_CNTL_RECORD_TYPE              9
#define ATOM_ENCODER_DVO_CF_RECORD_TYPE               10
#define ATOM_CONNECTOR_CF_RECORD_TYPE                 11
#define	ATOM_CONNECTOR_HARDCODE_DTD_RECORD_TYPE	      12
#define ATOM_CONNECTOR_PCIE_SUBCONNECTOR_RECORD_TYPE  13
#define ATOM_ROUTER_DDC_PATH_SELECT_RECORD_TYPE	      14
#define ATOM_ROUTER_DATA_CLOCK_PATH_SELECT_RECORD_TYPE	15
#define ATOM_CONNECTOR_HPDPIN_LUT_RECORD_TYPE          16 
#define ATOM_CONNECTOR_AUXDDC_LUT_RECORD_TYPE          17 
#define ATOM_OBJECT_LINK_RECORD_TYPE                   18 
#define ATOM_CONNECTOR_REMOTE_CAP_RECORD_TYPE          19
#define ATOM_ENCODER_CAP_RECORD_TYPE                   20


#define ATOM_MAX_OBJECT_RECORD_NUMBER             ATOM_ENCODER_CAP_RECORD_TYPE

typedef struct  _ATOM_I2C_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  ATOM_I2C_ID_CONFIG          sucI2cId; 
  UCHAR                       ucI2CAddr;              
}ATOM_I2C_RECORD;

typedef struct  _ATOM_HPD_INT_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucHPDIntGPIOID;         
  UCHAR                       ucPlugged_PinState;
}ATOM_HPD_INT_RECORD;


typedef struct  _ATOM_OUTPUT_PROTECTION_RECORD 
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucProtectionFlag;
  UCHAR                       ucReserved;
}ATOM_OUTPUT_PROTECTION_RECORD;

typedef struct  _ATOM_CONNECTOR_DEVICE_TAG
{
  ULONG                       ulACPIDeviceEnum;       
  USHORT                      usDeviceID;             
  USHORT                      usPadding;
}ATOM_CONNECTOR_DEVICE_TAG;

typedef struct  _ATOM_CONNECTOR_DEVICE_TAG_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucNumberOfDevice;
  UCHAR                       ucReserved;
  ATOM_CONNECTOR_DEVICE_TAG   asDeviceTag[1];         
}ATOM_CONNECTOR_DEVICE_TAG_RECORD;


typedef struct  _ATOM_CONNECTOR_DVI_EXT_INPUT_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR						            ucConfigGPIOID;
  UCHAR						            ucConfigGPIOState;	    
  UCHAR                       ucFlowinGPIPID;
  UCHAR                       ucExtInGPIPID;
}ATOM_CONNECTOR_DVI_EXT_INPUT_RECORD;

typedef struct  _ATOM_ENCODER_FPGA_CONTROL_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucCTL1GPIO_ID;
  UCHAR                       ucCTL1GPIOState;        
  UCHAR                       ucCTL2GPIO_ID;
  UCHAR                       ucCTL2GPIOState;        
  UCHAR                       ucCTL3GPIO_ID;
  UCHAR                       ucCTL3GPIOState;        
  UCHAR                       ucCTLFPGA_IN_ID;
  UCHAR                       ucPadding[3];
}ATOM_ENCODER_FPGA_CONTROL_RECORD;

typedef struct  _ATOM_CONNECTOR_CVTV_SHARE_DIN_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucGPIOID;               
  UCHAR                       ucTVActiveState;        
}ATOM_CONNECTOR_CVTV_SHARE_DIN_RECORD;

typedef struct  _ATOM_JTAG_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucTMSGPIO_ID;
  UCHAR                       ucTMSGPIOState;         
  UCHAR                       ucTCKGPIO_ID;
  UCHAR                       ucTCKGPIOState;         
  UCHAR                       ucTDOGPIO_ID;
  UCHAR                       ucTDOGPIOState;         
  UCHAR                       ucTDIGPIO_ID;
  UCHAR                       ucTDIGPIOState;         
  UCHAR                       ucPadding[2];
}ATOM_JTAG_RECORD;


typedef struct _ATOM_GPIO_PIN_CONTROL_PAIR
{
  UCHAR                       ucGPIOID;               
  UCHAR                       ucGPIO_PinState;        
}ATOM_GPIO_PIN_CONTROL_PAIR;

typedef struct  _ATOM_OBJECT_GPIO_CNTL_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucFlags;                
  UCHAR                       ucNumberOfPins;         
  ATOM_GPIO_PIN_CONTROL_PAIR  asGpio[1];              
}ATOM_OBJECT_GPIO_CNTL_RECORD;

#define GPIO_PIN_TYPE_INPUT             0x00
#define GPIO_PIN_TYPE_OUTPUT            0x10
#define GPIO_PIN_TYPE_HW_CONTROL        0x20

#define GPIO_PIN_OUTPUT_STATE_MASK      0x01
#define GPIO_PIN_OUTPUT_STATE_SHIFT     0
#define GPIO_PIN_STATE_ACTIVE_LOW       0x0
#define GPIO_PIN_STATE_ACTIVE_HIGH      0x1

#define ATOM_GPIO_INDEX_GLSYNC_REFCLK    0
#define ATOM_GPIO_INDEX_GLSYNC_HSYNC     1
#define ATOM_GPIO_INDEX_GLSYNC_VSYNC     2
#define ATOM_GPIO_INDEX_GLSYNC_SWAP_REQ  3
#define ATOM_GPIO_INDEX_GLSYNC_SWAP_GNT  4
#define ATOM_GPIO_INDEX_GLSYNC_INTERRUPT 5
#define ATOM_GPIO_INDEX_GLSYNC_V_RESET   6
#define ATOM_GPIO_INDEX_GLSYNC_SWAP_CNTL 7
#define ATOM_GPIO_INDEX_GLSYNC_SWAP_SEL  8
#define ATOM_GPIO_INDEX_GLSYNC_MAX       9

typedef struct  _ATOM_ENCODER_DVO_CF_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  ULONG                       ulStrengthControl;      
  UCHAR                       ucPadding[2];
}ATOM_ENCODER_DVO_CF_RECORD;

#define ATOM_ENCODER_CAP_RECORD_HBR2                  0x01         
#define ATOM_ENCODER_CAP_RECORD_HBR2_EN               0x02         

typedef struct  _ATOM_ENCODER_CAP_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  union {
    USHORT                    usEncoderCap;         
    struct {
#if ATOM_BIG_ENDIAN
      USHORT                  usReserved:14;        
      USHORT                  usHBR2En:1;           
      USHORT                  usHBR2Cap:1;          
#else
      USHORT                  usHBR2Cap:1;          
      USHORT                  usHBR2En:1;           
      USHORT                  usReserved:14;        
#endif
    };
  }; 
}ATOM_ENCODER_CAP_RECORD;                             

#define ATOM_CONNECTOR_CF_RECORD_CONNECTED_UPPER12BITBUNDLEA   1
#define ATOM_CONNECTOR_CF_RECORD_CONNECTED_LOWER12BITBUNDLEB   2

typedef struct  _ATOM_CONNECTOR_CF_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  USHORT                      usMaxPixClk;
  UCHAR                       ucFlowCntlGpioId;
  UCHAR                       ucSwapCntlGpioId;
  UCHAR                       ucConnectedDvoBundle;
  UCHAR                       ucPadding;
}ATOM_CONNECTOR_CF_RECORD;

typedef struct  _ATOM_CONNECTOR_HARDCODE_DTD_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
	ATOM_DTD_FORMAT							asTiming;
}ATOM_CONNECTOR_HARDCODE_DTD_RECORD;

typedef struct _ATOM_CONNECTOR_PCIE_SUBCONNECTOR_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;                
  UCHAR                       ucSubConnectorType;     
  UCHAR                       ucReserved;
}ATOM_CONNECTOR_PCIE_SUBCONNECTOR_RECORD;


typedef struct _ATOM_ROUTER_DDC_PATH_SELECT_RECORD
{
	ATOM_COMMON_RECORD_HEADER   sheader;                
	UCHAR												ucMuxType;							
	UCHAR												ucMuxControlPin;
	UCHAR												ucMuxState[2];					
}ATOM_ROUTER_DDC_PATH_SELECT_RECORD;

typedef struct _ATOM_ROUTER_DATA_CLOCK_PATH_SELECT_RECORD
{
	ATOM_COMMON_RECORD_HEADER   sheader;                
	UCHAR												ucMuxType;
	UCHAR												ucMuxControlPin;
	UCHAR												ucMuxState[2];					
}ATOM_ROUTER_DATA_CLOCK_PATH_SELECT_RECORD;

#define ATOM_ROUTER_MUX_PIN_STATE_MASK								0x0f
#define ATOM_ROUTER_MUX_PIN_SINGLE_STATE_COMPLEMENT		0x01

typedef struct _ATOM_CONNECTOR_HPDPIN_LUT_RECORD     
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  UCHAR                       ucHPDPINMap[MAX_NUMBER_OF_EXT_HPDPIN_LUT_ENTRIES];  
}ATOM_CONNECTOR_HPDPIN_LUT_RECORD;

typedef struct _ATOM_CONNECTOR_AUXDDC_LUT_RECORD  
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  ATOM_I2C_ID_CONFIG          ucAUXDDCMap[MAX_NUMBER_OF_EXT_AUXDDC_LUT_ENTRIES];  
}ATOM_CONNECTOR_AUXDDC_LUT_RECORD;

typedef struct _ATOM_OBJECT_LINK_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  USHORT                      usObjectID;         
}ATOM_OBJECT_LINK_RECORD;

typedef struct _ATOM_CONNECTOR_REMOTE_CAP_RECORD
{
  ATOM_COMMON_RECORD_HEADER   sheader;
  USHORT                      usReserved;
}ATOM_CONNECTOR_REMOTE_CAP_RECORD;

	
	
typedef struct  _ATOM_VOLTAGE_INFO_HEADER
{
   USHORT   usVDDCBaseLevel;                
   USHORT   usReserved;                     
   UCHAR    ucNumOfVoltageEntries;
   UCHAR    ucBytesPerVoltageEntry;
   UCHAR    ucVoltageStep;                  
   UCHAR    ucDefaultVoltageEntry;
   UCHAR    ucVoltageControlI2cLine;
   UCHAR    ucVoltageControlAddress;
   UCHAR    ucVoltageControlOffset;
}ATOM_VOLTAGE_INFO_HEADER;

typedef struct  _ATOM_VOLTAGE_INFO
{
   ATOM_COMMON_TABLE_HEADER	sHeader; 
   ATOM_VOLTAGE_INFO_HEADER viHeader;
   UCHAR    ucVoltageEntries[64];            
}ATOM_VOLTAGE_INFO;


typedef struct  _ATOM_VOLTAGE_FORMULA
{
   USHORT   usVoltageBaseLevel;             
   USHORT   usVoltageStep;                  
	 UCHAR		ucNumOfVoltageEntries;					
	 UCHAR		ucFlag;													
	 UCHAR		ucBaseVID;											
	 UCHAR		ucReserved;
	 UCHAR		ucVIDAdjustEntries[32];					
}ATOM_VOLTAGE_FORMULA;

typedef struct  _VOLTAGE_LUT_ENTRY
{
	 USHORT		usVoltageCode;									
	 USHORT		usVoltageValue;									
}VOLTAGE_LUT_ENTRY;

typedef struct  _ATOM_VOLTAGE_FORMULA_V2
{
	 UCHAR		ucNumOfVoltageEntries;					
	 UCHAR		ucReserved[3];
	 VOLTAGE_LUT_ENTRY asVIDAdjustEntries[32];
}ATOM_VOLTAGE_FORMULA_V2;

typedef struct _ATOM_VOLTAGE_CONTROL
{
	UCHAR		 ucVoltageControlId;							
  UCHAR    ucVoltageControlI2cLine;
  UCHAR    ucVoltageControlAddress;
  UCHAR    ucVoltageControlOffset;	 	
  USHORT   usGpioPin_AIndex;								
  UCHAR    ucGpioPinBitShift[9];						
	UCHAR		 ucReserved;
}ATOM_VOLTAGE_CONTROL;

#define	VOLTAGE_CONTROLLED_BY_HW							0x00
#define	VOLTAGE_CONTROLLED_BY_I2C_MASK				0x7F
#define	VOLTAGE_CONTROLLED_BY_GPIO						0x80
#define	VOLTAGE_CONTROL_ID_LM64								0x01									
#define	VOLTAGE_CONTROL_ID_DAC								0x02									
#define	VOLTAGE_CONTROL_ID_VT116xM						0x03									
#define VOLTAGE_CONTROL_ID_DS4402							0x04									
#define VOLTAGE_CONTROL_ID_UP6266 						0x05									
#define VOLTAGE_CONTROL_ID_SCORPIO						0x06
#define	VOLTAGE_CONTROL_ID_VT1556M						0x07									
#define	VOLTAGE_CONTROL_ID_CHL822x						0x08									
#define	VOLTAGE_CONTROL_ID_VT1586M						0x09
#define VOLTAGE_CONTROL_ID_UP1637 						0x0A

typedef struct  _ATOM_VOLTAGE_OBJECT
{
 	 UCHAR		ucVoltageType;									
	 UCHAR		ucSize;													
	 ATOM_VOLTAGE_CONTROL			asControl;			
 	 ATOM_VOLTAGE_FORMULA			asFormula;			
}ATOM_VOLTAGE_OBJECT;

typedef struct  _ATOM_VOLTAGE_OBJECT_V2
{
 	 UCHAR		ucVoltageType;									
	 UCHAR		ucSize;													
	 ATOM_VOLTAGE_CONTROL			asControl;			
 	 ATOM_VOLTAGE_FORMULA_V2	asFormula;			
}ATOM_VOLTAGE_OBJECT_V2;

typedef struct  _ATOM_VOLTAGE_OBJECT_INFO
{
   ATOM_COMMON_TABLE_HEADER	sHeader; 
	 ATOM_VOLTAGE_OBJECT			asVoltageObj[3];	
}ATOM_VOLTAGE_OBJECT_INFO;

typedef struct  _ATOM_VOLTAGE_OBJECT_INFO_V2
{
   ATOM_COMMON_TABLE_HEADER	sHeader; 
	 ATOM_VOLTAGE_OBJECT_V2			asVoltageObj[3];	
}ATOM_VOLTAGE_OBJECT_INFO_V2;

typedef struct  _ATOM_LEAKID_VOLTAGE
{
	UCHAR		ucLeakageId;
	UCHAR		ucReserved;
	USHORT	usVoltage;
}ATOM_LEAKID_VOLTAGE;

typedef struct _ATOM_VOLTAGE_OBJECT_HEADER_V3{
 	 UCHAR		ucVoltageType;									
   UCHAR		ucVoltageMode;							    
	 USHORT		usSize;													
}ATOM_VOLTAGE_OBJECT_HEADER_V3;

typedef struct  _VOLTAGE_LUT_ENTRY_V2
{
	 ULONG		ulVoltageId;									  
	 USHORT		usVoltageValue;									
}VOLTAGE_LUT_ENTRY_V2;

typedef struct  _LEAKAGE_VOLTAGE_LUT_ENTRY_V2
{
  USHORT	usVoltageLevel; 							  
  USHORT  usVoltageId;                    
	USHORT	usLeakageId;									  
}LEAKAGE_VOLTAGE_LUT_ENTRY_V2;

typedef struct  _ATOM_I2C_VOLTAGE_OBJECT_V3
{
   ATOM_VOLTAGE_OBJECT_HEADER_V3 sHeader;
   UCHAR	ucVoltageRegulatorId;					  
   UCHAR    ucVoltageControlI2cLine;
   UCHAR    ucVoltageControlAddress;
   UCHAR    ucVoltageControlOffset;	 	
   ULONG    ulReserved;
   VOLTAGE_LUT_ENTRY asVolI2cLut[1];        
}ATOM_I2C_VOLTAGE_OBJECT_V3;

typedef struct  _ATOM_GPIO_VOLTAGE_OBJECT_V3
{
   ATOM_VOLTAGE_OBJECT_HEADER_V3 sHeader;   
   UCHAR    ucVoltageGpioCntlId;         
   UCHAR    ucGpioEntryNum;              
   UCHAR    ucPhaseDelay;                
   UCHAR    ucReserved;   
   ULONG    ulGpioMaskVal;               
   VOLTAGE_LUT_ENTRY_V2 asVolGpioLut[1];   
}ATOM_GPIO_VOLTAGE_OBJECT_V3;

typedef struct  _ATOM_LEAKAGE_VOLTAGE_OBJECT_V3
{
   ATOM_VOLTAGE_OBJECT_HEADER_V3 sHeader;
   UCHAR    ucLeakageCntlId;             
   UCHAR    ucLeakageEntryNum;           
   UCHAR    ucReserved[2];               
   ULONG    ulMaxVoltageLevel;
   LEAKAGE_VOLTAGE_LUT_ENTRY_V2 asLeakageIdLut[1];   
}ATOM_LEAKAGE_VOLTAGE_OBJECT_V3;

typedef union _ATOM_VOLTAGE_OBJECT_V3{
  ATOM_GPIO_VOLTAGE_OBJECT_V3 asGpioVoltageObj;
  ATOM_I2C_VOLTAGE_OBJECT_V3 asI2cVoltageObj;
  ATOM_LEAKAGE_VOLTAGE_OBJECT_V3 asLeakageObj;
}ATOM_VOLTAGE_OBJECT_V3;

typedef struct  _ATOM_VOLTAGE_OBJECT_INFO_V3_1
{
   ATOM_COMMON_TABLE_HEADER	sHeader; 
	 ATOM_VOLTAGE_OBJECT_V3			asVoltageObj[3];	
}ATOM_VOLTAGE_OBJECT_INFO_V3_1;

typedef struct  _ATOM_ASIC_PROFILE_VOLTAGE
{
	UCHAR		ucProfileId;
	UCHAR		ucReserved;
	USHORT	usSize;
	USHORT	usEfuseSpareStartAddr;
	USHORT	usFuseIndex[8];												
	ATOM_LEAKID_VOLTAGE					asLeakVol[2];			
}ATOM_ASIC_PROFILE_VOLTAGE;

#define	ATOM_ASIC_PROFILE_ID_EFUSE_VOLTAGE			1		
#define	ATOM_ASIC_PROFILE_ID_EFUSE_PERFORMANCE_VOLTAGE			1
#define	ATOM_ASIC_PROFILE_ID_EFUSE_THERMAL_VOLTAGE					2

typedef struct  _ATOM_ASIC_PROFILING_INFO
{
  ATOM_COMMON_TABLE_HEADER			asHeader; 
	ATOM_ASIC_PROFILE_VOLTAGE			asVoltage;
}ATOM_ASIC_PROFILING_INFO;

typedef struct _ATOM_POWER_SOURCE_OBJECT
{
	UCHAR	ucPwrSrcId;													
	UCHAR	ucPwrSensorType;										
	UCHAR	ucPwrSensId;											  
	UCHAR	ucPwrSensSlaveAddr;									
	UCHAR ucPwrSensRegIndex;									
	UCHAR ucPwrSensRegBitMask;								
	UCHAR	ucPwrSensActiveState;								
	UCHAR	ucReserve[3];												
	USHORT usSensPwr;													
}ATOM_POWER_SOURCE_OBJECT;

typedef struct _ATOM_POWER_SOURCE_INFO
{
		ATOM_COMMON_TABLE_HEADER		asHeader;
		UCHAR												asPwrbehave[16];
		ATOM_POWER_SOURCE_OBJECT		asPwrObj[1];
}ATOM_POWER_SOURCE_INFO;


#define POWERSOURCE_PCIE_ID1						0x00
#define POWERSOURCE_6PIN_CONNECTOR_ID1	0x01
#define POWERSOURCE_8PIN_CONNECTOR_ID1	0x02
#define POWERSOURCE_6PIN_CONNECTOR_ID2	0x04
#define POWERSOURCE_8PIN_CONNECTOR_ID2	0x08

#define POWER_SENSOR_ALWAYS							0x00
#define POWER_SENSOR_GPIO								0x01
#define POWER_SENSOR_I2C								0x02

typedef struct _ATOM_CLK_VOLT_CAPABILITY
{
  ULONG      ulVoltageIndex;                      
  ULONG      ulMaximumSupportedCLK;               
}ATOM_CLK_VOLT_CAPABILITY;

typedef struct _ATOM_AVAILABLE_SCLK_LIST
{
  ULONG      ulSupportedSCLK;               
  USHORT     usVoltageIndex;                
  USHORT     usVoltageID;                   
}ATOM_AVAILABLE_SCLK_LIST;

#define ATOM_IGP_INFO_V6_SYSTEM_CONFIG__PCIE_POWER_GATING_ENABLE             1       

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_V6
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  ULONG  ulBootUpEngineClock;
  ULONG  ulDentistVCOFreq;          
  ULONG  ulBootUpUMAClock;          
  ATOM_CLK_VOLT_CAPABILITY   sDISPCLK_Voltage[4];            
  ULONG  ulBootUpReqDisplayVector;
  ULONG  ulOtherDisplayMisc;
  ULONG  ulGPUCapInfo;
  ULONG  ulSB_MMIO_Base_Addr;
  USHORT usRequestedPWMFreqInHz;
  UCHAR  ucHtcTmpLmt;   
  UCHAR  ucHtcHystLmt;
  ULONG  ulMinEngineClock;           
  ULONG  ulSystemConfig;            
  ULONG  ulCPUCapInfo;              
  USHORT usNBP0Voltage;               
  USHORT usNBP1Voltage;
  USHORT usBootUpNBVoltage;                       
  USHORT usExtDispConnInfoOffset;
  USHORT usPanelRefreshRateRange;     
  UCHAR  ucMemoryType;  
  UCHAR  ucUMAChannelNumber;
  ULONG  ulCSR_M3_ARB_CNTL_DEFAULT[10];  
  ULONG  ulCSR_M3_ARB_CNTL_UVD[10]; 
  ULONG  ulCSR_M3_ARB_CNTL_FS3D[10];
  ATOM_AVAILABLE_SCLK_LIST   sAvail_SCLK[5];
  ULONG  ulGMCRestoreResetTime;
  ULONG  ulMinimumNClk;
  ULONG  ulIdleNClk;
  ULONG  ulDDR_DLL_PowerUpTime;
  ULONG  ulDDR_PLL_PowerUpTime;
  USHORT usPCIEClkSSPercentage;
  USHORT usPCIEClkSSType;
  USHORT usLvdsSSPercentage;
  USHORT usLvdsSSpreadRateIn10Hz;
  USHORT usHDMISSPercentage;
  USHORT usHDMISSpreadRateIn10Hz;
  USHORT usDVISSPercentage;
  USHORT usDVISSpreadRateIn10Hz;
  ULONG  SclkDpmBoostMargin;
  ULONG  SclkDpmThrottleMargin;
  USHORT SclkDpmTdpLimitPG; 
  USHORT SclkDpmTdpLimitBoost;
  ULONG  ulBoostEngineCLock;
  UCHAR  ulBoostVid_2bit;  
  UCHAR  EnableBoost;
  USHORT GnbTdpLimit;
  USHORT usMaxLVDSPclkFreqInSingleLink;
  UCHAR  ucLvdsMisc;
  UCHAR  ucLVDSReserved;
  ULONG  ulReserved3[15]; 
  ATOM_EXTERNAL_DISPLAY_CONNECTION_INFO sExtDispConnInfo;   
}ATOM_INTEGRATED_SYSTEM_INFO_V6;   

#define INTEGRATED_SYSTEM_INFO_V6_GPUCAPINFO__TMDSHDMI_COHERENT_SINGLEPLL_MODE       0x01
#define INTEGRATED_SYSTEM_INFO_V6_GPUCAPINFO__DISABLE_AUX_HW_MODE_DETECTION          0x08

#define SYS_INFO_LVDSMISC__888_FPDI_MODE                                             0x01
#define SYS_INFO_LVDSMISC__DL_CH_SWAP                                                0x02
#define SYS_INFO_LVDSMISC__888_BPC                                                   0x04
#define SYS_INFO_LVDSMISC__OVERRIDE_EN                                               0x08
#define SYS_INFO_LVDSMISC__BLON_ACTIVE_LOW                                           0x10

#define SYS_INFO_LVDSMISC__VSYNC_ACTIVE_LOW                                          0x04
#define SYS_INFO_LVDSMISC__HSYNC_ACTIVE_LOW                                          0x08


typedef struct _ATOM_FUSION_SYSTEM_INFO_V1
{
  ATOM_INTEGRATED_SYSTEM_INFO_V6    sIntegratedSysInfo;   
  ULONG  ulPowerplayTable[128];  
}ATOM_FUSION_SYSTEM_INFO_V1; 
 

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_V1_7
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  ULONG  ulBootUpEngineClock;
  ULONG  ulDentistVCOFreq;
  ULONG  ulBootUpUMAClock;
  ATOM_CLK_VOLT_CAPABILITY   sDISPCLK_Voltage[4];
  ULONG  ulBootUpReqDisplayVector;
  ULONG  ulOtherDisplayMisc;
  ULONG  ulGPUCapInfo;
  ULONG  ulSB_MMIO_Base_Addr;
  USHORT usRequestedPWMFreqInHz;
  UCHAR  ucHtcTmpLmt;
  UCHAR  ucHtcHystLmt;
  ULONG  ulMinEngineClock;
  ULONG  ulSystemConfig;            
  ULONG  ulCPUCapInfo;
  USHORT usNBP0Voltage;               
  USHORT usNBP1Voltage;
  USHORT usBootUpNBVoltage;                       
  USHORT usExtDispConnInfoOffset;
  USHORT usPanelRefreshRateRange;     
  UCHAR  ucMemoryType;  
  UCHAR  ucUMAChannelNumber;
  UCHAR  strVBIOSMsg[40];
  ULONG  ulReserved[20];
  ATOM_AVAILABLE_SCLK_LIST   sAvail_SCLK[5];
  ULONG  ulGMCRestoreResetTime;
  ULONG  ulMinimumNClk;
  ULONG  ulIdleNClk;
  ULONG  ulDDR_DLL_PowerUpTime;
  ULONG  ulDDR_PLL_PowerUpTime;
  USHORT usPCIEClkSSPercentage;
  USHORT usPCIEClkSSType;
  USHORT usLvdsSSPercentage;
  USHORT usLvdsSSpreadRateIn10Hz;
  USHORT usHDMISSPercentage;
  USHORT usHDMISSpreadRateIn10Hz;
  USHORT usDVISSPercentage;
  USHORT usDVISSpreadRateIn10Hz;
  ULONG  SclkDpmBoostMargin;
  ULONG  SclkDpmThrottleMargin;
  USHORT SclkDpmTdpLimitPG; 
  USHORT SclkDpmTdpLimitBoost;
  ULONG  ulBoostEngineCLock;
  UCHAR  ulBoostVid_2bit;  
  UCHAR  EnableBoost;
  USHORT GnbTdpLimit;
  USHORT usMaxLVDSPclkFreqInSingleLink;
  UCHAR  ucLvdsMisc;
  UCHAR  ucLVDSReserved;
  UCHAR  ucLVDSPwrOnSeqDIGONtoDE_in4Ms;
  UCHAR  ucLVDSPwrOnSeqDEtoVARY_BL_in4Ms;
  UCHAR  ucLVDSPwrOffSeqVARY_BLtoDE_in4Ms;
  UCHAR  ucLVDSPwrOffSeqDEtoDIGON_in4Ms;
  UCHAR  ucLVDSOffToOnDelay_in4Ms;
  UCHAR  ucLVDSPwrOnSeqVARY_BLtoBLON_in4Ms;
  UCHAR  ucLVDSPwrOffSeqBLONtoVARY_BL_in4Ms;
  UCHAR  ucLVDSReserved1;
  ULONG  ulLCDBitDepthControlVal;
  ULONG  ulNbpStateMemclkFreq[4];
  USHORT usNBP2Voltage;               
  USHORT usNBP3Voltage;
  ULONG  ulNbpStateNClkFreq[4];
  UCHAR  ucNBDPMEnable;
  UCHAR  ucReserved[3];
  UCHAR  ucDPMState0VclkFid;
  UCHAR  ucDPMState0DclkFid;
  UCHAR  ucDPMState1VclkFid;
  UCHAR  ucDPMState1DclkFid;
  UCHAR  ucDPMState2VclkFid;
  UCHAR  ucDPMState2DclkFid;
  UCHAR  ucDPMState3VclkFid;
  UCHAR  ucDPMState3DclkFid;
  ATOM_EXTERNAL_DISPLAY_CONNECTION_INFO sExtDispConnInfo;
}ATOM_INTEGRATED_SYSTEM_INFO_V1_7;

#define INTEGRATED_SYSTEM_INFO__GET_EDID_CALLBACK_FUNC_SUPPORT            0x01
#define INTEGRATED_SYSTEM_INFO__GET_BOOTUP_DISPLAY_CALLBACK_FUNC_SUPPORT  0x02
#define INTEGRATED_SYSTEM_INFO__GET_EXPANSION_CALLBACK_FUNC_SUPPORT       0x04
#define INTEGRATED_SYSTEM_INFO__FAST_BOOT_SUPPORT                         0x08

#define SYS_INFO_GPUCAPS__TMDSHDMI_COHERENT_SINGLEPLL_MODE                0x01
#define SYS_INFO_GPUCAPS__DP_SINGLEPLL_MODE                               0x02
#define SYS_INFO_GPUCAPS__DISABLE_AUX_MODE_DETECT                         0x08


#define ICS91719  1
#define ICS91720  2

typedef struct _ATOM_I2C_DATA_RECORD
{
  UCHAR         ucNunberOfBytes;                                              
  UCHAR         ucI2CData[1];                                                 
}ATOM_I2C_DATA_RECORD;


typedef struct _ATOM_I2C_DEVICE_SETUP_INFO
{
  ATOM_I2C_ID_CONFIG_ACCESS       sucI2cId;               
  UCHAR		                        ucSSChipID;             
  UCHAR		                        ucSSChipSlaveAddr;      
  UCHAR                           ucNumOfI2CDataRecords;  
  ATOM_I2C_DATA_RECORD            asI2CData[1];  
}ATOM_I2C_DEVICE_SETUP_INFO;

typedef struct  _ATOM_ASIC_MVDD_INFO
{
  ATOM_COMMON_TABLE_HEADER	      sHeader; 
  ATOM_I2C_DEVICE_SETUP_INFO      asI2CSetup[1];
}ATOM_ASIC_MVDD_INFO;

#define ATOM_MCLK_SS_INFO         ATOM_ASIC_MVDD_INFO


typedef struct _ATOM_ASIC_SS_ASSIGNMENT
{
	ULONG								ulTargetClockRange;						
  USHORT              usSpreadSpectrumPercentage;		
	USHORT							usSpreadRateInKhz;						
  UCHAR               ucClockIndication;					  
	UCHAR								ucSpreadSpectrumMode;					
	UCHAR								ucReserved[2];
}ATOM_ASIC_SS_ASSIGNMENT;

#define ASIC_INTERNAL_MEMORY_SS			1
#define ASIC_INTERNAL_ENGINE_SS			2
#define ASIC_INTERNAL_UVD_SS        3
#define ASIC_INTERNAL_SS_ON_TMDS    4
#define ASIC_INTERNAL_SS_ON_HDMI    5
#define ASIC_INTERNAL_SS_ON_LVDS    6
#define ASIC_INTERNAL_SS_ON_DP      7
#define ASIC_INTERNAL_SS_ON_DCPLL   8
#define ASIC_EXTERNAL_SS_ON_DP_CLOCK 9
#define ASIC_INTERNAL_VCE_SS        10

typedef struct _ATOM_ASIC_SS_ASSIGNMENT_V2
{
	ULONG								ulTargetClockRange;						
                                                    
  USHORT              usSpreadSpectrumPercentage;		
	USHORT							usSpreadRateIn10Hz;						
  UCHAR               ucClockIndication;					  
	UCHAR								ucSpreadSpectrumMode;					
	UCHAR								ucReserved[2];
}ATOM_ASIC_SS_ASSIGNMENT_V2;


typedef struct _ATOM_ASIC_INTERNAL_SS_INFO
{
  ATOM_COMMON_TABLE_HEADER	      sHeader; 
  ATOM_ASIC_SS_ASSIGNMENT		      asSpreadSpectrum[4];
}ATOM_ASIC_INTERNAL_SS_INFO;

typedef struct _ATOM_ASIC_INTERNAL_SS_INFO_V2
{
  ATOM_COMMON_TABLE_HEADER	      sHeader; 
  ATOM_ASIC_SS_ASSIGNMENT_V2		  asSpreadSpectrum[1];      
}ATOM_ASIC_INTERNAL_SS_INFO_V2;

typedef struct _ATOM_ASIC_SS_ASSIGNMENT_V3
{
	ULONG								ulTargetClockRange;						
                                                    
  USHORT              usSpreadSpectrumPercentage;		
	USHORT							usSpreadRateIn10Hz;						
  UCHAR               ucClockIndication;					  
	UCHAR								ucSpreadSpectrumMode;					
	UCHAR								ucReserved[2];
}ATOM_ASIC_SS_ASSIGNMENT_V3;

typedef struct _ATOM_ASIC_INTERNAL_SS_INFO_V3
{
  ATOM_COMMON_TABLE_HEADER	      sHeader; 
  ATOM_ASIC_SS_ASSIGNMENT_V3		  asSpreadSpectrum[1];      
}ATOM_ASIC_INTERNAL_SS_INFO_V3;


#define ATOM_DEVICE_CONNECT_INFO_DEF  0
#define ATOM_ROM_LOCATION_DEF         1
#define ATOM_TV_STANDARD_DEF          2
#define ATOM_ACTIVE_INFO_DEF          3
#define ATOM_LCD_INFO_DEF             4
#define ATOM_DOS_REQ_INFO_DEF         5
#define ATOM_ACC_CHANGE_INFO_DEF      6
#define ATOM_DOS_MODE_INFO_DEF        7
#define ATOM_I2C_CHANNEL_STATUS_DEF   8
#define ATOM_I2C_CHANNEL_STATUS1_DEF  9
#define ATOM_INTERNAL_TIMER_DEF       10

#define ATOM_S0_CRT1_MONO               0x00000001L
#define ATOM_S0_CRT1_COLOR              0x00000002L
#define ATOM_S0_CRT1_MASK               (ATOM_S0_CRT1_MONO+ATOM_S0_CRT1_COLOR)

#define ATOM_S0_TV1_COMPOSITE_A         0x00000004L
#define ATOM_S0_TV1_SVIDEO_A            0x00000008L
#define ATOM_S0_TV1_MASK_A              (ATOM_S0_TV1_COMPOSITE_A+ATOM_S0_TV1_SVIDEO_A)

#define ATOM_S0_CV_A                    0x00000010L
#define ATOM_S0_CV_DIN_A                0x00000020L
#define ATOM_S0_CV_MASK_A               (ATOM_S0_CV_A+ATOM_S0_CV_DIN_A)


#define ATOM_S0_CRT2_MONO               0x00000100L
#define ATOM_S0_CRT2_COLOR              0x00000200L
#define ATOM_S0_CRT2_MASK               (ATOM_S0_CRT2_MONO+ATOM_S0_CRT2_COLOR)

#define ATOM_S0_TV1_COMPOSITE           0x00000400L
#define ATOM_S0_TV1_SVIDEO              0x00000800L
#define ATOM_S0_TV1_SCART               0x00004000L
#define ATOM_S0_TV1_MASK                (ATOM_S0_TV1_COMPOSITE+ATOM_S0_TV1_SVIDEO+ATOM_S0_TV1_SCART)

#define ATOM_S0_CV                      0x00001000L
#define ATOM_S0_CV_DIN                  0x00002000L
#define ATOM_S0_CV_MASK                 (ATOM_S0_CV+ATOM_S0_CV_DIN)

#define ATOM_S0_DFP1                    0x00010000L
#define ATOM_S0_DFP2                    0x00020000L
#define ATOM_S0_LCD1                    0x00040000L
#define ATOM_S0_LCD2                    0x00080000L
#define ATOM_S0_DFP6                    0x00100000L
#define ATOM_S0_DFP3                    0x00200000L
#define ATOM_S0_DFP4                    0x00400000L
#define ATOM_S0_DFP5                    0x00800000L

#define ATOM_S0_DFP_MASK                ATOM_S0_DFP1 | ATOM_S0_DFP2 | ATOM_S0_DFP3 | ATOM_S0_DFP4 | ATOM_S0_DFP5 | ATOM_S0_DFP6

#define ATOM_S0_FAD_REGISTER_BUG        0x02000000L 
                                                    

#define ATOM_S0_THERMAL_STATE_MASK      0x1C000000L
#define ATOM_S0_THERMAL_STATE_SHIFT     26

#define ATOM_S0_SYSTEM_POWER_STATE_MASK 0xE0000000L
#define ATOM_S0_SYSTEM_POWER_STATE_SHIFT 29 

#define ATOM_S0_SYSTEM_POWER_STATE_VALUE_AC     1
#define ATOM_S0_SYSTEM_POWER_STATE_VALUE_DC     2
#define ATOM_S0_SYSTEM_POWER_STATE_VALUE_LITEAC 3
#define ATOM_S0_SYSTEM_POWER_STATE_VALUE_LIT2AC 4

#define ATOM_S0_CRT1_MONOb0             0x01
#define ATOM_S0_CRT1_COLORb0            0x02
#define ATOM_S0_CRT1_MASKb0             (ATOM_S0_CRT1_MONOb0+ATOM_S0_CRT1_COLORb0)

#define ATOM_S0_TV1_COMPOSITEb0         0x04
#define ATOM_S0_TV1_SVIDEOb0            0x08
#define ATOM_S0_TV1_MASKb0              (ATOM_S0_TV1_COMPOSITEb0+ATOM_S0_TV1_SVIDEOb0)

#define ATOM_S0_CVb0                    0x10
#define ATOM_S0_CV_DINb0                0x20
#define ATOM_S0_CV_MASKb0               (ATOM_S0_CVb0+ATOM_S0_CV_DINb0)

#define ATOM_S0_CRT2_MONOb1             0x01
#define ATOM_S0_CRT2_COLORb1            0x02
#define ATOM_S0_CRT2_MASKb1             (ATOM_S0_CRT2_MONOb1+ATOM_S0_CRT2_COLORb1)

#define ATOM_S0_TV1_COMPOSITEb1         0x04
#define ATOM_S0_TV1_SVIDEOb1            0x08
#define ATOM_S0_TV1_SCARTb1             0x40
#define ATOM_S0_TV1_MASKb1              (ATOM_S0_TV1_COMPOSITEb1+ATOM_S0_TV1_SVIDEOb1+ATOM_S0_TV1_SCARTb1)

#define ATOM_S0_CVb1                    0x10
#define ATOM_S0_CV_DINb1                0x20
#define ATOM_S0_CV_MASKb1               (ATOM_S0_CVb1+ATOM_S0_CV_DINb1)

#define ATOM_S0_DFP1b2                  0x01
#define ATOM_S0_DFP2b2                  0x02
#define ATOM_S0_LCD1b2                  0x04
#define ATOM_S0_LCD2b2                  0x08
#define ATOM_S0_DFP6b2                  0x10
#define ATOM_S0_DFP3b2                  0x20
#define ATOM_S0_DFP4b2                  0x40
#define ATOM_S0_DFP5b2                  0x80


#define ATOM_S0_THERMAL_STATE_MASKb3    0x1C
#define ATOM_S0_THERMAL_STATE_SHIFTb3   2

#define ATOM_S0_SYSTEM_POWER_STATE_MASKb3 0xE0
#define ATOM_S0_LCD1_SHIFT              18

#define ATOM_S1_ROM_LOCATION_MASK       0x0000FFFFL
#define ATOM_S1_PCI_BUS_DEV_MASK        0xFFFF0000L

#define ATOM_S2_TV1_STANDARD_MASK       0x0000000FL
#define ATOM_S2_CURRENT_BL_LEVEL_MASK   0x0000FF00L
#define ATOM_S2_CURRENT_BL_LEVEL_SHIFT  8

#define ATOM_S2_FORCEDLOWPWRMODE_STATE_MASK       0x0C000000L
#define ATOM_S2_FORCEDLOWPWRMODE_STATE_MASK_SHIFT 26
#define ATOM_S2_FORCEDLOWPWRMODE_STATE_CHANGE     0x10000000L

#define ATOM_S2_DEVICE_DPMS_STATE       0x00010000L
#define ATOM_S2_VRI_BRIGHT_ENABLE       0x20000000L

#define ATOM_S2_DISPLAY_ROTATION_0_DEGREE     0x0
#define ATOM_S2_DISPLAY_ROTATION_90_DEGREE    0x1
#define ATOM_S2_DISPLAY_ROTATION_180_DEGREE   0x2
#define ATOM_S2_DISPLAY_ROTATION_270_DEGREE   0x3
#define ATOM_S2_DISPLAY_ROTATION_DEGREE_SHIFT 30
#define ATOM_S2_DISPLAY_ROTATION_ANGLE_MASK   0xC0000000L


#define ATOM_S2_TV1_STANDARD_MASKb0     0x0F
#define ATOM_S2_CURRENT_BL_LEVEL_MASKb1 0xFF
#define ATOM_S2_DEVICE_DPMS_STATEb2     0x01

#define ATOM_S2_DEVICE_DPMS_MASKw1      0x3FF
#define ATOM_S2_FORCEDLOWPWRMODE_STATE_MASKb3     0x0C
#define ATOM_S2_FORCEDLOWPWRMODE_STATE_CHANGEb3   0x10
#define ATOM_S2_TMDS_COHERENT_MODEb3    0x10          
#define ATOM_S2_VRI_BRIGHT_ENABLEb3     0x20
#define ATOM_S2_ROTATION_STATE_MASKb3   0xC0


#define ATOM_S3_CRT1_ACTIVE             0x00000001L
#define ATOM_S3_LCD1_ACTIVE             0x00000002L
#define ATOM_S3_TV1_ACTIVE              0x00000004L
#define ATOM_S3_DFP1_ACTIVE             0x00000008L
#define ATOM_S3_CRT2_ACTIVE             0x00000010L
#define ATOM_S3_LCD2_ACTIVE             0x00000020L
#define ATOM_S3_DFP6_ACTIVE             0x00000040L
#define ATOM_S3_DFP2_ACTIVE             0x00000080L
#define ATOM_S3_CV_ACTIVE               0x00000100L
#define ATOM_S3_DFP3_ACTIVE							0x00000200L
#define ATOM_S3_DFP4_ACTIVE							0x00000400L
#define ATOM_S3_DFP5_ACTIVE							0x00000800L

#define ATOM_S3_DEVICE_ACTIVE_MASK      0x00000FFFL

#define ATOM_S3_LCD_FULLEXPANSION_ACTIVE         0x00001000L
#define ATOM_S3_LCD_EXPANSION_ASPEC_RATIO_ACTIVE 0x00002000L

#define ATOM_S3_CRT1_CRTC_ACTIVE        0x00010000L
#define ATOM_S3_LCD1_CRTC_ACTIVE        0x00020000L
#define ATOM_S3_TV1_CRTC_ACTIVE         0x00040000L
#define ATOM_S3_DFP1_CRTC_ACTIVE        0x00080000L
#define ATOM_S3_CRT2_CRTC_ACTIVE        0x00100000L
#define ATOM_S3_LCD2_CRTC_ACTIVE        0x00200000L
#define ATOM_S3_DFP6_CRTC_ACTIVE        0x00400000L
#define ATOM_S3_DFP2_CRTC_ACTIVE        0x00800000L
#define ATOM_S3_CV_CRTC_ACTIVE          0x01000000L
#define ATOM_S3_DFP3_CRTC_ACTIVE				0x02000000L
#define ATOM_S3_DFP4_CRTC_ACTIVE				0x04000000L
#define ATOM_S3_DFP5_CRTC_ACTIVE				0x08000000L

#define ATOM_S3_DEVICE_CRTC_ACTIVE_MASK 0x0FFF0000L
#define ATOM_S3_ASIC_GUI_ENGINE_HUNG    0x20000000L
#define ATOM_S3_ALLOW_FAST_PWR_SWITCH   0x40000000L
#define ATOM_S3_RQST_GPU_USE_MIN_PWR    0x80000000L

#define ATOM_S3_CRT1_ACTIVEb0           0x01
#define ATOM_S3_LCD1_ACTIVEb0           0x02
#define ATOM_S3_TV1_ACTIVEb0            0x04
#define ATOM_S3_DFP1_ACTIVEb0           0x08
#define ATOM_S3_CRT2_ACTIVEb0           0x10
#define ATOM_S3_LCD2_ACTIVEb0           0x20
#define ATOM_S3_DFP6_ACTIVEb0           0x40
#define ATOM_S3_DFP2_ACTIVEb0           0x80
#define ATOM_S3_CV_ACTIVEb1             0x01
#define ATOM_S3_DFP3_ACTIVEb1						0x02
#define ATOM_S3_DFP4_ACTIVEb1						0x04
#define ATOM_S3_DFP5_ACTIVEb1						0x08

#define ATOM_S3_ACTIVE_CRTC1w0          0xFFF

#define ATOM_S3_CRT1_CRTC_ACTIVEb2      0x01
#define ATOM_S3_LCD1_CRTC_ACTIVEb2      0x02
#define ATOM_S3_TV1_CRTC_ACTIVEb2       0x04
#define ATOM_S3_DFP1_CRTC_ACTIVEb2      0x08
#define ATOM_S3_CRT2_CRTC_ACTIVEb2      0x10
#define ATOM_S3_LCD2_CRTC_ACTIVEb2      0x20
#define ATOM_S3_DFP6_CRTC_ACTIVEb2      0x40
#define ATOM_S3_DFP2_CRTC_ACTIVEb2      0x80
#define ATOM_S3_CV_CRTC_ACTIVEb3        0x01
#define ATOM_S3_DFP3_CRTC_ACTIVEb3			0x02
#define ATOM_S3_DFP4_CRTC_ACTIVEb3			0x04
#define ATOM_S3_DFP5_CRTC_ACTIVEb3			0x08

#define ATOM_S3_ACTIVE_CRTC2w1          0xFFF

#define ATOM_S4_LCD1_PANEL_ID_MASK      0x000000FFL
#define ATOM_S4_LCD1_REFRESH_MASK       0x0000FF00L
#define ATOM_S4_LCD1_REFRESH_SHIFT      8

#define ATOM_S4_LCD1_PANEL_ID_MASKb0	  0x0FF
#define ATOM_S4_LCD1_REFRESH_MASKb1		  ATOM_S4_LCD1_PANEL_ID_MASKb0
#define ATOM_S4_VRAM_INFO_MASKb2        ATOM_S4_LCD1_PANEL_ID_MASKb0

#define ATOM_S5_DOS_REQ_CRT1b0          0x01
#define ATOM_S5_DOS_REQ_LCD1b0          0x02
#define ATOM_S5_DOS_REQ_TV1b0           0x04
#define ATOM_S5_DOS_REQ_DFP1b0          0x08
#define ATOM_S5_DOS_REQ_CRT2b0          0x10
#define ATOM_S5_DOS_REQ_LCD2b0          0x20
#define ATOM_S5_DOS_REQ_DFP6b0          0x40
#define ATOM_S5_DOS_REQ_DFP2b0          0x80
#define ATOM_S5_DOS_REQ_CVb1            0x01
#define ATOM_S5_DOS_REQ_DFP3b1					0x02
#define ATOM_S5_DOS_REQ_DFP4b1					0x04
#define ATOM_S5_DOS_REQ_DFP5b1					0x08

#define ATOM_S5_DOS_REQ_DEVICEw0        0x0FFF

#define ATOM_S5_DOS_REQ_CRT1            0x0001
#define ATOM_S5_DOS_REQ_LCD1            0x0002
#define ATOM_S5_DOS_REQ_TV1             0x0004
#define ATOM_S5_DOS_REQ_DFP1            0x0008
#define ATOM_S5_DOS_REQ_CRT2            0x0010
#define ATOM_S5_DOS_REQ_LCD2            0x0020
#define ATOM_S5_DOS_REQ_DFP6            0x0040
#define ATOM_S5_DOS_REQ_DFP2            0x0080
#define ATOM_S5_DOS_REQ_CV              0x0100
#define ATOM_S5_DOS_REQ_DFP3            0x0200
#define ATOM_S5_DOS_REQ_DFP4            0x0400
#define ATOM_S5_DOS_REQ_DFP5            0x0800

#define ATOM_S5_DOS_FORCE_CRT1b2        ATOM_S5_DOS_REQ_CRT1b0
#define ATOM_S5_DOS_FORCE_TV1b2         ATOM_S5_DOS_REQ_TV1b0
#define ATOM_S5_DOS_FORCE_CRT2b2        ATOM_S5_DOS_REQ_CRT2b0
#define ATOM_S5_DOS_FORCE_CVb3          ATOM_S5_DOS_REQ_CVb1
#define ATOM_S5_DOS_FORCE_DEVICEw1      (ATOM_S5_DOS_FORCE_CRT1b2+ATOM_S5_DOS_FORCE_TV1b2+ATOM_S5_DOS_FORCE_CRT2b2+\
                                        (ATOM_S5_DOS_FORCE_CVb3<<8))

#define ATOM_S6_DEVICE_CHANGE           0x00000001L
#define ATOM_S6_SCALER_CHANGE           0x00000002L
#define ATOM_S6_LID_CHANGE              0x00000004L
#define ATOM_S6_DOCKING_CHANGE          0x00000008L
#define ATOM_S6_ACC_MODE                0x00000010L
#define ATOM_S6_EXT_DESKTOP_MODE        0x00000020L
#define ATOM_S6_LID_STATE               0x00000040L
#define ATOM_S6_DOCK_STATE              0x00000080L
#define ATOM_S6_CRITICAL_STATE          0x00000100L
#define ATOM_S6_HW_I2C_BUSY_STATE       0x00000200L
#define ATOM_S6_THERMAL_STATE_CHANGE    0x00000400L
#define ATOM_S6_INTERRUPT_SET_BY_BIOS   0x00000800L
#define ATOM_S6_REQ_LCD_EXPANSION_FULL         0x00001000L 
#define ATOM_S6_REQ_LCD_EXPANSION_ASPEC_RATIO  0x00002000L 

#define ATOM_S6_DISPLAY_STATE_CHANGE    0x00004000L        
#define ATOM_S6_I2C_STATE_CHANGE        0x00008000L        

#define ATOM_S6_ACC_REQ_CRT1            0x00010000L
#define ATOM_S6_ACC_REQ_LCD1            0x00020000L
#define ATOM_S6_ACC_REQ_TV1             0x00040000L
#define ATOM_S6_ACC_REQ_DFP1            0x00080000L
#define ATOM_S6_ACC_REQ_CRT2            0x00100000L
#define ATOM_S6_ACC_REQ_LCD2            0x00200000L
#define ATOM_S6_ACC_REQ_DFP6            0x00400000L
#define ATOM_S6_ACC_REQ_DFP2            0x00800000L
#define ATOM_S6_ACC_REQ_CV              0x01000000L
#define ATOM_S6_ACC_REQ_DFP3						0x02000000L
#define ATOM_S6_ACC_REQ_DFP4						0x04000000L
#define ATOM_S6_ACC_REQ_DFP5						0x08000000L

#define ATOM_S6_ACC_REQ_MASK                0x0FFF0000L
#define ATOM_S6_SYSTEM_POWER_MODE_CHANGE    0x10000000L
#define ATOM_S6_ACC_BLOCK_DISPLAY_SWITCH    0x20000000L
#define ATOM_S6_VRI_BRIGHTNESS_CHANGE       0x40000000L
#define ATOM_S6_CONFIG_DISPLAY_CHANGE_MASK  0x80000000L

#define ATOM_S6_DEVICE_CHANGEb0         0x01
#define ATOM_S6_SCALER_CHANGEb0         0x02
#define ATOM_S6_LID_CHANGEb0            0x04
#define ATOM_S6_DOCKING_CHANGEb0        0x08
#define ATOM_S6_ACC_MODEb0              0x10
#define ATOM_S6_EXT_DESKTOP_MODEb0      0x20
#define ATOM_S6_LID_STATEb0             0x40
#define ATOM_S6_DOCK_STATEb0            0x80
#define ATOM_S6_CRITICAL_STATEb1        0x01
#define ATOM_S6_HW_I2C_BUSY_STATEb1     0x02  
#define ATOM_S6_THERMAL_STATE_CHANGEb1  0x04
#define ATOM_S6_INTERRUPT_SET_BY_BIOSb1 0x08
#define ATOM_S6_REQ_LCD_EXPANSION_FULLb1        0x10    
#define ATOM_S6_REQ_LCD_EXPANSION_ASPEC_RATIOb1 0x20 

#define ATOM_S6_ACC_REQ_CRT1b2          0x01
#define ATOM_S6_ACC_REQ_LCD1b2          0x02
#define ATOM_S6_ACC_REQ_TV1b2           0x04
#define ATOM_S6_ACC_REQ_DFP1b2          0x08
#define ATOM_S6_ACC_REQ_CRT2b2          0x10
#define ATOM_S6_ACC_REQ_LCD2b2          0x20
#define ATOM_S6_ACC_REQ_DFP6b2          0x40
#define ATOM_S6_ACC_REQ_DFP2b2          0x80
#define ATOM_S6_ACC_REQ_CVb3            0x01
#define ATOM_S6_ACC_REQ_DFP3b3          0x02
#define ATOM_S6_ACC_REQ_DFP4b3          0x04
#define ATOM_S6_ACC_REQ_DFP5b3          0x08

#define ATOM_S6_ACC_REQ_DEVICEw1        ATOM_S5_DOS_REQ_DEVICEw0
#define ATOM_S6_SYSTEM_POWER_MODE_CHANGEb3 0x10
#define ATOM_S6_ACC_BLOCK_DISPLAY_SWITCHb3 0x20
#define ATOM_S6_VRI_BRIGHTNESS_CHANGEb3    0x40
#define ATOM_S6_CONFIG_DISPLAY_CHANGEb3    0x80

#define ATOM_S6_DEVICE_CHANGE_SHIFT             0
#define ATOM_S6_SCALER_CHANGE_SHIFT             1
#define ATOM_S6_LID_CHANGE_SHIFT                2
#define ATOM_S6_DOCKING_CHANGE_SHIFT            3
#define ATOM_S6_ACC_MODE_SHIFT                  4
#define ATOM_S6_EXT_DESKTOP_MODE_SHIFT          5
#define ATOM_S6_LID_STATE_SHIFT                 6
#define ATOM_S6_DOCK_STATE_SHIFT                7
#define ATOM_S6_CRITICAL_STATE_SHIFT            8
#define ATOM_S6_HW_I2C_BUSY_STATE_SHIFT         9
#define ATOM_S6_THERMAL_STATE_CHANGE_SHIFT      10
#define ATOM_S6_INTERRUPT_SET_BY_BIOS_SHIFT     11
#define ATOM_S6_REQ_SCALER_SHIFT                12
#define ATOM_S6_REQ_SCALER_ARATIO_SHIFT         13
#define ATOM_S6_DISPLAY_STATE_CHANGE_SHIFT      14
#define ATOM_S6_I2C_STATE_CHANGE_SHIFT          15
#define ATOM_S6_SYSTEM_POWER_MODE_CHANGE_SHIFT  28
#define ATOM_S6_ACC_BLOCK_DISPLAY_SWITCH_SHIFT  29
#define ATOM_S6_VRI_BRIGHTNESS_CHANGE_SHIFT     30
#define ATOM_S6_CONFIG_DISPLAY_CHANGE_SHIFT     31

#define ATOM_S7_DOS_MODE_TYPEb0             0x03
#define ATOM_S7_DOS_MODE_VGAb0              0x00
#define ATOM_S7_DOS_MODE_VESAb0             0x01
#define ATOM_S7_DOS_MODE_EXTb0              0x02
#define ATOM_S7_DOS_MODE_PIXEL_DEPTHb0      0x0C
#define ATOM_S7_DOS_MODE_PIXEL_FORMATb0     0xF0
#define ATOM_S7_DOS_8BIT_DAC_ENb1           0x01
#define ATOM_S7_DOS_MODE_NUMBERw1           0x0FFFF

#define ATOM_S7_DOS_8BIT_DAC_EN_SHIFT       8

#define ATOM_S8_I2C_CHANNEL_BUSY_MASK       0x00000FFFF
#define ATOM_S8_I2C_HW_ENGINE_BUSY_MASK     0x0FFFF0000   

#define ATOM_S8_I2C_CHANNEL_BUSY_SHIFT      0
#define ATOM_S8_I2C_ENGINE_BUSY_SHIFT       16

#ifndef ATOM_S9_I2C_CHANNEL_COMPLETED_MASK 
#define ATOM_S9_I2C_CHANNEL_COMPLETED_MASK  0x0000FFFF
#endif
#ifndef ATOM_S9_I2C_CHANNEL_ABORTED_MASK  
#define ATOM_S9_I2C_CHANNEL_ABORTED_MASK    0xFFFF0000
#endif
#ifndef ATOM_S9_I2C_CHANNEL_COMPLETED_SHIFT 
#define ATOM_S9_I2C_CHANNEL_COMPLETED_SHIFT 0
#endif
#ifndef ATOM_S9_I2C_CHANNEL_ABORTED_SHIFT   
#define ATOM_S9_I2C_CHANNEL_ABORTED_SHIFT   16
#endif

 
#define ATOM_FLAG_SET                         0x20
#define ATOM_FLAG_CLEAR                       0
#define CLEAR_ATOM_S6_ACC_MODE                ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_ACC_MODE_SHIFT | ATOM_FLAG_CLEAR)
#define SET_ATOM_S6_DEVICE_CHANGE             ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_DEVICE_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_VRI_BRIGHTNESS_CHANGE     ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_VRI_BRIGHTNESS_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_SCALER_CHANGE             ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_SCALER_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_LID_CHANGE                ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_LID_CHANGE_SHIFT | ATOM_FLAG_SET)

#define SET_ATOM_S6_LID_STATE                 ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_LID_STATE_SHIFT | ATOM_FLAG_SET)
#define CLEAR_ATOM_S6_LID_STATE               ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_LID_STATE_SHIFT | ATOM_FLAG_CLEAR)

#define SET_ATOM_S6_DOCK_CHANGE			          ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_DOCKING_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_DOCK_STATE                ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_DOCK_STATE_SHIFT | ATOM_FLAG_SET)
#define CLEAR_ATOM_S6_DOCK_STATE              ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_DOCK_STATE_SHIFT | ATOM_FLAG_CLEAR)

#define SET_ATOM_S6_THERMAL_STATE_CHANGE      ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_THERMAL_STATE_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_SYSTEM_POWER_MODE_CHANGE  ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_SYSTEM_POWER_MODE_CHANGE_SHIFT | ATOM_FLAG_SET)
#define SET_ATOM_S6_INTERRUPT_SET_BY_BIOS     ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_INTERRUPT_SET_BY_BIOS_SHIFT | ATOM_FLAG_SET)

#define SET_ATOM_S6_CRITICAL_STATE            ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_CRITICAL_STATE_SHIFT | ATOM_FLAG_SET)
#define CLEAR_ATOM_S6_CRITICAL_STATE          ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_CRITICAL_STATE_SHIFT | ATOM_FLAG_CLEAR)

#define SET_ATOM_S6_REQ_SCALER                ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_REQ_SCALER_SHIFT | ATOM_FLAG_SET)  
#define CLEAR_ATOM_S6_REQ_SCALER              ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_REQ_SCALER_SHIFT | ATOM_FLAG_CLEAR )

#define SET_ATOM_S6_REQ_SCALER_ARATIO         ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_REQ_SCALER_ARATIO_SHIFT | ATOM_FLAG_SET )
#define CLEAR_ATOM_S6_REQ_SCALER_ARATIO       ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_REQ_SCALER_ARATIO_SHIFT | ATOM_FLAG_CLEAR )

#define SET_ATOM_S6_I2C_STATE_CHANGE          ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_I2C_STATE_CHANGE_SHIFT | ATOM_FLAG_SET )

#define SET_ATOM_S6_DISPLAY_STATE_CHANGE      ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_DISPLAY_STATE_CHANGE_SHIFT | ATOM_FLAG_SET )

#define SET_ATOM_S6_DEVICE_RECONFIG           ((ATOM_ACC_CHANGE_INFO_DEF << 8 )|ATOM_S6_CONFIG_DISPLAY_CHANGE_SHIFT | ATOM_FLAG_SET)
#define CLEAR_ATOM_S0_LCD1                    ((ATOM_DEVICE_CONNECT_INFO_DEF << 8 )|  ATOM_S0_LCD1_SHIFT | ATOM_FLAG_CLEAR )
#define SET_ATOM_S7_DOS_8BIT_DAC_EN           ((ATOM_DOS_MODE_INFO_DEF << 8 )|ATOM_S7_DOS_8BIT_DAC_EN_SHIFT | ATOM_FLAG_SET )
#define CLEAR_ATOM_S7_DOS_8BIT_DAC_EN         ((ATOM_DOS_MODE_INFO_DEF << 8 )|ATOM_S7_DOS_8BIT_DAC_EN_SHIFT | ATOM_FLAG_CLEAR )

	

#ifdef __cplusplus
#define GetIndexIntoMasterTable(MasterOrData, FieldName) ((reinterpret_cast<char*>(&(static_cast<ATOM_MASTER_LIST_OF_##MasterOrData##_TABLES*>(0))->FieldName)-static_cast<char*>(0))/sizeof(USHORT))

#define GET_COMMAND_TABLE_COMMANDSET_REVISION(TABLE_HEADER_OFFSET) (((static_cast<ATOM_COMMON_TABLE_HEADER*>(TABLE_HEADER_OFFSET))->ucTableFormatRevision )&0x3F)
#define GET_COMMAND_TABLE_PARAMETER_REVISION(TABLE_HEADER_OFFSET)  (((static_cast<ATOM_COMMON_TABLE_HEADER*>(TABLE_HEADER_OFFSET))->ucTableContentRevision)&0x3F)
#else 
#define	GetIndexIntoMasterTable(MasterOrData, FieldName) (((char*)(&((ATOM_MASTER_LIST_OF_##MasterOrData##_TABLES*)0)->FieldName)-(char*)0)/sizeof(USHORT))

#define GET_COMMAND_TABLE_COMMANDSET_REVISION(TABLE_HEADER_OFFSET) ((((ATOM_COMMON_TABLE_HEADER*)TABLE_HEADER_OFFSET)->ucTableFormatRevision)&0x3F)
#define GET_COMMAND_TABLE_PARAMETER_REVISION(TABLE_HEADER_OFFSET)  ((((ATOM_COMMON_TABLE_HEADER*)TABLE_HEADER_OFFSET)->ucTableContentRevision)&0x3F)
#endif 

#define GET_DATA_TABLE_MAJOR_REVISION GET_COMMAND_TABLE_COMMANDSET_REVISION
#define GET_DATA_TABLE_MINOR_REVISION GET_COMMAND_TABLE_PARAMETER_REVISION

	
#define ATOM_DAC_SRC					0x80
#define ATOM_SRC_DAC1					0
#define ATOM_SRC_DAC2					0x80

typedef struct _MEMORY_PLLINIT_PARAMETERS
{
  ULONG ulTargetMemoryClock; 
  UCHAR   ucAction;					 
  UCHAR   ucFbDiv_Hi;				 
  UCHAR   ucFbDiv;					 
  UCHAR   ucPostDiv;				 
}MEMORY_PLLINIT_PARAMETERS;

#define MEMORY_PLLINIT_PS_ALLOCATION  MEMORY_PLLINIT_PARAMETERS


#define	GPIO_PIN_WRITE													0x01			
#define	GPIO_PIN_READ														0x00

typedef struct  _GPIO_PIN_CONTROL_PARAMETERS
{
  UCHAR ucGPIO_ID;           
  UCHAR ucGPIOBitShift;	     
	UCHAR ucGPIOBitVal;		     
  UCHAR ucAction;				     
}GPIO_PIN_CONTROL_PARAMETERS;

typedef struct _ENABLE_SCALER_PARAMETERS
{
  UCHAR ucScaler;            
  UCHAR ucEnable;            
  UCHAR ucTVStandard;        
  UCHAR ucPadding[1];
}ENABLE_SCALER_PARAMETERS; 
#define ENABLE_SCALER_PS_ALLOCATION ENABLE_SCALER_PARAMETERS 

#define SCALER_BYPASS_AUTO_CENTER_NO_REPLICATION    0
#define SCALER_BYPASS_AUTO_CENTER_AUTO_REPLICATION  1
#define SCALER_ENABLE_2TAP_ALPHA_MODE               2
#define SCALER_ENABLE_MULTITAP_MODE                 3

typedef struct _ENABLE_HARDWARE_ICON_CURSOR_PARAMETERS
{
  ULONG  usHWIconHorzVertPosn;        
  UCHAR  ucHWIconVertOffset;          
  UCHAR  ucHWIconHorzOffset;          
  UCHAR  ucSelection;                 
  UCHAR  ucEnable;                    
}ENABLE_HARDWARE_ICON_CURSOR_PARAMETERS;

typedef struct _ENABLE_HARDWARE_ICON_CURSOR_PS_ALLOCATION
{
  ENABLE_HARDWARE_ICON_CURSOR_PARAMETERS  sEnableIcon;
  ENABLE_CRTC_PARAMETERS                  sReserved;  
}ENABLE_HARDWARE_ICON_CURSOR_PS_ALLOCATION;

typedef struct _ENABLE_GRAPH_SURFACE_PARAMETERS
{
  USHORT usHight;                     
  USHORT usWidth;                     
  UCHAR  ucSurface;                   
  UCHAR  ucPadding[3];
}ENABLE_GRAPH_SURFACE_PARAMETERS;

typedef struct _ENABLE_GRAPH_SURFACE_PARAMETERS_V1_2
{
  USHORT usHight;                     
  USHORT usWidth;                     
  UCHAR  ucSurface;                   
  UCHAR  ucEnable;                    
  UCHAR  ucPadding[2];
}ENABLE_GRAPH_SURFACE_PARAMETERS_V1_2;

typedef struct _ENABLE_GRAPH_SURFACE_PARAMETERS_V1_3
{
  USHORT usHight;                     
  USHORT usWidth;                     
  UCHAR  ucSurface;                   
  UCHAR  ucEnable;                    
  USHORT usDeviceId;                  
}ENABLE_GRAPH_SURFACE_PARAMETERS_V1_3;

typedef struct _ENABLE_GRAPH_SURFACE_PARAMETERS_V1_4
{
  USHORT usHight;                     
  USHORT usWidth;                     
  USHORT usGraphPitch;
  UCHAR  ucColorDepth;
  UCHAR  ucPixelFormat;
  UCHAR  ucSurface;                   
  UCHAR  ucEnable;                    
  UCHAR  ucModeType;
  UCHAR  ucReserved;
}ENABLE_GRAPH_SURFACE_PARAMETERS_V1_4;

#define ATOM_GRAPH_CONTROL_SET_PITCH             0x0f
#define ATOM_GRAPH_CONTROL_SET_DISP_START        0x10

typedef struct _ENABLE_GRAPH_SURFACE_PS_ALLOCATION
{
  ENABLE_GRAPH_SURFACE_PARAMETERS sSetSurface;          
  ENABLE_YUV_PS_ALLOCATION        sReserved; 
}ENABLE_GRAPH_SURFACE_PS_ALLOCATION;

typedef struct _MEMORY_CLEAN_UP_PARAMETERS
{
  USHORT  usMemoryStart;                
  USHORT  usMemorySize;                 
}MEMORY_CLEAN_UP_PARAMETERS;
#define MEMORY_CLEAN_UP_PS_ALLOCATION MEMORY_CLEAN_UP_PARAMETERS

typedef struct  _GET_DISPLAY_SURFACE_SIZE_PARAMETERS
{
  USHORT  usX_Size;                     
  USHORT  usY_Size;
}GET_DISPLAY_SURFACE_SIZE_PARAMETERS; 

typedef struct  _GET_DISPLAY_SURFACE_SIZE_PARAMETERS_V2
{
  union{
    USHORT  usX_Size;                     
    USHORT  usSurface; 
  };
  USHORT usY_Size;
  USHORT usDispXStart;               
  USHORT usDispYStart;
}GET_DISPLAY_SURFACE_SIZE_PARAMETERS_V2; 


typedef struct _PALETTE_DATA_CONTROL_PARAMETERS_V3 
{
  UCHAR  ucLutId;
  UCHAR  ucAction;
  USHORT usLutStartIndex;
  USHORT usLutLength;
  USHORT usLutOffsetInVram;
}PALETTE_DATA_CONTROL_PARAMETERS_V3;

#define PALETTE_DATA_AUTO_FILL            1
#define PALETTE_DATA_READ                 2
#define PALETTE_DATA_WRITE                3


typedef struct _INTERRUPT_SERVICE_PARAMETERS_V2
{
  UCHAR  ucInterruptId;
  UCHAR  ucServiceId;
  UCHAR  ucStatus;
  UCHAR  ucReserved;
}INTERRUPT_SERVICE_PARAMETER_V2;

#define HDP1_INTERRUPT_ID                 1
#define HDP2_INTERRUPT_ID                 2
#define HDP3_INTERRUPT_ID                 3
#define HDP4_INTERRUPT_ID                 4
#define HDP5_INTERRUPT_ID                 5
#define HDP6_INTERRUPT_ID                 6
#define SW_INTERRUPT_ID                   11   

#define INTERRUPT_SERVICE_GEN_SW_INT      1
#define INTERRUPT_SERVICE_GET_STATUS      2

 
#define INTERRUPT_STATUS__INT_TRIGGER     1
#define INTERRUPT_STATUS__HPD_HIGH        2

typedef struct _INDIRECT_IO_ACCESS
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  UCHAR                    IOAccessSequence[256];
} INDIRECT_IO_ACCESS;

#define INDIRECT_READ              0x00
#define INDIRECT_WRITE             0x80

#define INDIRECT_IO_MM             0
#define INDIRECT_IO_PLL            1
#define INDIRECT_IO_MC             2
#define INDIRECT_IO_PCIE           3
#define INDIRECT_IO_PCIEP          4
#define INDIRECT_IO_NBMISC         5

#define INDIRECT_IO_PLL_READ       INDIRECT_IO_PLL   | INDIRECT_READ
#define INDIRECT_IO_PLL_WRITE      INDIRECT_IO_PLL   | INDIRECT_WRITE
#define INDIRECT_IO_MC_READ        INDIRECT_IO_MC    | INDIRECT_READ
#define INDIRECT_IO_MC_WRITE       INDIRECT_IO_MC    | INDIRECT_WRITE
#define INDIRECT_IO_PCIE_READ      INDIRECT_IO_PCIE  | INDIRECT_READ
#define INDIRECT_IO_PCIE_WRITE     INDIRECT_IO_PCIE  | INDIRECT_WRITE
#define INDIRECT_IO_PCIEP_READ     INDIRECT_IO_PCIEP | INDIRECT_READ
#define INDIRECT_IO_PCIEP_WRITE    INDIRECT_IO_PCIEP | INDIRECT_WRITE
#define INDIRECT_IO_NBMISC_READ    INDIRECT_IO_NBMISC | INDIRECT_READ
#define INDIRECT_IO_NBMISC_WRITE   INDIRECT_IO_NBMISC | INDIRECT_WRITE

typedef struct _ATOM_OEM_INFO
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  ATOM_I2C_ID_CONFIG_ACCESS sucI2cId;
}ATOM_OEM_INFO;

typedef struct _ATOM_TV_MODE
{
   UCHAR	ucVMode_Num;			  
   UCHAR	ucTV_Mode_Num;			
}ATOM_TV_MODE;

typedef struct _ATOM_BIOS_INT_TVSTD_MODE
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
   USHORT	usTV_Mode_LUT_Offset;	
   USHORT	usTV_FIFO_Offset;		  
   USHORT	usNTSC_Tbl_Offset;		
   USHORT	usPAL_Tbl_Offset;		  
   USHORT	usCV_Tbl_Offset;		  
}ATOM_BIOS_INT_TVSTD_MODE;


typedef struct _ATOM_TV_MODE_SCALER_PTR
{
   USHORT	ucFilter0_Offset;		
   USHORT	usFilter1_Offset;		
   UCHAR	ucTV_Mode_Num;
}ATOM_TV_MODE_SCALER_PTR;

typedef struct _ATOM_STANDARD_VESA_TIMING
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  ATOM_DTD_FORMAT 				 aModeTimings[16];      
}ATOM_STANDARD_VESA_TIMING;


typedef struct _ATOM_STD_FORMAT
{ 
  USHORT    usSTD_HDisp;
  USHORT    usSTD_VDisp;
  USHORT    usSTD_RefreshRate;
  USHORT    usReserved;
}ATOM_STD_FORMAT;

typedef struct _ATOM_VESA_TO_EXTENDED_MODE
{
  USHORT  usVESA_ModeNumber;
  USHORT  usExtendedModeNumber;
}ATOM_VESA_TO_EXTENDED_MODE;

typedef struct _ATOM_VESA_TO_INTENAL_MODE_LUT
{ 
  ATOM_COMMON_TABLE_HEADER   sHeader;  
  ATOM_VESA_TO_EXTENDED_MODE asVESA_ToExtendedModeInfo[76];
}ATOM_VESA_TO_INTENAL_MODE_LUT;

typedef struct _ATOM_MEMORY_VENDOR_BLOCK{
	UCHAR												ucMemoryType;
	UCHAR												ucMemoryVendor;
	UCHAR												ucAdjMCId;
	UCHAR												ucDynClkId;
	ULONG												ulDllResetClkRange;
}ATOM_MEMORY_VENDOR_BLOCK;


typedef struct _ATOM_MEMORY_SETTING_ID_CONFIG{
#if ATOM_BIG_ENDIAN
	ULONG												ucMemBlkId:8;
	ULONG												ulMemClockRange:24;
#else
	ULONG												ulMemClockRange:24;
	ULONG												ucMemBlkId:8;
#endif
}ATOM_MEMORY_SETTING_ID_CONFIG;

typedef union _ATOM_MEMORY_SETTING_ID_CONFIG_ACCESS
{
  ATOM_MEMORY_SETTING_ID_CONFIG slAccess;
  ULONG                         ulAccess;
}ATOM_MEMORY_SETTING_ID_CONFIG_ACCESS;


typedef struct _ATOM_MEMORY_SETTING_DATA_BLOCK{
	ATOM_MEMORY_SETTING_ID_CONFIG_ACCESS			ulMemoryID;
	ULONG															        aulMemData[1];
}ATOM_MEMORY_SETTING_DATA_BLOCK;


typedef struct _ATOM_INIT_REG_INDEX_FORMAT{
	 USHORT											usRegIndex;                                     
	 UCHAR											ucPreRegDataLength;                             
}ATOM_INIT_REG_INDEX_FORMAT;


typedef struct _ATOM_INIT_REG_BLOCK{
	USHORT													usRegIndexTblSize;													
	USHORT													usRegDataBlkSize;														
	ATOM_INIT_REG_INDEX_FORMAT			asRegIndexBuf[1];
	ATOM_MEMORY_SETTING_DATA_BLOCK	asRegDataBuf[1];
}ATOM_INIT_REG_BLOCK;

#define END_OF_REG_INDEX_BLOCK  0x0ffff
#define END_OF_REG_DATA_BLOCK   0x00000000
#define ATOM_INIT_REG_MASK_FLAG 0x80               
#define	CLOCK_RANGE_HIGHEST			0x00ffffff

#define VALUE_DWORD             SIZEOF ULONG
#define VALUE_SAME_AS_ABOVE     0
#define VALUE_MASK_DWORD        0x84

#define INDEX_ACCESS_RANGE_BEGIN	    (VALUE_DWORD + 1)
#define INDEX_ACCESS_RANGE_END		    (INDEX_ACCESS_RANGE_BEGIN + 1)
#define VALUE_INDEX_ACCESS_SINGLE	    (INDEX_ACCESS_RANGE_END + 1)
#define ACCESS_PLACEHOLDER             0x80

typedef struct _ATOM_MC_INIT_PARAM_TABLE
{ 
  ATOM_COMMON_TABLE_HEADER		sHeader;
  USHORT											usAdjustARB_SEQDataOffset;
  USHORT											usMCInitMemTypeTblOffset;
  USHORT											usMCInitCommonTblOffset;
  USHORT											usMCInitPowerDownTblOffset;
	ULONG												ulARB_SEQDataBuf[32];
	ATOM_INIT_REG_BLOCK					asMCInitMemType;
	ATOM_INIT_REG_BLOCK					asMCInitCommon;
}ATOM_MC_INIT_PARAM_TABLE;


#define _4Mx16              0x2
#define _4Mx32              0x3
#define _8Mx16              0x12
#define _8Mx32              0x13
#define _16Mx16             0x22
#define _16Mx32             0x23
#define _32Mx16             0x32
#define _32Mx32             0x33
#define _64Mx8              0x41
#define _64Mx16             0x42
#define _64Mx32             0x43
#define _128Mx8             0x51
#define _128Mx16            0x52
#define _256Mx8             0x61
#define _256Mx16            0x62

#define SAMSUNG             0x1
#define INFINEON            0x2
#define ELPIDA              0x3
#define ETRON               0x4
#define NANYA               0x5
#define HYNIX               0x6
#define MOSEL               0x7
#define WINBOND             0x8
#define ESMT                0x9
#define MICRON              0xF

#define QIMONDA             INFINEON
#define PROMOS              MOSEL
#define KRETON              INFINEON
#define ELIXIR              NANYA


#define UCODE_ROM_START_ADDRESS		0x1b800
#define	UCODE_SIGNATURE			0x4375434d 


typedef struct _MCuCodeHeader
{
  ULONG  ulSignature;
  UCHAR  ucRevision;
  UCHAR  ucChecksum;
  UCHAR  ucReserved1;
  UCHAR  ucReserved2;
  USHORT usParametersLength;
  USHORT usUCodeLength;
  USHORT usReserved1;
  USHORT usReserved2;
} MCuCodeHeader;


#define ATOM_MAX_NUMBER_OF_VRAM_MODULE	16

#define ATOM_VRAM_MODULE_MEMORY_VENDOR_ID_MASK	0xF
typedef struct _ATOM_VRAM_MODULE_V1
{
  ULONG                      ulReserved;
  USHORT                     usEMRSValue;  
  USHORT                     usMRSValue;
  USHORT                     usReserved;
  UCHAR                      ucExtMemoryID;     
  UCHAR                      ucMemoryType;      
  UCHAR                      ucMemoryVenderID;  
  UCHAR                      ucMemoryDeviceCfg; 
  UCHAR                      ucRow;             
  UCHAR                      ucColumn;          
  UCHAR                      ucBank;            
  UCHAR                      ucRank;            
  UCHAR                      ucChannelNum;      
  UCHAR                      ucChannelConfig;   
  UCHAR                      ucDefaultMVDDQ_ID; 
  UCHAR                      ucDefaultMVDDC_ID; 
  UCHAR                      ucReserved[2];
}ATOM_VRAM_MODULE_V1;


typedef struct _ATOM_VRAM_MODULE_V2
{
  ULONG                      ulReserved;
  ULONG                      ulFlags;     			
  ULONG                      ulEngineClock;     
  ULONG                      ulMemoryClock;     
  USHORT                     usEMRS2Value;      
  USHORT                     usEMRS3Value;      
  USHORT                     usEMRSValue;  
  USHORT                     usMRSValue;
  USHORT                     usReserved;
  UCHAR                      ucExtMemoryID;     
  UCHAR                      ucMemoryType;      
  UCHAR                      ucMemoryVenderID;  
  UCHAR                      ucMemoryDeviceCfg; 
  UCHAR                      ucRow;             
  UCHAR                      ucColumn;          
  UCHAR                      ucBank;            
  UCHAR                      ucRank;            
  UCHAR                      ucChannelNum;      
  UCHAR                      ucChannelConfig;   
  UCHAR                      ucDefaultMVDDQ_ID; 
  UCHAR                      ucDefaultMVDDC_ID; 
  UCHAR                      ucRefreshRateFactor;
  UCHAR                      ucReserved[3];
}ATOM_VRAM_MODULE_V2;


typedef	struct _ATOM_MEMORY_TIMING_FORMAT
{
	ULONG											 ulClkRange;				
  union{
	  USHORT										 usMRS;							
    USHORT                     usDDR3_MR0;
  };
  union{
	  USHORT										 usEMRS;						
    USHORT                     usDDR3_MR1;
  };
	UCHAR											 ucCL;							
	UCHAR											 ucWL;							
	UCHAR											 uctRAS;						
	UCHAR											 uctRC;							
	UCHAR											 uctRFC;						
	UCHAR											 uctRCDR;						
	UCHAR											 uctRCDW;						
	UCHAR											 uctRP;							
	UCHAR											 uctRRD;						
	UCHAR											 uctWR;							
	UCHAR											 uctWTR;						
	UCHAR											 uctPDIX;						
	UCHAR											 uctFAW;						
	UCHAR											 uctAOND;						
  union 
  {
    struct {
	    UCHAR											 ucflag;						
	    UCHAR											 ucReserved;						
    };
    USHORT                   usDDR3_MR2;
  };
}ATOM_MEMORY_TIMING_FORMAT;


typedef	struct _ATOM_MEMORY_TIMING_FORMAT_V1
{
	ULONG											 ulClkRange;				
	USHORT										 usMRS;							
	USHORT										 usEMRS;						
	UCHAR											 ucCL;							
	UCHAR											 ucWL;							
	UCHAR											 uctRAS;						
	UCHAR											 uctRC;							
	UCHAR											 uctRFC;						
	UCHAR											 uctRCDR;						
	UCHAR											 uctRCDW;						
	UCHAR											 uctRP;							
	UCHAR											 uctRRD;						
	UCHAR											 uctWR;							
	UCHAR											 uctWTR;						
	UCHAR											 uctPDIX;						
	UCHAR											 uctFAW;						
	UCHAR											 uctAOND;						
	UCHAR											 ucflag;						
	UCHAR											 uctCCDL;						
	UCHAR											 uctCRCRL;						
	UCHAR											 uctCRCWL;						
	UCHAR											 uctCKE;						
	UCHAR											 uctCKRSE;						
	UCHAR											 uctCKRSX;						
	UCHAR											 uctFAW32;						
	UCHAR											 ucMR5lo;					
	UCHAR											 ucMR5hi;					
	UCHAR											 ucTerminator;
}ATOM_MEMORY_TIMING_FORMAT_V1;

typedef	struct _ATOM_MEMORY_TIMING_FORMAT_V2
{
	ULONG											 ulClkRange;				
	USHORT										 usMRS;							
	USHORT										 usEMRS;						
	UCHAR											 ucCL;							
	UCHAR											 ucWL;							
	UCHAR											 uctRAS;						
	UCHAR											 uctRC;							
	UCHAR											 uctRFC;						
	UCHAR											 uctRCDR;						
	UCHAR											 uctRCDW;						
	UCHAR											 uctRP;							
	UCHAR											 uctRRD;						
	UCHAR											 uctWR;							
	UCHAR											 uctWTR;						
	UCHAR											 uctPDIX;						
	UCHAR											 uctFAW;						
	UCHAR											 uctAOND;						
	UCHAR											 ucflag;						
	UCHAR											 uctCCDL;						
	UCHAR											 uctCRCRL;						
	UCHAR											 uctCRCWL;						
	UCHAR											 uctCKE;						
	UCHAR											 uctCKRSE;						
	UCHAR											 uctCKRSX;						
	UCHAR											 uctFAW32;						
	UCHAR											 ucMR4lo;					
	UCHAR											 ucMR4hi;					
	UCHAR											 ucMR5lo;					
	UCHAR											 ucMR5hi;					
	UCHAR											 ucTerminator;
	UCHAR											 ucReserved;	
}ATOM_MEMORY_TIMING_FORMAT_V2;

typedef	struct _ATOM_MEMORY_FORMAT
{
	ULONG											 ulDllDisClock;			
  union{
    USHORT                     usEMRS2Value;      
    USHORT                     usDDR3_Reserved;   
  };
  union{
    USHORT                     usEMRS3Value;      
    USHORT                     usDDR3_MR3;        
  };
  UCHAR                      ucMemoryType;      
  UCHAR                      ucMemoryVenderID;  
  UCHAR                      ucRow;             
  UCHAR                      ucColumn;          
  UCHAR                      ucBank;            
  UCHAR                      ucRank;            
	UCHAR											 ucBurstSize;				
  UCHAR                      ucDllDisBit;				
  UCHAR                      ucRefreshRateFactor;	
	UCHAR											 ucDensity;					
	UCHAR											 ucPreamble;				
  UCHAR											 ucMemAttrib;				
	ATOM_MEMORY_TIMING_FORMAT	 asMemTiming[5];		
}ATOM_MEMORY_FORMAT;


typedef struct _ATOM_VRAM_MODULE_V3
{
	ULONG											 ulChannelMapCfg;		
	USHORT										 usSize;						
  USHORT                     usDefaultMVDDQ;		
  USHORT                     usDefaultMVDDC;		
	UCHAR                      ucExtMemoryID;     
  UCHAR                      ucChannelNum;      
	UCHAR											 ucChannelSize;			
	UCHAR											 ucVREFI;						
	UCHAR											 ucNPL_RT;					
	UCHAR											 ucFlag;						
	ATOM_MEMORY_FORMAT				 asMemory;					
}ATOM_VRAM_MODULE_V3;


#define NPL_RT_MASK															0x0f
#define BATTERY_ODT_MASK												0xc0

#define ATOM_VRAM_MODULE		 ATOM_VRAM_MODULE_V3

typedef struct _ATOM_VRAM_MODULE_V4
{
  ULONG	  ulChannelMapCfg;	                
  USHORT  usModuleSize;                     
  USHORT  usPrivateReserved;                
                                            
  USHORT  usReserved;
  UCHAR   ucExtMemoryID;    		            
  UCHAR   ucMemoryType;                     
  UCHAR   ucChannelNum;                     
  UCHAR   ucChannelWidth;                   
	UCHAR   ucDensity;                        
	UCHAR	  ucFlag;						                
	UCHAR	  ucMisc;						                
  UCHAR		ucVREFI;                          
  UCHAR   ucNPL_RT;                         
  UCHAR		ucPreamble;                       
  UCHAR   ucMemorySize;                     
                                            
  UCHAR   ucReserved[3];

  union{
    USHORT	usEMRS2Value;                   
    USHORT  usDDR3_Reserved;
  };
  union{
    USHORT	usEMRS3Value;                   
    USHORT  usDDR3_MR3;                     
  };  
  UCHAR   ucMemoryVenderID;  		            
  UCHAR	  ucRefreshRateFactor;              
  UCHAR   ucReserved2[2];
  ATOM_MEMORY_TIMING_FORMAT  asMemTiming[5];
}ATOM_VRAM_MODULE_V4;

#define VRAM_MODULE_V4_MISC_RANK_MASK       0x3
#define VRAM_MODULE_V4_MISC_DUAL_RANK       0x1
#define VRAM_MODULE_V4_MISC_BL_MASK         0x4
#define VRAM_MODULE_V4_MISC_BL8             0x4
#define VRAM_MODULE_V4_MISC_DUAL_CS         0x10

typedef struct _ATOM_VRAM_MODULE_V5
{
  ULONG	  ulChannelMapCfg;	                
  USHORT  usModuleSize;                     
  USHORT  usPrivateReserved;                
                                            
  USHORT  usReserved;
  UCHAR   ucExtMemoryID;    		            
  UCHAR   ucMemoryType;                     
  UCHAR   ucChannelNum;                     
  UCHAR   ucChannelWidth;                   
	UCHAR   ucDensity;                        
	UCHAR	  ucFlag;						                
	UCHAR	  ucMisc;						                
  UCHAR		ucVREFI;                          
  UCHAR   ucNPL_RT;                         
  UCHAR		ucPreamble;                       
  UCHAR   ucMemorySize;                     
                                            
  UCHAR   ucReserved[3];

  USHORT	usEMRS2Value;      		            
  USHORT	usEMRS3Value;      		            
  UCHAR   ucMemoryVenderID;  		            
  UCHAR	  ucRefreshRateFactor;              
  UCHAR	  ucFIFODepth;			                
  UCHAR   ucCDR_Bandwidth;		   
  ATOM_MEMORY_TIMING_FORMAT_V1  asMemTiming[5];
}ATOM_VRAM_MODULE_V5;

typedef struct _ATOM_VRAM_MODULE_V6
{
  ULONG	  ulChannelMapCfg;	                
  USHORT  usModuleSize;                     
  USHORT  usPrivateReserved;                
                                            
  USHORT  usReserved;
  UCHAR   ucExtMemoryID;    		            
  UCHAR   ucMemoryType;                     
  UCHAR   ucChannelNum;                     
  UCHAR   ucChannelWidth;                   
	UCHAR   ucDensity;                        
	UCHAR	  ucFlag;						                
	UCHAR	  ucMisc;						                
  UCHAR		ucVREFI;                          
  UCHAR   ucNPL_RT;                         
  UCHAR		ucPreamble;                       
  UCHAR   ucMemorySize;                     
                                            
  UCHAR   ucReserved[3];

  USHORT	usEMRS2Value;      		            
  USHORT	usEMRS3Value;      		            
  UCHAR   ucMemoryVenderID;  		            
  UCHAR	  ucRefreshRateFactor;              
  UCHAR	  ucFIFODepth;			                
  UCHAR   ucCDR_Bandwidth;		   
  ATOM_MEMORY_TIMING_FORMAT_V2  asMemTiming[5];
}ATOM_VRAM_MODULE_V6;

typedef struct _ATOM_VRAM_MODULE_V7
{
  ULONG	  ulChannelMapCfg;	                
  USHORT  usModuleSize;                     
  USHORT  usPrivateReserved;                
  USHORT  usEnableChannels;                 
  UCHAR   ucExtMemoryID;                    
  UCHAR   ucMemoryType;                     
  UCHAR   ucChannelNum;                     
  UCHAR   ucChannelWidth;                   
  UCHAR   ucDensity;                        
  UCHAR	  ucReserve;                        
  UCHAR	  ucMisc;                           
  UCHAR	  ucVREFI;                          
  UCHAR   ucNPL_RT;                         
  UCHAR	  ucPreamble;                       
  UCHAR   ucMemorySize;                     
  USHORT  usSEQSettingOffset;
  UCHAR   ucReserved;
  USHORT  usEMRS2Value;                     
  USHORT  usEMRS3Value;                     
  UCHAR   ucMemoryVenderID;                 
  UCHAR	  ucRefreshRateFactor;              
  UCHAR	  ucFIFODepth;                      
  UCHAR   ucCDR_Bandwidth;                  
  char    strMemPNString[20];               
}ATOM_VRAM_MODULE_V7;

typedef struct _ATOM_VRAM_INFO_V2
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  UCHAR                      ucNumOfVRAMModule;
  ATOM_VRAM_MODULE           aVramInfo[ATOM_MAX_NUMBER_OF_VRAM_MODULE];      
}ATOM_VRAM_INFO_V2;

typedef struct _ATOM_VRAM_INFO_V3
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
	USHORT										 usMemAdjustTblOffset;													 
	USHORT										 usMemClkPatchTblOffset;												 
	USHORT										 usRerseved;
	UCHAR           	         aVID_PinsShift[9];															 
  UCHAR                      ucNumOfVRAMModule;
  ATOM_VRAM_MODULE		       aVramInfo[ATOM_MAX_NUMBER_OF_VRAM_MODULE];      
	ATOM_INIT_REG_BLOCK				 asMemPatch;																		 
																																						 
}ATOM_VRAM_INFO_V3;

#define	ATOM_VRAM_INFO_LAST	     ATOM_VRAM_INFO_V3

typedef struct _ATOM_VRAM_INFO_V4
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  USHORT                     usMemAdjustTblOffset;													 
  USHORT                     usMemClkPatchTblOffset;												 
  USHORT										 usRerseved;
  UCHAR           	         ucMemDQ7_0ByteRemap;													   
  ULONG                      ulMemDQ7_0BitRemap;                             
  UCHAR                      ucReservde[4]; 
  UCHAR                      ucNumOfVRAMModule;
  ATOM_VRAM_MODULE_V4		     aVramInfo[ATOM_MAX_NUMBER_OF_VRAM_MODULE];      
	ATOM_INIT_REG_BLOCK				 asMemPatch;																		 
																																						 
}ATOM_VRAM_INFO_V4;

typedef struct _ATOM_VRAM_INFO_HEADER_V2_1
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  USHORT                     usMemAdjustTblOffset;													 
  USHORT                     usMemClkPatchTblOffset;												 
  USHORT                     usPerBytePresetOffset;                          
  USHORT                     usReserved[3];
  UCHAR                      ucNumOfVRAMModule;                              
  UCHAR                      ucMemoryClkPatchTblVer;                         
  UCHAR                      ucVramModuleVer;                                
  UCHAR                      ucReserved; 
  ATOM_VRAM_MODULE_V7		     aVramInfo[ATOM_MAX_NUMBER_OF_VRAM_MODULE];      
}ATOM_VRAM_INFO_HEADER_V2_1;


typedef struct _ATOM_VRAM_GPIO_DETECTION_INFO
{
  ATOM_COMMON_TABLE_HEADER   sHeader;
  UCHAR           	         aVID_PinsShift[9];   
}ATOM_VRAM_GPIO_DETECTION_INFO;


typedef struct _ATOM_MEMORY_TRAINING_INFO
{
	ATOM_COMMON_TABLE_HEADER   sHeader;
	UCHAR											 ucTrainingLoop;
	UCHAR											 ucReserved[3];
	ATOM_INIT_REG_BLOCK				 asMemTrainingSetting;
}ATOM_MEMORY_TRAINING_INFO;


typedef struct SW_I2C_CNTL_DATA_PARAMETERS
{
  UCHAR    ucControl;
  UCHAR    ucData; 
  UCHAR    ucSatus; 
  UCHAR    ucTemp; 
} SW_I2C_CNTL_DATA_PARAMETERS;

#define SW_I2C_CNTL_DATA_PS_ALLOCATION  SW_I2C_CNTL_DATA_PARAMETERS

typedef struct _SW_I2C_IO_DATA_PARAMETERS
{                               
  USHORT   GPIO_Info;
  UCHAR    ucAct; 
  UCHAR    ucData; 
 } SW_I2C_IO_DATA_PARAMETERS;

#define SW_I2C_IO_DATA_PS_ALLOCATION  SW_I2C_IO_DATA_PARAMETERS

#define SW_I2C_IO_RESET       0
#define SW_I2C_IO_GET         1
#define SW_I2C_IO_DRIVE       2
#define SW_I2C_IO_SET         3
#define SW_I2C_IO_START       4

#define SW_I2C_IO_CLOCK       0
#define SW_I2C_IO_DATA        0x80

#define SW_I2C_IO_ZERO        0
#define SW_I2C_IO_ONE         0x100

#define SW_I2C_CNTL_READ      0
#define SW_I2C_CNTL_WRITE     1
#define SW_I2C_CNTL_START     2
#define SW_I2C_CNTL_STOP      3
#define SW_I2C_CNTL_OPEN      4
#define SW_I2C_CNTL_CLOSE     5
#define SW_I2C_CNTL_WRITE1BIT 6

#define VESA_OEM_PRODUCT_REV			            "01.00"
#define VESA_MODE_ATTRIBUTE_MODE_SUPPORT	     0xBB	
#define VESA_MODE_WIN_ATTRIBUTE						     7
#define VESA_WIN_SIZE											     64

typedef struct _PTR_32_BIT_STRUCTURE
{
	USHORT	Offset16;			
	USHORT	Segment16;				
} PTR_32_BIT_STRUCTURE;

typedef union _PTR_32_BIT_UNION
{
	PTR_32_BIT_STRUCTURE	SegmentOffset;
	ULONG					        Ptr32_Bit;
} PTR_32_BIT_UNION;

typedef struct _VBE_1_2_INFO_BLOCK_UPDATABLE
{
	UCHAR				      VbeSignature[4];
	USHORT				    VbeVersion;
	PTR_32_BIT_UNION	OemStringPtr;
	UCHAR				      Capabilities[4];
	PTR_32_BIT_UNION	VideoModePtr;
	USHORT				    TotalMemory;
} VBE_1_2_INFO_BLOCK_UPDATABLE;


typedef struct _VBE_2_0_INFO_BLOCK_UPDATABLE
{
	VBE_1_2_INFO_BLOCK_UPDATABLE	CommonBlock;
	USHORT							    OemSoftRev;
	PTR_32_BIT_UNION				OemVendorNamePtr;
	PTR_32_BIT_UNION				OemProductNamePtr;
	PTR_32_BIT_UNION				OemProductRevPtr;
} VBE_2_0_INFO_BLOCK_UPDATABLE;

typedef union _VBE_VERSION_UNION
{
	VBE_2_0_INFO_BLOCK_UPDATABLE	VBE_2_0_InfoBlock;
	VBE_1_2_INFO_BLOCK_UPDATABLE	VBE_1_2_InfoBlock;
} VBE_VERSION_UNION;

typedef struct _VBE_INFO_BLOCK
{
	VBE_VERSION_UNION			UpdatableVBE_Info;
	UCHAR						      Reserved[222];
	UCHAR						      OemData[256];
} VBE_INFO_BLOCK;

typedef struct _VBE_FP_INFO
{
  USHORT	HSize;
	USHORT	VSize;
	USHORT	FPType;
	UCHAR		RedBPP;
	UCHAR		GreenBPP;
	UCHAR		BlueBPP;
	UCHAR		ReservedBPP;
	ULONG		RsvdOffScrnMemSize;
	ULONG		RsvdOffScrnMEmPtr;
	UCHAR		Reserved[14];
} VBE_FP_INFO;

typedef struct _VESA_MODE_INFO_BLOCK
{
  USHORT    ModeAttributes;  
	UCHAR     WinAAttributes;  
	UCHAR     WinBAttributes;  
	USHORT    WinGranularity;  
	USHORT    WinSize;         
	USHORT    WinASegment;     
	USHORT    WinBSegment;     
	ULONG     WinFuncPtr;      
	USHORT    BytesPerScanLine;

  USHORT    XResolution;      
	USHORT    YResolution;      
	UCHAR     XCharSize;        
	UCHAR     YCharSize;        
	UCHAR     NumberOfPlanes;   
	UCHAR     BitsPerPixel;     
	UCHAR     NumberOfBanks;    
	UCHAR     MemoryModel;      
	UCHAR     BankSize;         
	UCHAR     NumberOfImagePages;
	UCHAR     ReservedForPageFunction;

	UCHAR			RedMaskSize;        
	UCHAR			RedFieldPosition;   
	UCHAR			GreenMaskSize;      
	UCHAR			GreenFieldPosition; 
	UCHAR			BlueMaskSize;       
	UCHAR			BlueFieldPosition;  
	UCHAR			RsvdMaskSize;       
	UCHAR			RsvdFieldPosition;  
	UCHAR			DirectColorModeInfo;

	ULONG			PhysBasePtr;        
	ULONG			Reserved_1;         
	USHORT		Reserved_2;         

	USHORT		LinBytesPerScanLine;  
	UCHAR			BnkNumberOfImagePages;
	UCHAR			LinNumberOfImagPages; 
	UCHAR			LinRedMaskSize;       
	UCHAR			LinRedFieldPosition;  
	UCHAR			LinGreenMaskSize;     
	UCHAR			LinGreenFieldPosition;
	UCHAR			LinBlueMaskSize;      
	UCHAR			LinBlueFieldPosition; 
	UCHAR			LinRsvdMaskSize;      
	UCHAR			LinRsvdFieldPosition; 
	ULONG			MaxPixelClock;        
	UCHAR			Reserved;             
} VESA_MODE_INFO_BLOCK;

#define ATOM_BIOS_EXTENDED_FUNCTION_CODE        0xA0	        
#define ATOM_BIOS_FUNCTION_COP_MODE             0x00
#define ATOM_BIOS_FUNCTION_SHORT_QUERY1         0x04
#define ATOM_BIOS_FUNCTION_SHORT_QUERY2         0x05
#define ATOM_BIOS_FUNCTION_SHORT_QUERY3         0x06
#define ATOM_BIOS_FUNCTION_GET_DDC              0x0B   
#define ATOM_BIOS_FUNCTION_ASIC_DSTATE          0x0E
#define ATOM_BIOS_FUNCTION_DEBUG_PLAY           0x0F
#define ATOM_BIOS_FUNCTION_STV_STD              0x16
#define ATOM_BIOS_FUNCTION_DEVICE_DET           0x17
#define ATOM_BIOS_FUNCTION_DEVICE_SWITCH        0x18

#define ATOM_BIOS_FUNCTION_PANEL_CONTROL        0x82
#define ATOM_BIOS_FUNCTION_OLD_DEVICE_DET       0x83
#define ATOM_BIOS_FUNCTION_OLD_DEVICE_SWITCH    0x84
#define ATOM_BIOS_FUNCTION_HW_ICON              0x8A 
#define ATOM_BIOS_FUNCTION_SET_CMOS             0x8B
#define SUB_FUNCTION_UPDATE_DISPLAY_INFO        0x8000          
#define SUB_FUNCTION_UPDATE_EXPANSION_INFO      0x8100          

#define ATOM_BIOS_FUNCTION_DISPLAY_INFO         0x8D
#define ATOM_BIOS_FUNCTION_DEVICE_ON_OFF        0x8E
#define ATOM_BIOS_FUNCTION_VIDEO_STATE          0x8F 
#define ATOM_SUB_FUNCTION_GET_CRITICAL_STATE    0x0300          
#define ATOM_SUB_FUNCTION_GET_LIDSTATE          0x0700          
#define ATOM_SUB_FUNCTION_THERMAL_STATE_NOTICE  0x1400          
#define ATOM_SUB_FUNCTION_CRITICAL_STATE_NOTICE 0x8300          
#define ATOM_SUB_FUNCTION_SET_LIDSTATE          0x8500          
#define ATOM_SUB_FUNCTION_GET_REQ_DISPLAY_FROM_SBIOS_MODE 0x8900
#define ATOM_SUB_FUNCTION_INFORM_ADC_SUPPORT    0x9400          
     

#define ATOM_BIOS_FUNCTION_VESA_DPMS            0x4F10          
#define ATOM_SUB_FUNCTION_SET_DPMS              0x0001          
#define ATOM_SUB_FUNCTION_GET_DPMS              0x0002          
#define ATOM_PARAMETER_VESA_DPMS_ON             0x0000          
#define ATOM_PARAMETER_VESA_DPMS_STANDBY        0x0100          
#define ATOM_PARAMETER_VESA_DPMS_SUSPEND        0x0200          
#define ATOM_PARAMETER_VESA_DPMS_OFF            0x0400          
#define ATOM_PARAMETER_VESA_DPMS_REDUCE_ON      0x0800          

#define ATOM_BIOS_RETURN_CODE_MASK              0x0000FF00L
#define ATOM_BIOS_REG_HIGH_MASK                 0x0000FF00L
#define ATOM_BIOS_REG_LOW_MASK                  0x000000FFL


typedef struct _ASIC_TRANSMITTER_INFO
{
	USHORT usTransmitterObjId;
	USHORT usSupportDevice;
  UCHAR  ucTransmitterCmdTblId;
	UCHAR  ucConfig;
	UCHAR  ucEncoderID;					 
	UCHAR  ucOptionEncoderID;    
	UCHAR  uc2ndEncoderID;
	UCHAR  ucReserved;
}ASIC_TRANSMITTER_INFO;

#define ASIC_TRANSMITTER_INFO_CONFIG__DVO_SDR_MODE          0x01
#define ASIC_TRANSMITTER_INFO_CONFIG__COHERENT_MODE         0x02
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODEROBJ_ID_MASK    0xc4
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_A             0x00
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_B             0x04
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_C             0x40
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_D             0x44
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_E             0x80
#define ASIC_TRANSMITTER_INFO_CONFIG__ENCODER_F             0x84

typedef struct _ASIC_ENCODER_INFO
{
	UCHAR ucEncoderID;
	UCHAR ucEncoderConfig;
  USHORT usEncoderCmdTblId;
}ASIC_ENCODER_INFO;

typedef struct _ATOM_DISP_OUT_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
	USHORT ptrTransmitterInfo;
	USHORT ptrEncoderInfo;
	ASIC_TRANSMITTER_INFO  asTransmitterInfo[1];
	ASIC_ENCODER_INFO      asEncoderInfo[1];
}ATOM_DISP_OUT_INFO;

typedef struct _ATOM_DISP_OUT_INFO_V2
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
	USHORT ptrTransmitterInfo;
	USHORT ptrEncoderInfo;
  USHORT ptrMainCallParserFar;                  
	ASIC_TRANSMITTER_INFO  asTransmitterInfo[1];
	ASIC_ENCODER_INFO      asEncoderInfo[1];
}ATOM_DISP_OUT_INFO_V2;


typedef struct _ATOM_DISP_CLOCK_ID {
  UCHAR ucPpllId; 
  UCHAR ucPpllAttribute;
}ATOM_DISP_CLOCK_ID;

#define CLOCK_SOURCE_SHAREABLE            0x01
#define CLOCK_SOURCE_DP_MODE              0x02
#define CLOCK_SOURCE_NONE_DP_MODE         0x04

typedef struct _ASIC_TRANSMITTER_INFO_V2
{
	USHORT usTransmitterObjId;
	USHORT usDispClkIdOffset;    
  UCHAR  ucTransmitterCmdTblId;
	UCHAR  ucConfig;
	UCHAR  ucEncoderID;					 
	UCHAR  ucOptionEncoderID;    
	UCHAR  uc2ndEncoderID;
	UCHAR  ucReserved;
}ASIC_TRANSMITTER_INFO_V2;

typedef struct _ATOM_DISP_OUT_INFO_V3
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
	USHORT ptrTransmitterInfo;
	USHORT ptrEncoderInfo;
  USHORT ptrMainCallParserFar;                  
  USHORT usReserved;
  UCHAR  ucDCERevision;   
  UCHAR  ucMaxDispEngineNum;
  UCHAR  ucMaxActiveDispEngineNum;
  UCHAR  ucMaxPPLLNum;
  UCHAR  ucCoreRefClkSource;                          
  UCHAR  ucReserved[3];
	ASIC_TRANSMITTER_INFO_V2  asTransmitterInfo[1];     
}ATOM_DISP_OUT_INFO_V3;

typedef enum CORE_REF_CLK_SOURCE{
  CLOCK_SRC_XTALIN=0,
  CLOCK_SRC_XO_IN=1,
  CLOCK_SRC_XO_IN2=2,
}CORE_REF_CLK_SOURCE;

typedef struct _ATOM_DISPLAY_DEVICE_PRIORITY_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
	USHORT asDevicePriority[16];
}ATOM_DISPLAY_DEVICE_PRIORITY_INFO;

typedef struct _PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS
{
	USHORT	lpAuxRequest;
	USHORT  lpDataOut;
	UCHAR		ucChannelID;
	union
	{
  UCHAR   ucReplyStatus;
	UCHAR   ucDelay;
	};
  UCHAR   ucDataOutLen;
	UCHAR   ucReserved;
}PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS;

typedef struct _PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2
{
	USHORT	lpAuxRequest;
	USHORT  lpDataOut;
	UCHAR		ucChannelID;
	union
	{
  UCHAR   ucReplyStatus;
	UCHAR   ucDelay;
	};
  UCHAR   ucDataOutLen;
	UCHAR   ucHPD_ID;                                       
}PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2;

#define PROCESS_AUX_CHANNEL_TRANSACTION_PS_ALLOCATION			PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS


typedef struct _DP_ENCODER_SERVICE_PARAMETERS
{
	USHORT ucLinkClock;
	union 
	{
	UCHAR ucConfig;				
	UCHAR ucI2cId;				
	};
	UCHAR ucAction;
	UCHAR ucStatus;
	UCHAR ucLaneNum;
	UCHAR ucReserved[2];
}DP_ENCODER_SERVICE_PARAMETERS;

#define ATOM_DP_ACTION_GET_SINK_TYPE							0x01
#define ATOM_DP_ACTION_TRAINING_START							0x02
#define ATOM_DP_ACTION_TRAINING_COMPLETE					0x03
#define ATOM_DP_ACTION_TRAINING_PATTERN_SEL				0x04
#define ATOM_DP_ACTION_SET_VSWING_PREEMP					0x05
#define ATOM_DP_ACTION_GET_VSWING_PREEMP					0x06
#define ATOM_DP_ACTION_BLANKING                   0x07

#define ATOM_DP_CONFIG_ENCODER_SEL_MASK						0x03
#define ATOM_DP_CONFIG_DIG1_ENCODER								0x00
#define ATOM_DP_CONFIG_DIG2_ENCODER								0x01
#define ATOM_DP_CONFIG_EXTERNAL_ENCODER						0x02
#define ATOM_DP_CONFIG_LINK_SEL_MASK							0x04
#define ATOM_DP_CONFIG_LINK_A											0x00
#define ATOM_DP_CONFIG_LINK_B											0x04
#define DP_ENCODER_SERVICE_PS_ALLOCATION				WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS


typedef struct _DP_ENCODER_SERVICE_PARAMETERS_V2
{
	USHORT usExtEncoderObjId;   
  UCHAR  ucAuxId;
  UCHAR  ucAction;
  UCHAR  ucSinkType;          
  UCHAR  ucHPDId;             
	UCHAR  ucReserved[2];
}DP_ENCODER_SERVICE_PARAMETERS_V2;

typedef struct _DP_ENCODER_SERVICE_PS_ALLOCATION_V2
{
  DP_ENCODER_SERVICE_PARAMETERS_V2 asDPServiceParam;
  PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2 asAuxParam;
}DP_ENCODER_SERVICE_PS_ALLOCATION_V2;

#define DP_SERVICE_V2_ACTION_GET_SINK_TYPE							0x01
#define DP_SERVICE_V2_ACTION_DET_LCD_CONNECTION			    0x02


#define DPCD_SET_LINKRATE_LANENUM_PATTERN1_TBL_ADDR				ATOM_DP_TRAINING_TBL_ADDR		
#define DPCD_SET_SS_CNTL_TBL_ADDR													(ATOM_DP_TRAINING_TBL_ADDR + 8 )
#define DPCD_SET_LANE_VSWING_PREEMP_TBL_ADDR							(ATOM_DP_TRAINING_TBL_ADDR + 16 )
#define DPCD_SET_TRAINING_PATTERN0_TBL_ADDR								(ATOM_DP_TRAINING_TBL_ADDR + 24 )
#define DPCD_SET_TRAINING_PATTERN2_TBL_ADDR								(ATOM_DP_TRAINING_TBL_ADDR + 32)
#define DPCD_GET_LINKRATE_LANENUM_SS_TBL_ADDR							(ATOM_DP_TRAINING_TBL_ADDR + 40)
#define	DPCD_GET_LANE_STATUS_ADJUST_TBL_ADDR							(ATOM_DP_TRAINING_TBL_ADDR + 48)
#define DP_I2C_AUX_DDC_WRITE_START_TBL_ADDR								(ATOM_DP_TRAINING_TBL_ADDR + 60)
#define DP_I2C_AUX_DDC_WRITE_TBL_ADDR											(ATOM_DP_TRAINING_TBL_ADDR + 64)
#define DP_I2C_AUX_DDC_READ_START_TBL_ADDR								(ATOM_DP_TRAINING_TBL_ADDR + 72)
#define DP_I2C_AUX_DDC_READ_TBL_ADDR											(ATOM_DP_TRAINING_TBL_ADDR + 76)
#define DP_I2C_AUX_DDC_WRITE_END_TBL_ADDR                 (ATOM_DP_TRAINING_TBL_ADDR + 80) 
#define DP_I2C_AUX_DDC_READ_END_TBL_ADDR									(ATOM_DP_TRAINING_TBL_ADDR + 84)

typedef struct _PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS
{
	UCHAR   ucI2CSpeed;
 	union
	{
   UCHAR ucRegIndex;
   UCHAR ucStatus;
	};
	USHORT  lpI2CDataOut;
  UCHAR   ucFlag;               
  UCHAR   ucTransBytes;
  UCHAR   ucSlaveAddr;
  UCHAR   ucLineNumber;
}PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS;

#define PROCESS_I2C_CHANNEL_TRANSACTION_PS_ALLOCATION       PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS

#define HW_I2C_WRITE        1
#define HW_I2C_READ         0
#define I2C_2BYTE_ADDR      0x02

	
	
typedef struct  _ATOM_HW_MISC_OPERATION_INPUT_PARAMETER_V1_1 
{
  UCHAR  ucCmd;                
  UCHAR  ucReserved[3];
  ULONG  ulReserved;
}ATOM_HW_MISC_OPERATION_INPUT_PARAMETER_V1_1; 

typedef struct  _ATOM_HW_MISC_OPERATION_OUTPUT_PARAMETER_V1_1 
{
  UCHAR  ucReturnCode;        
  UCHAR  ucReserved[3];
  ULONG  ulReserved;
}ATOM_HW_MISC_OPERATION_OUTPUT_PARAMETER_V1_1;

#define  ATOM_GET_SDI_SUPPORT              0xF0

#define  ATOM_UNKNOWN_CMD                   0
#define  ATOM_FEATURE_NOT_SUPPORTED         1
#define  ATOM_FEATURE_SUPPORTED             2

typedef struct _ATOM_HW_MISC_OPERATION_PS_ALLOCATION
{
	ATOM_HW_MISC_OPERATION_INPUT_PARAMETER_V1_1        sInput_Output;
	PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS         sReserved; 
}ATOM_HW_MISC_OPERATION_PS_ALLOCATION;

	

typedef struct _SET_HWBLOCK_INSTANCE_PARAMETER_V2
{
   UCHAR ucHWBlkInst;                
   UCHAR ucReserved[3]; 
}SET_HWBLOCK_INSTANCE_PARAMETER_V2;

#define HWBLKINST_INSTANCE_MASK       0x07
#define HWBLKINST_HWBLK_MASK          0xF0
#define HWBLKINST_HWBLK_SHIFT         0x04

#define SELECT_DISP_ENGINE            0
#define SELECT_DISP_PLL               1
#define SELECT_DCIO_UNIPHY_LINK0      2
#define SELECT_DCIO_UNIPHY_LINK1      3
#define SELECT_DCIO_IMPCAL            4
#define SELECT_DCIO_DIG               6
#define SELECT_CRTC_PIXEL_RATE        7
#define SELECT_VGA_BLK                8

typedef struct _DIG_TRANSMITTER_INFO_HEADER_V3_1{  
  ATOM_COMMON_TABLE_HEADER sHeader;  
  USHORT usDPVsPreEmphSettingOffset;     
  USHORT usPhyAnalogRegListOffset;       
  USHORT usPhyAnalogSettingOffset;       
  USHORT usPhyPllRegListOffset;          
  USHORT usPhyPllSettingOffset;          
}DIG_TRANSMITTER_INFO_HEADER_V3_1;

typedef struct _CLOCK_CONDITION_REGESTER_INFO{
  USHORT usRegisterIndex;
  UCHAR  ucStartBit;
  UCHAR  ucEndBit;
}CLOCK_CONDITION_REGESTER_INFO;

typedef struct _CLOCK_CONDITION_SETTING_ENTRY{
  USHORT usMaxClockFreq;
  UCHAR  ucEncodeMode;
  UCHAR  ucPhySel;
  ULONG  ulAnalogSetting[1];
}CLOCK_CONDITION_SETTING_ENTRY;

typedef struct _CLOCK_CONDITION_SETTING_INFO{
  USHORT usEntrySize;
  CLOCK_CONDITION_SETTING_ENTRY asClkCondSettingEntry[1];
}CLOCK_CONDITION_SETTING_INFO;

typedef struct _PHY_CONDITION_REG_VAL{
  ULONG  ulCondition;
  ULONG  ulRegVal;
}PHY_CONDITION_REG_VAL;

typedef struct _PHY_CONDITION_REG_INFO{
  USHORT usRegIndex;
  USHORT usSize;
  PHY_CONDITION_REG_VAL asRegVal[1];
}PHY_CONDITION_REG_INFO;

typedef struct _PHY_ANALOG_SETTING_INFO{
  UCHAR  ucEncodeMode;
  UCHAR  ucPhySel;
  USHORT usSize;
  PHY_CONDITION_REG_INFO  asAnalogSetting[1];
}PHY_ANALOG_SETTING_INFO;

	

#define MC_MISC0__MEMORY_TYPE_MASK    0xF0000000
#define MC_MISC0__MEMORY_TYPE__GDDR1  0x10000000
#define MC_MISC0__MEMORY_TYPE__DDR2   0x20000000
#define MC_MISC0__MEMORY_TYPE__GDDR3  0x30000000
#define MC_MISC0__MEMORY_TYPE__GDDR4  0x40000000
#define MC_MISC0__MEMORY_TYPE__GDDR5  0x50000000
#define MC_MISC0__MEMORY_TYPE__DDR3   0xB0000000

	

typedef struct _ATOM_DAC_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  USHORT                   usMaxFrequency;      
  USHORT                   usReserved;
}ATOM_DAC_INFO;


typedef struct  _COMPASSIONATE_DATA           
{
  ATOM_COMMON_TABLE_HEADER sHeader; 

  
  UCHAR   ucDAC1_BG_Adjustment;
  UCHAR   ucDAC1_DAC_Adjustment;
  USHORT  usDAC1_FORCE_Data;
  
  UCHAR   ucDAC2_CRT2_BG_Adjustment;
  UCHAR   ucDAC2_CRT2_DAC_Adjustment;
  USHORT  usDAC2_CRT2_FORCE_Data;
  USHORT  usDAC2_CRT2_MUX_RegisterIndex;
  UCHAR   ucDAC2_CRT2_MUX_RegisterInfo;     
  UCHAR   ucDAC2_NTSC_BG_Adjustment;
  UCHAR   ucDAC2_NTSC_DAC_Adjustment;
  USHORT  usDAC2_TV1_FORCE_Data;
  USHORT  usDAC2_TV1_MUX_RegisterIndex;
  UCHAR   ucDAC2_TV1_MUX_RegisterInfo;      
  UCHAR   ucDAC2_CV_BG_Adjustment;
  UCHAR   ucDAC2_CV_DAC_Adjustment;
  USHORT  usDAC2_CV_FORCE_Data;
  USHORT  usDAC2_CV_MUX_RegisterIndex;
  UCHAR   ucDAC2_CV_MUX_RegisterInfo;       
  UCHAR   ucDAC2_PAL_BG_Adjustment;
  UCHAR   ucDAC2_PAL_DAC_Adjustment;
  USHORT  usDAC2_TV2_FORCE_Data;
}COMPASSIONATE_DATA;


typedef struct _ATOM_CONNECTOR_INFO
{
#if ATOM_BIG_ENDIAN
  UCHAR   bfConnectorType:4;
  UCHAR   bfAssociatedDAC:4;
#else
  UCHAR   bfAssociatedDAC:4;
  UCHAR   bfConnectorType:4;
#endif
}ATOM_CONNECTOR_INFO;

typedef union _ATOM_CONNECTOR_INFO_ACCESS
{
  ATOM_CONNECTOR_INFO sbfAccess;
  UCHAR               ucAccess;
}ATOM_CONNECTOR_INFO_ACCESS;

typedef struct _ATOM_CONNECTOR_INFO_I2C
{
  ATOM_CONNECTOR_INFO_ACCESS sucConnectorInfo;
  ATOM_I2C_ID_CONFIG_ACCESS  sucI2cId;
}ATOM_CONNECTOR_INFO_I2C;


typedef struct _ATOM_SUPPORTED_DEVICES_INFO
{ 
  ATOM_COMMON_TABLE_HEADER	sHeader;
  USHORT                    usDeviceSupport;
  ATOM_CONNECTOR_INFO_I2C   asConnInfo[ATOM_MAX_SUPPORTED_DEVICE_INFO];
}ATOM_SUPPORTED_DEVICES_INFO;

#define NO_INT_SRC_MAPPED       0xFF

typedef struct _ATOM_CONNECTOR_INC_SRC_BITMAP
{
  UCHAR   ucIntSrcBitmap;
}ATOM_CONNECTOR_INC_SRC_BITMAP;

typedef struct _ATOM_SUPPORTED_DEVICES_INFO_2
{ 
  ATOM_COMMON_TABLE_HEADER      sHeader;
  USHORT                        usDeviceSupport;
  ATOM_CONNECTOR_INFO_I2C       asConnInfo[ATOM_MAX_SUPPORTED_DEVICE_INFO_2];
  ATOM_CONNECTOR_INC_SRC_BITMAP asIntSrcInfo[ATOM_MAX_SUPPORTED_DEVICE_INFO_2];
}ATOM_SUPPORTED_DEVICES_INFO_2;

typedef struct _ATOM_SUPPORTED_DEVICES_INFO_2d1
{ 
  ATOM_COMMON_TABLE_HEADER      sHeader;
  USHORT                        usDeviceSupport;
  ATOM_CONNECTOR_INFO_I2C       asConnInfo[ATOM_MAX_SUPPORTED_DEVICE];
  ATOM_CONNECTOR_INC_SRC_BITMAP asIntSrcInfo[ATOM_MAX_SUPPORTED_DEVICE];
}ATOM_SUPPORTED_DEVICES_INFO_2d1;

#define ATOM_SUPPORTED_DEVICES_INFO_LAST ATOM_SUPPORTED_DEVICES_INFO_2d1



typedef struct _ATOM_MISC_CONTROL_INFO
{
   USHORT usFrequency;
   UCHAR  ucPLL_ChargePump;				                
   UCHAR  ucPLL_DutyCycle;				                
   UCHAR  ucPLL_VCO_Gain;				                  
   UCHAR  ucPLL_VoltageSwing;			                
}ATOM_MISC_CONTROL_INFO;  


#define ATOM_MAX_MISC_INFO       4

typedef struct _ATOM_TMDS_INFO
{
  ATOM_COMMON_TABLE_HEADER sHeader;  
  USHORT							usMaxFrequency;             
  ATOM_MISC_CONTROL_INFO				asMiscInfo[ATOM_MAX_MISC_INFO];
}ATOM_TMDS_INFO;


typedef struct _ATOM_ENCODER_ANALOG_ATTRIBUTE
{
  UCHAR ucTVStandard;     
  UCHAR ucPadding[1];
}ATOM_ENCODER_ANALOG_ATTRIBUTE;

typedef struct _ATOM_ENCODER_DIGITAL_ATTRIBUTE
{
  UCHAR ucAttribute;      
  UCHAR ucPadding[1];		
}ATOM_ENCODER_DIGITAL_ATTRIBUTE;

typedef union _ATOM_ENCODER_ATTRIBUTE
{
  ATOM_ENCODER_ANALOG_ATTRIBUTE sAlgAttrib;
  ATOM_ENCODER_DIGITAL_ATTRIBUTE sDigAttrib;
}ATOM_ENCODER_ATTRIBUTE;


typedef struct _DVO_ENCODER_CONTROL_PARAMETERS
{
  USHORT usPixelClock; 
  USHORT usEncoderID; 
  UCHAR  ucDeviceType;												
  UCHAR  ucAction;														
  ATOM_ENCODER_ATTRIBUTE usDevAttr;     		
}DVO_ENCODER_CONTROL_PARAMETERS;

typedef struct _DVO_ENCODER_CONTROL_PS_ALLOCATION
{                               
  DVO_ENCODER_CONTROL_PARAMETERS    sDVOEncoder;
  WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION      sReserved;     
}DVO_ENCODER_CONTROL_PS_ALLOCATION;


#define ATOM_XTMDS_ASIC_SI164_ID        1
#define ATOM_XTMDS_ASIC_SI178_ID        2
#define ATOM_XTMDS_ASIC_TFP513_ID       3
#define ATOM_XTMDS_SUPPORTED_SINGLELINK 0x00000001
#define ATOM_XTMDS_SUPPORTED_DUALLINK   0x00000002
#define ATOM_XTMDS_MVPU_FPGA            0x00000004

                           
typedef struct _ATOM_XTMDS_INFO
{
  ATOM_COMMON_TABLE_HEADER   sHeader;  
  USHORT                     usSingleLinkMaxFrequency; 
  ATOM_I2C_ID_CONFIG_ACCESS  sucI2cId;           
  UCHAR                      ucXtransimitterID;          
  UCHAR                      ucSupportedLink;    
  UCHAR                      ucSequnceAlterID;   
                                                 
  UCHAR                      ucMasterAddress;    
  UCHAR                      ucSlaveAddress;     
}ATOM_XTMDS_INFO;

typedef struct _DFP_DPMS_STATUS_CHANGE_PARAMETERS
{  
  UCHAR ucEnable;                     
  UCHAR ucDevice;                     
  UCHAR ucPadding[2];             
}DFP_DPMS_STATUS_CHANGE_PARAMETERS;


#define ATOM_PM_MISCINFO_SPLIT_CLOCK                     0x00000000L
#define ATOM_PM_MISCINFO_USING_MCLK_SRC                  0x00000001L
#define ATOM_PM_MISCINFO_USING_SCLK_SRC                  0x00000002L

#define ATOM_PM_MISCINFO_VOLTAGE_DROP_SUPPORT            0x00000004L
#define ATOM_PM_MISCINFO_VOLTAGE_DROP_ACTIVE_HIGH        0x00000008L

#define ATOM_PM_MISCINFO_LOAD_PERFORMANCE_EN             0x00000010L

#define ATOM_PM_MISCINFO_ENGINE_CLOCK_CONTRL_EN          0x00000020L
#define ATOM_PM_MISCINFO_MEMORY_CLOCK_CONTRL_EN          0x00000040L
#define ATOM_PM_MISCINFO_PROGRAM_VOLTAGE                 0x00000080L  
 
#define ATOM_PM_MISCINFO_ASIC_REDUCED_SPEED_SCLK_EN      0x00000100L
#define ATOM_PM_MISCINFO_ASIC_DYNAMIC_VOLTAGE_EN         0x00000200L
#define ATOM_PM_MISCINFO_ASIC_SLEEP_MODE_EN              0x00000400L
#define ATOM_PM_MISCINFO_LOAD_BALANCE_EN                 0x00000800L
#define ATOM_PM_MISCINFO_DEFAULT_DC_STATE_ENTRY_TRUE     0x00001000L
#define ATOM_PM_MISCINFO_DEFAULT_LOW_DC_STATE_ENTRY_TRUE 0x00002000L
#define ATOM_PM_MISCINFO_LOW_LCD_REFRESH_RATE            0x00004000L

#define ATOM_PM_MISCINFO_DRIVER_DEFAULT_MODE             0x00008000L
#define ATOM_PM_MISCINFO_OVER_CLOCK_MODE                 0x00010000L 
#define ATOM_PM_MISCINFO_OVER_DRIVE_MODE                 0x00020000L
#define ATOM_PM_MISCINFO_POWER_SAVING_MODE               0x00040000L
#define ATOM_PM_MISCINFO_THERMAL_DIODE_MODE              0x00080000L

#define ATOM_PM_MISCINFO_FRAME_MODULATION_MASK           0x00300000L  
#define ATOM_PM_MISCINFO_FRAME_MODULATION_SHIFT          20 

#define ATOM_PM_MISCINFO_DYN_CLK_3D_IDLE                 0x00400000L
#define ATOM_PM_MISCINFO_DYNAMIC_CLOCK_DIVIDER_BY_2      0x00800000L
#define ATOM_PM_MISCINFO_DYNAMIC_CLOCK_DIVIDER_BY_4      0x01000000L
#define ATOM_PM_MISCINFO_DYNAMIC_HDP_BLOCK_EN            0x02000000L  
#define ATOM_PM_MISCINFO_DYNAMIC_MC_HOST_BLOCK_EN        0x04000000L  
#define ATOM_PM_MISCINFO_3D_ACCELERATION_EN              0x08000000L  

#define ATOM_PM_MISCINFO_POWERPLAY_SETTINGS_GROUP_MASK   0x70000000L  
#define ATOM_PM_MISCINFO_POWERPLAY_SETTINGS_GROUP_SHIFT  28
#define ATOM_PM_MISCINFO_ENABLE_BACK_BIAS                0x80000000L

#define ATOM_PM_MISCINFO2_SYSTEM_AC_LITE_MODE            0x00000001L
#define ATOM_PM_MISCINFO2_MULTI_DISPLAY_SUPPORT          0x00000002L
#define ATOM_PM_MISCINFO2_DYNAMIC_BACK_BIAS_EN           0x00000004L
#define ATOM_PM_MISCINFO2_FS3D_OVERDRIVE_INFO            0x00000008L
#define ATOM_PM_MISCINFO2_FORCEDLOWPWR_MODE              0x00000010L
#define ATOM_PM_MISCINFO2_VDDCI_DYNAMIC_VOLTAGE_EN       0x00000020L
#define ATOM_PM_MISCINFO2_VIDEO_PLAYBACK_CAPABLE         0x00000040L  
                                                                      
#define ATOM_PM_MISCINFO2_NOT_VALID_ON_DC                0x00000080L
#define ATOM_PM_MISCINFO2_STUTTER_MODE_EN                0x00000100L
#define ATOM_PM_MISCINFO2_UVD_SUPPORT_MODE               0x00000200L 

typedef struct  _ATOM_POWERMODE_INFO
{
  ULONG     ulMiscInfo;                 
  ULONG     ulReserved1;                
  ULONG     ulReserved2;                
  USHORT    usEngineClock;
  USHORT    usMemoryClock;
  UCHAR     ucVoltageDropIndex;         
  UCHAR     ucSelectedPanel_RefreshRate;
  UCHAR     ucMinTemperature;
  UCHAR     ucMaxTemperature;
  UCHAR     ucNumPciELanes;             
}ATOM_POWERMODE_INFO;

typedef struct  _ATOM_POWERMODE_INFO_V2
{
  ULONG     ulMiscInfo;                 
  ULONG     ulMiscInfo2;                
  ULONG     ulEngineClock;                
  ULONG     ulMemoryClock;
  UCHAR     ucVoltageDropIndex;         
  UCHAR     ucSelectedPanel_RefreshRate;
  UCHAR     ucMinTemperature;
  UCHAR     ucMaxTemperature;
  UCHAR     ucNumPciELanes;             
}ATOM_POWERMODE_INFO_V2;

typedef struct  _ATOM_POWERMODE_INFO_V3
{
  ULONG     ulMiscInfo;                 
  ULONG     ulMiscInfo2;                
  ULONG     ulEngineClock;                
  ULONG     ulMemoryClock;
  UCHAR     ucVoltageDropIndex;         
  UCHAR     ucSelectedPanel_RefreshRate;
  UCHAR     ucMinTemperature;
  UCHAR     ucMaxTemperature;
  UCHAR     ucNumPciELanes;             
  UCHAR     ucVDDCI_VoltageDropIndex;   
}ATOM_POWERMODE_INFO_V3;


#define ATOM_MAX_NUMBEROF_POWER_BLOCK  8

#define ATOM_PP_OVERDRIVE_INTBITMAP_AUXWIN            0x01
#define ATOM_PP_OVERDRIVE_INTBITMAP_OVERDRIVE         0x02

#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_LM63      0x01
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_ADM1032   0x02
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_ADM1030   0x03
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_MUA6649   0x04
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_LM64      0x05
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_F75375    0x06
#define ATOM_PP_OVERDRIVE_THERMALCONTROLLER_ASC7512   0x07	


typedef struct  _ATOM_POWERPLAY_INFO
{
  ATOM_COMMON_TABLE_HEADER	sHeader; 
  UCHAR    ucOverdriveThermalController;
  UCHAR    ucOverdriveI2cLine;
  UCHAR    ucOverdriveIntBitmap;
  UCHAR    ucOverdriveControllerAddress;
  UCHAR    ucSizeOfPowerModeEntry;
  UCHAR    ucNumOfPowerModeEntries;
  ATOM_POWERMODE_INFO asPowerPlayInfo[ATOM_MAX_NUMBEROF_POWER_BLOCK];
}ATOM_POWERPLAY_INFO;

typedef struct  _ATOM_POWERPLAY_INFO_V2
{
  ATOM_COMMON_TABLE_HEADER	sHeader; 
  UCHAR    ucOverdriveThermalController;
  UCHAR    ucOverdriveI2cLine;
  UCHAR    ucOverdriveIntBitmap;
  UCHAR    ucOverdriveControllerAddress;
  UCHAR    ucSizeOfPowerModeEntry;
  UCHAR    ucNumOfPowerModeEntries;
  ATOM_POWERMODE_INFO_V2 asPowerPlayInfo[ATOM_MAX_NUMBEROF_POWER_BLOCK];
}ATOM_POWERPLAY_INFO_V2;
  
typedef struct  _ATOM_POWERPLAY_INFO_V3
{
  ATOM_COMMON_TABLE_HEADER	sHeader; 
  UCHAR    ucOverdriveThermalController;
  UCHAR    ucOverdriveI2cLine;
  UCHAR    ucOverdriveIntBitmap;
  UCHAR    ucOverdriveControllerAddress;
  UCHAR    ucSizeOfPowerModeEntry;
  UCHAR    ucNumOfPowerModeEntries;
  ATOM_POWERMODE_INFO_V3 asPowerPlayInfo[ATOM_MAX_NUMBEROF_POWER_BLOCK];
}ATOM_POWERPLAY_INFO_V3;

typedef struct _ATOM_PPLIB_THERMALCONTROLLER

{
    UCHAR ucType;           
    UCHAR ucI2cLine;        
    UCHAR ucI2cAddress;
    UCHAR ucFanParameters;  
    UCHAR ucFanMinRPM;      
    UCHAR ucFanMaxRPM;      
    UCHAR ucReserved;       
    UCHAR ucFlags;          
} ATOM_PPLIB_THERMALCONTROLLER;

#define ATOM_PP_FANPARAMETERS_TACHOMETER_PULSES_PER_REVOLUTION_MASK 0x0f
#define ATOM_PP_FANPARAMETERS_NOFAN                                 0x80    

#define ATOM_PP_THERMALCONTROLLER_NONE      0
#define ATOM_PP_THERMALCONTROLLER_LM63      1  
#define ATOM_PP_THERMALCONTROLLER_ADM1032   2  
#define ATOM_PP_THERMALCONTROLLER_ADM1030   3  
#define ATOM_PP_THERMALCONTROLLER_MUA6649   4  
#define ATOM_PP_THERMALCONTROLLER_LM64      5
#define ATOM_PP_THERMALCONTROLLER_F75375    6  
#define ATOM_PP_THERMALCONTROLLER_RV6xx     7
#define ATOM_PP_THERMALCONTROLLER_RV770     8
#define ATOM_PP_THERMALCONTROLLER_ADT7473   9
#define ATOM_PP_THERMALCONTROLLER_EXTERNAL_GPIO     11
#define ATOM_PP_THERMALCONTROLLER_EVERGREEN 12
#define ATOM_PP_THERMALCONTROLLER_EMC2103   13   
#define ATOM_PP_THERMALCONTROLLER_SUMO      14   
#define ATOM_PP_THERMALCONTROLLER_NISLANDS  15
#define ATOM_PP_THERMALCONTROLLER_SISLANDS  16
#define ATOM_PP_THERMALCONTROLLER_LM96163   17


#define ATOM_PP_THERMALCONTROLLER_ADT7473_WITH_INTERNAL   0x89    
#define ATOM_PP_THERMALCONTROLLER_EMC2103_WITH_INTERNAL   0x8D    

typedef struct _ATOM_PPLIB_STATE
{
    UCHAR ucNonClockStateIndex;
    UCHAR ucClockStateIndices[1]; 
} ATOM_PPLIB_STATE;


typedef struct _ATOM_PPLIB_FANTABLE
{
    UCHAR   ucFanTableFormat;                
    UCHAR   ucTHyst;                         
    USHORT  usTMin;                          
    USHORT  usTMed;                          
    USHORT  usTHigh;                         
    USHORT  usPWMMin;                        
    USHORT  usPWMMed;                        
    USHORT  usPWMHigh;                       
} ATOM_PPLIB_FANTABLE;

typedef struct _ATOM_PPLIB_FANTABLE2
{
    ATOM_PPLIB_FANTABLE basicTable;
    USHORT  usTMax;                          
} ATOM_PPLIB_FANTABLE2;

typedef struct _ATOM_PPLIB_EXTENDEDHEADER
{
    USHORT  usSize;
    ULONG   ulMaxEngineClock;   
    ULONG   ulMaxMemoryClock;   
    
    USHORT  usVCETableOffset; 
    USHORT  usUVDTableOffset;   
} ATOM_PPLIB_EXTENDEDHEADER;

#define ATOM_PP_PLATFORM_CAP_BACKBIAS 1
#define ATOM_PP_PLATFORM_CAP_POWERPLAY 2
#define ATOM_PP_PLATFORM_CAP_SBIOSPOWERSOURCE 4
#define ATOM_PP_PLATFORM_CAP_ASPM_L0s 8
#define ATOM_PP_PLATFORM_CAP_ASPM_L1 16
#define ATOM_PP_PLATFORM_CAP_HARDWAREDC 32
#define ATOM_PP_PLATFORM_CAP_GEMINIPRIMARY 64
#define ATOM_PP_PLATFORM_CAP_STEPVDDC 128
#define ATOM_PP_PLATFORM_CAP_VOLTAGECONTROL 256
#define ATOM_PP_PLATFORM_CAP_SIDEPORTCONTROL 512
#define ATOM_PP_PLATFORM_CAP_TURNOFFPLL_ASPML1 1024
#define ATOM_PP_PLATFORM_CAP_HTLINKCONTROL 2048
#define ATOM_PP_PLATFORM_CAP_MVDDCONTROL 4096
#define ATOM_PP_PLATFORM_CAP_GOTO_BOOT_ON_ALERT 0x2000              
#define ATOM_PP_PLATFORM_CAP_DONT_WAIT_FOR_VBLANK_ON_ALERT 0x4000   
#define ATOM_PP_PLATFORM_CAP_VDDCI_CONTROL 0x8000                   
#define ATOM_PP_PLATFORM_CAP_REGULATOR_HOT 0x00010000               
#define ATOM_PP_PLATFORM_CAP_BACO          0x00020000               


typedef struct _ATOM_PPLIB_POWERPLAYTABLE
{
      ATOM_COMMON_TABLE_HEADER sHeader;

      UCHAR ucDataRevision;

      UCHAR ucNumStates;
      UCHAR ucStateEntrySize;
      UCHAR ucClockInfoSize;
      UCHAR ucNonClockSize;

      
      USHORT usStateArrayOffset;

      
      
      USHORT usClockInfoArrayOffset;

      
      USHORT usNonClockInfoArrayOffset;

      USHORT usBackbiasTime;    
      USHORT usVoltageTime;     
      USHORT usTableSize;       

      ULONG ulPlatformCaps;            

      ATOM_PPLIB_THERMALCONTROLLER    sThermalController;

      USHORT usBootClockInfoOffset;
      USHORT usBootNonClockInfoOffset;

} ATOM_PPLIB_POWERPLAYTABLE;

typedef struct _ATOM_PPLIB_POWERPLAYTABLE2
{
    ATOM_PPLIB_POWERPLAYTABLE basicTable;
    UCHAR   ucNumCustomThermalPolicy;
    USHORT  usCustomThermalPolicyArrayOffset;
}ATOM_PPLIB_POWERPLAYTABLE2, *LPATOM_PPLIB_POWERPLAYTABLE2;

typedef struct _ATOM_PPLIB_POWERPLAYTABLE3
{
    ATOM_PPLIB_POWERPLAYTABLE2 basicTable2;
    USHORT                     usFormatID;                      
    USHORT                     usFanTableOffset;
    USHORT                     usExtendendedHeaderOffset;
} ATOM_PPLIB_POWERPLAYTABLE3, *LPATOM_PPLIB_POWERPLAYTABLE3;

typedef struct _ATOM_PPLIB_POWERPLAYTABLE4
{
    ATOM_PPLIB_POWERPLAYTABLE3 basicTable3;
    ULONG                      ulGoldenPPID;                    
    ULONG                      ulGoldenRevision;                
    USHORT                     usVddcDependencyOnSCLKOffset;
    USHORT                     usVddciDependencyOnMCLKOffset;
    USHORT                     usVddcDependencyOnMCLKOffset;
    USHORT                     usMaxClockVoltageOnDCOffset;
    USHORT                     usVddcPhaseShedLimitsTableOffset;    
    USHORT                     usReserved;  
} ATOM_PPLIB_POWERPLAYTABLE4, *LPATOM_PPLIB_POWERPLAYTABLE4;

typedef struct _ATOM_PPLIB_POWERPLAYTABLE5
{
    ATOM_PPLIB_POWERPLAYTABLE4 basicTable4;
    ULONG                      ulTDPLimit;
    ULONG                      ulNearTDPLimit;
    ULONG                      ulSQRampingThreshold;
    USHORT                     usCACLeakageTableOffset;         
    ULONG                      ulCACLeakage;                    
    USHORT                     usTDPODLimit;
    USHORT                     usLoadLineSlope;                 
} ATOM_PPLIB_POWERPLAYTABLE5, *LPATOM_PPLIB_POWERPLAYTABLE5;

#define ATOM_PPLIB_CLASSIFICATION_UI_MASK          0x0007
#define ATOM_PPLIB_CLASSIFICATION_UI_SHIFT         0
#define ATOM_PPLIB_CLASSIFICATION_UI_NONE          0
#define ATOM_PPLIB_CLASSIFICATION_UI_BATTERY       1
#define ATOM_PPLIB_CLASSIFICATION_UI_BALANCED      3
#define ATOM_PPLIB_CLASSIFICATION_UI_PERFORMANCE   5

#define ATOM_PPLIB_CLASSIFICATION_BOOT                   0x0008
#define ATOM_PPLIB_CLASSIFICATION_THERMAL                0x0010
#define ATOM_PPLIB_CLASSIFICATION_LIMITEDPOWERSOURCE     0x0020
#define ATOM_PPLIB_CLASSIFICATION_REST                   0x0040
#define ATOM_PPLIB_CLASSIFICATION_FORCED                 0x0080
#define ATOM_PPLIB_CLASSIFICATION_3DPERFORMANCE          0x0100
#define ATOM_PPLIB_CLASSIFICATION_OVERDRIVETEMPLATE      0x0200
#define ATOM_PPLIB_CLASSIFICATION_UVDSTATE               0x0400
#define ATOM_PPLIB_CLASSIFICATION_3DLOW                  0x0800
#define ATOM_PPLIB_CLASSIFICATION_ACPI                   0x1000
#define ATOM_PPLIB_CLASSIFICATION_HD2STATE               0x2000
#define ATOM_PPLIB_CLASSIFICATION_HDSTATE                0x4000
#define ATOM_PPLIB_CLASSIFICATION_SDSTATE                0x8000

#define ATOM_PPLIB_CLASSIFICATION2_LIMITEDPOWERSOURCE_2     0x0001
#define ATOM_PPLIB_CLASSIFICATION2_ULV                      0x0002
#define ATOM_PPLIB_CLASSIFICATION2_MVC                      0x0004   

#define ATOM_PPLIB_SINGLE_DISPLAY_ONLY           0x00000001
#define ATOM_PPLIB_SUPPORTS_VIDEO_PLAYBACK         0x00000002

#define ATOM_PPLIB_PCIE_LINK_SPEED_MASK            0x00000004
#define ATOM_PPLIB_PCIE_LINK_SPEED_SHIFT           2

#define ATOM_PPLIB_PCIE_LINK_WIDTH_MASK            0x000000F8
#define ATOM_PPLIB_PCIE_LINK_WIDTH_SHIFT           3

#define ATOM_PPLIB_LIMITED_REFRESHRATE_VALUE_MASK  0x00000F00
#define ATOM_PPLIB_LIMITED_REFRESHRATE_VALUE_SHIFT 8

#define ATOM_PPLIB_LIMITED_REFRESHRATE_UNLIMITED    0
#define ATOM_PPLIB_LIMITED_REFRESHRATE_50HZ         1

#define ATOM_PPLIB_SOFTWARE_DISABLE_LOADBALANCING        0x00001000
#define ATOM_PPLIB_SOFTWARE_ENABLE_SLEEP_FOR_TIMESTAMPS  0x00002000

#define ATOM_PPLIB_DISALLOW_ON_DC                       0x00004000

#define ATOM_PPLIB_ENABLE_VARIBRIGHT                     0x00008000

#define ATOM_PPLIB_SWSTATE_MEMORY_DLL_OFF               0x000010000

#define ATOM_PPLIB_M3ARB_MASK                       0x00060000
#define ATOM_PPLIB_M3ARB_SHIFT                      17

#define ATOM_PPLIB_ENABLE_DRR                       0x00080000

typedef struct _ATOM_PPLIB_THERMAL_STATE
{
    UCHAR   ucMinTemperature;
    UCHAR   ucMaxTemperature;
    UCHAR   ucThermalAction;
}ATOM_PPLIB_THERMAL_STATE, *LPATOM_PPLIB_THERMAL_STATE;

#define ATOM_PPLIB_NONCLOCKINFO_VER1      12
#define ATOM_PPLIB_NONCLOCKINFO_VER2      24
typedef struct _ATOM_PPLIB_NONCLOCK_INFO
{
      USHORT usClassification;
      UCHAR  ucMinTemperature;
      UCHAR  ucMaxTemperature;
      ULONG  ulCapsAndSettings;
      UCHAR  ucRequiredPower;
      USHORT usClassification2;
      ULONG  ulVCLK;
      ULONG  ulDCLK;
      UCHAR  ucUnused[5];
} ATOM_PPLIB_NONCLOCK_INFO;

typedef struct _ATOM_PPLIB_R600_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR ucEngineClockHigh;

      USHORT usMemoryClockLow;
      UCHAR ucMemoryClockHigh;

      USHORT usVDDC;
      USHORT usUnused1;
      USHORT usUnused2;

      ULONG ulFlags; 

} ATOM_PPLIB_R600_CLOCK_INFO;

#define ATOM_PPLIB_R600_FLAGS_PCIEGEN2          1
#define ATOM_PPLIB_R600_FLAGS_UVDSAFE           2
#define ATOM_PPLIB_R600_FLAGS_BACKBIASENABLE    4
#define ATOM_PPLIB_R600_FLAGS_MEMORY_ODT_OFF    8
#define ATOM_PPLIB_R600_FLAGS_MEMORY_DLL_OFF   16
#define ATOM_PPLIB_R600_FLAGS_LOWPOWER         32   

typedef struct _ATOM_PPLIB_EVERGREEN_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;

      USHORT usMemoryClockLow;
      UCHAR  ucMemoryClockHigh;

      USHORT usVDDC;
      USHORT usVDDCI;
      USHORT usUnused;

      ULONG ulFlags; 

} ATOM_PPLIB_EVERGREEN_CLOCK_INFO;

typedef struct _ATOM_PPLIB_SI_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;

      USHORT usMemoryClockLow;
      UCHAR  ucMemoryClockHigh;

      USHORT usVDDC;
      USHORT usVDDCI;
      UCHAR  ucPCIEGen;
      UCHAR  ucUnused1;

      ULONG ulFlags; 

} ATOM_PPLIB_SI_CLOCK_INFO;


typedef struct _ATOM_PPLIB_RS780_CLOCK_INFO

{
      USHORT usLowEngineClockLow;         
      UCHAR  ucLowEngineClockHigh;
      USHORT usHighEngineClockLow;        
      UCHAR  ucHighEngineClockHigh;
      USHORT usMemoryClockLow;            
      UCHAR  ucMemoryClockHigh;           
      UCHAR  ucPadding;                   
      USHORT usVDDC;                      
      UCHAR  ucMaxHTLinkWidth;            
      UCHAR  ucMinHTLinkWidth;            
      USHORT usHTLinkFreq;                
      ULONG  ulFlags; 
} ATOM_PPLIB_RS780_CLOCK_INFO;

#define ATOM_PPLIB_RS780_VOLTAGE_NONE       0 
#define ATOM_PPLIB_RS780_VOLTAGE_LOW        1 
#define ATOM_PPLIB_RS780_VOLTAGE_HIGH       2 
#define ATOM_PPLIB_RS780_VOLTAGE_VARIABLE   3 

#define ATOM_PPLIB_RS780_SPMCLK_NONE        0   
#define ATOM_PPLIB_RS780_SPMCLK_LOW         1
#define ATOM_PPLIB_RS780_SPMCLK_HIGH        2

#define ATOM_PPLIB_RS780_HTLINKFREQ_NONE       0 
#define ATOM_PPLIB_RS780_HTLINKFREQ_LOW        1 
#define ATOM_PPLIB_RS780_HTLINKFREQ_HIGH       2 

typedef struct _ATOM_PPLIB_SUMO_CLOCK_INFO{
      USHORT usEngineClockLow;  
      UCHAR  ucEngineClockHigh; 
      UCHAR  vddcIndex;         
      USHORT tdpLimit;
      
      USHORT rsv1;
      
      ULONG rsv2[2];
}ATOM_PPLIB_SUMO_CLOCK_INFO;



typedef struct _ATOM_PPLIB_STATE_V2
{
      
      
      UCHAR ucNumDPMLevels;
      
      
      UCHAR nonClockInfoIndex;
      UCHAR clockInfoIndex[1];
} ATOM_PPLIB_STATE_V2;

typedef struct _StateArray{
    
    UCHAR ucNumEntries;
    
    ATOM_PPLIB_STATE_V2 states[1];
}StateArray;


typedef struct _ClockInfoArray{
    
    UCHAR ucNumEntries;
    
    
    UCHAR ucEntrySize;
    
    UCHAR clockInfo[1];
}ClockInfoArray;

typedef struct _NonClockInfoArray{

    
    UCHAR ucNumEntries;
    
    UCHAR ucEntrySize;
    
    ATOM_PPLIB_NONCLOCK_INFO nonClockInfo[1];
}NonClockInfoArray;

typedef struct _ATOM_PPLIB_Clock_Voltage_Dependency_Record
{
    USHORT usClockLow;
    UCHAR  ucClockHigh;
    USHORT usVoltage;
}ATOM_PPLIB_Clock_Voltage_Dependency_Record;

typedef struct _ATOM_PPLIB_Clock_Voltage_Dependency_Table
{
    UCHAR ucNumEntries;                                                
    ATOM_PPLIB_Clock_Voltage_Dependency_Record entries[1];             
}ATOM_PPLIB_Clock_Voltage_Dependency_Table;

typedef struct _ATOM_PPLIB_Clock_Voltage_Limit_Record
{
    USHORT usSclkLow;
    UCHAR  ucSclkHigh;
    USHORT usMclkLow;
    UCHAR  ucMclkHigh;
    USHORT usVddc;
    USHORT usVddci;
}ATOM_PPLIB_Clock_Voltage_Limit_Record;

typedef struct _ATOM_PPLIB_Clock_Voltage_Limit_Table
{
    UCHAR ucNumEntries;                                                
    ATOM_PPLIB_Clock_Voltage_Limit_Record entries[1];                  
}ATOM_PPLIB_Clock_Voltage_Limit_Table;

typedef struct _ATOM_PPLIB_CAC_Leakage_Record
{
    USHORT usVddc;  
    ULONG  ulLeakageValue;
}ATOM_PPLIB_CAC_Leakage_Record;

typedef struct _ATOM_PPLIB_CAC_Leakage_Table
{
    UCHAR ucNumEntries;                                                 
    ATOM_PPLIB_CAC_Leakage_Record entries[1];                           
}ATOM_PPLIB_CAC_Leakage_Table;

typedef struct _ATOM_PPLIB_PhaseSheddingLimits_Record
{
    USHORT usVoltage;
    USHORT usSclkLow;
    UCHAR  ucSclkHigh;
    USHORT usMclkLow;
    UCHAR  ucMclkHigh;
}ATOM_PPLIB_PhaseSheddingLimits_Record;

typedef struct _ATOM_PPLIB_PhaseSheddingLimits_Table
{
    UCHAR ucNumEntries;                                                 
    ATOM_PPLIB_PhaseSheddingLimits_Record entries[1];                   
}ATOM_PPLIB_PhaseSheddingLimits_Table;

typedef struct _VCEClockInfo{
    USHORT usEVClkLow;
    UCHAR  ucEVClkHigh;
    USHORT usECClkLow;
    UCHAR  ucECClkHigh;
}VCEClockInfo;

typedef struct _VCEClockInfoArray{
    UCHAR ucNumEntries;
    VCEClockInfo entries[1];
}VCEClockInfoArray;

typedef struct _ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record
{
    USHORT usVoltage;
    UCHAR  ucVCEClockInfoIndex;
}ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record;

typedef struct _ATOM_PPLIB_VCE_Clock_Voltage_Limit_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_VCE_Clock_Voltage_Limit_Table;

typedef struct _ATOM_PPLIB_VCE_State_Record
{
    UCHAR  ucVCEClockInfoIndex;
    UCHAR  ucClockInfoIndex; 
}ATOM_PPLIB_VCE_State_Record;

typedef struct _ATOM_PPLIB_VCE_State_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_VCE_State_Record entries[1];
}ATOM_PPLIB_VCE_State_Table;


typedef struct _ATOM_PPLIB_VCE_Table
{
      UCHAR revid;
}ATOM_PPLIB_VCE_Table;


typedef struct _UVDClockInfo{
    USHORT usVClkLow;
    UCHAR  ucVClkHigh;
    USHORT usDClkLow;
    UCHAR  ucDClkHigh;
}UVDClockInfo;

typedef struct _UVDClockInfoArray{
    UCHAR ucNumEntries;
    UVDClockInfo entries[1];
}UVDClockInfoArray;

typedef struct _ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record
{
    USHORT usVoltage;
    UCHAR  ucUVDClockInfoIndex;
}ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record;

typedef struct _ATOM_PPLIB_UVD_Clock_Voltage_Limit_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_UVD_Clock_Voltage_Limit_Table;

typedef struct _ATOM_PPLIB_UVD_State_Record
{
    UCHAR  ucUVDClockInfoIndex;
    UCHAR  ucClockInfoIndex; 
}ATOM_PPLIB_UVD_State_Record;

typedef struct _ATOM_PPLIB_UVD_State_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_UVD_State_Record entries[1];
}ATOM_PPLIB_UVD_State_Table;


typedef struct _ATOM_PPLIB_UVD_Table
{
      UCHAR revid;
}ATOM_PPLIB_UVD_Table;



#define ATOM_MASTER_DATA_TABLE_REVISION   0x01
#define Object_Info												Object_Header			
#define	AdjustARB_SEQ											MC_InitParameter
#define	VRAM_GPIO_DetectionInfo						VoltageObjectInfo
#define	ASIC_VDDCI_Info                   ASIC_ProfilingInfo														
#define ASIC_MVDDQ_Info										MemoryTrainingInfo
#define SS_Info                           PPLL_SS_Info                      
#define ASIC_MVDDC_Info                   ASIC_InternalSS_Info
#define DispDevicePriorityInfo						SaveRestoreInfo
#define DispOutInfo												TV_VideoMode


#define ATOM_ENCODER_OBJECT_TABLE         ATOM_OBJECT_TABLE
#define ATOM_CONNECTOR_OBJECT_TABLE       ATOM_OBJECT_TABLE

#define DFP2I_OUTPUT_CONTROL_PARAMETERS    CRT1_OUTPUT_CONTROL_PARAMETERS
#define DFP2I_OUTPUT_CONTROL_PS_ALLOCATION DFP2I_OUTPUT_CONTROL_PARAMETERS

#define DFP1X_OUTPUT_CONTROL_PARAMETERS    CRT1_OUTPUT_CONTROL_PARAMETERS
#define DFP1X_OUTPUT_CONTROL_PS_ALLOCATION DFP1X_OUTPUT_CONTROL_PARAMETERS

#define DFP1I_OUTPUT_CONTROL_PARAMETERS    DFP1_OUTPUT_CONTROL_PARAMETERS
#define DFP1I_OUTPUT_CONTROL_PS_ALLOCATION DFP1_OUTPUT_CONTROL_PS_ALLOCATION

#define ATOM_DEVICE_DFP1I_SUPPORT          ATOM_DEVICE_DFP1_SUPPORT
#define ATOM_DEVICE_DFP1X_SUPPORT          ATOM_DEVICE_DFP2_SUPPORT

#define ATOM_DEVICE_DFP1I_INDEX            ATOM_DEVICE_DFP1_INDEX
#define ATOM_DEVICE_DFP1X_INDEX            ATOM_DEVICE_DFP2_INDEX
 
#define ATOM_DEVICE_DFP2I_INDEX            0x00000009
#define ATOM_DEVICE_DFP2I_SUPPORT          (0x1L << ATOM_DEVICE_DFP2I_INDEX)

#define ATOM_S0_DFP1I                      ATOM_S0_DFP1
#define ATOM_S0_DFP1X                      ATOM_S0_DFP2

#define ATOM_S0_DFP2I                      0x00200000L
#define ATOM_S0_DFP2Ib2                    0x20

#define ATOM_S2_DFP1I_DPMS_STATE           ATOM_S2_DFP1_DPMS_STATE
#define ATOM_S2_DFP1X_DPMS_STATE           ATOM_S2_DFP2_DPMS_STATE

#define ATOM_S2_DFP2I_DPMS_STATE           0x02000000L
#define ATOM_S2_DFP2I_DPMS_STATEb3         0x02

#define ATOM_S3_DFP2I_ACTIVEb1             0x02

#define ATOM_S3_DFP1I_ACTIVE               ATOM_S3_DFP1_ACTIVE 
#define ATOM_S3_DFP1X_ACTIVE               ATOM_S3_DFP2_ACTIVE

#define ATOM_S3_DFP2I_ACTIVE               0x00000200L

#define ATOM_S3_DFP1I_CRTC_ACTIVE          ATOM_S3_DFP1_CRTC_ACTIVE
#define ATOM_S3_DFP1X_CRTC_ACTIVE          ATOM_S3_DFP2_CRTC_ACTIVE
#define ATOM_S3_DFP2I_CRTC_ACTIVE          0x02000000L

#define ATOM_S3_DFP2I_CRTC_ACTIVEb3        0x02
#define ATOM_S5_DOS_REQ_DFP2Ib1            0x02

#define ATOM_S5_DOS_REQ_DFP2I              0x0200
#define ATOM_S6_ACC_REQ_DFP1I              ATOM_S6_ACC_REQ_DFP1
#define ATOM_S6_ACC_REQ_DFP1X              ATOM_S6_ACC_REQ_DFP2

#define ATOM_S6_ACC_REQ_DFP2Ib3            0x02
#define ATOM_S6_ACC_REQ_DFP2I              0x02000000L

#define TMDS1XEncoderControl               DVOEncoderControl           
#define DFP1XOutputControl                 DVOOutputControl

#define ExternalDFPOutputControl           DFP1XOutputControl
#define EnableExternalTMDS_Encoder         TMDS1XEncoderControl

#define DFP1IOutputControl                 TMDSAOutputControl
#define DFP2IOutputControl                 LVTMAOutputControl      

#define DAC1_ENCODER_CONTROL_PARAMETERS    DAC_ENCODER_CONTROL_PARAMETERS
#define DAC1_ENCODER_CONTROL_PS_ALLOCATION DAC_ENCODER_CONTROL_PS_ALLOCATION

#define DAC2_ENCODER_CONTROL_PARAMETERS    DAC_ENCODER_CONTROL_PARAMETERS
#define DAC2_ENCODER_CONTROL_PS_ALLOCATION DAC_ENCODER_CONTROL_PS_ALLOCATION

#define ucDac1Standard  ucDacStandard
#define ucDac2Standard  ucDacStandard  

#define TMDS1EncoderControl TMDSAEncoderControl
#define TMDS2EncoderControl LVTMAEncoderControl

#define DFP1OutputControl   TMDSAOutputControl
#define DFP2OutputControl   LVTMAOutputControl
#define CRT1OutputControl   DAC1OutputControl
#define CRT2OutputControl   DAC2OutputControl

#define EnableLVDS_SS   EnableSpreadSpectrumOnPPLL
#define ENABLE_LVDS_SS_PARAMETERS_V3  ENABLE_SPREAD_SPECTRUM_ON_PPLL  


#define ATOM_S6_ACC_REQ_TV2             0x00400000L
#define ATOM_DEVICE_TV2_INDEX           0x00000006
#define ATOM_DEVICE_TV2_SUPPORT         (0x1L << ATOM_DEVICE_TV2_INDEX)
#define ATOM_S0_TV2                     0x00100000L
#define ATOM_S3_TV2_ACTIVE              ATOM_S3_DFP6_ACTIVE
#define ATOM_S3_TV2_CRTC_ACTIVE         ATOM_S3_DFP6_CRTC_ACTIVE

#define ATOM_S2_CRT1_DPMS_STATE         0x00010000L
#define ATOM_S2_LCD1_DPMS_STATE	        0x00020000L
#define ATOM_S2_TV1_DPMS_STATE          0x00040000L
#define ATOM_S2_DFP1_DPMS_STATE         0x00080000L
#define ATOM_S2_CRT2_DPMS_STATE         0x00100000L
#define ATOM_S2_LCD2_DPMS_STATE         0x00200000L
#define ATOM_S2_TV2_DPMS_STATE          0x00400000L
#define ATOM_S2_DFP2_DPMS_STATE         0x00800000L
#define ATOM_S2_CV_DPMS_STATE           0x01000000L
#define ATOM_S2_DFP3_DPMS_STATE					0x02000000L
#define ATOM_S2_DFP4_DPMS_STATE					0x04000000L
#define ATOM_S2_DFP5_DPMS_STATE					0x08000000L

#define ATOM_S2_CRT1_DPMS_STATEb2       0x01
#define ATOM_S2_LCD1_DPMS_STATEb2       0x02
#define ATOM_S2_TV1_DPMS_STATEb2        0x04
#define ATOM_S2_DFP1_DPMS_STATEb2       0x08
#define ATOM_S2_CRT2_DPMS_STATEb2       0x10
#define ATOM_S2_LCD2_DPMS_STATEb2       0x20
#define ATOM_S2_TV2_DPMS_STATEb2        0x40
#define ATOM_S2_DFP2_DPMS_STATEb2       0x80
#define ATOM_S2_CV_DPMS_STATEb3         0x01
#define ATOM_S2_DFP3_DPMS_STATEb3				0x02
#define ATOM_S2_DFP4_DPMS_STATEb3				0x04
#define ATOM_S2_DFP5_DPMS_STATEb3				0x08

#define ATOM_S3_ASIC_GUI_ENGINE_HUNGb3	0x20
#define ATOM_S3_ALLOW_FAST_PWR_SWITCHb3 0x40
#define ATOM_S3_RQST_GPU_USE_MIN_PWRb3  0x80


#pragma pack() 

#pragma pack(1)

typedef struct {
  ULONG Signature;
  ULONG TableLength;      
  UCHAR Revision;
  UCHAR Checksum;
  UCHAR OemId[6];
  UCHAR OemTableId[8];    
  ULONG OemRevision;
  ULONG CreatorId;
  ULONG CreatorRevision;
} AMD_ACPI_DESCRIPTION_HEADER;
typedef struct {
  AMD_ACPI_DESCRIPTION_HEADER SHeader;
  UCHAR TableUUID[16];    
  ULONG VBIOSImageOffset; 
  ULONG Lib1ImageOffset;  
  ULONG Reserved[4];      
}UEFI_ACPI_VFCT;

typedef struct {
  ULONG  PCIBus;          
  ULONG  PCIDevice;       
  ULONG  PCIFunction;     
  USHORT VendorID;        
  USHORT DeviceID;        
  USHORT SSVID;           
  USHORT SSID;            
  ULONG  Revision;        
  ULONG  ImageLength;     
}VFCT_IMAGE_HEADER;


typedef struct {
  VFCT_IMAGE_HEADER	VbiosHeader;
  UCHAR	VbiosContent[1];
}GOP_VBIOS_CONTENT;

typedef struct {
  VFCT_IMAGE_HEADER	Lib1Header;
  UCHAR	Lib1Content[1];
}GOP_LIB1_CONTENT;

#pragma pack()


#endif 
