/*
 * This file describes the structure passed from the BootX application
 * (for MacOS) when it is used to boot Linux.
 *
 * Written by Benjamin Herrenschmidt.
 */


#ifndef __ASM_BOOTX_H__
#define __ASM_BOOTX_H__

#include <linux/types.h>

#ifdef macintosh
#include <Types.h>
#include "linux_type_defs.h"
#endif

#ifdef macintosh
#pragma options align=power
#endif


#define BOOT_INFO_VERSION               5
#define BOOT_INFO_COMPATIBLE_VERSION    1

#define BOOT_ARCH_PCI                   0x00000001UL
#define BOOT_ARCH_NUBUS                 0x00000002UL
#define BOOT_ARCH_NUBUS_PDM             0x00000010UL
#define BOOT_ARCH_NUBUS_PERFORMA        0x00000020UL
#define BOOT_ARCH_NUBUS_POWERBOOK       0x00000040UL

#define MAX_MEM_MAP_SIZE				26

typedef struct boot_info_map_entry
{
    __u32       physAddr;                
    __u32       size;                    
} boot_info_map_entry_t;


typedef struct boot_infos
{
    
    __u32       version;
    
    __u32       compatible_version;

    __u8*       logicalDisplayBase;

    
    __u32       machineID;

    
    __u32       architecture;

    __u32       deviceTreeOffset;        
    __u32       deviceTreeSize;          

    
    __u32       dispDeviceRect[4];       
    __u32       dispDeviceDepth;         
    __u8*       dispDeviceBase;          
    __u32       dispDeviceRowBytes;      
    __u32       dispDeviceColorsOffset;  
     __u32      dispDeviceRegEntryOffset;

    
    __u32       ramDisk;
    __u32       ramDiskSize;             

    
    __u32       kernelParamsOffset;

    

    boot_info_map_entry_t
    	        physMemoryMap[MAX_MEM_MAP_SIZE]; 
    __u32       physMemoryMapSize;               


    
    __u32       frameBufferSize;         

    

    
    __u32       totalParamsSize;

} boot_infos_t;

#ifdef __KERNEL__
#define BOOTX_COLORTABLE_SIZE    (256UL*3UL*2UL)

struct bootx_dt_prop {
	u32	name;
	int	length;
	u32	value;
	u32	next;
};

struct bootx_dt_node {
	u32	unused0;
	u32	unused1;
	u32	phandle;	
	u32	unused2;
	u32	unused3;
	u32	unused4;
	u32	unused5;
	u32	full_name;
	u32	properties;
	u32	parent;
	u32	child;
	u32	sibling;
	u32	next;
	u32	allnext;
};

extern void bootx_init(unsigned long r4, unsigned long phys);

#endif 

#ifdef macintosh
#pragma options align=reset
#endif

#endif
