/*
 *  linux/drivers/net/ethernet/ibm/ehea/ehea.h
 *
 *  eHEA ethernet device driver for IBM eServer System p
 *
 *  (C) Copyright IBM Corp. 2006
 *
 *  Authors:
 *       Christoph Raisch <raisch@de.ibm.com>
 *       Jan-Bernd Themann <themann@de.ibm.com>
 *       Thomas Klein <tklein@de.ibm.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __EHEA_H__
#define __EHEA_H__

#include <linux/module.h>
#include <linux/ethtool.h>
#include <linux/vmalloc.h>
#include <linux/if_vlan.h>

#include <asm/ibmebus.h>
#include <asm/abs_addr.h>
#include <asm/io.h>

#define DRV_NAME	"ehea"
#define DRV_VERSION	"EHEA_0107"

#define DLPAR_PORT_ADD_REM 1
#define DLPAR_MEM_ADD      2
#define DLPAR_MEM_REM      4
#define EHEA_CAPABILITIES  (DLPAR_PORT_ADD_REM | DLPAR_MEM_ADD | DLPAR_MEM_REM)

#define EHEA_MSG_DEFAULT (NETIF_MSG_LINK | NETIF_MSG_TIMER \
	| NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR)

#define EHEA_MAX_ENTRIES_RQ1 32767
#define EHEA_MAX_ENTRIES_RQ2 16383
#define EHEA_MAX_ENTRIES_RQ3 16383
#define EHEA_MAX_ENTRIES_SQ  32767
#define EHEA_MIN_ENTRIES_QP  127

#define EHEA_SMALL_QUEUES

#ifdef EHEA_SMALL_QUEUES
#define EHEA_MAX_CQE_COUNT      1023
#define EHEA_DEF_ENTRIES_SQ     1023
#define EHEA_DEF_ENTRIES_RQ1    1023
#define EHEA_DEF_ENTRIES_RQ2    1023
#define EHEA_DEF_ENTRIES_RQ3    511
#else
#define EHEA_MAX_CQE_COUNT      4080
#define EHEA_DEF_ENTRIES_SQ     4080
#define EHEA_DEF_ENTRIES_RQ1    8160
#define EHEA_DEF_ENTRIES_RQ2    2040
#define EHEA_DEF_ENTRIES_RQ3    2040
#endif

#define EHEA_MAX_ENTRIES_EQ 20

#define EHEA_SG_SQ  2
#define EHEA_SG_RQ1 1
#define EHEA_SG_RQ2 0
#define EHEA_SG_RQ3 0

#define EHEA_MAX_PACKET_SIZE    9022	
#define EHEA_RQ2_PKT_SIZE       2048
#define EHEA_L_PKT_SIZE         256	


#define EHEA_PD_ID        0xaabcdeff

#define EHEA_RQ2_THRESHOLD 	   1
#define EHEA_RQ3_THRESHOLD	   4	

#define EHEA_SPEED_10G         10000
#define EHEA_SPEED_1G           1000
#define EHEA_SPEED_100M          100
#define EHEA_SPEED_10M            10
#define EHEA_SPEED_AUTONEG         0

#define EHEA_BCMC_SCOPE_ALL	0x08
#define EHEA_BCMC_SCOPE_SINGLE	0x00
#define EHEA_BCMC_MULTICAST	0x04
#define EHEA_BCMC_BROADCAST	0x00
#define EHEA_BCMC_UNTAGGED	0x02
#define EHEA_BCMC_TAGGED	0x00
#define EHEA_BCMC_VLANID_ALL	0x01
#define EHEA_BCMC_VLANID_SINGLE	0x00

#define EHEA_CACHE_LINE          128

#define EHEA_MR_ACC_CTRL       0x00800000

#define EHEA_BUSMAP_START      0x8000000000000000ULL
#define EHEA_INVAL_ADDR        0xFFFFFFFFFFFFFFFFULL
#define EHEA_DIR_INDEX_SHIFT 13                   
#define EHEA_TOP_INDEX_SHIFT (EHEA_DIR_INDEX_SHIFT * 2)
#define EHEA_MAP_ENTRIES (1 << EHEA_DIR_INDEX_SHIFT)
#define EHEA_MAP_SIZE (0x10000)                   
#define EHEA_INDEX_MASK (EHEA_MAP_ENTRIES - 1)


#define EHEA_WATCH_DOG_TIMEOUT 10*HZ


void ehea_dump(void *adr, int len, char *msg);

#define EHEA_BMASK(pos, length) (((pos) << 16) + (length))

#define EHEA_BMASK_IBM(from, to) (((63 - to) << 16) + ((to) - (from) + 1))

#define EHEA_BMASK_SHIFTPOS(mask) (((mask) >> 16) & 0xffff)

#define EHEA_BMASK_MASK(mask) \
	(0xffffffffffffffffULL >> ((64 - (mask)) & 0xffff))

#define EHEA_BMASK_SET(mask, value) \
	((EHEA_BMASK_MASK(mask) & ((u64)(value))) << EHEA_BMASK_SHIFTPOS(mask))

#define EHEA_BMASK_GET(mask, value) \
	(EHEA_BMASK_MASK(mask) & (((u64)(value)) >> EHEA_BMASK_SHIFTPOS(mask)))

struct ehea_page {
	u8 entries[PAGE_SIZE];
};

struct hw_queue {
	u64 current_q_offset;		
	struct ehea_page **queue_pages;	
	u32 qe_size;			
	u32 queue_length;      		
	u32 pagesize;
	u32 toggle_state;		
	u32 reserved;			
};

struct h_epa {
	void __iomem *addr;
};

struct h_epa_user {
	u64 addr;
};

struct h_epas {
	struct h_epa kernel;	
	struct h_epa_user user;	
};

struct ehea_dir_bmap
{
	u64 ent[EHEA_MAP_ENTRIES];
};
struct ehea_top_bmap
{
	struct ehea_dir_bmap *dir[EHEA_MAP_ENTRIES];
};
struct ehea_bmap
{
	struct ehea_top_bmap *top[EHEA_MAP_ENTRIES];
};

struct ehea_qp;
struct ehea_cq;
struct ehea_eq;
struct ehea_port;
struct ehea_av;

struct ehea_qp_init_attr {
	
	u32 qp_token;           
	u8 low_lat_rq1;
	u8 signalingtype;       
	u8 rq_count;            
	u8 eqe_gen;             
	u16 max_nr_send_wqes;   
	u16 max_nr_rwqes_rq1;   
	u16 max_nr_rwqes_rq2;
	u16 max_nr_rwqes_rq3;
	u8 wqe_size_enc_sq;
	u8 wqe_size_enc_rq1;
	u8 wqe_size_enc_rq2;
	u8 wqe_size_enc_rq3;
	u8 swqe_imm_data_len;   
	u16 port_nr;
	u16 rq2_threshold;
	u16 rq3_threshold;
	u64 send_cq_handle;
	u64 recv_cq_handle;
	u64 aff_eq_handle;

	
	u32 qp_nr;
	u16 act_nr_send_wqes;
	u16 act_nr_rwqes_rq1;
	u16 act_nr_rwqes_rq2;
	u16 act_nr_rwqes_rq3;
	u8 act_wqe_size_enc_sq;
	u8 act_wqe_size_enc_rq1;
	u8 act_wqe_size_enc_rq2;
	u8 act_wqe_size_enc_rq3;
	u32 nr_sq_pages;
	u32 nr_rq1_pages;
	u32 nr_rq2_pages;
	u32 nr_rq3_pages;
	u32 liobn_sq;
	u32 liobn_rq1;
	u32 liobn_rq2;
	u32 liobn_rq3;
};

struct ehea_eq_attr {
	u32 type;
	u32 max_nr_of_eqes;
	u8 eqe_gen;        
	u64 eq_handle;
	u32 act_nr_of_eqes;
	u32 nr_pages;
	u32 ist1;          
	u32 ist2;
	u32 ist3;
	u32 ist4;
};


struct ehea_eq {
	struct ehea_adapter *adapter;
	struct hw_queue hw_queue;
	u64 fw_handle;
	struct h_epas epas;
	spinlock_t spinlock;
	struct ehea_eq_attr attr;
};

struct ehea_qp {
	struct ehea_adapter *adapter;
	u64 fw_handle;			
	struct hw_queue hw_squeue;
	struct hw_queue hw_rqueue1;
	struct hw_queue hw_rqueue2;
	struct hw_queue hw_rqueue3;
	struct h_epas epas;
	struct ehea_qp_init_attr init_attr;
};

struct ehea_cq_attr {
	
	u32 max_nr_of_cqes;
	u32 cq_token;
	u64 eq_handle;

	
	u32 act_nr_of_cqes;
	u32 nr_pages;
};

struct ehea_cq {
	struct ehea_adapter *adapter;
	u64 fw_handle;
	struct hw_queue hw_queue;
	struct h_epas epas;
	struct ehea_cq_attr attr;
};

struct ehea_mr {
	struct ehea_adapter *adapter;
	u64 handle;
	u64 vaddr;
	u32 lkey;
};

struct port_stats {
	int poll_receive_errors;
	int queue_stopped;
	int err_tcp_cksum;
	int err_ip_cksum;
	int err_frame_crc;
};

#define EHEA_IRQ_NAME_SIZE 20

struct ehea_q_skb_arr {
	struct sk_buff **arr;		
	int len;                	
	int index;			
	int os_skbs;			
};

struct ehea_port_res {
	struct napi_struct napi;
	struct port_stats p_stats;
	struct ehea_mr send_mr;       	
	struct ehea_mr recv_mr;       	
	struct ehea_port *port;
	char int_recv_name[EHEA_IRQ_NAME_SIZE];
	char int_send_name[EHEA_IRQ_NAME_SIZE];
	struct ehea_qp *qp;
	struct ehea_cq *send_cq;
	struct ehea_cq *recv_cq;
	struct ehea_eq *eq;
	struct ehea_q_skb_arr rq1_skba;
	struct ehea_q_skb_arr rq2_skba;
	struct ehea_q_skb_arr rq3_skba;
	struct ehea_q_skb_arr sq_skba;
	int sq_skba_size;
	int swqe_refill_th;
	atomic_t swqe_avail;
	int swqe_ll_count;
	u32 swqe_id_counter;
	u64 tx_packets;
	u64 tx_bytes;
	u64 rx_packets;
	u64 rx_bytes;
	int sq_restart_flag;
};


#define EHEA_MAX_PORTS 16

#define EHEA_NUM_PORTRES_FW_HANDLES    6  
#define EHEA_NUM_PORT_FW_HANDLES       1  
#define EHEA_NUM_ADAPTER_FW_HANDLES    2  

struct ehea_adapter {
	u64 handle;
	struct platform_device *ofdev;
	struct ehea_port *port[EHEA_MAX_PORTS];
	struct ehea_eq *neq;       
	struct tasklet_struct neq_tasklet;
	struct ehea_mr mr;
	u32 pd;                    
	u64 max_mc_mac;            
	int active_ports;
	struct list_head list;
};


struct ehea_mc_list {
	struct list_head list;
	u64 macaddr;
};

struct ehea_fw_handle_entry {
	u64 adh;               
	u64 fwh;               
};

struct ehea_fw_handle_array {
	struct ehea_fw_handle_entry *arr;
	int num_entries;
	struct mutex lock;
};

struct ehea_bcmc_reg_entry {
	u64 adh;               
	u32 port_id;           
	u8 reg_type;           
	u64 macaddr;
};

struct ehea_bcmc_reg_array {
	struct ehea_bcmc_reg_entry *arr;
	int num_entries;
	spinlock_t lock;
};

#define EHEA_PORT_UP 1
#define EHEA_PORT_DOWN 0
#define EHEA_PHY_LINK_UP 1
#define EHEA_PHY_LINK_DOWN 0
#define EHEA_MAX_PORT_RES 16
struct ehea_port {
	struct ehea_adapter *adapter;	 
	struct net_device *netdev;
	struct rtnl_link_stats64 stats;
	struct ehea_port_res port_res[EHEA_MAX_PORT_RES];
	struct platform_device  ofdev; 
	struct ehea_mc_list *mc_list;	 
	struct ehea_eq *qp_eq;
	struct work_struct reset_task;
	struct delayed_work stats_work;
	struct mutex port_lock;
	char int_aff_name[EHEA_IRQ_NAME_SIZE];
	int allmulti;			 
	int promisc;		 	 
	int num_mcs;
	int resets;
	unsigned long flags;
	u64 mac_addr;
	u32 logical_port_id;
	u32 port_speed;
	u32 msg_enable;
	u32 sig_comp_iv;
	u32 state;
	u8 phy_link;
	u8 full_duplex;
	u8 autoneg;
	u8 num_def_qps;
	wait_queue_head_t swqe_avail_wq;
	wait_queue_head_t restart_wq;
};

struct port_res_cfg {
	int max_entries_rcq;
	int max_entries_scq;
	int max_entries_sq;
	int max_entries_rq1;
	int max_entries_rq2;
	int max_entries_rq3;
};

enum ehea_flag_bits {
	__EHEA_STOP_XFER,
	__EHEA_DISABLE_PORT_RESET
};

void ehea_set_ethtool_ops(struct net_device *netdev);
int ehea_sense_port_attr(struct ehea_port *port);
int ehea_set_portspeed(struct ehea_port *port, u32 port_speed);

#endif	
