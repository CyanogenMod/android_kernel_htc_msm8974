/*
 * tsi148.h
 *
 * Support for the Tundra TSI148 VME Bridge chip
 *
 * Author: Tom Armistead
 * Updated and maintained by Ajit Prem
 * Copyright 2004 Motorola Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef TSI148_H
#define TSI148_H

#ifndef	PCI_VENDOR_ID_TUNDRA
#define	PCI_VENDOR_ID_TUNDRA 0x10e3
#endif

#ifndef	PCI_DEVICE_ID_TUNDRA_TSI148
#define	PCI_DEVICE_ID_TUNDRA_TSI148 0x148
#endif

#define TSI148_MAX_MASTER		8	
#define TSI148_MAX_SLAVE		8	
#define TSI148_MAX_DMA			2	
#define TSI148_MAX_MAILBOX		4	
#define TSI148_MAX_SEMAPHORE		8	

struct tsi148_driver {
	void __iomem *base;	
	wait_queue_head_t dma_queue[2];
	wait_queue_head_t iack_queue;
	void (*lm_callback[4])(int);	
	void *crcsr_kernel;
	dma_addr_t crcsr_bus;
	struct vme_master_resource *flush_image;
	struct mutex vme_rmw;		
	struct mutex vme_int;		
};

struct tsi148_dma_descriptor {
	u32 dsau;      
	u32 dsal;
	u32 ddau;      
	u32 ddal;
	u32 dsat;      
	u32 ddat;      
	u32 dnlau;     
	u32 dnlal;
	u32 dcnt;      
	u32 ddbs;      
};

struct tsi148_dma_entry {
	struct tsi148_dma_descriptor descriptor;
	struct list_head list;
};



#define TSI148_PCFS_ID			0x0
#define TSI148_PCFS_CSR			0x4
#define TSI148_PCFS_CLASS		0x8
#define TSI148_PCFS_MISC0		0xC
#define TSI148_PCFS_MBARL		0x10
#define TSI148_PCFS_MBARU		0x14

#define TSI148_PCFS_SUBID		0x28

#define TSI148_PCFS_CAPP		0x34

#define TSI148_PCFS_MISC1		0x3C

#define TSI148_PCFS_XCAPP		0x40
#define TSI148_PCFS_XSTAT		0x44


#define TSI148_LCSR_OT0_OTSAU		0x100
#define TSI148_LCSR_OT0_OTSAL		0x104
#define TSI148_LCSR_OT0_OTEAU		0x108
#define TSI148_LCSR_OT0_OTEAL		0x10C
#define TSI148_LCSR_OT0_OTOFU		0x110
#define TSI148_LCSR_OT0_OTOFL		0x114
#define TSI148_LCSR_OT0_OTBS		0x118
#define TSI148_LCSR_OT0_OTAT		0x11C

#define TSI148_LCSR_OT1_OTSAU		0x120
#define TSI148_LCSR_OT1_OTSAL		0x124
#define TSI148_LCSR_OT1_OTEAU		0x128
#define TSI148_LCSR_OT1_OTEAL		0x12C
#define TSI148_LCSR_OT1_OTOFU		0x130
#define TSI148_LCSR_OT1_OTOFL		0x134
#define TSI148_LCSR_OT1_OTBS		0x138
#define TSI148_LCSR_OT1_OTAT		0x13C

#define TSI148_LCSR_OT2_OTSAU		0x140
#define TSI148_LCSR_OT2_OTSAL		0x144
#define TSI148_LCSR_OT2_OTEAU		0x148
#define TSI148_LCSR_OT2_OTEAL		0x14C
#define TSI148_LCSR_OT2_OTOFU		0x150
#define TSI148_LCSR_OT2_OTOFL		0x154
#define TSI148_LCSR_OT2_OTBS		0x158
#define TSI148_LCSR_OT2_OTAT		0x15C

#define TSI148_LCSR_OT3_OTSAU		0x160
#define TSI148_LCSR_OT3_OTSAL		0x164
#define TSI148_LCSR_OT3_OTEAU		0x168
#define TSI148_LCSR_OT3_OTEAL		0x16C
#define TSI148_LCSR_OT3_OTOFU		0x170
#define TSI148_LCSR_OT3_OTOFL		0x174
#define TSI148_LCSR_OT3_OTBS		0x178
#define TSI148_LCSR_OT3_OTAT		0x17C

#define TSI148_LCSR_OT4_OTSAU		0x180
#define TSI148_LCSR_OT4_OTSAL		0x184
#define TSI148_LCSR_OT4_OTEAU		0x188
#define TSI148_LCSR_OT4_OTEAL		0x18C
#define TSI148_LCSR_OT4_OTOFU		0x190
#define TSI148_LCSR_OT4_OTOFL		0x194
#define TSI148_LCSR_OT4_OTBS		0x198
#define TSI148_LCSR_OT4_OTAT		0x19C

#define TSI148_LCSR_OT5_OTSAU		0x1A0
#define TSI148_LCSR_OT5_OTSAL		0x1A4
#define TSI148_LCSR_OT5_OTEAU		0x1A8
#define TSI148_LCSR_OT5_OTEAL		0x1AC
#define TSI148_LCSR_OT5_OTOFU		0x1B0
#define TSI148_LCSR_OT5_OTOFL		0x1B4
#define TSI148_LCSR_OT5_OTBS		0x1B8
#define TSI148_LCSR_OT5_OTAT		0x1BC

#define TSI148_LCSR_OT6_OTSAU		0x1C0
#define TSI148_LCSR_OT6_OTSAL		0x1C4
#define TSI148_LCSR_OT6_OTEAU		0x1C8
#define TSI148_LCSR_OT6_OTEAL		0x1CC
#define TSI148_LCSR_OT6_OTOFU		0x1D0
#define TSI148_LCSR_OT6_OTOFL		0x1D4
#define TSI148_LCSR_OT6_OTBS		0x1D8
#define TSI148_LCSR_OT6_OTAT		0x1DC

#define TSI148_LCSR_OT7_OTSAU		0x1E0
#define TSI148_LCSR_OT7_OTSAL		0x1E4
#define TSI148_LCSR_OT7_OTEAU		0x1E8
#define TSI148_LCSR_OT7_OTEAL		0x1EC
#define TSI148_LCSR_OT7_OTOFU		0x1F0
#define TSI148_LCSR_OT7_OTOFL		0x1F4
#define TSI148_LCSR_OT7_OTBS		0x1F8
#define TSI148_LCSR_OT7_OTAT		0x1FC

#define TSI148_LCSR_OT0		0x100
#define TSI148_LCSR_OT1		0x120
#define TSI148_LCSR_OT2		0x140
#define TSI148_LCSR_OT3		0x160
#define TSI148_LCSR_OT4		0x180
#define TSI148_LCSR_OT5		0x1A0
#define TSI148_LCSR_OT6		0x1C0
#define TSI148_LCSR_OT7		0x1E0

static const int TSI148_LCSR_OT[8] = { TSI148_LCSR_OT0, TSI148_LCSR_OT1,
					 TSI148_LCSR_OT2, TSI148_LCSR_OT3,
					 TSI148_LCSR_OT4, TSI148_LCSR_OT5,
					 TSI148_LCSR_OT6, TSI148_LCSR_OT7 };

#define TSI148_LCSR_OFFSET_OTSAU	0x0
#define TSI148_LCSR_OFFSET_OTSAL	0x4
#define TSI148_LCSR_OFFSET_OTEAU	0x8
#define TSI148_LCSR_OFFSET_OTEAL	0xC
#define TSI148_LCSR_OFFSET_OTOFU	0x10
#define TSI148_LCSR_OFFSET_OTOFL	0x14
#define TSI148_LCSR_OFFSET_OTBS		0x18
#define TSI148_LCSR_OFFSET_OTAT		0x1C

#define TSI148_LCSR_VIACK1	0x204
#define TSI148_LCSR_VIACK2	0x208
#define TSI148_LCSR_VIACK3	0x20C
#define TSI148_LCSR_VIACK4	0x210
#define TSI148_LCSR_VIACK5	0x214
#define TSI148_LCSR_VIACK6	0x218
#define TSI148_LCSR_VIACK7	0x21C

static const int TSI148_LCSR_VIACK[8] = { 0, TSI148_LCSR_VIACK1,
				TSI148_LCSR_VIACK2, TSI148_LCSR_VIACK3,
				TSI148_LCSR_VIACK4, TSI148_LCSR_VIACK5,
				TSI148_LCSR_VIACK6, TSI148_LCSR_VIACK7 };

#define TSI148_LCSR_RMWAU	0x220
#define TSI148_LCSR_RMWAL	0x224
#define TSI148_LCSR_RMWEN	0x228
#define TSI148_LCSR_RMWC	0x22C
#define TSI148_LCSR_RMWS	0x230

#define TSI148_LCSR_VMCTRL	0x234
#define TSI148_LCSR_VCTRL	0x238
#define TSI148_LCSR_VSTAT	0x23C

#define TSI148_LCSR_PSTAT	0x240

#define TSI148_LCSR_VMEFL	0x250

#define TSI148_LCSR_VEAU	0x260
#define TSI148_LCSR_VEAL	0x264
#define TSI148_LCSR_VEAT	0x268

#define TSI148_LCSR_EDPAU	0x270
#define TSI148_LCSR_EDPAL	0x274
#define TSI148_LCSR_EDPXA	0x278
#define TSI148_LCSR_EDPXS	0x27C
#define TSI148_LCSR_EDPAT	0x280

#define TSI148_LCSR_IT0_ITSAU		0x300
#define TSI148_LCSR_IT0_ITSAL		0x304
#define TSI148_LCSR_IT0_ITEAU		0x308
#define TSI148_LCSR_IT0_ITEAL		0x30C
#define TSI148_LCSR_IT0_ITOFU		0x310
#define TSI148_LCSR_IT0_ITOFL		0x314
#define TSI148_LCSR_IT0_ITAT		0x318

#define TSI148_LCSR_IT1_ITSAU		0x320
#define TSI148_LCSR_IT1_ITSAL		0x324
#define TSI148_LCSR_IT1_ITEAU		0x328
#define TSI148_LCSR_IT1_ITEAL		0x32C
#define TSI148_LCSR_IT1_ITOFU		0x330
#define TSI148_LCSR_IT1_ITOFL		0x334
#define TSI148_LCSR_IT1_ITAT		0x338

#define TSI148_LCSR_IT2_ITSAU		0x340
#define TSI148_LCSR_IT2_ITSAL		0x344
#define TSI148_LCSR_IT2_ITEAU		0x348
#define TSI148_LCSR_IT2_ITEAL		0x34C
#define TSI148_LCSR_IT2_ITOFU		0x350
#define TSI148_LCSR_IT2_ITOFL		0x354
#define TSI148_LCSR_IT2_ITAT		0x358

#define TSI148_LCSR_IT3_ITSAU		0x360
#define TSI148_LCSR_IT3_ITSAL		0x364
#define TSI148_LCSR_IT3_ITEAU		0x368
#define TSI148_LCSR_IT3_ITEAL		0x36C
#define TSI148_LCSR_IT3_ITOFU		0x370
#define TSI148_LCSR_IT3_ITOFL		0x374
#define TSI148_LCSR_IT3_ITAT		0x378

#define TSI148_LCSR_IT4_ITSAU		0x380
#define TSI148_LCSR_IT4_ITSAL		0x384
#define TSI148_LCSR_IT4_ITEAU		0x388
#define TSI148_LCSR_IT4_ITEAL		0x38C
#define TSI148_LCSR_IT4_ITOFU		0x390
#define TSI148_LCSR_IT4_ITOFL		0x394
#define TSI148_LCSR_IT4_ITAT		0x398

#define TSI148_LCSR_IT5_ITSAU		0x3A0
#define TSI148_LCSR_IT5_ITSAL		0x3A4
#define TSI148_LCSR_IT5_ITEAU		0x3A8
#define TSI148_LCSR_IT5_ITEAL		0x3AC
#define TSI148_LCSR_IT5_ITOFU		0x3B0
#define TSI148_LCSR_IT5_ITOFL		0x3B4
#define TSI148_LCSR_IT5_ITAT		0x3B8

#define TSI148_LCSR_IT6_ITSAU		0x3C0
#define TSI148_LCSR_IT6_ITSAL		0x3C4
#define TSI148_LCSR_IT6_ITEAU		0x3C8
#define TSI148_LCSR_IT6_ITEAL		0x3CC
#define TSI148_LCSR_IT6_ITOFU		0x3D0
#define TSI148_LCSR_IT6_ITOFL		0x3D4
#define TSI148_LCSR_IT6_ITAT		0x3D8

#define TSI148_LCSR_IT7_ITSAU		0x3E0
#define TSI148_LCSR_IT7_ITSAL		0x3E4
#define TSI148_LCSR_IT7_ITEAU		0x3E8
#define TSI148_LCSR_IT7_ITEAL		0x3EC
#define TSI148_LCSR_IT7_ITOFU		0x3F0
#define TSI148_LCSR_IT7_ITOFL		0x3F4
#define TSI148_LCSR_IT7_ITAT		0x3F8


#define TSI148_LCSR_IT0		0x300
#define TSI148_LCSR_IT1		0x320
#define TSI148_LCSR_IT2		0x340
#define TSI148_LCSR_IT3		0x360
#define TSI148_LCSR_IT4		0x380
#define TSI148_LCSR_IT5		0x3A0
#define TSI148_LCSR_IT6		0x3C0
#define TSI148_LCSR_IT7		0x3E0

static const int TSI148_LCSR_IT[8] = { TSI148_LCSR_IT0, TSI148_LCSR_IT1,
					 TSI148_LCSR_IT2, TSI148_LCSR_IT3,
					 TSI148_LCSR_IT4, TSI148_LCSR_IT5,
					 TSI148_LCSR_IT6, TSI148_LCSR_IT7 };

#define TSI148_LCSR_OFFSET_ITSAU	0x0
#define TSI148_LCSR_OFFSET_ITSAL	0x4
#define TSI148_LCSR_OFFSET_ITEAU	0x8
#define TSI148_LCSR_OFFSET_ITEAL	0xC
#define TSI148_LCSR_OFFSET_ITOFU	0x10
#define TSI148_LCSR_OFFSET_ITOFL	0x14
#define TSI148_LCSR_OFFSET_ITAT		0x18

#define TSI148_LCSR_GBAU	0x400
#define TSI148_LCSR_GBAL	0x404
#define TSI148_LCSR_GCSRAT	0x408

#define TSI148_LCSR_CBAU	0x40C
#define TSI148_LCSR_CBAL	0x410
#define TSI148_LCSR_CSRAT	0x414

#define TSI148_LCSR_CROU	0x418
#define TSI148_LCSR_CROL	0x41C
#define TSI148_LCSR_CRAT	0x420

#define TSI148_LCSR_LMBAU	0x424
#define TSI148_LCSR_LMBAL	0x428
#define TSI148_LCSR_LMAT	0x42C

#define TSI148_LCSR_BCU		0x430
#define TSI148_LCSR_BCL		0x434
#define TSI148_LCSR_BPGTR	0x438
#define TSI148_LCSR_BPCTR	0x43C
#define TSI148_LCSR_VICR	0x440

#define TSI148_LCSR_INTEN	0x448
#define TSI148_LCSR_INTEO	0x44C
#define TSI148_LCSR_INTS	0x450
#define TSI148_LCSR_INTC	0x454
#define TSI148_LCSR_INTM1	0x458
#define TSI148_LCSR_INTM2	0x45C

#define TSI148_LCSR_DCTL0	0x500
#define TSI148_LCSR_DSTA0	0x504
#define TSI148_LCSR_DCSAU0	0x508
#define TSI148_LCSR_DCSAL0	0x50C
#define TSI148_LCSR_DCDAU0	0x510
#define TSI148_LCSR_DCDAL0	0x514
#define TSI148_LCSR_DCLAU0	0x518
#define TSI148_LCSR_DCLAL0	0x51C
#define TSI148_LCSR_DSAU0	0x520
#define TSI148_LCSR_DSAL0	0x524
#define TSI148_LCSR_DDAU0	0x528
#define TSI148_LCSR_DDAL0	0x52C
#define TSI148_LCSR_DSAT0	0x530
#define TSI148_LCSR_DDAT0	0x534
#define TSI148_LCSR_DNLAU0	0x538
#define TSI148_LCSR_DNLAL0	0x53C
#define TSI148_LCSR_DCNT0	0x540
#define TSI148_LCSR_DDBS0	0x544

#define TSI148_LCSR_DCTL1	0x580
#define TSI148_LCSR_DSTA1	0x584
#define TSI148_LCSR_DCSAU1	0x588
#define TSI148_LCSR_DCSAL1	0x58C
#define TSI148_LCSR_DCDAU1	0x590
#define TSI148_LCSR_DCDAL1	0x594
#define TSI148_LCSR_DCLAU1	0x598
#define TSI148_LCSR_DCLAL1	0x59C
#define TSI148_LCSR_DSAU1	0x5A0
#define TSI148_LCSR_DSAL1	0x5A4
#define TSI148_LCSR_DDAU1	0x5A8
#define TSI148_LCSR_DDAL1	0x5AC
#define TSI148_LCSR_DSAT1	0x5B0
#define TSI148_LCSR_DDAT1	0x5B4
#define TSI148_LCSR_DNLAU1	0x5B8
#define TSI148_LCSR_DNLAL1	0x5BC
#define TSI148_LCSR_DCNT1	0x5C0
#define TSI148_LCSR_DDBS1	0x5C4

#define TSI148_LCSR_DMA0	0x500
#define TSI148_LCSR_DMA1	0x580


static const int TSI148_LCSR_DMA[TSI148_MAX_DMA] = { TSI148_LCSR_DMA0,
						TSI148_LCSR_DMA1 };

#define TSI148_LCSR_OFFSET_DCTL		0x0
#define TSI148_LCSR_OFFSET_DSTA		0x4
#define TSI148_LCSR_OFFSET_DCSAU	0x8
#define TSI148_LCSR_OFFSET_DCSAL	0xC
#define TSI148_LCSR_OFFSET_DCDAU	0x10
#define TSI148_LCSR_OFFSET_DCDAL	0x14
#define TSI148_LCSR_OFFSET_DCLAU	0x18
#define TSI148_LCSR_OFFSET_DCLAL	0x1C
#define TSI148_LCSR_OFFSET_DSAU		0x20
#define TSI148_LCSR_OFFSET_DSAL		0x24
#define TSI148_LCSR_OFFSET_DDAU		0x28
#define TSI148_LCSR_OFFSET_DDAL		0x2C
#define TSI148_LCSR_OFFSET_DSAT		0x30
#define TSI148_LCSR_OFFSET_DDAT		0x34
#define TSI148_LCSR_OFFSET_DNLAU	0x38
#define TSI148_LCSR_OFFSET_DNLAL	0x3C
#define TSI148_LCSR_OFFSET_DCNT		0x40
#define TSI148_LCSR_OFFSET_DDBS		0x44


#define TSI148_GCSR_ID		0x600
#define TSI148_GCSR_CSR		0x604
#define TSI148_GCSR_SEMA0	0x608
#define TSI148_GCSR_SEMA1	0x60C

#define TSI148_GCSR_MBOX0	0x610
#define TSI148_GCSR_MBOX1	0x614
#define TSI148_GCSR_MBOX2	0x618
#define TSI148_GCSR_MBOX3	0x61C

static const int TSI148_GCSR_MBOX[4] = { TSI148_GCSR_MBOX0,
					TSI148_GCSR_MBOX1,
					TSI148_GCSR_MBOX2,
					TSI148_GCSR_MBOX3 };


#define TSI148_CSRBCR	0xFF4
#define TSI148_CSRBSR	0xFF8
#define TSI148_CBAR	0xFFC





#define TSI148_PCFS_CMMD_SERR          (1<<8)	
#define TSI148_PCFS_CMMD_PERR          (1<<6)	
#define TSI148_PCFS_CMMD_MSTR          (1<<2)	
#define TSI148_PCFS_CMMD_MEMSP         (1<<1)	
#define TSI148_PCFS_CMMD_IOSP          (1<<0)	

#define TSI148_PCFS_STAT_RCPVE         (1<<15)	
#define TSI148_PCFS_STAT_SIGSE         (1<<14)	
#define TSI148_PCFS_STAT_RCVMA         (1<<13)	
#define TSI148_PCFS_STAT_RCVTA         (1<<12)	
#define TSI148_PCFS_STAT_SIGTA         (1<<11)	
#define TSI148_PCFS_STAT_SELTIM        (3<<9)	
#define TSI148_PCFS_STAT_DPAR          (1<<8)	
#define TSI148_PCFS_STAT_FAST          (1<<7)	
#define TSI148_PCFS_STAT_P66M          (1<<5)	
#define TSI148_PCFS_STAT_CAPL          (1<<4)	

#define TSI148_PCFS_CLAS_M             (0xFF<<24)	
#define TSI148_PCFS_SUBCLAS_M          (0xFF<<16)	
#define TSI148_PCFS_PROGIF_M           (0xFF<<8)	
#define TSI148_PCFS_REVID_M            (0xFF<<0)	

#define TSI148_PCFS_HEAD_M             (0xFF<<16)	
#define TSI148_PCFS_MLAT_M             (0xFF<<8)	
#define TSI148_PCFS_CLSZ_M             (0xFF<<0)	

#define TSI148_PCFS_MBARL_BASEL_M      (0xFFFFF<<12) 
#define TSI148_PCFS_MBARL_PRE          (1<<3)	
#define TSI148_PCFS_MBARL_MTYPE_M      (3<<1)	
#define TSI148_PCFS_MBARL_IOMEM        (1<<0)	

#define TSI148_PCFS_MSICAP_64BAC       (1<<7)	
#define TSI148_PCFS_MSICAP_MME_M       (7<<4)	
#define TSI148_PCFS_MSICAP_MMC_M       (7<<1)	
#define TSI148_PCFS_MSICAP_MSIEN       (1<<0)	

#define TSI148_PCFS_MSIAL_M            (0x3FFFFFFF<<2)	

#define TSI148_PCFS_MSIMD_M            (0xFFFF<<0)	

#define TSI148_PCFS_PCIXCAP_MOST_M     (7<<4)	
#define TSI148_PCFS_PCIXCAP_MMRBC_M    (3<<2)	
#define TSI148_PCFS_PCIXCAP_ERO        (1<<1)	
#define TSI148_PCFS_PCIXCAP_DPERE      (1<<0)	

#define TSI148_PCFS_PCIXSTAT_RSCEM     (1<<29)	
#define TSI148_PCFS_PCIXSTAT_DMCRS_M   (7<<26)	
#define TSI148_PCFS_PCIXSTAT_DMOST_M   (7<<23)	
#define TSI148_PCFS_PCIXSTAT_DMMRC_M   (3<<21)	
#define TSI148_PCFS_PCIXSTAT_DC        (1<<20)	
#define TSI148_PCFS_PCIXSTAT_USC       (1<<19)	
#define TSI148_PCFS_PCIXSTAT_SCD       (1<<18)	
#define TSI148_PCFS_PCIXSTAT_133C      (1<<17)	
#define TSI148_PCFS_PCIXSTAT_64D       (1<<16)	
#define TSI148_PCFS_PCIXSTAT_BN_M      (0xFF<<8)	
#define TSI148_PCFS_PCIXSTAT_DN_M      (0x1F<<3)	
#define TSI148_PCFS_PCIXSTAT_FN_M      (7<<0)	


#define TSI148_LCSR_OTSAL_M            (0xFFFF<<16)	

#define TSI148_LCSR_OTEAL_M            (0xFFFF<<16)	

#define TSI148_LCSR_OTOFFL_M           (0xFFFF<<16)	

#define TSI148_LCSR_OTBS_M             (0xFFFFF<<0)	

#define TSI148_LCSR_OTAT_EN            (1<<31)	
#define TSI148_LCSR_OTAT_MRPFD         (1<<18)	

#define TSI148_LCSR_OTAT_PFS_M         (3<<16)	
#define TSI148_LCSR_OTAT_PFS_2         (0<<16)	
#define TSI148_LCSR_OTAT_PFS_4         (1<<16)	
#define TSI148_LCSR_OTAT_PFS_8         (2<<16)	
#define TSI148_LCSR_OTAT_PFS_16        (3<<16)	

#define TSI148_LCSR_OTAT_2eSSTM_M      (7<<11)	
#define TSI148_LCSR_OTAT_2eSSTM_160    (0<<11)	
#define TSI148_LCSR_OTAT_2eSSTM_267    (1<<11)	
#define TSI148_LCSR_OTAT_2eSSTM_320    (2<<11)	

#define TSI148_LCSR_OTAT_TM_M          (7<<8)	
#define TSI148_LCSR_OTAT_TM_SCT        (0<<8)	
#define TSI148_LCSR_OTAT_TM_BLT        (1<<8)	
#define TSI148_LCSR_OTAT_TM_MBLT       (2<<8)	
#define TSI148_LCSR_OTAT_TM_2eVME      (3<<8)	
#define TSI148_LCSR_OTAT_TM_2eSST      (4<<8)	
#define TSI148_LCSR_OTAT_TM_2eSSTB     (5<<8)	

#define TSI148_LCSR_OTAT_DBW_M         (3<<6)	
#define TSI148_LCSR_OTAT_DBW_16        (0<<6)	
#define TSI148_LCSR_OTAT_DBW_32        (1<<6)	

#define TSI148_LCSR_OTAT_SUP           (1<<5)	
#define TSI148_LCSR_OTAT_PGM           (1<<4)	

#define TSI148_LCSR_OTAT_AMODE_M       (0xf<<0)	
#define TSI148_LCSR_OTAT_AMODE_A16     (0<<0)	
#define TSI148_LCSR_OTAT_AMODE_A24     (1<<0)	
#define TSI148_LCSR_OTAT_AMODE_A32     (2<<0)	
#define TSI148_LCSR_OTAT_AMODE_A64     (4<<0)	
#define TSI148_LCSR_OTAT_AMODE_CRCSR   (5<<0)	
#define TSI148_LCSR_OTAT_AMODE_USER1   (8<<0)	
#define TSI148_LCSR_OTAT_AMODE_USER2   (9<<0)	
#define TSI148_LCSR_OTAT_AMODE_USER3   (10<<0)	
#define TSI148_LCSR_OTAT_AMODE_USER4   (11<<0)	

#define TSI148_LCSR_VMCTRL_VSA         (1<<27)	
#define TSI148_LCSR_VMCTRL_VS          (1<<26)	
#define TSI148_LCSR_VMCTRL_DHB         (1<<25)	
#define TSI148_LCSR_VMCTRL_DWB         (1<<24)	

#define TSI148_LCSR_VMCTRL_RMWEN       (1<<20)	

#define TSI148_LCSR_VMCTRL_ATO_M       (7<<16)	
#define TSI148_LCSR_VMCTRL_ATO_32      (0<<16)	
#define TSI148_LCSR_VMCTRL_ATO_128     (1<<16)	
#define TSI148_LCSR_VMCTRL_ATO_512     (2<<16)	
#define TSI148_LCSR_VMCTRL_ATO_2M      (3<<16)	
#define TSI148_LCSR_VMCTRL_ATO_8M      (4<<16)	
#define TSI148_LCSR_VMCTRL_ATO_32M     (5<<16)	
#define TSI148_LCSR_VMCTRL_ATO_128M    (6<<16)	
#define TSI148_LCSR_VMCTRL_ATO_DIS     (7<<16)	

#define TSI148_LCSR_VMCTRL_VTOFF_M     (7<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_0     (0<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_1     (1<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_2     (2<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_4     (3<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_8     (4<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_16    (5<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_32    (6<<12)	
#define TSI148_LCSR_VMCTRL_VTOFF_64    (7<<12)	

#define TSI148_LCSR_VMCTRL_VTON_M      (7<<8)	
#define TSI148_LCSR_VMCTRL_VTON_4      (0<<8)	
#define TSI148_LCSR_VMCTRL_VTON_8      (1<<8)	
#define TSI148_LCSR_VMCTRL_VTON_16     (2<<8)	
#define TSI148_LCSR_VMCTRL_VTON_32     (3<<8)	
#define TSI148_LCSR_VMCTRL_VTON_64     (4<<8)	
#define TSI148_LCSR_VMCTRL_VTON_128    (5<<8)	
#define TSI148_LCSR_VMCTRL_VTON_256    (6<<8)	
#define TSI148_LCSR_VMCTRL_VTON_512    (7<<8)	

#define TSI148_LCSR_VMCTRL_VREL_M      (3<<3)	
#define TSI148_LCSR_VMCTRL_VREL_T_D    (0<<3)	
#define TSI148_LCSR_VMCTRL_VREL_T_R_D  (1<<3)	
#define TSI148_LCSR_VMCTRL_VREL_T_B_D  (2<<3)	
#define TSI148_LCSR_VMCTRL_VREL_T_D_R  (3<<3)	

#define TSI148_LCSR_VMCTRL_VFAIR       (1<<2)	
#define TSI148_LCSR_VMCTRL_VREQL_M     (3<<0)	

#define TSI148_LCSR_VCTRL_LRE          (1<<31)	

#define TSI148_LCSR_VCTRL_DLT_M        (0xF<<24)	
#define TSI148_LCSR_VCTRL_DLT_OFF      (0<<24)	
#define TSI148_LCSR_VCTRL_DLT_16       (1<<24)	
#define TSI148_LCSR_VCTRL_DLT_32       (2<<24)	
#define TSI148_LCSR_VCTRL_DLT_64       (3<<24)	
#define TSI148_LCSR_VCTRL_DLT_128      (4<<24)	
#define TSI148_LCSR_VCTRL_DLT_256      (5<<24)	
#define TSI148_LCSR_VCTRL_DLT_512      (6<<24)	
#define TSI148_LCSR_VCTRL_DLT_1024     (7<<24)	
#define TSI148_LCSR_VCTRL_DLT_2048     (8<<24)	
#define TSI148_LCSR_VCTRL_DLT_4096     (9<<24)	
#define TSI148_LCSR_VCTRL_DLT_8192     (0xA<<24)	
#define TSI148_LCSR_VCTRL_DLT_16384    (0xB<<24)	
#define TSI148_LCSR_VCTRL_DLT_32768    (0xC<<24)	

#define TSI148_LCSR_VCTRL_NERBB        (1<<20)	

#define TSI148_LCSR_VCTRL_SRESET       (1<<17)	
#define TSI148_LCSR_VCTRL_LRESET       (1<<16)	

#define TSI148_LCSR_VCTRL_SFAILAI      (1<<15)	
#define TSI148_LCSR_VCTRL_BID_M        (0x1F<<8)	

#define TSI148_LCSR_VCTRL_ATOEN        (1<<7)	
#define TSI148_LCSR_VCTRL_ROBIN        (1<<6)	

#define TSI148_LCSR_VCTRL_GTO_M        (7<<0)	
#define TSI148_LCSR_VCTRL_GTO_8	      (0<<0)	
#define TSI148_LCSR_VCTRL_GTO_16	      (1<<0)	
#define TSI148_LCSR_VCTRL_GTO_32	      (2<<0)	
#define TSI148_LCSR_VCTRL_GTO_64	      (3<<0)	
#define TSI148_LCSR_VCTRL_GTO_128      (4<<0)	
#define TSI148_LCSR_VCTRL_GTO_256      (5<<0)	
#define TSI148_LCSR_VCTRL_GTO_512      (6<<0)	
#define TSI148_LCSR_VCTRL_GTO_DIS      (7<<0)	

#define TSI148_LCSR_VSTAT_CPURST       (1<<15)	
#define TSI148_LCSR_VSTAT_BRDFL        (1<<14)	
#define TSI148_LCSR_VSTAT_PURSTS       (1<<12)	
#define TSI148_LCSR_VSTAT_BDFAILS      (1<<11)	
#define TSI148_LCSR_VSTAT_SYSFAILS     (1<<10)	
#define TSI148_LCSR_VSTAT_ACFAILS      (1<<9)	
#define TSI148_LCSR_VSTAT_SCONS        (1<<8)	
#define TSI148_LCSR_VSTAT_GAP          (1<<5)	
#define TSI148_LCSR_VSTAT_GA_M         (0x1F<<0)  

#define TSI148_LCSR_PSTAT_REQ64S       (1<<6)	
#define TSI148_LCSR_PSTAT_M66ENS       (1<<5)	
#define TSI148_LCSR_PSTAT_FRAMES       (1<<4)	
#define TSI148_LCSR_PSTAT_IRDYS        (1<<3)	
#define TSI148_LCSR_PSTAT_DEVSELS      (1<<2)	
#define TSI148_LCSR_PSTAT_STOPS        (1<<1)	
#define TSI148_LCSR_PSTAT_TRDYS        (1<<0)	

#define TSI148_LCSR_VEAT_VES           (1<<31)	
#define TSI148_LCSR_VEAT_VEOF          (1<<30)	
#define TSI148_LCSR_VEAT_VESCL         (1<<29)	
#define TSI148_LCSR_VEAT_2EOT          (1<<21)	
#define TSI148_LCSR_VEAT_2EST          (1<<20)	
#define TSI148_LCSR_VEAT_BERR          (1<<19)	
#define TSI148_LCSR_VEAT_LWORD         (1<<18)	
#define TSI148_LCSR_VEAT_WRITE         (1<<17)	
#define TSI148_LCSR_VEAT_IACK          (1<<16)	
#define TSI148_LCSR_VEAT_DS1           (1<<15)	
#define TSI148_LCSR_VEAT_DS0           (1<<14)	
#define TSI148_LCSR_VEAT_AM_M          (0x3F<<8)	
#define TSI148_LCSR_VEAT_XAM_M         (0xFF<<0)	


#define TSI148_LCSR_EDPAT_EDPCL        (1<<29)

#define TSI148_LCSR_ITSAL6432_M        (0xFFFF<<16)	
#define TSI148_LCSR_ITSAL24_M          (0x00FFF<<12)	
#define TSI148_LCSR_ITSAL16_M          (0x0000FFF<<4)	

#define TSI148_LCSR_ITEAL6432_M        (0xFFFF<<16)	
#define TSI148_LCSR_ITEAL24_M          (0x00FFF<<12)	
#define TSI148_LCSR_ITEAL16_M          (0x0000FFF<<4)	

#define TSI148_LCSR_ITOFFL6432_M       (0xFFFF<<16)	
#define TSI148_LCSR_ITOFFL24_M         (0xFFFFF<<12)	
#define TSI148_LCSR_ITOFFL16_M         (0xFFFFFFF<<4)	

#define TSI148_LCSR_ITAT_EN            (1<<31)	
#define TSI148_LCSR_ITAT_TH            (1<<18)	

#define TSI148_LCSR_ITAT_VFS_M         (3<<16)	
#define TSI148_LCSR_ITAT_VFS_64        (0<<16)	
#define TSI148_LCSR_ITAT_VFS_128       (1<<16)	
#define TSI148_LCSR_ITAT_VFS_256       (2<<16)	
#define TSI148_LCSR_ITAT_VFS_512       (3<<16)	

#define TSI148_LCSR_ITAT_2eSSTM_M      (7<<12)	
#define TSI148_LCSR_ITAT_2eSSTM_160    (0<<12)	
#define TSI148_LCSR_ITAT_2eSSTM_267    (1<<12)	
#define TSI148_LCSR_ITAT_2eSSTM_320    (2<<12)	

#define TSI148_LCSR_ITAT_2eSSTB        (1<<11)	
#define TSI148_LCSR_ITAT_2eSST         (1<<10)	
#define TSI148_LCSR_ITAT_2eVME         (1<<9)	
#define TSI148_LCSR_ITAT_MBLT          (1<<8)	
#define TSI148_LCSR_ITAT_BLT           (1<<7)	

#define TSI148_LCSR_ITAT_AS_M          (7<<4)	
#define TSI148_LCSR_ITAT_AS_A16        (0<<4)	
#define TSI148_LCSR_ITAT_AS_A24        (1<<4)	
#define TSI148_LCSR_ITAT_AS_A32        (2<<4)	
#define TSI148_LCSR_ITAT_AS_A64        (4<<4)	

#define TSI148_LCSR_ITAT_SUPR          (1<<3)	
#define TSI148_LCSR_ITAT_NPRIV         (1<<2)	
#define TSI148_LCSR_ITAT_PGM           (1<<1)	
#define TSI148_LCSR_ITAT_DATA          (1<<0)	

#define TSI148_LCSR_GBAL_M             (0x7FFFFFF<<5)	

#define TSI148_LCSR_GCSRAT_EN          (1<<7)	

#define TSI148_LCSR_GCSRAT_AS_M        (7<<4)	
#define TSI148_LCSR_GCSRAT_AS_A16       (0<<4)	
#define TSI148_LCSR_GCSRAT_AS_A24       (1<<4)	
#define TSI148_LCSR_GCSRAT_AS_A32       (2<<4)	
#define TSI148_LCSR_GCSRAT_AS_A64       (4<<4)	

#define TSI148_LCSR_GCSRAT_SUPR        (1<<3)	
#define TSI148_LCSR_GCSRAT_NPRIV       (1<<2)	
#define TSI148_LCSR_GCSRAT_PGM         (1<<1)	
#define TSI148_LCSR_GCSRAT_DATA        (1<<0)	

#define TSI148_LCSR_CBAL_M             (0xFFFFF<<12)

#define TSI148_LCSR_CRGAT_EN           (1<<7)	

#define TSI148_LCSR_CRGAT_AS_M         (7<<4)	
#define TSI148_LCSR_CRGAT_AS_A16       (0<<4)	
#define TSI148_LCSR_CRGAT_AS_A24       (1<<4)	
#define TSI148_LCSR_CRGAT_AS_A32       (2<<4)	
#define TSI148_LCSR_CRGAT_AS_A64       (4<<4)	

#define TSI148_LCSR_CRGAT_SUPR         (1<<3)	
#define TSI148_LCSR_CRGAT_NPRIV        (1<<2)	
#define TSI148_LCSR_CRGAT_PGM          (1<<1)	
#define TSI148_LCSR_CRGAT_DATA         (1<<0)	

#define TSI148_LCSR_CROL_M             (0x1FFF<<19)	

#define TSI148_LCSR_CRAT_EN            (1<<7)	

#define TSI148_LCSR_LMBAL_M            (0x7FFFFFF<<5)	

#define TSI148_LCSR_LMAT_EN            (1<<7)	

#define TSI148_LCSR_LMAT_AS_M          (7<<4)	
#define TSI148_LCSR_LMAT_AS_A16        (0<<4)	
#define TSI148_LCSR_LMAT_AS_A24        (1<<4)	
#define TSI148_LCSR_LMAT_AS_A32        (2<<4)	
#define TSI148_LCSR_LMAT_AS_A64        (4<<4)	

#define TSI148_LCSR_LMAT_SUPR          (1<<3)	
#define TSI148_LCSR_LMAT_NPRIV         (1<<2)	
#define TSI148_LCSR_LMAT_PGM           (1<<1)	
#define TSI148_LCSR_LMAT_DATA          (1<<0)	

#define TSI148_LCSR_BPGTR_BPGT_M       (0xFFFF<<0)	

#define TSI148_LCSR_BPCTR_BPCT_M       (0xFFFFFF<<0)	

#define TSI148_LCSR_VICR_CNTS_M        (3<<22)	
#define TSI148_LCSR_VICR_CNTS_DIS      (1<<22)	
#define TSI148_LCSR_VICR_CNTS_IRQ1     (2<<22)	
#define TSI148_LCSR_VICR_CNTS_IRQ2     (3<<22)	

#define TSI148_LCSR_VICR_EDGIS_M       (3<<20)	
#define TSI148_LCSR_VICR_EDGIS_DIS     (1<<20)	
#define TSI148_LCSR_VICR_EDGIS_IRQ1    (2<<20)	
#define TSI148_LCSR_VICR_EDGIS_IRQ2    (3<<20)	

#define TSI148_LCSR_VICR_IRQIF_M       (3<<18)	
#define TSI148_LCSR_VICR_IRQIF_NORM    (1<<18)	
#define TSI148_LCSR_VICR_IRQIF_PULSE   (2<<18)	
#define TSI148_LCSR_VICR_IRQIF_PROG    (3<<18)	
#define TSI148_LCSR_VICR_IRQIF_1U      (4<<18)	

#define TSI148_LCSR_VICR_IRQ2F_M       (3<<16)	
#define TSI148_LCSR_VICR_IRQ2F_NORM    (1<<16)	
#define TSI148_LCSR_VICR_IRQ2F_PULSE   (2<<16)	
#define TSI148_LCSR_VICR_IRQ2F_PROG    (3<<16)	
#define TSI148_LCSR_VICR_IRQ2F_1U      (4<<16)	

#define TSI148_LCSR_VICR_BIP           (1<<15)	

#define TSI148_LCSR_VICR_IRQC          (1<<12)	
#define TSI148_LCSR_VICR_IRQS          (1<<11)	

#define TSI148_LCSR_VICR_IRQL_M        (7<<8)	
#define TSI148_LCSR_VICR_IRQL_1        (1<<8)	
#define TSI148_LCSR_VICR_IRQL_2        (2<<8)	
#define TSI148_LCSR_VICR_IRQL_3        (3<<8)	
#define TSI148_LCSR_VICR_IRQL_4        (4<<8)	
#define TSI148_LCSR_VICR_IRQL_5        (5<<8)	
#define TSI148_LCSR_VICR_IRQL_6        (6<<8)	
#define TSI148_LCSR_VICR_IRQL_7        (7<<8)	

static const int TSI148_LCSR_VICR_IRQL[8] = { 0, TSI148_LCSR_VICR_IRQL_1,
			TSI148_LCSR_VICR_IRQL_2, TSI148_LCSR_VICR_IRQL_3,
			TSI148_LCSR_VICR_IRQL_4, TSI148_LCSR_VICR_IRQL_5,
			TSI148_LCSR_VICR_IRQL_6, TSI148_LCSR_VICR_IRQL_7 };

#define TSI148_LCSR_VICR_STID_M        (0xFF<<0)	

#define TSI148_LCSR_INTEN_DMA1EN       (1<<25)	
#define TSI148_LCSR_INTEN_DMA0EN       (1<<24)	
#define TSI148_LCSR_INTEN_LM3EN        (1<<23)	
#define TSI148_LCSR_INTEN_LM2EN        (1<<22)	
#define TSI148_LCSR_INTEN_LM1EN        (1<<21)	
#define TSI148_LCSR_INTEN_LM0EN        (1<<20)	
#define TSI148_LCSR_INTEN_MB3EN        (1<<19)	
#define TSI148_LCSR_INTEN_MB2EN        (1<<18)	
#define TSI148_LCSR_INTEN_MB1EN        (1<<17)	
#define TSI148_LCSR_INTEN_MB0EN        (1<<16)	
#define TSI148_LCSR_INTEN_PERREN       (1<<13)	
#define TSI148_LCSR_INTEN_VERREN       (1<<12)	
#define TSI148_LCSR_INTEN_VIEEN        (1<<11)	
#define TSI148_LCSR_INTEN_IACKEN       (1<<10)	
#define TSI148_LCSR_INTEN_SYSFLEN      (1<<9)	
#define TSI148_LCSR_INTEN_ACFLEN       (1<<8)	
#define TSI148_LCSR_INTEN_IRQ7EN       (1<<7)	
#define TSI148_LCSR_INTEN_IRQ6EN       (1<<6)	
#define TSI148_LCSR_INTEN_IRQ5EN       (1<<5)	
#define TSI148_LCSR_INTEN_IRQ4EN       (1<<4)	
#define TSI148_LCSR_INTEN_IRQ3EN       (1<<3)	
#define TSI148_LCSR_INTEN_IRQ2EN       (1<<2)	
#define TSI148_LCSR_INTEN_IRQ1EN       (1<<1)	

static const int TSI148_LCSR_INTEN_LMEN[4] = { TSI148_LCSR_INTEN_LM0EN,
					TSI148_LCSR_INTEN_LM1EN,
					TSI148_LCSR_INTEN_LM2EN,
					TSI148_LCSR_INTEN_LM3EN };

static const int TSI148_LCSR_INTEN_IRQEN[7] = { TSI148_LCSR_INTEN_IRQ1EN,
					TSI148_LCSR_INTEN_IRQ2EN,
					TSI148_LCSR_INTEN_IRQ3EN,
					TSI148_LCSR_INTEN_IRQ4EN,
					TSI148_LCSR_INTEN_IRQ5EN,
					TSI148_LCSR_INTEN_IRQ6EN,
					TSI148_LCSR_INTEN_IRQ7EN };

#define TSI148_LCSR_INTEO_DMA1EO       (1<<25)	
#define TSI148_LCSR_INTEO_DMA0EO       (1<<24)	
#define TSI148_LCSR_INTEO_LM3EO        (1<<23)	
#define TSI148_LCSR_INTEO_LM2EO        (1<<22)	
#define TSI148_LCSR_INTEO_LM1EO        (1<<21)	
#define TSI148_LCSR_INTEO_LM0EO        (1<<20)	
#define TSI148_LCSR_INTEO_MB3EO        (1<<19)	
#define TSI148_LCSR_INTEO_MB2EO        (1<<18)	
#define TSI148_LCSR_INTEO_MB1EO        (1<<17)	
#define TSI148_LCSR_INTEO_MB0EO        (1<<16)	
#define TSI148_LCSR_INTEO_PERREO       (1<<13)	
#define TSI148_LCSR_INTEO_VERREO       (1<<12)	
#define TSI148_LCSR_INTEO_VIEEO        (1<<11)	
#define TSI148_LCSR_INTEO_IACKEO       (1<<10)	
#define TSI148_LCSR_INTEO_SYSFLEO      (1<<9)	
#define TSI148_LCSR_INTEO_ACFLEO       (1<<8)	
#define TSI148_LCSR_INTEO_IRQ7EO       (1<<7)	
#define TSI148_LCSR_INTEO_IRQ6EO       (1<<6)	
#define TSI148_LCSR_INTEO_IRQ5EO       (1<<5)	
#define TSI148_LCSR_INTEO_IRQ4EO       (1<<4)	
#define TSI148_LCSR_INTEO_IRQ3EO       (1<<3)	
#define TSI148_LCSR_INTEO_IRQ2EO       (1<<2)	
#define TSI148_LCSR_INTEO_IRQ1EO       (1<<1)	

static const int TSI148_LCSR_INTEO_LMEO[4] = { TSI148_LCSR_INTEO_LM0EO,
					TSI148_LCSR_INTEO_LM1EO,
					TSI148_LCSR_INTEO_LM2EO,
					TSI148_LCSR_INTEO_LM3EO };

static const int TSI148_LCSR_INTEO_IRQEO[7] = { TSI148_LCSR_INTEO_IRQ1EO,
					TSI148_LCSR_INTEO_IRQ2EO,
					TSI148_LCSR_INTEO_IRQ3EO,
					TSI148_LCSR_INTEO_IRQ4EO,
					TSI148_LCSR_INTEO_IRQ5EO,
					TSI148_LCSR_INTEO_IRQ6EO,
					TSI148_LCSR_INTEO_IRQ7EO };

#define TSI148_LCSR_INTS_DMA1S         (1<<25)	
#define TSI148_LCSR_INTS_DMA0S         (1<<24)	
#define TSI148_LCSR_INTS_LM3S          (1<<23)	
#define TSI148_LCSR_INTS_LM2S          (1<<22)	
#define TSI148_LCSR_INTS_LM1S          (1<<21)	
#define TSI148_LCSR_INTS_LM0S          (1<<20)	
#define TSI148_LCSR_INTS_MB3S          (1<<19)	
#define TSI148_LCSR_INTS_MB2S          (1<<18)	
#define TSI148_LCSR_INTS_MB1S          (1<<17)	
#define TSI148_LCSR_INTS_MB0S          (1<<16)	
#define TSI148_LCSR_INTS_PERRS         (1<<13)	
#define TSI148_LCSR_INTS_VERRS         (1<<12)	
#define TSI148_LCSR_INTS_VIES          (1<<11)	
#define TSI148_LCSR_INTS_IACKS         (1<<10)	
#define TSI148_LCSR_INTS_SYSFLS        (1<<9)	
#define TSI148_LCSR_INTS_ACFLS         (1<<8)	
#define TSI148_LCSR_INTS_IRQ7S         (1<<7)	
#define TSI148_LCSR_INTS_IRQ6S         (1<<6)	
#define TSI148_LCSR_INTS_IRQ5S         (1<<5)	
#define TSI148_LCSR_INTS_IRQ4S         (1<<4)	
#define TSI148_LCSR_INTS_IRQ3S         (1<<3)	
#define TSI148_LCSR_INTS_IRQ2S         (1<<2)	
#define TSI148_LCSR_INTS_IRQ1S         (1<<1)	

static const int TSI148_LCSR_INTS_LMS[4] = { TSI148_LCSR_INTS_LM0S,
					TSI148_LCSR_INTS_LM1S,
					TSI148_LCSR_INTS_LM2S,
					TSI148_LCSR_INTS_LM3S };

static const int TSI148_LCSR_INTS_MBS[4] = { TSI148_LCSR_INTS_MB0S,
					TSI148_LCSR_INTS_MB1S,
					TSI148_LCSR_INTS_MB2S,
					TSI148_LCSR_INTS_MB3S };

#define TSI148_LCSR_INTC_DMA1C         (1<<25)	
#define TSI148_LCSR_INTC_DMA0C         (1<<24)	
#define TSI148_LCSR_INTC_LM3C          (1<<23)	
#define TSI148_LCSR_INTC_LM2C          (1<<22)	
#define TSI148_LCSR_INTC_LM1C          (1<<21)	
#define TSI148_LCSR_INTC_LM0C          (1<<20)	
#define TSI148_LCSR_INTC_MB3C          (1<<19)	
#define TSI148_LCSR_INTC_MB2C          (1<<18)	
#define TSI148_LCSR_INTC_MB1C          (1<<17)	
#define TSI148_LCSR_INTC_MB0C          (1<<16)	
#define TSI148_LCSR_INTC_PERRC         (1<<13)	
#define TSI148_LCSR_INTC_VERRC         (1<<12)	
#define TSI148_LCSR_INTC_VIEC          (1<<11)	
#define TSI148_LCSR_INTC_IACKC         (1<<10)	
#define TSI148_LCSR_INTC_SYSFLC        (1<<9)	
#define TSI148_LCSR_INTC_ACFLC         (1<<8)	

static const int TSI148_LCSR_INTC_LMC[4] = { TSI148_LCSR_INTC_LM0C,
					TSI148_LCSR_INTC_LM1C,
					TSI148_LCSR_INTC_LM2C,
					TSI148_LCSR_INTC_LM3C };

static const int TSI148_LCSR_INTC_MBC[4] = { TSI148_LCSR_INTC_MB0C,
					TSI148_LCSR_INTC_MB1C,
					TSI148_LCSR_INTC_MB2C,
					TSI148_LCSR_INTC_MB3C };

#define TSI148_LCSR_INTM1_DMA1M_M      (3<<18)	
#define TSI148_LCSR_INTM1_DMA0M_M      (3<<16)	
#define TSI148_LCSR_INTM1_LM3M_M       (3<<14)	
#define TSI148_LCSR_INTM1_LM2M_M       (3<<12)	
#define TSI148_LCSR_INTM1_LM1M_M       (3<<10)	
#define TSI148_LCSR_INTM1_LM0M_M       (3<<8)	
#define TSI148_LCSR_INTM1_MB3M_M       (3<<6)	
#define TSI148_LCSR_INTM1_MB2M_M       (3<<4)	
#define TSI148_LCSR_INTM1_MB1M_M       (3<<2)	
#define TSI148_LCSR_INTM1_MB0M_M       (3<<0)	

#define TSI148_LCSR_INTM2_PERRM_M      (3<<26)	
#define TSI148_LCSR_INTM2_VERRM_M      (3<<24)	
#define TSI148_LCSR_INTM2_VIEM_M       (3<<22)	
#define TSI148_LCSR_INTM2_IACKM_M      (3<<20)	
#define TSI148_LCSR_INTM2_SYSFLM_M     (3<<18)	
#define TSI148_LCSR_INTM2_ACFLM_M      (3<<16)	
#define TSI148_LCSR_INTM2_IRQ7M_M      (3<<14)	
#define TSI148_LCSR_INTM2_IRQ6M_M      (3<<12)	
#define TSI148_LCSR_INTM2_IRQ5M_M      (3<<10)	
#define TSI148_LCSR_INTM2_IRQ4M_M      (3<<8)	
#define TSI148_LCSR_INTM2_IRQ3M_M      (3<<6)	
#define TSI148_LCSR_INTM2_IRQ2M_M      (3<<4)	
#define TSI148_LCSR_INTM2_IRQ1M_M      (3<<2)	

#define TSI148_LCSR_DCTL_ABT           (1<<27)	
#define TSI148_LCSR_DCTL_PAU           (1<<26)	
#define TSI148_LCSR_DCTL_DGO           (1<<25)	

#define TSI148_LCSR_DCTL_MOD           (1<<23)	

#define TSI148_LCSR_DCTL_VBKS_M        (7<<12)	
#define TSI148_LCSR_DCTL_VBKS_32       (0<<12)	
#define TSI148_LCSR_DCTL_VBKS_64       (1<<12)	
#define TSI148_LCSR_DCTL_VBKS_128      (2<<12)	
#define TSI148_LCSR_DCTL_VBKS_256      (3<<12)	
#define TSI148_LCSR_DCTL_VBKS_512      (4<<12)	
#define TSI148_LCSR_DCTL_VBKS_1024     (5<<12)	
#define TSI148_LCSR_DCTL_VBKS_2048     (6<<12)	
#define TSI148_LCSR_DCTL_VBKS_4096     (7<<12)	

#define TSI148_LCSR_DCTL_VBOT_M        (7<<8)	
#define TSI148_LCSR_DCTL_VBOT_0        (0<<8)	
#define TSI148_LCSR_DCTL_VBOT_1        (1<<8)	
#define TSI148_LCSR_DCTL_VBOT_2        (2<<8)	
#define TSI148_LCSR_DCTL_VBOT_4        (3<<8)	
#define TSI148_LCSR_DCTL_VBOT_8        (4<<8)	
#define TSI148_LCSR_DCTL_VBOT_16       (5<<8)	
#define TSI148_LCSR_DCTL_VBOT_32       (6<<8)	
#define TSI148_LCSR_DCTL_VBOT_64       (7<<8)	

#define TSI148_LCSR_DCTL_PBKS_M        (7<<4)	
#define TSI148_LCSR_DCTL_PBKS_32       (0<<4)	
#define TSI148_LCSR_DCTL_PBKS_64       (1<<4)	
#define TSI148_LCSR_DCTL_PBKS_128      (2<<4)	
#define TSI148_LCSR_DCTL_PBKS_256      (3<<4)	
#define TSI148_LCSR_DCTL_PBKS_512      (4<<4)	
#define TSI148_LCSR_DCTL_PBKS_1024     (5<<4)	
#define TSI148_LCSR_DCTL_PBKS_2048     (6<<4)	
#define TSI148_LCSR_DCTL_PBKS_4096     (7<<4)	

#define TSI148_LCSR_DCTL_PBOT_M        (7<<0)	
#define TSI148_LCSR_DCTL_PBOT_0        (0<<0)	
#define TSI148_LCSR_DCTL_PBOT_1        (1<<0)	
#define TSI148_LCSR_DCTL_PBOT_2        (2<<0)	
#define TSI148_LCSR_DCTL_PBOT_4        (3<<0)	
#define TSI148_LCSR_DCTL_PBOT_8        (4<<0)	
#define TSI148_LCSR_DCTL_PBOT_16       (5<<0)	
#define TSI148_LCSR_DCTL_PBOT_32       (6<<0)	
#define TSI148_LCSR_DCTL_PBOT_64       (7<<0)	

#define TSI148_LCSR_DSTA_SMA           (1<<31)	
#define TSI148_LCSR_DSTA_RTA           (1<<30)	
#define TSI148_LCSR_DSTA_MRC           (1<<29)	
#define TSI148_LCSR_DSTA_VBE           (1<<28)	
#define TSI148_LCSR_DSTA_ABT           (1<<27)	
#define TSI148_LCSR_DSTA_PAU           (1<<26)	
#define TSI148_LCSR_DSTA_DON           (1<<25)	
#define TSI148_LCSR_DSTA_BSY           (1<<24)	

#define TSI148_LCSR_DCLAL_M            (0x3FFFFFF<<6)	

#define TSI148_LCSR_DSAT_TYP_M         (3<<28)	
#define TSI148_LCSR_DSAT_TYP_PCI       (0<<28)	
#define TSI148_LCSR_DSAT_TYP_VME       (1<<28)	
#define TSI148_LCSR_DSAT_TYP_PAT       (2<<28)	

#define TSI148_LCSR_DSAT_PSZ           (1<<25)	
#define TSI148_LCSR_DSAT_NIN           (1<<24)	

#define TSI148_LCSR_DSAT_2eSSTM_M      (3<<11)	
#define TSI148_LCSR_DSAT_2eSSTM_160    (0<<11)	
#define TSI148_LCSR_DSAT_2eSSTM_267    (1<<11)	
#define TSI148_LCSR_DSAT_2eSSTM_320    (2<<11)	

#define TSI148_LCSR_DSAT_TM_M          (7<<8)	
#define TSI148_LCSR_DSAT_TM_SCT        (0<<8)	
#define TSI148_LCSR_DSAT_TM_BLT        (1<<8)	
#define TSI148_LCSR_DSAT_TM_MBLT       (2<<8)	
#define TSI148_LCSR_DSAT_TM_2eVME      (3<<8)	
#define TSI148_LCSR_DSAT_TM_2eSST      (4<<8)	
#define TSI148_LCSR_DSAT_TM_2eSSTB     (5<<8)	

#define TSI148_LCSR_DSAT_DBW_M         (3<<6)	
#define TSI148_LCSR_DSAT_DBW_16        (0<<6)	
#define TSI148_LCSR_DSAT_DBW_32        (1<<6)	

#define TSI148_LCSR_DSAT_SUP           (1<<5)	
#define TSI148_LCSR_DSAT_PGM           (1<<4)	

#define TSI148_LCSR_DSAT_AMODE_M       (0xf<<0)	
#define TSI148_LCSR_DSAT_AMODE_A16     (0<<0)	
#define TSI148_LCSR_DSAT_AMODE_A24     (1<<0)	
#define TSI148_LCSR_DSAT_AMODE_A32     (2<<0)	
#define TSI148_LCSR_DSAT_AMODE_A64     (4<<0)	
#define TSI148_LCSR_DSAT_AMODE_CRCSR   (5<<0)	
#define TSI148_LCSR_DSAT_AMODE_USER1   (8<<0)	
#define TSI148_LCSR_DSAT_AMODE_USER2   (9<<0)	
#define TSI148_LCSR_DSAT_AMODE_USER3   (0xa<<0)	
#define TSI148_LCSR_DSAT_AMODE_USER4   (0xb<<0)	

#define TSI148_LCSR_DDAT_TYP_PCI       (0<<28)	
#define TSI148_LCSR_DDAT_TYP_VME       (1<<28)	

#define TSI148_LCSR_DDAT_2eSSTM_M      (3<<11)	
#define TSI148_LCSR_DDAT_2eSSTM_160    (0<<11)	
#define TSI148_LCSR_DDAT_2eSSTM_267    (1<<11)	
#define TSI148_LCSR_DDAT_2eSSTM_320    (2<<11)	

#define TSI148_LCSR_DDAT_TM_M          (7<<8)	
#define TSI148_LCSR_DDAT_TM_SCT        (0<<8)	
#define TSI148_LCSR_DDAT_TM_BLT        (1<<8)	
#define TSI148_LCSR_DDAT_TM_MBLT       (2<<8)	
#define TSI148_LCSR_DDAT_TM_2eVME      (3<<8)	
#define TSI148_LCSR_DDAT_TM_2eSST      (4<<8)	
#define TSI148_LCSR_DDAT_TM_2eSSTB     (5<<8)	

#define TSI148_LCSR_DDAT_DBW_M         (3<<6)	
#define TSI148_LCSR_DDAT_DBW_16        (0<<6)	
#define TSI148_LCSR_DDAT_DBW_32        (1<<6)	

#define TSI148_LCSR_DDAT_SUP           (1<<5)	
#define TSI148_LCSR_DDAT_PGM           (1<<4)	

#define TSI148_LCSR_DDAT_AMODE_M       (0xf<<0)	
#define TSI148_LCSR_DDAT_AMODE_A16      (0<<0)	
#define TSI148_LCSR_DDAT_AMODE_A24      (1<<0)	
#define TSI148_LCSR_DDAT_AMODE_A32      (2<<0)	
#define TSI148_LCSR_DDAT_AMODE_A64      (4<<0)	
#define TSI148_LCSR_DDAT_AMODE_CRCSR   (5<<0)	
#define TSI148_LCSR_DDAT_AMODE_USER1   (8<<0)	
#define TSI148_LCSR_DDAT_AMODE_USER2   (9<<0)	
#define TSI148_LCSR_DDAT_AMODE_USER3   (0xa<<0)	
#define TSI148_LCSR_DDAT_AMODE_USER4   (0xb<<0)	

#define TSI148_LCSR_DNLAL_DNLAL_M      (0x3FFFFFF<<6)	
#define TSI148_LCSR_DNLAL_LLA          (1<<0)  

#define TSI148_LCSR_DBS_M              (0x1FFFFF<<0)	


#define TSI148_GCSR_GCTRL_LRST         (1<<15)	
#define TSI148_GCSR_GCTRL_SFAILEN      (1<<14)	
#define TSI148_GCSR_GCTRL_BDFAILS      (1<<13)	
#define TSI148_GCSR_GCTRL_SCON         (1<<12)	
#define TSI148_GCSR_GCTRL_MEN          (1<<11)	

#define TSI148_GCSR_GCTRL_LMI3S        (1<<7)	
#define TSI148_GCSR_GCTRL_LMI2S        (1<<6)	
#define TSI148_GCSR_GCTRL_LMI1S        (1<<5)	
#define TSI148_GCSR_GCTRL_LMI0S        (1<<4)	
#define TSI148_GCSR_GCTRL_MBI3S        (1<<3)	
#define TSI148_GCSR_GCTRL_MBI2S        (1<<2)	
#define TSI148_GCSR_GCTRL_MBI1S        (1<<1)	
#define TSI148_GCSR_GCTRL_MBI0S        (1<<0)	

#define TSI148_GCSR_GAP                (1<<5)	
#define TSI148_GCSR_GA_M               (0x1F<<0)  


#define TSI148_CRCSR_CSRBCR_LRSTC      (1<<7)	
#define TSI148_CRCSR_CSRBCR_SFAILC     (1<<6)	
#define TSI148_CRCSR_CSRBCR_BDFAILS    (1<<5)	
#define TSI148_CRCSR_CSRBCR_MENC       (1<<4)	
#define TSI148_CRCSR_CSRBCR_BERRSC     (1<<3)	

#define TSI148_CRCSR_CSRBSR_LISTS      (1<<7)	
#define TSI148_CRCSR_CSRBSR_SFAILS     (1<<6)	
#define TSI148_CRCSR_CSRBSR_BDFAILS    (1<<5)	
#define TSI148_CRCSR_CSRBSR_MENS       (1<<4)	
#define TSI148_CRCSR_CSRBSR_BERRS      (1<<3)	

#define TSI148_CRCSR_CBAR_M            (0x1F<<3)	

#endif				
