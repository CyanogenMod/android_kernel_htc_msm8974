 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */
 

#ifndef LINUX_ATMDEV_H
#define LINUX_ATMDEV_H


#include <linux/atmapi.h>
#include <linux/atm.h>
#include <linux/atmioc.h>


#define ESI_LEN		6

#define ATM_OC3_PCR	(155520000/270*260/8/53)
#define ATM_25_PCR	((25600000/8-8000)/54)
			
#define ATM_OC12_PCR	(622080000/1080*1040/8/53)
#define ATM_DS3_PCR	(8000*12)
			


#define __AAL_STAT_ITEMS \
    __HANDLE_ITEM(tx);			 \
    __HANDLE_ITEM(tx_err);		 \
    __HANDLE_ITEM(rx);			 \
    __HANDLE_ITEM(rx_err);		 \
    __HANDLE_ITEM(rx_drop);		

struct atm_aal_stats {
#define __HANDLE_ITEM(i) int i
	__AAL_STAT_ITEMS
#undef __HANDLE_ITEM
};


struct atm_dev_stats {
	struct atm_aal_stats aal0;
	struct atm_aal_stats aal34;
	struct atm_aal_stats aal5;
} __ATM_API_ALIGN;


#define ATM_GETLINKRATE	_IOW('a',ATMIOC_ITF+1,struct atmif_sioc)
					
#define ATM_GETNAMES	_IOW('a',ATMIOC_ITF+3,struct atm_iobuf)
					
#define ATM_GETTYPE	_IOW('a',ATMIOC_ITF+4,struct atmif_sioc)
					
#define ATM_GETESI	_IOW('a',ATMIOC_ITF+5,struct atmif_sioc)
					
#define ATM_GETADDR	_IOW('a',ATMIOC_ITF+6,struct atmif_sioc)
					
#define ATM_RSTADDR	_IOW('a',ATMIOC_ITF+7,struct atmif_sioc)
					
#define ATM_ADDADDR	_IOW('a',ATMIOC_ITF+8,struct atmif_sioc)
					
#define ATM_DELADDR	_IOW('a',ATMIOC_ITF+9,struct atmif_sioc)
					
#define ATM_GETCIRANGE	_IOW('a',ATMIOC_ITF+10,struct atmif_sioc)
					
#define ATM_SETCIRANGE	_IOW('a',ATMIOC_ITF+11,struct atmif_sioc)
					
#define ATM_SETESI	_IOW('a',ATMIOC_ITF+12,struct atmif_sioc)
					
#define ATM_SETESIF	_IOW('a',ATMIOC_ITF+13,struct atmif_sioc)
					
#define ATM_ADDLECSADDR	_IOW('a', ATMIOC_ITF+14, struct atmif_sioc)
					
#define ATM_DELLECSADDR	_IOW('a', ATMIOC_ITF+15, struct atmif_sioc)
					
#define ATM_GETLECSADDR	_IOW('a', ATMIOC_ITF+16, struct atmif_sioc)
					

#define ATM_GETSTAT	_IOW('a',ATMIOC_SARCOM+0,struct atmif_sioc)
					
#define ATM_GETSTATZ	_IOW('a',ATMIOC_SARCOM+1,struct atmif_sioc)
					
#define ATM_GETLOOP	_IOW('a',ATMIOC_SARCOM+2,struct atmif_sioc)
					
#define ATM_SETLOOP	_IOW('a',ATMIOC_SARCOM+3,struct atmif_sioc)
					
#define ATM_QUERYLOOP	_IOW('a',ATMIOC_SARCOM+4,struct atmif_sioc)
					
#define ATM_SETSC	_IOW('a',ATMIOC_SPECIAL+1,int)
					
#define ATM_SETBACKEND	_IOW('a',ATMIOC_SPECIAL+2,atm_backend_t)
					
#define ATM_NEWBACKENDIF _IOW('a',ATMIOC_SPECIAL+3,atm_backend_t)
					
#define ATM_ADDPARTY  	_IOW('a', ATMIOC_SPECIAL+4,struct atm_iobuf)
 					
#ifdef CONFIG_COMPAT
#define COMPAT_ATM_ADDPARTY  	_IOW('a', ATMIOC_SPECIAL+4,struct compat_atm_iobuf)
#endif
#define ATM_DROPPARTY 	_IOW('a', ATMIOC_SPECIAL+5,int)
					

#define ATM_BACKEND_RAW		0	
#define ATM_BACKEND_PPP		1	
#define ATM_BACKEND_BR2684	2	

#define ATM_ITFTYP_LEN	8	


#define __ATM_LM_NONE	0	
#define __ATM_LM_AAL	1	
#define __ATM_LM_ATM	2	
#define __ATM_LM_PHY	8	
#define __ATM_LM_ANALOG 16	

#define __ATM_LM_MKLOC(n)	((n))	    
#define __ATM_LM_MKRMT(n)	((n) << 8)  

#define __ATM_LM_XTLOC(n)	((n) & 0xff)
#define __ATM_LM_XTRMT(n)	(((n) >> 8) & 0xff)

#define ATM_LM_NONE	0	

#define ATM_LM_LOC_AAL	__ATM_LM_MKLOC(__ATM_LM_AAL)
#define ATM_LM_LOC_ATM	__ATM_LM_MKLOC(__ATM_LM_ATM)
#define ATM_LM_LOC_PHY	__ATM_LM_MKLOC(__ATM_LM_PHY)
#define ATM_LM_LOC_ANALOG __ATM_LM_MKLOC(__ATM_LM_ANALOG)

#define ATM_LM_RMT_AAL	__ATM_LM_MKRMT(__ATM_LM_AAL)
#define ATM_LM_RMT_ATM	__ATM_LM_MKRMT(__ATM_LM_ATM)
#define ATM_LM_RMT_PHY	__ATM_LM_MKRMT(__ATM_LM_PHY)
#define ATM_LM_RMT_ANALOG __ATM_LM_MKRMT(__ATM_LM_ANALOG)



struct atm_iobuf {
	int length;
	void __user *buffer;
};


#define ATM_CI_MAX      -1              
 
struct atm_cirange {
	signed char	vpi_bits;	
	signed char	vci_bits;	
};


#define ATM_SC_RX	1024		
#define ATM_SC_TX	2048		

#define ATM_BACKLOG_DEFAULT 32 


#define ATM_MF_IMMED	 1	
#define ATM_MF_INC_RSV	 2	
#define ATM_MF_INC_SHP	 4	
#define ATM_MF_DEC_RSV	 8	
#define ATM_MF_DEC_SHP	16	
#define ATM_MF_BWD	32	

#define ATM_MF_SET	(ATM_MF_INC_RSV | ATM_MF_INC_SHP | ATM_MF_DEC_RSV | \
			  ATM_MF_DEC_SHP | ATM_MF_BWD)


#define ATM_VS_IDLE	0	
#define ATM_VS_CONNECTED 1	
#define ATM_VS_CLOSING	2	
#define ATM_VS_LISTEN	3	
#define ATM_VS_INUSE	4	
#define ATM_VS_BOUND	5	

#define ATM_VS2TXT_MAP \
    "IDLE", "CONNECTED", "CLOSING", "LISTEN", "INUSE", "BOUND"

#define ATM_VF2TXT_MAP \
    "ADDR",	"READY",	"PARTIAL",	"REGIS", \
    "RELEASED", "HASQOS",	"LISTEN",	"META", \
    "256",	"512",		"1024",		"2048", \
    "SESSION",	"HASSAP",	"BOUND",	"CLOSE"


#ifdef __KERNEL__

#include <linux/wait.h> 
#include <linux/time.h> 
#include <linux/net.h>
#include <linux/bug.h>
#include <linux/skbuff.h> 
#include <linux/uio.h>
#include <net/sock.h>
#include <linux/atomic.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>

extern struct proc_dir_entry *atm_proc_root;
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
struct compat_atm_iobuf {
	int length;
	compat_uptr_t buffer;
};
#endif

struct k_atm_aal_stats {
#define __HANDLE_ITEM(i) atomic_t i
	__AAL_STAT_ITEMS
#undef __HANDLE_ITEM
};


struct k_atm_dev_stats {
	struct k_atm_aal_stats aal0;
	struct k_atm_aal_stats aal34;
	struct k_atm_aal_stats aal5;
};

struct device;

enum {
	ATM_VF_ADDR,		
	ATM_VF_READY,		
	ATM_VF_PARTIAL,		
	ATM_VF_REGIS,		
	ATM_VF_BOUND,		
	ATM_VF_RELEASED,	
	ATM_VF_HASQOS,		
	ATM_VF_LISTEN,		
	ATM_VF_META,		
	ATM_VF_SESSION,		
	ATM_VF_HASSAP,		
	ATM_VF_CLOSE,		
	ATM_VF_WAITING,		
	ATM_VF_IS_CLIP,		
};


#define ATM_VF2VS(flags) \
    (test_bit(ATM_VF_READY,&(flags)) ? ATM_VS_CONNECTED : \
     test_bit(ATM_VF_RELEASED,&(flags)) ? ATM_VS_CLOSING : \
     test_bit(ATM_VF_LISTEN,&(flags)) ? ATM_VS_LISTEN : \
     test_bit(ATM_VF_REGIS,&(flags)) ? ATM_VS_INUSE : \
     test_bit(ATM_VF_BOUND,&(flags)) ? ATM_VS_BOUND : ATM_VS_IDLE)


enum {
	ATM_DF_REMOVED,		
};


#define ATM_PHY_SIG_LOST    0	
#define ATM_PHY_SIG_UNKNOWN 1	
#define ATM_PHY_SIG_FOUND   2	

#define ATM_ATMOPT_CLP	1	

struct atm_vcc {
	
	struct sock	sk;
	unsigned long	flags;		
	short		vpi;		
					
	int 		vci;
	unsigned long	aal_options;	
	unsigned long	atm_options;	
	struct atm_dev	*dev;		
	struct atm_qos	qos;		
	struct atm_sap	sap;		
	void (*push)(struct atm_vcc *vcc,struct sk_buff *skb);
	void (*pop)(struct atm_vcc *vcc,struct sk_buff *skb); 
	int (*push_oam)(struct atm_vcc *vcc,void *cell);
	int (*send)(struct atm_vcc *vcc,struct sk_buff *skb);
	void		*dev_data;	
	void		*proto_data;	
	struct k_atm_aal_stats *stats;	
	
	short		itf;		
	struct sockaddr_atmsvc local;
	struct sockaddr_atmsvc remote;
	
	struct atm_vcc	*session;	
	
	void		*user_back;	
					
					
};

static inline struct atm_vcc *atm_sk(struct sock *sk)
{
	return (struct atm_vcc *)sk;
}

static inline struct atm_vcc *ATM_SD(struct socket *sock)
{
	return atm_sk(sock->sk);
}

static inline struct sock *sk_atm(struct atm_vcc *vcc)
{
	return (struct sock *)vcc;
}

struct atm_dev_addr {
	struct sockaddr_atmsvc addr;	
	struct list_head entry;		
};

enum atm_addr_type_t { ATM_ADDR_LOCAL, ATM_ADDR_LECS };

struct atm_dev {
	const struct atmdev_ops *ops;	
	const struct atmphy_ops *phy;	
					
	const char	*type;		
	int		number;		
	void		*dev_data;	
	void		*phy_data;	
	unsigned long	flags;		
	struct list_head local;		
	struct list_head lecs;		
	unsigned char	esi[ESI_LEN];	
	struct atm_cirange ci_range;	
	struct k_atm_dev_stats stats;	
	char		signal;		
	int		link_rate;	
	atomic_t	refcnt;		
	spinlock_t	lock;		
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_entry; 
	char *proc_name;		
#endif
	struct device class_dev;	
	struct list_head dev_list;	
};

 

#define ATM_OF_IMMED  1		
#define ATM_OF_INRATE 2		



struct atmdev_ops { 
	void (*dev_close)(struct atm_dev *dev);
	int (*open)(struct atm_vcc *vcc);
	void (*close)(struct atm_vcc *vcc);
	int (*ioctl)(struct atm_dev *dev,unsigned int cmd,void __user *arg);
#ifdef CONFIG_COMPAT
	int (*compat_ioctl)(struct atm_dev *dev,unsigned int cmd,
			    void __user *arg);
#endif
	int (*getsockopt)(struct atm_vcc *vcc,int level,int optname,
	    void __user *optval,int optlen);
	int (*setsockopt)(struct atm_vcc *vcc,int level,int optname,
	    void __user *optval,unsigned int optlen);
	int (*send)(struct atm_vcc *vcc,struct sk_buff *skb);
	int (*send_oam)(struct atm_vcc *vcc,void *cell,int flags);
	void (*phy_put)(struct atm_dev *dev,unsigned char value,
	    unsigned long addr);
	unsigned char (*phy_get)(struct atm_dev *dev,unsigned long addr);
	int (*change_qos)(struct atm_vcc *vcc,struct atm_qos *qos,int flags);
	int (*proc_read)(struct atm_dev *dev,loff_t *pos,char *page);
	struct module *owner;
};

struct atmphy_ops {
	int (*start)(struct atm_dev *dev);
	int (*ioctl)(struct atm_dev *dev,unsigned int cmd,void __user *arg);
	void (*interrupt)(struct atm_dev *dev);
	int (*stop)(struct atm_dev *dev);
};

struct atm_skb_data {
	struct atm_vcc	*vcc;		
	unsigned long	atm_options;	
};

#define VCC_HTABLE_SIZE 32

extern struct hlist_head vcc_hash[VCC_HTABLE_SIZE];
extern rwlock_t vcc_sklist_lock;

#define ATM_SKB(skb) (((struct atm_skb_data *) (skb)->cb))

struct atm_dev *atm_dev_register(const char *type, struct device *parent,
				 const struct atmdev_ops *ops,
				 int number, 
				 unsigned long *flags);
struct atm_dev *atm_dev_lookup(int number);
void atm_dev_deregister(struct atm_dev *dev);

void atm_dev_signal_change(struct atm_dev *dev, char signal);

void vcc_insert_socket(struct sock *sk);

void atm_dev_release_vccs(struct atm_dev *dev);


static inline void atm_force_charge(struct atm_vcc *vcc,int truesize)
{
	atomic_add(truesize, &sk_atm(vcc)->sk_rmem_alloc);
}


static inline void atm_return(struct atm_vcc *vcc,int truesize)
{
	atomic_sub(truesize, &sk_atm(vcc)->sk_rmem_alloc);
}


static inline int atm_may_send(struct atm_vcc *vcc,unsigned int size)
{
	return (size + atomic_read(&sk_atm(vcc)->sk_wmem_alloc)) <
	       sk_atm(vcc)->sk_sndbuf;
}


static inline void atm_dev_hold(struct atm_dev *dev)
{
	atomic_inc(&dev->refcnt);
}


static inline void atm_dev_put(struct atm_dev *dev)
{
	if (atomic_dec_and_test(&dev->refcnt)) {
		BUG_ON(!test_bit(ATM_DF_REMOVED, &dev->flags));
		if (dev->ops->dev_close)
			dev->ops->dev_close(dev);
		put_device(&dev->class_dev);
	}
}


int atm_charge(struct atm_vcc *vcc,int truesize);
struct sk_buff *atm_alloc_charge(struct atm_vcc *vcc,int pdu_size,
    gfp_t gfp_flags);
int atm_pcr_goal(const struct atm_trafprm *tp);

void vcc_release_async(struct atm_vcc *vcc, int reply);

struct atm_ioctl {
	struct module *owner;
	int (*ioctl)(struct socket *, unsigned int cmd, unsigned long arg);
	struct list_head list;
};

void register_atm_ioctl(struct atm_ioctl *);

void deregister_atm_ioctl(struct atm_ioctl *);


int register_atmdevice_notifier(struct notifier_block *nb);
void unregister_atmdevice_notifier(struct notifier_block *nb);

#endif 

#endif
