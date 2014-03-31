#ifdef __KERNEL__
#ifndef __CPM2__
#define __CPM2__

#include <asm/immap_cpm2.h>
#include <asm/cpm.h>
#include <sysdev/fsl_soc.h>

#define CPM_CR_RST	((uint)0x80000000)
#define CPM_CR_PAGE	((uint)0x7c000000)
#define CPM_CR_SBLOCK	((uint)0x03e00000)
#define CPM_CR_FLG	((uint)0x00010000)
#define CPM_CR_MCN	((uint)0x00003fc0)
#define CPM_CR_OPCODE	((uint)0x0000000f)

#define CPM_CR_SCC1_SBLOCK	(0x04)
#define CPM_CR_SCC2_SBLOCK	(0x05)
#define CPM_CR_SCC3_SBLOCK	(0x06)
#define CPM_CR_SCC4_SBLOCK	(0x07)
#define CPM_CR_SMC1_SBLOCK	(0x08)
#define CPM_CR_SMC2_SBLOCK	(0x09)
#define CPM_CR_SPI_SBLOCK	(0x0a)
#define CPM_CR_I2C_SBLOCK	(0x0b)
#define CPM_CR_TIMER_SBLOCK	(0x0f)
#define CPM_CR_RAND_SBLOCK	(0x0e)
#define CPM_CR_FCC1_SBLOCK	(0x10)
#define CPM_CR_FCC2_SBLOCK	(0x11)
#define CPM_CR_FCC3_SBLOCK	(0x12)
#define CPM_CR_IDMA1_SBLOCK	(0x14)
#define CPM_CR_IDMA2_SBLOCK	(0x15)
#define CPM_CR_IDMA3_SBLOCK	(0x16)
#define CPM_CR_IDMA4_SBLOCK	(0x17)
#define CPM_CR_MCC1_SBLOCK	(0x1c)

#define CPM_CR_FCC_SBLOCK(x)	(x + 0x10)

#define CPM_CR_SCC1_PAGE	(0x00)
#define CPM_CR_SCC2_PAGE	(0x01)
#define CPM_CR_SCC3_PAGE	(0x02)
#define CPM_CR_SCC4_PAGE	(0x03)
#define CPM_CR_SMC1_PAGE	(0x07)
#define CPM_CR_SMC2_PAGE	(0x08)
#define CPM_CR_SPI_PAGE		(0x09)
#define CPM_CR_I2C_PAGE		(0x0a)
#define CPM_CR_TIMER_PAGE	(0x0a)
#define CPM_CR_RAND_PAGE	(0x0a)
#define CPM_CR_FCC1_PAGE	(0x04)
#define CPM_CR_FCC2_PAGE	(0x05)
#define CPM_CR_FCC3_PAGE	(0x06)
#define CPM_CR_IDMA1_PAGE	(0x07)
#define CPM_CR_IDMA2_PAGE	(0x08)
#define CPM_CR_IDMA3_PAGE	(0x09)
#define CPM_CR_IDMA4_PAGE	(0x0a)
#define CPM_CR_MCC1_PAGE	(0x07)
#define CPM_CR_MCC2_PAGE	(0x08)

#define CPM_CR_FCC_PAGE(x)	(x + 0x04)

#define CPM_CR_START_IDMA	((ushort)0x0009)

#define mk_cr_cmd(PG, SBC, MCN, OP) \
	((PG << 26) | (SBC << 21) | (MCN << 6) | OP)

#define NUM_CPM_HOST_PAGES	2

extern cpm_cpm2_t __iomem *cpmp; 

#define cpm_dpalloc cpm_muram_alloc
#define cpm_dpfree cpm_muram_free
#define cpm_dpram_addr cpm_muram_addr

extern void cpm2_reset(void);

#define CPM_BRG_RST		((uint)0x00020000)
#define CPM_BRG_EN		((uint)0x00010000)
#define CPM_BRG_EXTC_INT	((uint)0x00000000)
#define CPM_BRG_EXTC_CLK3_9	((uint)0x00004000)
#define CPM_BRG_EXTC_CLK5_15	((uint)0x00008000)
#define CPM_BRG_ATB		((uint)0x00002000)
#define CPM_BRG_CD_MASK		((uint)0x00001ffe)
#define CPM_BRG_DIV16		((uint)0x00000001)

#define CPM2_BRG_INT_CLK	(get_brgfreq())
#define CPM2_BRG_UART_CLK	(CPM2_BRG_INT_CLK/16)

extern void __cpm2_setbrg(uint brg, uint rate, uint clk, int div16, int src);

static inline void cpm_setbrg(uint brg, uint rate)
{
	__cpm2_setbrg(brg, rate, CPM2_BRG_UART_CLK, 0, CPM_BRG_EXTC_INT);
}

static inline void cpm2_fastbrg(uint brg, uint rate, int div16)
{
	__cpm2_setbrg(brg, rate, CPM2_BRG_INT_CLK, div16, CPM_BRG_EXTC_INT);
}

#define PROFF_SCC1		((uint)0x8000)
#define PROFF_SCC2		((uint)0x8100)
#define PROFF_SCC3		((uint)0x8200)
#define PROFF_SCC4		((uint)0x8300)
#define PROFF_FCC1		((uint)0x8400)
#define PROFF_FCC2		((uint)0x8500)
#define PROFF_FCC3		((uint)0x8600)
#define PROFF_MCC1		((uint)0x8700)
#define PROFF_SMC1_BASE		((uint)0x87fc)
#define PROFF_IDMA1_BASE	((uint)0x87fe)
#define PROFF_MCC2		((uint)0x8800)
#define PROFF_SMC2_BASE		((uint)0x88fc)
#define PROFF_IDMA2_BASE	((uint)0x88fe)
#define PROFF_SPI_BASE		((uint)0x89fc)
#define PROFF_IDMA3_BASE	((uint)0x89fe)
#define PROFF_TIMERS		((uint)0x8ae0)
#define PROFF_REVNUM		((uint)0x8af0)
#define PROFF_RAND		((uint)0x8af8)
#define PROFF_I2C_BASE		((uint)0x8afc)
#define PROFF_IDMA4_BASE	((uint)0x8afe)

#define PROFF_SCC_SIZE		((uint)0x100)
#define PROFF_FCC_SIZE		((uint)0x100)
#define PROFF_SMC_SIZE		((uint)64)

#define PROFF_SMC1	(0)
#define PROFF_SMC2	(64)


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
	uint	smc_stmp;	
} smc_uart_t;

#define SMCMR_REN	((ushort)0x0001)
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

#define SMCM_BRKE       ((unsigned char)0x40)   
#define SMCM_BRK        ((unsigned char)0x10)   
#define SMCM_TXE	((unsigned char)0x10)
#define SMCM_BSY	((unsigned char)0x04)
#define SMCM_TX		((unsigned char)0x02)
#define SMCM_RX		((unsigned char)0x01)

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

#define SCCM_TXE	((unsigned char)0x10)
#define SCCM_BSY	((unsigned char)0x04)
#define SCCM_TX		((unsigned char)0x02)
#define SCCM_RX		((unsigned char)0x01)

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

#define SCC_EB	((u_char) 0x10)	
#define SCC_GBL	((u_char) 0x20) 

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
	uint	scc_res1;	
	uint	scc_res2;	
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

#define FCC_GFMR_DIAG_NORM	((uint)0x00000000)
#define FCC_GFMR_DIAG_LE	((uint)0x40000000)
#define FCC_GFMR_DIAG_AE	((uint)0x80000000)
#define FCC_GFMR_DIAG_ALE	((uint)0xc0000000)
#define FCC_GFMR_TCI		((uint)0x20000000)
#define FCC_GFMR_TRX		((uint)0x10000000)
#define FCC_GFMR_TTX		((uint)0x08000000)
#define FCC_GFMR_TTX		((uint)0x08000000)
#define FCC_GFMR_CDP		((uint)0x04000000)
#define FCC_GFMR_CTSP		((uint)0x02000000)
#define FCC_GFMR_CDS		((uint)0x01000000)
#define FCC_GFMR_CTSS		((uint)0x00800000)
#define FCC_GFMR_SYNL_NONE	((uint)0x00000000)
#define FCC_GFMR_SYNL_AUTO	((uint)0x00004000)
#define FCC_GFMR_SYNL_8		((uint)0x00008000)
#define FCC_GFMR_SYNL_16	((uint)0x0000c000)
#define FCC_GFMR_RTSM		((uint)0x00002000)
#define FCC_GFMR_RENC_NRZ	((uint)0x00000000)
#define FCC_GFMR_RENC_NRZI	((uint)0x00000800)
#define FCC_GFMR_REVD		((uint)0x00000400)
#define FCC_GFMR_TENC_NRZ	((uint)0x00000000)
#define FCC_GFMR_TENC_NRZI	((uint)0x00000100)
#define FCC_GFMR_TCRC_16	((uint)0x00000000)
#define FCC_GFMR_TCRC_32	((uint)0x00000080)
#define FCC_GFMR_ENR		((uint)0x00000020)
#define FCC_GFMR_ENT		((uint)0x00000010)
#define FCC_GFMR_MODE_ENET	((uint)0x0000000c)
#define FCC_GFMR_MODE_ATM	((uint)0x0000000a)
#define FCC_GFMR_MODE_HDLC	((uint)0x00000000)

typedef struct fcc_param {
	ushort	fcc_riptr;	
	ushort	fcc_tiptr;	
	ushort	fcc_res1;
	ushort	fcc_mrblr;	
	uint	fcc_rstate;	
	uint	fcc_rbase;	
	ushort	fcc_rbdstat;	
	ushort	fcc_rbdlen;	
	uint	fcc_rdptr;	
	uint	fcc_tstate;	
	uint	fcc_tbase;	
	ushort	fcc_tbdstat;	
	ushort	fcc_tbdlen;	
	uint	fcc_tdptr;	
	uint	fcc_rbptr;	
	uint	fcc_tbptr;	
	uint	fcc_rcrc;	
	uint	fcc_res2;
	uint	fcc_tcrc;	
} fccp_t;


typedef struct fcc_enet {
	fccp_t	fen_genfcc;
	uint	fen_statbuf;	
	uint	fen_camptr;	
	uint	fen_cmask;	
	uint	fen_cpres;	
	uint	fen_crcec;	
	uint	fen_alec;	
	uint	fen_disfc;	
	ushort	fen_retlim;	
	ushort	fen_retcnt;	
	ushort	fen_pper;	
	ushort	fen_boffcnt;	
	uint	fen_gaddrh;	
	uint	fen_gaddrl;	
	ushort	fen_tfcstat;	
	ushort	fen_tfclen;
	uint	fen_tfcptr;
	ushort	fen_mflr;	
	ushort	fen_paddrh;	
	ushort	fen_paddrm;
	ushort	fen_paddrl;
	ushort	fen_ibdcount;	
	ushort	fen_ibdstart;	
	ushort	fen_ibdend;	
	ushort	fen_txlen;	
	uint	fen_ibdbase[8]; 
	uint	fen_iaddrh;	
	uint	fen_iaddrl;
	ushort	fen_minflr;	
	ushort	fen_taddrh;	
	ushort	fen_taddrm;
	ushort	fen_taddrl;
	ushort	fen_padptr;	
	ushort	fen_cftype;	
	ushort	fen_cfrange;	
	ushort	fen_maxb;	
	ushort	fen_maxd1;	
	ushort	fen_maxd2;	
	ushort	fen_maxd;	
	ushort	fen_dmacnt;	
	uint	fen_octc;	
	uint	fen_colc;	
	uint	fen_broc;	
	uint	fen_mulc;	
	uint	fen_uspc;	
	uint	fen_frgc;	
	uint	fen_ospc;	
	uint	fen_jbrc;	
	uint	fen_p64c;	
	uint	fen_p65c;	
	uint	fen_p128c;	
	uint	fen_p256c;	
	uint	fen_p512c;	
	uint	fen_p1024c;	
	uint	fen_cambuf;	
	ushort	fen_rfthr;	
	ushort	fen_rfcnt;	
} fcc_enet_t;

#define FCC_ENET_GRA	((ushort)0x0080)	
#define FCC_ENET_RXC	((ushort)0x0040)	
#define FCC_ENET_TXC	((ushort)0x0020)	
#define FCC_ENET_TXE	((ushort)0x0010)	
#define FCC_ENET_RXF	((ushort)0x0008)	
#define FCC_ENET_BSY	((ushort)0x0004)	
#define FCC_ENET_TXB	((ushort)0x0002)	
#define FCC_ENET_RXB	((ushort)0x0001)	

#define FCC_PSMR_HBC	((uint)0x80000000)	
#define FCC_PSMR_FC	((uint)0x40000000)	
#define FCC_PSMR_SBT	((uint)0x20000000)	
#define FCC_PSMR_LPB	((uint)0x10000000)	
#define FCC_PSMR_LCW	((uint)0x08000000)	
#define FCC_PSMR_FDE	((uint)0x04000000)	
#define FCC_PSMR_MON	((uint)0x02000000)	
#define FCC_PSMR_PRO	((uint)0x00400000)	
#define FCC_PSMR_FCE	((uint)0x00200000)	
#define FCC_PSMR_RSH	((uint)0x00100000)	
#define FCC_PSMR_CAM	((uint)0x00000400)	
#define FCC_PSMR_BRO	((uint)0x00000200)	
#define FCC_PSMR_ENCRC	((uint)0x00000080)	

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
} iic_t;

typedef struct idma {
	ushort ibase;		
	ushort dcm;		
	ushort ibdptr;		
	ushort dpr_buf;		
	ushort buf_inv;		
	ushort ss_max;		
	ushort dpr_in_ptr;	
	ushort sts;		
	ushort dpr_out_ptr;	
	ushort seob;		
	ushort deob;		
	ushort dts;		
	ushort ret_add;		
	ushort res0;		
	uint   bd_cnt;		
	uint   s_ptr;		
	uint   d_ptr;		
	uint   istate;		
	u_char res1[20];	
} idma_t;

#define IDMA_DCM_FB		((ushort)0x8000) 
#define IDMA_DCM_LP		((ushort)0x4000) 
#define IDMA_DCM_TC2		((ushort)0x0400) 
#define IDMA_DCM_DMA_WRAP_MASK	((ushort)0x01c0) 
#define IDMA_DCM_DMA_WRAP_64	((ushort)0x0000) 
#define IDMA_DCM_DMA_WRAP_128	((ushort)0x0040) 
#define IDMA_DCM_DMA_WRAP_256	((ushort)0x0080) 
#define IDMA_DCM_DMA_WRAP_512	((ushort)0x00c0) 
#define IDMA_DCM_DMA_WRAP_1024	((ushort)0x0100) 
#define IDMA_DCM_DMA_WRAP_2048	((ushort)0x0140) 
#define IDMA_DCM_SINC		((ushort)0x0020) 
#define IDMA_DCM_DINC		((ushort)0x0010) 
#define IDMA_DCM_ERM		((ushort)0x0008) 
#define IDMA_DCM_DT		((ushort)0x0004) 
#define IDMA_DCM_SD_MASK	((ushort)0x0003) 
#define IDMA_DCM_SD_MEM2MEM	((ushort)0x0000) 
#define IDMA_DCM_SD_PER2MEM	((ushort)0x0002) 
#define IDMA_DCM_SD_MEM2PER	((ushort)0x0001) 

typedef struct idma_bd {
	uint flags;
	uint len;	
	uint src;	
	uint dst;	
} idma_bd_t;

#define IDMA_BD_V	((uint)0x80000000)	
#define IDMA_BD_W	((uint)0x20000000)	
#define IDMA_BD_I	((uint)0x10000000)	
#define IDMA_BD_L	((uint)0x08000000)	
#define IDMA_BD_CM	((uint)0x02000000)	
#define IDMA_BD_SDN	((uint)0x00400000)	
#define IDMA_BD_DDN	((uint)0x00200000)	
#define IDMA_BD_DGBL	((uint)0x00100000)	
#define IDMA_BD_DBO_LE	((uint)0x00040000)	
#define IDMA_BD_DBO_BE	((uint)0x00080000)	
#define IDMA_BD_DDTB	((uint)0x00010000)	
#define IDMA_BD_SGBL	((uint)0x00002000)	
#define IDMA_BD_SBO_LE	((uint)0x00000800)	
#define IDMA_BD_SBO_BE	((uint)0x00001000)	
#define IDMA_BD_SDTB	((uint)0x00000200)	

typedef struct im_idma {
	u_char idsr;			
	u_char res0[3];
	u_char idmr;			
	u_char res1[3];
} im_idma_t;

#define IDMA_EVENT_SC	((unsigned char)0x08)	
#define IDMA_EVENT_OB	((unsigned char)0x04)	
#define IDMA_EVENT_EDN	((unsigned char)0x02)	
#define IDMA_EVENT_BC	((unsigned char)0x01)	

#define RCCR_TIME	((uint)0x80000000) 
#define RCCR_TIMEP_MASK	((uint)0x3f000000) 
#define RCCR_DR0M	((uint)0x00800000) 
#define RCCR_DR1M	((uint)0x00400000) 
#define RCCR_DR2M	((uint)0x00000080) 
#define RCCR_DR3M	((uint)0x00000040) 
#define RCCR_DR0QP_MASK	((uint)0x00300000) 
#define RCCR_DR0QP_HIGH ((uint)0x00000000) 
#define RCCR_DR0QP_MED	((uint)0x00100000) 
#define RCCR_DR0QP_LOW	((uint)0x00200000) 
#define RCCR_DR1QP_MASK	((uint)0x00030000) 
#define RCCR_DR1QP_HIGH ((uint)0x00000000) 
#define RCCR_DR1QP_MED	((uint)0x00010000) 
#define RCCR_DR1QP_LOW	((uint)0x00020000) 
#define RCCR_DR2QP_MASK	((uint)0x00000030) 
#define RCCR_DR2QP_HIGH ((uint)0x00000000) 
#define RCCR_DR2QP_MED	((uint)0x00000010) 
#define RCCR_DR2QP_LOW	((uint)0x00000020) 
#define RCCR_DR3QP_MASK	((uint)0x00000003) 
#define RCCR_DR3QP_HIGH ((uint)0x00000000) 
#define RCCR_DR3QP_MED	((uint)0x00000001) 
#define RCCR_DR3QP_LOW	((uint)0x00000002) 
#define RCCR_EIE	((uint)0x00080000) 
#define RCCR_SCD	((uint)0x00040000) 
#define RCCR_ERAM_MASK	((uint)0x0000e000) 
#define RCCR_ERAM_0KB	((uint)0x00000000) 
#define RCCR_ERAM_2KB	((uint)0x00002000) 
#define RCCR_ERAM_4KB	((uint)0x00004000) 
#define RCCR_ERAM_6KB	((uint)0x00006000) 
#define RCCR_ERAM_8KB	((uint)0x00008000) 
#define RCCR_ERAM_10KB	((uint)0x0000a000) 
#define RCCR_ERAM_12KB	((uint)0x0000c000) 
#define RCCR_EDM0	((uint)0x00000800) 
#define RCCR_EDM1	((uint)0x00000400) 
#define RCCR_EDM2	((uint)0x00000200) 
#define RCCR_EDM3	((uint)0x00000100) 
#define RCCR_DEM01	((uint)0x00000008) 
#define RCCR_DEM23	((uint)0x00000004) 

#define CMXFCR_FC1         0x40000000   
#define CMXFCR_RF1CS_MSK   0x38000000   
#define CMXFCR_TF1CS_MSK   0x07000000   
#define CMXFCR_FC2         0x00400000   
#define CMXFCR_RF2CS_MSK   0x00380000   
#define CMXFCR_TF2CS_MSK   0x00070000   
#define CMXFCR_FC3         0x00004000   
#define CMXFCR_RF3CS_MSK   0x00003800   
#define CMXFCR_TF3CS_MSK   0x00000700   

#define CMXFCR_RF1CS_BRG5  0x00000000   
#define CMXFCR_RF1CS_BRG6  0x08000000   
#define CMXFCR_RF1CS_BRG7  0x10000000   
#define CMXFCR_RF1CS_BRG8  0x18000000   
#define CMXFCR_RF1CS_CLK9  0x20000000   
#define CMXFCR_RF1CS_CLK10 0x28000000   
#define CMXFCR_RF1CS_CLK11 0x30000000   
#define CMXFCR_RF1CS_CLK12 0x38000000   

#define CMXFCR_TF1CS_BRG5  0x00000000   
#define CMXFCR_TF1CS_BRG6  0x01000000   
#define CMXFCR_TF1CS_BRG7  0x02000000   
#define CMXFCR_TF1CS_BRG8  0x03000000   
#define CMXFCR_TF1CS_CLK9  0x04000000   
#define CMXFCR_TF1CS_CLK10 0x05000000   
#define CMXFCR_TF1CS_CLK11 0x06000000   
#define CMXFCR_TF1CS_CLK12 0x07000000   

#define CMXFCR_RF2CS_BRG5  0x00000000   
#define CMXFCR_RF2CS_BRG6  0x00080000   
#define CMXFCR_RF2CS_BRG7  0x00100000   
#define CMXFCR_RF2CS_BRG8  0x00180000   
#define CMXFCR_RF2CS_CLK13 0x00200000   
#define CMXFCR_RF2CS_CLK14 0x00280000   
#define CMXFCR_RF2CS_CLK15 0x00300000   
#define CMXFCR_RF2CS_CLK16 0x00380000   

#define CMXFCR_TF2CS_BRG5  0x00000000   
#define CMXFCR_TF2CS_BRG6  0x00010000   
#define CMXFCR_TF2CS_BRG7  0x00020000   
#define CMXFCR_TF2CS_BRG8  0x00030000   
#define CMXFCR_TF2CS_CLK13 0x00040000   
#define CMXFCR_TF2CS_CLK14 0x00050000   
#define CMXFCR_TF2CS_CLK15 0x00060000   
#define CMXFCR_TF2CS_CLK16 0x00070000   

#define CMXFCR_RF3CS_BRG5  0x00000000   
#define CMXFCR_RF3CS_BRG6  0x00000800   
#define CMXFCR_RF3CS_BRG7  0x00001000   
#define CMXFCR_RF3CS_BRG8  0x00001800   
#define CMXFCR_RF3CS_CLK13 0x00002000   
#define CMXFCR_RF3CS_CLK14 0x00002800   
#define CMXFCR_RF3CS_CLK15 0x00003000   
#define CMXFCR_RF3CS_CLK16 0x00003800   

#define CMXFCR_TF3CS_BRG5  0x00000000   
#define CMXFCR_TF3CS_BRG6  0x00000100   
#define CMXFCR_TF3CS_BRG7  0x00000200   
#define CMXFCR_TF3CS_BRG8  0x00000300   
#define CMXFCR_TF3CS_CLK13 0x00000400   
#define CMXFCR_TF3CS_CLK14 0x00000500   
#define CMXFCR_TF3CS_CLK15 0x00000600   
#define CMXFCR_TF3CS_CLK16 0x00000700   

#define CMXSCR_GR1         0x80000000   
#define CMXSCR_SC1         0x40000000   
#define CMXSCR_RS1CS_MSK   0x38000000   
#define CMXSCR_TS1CS_MSK   0x07000000   
#define CMXSCR_GR2         0x00800000   
#define CMXSCR_SC2         0x00400000   
#define CMXSCR_RS2CS_MSK   0x00380000   
#define CMXSCR_TS2CS_MSK   0x00070000   
#define CMXSCR_GR3         0x00008000   
#define CMXSCR_SC3         0x00004000   
#define CMXSCR_RS3CS_MSK   0x00003800   
#define CMXSCR_TS3CS_MSK   0x00000700   
#define CMXSCR_GR4         0x00000080   
#define CMXSCR_SC4         0x00000040   
#define CMXSCR_RS4CS_MSK   0x00000038   
#define CMXSCR_TS4CS_MSK   0x00000007   

#define CMXSCR_RS1CS_BRG1  0x00000000   
#define CMXSCR_RS1CS_BRG2  0x08000000   
#define CMXSCR_RS1CS_BRG3  0x10000000   
#define CMXSCR_RS1CS_BRG4  0x18000000   
#define CMXSCR_RS1CS_CLK11 0x20000000   
#define CMXSCR_RS1CS_CLK12 0x28000000   
#define CMXSCR_RS1CS_CLK3  0x30000000   
#define CMXSCR_RS1CS_CLK4  0x38000000   

#define CMXSCR_TS1CS_BRG1  0x00000000   
#define CMXSCR_TS1CS_BRG2  0x01000000   
#define CMXSCR_TS1CS_BRG3  0x02000000   
#define CMXSCR_TS1CS_BRG4  0x03000000   
#define CMXSCR_TS1CS_CLK11 0x04000000   
#define CMXSCR_TS1CS_CLK12 0x05000000   
#define CMXSCR_TS1CS_CLK3  0x06000000   
#define CMXSCR_TS1CS_CLK4  0x07000000   

#define CMXSCR_RS2CS_BRG1  0x00000000   
#define CMXSCR_RS2CS_BRG2  0x00080000   
#define CMXSCR_RS2CS_BRG3  0x00100000   
#define CMXSCR_RS2CS_BRG4  0x00180000   
#define CMXSCR_RS2CS_CLK11 0x00200000   
#define CMXSCR_RS2CS_CLK12 0x00280000   
#define CMXSCR_RS2CS_CLK3  0x00300000   
#define CMXSCR_RS2CS_CLK4  0x00380000   

#define CMXSCR_TS2CS_BRG1  0x00000000   
#define CMXSCR_TS2CS_BRG2  0x00010000   
#define CMXSCR_TS2CS_BRG3  0x00020000   
#define CMXSCR_TS2CS_BRG4  0x00030000   
#define CMXSCR_TS2CS_CLK11 0x00040000   
#define CMXSCR_TS2CS_CLK12 0x00050000   
#define CMXSCR_TS2CS_CLK3  0x00060000   
#define CMXSCR_TS2CS_CLK4  0x00070000   

#define CMXSCR_RS3CS_BRG1  0x00000000   
#define CMXSCR_RS3CS_BRG2  0x00000800   
#define CMXSCR_RS3CS_BRG3  0x00001000   
#define CMXSCR_RS3CS_BRG4  0x00001800   
#define CMXSCR_RS3CS_CLK5  0x00002000   
#define CMXSCR_RS3CS_CLK6  0x00002800   
#define CMXSCR_RS3CS_CLK7  0x00003000   
#define CMXSCR_RS3CS_CLK8  0x00003800   

#define CMXSCR_TS3CS_BRG1  0x00000000   
#define CMXSCR_TS3CS_BRG2  0x00000100   
#define CMXSCR_TS3CS_BRG3  0x00000200   
#define CMXSCR_TS3CS_BRG4  0x00000300   
#define CMXSCR_TS3CS_CLK5  0x00000400   
#define CMXSCR_TS3CS_CLK6  0x00000500   
#define CMXSCR_TS3CS_CLK7  0x00000600   
#define CMXSCR_TS3CS_CLK8  0x00000700   

#define CMXSCR_RS4CS_BRG1  0x00000000   
#define CMXSCR_RS4CS_BRG2  0x00000008   
#define CMXSCR_RS4CS_BRG3  0x00000010   
#define CMXSCR_RS4CS_BRG4  0x00000018   
#define CMXSCR_RS4CS_CLK5  0x00000020   
#define CMXSCR_RS4CS_CLK6  0x00000028   
#define CMXSCR_RS4CS_CLK7  0x00000030   
#define CMXSCR_RS4CS_CLK8  0x00000038   

#define CMXSCR_TS4CS_BRG1  0x00000000   
#define CMXSCR_TS4CS_BRG2  0x00000001   
#define CMXSCR_TS4CS_BRG3  0x00000002   
#define CMXSCR_TS4CS_BRG4  0x00000003   
#define CMXSCR_TS4CS_CLK5  0x00000004   
#define CMXSCR_TS4CS_CLK6  0x00000005   
#define CMXSCR_TS4CS_CLK7  0x00000006   
#define CMXSCR_TS4CS_CLK8  0x00000007   

#define SIUMCR_BBD	0x80000000	
#define SIUMCR_ESE	0x40000000	
#define SIUMCR_PBSE	0x20000000	
#define SIUMCR_CDIS	0x10000000	
#define SIUMCR_DPPC00	0x00000000	
#define SIUMCR_DPPC01	0x04000000	
#define SIUMCR_DPPC10	0x08000000	
#define SIUMCR_DPPC11	0x0c000000	
#define SIUMCR_L2CPC00	0x00000000	
#define SIUMCR_L2CPC01	0x01000000	
#define SIUMCR_L2CPC10	0x02000000	
#define SIUMCR_L2CPC11	0x03000000	
#define SIUMCR_LBPC00	0x00000000	
#define SIUMCR_LBPC01	0x00400000	
#define SIUMCR_LBPC10	0x00800000	
#define SIUMCR_LBPC11	0x00c00000	
#define SIUMCR_APPC00	0x00000000	
#define SIUMCR_APPC01	0x00100000	
#define SIUMCR_APPC10	0x00200000	
#define SIUMCR_APPC11	0x00300000	
#define SIUMCR_CS10PC00	0x00000000	
#define SIUMCR_CS10PC01	0x00040000	
#define SIUMCR_CS10PC10	0x00080000	
#define SIUMCR_CS10PC11	0x000c0000	
#define SIUMCR_BCTLC00	0x00000000	
#define SIUMCR_BCTLC01	0x00010000	
#define SIUMCR_BCTLC10	0x00020000	
#define SIUMCR_BCTLC11	0x00030000	
#define SIUMCR_MMR00	0x00000000	
#define SIUMCR_MMR01	0x00004000	
#define SIUMCR_MMR10	0x00008000	
#define SIUMCR_MMR11	0x0000c000	
#define SIUMCR_LPBSE	0x00002000	

#define SCCR_PCI_MODE	0x00000100	
#define SCCR_PCI_MODCK	0x00000080	
#define SCCR_PCIDF_MSK	0x00000078	
#define SCCR_PCIDF_SHIFT 3

#ifndef CPM_IMMR_OFFSET
#define CPM_IMMR_OFFSET	0x101a8
#endif

#define FCC_PSMR_RMII	((uint)0x00020000)	


#define PC_CLK(x)	((uint)(1<<(x-1)))	

#define CMXFCR_RF1CS(x)	((uint)((x-5)<<27))	
#define CMXFCR_TF1CS(x)	((uint)((x-5)<<24))	
#define CMXFCR_RF2CS(x)	((uint)((x-9)<<19))	
#define CMXFCR_TF2CS(x) ((uint)((x-9)<<16))	
#define CMXFCR_RF3CS(x)	((uint)((x-9)<<11))	
#define CMXFCR_TF3CS(x) ((uint)((x-9)<<8))	

#define PC_F1RXCLK	PC_CLK(F1_RXCLK)
#define PC_F1TXCLK	PC_CLK(F1_TXCLK)
#define CMX1_CLK_ROUTE	(CMXFCR_RF1CS(F1_RXCLK) | CMXFCR_TF1CS(F1_TXCLK))
#define CMX1_CLK_MASK	((uint)0xff000000)

#define PC_F2RXCLK	PC_CLK(F2_RXCLK)
#define PC_F2TXCLK	PC_CLK(F2_TXCLK)
#define CMX2_CLK_ROUTE	(CMXFCR_RF2CS(F2_RXCLK) | CMXFCR_TF2CS(F2_TXCLK))
#define CMX2_CLK_MASK	((uint)0x00ff0000)

#define PC_F3RXCLK	PC_CLK(F3_RXCLK)
#define PC_F3TXCLK	PC_CLK(F3_TXCLK)
#define CMX3_CLK_ROUTE	(CMXFCR_RF3CS(F3_RXCLK) | CMXFCR_TF3CS(F3_TXCLK))
#define CMX3_CLK_MASK	((uint)0x0000ff00)

#define CPMUX_CLK_MASK (CMX3_CLK_MASK | CMX2_CLK_MASK)
#define CPMUX_CLK_ROUTE (CMX3_CLK_ROUTE | CMX2_CLK_ROUTE)

#define CLK_TRX (PC_F3TXCLK | PC_F3RXCLK | PC_F2TXCLK | PC_F2RXCLK)

#define PA1_COL		0x00000001U
#define PA1_CRS		0x00000002U
#define PA1_TXER	0x00000004U
#define PA1_TXEN	0x00000008U
#define PA1_RXDV	0x00000010U
#define PA1_RXER	0x00000020U
#define PA1_TXDAT	0x00003c00U
#define PA1_RXDAT	0x0003c000U
#define PA1_PSORA0	(PA1_RXDAT | PA1_TXDAT)
#define PA1_PSORA1	(PA1_COL | PA1_CRS | PA1_TXER | PA1_TXEN | \
		PA1_RXDV | PA1_RXER)
#define PA1_DIRA0	(PA1_RXDAT | PA1_CRS | PA1_COL | PA1_RXER | PA1_RXDV)
#define PA1_DIRA1	(PA1_TXDAT | PA1_TXEN | PA1_TXER)


#define PB2_TXER	0x00000001U
#define PB2_RXDV	0x00000002U
#define PB2_TXEN	0x00000004U
#define PB2_RXER	0x00000008U
#define PB2_COL		0x00000010U
#define PB2_CRS		0x00000020U
#define PB2_TXDAT	0x000003c0U
#define PB2_RXDAT	0x00003c00U
#define PB2_PSORB0	(PB2_RXDAT | PB2_TXDAT | PB2_CRS | PB2_COL | \
		PB2_RXER | PB2_RXDV | PB2_TXER)
#define PB2_PSORB1	(PB2_TXEN)
#define PB2_DIRB0	(PB2_RXDAT | PB2_CRS | PB2_COL | PB2_RXER | PB2_RXDV)
#define PB2_DIRB1	(PB2_TXDAT | PB2_TXEN | PB2_TXER)


#define PB3_RXDV	0x00004000U
#define PB3_RXER	0x00008000U
#define PB3_TXER	0x00010000U
#define PB3_TXEN	0x00020000U
#define PB3_COL		0x00040000U
#define PB3_CRS		0x00080000U
#define PB3_TXDAT	0x0f000000U
#define PC3_TXDAT	0x00000010U
#define PB3_RXDAT	0x00f00000U
#define PB3_PSORB0	(PB3_RXDAT | PB3_TXDAT | PB3_CRS | PB3_COL | \
		PB3_RXER | PB3_RXDV | PB3_TXER | PB3_TXEN)
#define PB3_PSORB1	0
#define PB3_DIRB0	(PB3_RXDAT | PB3_CRS | PB3_COL | PB3_RXER | PB3_RXDV)
#define PB3_DIRB1	(PB3_TXDAT | PB3_TXEN | PB3_TXER)
#define PC3_DIRC1	(PC3_TXDAT)

#define FCC_MEM_OFFSET(x) (CPM_FCC_SPECIAL_BASE + (x*128))
#define FCC1_MEM_OFFSET FCC_MEM_OFFSET(0)
#define FCC2_MEM_OFFSET FCC_MEM_OFFSET(1)
#define FCC3_MEM_OFFSET FCC_MEM_OFFSET(2)


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
	CPM_CLK_FCC1,
	CPM_CLK_FCC2,
	CPM_CLK_FCC3,
	CPM_CLK_SMC1,
	CPM_CLK_SMC2,
};

enum cpm_clk {
	CPM_CLK_NONE = 0,
	CPM_BRG1,	
	CPM_BRG2,	
	CPM_BRG3,	
	CPM_BRG4,	
	CPM_BRG5,	
	CPM_BRG6,	
	CPM_BRG7,	
	CPM_BRG8,	
	CPM_CLK1,	
	CPM_CLK2,	
	CPM_CLK3,	
	CPM_CLK4,	
	CPM_CLK5,	
	CPM_CLK6,	
	CPM_CLK7,	
	CPM_CLK8,	
	CPM_CLK9,	
	CPM_CLK10,	
	CPM_CLK11,	
	CPM_CLK12,	
	CPM_CLK13,	
	CPM_CLK14,	
	CPM_CLK15,	
	CPM_CLK16,	
	CPM_CLK17,	
	CPM_CLK18,	
	CPM_CLK19,	
	CPM_CLK20,	
	CPM_CLK_DUMMY
};

extern int cpm2_clk_setup(enum cpm_clk_target target, int clock, int mode);
extern int cpm2_smc_clk_setup(enum cpm_clk_target target, int clock);

#define CPM_PIN_INPUT     0
#define CPM_PIN_OUTPUT    1
#define CPM_PIN_PRIMARY   0
#define CPM_PIN_SECONDARY 2
#define CPM_PIN_GPIO      4
#define CPM_PIN_OPENDRAIN 8

void cpm2_set_pin(int port, int pin, int flags);

#endif 
#endif 
