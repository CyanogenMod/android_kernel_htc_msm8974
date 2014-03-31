/*
 * CAAM hardware register-level view
 *
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 */

#ifndef REGS_H
#define REGS_H

#include <linux/types.h>
#include <linux/io.h>


#ifdef __BIG_ENDIAN
#define wr_reg32(reg, data) out_be32(reg, data)
#define rd_reg32(reg) in_be32(reg)
#ifdef CONFIG_64BIT
#define wr_reg64(reg, data) out_be64(reg, data)
#define rd_reg64(reg) in_be64(reg)
#endif
#else
#ifdef __LITTLE_ENDIAN
#define wr_reg32(reg, data) __raw_writel(reg, data)
#define rd_reg32(reg) __raw_readl(reg)
#ifdef CONFIG_64BIT
#define wr_reg64(reg, data) __raw_writeq(reg, data)
#define rd_reg64(reg) __raw_readq(reg)
#endif
#endif
#endif

#ifndef CONFIG_64BIT
static inline void wr_reg64(u64 __iomem *reg, u64 data)
{
	wr_reg32((u32 __iomem *)reg, (data & 0xffffffff00000000ull) >> 32);
	wr_reg32((u32 __iomem *)reg + 1, data & 0x00000000ffffffffull);
}

static inline u64 rd_reg64(u64 __iomem *reg)
{
	return (((u64)rd_reg32((u32 __iomem *)reg)) << 32) |
		((u64)rd_reg32((u32 __iomem *)reg + 1));
}
#endif

struct jr_outentry {
	dma_addr_t desc;
	u32 jrstatus;	
} __packed;


#define CHA_NUM_DECONUM_SHIFT	56
#define CHA_NUM_DECONUM_MASK	(0xfull << CHA_NUM_DECONUM_SHIFT)

struct caam_perfmon {
	
	u64 req_dequeued;	
	u64 ob_enc_req;	
	u64 ib_dec_req;	
	u64 ob_enc_bytes;	
	u64 ob_prot_bytes;	
	u64 ib_dec_bytes;	
	u64 ib_valid_bytes;	
	u64 rsvd[13];

	
	u64 cha_rev;		
#define CTPR_QI_SHIFT		57
#define CTPR_QI_MASK		(0x1ull << CTPR_QI_SHIFT)
	u64 comp_parms;	
	u64 rsvd1[2];

	
	u64 faultaddr;	
	u32 faultliodn;	
	u32 faultdetail;	
	u32 rsvd2;
	u32 status;		
	u64 rsvd3;

	
	u32 rtic_id;		
	u32 ccb_id;		
	u64 cha_id;		
	u64 cha_num;		
	u64 caam_id;		
};

#define MSTRID_LOCK_LIODN	0x80000000
#define MSTRID_LOCK_MAKETRUSTED	0x00010000	

#define MSTRID_LIODN_MASK	0x0fff
struct masterid {
	u32 liodn_ms;	
	u32 liodn_ls;	
};

struct partid {
	u32 rsvd1;
	u32 pidr;	
};

struct rngtst {
	u32 mode;		
	u32 rsvd1[3];
	u32 reset;		
	u32 rsvd2[3];
	u32 status;		
	u32 rsvd3;
	u32 errstat;		
	u32 rsvd4;
	u32 errctl;		
	u32 rsvd5;
	u32 entropy;		
	u32 rsvd6[15];
	u32 verifctl;	
	u32 rsvd7;
	u32 verifstat;	
	u32 rsvd8;
	u32 verifdata;	
	u32 rsvd9;
	u32 xkey;		
	u32 rsvd10;
	u32 oscctctl;	
	u32 rsvd11;
	u32 oscct;		
	u32 rsvd12;
	u32 oscctstat;	
	u32 rsvd13[2];
	u32 ofifo[4];	
	u32 rsvd14[15];
};


#define KEK_KEY_SIZE		8
#define TKEK_KEY_SIZE		8
#define TDSK_KEY_SIZE		8

#define DECO_RESET	1	
#define DECO_RESET_0	(DECO_RESET << 0)
#define DECO_RESET_1	(DECO_RESET << 1)
#define DECO_RESET_2	(DECO_RESET << 2)
#define DECO_RESET_3	(DECO_RESET << 3)
#define DECO_RESET_4	(DECO_RESET << 4)

struct caam_ctrl {
	
	
	u32 rsvd1;
	u32 mcr;		
	u32 rsvd2[2];

	
	
	struct masterid jr_mid[4];	
	u32 rsvd3[12];
	struct masterid rtic_mid[4];	
	u32 rsvd4[7];
	u32 deco_rq;			
	struct partid deco_mid[5];	
	u32 rsvd5[22];

	
	u32 deco_avail;		
	u32 deco_reset;		
	u32 rsvd6[182];

	
	
	u32 kek[KEK_KEY_SIZE];	
	u32 tkek[TKEK_KEY_SIZE];	
	u32 tdsk[TDSK_KEY_SIZE];	
	u32 rsvd7[32];
	u64 sknonce;			
	u32 rsvd8[70];

	
	
	struct rngtst rtst[2];

	u32 rsvd9[448];

	
	struct caam_perfmon perfmon;
};

#define MCFGR_SWRESET		0x80000000 
#define MCFGR_WDENABLE		0x40000000 
#define MCFGR_WDFAIL		0x20000000 
#define MCFGR_DMA_RESET		0x10000000
#define MCFGR_LONG_PTR		0x00010000 

#define MCFGR_ARCACHE_SHIFT	12
#define MCFGR_ARCACHE_MASK	(0xf << MCFGR_ARCACHE_SHIFT)

#define MCFGR_AWCACHE_SHIFT	8
#define MCFGR_AWCACHE_MASK	(0xf << MCFGR_AWCACHE_SHIFT)

#define MCFGR_AXIPIPE_SHIFT	4
#define MCFGR_AXIPIPE_MASK	(0xf << MCFGR_AXIPIPE_SHIFT)

#define MCFGR_AXIPRI		0x00000008 
#define MCFGR_BURST_64		0x00000001 

struct caam_job_ring {
	
	u64 inpring_base;	
	u32 rsvd1;
	u32 inpring_size;	
	u32 rsvd2;
	u32 inpring_avail;	
	u32 rsvd3;
	u32 inpring_jobadd;	

	
	u64 outring_base;	
	u32 rsvd4;
	u32 outring_size;	
	u32 rsvd5;
	u32 outring_rmvd;	
	u32 rsvd6;
	u32 outring_used;	

	
	u32 rsvd7;
	u32 jroutstatus;	
	u32 rsvd8;
	u32 jrintstatus;	
	u32 rconfig_hi;	
	u32 rconfig_lo;

	
	u32 rsvd9;
	u32 inp_rdidx;	
	u32 rsvd10;
	u32 out_wtidx;	

	
	u32 rsvd11;
	u32 jrcommand;	

	u32 rsvd12[932];

	
	struct caam_perfmon perfmon;
};

#define JR_RINGSIZE_MASK	0x03ff
/*
 * jrstatus - Job Ring Output Status
 * All values in lo word
 * Also note, same values written out as status through QI
 * in the command/status field of a frame descriptor
 */
#define JRSTA_SSRC_SHIFT            28
#define JRSTA_SSRC_MASK             0xf0000000

#define JRSTA_SSRC_NONE             0x00000000
#define JRSTA_SSRC_CCB_ERROR        0x20000000
#define JRSTA_SSRC_JUMP_HALT_USER   0x30000000
#define JRSTA_SSRC_DECO             0x40000000
#define JRSTA_SSRC_JRERROR          0x60000000
#define JRSTA_SSRC_JUMP_HALT_CC     0x70000000

#define JRSTA_DECOERR_JUMP          0x08000000
#define JRSTA_DECOERR_INDEX_SHIFT   8
#define JRSTA_DECOERR_INDEX_MASK    0xff00
#define JRSTA_DECOERR_ERROR_MASK    0x00ff

#define JRSTA_DECOERR_NONE          0x00
#define JRSTA_DECOERR_LINKLEN       0x01
#define JRSTA_DECOERR_LINKPTR       0x02
#define JRSTA_DECOERR_JRCTRL        0x03
#define JRSTA_DECOERR_DESCCMD       0x04
#define JRSTA_DECOERR_ORDER         0x05
#define JRSTA_DECOERR_KEYCMD        0x06
#define JRSTA_DECOERR_LOADCMD       0x07
#define JRSTA_DECOERR_STORECMD      0x08
#define JRSTA_DECOERR_OPCMD         0x09
#define JRSTA_DECOERR_FIFOLDCMD     0x0a
#define JRSTA_DECOERR_FIFOSTCMD     0x0b
#define JRSTA_DECOERR_MOVECMD       0x0c
#define JRSTA_DECOERR_JUMPCMD       0x0d
#define JRSTA_DECOERR_MATHCMD       0x0e
#define JRSTA_DECOERR_SHASHCMD      0x0f
#define JRSTA_DECOERR_SEQCMD        0x10
#define JRSTA_DECOERR_DECOINTERNAL  0x11
#define JRSTA_DECOERR_SHDESCHDR     0x12
#define JRSTA_DECOERR_HDRLEN        0x13
#define JRSTA_DECOERR_BURSTER       0x14
#define JRSTA_DECOERR_DESCSIGNATURE 0x15
#define JRSTA_DECOERR_DMA           0x16
#define JRSTA_DECOERR_BURSTFIFO     0x17
#define JRSTA_DECOERR_JRRESET       0x1a
#define JRSTA_DECOERR_JOBFAIL       0x1b
#define JRSTA_DECOERR_DNRERR        0x80
#define JRSTA_DECOERR_UNDEFPCL      0x81
#define JRSTA_DECOERR_PDBERR        0x82
#define JRSTA_DECOERR_ANRPLY_LATE   0x83
#define JRSTA_DECOERR_ANRPLY_REPLAY 0x84
#define JRSTA_DECOERR_SEQOVF        0x85
#define JRSTA_DECOERR_INVSIGN       0x86
#define JRSTA_DECOERR_DSASIGN       0x87

#define JRSTA_CCBERR_JUMP           0x08000000
#define JRSTA_CCBERR_INDEX_MASK     0xff00
#define JRSTA_CCBERR_INDEX_SHIFT    8
#define JRSTA_CCBERR_CHAID_MASK     0x00f0
#define JRSTA_CCBERR_CHAID_SHIFT    4
#define JRSTA_CCBERR_ERRID_MASK     0x000f

#define JRSTA_CCBERR_CHAID_AES      (0x01 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_DES      (0x02 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_ARC4     (0x03 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_MD       (0x04 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_RNG      (0x05 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_SNOW     (0x06 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_KASUMI   (0x07 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_PK       (0x08 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_CRC      (0x09 << JRSTA_CCBERR_CHAID_SHIFT)

#define JRSTA_CCBERR_ERRID_NONE     0x00
#define JRSTA_CCBERR_ERRID_MODE     0x01
#define JRSTA_CCBERR_ERRID_DATASIZ  0x02
#define JRSTA_CCBERR_ERRID_KEYSIZ   0x03
#define JRSTA_CCBERR_ERRID_PKAMEMSZ 0x04
#define JRSTA_CCBERR_ERRID_PKBMEMSZ 0x05
#define JRSTA_CCBERR_ERRID_SEQUENCE 0x06
#define JRSTA_CCBERR_ERRID_PKDIVZRO 0x07
#define JRSTA_CCBERR_ERRID_PKMODEVN 0x08
#define JRSTA_CCBERR_ERRID_KEYPARIT 0x09
#define JRSTA_CCBERR_ERRID_ICVCHK   0x0a
#define JRSTA_CCBERR_ERRID_HARDWARE 0x0b
#define JRSTA_CCBERR_ERRID_CCMAAD   0x0c
#define JRSTA_CCBERR_ERRID_INVCHA   0x0f

#define JRINT_ERR_INDEX_MASK        0x3fff0000
#define JRINT_ERR_INDEX_SHIFT       16
#define JRINT_ERR_TYPE_MASK         0xf00
#define JRINT_ERR_TYPE_SHIFT        8
#define JRINT_ERR_HALT_MASK         0xc
#define JRINT_ERR_HALT_SHIFT        2
#define JRINT_ERR_HALT_INPROGRESS   0x4
#define JRINT_ERR_HALT_COMPLETE     0x8
#define JRINT_JR_ERROR              0x02
#define JRINT_JR_INT                0x01

#define JRINT_ERR_TYPE_WRITE        1
#define JRINT_ERR_TYPE_BAD_INPADDR  3
#define JRINT_ERR_TYPE_BAD_OUTADDR  4
#define JRINT_ERR_TYPE_INV_INPWRT   5
#define JRINT_ERR_TYPE_INV_OUTWRT   6
#define JRINT_ERR_TYPE_RESET        7
#define JRINT_ERR_TYPE_REMOVE_OFL   8
#define JRINT_ERR_TYPE_ADD_OFL      9

#define JRCFG_SOE		0x04
#define JRCFG_ICEN		0x02
#define JRCFG_IMSK		0x01
#define JRCFG_ICDCT_SHIFT	8
#define JRCFG_ICTT_SHIFT	16

#define JRCR_RESET                  0x01


struct rtic_element {
	u64 address;
	u32 rsvd;
	u32 length;
};

struct rtic_block {
	struct rtic_element element[2];
};

struct rtic_memhash {
	u32 memhash_be[32];
	u32 memhash_le[32];
};

struct caam_assurance {
    
	u32 rsvd1;
	u32 status;		
	u32 rsvd2;
	u32 cmd;		
	u32 rsvd3;
	u32 ctrl;		
	u32 rsvd4;
	u32 throttle;	
	u32 rsvd5[2];
	u64 watchdog;	
	u32 rsvd6;
	u32 rend;		
	u32 rsvd7[50];

	
	struct rtic_block memblk[4];	
	u32 rsvd8[32];

	
	struct rtic_memhash hash[4];	
	u32 rsvd_3[640];
};


struct caam_queue_if {
	u32 qi_control_hi;	
	u32 qi_control_lo;
	u32 rsvd1;
	u32 qi_status;	
	u32 qi_deq_cfg_hi;	
	u32 qi_deq_cfg_lo;
	u32 qi_enq_cfg_hi;	
	u32 qi_enq_cfg_lo;
	u32 rsvd2[1016];
};

#define QICTL_DQEN      0x01              
#define QICTL_STOP      0x02              
#define QICTL_SOE       0x04              

#define QICTL_MBSI	0x01
#define QICTL_MHWSI	0x02
#define QICTL_MWSI	0x04
#define QICTL_MDWSI	0x08
#define QICTL_CBSI	0x10		
#define QICTL_CHWSI	0x20		
#define QICTL_CWSI	0x40		
#define QICTL_CDWSI	0x80		
#define QICTL_MBSO	0x0100
#define QICTL_MHWSO	0x0200
#define QICTL_MWSO	0x0400
#define QICTL_MDWSO	0x0800
#define QICTL_CBSO	0x1000		
#define QICTL_CHWSO	0x2000		
#define QICTL_CWSO	0x4000		
#define QICTL_CDWSO     0x8000		
#define QICTL_DMBS	0x010000
#define QICTL_EPO	0x020000

#define QISTA_PHRDERR   0x01              
#define QISTA_CFRDERR   0x02              
#define QISTA_OFWRERR   0x04              
#define QISTA_BPDERR    0x08              
#define QISTA_BTSERR    0x10              
#define QISTA_CFWRERR   0x20              
#define QISTA_STOPD     0x80000000        

struct deco_sg_table {
	u64 addr;		
	u32 elen;		
	u32 bpid_offset;	
};

struct caam_deco {
	u32 rsvd1;
	u32 cls1_mode;	
	u32 rsvd2;
	u32 cls1_keysize;	
	u32 cls1_datasize_hi;	
	u32 cls1_datasize_lo;
	u32 rsvd3;
	u32 cls1_icvsize;	
	u32 rsvd4[5];
	u32 cha_ctrl;	
	u32 rsvd5;
	u32 irq_crtl;	
	u32 rsvd6;
	u32 clr_written;	/* CxCWR - Clear-Written */
	u32 ccb_status_hi;	
	u32 ccb_status_lo;
	u32 rsvd7[3];
	u32 aad_size;	
	u32 rsvd8;
	u32 cls1_iv_size;	
	u32 rsvd9[7];
	u32 pkha_a_size;	
	u32 rsvd10;
	u32 pkha_b_size;	
	u32 rsvd11;
	u32 pkha_n_size;	
	u32 rsvd12;
	u32 pkha_e_size;	
	u32 rsvd13[24];
	u32 cls1_ctx[16];	
	u32 rsvd14[48];
	u32 cls1_key[8];	
	u32 rsvd15[121];
	u32 cls2_mode;	
	u32 rsvd16;
	u32 cls2_keysize;	
	u32 cls2_datasize_hi;	
	u32 cls2_datasize_lo;
	u32 rsvd17;
	u32 cls2_icvsize;	
	u32 rsvd18[56];
	u32 cls2_ctx[18];	
	u32 rsvd19[46];
	u32 cls2_key[32];	
	u32 rsvd20[84];
	u32 inp_infofifo_hi;	
	u32 inp_infofifo_lo;
	u32 rsvd21[2];
	u64 inp_datafifo;	
	u32 rsvd22[2];
	u64 out_datafifo;	
	u32 rsvd23[2];
	u32 jr_ctl_hi;	
	u32 jr_ctl_lo;
	u64 jr_descaddr;	
	u32 op_status_hi;	
	u32 op_status_lo;
	u32 rsvd24[2];
	u32 liodn;		
	u32 td_liodn;	
	u32 rsvd26[6];
	u64 math[4];		
	u32 rsvd27[8];
	struct deco_sg_table gthr_tbl[4];	
	u32 rsvd28[16];
	struct deco_sg_table sctr_tbl[4];	
	u32 rsvd29[48];
	u32 descbuf[64];	
	u32 rsvd30[320];
};

struct caam_full {
	struct caam_ctrl __iomem ctrl;
	struct caam_job_ring jr[4];
	u64 rsvd[512];
	struct caam_assurance assure;
	struct caam_queue_if qi;
};

#endif 
