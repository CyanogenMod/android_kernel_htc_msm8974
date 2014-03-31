/*
 *  tms380tr.c: A network driver library for Texas Instruments TMS380-based
 *              Token Ring Adapters.
 *
 *  Originally sktr.c: Written 1997 by Christoph Goos
 *
 *  A fine result of the Linux Systems Network Architecture Project.
 *  http://www.vanheusden.com/sna/ 
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 *
 *  The following modules are currently available for card support:
 *	- tmspci (Generic PCI card support)
 *	- abyss (Madge PCI support)
 *      - tmsisa (SysKonnect TR4/16 ISA)
 *
 *  Sources:
 *  	- The hardware related parts of this driver are take from
 *  	  the SysKonnect Token Ring driver for Windows NT.
 *  	- I used the IBM Token Ring driver 'ibmtr.c' as a base for this
 *  	  driver, as well as the 'skeleton.c' driver by Donald Becker.
 *  	- Also various other drivers in the linux source tree were taken
 *  	  as samples for some tasks.
 *      - TI TMS380 Second-Generation Token Ring User's Guide
 *  	- TI datasheets for respective chips
 *  	- David Hein at Texas Instruments 
 *  	- Various Madge employees
 *
 *  Maintainer(s):
 *    JS	Jay Schulist		jschlst@samba.org
 *    CG	Christoph Goos		cgoos@syskonnect.de
 *    AF	Adam Fritzler
 *    MLP       Mike Phillips           phillim@amtrak.com
 *    JF	Jochen Friedrich	jochen@scram.de
 *     
 *  Modification History:
 *	29-Aug-97	CG	Created
 *	04-Apr-98	CG	Fixed problems caused by tok_timer_check
 *	10-Apr-98	CG	Fixed lockups at cable disconnection
 *	27-May-98	JS	Formated to Linux Kernel Format
 *	31-May-98	JS	Hacked in PCI support
 *	16-Jun-98	JS	Modulized for multiple cards with one driver
 *	   Sep-99	AF	Renamed to tms380tr (supports more than SK's)
 *      23-Sep-99	AF      Added Compaq and Thomas-Conrad PCI support
 *				Fixed a bug causing double copies on PCI
 *				Fixed for new multicast stuff (2.2/2.3)
 *	25-Sep-99	AF	Uped TPL_NUM from 3 to 9
 *				Removed extraneous 'No free TPL'
 *	22-Dec-99	AF	Added Madge PCI Mk2 support and generalized
 *				parts of the initilization procedure.
 *	30-Dec-99	AF	Turned tms380tr into a library ala 8390.
 *				Madge support is provided in the abyss module
 *				Generic PCI support is in the tmspci module.
 *	30-Nov-00	JF	Updated PCI code to support IO MMU via
 *				pci_map_static(). Alpha uses this MMU for ISA
 *				as well.
 *      14-Jan-01	JF	Fix DMA on ifdown/ifup sequences. Some 
 *      			cleanup.
 *	13-Jan-02	JF	Add spinlock to fix race condition.
 *	09-Nov-02	JF	Fixed printks to not SPAM the console during
 *				normal operation.
 *	30-Dec-02	JF	Removed incorrect __init from 
 *				tms380tr_init_card.
 *	22-Jul-05	JF	Converted to dma-mapping.
 *      			
 *  To do:
 *    1. Multi/Broadcast packet handling (this may have fixed itself)
 *    2. Write a sktrisa module that includes the old ISA support (done)
 *    3. Allow modules to load their own microcode
 *    4. Speed up the BUD process -- freezing the kernel for 3+sec is
 *         quite unacceptable.
 *    5. Still a few remaining stalls when the cable is unplugged.
 */

#ifdef MODULE
static const char version[] = "tms380tr.c: v1.10 30/12/2002 by Christoph Goos, Adam Fritzler\n";
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/trdevice.h>
#include <linux/firmware.h>
#include <linux/bitops.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "tms380tr.h"		

#ifndef TMS380TR_DEBUG
#define TMS380TR_DEBUG 0
#endif
static unsigned int tms380tr_debug = TMS380TR_DEBUG;


static int      tms380tr_bringup_diags(struct net_device *dev);
static void	tms380tr_cancel_tx_queue(struct net_local* tp);
static int 	tms380tr_chipset_init(struct net_device *dev);
static void 	tms380tr_chk_irq(struct net_device *dev);
static void 	tms380tr_chk_outstanding_cmds(struct net_device *dev);
static void 	tms380tr_chk_src_addr(unsigned char *frame, unsigned char *hw_addr);
static unsigned char tms380tr_chk_ssb(struct net_local *tp, unsigned short IrqType);
int	 	tms380tr_close(struct net_device *dev);
static void 	tms380tr_cmd_status_irq(struct net_device *dev);
static void 	tms380tr_disable_interrupts(struct net_device *dev);
#if TMS380TR_DEBUG > 0
static void 	tms380tr_dump(unsigned char *Data, int length);
#endif
static void 	tms380tr_enable_interrupts(struct net_device *dev);
static void 	tms380tr_exec_cmd(struct net_device *dev, unsigned short Command);
static void 	tms380tr_exec_sifcmd(struct net_device *dev, unsigned int WriteValue);
static struct net_device_stats *tms380tr_get_stats(struct net_device *dev);
static netdev_tx_t tms380tr_hardware_send_packet(struct sk_buff *skb,
						       struct net_device *dev);
static int 	tms380tr_init_adapter(struct net_device *dev);
static void 	tms380tr_init_ipb(struct net_local *tp);
static void 	tms380tr_init_net_local(struct net_device *dev);
static void 	tms380tr_init_opb(struct net_device *dev);
int		tms380tr_open(struct net_device *dev);
static void	tms380tr_open_adapter(struct net_device *dev);
static void 	tms380tr_rcv_status_irq(struct net_device *dev);
static int 	tms380tr_read_ptr(struct net_device *dev);
static void 	tms380tr_read_ram(struct net_device *dev, unsigned char *Data,
			unsigned short Address, int Length);
static int 	tms380tr_reset_adapter(struct net_device *dev);
static void 	tms380tr_reset_interrupt(struct net_device *dev);
static void 	tms380tr_ring_status_irq(struct net_device *dev);
static netdev_tx_t tms380tr_send_packet(struct sk_buff *skb,
					      struct net_device *dev);
static void 	tms380tr_set_multicast_list(struct net_device *dev);
static int	tms380tr_set_mac_address(struct net_device *dev, void *addr);
static void 	tms380tr_timer_chk(unsigned long data);
static void 	tms380tr_timer_end_wait(unsigned long data);
static void 	tms380tr_tx_status_irq(struct net_device *dev);
static void 	tms380tr_update_rcv_stats(struct net_local *tp,
			unsigned char DataPtr[], unsigned int Length);
void	 	tms380tr_wait(unsigned long time);
static void 	tms380tr_write_rpl_status(RPL *rpl, unsigned int Status);
static void 	tms380tr_write_tpl_status(TPL *tpl, unsigned int Status);

#define SIFREADB(reg) \
	(((struct net_local *)netdev_priv(dev))->sifreadb(dev, reg))
#define SIFWRITEB(val, reg) \
	(((struct net_local *)netdev_priv(dev))->sifwriteb(dev, val, reg))
#define SIFREADW(reg) \
	(((struct net_local *)netdev_priv(dev))->sifreadw(dev, reg))
#define SIFWRITEW(val, reg) \
	(((struct net_local *)netdev_priv(dev))->sifwritew(dev, val, reg))



#if 0 
static int madgemc_sifprobe(struct net_device *dev)
{
        unsigned char old, chk1, chk2;
	
	old = SIFREADB(SIFADR);  

        chk1 = 0;       
        do {
		madgemc_setregpage(dev, 0);
                
		SIFWRITEB(chk1, SIFADR);
		chk2 = SIFREADB(SIFADR);
		if (chk2 != chk1)
			return -1;
		
		madgemc_setregpage(dev, 1);
                
		chk2 = SIFREADB(SIFADD);
		if (chk2 != chk1)
			return -1;

		madgemc_setregpage(dev, 0);
                chk2 ^= 0x0FE;
		SIFWRITEB(chk2, SIFADR);

                
		madgemc_setregpage(dev, 1);
		chk2 = SIFREADB(SIFADD);
		madgemc_setregpage(dev, 0);
                chk2 ^= 0x0FE;

                if(chk1 != chk2)
                        return -1;    
                chk1 -= 2;
        } while(chk1 != 0);     

	madgemc_setregpage(dev, 0); 
        
	SIFWRITEB(old, SIFADR);

        return 0;
}
#endif

int tms380tr_open(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	int err;
	
	
	spin_lock_init(&tp->lock);
	init_timer(&tp->timer);

	

#ifdef CONFIG_ISA
	if(dev->dma > 0) 
	{
		unsigned long flags=claim_dma_lock();
		disable_dma(dev->dma);
		set_dma_mode(dev->dma, DMA_MODE_CASCADE);
		enable_dma(dev->dma);
		release_dma_lock(flags);
	}
#endif
	
	err = tms380tr_chipset_init(dev);
  	if(err)
	{
		printk(KERN_INFO "%s: Chipset initialization error\n", 
			dev->name);
		return -1;
	}

	tp->timer.expires	= jiffies + 30*HZ;
	tp->timer.function	= tms380tr_timer_end_wait;
	tp->timer.data		= (unsigned long)dev;
	add_timer(&tp->timer);

	printk(KERN_DEBUG "%s: Adapter RAM size: %dK\n", 
	       dev->name, tms380tr_read_ptr(dev));

	tms380tr_enable_interrupts(dev);
	tms380tr_open_adapter(dev);

	netif_start_queue(dev);
	
	tp->Sleeping = 1;
	interruptible_sleep_on(&tp->wait_for_tok_int);
	del_timer(&tp->timer);

	
	if(tp->AdapterVirtOpenFlag == 0)
	{
		tms380tr_disable_interrupts(dev);
		return -1;
	}

	tp->StartTime = jiffies;

	
	tp->timer.expires	= jiffies + 2*HZ;
	tp->timer.function	= tms380tr_timer_chk;
	tp->timer.data		= (unsigned long)dev;
	add_timer(&tp->timer);

	return 0;
}

static void tms380tr_timer_end_wait(unsigned long data)
{
	struct net_device *dev = (struct net_device*)data;
	struct net_local *tp = netdev_priv(dev);

	if(tp->Sleeping)
	{
		tp->Sleeping = 0;
		wake_up_interruptible(&tp->wait_for_tok_int);
	}
}

static int tms380tr_chipset_init(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	int err;

	tms380tr_init_ipb(tp);
	tms380tr_init_opb(dev);
	tms380tr_init_net_local(dev);

	if(tms380tr_debug > 3)
		printk(KERN_DEBUG "%s: Resetting adapter...\n", dev->name);
	err = tms380tr_reset_adapter(dev);
	if(err < 0)
		return -1;

	if(tms380tr_debug > 3)
		printk(KERN_DEBUG "%s: Bringup diags...\n", dev->name);
	err = tms380tr_bringup_diags(dev);
	if(err < 0)
		return -1;

	if(tms380tr_debug > 3)
		printk(KERN_DEBUG "%s: Init adapter...\n", dev->name);
	err = tms380tr_init_adapter(dev);
	if(err < 0)
		return -1;

	if(tms380tr_debug > 3)
		printk(KERN_DEBUG "%s: Done!\n", dev->name);
	return 0;
}

static void tms380tr_init_net_local(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	int i;
	dma_addr_t dmabuf;

	tp->scb.CMD	= 0;
	tp->scb.Parm[0] = 0;
	tp->scb.Parm[1] = 0;

	tp->ssb.STS	= 0;
	tp->ssb.Parm[0] = 0;
	tp->ssb.Parm[1] = 0;
	tp->ssb.Parm[2] = 0;

	tp->CMDqueue	= 0;

	tp->AdapterOpenFlag	= 0;
	tp->AdapterVirtOpenFlag = 0;
	tp->ScbInUse		= 0;
	tp->OpenCommandIssued	= 0;
	tp->ReOpenInProgress	= 0;
	tp->HaltInProgress	= 0;
	tp->TransmitHaltScheduled = 0;
	tp->LobeWireFaultLogged	= 0;
	tp->LastOpenStatus	= 0;
	tp->MaxPacketSize	= DEFAULT_PACKET_SIZE;

	
	for (i = 0; i < TPL_NUM; i++)
	{
		tp->Tpl[i].NextTPLAddr = htonl(((char *)(&tp->Tpl[(i+1) % TPL_NUM]) - (char *)tp) + tp->dmabuffer); 
		tp->Tpl[i].Status	= 0;
		tp->Tpl[i].FrameSize	= 0;
		tp->Tpl[i].FragList[0].DataCount	= 0;
		tp->Tpl[i].FragList[0].DataAddr		= 0;
		tp->Tpl[i].NextTPLPtr	= &tp->Tpl[(i+1) % TPL_NUM];
		tp->Tpl[i].MData	= NULL;
		tp->Tpl[i].TPLIndex	= i;
		tp->Tpl[i].DMABuff	= 0;
		tp->Tpl[i].BusyFlag	= 0;
	}

	tp->TplFree = tp->TplBusy = &tp->Tpl[0];

	
	for (i = 0; i < RPL_NUM; i++)
	{
		tp->Rpl[i].NextRPLAddr = htonl(((char *)(&tp->Rpl[(i+1) % RPL_NUM]) - (char *)tp) + tp->dmabuffer); 
		tp->Rpl[i].Status = (RX_VALID | RX_START_FRAME | RX_END_FRAME | RX_FRAME_IRQ);
		tp->Rpl[i].FrameSize = 0;
		tp->Rpl[i].FragList[0].DataCount = cpu_to_be16((unsigned short)tp->MaxPacketSize);

		
		tp->Rpl[i].Skb = dev_alloc_skb(tp->MaxPacketSize);
			tp->Rpl[i].DMABuff = 0;

		
		if(tp->Rpl[i].Skb == NULL)
		{
			tp->Rpl[i].SkbStat = SKB_UNAVAILABLE;
			tp->Rpl[i].FragList[0].DataAddr = htonl(((char *)tp->LocalRxBuffers[i] - (char *)tp) + tp->dmabuffer);
			tp->Rpl[i].MData = tp->LocalRxBuffers[i];
		}
		else	
		{
			tp->Rpl[i].Skb->dev = dev;
			skb_put(tp->Rpl[i].Skb, tp->MaxPacketSize);

			
			dmabuf = dma_map_single(tp->pdev, tp->Rpl[i].Skb->data, tp->MaxPacketSize, DMA_FROM_DEVICE);
			if(tp->dmalimit && (dmabuf + tp->MaxPacketSize > tp->dmalimit))
			{
				tp->Rpl[i].SkbStat = SKB_DATA_COPY;
				tp->Rpl[i].FragList[0].DataAddr = htonl(((char *)tp->LocalRxBuffers[i] - (char *)tp) + tp->dmabuffer);
				tp->Rpl[i].MData = tp->LocalRxBuffers[i];
			}
			else	
			{
				tp->Rpl[i].SkbStat = SKB_DMA_DIRECT;
				tp->Rpl[i].FragList[0].DataAddr = htonl(dmabuf);
				tp->Rpl[i].MData = tp->Rpl[i].Skb->data;
				tp->Rpl[i].DMABuff = dmabuf;
			}
		}

		tp->Rpl[i].NextRPLPtr = &tp->Rpl[(i+1) % RPL_NUM];
		tp->Rpl[i].RPLIndex = i;
	}

	tp->RplHead = &tp->Rpl[0];
	tp->RplTail = &tp->Rpl[RPL_NUM-1];
	tp->RplTail->Status = (RX_START_FRAME | RX_END_FRAME | RX_FRAME_IRQ);
}

static void tms380tr_init_ipb(struct net_local *tp)
{
	tp->ipb.Init_Options	= BURST_MODE;
	tp->ipb.CMD_Status_IV	= 0;
	tp->ipb.TX_IV		= 0;
	tp->ipb.RX_IV		= 0;
	tp->ipb.Ring_Status_IV	= 0;
	tp->ipb.SCB_Clear_IV	= 0;
	tp->ipb.Adapter_CHK_IV	= 0;
	tp->ipb.RX_Burst_Size	= BURST_SIZE;
	tp->ipb.TX_Burst_Size	= BURST_SIZE;
	tp->ipb.DMA_Abort_Thrhld = DMA_RETRIES;
	tp->ipb.SCB_Addr	= 0;
	tp->ipb.SSB_Addr	= 0;
}

static void tms380tr_init_opb(struct net_device *dev)
{
	struct net_local *tp;
	unsigned long Addr;
	unsigned short RplSize    = RPL_SIZE;
	unsigned short TplSize    = TPL_SIZE;
	unsigned short BufferSize = BUFFER_SIZE;
	int i;

	tp = netdev_priv(dev);

	tp->ocpl.OPENOptions 	 = 0;
	tp->ocpl.OPENOptions 	|= ENABLE_FULL_DUPLEX_SELECTION;
	tp->ocpl.FullDuplex 	 = 0;
	tp->ocpl.FullDuplex 	|= OPEN_FULL_DUPLEX_OFF;

        for (i=0;i<6;i++)
                tp->ocpl.NodeAddr[i] = ((unsigned char *)dev->dev_addr)[i];

	tp->ocpl.GroupAddr	 = 0;
	tp->ocpl.FunctAddr	 = 0;
	tp->ocpl.RxListSize	 = cpu_to_be16((unsigned short)RplSize);
	tp->ocpl.TxListSize	 = cpu_to_be16((unsigned short)TplSize);
	tp->ocpl.BufSize	 = cpu_to_be16((unsigned short)BufferSize);
	tp->ocpl.Reserved	 = 0;
	tp->ocpl.TXBufMin	 = TX_BUF_MIN;
	tp->ocpl.TXBufMax	 = TX_BUF_MAX;

	Addr = htonl(((char *)tp->ProductID - (char *)tp) + tp->dmabuffer);

	tp->ocpl.ProdIDAddr[0]	 = LOWORD(Addr);
	tp->ocpl.ProdIDAddr[1]	 = HIWORD(Addr);
}

static void tms380tr_open_adapter(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);

	if(tp->OpenCommandIssued)
		return;

	tp->OpenCommandIssued = 1;
	tms380tr_exec_cmd(dev, OC_OPEN);
}

static void tms380tr_disable_interrupts(struct net_device *dev)
{
	SIFWRITEB(0, SIFACL);
}

static void tms380tr_enable_interrupts(struct net_device *dev)
{
	SIFWRITEB(ACL_SINTEN, SIFACL);
}

static void tms380tr_exec_cmd(struct net_device *dev, unsigned short Command)
{
	struct net_local *tp = netdev_priv(dev);

	tp->CMDqueue |= Command;
	tms380tr_chk_outstanding_cmds(dev);
}

static void tms380tr_timeout(struct net_device *dev)
{
	dev->trans_start = jiffies; 
	netif_wake_queue(dev);
}

static netdev_tx_t tms380tr_send_packet(struct sk_buff *skb,
					      struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	netdev_tx_t rc;

	rc = tms380tr_hardware_send_packet(skb, dev);
	if(tp->TplFree->NextTPLPtr->BusyFlag)
		netif_stop_queue(dev);
	return rc;
}

static netdev_tx_t tms380tr_hardware_send_packet(struct sk_buff *skb,
						       struct net_device *dev)
{
	TPL *tpl;
	short length;
	unsigned char *buf;
	unsigned long flags;
	int i;
	dma_addr_t dmabuf, newbuf;
	struct net_local *tp = netdev_priv(dev);
   
	spin_lock_irqsave(&tp->lock, flags);
	if(tp->TplFree->NextTPLPtr->BusyFlag)  { 
		if (tms380tr_debug > 0)
			printk(KERN_DEBUG "%s: No free TPL\n", dev->name);
		spin_unlock_irqrestore(&tp->lock, flags);
		return NETDEV_TX_BUSY;
	}

	dmabuf = 0;

	

	length	= skb->len;
	dmabuf = dma_map_single(tp->pdev, skb->data, length, DMA_TO_DEVICE);
	if(tp->dmalimit && (dmabuf + length > tp->dmalimit)) {
		
		dma_unmap_single(tp->pdev, dmabuf, length, DMA_TO_DEVICE);
		dmabuf  = 0;
		i 	= tp->TplFree->TPLIndex;
		buf 	= tp->LocalTxBuffers[i];
		skb_copy_from_linear_data(skb, buf, length);
		newbuf 	= ((char *)buf - (char *)tp) + tp->dmabuffer;
	}
	else {
		
		newbuf	= dmabuf;
		buf	= skb->data;
	}
	
	tms380tr_chk_src_addr(buf, dev->dev_addr);
	tp->LastSendTime	= jiffies;
	tpl 			= tp->TplFree;	
	tpl->BusyFlag 		= 1;		
	tp->TplFree 		= tpl->NextTPLPtr;
    
	
	tpl->Skb = skb;
	tpl->DMABuff = dmabuf;
	tpl->FragList[0].DataCount = cpu_to_be16((unsigned short)length);
	tpl->FragList[0].DataAddr  = htonl(newbuf);

	
	tpl->FrameSize 	= cpu_to_be16((unsigned short)length);
	tpl->MData 	= buf;

	
	tms380tr_write_tpl_status(tpl, TX_VALID | TX_START_FRAME
				| TX_END_FRAME | TX_PASS_SRC_ADDR
				| TX_FRAME_IRQ);

	
	tms380tr_exec_sifcmd(dev, CMD_TX_VALID);
	spin_unlock_irqrestore(&tp->lock, flags);

	return NETDEV_TX_OK;
}

/*
 * Write the given value to the 'Status' field of the specified TPL.
 * NOTE: This function should be used whenever the status of any TPL must be
 * modified by the driver, because the compiler may otherwise change the
 * order of instructions such that writing the TPL status may be executed at
 * an undesirable time. When this function is used, the status is always
 * written when the function is called.
 */
static void tms380tr_write_tpl_status(TPL *tpl, unsigned int Status)
{
	tpl->Status = Status;
}

static void tms380tr_chk_src_addr(unsigned char *frame, unsigned char *hw_addr)
{
	unsigned char SRBit;

	if((((unsigned long)frame[8]) & ~0x80) != 0)	
		return;
	if((unsigned short)frame[12] != 0)		
		return;

	SRBit = frame[8] & 0x80;
	memcpy(&frame[8], hw_addr, 6);
	frame[8] |= SRBit;
}

static void tms380tr_timer_chk(unsigned long data)
{
	struct net_device *dev = (struct net_device*)data;
	struct net_local *tp = netdev_priv(dev);

	if(tp->HaltInProgress)
		return;

	tms380tr_chk_outstanding_cmds(dev);
	if(time_before(tp->LastSendTime + SEND_TIMEOUT, jiffies) &&
	   (tp->TplFree != tp->TplBusy))
	{
		
		tp->LastSendTime = jiffies;
		tms380tr_exec_cmd(dev, OC_CLOSE);	
	}

	tp->timer.expires = jiffies + 2*HZ;
	add_timer(&tp->timer);

	if(tp->AdapterOpenFlag || tp->ReOpenInProgress)
		return;
	tp->ReOpenInProgress = 1;
	tms380tr_open_adapter(dev);
}

irqreturn_t tms380tr_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_local *tp;
	unsigned short irq_type;
	int handled = 0;

	tp = netdev_priv(dev);

	irq_type = SIFREADW(SIFSTS);

	while(irq_type & STS_SYSTEM_IRQ) {
		handled = 1;
		irq_type &= STS_IRQ_MASK;

		if(!tms380tr_chk_ssb(tp, irq_type)) {
			printk(KERN_DEBUG "%s: DATA LATE occurred\n", dev->name);
			break;
		}

		switch(irq_type) {
		case STS_IRQ_RECEIVE_STATUS:
			tms380tr_reset_interrupt(dev);
			tms380tr_rcv_status_irq(dev);
			break;

		case STS_IRQ_TRANSMIT_STATUS:
			
			if(tp->ssb.Parm[0] & COMMAND_COMPLETE) {
				tp->TransmitCommandActive = 0;
					tp->TransmitHaltScheduled = 0;

					
					tms380tr_exec_cmd(dev, OC_TRANSMIT);
				}

				tms380tr_reset_interrupt(dev);
				tms380tr_tx_status_irq(dev);
				break;

		case STS_IRQ_COMMAND_STATUS:
			tms380tr_cmd_status_irq(dev);
			break;
			
		case STS_IRQ_SCB_CLEAR:
			
			tp->ScbInUse = 0;
			tms380tr_chk_outstanding_cmds(dev);
			break;
			
		case STS_IRQ_RING_STATUS:
			tms380tr_ring_status_irq(dev);
			break;

		case STS_IRQ_ADAPTER_CHECK:
			tms380tr_chk_irq(dev);
			break;

		case STS_IRQ_LLC_STATUS:
			printk(KERN_DEBUG "tms380tr: unexpected LLC status IRQ\n");
			break;
			
		case STS_IRQ_TIMER:
			printk(KERN_DEBUG "tms380tr: unexpected Timer IRQ\n");
			break;
			
		case STS_IRQ_RECEIVE_PENDING:
			printk(KERN_DEBUG "tms380tr: unexpected Receive Pending IRQ\n");
			break;
			
		default:
			printk(KERN_DEBUG "Unknown Token Ring IRQ (0x%04x)\n", irq_type);
			break;
		}

		
		if(irq_type != STS_IRQ_TRANSMIT_STATUS &&
		   irq_type != STS_IRQ_RECEIVE_STATUS) {
			tms380tr_reset_interrupt(dev);
		}

		irq_type = SIFREADW(SIFSTS);
	}

	return IRQ_RETVAL(handled);
}

static void tms380tr_reset_interrupt(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	SSB *ssb = &tp->ssb;

	/*
	 * [Workaround for "Data Late"]
	 * Set all fields of the SSB to well-defined values so we can
	 * check if the adapter has written the SSB.
	 */

	ssb->STS	= (unsigned short) -1;
	ssb->Parm[0] 	= (unsigned short) -1;
	ssb->Parm[1] 	= (unsigned short) -1;
	ssb->Parm[2] 	= (unsigned short) -1;

	tms380tr_exec_sifcmd(dev, CMD_SSB_CLEAR | CMD_CLEAR_SYSTEM_IRQ);
}

/*
 * Check if the SSB has actually been written by the adapter.
 */
static unsigned char tms380tr_chk_ssb(struct net_local *tp, unsigned short IrqType)
{
	SSB *ssb = &tp->ssb;	


	

	if(IrqType != STS_IRQ_TRANSMIT_STATUS &&
	   IrqType != STS_IRQ_RECEIVE_STATUS &&
	   IrqType != STS_IRQ_COMMAND_STATUS &&
	   IrqType != STS_IRQ_RING_STATUS)
	{
		return 1;	
	}


	if(ssb->STS == (unsigned short) -1)
		return 0;	
	if(IrqType == STS_IRQ_COMMAND_STATUS)
		return 1;	
	if(ssb->Parm[0] == (unsigned short) -1)
		return 0;	
	if(IrqType == STS_IRQ_RING_STATUS)
		return 1;	

	
	if(ssb->Parm[1] == (unsigned short) -1)
		return 0;	
	if(ssb->Parm[2] == (unsigned short) -1)
		return 0;	

	return 1;	/* All SSB fields have been written by the adapter. */
}

static void tms380tr_cmd_status_irq(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned short ssb_cmd, ssb_parm_0;
	unsigned short ssb_parm_1;
	char *open_err = "Open error -";
	char *code_err = "Open code -";

	
	ssb_cmd    = tp->ssb.STS;
	ssb_parm_0 = tp->ssb.Parm[0];
	ssb_parm_1 = tp->ssb.Parm[1];

	if(ssb_cmd == OPEN)
	{
		tp->Sleeping = 0;
		if(!tp->ReOpenInProgress)
	    		wake_up_interruptible(&tp->wait_for_tok_int);

		tp->OpenCommandIssued = 0;
		tp->ScbInUse = 0;

		if((ssb_parm_0 & 0x00FF) == GOOD_COMPLETION)
		{
			
			tp->LobeWireFaultLogged	= 0;
			tp->AdapterOpenFlag 	= 1;
			tp->AdapterVirtOpenFlag = 1;
			tp->TransmitCommandActive = 0;
			tms380tr_exec_cmd(dev, OC_TRANSMIT);
			tms380tr_exec_cmd(dev, OC_RECEIVE);

			if(tp->ReOpenInProgress)
				tp->ReOpenInProgress = 0;

			return;
		}
		else 	
		{
	    		if(ssb_parm_0 & NODE_ADDR_ERROR)
				printk(KERN_INFO "%s: Node address error\n",
					dev->name);
	    		if(ssb_parm_0 & LIST_SIZE_ERROR)
				printk(KERN_INFO "%s: List size error\n",
					dev->name);
	    		if(ssb_parm_0 & BUF_SIZE_ERROR)
				printk(KERN_INFO "%s: Buffer size error\n",
					dev->name);
	    		if(ssb_parm_0 & TX_BUF_COUNT_ERROR)
				printk(KERN_INFO "%s: Tx buffer count error\n",
					dev->name);
	    		if(ssb_parm_0 & INVALID_OPEN_OPTION)
				printk(KERN_INFO "%s: Invalid open option\n",
					dev->name);
	    		if(ssb_parm_0 & OPEN_ERROR)
			{
				
				switch(ssb_parm_0 & OPEN_PHASES_MASK)
				{
					case LOBE_MEDIA_TEST:
						if(!tp->LobeWireFaultLogged)
						{
							tp->LobeWireFaultLogged = 1;
							printk(KERN_INFO "%s: %s Lobe wire fault (check cable !).\n", dev->name, open_err);
		    				}
						tp->ReOpenInProgress	= 1;
						tp->AdapterOpenFlag 	= 0;
						tp->AdapterVirtOpenFlag = 1;
						tms380tr_open_adapter(dev);
						return;

					case PHYSICAL_INSERTION:
						printk(KERN_INFO "%s: %s Physical insertion.\n", dev->name, open_err);
						break;

					case ADDRESS_VERIFICATION:
						printk(KERN_INFO "%s: %s Address verification.\n", dev->name, open_err);
						break;

					case PARTICIPATION_IN_RING_POLL:
						printk(KERN_INFO "%s: %s Participation in ring poll.\n", dev->name, open_err);
						break;

					case REQUEST_INITIALISATION:
						printk(KERN_INFO "%s: %s Request initialisation.\n", dev->name, open_err);
						break;

					case FULLDUPLEX_CHECK:
						printk(KERN_INFO "%s: %s Full duplex check.\n", dev->name, open_err);
						break;

					default:
						printk(KERN_INFO "%s: %s Unknown open phase\n", dev->name, open_err);
						break;
				}

				
				switch(ssb_parm_0 & OPEN_ERROR_CODES_MASK)
				{
					case OPEN_FUNCTION_FAILURE:
						printk(KERN_INFO "%s: %s OPEN_FUNCTION_FAILURE", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_FUNCTION_FAILURE;
						break;

					case OPEN_SIGNAL_LOSS:
						printk(KERN_INFO "%s: %s OPEN_SIGNAL_LOSS\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_SIGNAL_LOSS;
						break;

					case OPEN_TIMEOUT:
						printk(KERN_INFO "%s: %s OPEN_TIMEOUT\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_TIMEOUT;
						break;

					case OPEN_RING_FAILURE:
						printk(KERN_INFO "%s: %s OPEN_RING_FAILURE\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_RING_FAILURE;
						break;

					case OPEN_RING_BEACONING:
						printk(KERN_INFO "%s: %s OPEN_RING_BEACONING\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_RING_BEACONING;
						break;

					case OPEN_DUPLICATE_NODEADDR:
						printk(KERN_INFO "%s: %s OPEN_DUPLICATE_NODEADDR\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_DUPLICATE_NODEADDR;
						break;

					case OPEN_REQUEST_INIT:
						printk(KERN_INFO "%s: %s OPEN_REQUEST_INIT\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_REQUEST_INIT;
						break;

					case OPEN_REMOVE_RECEIVED:
						printk(KERN_INFO "%s: %s OPEN_REMOVE_RECEIVED", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_REMOVE_RECEIVED;
						break;

					case OPEN_FULLDUPLEX_SET:
						printk(KERN_INFO "%s: %s OPEN_FULLDUPLEX_SET\n", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_FULLDUPLEX_SET;
						break;

					default:
						printk(KERN_INFO "%s: %s Unknown open err code", dev->name, code_err);
						tp->LastOpenStatus =
							OPEN_FUNCTION_FAILURE;
						break;
				}
			}

			tp->AdapterOpenFlag 	= 0;
			tp->AdapterVirtOpenFlag = 0;

			return;
		}
	}
	else
	{
		if(ssb_cmd != READ_ERROR_LOG)
			return;

		tp->MacStat.line_errors += tp->errorlogtable.Line_Error;
		tp->MacStat.burst_errors += tp->errorlogtable.Burst_Error;
		tp->MacStat.A_C_errors += tp->errorlogtable.ARI_FCI_Error;
		tp->MacStat.lost_frames += tp->errorlogtable.Lost_Frame_Error;
		tp->MacStat.recv_congest_count += tp->errorlogtable.Rx_Congest_Error;
		tp->MacStat.rx_errors += tp->errorlogtable.Rx_Congest_Error;
		tp->MacStat.frame_copied_errors += tp->errorlogtable.Frame_Copied_Error;
		tp->MacStat.token_errors += tp->errorlogtable.Token_Error;
		tp->MacStat.dummy1 += tp->errorlogtable.DMA_Bus_Error;
		tp->MacStat.dummy1 += tp->errorlogtable.DMA_Parity_Error;
		tp->MacStat.abort_delimiters += tp->errorlogtable.AbortDelimeters;
		tp->MacStat.frequency_errors += tp->errorlogtable.Frequency_Error;
		tp->MacStat.internal_errors += tp->errorlogtable.Internal_Error;
	}
}

int tms380tr_close(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	netif_stop_queue(dev);
	
	del_timer(&tp->timer);

	

	tp->HaltInProgress 	= 1;
	tms380tr_exec_cmd(dev, OC_CLOSE);
	tp->timer.expires	= jiffies + 1*HZ;
	tp->timer.function 	= tms380tr_timer_end_wait;
	tp->timer.data 		= (unsigned long)dev;
	add_timer(&tp->timer);

	tms380tr_enable_interrupts(dev);

	tp->Sleeping = 1;
	interruptible_sleep_on(&tp->wait_for_tok_int);
	tp->TransmitCommandActive = 0;
    
	del_timer(&tp->timer);
	tms380tr_disable_interrupts(dev);
   
#ifdef CONFIG_ISA
	if(dev->dma > 0) 
	{
		unsigned long flags=claim_dma_lock();
		disable_dma(dev->dma);
		release_dma_lock(flags);
	}
#endif
	
	SIFWRITEW(0xFF00, SIFCMD);
#if 0
	if(dev->dma > 0) 
		SIFWRITEB(0xff, POSREG);
#endif
	tms380tr_cancel_tx_queue(tp);

	return 0;
}

static struct net_device_stats *tms380tr_get_stats(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);

	return (struct net_device_stats *)&tp->MacStat;
}

static void tms380tr_set_multicast_list(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned int OpenOptions;
	
	OpenOptions = tp->ocpl.OPENOptions &
		~(PASS_ADAPTER_MAC_FRAMES
		  | PASS_ATTENTION_FRAMES
		  | PASS_BEACON_MAC_FRAMES
		  | COPY_ALL_MAC_FRAMES
		  | COPY_ALL_NON_MAC_FRAMES);
	
	tp->ocpl.FunctAddr = 0;
	
	if(dev->flags & IFF_PROMISC)
		
		OpenOptions |= COPY_ALL_NON_MAC_FRAMES |
			COPY_ALL_MAC_FRAMES;
	else
	{
		if(dev->flags & IFF_ALLMULTI)
		{
			
			tp->ocpl.FunctAddr = 0xFFFFFFFF;
		}
		else
		{
			struct netdev_hw_addr *ha;

			netdev_for_each_mc_addr(ha, dev) {
				((char *)(&tp->ocpl.FunctAddr))[0] |=
					ha->addr[2];
				((char *)(&tp->ocpl.FunctAddr))[1] |=
					ha->addr[3];
				((char *)(&tp->ocpl.FunctAddr))[2] |=
					ha->addr[4];
				((char *)(&tp->ocpl.FunctAddr))[3] |=
					ha->addr[5];
			}
		}
		tms380tr_exec_cmd(dev, OC_SET_FUNCT_ADDR);
	}
	
	tp->ocpl.OPENOptions = OpenOptions;
	tms380tr_exec_cmd(dev, OC_MODIFY_OPEN_PARMS);
}

void tms380tr_wait(unsigned long time)
{
#if 0
	long tmp;
	
	tmp = jiffies + time/(1000000/HZ);
	do {
		tmp = schedule_timeout_interruptible(tmp);
	} while(time_after(tmp, jiffies));
#else
	mdelay(time / 1000);
#endif
}

static void tms380tr_exec_sifcmd(struct net_device *dev, unsigned int WriteValue)
{
	unsigned short cmd;
	unsigned short SifStsValue;
	unsigned long loop_counter;

	WriteValue = ((WriteValue ^ CMD_SYSTEM_IRQ) | CMD_INTERRUPT_ADAPTER);
	cmd = (unsigned short)WriteValue;
	loop_counter = 0,5 * 800000;
	do {
		SifStsValue = SIFREADW(SIFSTS);
	} while((SifStsValue & CMD_INTERRUPT_ADAPTER) && loop_counter--);
	SIFWRITEW(cmd, SIFCMD);
}

static int tms380tr_reset_adapter(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned short *fw_ptr;
	unsigned short count, c, count2;
	const struct firmware *fw_entry = NULL;

	if (request_firmware(&fw_entry, "tms380tr.bin", tp->pdev) != 0) {
		printk(KERN_ALERT "%s: firmware %s is missing, cannot start.\n",
			dev->name, "tms380tr.bin");
		return -1;
	}

	fw_ptr = (unsigned short *)fw_entry->data;
	count2 = fw_entry->size / 2;

	
	SIFWRITEW(ACL_ARESET, SIFACL);
	tms380tr_wait(40);
	
	c = SIFREADW(SIFACL);
	tms380tr_wait(20);

	if(dev->dma == 0)	
	{
		c &= ~(ACL_NSELOUT0 | ACL_NSELOUT1);	
		if(tp->setnselout)
		  c |= (*tp->setnselout)(dev);
	}

	
	tp->ScbInUse = 0;

	c &= ~ACL_ARESET;		
	c |=  ACL_CPHALT;		
	c |= ACL_BOOT;
	c |= ACL_SINTEN;
	c &= ~ACL_PSDMAEN;		
	SIFWRITEW(c, SIFACL);
	tms380tr_wait(40);

	count = 0;
	
	do {
		if (count2 < 3) continue;

		
		SIFWRITEW(*fw_ptr, SIFADX);
		fw_ptr++;
		count2--;
		
		SIFWRITEW(*fw_ptr, SIFADD);
		fw_ptr++;
		count2--;

		if((count = *fw_ptr) != 0)	
		{
			fw_ptr++;	
			count2--;
			if (count > count2) continue;

			for(; count > 0; count--)
			{
				SIFWRITEW(*fw_ptr, SIFINC);
				fw_ptr++;
				count2--;
			}
		}
		else	
		{
			c = SIFREADW(SIFACL);
			c &= (~ACL_CPHALT | ACL_SINTEN);

			
			SIFWRITEW(c, SIFACL);
			release_firmware(fw_entry);
			return 1;
		}
	} while(count == 0);

	release_firmware(fw_entry);
	printk(KERN_INFO "%s: Adapter Download Failed\n", dev->name);
	return -1;
}

MODULE_FIRMWARE("tms380tr.bin");

static int tms380tr_bringup_diags(struct net_device *dev)
{
	int loop_cnt, retry_cnt;
	unsigned short Status;

	tms380tr_wait(HALF_SECOND);
	tms380tr_exec_sifcmd(dev, EXEC_SOFT_RESET);
	tms380tr_wait(HALF_SECOND);

	retry_cnt = BUD_MAX_RETRIES;	

	do {
		retry_cnt--;
		if(tms380tr_debug > 3)
			printk(KERN_DEBUG "BUD-Status: ");
		loop_cnt = BUD_MAX_LOOPCNT;	
		do {			
			loop_cnt--;
			tms380tr_wait(HALF_SECOND);
			Status = SIFREADW(SIFSTS);
			Status &= STS_MASK;

			if(tms380tr_debug > 3)
				printk(KERN_DEBUG " %04X\n", Status);
			
			if(Status == STS_INITIALIZE)
				return 1;
		
		} while((loop_cnt > 0) && ((Status & (STS_ERROR | STS_TEST))
			!= (STS_ERROR | STS_TEST)));

		
		if(retry_cnt > 0)
		{
			printk(KERN_INFO "%s: Adapter Software Reset.\n", 
				dev->name);
			tms380tr_exec_sifcmd(dev, EXEC_SOFT_RESET);
			tms380tr_wait(HALF_SECOND);
		}
	} while(retry_cnt > 0);

	Status = SIFREADW(SIFSTS);
	
	printk(KERN_INFO "%s: Hardware error\n", dev->name);
	
	Status &= 0x001f;
	if (Status & 0x0010)
		printk(KERN_INFO "%s: BUD Error: Timeout\n", dev->name);
	else if ((Status & 0x000f) > 6)
		printk(KERN_INFO "%s: BUD Error: Illegal Failure\n", dev->name);
	else
		printk(KERN_INFO "%s: Bring Up Diagnostics Error (%04X) occurred\n", dev->name, Status & 0x000f);

	return -1;
}

static int tms380tr_init_adapter(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);

	const unsigned char SCB_Test[6] = {0x00, 0x00, 0xC1, 0xE2, 0xD4, 0x8B};
	const unsigned char SSB_Test[8] = {0xFF, 0xFF, 0xD1, 0xD7,
						0xC5, 0xD9, 0xC3, 0xD4};
	void *ptr = (void *)&tp->ipb;
	unsigned short *ipb_ptr = (unsigned short *)ptr;
	unsigned char *cb_ptr = (unsigned char *) &tp->scb;
	unsigned char *sb_ptr = (unsigned char *) &tp->ssb;
	unsigned short Status;
	int i, loop_cnt, retry_cnt;

	
	tp->ipb.SCB_Addr = SWAPW(((char *)&tp->scb - (char *)tp) + tp->dmabuffer);
	tp->ipb.SSB_Addr = SWAPW(((char *)&tp->ssb - (char *)tp) + tp->dmabuffer);

	if(tms380tr_debug > 3)
	{
		printk(KERN_DEBUG "%s: buffer (real): %lx\n", dev->name, (long) &tp->scb);
		printk(KERN_DEBUG "%s: buffer (virt): %lx\n", dev->name, (long) ((char *)&tp->scb - (char *)tp) + (long) tp->dmabuffer);
		printk(KERN_DEBUG "%s: buffer (DMA) : %lx\n", dev->name, (long) tp->dmabuffer);
		printk(KERN_DEBUG "%s: buffer (tp)  : %lx\n", dev->name, (long) tp);
	}
	
	retry_cnt = INIT_MAX_RETRIES;

	do {
		retry_cnt--;

		
		SIFWRITEW(0x0001, SIFADX);

		
		SIFWRITEW(0x0A00, SIFADD);

		
		for(i = 0; i < 11; i++)
			SIFWRITEW(ipb_ptr[i], SIFINC);

		
		tms380tr_exec_sifcmd(dev, CMD_EXECUTE);

		loop_cnt = INIT_MAX_LOOPCNT;	

		
		do {
			Status = 0;
			loop_cnt--;
			tms380tr_wait(HALF_SECOND);

			
			Status = SIFREADW(SIFSTS);
			Status &= STS_MASK;
		} while(((Status &(STS_INITIALIZE | STS_ERROR | STS_TEST)) != 0) &&
			((Status & STS_ERROR) == 0) && (loop_cnt != 0));

		if((Status & (STS_INITIALIZE | STS_ERROR | STS_TEST)) == 0)
		{
			
			i = 0;
			do {	
				if(SCB_Test[i] != *(cb_ptr + i))
				{
					printk(KERN_INFO "%s: DMA failed\n", dev->name);
					
					return -1;
				}
				i++;
			} while(i < 6);

			i = 0;
			do {	
				if(SSB_Test[i] != *(sb_ptr + i))
					
					return -1;
				i++;
			} while (i < 8);

			return 1;	
		}
		else
		{
			if((Status & STS_ERROR) != 0)
			{
				
				Status = SIFREADW(SIFSTS);
				Status &= STS_ERROR_MASK;
				
				printk(KERN_INFO "%s: Status error: %d\n", dev->name, Status);
				return -1; 
			}
			else
			{
				if(retry_cnt > 0)
				{
					
					tms380tr_exec_sifcmd(dev, EXEC_SOFT_RESET);
					tms380tr_wait(HALF_SECOND);
				}
			}
		}
	} while(retry_cnt > 0);

	printk(KERN_INFO "%s: Retry exceeded\n", dev->name);
	return -1;
}

static void tms380tr_chk_outstanding_cmds(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned long Addr = 0;

	if(tp->CMDqueue == 0)
		return;		

	
	if(tp->ScbInUse == 1)
		return;

	if (tp->AdapterOpenFlag == 0) {
		if (tp->CMDqueue & OC_OPEN) {
			
			tp->CMDqueue ^= OC_OPEN;

			Addr = htonl(((char *)&tp->ocpl - (char *)tp) + tp->dmabuffer);
			tp->scb.Parm[0] = LOWORD(Addr);
			tp->scb.Parm[1] = HIWORD(Addr);
			tp->scb.CMD = OPEN;
		} else
			return;		
	} else {
		if (tp->CMDqueue & OC_CLOSE) {
			tp->CMDqueue ^= OC_CLOSE;
			tp->AdapterOpenFlag = 0;
			tp->scb.Parm[0] = 0; 
			tp->scb.Parm[1] = 0; 
			tp->scb.CMD = CLOSE;
			if(!tp->HaltInProgress)
				tp->CMDqueue |= OC_OPEN; 
			else
				tp->CMDqueue = 0;	
		} else if (tp->CMDqueue & OC_RECEIVE) {
			tp->CMDqueue ^= OC_RECEIVE;
			Addr = htonl(((char *)tp->RplHead - (char *)tp) + tp->dmabuffer);
			tp->scb.Parm[0] = LOWORD(Addr);
			tp->scb.Parm[1] = HIWORD(Addr);
			tp->scb.CMD = RECEIVE;
		} else if (tp->CMDqueue & OC_TRANSMIT_HALT) {
			tp->CMDqueue ^= OC_TRANSMIT_HALT;
			tp->scb.CMD = TRANSMIT_HALT;

			tp->scb.Parm[0] = 0;
			tp->scb.Parm[1] = 0;
		} else if (tp->CMDqueue & OC_TRANSMIT) {
			if (tp->TransmitCommandActive) {
				if (!tp->TransmitHaltScheduled) {
					tp->TransmitHaltScheduled = 1;
					tms380tr_exec_cmd(dev, OC_TRANSMIT_HALT);
				}
				tp->TransmitCommandActive = 0;
				return;
			}

			tp->CMDqueue ^= OC_TRANSMIT;
			tms380tr_cancel_tx_queue(tp);
			Addr = htonl(((char *)tp->TplBusy - (char *)tp) + tp->dmabuffer);
			tp->scb.Parm[0] = LOWORD(Addr);
			tp->scb.Parm[1] = HIWORD(Addr);
			tp->scb.CMD = TRANSMIT;
			tp->TransmitCommandActive = 1;
		} else if (tp->CMDqueue & OC_MODIFY_OPEN_PARMS) {
			tp->CMDqueue ^= OC_MODIFY_OPEN_PARMS;
			tp->scb.Parm[0] = tp->ocpl.OPENOptions; 
			tp->scb.Parm[0] |= ENABLE_FULL_DUPLEX_SELECTION;
			tp->scb.Parm[1] = 0; 
			tp->scb.CMD = MODIFY_OPEN_PARMS;
		} else if (tp->CMDqueue & OC_SET_FUNCT_ADDR) {
			tp->CMDqueue ^= OC_SET_FUNCT_ADDR;
			tp->scb.Parm[0] = LOWORD(tp->ocpl.FunctAddr);
			tp->scb.Parm[1] = HIWORD(tp->ocpl.FunctAddr);
			tp->scb.CMD = SET_FUNCT_ADDR;
		} else if (tp->CMDqueue & OC_SET_GROUP_ADDR) {
			tp->CMDqueue ^= OC_SET_GROUP_ADDR;
			tp->scb.Parm[0] = LOWORD(tp->ocpl.GroupAddr);
			tp->scb.Parm[1] = HIWORD(tp->ocpl.GroupAddr);
			tp->scb.CMD = SET_GROUP_ADDR;
		} else if (tp->CMDqueue & OC_READ_ERROR_LOG) {
			tp->CMDqueue ^= OC_READ_ERROR_LOG;
			Addr = htonl(((char *)&tp->errorlogtable - (char *)tp) + tp->dmabuffer);
			tp->scb.Parm[0] = LOWORD(Addr);
			tp->scb.Parm[1] = HIWORD(Addr);
			tp->scb.CMD = READ_ERROR_LOG;
		} else {
			printk(KERN_WARNING "CheckForOutstandingCommand: unknown Command\n");
			tp->CMDqueue = 0;
			return;
		}
	}

	tp->ScbInUse = 1;	

	
	tms380tr_exec_sifcmd(dev, CMD_EXECUTE | CMD_SCB_REQUEST);
}

static void tms380tr_ring_status_irq(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);

	tp->CurrentRingStatus = be16_to_cpu((unsigned short)tp->ssb.Parm[0]);

	
	if(tp->ssb.Parm[0] & SIGNAL_LOSS)
	{
		printk(KERN_INFO "%s: Signal Loss\n", dev->name);
		tp->MacStat.line_errors++;
	}

	
	if(tp->ssb.Parm[0] & LOBE_WIRE_FAULT)
	{
		printk(KERN_INFO "%s: Lobe Wire Fault, Reopen Adapter\n", 
			dev->name);
		tp->MacStat.line_errors++;
	}

	if(tp->ssb.Parm[0] & RING_RECOVERY)
		printk(KERN_INFO "%s: Ring Recovery\n", dev->name);

	
	if(tp->ssb.Parm[0] & COUNTER_OVERFLOW)
	{
		printk(KERN_INFO "%s: Counter Overflow\n", dev->name);
		tms380tr_exec_cmd(dev, OC_READ_ERROR_LOG);
	}

	
	if(tp->ssb.Parm[0] & REMOVE_RECEIVED)
		printk(KERN_INFO "%s: Remove Received, Reopen Adapter\n", 
			dev->name);

	
	if(tp->ssb.Parm[0] & AUTO_REMOVAL_ERROR)
		printk(KERN_INFO "%s: Auto Removal Error, Reopen Adapter\n", 
			dev->name);

	if(tp->ssb.Parm[0] & HARD_ERROR)
		printk(KERN_INFO "%s: Hard Error\n", dev->name);

	if(tp->ssb.Parm[0] & SOFT_ERROR)
		printk(KERN_INFO "%s: Soft Error\n", dev->name);

	if(tp->ssb.Parm[0] & TRANSMIT_BEACON)
		printk(KERN_INFO "%s: Transmit Beacon\n", dev->name);

	if(tp->ssb.Parm[0] & SINGLE_STATION)
		printk(KERN_INFO "%s: Single Station\n", dev->name);

	
	if(tp->ssb.Parm[0] & ADAPTER_CLOSED)
	{
		printk(KERN_INFO "%s: Adapter closed (Reopening)," 
			"CurrentRingStat %x\n",
			dev->name, tp->CurrentRingStatus);
		tp->AdapterOpenFlag = 0;
		tms380tr_open_adapter(dev);
	}
}

static void tms380tr_chk_irq(struct net_device *dev)
{
	int i;
	unsigned short AdapterCheckBlock[4];
	struct net_local *tp = netdev_priv(dev);

	tp->AdapterOpenFlag = 0;	

	
	SIFWRITEW(0x0001, SIFADX);
	
	SIFWRITEW(CHECKADDR, SIFADR);

	
	for(i = 0; i < 4; i++)
		AdapterCheckBlock[i] = SIFREADW(SIFINC);

	if(tms380tr_debug > 3)
	{
		printk(KERN_DEBUG "%s: AdapterCheckBlock: ", dev->name);
		for (i = 0; i < 4; i++)
			printk("%04X", AdapterCheckBlock[i]);
		printk("\n");
	}

	switch(AdapterCheckBlock[0])
	{
		case DIO_PARITY:
			printk(KERN_INFO "%s: DIO parity error\n", dev->name);
			break;

		case DMA_READ_ABORT:
			printk(KERN_INFO "%s DMA read operation aborted:\n",
				dev->name);
			switch (AdapterCheckBlock[1])
			{
				case 0:
					printk(KERN_INFO "Timeout\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2],
						AdapterCheckBlock[3]);
					break;

				case 1:
					printk(KERN_INFO "Parity error\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2], 
						AdapterCheckBlock[3]);
					break;

				case 2: 
					printk(KERN_INFO "Bus error\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2], 
						AdapterCheckBlock[3]);
					break;

				default:
					printk(KERN_INFO "Unknown error.\n");
					break;
			}
			break;

		case DMA_WRITE_ABORT:
			printk(KERN_INFO "%s: DMA write operation aborted:\n",
				dev->name);
			switch (AdapterCheckBlock[1])
			{
				case 0: 
					printk(KERN_INFO "Timeout\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2], 
						AdapterCheckBlock[3]);
					break;

				case 1: 
					printk(KERN_INFO "Parity error\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2], 
						AdapterCheckBlock[3]);
					break;

				case 2: 
					printk(KERN_INFO "Bus error\n");
					printk(KERN_INFO "Address: %04X %04X\n",
						AdapterCheckBlock[2], 
						AdapterCheckBlock[3]);
					break;

				default:
					printk(KERN_INFO "Unknown error.\n");
					break;
			}
			break;

		case ILLEGAL_OP_CODE:
			printk(KERN_INFO "%s: Illegal operation code in firmware\n",
				dev->name);
			
			break;

		case PARITY_ERRORS:
			printk(KERN_INFO "%s: Adapter internal bus parity error\n",
				dev->name);
			
			break;

		case RAM_DATA_ERROR:
			printk(KERN_INFO "%s: RAM data error\n", dev->name);
			
			break;

		case RAM_PARITY_ERROR:
			printk(KERN_INFO "%s: RAM parity error\n", dev->name);
			
			break;

		case RING_UNDERRUN:
			printk(KERN_INFO "%s: Internal DMA underrun detected\n",
				dev->name);
			break;

		case INVALID_IRQ:
			printk(KERN_INFO "%s: Unrecognized interrupt detected\n",
				dev->name);
			
			break;

		case INVALID_ERROR_IRQ:
			printk(KERN_INFO "%s: Unrecognized error interrupt detected\n",
				dev->name);
			
			break;

		case INVALID_XOP:
			printk(KERN_INFO "%s: Unrecognized XOP request detected\n",
				dev->name);
			
			break;

		default:
			printk(KERN_INFO "%s: Unknown status", dev->name);
			break;
	}

	if(tms380tr_chipset_init(dev) == 1)
	{
		
		tp->AdapterOpenFlag = 1;
	}
}

static int tms380tr_read_ptr(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned short adapterram;

	tms380tr_read_ram(dev, (unsigned char *)&tp->intptrs.BurnedInAddrPtr,
			ADAPTER_INT_PTRS, 16);
	tms380tr_read_ram(dev, (unsigned char *)&adapterram,
			cpu_to_be16((unsigned short)tp->intptrs.AdapterRAMPtr), 2);
	return be16_to_cpu(adapterram); 
}

static void tms380tr_read_ram(struct net_device *dev, unsigned char *Data,
				unsigned short Address, int Length)
{
	int i;
	unsigned short old_sifadx, old_sifadr, InWord;

	
	old_sifadx = SIFREADW(SIFADX);
	old_sifadr = SIFREADW(SIFADR);

	
	SIFWRITEW(0x0001, SIFADX);
	
        SIFWRITEW(Address, SIFADR);

	
	i = 0;
	for(;;)
	{
		InWord = SIFREADW(SIFINC);

		*(Data + i) = HIBYTE(InWord);	
		if(++i == Length)		
			break;

		*(Data + i) = LOBYTE(InWord);	
		if (++i == Length)		
			break;
	}

	
	SIFWRITEW(old_sifadx, SIFADX);
	SIFWRITEW(old_sifadr, SIFADR);
}

static void tms380tr_cancel_tx_queue(struct net_local* tp)
{
	TPL *tpl;

	if(tp->TransmitCommandActive)
		return;

	for(;;)
	{
		tpl = tp->TplBusy;
		if(!tpl->BusyFlag)
			break;
		
		tp->TplBusy = tpl->NextTPLPtr;
		tms380tr_write_tpl_status(tpl, 0);	
		tpl->BusyFlag = 0;		

		printk(KERN_INFO "Cancel tx (%08lXh).\n", (unsigned long)tpl);
		if (tpl->DMABuff)
			dma_unmap_single(tp->pdev, tpl->DMABuff, tpl->Skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_any(tpl->Skb);
	}
}

static void tms380tr_tx_status_irq(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned char HighByte, HighAc, LowAc;
	TPL *tpl;


	for(;;)
	{
		tpl = tp->TplBusy;
		if(!tpl->BusyFlag || (tpl->Status
			& (TX_VALID | TX_FRAME_COMPLETE))
			!= TX_FRAME_COMPLETE)
		{
			break;
		}

		
		tp->TplBusy = tpl->NextTPLPtr ;

		
		if(DIRECTED_FRAME(tpl) && (tpl->Status & TX_ERROR) == 0)
		{
			HighByte = GET_TRANSMIT_STATUS_HIGH_BYTE(tpl->Status);
			HighAc   = GET_FRAME_STATUS_HIGH_AC(HighByte);
			LowAc    = GET_FRAME_STATUS_LOW_AC(HighByte);

			if((HighAc != LowAc) || (HighAc == AC_NOT_RECOGNIZED))
			{
				printk(KERN_DEBUG "%s: (DA=%08lX not recognized)\n",
					dev->name,
					*(unsigned long *)&tpl->MData[2+2]);
			}
			else
			{
				if(tms380tr_debug > 3)
					printk(KERN_DEBUG "%s: Directed frame tx'd\n", 
						dev->name);
			}
		}
		else
		{
			if(!DIRECTED_FRAME(tpl))
			{
				if(tms380tr_debug > 3)
					printk(KERN_DEBUG "%s: Broadcast frame tx'd\n",
						dev->name);
			}
		}

		tp->MacStat.tx_packets++;
		if (tpl->DMABuff)
			dma_unmap_single(tp->pdev, tpl->DMABuff, tpl->Skb->len, DMA_TO_DEVICE);
		dev_kfree_skb_irq(tpl->Skb);
		tpl->BusyFlag = 0;	
	}

	if(!tp->TplFree->NextTPLPtr->BusyFlag)
		netif_wake_queue(dev);
}

static void tms380tr_rcv_status_irq(struct net_device *dev)
{
	struct net_local *tp = netdev_priv(dev);
	unsigned char *ReceiveDataPtr;
	struct sk_buff *skb;
	unsigned int Length, Length2;
	RPL *rpl;
	RPL *SaveHead;
	dma_addr_t dmabuf;


	for(;;)
	{
		rpl = tp->RplHead;
		if(rpl->Status & RX_VALID)
			break;		

		
		SaveHead = tp->RplHead;
		tp->RplHead = rpl->NextRPLPtr;

		Length = be16_to_cpu(rpl->FrameSize);

		if((rpl->Status & VALID_SINGLE_BUFFER_FRAME)
			== VALID_SINGLE_BUFFER_FRAME)
		{
			ReceiveDataPtr = rpl->MData;

			/* Workaround for delayed write of FrameSize on ISA
			 * (FrameSize is false but valid-bit is reset)
			 * Frame size is set to zero when the RPL is freed.
			 * Length2 is there because there have also been
			 * cases where the FrameSize was partially written
			 */
			Length2 = be16_to_cpu(rpl->FrameSize);

			if(Length == 0 || Length != Length2)
			{
				tp->RplHead = SaveHead;
				break;	
			}
			tms380tr_update_rcv_stats(tp,ReceiveDataPtr,Length);
			  
			if(tms380tr_debug > 3)
				printk(KERN_DEBUG "%s: Packet Length %04X (%d)\n",
					dev->name, Length, Length);
			  
			skb = rpl->Skb;
			if(rpl->SkbStat == SKB_UNAVAILABLE)
			{
				
				skb = dev_alloc_skb(tp->MaxPacketSize);
				if(skb == NULL)
				{
					
				}
				else
				{
					skb_put(skb, tp->MaxPacketSize);
					rpl->SkbStat 	= SKB_DATA_COPY;
					ReceiveDataPtr 	= rpl->MData;
				}
			}

			if(skb && (rpl->SkbStat == SKB_DATA_COPY ||
				   rpl->SkbStat == SKB_DMA_DIRECT))
			{
				if(rpl->SkbStat == SKB_DATA_COPY)
					skb_copy_to_linear_data(skb, ReceiveDataPtr,
						       Length);

				
				rpl->Skb = NULL;
				skb_trim(skb,Length);
				skb->protocol = tr_type_trans(skb,dev);
				netif_rx(skb);
			}
		}
		else	
		{
			if(rpl->Skb != NULL)
				dev_kfree_skb_irq(rpl->Skb);

			
			if(rpl->Status & RX_START_FRAME)
				
				tp->MacStat.rx_errors++;
		}
		if (rpl->DMABuff)
			dma_unmap_single(tp->pdev, rpl->DMABuff, tp->MaxPacketSize, DMA_TO_DEVICE);
		rpl->DMABuff = 0;

		
		rpl->Skb = dev_alloc_skb(tp->MaxPacketSize);
		
		if(rpl->Skb == NULL)
		{
			rpl->SkbStat = SKB_UNAVAILABLE;
			rpl->FragList[0].DataAddr = htonl(((char *)tp->LocalRxBuffers[rpl->RPLIndex] - (char *)tp) + tp->dmabuffer);
			rpl->MData = tp->LocalRxBuffers[rpl->RPLIndex];
		}
		else	
		{
			rpl->Skb->dev = dev;
			skb_put(rpl->Skb, tp->MaxPacketSize);

			
			dmabuf = dma_map_single(tp->pdev, rpl->Skb->data, tp->MaxPacketSize, DMA_FROM_DEVICE);
			if(tp->dmalimit && (dmabuf + tp->MaxPacketSize > tp->dmalimit))
			{
				rpl->SkbStat = SKB_DATA_COPY;
				rpl->FragList[0].DataAddr = htonl(((char *)tp->LocalRxBuffers[rpl->RPLIndex] - (char *)tp) + tp->dmabuffer);
				rpl->MData = tp->LocalRxBuffers[rpl->RPLIndex];
			}
			else
			{
				
				rpl->SkbStat = SKB_DMA_DIRECT;
				rpl->FragList[0].DataAddr = htonl(dmabuf);
				rpl->MData = rpl->Skb->data;
				rpl->DMABuff = dmabuf;
			}
		}

		rpl->FragList[0].DataCount = cpu_to_be16((unsigned short)tp->MaxPacketSize);
		rpl->FrameSize = 0;

		
		tp->RplTail->FrameSize = 0;

		
		tms380tr_write_rpl_status(tp->RplTail, RX_VALID | RX_FRAME_IRQ);

		
		tp->RplTail = tp->RplTail->NextRPLPtr;

		
		tms380tr_exec_sifcmd(dev, CMD_RX_VALID);
	}
}

/*
 * This function should be used whenever the status of any RPL must be
 * modified by the driver, because the compiler may otherwise change the
 * order of instructions such that writing the RPL status may be executed
 * at an undesirable time. When this function is used, the status is
 * always written when the function is called.
 */
static void tms380tr_write_rpl_status(RPL *rpl, unsigned int Status)
{
	rpl->Status = Status;
}

static void tms380tr_update_rcv_stats(struct net_local *tp, unsigned char DataPtr[],
					unsigned int Length)
{
	tp->MacStat.rx_packets++;
	tp->MacStat.rx_bytes += Length;
	
	
	if(DataPtr[2] & GROUP_BIT)
		tp->MacStat.multicast++;
}

static int tms380tr_set_mac_address(struct net_device *dev, void *addr)
{
	struct net_local *tp = netdev_priv(dev);
	struct sockaddr *saddr = addr;
	
	if (tp->AdapterOpenFlag || tp->AdapterVirtOpenFlag) {
		printk(KERN_WARNING "%s: Cannot set MAC/LAA address while card is open\n", dev->name);
		return -EIO;
	}
	memcpy(dev->dev_addr, saddr->sa_data, dev->addr_len);
	return 0;
}

#if TMS380TR_DEBUG > 0
static void tms380tr_dump(unsigned char *Data, int length)
{
	int i, j;

	for (i = 0, j = 0; i < length / 8; i++, j += 8)
	{
		printk(KERN_DEBUG "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       Data[j+0],Data[j+1],Data[j+2],Data[j+3],
		       Data[j+4],Data[j+5],Data[j+6],Data[j+7]);
	}
}
#endif

void tmsdev_term(struct net_device *dev)
{
	struct net_local *tp;

	tp = netdev_priv(dev);
	dma_unmap_single(tp->pdev, tp->dmabuffer, sizeof(struct net_local),
		DMA_BIDIRECTIONAL);
}

const struct net_device_ops tms380tr_netdev_ops = {
	.ndo_open		= tms380tr_open,
	.ndo_stop		= tms380tr_close,
	.ndo_start_xmit		= tms380tr_send_packet,
	.ndo_tx_timeout		= tms380tr_timeout,
	.ndo_get_stats		= tms380tr_get_stats,
	.ndo_set_rx_mode	= tms380tr_set_multicast_list,
	.ndo_set_mac_address	= tms380tr_set_mac_address,
};
EXPORT_SYMBOL(tms380tr_netdev_ops);

int tmsdev_init(struct net_device *dev, struct device *pdev)
{
	struct net_local *tms_local;

	memset(netdev_priv(dev), 0, sizeof(struct net_local));
	tms_local = netdev_priv(dev);
	init_waitqueue_head(&tms_local->wait_for_tok_int);
	if (pdev->dma_mask)
		tms_local->dmalimit = *pdev->dma_mask;
	else
		return -ENOMEM;
	tms_local->pdev = pdev;
	tms_local->dmabuffer = dma_map_single(pdev, (void *)tms_local,
	    sizeof(struct net_local), DMA_BIDIRECTIONAL);
	if (tms_local->dmabuffer + sizeof(struct net_local) > 
			tms_local->dmalimit)
	{
		printk(KERN_INFO "%s: Memory not accessible for DMA\n",
			dev->name);
		tmsdev_term(dev);
		return -ENOMEM;
	}
	
	dev->netdev_ops		= &tms380tr_netdev_ops;
	dev->watchdog_timeo	= HZ;

	return 0;
}

EXPORT_SYMBOL(tms380tr_open);
EXPORT_SYMBOL(tms380tr_close);
EXPORT_SYMBOL(tms380tr_interrupt);
EXPORT_SYMBOL(tmsdev_init);
EXPORT_SYMBOL(tmsdev_term);
EXPORT_SYMBOL(tms380tr_wait);

#ifdef MODULE

static struct module *TMS380_module = NULL;

int init_module(void)
{
	printk(KERN_DEBUG "%s", version);
	
	TMS380_module = &__this_module;
	return 0;
}

void cleanup_module(void)
{
	TMS380_module = NULL;
}
#endif

MODULE_LICENSE("GPL");

