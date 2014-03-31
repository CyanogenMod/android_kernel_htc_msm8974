/*
 * Copyright (c) 2003-2008 Chelsio, Inc. All rights reserved.
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


#ifndef __T3_ADAPTER_H__
#define __T3_ADAPTER_H__

#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/cache.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include "t3cdev.h"
#include <asm/io.h>

struct adapter;
struct sge_qset;
struct port_info;

enum mac_idx_types {
	LAN_MAC_IDX	= 0,
	SAN_MAC_IDX,

	MAX_MAC_IDX
};

struct iscsi_config {
	__u8	mac_addr[ETH_ALEN];
	__u32	flags;
	int (*send)(struct port_info *pi, struct sk_buff **skb);
	int (*recv)(struct port_info *pi, struct sk_buff *skb);
};

struct port_info {
	struct adapter *adapter;
	struct sge_qset *qs;
	u8 port_id;
	u8 nqsets;
	u8 first_qset;
	struct cphy phy;
	struct cmac mac;
	struct link_config link_config;
	struct net_device_stats netstats;
	int activity;
	__be32 iscsi_ipv4addr;
	struct iscsi_config iscsic;

	int link_fault; 
};

enum {				
	FULL_INIT_DONE = (1 << 0),
	USING_MSI = (1 << 1),
	USING_MSIX = (1 << 2),
	QUEUES_BOUND = (1 << 3),
	TP_PARITY_INIT = (1 << 4),
	NAPI_INIT = (1 << 5),
};

struct fl_pg_chunk {
	struct page *page;
	void *va;
	unsigned int offset;
	unsigned long *p_cnt;
	dma_addr_t mapping;
};

struct rx_desc;
struct rx_sw_desc;

struct sge_fl {                     
	unsigned int buf_size;      
	unsigned int credits;       
	unsigned int pend_cred;     
	unsigned int size;          
	unsigned int cidx;          
	unsigned int pidx;          
	unsigned int gen;           
	struct fl_pg_chunk pg_chunk;
	unsigned int use_pages;     
	unsigned int order;	    
	unsigned int alloc_size;    
	struct rx_desc *desc;       
	struct rx_sw_desc *sdesc;   
	dma_addr_t   phys_addr;     
	unsigned int cntxt_id;      
	unsigned long empty;        
	unsigned long alloc_failed; 
};

# define RX_BUNDLE_SIZE 8

struct rsp_desc;

struct sge_rspq {		
	unsigned int credits;	
	unsigned int size;	
	unsigned int cidx;	
	unsigned int gen;	
	unsigned int polling;	
	unsigned int holdoff_tmr;	
	unsigned int next_holdoff;	
	unsigned int rx_recycle_buf; 
	struct rsp_desc *desc;	
	dma_addr_t phys_addr;	
	unsigned int cntxt_id;	
	spinlock_t lock;	
	struct sk_buff_head rx_queue; 
	struct sk_buff *pg_skb; 

	unsigned long offload_pkts;
	unsigned long offload_bundles;
	unsigned long eth_pkts;	
	unsigned long pure_rsps;	
	unsigned long imm_data;	
	unsigned long rx_drops;	
	unsigned long async_notif; 
	unsigned long empty;	
	unsigned long nomem;	
	unsigned long unhandled_irqs;	
	unsigned long starved;
	unsigned long restarted;
};

struct tx_desc;
struct tx_sw_desc;

struct sge_txq {		
	unsigned long flags;	
	unsigned int in_use;	
	unsigned int size;	
	unsigned int processed;	
	unsigned int cleaned;	
	unsigned int stop_thres;	
	unsigned int cidx;	
	unsigned int pidx;	
	unsigned int gen;	
	unsigned int unacked;	
	struct tx_desc *desc;	
	struct tx_sw_desc *sdesc;	
	spinlock_t lock;	
	unsigned int token;	
	dma_addr_t phys_addr;	
	struct sk_buff_head sendq;	
	struct tasklet_struct qresume_tsk;	
	unsigned int cntxt_id;	
	unsigned long stops;	
	unsigned long restarts;	
};

enum {				
	SGE_PSTAT_TSO,		
	SGE_PSTAT_RX_CSUM_GOOD,	
	SGE_PSTAT_TX_CSUM,	
	SGE_PSTAT_VLANEX,	
	SGE_PSTAT_VLANINS,	

	SGE_PSTAT_MAX		
};

struct napi_gro_fraginfo;

struct sge_qset {		
	struct adapter *adap;
	struct napi_struct napi;
	struct sge_rspq rspq;
	struct sge_fl fl[SGE_RXQ_PER_SET];
	struct sge_txq txq[SGE_TXQ_PER_SET];
	int nomem;
	void *lro_va;
	struct net_device *netdev;
	struct netdev_queue *tx_q;	
	unsigned long txq_stopped;	
	struct timer_list tx_reclaim_timer;	
	struct timer_list rx_reclaim_timer;	
	unsigned long port_stats[SGE_PSTAT_MAX];
} ____cacheline_aligned;

struct sge {
	struct sge_qset qs[SGE_QSETS];
	spinlock_t reg_lock;	
};

struct adapter {
	struct t3cdev tdev;
	struct list_head adapter_list;
	void __iomem *regs;
	struct pci_dev *pdev;
	unsigned long registered_device_map;
	unsigned long open_device_map;
	unsigned long flags;

	const char *name;
	int msg_enable;
	unsigned int mmio_len;

	struct adapter_params params;
	unsigned int slow_intr_mask;
	unsigned long irq_stats[IRQ_NUM_STATS];

	int msix_nvectors;
	struct {
		unsigned short vec;
		char desc[22];
	} msix_info[SGE_QSETS + 1];

	
	struct sge sge;
	struct mc7 pmrx;
	struct mc7 pmtx;
	struct mc7 cm;
	struct mc5 mc5;

	struct net_device *port[MAX_NPORTS];
	unsigned int check_task_cnt;
	struct delayed_work adap_check_task;
	struct work_struct ext_intr_handler_task;
	struct work_struct fatal_error_handler_task;
	struct work_struct link_fault_handler_task;

	struct work_struct db_full_task;
	struct work_struct db_empty_task;
	struct work_struct db_drop_task;

	struct dentry *debugfs_root;

	struct mutex mdio_lock;
	spinlock_t stats_lock;
	spinlock_t work_lock;

	struct sk_buff *nofail_skb;
};

static inline u32 t3_read_reg(struct adapter *adapter, u32 reg_addr)
{
	u32 val = readl(adapter->regs + reg_addr);

	CH_DBG(adapter, MMIO, "read register 0x%x value 0x%x\n", reg_addr, val);
	return val;
}

static inline void t3_write_reg(struct adapter *adapter, u32 reg_addr, u32 val)
{
	CH_DBG(adapter, MMIO, "setting register 0x%x to 0x%x\n", reg_addr, val);
	writel(val, adapter->regs + reg_addr);
}

static inline struct port_info *adap2pinfo(struct adapter *adap, int idx)
{
	return netdev_priv(adap->port[idx]);
}

static inline int phy2portid(struct cphy *phy)
{
	struct adapter *adap = phy->adapter;
	struct port_info *port0 = adap2pinfo(adap, 0);

	return &port0->phy == phy ? 0 : 1;
}

#define OFFLOAD_DEVMAP_BIT 15

#define tdev2adap(d) container_of(d, struct adapter, tdev)

static inline int offload_running(struct adapter *adapter)
{
	return test_bit(OFFLOAD_DEVMAP_BIT, &adapter->open_device_map);
}

int t3_offload_tx(struct t3cdev *tdev, struct sk_buff *skb);

void t3_os_ext_intr_handler(struct adapter *adapter);
void t3_os_link_changed(struct adapter *adapter, int port_id, int link_status,
			int speed, int duplex, int fc);
void t3_os_phymod_changed(struct adapter *adap, int port_id);
void t3_os_link_fault(struct adapter *adapter, int port_id, int state);
void t3_os_link_fault_handler(struct adapter *adapter, int port_id);

void t3_sge_start(struct adapter *adap);
void t3_sge_stop(struct adapter *adap);
void t3_start_sge_timers(struct adapter *adap);
void t3_stop_sge_timers(struct adapter *adap);
void t3_free_sge_resources(struct adapter *adap);
void t3_sge_err_intr_handler(struct adapter *adapter);
irq_handler_t t3_intr_handler(struct adapter *adap, int polling);
netdev_tx_t t3_eth_xmit(struct sk_buff *skb, struct net_device *dev);
int t3_mgmt_tx(struct adapter *adap, struct sk_buff *skb);
void t3_update_qset_coalesce(struct sge_qset *qs, const struct qset_params *p);
int t3_sge_alloc_qset(struct adapter *adapter, unsigned int id, int nports,
		      int irq_vec_idx, const struct qset_params *p,
		      int ntxq, struct net_device *dev,
		      struct netdev_queue *netdevq);
extern struct workqueue_struct *cxgb3_wq;

int t3_get_edc_fw(struct cphy *phy, int edc_idx, int size);

#endif				
