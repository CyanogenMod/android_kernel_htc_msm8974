/*
 * Copyright (c) 2008-2009 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 Intel Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#ifndef _LIBFCOE_H
#define _LIBFCOE_H

#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <scsi/fc/fc_fcoe.h>
#include <scsi/libfc.h>

#define FCOE_MAX_CMD_LEN	16	

#define FCOE_MTU	2158

#define FCOE_CTLR_START_DELAY	2000	
#define FCOE_CTRL_SOL_TOV	2000	
#define FCOE_CTLR_FCF_LIMIT	20	
#define FCOE_CTLR_VN2VN_LOGIN_LIMIT 3	

enum fip_state {
	FIP_ST_DISABLED,
	FIP_ST_LINK_WAIT,
	FIP_ST_AUTO,
	FIP_ST_NON_FIP,
	FIP_ST_ENABLED,
	FIP_ST_VNMP_START,
	FIP_ST_VNMP_PROBE1,
	FIP_ST_VNMP_PROBE2,
	FIP_ST_VNMP_CLAIM,
	FIP_ST_VNMP_UP,
};

#define FIP_MODE_AUTO		FIP_ST_AUTO
#define FIP_MODE_NON_FIP	FIP_ST_NON_FIP
#define FIP_MODE_FABRIC		FIP_ST_ENABLED
#define FIP_MODE_VN2VN		FIP_ST_VNMP_START

struct fcoe_ctlr {
	enum fip_state state;
	enum fip_state mode;
	struct fc_lport *lp;
	struct fcoe_fcf *sel_fcf;
	struct list_head fcfs;
	u16 fcf_count;
	unsigned long sol_time;
	unsigned long sel_time;
	unsigned long port_ka_time;
	unsigned long ctlr_ka_time;
	struct timer_list timer;
	struct work_struct timer_work;
	struct work_struct recv_work;
	struct sk_buff_head fip_recv_list;
	struct sk_buff *flogi_req;

	struct rnd_state rnd_state;
	u32 port_id;

	u16 user_mfs;
	u16 flogi_oxid;
	u8 flogi_req_send;
	u8 flogi_count;
	u8 map_dest;
	u8 spma;
	u8 probe_tries;
	u8 priority;
	u8 dest_addr[ETH_ALEN];
	u8 ctl_src_addr[ETH_ALEN];

	void (*send)(struct fcoe_ctlr *, struct sk_buff *);
	void (*update_mac)(struct fc_lport *, u8 *addr);
	u8 * (*get_src_addr)(struct fc_lport *);
	struct mutex ctlr_mutex;
	spinlock_t ctlr_lock;
};

struct fcoe_fcf {
	struct list_head list;
	unsigned long time;

	u64 switch_name;
	u64 fabric_name;
	u32 fc_map;
	u16 vfid;
	u8 fcf_mac[ETH_ALEN];
	u8 fcoe_mac[ETH_ALEN];

	u8 pri;
	u8 flogi_sent;
	u16 flags;
	u32 fka_period;
	u8 fd_flags:1;
};

struct fcoe_rport {
	unsigned long time;
	u16 fcoe_len;
	u16 flags;
	u8 login_count;
	u8 enode_mac[ETH_ALEN];
	u8 vn_mac[ETH_ALEN];
};

void fcoe_ctlr_init(struct fcoe_ctlr *, enum fip_state);
void fcoe_ctlr_destroy(struct fcoe_ctlr *);
void fcoe_ctlr_link_up(struct fcoe_ctlr *);
int fcoe_ctlr_link_down(struct fcoe_ctlr *);
int fcoe_ctlr_els_send(struct fcoe_ctlr *, struct fc_lport *, struct sk_buff *);
void fcoe_ctlr_recv(struct fcoe_ctlr *, struct sk_buff *);
int fcoe_ctlr_recv_flogi(struct fcoe_ctlr *, struct fc_lport *,
			 struct fc_frame *);

u64 fcoe_wwn_from_mac(unsigned char mac[], unsigned int, unsigned int);
int fcoe_libfc_config(struct fc_lport *, struct fcoe_ctlr *,
		      const struct libfc_function_template *, int init_fcp);
u32 fcoe_fc_crc(struct fc_frame *fp);
int fcoe_start_io(struct sk_buff *skb);
int fcoe_get_wwn(struct net_device *netdev, u64 *wwn, int type);
void __fcoe_get_lesb(struct fc_lport *lport, struct fc_els_lesb *fc_lesb,
		     struct net_device *netdev);
void fcoe_wwn_to_str(u64 wwn, char *buf, int len);
int fcoe_validate_vport_create(struct fc_vport *vport);

static inline bool is_fip_mode(struct fcoe_ctlr *fip)
{
	return fip->state == FIP_ST_ENABLED;
}

#define MODULE_ALIAS_FCOE_PCI(ven, dev) \
	MODULE_ALIAS("fcoe-pci:"	\
		"v" __stringify(ven)	\
		"d" __stringify(dev) "sv*sd*bc*sc*i*")

#define FCOE_TRANSPORT_DEFAULT	"fcoe"

struct fcoe_transport {
	char name[IFNAMSIZ];
	bool attached;
	struct list_head list;
	bool (*match) (struct net_device *device);
	int (*create) (struct net_device *device, enum fip_state fip_mode);
	int (*destroy) (struct net_device *device);
	int (*enable) (struct net_device *device);
	int (*disable) (struct net_device *device);
};

struct fcoe_percpu_s {
	struct task_struct *thread;
	struct sk_buff_head fcoe_rx_list;
	struct page *crc_eof_page;
	int crc_eof_offset;
};

struct fcoe_port {
	void		      *priv;
	struct fc_lport	      *lport;
	struct sk_buff_head   fcoe_pending_queue;
	u8		      fcoe_pending_queue_active;
	u8		      priority;
	u32		      max_queue_depth;
	u32		      min_queue_depth;
	struct timer_list     timer;
	struct work_struct    destroy_work;
	u8		      data_src_addr[ETH_ALEN];
};
void fcoe_clean_pending_queue(struct fc_lport *);
void fcoe_check_wait_queue(struct fc_lport *lport, struct sk_buff *skb);
void fcoe_queue_timer(ulong lport);
int fcoe_get_paged_crc_eof(struct sk_buff *skb, int tlen,
			   struct fcoe_percpu_s *fps);

struct fcoe_netdev_mapping {
	struct list_head list;
	struct net_device *netdev;
	struct fcoe_transport *ft;
};

int fcoe_transport_attach(struct fcoe_transport *ft);
int fcoe_transport_detach(struct fcoe_transport *ft);

#endif 
