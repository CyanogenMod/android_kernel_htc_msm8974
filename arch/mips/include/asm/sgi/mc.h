/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * mc.h: Definitions for SGI Memory Controller
 *
 * Copyright (C) 1996 David S. Miller
 * Copyright (C) 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */

#ifndef _SGI_MC_H
#define _SGI_MC_H

struct sgimc_regs {
	u32 _unused0;
	volatile u32 cpuctrl0;	
#define SGIMC_CCTRL0_REFS	0x0000000f 
#define SGIMC_CCTRL0_EREFRESH	0x00000010 
#define SGIMC_CCTRL0_EPERRGIO	0x00000020 
#define SGIMC_CCTRL0_EPERRMEM	0x00000040 
#define SGIMC_CCTRL0_EPERRCPU	0x00000080 
#define SGIMC_CCTRL0_WDOG	0x00000100 
#define SGIMC_CCTRL0_SYSINIT	0x00000200 
#define SGIMC_CCTRL0_GFXRESET	0x00000400 
#define SGIMC_CCTRL0_EISALOCK	0x00000800 
#define SGIMC_CCTRL0_EPERRSCMD	0x00001000 
#define SGIMC_CCTRL0_IENAB	0x00002000 
#define SGIMC_CCTRL0_ESNOOP	0x00004000 
#define SGIMC_CCTRL0_EPROMWR	0x00008000 
#define SGIMC_CCTRL0_WRESETPMEM	0x00010000 
#define SGIMC_CCTRL0_LENDIAN	0x00020000 
#define SGIMC_CCTRL0_WRESETDMEM	0x00040000 
#define SGIMC_CCTRL0_CMEMBADPAR	0x02000000 
#define SGIMC_CCTRL0_R4KNOCHKPARR 0x04000000 
#define SGIMC_CCTRL0_GIOBTOB	0x08000000 
	u32 _unused1;
	volatile u32 cpuctrl1;	
#define SGIMC_CCTRL1_EGIOTIMEO	0x00000010 
#define SGIMC_CCTRL1_FIXEDEHPC	0x00001000 
#define SGIMC_CCTRL1_LITTLEHPC	0x00002000 
#define SGIMC_CCTRL1_FIXEDEEXP0	0x00004000 
#define SGIMC_CCTRL1_LITTLEEXP0	0x00008000 
#define SGIMC_CCTRL1_FIXEDEEXP1	0x00010000 
#define SGIMC_CCTRL1_LITTLEEXP1	0x00020000 

	u32 _unused2;
	volatile u32 watchdogt;	

	u32 _unused3;
	volatile u32 systemid;	
#define SGIMC_SYSID_MASKREV	0x0000000f 
#define SGIMC_SYSID_EPRESENT	0x00000010 

	u32 _unused4[3];
	volatile u32 divider;	

	u32 _unused5;
	u32 eeprom;		
#define SGIMC_EEPROM_PRE	0x00000001 
#define SGIMC_EEPROM_CSEL	0x00000002 
#define SGIMC_EEPROM_SECLOCK	0x00000004 
#define SGIMC_EEPROM_SDATAO	0x00000008 
#define SGIMC_EEPROM_SDATAI	0x00000010 

	u32 _unused6[3];
	volatile u32 rcntpre;	

	u32 _unused7;
	volatile u32 rcounter;	

	u32 _unused8[13];
	volatile u32 giopar;	
#define SGIMC_GIOPAR_HPC64	0x00000001 
#define SGIMC_GIOPAR_GFX64	0x00000002 
#define SGIMC_GIOPAR_EXP064	0x00000004 
#define SGIMC_GIOPAR_EXP164	0x00000008 
#define SGIMC_GIOPAR_EISA64	0x00000010 
#define SGIMC_GIOPAR_HPC264	0x00000020 
#define SGIMC_GIOPAR_RTIMEGFX	0x00000040 
#define SGIMC_GIOPAR_RTIMEEXP0	0x00000080 
#define SGIMC_GIOPAR_RTIMEEXP1	0x00000100 
#define SGIMC_GIOPAR_MASTEREISA	0x00000200 
#define SGIMC_GIOPAR_ONEBUS	0x00000400 
#define SGIMC_GIOPAR_MASTERGFX	0x00000800 
#define SGIMC_GIOPAR_MASTEREXP0	0x00001000 
#define SGIMC_GIOPAR_MASTEREXP1	0x00002000 
#define SGIMC_GIOPAR_PLINEEXP0	0x00004000 
#define SGIMC_GIOPAR_PLINEEXP1	0x00008000 

	u32 _unused9;
	volatile u32 cputp;	

	u32 _unused10[3];
	volatile u32 lbursttp;	

	u32 _unused11[9];
	volatile u32 mconfig0;	
	u32 _unused12;
	volatile u32 mconfig1;	
#define SGIMC_MCONFIG_BASEADDR	0x000000ff 
#define SGIMC_MCONFIG_RMASK	0x00001f00 
#define SGIMC_MCONFIG_BVALID	0x00002000 
#define SGIMC_MCONFIG_SBANKS	0x00004000 

	u32 _unused13;
	volatile u32 cmacc;        
	u32 _unused14;
	volatile u32 gmacc;        

	
#define SGIMC_MACC_ALIASBIG	0x20000000 

	
	u32 _unused15;
	volatile u32 cerr;	
	u32 _unused16;
	volatile u32 cstat;	
#define SGIMC_CSTAT_RD		0x00000100 
#define SGIMC_CSTAT_PAR		0x00000200 
#define SGIMC_CSTAT_ADDR	0x00000400 
#define SGIMC_CSTAT_SYSAD_PAR	0x00000800 
#define SGIMC_CSTAT_SYSCMD_PAR	0x00001000 
#define SGIMC_CSTAT_BAD_DATA	0x00002000 
#define SGIMC_CSTAT_PAR_MASK	0x00001f00 
#define SGIMC_CSTAT_RD_PAR	(SGIMC_CSTAT_RD | SGIMC_CSTAT_PAR)

	u32 _unused17;
	volatile u32 gerr;	
	u32 _unused18;
	volatile u32 gstat;	
#define SGIMC_GSTAT_RD		0x00000100 
#define SGIMC_GSTAT_WR		0x00000200 
#define SGIMC_GSTAT_TIME	0x00000400 
#define SGIMC_GSTAT_PROM	0x00000800 
#define SGIMC_GSTAT_ADDR	0x00001000 
#define SGIMC_GSTAT_BC		0x00002000 
#define SGIMC_GSTAT_PIO_RD	0x00004000 
#define SGIMC_GSTAT_PIO_WR	0x00008000 

	
	u32 _unused19;
	volatile u32 syssembit;		
	u32 _unused20;
	volatile u32 mlock;		
	u32 _unused21;
	volatile u32 elock;		

	
	u32 _unused22[15];
	volatile u32 gio_dma_trans;	
	u32 _unused23;
	volatile u32 gio_dma_sbits;	
	u32 _unused24;
	volatile u32 dma_intr_cause;	
	u32 _unused25;
	volatile u32 dma_ctrl;		

	
	u32 _unused26[5];
	volatile u32 dtlb_hi0;
	u32 _unused27;
	volatile u32 dtlb_lo0;

	
	u32 _unused28;
	volatile u32 dtlb_hi1;
	u32 _unused29;
	volatile u32 dtlb_lo1;

	
	u32 _unused30;
	volatile u32 dtlb_hi2;
	u32 _unused31;
	volatile u32 dtlb_lo2;

	
	u32 _unused32;
	volatile u32 dtlb_hi3;
	u32 _unused33;
	volatile u32 dtlb_lo3;

	u32 _unused34[0x0392];

	u32 _unused35;
	volatile u32 rpsscounter;	

	u32 _unused36[0x1000/4-2*4];

	u32 _unused37;
	volatile u32 maddronly;		
	u32 _unused38;
	volatile u32 maddrpdeflts;	
	u32 _unused39;
	volatile u32 dmasz;		
	u32 _unused40;
	volatile u32 ssize;		
	u32 _unused41;
	volatile u32 gmaddronly;	
	u32 _unused42;
	volatile u32 dmaddnpgo;		
	u32 _unused43;
	volatile u32 dmamode;		
	u32 _unused44;
	volatile u32 dmaccount;		
	u32 _unused45;
	volatile u32 dmastart;		
	u32 _unused46;
	volatile u32 dmarunning;	
	u32 _unused47;
	volatile u32 maddrdefstart;	
};

extern struct sgimc_regs *sgimc;
#define SGIMC_BASE		0x1fa00000	

#define SGIMC_SEG0_BADDR	0x08000000
#define SGIMC_SEG1_BADDR	0x20000000

#define SGIMC_SEG0_SIZE_ALL		0x10000000 
#define SGIMC_SEG1_SIZE_IP20_IP22	0x08000000 
#define SGIMC_SEG1_SIZE_IP26_IP28	0x20000000 

extern void sgimc_init(void);

#endif 
