/*
** asm/bootinfo.h -- Definition of the Linux/m68k boot information structure
**
** Copyright 1992 by Greg Harp
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
** Created 09/29/92 by Greg Harp
**
** 5/2/94 Roman Hodek:
**   Added bi_atari part of the machine dependent union bi_un; for now it
**   contains just a model field to distinguish between TT and Falcon.
** 26/7/96 Roman Zippel:
**   Renamed to setup.h; added some useful macros to allow gcc some
**   optimizations if possible.
** 5/10/96 Geert Uytterhoeven:
**   Redesign of the boot information structure; renamed to bootinfo.h again
** 27/11/96 Geert Uytterhoeven:
**   Backwards compatibility with bootinfo interface version 1.0
*/

#ifndef _M68K_BOOTINFO_H
#define _M68K_BOOTINFO_H



#ifndef __ASSEMBLY__

struct bi_record {
    unsigned short tag;			
    unsigned short size;		
    unsigned long data[0];		
};

#endif 



#define BI_LAST			0x0000	
#define BI_MACHTYPE		0x0001	
#define BI_CPUTYPE		0x0002	
#define BI_FPUTYPE		0x0003	
#define BI_MMUTYPE		0x0004	
#define BI_MEMCHUNK		0x0005	
					
#define BI_RAMDISK		0x0006	
					
#define BI_COMMAND_LINE		0x0007	
					


#define BI_AMIGA_MODEL		0x8000	
#define BI_AMIGA_AUTOCON	0x8001	
					
#define BI_AMIGA_CHIP_SIZE	0x8002	
#define BI_AMIGA_VBLANK		0x8003	
#define BI_AMIGA_PSFREQ		0x8004	
#define BI_AMIGA_ECLOCK		0x8005	
#define BI_AMIGA_CHIPSET	0x8006	
#define BI_AMIGA_SERPER		0x8007	


#define BI_ATARI_MCH_COOKIE	0x8000	
#define BI_ATARI_MCH_TYPE	0x8001	
					

#define ATARI_MCH_ST		0
#define ATARI_MCH_STE		1
#define ATARI_MCH_TT		2
#define ATARI_MCH_FALCON	3

#define ATARI_MACH_NORMAL	0	
#define ATARI_MACH_MEDUSA	1	
#define ATARI_MACH_HADES	2	
#define ATARI_MACH_AB40		3	


#define BI_VME_TYPE		0x8000	
#define BI_VME_BRDINFO		0x8001	

#define	VME_TYPE_TP34V		0x0034	
#define VME_TYPE_MVME147	0x0147	
#define VME_TYPE_MVME162	0x0162	
#define VME_TYPE_MVME166	0x0166	
#define VME_TYPE_MVME167	0x0167	
#define VME_TYPE_MVME172	0x0172	
#define VME_TYPE_MVME177	0x0177	
#define VME_TYPE_BVME4000	0x4000	
#define VME_TYPE_BVME6000	0x6000	




#define BI_MAC_MODEL		0x8000	
#define BI_MAC_VADDR		0x8001	
#define BI_MAC_VDEPTH		0x8002	
#define BI_MAC_VROW		0x8003	
#define BI_MAC_VDIM		0x8004	
#define BI_MAC_VLOGICAL		0x8005	
#define BI_MAC_SCCBASE		0x8006	
#define BI_MAC_BTIME		0x8007	
#define BI_MAC_GMTBIAS		0x8008	
#define BI_MAC_MEMSIZE		0x8009	
#define BI_MAC_CPUID		0x800a	
#define BI_MAC_ROMBASE		0x800b	


#define BI_MAC_VIA1BASE		0x8010	
#define BI_MAC_VIA2BASE		0x8011	
#define BI_MAC_VIA2TYPE		0x8012	
#define BI_MAC_ADBTYPE		0x8013	
#define BI_MAC_ASCBASE		0x8014	
#define BI_MAC_SCSI5380		0x8015	
#define BI_MAC_SCSIDMA		0x8016	
#define BI_MAC_SCSI5396		0x8017	
#define BI_MAC_IDETYPE		0x8018	
#define BI_MAC_IDEBASE		0x8019	
#define BI_MAC_NUBUS		0x801a	
#define BI_MAC_SLOTMASK		0x801b	
#define BI_MAC_SCCTYPE		0x801c	
#define BI_MAC_ETHTYPE		0x801d	
#define BI_MAC_ETHBASE		0x801e	
#define BI_MAC_PMU		0x801f	
#define BI_MAC_IOP_SWIM		0x8020	
#define BI_MAC_IOP_ADB		0x8021	


#ifndef __ASSEMBLY__

struct mac_booter_data
{
	unsigned long videoaddr;
	unsigned long videorow;
	unsigned long videodepth;
	unsigned long dimensions;
	unsigned long args;
	unsigned long boottime;
	unsigned long gmtbias;
	unsigned long bootver;
	unsigned long videological;
	unsigned long sccbase;
	unsigned long id;
	unsigned long memsize;
	unsigned long serialmf;
	unsigned long serialhsk;
	unsigned long serialgpi;
	unsigned long printmf;
	unsigned long printhsk;
	unsigned long printgpi;
	unsigned long cpuid;
	unsigned long rombase;
	unsigned long adbdelay;
	unsigned long timedbra;
};

extern struct mac_booter_data
	mac_bi_data;

#endif


#define BI_APOLLO_MODEL         0x8000  


#define BI_HP300_MODEL		0x8000	
#define BI_HP300_UART_SCODE	0x8001	
#define BI_HP300_UART_ADDR	0x8002	


#define BOOTINFOV_MAGIC			0x4249561A	
#define MK_BI_VERSION(major,minor)	(((major)<<16)+(minor))
#define BI_VERSION_MAJOR(v)		(((v) >> 16) & 0xffff)
#define BI_VERSION_MINOR(v)		((v) & 0xffff)

#ifndef __ASSEMBLY__

struct bootversion {
    unsigned short branch;
    unsigned long magic;
    struct {
	unsigned long machtype;
	unsigned long version;
    } machversions[0];
};

#endif 

#define AMIGA_BOOTI_VERSION    MK_BI_VERSION( 2, 0 )
#define ATARI_BOOTI_VERSION    MK_BI_VERSION( 2, 1 )
#define MAC_BOOTI_VERSION      MK_BI_VERSION( 2, 0 )
#define MVME147_BOOTI_VERSION  MK_BI_VERSION( 2, 0 )
#define MVME16x_BOOTI_VERSION  MK_BI_VERSION( 2, 0 )
#define BVME6000_BOOTI_VERSION MK_BI_VERSION( 2, 0 )
#define Q40_BOOTI_VERSION      MK_BI_VERSION( 2, 0 )
#define HP300_BOOTI_VERSION    MK_BI_VERSION( 2, 0 )

#ifdef BOOTINFO_COMPAT_1_0


#define COMPAT_AMIGA_BOOTI_VERSION    MK_BI_VERSION( 1, 0 )
#define COMPAT_ATARI_BOOTI_VERSION    MK_BI_VERSION( 1, 0 )
#define COMPAT_MAC_BOOTI_VERSION      MK_BI_VERSION( 1, 0 )

#include <linux/zorro.h>

#define COMPAT_NUM_AUTO    16

struct compat_bi_Amiga {
    int model;
    int num_autocon;
    struct ConfigDev autocon[COMPAT_NUM_AUTO];
    unsigned long chip_size;
    unsigned char vblank;
    unsigned char psfreq;
    unsigned long eclock;
    unsigned long chipset;
    unsigned long hw_present;
};

struct compat_bi_Atari {
    unsigned long hw_present;
    unsigned long mch_cookie;
};

#ifndef __ASSEMBLY__

struct compat_bi_Macintosh
{
	unsigned long videoaddr;
	unsigned long videorow;
	unsigned long videodepth;
	unsigned long dimensions;
	unsigned long args;
	unsigned long boottime;
	unsigned long gmtbias;
	unsigned long bootver;
	unsigned long videological;
	unsigned long sccbase;
	unsigned long id;
	unsigned long memsize;
	unsigned long serialmf;
	unsigned long serialhsk;
	unsigned long serialgpi;
	unsigned long printmf;
	unsigned long printhsk;
	unsigned long printgpi;
	unsigned long cpuid;
	unsigned long rombase;
	unsigned long adbdelay;
	unsigned long timedbra;
};

#endif

struct compat_mem_info {
    unsigned long addr;
    unsigned long size;
};

#define COMPAT_NUM_MEMINFO  4

#define COMPAT_CPUB_68020 0
#define COMPAT_CPUB_68030 1
#define COMPAT_CPUB_68040 2
#define COMPAT_CPUB_68060 3
#define COMPAT_FPUB_68881 5
#define COMPAT_FPUB_68882 6
#define COMPAT_FPUB_68040 7
#define COMPAT_FPUB_68060 8

#define COMPAT_CPU_68020    (1<<COMPAT_CPUB_68020)
#define COMPAT_CPU_68030    (1<<COMPAT_CPUB_68030)
#define COMPAT_CPU_68040    (1<<COMPAT_CPUB_68040)
#define COMPAT_CPU_68060    (1<<COMPAT_CPUB_68060)
#define COMPAT_CPU_MASK     (31)
#define COMPAT_FPU_68881    (1<<COMPAT_FPUB_68881)
#define COMPAT_FPU_68882    (1<<COMPAT_FPUB_68882)
#define COMPAT_FPU_68040    (1<<COMPAT_FPUB_68040)
#define COMPAT_FPU_68060    (1<<COMPAT_FPUB_68060)
#define COMPAT_FPU_MASK     (0xfe0)

#define COMPAT_CL_SIZE      (256)

struct compat_bootinfo {
    unsigned long machtype;
    unsigned long cputype;
    struct compat_mem_info memory[COMPAT_NUM_MEMINFO];
    int num_memory;
    unsigned long ramdisk_size;
    unsigned long ramdisk_addr;
    char command_line[COMPAT_CL_SIZE];
    union {
	struct compat_bi_Amiga     bi_ami;
	struct compat_bi_Atari     bi_ata;
	struct compat_bi_Macintosh bi_mac;
    } bi_un;
};

#define bi_amiga	bi_un.bi_ami
#define bi_atari	bi_un.bi_ata
#define bi_mac		bi_un.bi_mac

#endif 


#endif 
