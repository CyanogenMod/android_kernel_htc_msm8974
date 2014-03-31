
/*
 *
 Copyright (c) Eicon Networks, 2002.
 *
 This source file is supplied for the use with
 Eicon Networks range of DIVA Server Adapters.
 *
 Eicon File Revision :    2.1
 *
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
 *
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
 implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.
 *
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef __DIVA_XDI_COMMON_IO_H_INC__ 
#define __DIVA_XDI_COMMON_IO_H_INC__
#define DI_MAX_LINKS    MAX_ADAPTER
#define ISDN_MAX_NUM_LEN 60
typedef struct {
	dword         Num;
	DEVICE_NAME   DeviceName[4];
	PISDN_ADAPTER QuadroAdapter[4];
} ADAPTER_LIST_ENTRY, *PADAPTER_LIST_ENTRY;
#define MAX_MAPPED_ENTRIES 8
typedef struct {
	void *Address;
	dword    Length;
} ADAPTER_MEMORY;
#define DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON      0x01
#define DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON 0x02
typedef struct _diva_xdi_capi_cfg {
	byte cfg_1;
} diva_xdi_capi_cfg_t;
struct _ISDN_ADAPTER {
	void (*DIRequest)(PISDN_ADAPTER, ENTITY *);
	int State; 
	int Initialized;
	int RegisteredWithDidd;
	int Unavailable;  
	int ResourcesClaimed;
	int PnpBiosConfigUsed;
	dword Logging;
	dword features;
	char ProtocolIdString[80];
	ADAPTER_MEMORY MappedMemory[MAX_MAPPED_ENTRIES];
	CARD_PROPERTIES Properties;
	dword cardType;
	dword protocol_id;       
	char protocol_name[8];  
	dword BusType;
	dword BusNumber;
	dword slotNumber;
	dword slotId;
	dword ControllerNumber;  
	PISDN_ADAPTER MultiMaster;       
	PADAPTER_LIST_ENTRY QuadroList;        
	PDEVICE_OBJECT DeviceObject;
	dword DeviceId;
	diva_os_adapter_irq_info_t irq_info;
	dword volatile IrqCount;
	int trapped;
	dword DspCodeBaseAddr;
	dword MaxDspCodeSize;
	dword downloadAddr;
	dword DspCodeBaseAddrTable[4]; 
	dword MaxDspCodeSizeTable[4]; 
	dword downloadAddrTable[4]; 
	dword MemoryBase;
	dword MemorySize;
	byte __iomem *Address;
	byte __iomem *Config;
	byte __iomem *Control;
	byte __iomem *reset;
	byte __iomem *port;
	byte __iomem *ram;
	byte __iomem *cfg;
	byte __iomem *prom;
	byte __iomem *ctlReg;
	struct pc_maint  *pcm;
	diva_os_dependent_devica_name_t os_name;
	byte Name[32];
	dword serialNo;
	dword ANum;
	dword ArchiveType; 
	char *ProtocolSuffix; 
	char Archive[32];
	char Protocol[32];
	char AddDownload[32]; 
	char Oad1[ISDN_MAX_NUM_LEN];
	char Osa1[ISDN_MAX_NUM_LEN];
	char Oad2[ISDN_MAX_NUM_LEN];
	char Osa2[ISDN_MAX_NUM_LEN];
	char Spid1[ISDN_MAX_NUM_LEN];
	char Spid2[ISDN_MAX_NUM_LEN];
	byte nosig;
	byte BriLayer2LinkCount; 
	dword Channels;
	dword tei;
	dword nt2;
	dword TerminalCount;
	dword WatchDog;
	dword Permanent;
	dword BChMask; 
	dword StableL2;
	dword DidLen;
	dword NoOrderCheck;
	dword ForceLaw; 
	dword SigFlags;
	dword LowChannel;
	dword NoHscx30;
	dword ProtVersion;
	dword crc4;
	dword L1TristateOrQsig; 
	dword InitialDspInfo;
	dword ModemGuardTone;
	dword ModemMinSpeed;
	dword ModemMaxSpeed;
	dword ModemOptions;
	dword ModemOptions2;
	dword ModemNegotiationMode;
	dword ModemModulationsMask;
	dword ModemTransmitLevel;
	dword FaxOptions;
	dword FaxMaxSpeed;
	dword Part68LevelLimiter;
	dword UsEktsNumCallApp;
	byte UsEktsFeatAddConf;
	byte UsEktsFeatRemoveConf;
	byte UsEktsFeatCallTransfer;
	byte UsEktsFeatMsgWaiting;
	byte QsigDialect;
	byte ForceVoiceMailAlert;
	byte DisableAutoSpid;
	byte ModemCarrierWaitTimeSec;
	byte ModemCarrierLossWaitTimeTenthSec;
	byte PiafsLinkTurnaroundInFrames;
	byte DiscAfterProgress;
	byte AniDniLimiter[3];
	byte TxAttenuation;  
	word QsigFeatures;
	dword GenerateRingtone;
	dword SupplementaryServicesFeatures;
	dword R2Dialect;
	dword R2CasOptions;
	dword FaxV34Options;
	dword DisabledDspMask;
	dword AdapterTestMask;
	dword DspImageLength;
	word AlertToIn20mSecTicks;
	word ModemEyeSetup;
	byte R2CtryLength;
	byte CCBSRelTimer;
	byte *PcCfgBufferFile;
	byte *PcCfgBuffer; 
	diva_os_dump_file_t dump_file; 
	diva_os_board_trace_t board_trace; 
	diva_os_spin_lock_t isr_spin_lock;
	diva_os_spin_lock_t data_spin_lock;
	diva_os_soft_isr_t req_soft_isr;
	diva_os_soft_isr_t isr_soft_isr;
	diva_os_atomic_t  in_dpc;
	PBUFFER RBuffer;        
	word e_max;
	word e_count;
	E_INFO *e_tbl;
	word assign;         
	word head;           
	word tail;           
	ADAPTER a;             
	void (*out)(ADAPTER *a);
	byte (*dpc)(ADAPTER *a);
	byte (*tst_irq)(ADAPTER *a);
	void (*clr_irq)(ADAPTER *a);
	int (*load)(PISDN_ADAPTER);
	int (*mapmem)(PISDN_ADAPTER);
	int (*chkIrq)(PISDN_ADAPTER);
	void (*disIrq)(PISDN_ADAPTER);
	void (*start)(PISDN_ADAPTER);
	void (*stop)(PISDN_ADAPTER);
	void (*rstFnc)(PISDN_ADAPTER);
	void (*trapFnc)(PISDN_ADAPTER);
	dword (*DetectDsps)(PISDN_ADAPTER);
	void (*os_trap_nfy_Fnc)(PISDN_ADAPTER, dword);
	diva_os_isr_callback_t diva_isr_handler;
	dword sdram_bar;  
	dword fpga_features;
	volatile int pcm_pending;
	volatile void *pcm_data;
	diva_xdi_capi_cfg_t capi_cfg;
	dword tasks;
	void *dma_map;
	int (*DivaAdapterTestProc)(PISDN_ADAPTER);
	void *AdapterTestMemoryStart;
	dword AdapterTestMemoryLength;
	const byte *cfg_lib_memory_init;
	dword cfg_lib_memory_init_length;
};
struct e_info_s {
	ENTITY *e;
	byte          next;                   
	word          assign_ref;             
};
struct s_load {
	byte ctrl;
	byte card;
	byte msize;
	byte fill0;
	word ebit;
	word elocl;
	word eloch;
	byte reserved[20];
	word signature;
	byte fill[224];
	byte b[256];
};
#define PR_RAM  ((struct pr_ram *)0)
#define RAM ((struct dual *)0)
extern void *PTR_P(ADAPTER *a, ENTITY *e, void *P);
extern void *PTR_X(ADAPTER *a, ENTITY *e);
extern void *PTR_R(ADAPTER *a, ENTITY *e);
extern void CALLBACK(ADAPTER *a, ENTITY *e);
extern void set_ram(void **adr_ptr);
byte io_in(ADAPTER *a, void *adr);
word io_inw(ADAPTER *a, void *adr);
void io_in_buffer(ADAPTER *a, void *adr, void *P, word length);
void io_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void io_out(ADAPTER *a, void *adr, byte data);
void io_outw(ADAPTER *a, void *adr, word data);
void io_out_buffer(ADAPTER *a, void *adr, void *P, word length);
void io_inc(ADAPTER *a, void *adr);
void bri_in_buffer(PISDN_ADAPTER IoAdapter, dword Pos,
		   void *Buf, dword Len);
int bri_out_buffer(PISDN_ADAPTER IoAdapter, dword Pos,
		   void *Buf, dword Len, int Verify);
byte mem_in(ADAPTER *a, void *adr);
word mem_inw(ADAPTER *a, void *adr);
void mem_in_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_look_ahead(ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void mem_out(ADAPTER *a, void *adr, byte data);
void mem_outw(ADAPTER *a, void *adr, word data);
void mem_out_buffer(ADAPTER *a, void *adr, void *P, word length);
void mem_inc(ADAPTER *a, void *adr);
void mem_in_dw(ADAPTER *a, void *addr, dword *data, int dwords);
void mem_out_dw(ADAPTER *a, void *addr, const dword *data, int dwords);
extern IDI_CALL Requests[MAX_ADAPTER];
extern void     DIDpcRoutine(struct _diva_os_soft_isr *psoft_isr,
			     void *context);
extern void     request(PISDN_ADAPTER, ENTITY *);
typedef struct {
	word *buf;
	word  cnt;
	word  out;
} Xdesc;
extern void dump_trap_frame(PISDN_ADAPTER IoAdapter, byte __iomem *exception);
extern void dump_xlog_buffer(PISDN_ADAPTER IoAdapter, Xdesc *xlogDesc);
#endif  
