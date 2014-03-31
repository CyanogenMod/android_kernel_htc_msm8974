#include "drxk_map.h"

#define DRXK_VERSION_MAJOR 0
#define DRXK_VERSION_MINOR 9
#define DRXK_VERSION_PATCH 4300

#define HI_I2C_DELAY        42
#define HI_I2C_BRIDGE_DELAY 350
#define DRXK_MAX_RETRIES    100

#define DRIVER_4400 1

#define DRXX_JTAGID   0x039210D9
#define DRXX_J_JTAGID 0x239310D9
#define DRXX_K_JTAGID 0x039210D9

#define DRX_UNKNOWN     254
#define DRX_AUTO        255

#define DRX_SCU_READY   0
#define DRXK_MAX_WAITTIME (200)
#define SCU_RESULT_OK      0
#define SCU_RESULT_SIZE   -4
#define SCU_RESULT_INVPAR -3
#define SCU_RESULT_UNKSTD -2
#define SCU_RESULT_UNKCMD -1

#ifndef DRXK_OFDM_TR_SHUTDOWN_TIMEOUT
#define DRXK_OFDM_TR_SHUTDOWN_TIMEOUT (200)
#endif

#define DRXK_8VSB_MPEG_BIT_RATE     19392658UL  
#define DRXK_DVBT_MPEG_BIT_RATE     32000000UL  
#define DRXK_QAM16_MPEG_BIT_RATE    27000000UL  
#define DRXK_QAM32_MPEG_BIT_RATE    33000000UL  
#define DRXK_QAM64_MPEG_BIT_RATE    40000000UL  
#define DRXK_QAM128_MPEG_BIT_RATE   46000000UL  
#define DRXK_QAM256_MPEG_BIT_RATE   52000000UL  
#define DRXK_MAX_MPEG_BIT_RATE      52000000UL  

#define   IQM_CF_OUT_ENA_OFDM__M                                            0x4
#define     IQM_FS_ADJ_SEL_B_QAM                                            0x1
#define     IQM_FS_ADJ_SEL_B_OFF                                            0x0
#define     IQM_FS_ADJ_SEL_B_VSB                                            0x2
#define     IQM_RC_ADJ_SEL_B_OFF                                            0x0
#define     IQM_RC_ADJ_SEL_B_QAM                                            0x1
#define     IQM_RC_ADJ_SEL_B_VSB                                            0x2

enum OperationMode {
	OM_NONE,
	OM_QAM_ITU_A,
	OM_QAM_ITU_B,
	OM_QAM_ITU_C,
	OM_DVBT
};

enum DRXPowerMode {
	DRX_POWER_UP = 0,
	DRX_POWER_MODE_1,
	DRX_POWER_MODE_2,
	DRX_POWER_MODE_3,
	DRX_POWER_MODE_4,
	DRX_POWER_MODE_5,
	DRX_POWER_MODE_6,
	DRX_POWER_MODE_7,
	DRX_POWER_MODE_8,

	DRX_POWER_MODE_9,
	DRX_POWER_MODE_10,
	DRX_POWER_MODE_11,
	DRX_POWER_MODE_12,
	DRX_POWER_MODE_13,
	DRX_POWER_MODE_14,
	DRX_POWER_MODE_15,
	DRX_POWER_MODE_16,
	DRX_POWER_DOWN = 255
};


#ifndef DRXK_POWER_DOWN_OFDM
#define DRXK_POWER_DOWN_OFDM        DRX_POWER_MODE_1
#endif

#ifndef DRXK_POWER_DOWN_CORE
#define DRXK_POWER_DOWN_CORE        DRX_POWER_MODE_9
#endif

#ifndef DRXK_POWER_DOWN_PLL
#define DRXK_POWER_DOWN_PLL         DRX_POWER_MODE_10
#endif


enum AGC_CTRL_MODE { DRXK_AGC_CTRL_AUTO = 0, DRXK_AGC_CTRL_USER, DRXK_AGC_CTRL_OFF };
enum EDrxkState { DRXK_UNINITIALIZED = 0, DRXK_STOPPED, DRXK_DTV_STARTED, DRXK_ATV_STARTED, DRXK_POWERED_DOWN };
enum EDrxkCoefArrayIndex {
	DRXK_COEF_IDX_MN = 0,
	DRXK_COEF_IDX_FM    ,
	DRXK_COEF_IDX_L     ,
	DRXK_COEF_IDX_LP    ,
	DRXK_COEF_IDX_BG    ,
	DRXK_COEF_IDX_DK    ,
	DRXK_COEF_IDX_I     ,
	DRXK_COEF_IDX_MAX
};
enum EDrxkSifAttenuation {
	DRXK_SIF_ATTENUATION_0DB,
	DRXK_SIF_ATTENUATION_3DB,
	DRXK_SIF_ATTENUATION_6DB,
	DRXK_SIF_ATTENUATION_9DB
};
enum EDrxkConstellation {
	DRX_CONSTELLATION_BPSK = 0,
	DRX_CONSTELLATION_QPSK,
	DRX_CONSTELLATION_PSK8,
	DRX_CONSTELLATION_QAM16,
	DRX_CONSTELLATION_QAM32,
	DRX_CONSTELLATION_QAM64,
	DRX_CONSTELLATION_QAM128,
	DRX_CONSTELLATION_QAM256,
	DRX_CONSTELLATION_QAM512,
	DRX_CONSTELLATION_QAM1024,
	DRX_CONSTELLATION_UNKNOWN = DRX_UNKNOWN,
	DRX_CONSTELLATION_AUTO    = DRX_AUTO
};
enum EDrxkInterleaveMode {
	DRXK_QAM_I12_J17    = 16,
	DRXK_QAM_I_UNKNOWN  = DRX_UNKNOWN
};
enum {
	DRXK_SPIN_A1 = 0,
	DRXK_SPIN_A2,
	DRXK_SPIN_A3,
	DRXK_SPIN_UNKNOWN
};

enum DRXKCfgDvbtSqiSpeed {
	DRXK_DVBT_SQI_SPEED_FAST = 0,
	DRXK_DVBT_SQI_SPEED_MEDIUM,
	DRXK_DVBT_SQI_SPEED_SLOW,
	DRXK_DVBT_SQI_SPEED_UNKNOWN = DRX_UNKNOWN
} ;

enum DRXFftmode_t {
	DRX_FFTMODE_2K = 0,
	DRX_FFTMODE_4K,
	DRX_FFTMODE_8K,
	DRX_FFTMODE_UNKNOWN = DRX_UNKNOWN,
	DRX_FFTMODE_AUTO    = DRX_AUTO
};

enum DRXMPEGStrWidth_t {
	DRX_MPEG_STR_WIDTH_1,
	DRX_MPEG_STR_WIDTH_8
};

enum DRXQamLockRange_t {
	DRX_QAM_LOCKRANGE_NORMAL,
	DRX_QAM_LOCKRANGE_EXTENDED
};

struct DRXKCfgDvbtEchoThres_t {
	u16             threshold;
	enum DRXFftmode_t      fftMode;
} ;

struct SCfgAgc {
	enum AGC_CTRL_MODE     ctrlMode;        
	u16            outputLevel;     
	u16            minOutputLevel;  
	u16            maxOutputLevel;  
	u16            speed;           
	u16            top;             
	u16            cutOffCurrent;   
	u16            IngainTgtMax;
	u16            FastClipCtrlDelay;
};

struct SCfgPreSaw {
	u16        reference; 
	bool          usePreSaw; 
};

struct DRXKOfdmScCmd_t {
	u16 cmd;        
	u16 subcmd;     
	u16 param0;     
	u16 param1;     
	u16 param2;     
	u16 param3;     
	u16 param4;     
};

struct drxk_state {
	struct dvb_frontend frontend;
	struct dtv_frontend_properties props;
	struct device *dev;

	struct i2c_adapter *i2c;
	u8     demod_address;
	void  *priv;

	struct mutex mutex;

	u32    m_Instance;           

	int    m_ChunkSize;
	u8 Chunk[256];

	bool   m_hasLNA;
	bool   m_hasDVBT;
	bool   m_hasDVBC;
	bool   m_hasAudio;
	bool   m_hasATV;
	bool   m_hasOOB;
	bool   m_hasSAWSW;         
	bool   m_hasGPIO1;         
	bool   m_hasGPIO2;         
	bool   m_hasIRQN;          
	u16    m_oscClockFreq;
	u16    m_HICfgTimingDiv;
	u16    m_HICfgBridgeDelay;
	u16    m_HICfgWakeUpKey;
	u16    m_HICfgTimeout;
	u16    m_HICfgCtrl;
	s32    m_sysClockFreq;      

	enum EDrxkState    m_DrxkState;      
	enum OperationMode m_OperationMode;  
	struct SCfgAgc     m_vsbRfAgcCfg;    
	struct SCfgAgc     m_vsbIfAgcCfg;    
	u16                m_vsbPgaCfg;      
	struct SCfgPreSaw  m_vsbPreSawCfg;   
	s32    m_Quality83percent;  
	s32    m_Quality93percent;  
	bool   m_smartAntInverted;
	bool   m_bDebugEnableBridge;
	bool   m_bPDownOpenBridge;  
	bool   m_bPowerDown;        

	u32    m_IqmFsRateOfs;      /**< frequency shift as written to DRXK register (28bit fixpoint) */

	bool   m_enableMPEGOutput;  
	bool   m_insertRSByte;      
	bool   m_enableParallel;    
	bool   m_invertDATA;        
	bool   m_invertERR;         
	bool   m_invertSTR;         
	bool   m_invertVAL;         
	bool   m_invertCLK;         
	bool   m_DVBCStaticCLK;
	bool   m_DVBTStaticCLK;     
	u32    m_DVBTBitrate;
	u32    m_DVBCBitrate;

	u8     m_TSDataStrength;
	u8     m_TSClockkStrength;

	bool   m_itut_annex_c;      

	enum DRXMPEGStrWidth_t  m_widthSTR;    
	u32    m_mpegTsStaticBitrate;          

	     
	s32    m_MpegLockTimeOut;      
	s32    m_DemodLockTimeOut;     

	bool   m_disableTEIhandling;

	bool   m_RfAgcPol;
	bool   m_IfAgcPol;

	struct SCfgAgc    m_atvRfAgcCfg;  
	struct SCfgAgc    m_atvIfAgcCfg;  
	struct SCfgPreSaw m_atvPreSawCfg; 
	bool              m_phaseCorrectionBypass;
	s16               m_atvTopVidPeak;
	u16               m_atvTopNoiseTh;
	enum EDrxkSifAttenuation m_sifAttenuation;
	bool              m_enableCVBSOutput;
	bool              m_enableSIFOutput;
	bool              m_bMirrorFreqSpect;
	enum EDrxkConstellation  m_Constellation; 
	u32               m_CurrSymbolRate;       
	struct SCfgAgc    m_qamRfAgcCfg;          
	struct SCfgAgc    m_qamIfAgcCfg;          
	u16               m_qamPgaCfg;            
	struct SCfgPreSaw m_qamPreSawCfg;         
	enum EDrxkInterleaveMode m_qamInterleaveMode; 
	u16               m_fecRsPlen;
	u16               m_fecRsPrescale;

	enum DRXKCfgDvbtSqiSpeed m_sqiSpeed;

	u16               m_GPIO;
	u16               m_GPIOCfg;

	struct SCfgAgc    m_dvbtRfAgcCfg;     
	struct SCfgAgc    m_dvbtIfAgcCfg;     
	struct SCfgPreSaw m_dvbtPreSawCfg;    

	u16               m_agcFastClipCtrlDelay;
	bool              m_adcCompPassed;
	u16               m_adcCompCoef[64];
	u16               m_adcState;

	u8               *m_microcode;
	int               m_microcode_length;
	bool              m_DRXK_A1_PATCH_CODE;
	bool              m_DRXK_A1_ROM_CODE;
	bool              m_DRXK_A2_ROM_CODE;
	bool              m_DRXK_A3_ROM_CODE;
	bool              m_DRXK_A2_PATCH_CODE;
	bool              m_DRXK_A3_PATCH_CODE;

	bool              m_rfmirror;
	u8                m_deviceSpin;
	u32               m_iqmRcRate;

	enum DRXPowerMode m_currentPowerMode;


	u16	UIO_mask;	

	bool	enable_merr_cfg;
	bool	single_master;
	bool	no_i2c_bridge;
	bool	antenna_dvbt;
	u16	antenna_gpio;

	const char *microcode_name;
};

#define NEVER_LOCK 0
#define NOT_LOCKED 1
#define DEMOD_LOCK 2
#define FEC_LOCK   3
#define MPEG_LOCK  4

