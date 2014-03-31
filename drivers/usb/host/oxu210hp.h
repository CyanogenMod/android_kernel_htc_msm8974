
#define OXU_DEVICEID			0x00
	#define OXU_REV_MASK		0xffff0000
	#define OXU_REV_SHIFT		16
	#define OXU_REV_2100		0x2100
	#define OXU_BO_SHIFT		8
	#define OXU_BO_MASK		(0x3 << OXU_BO_SHIFT)
	#define OXU_MAJ_REV_SHIFT	4
	#define OXU_MAJ_REV_MASK	(0xf << OXU_MAJ_REV_SHIFT)
	#define OXU_MIN_REV_SHIFT	0
	#define OXU_MIN_REV_MASK	(0xf << OXU_MIN_REV_SHIFT)
#define OXU_HOSTIFCONFIG		0x04
#define OXU_SOFTRESET			0x08
	#define OXU_SRESET		(1 << 0)

#define OXU_PIOBURSTREADCTRL		0x0C

#define OXU_CHIPIRQSTATUS		0x10
#define OXU_CHIPIRQEN_SET		0x14
#define OXU_CHIPIRQEN_CLR		0x18
	#define OXU_USBSPHLPWUI		0x00000080
	#define OXU_USBOTGLPWUI		0x00000040
	#define OXU_USBSPHI		0x00000002
	#define OXU_USBOTGI		0x00000001

#define OXU_CLKCTRL_SET			0x1C
	#define OXU_SYSCLKEN		0x00000008
	#define OXU_USBSPHCLKEN		0x00000002
	#define OXU_USBOTGCLKEN		0x00000001

#define OXU_ASO				0x68
	#define OXU_SPHPOEN		0x00000100
	#define OXU_OVRCCURPUPDEN	0x00000800
	#define OXU_ASO_OP		(1 << 10)
	#define OXU_COMPARATOR		0x000004000

#define OXU_USBMODE			0x1A8
	#define OXU_VBPS		0x00000020
	#define OXU_ES_LITTLE		0x00000000
	#define OXU_CM_HOST_ONLY	0x00000003


#define EHCI_TUNE_CERR		3	
#define EHCI_TUNE_RL_HS		4	
#define EHCI_TUNE_RL_TT		0
#define EHCI_TUNE_MULT_HS	1	
#define EHCI_TUNE_MULT_TT	1
#define EHCI_TUNE_FLS		2	

struct oxu_hcd;


struct ehci_caps {
	u32		hc_capbase;
#define HC_LENGTH(p)		(((p)>>00)&0x00ff)	
#define HC_VERSION(p)		(((p)>>16)&0xffff)	
	u32		hcs_params;     
#define HCS_DEBUG_PORT(p)	(((p)>>20)&0xf)	
#define HCS_INDICATOR(p)	((p)&(1 << 16))	
#define HCS_N_CC(p)		(((p)>>12)&0xf)	
#define HCS_N_PCC(p)		(((p)>>8)&0xf)	
#define HCS_PORTROUTED(p)	((p)&(1 << 7))	
#define HCS_PPC(p)		((p)&(1 << 4))	
#define HCS_N_PORTS(p)		(((p)>>0)&0xf)	

	u32		hcc_params;      
#define HCC_EXT_CAPS(p)		(((p)>>8)&0xff)	
#define HCC_ISOC_CACHE(p)       ((p)&(1 << 7))  
#define HCC_ISOC_THRES(p)       (((p)>>4)&0x7)  
#define HCC_CANPARK(p)		((p)&(1 << 2))  
#define HCC_PGM_FRAMELISTLEN(p) ((p)&(1 << 1))  
#define HCC_64BIT_ADDR(p)       ((p)&(1))       
	u8		portroute[8];	 
} __attribute__ ((packed));


struct ehci_regs {
	
	u32		command;
#define CMD_PARK	(1<<11)		
#define CMD_PARK_CNT(c)	(((c)>>8)&3)	
#define CMD_LRESET	(1<<7)		
#define CMD_IAAD	(1<<6)		
#define CMD_ASE		(1<<5)		
#define CMD_PSE		(1<<4)		
#define CMD_RESET	(1<<1)		
#define CMD_RUN		(1<<0)		

	
	u32		status;
#define STS_ASS		(1<<15)		
#define STS_PSS		(1<<14)		
#define STS_RECL	(1<<13)		
#define STS_HALT	(1<<12)		
	
#define STS_IAA		(1<<5)		
#define STS_FATAL	(1<<4)		
#define STS_FLR		(1<<3)		
#define STS_PCD		(1<<2)		
#define STS_ERR		(1<<1)		
#define STS_INT		(1<<0)		

#define INTR_MASK (STS_IAA | STS_FATAL | STS_PCD | STS_ERR | STS_INT)

	
	u32		intr_enable;

	
	u32		frame_index;	
	
	u32		segment;	
	
	u32		frame_list;	
	
	u32		async_next;	

	u32		reserved[9];

	
	u32		configured_flag;
#define FLAG_CF		(1<<0)		

	
	u32		port_status[0];	
#define PORT_WKOC_E	(1<<22)		
#define PORT_WKDISC_E	(1<<21)		
#define PORT_WKCONN_E	(1<<20)		
#define PORT_LED_OFF	(0<<14)
#define PORT_LED_AMBER	(1<<14)
#define PORT_LED_GREEN	(2<<14)
#define PORT_LED_MASK	(3<<14)
#define PORT_OWNER	(1<<13)		
#define PORT_POWER	(1<<12)		
#define PORT_USB11(x) (((x)&(3<<10)) == (1<<10))	
#define PORT_RESET	(1<<8)		
#define PORT_SUSPEND	(1<<7)		
#define PORT_RESUME	(1<<6)		
#define PORT_OCC	(1<<5)		
#define PORT_OC		(1<<4)		
#define PORT_PEC	(1<<3)		
#define PORT_PE		(1<<2)		
#define PORT_CSC	(1<<1)		
#define PORT_CONNECT	(1<<0)		
#define PORT_RWC_BITS   (PORT_CSC | PORT_PEC | PORT_OCC)
} __attribute__ ((packed));

struct ehci_dbg_port {
	u32	control;
#define DBGP_OWNER	(1<<30)
#define DBGP_ENABLED	(1<<28)
#define DBGP_DONE	(1<<16)
#define DBGP_INUSE	(1<<10)
#define DBGP_ERRCODE(x)	(((x)>>7)&0x07)
#	define DBGP_ERR_BAD	1
#	define DBGP_ERR_SIGNAL	2
#define DBGP_ERROR	(1<<6)
#define DBGP_GO		(1<<5)
#define DBGP_OUT	(1<<4)
#define DBGP_LEN(x)	(((x)>>0)&0x0f)
	u32	pids;
#define DBGP_PID_GET(x)		(((x)>>16)&0xff)
#define DBGP_PID_SET(data, tok)	(((data)<<8)|(tok))
	u32	data03;
	u32	data47;
	u32	address;
#define DBGP_EPADDR(dev, ep)	(((dev)<<8)|(ep))
} __attribute__ ((packed));


#define	QTD_NEXT(dma)	cpu_to_le32((u32)dma)

struct ehci_qtd {
	
	__le32			hw_next;		
	__le32			hw_alt_next;		
	__le32			hw_token;		
#define	QTD_TOGGLE	(1 << 31)	
#define	QTD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	QTD_IOC		(1 << 15)	
#define	QTD_CERR(tok)	(((tok)>>10) & 0x3)
#define	QTD_PID(tok)	(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE	(1 << 7)	
#define	QTD_STS_HALT	(1 << 6)	
#define	QTD_STS_DBE	(1 << 5)	
#define	QTD_STS_BABBLE	(1 << 4)	
#define	QTD_STS_XACT	(1 << 3)	
#define	QTD_STS_MMF	(1 << 2)	
#define	QTD_STS_STS	(1 << 1)	
#define	QTD_STS_PING	(1 << 0)	
	__le32			hw_buf[5];		
	__le32			hw_buf_hi[5];		

	
	dma_addr_t		qtd_dma;		
	struct list_head	qtd_list;		
	struct urb		*urb;			
	size_t			length;			

	u32			qtd_buffer_len;
	void			*buffer;
	dma_addr_t		buffer_dma;
	void			*transfer_buffer;
	void			*transfer_dma;
} __attribute__ ((aligned(32)));

#define QTD_MASK cpu_to_le32 (~0x1f)

#define IS_SHORT_READ(token) (QTD_LENGTH(token) != 0 && QTD_PID(token) == 1)

#define Q_NEXT_TYPE(dma) ((dma) & cpu_to_le32 (3 << 1))

#define Q_TYPE_QH	cpu_to_le32 (1 << 1)

#define	QH_NEXT(dma)	(cpu_to_le32(((u32)dma)&~0x01f)|Q_TYPE_QH)

#define	EHCI_LIST_END	cpu_to_le32(1) 

union ehci_shadow {
	struct ehci_qh		*qh;		
	__le32			*hw_next;	
	void			*ptr;
};


struct ehci_qh {
	
	__le32			hw_next;	 
	__le32			hw_info1;	
#define	QH_HEAD		0x00008000
	__le32			hw_info2;	
#define	QH_SMASK	0x000000ff
#define	QH_CMASK	0x0000ff00
#define	QH_HUBADDR	0x007f0000
#define	QH_HUBPORT	0x3f800000
#define	QH_MULT		0xc0000000
	__le32			hw_current;	 

	
	__le32			hw_qtd_next;
	__le32			hw_alt_next;
	__le32			hw_token;
	__le32			hw_buf[5];
	__le32			hw_buf_hi[5];

	
	dma_addr_t		qh_dma;		
	union ehci_shadow	qh_next;	
	struct list_head	qtd_list;	
	struct ehci_qtd		*dummy;
	struct ehci_qh		*reclaim;	

	struct oxu_hcd		*oxu;
	struct kref		kref;
	unsigned		stamp;

	u8			qh_state;
#define	QH_STATE_LINKED		1		
#define	QH_STATE_UNLINK		2		
#define	QH_STATE_IDLE		3		
#define	QH_STATE_UNLINK_WAIT	4		
#define	QH_STATE_COMPLETING	5		

	
	u8			usecs;		
	u8			gap_uf;		
	u8			c_usecs;	
	u16			tt_usecs;	
	unsigned short		period;		
	unsigned short		start;		
#define NO_FRAME ((unsigned short)~0)			
	struct usb_device	*dev;		
} __attribute__ ((aligned(32)));


#define OXU_OTG_CORE_OFFSET	0x00400
#define OXU_OTG_CAP_OFFSET	(OXU_OTG_CORE_OFFSET + 0x100)
#define OXU_SPH_CORE_OFFSET	0x00800
#define OXU_SPH_CAP_OFFSET	(OXU_SPH_CORE_OFFSET + 0x100)

#define OXU_OTG_MEM		0xE000
#define OXU_SPH_MEM		0x16000

#define	DEFAULT_I_TDPS		1024
#define QHEAD_NUM		16
#define QTD_NUM			32
#define SITD_NUM		8
#define MURB_NUM		8

#define BUFFER_NUM		8
#define BUFFER_SIZE		512

struct oxu_info {
	struct usb_hcd *hcd[2];
};

struct oxu_buf {
	u8			buffer[BUFFER_SIZE];
} __attribute__ ((aligned(BUFFER_SIZE)));

struct oxu_onchip_mem {
	struct oxu_buf		db_pool[BUFFER_NUM];

	u32			frame_list[DEFAULT_I_TDPS];
	struct ehci_qh		qh_pool[QHEAD_NUM];
	struct ehci_qtd		qtd_pool[QTD_NUM];
} __attribute__ ((aligned(4 << 10)));

#define	EHCI_MAX_ROOT_PORTS	15		

struct oxu_murb {
	struct urb		urb;
	struct urb		*main;
	u8			last;
};

struct oxu_hcd {				
	unsigned int		is_otg:1;

	u8			qh_used[QHEAD_NUM];
	u8			qtd_used[QTD_NUM];
	u8			db_used[BUFFER_NUM];
	u8			murb_used[MURB_NUM];

	struct oxu_onchip_mem	__iomem *mem;
	spinlock_t		mem_lock;

	struct timer_list	urb_timer;

	struct ehci_caps __iomem *caps;
	struct ehci_regs __iomem *regs;

	__u32			hcs_params;	
	spinlock_t		lock;

	
	struct ehci_qh		*async;
	struct ehci_qh		*reclaim;
	unsigned		reclaim_ready:1;
	unsigned		scanning:1;

	
	unsigned		periodic_size;
	__le32			*periodic;	
	dma_addr_t		periodic_dma;
	unsigned		i_thresh;	

	union ehci_shadow	*pshadow;	
	int			next_uframe;	
	unsigned		periodic_sched;	

	
	unsigned long		reset_done[EHCI_MAX_ROOT_PORTS];
	
	unsigned long		bus_suspended;	
	unsigned long		companion_ports;

	struct timer_list	watchdog;
	unsigned long		actions;
	unsigned		stamp;
	unsigned long		next_statechange;
	u32			command;

	
	struct list_head	urb_list;	
	struct oxu_murb		*murb_pool;	
	unsigned urb_len;

	u8			sbrn;		
};

#define EHCI_IAA_JIFFIES	(HZ/100)	
#define EHCI_IO_JIFFIES	 	(HZ/10)		
#define EHCI_ASYNC_JIFFIES      (HZ/20)		
#define EHCI_SHRINK_JIFFIES     (HZ/200)	

enum ehci_timer_action {
	TIMER_IO_WATCHDOG,
	TIMER_IAA_WATCHDOG,
	TIMER_ASYNC_SHRINK,
	TIMER_ASYNC_OFF,
};

#include <linux/oxu210hp.h>
