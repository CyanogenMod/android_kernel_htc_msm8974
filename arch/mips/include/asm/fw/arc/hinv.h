#ifndef _ASM_ARC_HINV_H
#define _ASM_ARC_HINV_H

#include <asm/sgidefs.h>
#include <asm/fw/arc/types.h>

typedef enum configclass {
	SystemClass,
	ProcessorClass,
	CacheClass,
#ifndef	_NT_PROM
	MemoryClass,
	AdapterClass,
	ControllerClass,
	PeripheralClass
#else	
	AdapterClass,
	ControllerClass,
	PeripheralClass,
	MemoryClass
#endif	
} CONFIGCLASS;

typedef enum configtype {
	ARC,
	CPU,
	FPU,
	PrimaryICache,
	PrimaryDCache,
	SecondaryICache,
	SecondaryDCache,
	SecondaryCache,
#ifndef	_NT_PROM
	Memory,
#endif
	EISAAdapter,
	TCAdapter,
	SCSIAdapter,
	DTIAdapter,
	MultiFunctionAdapter,
	DiskController,
	TapeController,
	CDROMController,
	WORMController,
	SerialController,
	NetworkController,
	DisplayController,
	ParallelController,
	PointerController,
	KeyboardController,
	AudioController,
	OtherController,
	DiskPeripheral,
	FloppyDiskPeripheral,
	TapePeripheral,
	ModemPeripheral,
	MonitorPeripheral,
	PrinterPeripheral,
	PointerPeripheral,
	KeyboardPeripheral,
	TerminalPeripheral,
	LinePeripheral,
	NetworkPeripheral,
#ifdef	_NT_PROM
	Memory,
#endif
	OtherPeripheral,

	
	
	

	XTalkAdapter,
	PCIAdapter,
	GIOAdapter,
	TPUAdapter,

	Anonymous
} CONFIGTYPE;

typedef enum {
	Failed = 1,
	ReadOnly = 2,
	Removable = 4,
	ConsoleIn = 8,
	ConsoleOut = 16,
	Input = 32,
	Output = 64
} IDENTIFIERFLAG;

#ifndef NULL			
#define	NULL	0
#endif

union key_u {
	struct {
#ifdef	_MIPSEB
		unsigned char  c_bsize;		
		unsigned char  c_lsize;		
		unsigned short c_size;		
#else	
		unsigned short c_size;		
		unsigned char  c_lsize;		
		unsigned char  c_bsize;		
#endif	
	} cache;
	ULONG FullKey;
};

#if _MIPS_SIM == _MIPS_SIM_ABI64
#define SGI_ARCS_VERS	64			
#define SGI_ARCS_REV	0			
#else
#define SGI_ARCS_VERS	1			
#define SGI_ARCS_REV	10			
#endif

typedef struct component {
	CONFIGCLASS	Class;
	CONFIGTYPE	Type;
	IDENTIFIERFLAG	Flags;
	USHORT		Version;
	USHORT		Revision;
	ULONG 		Key;
	ULONG		AffinityMask;
	ULONG		ConfigurationDataSize;
	ULONG		IdentifierLength;
	char		*Identifier;
} COMPONENT;

struct cfgdata {
	char *name;			
	int minlen;			
	CONFIGTYPE type;		
};

typedef struct systemid {
	CHAR VendorId[8];
	CHAR ProductId[8];
} SYSTEMID;

typedef enum memorytype {
	ExceptionBlock,
	SPBPage,			
#ifndef	_NT_PROM
	FreeContiguous,
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent
#else	
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent,
	FreeContiguous
#endif	
} MEMORYTYPE;

typedef struct memorydescriptor {
	MEMORYTYPE	Type;
	LONG		BasePage;
	LONG		PageCount;
} MEMORYDESCRIPTOR;

#endif 
