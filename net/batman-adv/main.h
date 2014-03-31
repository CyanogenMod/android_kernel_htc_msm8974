/*
 * Copyright (C) 2007-2012 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#ifndef _NET_BATMAN_ADV_MAIN_H_
#define _NET_BATMAN_ADV_MAIN_H_

#define DRIVER_AUTHOR "Marek Lindner <lindner_marek@yahoo.de>, " \
		      "Simon Wunderlich <siwu@hrz.tu-chemnitz.de>"
#define DRIVER_DESC   "B.A.T.M.A.N. advanced"
#define DRIVER_DEVICE "batman-adv"

#ifndef SOURCE_VERSION
#define SOURCE_VERSION "2012.1.0"
#endif


#define TQ_MAX_VALUE 255
#define JITTER 20

 
#define TTL 50

#define PURGE_TIMEOUT 200000 
#define TT_LOCAL_TIMEOUT 3600000 
#define TT_CLIENT_ROAM_TIMEOUT 600000 
#define TQ_LOCAL_WINDOW_SIZE 64
#define TT_REQUEST_TIMEOUT 3000 

#define TQ_GLOBAL_WINDOW_SIZE 5
#define TQ_LOCAL_BIDRECT_SEND_MINIMUM 1
#define TQ_LOCAL_BIDRECT_RECV_MINIMUM 1
#define TQ_TOTAL_BIDRECT_LIMIT 1

#define TT_OGM_APPEND_MAX 3 

#define ROAMING_MAX_TIME 20000 
#define ROAMING_MAX_COUNT 5

#define NO_FLAGS 0

#define NULL_IFINDEX 0 

#define NUM_WORDS (TQ_LOCAL_WINDOW_SIZE / WORD_BIT_SIZE)

#define LOG_BUF_LEN 8192	  

#define VIS_INTERVAL 5000	

#define BONDING_TQ_THRESHOLD	50

#define MAX_AGGREGATION_BYTES 512
#define MAX_AGGREGATION_MS 100

#define SOFTIF_NEIGH_TIMEOUT 180000 

#define RESET_PROTECTION_MS 30000
#define EXPECTED_SEQNO_RANGE	65536

enum mesh_state {
	MESH_INACTIVE,
	MESH_ACTIVE,
	MESH_DEACTIVATING
};

#define BCAST_QUEUE_LEN		256
#define BATMAN_QUEUE_LEN	256

enum uev_action {
	UEV_ADD = 0,
	UEV_DEL,
	UEV_CHANGE
};

enum uev_type {
	UEV_GW = 0
};

#define GW_THRESHOLD	50

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

enum dbg_level {
	DBG_BATMAN = 1 << 0,
	DBG_ROUTES = 1 << 1, 
	DBG_TT	   = 1 << 2, 
	DBG_ALL    = 7
};


#include <linux/mutex.h>	
#include <linux/module.h>	
#include <linux/netdevice.h>	
#include <linux/etherdevice.h>  
#include <linux/if_ether.h>	
#include <linux/poll.h>		
#include <linux/kthread.h>	
#include <linux/pkt_sched.h>	
#include <linux/workqueue.h>	
#include <linux/slab.h>
#include <net/sock.h>		
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include "types.h"

extern char bat_routing_algo[];
extern struct list_head hardif_list;

extern unsigned char broadcast_addr[];
extern struct workqueue_struct *bat_event_workqueue;

int mesh_init(struct net_device *soft_iface);
void mesh_free(struct net_device *soft_iface);
void inc_module_count(void);
void dec_module_count(void);
int is_my_mac(const uint8_t *addr);
int bat_algo_register(struct bat_algo_ops *bat_algo_ops);
int bat_algo_select(struct bat_priv *bat_priv, char *name);
int bat_algo_seq_print_text(struct seq_file *seq, void *offset);

#ifdef CONFIG_BATMAN_ADV_DEBUG
int debug_log(struct bat_priv *bat_priv, const char *fmt, ...) __printf(2, 3);

#define bat_dbg(type, bat_priv, fmt, arg...)			\
	do {							\
		if (atomic_read(&bat_priv->log_level) & type)	\
			debug_log(bat_priv, fmt, ## arg);	\
	}							\
	while (0)
#else 
__printf(3, 4)
static inline void bat_dbg(int type __always_unused,
			   struct bat_priv *bat_priv __always_unused,
			   const char *fmt __always_unused, ...)
{
}
#endif

#define bat_info(net_dev, fmt, arg...)					\
	do {								\
		struct net_device *_netdev = (net_dev);                 \
		struct bat_priv *_batpriv = netdev_priv(_netdev);       \
		bat_dbg(DBG_ALL, _batpriv, fmt, ## arg);		\
		pr_info("%s: " fmt, _netdev->name, ## arg);		\
	} while (0)
#define bat_err(net_dev, fmt, arg...)					\
	do {								\
		struct net_device *_netdev = (net_dev);                 \
		struct bat_priv *_batpriv = netdev_priv(_netdev);       \
		bat_dbg(DBG_ALL, _batpriv, fmt, ## arg);		\
		pr_err("%s: " fmt, _netdev->name, ## arg);		\
	} while (0)


static inline int compare_eth(const void *data1, const void *data2)
{
	return (memcmp(data1, data2, ETH_ALEN) == 0 ? 1 : 0);
}

static inline bool has_timed_out(unsigned long timestamp, unsigned int timeout)
{
	return time_is_before_jiffies(timestamp + msecs_to_jiffies(timeout));
}

#define atomic_dec_not_zero(v)	atomic_add_unless((v), -1, 0)

#define smallest_signed_int(x) (1u << (7u + 8u * (sizeof(x) - 1u)))

#define seq_before(x, y) ({typeof(x) _d1 = (x); \
			  typeof(y) _d2 = (y); \
			  typeof(x) _dummy = (_d1 - _d2); \
			  (void) (&_d1 == &_d2); \
			  _dummy > smallest_signed_int(_dummy); })
#define seq_after(x, y) seq_before(y, x)

#endif 
