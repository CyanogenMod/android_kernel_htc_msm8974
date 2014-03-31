#ifndef _SPARC_ASI_H
#define _SPARC_ASI_H

/* asi.h:  Address Space Identifier values for the sparc.
 *
 * Copyright (C) 1995,1996 David S. Miller (davem@caip.rutgers.edu)
 *
 * Pioneer work for sun4m: Paul Hatchman (paul@sfe.com.au)
 * Joint edition for sun4c+sun4m: Pete A. Zaitcev <zaitcev@ipmce.su>
 */


#define ASI_NULL1           0x00
#define ASI_NULL2           0x01

#define ASI_CONTROL         0x02
#define ASI_SEGMAP          0x03
#define ASI_PTE             0x04
#define ASI_HWFLUSHSEG      0x05
#define ASI_HWFLUSHPAGE     0x06
#define ASI_REGMAP          0x06
#define ASI_HWFLUSHCONTEXT  0x07

#define ASI_USERTXT         0x08
#define ASI_KERNELTXT       0x09
#define ASI_USERDATA        0x0a
#define ASI_KERNELDATA      0x0b

#define ASI_FLUSHSEG        0x0c
#define ASI_FLUSHPG         0x0d
#define ASI_FLUSHCTX        0x0e

#define ASI_M_RES00         0x00   
#define ASI_M_UNA01         0x01   
#define ASI_M_MXCC          0x02   
#define ASI_M_FLUSH_PROBE   0x03   
#ifndef CONFIG_SPARC_LEON
#define ASI_M_MMUREGS       0x04   
#else
#define ASI_M_MMUREGS       0x19
#endif 
#define ASI_M_TLBDIAG       0x05   
#define ASI_M_DIAGS         0x06   
#define ASI_M_IODIAG        0x07   
#define ASI_M_USERTXT       0x08   
#define ASI_M_KERNELTXT     0x09   
#define ASI_M_USERDATA      0x0A   
#define ASI_M_KERNELDATA    0x0B   
#define ASI_M_TXTC_TAG      0x0C   
#define ASI_M_TXTC_DATA     0x0D   
#define ASI_M_DATAC_TAG     0x0E   
#define ASI_M_DATAC_DATA    0x0F   


#define ASI_M_FLUSH_PAGE    0x10   
#define ASI_M_FLUSH_SEG     0x11   
#define ASI_M_FLUSH_REGION  0x12   
#define ASI_M_FLUSH_CTX     0x13   
#define ASI_M_FLUSH_USER    0x14   

#define ASI_M_BCOPY         0x17   

#define ASI_M_IFLUSH_PAGE   0x18   
#define ASI_M_IFLUSH_SEG    0x19   
#define ASI_M_IFLUSH_REGION 0x1A   
#define ASI_M_IFLUSH_CTX    0x1B   
#define ASI_M_IFLUSH_USER   0x1C   

#define ASI_M_BFILL         0x1F


#define ASI_M_BYPASS       0x20   
#define ASI_M_FBMEM        0x29   
#define ASI_M_VMEUS        0x2A   
#define ASI_M_VMEPS        0x2B   
#define ASI_M_VMEUT        0x2C   
#define ASI_M_VMEPT        0x2D   
#define ASI_M_SBUS         0x2E   
#define ASI_M_CTL          0x2F   


#define ASI_M_FLUSH_IWHOLE 0x31   

#define ASI_M_IC_FLCLEAR   0x36
#define ASI_M_DC_FLCLEAR   0x37

#define ASI_M_DCDR         0x39   

#define ASI_M_VIKING_TMP1  0x40	  
  

#define ASI_M_ACTION       0x4c   

#define ASI_N			0x04 
#define ASI_NL			0x0c 
#define ASI_AIUP		0x10 
#define ASI_AIUS		0x11 
#define ASI_AIUPL		0x18 
#define ASI_AIUSL		0x19 
#define ASI_P			0x80 
#define ASI_S			0x81 
#define ASI_PNF			0x82 
#define ASI_SNF			0x83 
#define ASI_PL			0x88 
#define ASI_SL			0x89 
#define ASI_PNFL		0x8a 
#define ASI_SNFL		0x8b 

#define ASI_PHYS_USE_EC		0x14 
#define ASI_PHYS_BYPASS_EC_E	0x15 
#define ASI_BLK_AIUP_4V		0x16 
#define ASI_BLK_AIUS_4V		0x17 
#define ASI_PHYS_USE_EC_L	0x1c 
#define ASI_PHYS_BYPASS_EC_E_L	0x1d 
#define ASI_BLK_AIUP_L_4V	0x1e 
#define ASI_BLK_AIUS_L_4V	0x1f 
#define ASI_SCRATCHPAD		0x20 
#define ASI_MMU			0x21 
#define ASI_BLK_INIT_QUAD_LDD_AIUS 0x23 
#define ASI_NUCLEUS_QUAD_LDD	0x24 
#define ASI_QUEUE		0x25 
#define ASI_QUAD_LDD_PHYS_4V	0x26 
#define ASI_NUCLEUS_QUAD_LDD_L	0x2c 
#define ASI_QUAD_LDD_PHYS_L_4V	0x2e 
#define ASI_PCACHE_DATA_STATUS	0x30 
#define ASI_PCACHE_DATA		0x31 
#define ASI_PCACHE_TAG		0x32 
#define ASI_PCACHE_SNOOP_TAG	0x33 
#define ASI_QUAD_LDD_PHYS	0x34 
#define ASI_WCACHE_VALID_BITS	0x38 
#define ASI_WCACHE_DATA		0x39 
#define ASI_WCACHE_TAG		0x3a 
#define ASI_WCACHE_SNOOP_TAG	0x3b 
#define ASI_QUAD_LDD_PHYS_L	0x3c 
#define ASI_SRAM_FAST_INIT	0x40 
#define ASI_CORE_AVAILABLE	0x41 
#define ASI_CORE_ENABLE_STAT	0x41 
#define ASI_CORE_ENABLE		0x41 
#define ASI_XIR_STEERING	0x41 
#define ASI_CORE_RUNNING_RW	0x41 
#define ASI_CORE_RUNNING_W1S	0x41 
#define ASI_CORE_RUNNING_W1C	0x41 
#define ASI_CORE_RUNNING_STAT	0x41 
#define ASI_CMT_ERROR_STEERING	0x41 
#define ASI_DCACHE_INVALIDATE	0x42 
#define ASI_DCACHE_UTAG		0x43 
#define ASI_DCACHE_SNOOP_TAG	0x44 
#define ASI_LSU_CONTROL		0x45 
#define ASI_DCU_CONTROL_REG	0x45 
#define ASI_DCACHE_DATA		0x46 
#define ASI_DCACHE_TAG		0x47 
#define ASI_INTR_DISPATCH_STAT	0x48 
#define ASI_INTR_RECEIVE	0x49 
#define ASI_UPA_CONFIG		0x4a 
#define ASI_JBUS_CONFIG		0x4a 
#define ASI_SAFARI_CONFIG	0x4a 
#define ASI_SAFARI_ADDRESS	0x4a 
#define ASI_ESTATE_ERROR_EN	0x4b 
#define ASI_AFSR		0x4c 
#define ASI_AFAR		0x4d 
#define ASI_EC_TAG_DATA		0x4e 
#define ASI_IMMU		0x50 
#define ASI_IMMU_TSB_8KB_PTR	0x51 
#define ASI_IMMU_TSB_64KB_PTR	0x52 
#define ASI_ITLB_DATA_IN	0x54 
#define ASI_ITLB_DATA_ACCESS	0x55 
#define ASI_ITLB_TAG_READ	0x56 
#define ASI_IMMU_DEMAP		0x57 
#define ASI_DMMU		0x58 
#define ASI_DMMU_TSB_8KB_PTR	0x59 
#define ASI_DMMU_TSB_64KB_PTR	0x5a 
#define ASI_DMMU_TSB_DIRECT_PTR	0x5b 
#define ASI_DTLB_DATA_IN	0x5c 
#define ASI_DTLB_DATA_ACCESS	0x5d 
#define ASI_DTLB_TAG_READ	0x5e 
#define ASI_DMMU_DEMAP		0x5f 
#define ASI_IIU_INST_TRAP	0x60 
#define ASI_INTR_ID		0x63 
#define ASI_CORE_ID		0x63 
#define ASI_CESR_ID		0x63 
#define ASI_IC_INSTR		0x66 
#define ASI_IC_TAG		0x67 
#define ASI_IC_STAG		0x68 
#define ASI_IC_PRE_DECODE	0x6e 
#define ASI_IC_NEXT_FIELD	0x6f 
#define ASI_BRPRED_ARRAY	0x6f 
#define ASI_BLK_AIUP		0x70 
#define ASI_BLK_AIUS		0x71 
#define ASI_MCU_CTRL_REG	0x72 
#define ASI_EC_DATA		0x74 
#define ASI_EC_CTRL		0x75 
#define ASI_EC_W		0x76 
#define ASI_UDB_ERROR_W		0x77 
#define ASI_UDB_CONTROL_W	0x77 
#define ASI_INTR_W		0x77 
#define ASI_INTR_DATAN_W	0x77 
#define ASI_INTR_DISPATCH_W	0x77 
#define ASI_BLK_AIUPL		0x78 
#define ASI_BLK_AIUSL		0x79 
#define ASI_EC_R		0x7e 
#define ASI_UDBH_ERROR_R	0x7f 
#define ASI_UDBL_ERROR_R	0x7f 
#define ASI_UDBH_CONTROL_R	0x7f 
#define ASI_UDBL_CONTROL_R	0x7f 
#define ASI_INTR_R		0x7f 
#define ASI_INTR_DATAN_R	0x7f 
#define ASI_PST8_P		0xc0 
#define ASI_PST8_S		0xc1 
#define ASI_PST16_P		0xc2 
#define ASI_PST16_S		0xc3 
#define ASI_PST32_P		0xc4 
#define ASI_PST32_S		0xc5 
#define ASI_PST8_PL		0xc8 
#define ASI_PST8_SL		0xc9 
#define ASI_PST16_PL		0xca 
#define ASI_PST16_SL		0xcb 
#define ASI_PST32_PL		0xcc 
#define ASI_PST32_SL		0xcd 
#define ASI_FL8_P		0xd0 
#define ASI_FL8_S		0xd1 
#define ASI_FL16_P		0xd2 
#define ASI_FL16_S		0xd3 
#define ASI_FL8_PL		0xd8 
#define ASI_FL8_SL		0xd9 
#define ASI_FL16_PL		0xda 
#define ASI_FL16_SL		0xdb 
#define ASI_BLK_COMMIT_P	0xe0 
#define ASI_BLK_COMMIT_S	0xe1 
#define ASI_BLK_INIT_QUAD_LDD_P	0xe2 
#define ASI_BLK_P		0xf0 
#define ASI_BLK_S		0xf1 
#define ASI_BLK_PL		0xf8 
#define ASI_BLK_SL		0xf9 

#endif 
