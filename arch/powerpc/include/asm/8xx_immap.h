/*
 * MPC8xx Internal Memory Map
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * The I/O on the MPC860 is comprised of blocks of special registers
 * and the dual port ram for the Communication Processor Module.
 * Within this space are functional units such as the SIU, memory
 * controller, system timers, and other control functions.  It is
 * a combination that I found difficult to separate into logical
 * functional files.....but anyone else is welcome to try.  -- Dan
 */
#ifdef __KERNEL__
#ifndef __IMMAP_8XX__
#define __IMMAP_8XX__

typedef	struct sys_conf {
	uint	sc_siumcr;
	uint	sc_sypcr;
	uint	sc_swt;
	char	res1[2];
	ushort	sc_swsr;
	uint	sc_sipend;
	uint	sc_simask;
	uint	sc_siel;
	uint	sc_sivec;
	uint	sc_tesr;
	char	res2[0xc];
	uint	sc_sdcr;
	char	res3[0x4c];
} sysconf8xx_t;

typedef struct pcmcia_conf {
	uint	pcmc_pbr0;
	uint	pcmc_por0;
	uint	pcmc_pbr1;
	uint	pcmc_por1;
	uint	pcmc_pbr2;
	uint	pcmc_por2;
	uint	pcmc_pbr3;
	uint	pcmc_por3;
	uint	pcmc_pbr4;
	uint	pcmc_por4;
	uint	pcmc_pbr5;
	uint	pcmc_por5;
	uint	pcmc_pbr6;
	uint	pcmc_por6;
	uint	pcmc_pbr7;
	uint	pcmc_por7;
	char	res1[0x20];
	uint	pcmc_pgcra;
	uint	pcmc_pgcrb;
	uint	pcmc_pscr;
	char	res2[4];
	uint	pcmc_pipr;
	char	res3[4];
	uint	pcmc_per;
	char	res4[4];
} pcmconf8xx_t;

typedef struct	mem_ctlr {
	uint	memc_br0;
	uint	memc_or0;
	uint	memc_br1;
	uint	memc_or1;
	uint	memc_br2;
	uint	memc_or2;
	uint	memc_br3;
	uint	memc_or3;
	uint	memc_br4;
	uint	memc_or4;
	uint	memc_br5;
	uint	memc_or5;
	uint	memc_br6;
	uint	memc_or6;
	uint	memc_br7;
	uint	memc_or7;
	char	res1[0x24];
	uint	memc_mar;
	uint	memc_mcr;
	char	res2[4];
	uint	memc_mamr;
	uint	memc_mbmr;
	ushort	memc_mstat;
	ushort	memc_mptpr;
	uint	memc_mdr;
	char	res3[0x80];
} memctl8xx_t;

#define BR_BA_MSK	0xffff8000	
#define BR_AT_MSK	0x00007000	
#define BR_PS_MSK	0x00000c00	
#define BR_PS_32	0x00000000	
#define BR_PS_16	0x00000800	
#define BR_PS_8		0x00000400	
#define BR_PARE		0x00000200	
#define BR_WP		0x00000100	
#define BR_MS_MSK	0x000000c0	
#define BR_MS_GPCM	0x00000000	
#define BR_MS_UPMA	0x00000080	
#define BR_MS_UPMB	0x000000c0	
#define BR_V		0x00000001	

#define OR_AM_MSK	0xffff8000	
#define OR_ATM_MSK	0x00007000	
#define OR_CSNT_SAM	0x00000800	
					
#define OR_ACS_MSK	0x00000600	
#define OR_ACS_DIV1	0x00000000	
#define OR_ACS_DIV4	0x00000400	
#define OR_ACS_DIV2	0x00000600	
#define OR_G5LA		0x00000400	/* Output #GPL5 on #GPL_A5		*/
#define OR_G5LS		0x00000200	/* Drive #GPL high on falling edge of...*/
#define OR_BI		0x00000100	
#define OR_SCY_MSK	0x000000f0	
#define OR_SCY_0_CLK	0x00000000	
#define OR_SCY_1_CLK	0x00000010	
#define OR_SCY_2_CLK	0x00000020	
#define OR_SCY_3_CLK	0x00000030	
#define OR_SCY_4_CLK	0x00000040	
#define OR_SCY_5_CLK	0x00000050	
#define OR_SCY_6_CLK	0x00000060	
#define OR_SCY_7_CLK	0x00000070	
#define OR_SCY_8_CLK	0x00000080	
#define OR_SCY_9_CLK	0x00000090	
#define OR_SCY_10_CLK	0x000000a0	
#define OR_SCY_11_CLK	0x000000b0	
#define OR_SCY_12_CLK	0x000000c0	
#define OR_SCY_13_CLK	0x000000d0	
#define OR_SCY_14_CLK	0x000000e0	
#define OR_SCY_15_CLK	0x000000f0	
#define OR_SETA		0x00000008	
#define OR_TRLX		0x00000004	
#define OR_EHTR		0x00000002	

typedef struct	sys_int_timers {
	ushort	sit_tbscr;
	char	res0[0x02];
	uint	sit_tbreff0;
	uint	sit_tbreff1;
	char	res1[0x14];
	ushort	sit_rtcsc;
	char	res2[0x02];
	uint	sit_rtc;
	uint	sit_rtsec;
	uint	sit_rtcal;
	char	res3[0x10];
	ushort	sit_piscr;
	char	res4[2];
	uint	sit_pitc;
	uint	sit_pitr;
	char	res5[0x34];
} sit8xx_t;

#define TBSCR_TBIRQ_MASK	((ushort)0xff00)
#define TBSCR_REFA		((ushort)0x0080)
#define TBSCR_REFB		((ushort)0x0040)
#define TBSCR_REFAE		((ushort)0x0008)
#define TBSCR_REFBE		((ushort)0x0004)
#define TBSCR_TBF		((ushort)0x0002)
#define TBSCR_TBE		((ushort)0x0001)

#define RTCSC_RTCIRQ_MASK	((ushort)0xff00)
#define RTCSC_SEC		((ushort)0x0080)
#define RTCSC_ALR		((ushort)0x0040)
#define RTCSC_38K		((ushort)0x0010)
#define RTCSC_SIE		((ushort)0x0008)
#define RTCSC_ALE		((ushort)0x0004)
#define RTCSC_RTF		((ushort)0x0002)
#define RTCSC_RTE		((ushort)0x0001)

#define PISCR_PIRQ_MASK		((ushort)0xff00)
#define PISCR_PS		((ushort)0x0080)
#define PISCR_PIE		((ushort)0x0004)
#define PISCR_PTF		((ushort)0x0002)
#define PISCR_PTE		((ushort)0x0001)

typedef struct clk_and_reset {
	uint	car_sccr;
	uint	car_plprcr;
	uint	car_rsr;
	char	res[0x74];        
} car8xx_t;

typedef struct sitk {
	uint	sitk_tbscrk;
	uint	sitk_tbreff0k;
	uint	sitk_tbreff1k;
	uint	sitk_tbk;
	char	res1[0x10];
	uint	sitk_rtcsck;
	uint	sitk_rtck;
	uint	sitk_rtseck;
	uint	sitk_rtcalk;
	char	res2[0x10];
	uint	sitk_piscrk;
	uint	sitk_pitck;
	char	res3[0x38];
} sitk8xx_t;

typedef struct cark {
	uint	cark_sccrk;
	uint	cark_plprcrk;
	uint	cark_rsrk;
	char	res[0x474];
} cark8xx_t;

#define KAPWR_KEY	((unsigned int)0x55ccaa33)

typedef struct vid823 {
	ushort	vid_vccr;
	ushort	res1;
	u_char	vid_vsr;
	u_char	res2;
	u_char	vid_vcmr;
	u_char	res3;
	uint	vid_vbcb;
	uint	res4;
	uint	vid_vfcr0;
	uint	vid_vfaa0;
	uint	vid_vfba0;
	uint	vid_vfcr1;
	uint	vid_vfaa1;
	uint	vid_vfba1;
	u_char	res5[0x18];
} vid823_t;

typedef struct lcd {
	uint	lcd_lccr;
	uint	lcd_lchcr;
	uint	lcd_lcvcr;
	char	res1[4];
	uint	lcd_lcfaa;
	uint	lcd_lcfba;
	char	lcd_lcsr;
	char	res2[0x7];
} lcd823_t;

typedef struct i2c {
	u_char	i2c_i2mod;
	char	res1[3];
	u_char	i2c_i2add;
	char	res2[3];
	u_char	i2c_i2brg;
	char	res3[3];
	u_char	i2c_i2com;
	char	res4[3];
	u_char	i2c_i2cer;
	char	res5[3];
	u_char	i2c_i2cmr;
	char	res6[0x8b];
} i2c8xx_t;

typedef struct sdma_csr {
	char	res1[4];
	uint	sdma_sdar;
	u_char	sdma_sdsr;
	char	res3[3];
	u_char	sdma_sdmr;
	char	res4[3];
	u_char	sdma_idsr1;
	char	res5[3];
	u_char	sdma_idmr1;
	char	res6[3];
	u_char	sdma_idsr2;
	char	res7[3];
	u_char	sdma_idmr2;
	char	res8[0x13];
} sdma8xx_t;

typedef struct cpm_ic {
	ushort	cpic_civr;
	char	res[0xe];
	uint	cpic_cicr;
	uint	cpic_cipr;
	uint	cpic_cimr;
	uint	cpic_cisr;
} cpic8xx_t;

typedef struct io_port {
	ushort	iop_padir;
	ushort	iop_papar;
	ushort	iop_paodr;
	ushort	iop_padat;
	char	res1[8];
	ushort	iop_pcdir;
	ushort	iop_pcpar;
	ushort	iop_pcso;
	ushort	iop_pcdat;
	ushort	iop_pcint;
	char	res2[6];
	ushort	iop_pddir;
	ushort	iop_pdpar;
	char	res3[2];
	ushort	iop_pddat;
	uint	utmode;
	char	res4[4];
} iop8xx_t;

typedef struct cpm_timers {
	ushort	cpmt_tgcr;
	char	res1[0xe];
	ushort	cpmt_tmr1;
	ushort	cpmt_tmr2;
	ushort	cpmt_trr1;
	ushort	cpmt_trr2;
	ushort	cpmt_tcr1;
	ushort	cpmt_tcr2;
	ushort	cpmt_tcn1;
	ushort	cpmt_tcn2;
	ushort	cpmt_tmr3;
	ushort	cpmt_tmr4;
	ushort	cpmt_trr3;
	ushort	cpmt_trr4;
	ushort	cpmt_tcr3;
	ushort	cpmt_tcr4;
	ushort	cpmt_tcn3;
	ushort	cpmt_tcn4;
	ushort	cpmt_ter1;
	ushort	cpmt_ter2;
	ushort	cpmt_ter3;
	ushort	cpmt_ter4;
	char	res2[8];
} cpmtimer8xx_t;

typedef struct scc {		
	uint	scc_gsmrl;
	uint	scc_gsmrh;
	ushort	scc_psmr;
	char	res1[2];
	ushort	scc_todr;
	ushort	scc_dsr;
	ushort	scc_scce;
	char	res2[2];
	ushort	scc_sccm;
	char	res3;
	u_char	scc_sccs;
	char	res4[8];
} scc_t;

typedef struct smc {		
	char	res1[2];
	ushort	smc_smcmr;
	char	res2[2];
	u_char	smc_smce;
	char	res3[3];
	u_char	smc_smcm;
	char	res4[5];
} smc_t;


typedef struct fec {
	uint	fec_addr_low;		
	ushort	fec_addr_high;		
	ushort	res1;			
	uint	fec_grp_hash_table_high;	
	uint	fec_grp_hash_table_low;	
	uint	fec_r_des_start;	
	uint	fec_x_des_start;	
	uint	fec_r_buff_size;	
	uint	res2[9];		
	uint	fec_ecntrl;		
	uint	fec_ievent;		
	uint	fec_imask;		
	uint	fec_ivec;		
	uint	fec_r_des_active;	
	uint	fec_x_des_active;	
	uint	res3[10];		
	uint	fec_mii_data;		
	uint	fec_mii_speed;		
	uint	res4[17];		
	uint	fec_r_bound;		
	uint	fec_r_fstart;		
	uint	res5[6];		
	uint	fec_x_fstart;		
	uint	res6[17];		
	uint	fec_fun_code;		
	uint	res7[3];		
	uint	fec_r_cntrl;		
	uint	fec_r_hash;		
	uint	res8[14];		
	uint	fec_x_cntrl;		
	uint	res9[0x1e];		
} fec_t;

union fec_lcd {
	fec_t	fl_un_fec;
	u_char	fl_un_cmap[0x200];
};

typedef struct comm_proc {
	ushort	cp_cpcr;
	u_char	res1[2];
	ushort	cp_rccr;
	u_char	res2;
	u_char	cp_rmds;
	u_char	res3[4];
	ushort	cp_cpmcr1;
	ushort	cp_cpmcr2;
	ushort	cp_cpmcr3;
	ushort	cp_cpmcr4;
	u_char	res4[2];
	ushort	cp_rter;
	u_char	res5[2];
	ushort	cp_rtmr;
	u_char	res6[0x14];

	uint	cp_brgc1;
	uint	cp_brgc2;
	uint	cp_brgc3;
	uint	cp_brgc4;

	scc_t	cp_scc[4];

	smc_t	cp_smc[2];

	ushort	cp_spmode;
	u_char	res7[4];
	u_char	cp_spie;
	u_char	res8[3];
	u_char	cp_spim;
	u_char	res9[2];
	u_char	cp_spcom;
	u_char	res10[2];

	u_char	res11[2];
	ushort	cp_pipc;
	u_char	res12[2];
	ushort	cp_ptpr;
	uint	cp_pbdir;
	uint	cp_pbpar;
	u_char	res13[2];
	ushort	cp_pbodr;
	uint	cp_pbdat;

	uint	cp_pedir;
	uint	cp_pepar;
	uint	cp_peso;
	uint	cp_peodr;
	uint	cp_pedat;

	uint	cp_cptr;

	uint	cp_simode;
	u_char	cp_sigmr;
	u_char	res15;
	u_char	cp_sistr;
	u_char	cp_sicmr;
	u_char	res16[4];
	uint	cp_sicr;
	uint	cp_sirp;
	u_char	res17[0xc];

	u_char	cp_vcram[0x100];
	u_char	cp_siram[0x200];

	union	fec_lcd	fl_un;
#define cp_fec		fl_un.fl_un_fec
#define lcd_cmap	fl_un.fl_un_cmap
	char	res18[0xE00];

	
	fec_t	cp_fec2;
#define cp_fec1	cp_fec	

	u_char	cp_dpmem[0x1C00];	
	u_char	cp_dparam[0x400];	
} cpm8xx_t;

typedef struct immap {
	sysconf8xx_t	im_siu_conf;	
	pcmconf8xx_t	im_pcmcia;	
	memctl8xx_t	im_memctl;	
	sit8xx_t	im_sit;		
	car8xx_t	im_clkrst;	
	sitk8xx_t	im_sitk;	
	cark8xx_t	im_clkrstk;	
	vid823_t	im_vid;		
	lcd823_t	im_lcd;		
	i2c8xx_t	im_i2c;		
	sdma8xx_t	im_sdma;	
	cpic8xx_t	im_cpic;	
	iop8xx_t	im_ioport;	
	cpmtimer8xx_t	im_cpmtimer;	
	cpm8xx_t	im_cpm;		
} immap_t;

#endif 
#endif 
