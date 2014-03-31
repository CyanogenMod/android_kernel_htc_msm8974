/*
 *  Copyright (C) 2002 Intersil Americas Inc.
 *  Copyright (C) 2003 Herbert Valerio Riedel <hvr@gnu.org>
 *  Copyright (C) 2003 Luis R. Rodriguez <mcgrof@ruslug.rutgers.edu>
 *  Copyright (C) 2003 Aurelien Alleaume <slts@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _ISLPCI_DEV_H
#define _ISLPCI_DEV_H

#include <linux/irqreturn.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "isl_38xx.h"
#include "isl_oid.h"
#include "islpci_mgt.h"

typedef enum {
	PRV_STATE_OFF = 0,	
	PRV_STATE_PREBOOT,	
	PRV_STATE_BOOT,		
	PRV_STATE_POSTBOOT,	
	PRV_STATE_PREINIT,	
	PRV_STATE_INIT,		
	PRV_STATE_READY,	
	PRV_STATE_SLEEP		
} islpci_state_t;

struct mac_entry {
   struct list_head _list;
   char addr[ETH_ALEN];
};

struct islpci_acl {
   enum { MAC_POLICY_OPEN=0, MAC_POLICY_ACCEPT=1, MAC_POLICY_REJECT=2 } policy;
   struct list_head mac_list;  
   int size;   
   struct mutex lock;   
};

struct islpci_membuf {
	int size;                   
	void *mem;                  
	dma_addr_t pci_addr;        
};

#define MAX_BSS_WPA_IE_COUNT 64
#define MAX_WPA_IE_LEN 64
struct islpci_bss_wpa_ie {
	struct list_head list;
	unsigned long last_update;
	u8 bssid[ETH_ALEN];
	u8 wpa_ie[MAX_WPA_IE_LEN];
	size_t wpa_ie_len;

};

typedef struct {
	spinlock_t slock;	

	u32 priv_oid;

	
	u32 iw_mode;
        struct rw_semaphore mib_sem;
	void **mib;
	char nickname[IW_ESSID_MAX_SIZE+1];

	
	struct work_struct stats_work;
	struct mutex stats_lock;
	
	unsigned long stats_timestamp;
	struct iw_statistics local_iwstatistics;
	struct iw_statistics iwstatistics;

	struct iw_spy_data spy_data; 

	struct iw_public_data wireless_data;

	int monitor_type; 

	struct islpci_acl acl;

	
	struct pci_dev *pdev;	
	char firmware[33];

	void __iomem *device_base;	

	
	void *driver_mem_address;	
	dma_addr_t device_host_address;	
	dma_addr_t device_psm_buffer;	

	
	struct net_device *ndev;

	
	struct isl38xx_cb *control_block;	

	u32 index_mgmt_rx;              
	u32 index_mgmt_tx;              
	u32 free_data_rx;	
	u32 free_data_tx;	
	u32 data_low_tx_full;	

	
	struct islpci_membuf mgmt_tx[ISL38XX_CB_MGMT_QSIZE];
	struct islpci_membuf mgmt_rx[ISL38XX_CB_MGMT_QSIZE];
	struct sk_buff *data_low_tx[ISL38XX_CB_TX_QSIZE];
	struct sk_buff *data_low_rx[ISL38XX_CB_RX_QSIZE];
	dma_addr_t pci_map_tx_address[ISL38XX_CB_TX_QSIZE];
	dma_addr_t pci_map_rx_address[ISL38XX_CB_RX_QSIZE];

	
	wait_queue_head_t reset_done;

	
	struct mutex mgmt_lock; 
	struct islpci_mgmtframe *mgmt_received;	  
	wait_queue_head_t mgmt_wqueue;            

	
	islpci_state_t state;
	int state_off;		

	
	int wpa; 
	struct list_head bss_wpa_list;
	int num_bss_wpa;
	struct mutex wpa_lock;
	u8 wpa_ie[MAX_WPA_IE_LEN];
	size_t wpa_ie_len;

	struct work_struct reset_task;
	int reset_task_pending;
} islpci_private;

static inline islpci_state_t
islpci_get_state(islpci_private *priv)
{
	
	return priv->state;
	
}

islpci_state_t islpci_set_state(islpci_private *priv, islpci_state_t new_state);

#define ISLPCI_TX_TIMEOUT               (2*HZ)

irqreturn_t islpci_interrupt(int, void *);

int prism54_post_setup(islpci_private *, int);
int islpci_reset(islpci_private *, int);

static inline void
islpci_trigger(islpci_private *priv)
{
	isl38xx_trigger_device(islpci_get_state(priv) == PRV_STATE_SLEEP,
			       priv->device_base);
}

int islpci_free_memory(islpci_private *);
struct net_device *islpci_setup(struct pci_dev *);

#define DRV_NAME	"prism54"
#define DRV_VERSION	"1.2"

#endif				
