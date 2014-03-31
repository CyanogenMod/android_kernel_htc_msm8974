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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/pci_ids.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/bcma/bcma.h>
#include <asm/unaligned.h>
#include <defs.h>
#include <brcmu_wifi.h>
#include <brcmu_utils.h>
#include <brcm_hw_ids.h>
#include <soc.h>
#include "sdio_host.h"
#include "sdio_chip.h"

#define DCMD_RESP_TIMEOUT  2000	

#ifdef DEBUG

#define BRCMF_TRAP_INFO_SIZE	80

#define CBUF_LEN	(128)

struct rte_log_le {
	__le32 buf;		
	__le32 buf_size;
	__le32 idx;
	char *_buf_compat;	
};

struct rte_console {
	uint vcons_in;
	uint vcons_out;

	/* Output (logging) buffer
	 * Console output is written to a ring buffer log_buf at index log_idx.
	 * The host may read the output when it sees log_idx advance.
	 * Output will be lost if the output wraps around faster than the host
	 * polls.
	 */
	struct rte_log_le log_le;

	uint cbuf_idx;
	char cbuf[CBUF_LEN];
};

#endif				
#include <chipcommon.h>

#include "dhd_bus.h"
#include "dhd_dbg.h"

#define TXQLEN		2048	
#define TXHI		(TXQLEN - 256)	
#define TXLOW		(TXHI - 256)	
#define PRIOMASK	7

#define TXRETRIES	2	

#define BRCMF_RXBOUND	50	

#define BRCMF_TXBOUND	20	

#define BRCMF_TXMINMAX	1	

#define MEMBLOCK	2048	
#define MAX_DATA_BUF	(32 * 1024)	

#define BRCMF_FIRSTREAD	(1 << 6)



#define SBSDIO_DEVCTL_SETBUSY		0x01
#define SBSDIO_DEVCTL_SPI_INTR_SYNC	0x02
#define SBSDIO_DEVCTL_CA_INT_ONLY	0x04
#define SBSDIO_DEVCTL_PADS_ISO		0x08
#define SBSDIO_DEVCTL_SB_RST_CTL	0x30
#define SBSDIO_DEVCTL_RST_CORECTL	0x00
#define SBSDIO_DEVCTL_RST_BPRESET	0x10
#define SBSDIO_DEVCTL_RST_NOBPRESET	0x20


#define SBSDIO_CIS_BASE_COMMON		0x1000
#define SBSDIO_CIS_SIZE_LIMIT		0x200
#define SBSDIO_CIS_OFT_ADDR_MASK	0x1FFFF

#define SBSDIO_CIS_MANFID_TUPLE_LEN	6

#define I_SMB_SW0	(1 << 0)	
#define I_SMB_SW1	(1 << 1)	
#define I_SMB_SW2	(1 << 2)	
#define I_SMB_SW3	(1 << 3)	
#define I_SMB_SW_MASK	0x0000000f	
#define I_SMB_SW_SHIFT	0	
#define I_HMB_SW0	(1 << 4)	
#define I_HMB_SW1	(1 << 5)	
#define I_HMB_SW2	(1 << 6)	
#define I_HMB_SW3	(1 << 7)	
#define I_HMB_SW_MASK	0x000000f0	
#define I_HMB_SW_SHIFT	4	
#define I_WR_OOSYNC	(1 << 8)	
#define I_RD_OOSYNC	(1 << 9)	
#define	I_PC		(1 << 10)	
#define	I_PD		(1 << 11)	
#define	I_DE		(1 << 12)	
#define	I_RU		(1 << 13)	
#define	I_RO		(1 << 14)	
#define	I_XU		(1 << 15)	
#define	I_RI		(1 << 16)	
#define I_BUSPWR	(1 << 17)	
#define I_XMTDATA_AVAIL (1 << 23)	
#define	I_XI		(1 << 24)	
#define I_RF_TERM	(1 << 25)	
#define I_WF_TERM	(1 << 26)	
#define I_PCMCIA_XU	(1 << 27)	
#define I_SBINT		(1 << 28)	
#define I_CHIPACTIVE	(1 << 29)	
#define I_SRESET	(1 << 30)	
#define I_IOE2		(1U << 31)	
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)
#define I_DMA		(I_RI | I_XI | I_ERRORS)

#define CC_CISRDY		(1 << 0)	
#define CC_BPRESEN		(1 << 1)	
#define CC_F2RDY		(1 << 2)	
#define CC_CLRPADSISO		(1 << 3)	
#define CC_XMTDATAAVAIL_MODE	(1 << 4)
#define CC_XMTDATAAVAIL_CTRL	(1 << 5)

#define SFC_RF_TERM	(1 << 0)	
#define SFC_WF_TERM	(1 << 1)	
#define SFC_CRC4WOOS	(1 << 2)	
#define SFC_ABORTALL	(1 << 3)	

#define SDPCM_FRAMETAG_LEN	4	

#define SDPCM_HDRLEN	(SDPCM_FRAMETAG_LEN + SDPCM_SWHEADER_LEN)
#define SDPCM_RESERVE	(SDPCM_HDRLEN + BRCMF_SDALIGN)


#define SMB_NAK		(1 << 0)	
#define SMB_INT_ACK	(1 << 1)	
#define SMB_USE_OOB	(1 << 2)	
#define SMB_DEV_INT	(1 << 3)	

#define SMB_DATA_VERSION_SHIFT	16	


#define I_HMB_FC_STATE	I_HMB_SW0	
#define I_HMB_FC_CHANGE	I_HMB_SW1	
#define I_HMB_FRAME_IND	I_HMB_SW2	
#define I_HMB_HOST_INT	I_HMB_SW3	

#define HMB_DATA_NAKHANDLED	1	
#define HMB_DATA_DEVREADY	2	
#define HMB_DATA_FC		4	
#define HMB_DATA_FWREADY	8	

#define HMB_DATA_FCDATA_MASK	0xff000000
#define HMB_DATA_FCDATA_SHIFT	24

#define HMB_DATA_VERSION_MASK	0x00ff0000
#define HMB_DATA_VERSION_SHIFT	16


#define SDPCM_PROT_VERSION	4

#define SDPCM_PACKET_SEQUENCE(p)	(((u8 *)p)[0] & 0xff)

#define SDPCM_CHANNEL_MASK		0x00000f00
#define SDPCM_CHANNEL_SHIFT		8
#define SDPCM_PACKET_CHANNEL(p)		(((u8 *)p)[1] & 0x0f)

#define SDPCM_NEXTLEN_OFFSET		2

#define SDPCM_DOFFSET_OFFSET		3	
#define SDPCM_DOFFSET_VALUE(p)		(((u8 *)p)[SDPCM_DOFFSET_OFFSET] & 0xff)
#define SDPCM_DOFFSET_MASK		0xff000000
#define SDPCM_DOFFSET_SHIFT		24
#define SDPCM_FCMASK_OFFSET		4	
#define SDPCM_FCMASK_VALUE(p)		(((u8 *)p)[SDPCM_FCMASK_OFFSET] & 0xff)
#define SDPCM_WINDOW_OFFSET		5	
#define SDPCM_WINDOW_VALUE(p)		(((u8 *)p)[SDPCM_WINDOW_OFFSET] & 0xff)

#define SDPCM_SWHEADER_LEN	8	

#define SDPCM_CONTROL_CHANNEL	0	
#define SDPCM_EVENT_CHANNEL	1	
#define SDPCM_DATA_CHANNEL	2	
#define SDPCM_GLOM_CHANNEL	3	
#define SDPCM_TEST_CHANNEL	15	

#define SDPCM_SEQUENCE_WRAP	256	

#define SDPCM_GLOMDESC(p)	(((u8 *)p)[1] & 0x80)

#define SDPCM_SHARED_VERSION       0x0002
#define SDPCM_SHARED_VERSION_MASK  0x00FF
#define SDPCM_SHARED_ASSERT_BUILT  0x0100
#define SDPCM_SHARED_ASSERT        0x0200
#define SDPCM_SHARED_TRAP          0x0400

#define MAX_HDR_READ	(1 << 6)
#define MAX_RX_DATASZ	2048

#define BRCMF_WAIT_F2RDY	3000

#undef PMU_MAX_TRANSITION_DLY
#define PMU_MAX_TRANSITION_DLY 1000000

#define BRCMF_INIT_CLKCTL1	(SBSDIO_FORCE_HW_CLKREQ_OFF |	\
					SBSDIO_ALP_AVAIL_REQ)

#define F2SYNC	(SDIO_REQ_4BYTE | SDIO_REQ_FIXED)

#define BRCMF_SDIO_FW_NAME	"brcm/brcmfmac-sdio.bin"
#define BRCMF_SDIO_NV_NAME	"brcm/brcmfmac-sdio.txt"
MODULE_FIRMWARE(BRCMF_SDIO_FW_NAME);
MODULE_FIRMWARE(BRCMF_SDIO_NV_NAME);

#define BRCMF_IDLE_IMMEDIATE	(-1)	
#define BRCMF_IDLE_ACTIVE	0	
#define BRCMF_IDLE_INTERVAL	1

static uint prio2prec(u32 prio)
{
	return (prio == PRIO_8021D_NONE || prio == PRIO_8021D_BE) ?
	       (prio^2) : prio;
}

struct sdpcmd_regs {
	u32 corecontrol;		
	u32 corestatus;			
	u32 PAD[1];
	u32 biststatus;			

	
	u16 pcmciamesportaladdr;	
	u16 PAD[1];
	u16 pcmciamesportalmask;	
	u16 PAD[1];
	u16 pcmciawrframebc;		
	u16 PAD[1];
	u16 pcmciaunderflowtimer;	
	u16 PAD[1];

	
	u32 intstatus;			
	u32 hostintmask;		
	u32 intmask;			
	u32 sbintstatus;		
	u32 sbintmask;			
	u32 funcintmask;		
	u32 PAD[2];
	u32 tosbmailbox;		
	u32 tohostmailbox;		
	u32 tosbmailboxdata;		
	u32 tohostmailboxdata;		

	
	u32 sdioaccess;			
	u32 PAD[3];

	
	u8 pcmciaframectrl;		
	u8 PAD[3];
	u8 pcmciawatermark;		
	u8 PAD[155];

	
	u32 intrcvlazy;			
	u32 PAD[3];

	
	u32 cmd52rd;			
	u32 cmd52wr;			
	u32 cmd53rd;			
	u32 cmd53wr;			
	u32 abort;			
	u32 datacrcerror;		
	u32 rdoutofsync;		
	u32 wroutofsync;		
	u32 writebusy;			
	u32 readwait;			
	u32 readterm;			
	u32 writeterm;			
	u32 PAD[40];
	u32 clockctlstatus;		
	u32 PAD[7];

	u32 PAD[128];			

	
	char cis[512];			

	
	char pcmciafcr[256];		
	u16 PAD[55];

	
	u16 backplanecsr;		
	u16 backplaneaddr0;		
	u16 backplaneaddr1;		
	u16 backplaneaddr2;		
	u16 backplaneaddr3;		
	u16 backplanedata0;		
	u16 backplanedata1;		
	u16 backplanedata2;		
	u16 backplanedata3;		
	u16 PAD[31];

	
	u16 spromstatus;		
	u32 PAD[464];

	u16 PAD[0x80];
};

#ifdef DEBUG
struct brcmf_console {
	uint count;		
	uint log_addr;		
	struct rte_log_le log_le;	
	uint bufsize;		
	u8 *buf;		
	uint last;		
};
#endif				

struct sdpcm_shared {
	u32 flags;
	u32 trap_addr;
	u32 assert_exp_addr;
	u32 assert_file_addr;
	u32 assert_line;
	u32 console_addr;	
	u32 msgtrace_addr;
	u8 tag[32];
};

struct sdpcm_shared_le {
	__le32 flags;
	__le32 trap_addr;
	__le32 assert_exp_addr;
	__le32 assert_file_addr;
	__le32 assert_line;
	__le32 console_addr;	
	__le32 msgtrace_addr;
	u8 tag[32];
};


struct brcmf_sdio {
	struct brcmf_sdio_dev *sdiodev;	
	struct chip_info *ci;	
	char *vars;		
	uint varsz;		

	u32 ramsize;		

	u32 hostintmask;	
	u32 intstatus;	
	bool dpc_sched;		
	bool fcstate;		

	uint blocksize;		
	uint roundup;		

	struct pktq txq;	
	u8 flowcontrol;	
	u8 tx_seq;		
	u8 tx_max;		

	u8 hdrbuf[MAX_HDR_READ + BRCMF_SDALIGN];
	u8 *rxhdr;		
	u16 nextlen;		
	u8 rx_seq;		
	bool rxskip;		

	uint rxbound;		
	uint txbound;		
	uint txminmax;

	struct sk_buff *glomd;	
	struct sk_buff_head glom; 
	uint glomerr;		

	u8 *rxbuf;		
	uint rxblen;		
	u8 *rxctl;		
	u8 *databuf;		
	u8 *dataptr;		
	uint rxlen;		

	u8 sdpcm_ver;	

	bool intr;		
	bool poll;		
	bool ipend;		
	uint intrcount;		
	uint lastintrs;		
	uint spurious;		
	uint pollrate;		
	uint polltick;		
	uint pollcnt;		

#ifdef DEBUG
	uint console_interval;
	struct brcmf_console console;	
	uint console_addr;	
#endif				

	uint regfails;		

	uint clkstate;		
	bool activity;		
	s32 idletime;		
	s32 idlecount;	
	s32 idleclock;	
	s32 sd_rxchain;
	bool use_rxchain;	
	bool sleeping;		
	bool rxflow_mode;	
	bool rxflow;		
	bool alp_only;		
	bool usebufpool;

	
	uint tx_sderrs;		
	uint fcqueued;		
	uint rxrtx;		
	uint rx_toolong;	
	uint rxc_errors;	
	uint rx_hdrfail;	
	uint rx_badhdr;		
	uint rx_badseq;		
	uint fc_rcvd;		
	uint fc_xoff;		
	uint fc_xon;		
	uint rxglomfail;	
	uint rxglomframes;	
	uint rxglompkts;	
	uint f2rxhdrs;		
	uint f2rxdata;		
	uint f2txdata;		
	uint f1regdata;		
	uint tickcnt;		
	unsigned long tx_ctlerrs;	
	unsigned long tx_ctlpkts;	
	unsigned long rx_ctlerrs;	
	unsigned long rx_ctlpkts;	
	unsigned long rx_readahead_cnt;	

	u8 *ctrl_frame_buf;
	u32 ctrl_frame_len;
	bool ctrl_frame_stat;

	spinlock_t txqlock;
	wait_queue_head_t ctrl_wait;
	wait_queue_head_t dcmd_resp_wait;

	struct timer_list timer;
	struct completion watchdog_wait;
	struct task_struct *watchdog_tsk;
	bool wd_timer_valid;
	uint save_ms;

	struct task_struct *dpc_tsk;
	struct completion dpc_wait;
	struct list_head dpc_tsklst;
	spinlock_t dpc_tl_lock;

	struct semaphore sdsem;

	const struct firmware *firmware;
	u32 fw_ptr;

	bool txoff;		
};

#define CLK_NONE	0
#define CLK_SDONLY	1
#define CLK_PENDING	2	
#define CLK_AVAIL	3

#ifdef DEBUG
static int qcount[NUMPRIO];
static int tx_packets[NUMPRIO];
#endif				

#define SDIO_DRIVE_STRENGTH	6	

#define RETRYCHAN(chan) ((chan) == SDPCM_EVENT_CHANNEL)

static const uint retry_limit = 2;

static const uint max_roundup = 512;

#define ALIGNMENT  4

static void pkt_align(struct sk_buff *p, int len, int align)
{
	uint datalign;
	datalign = (unsigned long)(p->data);
	datalign = roundup(datalign, (align)) - datalign;
	if (datalign)
		skb_pull(p, datalign);
	__skb_trim(p, len);
}

static bool data_ok(struct brcmf_sdio *bus)
{
	return (u8)(bus->tx_max - bus->tx_seq) != 0 &&
	       ((u8)(bus->tx_max - bus->tx_seq) & 0x80) == 0;
}

static void
r_sdreg32(struct brcmf_sdio *bus, u32 *regvar, u32 reg_offset, u32 *retryvar)
{
	u8 idx = brcmf_sdio_chip_getinfidx(bus->ci, BCMA_CORE_SDIO_DEV);
	*retryvar = 0;
	do {
		*regvar = brcmf_sdcard_reg_read(bus->sdiodev,
				bus->ci->c_inf[idx].base + reg_offset,
				sizeof(u32));
	} while (brcmf_sdcard_regfail(bus->sdiodev) &&
		 (++(*retryvar) <= retry_limit));
	if (*retryvar) {
		bus->regfails += (*retryvar-1);
		if (*retryvar > retry_limit) {
			brcmf_dbg(ERROR, "FAILED READ %Xh\n", reg_offset);
			*regvar = 0;
		}
	}
}

static void
w_sdreg32(struct brcmf_sdio *bus, u32 regval, u32 reg_offset, u32 *retryvar)
{
	u8 idx = brcmf_sdio_chip_getinfidx(bus->ci, BCMA_CORE_SDIO_DEV);
	*retryvar = 0;
	do {
		brcmf_sdcard_reg_write(bus->sdiodev,
				       bus->ci->c_inf[idx].base + reg_offset,
				       sizeof(u32), regval);
	} while (brcmf_sdcard_regfail(bus->sdiodev) &&
		 (++(*retryvar) <= retry_limit));
	if (*retryvar) {
		bus->regfails += (*retryvar-1);
		if (*retryvar > retry_limit)
			brcmf_dbg(ERROR, "FAILED REGISTER WRITE %Xh\n",
				  reg_offset);
	}
}

#define PKT_AVAILABLE()		(intstatus & I_HMB_FRAME_IND)

#define HOSTINTMASK		(I_HMB_SW_MASK | I_CHIPACTIVE)

static void brcmf_sdbrcm_pktfree2(struct brcmf_sdio *bus, struct sk_buff *pkt)
{
	if (bus->usebufpool)
		brcmu_pkt_buf_free_skb(pkt);
}

static int brcmf_sdbrcm_htclk(struct brcmf_sdio *bus, bool on, bool pendok)
{
	int err;
	u8 clkctl, clkreq, devctl;
	unsigned long timeout;

	brcmf_dbg(TRACE, "Enter\n");

	clkctl = 0;

	if (on) {
		
		clkreq =
		    bus->alp_only ? SBSDIO_ALP_AVAIL_REQ : SBSDIO_HT_AVAIL_REQ;

		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				       SBSDIO_FUNC1_CHIPCLKCSR, clkreq, &err);
		if (err) {
			brcmf_dbg(ERROR, "HT Avail request error: %d\n", err);
			return -EBADE;
		}

		
		clkctl = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					       SBSDIO_FUNC1_CHIPCLKCSR, &err);
		if (err) {
			brcmf_dbg(ERROR, "HT Avail read error: %d\n", err);
			return -EBADE;
		}

		
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only) && pendok) {
			
			devctl = brcmf_sdcard_cfg_read(bus->sdiodev,
					SDIO_FUNC_1,
					SBSDIO_DEVICE_CTL, &err);
			if (err) {
				brcmf_dbg(ERROR, "Devctl error setting CA: %d\n",
					  err);
				return -EBADE;
			}

			devctl |= SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
					       SBSDIO_DEVICE_CTL, devctl, &err);
			brcmf_dbg(INFO, "CLKCTL: set PENDING\n");
			bus->clkstate = CLK_PENDING;

			return 0;
		} else if (bus->clkstate == CLK_PENDING) {
			
			devctl =
			    brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
						  SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				SBSDIO_DEVICE_CTL, devctl, &err);
		}

		
		timeout = jiffies +
			  msecs_to_jiffies(PMU_MAX_TRANSITION_DLY/1000);
		while (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
			clkctl = brcmf_sdcard_cfg_read(bus->sdiodev,
						       SDIO_FUNC_1,
						       SBSDIO_FUNC1_CHIPCLKCSR,
						       &err);
			if (time_after(jiffies, timeout))
				break;
			else
				usleep_range(5000, 10000);
		}
		if (err) {
			brcmf_dbg(ERROR, "HT Avail request error: %d\n", err);
			return -EBADE;
		}
		if (!SBSDIO_CLKAV(clkctl, bus->alp_only)) {
			brcmf_dbg(ERROR, "HT Avail timeout (%d): clkctl 0x%02x\n",
				  PMU_MAX_TRANSITION_DLY, clkctl);
			return -EBADE;
		}

		
		bus->clkstate = CLK_AVAIL;
		brcmf_dbg(INFO, "CLKCTL: turned ON\n");

#if defined(DEBUG)
		if (!bus->alp_only) {
			if (SBSDIO_ALPONLY(clkctl))
				brcmf_dbg(ERROR, "HT Clock should be on\n");
		}
#endif				

		bus->activity = true;
	} else {
		clkreq = 0;

		if (bus->clkstate == CLK_PENDING) {
			
			devctl = brcmf_sdcard_cfg_read(bus->sdiodev,
					SDIO_FUNC_1,
					SBSDIO_DEVICE_CTL, &err);
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				SBSDIO_DEVICE_CTL, devctl, &err);
		}

		bus->clkstate = CLK_SDONLY;
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			SBSDIO_FUNC1_CHIPCLKCSR, clkreq, &err);
		brcmf_dbg(INFO, "CLKCTL: turned OFF\n");
		if (err) {
			brcmf_dbg(ERROR, "Failed access turning clock off: %d\n",
				  err);
			return -EBADE;
		}
	}
	return 0;
}

static int brcmf_sdbrcm_sdclk(struct brcmf_sdio *bus, bool on)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (on)
		bus->clkstate = CLK_SDONLY;
	else
		bus->clkstate = CLK_NONE;

	return 0;
}

static int brcmf_sdbrcm_clkctl(struct brcmf_sdio *bus, uint target, bool pendok)
{
#ifdef DEBUG
	uint oldstate = bus->clkstate;
#endif				

	brcmf_dbg(TRACE, "Enter\n");

	
	if (bus->clkstate == target) {
		if (target == CLK_AVAIL) {
			brcmf_sdbrcm_wd_timer(bus, BRCMF_WD_POLL_MS);
			bus->activity = true;
		}
		return 0;
	}

	switch (target) {
	case CLK_AVAIL:
		
		if (bus->clkstate == CLK_NONE)
			brcmf_sdbrcm_sdclk(bus, true);
		
		brcmf_sdbrcm_htclk(bus, true, pendok);
		brcmf_sdbrcm_wd_timer(bus, BRCMF_WD_POLL_MS);
		bus->activity = true;
		break;

	case CLK_SDONLY:
		
		if (bus->clkstate == CLK_NONE)
			brcmf_sdbrcm_sdclk(bus, true);
		else if (bus->clkstate == CLK_AVAIL)
			brcmf_sdbrcm_htclk(bus, false, false);
		else
			brcmf_dbg(ERROR, "request for %d -> %d\n",
				  bus->clkstate, target);
		brcmf_sdbrcm_wd_timer(bus, BRCMF_WD_POLL_MS);
		break;

	case CLK_NONE:
		
		if (bus->clkstate == CLK_AVAIL)
			brcmf_sdbrcm_htclk(bus, false, false);
		
		brcmf_sdbrcm_sdclk(bus, false);
		brcmf_sdbrcm_wd_timer(bus, 0);
		break;
	}
#ifdef DEBUG
	brcmf_dbg(INFO, "%d -> %d\n", oldstate, bus->clkstate);
#endif				

	return 0;
}

static int brcmf_sdbrcm_bussleep(struct brcmf_sdio *bus, bool sleep)
{
	uint retries = 0;

	brcmf_dbg(INFO, "request %s (currently %s)\n",
		  sleep ? "SLEEP" : "WAKE",
		  bus->sleeping ? "SLEEP" : "WAKE");

	
	if (sleep == bus->sleeping)
		return 0;

	
	if (sleep) {
		
		if (bus->dpc_sched || bus->rxskip || pktq_len(&bus->txq))
			return -EBUSY;

		
		brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);

		
		w_sdreg32(bus, SMB_USE_OOB,
			  offsetof(struct sdpcmd_regs, tosbmailbox), &retries);
		if (retries > retry_limit)
			brcmf_dbg(ERROR, "CANNOT SIGNAL CHIP, WILL NOT WAKE UP!!\n");

		
		brcmf_sdbrcm_clkctl(bus, CLK_SDONLY, false);

		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			SBSDIO_FUNC1_CHIPCLKCSR,
			SBSDIO_FORCE_HW_CLKREQ_OFF, NULL);

		
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			SBSDIO_DEVICE_CTL,
			SBSDIO_DEVCTL_PADS_ISO, NULL);

		
		bus->sleeping = true;

	} else {
		

		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			SBSDIO_FUNC1_CHIPCLKCSR, 0, NULL);

		
		brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);

		
		w_sdreg32(bus, 0, offsetof(struct sdpcmd_regs, tosbmailboxdata),
			  &retries);
		if (retries <= retry_limit)
			w_sdreg32(bus, SMB_DEV_INT,
				  offsetof(struct sdpcmd_regs, tosbmailbox),
				  &retries);

		if (retries > retry_limit)
			brcmf_dbg(ERROR, "CANNOT SIGNAL CHIP TO CLEAR OOB!!\n");

		
		brcmf_sdbrcm_clkctl(bus, CLK_SDONLY, false);

		
		bus->sleeping = false;
	}

	return 0;
}

static void bus_wake(struct brcmf_sdio *bus)
{
	if (bus->sleeping)
		brcmf_sdbrcm_bussleep(bus, false);
}

static u32 brcmf_sdbrcm_hostmail(struct brcmf_sdio *bus)
{
	u32 intstatus = 0;
	u32 hmb_data;
	u8 fcbits;
	uint retries = 0;

	brcmf_dbg(TRACE, "Enter\n");

	
	r_sdreg32(bus, &hmb_data,
		  offsetof(struct sdpcmd_regs, tohostmailboxdata), &retries);

	if (retries <= retry_limit)
		w_sdreg32(bus, SMB_INT_ACK,
			  offsetof(struct sdpcmd_regs, tosbmailbox), &retries);
	bus->f1regdata += 2;

	
	if (hmb_data & HMB_DATA_NAKHANDLED) {
		brcmf_dbg(INFO, "Dongle reports NAK handled, expect rtx of %d\n",
			  bus->rx_seq);
		if (!bus->rxskip)
			brcmf_dbg(ERROR, "unexpected NAKHANDLED!\n");

		bus->rxskip = false;
		intstatus |= I_HMB_FRAME_IND;
	}

	if (hmb_data & (HMB_DATA_DEVREADY | HMB_DATA_FWREADY)) {
		bus->sdpcm_ver =
		    (hmb_data & HMB_DATA_VERSION_MASK) >>
		    HMB_DATA_VERSION_SHIFT;
		if (bus->sdpcm_ver != SDPCM_PROT_VERSION)
			brcmf_dbg(ERROR, "Version mismatch, dongle reports %d, "
				  "expecting %d\n",
				  bus->sdpcm_ver, SDPCM_PROT_VERSION);
		else
			brcmf_dbg(INFO, "Dongle ready, protocol version %d\n",
				  bus->sdpcm_ver);
	}

	if (hmb_data & HMB_DATA_FC) {
		fcbits = (hmb_data & HMB_DATA_FCDATA_MASK) >>
							HMB_DATA_FCDATA_SHIFT;

		if (fcbits & ~bus->flowcontrol)
			bus->fc_xoff++;

		if (bus->flowcontrol & ~fcbits)
			bus->fc_xon++;

		bus->fc_rcvd++;
		bus->flowcontrol = fcbits;
	}

	
	if (hmb_data & ~(HMB_DATA_DEVREADY |
			 HMB_DATA_NAKHANDLED |
			 HMB_DATA_FC |
			 HMB_DATA_FWREADY |
			 HMB_DATA_FCDATA_MASK | HMB_DATA_VERSION_MASK))
		brcmf_dbg(ERROR, "Unknown mailbox data content: 0x%02x\n",
			  hmb_data);

	return intstatus;
}

static void brcmf_sdbrcm_rxfail(struct brcmf_sdio *bus, bool abort, bool rtx)
{
	uint retries = 0;
	u16 lastrbc;
	u8 hi, lo;
	int err;

	brcmf_dbg(ERROR, "%sterminate frame%s\n",
		  abort ? "abort command, " : "",
		  rtx ? ", send NAK" : "");

	if (abort)
		brcmf_sdcard_abort(bus->sdiodev, SDIO_FUNC_2);

	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			       SBSDIO_FUNC1_FRAMECTRL,
			       SFC_RF_TERM, &err);
	bus->f1regdata++;

	
	for (lastrbc = retries = 0xffff; retries > 0; retries--) {
		hi = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					   SBSDIO_FUNC1_RFRAMEBCHI, NULL);
		lo = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					   SBSDIO_FUNC1_RFRAMEBCLO, NULL);
		bus->f1regdata += 2;

		if ((hi == 0) && (lo == 0))
			break;

		if ((hi > (lastrbc >> 8)) && (lo > (lastrbc & 0x00ff))) {
			brcmf_dbg(ERROR, "count growing: last 0x%04x now 0x%04x\n",
				  lastrbc, (hi << 8) + lo);
		}
		lastrbc = (hi << 8) + lo;
	}

	if (!retries)
		brcmf_dbg(ERROR, "count never zeroed: last 0x%04x\n", lastrbc);
	else
		brcmf_dbg(INFO, "flush took %d iterations\n", 0xffff - retries);

	if (rtx) {
		bus->rxrtx++;
		w_sdreg32(bus, SMB_NAK,
			  offsetof(struct sdpcmd_regs, tosbmailbox), &retries);

		bus->f1regdata++;
		if (retries <= retry_limit)
			bus->rxskip = true;
	}

	
	bus->nextlen = 0;

	
	if (err || brcmf_sdcard_regfail(bus->sdiodev))
		bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
}

static uint brcmf_sdbrcm_glom_from_buf(struct brcmf_sdio *bus, uint len)
{
	uint n, ret = 0;
	struct sk_buff *p;
	u8 *buf;

	buf = bus->dataptr;

	
	skb_queue_walk(&bus->glom, p) {
		n = min_t(uint, p->len, len);
		memcpy(p->data, buf, n);
		buf += n;
		len -= n;
		ret += n;
		if (!len)
			break;
	}

	return ret;
}

static uint brcmf_sdbrcm_glom_len(struct brcmf_sdio *bus)
{
	struct sk_buff *p;
	uint total;

	total = 0;
	skb_queue_walk(&bus->glom, p)
		total += p->len;
	return total;
}

static void brcmf_sdbrcm_free_glom(struct brcmf_sdio *bus)
{
	struct sk_buff *cur, *next;

	skb_queue_walk_safe(&bus->glom, cur, next) {
		skb_unlink(cur, &bus->glom);
		brcmu_pkt_buf_free_skb(cur);
	}
}

static u8 brcmf_sdbrcm_rxglom(struct brcmf_sdio *bus, u8 rxseq)
{
	u16 dlen, totlen;
	u8 *dptr, num = 0;

	u16 sublen, check;
	struct sk_buff *pfirst, *pnext;

	int errcode;
	u8 chan, seq, doff, sfdoff;
	u8 txmax;

	int ifidx = 0;
	bool usechain = bus->use_rxchain;

	
	

	brcmf_dbg(TRACE, "start: glomd %p glom %p\n",
		  bus->glomd, skb_peek(&bus->glom));

	
	if (bus->glomd) {
		pfirst = pnext = NULL;
		dlen = (u16) (bus->glomd->len);
		dptr = bus->glomd->data;
		if (!dlen || (dlen & 1)) {
			brcmf_dbg(ERROR, "bad glomd len(%d), ignore descriptor\n",
				  dlen);
			dlen = 0;
		}

		for (totlen = num = 0; dlen; num++) {
			
			sublen = get_unaligned_le16(dptr);
			dlen -= sizeof(u16);
			dptr += sizeof(u16);
			if ((sublen < SDPCM_HDRLEN) ||
			    ((num == 0) && (sublen < (2 * SDPCM_HDRLEN)))) {
				brcmf_dbg(ERROR, "descriptor len %d bad: %d\n",
					  num, sublen);
				pnext = NULL;
				break;
			}
			if (sublen % BRCMF_SDALIGN) {
				brcmf_dbg(ERROR, "sublen %d not multiple of %d\n",
					  sublen, BRCMF_SDALIGN);
				usechain = false;
			}
			totlen += sublen;

			if (!dlen) {
				sublen +=
				    (roundup(totlen, bus->blocksize) - totlen);
				totlen = roundup(totlen, bus->blocksize);
			}

			
			pnext = brcmu_pkt_buf_get_skb(sublen + BRCMF_SDALIGN);
			if (pnext == NULL) {
				brcmf_dbg(ERROR, "bcm_pkt_buf_get_skb failed, num %d len %d\n",
					  num, sublen);
				break;
			}
			skb_queue_tail(&bus->glom, pnext);

			
			pkt_align(pnext, sublen, BRCMF_SDALIGN);
		}

		if (pnext) {
			brcmf_dbg(GLOM, "allocated %d-byte packet chain for %d subframes\n",
				  totlen, num);
			if (BRCMF_GLOM_ON() && bus->nextlen &&
			    totlen != bus->nextlen) {
				brcmf_dbg(GLOM, "glomdesc mismatch: nextlen %d glomdesc %d rxseq %d\n",
					  bus->nextlen, totlen, rxseq);
			}
			pfirst = pnext = NULL;
		} else {
			brcmf_sdbrcm_free_glom(bus);
			num = 0;
		}

		
		brcmu_pkt_buf_free_skb(bus->glomd);
		bus->glomd = NULL;
		bus->nextlen = 0;
	}

	if (!skb_queue_empty(&bus->glom)) {
		if (BRCMF_GLOM_ON()) {
			brcmf_dbg(GLOM, "try superframe read, packet chain:\n");
			skb_queue_walk(&bus->glom, pnext) {
				brcmf_dbg(GLOM, "    %p: %p len 0x%04x (%d)\n",
					  pnext, (u8 *) (pnext->data),
					  pnext->len, pnext->len);
			}
		}

		pfirst = skb_peek(&bus->glom);
		dlen = (u16) brcmf_sdbrcm_glom_len(bus);

		if (usechain) {
			errcode = brcmf_sdcard_recv_chain(bus->sdiodev,
					bus->sdiodev->sbwad,
					SDIO_FUNC_2, F2SYNC, &bus->glom);
		} else if (bus->dataptr) {
			errcode = brcmf_sdcard_recv_buf(bus->sdiodev,
					bus->sdiodev->sbwad,
					SDIO_FUNC_2, F2SYNC,
					bus->dataptr, dlen);
			sublen = (u16) brcmf_sdbrcm_glom_from_buf(bus, dlen);
			if (sublen != dlen) {
				brcmf_dbg(ERROR, "FAILED TO COPY, dlen %d sublen %d\n",
					  dlen, sublen);
				errcode = -1;
			}
			pnext = NULL;
		} else {
			brcmf_dbg(ERROR, "COULDN'T ALLOC %d-BYTE GLOM, FORCE FAILURE\n",
				  dlen);
			errcode = -1;
		}
		bus->f2rxdata++;

		
		if (errcode < 0) {
			brcmf_dbg(ERROR, "glom read of %d bytes failed: %d\n",
				  dlen, errcode);
			bus->sdiodev->bus_if->dstats.rx_errors++;

			if (bus->glomerr++ < 3) {
				brcmf_sdbrcm_rxfail(bus, true, true);
			} else {
				bus->glomerr = 0;
				brcmf_sdbrcm_rxfail(bus, true, false);
				bus->rxglomfail++;
				brcmf_sdbrcm_free_glom(bus);
			}
			return 0;
		}

		brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
				   pfirst->data, min_t(int, pfirst->len, 48),
				   "SUPERFRAME:\n");

		
		dptr = (u8 *) (pfirst->data);
		sublen = get_unaligned_le16(dptr);
		check = get_unaligned_le16(dptr + sizeof(u16));

		chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
		seq = SDPCM_PACKET_SEQUENCE(&dptr[SDPCM_FRAMETAG_LEN]);
		bus->nextlen = dptr[SDPCM_FRAMETAG_LEN + SDPCM_NEXTLEN_OFFSET];
		if ((bus->nextlen << 4) > MAX_RX_DATASZ) {
			brcmf_dbg(INFO, "nextlen too large (%d) seq %d\n",
				  bus->nextlen, seq);
			bus->nextlen = 0;
		}
		doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);
		txmax = SDPCM_WINDOW_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);

		errcode = 0;
		if ((u16)~(sublen ^ check)) {
			brcmf_dbg(ERROR, "(superframe): HW hdr error: len/check 0x%04x/0x%04x\n",
				  sublen, check);
			errcode = -1;
		} else if (roundup(sublen, bus->blocksize) != dlen) {
			brcmf_dbg(ERROR, "(superframe): len 0x%04x, rounded 0x%04x, expect 0x%04x\n",
				  sublen, roundup(sublen, bus->blocksize),
				  dlen);
			errcode = -1;
		} else if (SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]) !=
			   SDPCM_GLOM_CHANNEL) {
			brcmf_dbg(ERROR, "(superframe): bad channel %d\n",
				  SDPCM_PACKET_CHANNEL(
					  &dptr[SDPCM_FRAMETAG_LEN]));
			errcode = -1;
		} else if (SDPCM_GLOMDESC(&dptr[SDPCM_FRAMETAG_LEN])) {
			brcmf_dbg(ERROR, "(superframe): got 2nd descriptor?\n");
			errcode = -1;
		} else if ((doff < SDPCM_HDRLEN) ||
			   (doff > (pfirst->len - SDPCM_HDRLEN))) {
			brcmf_dbg(ERROR, "(superframe): Bad data offset %d: HW %d pkt %d min %d\n",
				  doff, sublen, pfirst->len, SDPCM_HDRLEN);
			errcode = -1;
		}

		
		if (rxseq != seq) {
			brcmf_dbg(INFO, "(superframe) rx_seq %d, expected %d\n",
				  seq, rxseq);
			bus->rx_badseq++;
			rxseq = seq;
		}

		
		if ((u8) (txmax - bus->tx_seq) > 0x40) {
			brcmf_dbg(ERROR, "unlikely tx max %d with tx_seq %d\n",
				  txmax, bus->tx_seq);
			txmax = bus->tx_seq + 2;
		}
		bus->tx_max = txmax;

		
		skb_pull(pfirst, doff);
		sfdoff = doff;
		num = 0;

		
		skb_queue_walk(&bus->glom, pnext) {
			
			if (errcode)
				break;

			dptr = (u8 *) (pnext->data);
			dlen = (u16) (pnext->len);
			sublen = get_unaligned_le16(dptr);
			check = get_unaligned_le16(dptr + sizeof(u16));
			chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);
			brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
					   dptr, 32, "subframe:\n");

			if ((u16)~(sublen ^ check)) {
				brcmf_dbg(ERROR, "(subframe %d): HW hdr error: len/check 0x%04x/0x%04x\n",
					  num, sublen, check);
				errcode = -1;
			} else if ((sublen > dlen) || (sublen < SDPCM_HDRLEN)) {
				brcmf_dbg(ERROR, "(subframe %d): length mismatch: len 0x%04x, expect 0x%04x\n",
					  num, sublen, dlen);
				errcode = -1;
			} else if ((chan != SDPCM_DATA_CHANNEL) &&
				   (chan != SDPCM_EVENT_CHANNEL)) {
				brcmf_dbg(ERROR, "(subframe %d): bad channel %d\n",
					  num, chan);
				errcode = -1;
			} else if ((doff < SDPCM_HDRLEN) || (doff > sublen)) {
				brcmf_dbg(ERROR, "(subframe %d): Bad data offset %d: HW %d min %d\n",
					  num, doff, sublen, SDPCM_HDRLEN);
				errcode = -1;
			}
			
			num++;
		}

		if (errcode) {
			if (bus->glomerr++ < 3) {
				
				skb_push(pfirst, sfdoff);
				brcmf_sdbrcm_rxfail(bus, true, true);
			} else {
				bus->glomerr = 0;
				brcmf_sdbrcm_rxfail(bus, true, false);
				bus->rxglomfail++;
				brcmf_sdbrcm_free_glom(bus);
			}
			bus->nextlen = 0;
			return 0;
		}

		

		skb_queue_walk_safe(&bus->glom, pfirst, pnext) {
			dptr = (u8 *) (pfirst->data);
			sublen = get_unaligned_le16(dptr);
			chan = SDPCM_PACKET_CHANNEL(&dptr[SDPCM_FRAMETAG_LEN]);
			seq = SDPCM_PACKET_SEQUENCE(&dptr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(&dptr[SDPCM_FRAMETAG_LEN]);

			brcmf_dbg(GLOM, "Get subframe %d, %p(%p/%d), sublen %d chan %d seq %d\n",
				  num, pfirst, pfirst->data,
				  pfirst->len, sublen, chan, seq);


			if (rxseq != seq) {
				brcmf_dbg(GLOM, "rx_seq %d, expected %d\n",
					  seq, rxseq);
				bus->rx_badseq++;
				rxseq = seq;
			}
			rxseq++;

			brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_DATA_ON(),
					   dptr, dlen, "Rx Subframe Data:\n");

			__skb_trim(pfirst, sublen);
			skb_pull(pfirst, doff);

			if (pfirst->len == 0) {
				skb_unlink(pfirst, &bus->glom);
				brcmu_pkt_buf_free_skb(pfirst);
				continue;
			} else if (brcmf_proto_hdrpull(bus->sdiodev->dev,
						       &ifidx, pfirst) != 0) {
				brcmf_dbg(ERROR, "rx protocol error\n");
				bus->sdiodev->bus_if->dstats.rx_errors++;
				skb_unlink(pfirst, &bus->glom);
				brcmu_pkt_buf_free_skb(pfirst);
				continue;
			}

			brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
					   pfirst->data,
					   min_t(int, pfirst->len, 32),
					   "subframe %d to stack, %p (%p/%d) nxt/lnk %p/%p\n",
					   bus->glom.qlen, pfirst, pfirst->data,
					   pfirst->len, pfirst->next,
					   pfirst->prev);
		}
		
		if (bus->glom.qlen) {
			up(&bus->sdsem);
			brcmf_rx_frame(bus->sdiodev->dev, ifidx, &bus->glom);
			down(&bus->sdsem);
		}

		bus->rxglomframes++;
		bus->rxglompkts += bus->glom.qlen;
	}
	return num;
}

static int brcmf_sdbrcm_dcmd_resp_wait(struct brcmf_sdio *bus, uint *condition,
					bool *pending)
{
	DECLARE_WAITQUEUE(wait, current);
	int timeout = msecs_to_jiffies(DCMD_RESP_TIMEOUT);

	
	add_wait_queue(&bus->dcmd_resp_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	while (!(*condition) && (!signal_pending(current) && timeout))
		timeout = schedule_timeout(timeout);

	if (signal_pending(current))
		*pending = true;

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&bus->dcmd_resp_wait, &wait);

	return timeout;
}

static int brcmf_sdbrcm_dcmd_resp_wake(struct brcmf_sdio *bus)
{
	if (waitqueue_active(&bus->dcmd_resp_wait))
		wake_up_interruptible(&bus->dcmd_resp_wait);

	return 0;
}
static void
brcmf_sdbrcm_read_control(struct brcmf_sdio *bus, u8 *hdr, uint len, uint doff)
{
	uint rdlen, pad;

	int sdret;

	brcmf_dbg(TRACE, "Enter\n");

	
	bus->rxctl = bus->rxbuf;
	bus->rxctl += BRCMF_FIRSTREAD;
	pad = ((unsigned long)bus->rxctl % BRCMF_SDALIGN);
	if (pad)
		bus->rxctl += (BRCMF_SDALIGN - pad);
	bus->rxctl -= BRCMF_FIRSTREAD;

	
	memcpy(bus->rxctl, hdr, BRCMF_FIRSTREAD);
	if (len <= BRCMF_FIRSTREAD)
		goto gotpkt;

	
	rdlen = len - BRCMF_FIRSTREAD;
	if (bus->roundup && bus->blocksize && (rdlen > bus->blocksize)) {
		pad = bus->blocksize - (rdlen % bus->blocksize);
		if ((pad <= bus->roundup) && (pad < bus->blocksize) &&
		    ((len + pad) < bus->sdiodev->bus_if->maxctl))
			rdlen += pad;
	} else if (rdlen % BRCMF_SDALIGN) {
		rdlen += BRCMF_SDALIGN - (rdlen % BRCMF_SDALIGN);
	}

	
	if (rdlen & (ALIGNMENT - 1))
		rdlen = roundup(rdlen, ALIGNMENT);

	
	if ((rdlen + BRCMF_FIRSTREAD) > bus->sdiodev->bus_if->maxctl) {
		brcmf_dbg(ERROR, "%d-byte control read exceeds %d-byte buffer\n",
			  rdlen, bus->sdiodev->bus_if->maxctl);
		bus->sdiodev->bus_if->dstats.rx_errors++;
		brcmf_sdbrcm_rxfail(bus, false, false);
		goto done;
	}

	if ((len - doff) > bus->sdiodev->bus_if->maxctl) {
		brcmf_dbg(ERROR, "%d-byte ctl frame (%d-byte ctl data) exceeds %d-byte limit\n",
			  len, len - doff, bus->sdiodev->bus_if->maxctl);
		bus->sdiodev->bus_if->dstats.rx_errors++;
		bus->rx_toolong++;
		brcmf_sdbrcm_rxfail(bus, false, false);
		goto done;
	}

	
	sdret = brcmf_sdcard_recv_buf(bus->sdiodev,
				bus->sdiodev->sbwad,
				SDIO_FUNC_2,
				F2SYNC, (bus->rxctl + BRCMF_FIRSTREAD), rdlen);
	bus->f2rxdata++;

	
	if (sdret < 0) {
		brcmf_dbg(ERROR, "read %d control bytes failed: %d\n",
			  rdlen, sdret);
		bus->rxc_errors++;
		brcmf_sdbrcm_rxfail(bus, true, true);
		goto done;
	}

gotpkt:

	brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_CTL_ON(),
			   bus->rxctl, len, "RxCtrl:\n");

	
	bus->rxctl += doff;
	bus->rxlen = len - doff;

done:
	
	brcmf_sdbrcm_dcmd_resp_wake(bus);
}

static void brcmf_pad(struct brcmf_sdio *bus, u16 *pad, u16 *rdlen)
{
	if (bus->roundup && bus->blocksize && *rdlen > bus->blocksize) {
		*pad = bus->blocksize - (*rdlen % bus->blocksize);
		if (*pad <= bus->roundup && *pad < bus->blocksize &&
		    *rdlen + *pad + BRCMF_FIRSTREAD < MAX_RX_DATASZ)
			*rdlen += *pad;
	} else if (*rdlen % BRCMF_SDALIGN) {
		*rdlen += BRCMF_SDALIGN - (*rdlen % BRCMF_SDALIGN);
	}
}

static void
brcmf_alloc_pkt_and_read(struct brcmf_sdio *bus, u16 rdlen,
			 struct sk_buff **pkt, u8 **rxbuf)
{
	int sdret;		

	*pkt = brcmu_pkt_buf_get_skb(rdlen + BRCMF_SDALIGN);
	if (*pkt == NULL)
		return;

	pkt_align(*pkt, rdlen, BRCMF_SDALIGN);
	*rxbuf = (u8 *) ((*pkt)->data);
	
	sdret = brcmf_sdcard_recv_pkt(bus->sdiodev, bus->sdiodev->sbwad,
				      SDIO_FUNC_2, F2SYNC, *pkt);
	bus->f2rxdata++;

	if (sdret < 0) {
		brcmf_dbg(ERROR, "(nextlen): read %d bytes failed: %d\n",
			  rdlen, sdret);
		brcmu_pkt_buf_free_skb(*pkt);
		bus->sdiodev->bus_if->dstats.rx_errors++;
		brcmf_sdbrcm_rxfail(bus, true, true);
		*pkt = NULL;
	}
}

static int
brcmf_check_rxbuf(struct brcmf_sdio *bus, struct sk_buff *pkt, u8 *rxbuf,
		  u8 rxseq, u16 nextlen, u16 *len)
{
	u16 check;
	bool len_consistent;	

	memcpy(bus->rxhdr, rxbuf, SDPCM_HDRLEN);

	
	*len = get_unaligned_le16(bus->rxhdr);
	check = get_unaligned_le16(bus->rxhdr + sizeof(u16));

	
	if (!(*len | check)) {
		brcmf_dbg(INFO, "(nextlen): read zeros in HW header???\n");
		goto fail;
	}

	
	if ((u16)~(*len ^ check)) {
		brcmf_dbg(ERROR, "(nextlen): HW hdr error: nextlen/len/check 0x%04x/0x%04x/0x%04x\n",
			  nextlen, *len, check);
		bus->rx_badhdr++;
		brcmf_sdbrcm_rxfail(bus, false, false);
		goto fail;
	}

	
	if (*len < SDPCM_HDRLEN) {
		brcmf_dbg(ERROR, "(nextlen): HW hdr length invalid: %d\n",
			  *len);
		goto fail;
	}

	
	len_consistent = (nextlen != (roundup(*len, 16) >> 4));
	if (len_consistent) {
		brcmf_dbg(ERROR, "(nextlen): mismatch, nextlen %d len %d rnd %d; expected rxseq %d\n",
			  nextlen, *len, roundup(*len, 16),
			  rxseq);
		brcmf_sdbrcm_rxfail(bus, true, true);
		goto fail;
	}

	return 0;

fail:
	brcmf_sdbrcm_pktfree2(bus, pkt);
	return -EINVAL;
}

static uint
brcmf_sdbrcm_readframes(struct brcmf_sdio *bus, uint maxframes, bool *finished)
{
	u16 len, check;	
	u8 chan, seq, doff;	
	u8 fcbits;		

	struct sk_buff *pkt;		
	u16 pad;		
	u16 rdlen;		
	u8 rxseq;		
	uint rxleft = 0;	
	int sdret;		
	u8 txmax;		
	u8 *rxbuf;
	int ifidx = 0;
	uint rxcount = 0;	

	brcmf_dbg(TRACE, "Enter\n");

	
	*finished = false;

	for (rxseq = bus->rx_seq, rxleft = maxframes;
	     !bus->rxskip && rxleft &&
	     bus->sdiodev->bus_if->state != BRCMF_BUS_DOWN;
	     rxseq++, rxleft--) {

		
		if (bus->glomd || !skb_queue_empty(&bus->glom)) {
			u8 cnt;
			brcmf_dbg(GLOM, "calling rxglom: glomd %p, glom %p\n",
				  bus->glomd, skb_peek(&bus->glom));
			cnt = brcmf_sdbrcm_rxglom(bus, rxseq);
			brcmf_dbg(GLOM, "rxglom returned %d\n", cnt);
			rxseq += cnt - 1;
			rxleft = (rxleft > cnt) ? (rxleft - cnt) : 1;
			continue;
		}

		
		if (bus->nextlen) {
			u16 nextlen = bus->nextlen;
			bus->nextlen = 0;

			rdlen = len = nextlen << 4;
			brcmf_pad(bus, &pad, &rdlen);

			brcmf_alloc_pkt_and_read(bus, rdlen, &pkt, &rxbuf);
			if (pkt == NULL) {
				
				brcmf_dbg(ERROR, "(nextlen): brcmf_alloc_pkt_and_read failed: len %d rdlen %d expected rxseq %d\n",
					  len, rdlen, rxseq);
				continue;
			}

			if (brcmf_check_rxbuf(bus, pkt, rxbuf, rxseq, nextlen,
					      &len) < 0)
				continue;

			
			chan = SDPCM_PACKET_CHANNEL(
					&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			seq = SDPCM_PACKET_SEQUENCE(
					&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			doff = SDPCM_DOFFSET_VALUE(
					&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
			txmax = SDPCM_WINDOW_VALUE(
					&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

			bus->nextlen =
			    bus->rxhdr[SDPCM_FRAMETAG_LEN +
				       SDPCM_NEXTLEN_OFFSET];
			if ((bus->nextlen << 4) > MAX_RX_DATASZ) {
				brcmf_dbg(INFO, "(nextlen): got frame w/nextlen too large (%d), seq %d\n",
					  bus->nextlen, seq);
				bus->nextlen = 0;
			}

			bus->rx_readahead_cnt++;

			
			fcbits = SDPCM_FCMASK_VALUE(
					&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

			if (bus->flowcontrol != fcbits) {
				if (~bus->flowcontrol & fcbits)
					bus->fc_xoff++;

				if (bus->flowcontrol & ~fcbits)
					bus->fc_xon++;

				bus->fc_rcvd++;
				bus->flowcontrol = fcbits;
			}

			
			if (rxseq != seq) {
				brcmf_dbg(INFO, "(nextlen): rx_seq %d, expected %d\n",
					  seq, rxseq);
				bus->rx_badseq++;
				rxseq = seq;
			}

			
			if ((u8) (txmax - bus->tx_seq) > 0x40) {
				brcmf_dbg(ERROR, "got unlikely tx max %d with tx_seq %d\n",
					  txmax, bus->tx_seq);
				txmax = bus->tx_seq + 2;
			}
			bus->tx_max = txmax;

			brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_DATA_ON(),
					   rxbuf, len, "Rx Data:\n");
			brcmf_dbg_hex_dump(!(BRCMF_BYTES_ON() &&
					     BRCMF_DATA_ON()) &&
					   BRCMF_HDRS_ON(),
					   bus->rxhdr, SDPCM_HDRLEN,
					   "RxHdr:\n");

			if (chan == SDPCM_CONTROL_CHANNEL) {
				brcmf_dbg(ERROR, "(nextlen): readahead on control packet %d?\n",
					  seq);
				
				bus->nextlen = 0;
				brcmf_sdbrcm_rxfail(bus, false, true);
				brcmf_sdbrcm_pktfree2(bus, pkt);
				continue;
			}

			
			if ((doff < SDPCM_HDRLEN) || (doff > len)) {
				brcmf_dbg(ERROR, "(nextlen): bad data offset %d: HW len %d min %d\n",
					  doff, len, SDPCM_HDRLEN);
				brcmf_sdbrcm_rxfail(bus, false, false);
				brcmf_sdbrcm_pktfree2(bus, pkt);
				continue;
			}

			
			goto deliver;
		}

		
		sdret = brcmf_sdcard_recv_buf(bus->sdiodev, bus->sdiodev->sbwad,
					      SDIO_FUNC_2, F2SYNC, bus->rxhdr,
					      BRCMF_FIRSTREAD);
		bus->f2rxhdrs++;

		if (sdret < 0) {
			brcmf_dbg(ERROR, "RXHEADER FAILED: %d\n", sdret);
			bus->rx_hdrfail++;
			brcmf_sdbrcm_rxfail(bus, true, true);
			continue;
		}
		brcmf_dbg_hex_dump(BRCMF_BYTES_ON() || BRCMF_HDRS_ON(),
				   bus->rxhdr, SDPCM_HDRLEN, "RxHdr:\n");


		
		len = get_unaligned_le16(bus->rxhdr);
		check = get_unaligned_le16(bus->rxhdr + sizeof(u16));

		
		if (!(len | check)) {
			*finished = true;
			break;
		}

		
		if ((u16) ~(len ^ check)) {
			brcmf_dbg(ERROR, "HW hdr err: len/check 0x%04x/0x%04x\n",
				  len, check);
			bus->rx_badhdr++;
			brcmf_sdbrcm_rxfail(bus, false, false);
			continue;
		}

		
		if (len < SDPCM_HDRLEN) {
			brcmf_dbg(ERROR, "HW hdr length invalid: %d\n", len);
			continue;
		}

		
		chan = SDPCM_PACKET_CHANNEL(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		seq = SDPCM_PACKET_SEQUENCE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		doff = SDPCM_DOFFSET_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);
		txmax = SDPCM_WINDOW_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

		
		if ((doff < SDPCM_HDRLEN) || (doff > len)) {
			brcmf_dbg(ERROR, "Bad data offset %d: HW len %d, min %d seq %d\n",
				  doff, len, SDPCM_HDRLEN, seq);
			bus->rx_badhdr++;
			brcmf_sdbrcm_rxfail(bus, false, false);
			continue;
		}

		
		bus->nextlen =
		    bus->rxhdr[SDPCM_FRAMETAG_LEN + SDPCM_NEXTLEN_OFFSET];
		if ((bus->nextlen << 4) > MAX_RX_DATASZ) {
			brcmf_dbg(INFO, "(nextlen): got frame w/nextlen too large (%d), seq %d\n",
				  bus->nextlen, seq);
			bus->nextlen = 0;
		}

		
		fcbits = SDPCM_FCMASK_VALUE(&bus->rxhdr[SDPCM_FRAMETAG_LEN]);

		if (bus->flowcontrol != fcbits) {
			if (~bus->flowcontrol & fcbits)
				bus->fc_xoff++;

			if (bus->flowcontrol & ~fcbits)
				bus->fc_xon++;

			bus->fc_rcvd++;
			bus->flowcontrol = fcbits;
		}

		
		if (rxseq != seq) {
			brcmf_dbg(INFO, "rx_seq %d, expected %d\n", seq, rxseq);
			bus->rx_badseq++;
			rxseq = seq;
		}

		
		if ((u8) (txmax - bus->tx_seq) > 0x40) {
			brcmf_dbg(ERROR, "unlikely tx max %d with tx_seq %d\n",
				  txmax, bus->tx_seq);
			txmax = bus->tx_seq + 2;
		}
		bus->tx_max = txmax;

		
		if (chan == SDPCM_CONTROL_CHANNEL) {
			brcmf_sdbrcm_read_control(bus, bus->rxhdr, len, doff);
			continue;
		}


		
		rdlen = (len > BRCMF_FIRSTREAD) ? (len - BRCMF_FIRSTREAD) : 0;

		
		if (bus->roundup && bus->blocksize &&
			(rdlen > bus->blocksize)) {
			pad = bus->blocksize - (rdlen % bus->blocksize);
			if ((pad <= bus->roundup) && (pad < bus->blocksize) &&
			    ((rdlen + pad + BRCMF_FIRSTREAD) < MAX_RX_DATASZ))
				rdlen += pad;
		} else if (rdlen % BRCMF_SDALIGN) {
			rdlen += BRCMF_SDALIGN - (rdlen % BRCMF_SDALIGN);
		}

		
		if (rdlen & (ALIGNMENT - 1))
			rdlen = roundup(rdlen, ALIGNMENT);

		if ((rdlen + BRCMF_FIRSTREAD) > MAX_RX_DATASZ) {
			
			brcmf_dbg(ERROR, "too long: len %d rdlen %d\n",
				  len, rdlen);
			bus->sdiodev->bus_if->dstats.rx_errors++;
			bus->rx_toolong++;
			brcmf_sdbrcm_rxfail(bus, false, false);
			continue;
		}

		pkt = brcmu_pkt_buf_get_skb(rdlen +
					    BRCMF_FIRSTREAD + BRCMF_SDALIGN);
		if (!pkt) {
			
			brcmf_dbg(ERROR, "brcmu_pkt_buf_get_skb failed: rdlen %d chan %d\n",
				  rdlen, chan);
			bus->sdiodev->bus_if->dstats.rx_dropped++;
			brcmf_sdbrcm_rxfail(bus, false, RETRYCHAN(chan));
			continue;
		}

		
		skb_pull(pkt, BRCMF_FIRSTREAD);
		pkt_align(pkt, rdlen, BRCMF_SDALIGN);

		
		sdret = brcmf_sdcard_recv_pkt(bus->sdiodev, bus->sdiodev->sbwad,
					      SDIO_FUNC_2, F2SYNC, pkt);
		bus->f2rxdata++;

		if (sdret < 0) {
			brcmf_dbg(ERROR, "read %d %s bytes failed: %d\n", rdlen,
				  ((chan == SDPCM_EVENT_CHANNEL) ? "event"
				   : ((chan == SDPCM_DATA_CHANNEL) ? "data"
				      : "test")), sdret);
			brcmu_pkt_buf_free_skb(pkt);
			bus->sdiodev->bus_if->dstats.rx_errors++;
			brcmf_sdbrcm_rxfail(bus, true, RETRYCHAN(chan));
			continue;
		}

		
		skb_push(pkt, BRCMF_FIRSTREAD);
		memcpy(pkt->data, bus->rxhdr, BRCMF_FIRSTREAD);

		brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_DATA_ON(),
				   pkt->data, len, "Rx Data:\n");

deliver:
		
		if (chan == SDPCM_GLOM_CHANNEL) {
			if (SDPCM_GLOMDESC(&bus->rxhdr[SDPCM_FRAMETAG_LEN])) {
				brcmf_dbg(GLOM, "glom descriptor, %d bytes:\n",
					  len);
				brcmf_dbg_hex_dump(BRCMF_GLOM_ON(),
						   pkt->data, len,
						   "Glom Data:\n");
				__skb_trim(pkt, len);
				skb_pull(pkt, SDPCM_HDRLEN);
				bus->glomd = pkt;
			} else {
				brcmf_dbg(ERROR, "%s: glom superframe w/o "
					  "descriptor!\n", __func__);
				brcmf_sdbrcm_rxfail(bus, false, false);
			}
			continue;
		}

		
		__skb_trim(pkt, len);
		skb_pull(pkt, doff);

		if (pkt->len == 0) {
			brcmu_pkt_buf_free_skb(pkt);
			continue;
		} else if (brcmf_proto_hdrpull(bus->sdiodev->dev, &ifidx,
			   pkt) != 0) {
			brcmf_dbg(ERROR, "rx protocol error\n");
			brcmu_pkt_buf_free_skb(pkt);
			bus->sdiodev->bus_if->dstats.rx_errors++;
			continue;
		}

		
		up(&bus->sdsem);
		brcmf_rx_packet(bus->sdiodev->dev, ifidx, pkt);
		down(&bus->sdsem);
	}
	rxcount = maxframes - rxleft;
	
	if (!rxleft)
		brcmf_dbg(DATA, "hit rx limit of %d frames\n",
			  maxframes);
	else
		brcmf_dbg(DATA, "processed %d frames\n", rxcount);
	
	if (bus->rxskip)
		rxseq--;
	bus->rx_seq = rxseq;

	return rxcount;
}

static void
brcmf_sdbrcm_wait_for_event(struct brcmf_sdio *bus, bool *lockvar)
{
	up(&bus->sdsem);
	wait_event_interruptible_timeout(bus->ctrl_wait, !*lockvar, HZ * 2);
	down(&bus->sdsem);
	return;
}

static void
brcmf_sdbrcm_wait_event_wakeup(struct brcmf_sdio *bus)
{
	if (waitqueue_active(&bus->ctrl_wait))
		wake_up_interruptible(&bus->ctrl_wait);
	return;
}

static int brcmf_sdbrcm_txpkt(struct brcmf_sdio *bus, struct sk_buff *pkt,
			      uint chan, bool free_pkt)
{
	int ret;
	u8 *frame;
	u16 len, pad = 0;
	u32 swheader;
	struct sk_buff *new;
	int i;

	brcmf_dbg(TRACE, "Enter\n");

	frame = (u8 *) (pkt->data);

	
	pad = ((unsigned long)frame % BRCMF_SDALIGN);
	if (pad) {
		if (skb_headroom(pkt) < pad) {
			brcmf_dbg(INFO, "insufficient headroom %d for %d pad\n",
				  skb_headroom(pkt), pad);
			bus->sdiodev->bus_if->tx_realloc++;
			new = brcmu_pkt_buf_get_skb(pkt->len + BRCMF_SDALIGN);
			if (!new) {
				brcmf_dbg(ERROR, "couldn't allocate new %d-byte packet\n",
					  pkt->len + BRCMF_SDALIGN);
				ret = -ENOMEM;
				goto done;
			}

			pkt_align(new, pkt->len, BRCMF_SDALIGN);
			memcpy(new->data, pkt->data, pkt->len);
			if (free_pkt)
				brcmu_pkt_buf_free_skb(pkt);
			
			free_pkt = true;
			pkt = new;
			frame = (u8 *) (pkt->data);
			
			pad = 0;
		} else {
			skb_push(pkt, pad);
			frame = (u8 *) (pkt->data);
			
			memset(frame, 0, pad + SDPCM_HDRLEN);
		}
	}
	

	
	len = (u16) (pkt->len);
	*(__le16 *) frame = cpu_to_le16(len);
	*(((__le16 *) frame) + 1) = cpu_to_le16(~len);

	
	swheader =
	    ((chan << SDPCM_CHANNEL_SHIFT) & SDPCM_CHANNEL_MASK) | bus->tx_seq |
	    (((pad +
	       SDPCM_HDRLEN) << SDPCM_DOFFSET_SHIFT) & SDPCM_DOFFSET_MASK);

	put_unaligned_le32(swheader, frame + SDPCM_FRAMETAG_LEN);
	put_unaligned_le32(0, frame + SDPCM_FRAMETAG_LEN + sizeof(swheader));

#ifdef DEBUG
	tx_packets[pkt->priority]++;
#endif

	brcmf_dbg_hex_dump(BRCMF_BYTES_ON() &&
			   ((BRCMF_CTL_ON() && chan == SDPCM_CONTROL_CHANNEL) ||
			    (BRCMF_DATA_ON() && chan != SDPCM_CONTROL_CHANNEL)),
			   frame, len, "Tx Frame:\n");
	brcmf_dbg_hex_dump(!(BRCMF_BYTES_ON() &&
			     ((BRCMF_CTL_ON() &&
			       chan == SDPCM_CONTROL_CHANNEL) ||
			      (BRCMF_DATA_ON() &&
			       chan != SDPCM_CONTROL_CHANNEL))) &&
			   BRCMF_HDRS_ON(),
			   frame, min_t(u16, len, 16), "TxHdr:\n");

	
	if (bus->roundup && bus->blocksize && (len > bus->blocksize)) {
		u16 pad = bus->blocksize - (len % bus->blocksize);
		if ((pad <= bus->roundup) && (pad < bus->blocksize))
				len += pad;
	} else if (len % BRCMF_SDALIGN) {
		len += BRCMF_SDALIGN - (len % BRCMF_SDALIGN);
	}

	
	if (len & (ALIGNMENT - 1))
			len = roundup(len, ALIGNMENT);

	ret = brcmf_sdcard_send_pkt(bus->sdiodev, bus->sdiodev->sbwad,
				    SDIO_FUNC_2, F2SYNC, pkt);
	bus->f2txdata++;

	if (ret < 0) {
		
		brcmf_dbg(INFO, "sdio error %d, abort command and terminate frame\n",
			  ret);
		bus->tx_sderrs++;

		brcmf_sdcard_abort(bus->sdiodev, SDIO_FUNC_2);
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				 SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM,
				 NULL);
		bus->f1regdata++;

		for (i = 0; i < 3; i++) {
			u8 hi, lo;
			hi = brcmf_sdcard_cfg_read(bus->sdiodev,
					     SDIO_FUNC_1,
					     SBSDIO_FUNC1_WFRAMEBCHI,
					     NULL);
			lo = brcmf_sdcard_cfg_read(bus->sdiodev,
					     SDIO_FUNC_1,
					     SBSDIO_FUNC1_WFRAMEBCLO,
					     NULL);
			bus->f1regdata += 2;
			if ((hi == 0) && (lo == 0))
				break;
		}

	}
	if (ret == 0)
		bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;

done:
	
	skb_pull(pkt, SDPCM_HDRLEN + pad);
	up(&bus->sdsem);
	brcmf_txcomplete(bus->sdiodev->dev, pkt, ret != 0);
	down(&bus->sdsem);

	if (free_pkt)
		brcmu_pkt_buf_free_skb(pkt);

	return ret;
}

static uint brcmf_sdbrcm_sendfromq(struct brcmf_sdio *bus, uint maxframes)
{
	struct sk_buff *pkt;
	u32 intstatus = 0;
	uint retries = 0;
	int ret = 0, prec_out;
	uint cnt = 0;
	uint datalen;
	u8 tx_prec_map;

	brcmf_dbg(TRACE, "Enter\n");

	tx_prec_map = ~bus->flowcontrol;

	
	for (cnt = 0; (cnt < maxframes) && data_ok(bus); cnt++) {
		spin_lock_bh(&bus->txqlock);
		pkt = brcmu_pktq_mdeq(&bus->txq, tx_prec_map, &prec_out);
		if (pkt == NULL) {
			spin_unlock_bh(&bus->txqlock);
			break;
		}
		spin_unlock_bh(&bus->txqlock);
		datalen = pkt->len - SDPCM_HDRLEN;

		ret = brcmf_sdbrcm_txpkt(bus, pkt, SDPCM_DATA_CHANNEL, true);
		if (ret)
			bus->sdiodev->bus_if->dstats.tx_errors++;
		else
			bus->sdiodev->bus_if->dstats.tx_bytes += datalen;

		
		if (!bus->intr && cnt) {
			
			r_sdreg32(bus, &intstatus,
				  offsetof(struct sdpcmd_regs, intstatus),
				  &retries);
			bus->f2txdata++;
			if (brcmf_sdcard_regfail(bus->sdiodev))
				break;
			if (intstatus & bus->hostintmask)
				bus->ipend = true;
		}
	}

	
	if (bus->sdiodev->bus_if->drvr_up &&
	    (bus->sdiodev->bus_if->state == BRCMF_BUS_DATA) &&
	    bus->txoff && (pktq_len(&bus->txq) < TXLOW)) {
		bus->txoff = OFF;
		brcmf_txflowcontrol(bus->sdiodev->dev, 0, OFF);
	}

	return cnt;
}

static void brcmf_sdbrcm_bus_stop(struct device *dev)
{
	u32 local_hostintmask;
	u8 saveclk;
	uint retries;
	int err;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter\n");

	if (bus->watchdog_tsk) {
		send_sig(SIGTERM, bus->watchdog_tsk, 1);
		kthread_stop(bus->watchdog_tsk);
		bus->watchdog_tsk = NULL;
	}

	if (bus->dpc_tsk && bus->dpc_tsk != current) {
		send_sig(SIGTERM, bus->dpc_tsk, 1);
		kthread_stop(bus->dpc_tsk);
		bus->dpc_tsk = NULL;
	}

	down(&bus->sdsem);

	bus_wake(bus);

	
	brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);

	
	w_sdreg32(bus, 0, offsetof(struct sdpcmd_regs, hostintmask), &retries);
	local_hostintmask = bus->hostintmask;
	bus->hostintmask = 0;

	
	bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;

	
	saveclk = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					SBSDIO_FUNC1_CHIPCLKCSR, &err);
	if (!err) {
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				       SBSDIO_FUNC1_CHIPCLKCSR,
				       (saveclk | SBSDIO_FORCE_HT), &err);
	}
	if (err)
		brcmf_dbg(ERROR, "Failed to force clock for F2: err %d\n", err);

	
	brcmf_dbg(INTR, "disable SDIO interrupts\n");
	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_0, SDIO_CCCR_IOEx,
			 SDIO_FUNC_ENABLE_1, NULL);

	
	w_sdreg32(bus, local_hostintmask,
		  offsetof(struct sdpcmd_regs, intstatus), &retries);

	
	brcmf_sdbrcm_clkctl(bus, CLK_SDONLY, false);

	
	brcmu_pktq_flush(&bus->txq, true, NULL, NULL);

	
	if (bus->glomd)
		brcmu_pkt_buf_free_skb(bus->glomd);
	brcmf_sdbrcm_free_glom(bus);

	
	bus->rxlen = 0;
	brcmf_sdbrcm_dcmd_resp_wake(bus);

	
	bus->rxskip = false;
	bus->tx_seq = bus->rx_seq = 0;

	up(&bus->sdsem);
}

static bool brcmf_sdbrcm_dpc(struct brcmf_sdio *bus)
{
	u32 intstatus, newstatus = 0;
	uint retries = 0;
	uint rxlimit = bus->rxbound;	
	uint txlimit = bus->txbound;	
	uint framecnt = 0;	
	bool rxdone = true;	
	bool resched = false;	

	brcmf_dbg(TRACE, "Enter\n");

	
	intstatus = bus->intstatus;

	down(&bus->sdsem);

	
	if (bus->clkstate == CLK_PENDING) {
		int err;
		u8 clkctl, devctl = 0;

#ifdef DEBUG
		
		devctl = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					       SBSDIO_DEVICE_CTL, &err);
		if (err) {
			brcmf_dbg(ERROR, "error reading DEVCTL: %d\n", err);
			bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
		}
#endif				

		
		clkctl = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					       SBSDIO_FUNC1_CHIPCLKCSR, &err);
		if (err) {
			brcmf_dbg(ERROR, "error reading CSR: %d\n",
				  err);
			bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
		}

		brcmf_dbg(INFO, "DPC: PENDING, devctl 0x%02x clkctl 0x%02x\n",
			  devctl, clkctl);

		if (SBSDIO_HTAV(clkctl)) {
			devctl = brcmf_sdcard_cfg_read(bus->sdiodev,
						       SDIO_FUNC_1,
						       SBSDIO_DEVICE_CTL, &err);
			if (err) {
				brcmf_dbg(ERROR, "error reading DEVCTL: %d\n",
					  err);
				bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
			}
			devctl &= ~SBSDIO_DEVCTL_CA_INT_ONLY;
			brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				SBSDIO_DEVICE_CTL, devctl, &err);
			if (err) {
				brcmf_dbg(ERROR, "error writing DEVCTL: %d\n",
					  err);
				bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
			}
			bus->clkstate = CLK_AVAIL;
		} else {
			goto clkwait;
		}
	}

	bus_wake(bus);

	
	brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, true);
	if (bus->clkstate == CLK_PENDING)
		goto clkwait;

	
	if (bus->ipend) {
		bus->ipend = false;
		r_sdreg32(bus, &newstatus,
			  offsetof(struct sdpcmd_regs, intstatus), &retries);
		bus->f1regdata++;
		if (brcmf_sdcard_regfail(bus->sdiodev))
			newstatus = 0;
		newstatus &= bus->hostintmask;
		bus->fcstate = !!(newstatus & I_HMB_FC_STATE);
		if (newstatus) {
			w_sdreg32(bus, newstatus,
				  offsetof(struct sdpcmd_regs, intstatus),
				  &retries);
			bus->f1regdata++;
		}
	}

	
	intstatus |= newstatus;
	bus->intstatus = 0;

	if (intstatus & I_HMB_FC_CHANGE) {
		intstatus &= ~I_HMB_FC_CHANGE;
		w_sdreg32(bus, I_HMB_FC_CHANGE,
			  offsetof(struct sdpcmd_regs, intstatus), &retries);

		r_sdreg32(bus, &newstatus,
			  offsetof(struct sdpcmd_regs, intstatus), &retries);
		bus->f1regdata += 2;
		bus->fcstate =
		    !!(newstatus & (I_HMB_FC_STATE | I_HMB_FC_CHANGE));
		intstatus |= (newstatus & bus->hostintmask);
	}

	
	if (intstatus & I_HMB_HOST_INT) {
		intstatus &= ~I_HMB_HOST_INT;
		intstatus |= brcmf_sdbrcm_hostmail(bus);
	}

	
	if (intstatus & I_WR_OOSYNC) {
		brcmf_dbg(ERROR, "Dongle reports WR_OOSYNC\n");
		intstatus &= ~I_WR_OOSYNC;
	}

	if (intstatus & I_RD_OOSYNC) {
		brcmf_dbg(ERROR, "Dongle reports RD_OOSYNC\n");
		intstatus &= ~I_RD_OOSYNC;
	}

	if (intstatus & I_SBINT) {
		brcmf_dbg(ERROR, "Dongle reports SBINT\n");
		intstatus &= ~I_SBINT;
	}

	
	if (intstatus & I_CHIPACTIVE) {
		brcmf_dbg(INFO, "Dongle reports CHIPACTIVE\n");
		intstatus &= ~I_CHIPACTIVE;
	}

	
	if (bus->rxskip)
		intstatus &= ~I_HMB_FRAME_IND;

	
	if (PKT_AVAILABLE()) {
		framecnt = brcmf_sdbrcm_readframes(bus, rxlimit, &rxdone);
		if (rxdone || bus->rxskip)
			intstatus &= ~I_HMB_FRAME_IND;
		rxlimit -= min(framecnt, rxlimit);
	}

	
	bus->intstatus = intstatus;

clkwait:
	if (data_ok(bus) && bus->ctrl_frame_stat &&
		(bus->clkstate == CLK_AVAIL)) {
		int ret, i;

		ret = brcmf_sdcard_send_buf(bus->sdiodev, bus->sdiodev->sbwad,
			SDIO_FUNC_2, F2SYNC, (u8 *) bus->ctrl_frame_buf,
			(u32) bus->ctrl_frame_len);

		if (ret < 0) {
			brcmf_dbg(INFO, "sdio error %d, abort command and terminate frame\n",
				  ret);
			bus->tx_sderrs++;

			brcmf_sdcard_abort(bus->sdiodev, SDIO_FUNC_2);

			brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
					 SBSDIO_FUNC1_FRAMECTRL, SFC_WF_TERM,
					 NULL);
			bus->f1regdata++;

			for (i = 0; i < 3; i++) {
				u8 hi, lo;
				hi = brcmf_sdcard_cfg_read(bus->sdiodev,
						     SDIO_FUNC_1,
						     SBSDIO_FUNC1_WFRAMEBCHI,
						     NULL);
				lo = brcmf_sdcard_cfg_read(bus->sdiodev,
						     SDIO_FUNC_1,
						     SBSDIO_FUNC1_WFRAMEBCLO,
						     NULL);
				bus->f1regdata += 2;
				if ((hi == 0) && (lo == 0))
					break;
			}

		}
		if (ret == 0)
			bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;

		brcmf_dbg(INFO, "Return_dpc value is : %d\n", ret);
		bus->ctrl_frame_stat = false;
		brcmf_sdbrcm_wait_event_wakeup(bus);
	}
	
	else if ((bus->clkstate == CLK_AVAIL) && !bus->fcstate &&
		 brcmu_pktq_mlen(&bus->txq, ~bus->flowcontrol) && txlimit
		 && data_ok(bus)) {
		framecnt = rxdone ? txlimit : min(txlimit, bus->txminmax);
		framecnt = brcmf_sdbrcm_sendfromq(bus, framecnt);
		txlimit -= framecnt;
	}

	if ((bus->sdiodev->bus_if->state == BRCMF_BUS_DOWN) ||
	    brcmf_sdcard_regfail(bus->sdiodev)) {
		brcmf_dbg(ERROR, "failed backplane access over SDIO, halting operation %d\n",
			  brcmf_sdcard_regfail(bus->sdiodev));
		bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
		bus->intstatus = 0;
	} else if (bus->clkstate == CLK_PENDING) {
		brcmf_dbg(INFO, "rescheduled due to CLK_PENDING awaiting I_CHIPACTIVE interrupt\n");
		resched = true;
	} else if (bus->intstatus || bus->ipend ||
		(!bus->fcstate && brcmu_pktq_mlen(&bus->txq, ~bus->flowcontrol)
		 && data_ok(bus)) || PKT_AVAILABLE()) {
		resched = true;
	}

	bus->dpc_sched = resched;

	
	if ((bus->clkstate != CLK_PENDING)
	    && bus->idletime == BRCMF_IDLE_IMMEDIATE) {
		bus->activity = false;
		brcmf_sdbrcm_clkctl(bus, CLK_NONE, false);
	}

	up(&bus->sdsem);

	return resched;
}

static inline void brcmf_sdbrcm_adddpctsk(struct brcmf_sdio *bus)
{
	struct list_head *new_hd;
	unsigned long flags;

	if (in_interrupt())
		new_hd = kzalloc(sizeof(struct list_head), GFP_ATOMIC);
	else
		new_hd = kzalloc(sizeof(struct list_head), GFP_KERNEL);
	if (new_hd == NULL)
		return;

	spin_lock_irqsave(&bus->dpc_tl_lock, flags);
	list_add_tail(new_hd, &bus->dpc_tsklst);
	spin_unlock_irqrestore(&bus->dpc_tl_lock, flags);
}

static int brcmf_sdbrcm_dpc_thread(void *data)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *) data;
	struct list_head *cur_hd, *tmp_hd;
	unsigned long flags;

	allow_signal(SIGTERM);
	
	while (1) {
		if (kthread_should_stop())
			break;

		if (list_empty(&bus->dpc_tsklst))
			if (wait_for_completion_interruptible(&bus->dpc_wait))
				break;

		spin_lock_irqsave(&bus->dpc_tl_lock, flags);
		list_for_each_safe(cur_hd, tmp_hd, &bus->dpc_tsklst) {
			spin_unlock_irqrestore(&bus->dpc_tl_lock, flags);

			if (bus->sdiodev->bus_if->state == BRCMF_BUS_DOWN) {
				
				brcmf_sdbrcm_bus_stop(bus->sdiodev->dev);
				bus->dpc_tsk = NULL;
				spin_lock_irqsave(&bus->dpc_tl_lock, flags);
				break;
			}

			if (brcmf_sdbrcm_dpc(bus))
				brcmf_sdbrcm_adddpctsk(bus);

			spin_lock_irqsave(&bus->dpc_tl_lock, flags);
			list_del(cur_hd);
			kfree(cur_hd);
		}
		spin_unlock_irqrestore(&bus->dpc_tl_lock, flags);
	}
	return 0;
}

static int brcmf_sdbrcm_bus_txdata(struct device *dev, struct sk_buff *pkt)
{
	int ret = -EBADE;
	uint datalen, prec;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter\n");

	datalen = pkt->len;

	
	skb_push(pkt, SDPCM_HDRLEN);
	

	prec = prio2prec((pkt->priority & PRIOMASK));

	brcmf_dbg(TRACE, "deferring pktq len %d\n", pktq_len(&bus->txq));
	bus->fcqueued++;

	
	spin_lock_bh(&bus->txqlock);
	if (!brcmf_c_prec_enq(bus->sdiodev->dev, &bus->txq, pkt, prec)) {
		skb_pull(pkt, SDPCM_HDRLEN);
		brcmf_txcomplete(bus->sdiodev->dev, pkt, false);
		brcmu_pkt_buf_free_skb(pkt);
		brcmf_dbg(ERROR, "out of bus->txq !!!\n");
		ret = -ENOSR;
	} else {
		ret = 0;
	}
	spin_unlock_bh(&bus->txqlock);

	if (pktq_len(&bus->txq) >= TXHI) {
		bus->txoff = ON;
		brcmf_txflowcontrol(bus->sdiodev->dev, 0, ON);
	}

#ifdef DEBUG
	if (pktq_plen(&bus->txq, prec) > qcount[prec])
		qcount[prec] = pktq_plen(&bus->txq, prec);
#endif
	
	if (!bus->dpc_sched) {
		bus->dpc_sched = true;
		if (bus->dpc_tsk) {
			brcmf_sdbrcm_adddpctsk(bus);
			complete(&bus->dpc_wait);
		}
	}

	return ret;
}

static int
brcmf_sdbrcm_membytes(struct brcmf_sdio *bus, bool write, u32 address, u8 *data,
		 uint size)
{
	int bcmerror = 0;
	u32 sdaddr;
	uint dsize;

	
	sdaddr = address & SBSDIO_SB_OFT_ADDR_MASK;
	if ((sdaddr + size) & SBSDIO_SBWINDOW_MASK)
		dsize = (SBSDIO_SB_OFT_ADDR_LIMIT - sdaddr);
	else
		dsize = size;

	
	bcmerror = brcmf_sdcard_set_sbaddr_window(bus->sdiodev, address);
	if (bcmerror) {
		brcmf_dbg(ERROR, "window change failed\n");
		goto xfer_done;
	}

	
	while (size) {
		brcmf_dbg(INFO, "%s %d bytes at offset 0x%08x in window 0x%08x\n",
			  write ? "write" : "read", dsize,
			  sdaddr, address & SBSDIO_SBWINDOW_MASK);
		bcmerror = brcmf_sdcard_rwdata(bus->sdiodev, write,
					       sdaddr, data, dsize);
		if (bcmerror) {
			brcmf_dbg(ERROR, "membytes transfer failed\n");
			break;
		}

		
		size -= dsize;
		if (size) {
			data += dsize;
			address += dsize;
			bcmerror = brcmf_sdcard_set_sbaddr_window(bus->sdiodev,
								  address);
			if (bcmerror) {
				brcmf_dbg(ERROR, "window change failed\n");
				break;
			}
			sdaddr = 0;
			dsize = min_t(uint, SBSDIO_SB_OFT_ADDR_LIMIT, size);
		}
	}

xfer_done:
	
	if (brcmf_sdcard_set_sbaddr_window(bus->sdiodev, bus->sdiodev->sbwad))
		brcmf_dbg(ERROR, "FAILED to set window back to 0x%x\n",
			  bus->sdiodev->sbwad);

	return bcmerror;
}

#ifdef DEBUG
#define CONSOLE_LINE_MAX	192

static int brcmf_sdbrcm_readconsole(struct brcmf_sdio *bus)
{
	struct brcmf_console *c = &bus->console;
	u8 line[CONSOLE_LINE_MAX], ch;
	u32 n, idx, addr;
	int rv;

	
	if (bus->console_addr == 0)
		return 0;

	
	addr = bus->console_addr + offsetof(struct rte_console, log_le);
	rv = brcmf_sdbrcm_membytes(bus, false, addr, (u8 *)&c->log_le,
				   sizeof(c->log_le));
	if (rv < 0)
		return rv;

	
	if (c->buf == NULL) {
		c->bufsize = le32_to_cpu(c->log_le.buf_size);
		c->buf = kmalloc(c->bufsize, GFP_ATOMIC);
		if (c->buf == NULL)
			return -ENOMEM;
	}

	idx = le32_to_cpu(c->log_le.idx);

	
	if (idx > c->bufsize)
		return -EBADE;

	if (idx == c->last)
		return 0;

	
	addr = le32_to_cpu(c->log_le.buf);
	rv = brcmf_sdbrcm_membytes(bus, false, addr, c->buf, c->bufsize);
	if (rv < 0)
		return rv;

	while (c->last != idx) {
		for (n = 0; n < CONSOLE_LINE_MAX - 2; n++) {
			if (c->last == idx) {
				if (c->last >= n)
					c->last -= n;
				else
					c->last = c->bufsize - n;
				goto break2;
			}
			ch = c->buf[c->last];
			c->last = (c->last + 1) % c->bufsize;
			if (ch == '\n')
				break;
			line[n] = ch;
		}

		if (n > 0) {
			if (line[n - 1] == '\r')
				n--;
			line[n] = 0;
			pr_debug("CONSOLE: %s\n", line);
		}
	}
break2:

	return 0;
}
#endif				

static int brcmf_tx_frame(struct brcmf_sdio *bus, u8 *frame, u16 len)
{
	int i;
	int ret;

	bus->ctrl_frame_stat = false;
	ret = brcmf_sdcard_send_buf(bus->sdiodev, bus->sdiodev->sbwad,
				    SDIO_FUNC_2, F2SYNC, frame, len);

	if (ret < 0) {
		
		brcmf_dbg(INFO, "sdio error %d, abort command and terminate frame\n",
			  ret);
		bus->tx_sderrs++;

		brcmf_sdcard_abort(bus->sdiodev, SDIO_FUNC_2);

		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				       SBSDIO_FUNC1_FRAMECTRL,
				       SFC_WF_TERM, NULL);
		bus->f1regdata++;

		for (i = 0; i < 3; i++) {
			u8 hi, lo;
			hi = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
						   SBSDIO_FUNC1_WFRAMEBCHI,
						   NULL);
			lo = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
						   SBSDIO_FUNC1_WFRAMEBCLO,
						   NULL);
			bus->f1regdata += 2;
			if (hi == 0 && lo == 0)
				break;
		}
		return ret;
	}

	bus->tx_seq = (bus->tx_seq + 1) % SDPCM_SEQUENCE_WRAP;

	return ret;
}

static int
brcmf_sdbrcm_bus_txctl(struct device *dev, unsigned char *msg, uint msglen)
{
	u8 *frame;
	u16 len;
	u32 swheader;
	uint retries = 0;
	u8 doff = 0;
	int ret = -1;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter\n");

	
	frame = msg - SDPCM_HDRLEN;
	len = (msglen += SDPCM_HDRLEN);

	
	doff = ((unsigned long)frame % BRCMF_SDALIGN);
	if (doff) {
		frame -= doff;
		len += doff;
		msglen += doff;
		memset(frame, 0, doff + SDPCM_HDRLEN);
	}
	
	doff += SDPCM_HDRLEN;

	
	if (bus->roundup && bus->blocksize && (len > bus->blocksize)) {
		u16 pad = bus->blocksize - (len % bus->blocksize);
		if ((pad <= bus->roundup) && (pad < bus->blocksize))
			len += pad;
	} else if (len % BRCMF_SDALIGN) {
		len += BRCMF_SDALIGN - (len % BRCMF_SDALIGN);
	}

	
	if (len & (ALIGNMENT - 1))
		len = roundup(len, ALIGNMENT);

	

	
	down(&bus->sdsem);

	bus_wake(bus);

	
	brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);

	
	*(__le16 *) frame = cpu_to_le16((u16) msglen);
	*(((__le16 *) frame) + 1) = cpu_to_le16(~msglen);

	
	swheader =
	    ((SDPCM_CONTROL_CHANNEL << SDPCM_CHANNEL_SHIFT) &
	     SDPCM_CHANNEL_MASK)
	    | bus->tx_seq | ((doff << SDPCM_DOFFSET_SHIFT) &
			     SDPCM_DOFFSET_MASK);
	put_unaligned_le32(swheader, frame + SDPCM_FRAMETAG_LEN);
	put_unaligned_le32(0, frame + SDPCM_FRAMETAG_LEN + sizeof(swheader));

	if (!data_ok(bus)) {
		brcmf_dbg(INFO, "No bus credit bus->tx_max %d, bus->tx_seq %d\n",
			  bus->tx_max, bus->tx_seq);
		bus->ctrl_frame_stat = true;
		
		bus->ctrl_frame_buf = frame;
		bus->ctrl_frame_len = len;

		brcmf_sdbrcm_wait_for_event(bus, &bus->ctrl_frame_stat);

		if (!bus->ctrl_frame_stat) {
			brcmf_dbg(INFO, "ctrl_frame_stat == false\n");
			ret = 0;
		} else {
			brcmf_dbg(INFO, "ctrl_frame_stat == true\n");
			ret = -1;
		}
	}

	if (ret == -1) {
		brcmf_dbg_hex_dump(BRCMF_BYTES_ON() && BRCMF_CTL_ON(),
				   frame, len, "Tx Frame:\n");
		brcmf_dbg_hex_dump(!(BRCMF_BYTES_ON() && BRCMF_CTL_ON()) &&
				   BRCMF_HDRS_ON(),
				   frame, min_t(u16, len, 16), "TxHdr:\n");

		do {
			ret = brcmf_tx_frame(bus, frame, len);
		} while (ret < 0 && retries++ < TXRETRIES);
	}

	if ((bus->idletime == BRCMF_IDLE_IMMEDIATE) && !bus->dpc_sched) {
		bus->activity = false;
		brcmf_sdbrcm_clkctl(bus, CLK_NONE, true);
	}

	up(&bus->sdsem);

	if (ret)
		bus->tx_ctlerrs++;
	else
		bus->tx_ctlpkts++;

	return ret ? -EIO : 0;
}

static int
brcmf_sdbrcm_bus_rxctl(struct device *dev, unsigned char *msg, uint msglen)
{
	int timeleft;
	uint rxlen = 0;
	bool pending;
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;

	brcmf_dbg(TRACE, "Enter\n");

	
	timeleft = brcmf_sdbrcm_dcmd_resp_wait(bus, &bus->rxlen, &pending);

	down(&bus->sdsem);
	rxlen = bus->rxlen;
	memcpy(msg, bus->rxctl, min(msglen, rxlen));
	bus->rxlen = 0;
	up(&bus->sdsem);

	if (rxlen) {
		brcmf_dbg(CTL, "resumed on rxctl frame, got %d expected %d\n",
			  rxlen, msglen);
	} else if (timeleft == 0) {
		brcmf_dbg(ERROR, "resumed on timeout\n");
	} else if (pending) {
		brcmf_dbg(CTL, "cancelled\n");
		return -ERESTARTSYS;
	} else {
		brcmf_dbg(CTL, "resumed for unknown reason?\n");
	}

	if (rxlen)
		bus->rx_ctlpkts++;
	else
		bus->rx_ctlerrs++;

	return rxlen ? (int)rxlen : -ETIMEDOUT;
}

static int brcmf_sdbrcm_downloadvars(struct brcmf_sdio *bus, void *arg, int len)
{
	int bcmerror = 0;

	brcmf_dbg(TRACE, "Enter\n");

	
	if (bus->sdiodev->bus_if->drvr_up) {
		bcmerror = -EISCONN;
		goto err;
	}
	if (!len) {
		bcmerror = -EOVERFLOW;
		goto err;
	}

	
	kfree(bus->vars);

	bus->vars = kmalloc(len, GFP_ATOMIC);
	bus->varsz = bus->vars ? len : 0;
	if (bus->vars == NULL) {
		bcmerror = -ENOMEM;
		goto err;
	}

	memcpy(bus->vars, arg, bus->varsz);
err:
	return bcmerror;
}

static int brcmf_sdbrcm_write_vars(struct brcmf_sdio *bus)
{
	int bcmerror = 0;
	u32 varsize;
	u32 varaddr;
	u8 *vbuffer;
	u32 varsizew;
	__le32 varsizew_le;
#ifdef DEBUG
	char *nvram_ularray;
#endif				

	/* Even if there are no vars are to be written, we still
		 need to set the ramsize. */
	varsize = bus->varsz ? roundup(bus->varsz, 4) : 0;
	varaddr = (bus->ramsize - 4) - varsize;

	if (bus->vars) {
		vbuffer = kzalloc(varsize, GFP_ATOMIC);
		if (!vbuffer)
			return -ENOMEM;

		memcpy(vbuffer, bus->vars, bus->varsz);

		
		bcmerror =
		    brcmf_sdbrcm_membytes(bus, true, varaddr, vbuffer, varsize);
#ifdef DEBUG
		
		brcmf_dbg(INFO, "Compare NVRAM dl & ul; varsize=%d\n", varsize);
		nvram_ularray = kmalloc(varsize, GFP_ATOMIC);
		if (!nvram_ularray) {
			kfree(vbuffer);
			return -ENOMEM;
		}

		
		memset(nvram_ularray, 0xaa, varsize);

		
		bcmerror =
		    brcmf_sdbrcm_membytes(bus, false, varaddr, nvram_ularray,
				     varsize);
		if (bcmerror) {
			brcmf_dbg(ERROR, "error %d on reading %d nvram bytes at 0x%08x\n",
				  bcmerror, varsize, varaddr);
		}
		
		if (memcmp(vbuffer, nvram_ularray, varsize))
			brcmf_dbg(ERROR, "Downloaded NVRAM image is corrupted\n");
		else
			brcmf_dbg(ERROR, "Download/Upload/Compare of NVRAM ok\n");

		kfree(nvram_ularray);
#endif				

		kfree(vbuffer);
	}

	
	brcmf_dbg(INFO, "Physical memory size: %d\n", bus->ramsize);
	brcmf_dbg(INFO, "Vars are at %d, orig varsize is %d\n",
		  varaddr, varsize);
	varsize = ((bus->ramsize - 4) - varaddr);

	if (bcmerror) {
		varsizew = 0;
		varsizew_le = cpu_to_le32(0);
	} else {
		varsizew = varsize / 4;
		varsizew = (~varsizew << 16) | (varsizew & 0x0000FFFF);
		varsizew_le = cpu_to_le32(varsizew);
	}

	brcmf_dbg(INFO, "New varsize is %d, length token=0x%08x\n",
		  varsize, varsizew);

	
	bcmerror = brcmf_sdbrcm_membytes(bus, true, (bus->ramsize - 4),
					 (u8 *)&varsizew_le, 4);

	return bcmerror;
}

static int brcmf_sdbrcm_download_state(struct brcmf_sdio *bus, bool enter)
{
	uint retries;
	int bcmerror = 0;
	struct chip_info *ci = bus->ci;

	if (enter) {
		bus->alp_only = true;

		ci->coredisable(bus->sdiodev, ci, BCMA_CORE_ARM_CM3);

		ci->resetcore(bus->sdiodev, ci, BCMA_CORE_INTERNAL_MEM);

		
		if (bus->ramsize) {
			u32 zeros = 0;
			brcmf_sdbrcm_membytes(bus, true, bus->ramsize - 4,
					 (u8 *)&zeros, 4);
		}
	} else {
		if (!ci->iscoreup(bus->sdiodev, ci, BCMA_CORE_INTERNAL_MEM)) {
			brcmf_dbg(ERROR, "SOCRAM core is down after reset?\n");
			bcmerror = -EBADE;
			goto fail;
		}

		bcmerror = brcmf_sdbrcm_write_vars(bus);
		if (bcmerror) {
			brcmf_dbg(ERROR, "no vars written to RAM\n");
			bcmerror = 0;
		}

		w_sdreg32(bus, 0xFFFFFFFF,
			  offsetof(struct sdpcmd_regs, intstatus), &retries);

		ci->resetcore(bus->sdiodev, ci, BCMA_CORE_ARM_CM3);

		
		bus->alp_only = false;

		bus->sdiodev->bus_if->state = BRCMF_BUS_LOAD;
	}
fail:
	return bcmerror;
}

static int brcmf_sdbrcm_get_image(char *buf, int len, struct brcmf_sdio *bus)
{
	if (bus->firmware->size < bus->fw_ptr + len)
		len = bus->firmware->size - bus->fw_ptr;

	memcpy(buf, &bus->firmware->data[bus->fw_ptr], len);
	bus->fw_ptr += len;
	return len;
}

static int brcmf_sdbrcm_download_code_file(struct brcmf_sdio *bus)
{
	int offset = 0;
	uint len;
	u8 *memblock = NULL, *memptr;
	int ret;

	brcmf_dbg(INFO, "Enter\n");

	ret = request_firmware(&bus->firmware, BRCMF_SDIO_FW_NAME,
			       &bus->sdiodev->func[2]->dev);
	if (ret) {
		brcmf_dbg(ERROR, "Fail to request firmware %d\n", ret);
		return ret;
	}
	bus->fw_ptr = 0;

	memptr = memblock = kmalloc(MEMBLOCK + BRCMF_SDALIGN, GFP_ATOMIC);
	if (memblock == NULL) {
		ret = -ENOMEM;
		goto err;
	}
	if ((u32)(unsigned long)memblock % BRCMF_SDALIGN)
		memptr += (BRCMF_SDALIGN -
			   ((u32)(unsigned long)memblock % BRCMF_SDALIGN));

	
	while ((len =
		brcmf_sdbrcm_get_image((char *)memptr, MEMBLOCK, bus))) {
		ret = brcmf_sdbrcm_membytes(bus, true, offset, memptr, len);
		if (ret) {
			brcmf_dbg(ERROR, "error %d on writing %d membytes at 0x%08x\n",
				  ret, MEMBLOCK, offset);
			goto err;
		}

		offset += MEMBLOCK;
	}

err:
	kfree(memblock);

	release_firmware(bus->firmware);
	bus->fw_ptr = 0;

	return ret;
}


static uint brcmf_process_nvram_vars(char *varbuf, uint len)
{
	char *dp;
	bool findNewline;
	int column;
	uint buf_len, n;

	dp = varbuf;

	findNewline = false;
	column = 0;

	for (n = 0; n < len; n++) {
		if (varbuf[n] == 0)
			break;
		if (varbuf[n] == '\r')
			continue;
		if (findNewline && varbuf[n] != '\n')
			continue;
		findNewline = false;
		if (varbuf[n] == '#') {
			findNewline = true;
			continue;
		}
		if (varbuf[n] == '\n') {
			if (column == 0)
				continue;
			*dp++ = 0;
			column = 0;
			continue;
		}
		*dp++ = varbuf[n];
		column++;
	}
	buf_len = dp - varbuf;

	while (dp < varbuf + n)
		*dp++ = 0;

	return buf_len;
}

static int brcmf_sdbrcm_download_nvram(struct brcmf_sdio *bus)
{
	uint len;
	char *memblock = NULL;
	char *bufp;
	int ret;

	ret = request_firmware(&bus->firmware, BRCMF_SDIO_NV_NAME,
			       &bus->sdiodev->func[2]->dev);
	if (ret) {
		brcmf_dbg(ERROR, "Fail to request nvram %d\n", ret);
		return ret;
	}
	bus->fw_ptr = 0;

	memblock = kmalloc(MEMBLOCK, GFP_ATOMIC);
	if (memblock == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	len = brcmf_sdbrcm_get_image(memblock, MEMBLOCK, bus);

	if (len > 0 && len < MEMBLOCK) {
		bufp = (char *)memblock;
		bufp[len] = 0;
		len = brcmf_process_nvram_vars(bufp, len);
		bufp += len;
		*bufp++ = 0;
		if (len)
			ret = brcmf_sdbrcm_downloadvars(bus, memblock, len + 1);
		if (ret)
			brcmf_dbg(ERROR, "error downloading vars: %d\n", ret);
	} else {
		brcmf_dbg(ERROR, "error reading nvram file: %d\n", len);
		ret = -EIO;
	}

err:
	kfree(memblock);

	release_firmware(bus->firmware);
	bus->fw_ptr = 0;

	return ret;
}

static int _brcmf_sdbrcm_download_firmware(struct brcmf_sdio *bus)
{
	int bcmerror = -1;

	
	if (brcmf_sdbrcm_download_state(bus, true)) {
		brcmf_dbg(ERROR, "error placing ARM core in reset\n");
		goto err;
	}

	
	if (brcmf_sdbrcm_download_code_file(bus)) {
		brcmf_dbg(ERROR, "dongle image file download failed\n");
		goto err;
	}

	
	if (brcmf_sdbrcm_download_nvram(bus))
		brcmf_dbg(ERROR, "dongle nvram file download failed\n");

	
	if (brcmf_sdbrcm_download_state(bus, false)) {
		brcmf_dbg(ERROR, "error getting out of ARM core reset\n");
		goto err;
	}

	bcmerror = 0;

err:
	return bcmerror;
}

static bool
brcmf_sdbrcm_download_firmware(struct brcmf_sdio *bus)
{
	bool ret;

	
	brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);

	ret = _brcmf_sdbrcm_download_firmware(bus) == 0;

	brcmf_sdbrcm_clkctl(bus, CLK_SDONLY, false);

	return ret;
}

static int brcmf_sdbrcm_bus_init(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	struct brcmf_sdio_dev *sdiodev = bus_if->bus_priv.sdio;
	struct brcmf_sdio *bus = sdiodev->bus;
	unsigned long timeout;
	uint retries = 0;
	u8 ready, enable;
	int err, ret = 0;
	u8 saveclk;

	brcmf_dbg(TRACE, "Enter\n");

	
	if (bus_if->state == BRCMF_BUS_DOWN) {
		if (!(brcmf_sdbrcm_download_firmware(bus)))
			return -1;
	}

	if (!bus->sdiodev->bus_if->drvr)
		return 0;

	
	bus->tickcnt = 0;
	brcmf_sdbrcm_wd_timer(bus, BRCMF_WD_POLL_MS);

	down(&bus->sdsem);

	
	brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);
	if (bus->clkstate != CLK_AVAIL)
		goto exit;

	
	saveclk =
	    brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
				  SBSDIO_FUNC1_CHIPCLKCSR, &err);
	if (!err) {
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				       SBSDIO_FUNC1_CHIPCLKCSR,
				       (saveclk | SBSDIO_FORCE_HT), &err);
	}
	if (err) {
		brcmf_dbg(ERROR, "Failed to force clock for F2: err %d\n", err);
		goto exit;
	}

	
	w_sdreg32(bus, SDPCM_PROT_VERSION << SMB_DATA_VERSION_SHIFT,
		  offsetof(struct sdpcmd_regs, tosbmailboxdata), &retries);
	enable = (SDIO_FUNC_ENABLE_1 | SDIO_FUNC_ENABLE_2);

	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_0, SDIO_CCCR_IOEx,
			       enable, NULL);

	timeout = jiffies + msecs_to_jiffies(BRCMF_WAIT_F2RDY);
	ready = 0;
	while (enable != ready) {
		ready = brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_0,
					      SDIO_CCCR_IORx, NULL);
		if (time_after(jiffies, timeout))
			break;
		else if (time_after(jiffies, timeout - BRCMF_WAIT_F2RDY + 50))
			
			msleep_interruptible(20);
	}

	brcmf_dbg(INFO, "enable 0x%02x, ready 0x%02x\n", enable, ready);

	
	if (ready == enable) {
		
		bus->hostintmask = HOSTINTMASK;
		w_sdreg32(bus, bus->hostintmask,
			  offsetof(struct sdpcmd_regs, hostintmask), &retries);

		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
				       SBSDIO_WATERMARK, 8, &err);
	} else {
		
		enable = SDIO_FUNC_ENABLE_1;
		brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_0,
				       SDIO_CCCR_IOEx, enable, NULL);
		ret = -ENODEV;
	}

	
	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			       SBSDIO_FUNC1_CHIPCLKCSR, saveclk, &err);

	
	if (!ret)
		brcmf_sdbrcm_clkctl(bus, CLK_NONE, false);

exit:
	up(&bus->sdsem);

	return ret;
}

void brcmf_sdbrcm_isr(void *arg)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *) arg;

	brcmf_dbg(TRACE, "Enter\n");

	if (!bus) {
		brcmf_dbg(ERROR, "bus is null pointer, exiting\n");
		return;
	}

	if (bus->sdiodev->bus_if->state == BRCMF_BUS_DOWN) {
		brcmf_dbg(ERROR, "bus is down. we have nothing to do\n");
		return;
	}
	
	bus->intrcount++;
	bus->ipend = true;

	
	if (bus->sleeping) {
		brcmf_dbg(ERROR, "INTERRUPT WHILE SLEEPING??\n");
		return;
	}

	
	if (!bus->intr)
		brcmf_dbg(ERROR, "isr w/o interrupt configured!\n");

	bus->dpc_sched = true;
	if (bus->dpc_tsk) {
		brcmf_sdbrcm_adddpctsk(bus);
		complete(&bus->dpc_wait);
	}
}

static bool brcmf_sdbrcm_bus_watchdog(struct brcmf_sdio *bus)
{
#ifdef DEBUG
	struct brcmf_bus *bus_if = dev_get_drvdata(bus->sdiodev->dev);
#endif	

	brcmf_dbg(TIMER, "Enter\n");

	
	if (bus->sleeping)
		return false;

	down(&bus->sdsem);

	
	if (bus->poll && (++bus->polltick >= bus->pollrate)) {
		u32 intstatus = 0;

		
		bus->polltick = 0;

		
		if (!bus->intr || (bus->intrcount == bus->lastintrs)) {

			if (!bus->dpc_sched) {
				u8 devpend;
				devpend = brcmf_sdcard_cfg_read(bus->sdiodev,
						SDIO_FUNC_0, SDIO_CCCR_INTx,
						NULL);
				intstatus =
				    devpend & (INTR_STATUS_FUNC1 |
					       INTR_STATUS_FUNC2);
			}

			if (intstatus) {
				bus->pollcnt++;
				bus->ipend = true;

				bus->dpc_sched = true;
				if (bus->dpc_tsk) {
					brcmf_sdbrcm_adddpctsk(bus);
					complete(&bus->dpc_wait);
				}
			}
		}

		
		bus->lastintrs = bus->intrcount;
	}
#ifdef DEBUG
	
	if (bus_if->state == BRCMF_BUS_DATA &&
	    bus->console_interval != 0) {
		bus->console.count += BRCMF_WD_POLL_MS;
		if (bus->console.count >= bus->console_interval) {
			bus->console.count -= bus->console_interval;
			
			brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);
			if (brcmf_sdbrcm_readconsole(bus) < 0)
				
				bus->console_interval = 0;
		}
	}
#endif				

	
	if ((bus->idletime > 0) && (bus->clkstate == CLK_AVAIL)) {
		if (++bus->idlecount >= bus->idletime) {
			bus->idlecount = 0;
			if (bus->activity) {
				bus->activity = false;
				brcmf_sdbrcm_wd_timer(bus, BRCMF_WD_POLL_MS);
			} else {
				brcmf_sdbrcm_clkctl(bus, CLK_NONE, false);
			}
		}
	}

	up(&bus->sdsem);

	return bus->ipend;
}

static bool brcmf_sdbrcm_chipmatch(u16 chipid)
{
	if (chipid == BCM4329_CHIP_ID)
		return true;
	if (chipid == BCM4330_CHIP_ID)
		return true;
	return false;
}

static void brcmf_sdbrcm_release_malloc(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	kfree(bus->rxbuf);
	bus->rxctl = bus->rxbuf = NULL;
	bus->rxlen = 0;

	kfree(bus->databuf);
	bus->databuf = NULL;
}

static bool brcmf_sdbrcm_probe_malloc(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (bus->sdiodev->bus_if->maxctl) {
		bus->rxblen =
		    roundup((bus->sdiodev->bus_if->maxctl + SDPCM_HDRLEN),
			    ALIGNMENT) + BRCMF_SDALIGN;
		bus->rxbuf = kmalloc(bus->rxblen, GFP_ATOMIC);
		if (!(bus->rxbuf))
			goto fail;
	}

	
	bus->databuf = kmalloc(MAX_DATA_BUF, GFP_ATOMIC);
	if (!(bus->databuf)) {
		
		if (!bus->rxblen)
			kfree(bus->rxbuf);
		goto fail;
	}

	
	if ((unsigned long)bus->databuf % BRCMF_SDALIGN)
		bus->dataptr = bus->databuf + (BRCMF_SDALIGN -
			       ((unsigned long)bus->databuf % BRCMF_SDALIGN));
	else
		bus->dataptr = bus->databuf;

	return true;

fail:
	return false;
}

static bool
brcmf_sdbrcm_probe_attach(struct brcmf_sdio *bus, u32 regsva)
{
	u8 clkctl = 0;
	int err = 0;
	int reg_addr;
	u32 reg_val;
	u8 idx;

	bus->alp_only = true;

	
	if (brcmf_sdcard_set_sbaddr_window(bus->sdiodev, SI_ENUM_BASE))
		brcmf_dbg(ERROR, "FAILED to return to SI_ENUM_BASE\n");

	pr_debug("F1 signature read @0x18000000=0x%4x\n",
		 brcmf_sdcard_reg_read(bus->sdiodev, SI_ENUM_BASE, 4));


	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			       SBSDIO_FUNC1_CHIPCLKCSR,
			       BRCMF_INIT_CLKCTL1, &err);
	if (!err)
		clkctl =
		    brcmf_sdcard_cfg_read(bus->sdiodev, SDIO_FUNC_1,
					  SBSDIO_FUNC1_CHIPCLKCSR, &err);

	if (err || ((clkctl & ~SBSDIO_AVBITS) != BRCMF_INIT_CLKCTL1)) {
		brcmf_dbg(ERROR, "ChipClkCSR access: err %d wrote 0x%02x read 0x%02x\n",
			  err, BRCMF_INIT_CLKCTL1, clkctl);
		goto fail;
	}

	if (brcmf_sdio_chip_attach(bus->sdiodev, &bus->ci, regsva)) {
		brcmf_dbg(ERROR, "brcmf_sdio_chip_attach failed!\n");
		goto fail;
	}

	if (!brcmf_sdbrcm_chipmatch((u16) bus->ci->chip)) {
		brcmf_dbg(ERROR, "unsupported chip: 0x%04x\n", bus->ci->chip);
		goto fail;
	}

	brcmf_sdio_chip_drivestrengthinit(bus->sdiodev, bus->ci,
					  SDIO_DRIVE_STRENGTH);

	
	bus->ramsize = bus->ci->ramsize;
	if (!(bus->ramsize)) {
		brcmf_dbg(ERROR, "failed to find SOCRAM memory!\n");
		goto fail;
	}

	
	idx = brcmf_sdio_chip_getinfidx(bus->ci, BCMA_CORE_SDIO_DEV);
	reg_addr = bus->ci->c_inf[idx].base +
		   offsetof(struct sdpcmd_regs, corecontrol);
	reg_val = brcmf_sdcard_reg_read(bus->sdiodev, reg_addr, sizeof(u32));
	brcmf_sdcard_reg_write(bus->sdiodev, reg_addr, sizeof(u32),
			       reg_val | CC_BPRESEN);

	brcmu_pktq_init(&bus->txq, (PRIOMASK + 1), TXQLEN);

	
	bus->rxhdr = (u8 *) roundup((unsigned long)&bus->hdrbuf[0],
				    BRCMF_SDALIGN);

	
	bus->intr = true;
	bus->poll = false;
	if (bus->poll)
		bus->pollrate = 1;

	return true;

fail:
	return false;
}

static bool brcmf_sdbrcm_probe_init(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	
	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_0, SDIO_CCCR_IOEx,
			       SDIO_FUNC_ENABLE_1, NULL);

	bus->sdiodev->bus_if->state = BRCMF_BUS_DOWN;
	bus->sleeping = false;
	bus->rxflow = false;

	
	brcmf_sdcard_cfg_write(bus->sdiodev, SDIO_FUNC_1,
			       SBSDIO_FUNC1_CHIPCLKCSR, 0, NULL);

	
	bus->clkstate = CLK_SDONLY;
	bus->idletime = BRCMF_IDLE_INTERVAL;
	bus->idleclock = BRCMF_IDLE_ACTIVE;

	
	bus->blocksize = bus->sdiodev->func[2]->cur_blksize;
	bus->roundup = min(max_roundup, bus->blocksize);

	
	bus->use_rxchain = false;
	bus->sd_rxchain = false;

	return true;
}

static int
brcmf_sdbrcm_watchdog_thread(void *data)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *)data;

	allow_signal(SIGTERM);
	
	while (1) {
		if (kthread_should_stop())
			break;
		if (!wait_for_completion_interruptible(&bus->watchdog_wait)) {
			brcmf_sdbrcm_bus_watchdog(bus);
			
			bus->tickcnt++;
		} else
			break;
	}
	return 0;
}

static void
brcmf_sdbrcm_watchdog(unsigned long data)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *)data;

	if (bus->watchdog_tsk) {
		complete(&bus->watchdog_wait);
		
		if (bus->wd_timer_valid)
			mod_timer(&bus->timer,
				  jiffies + BRCMF_WD_POLL_MS * HZ / 1000);
	}
}

static void brcmf_sdbrcm_release_dongle(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (bus->ci) {
		brcmf_sdbrcm_clkctl(bus, CLK_AVAIL, false);
		brcmf_sdbrcm_clkctl(bus, CLK_NONE, false);
		brcmf_sdio_chip_detach(&bus->ci);
		if (bus->vars && bus->varsz)
			kfree(bus->vars);
		bus->vars = NULL;
	}

	brcmf_dbg(TRACE, "Disconnected\n");
}

static void brcmf_sdbrcm_release(struct brcmf_sdio *bus)
{
	brcmf_dbg(TRACE, "Enter\n");

	if (bus) {
		
		brcmf_sdcard_intr_dereg(bus->sdiodev);

		if (bus->sdiodev->bus_if->drvr) {
			brcmf_detach(bus->sdiodev->dev);
			brcmf_sdbrcm_release_dongle(bus);
		}

		brcmf_sdbrcm_release_malloc(bus);

		kfree(bus);
	}

	brcmf_dbg(TRACE, "Disconnected\n");
}

void *brcmf_sdbrcm_probe(u32 regsva, struct brcmf_sdio_dev *sdiodev)
{
	int ret;
	struct brcmf_sdio *bus;

	brcmf_dbg(TRACE, "Enter\n");


	
	bus = kzalloc(sizeof(struct brcmf_sdio), GFP_ATOMIC);
	if (!bus)
		goto fail;

	bus->sdiodev = sdiodev;
	sdiodev->bus = bus;
	skb_queue_head_init(&bus->glom);
	bus->txbound = BRCMF_TXBOUND;
	bus->rxbound = BRCMF_RXBOUND;
	bus->txminmax = BRCMF_TXMINMAX;
	bus->tx_seq = SDPCM_SEQUENCE_WRAP - 1;
	bus->usebufpool = false;	

	
	if (!(brcmf_sdbrcm_probe_attach(bus, regsva))) {
		brcmf_dbg(ERROR, "brcmf_sdbrcm_probe_attach failed\n");
		goto fail;
	}

	spin_lock_init(&bus->txqlock);
	init_waitqueue_head(&bus->ctrl_wait);
	init_waitqueue_head(&bus->dcmd_resp_wait);

	
	init_timer(&bus->timer);
	bus->timer.data = (unsigned long)bus;
	bus->timer.function = brcmf_sdbrcm_watchdog;

	
	sema_init(&bus->sdsem, 1);

	
	init_completion(&bus->watchdog_wait);
	bus->watchdog_tsk = kthread_run(brcmf_sdbrcm_watchdog_thread,
					bus, "brcmf_watchdog");
	if (IS_ERR(bus->watchdog_tsk)) {
		pr_warn("brcmf_watchdog thread failed to start\n");
		bus->watchdog_tsk = NULL;
	}
	
	init_completion(&bus->dpc_wait);
	INIT_LIST_HEAD(&bus->dpc_tsklst);
	spin_lock_init(&bus->dpc_tl_lock);
	bus->dpc_tsk = kthread_run(brcmf_sdbrcm_dpc_thread,
				   bus, "brcmf_dpc");
	if (IS_ERR(bus->dpc_tsk)) {
		pr_warn("brcmf_dpc thread failed to start\n");
		bus->dpc_tsk = NULL;
	}

	
	bus->sdiodev->bus_if->brcmf_bus_stop = brcmf_sdbrcm_bus_stop;
	bus->sdiodev->bus_if->brcmf_bus_init = brcmf_sdbrcm_bus_init;
	bus->sdiodev->bus_if->brcmf_bus_txdata = brcmf_sdbrcm_bus_txdata;
	bus->sdiodev->bus_if->brcmf_bus_txctl = brcmf_sdbrcm_bus_txctl;
	bus->sdiodev->bus_if->brcmf_bus_rxctl = brcmf_sdbrcm_bus_rxctl;
	
	ret = brcmf_attach(SDPCM_RESERVE, bus->sdiodev->dev);
	if (ret != 0) {
		brcmf_dbg(ERROR, "brcmf_attach failed\n");
		goto fail;
	}

	
	if (!(brcmf_sdbrcm_probe_malloc(bus))) {
		brcmf_dbg(ERROR, "brcmf_sdbrcm_probe_malloc failed\n");
		goto fail;
	}

	if (!(brcmf_sdbrcm_probe_init(bus))) {
		brcmf_dbg(ERROR, "brcmf_sdbrcm_probe_init failed\n");
		goto fail;
	}

	
	brcmf_dbg(INTR, "disable SDIO interrupts (not interested yet)\n");
	ret = brcmf_sdcard_intr_reg(bus->sdiodev);
	if (ret != 0) {
		brcmf_dbg(ERROR, "FAILED: sdcard_intr_reg returned %d\n", ret);
		goto fail;
	}
	brcmf_dbg(INTR, "registered SDIO interrupt function ok\n");

	brcmf_dbg(INFO, "completed!!\n");

	
	ret = brcmf_bus_start(bus->sdiodev->dev);
	if (ret != 0) {
		if (ret == -ENOLINK) {
			brcmf_dbg(ERROR, "dongle is not responding\n");
			goto fail;
		}
	}

	
	if (brcmf_add_if(bus->sdiodev->dev, 0, "wlan%d", NULL)) {
		brcmf_dbg(ERROR, "Add primary net device interface failed!!\n");
		goto fail;
	}

	return bus;

fail:
	brcmf_sdbrcm_release(bus);
	return NULL;
}

void brcmf_sdbrcm_disconnect(void *ptr)
{
	struct brcmf_sdio *bus = (struct brcmf_sdio *)ptr;

	brcmf_dbg(TRACE, "Enter\n");

	if (bus)
		brcmf_sdbrcm_release(bus);

	brcmf_dbg(TRACE, "Disconnected\n");
}

void
brcmf_sdbrcm_wd_timer(struct brcmf_sdio *bus, uint wdtick)
{
	
	if (!wdtick && bus->wd_timer_valid) {
		del_timer_sync(&bus->timer);
		bus->wd_timer_valid = false;
		bus->save_ms = wdtick;
		return;
	}

	
	if (bus->sdiodev->bus_if->state == BRCMF_BUS_DOWN)
		return;

	if (wdtick) {
		if (bus->save_ms != BRCMF_WD_POLL_MS) {
			if (bus->wd_timer_valid)
				
				del_timer_sync(&bus->timer);

			bus->timer.expires =
				jiffies + BRCMF_WD_POLL_MS * HZ / 1000;
			add_timer(&bus->timer);

		} else {
			
			mod_timer(&bus->timer,
				jiffies + BRCMF_WD_POLL_MS * HZ / 1000);
		}

		bus->wd_timer_valid = true;
		bus->save_ms = wdtick;
	}
}
