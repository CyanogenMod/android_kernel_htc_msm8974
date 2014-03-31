
#define DRV_NAME	"3c505"
#define DRV_VERSION	"1.10a"





#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gfp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>

#include "3c505.h"


#define timeout_msg "*** timeout at %s:%s (line %d) ***\n"
#define TIMEOUT_MSG(lineno) \
	pr_notice(timeout_msg, __FILE__, __func__, (lineno))

#define invalid_pcb_msg "*** invalid pcb length %d at %s:%s (line %d) ***\n"
#define INVALID_PCB_MSG(len) \
	pr_notice(invalid_pcb_msg, (len), __FILE__, __func__, __LINE__)

#define search_msg "%s: Looking for 3c505 adapter at address %#x..."

#define stilllooking_msg "still looking..."

#define found_msg "found.\n"

#define notfound_msg "not found (reason = %d)\n"

#define couldnot_msg "%s: 3c505 not found\n"


#ifdef ELP_DEBUG
static int elp_debug = ELP_DEBUG;
#else
static int elp_debug;
#endif
#define debug elp_debug



static int addr_list[] __initdata = {0x300, 0x280, 0x310, 0};


static unsigned long dma_mem_alloc(int size)
{
	int order = get_order(size);
	return __get_dma_pages(GFP_KERNEL, order);
}



static inline unsigned char inb_status(unsigned int base_addr)
{
	return inb(base_addr + PORT_STATUS);
}

static inline int inb_command(unsigned int base_addr)
{
	return inb(base_addr + PORT_COMMAND);
}

static inline void outb_control(unsigned char val, struct net_device *dev)
{
	outb(val, dev->base_addr + PORT_CONTROL);
	((elp_device *)(netdev_priv(dev)))->hcr_val = val;
}

#define HCR_VAL(x)   (((elp_device *)(netdev_priv(x)))->hcr_val)

static inline void outb_command(unsigned char val, unsigned int base_addr)
{
	outb(val, base_addr + PORT_COMMAND);
}

static inline unsigned int backlog_next(unsigned int n)
{
	return (n + 1) % BACKLOG_SIZE;
}



#define	GET_ASF(addr) \
	(get_status(addr)&ASF_PCB_MASK)

static inline int get_status(unsigned int base_addr)
{
	unsigned long timeout = jiffies + 10*HZ/100;
	register int stat1;
	do {
		stat1 = inb_status(base_addr);
	} while (stat1 != inb_status(base_addr) && time_before(jiffies, timeout));
	if (time_after_eq(jiffies, timeout))
		TIMEOUT_MSG(__LINE__);
	return stat1;
}

static inline void set_hsf(struct net_device *dev, int hsf)
{
	elp_device *adapter = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&adapter->lock, flags);
	outb_control((HCR_VAL(dev) & ~HSF_PCB_MASK) | hsf, dev);
	spin_unlock_irqrestore(&adapter->lock, flags);
}

static bool start_receive(struct net_device *, pcb_struct *);

static inline void adapter_reset(struct net_device *dev)
{
	unsigned long timeout;
	elp_device *adapter = netdev_priv(dev);
	unsigned char orig_hcr = adapter->hcr_val;

	outb_control(0, dev);

	if (inb_status(dev->base_addr) & ACRF) {
		do {
			inb_command(dev->base_addr);
			timeout = jiffies + 2*HZ/100;
			while (time_before_eq(jiffies, timeout) && !(inb_status(dev->base_addr) & ACRF));
		} while (inb_status(dev->base_addr) & ACRF);
		set_hsf(dev, HSF_PCB_NAK);
	}
	outb_control(adapter->hcr_val | ATTN | DIR, dev);
	mdelay(10);
	outb_control(adapter->hcr_val & ~ATTN, dev);
	mdelay(10);
	outb_control(adapter->hcr_val | FLSH, dev);
	mdelay(10);
	outb_control(adapter->hcr_val & ~FLSH, dev);
	mdelay(10);

	outb_control(orig_hcr, dev);
	if (!start_receive(dev, &adapter->tx_pcb))
		pr_err("%s: start receive command failed\n", dev->name);
}

static inline void check_3c505_dma(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);
	if (adapter->dmaing && time_after(jiffies, adapter->current_dma.start_time + 10)) {
		unsigned long flags, f;
		pr_err("%s: DMA %s timed out, %d bytes left\n", dev->name,
			adapter->current_dma.direction ? "download" : "upload",
			get_dma_residue(dev->dma));
		spin_lock_irqsave(&adapter->lock, flags);
		adapter->dmaing = 0;
		adapter->busy = 0;

		f=claim_dma_lock();
		disable_dma(dev->dma);
		release_dma_lock(f);

		if (adapter->rx_active)
			adapter->rx_active--;
		outb_control(adapter->hcr_val & ~(DMAE | TCEN | DIR), dev);
		spin_unlock_irqrestore(&adapter->lock, flags);
	}
}

static inline bool send_pcb_slow(unsigned int base_addr, unsigned char byte)
{
	unsigned long timeout;
	outb_command(byte, base_addr);
	for (timeout = jiffies + 5*HZ/100; time_before(jiffies, timeout);) {
		if (inb_status(base_addr) & HCRE)
			return false;
	}
	pr_warning("3c505: send_pcb_slow timed out\n");
	return true;
}

static inline bool send_pcb_fast(unsigned int base_addr, unsigned char byte)
{
	unsigned int timeout;
	outb_command(byte, base_addr);
	for (timeout = 0; timeout < 40000; timeout++) {
		if (inb_status(base_addr) & HCRE)
			return false;
	}
	pr_warning("3c505: send_pcb_fast timed out\n");
	return true;
}

static inline void prime_rx(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);
	while (adapter->rx_active < ELP_RX_PCBS && netif_running(dev)) {
		if (!start_receive(dev, &adapter->itx_pcb))
			break;
	}
}



static bool send_pcb(struct net_device *dev, pcb_struct * pcb)
{
	int i;
	unsigned long timeout;
	elp_device *adapter = netdev_priv(dev);
	unsigned long flags;

	check_3c505_dma(dev);

	if (adapter->dmaing && adapter->current_dma.direction == 0)
		return false;

	
	if (test_and_set_bit(1, &adapter->send_pcb_semaphore)) {
		if (elp_debug >= 3) {
			pr_debug("%s: send_pcb entered while threaded\n", dev->name);
		}
		return false;
	}
	set_hsf(dev, 0);

	if (send_pcb_slow(dev->base_addr, pcb->command))
		goto abort;

	spin_lock_irqsave(&adapter->lock, flags);

	if (send_pcb_fast(dev->base_addr, pcb->length))
		goto sti_abort;

	for (i = 0; i < pcb->length; i++) {
		if (send_pcb_fast(dev->base_addr, pcb->data.raw[i]))
			goto sti_abort;
	}

	outb_control(adapter->hcr_val | 3, dev);	
	outb_command(2 + pcb->length, dev->base_addr);

	
	spin_unlock_irqrestore(&adapter->lock, flags);

	for (timeout = jiffies + 5*HZ/100; time_before(jiffies, timeout);) {
		switch (GET_ASF(dev->base_addr)) {
		case ASF_PCB_ACK:
			adapter->send_pcb_semaphore = 0;
			return true;

		case ASF_PCB_NAK:
#ifdef ELP_DEBUG
			pr_debug("%s: send_pcb got NAK\n", dev->name);
#endif
			goto abort;
		}
	}

	if (elp_debug >= 1)
		pr_debug("%s: timeout waiting for PCB acknowledge (status %02x)\n",
			dev->name, inb_status(dev->base_addr));
	goto abort;

      sti_abort:
	spin_unlock_irqrestore(&adapter->lock, flags);
      abort:
	adapter->send_pcb_semaphore = 0;
	return false;
}



static bool receive_pcb(struct net_device *dev, pcb_struct * pcb)
{
	int i, j;
	int total_length;
	int stat;
	unsigned long timeout;
	unsigned long flags;

	elp_device *adapter = netdev_priv(dev);

	set_hsf(dev, 0);

	
	timeout = jiffies + 2*HZ/100;
	while (((stat = get_status(dev->base_addr)) & ACRF) == 0 && time_before(jiffies, timeout));
	if (time_after_eq(jiffies, timeout)) {
		TIMEOUT_MSG(__LINE__);
		return false;
	}
	pcb->command = inb_command(dev->base_addr);

	
	timeout = jiffies + 3*HZ/100;
	while (((stat = get_status(dev->base_addr)) & ACRF) == 0 && time_before(jiffies, timeout));
	if (time_after_eq(jiffies, timeout)) {
		TIMEOUT_MSG(__LINE__);
		pr_info("%s: status %02x\n", dev->name, stat);
		return false;
	}
	pcb->length = inb_command(dev->base_addr);

	if (pcb->length > MAX_PCB_DATA) {
		INVALID_PCB_MSG(pcb->length);
		adapter_reset(dev);
		return false;
	}
	
	spin_lock_irqsave(&adapter->lock, flags);
	for (i = 0; i < MAX_PCB_DATA; i++) {
		for (j = 0; j < 20000; j++) {
			stat = get_status(dev->base_addr);
			if (stat & ACRF)
				break;
		}
		pcb->data.raw[i] = inb_command(dev->base_addr);
		if ((stat & ASF_PCB_MASK) == ASF_PCB_END || j >= 20000)
			break;
	}
	spin_unlock_irqrestore(&adapter->lock, flags);
	if (i >= MAX_PCB_DATA) {
		INVALID_PCB_MSG(i);
		return false;
	}
	if (j >= 20000) {
		TIMEOUT_MSG(__LINE__);
		return false;
	}
	
	total_length = pcb->data.raw[i];

	
	if (total_length != (pcb->length + 2)) {
		if (elp_debug >= 2)
			pr_warning("%s: mangled PCB received\n", dev->name);
		set_hsf(dev, HSF_PCB_NAK);
		return false;
	}

	if (pcb->command == CMD_RECEIVE_PACKET_COMPLETE) {
		if (test_and_set_bit(0, (void *) &adapter->busy)) {
			if (backlog_next(adapter->rx_backlog.in) == adapter->rx_backlog.out) {
				set_hsf(dev, HSF_PCB_NAK);
				pr_warning("%s: PCB rejected, transfer in progress and backlog full\n", dev->name);
				pcb->command = 0;
				return true;
			} else {
				pcb->command = 0xff;
			}
		}
	}
	set_hsf(dev, HSF_PCB_ACK);
	return true;
}


static bool start_receive(struct net_device *dev, pcb_struct * tx_pcb)
{
	bool status;
	elp_device *adapter = netdev_priv(dev);

	if (elp_debug >= 3)
		pr_debug("%s: restarting receiver\n", dev->name);
	tx_pcb->command = CMD_RECEIVE_PACKET;
	tx_pcb->length = sizeof(struct Rcv_pkt);
	tx_pcb->data.rcv_pkt.buf_seg
	    = tx_pcb->data.rcv_pkt.buf_ofs = 0;		
	tx_pcb->data.rcv_pkt.buf_len = 1600;
	tx_pcb->data.rcv_pkt.timeout = 0;	
	status = send_pcb(dev, tx_pcb);
	if (status)
		adapter->rx_active++;
	return status;
}


static void receive_packet(struct net_device *dev, int len)
{
	int rlen;
	elp_device *adapter = netdev_priv(dev);
	void *target;
	struct sk_buff *skb;
	unsigned long flags;

	rlen = (len + 1) & ~1;
	skb = netdev_alloc_skb(dev, rlen + 2);

	if (!skb) {
		pr_warning("%s: memory squeeze, dropping packet\n", dev->name);
		target = adapter->dma_buffer;
		adapter->current_dma.target = NULL;
		
		return;
	}

	skb_reserve(skb, 2);
	target = skb_put(skb, rlen);
	if ((unsigned long)(target + rlen) >= MAX_DMA_ADDRESS) {
		adapter->current_dma.target = target;
		target = adapter->dma_buffer;
	} else {
		adapter->current_dma.target = NULL;
	}

	
	if (test_and_set_bit(0, (void *) &adapter->dmaing))
		pr_err("%s: rx blocked, DMA in progress, dir %d\n",
			dev->name, adapter->current_dma.direction);

	adapter->current_dma.direction = 0;
	adapter->current_dma.length = rlen;
	adapter->current_dma.skb = skb;
	adapter->current_dma.start_time = jiffies;

	outb_control(adapter->hcr_val | DIR | TCEN | DMAE, dev);

	flags=claim_dma_lock();
	disable_dma(dev->dma);
	clear_dma_ff(dev->dma);
	set_dma_mode(dev->dma, 0x04);	
	set_dma_addr(dev->dma, isa_virt_to_bus(target));
	set_dma_count(dev->dma, rlen);
	enable_dma(dev->dma);
	release_dma_lock(flags);

	if (elp_debug >= 3) {
		pr_debug("%s: rx DMA transfer started\n", dev->name);
	}

	if (adapter->rx_active)
		adapter->rx_active--;

	if (!adapter->busy)
		pr_warning("%s: receive_packet called, busy not set.\n", dev->name);
}


static irqreturn_t elp_interrupt(int irq, void *dev_id)
{
	int len;
	int dlen;
	int icount = 0;
	struct net_device *dev = dev_id;
	elp_device *adapter = netdev_priv(dev);
	unsigned long timeout;

	spin_lock(&adapter->lock);

	do {
		if (inb_status(dev->base_addr) & DONE) {
			if (!adapter->dmaing)
				pr_warning("%s: phantom DMA completed\n", dev->name);

			if (elp_debug >= 3)
				pr_debug("%s: %s DMA complete, status %02x\n", dev->name,
					adapter->current_dma.direction ? "tx" : "rx",
					inb_status(dev->base_addr));

			outb_control(adapter->hcr_val & ~(DMAE | TCEN | DIR), dev);
			if (adapter->current_dma.direction) {
				dev_kfree_skb_irq(adapter->current_dma.skb);
			} else {
				struct sk_buff *skb = adapter->current_dma.skb;
				if (skb) {
					if (adapter->current_dma.target) {
				  	
				  	memcpy(adapter->current_dma.target, adapter->dma_buffer, adapter->current_dma.length);
					}
					skb->protocol = eth_type_trans(skb,dev);
					dev->stats.rx_bytes += skb->len;
					netif_rx(skb);
				}
			}
			adapter->dmaing = 0;
			if (adapter->rx_backlog.in != adapter->rx_backlog.out) {
				int t = adapter->rx_backlog.length[adapter->rx_backlog.out];
				adapter->rx_backlog.out = backlog_next(adapter->rx_backlog.out);
				if (elp_debug >= 2)
					pr_debug("%s: receiving backlogged packet (%d)\n", dev->name, t);
				receive_packet(dev, t);
			} else {
				adapter->busy = 0;
			}
		} else {
			
			check_3c505_dma(dev);
		}

		timeout = jiffies + 3*HZ/100;
		while ((inb_status(dev->base_addr) & ACRF) != 0 && time_before(jiffies, timeout)) {
			if (receive_pcb(dev, &adapter->irx_pcb)) {
				switch (adapter->irx_pcb.command)
				{
				case 0:
					break;
				case 0xff:
				case CMD_RECEIVE_PACKET_COMPLETE:
					
					if (!netif_running(dev))
						break;
					len = adapter->irx_pcb.data.rcv_resp.pkt_len;
					dlen = adapter->irx_pcb.data.rcv_resp.buf_len;
					if (adapter->irx_pcb.data.rcv_resp.timeout != 0) {
						pr_err("%s: interrupt - packet not received correctly\n", dev->name);
					} else {
						if (elp_debug >= 3) {
							pr_debug("%s: interrupt - packet received of length %i (%i)\n",
								dev->name, len, dlen);
						}
						if (adapter->irx_pcb.command == 0xff) {
							if (elp_debug >= 2)
								pr_debug("%s: adding packet to backlog (len = %d)\n",
									dev->name, dlen);
							adapter->rx_backlog.length[adapter->rx_backlog.in] = dlen;
							adapter->rx_backlog.in = backlog_next(adapter->rx_backlog.in);
						} else {
							receive_packet(dev, dlen);
						}
						if (elp_debug >= 3)
							pr_debug("%s: packet received\n", dev->name);
					}
					break;

				case CMD_CONFIGURE_82586_RESPONSE:
					adapter->got[CMD_CONFIGURE_82586] = 1;
					if (elp_debug >= 3)
						pr_debug("%s: interrupt - configure response received\n", dev->name);
					break;

				case CMD_CONFIGURE_ADAPTER_RESPONSE:
					adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 1;
					if (elp_debug >= 3)
						pr_debug("%s: Adapter memory configuration %s.\n", dev->name,
						       adapter->irx_pcb.data.failed ? "failed" : "succeeded");
					break;

				case CMD_LOAD_MULTICAST_RESPONSE:
					adapter->got[CMD_LOAD_MULTICAST_LIST] = 1;
					if (elp_debug >= 3)
						pr_debug("%s: Multicast address list loading %s.\n", dev->name,
						       adapter->irx_pcb.data.failed ? "failed" : "succeeded");
					break;

				case CMD_SET_ADDRESS_RESPONSE:
					adapter->got[CMD_SET_STATION_ADDRESS] = 1;
					if (elp_debug >= 3)
						pr_debug("%s: Ethernet address setting %s.\n", dev->name,
						       adapter->irx_pcb.data.failed ? "failed" : "succeeded");
					break;


				case CMD_NETWORK_STATISTICS_RESPONSE:
					dev->stats.rx_packets += adapter->irx_pcb.data.netstat.tot_recv;
					dev->stats.tx_packets += adapter->irx_pcb.data.netstat.tot_xmit;
					dev->stats.rx_crc_errors += adapter->irx_pcb.data.netstat.err_CRC;
					dev->stats.rx_frame_errors += adapter->irx_pcb.data.netstat.err_align;
					dev->stats.rx_fifo_errors += adapter->irx_pcb.data.netstat.err_ovrrun;
					dev->stats.rx_over_errors += adapter->irx_pcb.data.netstat.err_res;
					adapter->got[CMD_NETWORK_STATISTICS] = 1;
					if (elp_debug >= 3)
						pr_debug("%s: interrupt - statistics response received\n", dev->name);
					break;

				case CMD_TRANSMIT_PACKET_COMPLETE:
					if (elp_debug >= 3)
						pr_debug("%s: interrupt - packet sent\n", dev->name);
					if (!netif_running(dev))
						break;
					switch (adapter->irx_pcb.data.xmit_resp.c_stat) {
					case 0xffff:
						dev->stats.tx_aborted_errors++;
						pr_info("%s: transmit timed out, network cable problem?\n", dev->name);
						break;
					case 0xfffe:
						dev->stats.tx_fifo_errors++;
						pr_info("%s: transmit timed out, FIFO underrun\n", dev->name);
						break;
					}
					netif_wake_queue(dev);
					break;

				default:
					pr_debug("%s: unknown PCB received - %2.2x\n",
						dev->name, adapter->irx_pcb.command);
					break;
				}
			} else {
				pr_warning("%s: failed to read PCB on interrupt\n", dev->name);
				adapter_reset(dev);
			}
		}

	} while (icount++ < 5 && (inb_status(dev->base_addr) & (ACRF | DONE)));

	prime_rx(dev);

	spin_unlock(&adapter->lock);
	return IRQ_HANDLED;
}



static int elp_open(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);
	int retval;

	if (elp_debug >= 3)
		pr_debug("%s: request to open device\n", dev->name);

	if (adapter == NULL) {
		pr_err("%s: Opening a non-existent physical device\n", dev->name);
		return -EAGAIN;
	}
	outb_control(0, dev);

	inb_command(dev->base_addr);
	adapter_reset(dev);

	adapter->rx_active = 0;

	adapter->busy = 0;
	adapter->send_pcb_semaphore = 0;
	adapter->rx_backlog.in = 0;
	adapter->rx_backlog.out = 0;

	spin_lock_init(&adapter->lock);

	if ((retval = request_irq(dev->irq, elp_interrupt, 0, dev->name, dev))) {
		pr_err("%s: could not allocate IRQ%d\n", dev->name, dev->irq);
		return retval;
	}
	if ((retval = request_dma(dev->dma, dev->name))) {
		free_irq(dev->irq, dev);
		pr_err("%s: could not allocate DMA%d channel\n", dev->name, dev->dma);
		return retval;
	}
	adapter->dma_buffer = (void *) dma_mem_alloc(DMA_BUFFER_SIZE);
	if (!adapter->dma_buffer) {
		pr_err("%s: could not allocate DMA buffer\n", dev->name);
		free_dma(dev->dma);
		free_irq(dev->irq, dev);
		return -ENOMEM;
	}
	adapter->dmaing = 0;

	outb_control(CMDE, dev);

	if (elp_debug >= 3)
		pr_debug("%s: sending 3c505 memory configuration command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_ADAPTER_MEMORY;
	adapter->tx_pcb.data.memconf.cmd_q = 10;
	adapter->tx_pcb.data.memconf.rcv_q = 20;
	adapter->tx_pcb.data.memconf.mcast = 10;
	adapter->tx_pcb.data.memconf.frame = 20;
	adapter->tx_pcb.data.memconf.rcv_b = 20;
	adapter->tx_pcb.data.memconf.progs = 0;
	adapter->tx_pcb.length = sizeof(struct Memconf);
	adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		pr_err("%s: couldn't send memory configuration command\n", dev->name);
	else {
		unsigned long timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] == 0 && time_before(jiffies, timeout));
		if (time_after_eq(jiffies, timeout))
			TIMEOUT_MSG(__LINE__);
	}


	if (elp_debug >= 3)
		pr_debug("%s: sending 82586 configure command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_82586;
	adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD;
	adapter->tx_pcb.length = 2;
	adapter->got[CMD_CONFIGURE_82586] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		pr_err("%s: couldn't send 82586 configure command\n", dev->name);
	else {
		unsigned long timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_CONFIGURE_82586] == 0 && time_before(jiffies, timeout));
		if (time_after_eq(jiffies, timeout))
			TIMEOUT_MSG(__LINE__);
	}

	
	

	prime_rx(dev);
	if (elp_debug >= 3)
		pr_debug("%s: %d receive PCBs active\n", dev->name, adapter->rx_active);


	netif_start_queue(dev);
	return 0;
}



static netdev_tx_t send_packet(struct net_device *dev, struct sk_buff *skb)
{
	elp_device *adapter = netdev_priv(dev);
	unsigned long target;
	unsigned long flags;

	unsigned int nlen = (((skb->len < 60) ? 60 : skb->len) + 1) & (~1);

	if (test_and_set_bit(0, (void *) &adapter->busy)) {
		if (elp_debug >= 2)
			pr_debug("%s: transmit blocked\n", dev->name);
		return false;
	}

	dev->stats.tx_bytes += nlen;

	adapter->tx_pcb.command = CMD_TRANSMIT_PACKET;
	adapter->tx_pcb.length = sizeof(struct Xmit_pkt);
	adapter->tx_pcb.data.xmit_pkt.buf_ofs
	    = adapter->tx_pcb.data.xmit_pkt.buf_seg = 0;	
	adapter->tx_pcb.data.xmit_pkt.pkt_len = nlen;

	if (!send_pcb(dev, &adapter->tx_pcb)) {
		adapter->busy = 0;
		return false;
	}
	
	if (test_and_set_bit(0, (void *) &adapter->dmaing))
		pr_debug("%s: tx: DMA %d in progress\n", dev->name, adapter->current_dma.direction);

	adapter->current_dma.direction = 1;
	adapter->current_dma.start_time = jiffies;

	if ((unsigned long)(skb->data + nlen) >= MAX_DMA_ADDRESS || nlen != skb->len) {
		skb_copy_from_linear_data(skb, adapter->dma_buffer, nlen);
		memset(adapter->dma_buffer+skb->len, 0, nlen-skb->len);
		target = isa_virt_to_bus(adapter->dma_buffer);
	}
	else {
		target = isa_virt_to_bus(skb->data);
	}
	adapter->current_dma.skb = skb;

	flags=claim_dma_lock();
	disable_dma(dev->dma);
	clear_dma_ff(dev->dma);
	set_dma_mode(dev->dma, 0x48);	
	set_dma_addr(dev->dma, target);
	set_dma_count(dev->dma, nlen);
	outb_control(adapter->hcr_val | DMAE | TCEN, dev);
	enable_dma(dev->dma);
	release_dma_lock(flags);

	if (elp_debug >= 3)
		pr_debug("%s: DMA transfer started\n", dev->name);

	return true;
}


static void elp_timeout(struct net_device *dev)
{
	int stat;

	stat = inb_status(dev->base_addr);
	pr_warning("%s: transmit timed out, lost %s?\n", dev->name,
		   (stat & ACRF) ? "interrupt" : "command");
	if (elp_debug >= 1)
		pr_debug("%s: status %#02x\n", dev->name, stat);
	dev->trans_start = jiffies; 
	dev->stats.tx_dropped++;
	netif_wake_queue(dev);
}


static netdev_tx_t elp_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long flags;
	elp_device *adapter = netdev_priv(dev);

	spin_lock_irqsave(&adapter->lock, flags);
	check_3c505_dma(dev);

	if (elp_debug >= 3)
		pr_debug("%s: request to send packet of length %d\n", dev->name, (int) skb->len);

	netif_stop_queue(dev);

	if (!send_packet(dev, skb)) {
		if (elp_debug >= 2) {
			pr_debug("%s: failed to transmit packet\n", dev->name);
		}
		spin_unlock_irqrestore(&adapter->lock, flags);
		return NETDEV_TX_BUSY;
	}
	if (elp_debug >= 3)
		pr_debug("%s: packet of length %d sent\n", dev->name, (int) skb->len);

	prime_rx(dev);
	spin_unlock_irqrestore(&adapter->lock, flags);
	netif_start_queue(dev);
	return NETDEV_TX_OK;
}


static struct net_device_stats *elp_get_stats(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);

	if (elp_debug >= 3)
		pr_debug("%s: request for stats\n", dev->name);

	if (!netif_running(dev))
		return &dev->stats;

	
	adapter->tx_pcb.command = CMD_NETWORK_STATISTICS;
	adapter->tx_pcb.length = 0;
	adapter->got[CMD_NETWORK_STATISTICS] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		pr_err("%s: couldn't send get statistics command\n", dev->name);
	else {
		unsigned long timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_NETWORK_STATISTICS] == 0 && time_before(jiffies, timeout));
		if (time_after_eq(jiffies, timeout)) {
			TIMEOUT_MSG(__LINE__);
			return &dev->stats;
		}
	}

	
	return &dev->stats;
}


static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "ISA 0x%lx", dev->base_addr);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 level)
{
	debug = level;
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
};


static int elp_close(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);

	if (elp_debug >= 3)
		pr_debug("%s: request to close device\n", dev->name);

	netif_stop_queue(dev);

	(void) elp_get_stats(dev);

	outb_control(0, dev);

	free_irq(dev->irq, dev);

	free_dma(dev->dma);
	free_pages((unsigned long) adapter->dma_buffer, get_order(DMA_BUFFER_SIZE));

	return 0;
}



static void elp_set_mc_list(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);
	struct netdev_hw_addr *ha;
	int i;
	unsigned long flags;

	if (elp_debug >= 3)
		pr_debug("%s: request to set multicast list\n", dev->name);

	spin_lock_irqsave(&adapter->lock, flags);

	if (!(dev->flags & (IFF_PROMISC | IFF_ALLMULTI))) {
		
		
		adapter->tx_pcb.command = CMD_LOAD_MULTICAST_LIST;
		adapter->tx_pcb.length = 6 * netdev_mc_count(dev);
		i = 0;
		netdev_for_each_mc_addr(ha, dev)
			memcpy(adapter->tx_pcb.data.multicast[i++],
			       ha->addr, 6);
		adapter->got[CMD_LOAD_MULTICAST_LIST] = 0;
		if (!send_pcb(dev, &adapter->tx_pcb))
			pr_err("%s: couldn't send set_multicast command\n", dev->name);
		else {
			unsigned long timeout = jiffies + TIMEOUT;
			while (adapter->got[CMD_LOAD_MULTICAST_LIST] == 0 && time_before(jiffies, timeout));
			if (time_after_eq(jiffies, timeout)) {
				TIMEOUT_MSG(__LINE__);
			}
		}
		if (!netdev_mc_empty(dev))
			adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD | RECV_MULTI;
		else		
			adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD;
	} else
		adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_PROMISC;
	if (elp_debug >= 3)
		pr_debug("%s: sending 82586 configure command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_82586;
	adapter->tx_pcb.length = 2;
	adapter->got[CMD_CONFIGURE_82586] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
	{
		spin_unlock_irqrestore(&adapter->lock, flags);
		pr_err("%s: couldn't send 82586 configure command\n", dev->name);
	}
	else {
		unsigned long timeout = jiffies + TIMEOUT;
		spin_unlock_irqrestore(&adapter->lock, flags);
		while (adapter->got[CMD_CONFIGURE_82586] == 0 && time_before(jiffies, timeout));
		if (time_after_eq(jiffies, timeout))
			TIMEOUT_MSG(__LINE__);
	}
}


static int __init elp_sense(struct net_device *dev)
{
	int addr = dev->base_addr;
	const char *name = dev->name;
	byte orig_HSR;

	if (!request_region(addr, ELP_IO_EXTENT, "3c505"))
		return -ENODEV;

	orig_HSR = inb_status(addr);

	if (elp_debug > 0)
		pr_debug(search_msg, name, addr);

	if (orig_HSR == 0xff) {
		if (elp_debug > 0)
			pr_cont(notfound_msg, 1);
		goto out;
	}

	
	if (elp_debug > 0)
		pr_cont(stilllooking_msg);

	if (orig_HSR & DIR) {
		
		outb(0, dev->base_addr + PORT_CONTROL);
		msleep(300);
		if (inb_status(addr) & DIR) {
			if (elp_debug > 0)
				pr_cont(notfound_msg, 2);
			goto out;
		}
	} else {
		
		outb(DIR, dev->base_addr + PORT_CONTROL);
		msleep(300);
		if (!(inb_status(addr) & DIR)) {
			if (elp_debug > 0)
				pr_cont(notfound_msg, 3);
			goto out;
		}
	}
	if (elp_debug > 0)
		pr_cont(found_msg);

	return 0;
out:
	release_region(addr, ELP_IO_EXTENT);
	return -ENODEV;
}


static int __init elp_autodetect(struct net_device *dev)
{
	int idx = 0;

	if (dev->base_addr != 0) {	
		if (elp_sense(dev) == 0)
			return dev->base_addr;
	} else
		while ((dev->base_addr = addr_list[idx++])) {
			if (elp_sense(dev) == 0)
				return dev->base_addr;
		}

	
	if (elp_debug > 0)
		pr_debug(couldnot_msg, dev->name);

	return 0;		
}

static const struct net_device_ops elp_netdev_ops = {
	.ndo_open		= elp_open,
	.ndo_stop		= elp_close,
	.ndo_get_stats 		= elp_get_stats,
	.ndo_start_xmit		= elp_start_xmit,
	.ndo_tx_timeout 	= elp_timeout,
	.ndo_set_rx_mode	= elp_set_mc_list,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};



static int __init elplus_setup(struct net_device *dev)
{
	elp_device *adapter = netdev_priv(dev);
	int i, tries, tries1, okay;
	unsigned long timeout;
	unsigned long cookie = 0;
	int err = -ENODEV;


	dev->base_addr = elp_autodetect(dev);
	if (!dev->base_addr)
		return -ENODEV;

	adapter->send_pcb_semaphore = 0;

	for (tries1 = 0; tries1 < 3; tries1++) {
		outb_control((adapter->hcr_val | CMDE) & ~DIR, dev);
		timeout = jiffies + 5*HZ/100;
		okay = 0;
		while (time_before(jiffies, timeout) && !(inb_status(dev->base_addr) & HCRE));
		if ((inb_status(dev->base_addr) & HCRE)) {
			outb_command(0, dev->base_addr);	
			timeout = jiffies + 5*HZ/100;
			while (time_before(jiffies, timeout) && !(inb_status(dev->base_addr) & HCRE));
			if (inb_status(dev->base_addr) & HCRE)
				okay = 1;
		}
		if (!okay) {
			pr_err("%s: command register wouldn't drain, ", dev->name);
			if ((inb_status(dev->base_addr) & 7) == 3) {
				pr_cont("assuming 3c505 still starting\n");
				timeout = jiffies + 10*HZ;
				while (time_before(jiffies, timeout) && (inb_status(dev->base_addr) & 7));
				if (inb_status(dev->base_addr) & 7) {
					pr_err("%s: 3c505 failed to start\n", dev->name);
				} else {
					okay = 1;  
				}
			} else {
				pr_cont("3c505 is sulking\n");
			}
		}
		for (tries = 0; tries < 5 && okay; tries++) {

			adapter->tx_pcb.command = CMD_STATION_ADDRESS;
			adapter->tx_pcb.length = 0;
			cookie = probe_irq_on();
			if (!send_pcb(dev, &adapter->tx_pcb)) {
				pr_err("%s: could not send first PCB\n", dev->name);
				probe_irq_off(cookie);
				continue;
			}
			if (!receive_pcb(dev, &adapter->rx_pcb)) {
				pr_err("%s: could not read first PCB\n", dev->name);
				probe_irq_off(cookie);
				continue;
			}
			if ((adapter->rx_pcb.command != CMD_ADDRESS_RESPONSE) ||
			    (adapter->rx_pcb.length != 6)) {
				pr_err("%s: first PCB wrong (%d, %d)\n", dev->name,
					adapter->rx_pcb.command, adapter->rx_pcb.length);
				probe_irq_off(cookie);
				continue;
			}
			goto okay;
		}
		pr_info("%s: resetting adapter\n", dev->name);
		outb_control(adapter->hcr_val | FLSH | ATTN, dev);
		outb_control(adapter->hcr_val & ~(FLSH | ATTN), dev);
	}
	pr_err("%s: failed to initialise 3c505\n", dev->name);
	goto out;

      okay:
	if (dev->irq) {		
		int rpt = probe_irq_off(cookie);
		if (dev->irq != rpt) {
			pr_warning("%s: warning, irq %d configured but %d detected\n", dev->name, dev->irq, rpt);
		}
		
	} else		       
		dev->irq = probe_irq_off(cookie);
	switch (dev->irq) {    
	case 0:
		pr_err("%s: IRQ probe failed: check 3c505 jumpers.\n",
		       dev->name);
		goto out;
	case 1:
	case 6:
	case 8:
	case 13:
		pr_err("%s: Impossible IRQ %d reported by probe_irq_off().\n",
		       dev->name, dev->irq);
		       goto out;
	}
	outb_control(adapter->hcr_val & ~CMDE, dev);

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = adapter->rx_pcb.data.eth_addr[i];

	
	if (!dev->dma) {
		if (dev->mem_start) {
			dev->dma = dev->mem_start & 7;
		}
		else {
			pr_warning("%s: warning, DMA channel not specified, using default\n", dev->name);
			dev->dma = ELP_DMA;
		}
	}

	pr_info("%s: 3c505 at %#lx, irq %d, dma %d, addr %pM, ",
		dev->name, dev->base_addr, dev->irq, dev->dma, dev->dev_addr);

	adapter->tx_pcb.command = CMD_ADAPTER_INFO;
	adapter->tx_pcb.length = 0;
	if (!send_pcb(dev, &adapter->tx_pcb) ||
	    !receive_pcb(dev, &adapter->rx_pcb) ||
	    (adapter->rx_pcb.command != CMD_ADAPTER_INFO_RESPONSE) ||
	    (adapter->rx_pcb.length != 10)) {
		pr_cont("not responding to second PCB\n");
	}
	pr_cont("rev %d.%d, %dk\n", adapter->rx_pcb.data.info.major_vers,
		adapter->rx_pcb.data.info.minor_vers, adapter->rx_pcb.data.info.RAM_sz);

	adapter->tx_pcb.command = CMD_CONFIGURE_ADAPTER_MEMORY;
	adapter->tx_pcb.length = 12;
	adapter->tx_pcb.data.memconf.cmd_q = 8;
	adapter->tx_pcb.data.memconf.rcv_q = 8;
	adapter->tx_pcb.data.memconf.mcast = 10;
	adapter->tx_pcb.data.memconf.frame = 10;
	adapter->tx_pcb.data.memconf.rcv_b = 10;
	adapter->tx_pcb.data.memconf.progs = 0;
	if (!send_pcb(dev, &adapter->tx_pcb) ||
	    !receive_pcb(dev, &adapter->rx_pcb) ||
	    (adapter->rx_pcb.command != CMD_CONFIGURE_ADAPTER_RESPONSE) ||
	    (adapter->rx_pcb.length != 2)) {
		pr_err("%s: could not configure adapter memory\n", dev->name);
	}
	if (adapter->rx_pcb.data.configure) {
		pr_err("%s: adapter configuration failed\n", dev->name);
	}

	dev->netdev_ops = &elp_netdev_ops;
	dev->watchdog_timeo = 10*HZ;
	dev->ethtool_ops = &netdev_ethtool_ops;		

	dev->mem_start = dev->mem_end = 0;

	err = register_netdev(dev);
	if (err)
		goto out;

	return 0;
out:
	release_region(dev->base_addr, ELP_IO_EXTENT);
	return err;
}

#ifndef MODULE
struct net_device * __init elplus_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(elp_device));
	int err;
	if (!dev)
		return ERR_PTR(-ENOMEM);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = elplus_setup(dev);
	if (err) {
		free_netdev(dev);
		return ERR_PTR(err);
	}
	return dev;
}

#else
static struct net_device *dev_3c505[ELP_MAX_CARDS];
static int io[ELP_MAX_CARDS];
static int irq[ELP_MAX_CARDS];
static int dma[ELP_MAX_CARDS];
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param_array(dma, int, NULL, 0);
MODULE_PARM_DESC(io, "EtherLink Plus I/O base address(es)");
MODULE_PARM_DESC(irq, "EtherLink Plus IRQ number(s) (assigned)");
MODULE_PARM_DESC(dma, "EtherLink Plus DMA channel(s)");

int __init init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < ELP_MAX_CARDS; this_dev++) {
		struct net_device *dev = alloc_etherdev(sizeof(elp_device));
		if (!dev)
			break;

		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		if (dma[this_dev]) {
			dev->dma = dma[this_dev];
		} else {
			dev->dma = ELP_DMA;
			pr_warning("3c505.c: warning, using default DMA channel,\n");
		}
		if (io[this_dev] == 0) {
			if (this_dev) {
				free_netdev(dev);
				break;
			}
			pr_notice("3c505.c: module autoprobe not recommended, give io=xx.\n");
		}
		if (elplus_setup(dev) != 0) {
			pr_warning("3c505.c: Failed to register card at 0x%x.\n", io[this_dev]);
			free_netdev(dev);
			break;
		}
		dev_3c505[this_dev] = dev;
		found++;
	}
	if (!found)
		return -ENODEV;
	return 0;
}

void __exit cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < ELP_MAX_CARDS; this_dev++) {
		struct net_device *dev = dev_3c505[this_dev];
		if (dev) {
			unregister_netdev(dev);
			release_region(dev->base_addr, ELP_IO_EXTENT);
			free_netdev(dev);
		}
	}
}

#endif				
MODULE_LICENSE("GPL");
