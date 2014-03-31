/*
 * This file is part of the Chelsio T4 Ethernet driver for Linux.
 *
 * Copyright (c) 2003-2010 Chelsio Communications, Inc. All rights reserved.
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

#ifndef __CXGB4_H__
#define __CXGB4_H__

#include <linux/bitops.h>
#include <linux/cache.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <asm/io.h>
#include "cxgb4_uld.h"
#include "t4_hw.h"

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 1
#define FW_VERSION_MICRO 0

enum {
	MAX_NPORTS = 4,     
	SERNUM_LEN = 24,    
	EC_LEN     = 16,    
	ID_LEN     = 16,    
};

enum {
	MEM_EDC0,
	MEM_EDC1,
	MEM_MC
};

enum dev_master {
	MASTER_CANT,
	MASTER_MAY,
	MASTER_MUST
};

enum dev_state {
	DEV_STATE_UNINIT,
	DEV_STATE_INIT,
	DEV_STATE_ERR
};

enum {
	PAUSE_RX      = 1 << 0,
	PAUSE_TX      = 1 << 1,
	PAUSE_AUTONEG = 1 << 2
};

struct port_stats {
	u64 tx_octets;            
	u64 tx_frames;            
	u64 tx_bcast_frames;      
	u64 tx_mcast_frames;      
	u64 tx_ucast_frames;      
	u64 tx_error_frames;      

	u64 tx_frames_64;         
	u64 tx_frames_65_127;
	u64 tx_frames_128_255;
	u64 tx_frames_256_511;
	u64 tx_frames_512_1023;
	u64 tx_frames_1024_1518;
	u64 tx_frames_1519_max;

	u64 tx_drop;              
	u64 tx_pause;             
	u64 tx_ppp0;              
	u64 tx_ppp1;              
	u64 tx_ppp2;              
	u64 tx_ppp3;              
	u64 tx_ppp4;              
	u64 tx_ppp5;              
	u64 tx_ppp6;              
	u64 tx_ppp7;              

	u64 rx_octets;            
	u64 rx_frames;            
	u64 rx_bcast_frames;      
	u64 rx_mcast_frames;      
	u64 rx_ucast_frames;      
	u64 rx_too_long;          
	u64 rx_jabber;            
	u64 rx_fcs_err;           
	u64 rx_len_err;           
	u64 rx_symbol_err;        
	u64 rx_runt;              

	u64 rx_frames_64;         
	u64 rx_frames_65_127;
	u64 rx_frames_128_255;
	u64 rx_frames_256_511;
	u64 rx_frames_512_1023;
	u64 rx_frames_1024_1518;
	u64 rx_frames_1519_max;

	u64 rx_pause;             
	u64 rx_ppp0;              
	u64 rx_ppp1;              
	u64 rx_ppp2;              
	u64 rx_ppp3;              
	u64 rx_ppp4;              
	u64 rx_ppp5;              
	u64 rx_ppp6;              
	u64 rx_ppp7;              

	u64 rx_ovflow0;           
	u64 rx_ovflow1;           
	u64 rx_ovflow2;           
	u64 rx_ovflow3;           
	u64 rx_trunc0;            
	u64 rx_trunc1;            
	u64 rx_trunc2;            
	u64 rx_trunc3;            
};

struct lb_port_stats {
	u64 octets;
	u64 frames;
	u64 bcast_frames;
	u64 mcast_frames;
	u64 ucast_frames;
	u64 error_frames;

	u64 frames_64;
	u64 frames_65_127;
	u64 frames_128_255;
	u64 frames_256_511;
	u64 frames_512_1023;
	u64 frames_1024_1518;
	u64 frames_1519_max;

	u64 drop;

	u64 ovflow0;
	u64 ovflow1;
	u64 ovflow2;
	u64 ovflow3;
	u64 trunc0;
	u64 trunc1;
	u64 trunc2;
	u64 trunc3;
};

struct tp_tcp_stats {
	u32 tcpOutRsts;
	u64 tcpInSegs;
	u64 tcpOutSegs;
	u64 tcpRetransSegs;
};

struct tp_err_stats {
	u32 macInErrs[4];
	u32 hdrInErrs[4];
	u32 tcpInErrs[4];
	u32 tnlCongDrops[4];
	u32 ofldChanDrops[4];
	u32 tnlTxDrops[4];
	u32 ofldVlanDrops[4];
	u32 tcp6InErrs[4];
	u32 ofldNoNeigh;
	u32 ofldCongDefer;
};

struct tp_params {
	unsigned int ntxchan;        
	unsigned int tre;            
};

struct vpd_params {
	unsigned int cclk;
	u8 ec[EC_LEN + 1];
	u8 sn[SERNUM_LEN + 1];
	u8 id[ID_LEN + 1];
};

struct pci_params {
	unsigned char speed;
	unsigned char width;
};

struct adapter_params {
	struct tp_params  tp;
	struct vpd_params vpd;
	struct pci_params pci;

	unsigned int sf_size;             
	unsigned int sf_nsec;             
	unsigned int sf_fw_start;         

	unsigned int fw_vers;
	unsigned int tp_vers;
	u8 api_vers[7];

	unsigned short mtus[NMTUS];
	unsigned short a_wnd[NCCTRL_WIN];
	unsigned short b_wnd[NCCTRL_WIN];

	unsigned char nports;             
	unsigned char portvec;
	unsigned char rev;                
	unsigned char offload;

	unsigned int ofldq_wr_cred;
};

struct trace_params {
	u32 data[TRACE_LEN / 4];
	u32 mask[TRACE_LEN / 4];
	unsigned short snap_len;
	unsigned short min_len;
	unsigned char skip_ofst;
	unsigned char skip_len;
	unsigned char invert;
	unsigned char port;
};

struct link_config {
	unsigned short supported;        
	unsigned short advertising;      
	unsigned short requested_speed;  
	unsigned short speed;            
	unsigned char  requested_fc;     
	unsigned char  fc;               
	unsigned char  autoneg;          
	unsigned char  link_ok;          
};

#define FW_LEN16(fw_struct) FW_CMD_LEN16(sizeof(fw_struct) / 16)

enum {
	MAX_ETH_QSETS = 32,           
	MAX_OFLD_QSETS = 16,          
	MAX_CTRL_QUEUES = NCHAN,      
	MAX_RDMA_QUEUES = NCHAN,      
};

enum {
	MAX_EGRQ = 128,         
	MAX_INGQ = 64           
};

struct adapter;
struct sge_rspq;

struct port_info {
	struct adapter *adapter;
	u16    viid;
	s16    xact_addr_filt;        
	u16    rss_size;              
	s8     mdio_addr;
	u8     port_type;
	u8     mod_type;
	u8     port_id;
	u8     tx_chan;
	u8     lport;                 
	u8     nqsets;                
	u8     first_qset;            
	u8     rss_mode;
	struct link_config link_cfg;
	u16   *rss;
};

struct dentry;
struct work_struct;

enum {                                 
	FULL_INIT_DONE     = (1 << 0),
	USING_MSI          = (1 << 1),
	USING_MSIX         = (1 << 2),
	FW_OK              = (1 << 4),
};

struct rx_sw_desc;

struct sge_fl {                     
	unsigned int avail;         
	unsigned int pend_cred;     
	unsigned int cidx;          
	unsigned int pidx;          
	unsigned long alloc_failed; 
	unsigned long large_alloc_failed;
	unsigned long starving;
	
	unsigned int cntxt_id;      
	unsigned int size;          
	struct rx_sw_desc *sdesc;   
	__be64 *desc;               
	dma_addr_t addr;            
};

struct pkt_gl {
	struct page_frag frags[MAX_SKB_FRAGS];
	void *va;                         
	unsigned int nfrags;              
	unsigned int tot_len;             
};

typedef int (*rspq_handler_t)(struct sge_rspq *q, const __be64 *rsp,
			      const struct pkt_gl *gl);

struct sge_rspq {                   
	struct napi_struct napi;
	const __be64 *cur_desc;     
	unsigned int cidx;          
	u8 gen;                     
	u8 intr_params;             
	u8 next_intr_params;        
	u8 pktcnt_idx;              
	u8 uld;                     
	u8 idx;                     
	int offset;                 
	u16 cntxt_id;               
	u16 abs_id;                 
	__be64 *desc;               
	dma_addr_t phys_addr;       
	unsigned int iqe_len;       
	unsigned int size;          
	struct adapter *adap;
	struct net_device *netdev;  
	rspq_handler_t handler;
};

struct sge_eth_stats {              
	unsigned long pkts;         
	unsigned long lro_pkts;     
	unsigned long lro_merged;   
	unsigned long rx_cso;       
	unsigned long vlan_ex;      
	unsigned long rx_drops;     
};

struct sge_eth_rxq {                
	struct sge_rspq rspq;
	struct sge_fl fl;
	struct sge_eth_stats stats;
} ____cacheline_aligned_in_smp;

struct sge_ofld_stats {             
	unsigned long pkts;         
	unsigned long imm;          
	unsigned long an;           
	unsigned long nomem;        
};

struct sge_ofld_rxq {               
	struct sge_rspq rspq;
	struct sge_fl fl;
	struct sge_ofld_stats stats;
} ____cacheline_aligned_in_smp;

struct tx_desc {
	__be64 flit[8];
};

struct tx_sw_desc;

struct sge_txq {
	unsigned int  in_use;       
	unsigned int  size;         
	unsigned int  cidx;         
	unsigned int  pidx;         
	unsigned long stops;        
	unsigned long restarts;     
	unsigned int  cntxt_id;     
	struct tx_desc *desc;       
	struct tx_sw_desc *sdesc;   
	struct sge_qstat *stat;     
	dma_addr_t    phys_addr;    
};

struct sge_eth_txq {                
	struct sge_txq q;
	struct netdev_queue *txq;   
	unsigned long tso;          
	unsigned long tx_cso;       
	unsigned long vlan_ins;     
	unsigned long mapping_err;  
} ____cacheline_aligned_in_smp;

struct sge_ofld_txq {               
	struct sge_txq q;
	struct adapter *adap;
	struct sk_buff_head sendq;  
	struct tasklet_struct qresume_tsk; 
	u8 full;                    
	unsigned long mapping_err;  
} ____cacheline_aligned_in_smp;

struct sge_ctrl_txq {               
	struct sge_txq q;
	struct adapter *adap;
	struct sk_buff_head sendq;  
	struct tasklet_struct qresume_tsk; 
	u8 full;                    
} ____cacheline_aligned_in_smp;

struct sge {
	struct sge_eth_txq ethtxq[MAX_ETH_QSETS];
	struct sge_ofld_txq ofldtxq[MAX_OFLD_QSETS];
	struct sge_ctrl_txq ctrlq[MAX_CTRL_QUEUES];

	struct sge_eth_rxq ethrxq[MAX_ETH_QSETS];
	struct sge_ofld_rxq ofldrxq[MAX_OFLD_QSETS];
	struct sge_ofld_rxq rdmarxq[MAX_RDMA_QUEUES];
	struct sge_rspq fw_evtq ____cacheline_aligned_in_smp;

	struct sge_rspq intrq ____cacheline_aligned_in_smp;
	spinlock_t intrq_lock;

	u16 max_ethqsets;           
	u16 ethqsets;               
	u16 ethtxq_rover;           
	u16 ofldqsets;              
	u16 rdmaqs;                 
	u16 ofld_rxq[MAX_OFLD_QSETS];
	u16 rdma_rxq[NCHAN];
	u16 timer_val[SGE_NTIMERS];
	u8 counter_val[SGE_NCOUNTERS];
	unsigned int starve_thres;
	u8 idma_state[2];
	unsigned int egr_start;
	unsigned int ingr_start;
	void *egr_map[MAX_EGRQ];    
	struct sge_rspq *ingr_map[MAX_INGQ]; 
	DECLARE_BITMAP(starving_fl, MAX_EGRQ);
	DECLARE_BITMAP(txq_maperr, MAX_EGRQ);
	struct timer_list rx_timer; 
	struct timer_list tx_timer; 
};

#define for_each_ethrxq(sge, i) for (i = 0; i < (sge)->ethqsets; i++)
#define for_each_ofldrxq(sge, i) for (i = 0; i < (sge)->ofldqsets; i++)
#define for_each_rdmarxq(sge, i) for (i = 0; i < (sge)->rdmaqs; i++)

struct l2t_data;

struct adapter {
	void __iomem *regs;
	struct pci_dev *pdev;
	struct device *pdev_dev;
	unsigned int fn;
	unsigned int flags;

	int msg_enable;

	struct adapter_params params;
	struct cxgb4_virt_res vres;
	unsigned int swintr;

	unsigned int wol;

	struct {
		unsigned short vec;
		char desc[IFNAMSIZ + 10];
	} msix_info[MAX_INGQ + 1];

	struct sge sge;

	struct net_device *port[MAX_NPORTS];
	u8 chan_map[NCHAN];                   

	struct l2t_data *l2t;
	void *uld_handle[CXGB4_ULD_MAX];
	struct list_head list_node;

	struct tid_info tids;
	void **tid_release_head;
	spinlock_t tid_release_lock;
	struct work_struct tid_release_task;
	bool tid_release_task_busy;

	struct dentry *debugfs_root;

	spinlock_t stats_lock;
};

static inline u32 t4_read_reg(struct adapter *adap, u32 reg_addr)
{
	return readl(adap->regs + reg_addr);
}

static inline void t4_write_reg(struct adapter *adap, u32 reg_addr, u32 val)
{
	writel(val, adap->regs + reg_addr);
}

#ifndef readq
static inline u64 readq(const volatile void __iomem *addr)
{
	return readl(addr) + ((u64)readl(addr + 4) << 32);
}

static inline void writeq(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val >> 32, addr + 4);
}
#endif

static inline u64 t4_read_reg64(struct adapter *adap, u32 reg_addr)
{
	return readq(adap->regs + reg_addr);
}

static inline void t4_write_reg64(struct adapter *adap, u32 reg_addr, u64 val)
{
	writeq(val, adap->regs + reg_addr);
}

static inline struct port_info *netdev2pinfo(const struct net_device *dev)
{
	return netdev_priv(dev);
}

static inline struct port_info *adap2pinfo(struct adapter *adap, int idx)
{
	return netdev_priv(adap->port[idx]);
}

static inline struct adapter *netdev2adap(const struct net_device *dev)
{
	return netdev2pinfo(dev)->adapter;
}

void t4_os_portmod_changed(const struct adapter *adap, int port_id);
void t4_os_link_changed(struct adapter *adap, int port_id, int link_stat);

void *t4_alloc_mem(size_t size);

void t4_free_sge_resources(struct adapter *adap);
irq_handler_t t4_intr_handler(struct adapter *adap);
netdev_tx_t t4_eth_xmit(struct sk_buff *skb, struct net_device *dev);
int t4_ethrx_handler(struct sge_rspq *q, const __be64 *rsp,
		     const struct pkt_gl *gl);
int t4_mgmt_tx(struct adapter *adap, struct sk_buff *skb);
int t4_ofld_send(struct adapter *adap, struct sk_buff *skb);
int t4_sge_alloc_rxq(struct adapter *adap, struct sge_rspq *iq, bool fwevtq,
		     struct net_device *dev, int intr_idx,
		     struct sge_fl *fl, rspq_handler_t hnd);
int t4_sge_alloc_eth_txq(struct adapter *adap, struct sge_eth_txq *txq,
			 struct net_device *dev, struct netdev_queue *netdevq,
			 unsigned int iqid);
int t4_sge_alloc_ctrl_txq(struct adapter *adap, struct sge_ctrl_txq *txq,
			  struct net_device *dev, unsigned int iqid,
			  unsigned int cmplqid);
int t4_sge_alloc_ofld_txq(struct adapter *adap, struct sge_ofld_txq *txq,
			  struct net_device *dev, unsigned int iqid);
irqreturn_t t4_sge_intr_msix(int irq, void *cookie);
void t4_sge_init(struct adapter *adap);
void t4_sge_start(struct adapter *adap);
void t4_sge_stop(struct adapter *adap);

#define for_each_port(adapter, iter) \
	for (iter = 0; iter < (adapter)->params.nports; ++iter)

static inline unsigned int core_ticks_per_usec(const struct adapter *adap)
{
	return adap->params.vpd.cclk / 1000;
}

static inline unsigned int us_to_core_ticks(const struct adapter *adap,
					    unsigned int us)
{
	return (us * adap->params.vpd.cclk) / 1000;
}

void t4_set_reg_field(struct adapter *adap, unsigned int addr, u32 mask,
		      u32 val);

int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok);

static inline int t4_wr_mbox(struct adapter *adap, int mbox, const void *cmd,
			     int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, true);
}

static inline int t4_wr_mbox_ns(struct adapter *adap, int mbox, const void *cmd,
				int size, void *rpl)
{
	return t4_wr_mbox_meat(adap, mbox, cmd, size, rpl, false);
}

void t4_intr_enable(struct adapter *adapter);
void t4_intr_disable(struct adapter *adapter);
int t4_slow_intr_handler(struct adapter *adapter);

int t4_wait_dev_ready(struct adapter *adap);
int t4_link_start(struct adapter *adap, unsigned int mbox, unsigned int port,
		  struct link_config *lc);
int t4_restart_aneg(struct adapter *adap, unsigned int mbox, unsigned int port);
int t4_seeprom_wp(struct adapter *adapter, bool enable);
int t4_load_fw(struct adapter *adapter, const u8 *fw_data, unsigned int size);
int t4_check_fw_version(struct adapter *adapter);
int t4_prep_adapter(struct adapter *adapter);
int t4_port_init(struct adapter *adap, int mbox, int pf, int vf);
void t4_fatal_err(struct adapter *adapter);
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq);
int t4_config_glbl_rss(struct adapter *adapter, int mbox, unsigned int mode,
		       unsigned int flags);
int t4_mc_read(struct adapter *adap, u32 addr, __be32 *data, u64 *parity);
int t4_edc_read(struct adapter *adap, int idx, u32 addr, __be32 *data,
		u64 *parity);

void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p);
void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log);
void t4_tp_get_tcp_stats(struct adapter *adap, struct tp_tcp_stats *v4,
			 struct tp_tcp_stats *v6);
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta);

void t4_wol_magic_enable(struct adapter *adap, unsigned int port,
			 const u8 *addr);
int t4_wol_pat_enable(struct adapter *adap, unsigned int port, unsigned int map,
		      u64 mask0, u64 mask1, unsigned int crc, bool enable);

int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state);
int t4_fw_bye(struct adapter *adap, unsigned int mbox);
int t4_early_init(struct adapter *adap, unsigned int mbox);
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset);
int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val);
int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val);
int t4_cfg_pfvf(struct adapter *adap, unsigned int mbox, unsigned int pf,
		unsigned int vf, unsigned int txq, unsigned int txq_eth_ctrl,
		unsigned int rxqi, unsigned int rxq, unsigned int tc,
		unsigned int vi, unsigned int cmask, unsigned int pmask,
		unsigned int nexact, unsigned int rcaps, unsigned int wxcaps);
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		unsigned int *rss_size);
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		int mtu, int promisc, int all_multi, int bcast, int vlanex,
		bool sleep_ok);
int t4_alloc_mac_filt(struct adapter *adap, unsigned int mbox,
		      unsigned int viid, bool free, unsigned int naddr,
		      const u8 **addr, u16 *idx, u64 *hash, bool sleep_ok);
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, bool add_smt);
int t4_set_addr_hash(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     bool ucast, u64 vec, bool sleep_ok);
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en);
int t4_identify_port(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     unsigned int nblinks);
int t4_mdio_rd(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, u16 *valp);
int t4_mdio_wr(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, u16 val);
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id);
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid);
int t4_ctrl_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_ofld_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid);
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl);
#endif 
