
#ifndef DE600_IO
#define DE600_IO	0x378
#endif

#define DATA_PORT	(DE600_IO)
#define STATUS_PORT	(DE600_IO + 1)
#define COMMAND_PORT	(DE600_IO + 2)

#ifndef DE600_IRQ
#define DE600_IRQ	7
#endif

#define SELECT_NIC	0x04 
#define SELECT_PRN	0x1c 
#define NML_PRN		0xec 
#define IRQEN		0x10 

#define RX_BUSY		0x80
#define RX_GOOD		0x40
#define TX_FAILED16	0x10
#define TX_BUSY		0x08

#define WRITE_DATA	0x00 
#define READ_DATA	0x01 
#define STATUS		0x02 
#define COMMAND		0x03 
#define NULL_COMMAND	0x04 
#define RX_LEN		0x05 
#define TX_ADDR		0x06 
#define RW_ADDR		0x07 
#define HI_NIBBLE	0x08 

#define RX_ALL		0x01 
#define RX_BP		0x02 
#define RX_MBP		0x03 

#define TX_ENABLE	0x04 
#define RX_ENABLE	0x08 

#define RESET		0x80 
#define STOP_RESET	0x00 

#define RX_PAGE2_SELECT	0x10 
#define RX_BASE_PAGE	0x20 
#define FLIP_IRQ	0x40 

#define MEM_2K		0x0800 
#define MEM_4K		0x1000 
#define MEM_6K		0x1800 
#define NODE_ADDRESS	0x2000 

#define RUNT 60		


static u8	de600_read_status(struct net_device *dev);
static u8	de600_read_byte(unsigned char type, struct net_device *dev);

static int	de600_open(struct net_device *dev);
static int	de600_close(struct net_device *dev);
static int	de600_start_xmit(struct sk_buff *skb, struct net_device *dev);

static irqreturn_t de600_interrupt(int irq, void *dev_id);
static int	de600_tx_intr(struct net_device *dev, int irq_status);
static void	de600_rx_intr(struct net_device *dev);

static void	trigger_interrupt(struct net_device *dev);
static int	adapter_init(struct net_device *dev);


#define select_prn() outb_p(SELECT_PRN, COMMAND_PORT); DE600_SLOW_DOWN
#define select_nic() outb_p(SELECT_NIC, COMMAND_PORT); DE600_SLOW_DOWN

#define de600_put_byte(data) ( \
	outb_p(((data) << 4)   | WRITE_DATA            , DATA_PORT), \
	outb_p(((data) & 0xf0) | WRITE_DATA | HI_NIBBLE, DATA_PORT))

#define de600_put_command(cmd) ( \
	outb_p(( rx_page        << 4)   | COMMAND            , DATA_PORT), \
	outb_p(( rx_page        & 0xf0) | COMMAND | HI_NIBBLE, DATA_PORT), \
	outb_p(((rx_page | cmd) << 4)   | COMMAND            , DATA_PORT), \
	outb_p(((rx_page | cmd) & 0xf0) | COMMAND | HI_NIBBLE, DATA_PORT))

#define de600_setup_address(addr,type) ( \
	outb_p((((addr) << 4) & 0xf0) | type            , DATA_PORT), \
	outb_p(( (addr)       & 0xf0) | type | HI_NIBBLE, DATA_PORT), \
	outb_p((((addr) >> 4) & 0xf0) | type            , DATA_PORT), \
	outb_p((((addr) >> 8) & 0xf0) | type | HI_NIBBLE, DATA_PORT))

#define rx_page_adr() ((rx_page & RX_PAGE2_SELECT)?(MEM_6K):(MEM_4K))

#define next_rx_page() (rx_page ^= RX_PAGE2_SELECT)

#define tx_page_adr(a) (((a) + 1) * MEM_2K)
