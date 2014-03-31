/*
	drivers/net/ethernet/dec/tulip/tulip.h

	Copyright 2000,2001  The Linux Kernel Team
	Written/copyright 1994-2001 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	Please submit bugs to http://bugzilla.kernel.org/ .
*/

#ifndef __NET_TULIP_H__
#define __NET_TULIP_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/unaligned.h>



#define TULIP_DEBUG 1

#ifdef CONFIG_TULIP_MMIO
#define TULIP_BAR	1	
#else
#define TULIP_BAR	0	
#endif



struct tulip_chip_table {
	char *chip_name;
	int io_size;
	int valid_intrs;	
	int flags;
	void (*media_timer) (unsigned long);
	work_func_t media_task;
};


enum tbl_flag {
	HAS_MII			= 0x00001,
	HAS_MEDIA_TABLE		= 0x00002,
	CSR12_IN_SROM		= 0x00004,
	ALWAYS_CHECK_MII	= 0x00008,
	HAS_ACPI		= 0x00010,
	MC_HASH_ONLY		= 0x00020, 
	HAS_PNICNWAY		= 0x00080,
	HAS_NWAY		= 0x00040, 
	HAS_INTR_MITIGATION	= 0x00100,
	IS_ASIX			= 0x00200,
	HAS_8023X		= 0x00400,
	COMET_MAC_ADDR		= 0x00800,
	HAS_PCI_MWI		= 0x01000,
	HAS_PHY_IRQ		= 0x02000,
	HAS_SWAPPED_SEEPROM	= 0x04000,
	NEEDS_FAKE_MEDIA_TABLE	= 0x08000,
	COMET_PM		= 0x10000,
};


enum chips {
	DC21040 = 0,
	DC21041 = 1,
	DC21140 = 2,
	DC21142 = 3, DC21143 = 3,
	LC82C168,
	MX98713,
	MX98715,
	MX98725,
	AX88140,
	PNIC2,
	COMET,
	COMPEX9881,
	I21145,
	DM910X,
	CONEXANT,
};


enum MediaIs {
	MediaIsFD = 1,
	MediaAlwaysFD = 2,
	MediaIsMII = 4,
	MediaIsFx = 8,
	MediaIs100 = 16
};


enum tulip_offsets {
	CSR0 = 0,
	CSR1 = 0x08,
	CSR2 = 0x10,
	CSR3 = 0x18,
	CSR4 = 0x20,
	CSR5 = 0x28,
	CSR6 = 0x30,
	CSR7 = 0x38,
	CSR8 = 0x40,
	CSR9 = 0x48,
	CSR10 = 0x50,
	CSR11 = 0x58,
	CSR12 = 0x60,
	CSR13 = 0x68,
	CSR14 = 0x70,
	CSR15 = 0x78,
	CSR18 = 0x88,
	CSR19 = 0x8c,
	CSR20 = 0x90,
	CSR27 = 0xAC,
	CSR28 = 0xB0,
};

enum pci_cfg_driver_reg {
	CFDD = 0x40,
	CFDD_Sleep = (1 << 31),
	CFDD_Snooze = (1 << 30),
};

#define RxPollInt (RxIntr|RxNoBuf|RxDied|RxJabber)

enum status_bits {
	TimerInt = 0x800,
	SystemError = 0x2000,
	TPLnkFail = 0x1000,
	TPLnkPass = 0x10,
	NormalIntr = 0x10000,
	AbnormalIntr = 0x8000,
	RxJabber = 0x200,
	RxDied = 0x100,
	RxNoBuf = 0x80,
	RxIntr = 0x40,
	TxFIFOUnderflow = 0x20,
	RxErrIntr = 0x10,
	TxJabber = 0x08,
	TxNoBuf = 0x04,
	TxDied = 0x02,
	TxIntr = 0x01,
};

#define CSR5_TS	0x00700000
#define CSR5_RS	0x000e0000

enum tulip_mode_bits {
	TxThreshold		= (1 << 22),
	FullDuplex		= (1 << 9),
	TxOn			= 0x2000,
	AcceptBroadcast		= 0x0100,
	AcceptAllMulticast	= 0x0080,
	AcceptAllPhys		= 0x0040,
	AcceptRunt		= 0x0008,
	RxOn			= 0x0002,
	RxTx			= (TxOn | RxOn),
};


enum tulip_busconfig_bits {
	MWI			= (1 << 24),
	MRL			= (1 << 23),
	MRM			= (1 << 21),
	CALShift		= 14,
	BurstLenShift		= 8,
};


struct tulip_rx_desc {
	__le32 status;
	__le32 length;
	__le32 buffer1;
	__le32 buffer2;
};


struct tulip_tx_desc {
	__le32 status;
	__le32 length;
	__le32 buffer1;
	__le32 buffer2;		
};


enum desc_status_bits {
	DescOwned    = 0x80000000,
	DescWholePkt = 0x60000000,
	DescEndPkt   = 0x40000000,
	DescStartPkt = 0x20000000,
	DescEndRing  = 0x02000000,
	DescUseLink  = 0x01000000,

	RxDescErrorSummary = 0x8000,
	RxDescCRCError = 0x0002,
	RxDescCollisionSeen = 0x0040,

	RxDescFrameTooLong = 0x0080,
	RxDescRunt = 0x0800,
	RxDescDescErr = 0x4000,
	RxWholePkt   = 0x00000300,
	RxLengthOver2047 = 0x38000010
};


enum t21143_csr6_bits {
	csr6_sc = (1<<31),
	csr6_ra = (1<<30),
	csr6_ign_dest_msb = (1<<26),
	csr6_mbo = (1<<25),
	csr6_scr = (1<<24),  
	csr6_pcs = (1<<23),  
	csr6_ttm = (1<<22),  
	csr6_sf = (1<<21),   
	csr6_hbd = (1<<19),  
	csr6_ps = (1<<18),   
	csr6_ca = (1<<17),   
	csr6_trh = (1<<15),  
	csr6_trl = (1<<14),  



	csr6_fc = (1<<12),   
	csr6_om_int_loop = (1<<10), 
	csr6_om_ext_loop = (1<<11), 
	
	csr6_fd = (1<<9),    
	csr6_pm = (1<<7),    
	csr6_pr = (1<<6),    
	csr6_sb = (1<<5),    
	csr6_if = (1<<4),    
	csr6_pb = (1<<3),    
	csr6_ho = (1<<2),    
	csr6_hp = (1<<0),    

	csr6_mask_capture = (csr6_sc | csr6_ca),
	csr6_mask_defstate = (csr6_mask_capture | csr6_mbo),
	csr6_mask_hdcap = (csr6_mask_defstate | csr6_hbd | csr6_ps),
	csr6_mask_hdcaptt = (csr6_mask_hdcap  | csr6_trh | csr6_trl),
	csr6_mask_fullcap = (csr6_mask_hdcaptt | csr6_fd),
	csr6_mask_fullpromisc = (csr6_pr | csr6_pm),
	csr6_mask_filters = (csr6_hp | csr6_ho | csr6_if),
	csr6_mask_100bt = (csr6_scr | csr6_pcs | csr6_hbd),
};

enum tulip_comet_csr13_bits {
        comet_csr13_linkoffe = (1 << 17),
        comet_csr13_linkone = (1 << 16),
        comet_csr13_wfre = (1 << 10),
        comet_csr13_mpre = (1 << 9),
        comet_csr13_lsce = (1 << 8),
        comet_csr13_wfr = (1 << 2),
        comet_csr13_mpr = (1 << 1),
        comet_csr13_lsc = (1 << 0),
};

enum tulip_comet_csr18_bits {
        comet_csr18_pmes_sticky = (1 << 24),
        comet_csr18_pm_mode = (1 << 19),
        comet_csr18_apm_mode = (1 << 18),
        comet_csr18_d3a = (1 << 7)
};

enum tulip_comet_csr20_bits {
        comet_csr20_pmes = (1 << 15),
};


#define TX_RING_SIZE	32
#define RX_RING_SIZE	128
#define MEDIA_MASK     31

#define PKT_BUF_SZ	(1536 + 4)	

#define TULIP_MIN_CACHE_LINE	8	

#if defined(__sparc__) || defined(__hppa__)
#define TULIP_MAX_CACHE_LINE	16	
#else
#define TULIP_MAX_CACHE_LINE	32	
#endif


#define DESC_RING_WRAP 0x02000000


#define EEPROM_SIZE 512 	


#define RUN_AT(x) (jiffies + (x))

#define get_u16(ptr) get_unaligned_le16((ptr))

struct medialeaf {
	u8 type;
	u8 media;
	unsigned char *leafdata;
};


struct mediatable {
	u16 defaultmedia;
	u8 leafcount;
	u8 csr12dir;		
	unsigned has_mii:1;
	unsigned has_nonmii:1;
	unsigned has_reset:6;
	u32 csr15dir;
	u32 csr15val;		
	struct medialeaf mleaf[0];
};


struct mediainfo {
	struct mediainfo *next;
	int info_type;
	int index;
	unsigned char *info;
};

struct ring_info {
	struct sk_buff	*skb;
	dma_addr_t	mapping;
};


struct tulip_private {
	const char *product_name;
	struct net_device *next_module;
	struct tulip_rx_desc *rx_ring;
	struct tulip_tx_desc *tx_ring;
	dma_addr_t rx_ring_dma;
	dma_addr_t tx_ring_dma;
	
	struct ring_info tx_buffers[TX_RING_SIZE];
	
	struct ring_info rx_buffers[RX_RING_SIZE];
	u16 setup_frame[96];	
	int chip_id;
	int revision;
	int flags;
	struct napi_struct napi;
	struct timer_list timer;	
	struct timer_list oom_timer;    
	u32 mc_filter[2];
	spinlock_t lock;
	spinlock_t mii_lock;
	unsigned int cur_rx, cur_tx;	
	unsigned int dirty_rx, dirty_tx;	

#ifdef 	CONFIG_TULIP_NAPI_HW_MITIGATION
        int mit_on;
#endif
	unsigned int full_duplex:1;	
	unsigned int full_duplex_lock:1;
	unsigned int fake_addr:1;	
	unsigned int default_port:4;	
	unsigned int media2:4;	
	unsigned int medialock:1;	
	unsigned int mediasense:1;	
	unsigned int nway:1, nwayset:1;		
	unsigned int timeout_recovery:1;
	unsigned int csr0;	
	unsigned int csr6;	
	unsigned char eeprom[EEPROM_SIZE];	
	void (*link_change) (struct net_device * dev, int csr5);
        struct ethtool_wolinfo wolinfo;        
	u16 sym_advertise, mii_advertise; 
	u16 lpar;		
	u16 advertising[4];
	signed char phys[4], mii_cnt;	
	struct mediatable *mtable;
	int cur_index;		
	int saved_if_port;
	struct pci_dev *pdev;
	int ttimer;
	int susp_rx;
	unsigned long nir;
	void __iomem *base_addr;
	int csr12_shadow;
	int pad0;		
	struct work_struct media_work;
	struct net_device *dev;
};


struct eeprom_fixup {
	char *name;
	unsigned char addr0;
	unsigned char addr1;
	unsigned char addr2;
	u16 newtable[32];	
};


extern u16 t21142_csr14[];
void t21142_media_task(struct work_struct *work);
void t21142_start_nway(struct net_device *dev);
void t21142_lnk_change(struct net_device *dev, int csr5);


void pnic2_lnk_change(struct net_device *dev, int csr5);
void pnic2_timer(unsigned long data);
void pnic2_start_nway(struct net_device *dev);
void pnic2_lnk_change(struct net_device *dev, int csr5);

void tulip_parse_eeprom(struct net_device *dev);
int tulip_read_eeprom(struct net_device *dev, int location, int addr_len);

extern unsigned int tulip_max_interrupt_work;
extern int tulip_rx_copybreak;
irqreturn_t tulip_interrupt(int irq, void *dev_instance);
int tulip_refill_rx(struct net_device *dev);
#ifdef CONFIG_TULIP_NAPI
int tulip_poll(struct napi_struct *napi, int budget);
#endif


int tulip_mdio_read(struct net_device *dev, int phy_id, int location);
void tulip_mdio_write(struct net_device *dev, int phy_id, int location, int value);
void tulip_select_media(struct net_device *dev, int startup);
int tulip_check_duplex(struct net_device *dev);
void tulip_find_mii (struct net_device *dev, int board_idx);

void pnic_do_nway(struct net_device *dev);
void pnic_lnk_change(struct net_device *dev, int csr5);
void pnic_timer(unsigned long data);

void tulip_media_task(struct work_struct *work);
void mxic_timer(unsigned long data);
void comet_timer(unsigned long data);

extern int tulip_debug;
extern const char * const medianame[];
extern const char tulip_media_cap[];
extern struct tulip_chip_table tulip_tbl[];
void oom_timer(unsigned long data);
extern u8 t21040_csr13[];

static inline void tulip_start_rxtx(struct tulip_private *tp)
{
	void __iomem *ioaddr = tp->base_addr;
	iowrite32(tp->csr6 | RxTx, ioaddr + CSR6);
	barrier();
	(void) ioread32(ioaddr + CSR6); 
}

static inline void tulip_stop_rxtx(struct tulip_private *tp)
{
	void __iomem *ioaddr = tp->base_addr;
	u32 csr6 = ioread32(ioaddr + CSR6);

	if (csr6 & RxTx) {
		unsigned i=1300/10;
		iowrite32(csr6 & ~RxTx, ioaddr + CSR6);
		barrier();
		while (--i && (ioread32(ioaddr + CSR5) & (CSR5_TS|CSR5_RS)))
			udelay(10);

		if (!i)
			netdev_dbg(tp->dev, "tulip_stop_rxtx() failed (CSR5 0x%x CSR6 0x%x)\n",
				   ioread32(ioaddr + CSR5),
				   ioread32(ioaddr + CSR6));
	}
}

static inline void tulip_restart_rxtx(struct tulip_private *tp)
{
	tulip_stop_rxtx(tp);
	udelay(5);
	tulip_start_rxtx(tp);
}

static inline void tulip_tx_timeout_complete(struct tulip_private *tp, void __iomem *ioaddr)
{
	
	tulip_restart_rxtx(tp);
	
	iowrite32(0, ioaddr + CSR1);

	tp->dev->stats.tx_errors++;
}

#endif 
