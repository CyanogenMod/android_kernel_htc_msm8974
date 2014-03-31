 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */
 
 
#ifndef _ATMCLIP_H
#define _ATMCLIP_H

#include <linux/netdevice.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmarp.h>
#include <linux/spinlock.h>
#include <net/neighbour.h>


#define CLIP_VCC(vcc) ((struct clip_vcc *) ((vcc)->user_back))

struct sk_buff;

struct clip_vcc {
	struct atm_vcc	*vcc;		
	struct atmarp_entry *entry;	
	int		xoff;		
	unsigned char	encap;		
	unsigned long	last_use;	
	unsigned long	idle_timeout;	
	void (*old_push)(struct atm_vcc *vcc,struct sk_buff *skb);
					
	void (*old_pop)(struct atm_vcc *vcc,struct sk_buff *skb);
					
	struct clip_vcc	*next;		
};


struct atmarp_entry {
	struct clip_vcc	*vccs;		
	unsigned long	expires;	
	struct neighbour *neigh;	
};

#define PRIV(dev) ((struct clip_priv *) netdev_priv(dev))

struct clip_priv {
	int number;			
	spinlock_t xoff_lock;		
	struct net_device *next;	
};

#endif
