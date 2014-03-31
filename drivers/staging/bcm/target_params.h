#ifndef TARGET_PARAMS_H
#define TARGET_PARAMS_H

typedef struct _TARGET_PARAMS
{
      B_UINT32 m_u32CfgVersion;

      
      B_UINT32 m_u32CenterFrequency;
      B_UINT32 m_u32BandAScan;
      B_UINT32 m_u32BandBScan;
      B_UINT32 m_u32BandCScan;


      
      B_UINT32 m_u32ErtpsOptions;

      B_UINT32 m_u32PHSEnable;


      
      B_UINT32 m_u32HoEnable;

      B_UINT32 m_u32HoReserved1;
      B_UINT32 m_u32HoReserved2;
      

      B_UINT32 m_u32MimoEnable;

      B_UINT32 m_u32SecurityEnable;

      B_UINT32 m_u32PowerSavingModesEnable; 
      B_UINT32 m_u32PowerSavingModeOptions;

      B_UINT32 m_u32ArqEnable;

      
      B_UINT32 m_u32HarqEnable;
       
       B_UINT32  m_u32EEPROMFlag;
       
       
      B_UINT32   m_u32Customize;
      B_UINT32   m_u32ConfigBW;  
      B_UINT32   m_u32ShutDownInitThresholdTimer;

      B_UINT32  m_u32RadioParameter;
      B_UINT32  m_u32PhyParameter1;
      B_UINT32  m_u32PhyParameter2;
      B_UINT32  m_u32PhyParameter3;

      B_UINT32	  m_u32TestOptions; 

	B_UINT32 m_u32MaxMACDataperDLFrame;
	B_UINT32 m_u32MaxMACDataperULFrame;

	B_UINT32 m_u32Corr2MacFlags;

    
	B_UINT32 HostDrvrConfig1;
    B_UINT32 HostDrvrConfig2;
    B_UINT32 HostDrvrConfig3;
    B_UINT32 HostDrvrConfig4;
    B_UINT32 HostDrvrConfig5;
    B_UINT32 HostDrvrConfig6;
    B_UINT32 m_u32SegmentedPUSCenable;

	

    
    
    
	B_UINT32 m_u32BandAMCEnable;

} stTargetParams,TARGET_PARAMS,*PTARGET_PARAMS, STARGETPARAMS, *PSTARGETPARAMS;

#endif
