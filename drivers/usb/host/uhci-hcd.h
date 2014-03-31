#ifndef __LINUX_UHCI_HCD_H
#define __LINUX_UHCI_HCD_H

#include <linux/list.h>
#include <linux/usb.h>

#define usb_packetid(pipe)	(usb_pipein(pipe) ? USB_PID_IN : USB_PID_OUT)
#define PIPE_DEVEP_MASK		0x0007ff00



#define USBCMD		0
#define   USBCMD_RS		0x0001	
#define   USBCMD_HCRESET	0x0002	
#define   USBCMD_GRESET		0x0004	
#define   USBCMD_EGSM		0x0008	
#define   USBCMD_FGR		0x0010	
#define   USBCMD_SWDBG		0x0020	
#define   USBCMD_CF		0x0040	
#define   USBCMD_MAXP		0x0080	

#define USBSTS		2
#define   USBSTS_USBINT		0x0001	
#define   USBSTS_ERROR		0x0002	
#define   USBSTS_RD		0x0004	
#define   USBSTS_HSE		0x0008	
#define   USBSTS_HCPE		0x0010	
#define   USBSTS_HCH		0x0020	

#define USBINTR		4
#define   USBINTR_TIMEOUT	0x0001	
#define   USBINTR_RESUME	0x0002	
#define   USBINTR_IOC		0x0004	
#define   USBINTR_SP		0x0008	

#define USBFRNUM	6
#define USBFLBASEADD	8
#define USBSOF		12
#define   USBSOF_DEFAULT	64	

#define USBPORTSC1	16
#define USBPORTSC2	18
#define   USBPORTSC_CCS		0x0001	
#define   USBPORTSC_CSC		0x0002	
#define   USBPORTSC_PE		0x0004	
#define   USBPORTSC_PEC		0x0008	
#define   USBPORTSC_DPLUS	0x0010	
#define   USBPORTSC_DMINUS	0x0020	
#define   USBPORTSC_RD		0x0040	
#define   USBPORTSC_RES1	0x0080	
#define   USBPORTSC_LSDA	0x0100	
#define   USBPORTSC_PR		0x0200	
#define   USBPORTSC_OC		0x0400	
#define   USBPORTSC_OCC		0x0800	
#define   USBPORTSC_SUSP	0x1000	
#define   USBPORTSC_RES2	0x2000	
#define   USBPORTSC_RES3	0x4000	
#define   USBPORTSC_RES4	0x8000	

#define USBLEGSUP		0xc0
#define   USBLEGSUP_DEFAULT	0x2000	
#define   USBLEGSUP_RWC		0x8f00	
#define   USBLEGSUP_RO		0x5040	

#define USBRES_INTEL		0xc4
#define   USBPORT1EN		0x01
#define   USBPORT2EN		0x02

#define UHCI_PTR_BITS(uhci)	cpu_to_hc32((uhci), 0x000F)
#define UHCI_PTR_TERM(uhci)	cpu_to_hc32((uhci), 0x0001)
#define UHCI_PTR_QH(uhci)	cpu_to_hc32((uhci), 0x0002)
#define UHCI_PTR_DEPTH(uhci)	cpu_to_hc32((uhci), 0x0004)
#define UHCI_PTR_BREADTH(uhci)	cpu_to_hc32((uhci), 0x0000)

#define UHCI_NUMFRAMES		1024	
#define UHCI_MAX_SOF_NUMBER	2047	
#define CAN_SCHEDULE_FRAMES	1000	
#define MAX_PHASE		32	

#define FSBR_OFF_DELAY		msecs_to_jiffies(10)

#define QH_WAIT_TIMEOUT		msecs_to_jiffies(200)


#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_DESC
typedef __u32 __bitwise __hc32;
typedef __u16 __bitwise __hc16;
#else
#define __hc32	__le32
#define __hc16	__le16
#endif


#define QH_STATE_IDLE		1	
#define QH_STATE_UNLINKING	2	
#define QH_STATE_ACTIVE		3	

struct uhci_qh {
	
	__hc32 link;			
	__hc32 element;			

	
	dma_addr_t dma_handle;

	struct list_head node;		
	struct usb_host_endpoint *hep;	
	struct usb_device *udev;
	struct list_head queue;		
	struct uhci_td *dummy_td;	
	struct uhci_td *post_td;	

	struct usb_iso_packet_descriptor *iso_packet_desc;
					
	unsigned long advance_jiffies;	
	unsigned int unlink_frame;	
	unsigned int period;		
	short phase;			
	short load;			
	unsigned int iso_frame;		

	int state;			
	int type;			
	int skel;			

	unsigned int initial_toggle:1;	
	unsigned int needs_fixup:1;	
	unsigned int is_stopped:1;	
	unsigned int wait_expired:1;	
	unsigned int bandwidth_reserved:1;	
} __attribute__((aligned(16)));

#define qh_element(qh)		ACCESS_ONCE((qh)->element)

#define LINK_TO_QH(uhci, qh)	(UHCI_PTR_QH((uhci)) | \
				cpu_to_hc32((uhci), (qh)->dma_handle))



#define TD_CTRL_SPD		(1 << 29)	
#define TD_CTRL_C_ERR_MASK	(3 << 27)	
#define TD_CTRL_C_ERR_SHIFT	27
#define TD_CTRL_LS		(1 << 26)	
#define TD_CTRL_IOS		(1 << 25)	
#define TD_CTRL_IOC		(1 << 24)	
#define TD_CTRL_ACTIVE		(1 << 23)	
#define TD_CTRL_STALLED		(1 << 22)	
#define TD_CTRL_DBUFERR		(1 << 21)	
#define TD_CTRL_BABBLE		(1 << 20)	
#define TD_CTRL_NAK		(1 << 19)	
#define TD_CTRL_CRCTIMEO	(1 << 18)	
#define TD_CTRL_BITSTUFF	(1 << 17)	
#define TD_CTRL_ACTLEN_MASK	0x7FF	

#define TD_CTRL_ANY_ERROR	(TD_CTRL_STALLED | TD_CTRL_DBUFERR | \
				 TD_CTRL_BABBLE | TD_CTRL_CRCTIME | \
				 TD_CTRL_BITSTUFF)

#define uhci_maxerr(err)		((err) << TD_CTRL_C_ERR_SHIFT)
#define uhci_status_bits(ctrl_sts)	((ctrl_sts) & 0xF60000)
#define uhci_actual_length(ctrl_sts)	(((ctrl_sts) + 1) & \
			TD_CTRL_ACTLEN_MASK)	

#define td_token(uhci, td)	hc32_to_cpu((uhci), (td)->token)
#define TD_TOKEN_DEVADDR_SHIFT	8
#define TD_TOKEN_TOGGLE_SHIFT	19
#define TD_TOKEN_TOGGLE		(1 << 19)
#define TD_TOKEN_EXPLEN_SHIFT	21
#define TD_TOKEN_EXPLEN_MASK	0x7FF	
#define TD_TOKEN_PID_MASK	0xFF

#define uhci_explen(len)	((((len) - 1) & TD_TOKEN_EXPLEN_MASK) << \
					TD_TOKEN_EXPLEN_SHIFT)

#define uhci_expected_length(token) ((((token) >> TD_TOKEN_EXPLEN_SHIFT) + \
					1) & TD_TOKEN_EXPLEN_MASK)
#define uhci_toggle(token)	(((token) >> TD_TOKEN_TOGGLE_SHIFT) & 1)
#define uhci_endpoint(token)	(((token) >> 15) & 0xf)
#define uhci_devaddr(token)	(((token) >> TD_TOKEN_DEVADDR_SHIFT) & 0x7f)
#define uhci_devep(token)	(((token) >> TD_TOKEN_DEVADDR_SHIFT) & 0x7ff)
#define uhci_packetid(token)	((token) & TD_TOKEN_PID_MASK)
#define uhci_packetout(token)	(uhci_packetid(token) != USB_PID_IN)
#define uhci_packetin(token)	(uhci_packetid(token) == USB_PID_IN)

struct uhci_td {
	
	__hc32 link;
	__hc32 status;
	__hc32 token;
	__hc32 buffer;

	
	dma_addr_t dma_handle;

	struct list_head list;

	int frame;			
	struct list_head fl_list;
} __attribute__((aligned(16)));

#define td_status(uhci, td)		hc32_to_cpu((uhci), \
						ACCESS_ONCE((td)->status))

#define LINK_TO_TD(uhci, td)		(cpu_to_hc32((uhci), (td)->dma_handle))




#define UHCI_NUM_SKELQH		11
#define SKEL_UNLINK		0
#define skel_unlink_qh		skelqh[SKEL_UNLINK]
#define SKEL_ISO		1
#define skel_iso_qh		skelqh[SKEL_ISO]
	
#define SKEL_INDEX(exponent)	(9 - exponent)
#define SKEL_ASYNC		9
#define skel_async_qh		skelqh[SKEL_ASYNC]
#define SKEL_TERM		10
#define skel_term_qh		skelqh[SKEL_TERM]

#define SKEL_LS_CONTROL		20
#define SKEL_FS_CONTROL		21
#define SKEL_FSBR		SKEL_FS_CONTROL
#define SKEL_BULK		22


enum uhci_rh_state {
	UHCI_RH_RESET,
	UHCI_RH_SUSPENDED,

	UHCI_RH_AUTO_STOPPED,
	UHCI_RH_RESUMING,

	UHCI_RH_SUSPENDING,

	UHCI_RH_RUNNING,		
	UHCI_RH_RUNNING_NODEVS,		
};

struct uhci_hcd {

	
	struct dentry *dentry;

	
	unsigned long io_addr;

	
	void __iomem *regs;

	struct dma_pool *qh_pool;
	struct dma_pool *td_pool;

	struct uhci_td *term_td;	
	struct uhci_qh *skelqh[UHCI_NUM_SKELQH];	
	struct uhci_qh *next_qh;	

	spinlock_t lock;

	dma_addr_t frame_dma_handle;	
	__hc32 *frame;
	void **frame_cpu;		

	enum uhci_rh_state rh_state;
	unsigned long auto_stop_time;		

	unsigned int frame_number;		
	unsigned int is_stopped;
#define UHCI_IS_STOPPED		9999		
	unsigned int last_iso_frame;		
	unsigned int cur_iso_frame;		

	unsigned int scan_in_progress:1;	
	unsigned int need_rescan:1;		
	unsigned int dead:1;			
	unsigned int RD_enable:1;		
	unsigned int is_initialized:1;		
	unsigned int fsbr_is_on:1;		
	unsigned int fsbr_is_wanted:1;		
	unsigned int fsbr_expiring:1;		

	struct timer_list fsbr_timer;		

	
	unsigned int oc_low:1;			
	unsigned int wait_for_hp:1;		
	unsigned int big_endian_mmio:1;		
	unsigned int big_endian_desc:1;		

	
	unsigned long port_c_suspend;		
	unsigned long resuming_ports;
	unsigned long ports_timeout;		

	struct list_head idle_qh_list;		

	int rh_numports;			

	wait_queue_head_t waitqh;		
	int num_waiting;			

	int total_load;				
	short load[MAX_PHASE];			

	
	void	(*reset_hc) (struct uhci_hcd *uhci);
	int	(*check_and_reset_hc) (struct uhci_hcd *uhci);
	
	void	(*configure_hc) (struct uhci_hcd *uhci);
	
	int	(*resume_detect_interrupts_are_broken) (struct uhci_hcd *uhci);
	
	int	(*global_suspend_mode_is_broken) (struct uhci_hcd *uhci);
};

static inline struct uhci_hcd *hcd_to_uhci(struct usb_hcd *hcd)
{
	return (struct uhci_hcd *) (hcd->hcd_priv);
}
static inline struct usb_hcd *uhci_to_hcd(struct uhci_hcd *uhci)
{
	return container_of((void *) uhci, struct usb_hcd, hcd_priv);
}

#define uhci_dev(u)	(uhci_to_hcd(u)->self.controller)

#define uhci_frame_before_eq(f1, f2)	(0 <= (int) ((f2) - (f1)))


struct urb_priv {
	struct list_head node;		

	struct urb *urb;

	struct uhci_qh *qh;		
	struct list_head td_list;

	unsigned fsbr:1;		
};



#define PCI_VENDOR_ID_GENESYS		0x17a0
#define PCI_DEVICE_ID_GL880S_UHCI	0x8083


#ifndef CONFIG_USB_UHCI_SUPPORT_NON_PCI_HC
static inline u32 uhci_readl(const struct uhci_hcd *uhci, int reg)
{
	return inl(uhci->io_addr + reg);
}

static inline void uhci_writel(const struct uhci_hcd *uhci, u32 val, int reg)
{
	outl(val, uhci->io_addr + reg);
}

static inline u16 uhci_readw(const struct uhci_hcd *uhci, int reg)
{
	return inw(uhci->io_addr + reg);
}

static inline void uhci_writew(const struct uhci_hcd *uhci, u16 val, int reg)
{
	outw(val, uhci->io_addr + reg);
}

static inline u8 uhci_readb(const struct uhci_hcd *uhci, int reg)
{
	return inb(uhci->io_addr + reg);
}

static inline void uhci_writeb(const struct uhci_hcd *uhci, u8 val, int reg)
{
	outb(val, uhci->io_addr + reg);
}

#else
#ifdef CONFIG_PCI
#define uhci_has_pci_registers(u)	((u)->io_addr != 0)
#else
#define uhci_has_pci_registers(u)	0
#endif

#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
#define uhci_big_endian_mmio(u)		((u)->big_endian_mmio)
#else
#define uhci_big_endian_mmio(u)		0
#endif

static inline u32 uhci_readl(const struct uhci_hcd *uhci, int reg)
{
	if (uhci_has_pci_registers(uhci))
		return inl(uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		return readl_be(uhci->regs + reg);
#endif
	else
		return readl(uhci->regs + reg);
}

static inline void uhci_writel(const struct uhci_hcd *uhci, u32 val, int reg)
{
	if (uhci_has_pci_registers(uhci))
		outl(val, uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		writel_be(val, uhci->regs + reg);
#endif
	else
		writel(val, uhci->regs + reg);
}

static inline u16 uhci_readw(const struct uhci_hcd *uhci, int reg)
{
	if (uhci_has_pci_registers(uhci))
		return inw(uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		return readw_be(uhci->regs + reg);
#endif
	else
		return readw(uhci->regs + reg);
}

static inline void uhci_writew(const struct uhci_hcd *uhci, u16 val, int reg)
{
	if (uhci_has_pci_registers(uhci))
		outw(val, uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		writew_be(val, uhci->regs + reg);
#endif
	else
		writew(val, uhci->regs + reg);
}

static inline u8 uhci_readb(const struct uhci_hcd *uhci, int reg)
{
	if (uhci_has_pci_registers(uhci))
		return inb(uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		return readb_be(uhci->regs + reg);
#endif
	else
		return readb(uhci->regs + reg);
}

static inline void uhci_writeb(const struct uhci_hcd *uhci, u8 val, int reg)
{
	if (uhci_has_pci_registers(uhci))
		outb(val, uhci->io_addr + reg);
#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_MMIO
	else if (uhci_big_endian_mmio(uhci))
		writeb_be(val, uhci->regs + reg);
#endif
	else
		writeb(val, uhci->regs + reg);
}
#endif 

#ifdef CONFIG_USB_UHCI_BIG_ENDIAN_DESC
#define uhci_big_endian_desc(u)		((u)->big_endian_desc)

static inline __hc32 cpu_to_hc32(const struct uhci_hcd *uhci, const u32 x)
{
	return uhci_big_endian_desc(uhci)
		? (__force __hc32)cpu_to_be32(x)
		: (__force __hc32)cpu_to_le32(x);
}

static inline u32 hc32_to_cpu(const struct uhci_hcd *uhci, const __hc32 x)
{
	return uhci_big_endian_desc(uhci)
		? be32_to_cpu((__force __be32)x)
		: le32_to_cpu((__force __le32)x);
}

#else
static inline __hc32 cpu_to_hc32(const struct uhci_hcd *uhci, const u32 x)
{
	return cpu_to_le32(x);
}

static inline u32 hc32_to_cpu(const struct uhci_hcd *uhci, const __hc32 x)
{
	return le32_to_cpu(x);
}
#endif

#endif
