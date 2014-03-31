/*
 * MPC8xx Communication Processor Module.
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * This file contains structures and information for the communication
 * processor channels.  Some CPM control and status is available
 * through the MPC8xx internal memory map.  See immap.h for details.
 * This file only contains what I need for the moment, not the total
 * CPM capabilities.  I (or someone else) will add definitions as they
 * are needed.  -- Dan
 *
 * On the MBX board, EPPC-Bug loads CPM microcode into the first 512
 * bytes of the DP RAM and relocates the I2C parameter area to the
 * IDMA1 space.  The remaining DP RAM is available for buffer descriptors
 * or other use.
 */
#ifndef __CPM1__
#define __CPM1__

#include <linux/init.h>
#include <asm/8xx_immap.h>
#include <asm/ptrace.h>
#include <asm/cpm.h>

#define CPM_CR_RST	((ushort)0x8000)
#define CPM_CR_OPCODE	((ushort)0x0f00)
#define CPM_CR_CHAN	((ushort)0x00f0)
#define CPM_CR_FLG	((ushort)0x0001)

#define CPM_CR_CH_SCC1		((ushort)0x0000)
#define CPM_CR_CH_I2C		((ushort)0x0001)	
#define CPM_CR_CH_SCC2		((ushort)0x0004)
#define CPM_CR_CH_SPI		((ushort)0x0005)	
#define CPM_CR_CH_TIMER		CPM_CR_CH_SPI
#define CPM_CR_CH_SCC3		((ushort)0x0008)
#define CPM_CR_CH_SMC1		((ushort)0x0009)	
#define CPM_CR_CH_SCC4		((ushort)0x000c)
#define CPM_CR_CH_SMC2		((ushort)0x000d)	

#define mk_cr_cmd(CH, CMD)	((CMD << 8) | (CH << 4))

extern cpm8xx_t __iomem *cpmp; 

#define cpm_dpalloc cpm_muram_alloc
#define cpm_dpfree cpm_muram_free
#define cpm_dpram_addr cpm_muram_addr
#define cpm_dpram_phys cpm_muram_dma

extern void cpm_setbrg(uint brg, uint rate);

extern void __init cpm_load_patch(cpm8xx_t *cp);

extern void cpm_reset(void);

#define PROFF_SCC1	((uint)0x0000)
#define PROFF_IIC	((uint)0x0080)
#define PROFF_SCC2	((uint)0x0100)
#define PROFF_SPI	((uint)0x0180)
#define PROFF_SCC3	((uint)0x0200)
#define PROFF_SMC1	((uint)0x0280)
#define PROFF_SCC4	((uint)0x0300)
#define PROFF_SMC2	((uint)0x0380)

typedef struct smc_uart {
	ushort	smc_rbase;	
	ushort	smc_tbase;	
	u_char	smc_rfcr;	
	u_char	smc_tfcr;	
	ushort	smc_mrblr;	
	uint	smc_rstate;	
	uint	smc_idp;	
	ushort	smc_rbptr;	
	ushort	smc_ibc;	
	uint	smc_rxtmp;	
	uint	smc_tstate;	
	uint	smc_tdp;	
	ushort	smc_tbptr;	
	ushort	smc_tbc;	
	uint	smc_txtmp;	
	ushort	smc_maxidl;	
	ushort	smc_tmpidl;	
	ushort	smc_brklen;	
	ushort	smc_brkec;	
	ushort	smc_brkcr;	
	ushort	smc_rmask;	
	char	res1[8];	
	ushort	smc_rpbase;	
} smc_uart_t;

#define SMC_EB	((u_char)0x10)	

#define	SMCMR_REN	((ushort)0x0001)
#define SMCMR_TEN	((ushort)0x0002)
#define SMCMR_DM	((ushort)0x000c)
#define SMCMR_SM_GCI	((ushort)0x0000)
#define SMCMR_SM_UART	((ushort)0x0020)
#define SMCMR_SM_TRANS	((ushort)0x0030)
#define SMCMR_SM_MASK	((ushort)0x0030)
#define SMCMR_PM_EVEN	((ushort)0x0100)	
#define SMCMR_REVD	SMCMR_PM_EVEN
#define SMCMR_PEN	((ushort)0x0200)	
#define SMCMR_BS	SMCMR_PEN
#define SMCMR_SL	((ushort)0x0400)	
#define SMCR_CLEN_MASK	((ushort)0x7800)	
#define smcr_mk_clen(C)	(((C) << 11) & SMCR_CLEN_MASK)

typedef struct smc_centronics {
	ushort	scent_rbase;
	ushort	scent_tbase;
	u_char	scent_cfcr;
	u_char	scent_smask;
	ushort	scent_mrblr;
	uint	scent_rstate;
	uint	scent_r_ptr;
	ushort	scent_rbptr;
	ushort	scent_r_cnt;
	uint	scent_rtemp;
	uint	scent_tstate;
	uint	scent_t_ptr;
	ushort	scent_tbptr;
	ushort	scent_t_cnt;
	uint	scent_ttemp;
	ushort	scent_max_sl;
	ushort	scent_sl_cnt;
	ushort	scent_character1;
	ushort	scent_character2;
	ushort	scent_character3;
	ushort	scent_character4;
	ushort	scent_character5;
	ushort	scent_character6;
	ushort	scent_character7;
	ushort	scent_character8;
	ushort	scent_rccm;
	ushort	scent_rccr;
} smc_cent_t;

#define SMC_CENT_F	((u_char)0x08)
#define SMC_CENT_PE	((u_char)0x04)
#define SMC_CENT_S	((u_char)0x02)

#define	SMCM_BRKE	((unsigned char)0x40)	
#define	SMCM_BRK	((unsigned char)0x10)	
#define	SMCM_TXE	((unsigned char)0x10)	
#define	SMCM_BSY	((unsigned char)0x04)
#define	SMCM_TX		((unsigned char)0x02)
#define	SMCM_RX		((unsigned char)0x01)

#define CPM_BRG_RST		((uint)0x00020000)
#define CPM_BRG_EN		((uint)0x00010000)
#define CPM_BRG_EXTC_INT	((uint)0x00000000)
#define CPM_BRG_EXTC_CLK2	((uint)0x00004000)
#define CPM_BRG_EXTC_CLK6	((uint)0x00008000)
#define CPM_BRG_ATB		((uint)0x00002000)
#define CPM_BRG_CD_MASK		((uint)0x00001ffe)
#define CPM_BRG_DIV16		((uint)0x00000001)

#define SICR_RCLK_SCC1_BRG1	((uint)0x00000000)
#define SICR_TCLK_SCC1_BRG1	((uint)0x00000000)
#define SICR_RCLK_SCC2_BRG2	((uint)0x00000800)
#define SICR_TCLK_SCC2_BRG2	((uint)0x00000100)
#define SICR_RCLK_SCC3_BRG3	((uint)0x00100000)
#define SICR_TCLK_SCC3_BRG3	((uint)0x00020000)
#define SICR_RCLK_SCC4_BRG4	((uint)0x18000000)
#define SICR_TCLK_SCC4_BRG4	((uint)0x03000000)

#define SCC_GSMRH_IRP		((uint)0x00040000)
#define SCC_GSMRH_GDE		((uint)0x00010000)
#define SCC_GSMRH_TCRC_CCITT	((uint)0x00008000)
#define SCC_GSMRH_TCRC_BISYNC	((uint)0x00004000)
#define SCC_GSMRH_TCRC_HDLC	((uint)0x00000000)
#define SCC_GSMRH_REVD		((uint)0x00002000)
#define SCC_GSMRH_TRX		((uint)0x00001000)
#define SCC_GSMRH_TTX		((uint)0x00000800)
#define SCC_GSMRH_CDP		((uint)0x00000400)
#define SCC_GSMRH_CTSP		((uint)0x00000200)
#define SCC_GSMRH_CDS		((uint)0x00000100)
#define SCC_GSMRH_CTSS		((uint)0x00000080)
#define SCC_GSMRH_TFL		((uint)0x00000040)
#define SCC_GSMRH_RFW		((uint)0x00000020)
#define SCC_GSMRH_TXSY		((uint)0x00000010)
#define SCC_GSMRH_SYNL16	((uint)0x0000000c)
#define SCC_GSMRH_SYNL8		((uint)0x00000008)
#define SCC_GSMRH_SYNL4		((uint)0x00000004)
#define SCC_GSMRH_RTSM		((uint)0x00000002)
#define SCC_GSMRH_RSYN		((uint)0x00000001)

#define SCC_GSMRL_SIR		((uint)0x80000000)	
#define SCC_GSMRL_EDGE_NONE	((uint)0x60000000)
#define SCC_GSMRL_EDGE_NEG	((uint)0x40000000)
#define SCC_GSMRL_EDGE_POS	((uint)0x20000000)
#define SCC_GSMRL_EDGE_BOTH	((uint)0x00000000)
#define SCC_GSMRL_TCI		((uint)0x10000000)
#define SCC_GSMRL_TSNC_3	((uint)0x0c000000)
#define SCC_GSMRL_TSNC_4	((uint)0x08000000)
#define SCC_GSMRL_TSNC_14	((uint)0x04000000)
#define SCC_GSMRL_TSNC_INF	((uint)0x00000000)
#define SCC_GSMRL_RINV		((uint)0x02000000)
#define SCC_GSMRL_TINV		((uint)0x01000000)
#define SCC_GSMRL_TPL_128	((uint)0x00c00000)
#define SCC_GSMRL_TPL_64	((uint)0x00a00000)
#define SCC_GSMRL_TPL_48	((uint)0x00800000)
#define SCC_GSMRL_TPL_32	((uint)0x00600000)
#define SCC_GSMRL_TPL_16	((uint)0x00400000)
#define SCC_GSMRL_TPL_8		((uint)0x00200000)
#define SCC_GSMRL_TPL_NONE	((uint)0x00000000)
#define SCC_GSMRL_TPP_ALL1	((uint)0x00180000)
#define SCC_GSMRL_TPP_01	((uint)0x00100000)
#define SCC_GSMRL_TPP_10	((uint)0x00080000)
#define SCC_GSMRL_TPP_ZEROS	((uint)0x00000000)
#define SCC_GSMRL_TEND		((uint)0x00040000)
#define SCC_GSMRL_TDCR_32	((uint)0x00030000)
#define SCC_GSMRL_TDCR_16	((uint)0x00020000)
#define SCC_GSMRL_TDCR_8	((uint)0x00010000)
#define SCC_GSMRL_TDCR_1	((uint)0x00000000)
#define SCC_GSMRL_RDCR_32	((uint)0x0000c000)
#define SCC_GSMRL_RDCR_16	((uint)0x00008000)
#define SCC_GSMRL_RDCR_8	((uint)0x00004000)
#define SCC_GSMRL_RDCR_1	((uint)0x00000000)
#define SCC_GSMRL_RENC_DFMAN	((uint)0x00003000)
#define SCC_GSMRL_RENC_MANCH	((uint)0x00002000)
#define SCC_GSMRL_RENC_FM0	((uint)0x00001000)
#define SCC_GSMRL_RENC_NRZI	((uint)0x00000800)
#define SCC_GSMRL_RENC_NRZ	((uint)0x00000000)
#define SCC_GSMRL_TENC_DFMAN	((uint)0x00000600)
#define SCC_GSMRL_TENC_MANCH	((uint)0x00000400)
#define SCC_GSMRL_TENC_FM0	((uint)0x00000200)
#define SCC_GSMRL_TENC_NRZI	((uint)0x00000100)
#define SCC_GSMRL_TENC_NRZ	((uint)0x00000000)
#define SCC_GSMRL_DIAG_LE	((uint)0x000000c0)	
#define SCC_GSMRL_DIAG_ECHO	((uint)0x00000080)
#define SCC_GSMRL_DIAG_LOOP	((uint)0x00000040)
#define SCC_GSMRL_DIAG_NORM	((uint)0x00000000)
#define SCC_GSMRL_ENR		((uint)0x00000020)
#define SCC_GSMRL_ENT		((uint)0x00000010)
#define SCC_GSMRL_MODE_ENET	((uint)0x0000000c)
#define SCC_GSMRL_MODE_QMC	((uint)0x0000000a)
#define SCC_GSMRL_MODE_DDCMP	((uint)0x00000009)
#define SCC_GSMRL_MODE_BISYNC	((uint)0x00000008)
#define SCC_GSMRL_MODE_V14	((uint)0x00000007)
#define SCC_GSMRL_MODE_AHDLC	((uint)0x00000006)
#define SCC_GSMRL_MODE_PROFIBUS	((uint)0x00000005)
#define SCC_GSMRL_MODE_UART	((uint)0x00000004)
#define SCC_GSMRL_MODE_SS7	((uint)0x00000003)
#define SCC_GSMRL_MODE_ATALK	((uint)0x00000002)
#define SCC_GSMRL_MODE_HDLC	((uint)0x00000000)

#define SCC_TODR_TOD		((ushort)0x8000)

#define	SCCM_TXE	((unsigned char)0x10)
#define	SCCM_BSY	((unsigned char)0x04)
#define	SCCM_TX		((unsigned char)0x02)
#define	SCCM_RX		((unsigned char)0x01)

typedef struct scc_param {
	ushort	scc_rbase;	
	ushort	scc_tbase;	
	u_char	scc_rfcr;	
	u_char	scc_tfcr;	
	ushort	scc_mrblr;	
	uint	scc_rstate;	
	uint	scc_idp;	
	ushort	scc_rbptr;	
	ushort	scc_ibc;	
	uint	scc_rxtmp;	
	uint	scc_tstate;	
	uint	scc_tdp;	
	ushort	scc_tbptr;	
	ushort	scc_tbc;	
	uint	scc_txtmp;	
	uint	scc_rcrc;	
	uint	scc_tcrc;	
} sccp_t;

#define SCC_EB	((u_char)0x10)	

typedef struct scc_enet {
	sccp_t	sen_genscc;
	uint	sen_cpres;	
	uint	sen_cmask;	
	uint	sen_crcec;	
	uint	sen_alec;	
	uint	sen_disfc;	
	ushort	sen_pads;	
	ushort	sen_retlim;	
	ushort	sen_retcnt;	
	ushort	sen_maxflr;	
	ushort	sen_minflr;	
	ushort	sen_maxd1;	
	ushort	sen_maxd2;	
	ushort	sen_maxd;	
	ushort	sen_dmacnt;	
	ushort	sen_maxb;	
	ushort	sen_gaddr1;	
	ushort	sen_gaddr2;
	ushort	sen_gaddr3;
	ushort	sen_gaddr4;
	uint	sen_tbuf0data0;	
	uint	sen_tbuf0data1;	
	uint	sen_tbuf0rba;	
	uint	sen_tbuf0crc;	
	ushort	sen_tbuf0bcnt;	
	ushort	sen_paddrh;	
	ushort	sen_paddrm;
	ushort	sen_paddrl;	
	ushort	sen_pper;	
	ushort	sen_rfbdptr;	
	ushort	sen_tfbdptr;	
	ushort	sen_tlbdptr;	
	uint	sen_tbuf1data0;	
	uint	sen_tbuf1data1;	
	uint	sen_tbuf1rba;	
	uint	sen_tbuf1crc;	
	ushort	sen_tbuf1bcnt;	
	ushort	sen_txlen;	
	ushort	sen_iaddr1;	
	ushort	sen_iaddr2;
	ushort	sen_iaddr3;
	ushort	sen_iaddr4;
	ushort	sen_boffcnt;	

	ushort	sen_taddrh;	
	ushort	sen_taddrm;
	ushort	sen_taddrl;	
} scc_enet_t;

#define SCCE_ENET_GRA	((ushort)0x0080)	
#define SCCE_ENET_TXE	((ushort)0x0010)	
#define SCCE_ENET_RXF	((ushort)0x0008)	
#define SCCE_ENET_BSY	((ushort)0x0004)	
#define SCCE_ENET_TXB	((ushort)0x0002)	
#define SCCE_ENET_RXB	((ushort)0x0001)	

#define SCC_PSMR_HBC	((ushort)0x8000)	
#define SCC_PSMR_FC	((ushort)0x4000)	
#define SCC_PSMR_RSH	((ushort)0x2000)	
#define SCC_PSMR_IAM	((ushort)0x1000)	
#define SCC_PSMR_ENCRC	((ushort)0x0800)	
#define SCC_PSMR_PRO	((ushort)0x0200)	
#define SCC_PSMR_BRO	((ushort)0x0100)	
#define SCC_PSMR_SBT	((ushort)0x0080)	
#define SCC_PSMR_LPB	((ushort)0x0040)	
#define SCC_PSMR_SIP	((ushort)0x0020)	
#define SCC_PSMR_LCW	((ushort)0x0010)	
#define SCC_PSMR_NIB22	((ushort)0x000a)	
#define SCC_PSMR_FDE	((ushort)0x0001)	

typedef struct scc_uart {
	sccp_t	scc_genscc;
	char	res1[8];	
	ushort	scc_maxidl;	
	ushort	scc_idlc;	
	ushort	scc_brkcr;	
	ushort	scc_parec;	
	ushort	scc_frmec;	
	ushort	scc_nosec;	
	ushort	scc_brkec;	
	ushort	scc_brkln;	
	ushort	scc_uaddr1;	
	ushort	scc_uaddr2;	
	ushort	scc_rtemp;	
	ushort	scc_toseq;	
	ushort	scc_char1;	
	ushort	scc_char2;	
	ushort	scc_char3;	
	ushort	scc_char4;	
	ushort	scc_char5;	
	ushort	scc_char6;	
	ushort	scc_char7;	
	ushort	scc_char8;	
	ushort	scc_rccm;	
	ushort	scc_rccr;	
	ushort	scc_rlbc;	
} scc_uart_t;

#define UART_SCCM_GLR		((ushort)0x1000)
#define UART_SCCM_GLT		((ushort)0x0800)
#define UART_SCCM_AB		((ushort)0x0200)
#define UART_SCCM_IDL		((ushort)0x0100)
#define UART_SCCM_GRA		((ushort)0x0080)
#define UART_SCCM_BRKE		((ushort)0x0040)
#define UART_SCCM_BRKS		((ushort)0x0020)
#define UART_SCCM_CCR		((ushort)0x0008)
#define UART_SCCM_BSY		((ushort)0x0004)
#define UART_SCCM_TX		((ushort)0x0002)
#define UART_SCCM_RX		((ushort)0x0001)

#define SCU_PSMR_FLC		((ushort)0x8000)
#define SCU_PSMR_SL		((ushort)0x4000)
#define SCU_PSMR_CL		((ushort)0x3000)
#define SCU_PSMR_UM		((ushort)0x0c00)
#define SCU_PSMR_FRZ		((ushort)0x0200)
#define SCU_PSMR_RZS		((ushort)0x0100)
#define SCU_PSMR_SYN		((ushort)0x0080)
#define SCU_PSMR_DRT		((ushort)0x0040)
#define SCU_PSMR_PEN		((ushort)0x0010)
#define SCU_PSMR_RPM		((ushort)0x000c)
#define SCU_PSMR_REVP		((ushort)0x0008)
#define SCU_PSMR_TPM		((ushort)0x0003)
#define SCU_PSMR_TEVP		((ushort)0x0002)

typedef struct scc_trans {
	sccp_t	st_genscc;
	uint	st_cpres;	
	uint	st_cmask;	
} scc_trans_t;

typedef struct iic {
	ushort	iic_rbase;	
	ushort	iic_tbase;	
	u_char	iic_rfcr;	
	u_char	iic_tfcr;	
	ushort	iic_mrblr;	
	uint	iic_rstate;	
	uint	iic_rdp;	
	ushort	iic_rbptr;	
	ushort	iic_rbc;	
	uint	iic_rxtmp;	
	uint	iic_tstate;	
	uint	iic_tdp;	
	ushort	iic_tbptr;	
	ushort	iic_tbc;	
	uint	iic_txtmp;	
	char	res1[4];	
	ushort	iic_rpbase;	
	char	res2[2];	
} iic_t;

#define RCCR_TIME	0x8000			
#define RCCR_TIMEP(t)	(((t) & 0x3F)<<8)	
#define RCCR_TIME_MASK	0x00FF			

#define PROFF_RTMR	((uint)0x01B0)

typedef struct risc_timer_pram {
	unsigned short	tm_base;	
	unsigned short	tm_ptr;		
	unsigned short	r_tmr;		
	unsigned short	r_tmv;		
	unsigned long	tm_cmd;		
	unsigned long	tm_cnt;		
} rt_pram_t;

#define TM_CMD_VALID	0x80000000	
#define TM_CMD_RESTART	0x40000000	
#define TM_CMD_PWM	0x20000000	
#define TM_CMD_NUM(n)	(((n)&0xF)<<16)	
#define TM_CMD_PERIOD(p) ((p)&0xFFFF)	

#define CPMVEC_NR		32
#define	CPMVEC_PIO_PC15		((ushort)0x1f)
#define	CPMVEC_SCC1		((ushort)0x1e)
#define	CPMVEC_SCC2		((ushort)0x1d)
#define	CPMVEC_SCC3		((ushort)0x1c)
#define	CPMVEC_SCC4		((ushort)0x1b)
#define	CPMVEC_PIO_PC14		((ushort)0x1a)
#define	CPMVEC_TIMER1		((ushort)0x19)
#define	CPMVEC_PIO_PC13		((ushort)0x18)
#define	CPMVEC_PIO_PC12		((ushort)0x17)
#define	CPMVEC_SDMA_CB_ERR	((ushort)0x16)
#define CPMVEC_IDMA1		((ushort)0x15)
#define CPMVEC_IDMA2		((ushort)0x14)
#define CPMVEC_TIMER2		((ushort)0x12)
#define CPMVEC_RISCTIMER	((ushort)0x11)
#define CPMVEC_I2C		((ushort)0x10)
#define	CPMVEC_PIO_PC11		((ushort)0x0f)
#define	CPMVEC_PIO_PC10		((ushort)0x0e)
#define CPMVEC_TIMER3		((ushort)0x0c)
#define	CPMVEC_PIO_PC9		((ushort)0x0b)
#define	CPMVEC_PIO_PC8		((ushort)0x0a)
#define	CPMVEC_PIO_PC7		((ushort)0x09)
#define CPMVEC_TIMER4		((ushort)0x07)
#define	CPMVEC_PIO_PC6		((ushort)0x06)
#define	CPMVEC_SPI		((ushort)0x05)
#define	CPMVEC_SMC1		((ushort)0x04)
#define	CPMVEC_SMC2		((ushort)0x03)
#define	CPMVEC_PIO_PC5		((ushort)0x02)
#define	CPMVEC_PIO_PC4		((ushort)0x01)
#define	CPMVEC_ERROR		((ushort)0x00)

#define	CICR_SCD_SCC4		((uint)0x00c00000)	
#define	CICR_SCC_SCC3		((uint)0x00200000)	
#define	CICR_SCB_SCC2		((uint)0x00040000)	
#define	CICR_SCA_SCC1		((uint)0x00000000)	
#define CICR_IRL_MASK		((uint)0x0000e000)	
#define CICR_HP_MASK		((uint)0x00001f00)	
#define CICR_IEN		((uint)0x00000080)	
#define CICR_SPS		((uint)0x00000001)	

#define CPM_PIN_INPUT     0
#define CPM_PIN_OUTPUT    1
#define CPM_PIN_PRIMARY   0
#define CPM_PIN_SECONDARY 2
#define CPM_PIN_GPIO      4
#define CPM_PIN_OPENDRAIN 8

enum cpm_port {
	CPM_PORTA,
	CPM_PORTB,
	CPM_PORTC,
	CPM_PORTD,
	CPM_PORTE,
};

void cpm1_set_pin(enum cpm_port port, int pin, int flags);

enum cpm_clk_dir {
	CPM_CLK_RX,
	CPM_CLK_TX,
	CPM_CLK_RTX
};

enum cpm_clk_target {
	CPM_CLK_SCC1,
	CPM_CLK_SCC2,
	CPM_CLK_SCC3,
	CPM_CLK_SCC4,
	CPM_CLK_SMC1,
	CPM_CLK_SMC2,
};

enum cpm_clk {
	CPM_BRG1,	
	CPM_BRG2,	
	CPM_BRG3,	
	CPM_BRG4,	
	CPM_CLK1,	
	CPM_CLK2,	
	CPM_CLK3,	
	CPM_CLK4,	
	CPM_CLK5,	
	CPM_CLK6,	
	CPM_CLK7,	
	CPM_CLK8,	
};

int cpm1_clk_setup(enum cpm_clk_target target, int clock, int mode);

#endif 
