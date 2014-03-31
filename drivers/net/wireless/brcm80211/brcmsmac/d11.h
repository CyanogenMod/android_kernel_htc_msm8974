/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_BRCM_D11_H_
#define	_BRCM_D11_H_

#include <linux/ieee80211.h>

#include <defs.h>
#include "pub.h"
#include "dma.h"

#define	RX_FIFO			0	
#define	RX_TXSTATUS_FIFO	3	

#define	TX_AC_BK_FIFO		0	
#define	TX_AC_BE_FIFO		1	
#define	TX_AC_VI_FIFO		2	
#define	TX_AC_VO_FIFO		3	
#define	TX_BCMC_FIFO		4	
#define	TX_ATIM_FIFO		5	


#define M_AC_TXLMT_BASE_ADDR         (0x180 * 2)
#define M_AC_TXLMT_ADDR(_ac)         (M_AC_TXLMT_BASE_ADDR + (2 * (_ac)))

#define	TX_DATA_FIFO		TX_AC_BE_FIFO
#define	TX_CTL_FIFO		TX_AC_VO_FIFO

#define WL_RSSI_ANT_MAX		4	

struct intctrlregs {
	u32 intstatus;
	u32 intmask;
};

struct pio2regs {
	u16 fifocontrol;
	u16 fifodata;
	u16 fifofree;	
	u16 PAD;
};

struct pio2regp {
	struct pio2regs tx;
	struct pio2regs rx;
};

struct pio4regs {
	u32 fifocontrol;
	u32 fifodata;
};

struct pio4regp {
	struct pio4regs tx;
	struct pio4regs rx;
};

/* read: 32-bit register that can be read as 32-bit or as 2 16-bit
 * write: only low 16b-it half can be written
 */
union pmqreg {
	u32 pmqhostdata;	
	struct {
		u16 pmqctrlstatus;	
		u16 PAD;
	} w;
};

struct fifo64 {
	struct dma64regs dmaxmt;	
	struct pio4regs piotx;	
	struct dma64regs dmarcv;	
	struct pio4regs piorx;	
};

struct d11regs {
	
	u32 PAD[3];		
	u32 biststatus;	
	u32 biststatus2;	
	u32 PAD;		
	u32 gptimer;		
	u32 usectimer;	

	
	struct intctrlregs intctrlregs[8];

	u32 PAD[40];		

	u32 intrcvlazy[4];	

	u32 PAD[4];		

	u32 maccontrol;	
	u32 maccommand;	
	u32 macintstatus;	
	u32 macintmask;	

	
	u32 tplatewrptr;	
	u32 tplatewrdata;	
	u32 PAD[2];		

	
	union pmqreg pmqreg;	
	u32 pmqpatl;		
	u32 pmqpath;		
	u32 PAD;		

	u32 chnstatus;	
	u32 psmdebug;	
	u32 phydebug;	
	u32 machwcap;	

	
	u32 objaddr;		
	u32 objdata;		
	u32 PAD[2];		

	u32 frmtxstatus;	
	u32 frmtxstatus2;	
	u32 PAD[2];		

	
	u32 tsf_timerlow;	
	u32 tsf_timerhigh;	
	u32 tsf_cfprep;	
	u32 tsf_cfpstart;	
	u32 tsf_cfpmaxdur32;	
	u32 PAD[3];		

	u32 maccontrol1;	
	u32 machwcap1;	
	u32 PAD[14];		

	
	u32 clk_ctl_st;	
	u32 hw_war;
	u32 d11_phypllctl;	
	u32 PAD[5];		

	
	struct fifo64 fifo64regs[6];

	
	struct dma32diag dmafifo;	

	u32 aggfifocnt;	
	u32 aggfifodata;	
	u32 PAD[16];		
	u16 radioregaddr;	
	u16 radioregdata;	

	u32 rfdisabledly;	

	
	u16 phyversion;	
	u16 phybbconfig;	
	u16 phyadcbias;	
	u16 phyanacore;	
	u16 phyrxstatus0;	
	u16 phyrxstatus1;	
	u16 phycrsth;	
	u16 phytxerror;	
	u16 phychannel;	
	u16 PAD[1];		
	u16 phytest;		
	u16 phy4waddr;	
	u16 phy4wdatahi;	
	u16 phy4wdatalo;	
	u16 phyregaddr;	
	u16 phyregdata;	

	

	
	u16 PAD[3];		
	u16 rcv_fifo_ctl;	
	u16 PAD;		
	u16 rcv_frm_cnt;	
	u16 PAD[4];		
	u16 rssi;		
	u16 PAD[5];		
	u16 rcm_ctl;		
	u16 rcm_mat_data;	
	u16 rcm_mat_mask;	
	u16 rcm_mat_dly;	
	u16 rcm_cond_mask_l;	
	u16 rcm_cond_mask_h;	
	u16 rcm_cond_dly;	
	u16 PAD[1];		
	u16 ext_ihr_addr;	
	u16 ext_ihr_data;	
	u16 rxe_phyrs_2;	
	u16 rxe_phyrs_3;	
	u16 phy_mode;	
	u16 rcmta_ctl;	
	u16 rcmta_size;	
	u16 rcmta_addr0;	
	u16 rcmta_addr1;	
	u16 rcmta_addr2;	
	u16 PAD[30];		

	

	u16 PAD;		
	u16 psm_maccontrol_h;	
	u16 psm_macintstatus_l;	
	u16 psm_macintstatus_h;	
	u16 psm_macintmask_l;	
	u16 psm_macintmask_h;	
	u16 PAD;		
	u16 psm_maccommand;	
	u16 psm_brc;		
	u16 psm_phy_hdr_param;	
	u16 psm_postcard;	
	u16 psm_pcard_loc_l;	
	u16 psm_pcard_loc_h;	
	u16 psm_gpio_in;	
	u16 psm_gpio_out;	
	u16 psm_gpio_oe;	

	u16 psm_bred_0;	
	u16 psm_bred_1;	
	u16 psm_bred_2;	
	u16 psm_bred_3;	
	u16 psm_brcl_0;	
	u16 psm_brcl_1;	
	u16 psm_brcl_2;	
	u16 psm_brcl_3;	
	u16 psm_brpo_0;	
	u16 psm_brpo_1;	
	u16 psm_brpo_2;	
	u16 psm_brpo_3;	
	u16 psm_brwk_0;	
	u16 psm_brwk_1;	
	u16 psm_brwk_2;	
	u16 psm_brwk_3;	

	u16 psm_base_0;	
	u16 psm_base_1;	
	u16 psm_base_2;	
	u16 psm_base_3;	
	u16 psm_base_4;	
	u16 psm_base_5;	
	u16 psm_base_6;	
	u16 psm_pc_reg_0;	
	u16 psm_pc_reg_1;	
	u16 psm_pc_reg_2;	
	u16 psm_pc_reg_3;	
	u16 PAD[0xD];	
	u16 psm_corectlsts;	
	u16 PAD[0x7];	

	
	u16 txe_ctl;		
	u16 txe_aux;		
	u16 txe_ts_loc;	
	u16 txe_time_out;	
	u16 txe_wm_0;	
	u16 txe_wm_1;	
	u16 txe_phyctl;	
	u16 txe_status;	
	u16 txe_mmplcp0;	
	u16 txe_mmplcp1;	
	u16 txe_phyctl1;	

	u16 PAD[0x05];	

	
	u16 xmtfifodef;	
	u16 xmtfifo_frame_cnt;	
	u16 xmtfifo_byte_cnt;	
	u16 xmtfifo_head;	
	u16 xmtfifo_rd_ptr;	
	u16 xmtfifo_wr_ptr;	
	u16 xmtfifodef1;	

	u16 PAD[0x09];	

	u16 xmtfifocmd;	
	u16 xmtfifoflush;	
	u16 xmtfifothresh;	
	u16 xmtfifordy;	
	u16 xmtfifoprirdy;	
	u16 xmtfiforqpri;	
	u16 xmttplatetxptr;	
	u16 PAD;		
	u16 xmttplateptr;	
	u16 smpl_clct_strptr;	
	u16 smpl_clct_stpptr;	
	u16 smpl_clct_curptr;	
	u16 PAD[0x04];	
	u16 xmttplatedatalo;	
	u16 xmttplatedatahi;	

	u16 PAD[2];		

	u16 xmtsel;		
	u16 xmttxcnt;	
	u16 xmttxshmaddr;	

	u16 PAD[0x09];	

	
	u16 PAD[0x40];	

	
	u16 PAD[0X02];	
	u16 tsf_cfpstrt_l;	
	u16 tsf_cfpstrt_h;	
	u16 PAD[0X05];	
	u16 tsf_cfppretbtt;	
	u16 PAD[0XD];	
	u16 tsf_clk_frac_l;	
	u16 tsf_clk_frac_h;	
	u16 PAD[0X14];	
	u16 tsf_random;	
	u16 PAD[0x05];	
	
	u16 tsf_gpt2_stat;	
	u16 tsf_gpt2_ctr_l;	
	u16 tsf_gpt2_ctr_h;	
	u16 tsf_gpt2_val_l;	
	u16 tsf_gpt2_val_h;	
	u16 tsf_gptall_stat;	
	u16 PAD[0x07];	

	
	u16 ifs_sifs_rx_tx_tx;	
	u16 ifs_sifs_nav_tx;	
	u16 ifs_slot;	
	u16 PAD;		
	u16 ifs_ctl;		
	u16 PAD[0x3];	
	u16 ifsstat;		
	u16 ifsmedbusyctl;	
	u16 iftxdur;		
	u16 PAD[0x3];	
	
	u16 ifs_aifsn;	
	u16 ifs_ctl1;	

	
	u16 scc_ctl;		
	u16 scc_timer_l;	
	u16 scc_timer_h;	
	u16 scc_frac;	
	u16 scc_fastpwrup_dly;	
	u16 scc_per;		
	u16 scc_per_frac;	
	u16 scc_cal_timer_l;	
	u16 scc_cal_timer_h;	
	u16 PAD;		

	u16 PAD[0x26];

	
	u16 nav_ctl;		
	u16 navstat;		
	u16 PAD[0x3e];	

	
	u16 PAD[0x20];	

	u16 wepctl;		
	u16 wepivloc;	
	u16 wepivkey;	
	u16 wepwkey;		

	u16 PAD[4];		
	u16 pcmctl;		
	u16 pcmstat;		
	u16 PAD[6];		

	u16 pmqctl;		
	u16 pmqstatus;	
	u16 pmqpat0;		
	u16 pmqpat1;		
	u16 pmqpat2;		

	u16 pmqdat;		
	u16 pmqdator;	
	u16 pmqhst;		
	u16 pmqpath0;	
	u16 pmqpath1;	
	u16 pmqpath2;	
	u16 pmqdath;		

	u16 PAD[0x04];	

	
	u16 PAD[0x380];	
};

#define D11REGOFFS(field)	offsetof(struct d11regs, field)

#define	PIHR_BASE	0x0400	

#define	BT_DONE		(1U << 31)	
#define	BT_B2S		(1 << 30)	

#define	I_PC		(1 << 10)	
#define	I_PD		(1 << 11)	
#define	I_DE		(1 << 12)	
#define	I_RU		(1 << 13)	
#define	I_RO		(1 << 14)	
#define	I_XU		(1 << 15)	
#define	I_RI		(1 << 16)	
#define	I_XI		(1 << 24)	

#define	IRL_TO_MASK		0x00ffffff	
#define	IRL_FC_MASK		0xff000000	
#define	IRL_FC_SHIFT		24	

#define	MCTL_GMODE		(1U << 31)
#define	MCTL_DISCARD_PMQ	(1 << 30)
#define	MCTL_WAKE		(1 << 26)
#define	MCTL_HPS		(1 << 25)
#define	MCTL_PROMISC		(1 << 24)
#define	MCTL_KEEPBADFCS		(1 << 23)
#define	MCTL_KEEPCONTROL	(1 << 22)
#define	MCTL_PHYLOCK		(1 << 21)
#define	MCTL_BCNS_PROMISC	(1 << 20)
#define	MCTL_LOCK_RADIO		(1 << 19)
#define	MCTL_AP			(1 << 18)
#define	MCTL_INFRA		(1 << 17)
#define	MCTL_BIGEND		(1 << 16)
#define	MCTL_GPOUT_SEL_MASK	(3 << 14)
#define	MCTL_GPOUT_SEL_SHIFT	14
#define	MCTL_EN_PSMDBG		(1 << 13)
#define	MCTL_IHR_EN		(1 << 10)
#define	MCTL_SHM_UPPER		(1 <<  9)
#define	MCTL_SHM_EN		(1 <<  8)
#define	MCTL_PSM_JMP_0		(1 <<  2)
#define	MCTL_PSM_RUN		(1 <<  1)
#define	MCTL_EN_MAC		(1 <<  0)

#define	MCMD_BCN0VLD		(1 <<  0)
#define	MCMD_BCN1VLD		(1 <<  1)
#define	MCMD_DIRFRMQVAL		(1 <<  2)
#define	MCMD_CCA		(1 <<  3)
#define	MCMD_BG_NOISE		(1 <<  4)
#define	MCMD_SKIP_SHMINIT	(1 <<  5)	
#define MCMD_SAMPLECOLL		MCMD_SKIP_SHMINIT 

#define	MI_MACSSPNDD		(1 <<  0)
#define	MI_BCNTPL		(1 <<  1)
#define	MI_TBTT			(1 <<  2)
#define	MI_BCNSUCCESS		(1 <<  3)
#define	MI_BCNCANCLD		(1 <<  4)
#define	MI_ATIMWINEND		(1 <<  5)
#define	MI_PMQ			(1 <<  6)
#define	MI_NSPECGEN_0		(1 <<  7)
#define	MI_NSPECGEN_1		(1 <<  8)
#define	MI_MACTXERR		(1 <<  9)
#define	MI_NSPECGEN_3		(1 << 10)
#define	MI_PHYTXERR		(1 << 11)
#define	MI_PME			(1 << 12)
#define	MI_GP0			(1 << 13)
#define	MI_GP1			(1 << 14)
#define	MI_DMAINT		(1 << 15)
#define	MI_TXSTOP		(1 << 16)
#define	MI_CCA			(1 << 17)
#define	MI_BG_NOISE		(1 << 18)
#define	MI_DTIM_TBTT		(1 << 19)
#define MI_PRQ			(1 << 20)
#define	MI_PWRUP		(1 << 21)
#define	MI_RESERVED3		(1 << 22)
#define	MI_RESERVED2		(1 << 23)
#define MI_RESERVED1		(1 << 25)
#define MI_RFDISABLE		(1 << 28)
#define	MI_TFS			(1 << 29)
#define	MI_PHYCHANGED		(1 << 30)
#define	MI_TO			(1U << 31)

#define	MCAP_TKIPMIC		0x80000000	

#define	PMQH_DATA_MASK		0xffff0000
#define	PMQH_BSSCFG		0x00100000
#define	PMQH_PMOFF		0x00010000
#define	PMQH_PMON		0x00020000
#define	PMQH_DASAT		0x00040000
#define	PMQH_ATIMFAIL		0x00080000
#define	PMQH_DEL_ENTRY		0x00000001
#define	PMQH_DEL_MULT		0x00000002
#define	PMQH_OFLO		0x00000004
#define	PMQH_NOT_EMPTY		0x00000008

#define	PDBG_CRS		(1 << 0)
#define	PDBG_TXA		(1 << 1)
#define	PDBG_TXF		(1 << 2)
#define	PDBG_TXE		(1 << 3)
#define	PDBG_RXF		(1 << 4)
#define	PDBG_RXS		(1 << 5)
#define	PDBG_RXFRG		(1 << 6)
#define	PDBG_RXV		(1 << 7)
#define	PDBG_RFD		(1 << 16)

#define	OBJADDR_SEL_MASK	0x000F0000
#define	OBJADDR_UCM_SEL		0x00000000
#define	OBJADDR_SHM_SEL		0x00010000
#define	OBJADDR_SCR_SEL		0x00020000
#define	OBJADDR_IHR_SEL		0x00030000
#define	OBJADDR_RCMTA_SEL	0x00040000
#define	OBJADDR_SRCHM_SEL	0x00060000
#define	OBJADDR_WINC		0x01000000
#define	OBJADDR_RINC		0x02000000
#define	OBJADDR_AUTO_INC	0x03000000

#define	WEP_PCMADDR		0x07d4
#define	WEP_PCMDATA		0x07d6

#define	TXS_V			(1 << 0)	
#define	TXS_STATUS_MASK		0xffff
#define	TXS_FID_MASK		0xffff0000
#define	TXS_FID_SHIFT		16

#define	TXS_SEQ_MASK		0xffff
#define	TXS_PTX_MASK		0xff0000
#define	TXS_PTX_SHIFT		16
#define	TXS_MU_MASK		0x01000000
#define	TXS_MU_SHIFT		24

#define CCS_ERSRC_REQ_D11PLL	0x00000100	
#define CCS_ERSRC_REQ_PHYPLL	0x00000200	
#define CCS_ERSRC_AVAIL_D11PLL	0x01000000	
#define CCS_ERSRC_AVAIL_PHYPLL	0x02000000	

#define CCS_ERSRC_REQ_HT    0x00000010	
#define CCS_ERSRC_AVAIL_HT  0x00020000	

#define	CFPREP_CBI_MASK		0xffffffc0
#define	CFPREP_CBI_SHIFT	6
#define	CFPREP_CFPP		0x00000001

#define TXFIFOCMD_RESET_MASK	(1 << 15)	
#define TXFIFOCMD_FIFOSEL_SHIFT	8	
#define TXFIFO_FIFOTOP_SHIFT	8	

#define TXFIFO_START_BLK16	 65	
#define TXFIFO_START_BLK	 6	
#define TXFIFO_SIZE_UNIT	256	
#define MBSS16_TEMPLMEM_MINBLKS	65	

#define	PV_AV_MASK		0xf000
#define	PV_AV_SHIFT		12
#define	PV_PT_MASK		0x0f00
#define	PV_PT_SHIFT		8
#define	PV_PV_MASK		0x000f
#define	PHY_TYPE(v)		((v & PV_PT_MASK) >> PV_PT_SHIFT)

#define	PHY_TYPE_N		4	
#define	PHY_TYPE_SSN		6	
#define	PHY_TYPE_LCN		8	
#define	PHY_TYPE_LCNXN		9	
#define	PHY_TYPE_NULL		0xf	

#define	ANA_11N_013		5

struct ofdm_phy_hdr {
	u8 rlpt[3];		
	u16 service;
	u8 pad;
} __packed;

#define	D11A_PHY_HDR_GRATE(phdr)	((phdr)->rlpt[0] & 0x0f)
#define	D11A_PHY_HDR_GRES(phdr)		(((phdr)->rlpt[0] >> 4) & 0x01)
#define	D11A_PHY_HDR_GLENGTH(phdr)	(((u32 *)((phdr)->rlpt) >> 5) & 0x0fff)
#define	D11A_PHY_HDR_GPARITY(phdr)	(((phdr)->rlpt[3] >> 1) & 0x01)
#define	D11A_PHY_HDR_GTAIL(phdr)	(((phdr)->rlpt[3] >> 2) & 0x3f)

#define	D11A_PHY_HDR_SRATE(phdr, rate)		\
	((phdr)->rlpt[0] = ((phdr)->rlpt[0] & 0xf0) | ((rate) & 0xf))
#define	D11A_PHY_HDR_SRES(phdr)		((phdr)->rlpt[0] &= 0xef)
#define	D11A_PHY_HDR_SLENGTH(phdr, length)	\
	(*(u32 *)((phdr)->rlpt) = *(u32 *)((phdr)->rlpt) | \
	(((length) & 0x0fff) << 5))
#define	D11A_PHY_HDR_STAIL(phdr)	((phdr)->rlpt[3] &= 0x03)

#define	D11A_PHY_HDR_LEN_L	3	
#define	D11A_PHY_HDR_LEN_R	2	

#define	D11A_PHY_TX_DELAY	(2)	

#define	D11A_PHY_HDR_TIME	(4)	
#define	D11A_PHY_PRE_TIME	(16)
#define	D11A_PHY_PREHDR_TIME	(D11A_PHY_PRE_TIME + D11A_PHY_HDR_TIME)

struct cck_phy_hdr {
	u8 signal;
	u8 service;
	u16 length;
	u16 crc;
} __packed;

#define	D11B_PHY_HDR_LEN	6

#define	D11B_PHY_TX_DELAY	(3)	

#define	D11B_PHY_LHDR_TIME	(D11B_PHY_HDR_LEN << 3)
#define	D11B_PHY_LPRE_TIME	(144)
#define	D11B_PHY_LPREHDR_TIME	(D11B_PHY_LPRE_TIME + D11B_PHY_LHDR_TIME)

#define	D11B_PHY_SHDR_TIME	(D11B_PHY_LHDR_TIME >> 1)
#define	D11B_PHY_SPRE_TIME	(D11B_PHY_LPRE_TIME >> 1)
#define	D11B_PHY_SPREHDR_TIME	(D11B_PHY_SPRE_TIME + D11B_PHY_SHDR_TIME)

#define	D11B_PLCP_SIGNAL_LOCKED	(1 << 2)
#define	D11B_PLCP_SIGNAL_LE	(1 << 7)

#define MIMO_PLCP_MCS_MASK	0x7f	
#define MIMO_PLCP_40MHZ		0x80	
#define MIMO_PLCP_AMPDU		0x08	

#define BRCMS_GET_CCK_PLCP_LEN(plcp) (plcp[4] + (plcp[5] << 8))
#define BRCMS_GET_MIMO_PLCP_LEN(plcp) (plcp[1] + (plcp[2] << 8))
#define BRCMS_SET_MIMO_PLCP_LEN(plcp, len) \
	do { \
		plcp[1] = len & 0xff; \
		plcp[2] = ((len >> 8) & 0xff); \
	} while (0);

#define BRCMS_SET_MIMO_PLCP_AMPDU(plcp) (plcp[3] |= MIMO_PLCP_AMPDU)
#define BRCMS_CLR_MIMO_PLCP_AMPDU(plcp) (plcp[3] &= ~MIMO_PLCP_AMPDU)
#define BRCMS_IS_MIMO_PLCP_AMPDU(plcp) (plcp[3] & MIMO_PLCP_AMPDU)

#define	D11_PHY_HDR_LEN	6

struct d11txh {
	__le16 MacTxControlLow;	
	__le16 MacTxControlHigh;	
	__le16 MacFrameControl;	
	__le16 TxFesTimeNormal;	
	__le16 PhyTxControlWord;	
	__le16 PhyTxControlWord_1;	
	__le16 PhyTxControlWord_1_Fbr;	
	__le16 PhyTxControlWord_1_Rts;	
	__le16 PhyTxControlWord_1_FbrRts;	
	__le16 MainRates;	
	__le16 XtraFrameTypes;	
	u8 IV[16];		
	u8 TxFrameRA[6];	
	__le16 TxFesTimeFallback;	
	u8 RTSPLCPFallback[6];	
	__le16 RTSDurFallback;	
	u8 FragPLCPFallback[6];	
	__le16 FragDurFallback;	
	__le16 MModeLen;	
	__le16 MModeFbrLen;	
	__le16 TstampLow;	
	__le16 TstampHigh;	
	__le16 ABI_MimoAntSel;	
	__le16 PreloadSize;	
	__le16 AmpduSeqCtl;	
	__le16 TxFrameID;	
	__le16 TxStatus;	
	__le16 MaxNMpdus;	
	__le16 MaxABytes_MRT;	
	__le16 MaxABytes_FBR;	
	__le16 MinMBytes;	
	u8 RTSPhyHeader[D11_PHY_HDR_LEN];	
	struct ieee80211_rts rts_frame;	
	u16 PAD;		
} __packed;

#define	D11_TXH_LEN		112	

#define FT_CCK	0
#define FT_OFDM	1
#define FT_HT	2
#define FT_N	3

#define TXC_AMPDU_SHIFT		9	
#define TXC_AMPDU_NONE		0	
#define TXC_AMPDU_FIRST		1	
#define TXC_AMPDU_MIDDLE	2	
#define TXC_AMPDU_LAST		3	

#define TXC_AMIC		0x8000
#define	TXC_SENDCTS		0x0800
#define TXC_AMPDU_MASK		0x0600
#define TXC_BW_40		0x0100
#define TXC_FREQBAND_5G		0x0080
#define	TXC_DFCS		0x0040
#define	TXC_IGNOREPMQ		0x0020
#define	TXC_HWSEQ		0x0010
#define	TXC_STARTMSDU		0x0008
#define	TXC_SENDRTS		0x0004
#define	TXC_LONGFRAME		0x0002
#define	TXC_IMMEDACK		0x0001

#define TXC_PREAMBLE_RTS_FB_SHORT	0x8000
#define TXC_PREAMBLE_RTS_MAIN_SHORT	0x4000
#define TXC_PREAMBLE_DATA_FB_SHORT	0x2000

#define	TXC_AMPDU_FBR		0x1000
#define	TXC_SECKEY_MASK		0x0FF0
#define	TXC_SECKEY_SHIFT	4
#define	TXC_ALT_TXPWR		0x0008
#define	TXC_SECTYPE_MASK	0x0007
#define	TXC_SECTYPE_SHIFT	0

#define AMPDU_FBR_NULL_DELIM  5	

#define	PHY_TXC_PWR_MASK	0xFC00
#define	PHY_TXC_PWR_SHIFT	10
#define	PHY_TXC_ANT_MASK	0x03C0	
#define	PHY_TXC_ANT_SHIFT	6
#define	PHY_TXC_ANT_0_1		0x00C0	
#define	PHY_TXC_LCNPHY_ANT_LAST	0x0000
#define	PHY_TXC_ANT_3		0x0200	
#define	PHY_TXC_ANT_2		0x0100	
#define	PHY_TXC_ANT_1		0x0080	
#define	PHY_TXC_ANT_0		0x0040	
#define	PHY_TXC_SHORT_HDR	0x0010

#define	PHY_TXC_OLD_ANT_0	0x0000
#define	PHY_TXC_OLD_ANT_1	0x0100
#define	PHY_TXC_OLD_ANT_LAST	0x0300

#define PHY_TXC1_BW_MASK		0x0007
#define PHY_TXC1_BW_10MHZ		0
#define PHY_TXC1_BW_10MHZ_UP		1
#define PHY_TXC1_BW_20MHZ		2
#define PHY_TXC1_BW_20MHZ_UP		3
#define PHY_TXC1_BW_40MHZ		4
#define PHY_TXC1_BW_40MHZ_DUP		5
#define PHY_TXC1_MODE_SHIFT		3
#define PHY_TXC1_MODE_MASK		0x0038
#define PHY_TXC1_MODE_SISO		0
#define PHY_TXC1_MODE_CDD		1
#define PHY_TXC1_MODE_STBC		2
#define PHY_TXC1_MODE_SDM		3

#define	PHY_TXC_HTANT_MASK		0x3fC0	

#define XFTS_RTS_FT_SHIFT	2
#define XFTS_FBRRTS_FT_SHIFT	4
#define XFTS_CHANNEL_SHIFT	8

#define	PHY_AWS_ANTDIV		0x2000

#define IFS_USEEDCF	(1 << 2)

#define IFS_CTL1_EDCRS	(1 << 3)
#define IFS_CTL1_EDCRS_20L (1 << 4)
#define IFS_CTL1_EDCRS_40 (1 << 5)

#define ABI_MAS_ADDR_BMP_IDX_MASK	0x0f00
#define ABI_MAS_ADDR_BMP_IDX_SHIFT	8
#define ABI_MAS_FBR_ANT_PTN_MASK	0x00f0
#define ABI_MAS_FBR_ANT_PTN_SHIFT	4
#define ABI_MAS_MRT_ANT_PTN_MASK	0x000f

struct tx_status {
	u16 framelen;
	u16 PAD;
	u16 frameid;
	u16 status;
	u16 lasttxtime;
	u16 sequence;
	u16 phyerr;
	u16 ackphyrxsh;
} __packed;

#define	TXSTATUS_LEN	16

#define	TX_STATUS_FRM_RTX_MASK	0xF000
#define	TX_STATUS_FRM_RTX_SHIFT	12
#define	TX_STATUS_RTS_RTX_MASK	0x0F00
#define	TX_STATUS_RTS_RTX_SHIFT	8
#define TX_STATUS_MASK		0x00FE
#define	TX_STATUS_PMINDCTD	(1 << 7) 
#define	TX_STATUS_INTERMEDIATE	(1 << 6) 
#define	TX_STATUS_AMPDU		(1 << 5) 
#define TX_STATUS_SUPR_MASK	0x1C	 
#define TX_STATUS_SUPR_SHIFT	2
#define	TX_STATUS_ACK_RCV	(1 << 1) 
#define	TX_STATUS_VALID		(1 << 0) 
#define	TX_STATUS_NO_ACK	0

#define	TX_STATUS_SUPR_PMQ	(1 << 2) 
#define	TX_STATUS_SUPR_FLUSH	(2 << 2) 
#define	TX_STATUS_SUPR_FRAG	(3 << 2) 
#define	TX_STATUS_SUPR_TBTT	(3 << 2) 
#define	TX_STATUS_SUPR_BADCH	(4 << 2) 
#define	TX_STATUS_SUPR_EXPTIME	(5 << 2) 
#define	TX_STATUS_SUPR_UF	(6 << 2) 

#define TX_STATUS_UNEXP(status) \
	((((status) & TX_STATUS_INTERMEDIATE) != 0) && \
	 TX_STATUS_UNEXP_AMPDU(status))

#define TX_STATUS_UNEXP_AMPDU(status) \
	((((status) & TX_STATUS_SUPR_MASK) != 0) && \
	 (((status) & TX_STATUS_SUPR_MASK) != TX_STATUS_SUPR_EXPTIME))

#define TX_STATUS_BA_BMAP03_MASK	0xF000	
#define TX_STATUS_BA_BMAP03_SHIFT	12	
#define TX_STATUS_BA_BMAP47_MASK	0x001E	
#define TX_STATUS_BA_BMAP47_SHIFT	3	


#define	RCM_INC_MASK_H		0x0080
#define	RCM_INC_MASK_L		0x0040
#define	RCM_INC_DATA		0x0020
#define	RCM_INDEX_MASK		0x001F
#define	RCM_SIZE		15

#define	RCM_MAC_OFFSET		0	
#define	RCM_BSSID_OFFSET	3	
#define	RCM_F_BSSID_0_OFFSET	6	
#define	RCM_F_BSSID_1_OFFSET	9	
#define	RCM_F_BSSID_2_OFFSET	12	

#define RCM_WEP_TA0_OFFSET	16
#define RCM_WEP_TA1_OFFSET	19
#define RCM_WEP_TA2_OFFSET	22
#define RCM_WEP_TA3_OFFSET	25


#define MAC_PHY_RESET		1
#define MAC_PHY_CLOCK_EN	2
#define MAC_PHY_FORCE_CLK	4


#define	WKEY_START		(1 << 8)
#define	WKEY_SEL_MASK		0x1F


#define RCMTA_SIZE 50

#define M_ADDR_BMP_BLK		(0x37e * 2)
#define M_ADDR_BMP_BLK_SZ	12

#define ADDR_BMP_RA		(1 << 0)	
#define ADDR_BMP_TA		(1 << 1)	
#define ADDR_BMP_BSSID		(1 << 2)	
#define ADDR_BMP_AP		(1 << 3)	
#define ADDR_BMP_STA		(1 << 4)	
#define ADDR_BMP_RESERVED1	(1 << 5)
#define ADDR_BMP_RESERVED2	(1 << 6)
#define ADDR_BMP_RESERVED3	(1 << 7)
#define ADDR_BMP_BSS_IDX_MASK	(3 << 8)	
#define ADDR_BMP_BSS_IDX_SHIFT	8

#define	WSEC_MAX_RCMTA_KEYS	54

#define	WSEC_MAX_TKMIC_ENGINE_KEYS		12	

#define WSEC_MAX_RXE_KEYS	4

#define	SKL_ALGO_MASK		0x0007
#define	SKL_ALGO_SHIFT		0
#define	SKL_KEYID_MASK		0x0008
#define	SKL_KEYID_SHIFT		3
#define	SKL_INDEX_MASK		0x03F0
#define	SKL_INDEX_SHIFT		4
#define	SKL_GRP_ALGO_MASK	0x1c00
#define	SKL_GRP_ALGO_SHIFT	10

#define	SKL_IBSS_INDEX_MASK	0x01F0
#define	SKL_IBSS_INDEX_SHIFT	4
#define	SKL_IBSS_KEYID1_MASK	0x0600
#define	SKL_IBSS_KEYID1_SHIFT	9
#define	SKL_IBSS_KEYID2_MASK	0x1800
#define	SKL_IBSS_KEYID2_SHIFT	11
#define	SKL_IBSS_KEYALGO_MASK	0xE000
#define	SKL_IBSS_KEYALGO_SHIFT	13

#define	WSEC_MODE_OFF		0
#define	WSEC_MODE_HW		1
#define	WSEC_MODE_SW		2

#define	WSEC_ALGO_OFF		0
#define	WSEC_ALGO_WEP1		1
#define	WSEC_ALGO_TKIP		2
#define	WSEC_ALGO_AES		3
#define	WSEC_ALGO_WEP128	4
#define	WSEC_ALGO_AES_LEGACY	5
#define	WSEC_ALGO_NALG		6

#define	AES_MODE_NONE		0
#define	AES_MODE_CCM		1

#define	WECR0_KEYREG_SHIFT	0
#define	WECR0_KEYREG_MASK	0x7
#define	WECR0_DECRYPT		(1 << 3)
#define	WECR0_IVINLINE		(1 << 4)
#define	WECR0_WEPALG_SHIFT	5
#define	WECR0_WEPALG_MASK	(0x7 << 5)
#define	WECR0_WKEYSEL_SHIFT	8
#define	WECR0_WKEYSEL_MASK	(0x7 << 8)
#define	WECR0_WKEYSTART		(1 << 11)
#define	WECR0_WEPINIT		(1 << 14)
#define	WECR0_ICVERR		(1 << 15)

#define	T_ACTS_TPL_BASE		(0)
#define	T_NULL_TPL_BASE		(0xc * 2)
#define	T_QNULL_TPL_BASE	(0x1c * 2)
#define	T_RR_TPL_BASE		(0x2c * 2)
#define	T_BCN0_TPL_BASE		(0x34 * 2)
#define	T_PRS_TPL_BASE		(0x134 * 2)
#define	T_BCN1_TPL_BASE		(0x234 * 2)
#define T_TX_FIFO_TXRAM_BASE	(T_ACTS_TPL_BASE + \
				 (TXFIFO_START_BLK * TXFIFO_SIZE_UNIT))

#define T_BA_TPL_BASE		T_QNULL_TPL_BASE 

#define T_RAM_ACCESS_SZ		4	


#define	M_MACHW_VER		(0x00b * 2)

#define	M_MACHW_CAP_L		(0x060 * 2)
#define	M_MACHW_CAP_H	(0x061 * 2)

#define M_EDCF_STATUS_OFF	(0x007 * 2)
#define M_TXF_CUR_INDEX		(0x018 * 2)
#define M_EDCF_QINFO		(0x120 * 2)

#define	M_DOT11_SLOT		(0x008 * 2)
#define	M_DOT11_DTIMPERIOD	(0x009 * 2)
#define	M_NOSLPZNATDTIM		(0x026 * 2)

#define	M_BCN0_FRM_BYTESZ	(0x00c * 2)	
#define	M_BCN1_FRM_BYTESZ	(0x00d * 2)	
#define	M_BCN_TXTSF_OFFSET	(0x00e * 2)
#define	M_TIMBPOS_INBEACON	(0x00f * 2)
#define	M_SFRMTXCNTFBRTHSD	(0x022 * 2)
#define	M_LFRMTXCNTFBRTHSD	(0x023 * 2)
#define	M_BCN_PCTLWD		(0x02a * 2)
#define M_BCN_LI		(0x05b * 2)	

#define M_MAXRXFRM_LEN		(0x010 * 2)

#define	M_RSP_PCTLWD		(0x011 * 2)

#define M_TXPWR_N		(0x012 * 2)
#define M_TXPWR_TARGET		(0x013 * 2)
#define M_TXPWR_MAX		(0x014 * 2)
#define M_TXPWR_CUR		(0x019 * 2)

#define	M_RX_PAD_DATA_OFFSET	(0x01a * 2)

#define	M_SEC_DEFIVLOC		(0x01e * 2)
#define	M_SEC_VALNUMSOFTMCHTA	(0x01f * 2)
#define	M_PHYVER		(0x028 * 2)
#define	M_PHYTYPE		(0x029 * 2)
#define	M_SECRXKEYS_PTR		(0x02b * 2)
#define	M_TKMICKEYS_PTR		(0x059 * 2)
#define	M_SECKINDXALGO_BLK	(0x2ea * 2)
#define M_SECKINDXALGO_BLK_SZ	54
#define	M_SECPSMRXTAMCH_BLK	(0x2fa * 2)
#define	M_TKIP_TSC_TTAK		(0x18c * 2)
#define	D11_MAX_KEY_SIZE	16

#define	M_MAX_ANTCNT		(0x02e * 2)	

#define	M_SSIDLEN		(0x024 * 2)
#define	M_PRB_RESP_FRM_LEN	(0x025 * 2)
#define	M_PRS_MAXTIME		(0x03a * 2)
#define	M_SSID			(0xb0 * 2)
#define	M_CTXPRS_BLK		(0xc0 * 2)
#define	C_CTX_PCTLWD_POS	(0x4 * 2)

#define M_OFDM_OFFSET		(0x027 * 2)

#define	M_B_TSSI_0		(0x02c * 2)
#define	M_B_TSSI_1		(0x02d * 2)

#define	M_HOST_FLAGS1		(0x02f * 2)
#define	M_HOST_FLAGS2		(0x030 * 2)
#define	M_HOST_FLAGS3		(0x031 * 2)
#define	M_HOST_FLAGS4		(0x03c * 2)
#define	M_HOST_FLAGS5		(0x06a * 2)
#define	M_HOST_FLAGS_SZ		16

#define M_RADAR_REG		(0x033 * 2)

#define	M_A_TSSI_0		(0x034 * 2)
#define	M_A_TSSI_1		(0x035 * 2)

#define M_NOISE_IF_COUNT	(0x034 * 2)
#define M_NOISE_IF_TIMEOUT	(0x035 * 2)

#define	M_RF_RX_SP_REG1		(0x036 * 2)

#define	M_G_TSSI_0		(0x038 * 2)
#define	M_G_TSSI_1		(0x039 * 2)

#define	M_JSSI_0		(0x44 * 2)
#define	M_JSSI_1		(0x45 * 2)
#define	M_JSSI_AUX		(0x46 * 2)

#define	M_CUR_2050_RADIOCODE	(0x47 * 2)

#define M_FIFOSIZE0		(0x4c * 2)
#define M_FIFOSIZE1		(0x4d * 2)
#define M_FIFOSIZE2		(0x4e * 2)
#define M_FIFOSIZE3		(0x4f * 2)
#define D11_MAX_TX_FRMS		32	

#define M_CURCHANNEL		(0x50 * 2)
#define D11_CURCHANNEL_5G	0x0100;
#define D11_CURCHANNEL_40	0x0200;
#define D11_CURCHANNEL_MAX	0x00FF;

#define M_BCMC_FID		(0x54 * 2)
#define INVALIDFID		0xffff

#define	M_BCN_PCTL1WD		(0x058 * 2)

#define M_TX_IDLE_BUSY_RATIO_X_16_CCK  (0x52 * 2)
#define M_TX_IDLE_BUSY_RATIO_X_16_OFDM (0x5A * 2)

#define M_LCN_RSSI_0		0x1332
#define M_LCN_RSSI_1		0x1338
#define M_LCN_RSSI_2		0x133e
#define M_LCN_RSSI_3		0x1344

#define M_LCN_SNR_A_0	0x1334
#define M_LCN_SNR_B_0	0x1336

#define M_LCN_SNR_A_1	0x133a
#define M_LCN_SNR_B_1	0x133c

#define M_LCN_SNR_A_2	0x1340
#define M_LCN_SNR_B_2	0x1342

#define M_LCN_SNR_A_3	0x1346
#define M_LCN_SNR_B_3	0x1348

#define M_LCN_LAST_RESET	(81*2)
#define M_LCN_LAST_LOC	(63*2)
#define M_LCNPHY_RESET_STATUS (4902)
#define M_LCNPHY_DSC_TIME	(0x98d*2)
#define M_LCNPHY_RESET_CNT_DSC (0x98b*2)
#define M_LCNPHY_RESET_CNT	(0x98c*2)

#define	M_RT_DIRMAP_A		(0xe0 * 2)
#define	M_RT_BBRSMAP_A		(0xf0 * 2)
#define	M_RT_DIRMAP_B		(0x100 * 2)
#define	M_RT_BBRSMAP_B		(0x110 * 2)

#define	M_RT_PRS_PLCP_POS	10
#define	M_RT_PRS_DUR_POS	16
#define	M_RT_OFDM_PCTL1_POS	18

#define M_20IN40_IQ			(0x380 * 2)

#define M_CURR_IDX1		(0x384 * 2)
#define M_CURR_IDX2		(0x387 * 2)

#define M_BSCALE_ANT0	(0x5e * 2)
#define M_BSCALE_ANT1	(0x5f * 2)

#define M_MIMO_ANTSEL_RXDFLT	(0x63 * 2)
#define M_ANTSEL_CLKDIV	(0x61 * 2)
#define M_MIMO_ANTSEL_TXDFLT	(0x64 * 2)

#define M_MIMO_MAXSYM	(0x5d * 2)
#define MIMO_MAXSYM_DEF		0x8000	
#define MIMO_MAXSYM_MAX		0xffff	

#define M_WATCHDOG_8TU		(0x1e * 2)
#define WATCHDOG_8TU_DEF	5
#define WATCHDOG_8TU_MAX	10

#define M_PKTENG_CTRL		(0x6c * 2)
#define M_PKTENG_IFS		(0x6d * 2)
#define M_PKTENG_FRMCNT_LO	(0x6e * 2)
#define M_PKTENG_FRMCNT_HI	(0x6f * 2)

#define M_LCN_PWR_IDX_MAX	(0x67 * 2) 
#define M_LCN_PWR_IDX_MIN	(0x66 * 2) 

#define M_PKTENG_MODE_TX		0x0001
#define M_PKTENG_MODE_TX_RIFS	        0x0004
#define M_PKTENG_MODE_TX_CTS            0x0008
#define M_PKTENG_MODE_RX		0x0002
#define M_PKTENG_MODE_RX_WITH_ACK	0x0402
#define M_PKTENG_MODE_MASK		0x0003
#define M_PKTENG_FRMCNT_VLD		0x0100

#define M_SMPL_COL_BMP		(0x37d * 2)
#define M_SMPL_COL_CTL		(0x3b2 * 2)

#define ANTSEL_CLKDIV_4MHZ	6
#define MIMO_ANTSEL_BUSY	0x4000	
#define MIMO_ANTSEL_SEL		0x8000	
#define MIMO_ANTSEL_WAIT	50	
#define MIMO_ANTSEL_OVERRIDE	0x8000	

struct shm_acparams {
	u16 txop;
	u16 cwmin;
	u16 cwmax;
	u16 cwcur;
	u16 aifs;
	u16 bslots;
	u16 reggap;
	u16 status;
	u16 rsvd[8];
} __packed;
#define M_EDCF_QLEN	(16 * 2)

#define WME_STATUS_NEWAC	(1 << 8)

#define MHFMAX		5	
#define MHF1		0	
#define MHF2		1	
#define MHF3		2	
#define MHF4		3	
#define MHF5		4	

#define	MHF1_ANTDIV		0x0001
#define	MHF1_EDCF		0x0100
#define MHF1_IQSWAP_WAR		0x0200
#define	MHF1_FORCEFASTCLK	0x0400


#define MHF2_TXBCMC_NOW		0x0040
#define MHF2_HWPWRCTL		0x0080
#define MHF2_NPHY40MHZ_WAR	0x0800

#define MHF3_ANTSEL_EN		0x0001
#define MHF3_ANTSEL_MODE	0x0002
#define MHF3_RESERVED1		0x0004
#define MHF3_RESERVED2		0x0008
#define MHF3_NPHY_MLADV_WAR	0x0010

#define MHF4_BPHY_TXCORE0	0x0080
#define MHF4_EXTPA_ENABLE	0x4000

#define MHF5_4313_GPIOCTRL	0x0001
#define MHF5_RESERVED1		0x0002
#define MHF5_RESERVED2		0x0004
#define	M_RADIO_PWR		(0x32 * 2)

#define	M_PHY_NOISE		(0x037 * 2)
#define	PHY_NOISE_MASK		0x00ff

struct d11rxhdr_le {
	__le16 RxFrameSize;
	u16 PAD;
	__le16 PhyRxStatus_0;
	__le16 PhyRxStatus_1;
	__le16 PhyRxStatus_2;
	__le16 PhyRxStatus_3;
	__le16 PhyRxStatus_4;
	__le16 PhyRxStatus_5;
	__le16 RxStatus1;
	__le16 RxStatus2;
	__le16 RxTSFTime;
	__le16 RxChan;
} __packed;

struct d11rxhdr {
	u16 RxFrameSize;
	u16 PAD;
	u16 PhyRxStatus_0;
	u16 PhyRxStatus_1;
	u16 PhyRxStatus_2;
	u16 PhyRxStatus_3;
	u16 PhyRxStatus_4;
	u16 PhyRxStatus_5;
	u16 RxStatus1;
	u16 RxStatus2;
	u16 RxTSFTime;
	u16 RxChan;
} __packed;

#define	PRXS0_FT_MASK		0x0003
#define	PRXS0_CLIP_MASK		0x000C
#define	PRXS0_CLIP_SHIFT	2
#define	PRXS0_UNSRATE		0x0010
#define	PRXS0_RXANT_UPSUBBAND	0x0020
#define	PRXS0_LCRS		0x0040
#define	PRXS0_SHORTH		0x0080
#define	PRXS0_PLCPFV		0x0100
#define	PRXS0_PLCPHCF		0x0200
#define	PRXS0_GAIN_CTL		0x4000
#define PRXS0_ANTSEL_MASK	0xF000
#define PRXS0_ANTSEL_SHIFT	0x12

#define	PRXS0_CCK		0x0000
#define	PRXS0_OFDM		0x0001
#define	PRXS0_PREN		0x0002
#define	PRXS0_STDN		0x0003

#define PRXS0_ANTSEL_0		0x0	
#define PRXS0_ANTSEL_1		0x2	
#define PRXS0_ANTSEL_2		0x4	
#define PRXS0_ANTSEL_3		0x8	

#define	PRXS1_JSSI_MASK		0x00FF
#define	PRXS1_JSSI_SHIFT	0
#define	PRXS1_SQ_MASK		0xFF00
#define	PRXS1_SQ_SHIFT		8

#define PRXS1_nphy_PWR0_MASK	0x00FF
#define PRXS1_nphy_PWR1_MASK	0xFF00

#define PRXS0_BAND	        0x0400	
#define PRXS0_RSVD	        0x0800	
#define PRXS0_UNUSED	        0xF000	

#define PRXS1_HTPHY_CORE_MASK	0x000F
#define PRXS1_HTPHY_ANTCFG_MASK	0x00F0
#define PRXS1_HTPHY_MMPLCPLenL_MASK	0xFF00

#define PRXS2_HTPHY_MMPLCPLenH_MASK	0x000F
#define PRXS2_HTPHY_MMPLCH_RATE_MASK	0x00F0
#define PRXS2_HTPHY_RXPWR_ANT0	0xFF00

#define PRXS3_HTPHY_RXPWR_ANT1	0x00FF
#define PRXS3_HTPHY_RXPWR_ANT2	0xFF00

#define PRXS4_HTPHY_RXPWR_ANT3	0x00FF
#define PRXS4_HTPHY_CFO		0xFF00

#define PRXS5_HTPHY_FFO	        0x00FF
#define PRXS5_HTPHY_AR	        0xFF00

#define HTPHY_MMPLCPLen(rxs) \
	((((rxs)->PhyRxStatus_1 & PRXS1_HTPHY_MMPLCPLenL_MASK) >> 8) | \
	(((rxs)->PhyRxStatus_2 & PRXS2_HTPHY_MMPLCPLenH_MASK) << 8))
#define HTPHY_RXPWR_ANT0(rxs) \
	((((rxs)->PhyRxStatus_2) & PRXS2_HTPHY_RXPWR_ANT0) >> 8)
#define HTPHY_RXPWR_ANT1(rxs) \
	(((rxs)->PhyRxStatus_3) & PRXS3_HTPHY_RXPWR_ANT1)
#define HTPHY_RXPWR_ANT2(rxs) \
	((((rxs)->PhyRxStatus_3) & PRXS3_HTPHY_RXPWR_ANT2) >> 8)

#define	RXS_BCNSENT		0x8000
#define	RXS_SECKINDX_MASK	0x07e0
#define	RXS_SECKINDX_SHIFT	5
#define	RXS_DECERR		(1 << 4)
#define	RXS_DECATMPT		(1 << 3)
#define	RXS_PBPRES		(1 << 2)
#define	RXS_RESPFRAMETX		(1 << 1)
#define	RXS_FCSERR		(1 << 0)

#define RXS_AMSDU_MASK		1
#define	RXS_AGGTYPE_MASK	0x6
#define	RXS_AGGTYPE_SHIFT	1
#define	RXS_PHYRXST_VALID	(1 << 8)
#define RXS_RXANT_MASK		0x3
#define RXS_RXANT_SHIFT		12

#define RXS_CHAN_40		0x1000
#define RXS_CHAN_5G		0x0800
#define	RXS_CHAN_ID_MASK	0x07f8
#define	RXS_CHAN_ID_SHIFT	3
#define	RXS_CHAN_PHYTYPE_MASK	0x0007
#define	RXS_CHAN_PHYTYPE_SHIFT	0

#define M_PWRIND_BLKS	(0x184 * 2)
#define M_PWRIND_MAP0	(M_PWRIND_BLKS + 0x0)
#define M_PWRIND_MAP1	(M_PWRIND_BLKS + 0x2)
#define M_PWRIND_MAP2	(M_PWRIND_BLKS + 0x4)
#define M_PWRIND_MAP3	(M_PWRIND_BLKS + 0x6)
#define M_PWRIND_MAP(core)  (M_PWRIND_BLKS + ((core)<<1))

#define	M_PSM_SOFT_REGS	0x0
#define	M_BOM_REV_MAJOR	(M_PSM_SOFT_REGS + 0x0)
#define	M_BOM_REV_MINOR	(M_PSM_SOFT_REGS + 0x2)
#define	M_UCODE_DBGST	(M_PSM_SOFT_REGS + 0x40) 
#define	M_UCODE_MACSTAT	(M_PSM_SOFT_REGS + 0xE0) 

#define M_AGING_THRSH	(0x3e * 2) 
#define	M_MBURST_SIZE	(0x40 * 2) 
#define	M_MBURST_TXOP	(0x41 * 2) 
#define M_SYNTHPU_DLY	(0x4a * 2) 
#define	M_PRETBTT	(0x4b * 2)

#define M_ALT_TXPWR_IDX		(M_PSM_SOFT_REGS + (0x3b * 2))
#define M_PHY_TX_FLT_PTR	(M_PSM_SOFT_REGS + (0x3d * 2))
#define M_CTS_DURATION		(M_PSM_SOFT_REGS + (0x5c * 2))
#define M_LP_RCCAL_OVR		(M_PSM_SOFT_REGS + (0x6b * 2))

#define M_RXSTATS_BLK_PTR	(M_PSM_SOFT_REGS + (0x65 * 2))

#define	DBGST_INACTIVE		0
#define	DBGST_INIT		1
#define	DBGST_ACTIVE		2
#define	DBGST_SUSPENDED		3
#define	DBGST_ASLEEP		4

enum _ePsmScratchPadRegDefinitions {
	S_RSV0 = 0,
	S_RSV1,
	S_RSV2,

	
	S_DOT11_CWMIN,		
	S_DOT11_CWMAX,		
	S_DOT11_CWCUR,		
	S_DOT11_SRC_LMT,	
	S_DOT11_LRC_LMT,	
	S_DOT11_DTIMCOUNT,	

	
	S_SEQ_NUM,		
	S_SEQ_NUM_FRAG,		
	S_FRMRETX_CNT,		
	S_SSRC,			
	S_SLRC,			
	S_EXP_RSP,		
	S_OLD_BREM,		
	S_OLD_CWWIN,		
	S_TXECTL,		
	S_CTXTST,		

	
	S_RXTST,		

	
	S_STREG,		

	S_TXPWR_SUM,		
	S_TXPWR_ITER,		
	S_RX_FRMTYPE,		
	S_THIS_AGG,		

	S_KEYINDX,
	S_RXFRMLEN,		

	
	S_RXTSFTMRVAL_WD3,	
	S_RXTSFTMRVAL_WD2,	
	S_RXTSFTMRVAL_WD1,	
	S_RXTSFTMRVAL_WD0,	
	S_RXSSN,		
	S_RXQOSFLD,		

	
	S_TMP0,			
	S_TMP1,			
	S_TMP2,			
	S_TMP3,			
	S_TMP4,			
	S_TMP5,			
	S_PRQPENALTY_CTR,	
	S_ANTCNT,		
	S_SYMBOL,		
	S_RXTP,			
	S_STREG2,		
	S_STREG3,		
	S_STREG4,		
	S_STREG5,		

	S_ADJPWR_IDX,
	S_CUR_PTR,		
	S_REVID4,		
	S_INDX,			
	S_ADDR0,		
	S_ADDR1,		
	S_ADDR2,		
	S_ADDR3,		
	S_ADDR4,		
	S_ADDR5,		
	S_TMP6,			
	S_KEYINDX_BU,		
	S_MFGTEST_TMP0,		
	S_RXESN,		
	S_STREG6,		
};

#define S_BEACON_INDX	S_OLD_BREM
#define S_PRS_INDX	S_OLD_CWWIN
#define S_PHYTYPE	S_SSRC
#define S_PHYVER	S_SLRC

#define SLOW_CTRL_PDE		(1 << 0)
#define SLOW_CTRL_FD		(1 << 8)

struct macstat {
	u16 txallfrm;	
	u16 txrtsfrm;	
	u16 txctsfrm;	
	u16 txackfrm;	
	u16 txdnlfrm;	
	u16 txbcnfrm;	
	u16 txfunfl[8];	
	u16 txtplunfl;	
	u16 txphyerr;	
	u16 pktengrxducast;	
	u16 pktengrxdmcast;	
	u16 rxfrmtoolong;	
	u16 rxfrmtooshrt;	
	u16 rxinvmachdr;	
	u16 rxbadfcs;	
	u16 rxbadplcp;	
	u16 rxcrsglitch;	
	u16 rxstrt;		
	u16 rxdfrmucastmbss;	
	u16 rxmfrmucastmbss;	
	u16 rxcfrmucast;	
	u16 rxrtsucast;	
	u16 rxctsucast;	
	u16 rxackucast;	
	u16 rxdfrmocast;	
	u16 rxmfrmocast;	
	u16 rxcfrmocast;	
	u16 rxrtsocast;	
	u16 rxctsocast;	
	u16 rxdfrmmcast;	
	u16 rxmfrmmcast;	
	u16 rxcfrmmcast;	
	u16 rxbeaconmbss;	
	u16 rxdfrmucastobss;	
	u16 rxbeaconobss;	
	u16 rxrsptmout;	
	u16 bcntxcancl;	
	u16 PAD;
	u16 rxf0ovfl;	
	u16 rxf1ovfl;	
	u16 rxf2ovfl;	
	u16 txsfovfl;	
	u16 pmqovfl;		
	u16 rxcgprqfrm;	
	u16 rxcgprsqovfl;	
	u16 txcgprsfail;	
	u16 txcgprssuc;	
	u16 prs_timeout;	
	u16 rxnack;
	u16 frmscons;
	u16 txnack;
	u16 txglitch_nack;
	u16 txburst;		
	u16 bphy_rxcrsglitch;	
	u16 phywatchdog;	
	u16 PAD;
	u16 bphy_badplcp;	
};

#define	SICF_PCLKE		0x0004	
#define	SICF_PRST		0x0008	
#define	SICF_MPCLKE		0x0010	
#define	SICF_FREF		0x0020	
#define	SICF_BWMASK		0x00c0	
#define	SICF_BW40		0x0080	
#define	SICF_BW20		0x0040	
#define	SICF_BW10		0x0000	
#define	SICF_GMODE		0x2000	

#define	SISF_2G_PHY		0x0001	
#define	SISF_5G_PHY		0x0002	
#define	SISF_FCLKA		0x0004	
#define	SISF_DB_PHY		0x0008	


#define	BPHY_REG_OFT_BASE	0x0
#define	BPHY_BB_CONFIG		0x01
#define	BPHY_ADCBIAS		0x02
#define	BPHY_ANACORE		0x03
#define	BPHY_PHYCRSTH		0x06
#define	BPHY_TEST		0x0a
#define	BPHY_PA_TX_TO		0x10
#define	BPHY_SYNTH_DC_TO	0x11
#define	BPHY_PA_TX_TIME_UP	0x12
#define	BPHY_RX_FLTR_TIME_UP	0x13
#define	BPHY_TX_POWER_OVERRIDE	0x14
#define	BPHY_RF_OVERRIDE	0x15
#define	BPHY_RF_TR_LOOKUP1	0x16
#define	BPHY_RF_TR_LOOKUP2	0x17
#define	BPHY_COEFFS		0x18
#define	BPHY_PLL_OUT		0x19
#define	BPHY_REFRESH_MAIN	0x1a
#define	BPHY_REFRESH_TO0	0x1b
#define	BPHY_REFRESH_TO1	0x1c
#define	BPHY_RSSI_TRESH		0x20
#define	BPHY_IQ_TRESH_HH	0x21
#define	BPHY_IQ_TRESH_H		0x22
#define	BPHY_IQ_TRESH_L		0x23
#define	BPHY_IQ_TRESH_LL	0x24
#define	BPHY_GAIN		0x25
#define	BPHY_LNA_GAIN_RANGE	0x26
#define	BPHY_JSSI		0x27
#define	BPHY_TSSI_CTL		0x28
#define	BPHY_TSSI		0x29
#define	BPHY_TR_LOSS_CTL	0x2a
#define	BPHY_LO_LEAKAGE		0x2b
#define	BPHY_LO_RSSI_ACC	0x2c
#define	BPHY_LO_IQMAG_ACC	0x2d
#define	BPHY_TX_DC_OFF1		0x2e
#define	BPHY_TX_DC_OFF2		0x2f
#define	BPHY_PEAK_CNT_THRESH	0x30
#define	BPHY_FREQ_OFFSET	0x31
#define	BPHY_DIVERSITY_CTL	0x32
#define	BPHY_PEAK_ENERGY_LO	0x33
#define	BPHY_PEAK_ENERGY_HI	0x34
#define	BPHY_SYNC_CTL		0x35
#define	BPHY_TX_PWR_CTRL	0x36
#define BPHY_TX_EST_PWR		0x37
#define	BPHY_STEP		0x38
#define	BPHY_WARMUP		0x39
#define	BPHY_LMS_CFF_READ	0x3a
#define	BPHY_LMS_COEFF_I	0x3b
#define	BPHY_LMS_COEFF_Q	0x3c
#define	BPHY_SIG_POW		0x3d
#define	BPHY_RFDC_CANCEL_CTL	0x3e
#define	BPHY_HDR_TYPE		0x40
#define	BPHY_SFD_TO		0x41
#define	BPHY_SFD_CTL		0x42
#define	BPHY_DEBUG		0x43
#define	BPHY_RX_DELAY_COMP	0x44
#define	BPHY_CRS_DROP_TO	0x45
#define	BPHY_SHORT_SFD_NZEROS	0x46
#define	BPHY_DSSS_COEFF1	0x48
#define	BPHY_DSSS_COEFF2	0x49
#define	BPHY_CCK_COEFF1		0x4a
#define	BPHY_CCK_COEFF2		0x4b
#define	BPHY_TR_CORR		0x4c
#define	BPHY_ANGLE_SCALE	0x4d
#define	BPHY_TX_PWR_BASE_IDX	0x4e
#define	BPHY_OPTIONAL_MODES2	0x4f
#define	BPHY_CCK_LMS_STEP	0x50
#define	BPHY_BYPASS		0x51
#define	BPHY_CCK_DELAY_LONG	0x52
#define	BPHY_CCK_DELAY_SHORT	0x53
#define	BPHY_PPROC_CHAN_DELAY	0x54
#define	BPHY_DDFS_ENABLE	0x58
#define	BPHY_PHASE_SCALE	0x59
#define	BPHY_FREQ_CONTROL	0x5a
#define	BPHY_LNA_GAIN_RANGE_10	0x5b
#define	BPHY_LNA_GAIN_RANGE_32	0x5c
#define	BPHY_OPTIONAL_MODES	0x5d
#define	BPHY_RX_STATUS2		0x5e
#define	BPHY_RX_STATUS3		0x5f
#define	BPHY_DAC_CONTROL	0x60
#define	BPHY_ANA11G_FILT_CTRL	0x62
#define	BPHY_REFRESH_CTRL	0x64
#define	BPHY_RF_OVERRIDE2	0x65
#define	BPHY_SPUR_CANCEL_CTRL	0x66
#define	BPHY_FINE_DIGIGAIN_CTRL	0x67
#define	BPHY_RSSI_LUT		0x88
#define	BPHY_RSSI_LUT_END	0xa7
#define	BPHY_TSSI_LUT		0xa8
#define	BPHY_TSSI_LUT_END	0xc7
#define	BPHY_TSSI2PWR_LUT	0x380
#define	BPHY_TSSI2PWR_LUT_END	0x39f
#define	BPHY_LOCOMP_LUT		0x3a0
#define	BPHY_LOCOMP_LUT_END	0x3bf
#define	BPHY_TXGAIN_LUT		0x3c0
#define	BPHY_TXGAIN_LUT_END	0x3ff

#define	PHY_BBC_ANT_MASK	0x0180
#define	PHY_BBC_ANT_SHIFT	7
#define	BB_DARWIN		0x1000
#define BBCFG_RESETCCA		0x4000
#define BBCFG_RESETRX		0x8000

#define	TST_DDFS		0x2000
#define	TST_TXFILT1		0x0800
#define	TST_UNSCRAM		0x0400
#define	TST_CARR_SUPP		0x0200
#define	TST_DC_COMP_LOOP	0x0100
#define	TST_LOOPBACK		0x0080
#define	TST_TXFILT0		0x0040
#define	TST_TXTEST_ENABLE	0x0020
#define	TST_TXTEST_RATE		0x0018
#define	TST_TXTEST_PHASE	0x0007

#define	TST_TXTEST_RATE_1MBPS	0
#define	TST_TXTEST_RATE_2MBPS	1
#define	TST_TXTEST_RATE_5_5MBPS	2
#define	TST_TXTEST_RATE_11MBPS	3
#define	TST_TXTEST_RATE_SHIFT	3

#define SHM_BYT_CNT	0x2	
#define MAX_BYT_CNT	0x600	

struct d11cnt {
	u32 txfrag;
	u32 txmulti;
	u32 txfail;
	u32 txretry;
	u32 txretrie;
	u32 rxdup;
	u32 txrts;
	u32 txnocts;
	u32 txnoack;
	u32 rxfrag;
	u32 rxmulti;
	u32 rxcrc;
	u32 txfrmsnt;
	u32 rxundec;
};

#endif				
