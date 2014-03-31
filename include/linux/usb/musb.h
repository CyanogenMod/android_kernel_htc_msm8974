
#ifndef __LINUX_USB_MUSB_H
#define __LINUX_USB_MUSB_H

enum musb_mode {
	MUSB_UNDEFINED = 0,
	MUSB_HOST,		
	MUSB_PERIPHERAL,	
	MUSB_OTG		
};

struct clk;

enum musb_fifo_style {
	FIFO_RXTX,
	FIFO_TX,
	FIFO_RX
} __attribute__ ((packed));

enum musb_buf_mode {
	BUF_SINGLE,
	BUF_DOUBLE
} __attribute__ ((packed));

struct musb_fifo_cfg {
	u8			hw_ep_num;
	enum musb_fifo_style	style;
	enum musb_buf_mode	mode;
	u16			maxpacket;
};

#define MUSB_EP_FIFO(ep, st, m, pkt)		\
{						\
	.hw_ep_num	= ep,			\
	.style		= st,			\
	.mode		= m,			\
	.maxpacket	= pkt,			\
}

#define MUSB_EP_FIFO_SINGLE(ep, st, pkt)	\
	MUSB_EP_FIFO(ep, st, BUF_SINGLE, pkt)

#define MUSB_EP_FIFO_DOUBLE(ep, st, pkt)	\
	MUSB_EP_FIFO(ep, st, BUF_DOUBLE, pkt)

struct musb_hdrc_eps_bits {
	const char	name[16];
	u8		bits;
};

struct musb_hdrc_config {
	struct musb_fifo_cfg	*fifo_cfg;	
	unsigned		fifo_cfg_size;	

	
	unsigned	multipoint:1;	
	unsigned	dyn_fifo:1 __deprecated; 
	unsigned	soft_con:1 __deprecated; 
	unsigned	utm_16:1 __deprecated; 
	unsigned	big_endian:1;	
	unsigned	mult_bulk_tx:1;	
	unsigned	mult_bulk_rx:1;	
	unsigned	high_iso_tx:1;	
	unsigned	high_iso_rx:1;	
	unsigned	dma:1 __deprecated; 
	unsigned	vendor_req:1 __deprecated; 

	u8		num_eps;	
	u8		dma_channels __deprecated; 
	u8		dyn_fifo_size;	
	u8		vendor_ctrl __deprecated; 
	u8		vendor_stat __deprecated; 
	u8		dma_req_chan __deprecated; 
	u8		ram_bits;	

	struct musb_hdrc_eps_bits *eps_bits __deprecated;
#ifdef CONFIG_BLACKFIN
	
	unsigned int	gpio_vrsel;
	unsigned int	gpio_vrsel_active;
	
	unsigned char   clkin;
#endif

};

struct musb_hdrc_platform_data {
	
	u8		mode;

	
	const char	*clock;

	
	int		(*set_vbus)(struct device *dev, int is_on);

	
	u8		power;

	
	u8		min_power;

	
	u8		potpgt;

	
	unsigned	extvbus:1;

	
	int		(*set_power)(int state);

	
	struct musb_hdrc_config	*config;

	
	void		*board_data;

	
	const void	*platform_ops;
};



#define	TUSB6010_OSCCLK_60	16667	
#define	TUSB6010_REFCLK_24	41667	
#define	TUSB6010_REFCLK_19	52083	

#ifdef	CONFIG_ARCH_OMAP2

extern int __init tusb6010_setup_interface(
		struct musb_hdrc_platform_data *data,
		unsigned ps_refclk, unsigned waitpin,
		unsigned async_cs, unsigned sync_cs,
		unsigned irq, unsigned dmachan);

extern int tusb6010_platform_retime(unsigned is_refclk);

#endif	

#endif 
