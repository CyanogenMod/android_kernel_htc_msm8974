/*********************************************************************
 *                
 * Filename:      irda_device.h
 * Version:       0.9
 * Description:   Contains various declarations used by the drivers
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Apr 14 12:41:42 1998
 * Modified at:   Mon Mar 20 09:08:57 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1999-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 1998 Thomas Davis, <ratbert@radiks.net>,
 *     Copyright (c) 2000-2002 Jean Tourrilhes <jt@hpl.hp.com>
 *
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/


#ifndef IRDA_DEVICE_H
#define IRDA_DEVICE_H

#include <linux/tty.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>		
#include <linux/irda.h>
#include <linux/types.h>

#include <net/pkt_sched.h>
#include <net/irda/irda.h>
#include <net/irda/qos.h>		
#include <net/irda/irqueue.h>		

struct irlap_cb;

#define IFF_SIR 	0x0001 
#define IFF_MIR 	0x0002 
#define IFF_FIR 	0x0004 
#define IFF_VFIR        0x0008 
#define IFF_PIO   	0x0010 
#define IFF_DMA		0x0020 
#define IFF_SHM         0x0040 
#define IFF_DONGLE      0x0080 
#define IFF_AIR         0x0100 

#define IO_XMIT 0x01
#define IO_RECV 0x02

typedef enum {
	IRDA_IRLAP, 
	IRDA_RAW,   
	SHARP_ASK,
	TV_REMOTE,  
} INFRARED_MODE;

typedef enum {
	IRDA_TASK_INIT,        
	IRDA_TASK_DONE,        
	IRDA_TASK_WAIT,
	IRDA_TASK_WAIT1,
	IRDA_TASK_WAIT2,
	IRDA_TASK_WAIT3,
	IRDA_TASK_CHILD_INIT,  
	IRDA_TASK_CHILD_WAIT,  
	IRDA_TASK_CHILD_DONE   
} IRDA_TASK_STATE;

struct irda_task;
typedef int (*IRDA_TASK_CALLBACK) (struct irda_task *task);

struct irda_task {
	irda_queue_t q;
	magic_t magic;

	IRDA_TASK_STATE state;
	IRDA_TASK_CALLBACK function;
	IRDA_TASK_CALLBACK finished;

	struct irda_task *parent;
	struct timer_list timer;

	void *instance; 
	void *param;    
};

struct dongle_reg;
typedef struct {
	struct dongle_reg *issue;     
	struct net_device *dev;           
	struct irda_task *speed_task; 
	struct irda_task *reset_task; 
	__u32 speed;                  

	
	int (*set_mode)(struct net_device *, int mode);
	int (*read)(struct net_device *dev, __u8 *buf, int len);
	int (*write)(struct net_device *dev, __u8 *buf, int len);
	int (*set_dtr_rts)(struct net_device *dev, int dtr, int rts);
} dongle_t;

struct dongle_reg {
	irda_queue_t q;         
	IRDA_DONGLE type;

	void (*open)(dongle_t *dongle, struct qos_info *qos);
	void (*close)(dongle_t *dongle);
	int  (*reset)(struct irda_task *task);
	int  (*change_speed)(struct irda_task *task);
	struct module *owner;
};

struct irda_skb_cb {
	unsigned int default_qdisc_pad;
	magic_t magic;       
	__u32   next_speed;  
	__u16   mtt;         
	__u16   xbofs;       
	__u16   next_xbofs;  
	void    *context;    
	void    (*destructor)(struct sk_buff *skb); 
	__u16   xbofs_delay; 
	__u8    line;        
};

typedef struct {
	int cfg_base;         
        int sir_base;         
	int fir_base;         
	int mem_base;         
        int sir_ext;          
	int fir_ext;          
        int irq, irq2;        
        int dma, dma2;        
        int fifo_size;        
        int irqflags;         
	int direction;        
	int enabled;          
	int suspended;        
	__u32 speed;          
	__u32 new_speed;      
	int dongle_id;        
} chipio_t;

typedef struct {
	int state;            
	int in_frame;         

	__u8 *head;	      
	__u8 *data;	      

	int len;	      
	int truesize;	      
	__u16 fcs;

	struct sk_buff *skb;	
} iobuff_t;

#define IRDA_SKB_MAX_MTU	2064
#define IRDA_SIR_MAX_FRAME	4269

#define IRDA_RX_COPY_THRESHOLD  256

int  irda_device_init(void);
void irda_device_cleanup(void);

struct irlap_cb *irlap_open(struct net_device *dev, struct qos_info *qos,
			    const char *hw_name);
void irlap_close(struct irlap_cb *self);

void irda_device_set_media_busy(struct net_device *dev, int status);
int  irda_device_is_media_busy(struct net_device *dev);
int  irda_device_is_receiving(struct net_device *dev);

static inline int irda_device_txqueue_empty(const struct net_device *dev)
{
	return qdisc_all_tx_empty(dev);
}
int  irda_device_set_raw_mode(struct net_device* self, int status);
struct net_device *alloc_irdadev(int sizeof_priv);

void irda_setup_dma(int channel, dma_addr_t buffer, int count, int mode);

static inline __u16 irda_get_mtt(const struct sk_buff *skb)
{
	const struct irda_skb_cb *cb = (const struct irda_skb_cb *) skb->cb;
	return (cb->magic == LAP_MAGIC) ? cb->mtt : 10000;
}

static inline __u32 irda_get_next_speed(const struct sk_buff *skb)
{
	const struct irda_skb_cb *cb = (const struct irda_skb_cb *) skb->cb;
	return (cb->magic == LAP_MAGIC) ? cb->next_speed : -1;
}

static inline __u16 irda_get_xbofs(const struct sk_buff *skb)
{
	const struct irda_skb_cb *cb = (const struct irda_skb_cb *) skb->cb;
	return (cb->magic == LAP_MAGIC) ? cb->xbofs : 10;
}

static inline __u16 irda_get_next_xbofs(const struct sk_buff *skb)
{
	const struct irda_skb_cb *cb = (const struct irda_skb_cb *) skb->cb;
	return (cb->magic == LAP_MAGIC) ? cb->next_xbofs : -1;
}
#endif 


