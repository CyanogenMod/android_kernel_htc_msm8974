/*
 * Blackfin core register bit & address definitions
 *
 * Copyright 2005-2008 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or GPL-2 (or later).
 */

#ifndef _DEF_LPBLACKFIN_H
#define _DEF_LPBLACKFIN_H

#include <mach/anomaly.h>

#define MK_BMSK_(x) (1<<x)
#define BFIN_DEPOSIT(mask, x)	(((x) << __ffs(mask)) & (mask))
#define BFIN_EXTRACT(mask, x)	(((x) & (mask)) >> __ffs(mask))

#ifndef __ASSEMBLY__

#include <linux/types.h>

#if ANOMALY_05000198
# define NOP_PAD_ANOMALY_05000198 "nop;"
#else
# define NOP_PAD_ANOMALY_05000198
#endif

#define _bfin_readX(addr, size, asm_size, asm_ext) ({ \
	u32 __v; \
	__asm__ __volatile__( \
		NOP_PAD_ANOMALY_05000198 \
		"%0 = " #asm_size "[%1]" #asm_ext ";" \
		: "=d" (__v) \
		: "a" (addr) \
	); \
	__v; })
#define _bfin_writeX(addr, val, size, asm_size) \
	__asm__ __volatile__( \
		NOP_PAD_ANOMALY_05000198 \
		#asm_size "[%0] = %1;" \
		: \
		: "a" (addr), "d" ((u##size)(val)) \
		: "memory" \
	)

#define bfin_read8(addr)  _bfin_readX(addr,  8, b, (z))
#define bfin_read16(addr) _bfin_readX(addr, 16, w, (z))
#define bfin_read32(addr) _bfin_readX(addr, 32,  ,    )
#define bfin_write8(addr, val)  _bfin_writeX(addr, val,  8, b)
#define bfin_write16(addr, val) _bfin_writeX(addr, val, 16, w)
#define bfin_write32(addr, val) _bfin_writeX(addr, val, 32,  )

#define bfin_read(addr) \
({ \
	sizeof(*(addr)) == 1 ? bfin_read8(addr)  : \
	sizeof(*(addr)) == 2 ? bfin_read16(addr) : \
	sizeof(*(addr)) == 4 ? bfin_read32(addr) : \
	({ BUG(); 0; }); \
})
#define bfin_write(addr, val) \
do { \
	switch (sizeof(*(addr))) { \
	case 1: bfin_write8(addr, val);  break; \
	case 2: bfin_write16(addr, val); break; \
	case 4: bfin_write32(addr, val); break; \
	default: BUG(); \
	} \
} while (0)

#define bfin_write_or(addr, bits) \
do { \
	typeof(addr) __addr = (addr); \
	bfin_write(__addr, bfin_read(__addr) | (bits)); \
} while (0)

#define bfin_write_and(addr, bits) \
do { \
	typeof(addr) __addr = (addr); \
	bfin_write(__addr, bfin_read(__addr) & (bits)); \
} while (0)

#endif 




#define ASTAT_AZ_P         0x00000000
#define ASTAT_AN_P         0x00000001
#define ASTAT_CC_P         0x00000005
#define ASTAT_AQ_P         0x00000006
#define ASTAT_RND_MOD_P    0x00000008
#define ASTAT_AC0_P        0x0000000C
#define ASTAT_AC0_COPY_P   0x00000002
#define ASTAT_AC1_P        0x0000000D
#define ASTAT_AV0_P        0x00000010
#define ASTAT_AV0S_P       0x00000011
#define ASTAT_AV1_P        0x00000012
#define ASTAT_AV1S_P       0x00000013
#define ASTAT_V_P          0x00000018
#define ASTAT_V_COPY_P     0x00000003
#define ASTAT_VS_P         0x00000019


#define ASTAT_AZ           MK_BMSK_(ASTAT_AZ_P)
#define ASTAT_AN           MK_BMSK_(ASTAT_AN_P)
#define ASTAT_AC0          MK_BMSK_(ASTAT_AC0_P)
#define ASTAT_AC0_COPY     MK_BMSK_(ASTAT_AC0_COPY_P)
#define ASTAT_AC1          MK_BMSK_(ASTAT_AC1_P)
#define ASTAT_AV0          MK_BMSK_(ASTAT_AV0_P)
#define ASTAT_AV1          MK_BMSK_(ASTAT_AV1_P)
#define ASTAT_CC           MK_BMSK_(ASTAT_CC_P)
#define ASTAT_AQ           MK_BMSK_(ASTAT_AQ_P)
#define ASTAT_RND_MOD      MK_BMSK_(ASTAT_RND_MOD_P)
#define ASTAT_V            MK_BMSK_(ASTAT_V_P)
#define ASTAT_V_COPY       MK_BMSK_(ASTAT_V_COPY_P)


#define SEQSTAT_EXCAUSE0_P      0x00000000	
#define SEQSTAT_EXCAUSE1_P      0x00000001	
#define SEQSTAT_EXCAUSE2_P      0x00000002	
#define SEQSTAT_EXCAUSE3_P      0x00000003	
#define SEQSTAT_EXCAUSE4_P      0x00000004	
#define SEQSTAT_EXCAUSE5_P      0x00000005	
#define SEQSTAT_IDLE_REQ_P      0x0000000C	
#define SEQSTAT_SFTRESET_P      0x0000000D	
#define SEQSTAT_HWERRCAUSE0_P   0x0000000E	
#define SEQSTAT_HWERRCAUSE1_P   0x0000000F	
#define SEQSTAT_HWERRCAUSE2_P   0x00000010	
#define SEQSTAT_HWERRCAUSE3_P   0x00000011	
#define SEQSTAT_HWERRCAUSE4_P   0x00000012	
#define SEQSTAT_EXCAUSE        (MK_BMSK_(SEQSTAT_EXCAUSE0_P) | \
                                MK_BMSK_(SEQSTAT_EXCAUSE1_P) | \
                                MK_BMSK_(SEQSTAT_EXCAUSE2_P) | \
                                MK_BMSK_(SEQSTAT_EXCAUSE3_P) | \
                                MK_BMSK_(SEQSTAT_EXCAUSE4_P) | \
                                MK_BMSK_(SEQSTAT_EXCAUSE5_P) | \
                                0)

#define SEQSTAT_SFTRESET       (MK_BMSK_(SEQSTAT_SFTRESET_P))

#define SEQSTAT_HWERRCAUSE     (MK_BMSK_(SEQSTAT_HWERRCAUSE0_P) | \
                                MK_BMSK_(SEQSTAT_HWERRCAUSE1_P) | \
                                MK_BMSK_(SEQSTAT_HWERRCAUSE2_P) | \
                                MK_BMSK_(SEQSTAT_HWERRCAUSE3_P) | \
                                MK_BMSK_(SEQSTAT_HWERRCAUSE4_P) | \
                                0)


#define SEQSTAT_HWERRCAUSE_SHIFT         (14)
#define SEQSTAT_HWERRCAUSE_SYSTEM_MMR    (0x02 << SEQSTAT_HWERRCAUSE_SHIFT)
#define SEQSTAT_HWERRCAUSE_EXTERN_ADDR   (0x03 << SEQSTAT_HWERRCAUSE_SHIFT)
#define SEQSTAT_HWERRCAUSE_PERF_FLOW     (0x12 << SEQSTAT_HWERRCAUSE_SHIFT)
#define SEQSTAT_HWERRCAUSE_RAISE_5       (0x18 << SEQSTAT_HWERRCAUSE_SHIFT)


#define SYSCFG_SSSTEP_P     0x00000000	
#define SYSCFG_CCEN_P       0x00000001	
#define SYSCFG_SNEN_P       0x00000002	


#define SYSCFG_SSSTEP         MK_BMSK_(SYSCFG_SSSTEP_P )
#define SYSCFG_CCEN           MK_BMSK_(SYSCFG_CCEN_P )
#define SYSCFG_SNEN	       MK_BMSK_(SYSCFG_SNEN_P)
#define SYSCFG_SSSSTEP         SYSCFG_SSSTEP
#define SYSCFG_CCCEN           SYSCFG_CCEN



#define SRAM_BASE_ADDRESS  0xFFE00000	
#define DMEM_CONTROL       0xFFE00004	
#define DCPLB_STATUS       0xFFE00008	
#define DCPLB_FAULT_STATUS 0xFFE00008	
#define DCPLB_FAULT_ADDR   0xFFE0000C	
#define DCPLB_ADDR0        0xFFE00100	
#define DCPLB_ADDR1        0xFFE00104	
#define DCPLB_ADDR2        0xFFE00108	
#define DCPLB_ADDR3        0xFFE0010C	
#define DCPLB_ADDR4        0xFFE00110	
#define DCPLB_ADDR5        0xFFE00114	
#define DCPLB_ADDR6        0xFFE00118	
#define DCPLB_ADDR7        0xFFE0011C	
#define DCPLB_ADDR8        0xFFE00120	
#define DCPLB_ADDR9        0xFFE00124	
#define DCPLB_ADDR10       0xFFE00128	
#define DCPLB_ADDR11       0xFFE0012C	
#define DCPLB_ADDR12       0xFFE00130	
#define DCPLB_ADDR13       0xFFE00134	
#define DCPLB_ADDR14       0xFFE00138	
#define DCPLB_ADDR15       0xFFE0013C	
#define DCPLB_DATA0        0xFFE00200	
#define DCPLB_DATA1        0xFFE00204	
#define DCPLB_DATA2        0xFFE00208	
#define DCPLB_DATA3        0xFFE0020C	
#define DCPLB_DATA4        0xFFE00210	
#define DCPLB_DATA5        0xFFE00214	
#define DCPLB_DATA6        0xFFE00218	
#define DCPLB_DATA7        0xFFE0021C	
#define DCPLB_DATA8        0xFFE00220	
#define DCPLB_DATA9        0xFFE00224	
#define DCPLB_DATA10       0xFFE00228	
#define DCPLB_DATA11       0xFFE0022C	
#define DCPLB_DATA12       0xFFE00230	
#define DCPLB_DATA13       0xFFE00234	
#define DCPLB_DATA14       0xFFE00238	
#define DCPLB_DATA15       0xFFE0023C	
#define DCPLB_DATA16       0xFFE00240	

#define DTEST_COMMAND      0xFFE00300	
#define DTEST_DATA0        0xFFE00400	
#define DTEST_DATA1        0xFFE00404	


#define IMEM_CONTROL       0xFFE01004	
#define ICPLB_STATUS       0xFFE01008	
#define CODE_FAULT_STATUS  0xFFE01008	
#define ICPLB_FAULT_ADDR   0xFFE0100C	
#define CODE_FAULT_ADDR    0xFFE0100C	
#define ICPLB_ADDR0        0xFFE01100	
#define ICPLB_ADDR1        0xFFE01104	
#define ICPLB_ADDR2        0xFFE01108	
#define ICPLB_ADDR3        0xFFE0110C	
#define ICPLB_ADDR4        0xFFE01110	
#define ICPLB_ADDR5        0xFFE01114	
#define ICPLB_ADDR6        0xFFE01118	
#define ICPLB_ADDR7        0xFFE0111C	
#define ICPLB_ADDR8        0xFFE01120	
#define ICPLB_ADDR9        0xFFE01124	
#define ICPLB_ADDR10       0xFFE01128	
#define ICPLB_ADDR11       0xFFE0112C	
#define ICPLB_ADDR12       0xFFE01130	
#define ICPLB_ADDR13       0xFFE01134	
#define ICPLB_ADDR14       0xFFE01138	
#define ICPLB_ADDR15       0xFFE0113C	
#define ICPLB_DATA0        0xFFE01200	
#define ICPLB_DATA1        0xFFE01204	
#define ICPLB_DATA2        0xFFE01208	
#define ICPLB_DATA3        0xFFE0120C	
#define ICPLB_DATA4        0xFFE01210	
#define ICPLB_DATA5        0xFFE01214	
#define ICPLB_DATA6        0xFFE01218	
#define ICPLB_DATA7        0xFFE0121C	
#define ICPLB_DATA8        0xFFE01220	
#define ICPLB_DATA9        0xFFE01224	
#define ICPLB_DATA10       0xFFE01228	
#define ICPLB_DATA11       0xFFE0122C	
#define ICPLB_DATA12       0xFFE01230	
#define ICPLB_DATA13       0xFFE01234	
#define ICPLB_DATA14       0xFFE01238	
#define ICPLB_DATA15       0xFFE0123C	
#define ITEST_COMMAND      0xFFE01300	
#define ITEST_DATA0        0xFFE01400	
#define ITEST_DATA1        0xFFE01404	


#define EVT0               0xFFE02000	
#define EVT1               0xFFE02004	
#define EVT2               0xFFE02008	
#define EVT3               0xFFE0200C	
#define EVT4               0xFFE02010	
#define EVT5               0xFFE02014	
#define EVT6               0xFFE02018	
#define EVT7               0xFFE0201C	
#define EVT8               0xFFE02020	
#define EVT9               0xFFE02024	
#define EVT10              0xFFE02028	
#define EVT11              0xFFE0202C	
#define EVT12              0xFFE02030	
#define EVT13              0xFFE02034	
#define EVT14              0xFFE02038	
#define EVT15              0xFFE0203C	
#define EVT_OVERRIDE       0xFFE02100	
#define IMASK              0xFFE02104	
#define IPEND              0xFFE02108	
#define ILAT               0xFFE0210C	
#define IPRIO              0xFFE02110	


#define TCNTL              0xFFE03000	
#define TPERIOD            0xFFE03004	
#define TSCALE             0xFFE03008	
#define TCOUNT             0xFFE0300C	

#define DSPID              0xFFE05000	

#define DBGSTAT            0xFFE05008	


#define TBUFCTL            0xFFE06000	
#define TBUFSTAT           0xFFE06004	
#define TBUF               0xFFE06100	


#define WPIACTL            0xFFE07000
#define WPIA0              0xFFE07040
#define WPIA1              0xFFE07044
#define WPIA2              0xFFE07048
#define WPIA3              0xFFE0704C
#define WPIA4              0xFFE07050
#define WPIA5              0xFFE07054
#define WPIACNT0           0xFFE07080
#define WPIACNT1           0xFFE07084
#define WPIACNT2           0xFFE07088
#define WPIACNT3           0xFFE0708C
#define WPIACNT4           0xFFE07090
#define WPIACNT5           0xFFE07094
#define WPDACTL            0xFFE07100
#define WPDA0              0xFFE07140
#define WPDA1              0xFFE07144
#define WPDACNT0           0xFFE07180
#define WPDACNT1           0xFFE07184
#define WPSTAT             0xFFE07200


#define PFCTL              0xFFE08000
#define PFCNTR0            0xFFE08100
#define PFCNTR1            0xFFE08104



#define EVT_EMU_P        0x00000000	
#define EVT_RST_P        0x00000001	
#define EVT_NMI_P        0x00000002	
#define EVT_EVX_P        0x00000003	
#define EVT_IRPTEN_P     0x00000004	
#define EVT_IVHW_P       0x00000005	
#define EVT_IVTMR_P      0x00000006	
#define EVT_IVG7_P       0x00000007	
#define EVT_IVG8_P       0x00000008	
#define EVT_IVG9_P       0x00000009	
#define EVT_IVG10_P      0x0000000a	
#define EVT_IVG11_P      0x0000000b	
#define EVT_IVG12_P      0x0000000c	
#define EVT_IVG13_P      0x0000000d	
#define EVT_IVG14_P      0x0000000e	
#define EVT_IVG15_P      0x0000000f	

#define EVT_EMU       MK_BMSK_(EVT_EMU_P   )	
#define EVT_RST       MK_BMSK_(EVT_RST_P   )	
#define EVT_NMI       MK_BMSK_(EVT_NMI_P   )	
#define EVT_EVX       MK_BMSK_(EVT_EVX_P   )	
#define EVT_IRPTEN    MK_BMSK_(EVT_IRPTEN_P)	
#define EVT_IVHW      MK_BMSK_(EVT_IVHW_P  )	
#define EVT_IVTMR     MK_BMSK_(EVT_IVTMR_P )	
#define EVT_IVG7      MK_BMSK_(EVT_IVG7_P  )	
#define EVT_IVG8      MK_BMSK_(EVT_IVG8_P  )	
#define EVT_IVG9      MK_BMSK_(EVT_IVG9_P  )	
#define EVT_IVG10     MK_BMSK_(EVT_IVG10_P )	
#define EVT_IVG11     MK_BMSK_(EVT_IVG11_P )	
#define EVT_IVG12     MK_BMSK_(EVT_IVG12_P )	
#define EVT_IVG13     MK_BMSK_(EVT_IVG13_P )	
#define EVT_IVG14     MK_BMSK_(EVT_IVG14_P )	
#define EVT_IVG15     MK_BMSK_(EVT_IVG15_P )	

#define ENDM_P			0x00	
#define DMCTL_ENDM_P		ENDM_P	

#define ENDCPLB_P		0x01	
#define DMCTL_ENDCPLB_P		ENDCPLB_P	
#define DMC0_P			0x02	
#define DMCTL_DMC0_P		DMC0_P	
#define DMC1_P			0x03	
#define DMCTL_DMC1_P		DMC1_P	
#define DCBS_P			0x04	
#define PORT_PREF0_P		0x12	
#define PORT_PREF1_P		0x13	

#define ENDM               0x00000001	
#define ENDCPLB            0x00000002	
#define ASRAM_BSRAM        0x00000000
#define ACACHE_BSRAM       0x00000008
#define ACACHE_BCACHE      0x0000000C
#define DCBS               0x00000010	
#define PORT_PREF0	   0x00001000	
#define PORT_PREF1	   0x00002000	

#define ENIM_P			0x00	
#define IMCTL_ENIM_P            0x00	
#define ENICPLB_P		0x01	
#define IMCTL_ENICPLB_P		0x01	
#define IMC_P			0x02	
#define IMCTL_IMC_P		0x02	
#define ILOC0_P			0x03	
#define ILOC1_P			0x04	
#define ILOC2_P			0x05	
#define ILOC3_P			0x06	
#define LRUPRIORST_P		0x0D	
#define ENIM               0x00000001	
#define ENICPLB            0x00000002	
#define IMC                0x00000004	
#define ILOC0		   0x00000008	
#define ILOC1		   0x00000010	
#define ILOC2		   0x00000020	
#define ILOC3		   0x00000040	
#define LRUPRIORST	   0x00002000	

#define TMPWR              0x00000001	
#define TMREN              0x00000002	
#define TAUTORLD           0x00000004	
#define TINT               0x00000008	

#define CPLB_VALID_P       0x00000000	
#define CPLB_LOCK_P        0x00000001	
#define CPLB_USER_RD_P     0x00000002	
#define CPLB_VALID         0x00000001	
#define CPLB_LOCK          0x00000002	
#define CPLB_USER_RD       0x00000004	

#define PAGE_SIZE_1KB      0x00000000	
#define PAGE_SIZE_4KB      0x00010000	
#define PAGE_SIZE_1MB      0x00020000	
#define PAGE_SIZE_4MB      0x00030000	
#define CPLB_L1SRAM        0x00000020	
#define CPLB_PORTPRIO	   0x00000200	
#define CPLB_L1_CHBL       0x00001000	
#define CPLB_LRUPRIO	   0x00000100	
#define CPLB_USER_WR       0x00000008	
#define CPLB_SUPV_WR       0x00000010	
#define CPLB_DIRTY         0x00000080	
#define CPLB_L1_AOW	   0x00008000	
#define CPLB_WT            0x00004000	

#define CPLB_ALL_ACCESS CPLB_SUPV_WR | CPLB_USER_RD | CPLB_USER_WR

#define TBUFPWR            0x0001
#define TBUFEN             0x0002
#define TBUFOVF            0x0004
#define TBUFCMPLP_SINGLE   0x0008
#define TBUFCMPLP_DOUBLE   0x0010
#define TBUFCMPLP          (TBUFCMPLP_SINGLE | TBUFCMPLP_DOUBLE)

#define TBUFCNT            0x001F

#define TEST_READ	   0x00000000	
#define TEST_WRITE	   0x00000002	
#define TEST_TAG	   0x00000000	
#define TEST_DATA	   0x00000004	
#define TEST_DW0	   0x00000000	
#define TEST_DW1	   0x00000008	
#define TEST_DW2	   0x00000010	
#define TEST_DW3	   0x00000018	
#define TEST_MB0	   0x00000000	
#define TEST_MB1	   0x00010000	
#define TEST_MB2	   0x00020000	
#define TEST_MB3	   0x00030000	
#define TEST_SET(x)	   ((x << 5) & 0x03E0)	
#define TEST_WAY0	   0x00000000	
#define TEST_WAY1	   0x04000000	
#define TEST_WAY2	   0x08000000	
#define TEST_WAY3	   0x0C000000	
#define TEST_BNKSELA	   0x00000000	
#define TEST_BNKSELB	   0x00800000	

#endif				
