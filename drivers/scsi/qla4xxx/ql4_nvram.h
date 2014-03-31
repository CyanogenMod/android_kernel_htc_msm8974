/*
 * QLogic iSCSI HBA Driver
 * Copyright (c)  2003-2010 QLogic Corporation
 *
 * See LICENSE.qla4xxx for copyright and licensing details.
 */

#ifndef _QL4XNVRM_H_
#define _QL4XNVRM_H_

#define FM93C56A_SIZE_8	 0x100
#define FM93C56A_SIZE_16 0x80
#define FM93C66A_SIZE_8	 0x200
#define FM93C66A_SIZE_16 0x100
#define FM93C86A_SIZE_16 0x400

#define	 FM93C56A_START	      0x1

#define	 FM93C56A_READ	      0x2
#define	 FM93C56A_WEN	      0x0
#define	 FM93C56A_WRITE	      0x1
#define	 FM93C56A_WRITE_ALL   0x0
#define	 FM93C56A_WDS	      0x0
#define	 FM93C56A_ERASE	      0x3
#define	 FM93C56A_ERASE_ALL   0x0

#define	 FM93C56A_WEN_EXT	 0x3
#define	 FM93C56A_WRITE_ALL_EXT	 0x1
#define	 FM93C56A_WDS_EXT	 0x0
#define	 FM93C56A_ERASE_ALL_EXT	 0x2

#define	 FM93C56A_NO_ADDR_BITS_16   8	
#define	 FM93C56A_NO_ADDR_BITS_8    9	
#define	 FM93C86A_NO_ADDR_BITS_16   10	

#define	 FM93C56A_DATA_BITS_16	 16
#define	 FM93C56A_DATA_BITS_8	 8

#define	 FM93C56A_READ_DUMMY_BITS   1
#define	 FM93C56A_READY		    0
#define	 FM93C56A_BUSY		    1
#define	 FM93C56A_CMD_BITS	    2

#define	 AUBURN_EEPROM_DI	    0x8
#define	 AUBURN_EEPROM_DI_0	    0x0
#define	 AUBURN_EEPROM_DI_1	    0x8
#define	 AUBURN_EEPROM_DO	    0x4
#define	 AUBURN_EEPROM_DO_0	    0x0
#define	 AUBURN_EEPROM_DO_1	    0x4
#define	 AUBURN_EEPROM_CS	    0x2
#define	 AUBURN_EEPROM_CS_0	    0x0
#define	 AUBURN_EEPROM_CS_1	    0x2
#define	 AUBURN_EEPROM_CLK_RISE	    0x1
#define	 AUBURN_EEPROM_CLK_FALL	    0x0

struct bios_params {
	uint16_t SpinUpDelay:1;
	uint16_t BIOSDisable:1;
	uint16_t MMAPEnable:1;
	uint16_t BootEnable:1;
	uint16_t Reserved0:12;
	uint8_t bootID0:7;
	uint8_t bootID0Valid:1;
	uint8_t bootLUN0[8];
	uint8_t bootID1:7;
	uint8_t bootID1Valid:1;
	uint8_t bootLUN1[8];
	uint16_t MaxLunsPerTarget;
	uint8_t Reserved1[10];
};

struct eeprom_port_cfg {

	
	u16 etherMtu_mac;

	
	u16 pauseThreshold_mac;
	u16 resumeThreshold_mac;
	u16 reserved[13];
};

struct eeprom_function_cfg {
	u8 reserved[30];

	
	u8 macAddress[6];
	u8 macAddressSecondary[6];
	u16 subsysVendorId;
	u16 subsysDeviceId;
};

struct eeprom_data {
	union {
		struct {	
			u8 asic_id[4]; 
			u8 version;	
			u8 reserved;	
			u16 board_id;	
#define	  EEPROM_BOARDID_ELDORADO    1
#define	  EEPROM_BOARDID_PLACER	     2

#define EEPROM_SERIAL_NUM_SIZE	     16
			u8 serial_number[EEPROM_SERIAL_NUM_SIZE]; 

			
			u16 ext_hw_conf; 
			u8 mac0[6];	
			u8 mac1[6];	
			u8 mac2[6];	
			u8 mac3[6];	
			u16 etherMtu;	
			u16 macConfig;	
#define	 MAC_CONFIG_ENABLE_ANEG	    0x0001
#define	 MAC_CONFIG_ENABLE_PAUSE    0x0002
			u16 phyConfig;	
#define	 PHY_CONFIG_PHY_ADDR_MASK	      0x1f
#define	 PHY_CONFIG_ENABLE_FW_MANAGEMENT_MASK 0x20
			u16 reserved_56;	

#define EEPROM_UNUSED_1_SIZE   2
			u8 unused_1[EEPROM_UNUSED_1_SIZE]; 
			u16 bufletSize;	
			u16 bufletCount;	
			u16 bufletPauseThreshold; 
			u16 tcpWindowThreshold50; 
			u16 tcpWindowThreshold25; 
			u16 tcpWindowThreshold0; 
			u16 ipHashTableBaseHi;	
			u16 ipHashTableBaseLo;	
			u16 ipHashTableSize;	
			u16 tcpHashTableBaseHi;	
			u16 tcpHashTableBaseLo;	
			u16 tcpHashTableSize;	
			u16 ncbTableBaseHi;	
			u16 ncbTableBaseLo;	
			u16 ncbTableSize;	
			u16 drbTableBaseHi;	
			u16 drbTableBaseLo;	
			u16 drbTableSize;	

#define EEPROM_UNUSED_2_SIZE   4
			u8 unused_2[EEPROM_UNUSED_2_SIZE]; 
			u16 ipReassemblyTimeout; 
			u16 tcpMaxWindowSizeHi;	
			u16 tcpMaxWindowSizeLo;	
			u32 net_ip_addr0;	
			u32 net_ip_addr1;	
			u32 scsi_ip_addr0;	
			u32 scsi_ip_addr1;	
#define EEPROM_UNUSED_3_SIZE   128	
			u8 unused_3[EEPROM_UNUSED_3_SIZE]; 
			u16 subsysVendorId_f0;	
			u16 subsysDeviceId_f0;	

			
#define FM93C56A_SIGNATURE  0x9356
#define FM93C66A_SIGNATURE  0x9366
			u16 signature;	

#define EEPROM_UNUSED_4_SIZE   250
			u8 unused_4[EEPROM_UNUSED_4_SIZE]; 
			u16 subsysVendorId_f1;	
			u16 subsysDeviceId_f1;	
			u16 checksum;	
		} __attribute__ ((packed)) isp4010;
		struct {	
			u8 asicId[4];	
			u8 version;	
			u8 reserved_5;	
			u16 boardId;	
			u8 boardIdStr[16];	
			u8 serialNumber[16];	

			
			u16 ext_hw_conf;	

			
			struct eeprom_port_cfg macCfg_port0; 

			
			struct eeprom_port_cfg macCfg_port1; 

			
			u16 bufletSize;	
			u16 bufletCount;	
			u16 tcpWindowThreshold50; 
			u16 tcpWindowThreshold25; 
			u16 tcpWindowThreshold0; 
			u16 ipHashTableBaseHi;	
			u16 ipHashTableBaseLo;	
			u16 ipHashTableSize;	
			u16 tcpHashTableBaseHi;	
			u16 tcpHashTableBaseLo;	
			u16 tcpHashTableSize;	
			u16 ncbTableBaseHi;	
			u16 ncbTableBaseLo;	
			u16 ncbTableSize;	
			u16 drbTableBaseHi;	
			u16 drbTableBaseLo;	
			u16 drbTableSize;	
			u16 reserved_142[4];	

			
			u16 ipReassemblyTimeout; 
			u16 tcpMaxWindowSize;	
			u16 ipSecurity;	
			u8 reserved_156[294]; 
			u16 qDebug[8];	
			struct eeprom_function_cfg funcCfg_fn0;	
			u16 reserved_510; 

			
			u8 oemSpace[432]; 
			struct bios_params sBIOSParams_fn1; 
			struct eeprom_function_cfg funcCfg_fn1;	
			u16 reserved_1022; 

			
			u8 reserved_1024[464];	
			struct eeprom_function_cfg funcCfg_fn2;	
			u16 reserved_1534; 

			
			u8 reserved_1536[432];	
			struct bios_params sBIOSParams_fn3; 
			struct eeprom_function_cfg funcCfg_fn3;	
			u16 checksum;	
		} __attribute__ ((packed)) isp4022;
	};
};


#endif	
