#ifndef FS_ENET_H
#define FS_ENET_H

#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/phy.h>
#include <linux/dma-mapping.h>

#include <linux/fs_enet_pd.h>
#include <asm/fs_pd.h>

#ifdef CONFIG_CPM1
#include <asm/cpm1.h>
#endif

#if defined(CONFIG_FS_ENET_HAS_FEC)
#include <asm/cpm.h>

#if defined(CONFIG_FS_ENET_MPC5121_FEC)
struct fec {
	u32 fec_reserved0;
	u32 fec_ievent;			
	u32 fec_imask;			
	u32 fec_reserved1;
	u32 fec_r_des_active;		
	u32 fec_x_des_active;		
	u32 fec_reserved2[3];
	u32 fec_ecntrl;			
	u32 fec_reserved3[6];
	u32 fec_mii_data;		
	u32 fec_mii_speed;		
	u32 fec_reserved4[7];
	u32 fec_mib_ctrlstat;		
	u32 fec_reserved5[7];
	u32 fec_r_cntrl;		
	u32 fec_reserved6[15];
	u32 fec_x_cntrl;		
	u32 fec_reserved7[7];
	u32 fec_addr_low;		
	u32 fec_addr_high;		
	u32 fec_opd;			
	u32 fec_reserved8[10];
	u32 fec_hash_table_high;	
	u32 fec_hash_table_low;		
	u32 fec_grp_hash_table_high;	
	u32 fec_grp_hash_table_low;	
	u32 fec_reserved9[7];
	u32 fec_x_wmrk;			
	u32 fec_reserved10;
	u32 fec_r_bound;		
	u32 fec_r_fstart;		
	u32 fec_reserved11[11];
	u32 fec_r_des_start;		
	u32 fec_x_des_start;		
	u32 fec_r_buff_size;		
	u32 fec_reserved12[26];
	u32 fec_dma_control;		
};
#endif

struct fec_info {
	struct fec __iomem *fecp;
	u32 mii_speed;
};
#endif

#ifdef CONFIG_CPM2
#include <asm/cpm2.h>
#endif

struct fs_ops {
	int (*setup_data)(struct net_device *dev);
	int (*allocate_bd)(struct net_device *dev);
	void (*free_bd)(struct net_device *dev);
	void (*cleanup_data)(struct net_device *dev);
	void (*set_multicast_list)(struct net_device *dev);
	void (*adjust_link)(struct net_device *dev);
	void (*restart)(struct net_device *dev);
	void (*stop)(struct net_device *dev);
	void (*napi_clear_rx_event)(struct net_device *dev);
	void (*napi_enable_rx)(struct net_device *dev);
	void (*napi_disable_rx)(struct net_device *dev);
	void (*rx_bd_done)(struct net_device *dev);
	void (*tx_kickstart)(struct net_device *dev);
	u32 (*get_int_events)(struct net_device *dev);
	void (*clear_int_events)(struct net_device *dev, u32 int_events);
	void (*ev_error)(struct net_device *dev, u32 int_events);
	int (*get_regs)(struct net_device *dev, void *p, int *sizep);
	int (*get_regs_len)(struct net_device *dev);
	void (*tx_restart)(struct net_device *dev);
};

struct phy_info {
	unsigned int id;
	const char *name;
	void (*startup) (struct net_device * dev);
	void (*shutdown) (struct net_device * dev);
	void (*ack_int) (struct net_device * dev);
};

#define MAX_MTU 1508		
#define MIN_MTU 46		
#define CRC_LEN 4

#define PKT_MAXBUF_SIZE		(MAX_MTU+ETH_HLEN+CRC_LEN)
#define PKT_MINBUF_SIZE		(MIN_MTU+ETH_HLEN+CRC_LEN)

#define PKT_MAXBLR_SIZE		((PKT_MAXBUF_SIZE + 31) & ~31)
#define ENET_RX_ALIGN  16
#define ENET_RX_FRSIZE L1_CACHE_ALIGN(PKT_MAXBUF_SIZE + ENET_RX_ALIGN - 1)

struct fs_enet_private {
	struct napi_struct napi;
	struct device *dev;	
	struct net_device *ndev;
	spinlock_t lock;	
	spinlock_t tx_lock;	
	struct fs_platform_info *fpi;
	const struct fs_ops *ops;
	int rx_ring, tx_ring;
	dma_addr_t ring_mem_addr;
	void __iomem *ring_base;
	struct sk_buff **rx_skbuff;
	struct sk_buff **tx_skbuff;
	cbd_t __iomem *rx_bd_base;	
	cbd_t __iomem *tx_bd_base;
	cbd_t __iomem *dirty_tx;	
	cbd_t __iomem *cur_rx;
	cbd_t __iomem *cur_tx;
	int tx_free;
	struct net_device_stats stats;
	struct timer_list phy_timer_list;
	const struct phy_info *phy;
	u32 msg_enable;
	struct mii_if_info mii_if;
	unsigned int last_mii_status;
	int interrupt;

	struct phy_device *phydev;
	int oldduplex, oldspeed, oldlink;	

	
	u32 ev_napi_rx;		
	u32 ev_rx;		
	u32 ev_tx;		
	u32 ev_err;		

	u16 bd_rx_empty;	
	u16 bd_rx_err;		

	union {
		struct {
			int idx;		
			void __iomem *fecp;	
			u32 hthi, htlo;		
		} fec;

		struct {
			int idx;		
			void __iomem *fccp;	
			void __iomem *ep;	
			void __iomem *fcccp;	
			void __iomem *mem;	
			u32 gaddrh, gaddrl;	
		} fcc;

		struct {
			int idx;		
			void __iomem *sccp;	
			void __iomem *ep;	
			u32 hthi, htlo;		
		} scc;

	};
};


void fs_init_bds(struct net_device *dev);
void fs_cleanup_bds(struct net_device *dev);


#define DRV_MODULE_NAME		"fs_enet"
#define PFX DRV_MODULE_NAME	": "
#define DRV_MODULE_VERSION	"1.0"
#define DRV_MODULE_RELDATE	"Aug 8, 2005"


int fs_enet_platform_init(void);
void fs_enet_platform_cleanup(void);


#if defined(CONFIG_CPM1)
#define __cbd_out32(addr, x)	__raw_writel(x, addr)
#define __cbd_out16(addr, x)	__raw_writew(x, addr)
#define __cbd_in32(addr)	__raw_readl(addr)
#define __cbd_in16(addr)	__raw_readw(addr)
#else
#define __cbd_out32(addr, x)	out_be32(addr, x)
#define __cbd_out16(addr, x)	out_be16(addr, x)
#define __cbd_in32(addr)	in_be32(addr)
#define __cbd_in16(addr)	in_be16(addr)
#endif

#define CBDW_SC(_cbd, _sc) 		__cbd_out16(&(_cbd)->cbd_sc, (_sc))
#define CBDW_DATLEN(_cbd, _datlen)	__cbd_out16(&(_cbd)->cbd_datlen, (_datlen))
#define CBDW_BUFADDR(_cbd, _bufaddr)	__cbd_out32(&(_cbd)->cbd_bufaddr, (_bufaddr))

#define CBDR_SC(_cbd) 			__cbd_in16(&(_cbd)->cbd_sc)
#define CBDR_DATLEN(_cbd)		__cbd_in16(&(_cbd)->cbd_datlen)
#define CBDR_BUFADDR(_cbd)		__cbd_in32(&(_cbd)->cbd_bufaddr)

#define CBDS_SC(_cbd, _sc) 		CBDW_SC(_cbd, CBDR_SC(_cbd) | (_sc))

#define CBDC_SC(_cbd, _sc) 		CBDW_SC(_cbd, CBDR_SC(_cbd) & ~(_sc))


extern const struct fs_ops fs_fec_ops;
extern const struct fs_ops fs_fcc_ops;
extern const struct fs_ops fs_scc_ops;


#endif
