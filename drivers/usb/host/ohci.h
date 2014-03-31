/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 *
 * This file is licenced under the GPL.
 */

typedef __u32 __bitwise __hc32;
typedef __u16 __bitwise __hc16;

struct ed {
	
	__hc32			hwINFO;      
	
#define ED_DEQUEUE	(1 << 27)
	
#define ED_ISO		(1 << 15)
#define ED_SKIP		(1 << 14)
#define ED_LOWSPEED	(1 << 13)
#define ED_OUT		(0x01 << 11)
#define ED_IN		(0x02 << 11)
	__hc32			hwTailP;	
	__hc32			hwHeadP;	
#define ED_C		(0x02)			
#define ED_H		(0x01)			
	__hc32			hwNextED;	

	
	dma_addr_t		dma;		
	struct td		*dummy;		

	
	struct ed		*ed_next;	
	struct ed		*ed_prev;	
	struct list_head	td_list;	

	u8			state;		
#define ED_IDLE		0x00		
#define ED_UNLINK	0x01		
#define ED_OPER		0x02		

	u8			type;		

	
	u8			branch;
	u16			interval;
	u16			load;
	u16			last_iso;	

	
	u16			tick;
} __attribute__ ((aligned(16)));

#define ED_MASK	((u32)~0x0f)		


struct td {
	
	__hc32		hwINFO;		

	
#define TD_CC       0xf0000000			
#define TD_CC_GET(td_p) ((td_p >>28) & 0x0f)
#define TD_DI       0x00E00000			
#define TD_DI_SET(X) (((X) & 0x07)<< 21)
#define TD_DONE     0x00020000			
#define TD_ISO      0x00010000			

	
#define TD_EC       0x0C000000			
#define TD_T        0x03000000			
#define TD_T_DATA0  0x02000000				
#define TD_T_DATA1  0x03000000				
#define TD_T_TOGGLE 0x00000000				
#define TD_DP       0x00180000			
#define TD_DP_SETUP 0x00000000			
#define TD_DP_IN    0x00100000				
#define TD_DP_OUT   0x00080000				
							
#define TD_R        0x00040000			

	

	__hc32		hwCBP;		
	__hc32		hwNextTD;	
	__hc32		hwBE;		

#define MAXPSW	2
	__hc16		hwPSW [MAXPSW];

	
	__u8		index;
	struct ed	*ed;
	struct td	*td_hash;	
	struct td	*next_dl_td;
	struct urb	*urb;

	dma_addr_t	td_dma;		
	dma_addr_t	data_dma;	

	struct list_head td_list;	
} __attribute__ ((aligned(32)));	

#define TD_MASK	((u32)~0x1f)		

#define TD_CC_NOERROR      0x00
#define TD_CC_CRC          0x01
#define TD_CC_BITSTUFFING  0x02
#define TD_CC_DATATOGGLEM  0x03
#define TD_CC_STALL        0x04
#define TD_DEVNOTRESP      0x05
#define TD_PIDCHECKFAIL    0x06
#define TD_UNEXPECTEDPID   0x07
#define TD_DATAOVERRUN     0x08
#define TD_DATAUNDERRUN    0x09
    
#define TD_BUFFEROVERRUN   0x0C
#define TD_BUFFERUNDERRUN  0x0D
    
#define TD_NOTACCESSED     0x0F


static const int cc_to_error [16] = {
	               0,
	               -EILSEQ,
	               -EPROTO,
	               -EILSEQ,
	               -EPIPE,
	               -ETIME,
	               -EPROTO,
	               -EPROTO,
	               -EOVERFLOW,
	               -EREMOTEIO,
	               -EIO,
	               -EIO,
	               -ECOMM,
	               -ENOSR,
	               -EALREADY,
	               -EALREADY
};


struct ohci_hcca {
#define NUM_INTS 32
	__hc32	int_table [NUM_INTS];	

	__hc32	frame_no;		
	__hc32	done_head;		
	u8	reserved_for_hc [116];
	u8	what [4];		
} __attribute__ ((aligned(256)));

struct ohci_regs {
	
	__hc32	revision;
	__hc32	control;
	__hc32	cmdstatus;
	__hc32	intrstatus;
	__hc32	intrenable;
	__hc32	intrdisable;

	
	__hc32	hcca;
	__hc32	ed_periodcurrent;
	__hc32	ed_controlhead;
	__hc32	ed_controlcurrent;
	__hc32	ed_bulkhead;
	__hc32	ed_bulkcurrent;
	__hc32	donehead;

	
	__hc32	fminterval;
	__hc32	fmremaining;
	__hc32	fmnumber;
	__hc32	periodicstart;
	__hc32	lsthresh;

	
	struct	ohci_roothub_regs {
		__hc32	a;
		__hc32	b;
		__hc32	status;
#define MAX_ROOT_PORTS	15	
		__hc32	portstatus [MAX_ROOT_PORTS];
	} roothub;

	

} __attribute__ ((aligned(32)));



#define OHCI_CTRL_CBSR	(3 << 0)	
#define OHCI_CTRL_PLE	(1 << 2)	
#define OHCI_CTRL_IE	(1 << 3)	
#define OHCI_CTRL_CLE	(1 << 4)	
#define OHCI_CTRL_BLE	(1 << 5)	
#define OHCI_CTRL_HCFS	(3 << 6)	
#define OHCI_CTRL_IR	(1 << 8)	
#define OHCI_CTRL_RWC	(1 << 9)	
#define OHCI_CTRL_RWE	(1 << 10)	

#	define OHCI_USB_RESET	(0 << 6)
#	define OHCI_USB_RESUME	(1 << 6)
#	define OHCI_USB_OPER	(2 << 6)
#	define OHCI_USB_SUSPEND	(3 << 6)

#define OHCI_HCR	(1 << 0)	
#define OHCI_CLF	(1 << 1)	
#define OHCI_BLF	(1 << 2)	
#define OHCI_OCR	(1 << 3)	
#define OHCI_SOC	(3 << 16)	

#define OHCI_INTR_SO	(1 << 0)	
#define OHCI_INTR_WDH	(1 << 1)	
#define OHCI_INTR_SF	(1 << 2)	
#define OHCI_INTR_RD	(1 << 3)	
#define OHCI_INTR_UE	(1 << 4)	
#define OHCI_INTR_FNO	(1 << 5)	
#define OHCI_INTR_RHSC	(1 << 6)	
#define OHCI_INTR_OC	(1 << 30)	
#define OHCI_INTR_MIE	(1 << 31)	



#define RH_PS_CCS            0x00000001		
#define RH_PS_PES            0x00000002		
#define RH_PS_PSS            0x00000004		
#define RH_PS_POCI           0x00000008		
#define RH_PS_PRS            0x00000010		
#define RH_PS_PPS            0x00000100		
#define RH_PS_LSDA           0x00000200		
#define RH_PS_CSC            0x00010000		
#define RH_PS_PESC           0x00020000		
#define RH_PS_PSSC           0x00040000		
#define RH_PS_OCIC           0x00080000		
#define RH_PS_PRSC           0x00100000		

#define RH_HS_LPS	     0x00000001		
#define RH_HS_OCI	     0x00000002		
#define RH_HS_DRWE	     0x00008000		
#define RH_HS_LPSC	     0x00010000		
#define RH_HS_OCIC	     0x00020000		
#define RH_HS_CRWE	     0x80000000		

#define RH_B_DR		0x0000ffff		
#define RH_B_PPCM	0xffff0000		

#define	RH_A_NDP	(0xff << 0)		
#define	RH_A_PSM	(1 << 8)		
#define	RH_A_NPS	(1 << 9)		
#define	RH_A_DT		(1 << 10)		
#define	RH_A_OCPM	(1 << 11)		
#define	RH_A_NOCP	(1 << 12)		
#define	RH_A_POTPGT	(0xff << 24)		


typedef struct urb_priv {
	struct ed		*ed;
	u16			length;		
	u16			td_cnt;		
	struct list_head	pending;
	struct td		*td [0];	

} urb_priv_t;

#define TD_HASH_SIZE    64    
#define TD_HASH_FUNC(td_dma) ((td_dma ^ (td_dma >> 6)) % TD_HASH_SIZE)



enum ohci_rh_state {
	OHCI_RH_HALTED,
	OHCI_RH_SUSPENDED,
	OHCI_RH_RUNNING
};

struct ohci_hcd {
	spinlock_t		lock;

	struct ohci_regs __iomem *regs;

	struct ohci_hcca	*hcca;
	dma_addr_t		hcca_dma;

	struct ed		*ed_rm_list;		

	struct ed		*ed_bulktail;		
	struct ed		*ed_controltail;	
	struct ed		*periodic [NUM_INTS];	

	struct usb_phy	*transceiver;
	void (*start_hnp)(struct ohci_hcd *ohci);

	struct dma_pool		*td_cache;
	struct dma_pool		*ed_cache;
	struct td		*td_hash [TD_HASH_SIZE];
	struct list_head	pending;

	enum ohci_rh_state	rh_state;
	int			num_ports;
	int			load [NUM_INTS];
	u32			hc_control;	
	unsigned long		next_statechange;	
	u32			fminterval;		
	unsigned		autostop:1;	

	unsigned long		flags;		
#define	OHCI_QUIRK_AMD756	0x01			
#define	OHCI_QUIRK_SUPERIO	0x02			
#define	OHCI_QUIRK_INITRESET	0x04			
#define	OHCI_QUIRK_BE_DESC	0x08			
#define	OHCI_QUIRK_BE_MMIO	0x10			
#define	OHCI_QUIRK_ZFMICRO	0x20			
#define	OHCI_QUIRK_NEC		0x40			
#define	OHCI_QUIRK_FRAME_NO	0x80			
#define	OHCI_QUIRK_HUB_POWER	0x100			
#define	OHCI_QUIRK_AMD_PLL	0x200			
#define	OHCI_QUIRK_AMD_PREFETCH	0x400			
	

	struct work_struct	nec_work;	

	
	struct timer_list	unlink_watchdog;
	unsigned		eds_scheduled;
	struct ed		*ed_to_check;
	unsigned		zf_delay;

#ifdef DEBUG
	struct dentry		*debug_dir;
	struct dentry		*debug_async;
	struct dentry		*debug_periodic;
	struct dentry		*debug_registers;
#endif
};

#ifdef CONFIG_PCI
static inline int quirk_nec(struct ohci_hcd *ohci)
{
	return ohci->flags & OHCI_QUIRK_NEC;
}
static inline int quirk_zfmicro(struct ohci_hcd *ohci)
{
	return ohci->flags & OHCI_QUIRK_ZFMICRO;
}
static inline int quirk_amdiso(struct ohci_hcd *ohci)
{
	return ohci->flags & OHCI_QUIRK_AMD_PLL;
}
static inline int quirk_amdprefetch(struct ohci_hcd *ohci)
{
	return ohci->flags & OHCI_QUIRK_AMD_PREFETCH;
}
#else
static inline int quirk_nec(struct ohci_hcd *ohci)
{
	return 0;
}
static inline int quirk_zfmicro(struct ohci_hcd *ohci)
{
	return 0;
}
static inline int quirk_amdiso(struct ohci_hcd *ohci)
{
	return 0;
}
static inline int quirk_amdprefetch(struct ohci_hcd *ohci)
{
	return 0;
}
#endif

static inline struct ohci_hcd *hcd_to_ohci (struct usb_hcd *hcd)
{
	return (struct ohci_hcd *) (hcd->hcd_priv);
}
static inline struct usb_hcd *ohci_to_hcd (const struct ohci_hcd *ohci)
{
	return container_of ((void *) ohci, struct usb_hcd, hcd_priv);
}


#ifndef DEBUG
#define STUB_DEBUG_FILES
#endif	

#define ohci_dbg(ohci, fmt, args...) \
	dev_dbg (ohci_to_hcd(ohci)->self.controller , fmt , ## args )
#define ohci_err(ohci, fmt, args...) \
	dev_err (ohci_to_hcd(ohci)->self.controller , fmt , ## args )
#define ohci_info(ohci, fmt, args...) \
	dev_info (ohci_to_hcd(ohci)->self.controller , fmt , ## args )
#define ohci_warn(ohci, fmt, args...) \
	dev_warn (ohci_to_hcd(ohci)->self.controller , fmt , ## args )

#ifdef OHCI_VERBOSE_DEBUG
#	define ohci_vdbg ohci_dbg
#else
#	define ohci_vdbg(ohci, fmt, args...) do { } while (0)
#endif



#ifdef CONFIG_USB_OHCI_BIG_ENDIAN_DESC
#ifdef CONFIG_USB_OHCI_LITTLE_ENDIAN
#define big_endian_desc(ohci)	(ohci->flags & OHCI_QUIRK_BE_DESC)
#else
#define big_endian_desc(ohci)	1		
#endif
#else
#define big_endian_desc(ohci)	0		
#endif

#ifdef CONFIG_USB_OHCI_BIG_ENDIAN_MMIO
#ifdef CONFIG_USB_OHCI_LITTLE_ENDIAN
#define big_endian_mmio(ohci)	(ohci->flags & OHCI_QUIRK_BE_MMIO)
#else
#define big_endian_mmio(ohci)	1		
#endif
#else
#define big_endian_mmio(ohci)	0		
#endif

static inline unsigned int _ohci_readl (const struct ohci_hcd *ohci,
					__hc32 __iomem * regs)
{
#ifdef CONFIG_USB_OHCI_BIG_ENDIAN_MMIO
	return big_endian_mmio(ohci) ?
		readl_be (regs) :
		readl (regs);
#else
	return readl (regs);
#endif
}

static inline void _ohci_writel (const struct ohci_hcd *ohci,
				 const unsigned int val, __hc32 __iomem *regs)
{
#ifdef CONFIG_USB_OHCI_BIG_ENDIAN_MMIO
	big_endian_mmio(ohci) ?
		writel_be (val, regs) :
		writel (val, regs);
#else
		writel (val, regs);
#endif
}

#define ohci_readl(o,r)		_ohci_readl(o,r)
#define ohci_writel(o,v,r)	_ohci_writel(o,v,r)



static inline __hc16 cpu_to_hc16 (const struct ohci_hcd *ohci, const u16 x)
{
	return big_endian_desc(ohci) ?
		(__force __hc16)cpu_to_be16(x) :
		(__force __hc16)cpu_to_le16(x);
}

static inline __hc16 cpu_to_hc16p (const struct ohci_hcd *ohci, const u16 *x)
{
	return big_endian_desc(ohci) ?
		cpu_to_be16p(x) :
		cpu_to_le16p(x);
}

static inline __hc32 cpu_to_hc32 (const struct ohci_hcd *ohci, const u32 x)
{
	return big_endian_desc(ohci) ?
		(__force __hc32)cpu_to_be32(x) :
		(__force __hc32)cpu_to_le32(x);
}

static inline __hc32 cpu_to_hc32p (const struct ohci_hcd *ohci, const u32 *x)
{
	return big_endian_desc(ohci) ?
		cpu_to_be32p(x) :
		cpu_to_le32p(x);
}

static inline u16 hc16_to_cpu (const struct ohci_hcd *ohci, const __hc16 x)
{
	return big_endian_desc(ohci) ?
		be16_to_cpu((__force __be16)x) :
		le16_to_cpu((__force __le16)x);
}

static inline u16 hc16_to_cpup (const struct ohci_hcd *ohci, const __hc16 *x)
{
	return big_endian_desc(ohci) ?
		be16_to_cpup((__force __be16 *)x) :
		le16_to_cpup((__force __le16 *)x);
}

static inline u32 hc32_to_cpu (const struct ohci_hcd *ohci, const __hc32 x)
{
	return big_endian_desc(ohci) ?
		be32_to_cpu((__force __be32)x) :
		le32_to_cpu((__force __le32)x);
}

static inline u32 hc32_to_cpup (const struct ohci_hcd *ohci, const __hc32 *x)
{
	return big_endian_desc(ohci) ?
		be32_to_cpup((__force __be32 *)x) :
		le32_to_cpup((__force __le32 *)x);
}



#ifdef CONFIG_PPC_MPC52xx
#define big_endian_frame_no_quirk(ohci)	(ohci->flags & OHCI_QUIRK_FRAME_NO)
#else
#define big_endian_frame_no_quirk(ohci)	0
#endif

static inline u16 ohci_frame_no(const struct ohci_hcd *ohci)
{
	u32 tmp;
	if (big_endian_desc(ohci)) {
		tmp = be32_to_cpup((__force __be32 *)&ohci->hcca->frame_no);
		if (!big_endian_frame_no_quirk(ohci))
			tmp >>= 16;
	} else
		tmp = le32_to_cpup((__force __le32 *)&ohci->hcca->frame_no);

	return (u16)tmp;
}

static inline __hc16 *ohci_hwPSWp(const struct ohci_hcd *ohci,
                                 const struct td *td, int index)
{
	return (__hc16 *)(big_endian_desc(ohci) ?
			&td->hwPSW[index ^ 1] : &td->hwPSW[index]);
}

static inline u16 ohci_hwPSW(const struct ohci_hcd *ohci,
                               const struct td *td, int index)
{
	return hc16_to_cpup(ohci, ohci_hwPSWp(ohci, td, index));
}


#define	FI			0x2edf		
#define	FSMP(fi)		(0x7fff & ((6 * ((fi) - 210)) / 7))
#define	FIT			(1 << 31)
#define LSTHRESH		0x628		

static inline void periodic_reinit (struct ohci_hcd *ohci)
{
	u32	fi = ohci->fminterval & 0x03fff;
	u32	fit = ohci_readl(ohci, &ohci->regs->fminterval) & FIT;

	ohci_writel (ohci, (fit ^ FIT) | ohci->fminterval,
						&ohci->regs->fminterval);
	ohci_writel (ohci, ((9 * fi) / 10) & 0x3fff,
						&ohci->regs->periodicstart);
}

#define read_roothub(hc, register, mask) ({ \
	u32 temp = ohci_readl (hc, &hc->regs->roothub.register); \
	if (temp == -1) \
		hc->rh_state = OHCI_RH_HALTED; \
	else if (hc->flags & OHCI_QUIRK_AMD756) \
		while (temp & mask) \
			temp = ohci_readl (hc, &hc->regs->roothub.register); \
	temp; })

static inline u32 roothub_a (struct ohci_hcd *hc)
	{ return read_roothub (hc, a, 0xfc0fe000); }
static inline u32 roothub_b (struct ohci_hcd *hc)
	{ return ohci_readl (hc, &hc->regs->roothub.b); }
static inline u32 roothub_status (struct ohci_hcd *hc)
	{ return ohci_readl (hc, &hc->regs->roothub.status); }
static inline u32 roothub_portstatus (struct ohci_hcd *hc, int i)
	{ return read_roothub (hc, portstatus [i], 0xffe0fce0); }
