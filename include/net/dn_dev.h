#ifndef _NET_DN_DEV_H
#define _NET_DN_DEV_H


struct dn_dev;

struct dn_ifaddr {
	struct dn_ifaddr __rcu *ifa_next;
	struct dn_dev    *ifa_dev;
	__le16            ifa_local;
	__le16            ifa_address;
	__u8              ifa_flags;
	__u8              ifa_scope;
	char              ifa_label[IFNAMSIZ];
	struct rcu_head   rcu;
};

#define DN_DEV_S_RU  0 
#define DN_DEV_S_CR  1 
#define DN_DEV_S_DS  2 
#define DN_DEV_S_RI  3 
#define DN_DEV_S_RV  4 
#define DN_DEV_S_RC  5 
#define DN_DEV_S_OF  6 
#define DN_DEV_S_HA  7 


struct dn_dev_parms {
	int type;	          
	int mode;	          
#define DN_DEV_BCAST  1
#define DN_DEV_UCAST  2
#define DN_DEV_MPOINT 4
	int state;                
	int forwarding;	          
	unsigned long t2;         
	unsigned long t3;         
	int priority;             
	char *name;               
	int  (*up)(struct net_device *);
	void (*down)(struct net_device *);
	void (*timer3)(struct net_device *, struct dn_ifaddr *ifa);
	void *sysctl;
};


struct dn_dev {
	struct dn_ifaddr __rcu *ifa_list;
	struct net_device *dev;
	struct dn_dev_parms parms;
	char use_long;
	struct timer_list timer;
	unsigned long t3;
	struct neigh_parms *neigh_parms;
	__u8 addr[ETH_ALEN];
	struct neighbour *router; 
	struct neighbour *peer;   
	unsigned long uptime;     
};

struct dn_short_packet {
	__u8    msgflg;
	__le16 dstnode;
	__le16 srcnode;
	__u8   forward;
} __packed;

struct dn_long_packet {
	__u8   msgflg;
	__u8   d_area;
	__u8   d_subarea;
	__u8   d_id[6];
	__u8   s_area;
	__u8   s_subarea;
	__u8   s_id[6];
	__u8   nl2;
	__u8   visit_ct;
	__u8   s_class;
	__u8   pt;
} __packed;


struct endnode_hello_message {
	__u8   msgflg;
	__u8   tiver[3];
	__u8   id[6];
	__u8   iinfo;
	__le16 blksize;
	__u8   area;
	__u8   seed[8];
	__u8   neighbor[6];
	__le16 timer;
	__u8   mpd;
	__u8   datalen;
	__u8   data[2];
} __packed;

struct rtnode_hello_message {
	__u8   msgflg;
	__u8   tiver[3];
	__u8   id[6];
	__u8   iinfo;
	__le16  blksize;
	__u8   priority;
	__u8   area;
	__le16  timer;
	__u8   mpd;
} __packed;


extern void dn_dev_init(void);
extern void dn_dev_cleanup(void);

extern int dn_dev_ioctl(unsigned int cmd, void __user *arg);

extern void dn_dev_devices_off(void);
extern void dn_dev_devices_on(void);

extern void dn_dev_init_pkt(struct sk_buff *skb);
extern void dn_dev_veri_pkt(struct sk_buff *skb);
extern void dn_dev_hello(struct sk_buff *skb);

extern void dn_dev_up(struct net_device *);
extern void dn_dev_down(struct net_device *);

extern int dn_dev_set_default(struct net_device *dev, int force);
extern struct net_device *dn_dev_get_default(void);
extern int dn_dev_bind_default(__le16 *addr);

extern int register_dnaddr_notifier(struct notifier_block *nb);
extern int unregister_dnaddr_notifier(struct notifier_block *nb);

static inline int dn_dev_islocal(struct net_device *dev, __le16 addr)
{
	struct dn_dev *dn_db;
	struct dn_ifaddr *ifa;
	int res = 0;

	rcu_read_lock();
	dn_db = rcu_dereference(dev->dn_ptr);
	if (dn_db == NULL) {
		printk(KERN_DEBUG "dn_dev_islocal: Called for non DECnet device\n");
		goto out;
	}

	for (ifa = rcu_dereference(dn_db->ifa_list);
	     ifa != NULL;
	     ifa = rcu_dereference(ifa->ifa_next))
		if ((addr ^ ifa->ifa_local) == 0) {
			res = 1;
			break;
		}
out:
	rcu_read_unlock();
	return res;
}

#endif 
