#ifndef _MPC_H_
#define _MPC_H_

#include <linux/types.h>
#include <linux/atm.h>
#include <linux/atmmpc.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include "mpoa_caches.h"

int msg_to_mpoad(struct k_message *msg, struct mpoa_client *mpc);

struct mpoa_client {
	struct mpoa_client *next;
	struct net_device *dev;      
	int dev_num;                 

	struct atm_vcc *mpoad_vcc;   
	uint8_t mps_ctrl_addr[ATM_ESA_LEN];  
	uint8_t our_ctrl_addr[ATM_ESA_LEN];  

	rwlock_t ingress_lock;
	struct in_cache_ops *in_ops; 
	in_cache_entry *in_cache;    

	rwlock_t egress_lock;
	struct eg_cache_ops *eg_ops; 
	eg_cache_entry *eg_cache;    

	uint8_t *mps_macs;           
	int number_of_mps_macs;      
	struct mpc_parameters parameters;  

	const struct net_device_ops *old_ops;
	struct net_device_ops new_ops;
};


struct atm_mpoa_qos {
	struct atm_mpoa_qos *next;
	__be32 ipaddr;
	struct atm_qos qos;
};


struct atm_mpoa_qos *atm_mpoa_add_qos(__be32 dst_ip, struct atm_qos *qos);
struct atm_mpoa_qos *atm_mpoa_search_qos(__be32 dst_ip);
int atm_mpoa_delete_qos(struct atm_mpoa_qos *qos);

struct seq_file;
void atm_mpoa_disp_qos(struct seq_file *m);

#ifdef CONFIG_PROC_FS
int mpc_proc_init(void);
void mpc_proc_clean(void);
#else
#define mpc_proc_init() (0)
#define mpc_proc_clean() do { } while(0)
#endif

#endif 
