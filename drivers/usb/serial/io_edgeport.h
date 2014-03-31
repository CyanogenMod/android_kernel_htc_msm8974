/************************************************************************
 *
 *	io_edgeport.h	Edgeport Linux Interface definitions
 *
 *	Copyright (C) 2000 Inside Out Networks, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *
 ************************************************************************/

#if !defined(_IO_EDGEPORT_H_)
#define	_IO_EDGEPORT_H_


#define MAX_RS232_PORTS		8	

#ifndef LOW8
	#define LOW8(a)		((unsigned char)(a & 0xff))
#endif
#ifndef HIGH8
	#define HIGH8(a)	((unsigned char)((a & 0xff00) >> 8))
#endif

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include "io_usbvend.h"



#define MAX_EDGEPORTS	64

struct comMapper {
	char	SerialNumber[MAX_SERIALNUMBER_LEN+1];	
	int	numPorts;				
	int	Original[MAX_RS232_PORTS];		
	int	Port[MAX_RS232_PORTS];			
};


#define EDGEPORT_CONFIG_DEVICE "/proc/edgeport"

#define PROC_GET_MAPPING_TO_PATH	1
#define PROC_GET_COM_ENTRY		2
#define PROC_GET_EDGE_MANUF_DESCRIPTOR	3
#define PROC_GET_BOOT_DESCRIPTOR	4
#define PROC_GET_PRODUCT_INFO		5
#define PROC_GET_STRINGS		6
#define PROC_GET_CURRENT_COM_MAPPING	7

#define PROC_READ_SETUP(Command, Argument)	((Command) + ((Argument)<<8))


#define PROC_SET_COM_MAPPING		1
#define PROC_SET_COM_ENTRY		2


struct procWrite {
	int	Command;
	union {
		struct comMapper	Entry;
		int			ComMappingBasedOnUSBPort;	
	} u;
};

struct edgeport_product_info {
	__u16	ProductId;			
	__u8	NumPorts;			
	__u8	ProdInfoVer;			

	__u32	IsServer        :1;		
	__u32	IsRS232         :1;		
	__u32	IsRS422         :1;		
	__u32	IsRS485         :1;		
	__u32	IsReserved      :28;		

	__u8	RomSize;			
	__u8	RamSize;			
	__u8	CpuRev;				
	__u8	BoardRev;			

	__u8	BootMajorVersion;		
	__u8	BootMinorVersion;		
	__le16	BootBuildNumber;		

	__u8	FirmwareMajorVersion;		
	__u8	FirmwareMinorVersion;		
	__le16	FirmwareBuildNumber;		

	__u8	ManufactureDescDate[3];		
	__u8	HardwareType;

	__u8	iDownloadFile;			
	__u8	EpicVer;			

	struct edge_compatibility_bits Epic;
};

#define EDGESTRING_MANUFNAME		1	
#define EDGESTRING_PRODNAME		2	
#define EDGESTRING_SERIALNUM		3	
#define EDGESTRING_ASSEMNUM		4	
#define EDGESTRING_OEMASSEMNUM		5	
#define EDGESTRING_MANUFDATE		6	
#define EDGESTRING_ORIGSERIALNUM	7	

struct string_block {
	__u16	NumStrings;			
	__u16	Strings[1];			
};



#endif
