/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : host
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* Refer to file ~/drivers/usb/host/ehci-dbg.h for copyright owners (kernel version 2.6.9)
* Code is modified for ST-Ericsson product 
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#ifndef	__PEHCI_H__
#define	__PEHCI_H__


#define	DRIVER_AUTHOR	"ST-ERICSSON	  "
#define	DRIVER_DESC "ISP1763 'Enhanced'	Host Controller	(EHCI) Driver"

#define	__ACTIVE		0x01
#define	__SLEEPY		0x02
#define	__SUSPEND		0x04
#define	__TRANSIENT		0x80

#define	USB_STATE_HALT		0
#define	USB_STATE_RUNNING	(__ACTIVE)
#define	USB_STATE_READY		(__ACTIVE|__SLEEPY)
#define	USB_STATE_QUIESCING	(__SUSPEND|__TRANSIENT|__ACTIVE)
#define	USB_STATE_RESUMING	(__SUSPEND|__TRANSIENT)
#define	USB_STATE_SUSPENDED	(__SUSPEND)

#define	HCD_MEMORY		0x0001
#define	HCD_USB2		0x0020
#define	HCD_USB11		0x0010

#define	HCD_IS_RUNNING(state) ((state) & __ACTIVE)
#define	HCD_IS_SUSPENDED(state)	((state) & __SUSPEND)


#define	HCD_IRQ			IRQ_GPIO(25)
#define	CMD_RESET		(1<<1)	
#define	CMD_RUN			(1<<0)	
#define	STS_PCD			(1<<2)	
#define	EHCI_STATE_UNLINK	0x8000	

#define	SETUP_PID		(2)
#define	OUT_PID			(0)
#define	IN_PID			(1)

#define	MULTI(x)		((x)<< 29)
#define	XFER_PER_UFRAME(x)	(((x) >> 29) & 0x3)

#define	QHA_VALID		(1<<0)
#define	QHA_ACTIVE		(1<<31)

#define	HC_MSOF_INT		(1<< 0)
#define	HC_MSEC_INT		(1 << 1)
#define	HC_EOT_INT		(1 << 3)
#define     HC_OPR_REG_INT	(1<<4)
#define     HC_CLK_RDY_INT	(1<<6)
#define	HC_INTL_INT		(1 << 7)
#define	HC_ATL_INT		(1 << 8)
#define	HC_ISO_INT		(1 << 9)
#define	HC_OTG_INT		(1 << 10)

#define	PTD_STATUS_HALTED	(1 << 30)
#define	PTD_XACT_ERROR		(1 << 28)
#define	PTD_BABBLE		(1 << 29)
#define PTD_ERROR		(PTD_STATUS_HALTED | PTD_XACT_ERROR | PTD_BABBLE)
#define	EPTYPE_BULK		(2 << 12)
#define	EPTYPE_CONTROL		(0 << 12)
#define	EPTYPE_INT		(3 << 12)
#define	EPTYPE_ISO		(1 << 12)

#define	PHCI_QHA_LENGTH		32

#define usb_inc_dev_use		usb_get_dev
#define usb_dec_dev_use		usb_put_dev
#define usb_free_dev		usb_put_dev
#define PTD_PERIODIC_SIZE	16
#define MAX_PERIODIC_SIZE	16
#define PTD_FRAME_MASK		0x1f
struct _periodic_list {
	int framenumber;
	struct list_head sitd_itd_head;
	char high_speed;	
	u16 ptdlocation;
};
typedef	struct _periodic_list periodic_list;


struct _isp1763_isoptd {
	u32 td_info1;
	u32 td_info2;
	u32 td_info3;
	u32 td_info4;
	u32 td_info5;
	u32 td_info6;
	u32 td_info7;
	u32 td_info8;
} __attribute__	((aligned(32)));

typedef	struct _isp1763_isoptd isp1763_isoptd;

struct _isp1763_qhint {
	u32 td_info1;
	u32 td_info2;
	u32 td_info3;
	u32 td_info4;
	u32 td_info5;
#define	INT_UNDERRUN (1	<< 2)
#define	INT_BABBLE    (1 << 1)
#define	INT_EXACT     (1 << 0)
	u32 td_info6;
	u32 td_info7;
	u32 td_info8;
} __attribute__	((aligned(32)));

typedef	struct _isp1763_qhint isp1763_qhint;


struct _isp1763_qha {
	u32 td_info1;		
	u32 td_info2;		
	u32 td_info3;		
	u32 td_info4;		
	u32 reserved[4];
};
typedef	struct _isp1763_qha isp1763_qha, *pisp1763_qha;




typedef	struct _ehci_regs {

	
	u32 command;
	u32 usbinterrupt;
	u32 usbstatus;
	u32 hcsparams;
	u32 frameindex;

	
	u16 hwmodecontrol;
	u16 interrupt;
	u16 interruptenable;
	u32 interruptthreshold;
	u16 iso_irq_mask_or;
	u16 int_irq_mask_or;
	u16 atl_irq_mask_or;
	u16 iso_irq_mask_and;
	u16 int_irq_mask_and;
	u16 atl_irq_mask_and;
	u16 buffer_status;

	
	u32 reset;
	u32 configflag;
	u32 ports[4];
	u32 pwrdwn_ctrl;

	
	u16 isotddonemap;
	u16 inttddonemap;
	u16 atltddonemap;
	u16 isotdskipmap;
	u16 inttdskipmap;
	u16 atltdskipmap;
	u16 isotdlastmap;
	u16 inttdlastmap;
	u16 atltdlastmap;
	u16 scratch;

} ehci_regs, *pehci_regs;

#define MEM_KV
#ifdef MEM_KV
typedef struct isp1763_mem_addr {
	u32 phy_addr;		
	u32 virt_addr;		
	u8 num_alloc;		
	u32 blk_size;		
	u8 blk_num;		
	u8 used;		
} isp1763_mem_addr_t;
#else
typedef struct isp1763_mem_addr {
	void *phy_addr;		
	void *virt_addr;	
	u8 usage;
	u32 blk_size;		
} isp1763_mem_addr_t;

#endif
#define	Q_NEXT_TYPE(dma) ((dma)	& __constant_cpu_to_le32 (3 << 1))

#define	Q_TYPE_ITD	__constant_cpu_to_le32 (0 << 1)
#define	Q_TYPE_QH	__constant_cpu_to_le32 (1 << 1)
#define	Q_TYPE_SITD	__constant_cpu_to_le32 (2 << 1)
#define	Q_TYPE_FSTN	__constant_cpu_to_le32 (3 << 1)

#define	QH_NEXT(dma)	cpu_to_le32((u32)dma)

struct ehci_qh {
	
	u32 hw_next;		
	u32 hw_info1;		

	u32 hw_info2;		
	u32 hw_current;		

	
	u32 hw_qtd_next;
	u32 hw_alt_next;
	u32 hw_token;
	u32 hw_buf[5];
	u32 hw_buf_hi[5];
	
	
	dma_addr_t qh_dma;	
	struct list_head qtd_list;	
	struct ehci_qtd	*dummy;
	struct ehci_qh *reclaim;	

	atomic_t refcount;
	wait_queue_head_t waitforcomplete;
	unsigned stamp;

	u8 qh_state;

	
	u8 usecs;		
	u8 gap_uf;		
	u8 c_usecs;		
	unsigned short period;	
	unsigned short start;	
	u8 datatoggle;		

	
	u8 ping;		

	

	u32 qtd_ptd_index;	
	u32 type;		

	
	struct usb_host_endpoint *ep;
	int next_uframe;	
	struct list_head itd_list;	
	isp1763_mem_addr_t memory_addr;
	struct _periodic_list periodic_list;
	
	u32 ssplit;
	u32 csplit;
	u8 totalptds;   
	u8 actualptds;	
};

typedef	struct {
	struct ehci_qh *qh;
	u16 length;		
	u16 td_cnt;		
	int state;		
	int timeout;		
	wait_queue_head_t wait;	
	
	struct timer_list urb_timer;
	struct list_head qtd_list;
	struct ehci_qtd	*qtd[0];	

} urb_priv_t;



#define	QH_HEAD			0x00008000
#define	QH_STATE_LINKED		1	
#define	QH_STATE_UNLINK		2	
#define	QH_STATE_IDLE		3	
#define	QH_STATE_UNLINK_WAIT	4	
#define	QH_STATE_COMPLETING	5	
#define	QH_STATE_TAKE_NEXT	8	
#define	NO_FRAME ((unsigned short)~0)	


#define EHCI_ITD_TRANLENGTH	0x0fff0000	
#define EHCI_ITD_PG		0x00007000	
#define EHCI_ITD_TRANOFFSET	0x00000fff	
#define EHCI_ITD_BUFFPTR	0xfffff000	

struct ehci_sitd {
	
	u32 hw_next;		
	u32 hw_transaction[8];	
#define EHCI_ISOC_ACTIVE	(1<<31)	
#define EHCI_ISOC_BUF_ERR	(1<<30)	
#define EHCI_ISOC_BABBLE	(1<<29)	
#define EHCI_ISOC_XACTERR	(1<<28)	

#define EHCI_ITD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define EHCI_ITD_IOC		(1 << 15)	

	u32 hw_bufp[7];		
	u32 hw_bufp_hi[7];	

	
	dma_addr_t sitd_dma;	
	struct urb *urb;
	struct list_head sitd_list;	
	dma_addr_t buf_dma;	

	
	u32 transaction;
	u16 index;		
	u16 uframe;		
	u16 usecs;
	
	struct isp1763_mem_addr mem_addr;
	int length;
	u32 framenumber;
	u32 ptdframe;
	int sitd_index;
	
	u32 ssplit;
	u32 csplit;
	u32 start_frame;
};

struct ehci_itd	{
	
	u32 hw_next;		
	u32 hw_transaction[8];	
#define	EHCI_ISOC_ACTIVE	(1<<31)	
#define	EHCI_ISOC_BUF_ERR	(1<<30)	
#define	EHCI_ISOC_BABBLE	(1<<29)	
#define	EHCI_ISOC_XACTERR	(1<<28)	

#define	EHCI_ITD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	EHCI_ITD_IOC		(1 << 15)	

	u32 hw_bufp[7];		
	u32 hw_bufp_hi[7];	

	
	dma_addr_t itd_dma;	
	struct urb *urb;
	struct list_head itd_list;	
	dma_addr_t buf_dma;	
	u8 num_of_pkts;		
	
	u32 transaction;
	u16 index;		
	u16 uframe;		
	u16 usecs;
	
	struct isp1763_mem_addr	mem_addr;
	int length;
	u32 multi;
	u32 framenumber;
	u32 ptdframe;
	int itd_index;
	
	u32 ssplit;
	u32 csplit;
};

struct ehci_qtd	{
	
	u32 hw_next;		
	u32 hw_alt_next;	
	u32 hw_token;		

	u32 hw_buf[5];		
	u32 hw_buf_hi[5];	

	
	dma_addr_t qtd_dma;	
	struct list_head qtd_list;	
	struct urb *urb;	
	size_t length;		
	u32 state;		
#define	QTD_STATE_NEW			0x100
#define	QTD_STATE_DONE			0x200
#define	QTD_STATE_SCHEDULED		0x400
#define	QTD_STATE_LAST			0x800
	struct isp1763_mem_addr	mem_addr;
};

#define	QTD_TOGGLE			(1 << 31)	
#define	QTD_LENGTH(tok)			(((tok)>>16) & 0x7fff)
#define	QTD_IOC				(1 << 15)	
#define	QTD_CERR(tok)			(((tok)>>10) & 0x3)
#define	QTD_PID(tok)			(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE			(1 << 7)	
#define	QTD_STS_HALT			(1 << 6)	
#define	QTD_STS_DBE			(1 << 5)	
#define	QTD_STS_BABBLE			(1 << 4)	
#define	QTD_STS_XACT			(1 << 3)	
#define	QTD_STS_MMF			(1 << 2)	
#define	QTD_STS_STS			(1 << 1)	
#define	QTD_STS_PING			(1 << 0)	

#define	EHCI_LIST_END	__constant_cpu_to_le32(1)	
#define	QTD_NEXT(dma)	cpu_to_le32((u32)dma)

struct _phci_driver;
struct _isp1763_hcd;
#define	EHCI_MAX_ROOT_PORTS 1

#include <linux/usb/hcd.h>

#define USBNET
#ifdef USBNET 
struct isp1763_async_cleanup_urb {
        struct list_head urb_list;
        struct urb *urb;
};
#endif


typedef	struct _phci_hcd {

	struct usb_hcd usb_hcd;
	spinlock_t lock;

	
	struct ehci_qh *async;
	struct ehci_qh *reclaim;
	
	unsigned periodic_size;
	int next_uframe;	
	int periodic_sched;	
	int periodic_more_urb;
	struct usb_device *otgdev;	
	struct timer_list rh_timer;	
	struct list_head dev_list;	
	struct list_head urb_list;	

	
	atomic_t nuofsofs;
	atomic_t missedsofs;

	struct isp1763_dev *dev;
	
	u8 *iobase;
	u32 iolength;
	u8 *plxiobase;
	u32 plxiolength;

	int irq;		
	int state;		
	unsigned long reset_done[EHCI_MAX_ROOT_PORTS];
	ehci_regs regs;

	struct _isp1763_qha qha;
	struct _isp1763_qhint qhint;
	struct _isp1763_isoptd isotd;

	struct tasklet_struct tasklet;
	
	struct timer_list watchdog;
	void (*worker_function)	(struct	_phci_hcd * hcd);
	struct _periodic_list periodic_list[PTD_PERIODIC_SIZE];
#ifdef USBNET 
	struct isp1763_async_cleanup_urb cleanup_urb;
#endif
} phci_hcd, *pphci_hcd;

typedef	struct hcd_dev {
	struct list_head dev_list;
	struct list_head urb_list;
} hcd_dev;

#define	usb_hcd_to_pehci_hcd(hcd)   container_of(hcd, struct _phci_hcd,	usb_hcd)

#ifdef CONFIG_PHCI_MEM_SLAB

#define	qha_alloc(t,c) kmem_cache_alloc(c,ALLOC_FLAGS)
#define	qha_free(c,x) kmem_cache_free(c,x)
static kmem_cache_t *qha_cache,	*qh_cache, *qtd_cache;
static int
phci_hcd_mem_init(void)
{
	
	qha_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				      SLAB_HWCACHE_ALIGN, NULL,	NULL);
	if (!qha_cache)	{
		printk("no TD cache?");
		return -ENOMEM;
	}

	
	qh_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				     SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!qh_cache) {
		printk("no TD cache?");
		return -ENOMEM;
	}

	
	qtd_cache = kmem_cache_create("phci_ptd", sizeof(isp1763_qha), 0,
				      SLAB_HWCACHE_ALIGN, NULL,	NULL);
	if (!qtd_cache)	{
		printk("no TD cache?");
		return -ENOMEM;
	}
	return 0;
}
static void
phci_mem_cleanup(void)
{
	if (qha_cache && kmem_cache_destroy(qha_cache))
		err("td_cache remained");
	qha_cache = 0;
}
#else

#define	qha_alloc(t,c)			kmalloc(t,ALLOC_FLAGS)
#define	qha_free(c,x)			kfree(x)
#define	qha_cache			0


#ifdef CONFIG_ISO_SUPPORT
#define BLK_128_	2
#define BLK_256_	3
#define BLK_1024_	1
#define BLK_2048_	3
#define BLK_4096_	3 
#define BLK_8196_	0 
#define BLK_TOTAL	(BLK_128_+BLK_256_ + BLK_1024_ +BLK_2048_+ BLK_4096_+BLK_8196_)

#define BLK_SIZE_128	128
#define BLK_SIZE_256	256
#define BLK_SIZE_1024	1024
#define BLK_SIZE_2048	2048
#define BLK_SIZE_4096	4096
#define BLK_SIZE_8192	8192

#define  COMMON_MEMORY	1

#else
#define BLK_256_	8
#define BLK_1024_	6
#define BLK_4096_	3
#define BLK_TOTAL	(BLK_256_ + BLK_1024_ + BLK_4096_)
#define BLK_SIZE_256	256
#define BLK_SIZE_1024	1024
#define BLK_SIZE_4096	4096
#endif
static void phci_hcd_mem_init(void);
static inline void
phci_mem_cleanup(void)
{
	return;
}

#endif

#define	PORT_WKOC_E			(1<<22)	
#define	PORT_WKDISC_E			(1<<21)	
#define	PORT_WKCONN_E			(1<<20)	
#define	PORT_OWNER			(1<<13)	
#define	PORT_POWER			(1<<12)	
#define	PORT_USB11(x)			(((x)&(3<<10))==(1<<10))	
#define	PORT_RESET			(1<<8)	
#define	PORT_SUSPEND			(1<<7)	
#define	PORT_RESUME			(1<<6)	
#define	PORT_OCC			(1<<5)	

#define	PORT_OC				(1<<4)	
#define	PORT_PEC			(1<<3)	
#define	PORT_PE				(1<<2)	
#define	PORT_CSC			(1<<1)	
#define	PORT_CONNECT			(1<<0)	
#define PORT_RWC_BITS	(PORT_CSC | PORT_PEC | PORT_OCC)	

#define	ATL_BUFFER			0x1
#define	INT_BUFFER			0x2
#define	ISO_BUFFER			0x4
#define	BUFFER_MAP			0x7

#define	TD_PTD_BUFF_TYPE_ATL		0	
#define	TD_PTD_BUFF_TYPE_INTL		1	
#define	TD_PTD_BUFF_TYPE_ISTL		2	
#define	TD_PTD_TOTAL_BUFF_TYPES		(TD_PTD_BUFF_TYPE_ISTL +1)
#define	TD_PTD_MAX_BUFF_TDS		16

#define	TD_PTD_INV_PTD_INDEX		0xFFFF
#define	INVALID_FRAME_NUMBER		0xFFFFFFFF
#define	HC_ATL_PL_SIZE			4096
#define	HC_ISTL_PL_SIZE			1024
#define	HC_INTL_PL_SIZE			1024

#define	TD_PTD_NEW			0x0000
#define	TD_PTD_ACTIVE			0x0001
#define	TD_PTD_IDLE			0x0002
#define	TD_PTD_REMOVE			0x0004
#define	TD_PTD_RELOAD			0x0008
#define	TD_PTD_IN_SCHEDULE		0x0010
#define	TD_PTD_DONE			0x0020

#define	PTD_RETRY(x)			(((x) >> 23) & 0x3)
#define	PTD_PID(x)			(((x) >> 10) & (0x3))
#define	PTD_NEXTTOGGLE(x)		(((x) >> 25) & (0x1))
#define	PTD_XFERRED_LENGTH(x)		((x) & 0x7fff)
#define	PTD_XFERRED_NONHSLENGTH(x)	((x) & 0x7ff)
#define	PTD_PING_STATE(x)		(((x) >> 26) & (0x1))

#define	DELETE_URB			0x0008
#define	NO_TRANSFER_ACTIVE		0xFFFF
#define	NO_TRANSFER_DONE		0x0000
#define	MAX_PTD_BUFFER_SIZE		4096	

typedef	struct td_ptd_map {
	u32 state;		
	u8 datatoggle;		
	u32 ptd_bitmap;		
	u32 ptd_header_addr;	
	u32 ptd_data_addr;	
	u32 ptd_ram_data_addr;
	u8 lasttd;		
	struct ehci_qh *qh;	
	struct ehci_qtd	*qtd;	
	struct ehci_itd	*itd;	
	struct ehci_sitd *sitd;	
	
	u32 grouptdmap;		
} td_ptd_map_t;

typedef	struct td_ptd_map_buff {
	u8 buffer_type;		
	u8 active_ptds;		
	u8 total_ptds;		
	u8 max_ptds;		
	u16 active_ptd_bitmap;	
	u16 pending_ptd_bitmap;	
	td_ptd_map_t map_list[TD_PTD_MAX_BUFF_TDS];	
} td_ptd_map_buff_t;


#define     USB_HCD_MAJOR           0
#define     USB_HCD_MODULE_NAME     "isp1763hcd"

#define HCD_IOC_MAGIC	'h'

#define     HCD_IOC_POWERDOWN							_IO(HCD_IOC_MAGIC, 1)
#define     HCD_IOC_POWERUP								_IO(HCD_IOC_MAGIC, 2)
#define     HCD_IOC_TESTSE0_NACK						_IO(HCD_IOC_MAGIC, 3)
#define     HCD_IOC_TEST_J								_IO(HCD_IOC_MAGIC,4)
#define     HCD_IOC_TEST_K								_IO(HCD_IOC_MAGIC,5)
#define     HCD_IOC_TEST_TESTPACKET						_IO(HCD_IOC_MAGIC,6)
#define     HCD_IOC_TEST_FORCE_ENABLE					_IO(HCD_IOC_MAGIC,7)
#define	  HCD_IOC_TEST_SUSPEND_RESUME				_IO(HCD_IOC_MAGIC,8)
#define     HCD_IOC_TEST_SINGLE_STEP_GET_DEV_DESC		_IO(HCD_IOC_MAGIC,9)
#define     HCD_IOC_TEST_SINGLE_STEP_SET_FEATURE		_IO(HCD_IOC_MAGIC,10)
#define     HCD_IOC_TEST_STOP							_IO(HCD_IOC_MAGIC,11)
#define     HCD_IOC_SUSPEND_BUS							_IO(HCD_IOC_MAGIC,12)
#define     HCD_IOC_RESUME_BUS							_IO(HCD_IOC_MAGIC,13)
#define     HCD_IOC_REMOTEWAKEUP_BUS					_IO(HCD_IOC_MAGIC,14)

#define HOST_COMPILANCE_TEST_ENABLE	1
#define HOST_COMP_TEST_SE0_NAK	1
#define HOST_COMP_TEST_J	2
#define HOST_COMP_TEST_K	3
#define HOST_COMP_TEST_PACKET		4
#define HOST_COMP_TEST_FORCE_ENABLE	5
#define HOST_COMP_HS_HOST_PORT_SUSPEND_RESUME	6
#define HOST_COMP_SINGLE_STEP_GET_DEV_DESC	7
#define HOST_COMP_SINGLE_STEP_SET_FEATURE	8

#endif
