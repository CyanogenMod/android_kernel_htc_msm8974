/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/gfp.h>

#ifndef SYM_HIPD_H
#define SYM_HIPD_H

#if 0
#define SYM_OPT_HANDLE_DEVICE_QUEUEING
#define SYM_OPT_LIMIT_COMMAND_REORDERING
#endif

#define DEBUG_ALLOC	(0x0001)
#define DEBUG_PHASE	(0x0002)
#define DEBUG_POLL	(0x0004)
#define DEBUG_QUEUE	(0x0008)
#define DEBUG_RESULT	(0x0010)
#define DEBUG_SCATTER	(0x0020)
#define DEBUG_SCRIPT	(0x0040)
#define DEBUG_TINY	(0x0080)
#define DEBUG_TIMING	(0x0100)
#define DEBUG_NEGO	(0x0200)
#define DEBUG_TAGS	(0x0400)
#define DEBUG_POINTER	(0x0800)

#ifndef DEBUG_FLAGS
#define DEBUG_FLAGS	(0x0000)
#endif

#ifndef sym_verbose
#define sym_verbose	(np->verbose)
#endif

#ifndef assert
#define	assert(expression) { \
	if (!(expression)) { \
		(void)panic( \
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}
#endif

#if	SYM_CONF_MAX_TAG_ORDER > 8
#error	"more than 256 tags per logical unit not allowed."
#endif
#define	SYM_CONF_MAX_TASK	(1<<SYM_CONF_MAX_TAG_ORDER)

#ifndef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif
#if	SYM_CONF_MAX_TAG > SYM_CONF_MAX_TASK
#undef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif

#define NO_TAG	(256)

#if	SYM_CONF_MAX_TARGET > 16
#error	"more than 16 targets not allowed."
#endif

#if	SYM_CONF_MAX_LUN > 64
#error	"more than 64 logical units per target not allowed."
#endif

#define	SYM_CONF_MIN_ASYNC (40)



#define SYM_MEM_WARN	1	

#define SYM_MEM_PAGE_ORDER 0	
#define SYM_MEM_CLUSTER_SHIFT	(PAGE_SHIFT+SYM_MEM_PAGE_ORDER)
#define SYM_MEM_FREE_UNUSED	
#define SYM_MEM_SHIFT	4
#define SYM_MEM_CLUSTER_SIZE	(1UL << SYM_MEM_CLUSTER_SHIFT)
#define SYM_MEM_CLUSTER_MASK	(SYM_MEM_CLUSTER_SIZE-1)

#ifdef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_QUEUE (SYM_CONF_MAX_START+2)
#else
#define	SYM_CONF_MAX_QUEUE (7*SYM_CONF_MAX_TASK+2)
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

#if	SYM_CONF_MAX_QUEUE > SYM_MEM_CLUSTER_SIZE/8
#undef	SYM_CONF_MAX_QUEUE
#define	SYM_CONF_MAX_QUEUE (SYM_MEM_CLUSTER_SIZE/8)
#undef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

#define MAX_QUEUE	SYM_CONF_MAX_QUEUE


#define INB_OFF(np, o)		ioread8(np->s.ioaddr + (o))
#define INW_OFF(np, o)		ioread16(np->s.ioaddr + (o))
#define INL_OFF(np, o)		ioread32(np->s.ioaddr + (o))

#define OUTB_OFF(np, o, val)	iowrite8((val), np->s.ioaddr + (o))
#define OUTW_OFF(np, o, val)	iowrite16((val), np->s.ioaddr + (o))
#define OUTL_OFF(np, o, val)	iowrite32((val), np->s.ioaddr + (o))

#define INB(np, r)		INB_OFF(np, offsetof(struct sym_reg, r))
#define INW(np, r)		INW_OFF(np, offsetof(struct sym_reg, r))
#define INL(np, r)		INL_OFF(np, offsetof(struct sym_reg, r))

#define OUTB(np, r, v)		OUTB_OFF(np, offsetof(struct sym_reg, r), (v))
#define OUTW(np, r, v)		OUTW_OFF(np, offsetof(struct sym_reg, r), (v))
#define OUTL(np, r, v)		OUTL_OFF(np, offsetof(struct sym_reg, r), (v))

#define OUTONB(np, r, m)	OUTB(np, r, INB(np, r) | (m))
#define OUTOFFB(np, r, m)	OUTB(np, r, INB(np, r) & ~(m))
#define OUTONW(np, r, m)	OUTW(np, r, INW(np, r) | (m))
#define OUTOFFW(np, r, m)	OUTW(np, r, INW(np, r) & ~(m))
#define OUTONL(np, r, m)	OUTL(np, r, INL(np, r) | (m))
#define OUTOFFL(np, r, m)	OUTL(np, r, INL(np, r) & ~(m))

#define OUTL_DSP(np, v)				\
	do {					\
		MEMORY_WRITE_BARRIER();		\
		OUTL(np, nc_dsp, (v));		\
	} while (0)

#define OUTONB_STD()				\
	do {					\
		MEMORY_WRITE_BARRIER();		\
		OUTONB(np, nc_dcntl, (STD|NOCOM));	\
	} while (0)

#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	
#define HS_DISCONNECT	(3)	
#define HS_WAIT		(4)	

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	
#define HS_UNEXPECTED	(6|HS_DONEMASK)	
#define HS_COMP_ERR	(7|HS_DONEMASK)	

#define	SIR_BAD_SCSI_STATUS	(1)
#define	SIR_SEL_ATN_NO_MSG_OUT	(2)
#define	SIR_MSG_RECEIVED	(3)
#define	SIR_MSG_WEIRD		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_SCRIPT_STOPPED	(7)
#define	SIR_REJECT_TO_SEND	(8)
#define	SIR_SWIDE_OVERRUN	(9)
#define	SIR_SODL_UNDERRUN	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_TARGET_SELECTED	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_ABORT_SENT		(17)
#define	SIR_RESEL_ABORTED	(18)
#define	SIR_MSG_OUT_DONE	(19)
#define	SIR_COMPLETE_ERROR	(20)
#define	SIR_DATA_OVERRUN	(21)
#define	SIR_BAD_PHASE		(22)
#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
#define	SIR_DMAP_DIRTY		(23)
#define	SIR_MAX			(23)
#else
#define	SIR_MAX			(22)
#endif

#define	XE_EXTRA_DATA	(1)	
#define	XE_BAD_PHASE	(1<<1)	
#define	XE_PARITY_ERR	(1<<2)	
#define	XE_SODL_UNRUN	(1<<3)	
#define	XE_SWIDE_OVRUN	(1<<4)	

#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(3)

#define CCB_HASH_SHIFT		8
#define CCB_HASH_SIZE		(1UL << CCB_HASH_SHIFT)
#define CCB_HASH_MASK		(CCB_HASH_SIZE-1)
#if 1
#define CCB_HASH_CODE(dsa)	\
	(((dsa) >> (_LGRU16_(sizeof(struct sym_ccb)))) & CCB_HASH_MASK)
#else
#define CCB_HASH_CODE(dsa)	(((dsa) >> 9) & CCB_HASH_MASK)
#endif

#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
#define SYM_DMAP_SHIFT	(4)
#define SYM_DMAP_SIZE	(1u<<SYM_DMAP_SHIFT)
#define SYM_DMAP_MASK	(SYM_DMAP_SIZE-1)
#endif

#define SYM_DISC_ENABLED	(1)
#define SYM_TAGS_ENABLED	(1<<1)
#define SYM_SCAN_BOOT_DISABLED	(1<<2)
#define SYM_SCAN_LUNS_DISABLED	(1<<3)

#define SYM_AVOID_BUS_RESET	(1)

#define SYM_SNOOP_TIMEOUT (10000000)
#define BUS_8_BIT	0
#define BUS_16_BIT	1

struct sym_trans {
	u8 period;
	u8 offset;
	unsigned int width:1;
	unsigned int iu:1;
	unsigned int dt:1;
	unsigned int qas:1;
	unsigned int check_nego:1;
	unsigned int renego:2;
};

struct sym_tcbh {
	u32	luntbl_sa;	
	u32	lun0_sa;	
	u_char	uval;		
	u_char	sval;		
	u_char	filler1;
	u_char	wval;		
};

struct sym_tcb {
	struct sym_tcbh head;

	u32	*luntbl;	
	int	nlcb;		

	struct sym_lcb *lun0p;		
#if SYM_CONF_MAX_LUN > 1
	struct sym_lcb **lunmp;		
#endif

#ifdef	SYM_HAVE_STCB
	struct sym_stcb s;
#endif

	
	struct sym_trans tgoal;

	
	struct sym_trans tprint;

	struct sym_ccb *  nego_cp;	

	u_char	to_reset;

	unsigned char	usrflags;
	unsigned char	usr_period;
	unsigned char	usr_width;
	unsigned short	usrtags;
	struct scsi_target *starget;
};

struct sym_lcbh {
	u32	resel_sa;

	u32	itl_task_sa;

	u32	itlq_tbl_sa;
};

struct sym_lcb {
	struct sym_lcbh head;

	u32	*itlq_tbl;	

	u_short	busy_itlq;	
	u_short	busy_itl;	

	u_short	ia_tag;		
	u_short	if_tag;		
	u_char	*cb_tags;	

#ifdef	SYM_HAVE_SLCB
	struct sym_slcb s;
#endif

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	SYM_QUEHEAD waiting_ccbq;
	SYM_QUEHEAD started_ccbq;
	int	num_sgood;
	u_short	started_tags;
	u_short	started_no_tag;
	u_short	started_max;
	u_short	started_limit;
#endif

#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
	u_char		tags_si;	
	u_short		tags_sum[2];	
	u_short		tags_since;	
#endif

	u_char to_clear;

	u_char	user_flags;
	u_char	curr_flags;
};

struct sym_actscr {
	u32	start;		
	u32	restart;	
};

struct sym_pmc {
	struct	sym_tblmove sg;	
	u32	ret;		
};

#if SYM_CONF_MAX_LUN <= 1
#define sym_lp(tp, lun) (!lun) ? (tp)->lun0p : NULL
#else
#define sym_lp(tp, lun) \
	(!lun) ? (tp)->lun0p : (tp)->lunmp ? (tp)->lunmp[(lun)] : NULL
#endif


#define  HX_REG	scr0
#define  HX_PRT	nc_scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  HF_REG	scr3
#define  HF_PRT	nc_scr3

#define  host_xflags   phys.head.status[0]
#define  host_status   phys.head.status[1]
#define  ssss_status   phys.head.status[2]
#define  host_flags    phys.head.status[3]

#define HF_IN_PM0	1u
#define HF_IN_PM1	(1u<<1)
#define HF_ACT_PM	(1u<<2)
#define HF_DP_SAVED	(1u<<3)
#define HF_SENSE	(1u<<4)
#define HF_EXT_ERR	(1u<<5)
#define HF_DATA_IN	(1u<<6)
#ifdef SYM_CONF_IARB_SUPPORT
#define HF_HINT_IARB	(1u<<7)
#endif

#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
#define	HX_DMAP_DIRTY	(1u<<7)
#endif


struct sym_ccbh {
	struct sym_actscr go;

	/*
	 *  SCRIPTS jump address that deal with data pointers.
	 *  'savep' points to the position in the script responsible 
	 *  for the actual transfer of data.
	 *  It's written on reception of a SAVE_DATA_POINTER message.
	 */
	u32	savep;		
	u32	lastp;		

	u8	status[4];
};

#if	SYM_CONF_GENERIC_SUPPORT
#define sym_set_script_dp(np, cp, dp)				\
	do {							\
		if (np->features & FE_LDSTR)			\
			cp->phys.head.lastp = cpu_to_scr(dp);	\
		else						\
			np->ccb_head.lastp = cpu_to_scr(dp);	\
	} while (0)
#define sym_get_script_dp(np, cp) 				\
	scr_to_cpu((np->features & FE_LDSTR) ?			\
		cp->phys.head.lastp : np->ccb_head.lastp)
#else
#define sym_set_script_dp(np, cp, dp)				\
	do {							\
		cp->phys.head.lastp = cpu_to_scr(dp);		\
	} while (0)

#define sym_get_script_dp(np, cp) (cp->phys.head.lastp)
#endif

struct sym_dsb {
	struct sym_ccbh head;

	struct sym_pmc pm0;
	struct sym_pmc pm1;

	struct sym_tblsel  select;
	struct sym_tblmove smsg;
	struct sym_tblmove smsg_ext;
	struct sym_tblmove cmd;
	struct sym_tblmove sense;
	struct sym_tblmove wresid;
	struct sym_tblmove data [SYM_CONF_MAX_SG];
};

struct sym_ccb {
	struct sym_dsb phys;

	struct scsi_cmnd *cmd;	
	u8	cdb_buf[16];	
#define	SYM_SNS_BBUF_LEN 32
	u8	sns_bbuf[SYM_SNS_BBUF_LEN]; 
	int	data_len;	
	int	segments;	

	u8	order;		
	unsigned char odd_byte_adjustment;	

	u_char	nego_status;	
	u_char	xerr_status;	
	u32	extra_bytes;	

	u_char	scsi_smsg [12];
	u_char	scsi_smsg2[12];

	u_char	sensecmd[6];	
	u_char	sv_scsi_status;	
	u_char	sv_xerr_status;	
	int	sv_resid;	

	u32	ccb_ba;		
	u_short	tag;		
				
	u_char	target;
	u_char	lun;
	struct sym_ccb *link_ccbh;	
	SYM_QUEHEAD link_ccbq;	
	u32	startp;		
	u32	goalp;		
	int	ext_sg;		
	int	ext_ofs;	
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	SYM_QUEHEAD link2_ccbq;	
	u_char	started;	
#endif
	u_char	to_abort;	
#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
	u_char	tags_si;	
#endif
};

#define CCB_BA(cp,lbl)	cpu_to_scr(cp->ccb_ba + offsetof(struct sym_ccb, lbl))

typedef struct device *m_pool_ident_t;

struct sym_hcb {
#if	SYM_CONF_GENERIC_SUPPORT
	struct sym_ccbh	ccb_head;
	struct sym_tcbh	tcb_head;
	struct sym_lcbh	lcb_head;
#endif
	struct sym_actscr idletask, notask, bad_itl, bad_itlq;
	u32 idletask_ba, notask_ba, bad_itl_ba, bad_itlq_ba;

	u32	*badluntbl;	
	u32	badlun_sa;	

	u32	hcb_ba;

	u32	scr_ram_seg;

	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest3, sv_ctest4,
		sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4, sv_scntl4,
		sv_stest1;

	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest3, rv_ctest4, 
		rv_ctest5, rv_stest2, rv_ccntl0, rv_ccntl1, rv_scntl4;

	struct sym_tcb	target[SYM_CONF_MAX_TARGET];

	u32		*targtbl;
	u32		targtbl_ba;

	m_pool_ident_t	bus_dmat;

	struct sym_shcb s;

	u32		mmio_ba;	
	u32		ram_ba;		

	u_char		*scripta0;	
	u_char		*scriptb0;
	u_char		*scriptz0;
	u32		scripta_ba;	
	u32		scriptb_ba;	
	u32		scriptz_ba;
	u_short		scripta_sz;	
	u_short		scriptb_sz;
	u_short		scriptz_sz;

	struct sym_fwa_ba fwa_bas;	
	struct sym_fwb_ba fwb_bas;	
	struct sym_fwz_ba fwz_bas;	
	void		(*fw_setup)(struct sym_hcb *np, struct sym_fw *fw);
	void		(*fw_patch)(struct Scsi_Host *);
	char		*fw_name;

	u_int	features;	
	u_char	myaddr;		
	u_char	maxburst;	
	u_char	maxwide;	
	u_char	minsync;	
	u_char	maxsync;	
	u_char	maxoffs;	
	u_char	minsync_dt;	
	u_char	maxsync_dt;	
	u_char	maxoffs_dt;	
	u_char	multiplier;	
	u_char	clock_divn;	
	u32	clock_khz;	
	u32	pciclk_khz;	
	volatile		
	u32	*squeue;	
	u32	squeue_ba;	
	u_short	squeueput;	
	u_short	actccbs;	

	u_short	dqueueget;	
	volatile		
	u32	*dqueue;	
	u32	dqueue_ba;	

	/*
	 *  Miscellaneous buffers accessed by the scripts-processor.
	 *  They shall be DWORD aligned, because they may be read or 
	 *  written with a script command.
	 */
	u_char		msgout[8];	
	u_char		msgin [8];	
	u32		lastmsg;	
	u32		scratch;	
					
	u_char		usrflags;	
	u_char		scsi_mode;	
	u_char		verbose;	

	struct sym_ccb **ccbh;			
					
	SYM_QUEHEAD	free_ccbq;	
	SYM_QUEHEAD	busy_ccbq;	

	SYM_QUEHEAD	comp_ccbq;

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	SYM_QUEHEAD	dummy_ccbq;
#endif

#ifdef SYM_CONF_IARB_SUPPORT
	u_short		iarb_max;	
	u_short		iarb_count;	
	struct sym_ccb *	last_cp;
#endif

	u_char		abrt_msg[4];	
	struct sym_tblmove abrt_tbl;	
	struct sym_tblsel  abrt_sel;	
	u_char		istat_sem;	

#if	SYM_CONF_DMA_ADDRESSING_MODE != 0
	u_char	use_dac;		
#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
	u_char	dmap_dirty;		
	u32	dmap_bah[SYM_DMAP_SIZE];
#endif
#endif
};

#if SYM_CONF_DMA_ADDRESSING_MODE == 0
#define use_dac(np)	0
#define set_dac(np)	do { } while (0)
#else
#define use_dac(np)	(np)->use_dac
#define set_dac(np)	(np)->use_dac = 1
#endif

#define HCB_BA(np, lbl)	(np->hcb_ba + offsetof(struct sym_hcb, lbl))


struct sym_fw * sym_find_firmware(struct sym_chip *chip);
void sym_fw_bind_script(struct sym_hcb *np, u32 *start, int len);

char *sym_driver_name(void);
void sym_print_xerr(struct scsi_cmnd *cmd, int x_status);
int sym_reset_scsi_bus(struct sym_hcb *np, int enab_int);
struct sym_chip *sym_lookup_chip_table(u_short device_id, u_char revision);
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
void sym_start_next_ccbs(struct sym_hcb *np, struct sym_lcb *lp, int maxn);
#else
void sym_put_start_queue(struct sym_hcb *np, struct sym_ccb *cp);
#endif
void sym_start_up(struct Scsi_Host *, int reason);
irqreturn_t sym_interrupt(struct Scsi_Host *);
int sym_clear_tasks(struct sym_hcb *np, int cam_status, int target, int lun, int task);
struct sym_ccb *sym_get_ccb(struct sym_hcb *np, struct scsi_cmnd *cmd, u_char tag_order);
void sym_free_ccb(struct sym_hcb *np, struct sym_ccb *cp);
struct sym_lcb *sym_alloc_lcb(struct sym_hcb *np, u_char tn, u_char ln);
int sym_free_lcb(struct sym_hcb *np, u_char tn, u_char ln);
int sym_queue_scsiio(struct sym_hcb *np, struct scsi_cmnd *csio, struct sym_ccb *cp);
int sym_abort_scsiio(struct sym_hcb *np, struct scsi_cmnd *ccb, int timed_out);
int sym_reset_scsi_target(struct sym_hcb *np, int target);
void sym_hcb_free(struct sym_hcb *np);
int sym_hcb_attach(struct Scsi_Host *shost, struct sym_fw *fw, struct sym_nvram *nvram);


#if   SYM_CONF_DMA_ADDRESSING_MODE == 0
#define DMA_DAC_MASK	DMA_BIT_MASK(32)
#define sym_build_sge(np, data, badd, len)	\
do {						\
	(data)->addr = cpu_to_scr(badd);	\
	(data)->size = cpu_to_scr(len);		\
} while (0)
#elif SYM_CONF_DMA_ADDRESSING_MODE == 1
#define DMA_DAC_MASK	DMA_BIT_MASK(40)
#define sym_build_sge(np, data, badd, len)				\
do {									\
	(data)->addr = cpu_to_scr(badd);				\
	(data)->size = cpu_to_scr((((badd) >> 8) & 0xff000000) + len);	\
} while (0)
#elif SYM_CONF_DMA_ADDRESSING_MODE == 2
#define DMA_DAC_MASK	DMA_BIT_MASK(64)
int sym_lookup_dmap(struct sym_hcb *np, u32 h, int s);
static inline void
sym_build_sge(struct sym_hcb *np, struct sym_tblmove *data, u64 badd, int len)
{
	u32 h = (badd>>32);
	int s = (h&SYM_DMAP_MASK);

	if (h != np->dmap_bah[s])
		goto bad;
good:
	(data)->addr = cpu_to_scr(badd);
	(data)->size = cpu_to_scr((s<<24) + len);
	return;
bad:
	s = sym_lookup_dmap(np, h, s);
	goto good;
}
#else
#error "Unsupported DMA addressing mode"
#endif


#define sym_get_mem_cluster()	\
	(void *) __get_free_pages(GFP_ATOMIC, SYM_MEM_PAGE_ORDER)
#define sym_free_mem_cluster(p)	\
	free_pages((unsigned long)p, SYM_MEM_PAGE_ORDER)

typedef struct sym_m_link {
	struct sym_m_link *next;
} *m_link_p;

typedef struct sym_m_vtob {	
	struct sym_m_vtob *next;
	void *vaddr;		
	dma_addr_t baddr;	
} *m_vtob_p;

#define VTOB_HASH_SHIFT		5
#define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#define VTOB_HASH_MASK		(VTOB_HASH_SIZE-1)
#define VTOB_HASH_CODE(m)	\
	((((unsigned long)(m)) >> SYM_MEM_CLUSTER_SHIFT) & VTOB_HASH_MASK)

typedef struct sym_m_pool {
	m_pool_ident_t	dev_dmat;	
	void * (*get_mem_cluster)(struct sym_m_pool *);
#ifdef	SYM_MEM_FREE_UNUSED
	void (*free_mem_cluster)(struct sym_m_pool *, void *);
#endif
#define M_GET_MEM_CLUSTER()		mp->get_mem_cluster(mp)
#define M_FREE_MEM_CLUSTER(p)		mp->free_mem_cluster(mp, p)
	int nump;
	m_vtob_p vtob[VTOB_HASH_SIZE];
	struct sym_m_pool *next;
	struct sym_m_link h[SYM_MEM_CLUSTER_SHIFT - SYM_MEM_SHIFT + 1];
} *m_pool_p;

void *__sym_calloc_dma(m_pool_ident_t dev_dmat, int size, char *name);
void __sym_mfree_dma(m_pool_ident_t dev_dmat, void *m, int size, char *name);
dma_addr_t __vtobus(m_pool_ident_t dev_dmat, void *m);

#define _uvptv_(p) ((void *)((u_long)(p)))

#define _sym_calloc_dma(np, l, n)	__sym_calloc_dma(np->bus_dmat, l, n)
#define _sym_mfree_dma(np, p, l, n)	\
			__sym_mfree_dma(np->bus_dmat, _uvptv_(p), l, n)
#define sym_calloc_dma(l, n)		_sym_calloc_dma(np, l, n)
#define sym_mfree_dma(p, l, n)		_sym_mfree_dma(np, p, l, n)
#define vtobus(p)			__vtobus(np->bus_dmat, _uvptv_(p))


#define sym_m_pool_match(mp_id1, mp_id2)	(mp_id1 == mp_id2)

static inline void *sym_m_get_dma_mem_cluster(m_pool_p mp, m_vtob_p vbp)
{
	void *vaddr = NULL;
	dma_addr_t baddr = 0;

	vaddr = dma_alloc_coherent(mp->dev_dmat, SYM_MEM_CLUSTER_SIZE, &baddr,
			GFP_ATOMIC);
	if (vaddr) {
		vbp->vaddr = vaddr;
		vbp->baddr = baddr;
	}
	return vaddr;
}

static inline void sym_m_free_dma_mem_cluster(m_pool_p mp, m_vtob_p vbp)
{
	dma_free_coherent(mp->dev_dmat, SYM_MEM_CLUSTER_SIZE, vbp->vaddr,
			vbp->baddr);
}

#endif 
