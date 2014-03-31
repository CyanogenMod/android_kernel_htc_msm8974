#ifndef SMIL_INCD
#define SMIL_INCD

#define K_BYTE              1024   
#define SECTSIZE            512    
#define REDTSIZE            16     

#define DUMMY_DATA          0xFF   

#define MAX_ZONENUM         128    
#define MAX_BLOCKNUM        0x0400 
#define MAX_SECTNUM         0x20   
#define MAX_LOGBLOCK        1000   

#define CIS_SEARCH_SECT     0x08   

#define NO_ASSIGN           0xFFFF 

#define COMPLETED           0      
#define REQ_ERASE           1      
#define REQ_FAIL            2      

#define RDERR_REASSIGN      1      
#define L2P_ERR_ERASE       1      

#define HW_ECC_SUPPORTED    1	   

#define WRDATA        0x80
#define READ_REDT     0x50
#define RDSTATUS      0x70

#define READ1         0x00 
#define READ2         0x01 
#define READ3         0x50 
#define RST_CHIP      0xFF
#define ERASE1        0x60
#define ERASE2        0xD0
#define READ_ID_1     0x90
#define READ_ID_2     0x91
#define READ_ID_3     0x9A

#define SM_CMD_RESET                0x00    
#define SM_CMD_READ_ID_1            0x10    
#define SM_CMD_READ_ID_2            0x20    
#define SM_CMD_READ_STAT            0x30    
#define SM_CMD_RDMULTPL_STAT        0x40    
#define SM_CMD_READ_1               0x50    
#define SM_CMD_READ_2               0x60    
#define SM_CMD_READ_3               0x70    
#define SM_CMD_PAGPRGM_TRUE         0x80    
#define SM_CMD_PAGPRGM_DUMY         0x90    
#define SM_CMD_PAGPRGM_MBLK         0xA0    
#define SM_CMD_BLKERASE             0xB0    
#define SM_CMD_BLKERASE_MULTPL      0xC0    

#define SM_CRADDTCT_DEBNCETIMER_EN  0x02
#define SM_CMD_START_BIT            0x01

#define SM_WaitCmdDone { while (!SM_CmdDone); }
#define SM_WaitDmaDone { while (!SM_DmaDone); }

#define WR_FAIL       0x01      
#define SUSPENDED     0x20      
#define READY         0x40      
#define WR_PRTCT      0x80      

#define BUSY_PROG 200 
#define BUSY_ERASE 4000 


#define BUSY_READ 200 


#define BUSY_RESET 600 

#define TIME_PON      3000      
#define TIME_CDCHK    200       
#define TIME_WPCHK    50        
#define TIME_5VCHK    10        

#define REDT_DATA     0x04
#define REDT_BLOCK    0x05
#define REDT_ADDR1H   0x06
#define REDT_ADDR1L   0x07
#define REDT_ADDR2H   0x0B
#define REDT_ADDR2L   0x0C
#define REDT_ECC10    0x0D
#define REDT_ECC11    0x0E
#define REDT_ECC12    0x0F
#define REDT_ECC20    0x08
#define REDT_ECC21    0x09
#define REDT_ECC22    0x0A

#define NOWP          0x00 
#define WP            0x80 
#define MASK          0x00 
#define FLASH         0x20 
#define AD3CYC        0x00 
#define AD4CYC        0x10 
#define BS16          0x00 
#define BS32          0x04 
#define PS256         0x00 
#define PS512         0x01 
#define MWP           0x80 
#define MFLASH        0x60 
#define MADC          0x10 
#define MBS           0x0C 
#define MPS           0x03 

#define NOSSFDC       0x00 
#define SSFDC1MB      0x01 
#define SSFDC2MB      0x02 
#define SSFDC4MB      0x03 
#define SSFDC8MB      0x04 
#define SSFDC16MB     0x05 
#define SSFDC32MB     0x06 
#define SSFDC64MB     0x07 
#define SSFDC128MB    0x08 
#define SSFDC256MB    0x09
#define SSFDC512MB    0x0A
#define SSFDC1GB      0x0B
#define SSFDC2GB      0x0C

struct SSFDCTYPE {
	BYTE Model;
	BYTE Attribute;
	BYTE MaxZones;
	BYTE MaxSectors;
	WORD MaxBlocks;
	WORD MaxLogBlocks;
};

typedef struct SSFDCTYPE_T {
	BYTE Model;
	BYTE Attribute;
	BYTE MaxZones;
	BYTE MaxSectors;
	WORD MaxBlocks;
	WORD MaxLogBlocks;
} *SSFDCTYPE_T;

struct ADDRESS {
	BYTE Zone;	
	BYTE Sector;	
	WORD PhyBlock;	
	WORD LogBlock;	
};

typedef struct ADDRESS_T {
	BYTE Zone;	
	BYTE Sector;	
	WORD PhyBlock;	
	WORD LogBlock;	
} *ADDRESS_T;

struct CIS_AREA {
	BYTE Sector;	
	WORD PhyBlock;	
};


extern BYTE IsSSFDCCompliance;
extern BYTE IsXDCompliance;

extern DWORD	ErrXDCode;
extern DWORD	ErrCode;
extern WORD	ReadBlock;
extern WORD	WriteBlock;
extern DWORD	MediaChange;

extern struct SSFDCTYPE  Ssfdc;
extern struct ADDRESS    Media;
extern struct CIS_AREA   CisArea;

int         Init_D_SmartMedia(void);
int         Pwoff_D_SmartMedia(void);
int         Check_D_SmartMedia(void);
int         Check_D_Parameter(struct us_data *, WORD *, BYTE *, BYTE *);
int         Media_D_ReadSector(struct us_data *, DWORD, WORD, BYTE *);
int         Media_D_WriteSector(struct us_data *, DWORD, WORD, BYTE *);
int         Media_D_CopySector(struct us_data *, DWORD, WORD, BYTE *);
int         Media_D_EraseBlock(struct us_data *, DWORD, WORD);
int         Media_D_EraseAll(struct us_data *);
int         Media_D_OneSectWriteStart(struct us_data *, DWORD, BYTE *);
int         Media_D_OneSectWriteNext(struct us_data *, BYTE *);
int         Media_D_OneSectWriteFlush(struct us_data *);

extern int	SM_FreeMem(void);	
void        SM_EnableLED(struct us_data *, BOOLEAN);
void        Led_D_TernOn(void);
void        Led_D_TernOff(void);

int         Media_D_EraseAllRedtData(DWORD Index, BOOLEAN CheckBlock);

int  Check_D_DataBlank(BYTE *);
int  Check_D_FailBlock(BYTE *);
int  Check_D_DataStatus(BYTE *);
int  Load_D_LogBlockAddr(BYTE *);
void Clr_D_RedundantData(BYTE *);
void Set_D_LogBlockAddr(BYTE *);
void Set_D_FailBlock(BYTE *);
void Set_D_DataStaus(BYTE *);

void Ssfdc_D_Reset(struct us_data *);
int  Ssfdc_D_ReadCisSect(struct us_data *, BYTE *, BYTE *);
void Ssfdc_D_WriteRedtMode(void);
void Ssfdc_D_ReadID(BYTE *, BYTE);
int  Ssfdc_D_ReadSect(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_ReadBlock(struct us_data *, WORD, BYTE *, BYTE *);
int  Ssfdc_D_WriteSect(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_WriteBlock(struct us_data *, WORD, BYTE *, BYTE *);
int  Ssfdc_D_CopyBlock(struct us_data *, WORD, BYTE *, BYTE *);
int  Ssfdc_D_WriteSectForCopy(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_EraseBlock(struct us_data *);
int  Ssfdc_D_ReadRedtData(struct us_data *, BYTE *);
int  Ssfdc_D_WriteRedtData(struct us_data *, BYTE *);
int  Ssfdc_D_CheckStatus(void);
int  Set_D_SsfdcModel(BYTE);
void Cnt_D_Reset(void);
int  Cnt_D_PowerOn(void);
void Cnt_D_PowerOff(void);
void Cnt_D_LedOn(void);
void Cnt_D_LedOff(void);
int  Check_D_CntPower(void);
int  Check_D_CardExist(void);
int  Check_D_CardStsChg(void);
int  Check_D_SsfdcWP(void);
int  SM_ReadBlock(struct us_data *, BYTE *, BYTE *);

int  Ssfdc_D_ReadSect_DMA(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_ReadSect_PIO(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_WriteSect_DMA(struct us_data *, BYTE *, BYTE *);
int  Ssfdc_D_WriteSect_PIO(struct us_data *, BYTE *, BYTE *);

int  Check_D_ReadError(BYTE *);
int  Check_D_Correct(BYTE *, BYTE *);
int  Check_D_CISdata(BYTE *, BYTE *);
void Set_D_RightECC(BYTE *);

void calculate_ecc(BYTE *, BYTE *, BYTE *, BYTE *, BYTE *);
BYTE correct_data(BYTE *, BYTE *, BYTE,   BYTE,   BYTE);
int  _Correct_D_SwECC(BYTE *, BYTE *, BYTE *);
void _Calculate_D_SwECC(BYTE *, BYTE *);

void SM_Init(void);

#endif 
