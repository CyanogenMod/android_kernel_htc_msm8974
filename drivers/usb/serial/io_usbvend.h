/************************************************************************
 *
 *	USBVEND.H		Vendor-specific USB definitions
 *
 *	NOTE: This must be kept in sync with the Edgeport firmware and
 *	must be kept backward-compatible with older firmware.
 *
 ************************************************************************
 *
 *	Copyright (C) 1998 Inside Out Networks, Inc.
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 ************************************************************************/

#if !defined(_USBVEND_H)
#define	_USBVEND_H



#define	USB_VENDOR_ID_ION	0x1608		
#define	USB_VENDOR_ID_TI	0x0451		
#define USB_VENDOR_ID_AXIOHM	0x05D9		


#define	ION_OEM_ID_ION		0		
#define	ION_OEM_ID_NLYNX	1		
#define	ION_OEM_ID_GENERIC	2		
#define	ION_OEM_ID_MAC		3		
#define	ION_OEM_ID_MEGAWOLF	4		
#define	ION_OEM_ID_MULTITECH	5		
#define	ION_OEM_ID_AGILENT	6		



#define ION_DEVICE_ID_80251_NETCHIP	0x020	
						

#define ION_DEVICE_ID_GENERATION_1	0x00	
#define ION_DEVICE_ID_GENERATION_2	0x01	
#define ION_DEVICE_ID_GENERATION_3	0x02	
#define ION_DEVICE_ID_GENERATION_4	0x03	
#define ION_GENERATION_MASK		0x03

#define ION_DEVICE_ID_HUB_MASK		0x0080	
						
						

#define EDGEPORT_DEVICE_ID_MASK			0x0ff	

#define	ION_DEVICE_ID_UNCONFIGURED_EDGE_DEVICE	0x000	
#define ION_DEVICE_ID_EDGEPORT_4		0x001	
#define	ION_DEVICE_ID_EDGEPORT_8R		0x002	
#define ION_DEVICE_ID_RAPIDPORT_4		0x003	
#define ION_DEVICE_ID_EDGEPORT_4T		0x004	
#define ION_DEVICE_ID_EDGEPORT_2		0x005	
#define ION_DEVICE_ID_EDGEPORT_4I		0x006	
#define ION_DEVICE_ID_EDGEPORT_2I		0x007	
#define	ION_DEVICE_ID_EDGEPORT_8RR		0x008	
#define	ION_DEVICE_ID_EDGEPORT_PARALLEL_PORT	0x00B	
#define	ION_DEVICE_ID_EDGEPORT_421		0x00C	
#define	ION_DEVICE_ID_EDGEPORT_21		0x00D	
#define ION_DEVICE_ID_EDGEPORT_8_DUAL_CPU	0x00E	
#define ION_DEVICE_ID_EDGEPORT_8		0x00F	
#define ION_DEVICE_ID_EDGEPORT_2_DIN		0x010	
#define ION_DEVICE_ID_EDGEPORT_4_DIN		0x011	
#define ION_DEVICE_ID_EDGEPORT_16_DUAL_CPU	0x012	
#define ION_DEVICE_ID_EDGEPORT_COMPATIBLE	0x013	
#define ION_DEVICE_ID_EDGEPORT_8I		0x014	
#define ION_DEVICE_ID_EDGEPORT_1		0x015	
#define ION_DEVICE_ID_EPOS44			0x016	
#define ION_DEVICE_ID_EDGEPORT_42		0x017	
#define ION_DEVICE_ID_EDGEPORT_412_8		0x018	
#define ION_DEVICE_ID_EDGEPORT_412_4		0x019	
#define ION_DEVICE_ID_EDGEPORT_22I		0x01A	

#define ION_DEVICE_ID_EDGEPORT_2C		0x01B	
#define ION_DEVICE_ID_EDGEPORT_221C		0x01C	
							
#define ION_DEVICE_ID_EDGEPORT_22C		0x01D	
							
#define ION_DEVICE_ID_EDGEPORT_21C		0x01E	
							




#define ION_DEVICE_ID_TI3410_EDGEPORT_1		0x040	
#define ION_DEVICE_ID_TI3410_EDGEPORT_1I	0x041	

#define ION_DEVICE_ID_EDGEPORT_4S		0x042	
#define ION_DEVICE_ID_EDGEPORT_8S		0x043	

#define ION_DEVICE_ID_EDGEPORT_E		0x0E0	

#define ION_DEVICE_ID_TI_EDGEPORT_4		0x0201	
#define ION_DEVICE_ID_TI_EDGEPORT_2		0x0205	
#define ION_DEVICE_ID_TI_EDGEPORT_4I		0x0206	
#define ION_DEVICE_ID_TI_EDGEPORT_2I		0x0207	
#define ION_DEVICE_ID_TI_EDGEPORT_421		0x020C	
#define ION_DEVICE_ID_TI_EDGEPORT_21		0x020D	
#define ION_DEVICE_ID_TI_EDGEPORT_416		0x0212  
#define ION_DEVICE_ID_TI_EDGEPORT_1		0x0215	
#define ION_DEVICE_ID_TI_EDGEPORT_42		0x0217	
#define ION_DEVICE_ID_TI_EDGEPORT_22I		0x021A	
#define ION_DEVICE_ID_TI_EDGEPORT_2C		0x021B	
#define ION_DEVICE_ID_TI_EDGEPORT_221C		0x021C	
							
#define ION_DEVICE_ID_TI_EDGEPORT_22C		0x021D	
							
#define ION_DEVICE_ID_TI_EDGEPORT_21C		0x021E	

#define ION_DEVICE_ID_TI_TI3410_EDGEPORT_1	0x0240	
#define ION_DEVICE_ID_TI_TI3410_EDGEPORT_1I	0x0241	

#define ION_DEVICE_ID_TI_EDGEPORT_4S		0x0242	
#define ION_DEVICE_ID_TI_EDGEPORT_8S		0x0243	
#define ION_DEVICE_ID_TI_EDGEPORT_8		0x0244	
#define ION_DEVICE_ID_TI_EDGEPORT_416B		0x0247	



#define ION_DEVICE_ID_WP_UNSERIALIZED		0x300	
#define ION_DEVICE_ID_WP_PROXIMITY		0x301	
#define ION_DEVICE_ID_WP_MOTION			0x302	
#define ION_DEVICE_ID_WP_MOISTURE		0x303	
#define ION_DEVICE_ID_WP_TEMPERATURE		0x304	
#define ION_DEVICE_ID_WP_HUMIDITY		0x305	

#define ION_DEVICE_ID_WP_POWER			0x306	
#define ION_DEVICE_ID_WP_LIGHT			0x307	
#define ION_DEVICE_ID_WP_RADIATION		0x308	
#define ION_DEVICE_ID_WP_ACCELERATION		0x309	
#define ION_DEVICE_ID_WP_DISTANCE		0x30A	
#define ION_DEVICE_ID_WP_PROX_DIST		0x30B	
							

#define ION_DEVICE_ID_PLUS_PWR_HP4CD		0x30C	
#define ION_DEVICE_ID_PLUS_PWR_HP4C		0x30D	
#define ION_DEVICE_ID_PLUS_PWR_PCI		0x30E	


#define	USB_VENDOR_ID_AXIOHM			0x05D9	

#define AXIOHM_DEVICE_ID_MASK			0xffff
#define AXIOHM_DEVICE_ID_EPIC_A758		0xA758
#define AXIOHM_DEVICE_ID_EPIC_A794		0xA794
#define AXIOHM_DEVICE_ID_EPIC_A225		0xA225


#define	USB_VENDOR_ID_NCR			0x0404	

#define NCR_DEVICE_ID_MASK			0xffff
#define NCR_DEVICE_ID_EPIC_0202			0x0202
#define NCR_DEVICE_ID_EPIC_0203			0x0203
#define NCR_DEVICE_ID_EPIC_0310			0x0310
#define NCR_DEVICE_ID_EPIC_0311			0x0311
#define NCR_DEVICE_ID_EPIC_0312			0x0312


#define USB_VENDOR_ID_SYMBOL			0x05E0	
#define SYMBOL_DEVICE_ID_MASK			0xffff
#define SYMBOL_DEVICE_ID_KEYFOB			0x0700


#define ION_DEVICE_ID_MT4X56USB			0x1403	


#define	GENERATION_ID_FROM_USB_PRODUCT_ID(ProductId)				\
			((__u16) ((ProductId >> 8) & (ION_GENERATION_MASK)))

#define	MAKE_USB_PRODUCT_ID(OemId, DeviceId)					\
			((__u16) (((OemId) << 10) || (DeviceId)))

#define	DEVICE_ID_FROM_USB_PRODUCT_ID(ProductId)				\
			((__u16) ((ProductId) & (EDGEPORT_DEVICE_ID_MASK)))

#define	OEM_ID_FROM_USB_PRODUCT_ID(ProductId)					\
			((__u16) (((ProductId) >> 10) & 0x3F))


#define EDGE_FW_GET_TX_CREDITS_SEND_THRESHOLD(InitialCredit, MaxPacketSize) (max(((InitialCredit) / 4), (MaxPacketSize)))

#define	EDGE_FW_BULK_MAX_PACKET_SIZE		64	
#define EDGE_FW_BULK_READ_BUFFER_SIZE		1024	

#define	EDGE_FW_INT_MAX_PACKET_SIZE		32	
							
							
#define EDGE_FW_INT_INTERVAL			2	



#define USB_REQUEST_ION_RESET_DEVICE	0	
#define USB_REQUEST_ION_GET_EPIC_DESC	1	
#define USB_REQUEST_ION_READ_RAM	3	
#define USB_REQUEST_ION_WRITE_RAM	4	
#define USB_REQUEST_ION_READ_ROM	5	
#define USB_REQUEST_ION_WRITE_ROM	6	
#define USB_REQUEST_ION_EXEC_DL_CODE	7	
						
#define USB_REQUEST_ION_ENABLE_SUSPEND	9	
						

#define USB_REQUEST_ION_SEND_IOSP	10	
#define USB_REQUEST_ION_RECV_IOSP	11	


#define USB_REQUEST_ION_DIS_INT_TIMER	0x80	
						
						
						



struct edge_compatibility_bits {
	
	

	__u32	VendEnableSuspend	:  1;	
	__u32	VendUnused		: 31;	

	
	

											
	__u32	IOSPOpen		:  1;	
	__u32	IOSPClose		:  1;	
	__u32	IOSPChase		:  1;	
	__u32	IOSPSetRxFlow		:  1;	
	__u32	IOSPSetTxFlow		:  1;	
	__u32	IOSPSetXChar		:  1;	
	__u32	IOSPRxCheck		:  1;	
	__u32	IOSPSetClrBreak		:  1;	
	__u32	IOSPWriteMCR		:  1;	
	__u32	IOSPWriteLCR		:  1;	
	__u32	IOSPSetBaudRate		:  1;	
	__u32	IOSPDisableIntPipe	:  1;	
	__u32	IOSPRxDataAvail		:  1;   
	__u32	IOSPTxPurge		:  1;	
	__u32	IOSPUnused		: 18;	

	

	__u32	TrueEdgeport		:  1;	
											
	__u32	GenUnused		: 31;	
};

#define EDGE_COMPATIBILITY_MASK0	0x0001
#define EDGE_COMPATIBILITY_MASK1	0x3FFF
#define EDGE_COMPATIBILITY_MASK2	0x0001

struct edge_compatibility_descriptor {
	__u8	Length;				
	__u8	DescType;			
	__u8	EpicVer;			
						
	__u8	NumPorts;			
	__u8	iDownloadFile;			
						
	__u8	Unused[3];			
						
	__u8	MajorVersion;			
	__u8	MinorVersion;			
	__le16	BuildNumber;			

	
	
	
	struct edge_compatibility_bits	Supports;
};

#define	EDGE_DOWNLOAD_FILE_NONE		0	
#define	EDGE_DOWNLOAD_FILE_INTERNAL	0xFF	
#define	EDGE_DOWNLOAD_FILE_I930		0xFF	
#define	EDGE_DOWNLOAD_FILE_80251	0xFE	




#define	EDGE_MANUF_DESC_ADDR_V1		0x00FF7F00
#define	EDGE_MANUF_DESC_LEN_V1		sizeof(EDGE_MANUF_DESCRIPTOR_V1)

#define	EDGE_MANUF_DESC_ADDR		0x00FF7C00
#define	EDGE_MANUF_DESC_LEN		sizeof(struct edge_manuf_descriptor)

#define	EDGE_BOOT_DESC_ADDR		0x00FF7FC0
#define	EDGE_BOOT_DESC_LEN		sizeof(struct edge_boot_descriptor)

// Define the max block size that may be read or written
#define	MAX_SIZE_REQ_ION_READ_MEM	((__u16)64)
#define	MAX_SIZE_REQ_ION_WRITE_MEM	((__u16)64)



// file, converted to a binary image, and then written by the serialization

#define MAX_SERIALNUMBER_LEN	12
#define MAX_ASSEMBLYNUMBER_LEN	14

struct edge_manuf_descriptor {

	__u16	RootDescTable[0x10];			
	__u8	DescriptorArea[0x2E0];			

							
	__u8	Length;					
	__u8	DescType;				
	__u8	DescVer;				
	__u8	NumRootDescEntries;			

	__u8	RomSize;				
	__u8	RamSize;				
	__u8	CpuRev;					
	__u8	BoardRev;				

	__u8	NumPorts;				
	__u8	DescDate[3];				
							

	__u8	SerNumLength;				
	__u8	SerNumDescType;				
	__le16	SerialNumber[MAX_SERIALNUMBER_LEN];	

	__u8	AssemblyNumLength;			
	__u8	AssemblyNumDescType;			
	__le16	AssemblyNumber[MAX_ASSEMBLYNUMBER_LEN];	

	__u8	OemAssyNumLength;			
	__u8	OemAssyNumDescType;			
	__le16	OemAssyNumber[MAX_ASSEMBLYNUMBER_LEN];	

	__u8	ManufDateLength;			
	__u8	ManufDateDescType;			
	__le16	ManufDate[6];				

	__u8	Reserved3[0x4D];			

	__u8	UartType;				
	__u8	IonPid;					
							
							
	__u8	IonConfig;				
							

};


#define MANUF_DESC_VER_1	1	
#define MANUF_DESC_VER_2	2	


#define MANUF_UART_EXAR_654_EARLY	0	
#define MANUF_UART_EXAR_654		1	
#define MANUF_UART_EXAR_2852		2	


#define	MANUF_CPU_REV_AD4		1	
#define	MANUF_CPU_REV_AD5		2	
#define	MANUF_CPU_80251			0x20	


#define MANUF_BOARD_REV_A		1	
#define MANUF_BOARD_REV_B		2	
#define MANUF_BOARD_REV_C		3	
#define MANUF_BOARD_REV_GENERATION_2	0x20	


#define	MANUF_CPU_REV_1			1	

#define MANUF_BOARD_REV_A		1	

#define	MANUF_SERNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->SerialNumber)
#define	MANUF_ASSYNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->AssemblyNumber)
#define	MANUF_OEMASSYNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->OemAssyNumber)
#define	MANUF_MANUFDATE_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->ManufDate)

#define	MANUF_ION_CONFIG_DIAG_NO_LOOP	0x20	
#define	MANUF_ION_CONFIG_DIAG		0x40	
						
#define	MANUF_ION_CONFIG_MASTER		0x80	
						

struct edge_boot_descriptor {
	__u8		Length;			
	__u8		DescType;		
	__u8		DescVer;		
	__u8		Reserved1;		

	__le16		BootCodeLength;		
						

	__u8		MajorVersion;		
	__u8		MinorVersion;		
	__le16		BuildNumber;		

	__u16		EnumRootDescTable;	
	__u8		NumDescTypes;		

	__u8		Reserved4;		

	__le16		Capabilities;		
	__u8		Reserved2[0x28];	
	__u8		UConfig0;		
	__u8		UConfig1;		
	__u8		Reserved3[6];		
						
};


#define BOOT_DESC_VER_1		1	
#define BOOT_DESC_VER_2		2	


	

#define	BOOT_CAP_RESET_CMD	0x0001	



#define UMP5152			0x52
#define UMP3410			0x10


#define I2C_DESC_TYPE_INFO_BASIC	0x01
#define I2C_DESC_TYPE_FIRMWARE_BASIC	0x02
#define I2C_DESC_TYPE_DEVICE		0x03
#define I2C_DESC_TYPE_CONFIG		0x04
#define I2C_DESC_TYPE_STRING		0x05
#define I2C_DESC_TYPE_FIRMWARE_AUTO	0x07	
#define I2C_DESC_TYPE_CONFIG_KLUDGE	0x14	
#define I2C_DESC_TYPE_WATCHPORT_VERSION	0x15	
#define I2C_DESC_TYPE_WATCHPORT_CALIBRATION_DATA 0x16	

#define I2C_DESC_TYPE_FIRMWARE_BLANK	0xf2

#define I2C_DESC_TYPE_ION		0	


struct ti_i2c_desc {
	__u8	Type;			
	__u16	Size;			
	__u8	CheckSum;		
	__u8	Data[0];		
} __attribute__((packed));

struct ti_i2c_firmware_rec {
	__u8	Ver_Major;		
	__u8	Ver_Minor;		
	__u8	Data[0];		
} __attribute__((packed));


struct watchport_firmware_version {
	__u8	Version_Major;		
	__u8	Version_Minor;
} __attribute__((packed));


struct ti_i2c_image_header {
	__le16	Length;
	__u8	CheckSum;
} __attribute__((packed));

struct ti_basic_descriptor {
	__u8	Power;		
				
				
				
				
				
				
				
	__u16	HubVid;		
	__u16	HubPid;		
	__u16	DevPid;		
	__u8	HubTime;	
	__u8	HubCurrent;	
} __attribute__((packed));


#define TI_CPU_REV_5052			2	
#define TI_CPU_REV_3410			3	

#define TI_BOARD_REV_TI_EP		0	
#define TI_BOARD_REV_COMPACT		1	
#define TI_BOARD_REV_WATCHPORT		2	


#define TI_GET_CPU_REVISION(x)		(__u8)((((x)>>4)&0x0f))
#define TI_GET_BOARD_REVISION(x)	(__u8)(((x)&0x0f))

#define TI_I2C_SIZE_MASK		0x1f  
#define TI_GET_I2C_SIZE(x)		((((x) & TI_I2C_SIZE_MASK)+1)*256)

#define TI_MAX_I2C_SIZE			(16 * 1024)

#define TI_MANUF_VERSION_0		0

#define TI_CONFIG2_RS232		0x01
#define TI_CONFIG2_RS422		0x02
#define TI_CONFIG2_RS485		0x04
#define TI_CONFIG2_SWITCHABLE		0x08

#define TI_CONFIG2_WATCHPORT		0x10


struct edge_ti_manuf_descriptor {
	__u8 IonConfig;		
	__u8 IonConfig2;	
	__u8 Version;		
	__u8 CpuRev_BoardRev;	
	__u8 NumPorts;		
	__u8 NumVirtualPorts;	
	__u8 HubConfig1;	
	__u8 HubConfig2;	
	__u8 TotalPorts;	
	__u8 Reserved;		
} __attribute__((packed));


#endif		
