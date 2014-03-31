/*
 * Copyright (c) 2005-2008 Chelsio, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __CHELSIO_COMMON_H
#define __CHELSIO_COMMON_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mdio.h>
#include "version.h"

#define CH_ERR(adap, fmt, ...)   dev_err(&adap->pdev->dev, fmt, ## __VA_ARGS__)
#define CH_WARN(adap, fmt, ...)  dev_warn(&adap->pdev->dev, fmt, ## __VA_ARGS__)
#define CH_ALERT(adap, fmt, ...) \
	dev_printk(KERN_ALERT, &adap->pdev->dev, fmt, ## __VA_ARGS__)

#define CH_MSG(adapter, level, category, fmt, ...) do { \
	if ((adapter)->msg_enable & NETIF_MSG_##category) \
		dev_printk(KERN_##level, &adapter->pdev->dev, fmt, \
			   ## __VA_ARGS__); \
} while (0)

#ifdef DEBUG
# define CH_DBG(adapter, category, fmt, ...) \
	CH_MSG(adapter, DEBUG, category, fmt, ## __VA_ARGS__)
#else
# define CH_DBG(adapter, category, fmt, ...)
#endif

#define NETIF_MSG_MMIO 0x8000000

enum {
	MAX_NPORTS = 2,		
	MAX_FRAME_SIZE = 10240,	
	EEPROMSIZE = 8192,	
	SERNUM_LEN     = 16,    
	RSS_TABLE_SIZE = 64,	
	TCB_SIZE = 128,		
	NMTUS = 16,		
	NCCTRL_WIN = 32,	
	PROTO_SRAM_LINES = 128, 
};

#define MAX_RX_COALESCING_LEN 12288U

enum {
	PAUSE_RX = 1 << 0,
	PAUSE_TX = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};

enum {
	SUPPORTED_IRQ      = 1 << 24
};

enum {				
	STAT_ULP_CH0_PBL_OOB,
	STAT_ULP_CH1_PBL_OOB,
	STAT_PCI_CORR_ECC,

	IRQ_NUM_STATS		
};

#define TP_VERSION_MAJOR	1
#define TP_VERSION_MINOR	1
#define TP_VERSION_MICRO	0

#define S_TP_VERSION_MAJOR		16
#define M_TP_VERSION_MAJOR		0xFF
#define V_TP_VERSION_MAJOR(x)		((x) << S_TP_VERSION_MAJOR)
#define G_TP_VERSION_MAJOR(x)		\
	    (((x) >> S_TP_VERSION_MAJOR) & M_TP_VERSION_MAJOR)

#define S_TP_VERSION_MINOR		8
#define M_TP_VERSION_MINOR		0xFF
#define V_TP_VERSION_MINOR(x)		((x) << S_TP_VERSION_MINOR)
#define G_TP_VERSION_MINOR(x)		\
	    (((x) >> S_TP_VERSION_MINOR) & M_TP_VERSION_MINOR)

#define S_TP_VERSION_MICRO		0
#define M_TP_VERSION_MICRO		0xFF
#define V_TP_VERSION_MICRO(x)		((x) << S_TP_VERSION_MICRO)
#define G_TP_VERSION_MICRO(x)		\
	    (((x) >> S_TP_VERSION_MICRO) & M_TP_VERSION_MICRO)

enum {
	SGE_QSETS = 8,		
	SGE_RXQ_PER_SET = 2,	
	SGE_TXQ_PER_SET = 3	
};

enum sge_context_type {		
	SGE_CNTXT_RDMA = 0,
	SGE_CNTXT_ETH = 2,
	SGE_CNTXT_OFLD = 4,
	SGE_CNTXT_CTRL = 5
};

enum {
	AN_PKT_SIZE = 32,	
	IMMED_PKT_SIZE = 48	
};

struct sg_ent {			
	__be32 len[2];
	__be64 addr[2];
};

#ifndef SGE_NUM_GENBITS
# define SGE_NUM_GENBITS 2
#endif

#define TX_DESC_FLITS 16U
#define WR_FLITS (TX_DESC_FLITS + 1 - SGE_NUM_GENBITS)

struct cphy;
struct adapter;

struct mdio_ops {
	int (*read)(struct net_device *dev, int phy_addr, int mmd_addr,
		    u16 reg_addr);
	int (*write)(struct net_device *dev, int phy_addr, int mmd_addr,
		     u16 reg_addr, u16 val);
	unsigned mode_support;
};

struct adapter_info {
	unsigned char nports0;        
	unsigned char nports1;        
	unsigned char phy_base_addr;	
	unsigned int gpio_out;	
	unsigned char gpio_intr[MAX_NPORTS]; 
	unsigned long caps;	
	const struct mdio_ops *mdio_ops;	
	const char *desc;	
};

struct mc5_stats {
	unsigned long parity_err;
	unsigned long active_rgn_full;
	unsigned long nfa_srch_err;
	unsigned long unknown_cmd;
	unsigned long reqq_parity_err;
	unsigned long dispq_parity_err;
	unsigned long del_act_empty;
};

struct mc7_stats {
	unsigned long corr_err;
	unsigned long uncorr_err;
	unsigned long parity_err;
	unsigned long addr_err;
};

struct mac_stats {
	u64 tx_octets;		
	u64 tx_octets_bad;	
	u64 tx_frames;		
	u64 tx_mcast_frames;	
	u64 tx_bcast_frames;	
	u64 tx_pause;		
	u64 tx_deferred;	
	u64 tx_late_collisions;	
	u64 tx_total_collisions;	
	u64 tx_excess_collisions;	
	u64 tx_underrun;	
	u64 tx_len_errs;	
	u64 tx_mac_internal_errs;	
	u64 tx_excess_deferral;	
	u64 tx_fcs_errs;	

	u64 tx_frames_64;	
	u64 tx_frames_65_127;
	u64 tx_frames_128_255;
	u64 tx_frames_256_511;
	u64 tx_frames_512_1023;
	u64 tx_frames_1024_1518;
	u64 tx_frames_1519_max;

	u64 rx_octets;		
	u64 rx_octets_bad;	
	u64 rx_frames;		
	u64 rx_mcast_frames;	
	u64 rx_bcast_frames;	
	u64 rx_pause;		
	u64 rx_fcs_errs;	
	u64 rx_align_errs;	
	u64 rx_symbol_errs;	
	u64 rx_data_errs;	
	u64 rx_sequence_errs;	
	u64 rx_runt;		
	u64 rx_jabber;		
	u64 rx_short;		
	u64 rx_too_long;	
	u64 rx_mac_internal_errs;	

	u64 rx_frames_64;	
	u64 rx_frames_65_127;
	u64 rx_frames_128_255;
	u64 rx_frames_256_511;
	u64 rx_frames_512_1023;
	u64 rx_frames_1024_1518;
	u64 rx_frames_1519_max;

	u64 rx_cong_drops;	

	unsigned long tx_fifo_parity_err;
	unsigned long rx_fifo_parity_err;
	unsigned long tx_fifo_urun;
	unsigned long rx_fifo_ovfl;
	unsigned long serdes_signal_loss;
	unsigned long xaui_pcs_ctc_err;
	unsigned long xaui_pcs_align_change;

	unsigned long num_toggled; 
	unsigned long num_resets;  

	unsigned long link_faults;  
};

struct tp_mib_stats {
	u32 ipInReceive_hi;
	u32 ipInReceive_lo;
	u32 ipInHdrErrors_hi;
	u32 ipInHdrErrors_lo;
	u32 ipInAddrErrors_hi;
	u32 ipInAddrErrors_lo;
	u32 ipInUnknownProtos_hi;
	u32 ipInUnknownProtos_lo;
	u32 ipInDiscards_hi;
	u32 ipInDiscards_lo;
	u32 ipInDelivers_hi;
	u32 ipInDelivers_lo;
	u32 ipOutRequests_hi;
	u32 ipOutRequests_lo;
	u32 ipOutDiscards_hi;
	u32 ipOutDiscards_lo;
	u32 ipOutNoRoutes_hi;
	u32 ipOutNoRoutes_lo;
	u32 ipReasmTimeout;
	u32 ipReasmReqds;
	u32 ipReasmOKs;
	u32 ipReasmFails;

	u32 reserved[8];

	u32 tcpActiveOpens;
	u32 tcpPassiveOpens;
	u32 tcpAttemptFails;
	u32 tcpEstabResets;
	u32 tcpOutRsts;
	u32 tcpCurrEstab;
	u32 tcpInSegs_hi;
	u32 tcpInSegs_lo;
	u32 tcpOutSegs_hi;
	u32 tcpOutSegs_lo;
	u32 tcpRetransSeg_hi;
	u32 tcpRetransSeg_lo;
	u32 tcpInErrs_hi;
	u32 tcpInErrs_lo;
	u32 tcpRtoMin;
	u32 tcpRtoMax;
};

struct tp_params {
	unsigned int nchan;	
	unsigned int pmrx_size;	
	unsigned int pmtx_size;	
	unsigned int cm_size;	
	unsigned int chan_rx_size;	
	unsigned int chan_tx_size;	
	unsigned int rx_pg_size;	
	unsigned int tx_pg_size;	
	unsigned int rx_num_pgs;	
	unsigned int tx_num_pgs;	
	unsigned int ntimer_qs;	
};

struct qset_params {		
	unsigned int polling;	
	unsigned int coalesce_usecs;	
	unsigned int rspq_size;	
	unsigned int fl_size;	
	unsigned int jumbo_size;	
	unsigned int txq_size[SGE_TXQ_PER_SET];	
	unsigned int cong_thres;	
	unsigned int vector;		
};

struct sge_params {
	unsigned int max_pkt_size;	
	struct qset_params qset[SGE_QSETS];
};

struct mc5_params {
	unsigned int mode;	
	unsigned int nservers;	
	unsigned int nfilters;	
	unsigned int nroutes;	
};

enum {
	DEFAULT_NSERVERS = 512,
	DEFAULT_NFILTERS = 128
};

enum {
	MC5_MODE_144_BIT = 1,
	MC5_MODE_72_BIT = 2
};

enum { MC5_MIN_TIDS = 16 };

struct vpd_params {
	unsigned int cclk;
	unsigned int mclk;
	unsigned int uclk;
	unsigned int mdc;
	unsigned int mem_timing;
	u8 sn[SERNUM_LEN + 1];
	u8 eth_base[6];
	u8 port_type[MAX_NPORTS];
	unsigned short xauicfg[2];
};

struct pci_params {
	unsigned int vpd_cap_addr;
	unsigned short speed;
	unsigned char width;
	unsigned char variant;
};

enum {
	PCI_VARIANT_PCI,
	PCI_VARIANT_PCIX_MODE1_PARITY,
	PCI_VARIANT_PCIX_MODE1_ECC,
	PCI_VARIANT_PCIX_266_MODE2,
	PCI_VARIANT_PCIE
};

struct adapter_params {
	struct sge_params sge;
	struct mc5_params mc5;
	struct tp_params tp;
	struct vpd_params vpd;
	struct pci_params pci;

	const struct adapter_info *info;

	unsigned short mtus[NMTUS];
	unsigned short a_wnd[NCCTRL_WIN];
	unsigned short b_wnd[NCCTRL_WIN];

	unsigned int nports;	
	unsigned int chan_map;  
	unsigned int stats_update_period;	
	unsigned int linkpoll_period;	
	unsigned int rev;	
	unsigned int offload;
};

enum {					    
	T3_REV_A  = 0,
	T3_REV_B  = 2,
	T3_REV_B2 = 3,
	T3_REV_C  = 4,
};

struct trace_params {
	u32 sip;
	u32 sip_mask;
	u32 dip;
	u32 dip_mask;
	u16 sport;
	u16 sport_mask;
	u16 dport;
	u16 dport_mask;
	u32 vlan:12;
	u32 vlan_mask:12;
	u32 intf:4;
	u32 intf_mask:4;
	u8 proto;
	u8 proto_mask;
};

struct link_config {
	unsigned int supported;	
	unsigned int advertising;	
	unsigned short requested_speed;	
	unsigned short speed;	
	unsigned char requested_duplex;	
	unsigned char duplex;	
	unsigned char requested_fc;	
	unsigned char fc;	
	unsigned char autoneg;	
	unsigned int link_ok;	
};

#define SPEED_INVALID   0xffff
#define DUPLEX_INVALID  0xff

struct mc5 {
	struct adapter *adapter;
	unsigned int tcam_size;
	unsigned char part_type;
	unsigned char parity_enabled;
	unsigned char mode;
	struct mc5_stats stats;
};

static inline unsigned int t3_mc5_size(const struct mc5 *p)
{
	return p->tcam_size;
}

struct mc7 {
	struct adapter *adapter;	
	unsigned int size;	
	unsigned int width;	
	unsigned int offset;	
	const char *name;	
	struct mc7_stats stats;	
};

static inline unsigned int t3_mc7_size(const struct mc7 *p)
{
	return p->size;
}

struct cmac {
	struct adapter *adapter;
	unsigned int offset;
	unsigned int nucast;	
	unsigned int tx_tcnt;
	unsigned int tx_xcnt;
	u64 tx_mcnt;
	unsigned int rx_xcnt;
	unsigned int rx_ocnt;
	u64 rx_mcnt;
	unsigned int toggle_cnt;
	unsigned int txen;
	u64 rx_pause;
	struct mac_stats stats;
};

enum {
	MAC_DIRECTION_RX = 1,
	MAC_DIRECTION_TX = 2,
	MAC_RXFIFO_SIZE = 32768
};

enum {
	PHY_LOOPBACK_TX = 1,
	PHY_LOOPBACK_RX = 2
};

enum {
	cphy_cause_link_change = 1,
	cphy_cause_fifo_error = 2,
	cphy_cause_module_change = 4,
};

enum {
	phy_modtype_none,
	phy_modtype_sr,
	phy_modtype_lr,
	phy_modtype_lrm,
	phy_modtype_twinax,
	phy_modtype_twinax_long,
	phy_modtype_unknown
};

struct cphy_ops {
	int (*reset)(struct cphy *phy, int wait);

	int (*intr_enable)(struct cphy *phy);
	int (*intr_disable)(struct cphy *phy);
	int (*intr_clear)(struct cphy *phy);
	int (*intr_handler)(struct cphy *phy);

	int (*autoneg_enable)(struct cphy *phy);
	int (*autoneg_restart)(struct cphy *phy);

	int (*advertise)(struct cphy *phy, unsigned int advertise_map);
	int (*set_loopback)(struct cphy *phy, int mmd, int dir, int enable);
	int (*set_speed_duplex)(struct cphy *phy, int speed, int duplex);
	int (*get_link_status)(struct cphy *phy, int *link_ok, int *speed,
			       int *duplex, int *fc);
	int (*power_down)(struct cphy *phy, int enable);

	u32 mmds;
};
enum {
	EDC_OPT_AEL2005 = 0,
	EDC_OPT_AEL2005_SIZE = 1084,
	EDC_TWX_AEL2005 = 1,
	EDC_TWX_AEL2005_SIZE = 1464,
	EDC_TWX_AEL2020 = 2,
	EDC_TWX_AEL2020_SIZE = 1628,
	EDC_MAX_SIZE = EDC_TWX_AEL2020_SIZE, 
};

struct cphy {
	u8 modtype;			
	short priv;			
	unsigned int caps;		
	struct adapter *adapter;	
	const char *desc;		
	unsigned long fifo_errors;	
	const struct cphy_ops *ops;	
	struct mdio_if_info mdio;
	u16 phy_cache[EDC_MAX_SIZE];	
};

static inline int t3_mdio_read(struct cphy *phy, int mmd, int reg,
			       unsigned int *valp)
{
	int rc = phy->mdio.mdio_read(phy->mdio.dev, phy->mdio.prtad, mmd, reg);
	*valp = (rc >= 0) ? rc : -1;
	return (rc >= 0) ? 0 : rc;
}

static inline int t3_mdio_write(struct cphy *phy, int mmd, int reg,
				unsigned int val)
{
	return phy->mdio.mdio_write(phy->mdio.dev, phy->mdio.prtad, mmd,
				    reg, val);
}

static inline void cphy_init(struct cphy *phy, struct adapter *adapter,
			     int phy_addr, struct cphy_ops *phy_ops,
			     const struct mdio_ops *mdio_ops,
			      unsigned int caps, const char *desc)
{
	phy->caps = caps;
	phy->adapter = adapter;
	phy->desc = desc;
	phy->ops = phy_ops;
	if (mdio_ops) {
		phy->mdio.prtad = phy_addr;
		phy->mdio.mmds = phy_ops->mmds;
		phy->mdio.mode_support = mdio_ops->mode_support;
		phy->mdio.mdio_read = mdio_ops->read;
		phy->mdio.mdio_write = mdio_ops->write;
	}
}

#define MAC_STATS_ACCUM_SECS 180

#define XGM_REG(reg_addr, idx) \
	((reg_addr) + (idx) * (XGMAC0_1_BASE_ADDR - XGMAC0_0_BASE_ADDR))

struct addr_val_pair {
	unsigned int reg_addr;
	unsigned int val;
};

#include "adapter.h"

#ifndef PCI_VENDOR_ID_CHELSIO
# define PCI_VENDOR_ID_CHELSIO 0x1425
#endif

#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; ++iter)

#define adapter_info(adap) ((adap)->params.info)

static inline int uses_xaui(const struct adapter *adap)
{
	return adapter_info(adap)->caps & SUPPORTED_AUI;
}

static inline int is_10G(const struct adapter *adap)
{
	return adapter_info(adap)->caps & SUPPORTED_10000baseT_Full;
}

static inline int is_offload(const struct adapter *adap)
{
	return adap->params.offload;
}

static inline unsigned int core_ticks_per_usec(const struct adapter *adap)
{
	return adap->params.vpd.cclk / 1000;
}

static inline unsigned int is_pcie(const struct adapter *adap)
{
	return adap->params.pci.variant == PCI_VARIANT_PCIE;
}

void t3_set_reg_field(struct adapter *adap, unsigned int addr, u32 mask,
		      u32 val);
void t3_write_regs(struct adapter *adapter, const struct addr_val_pair *p,
		   int n, unsigned int offset);
int t3_wait_op_done_val(struct adapter *adapter, int reg, u32 mask,
			int polarity, int attempts, int delay, u32 *valp);
static inline int t3_wait_op_done(struct adapter *adapter, int reg, u32 mask,
				  int polarity, int attempts, int delay)
{
	return t3_wait_op_done_val(adapter, reg, mask, polarity, attempts,
				   delay, NULL);
}
int t3_mdio_change_bits(struct cphy *phy, int mmd, int reg, unsigned int clear,
			unsigned int set);
int t3_phy_reset(struct cphy *phy, int mmd, int wait);
int t3_phy_advertise(struct cphy *phy, unsigned int advert);
int t3_phy_advertise_fiber(struct cphy *phy, unsigned int advert);
int t3_set_phy_speed_duplex(struct cphy *phy, int speed, int duplex);
int t3_phy_lasi_intr_enable(struct cphy *phy);
int t3_phy_lasi_intr_disable(struct cphy *phy);
int t3_phy_lasi_intr_clear(struct cphy *phy);
int t3_phy_lasi_intr_handler(struct cphy *phy);

void t3_intr_enable(struct adapter *adapter);
void t3_intr_disable(struct adapter *adapter);
void t3_intr_clear(struct adapter *adapter);
void t3_xgm_intr_enable(struct adapter *adapter, int idx);
void t3_xgm_intr_disable(struct adapter *adapter, int idx);
void t3_port_intr_enable(struct adapter *adapter, int idx);
void t3_port_intr_disable(struct adapter *adapter, int idx);
int t3_slow_intr_handler(struct adapter *adapter);
int t3_phy_intr_handler(struct adapter *adapter);

void t3_link_changed(struct adapter *adapter, int port_id);
void t3_link_fault(struct adapter *adapter, int port_id);
int t3_link_start(struct cphy *phy, struct cmac *mac, struct link_config *lc);
const struct adapter_info *t3_get_adapter_info(unsigned int board_id);
int t3_seeprom_read(struct adapter *adapter, u32 addr, __le32 *data);
int t3_seeprom_write(struct adapter *adapter, u32 addr, __le32 data);
int t3_seeprom_wp(struct adapter *adapter, int enable);
int t3_get_tp_version(struct adapter *adapter, u32 *vers);
int t3_check_tpsram_version(struct adapter *adapter);
int t3_check_tpsram(struct adapter *adapter, const u8 *tp_ram,
		    unsigned int size);
int t3_set_proto_sram(struct adapter *adap, const u8 *data);
int t3_load_fw(struct adapter *adapter, const u8 * fw_data, unsigned int size);
int t3_get_fw_version(struct adapter *adapter, u32 *vers);
int t3_check_fw_version(struct adapter *adapter);
int t3_init_hw(struct adapter *adapter, u32 fw_params);
int t3_reset_adapter(struct adapter *adapter);
int t3_prep_adapter(struct adapter *adapter, const struct adapter_info *ai,
		    int reset);
int t3_replay_prep_adapter(struct adapter *adapter);
void t3_led_ready(struct adapter *adapter);
void t3_fatal_err(struct adapter *adapter);
void t3_set_vlan_accel(struct adapter *adapter, unsigned int ports, int on);
void t3_config_rss(struct adapter *adapter, unsigned int rss_config,
		   const u8 * cpus, const u16 *rspq);
int t3_cim_ctl_blk_read(struct adapter *adap, unsigned int addr,
			unsigned int n, unsigned int *valp);
int t3_mc7_bd_read(struct mc7 *mc7, unsigned int start, unsigned int n,
		   u64 *buf);

int t3_mac_reset(struct cmac *mac);
void t3b_pcs_reset(struct cmac *mac);
void t3_mac_disable_exact_filters(struct cmac *mac);
void t3_mac_enable_exact_filters(struct cmac *mac);
int t3_mac_enable(struct cmac *mac, int which);
int t3_mac_disable(struct cmac *mac, int which);
int t3_mac_set_mtu(struct cmac *mac, unsigned int mtu);
int t3_mac_set_rx_mode(struct cmac *mac, struct net_device *dev);
int t3_mac_set_address(struct cmac *mac, unsigned int idx, u8 addr[6]);
int t3_mac_set_num_ucast(struct cmac *mac, int n);
const struct mac_stats *t3_mac_update_stats(struct cmac *mac);
int t3_mac_set_speed_duplex_fc(struct cmac *mac, int speed, int duplex, int fc);
int t3b2_mac_watchdog_task(struct cmac *mac);

void t3_mc5_prep(struct adapter *adapter, struct mc5 *mc5, int mode);
int t3_mc5_init(struct mc5 *mc5, unsigned int nservers, unsigned int nfilters,
		unsigned int nroutes);
void t3_mc5_intr_handler(struct mc5 *mc5);

void t3_tp_set_offload_mode(struct adapter *adap, int enable);
void t3_tp_get_mib_stats(struct adapter *adap, struct tp_mib_stats *tps);
void t3_load_mtus(struct adapter *adap, unsigned short mtus[NMTUS],
		  unsigned short alpha[NCCTRL_WIN],
		  unsigned short beta[NCCTRL_WIN], unsigned short mtu_cap);
void t3_config_trace_filter(struct adapter *adapter,
			    const struct trace_params *tp, int filter_index,
			    int invert, int enable);
int t3_config_sched(struct adapter *adap, unsigned int kbps, int sched);

void t3_sge_prep(struct adapter *adap, struct sge_params *p);
void t3_sge_init(struct adapter *adap, struct sge_params *p);
int t3_sge_init_ecntxt(struct adapter *adapter, unsigned int id, int gts_enable,
		       enum sge_context_type type, int respq, u64 base_addr,
		       unsigned int size, unsigned int token, int gen,
		       unsigned int cidx);
int t3_sge_init_flcntxt(struct adapter *adapter, unsigned int id,
			int gts_enable, u64 base_addr, unsigned int size,
			unsigned int esize, unsigned int cong_thres, int gen,
			unsigned int cidx);
int t3_sge_init_rspcntxt(struct adapter *adapter, unsigned int id,
			 int irq_vec_idx, u64 base_addr, unsigned int size,
			 unsigned int fl_thres, int gen, unsigned int cidx);
int t3_sge_init_cqcntxt(struct adapter *adapter, unsigned int id, u64 base_addr,
			unsigned int size, int rspq, int ovfl_mode,
			unsigned int credits, unsigned int credit_thres);
int t3_sge_enable_ecntxt(struct adapter *adapter, unsigned int id, int enable);
int t3_sge_disable_fl(struct adapter *adapter, unsigned int id);
int t3_sge_disable_rspcntxt(struct adapter *adapter, unsigned int id);
int t3_sge_disable_cqcntxt(struct adapter *adapter, unsigned int id);
int t3_sge_cqcntxt_op(struct adapter *adapter, unsigned int id, unsigned int op,
		      unsigned int credits);

int t3_vsc8211_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops);
int t3_ael1002_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops);
int t3_ael1006_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops);
int t3_ael2005_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops);
int t3_ael2020_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops);
int t3_qt2045_phy_prep(struct cphy *phy, struct adapter *adapter, int phy_addr,
		       const struct mdio_ops *mdio_ops);
int t3_xaui_direct_phy_prep(struct cphy *phy, struct adapter *adapter,
			    int phy_addr, const struct mdio_ops *mdio_ops);
int t3_aq100x_phy_prep(struct cphy *phy, struct adapter *adapter,
			    int phy_addr, const struct mdio_ops *mdio_ops);
#endif				
