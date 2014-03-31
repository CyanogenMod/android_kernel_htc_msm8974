#define RCS_ID "$Id: scc.c,v 1.75 1998/11/04 15:15:01 jreuter Exp jreuter $"

#define VERSION "3.0"


/*
   ********************************************************************
   *   SCC.C - Linux driver for Z8530 based HDLC cards for AX.25      *
   ********************************************************************


   ********************************************************************

	Copyright (c) 1993, 2000 Joerg Reuter DL1BKE

	portions (c) 1993 Guido ten Dolle PE1NNZ

   ********************************************************************
   
   The driver and the programs in the archive are UNDER CONSTRUCTION.
   The code is likely to fail, and so your kernel could --- even 
   a whole network. 

   This driver is intended for Amateur Radio use. If you are running it
   for commercial purposes, please drop me a note. I am nosy...

   ...BUT:
 
   ! You  m u s t  recognize the appropriate legislations of your country !
   ! before you connect a radio to the SCC board and start to transmit or !
   ! receive. The GPL allows you to use the  d r i v e r,  NOT the RADIO! !

   For non-Amateur-Radio use please note that you might need a special
   allowance/licence from the designer of the SCC Board and/or the
   MODEM. 

   This program is free software; you can redistribute it and/or modify 
   it under the terms of the (modified) GNU General Public License 
   delivered with the Linux kernel source.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should find a copy of the GNU General Public License in 
   /usr/src/linux/COPYING; 
   
   ******************************************************************** 

		
   Incomplete history of z8530drv:
   -------------------------------

   1994-09-13	started to write the driver, rescued most of my own
		code (and Hans Alblas' memory buffer pool concept) from 
		an earlier project "sccdrv" which was initiated by 
		Guido ten Dolle. Not much of the old driver survived, 
		though. The first version I put my hands on was sccdrv1.3
		from August 1993. The memory buffer pool concept
		appeared in an unauthorized sccdrv version (1.5) from
		August 1994.

   1995-01-31	changed copyright notice to GPL without limitations.
   
     .
     .	<SNIP>
     .
   		  
   1996-10-05	New semester, new driver... 

   		  * KISS TNC emulator removed (TTY driver)
   		  * Source moved to drivers/net/
   		  * Includes Z8530 defines from drivers/net/z8530.h
   		  * Uses sk_buffer memory management
   		  * Reduced overhead of /proc/net/z8530drv output
   		  * Streamlined quite a lot things
   		  * Invents brand new bugs... ;-)

   		  The move to version number 3.0 reflects theses changes.
   		  You can use 'kissbridge' if you need a KISS TNC emulator.

   1996-12-13	Fixed for Linux networking changes. (G4KLX)
   1997-01-08	Fixed the remaining problems.
   1997-04-02	Hopefully fixed the problems with the new *_timer()
   		routines, added calibration code.
   1997-10-12	Made SCC_DELAY a CONFIG option, added CONFIG_SCC_TRXECHO
   1998-01-29	Small fix to avoid lock-up on initialization
   1998-09-29	Fixed the "grouping" bugs, tx_inhibit works again,
   		using dev->tx_queue_len now instead of MAXQUEUE now.
   1998-10-21	Postponed the spinlock changes, would need a lot of
   		testing I currently don't have the time to. Softdcd doesn't
   		work.
   1998-11-04	Softdcd does not work correctly in DPLL mode, in fact it 
   		never did. The DPLL locks on noise, the SYNC unit sees
   		flags that aren't... Restarting the DPLL does not help
   		either, it resynchronizes too slow and the first received
   		frame gets lost.
   2000-02-13	Fixed for new network driver interface changes, still
   		does TX timeouts itself since it uses its own queue
   		scheme.

   Thanks to all who contributed to this driver with ideas and bug
   reports!
   
   NB -- if you find errors, change something, please let me know
      	 first before you distribute it... And please don't touch
   	 the version number. Just replace my callsign in
   	 "v3.0.dl1bke" with your own. Just to avoid confusion...

   If you want to add your modification to the linux distribution
   please (!) contact me first.
   
   New versions of the driver will be announced on the linux-hams
   mailing list on vger.kernel.org. To subscribe send an e-mail
   to majordomo@vger.kernel.org with the following line in
   the body of the mail:
   
	   subscribe linux-hams
	   
   The content of the "Subject" field will be ignored.

   vy 73,
   Joerg Reuter	ampr-net: dl1bke@db0pra.ampr.org
		AX-25   : DL1BKE @ DB0ABH.#BAY.DEU.EU
		Internet: jreuter@yaina.de
		www     : http://yaina.de/jreuter
*/


#undef  SCC_LDELAY		
#undef  SCC_DONT_CHECK		

#define SCC_MAXCHIPS	4       
#define SCC_BUFSIZE	384     
#undef	SCC_DEBUG

#define SCC_DEFAULT_CLOCK	4915200 
				


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/socket.h>
#include <linux/init.h>
#include <linux/scc.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/bitops.h>

#include <net/net_namespace.h>
#include <net/ax25.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "z8530.h"

static const char banner[] __initdata = KERN_INFO \
	"AX.25: Z8530 SCC driver version "VERSION".dl1bke\n";

static void t_dwait(unsigned long);
static void t_txdelay(unsigned long);
static void t_tail(unsigned long);
static void t_busy(unsigned long);
static void t_maxkeyup(unsigned long);
static void t_idle(unsigned long);
static void scc_tx_done(struct scc_channel *);
static void scc_start_tx_timer(struct scc_channel *, void (*)(unsigned long), unsigned long);
static void scc_start_maxkeyup(struct scc_channel *);
static void scc_start_defer(struct scc_channel *);

static void z8530_init(void);

static void init_channel(struct scc_channel *scc);
static void scc_key_trx (struct scc_channel *scc, char tx);
static void scc_init_timer(struct scc_channel *scc);

static int scc_net_alloc(const char *name, struct scc_channel *scc);
static void scc_net_setup(struct net_device *dev);
static int scc_net_open(struct net_device *dev);
static int scc_net_close(struct net_device *dev);
static void scc_net_rx(struct scc_channel *scc, struct sk_buff *skb);
static netdev_tx_t scc_net_tx(struct sk_buff *skb,
			      struct net_device *dev);
static int scc_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static int scc_net_set_mac_address(struct net_device *dev, void *addr);
static struct net_device_stats * scc_net_get_stats(struct net_device *dev);

static unsigned char SCC_DriverName[] = "scc";

static struct irqflags { unsigned char used : 1; } Ivec[NR_IRQS];
	
static struct scc_channel SCC_Info[2 * SCC_MAXCHIPS];	

static struct scc_ctrl {
	io_port chan_A;
	io_port chan_B;
	int irq;
} SCC_ctrl[SCC_MAXCHIPS+1];

static unsigned char Driver_Initialized;
static int Nchips;
static io_port Vector_Latch;




static DEFINE_SPINLOCK(iolock);	

static inline unsigned char InReg(io_port port, unsigned char reg)
{
	unsigned long flags;
	unsigned char r;

	spin_lock_irqsave(&iolock, flags);	
#ifdef SCC_LDELAY
	Outb(port, reg);
	udelay(SCC_LDELAY);
	r=Inb(port);
	udelay(SCC_LDELAY);
#else
	Outb(port, reg);
	r=Inb(port);
#endif
	spin_unlock_irqrestore(&iolock, flags);
	return r;
}

static inline void OutReg(io_port port, unsigned char reg, unsigned char val)
{
	unsigned long flags;

	spin_lock_irqsave(&iolock, flags);
#ifdef SCC_LDELAY
	Outb(port, reg); udelay(SCC_LDELAY);
	Outb(port, val); udelay(SCC_LDELAY);
#else
	Outb(port, reg);
	Outb(port, val);
#endif
	spin_unlock_irqrestore(&iolock, flags);
}

static inline void wr(struct scc_channel *scc, unsigned char reg,
	unsigned char val)
{
	OutReg(scc->ctrl, reg, (scc->wreg[reg] = val));
}

static inline void or(struct scc_channel *scc, unsigned char reg, unsigned char val)
{
	OutReg(scc->ctrl, reg, (scc->wreg[reg] |= val));
}

static inline void cl(struct scc_channel *scc, unsigned char reg, unsigned char val)
{
	OutReg(scc->ctrl, reg, (scc->wreg[reg] &= ~val));
}


static inline void scc_discard_buffers(struct scc_channel *scc)
{
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);	
	if (scc->tx_buff != NULL)
	{
		dev_kfree_skb(scc->tx_buff);
		scc->tx_buff = NULL;
	}
	
	while (!skb_queue_empty(&scc->tx_queue))
		dev_kfree_skb(skb_dequeue(&scc->tx_queue));

	spin_unlock_irqrestore(&scc->lock, flags);
}






static inline void scc_notify(struct scc_channel *scc, int event)
{
	struct sk_buff *skb;
	char *bp;
	
        if (scc->kiss.fulldup != KISS_DUPLEX_OPTIMA)
		return;

	skb = dev_alloc_skb(2);
	if (skb != NULL)
	{
		bp = skb_put(skb, 2);
		*bp++ = PARAM_HWEVENT;
		*bp++ = event;
		scc_net_rx(scc, skb);
	} else
		scc->stat.nospace++;
}

static inline void flush_rx_FIFO(struct scc_channel *scc)
{
	int k;
	
	for (k=0; k<3; k++)
		Inb(scc->data);
		
	if(scc->rx_buff != NULL)		
	{
		scc->stat.rxerrs++;  
		dev_kfree_skb_irq(scc->rx_buff);
		scc->rx_buff = NULL;
	}
}

static void start_hunt(struct scc_channel *scc)
{
	if ((scc->modem.clocksrc != CLK_EXTERNAL))
		OutReg(scc->ctrl,R14,SEARCH|scc->wreg[R14]); 
	or(scc,R3,ENT_HM|RxENABLE);  
}


static inline void scc_txint(struct scc_channel *scc)
{
	struct sk_buff *skb;

	scc->stat.txints++;
	skb = scc->tx_buff;
	
	
	
	if (skb == NULL)
	{
		skb = skb_dequeue(&scc->tx_queue);
		scc->tx_buff = skb;
		netif_wake_queue(scc->dev);

		if (skb == NULL)
		{
			scc_tx_done(scc);
			Outb(scc->ctrl, RES_Tx_P);
			return;
		}
		
		if (skb->len == 0)		
		{
			dev_kfree_skb_irq(skb);
			scc->tx_buff = NULL;
			scc_tx_done(scc);
			Outb(scc->ctrl, RES_Tx_P);
			return;
		}

		scc->stat.tx_state = TXS_ACTIVE;

		OutReg(scc->ctrl, R0, RES_Tx_CRC);
						
		or(scc,R10,ABUNDER);		
		Outb(scc->data,*skb->data);	
		skb_pull(skb, 1);

		if (!scc->enhanced)		
			Outb(scc->ctrl,RES_EOM_L);
		return;
	}
	
	
	
	if (skb->len == 0)
	{
		Outb(scc->ctrl, RES_Tx_P);	
		cl(scc, R10, ABUNDER);		
		dev_kfree_skb_irq(skb);
		scc->tx_buff = NULL;
		scc->stat.tx_state = TXS_NEWFRAME; 
		return;
	} 
	
	
	
	Outb(scc->data,*skb->data);		
	skb_pull(skb, 1);
}


static inline void scc_exint(struct scc_channel *scc)
{
	unsigned char status,changes,chg_and_stat;

	scc->stat.exints++;

	status = InReg(scc->ctrl,R0);
	changes = status ^ scc->status;
	chg_and_stat = changes & status;
	
	

	if (chg_and_stat & BRK_ABRT)		
		flush_rx_FIFO(scc);

	

	if ((changes & SYNC_HUNT) && scc->kiss.softdcd)
	{
		if (status & SYNC_HUNT)
		{
			scc->dcd = 0;
			flush_rx_FIFO(scc);
			if ((scc->modem.clocksrc != CLK_EXTERNAL))
				OutReg(scc->ctrl,R14,SEARCH|scc->wreg[R14]); 
		} else {
			scc->dcd = 1;
		}

		scc_notify(scc, scc->dcd? HWEV_DCD_OFF:HWEV_DCD_ON);
	}

	
	
	
	if((changes & DCD) && !scc->kiss.softdcd) 
	{
		if(status & DCD)                
		{
			start_hunt(scc);
			scc->dcd = 1;
		} else {                        
			cl(scc,R3,ENT_HM|RxENABLE); 
			flush_rx_FIFO(scc);
			scc->dcd = 0;
		}
		
		scc_notify(scc, scc->dcd? HWEV_DCD_ON:HWEV_DCD_OFF);
	}

#ifdef notdef
	 
	if (chg_and_stat & CTS)			
	{
		if (scc->kiss.txdelay == 0)	
			scc_start_tx_timer(scc, t_txdelay, 0);
	}
#endif
	
	if (scc->stat.tx_state == TXS_ACTIVE && (status & TxEOM))
	{
		scc->stat.tx_under++;	  
		Outb(scc->ctrl, RES_EXT_INT);	

		if (scc->tx_buff != NULL)
		{
			dev_kfree_skb_irq(scc->tx_buff);
			scc->tx_buff = NULL;
		}
		
		or(scc,R10,ABUNDER);
		scc_start_tx_timer(scc, t_txdelay, 0);	
	}
		
	scc->status = status;
	Outb(scc->ctrl,RES_EXT_INT);
}


static inline void scc_rxint(struct scc_channel *scc)
{
	struct sk_buff *skb;

	scc->stat.rxints++;

	if((scc->wreg[5] & RTS) && scc->kiss.fulldup == KISS_DUPLEX_HALF)
	{
		Inb(scc->data);		
		or(scc,R3,ENT_HM);	
		return;
	}

	skb = scc->rx_buff;
	
	if (skb == NULL)
	{
		skb = dev_alloc_skb(scc->stat.bufsize);
		if (skb == NULL)
		{
			scc->dev_stat.rx_dropped++;
			scc->stat.nospace++;
			Inb(scc->data);
			or(scc, R3, ENT_HM);
			return;
		}
		
		scc->rx_buff = skb;
		*(skb_put(skb, 1)) = 0;	
	}
	
	if (skb->len >= scc->stat.bufsize)
	{
#ifdef notdef
		printk(KERN_DEBUG "z8530drv: oops, scc_rxint() received huge frame...\n");
#endif
		dev_kfree_skb_irq(skb);
		scc->rx_buff = NULL;
		Inb(scc->data);
		or(scc, R3, ENT_HM);
		return;
	}

	*(skb_put(skb, 1)) = Inb(scc->data);
}


static inline void scc_spint(struct scc_channel *scc)
{
	unsigned char status;
	struct sk_buff *skb;

	scc->stat.spints++;

	status = InReg(scc->ctrl,R1);		
	
	Inb(scc->data);				
	skb = scc->rx_buff;

	if(status & Rx_OVR)			
	{
		scc->stat.rx_over++;             
		or(scc,R3,ENT_HM);               
		
		if (skb != NULL) 
			dev_kfree_skb_irq(skb);
		scc->rx_buff = skb = NULL;
	}

	if(status & END_FR && skb != NULL)	
	{
		
		
		if (!(status & CRC_ERR) && (status & 0xe) == RES8 && skb->len > 0)
		{
			
			skb_trim(skb, skb->len-1);
			scc_net_rx(scc, skb);
			scc->rx_buff = NULL;
			scc->stat.rxframes++;
		} else {				
			dev_kfree_skb_irq(skb);
			scc->rx_buff = NULL;
			scc->stat.rxerrs++;
		}
	} 

	Outb(scc->ctrl,ERR_RES);
}



static void scc_isr_dispatch(struct scc_channel *scc, int vector)
{
	spin_lock(&scc->lock);
	switch (vector & VECTOR_MASK)
	{
		case TXINT: scc_txint(scc); break;
		case EXINT: scc_exint(scc); break;
		case RXINT: scc_rxint(scc); break;
		case SPINT: scc_spint(scc); break;
	}
	spin_unlock(&scc->lock);
}


#define SCC_IRQTIMEOUT 30000

static irqreturn_t scc_isr(int irq, void *dev_id)
{
	int chip_irq = (long) dev_id;
	unsigned char vector;	
	struct scc_channel *scc;
	struct scc_ctrl *ctrl;
	int k;
	
	if (Vector_Latch)
	{
	    	for(k=0; k < SCC_IRQTIMEOUT; k++)
    		{
			Outb(Vector_Latch, 0);      
        
			
			if((vector=Inb(Vector_Latch)) >= 16 * Nchips) break; 
			if (vector & 0x01) break;
        	 
		        scc=&SCC_Info[vector >> 3 ^ 0x01];
			if (!scc->dev) break;

			scc_isr_dispatch(scc, vector);

			OutReg(scc->ctrl,R0,RES_H_IUS);              
		}  

		if (k == SCC_IRQTIMEOUT)
			printk(KERN_WARNING "z8530drv: endless loop in scc_isr()?\n");

		return IRQ_HANDLED;
	}


	ctrl = SCC_ctrl;
	while (ctrl->chan_A)
	{
		if (ctrl->irq != chip_irq)
		{
			ctrl++;
			continue;
		}

		scc = NULL;
		for (k = 0; InReg(ctrl->chan_A,R3) && k < SCC_IRQTIMEOUT; k++)
		{
			vector=InReg(ctrl->chan_B,R2);	
			if (vector & 0x01) break; 

			scc = &SCC_Info[vector >> 3 ^ 0x01];
		        if (!scc->dev) break;

			scc_isr_dispatch(scc, vector);
		}

		if (k == SCC_IRQTIMEOUT)
		{
			printk(KERN_WARNING "z8530drv: endless loop in scc_isr()?!\n");
			break;
		}


		if (scc != NULL)
		{
			OutReg(scc->ctrl,R0,RES_H_IUS);
			ctrl = SCC_ctrl; 
		} else
			ctrl++;
	}
	return IRQ_HANDLED;
}






static inline void set_brg(struct scc_channel *scc, unsigned int tc)
{
	cl(scc,R14,BRENABL);		
	wr(scc,R12,tc & 255);		
	wr(scc,R13,tc >> 8);   		
	or(scc,R14,BRENABL);		
}

static inline void set_speed(struct scc_channel *scc)
{
	unsigned long flags;
	spin_lock_irqsave(&scc->lock, flags);

	if (scc->modem.speed > 0)	
		set_brg(scc, (unsigned) (scc->clock / (scc->modem.speed * 64)) - 2);
		
	spin_unlock_irqrestore(&scc->lock, flags);
}



static inline void init_brg(struct scc_channel *scc)
{
	wr(scc, R14, BRSRC);				
	OutReg(scc->ctrl, R14, SSBR|scc->wreg[R14]);	
	OutReg(scc->ctrl, R14, SNRZI|scc->wreg[R14]);	
}

 
static void init_channel(struct scc_channel *scc)
{
	del_timer(&scc->tx_t);
	del_timer(&scc->tx_wdog);

	disable_irq(scc->irq);

	wr(scc,R4,X1CLK|SDLC);		
	wr(scc,R1,0);			
	wr(scc,R3,Rx8|RxCRC_ENAB);		
	wr(scc,R5,Tx8|DTR|TxCRC_ENAB);	
	wr(scc,R6,0);			
	wr(scc,R7,FLAG);		
	wr(scc,R9,VIS);			
	wr(scc,R10,(scc->modem.nrz? NRZ : NRZI)|CRCPS|ABUNDER); 
	wr(scc,R14, 0);


  

   		
	switch(scc->modem.clocksrc)
	{
		case CLK_DPLL:
			wr(scc, R11, RCDPLL|TCDPLL|TRxCOI|TRxCDP);
			init_brg(scc);
			break;

		case CLK_DIVIDER:
			wr(scc, R11, ((scc->brand & BAYCOM)? TRxCDP : TRxCBR) | RCDPLL|TCRTxCP|TRxCOI);
			init_brg(scc);
			break;

		case CLK_EXTERNAL:
			wr(scc, R11, (scc->brand & BAYCOM)? RCTRxCP|TCRTxCP : RCRTxCP|TCTRxCP);
			OutReg(scc->ctrl, R14, DISDPLL);
			break;

	}
	
	set_speed(scc);			
	
	if(scc->enhanced)
	{
		or(scc,R15,SHDLCE|FIFOE);	
		wr(scc,R7,AUTOEOM);
	}

	if(scc->kiss.softdcd || (InReg(scc->ctrl,R0) & DCD))
						
	{
		start_hunt(scc);
	}
	
	

	wr(scc,R15, BRKIE|TxUIE|(scc->kiss.softdcd? SYNCIE:DCDIE));

	Outb(scc->ctrl,RES_EXT_INT);	
	Outb(scc->ctrl,RES_EXT_INT);	

	or(scc,R1,INT_ALL_Rx|TxINT_ENAB|EXT_INT_ENAB); 
	
	scc->status = InReg(scc->ctrl,R0);	
	
	or(scc,R9,MIE);			
	
	scc_init_timer(scc);
			
	enable_irq(scc->irq);
}







static void scc_key_trx(struct scc_channel *scc, char tx)
{
	unsigned int time_const;
		
	if (scc->brand & PRIMUS)
		Outb(scc->ctrl + 4, scc->option | (tx? 0x80 : 0));

	if (scc->modem.speed < 300) 
		scc->modem.speed = 1200;

	time_const = (unsigned) (scc->clock / (scc->modem.speed * (tx? 2:64))) - 2;

	disable_irq(scc->irq);

	if (tx)
	{
		or(scc, R1, TxINT_ENAB);	
		or(scc, R15, TxUIE);
	}

	if (scc->modem.clocksrc == CLK_DPLL)
	{				
		if (tx)
		{
#ifdef CONFIG_SCC_TRXECHO
			cl(scc, R3, RxENABLE|ENT_HM);	
			cl(scc, R15, DCDIE|SYNCIE);	
#endif
			set_brg(scc, time_const);	

			
			wr(scc, R11, RCDPLL|TCBR|TRxCOI|TRxCBR);
			
			
			if (scc->kiss.tx_inhibit)
			{
				or(scc,R5, TxENAB);
				scc->wreg[R5] |= RTS;
			} else {
				or(scc,R5,RTS|TxENAB);	
			}
		} else {
			cl(scc,R5,RTS|TxENAB);
			
			set_brg(scc, time_const);	
			
			
			wr(scc, R11, RCDPLL|TCDPLL|TRxCOI|TRxCDP);

#ifndef CONFIG_SCC_TRXECHO
			if (scc->kiss.softdcd)
#endif
			{
				or(scc,R15, scc->kiss.softdcd? SYNCIE:DCDIE);
				start_hunt(scc);
			}
		}
	} else {
		if (tx)
		{
#ifdef CONFIG_SCC_TRXECHO
			if (scc->kiss.fulldup == KISS_DUPLEX_HALF)
			{
				cl(scc, R3, RxENABLE);
				cl(scc, R15, DCDIE|SYNCIE);
			}
#endif
				
			if (scc->kiss.tx_inhibit)
			{
				or(scc,R5, TxENAB);
				scc->wreg[R5] |= RTS;
			} else {	
				or(scc,R5,RTS|TxENAB);	
			}
		} else {
			cl(scc,R5,RTS|TxENAB);		

			if ((scc->kiss.fulldup == KISS_DUPLEX_HALF) &&
#ifndef CONFIG_SCC_TRXECHO
			    scc->kiss.softdcd)
#else
			    1)
#endif
			{
				or(scc, R15, scc->kiss.softdcd? SYNCIE:DCDIE);
				start_hunt(scc);
			}
		}
	}

	enable_irq(scc->irq);
}



static void __scc_start_tx_timer(struct scc_channel *scc, void (*handler)(unsigned long), unsigned long when)
{
	del_timer(&scc->tx_t);

	if (when == 0)
	{
		handler((unsigned long) scc);
	} else 
	if (when != TIMER_OFF)
	{
		scc->tx_t.data = (unsigned long) scc;
		scc->tx_t.function = handler;
		scc->tx_t.expires = jiffies + (when*HZ)/100;
		add_timer(&scc->tx_t);
	}
}

static void scc_start_tx_timer(struct scc_channel *scc, void (*handler)(unsigned long), unsigned long when)
{
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);
	__scc_start_tx_timer(scc, handler, when);
	spin_unlock_irqrestore(&scc->lock, flags);
}

static void scc_start_defer(struct scc_channel *scc)
{
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);
	del_timer(&scc->tx_wdog);
	
	if (scc->kiss.maxdefer != 0 && scc->kiss.maxdefer != TIMER_OFF)
	{
		scc->tx_wdog.data = (unsigned long) scc;
		scc->tx_wdog.function = t_busy;
		scc->tx_wdog.expires = jiffies + HZ*scc->kiss.maxdefer;
		add_timer(&scc->tx_wdog);
	}
	spin_unlock_irqrestore(&scc->lock, flags);
}

static void scc_start_maxkeyup(struct scc_channel *scc)
{
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);
	del_timer(&scc->tx_wdog);
	
	if (scc->kiss.maxkeyup != 0 && scc->kiss.maxkeyup != TIMER_OFF)
	{
		scc->tx_wdog.data = (unsigned long) scc;
		scc->tx_wdog.function = t_maxkeyup;
		scc->tx_wdog.expires = jiffies + HZ*scc->kiss.maxkeyup;
		add_timer(&scc->tx_wdog);
	}
	spin_unlock_irqrestore(&scc->lock, flags);
}


static void scc_tx_done(struct scc_channel *scc)
{
				 
	switch (scc->kiss.fulldup)
	{
		case KISS_DUPLEX_LINK:
			scc->stat.tx_state = TXS_IDLE2;
			if (scc->kiss.idletime != TIMER_OFF)
				scc_start_tx_timer(scc, t_idle,
						   scc->kiss.idletime*100);
			break;
		case KISS_DUPLEX_OPTIMA:
			scc_notify(scc, HWEV_ALL_SENT);
			break;
		default:
			scc->stat.tx_state = TXS_BUSY;
			scc_start_tx_timer(scc, t_tail, scc->kiss.tailtime);
	}

	netif_wake_queue(scc->dev);
}


static unsigned char Rand = 17;

static inline int is_grouped(struct scc_channel *scc)
{
	int k;
	struct scc_channel *scc2;
	unsigned char grp1, grp2;

	grp1 = scc->kiss.group;
	
	for (k = 0; k < (Nchips * 2); k++)
	{
		scc2 = &SCC_Info[k];
		grp2 = scc2->kiss.group;
		
		if (scc2 == scc || !(scc2->dev && grp2))
			continue;
		
		if ((grp1 & 0x3f) == (grp2 & 0x3f))
		{
			if ( (grp1 & TXGROUP) && (scc2->wreg[R5] & RTS) )
				return 1;
			
			if ( (grp1 & RXGROUP) && scc2->dcd )
				return 1;
		}
	}
	return 0;
}


static void t_dwait(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;
	
	if (scc->stat.tx_state == TXS_WAIT)	
	{
		if (skb_queue_empty(&scc->tx_queue)) {	
			scc->stat.tx_state = TXS_IDLE;
			netif_wake_queue(scc->dev);	
			return;
		}

		scc->stat.tx_state = TXS_BUSY;
	}

	if (scc->kiss.fulldup == KISS_DUPLEX_HALF)
	{
		Rand = Rand * 17 + 31;
		
		if (scc->dcd || (scc->kiss.persist) < Rand || (scc->kiss.group && is_grouped(scc)) )
		{
			scc_start_defer(scc);
			scc_start_tx_timer(scc, t_dwait, scc->kiss.slottime);
			return ;
		}
	}

	if ( !(scc->wreg[R5] & RTS) )
	{
		scc_key_trx(scc, TX_ON);
		scc_start_tx_timer(scc, t_txdelay, scc->kiss.txdelay);
	} else {
		scc_start_tx_timer(scc, t_txdelay, 0);
	}
}



static void t_txdelay(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;

	scc_start_maxkeyup(scc);

	if (scc->tx_buff == NULL)
	{
		disable_irq(scc->irq);
		scc_txint(scc);	
		enable_irq(scc->irq);
	}
}
	


static void t_tail(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags); 
 	del_timer(&scc->tx_wdog);	
 	scc_key_trx(scc, TX_OFF);
	spin_unlock_irqrestore(&scc->lock, flags);

 	if (scc->stat.tx_state == TXS_TIMEOUT)		
 	{
 		scc->stat.tx_state = TXS_WAIT;
		scc_start_tx_timer(scc, t_dwait, scc->kiss.mintime*100);
 		return;
 	}
 	
 	scc->stat.tx_state = TXS_IDLE;
	netif_wake_queue(scc->dev);
}



static void t_busy(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;

	del_timer(&scc->tx_t);
	netif_stop_queue(scc->dev);	

	scc_discard_buffers(scc);
	scc->stat.txerrs++;
	scc->stat.tx_state = TXS_IDLE;

	netif_wake_queue(scc->dev);	
}


static void t_maxkeyup(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;
	unsigned long flags;

	spin_lock_irqsave(&scc->lock, flags);

	netif_stop_queue(scc->dev);
	scc_discard_buffers(scc);

	del_timer(&scc->tx_t);

	cl(scc, R1, TxINT_ENAB);	
	cl(scc, R15, TxUIE);		
	OutReg(scc->ctrl, R0, RES_Tx_P);

	spin_unlock_irqrestore(&scc->lock, flags);

	scc->stat.txerrs++;
	scc->stat.tx_state = TXS_TIMEOUT;
	scc_start_tx_timer(scc, t_tail, scc->kiss.tailtime);
}


static void t_idle(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;
	
	del_timer(&scc->tx_wdog);

	scc_key_trx(scc, TX_OFF);
	if(scc->kiss.mintime)
		scc_start_tx_timer(scc, t_dwait, scc->kiss.mintime*100);
	scc->stat.tx_state = TXS_WAIT;
}

static void scc_init_timer(struct scc_channel *scc)
{
	unsigned long flags;

	spin_lock_irqsave(&scc->lock, flags);	
	scc->stat.tx_state = TXS_IDLE;
	spin_unlock_irqrestore(&scc->lock, flags);
}





#define CAST(x) (unsigned long)(x)

static unsigned int scc_set_param(struct scc_channel *scc, unsigned int cmd, unsigned int arg)
{
	switch (cmd)
	{
		case PARAM_TXDELAY:	scc->kiss.txdelay=arg;		break;
		case PARAM_PERSIST:	scc->kiss.persist=arg;		break;
		case PARAM_SLOTTIME:	scc->kiss.slottime=arg;		break;
		case PARAM_TXTAIL:	scc->kiss.tailtime=arg;		break;
		case PARAM_FULLDUP:	scc->kiss.fulldup=arg;		break;
		case PARAM_DTR:		break; 
		case PARAM_GROUP:	scc->kiss.group=arg;		break;
		case PARAM_IDLE:	scc->kiss.idletime=arg;		break;
		case PARAM_MIN:		scc->kiss.mintime=arg;		break;
		case PARAM_MAXKEY:	scc->kiss.maxkeyup=arg;		break;
		case PARAM_WAIT:	scc->kiss.waittime=arg;		break;
		case PARAM_MAXDEFER:	scc->kiss.maxdefer=arg;		break;
		case PARAM_TX:		scc->kiss.tx_inhibit=arg;	break;

		case PARAM_SOFTDCD:	
			scc->kiss.softdcd=arg;
			if (arg)
			{
				or(scc, R15, SYNCIE);
				cl(scc, R15, DCDIE);
				start_hunt(scc);
			} else {
				or(scc, R15, DCDIE);
				cl(scc, R15, SYNCIE);
			}
			break;
				
		case PARAM_SPEED:
			if (arg < 256)
				scc->modem.speed=arg*100;
			else
				scc->modem.speed=arg;

			if (scc->stat.tx_state == 0)	
				set_speed(scc);
			break;
			
		case PARAM_RTS:	
			if ( !(scc->wreg[R5] & RTS) )
			{
				if (arg != TX_OFF) {
					scc_key_trx(scc, TX_ON);
					scc_start_tx_timer(scc, t_txdelay, scc->kiss.txdelay);
				}
			} else {
				if (arg == TX_OFF)
				{
					scc->stat.tx_state = TXS_BUSY;
					scc_start_tx_timer(scc, t_tail, scc->kiss.tailtime);
				}
			}
			break;
			
		case PARAM_HWEVENT:
			scc_notify(scc, scc->dcd? HWEV_DCD_ON:HWEV_DCD_OFF);
			break;

		default:		return -EINVAL;
	}
	
	return 0;
}


 
static unsigned long scc_get_param(struct scc_channel *scc, unsigned int cmd)
{
	switch (cmd)
	{
		case PARAM_TXDELAY:	return CAST(scc->kiss.txdelay);
		case PARAM_PERSIST:	return CAST(scc->kiss.persist);
		case PARAM_SLOTTIME:	return CAST(scc->kiss.slottime);
		case PARAM_TXTAIL:	return CAST(scc->kiss.tailtime);
		case PARAM_FULLDUP:	return CAST(scc->kiss.fulldup);
		case PARAM_SOFTDCD:	return CAST(scc->kiss.softdcd);
		case PARAM_DTR:		return CAST((scc->wreg[R5] & DTR)? 1:0);
		case PARAM_RTS:		return CAST((scc->wreg[R5] & RTS)? 1:0);
		case PARAM_SPEED:	return CAST(scc->modem.speed);
		case PARAM_GROUP:	return CAST(scc->kiss.group);
		case PARAM_IDLE:	return CAST(scc->kiss.idletime);
		case PARAM_MIN:		return CAST(scc->kiss.mintime);
		case PARAM_MAXKEY:	return CAST(scc->kiss.maxkeyup);
		case PARAM_WAIT:	return CAST(scc->kiss.waittime);
		case PARAM_MAXDEFER:	return CAST(scc->kiss.maxdefer);
		case PARAM_TX:		return CAST(scc->kiss.tx_inhibit);
		default:		return NO_SUCH_PARAM;
	}

}

#undef CAST


static void scc_stop_calibrate(unsigned long channel)
{
	struct scc_channel *scc = (struct scc_channel *) channel;
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);
	del_timer(&scc->tx_wdog);
	scc_key_trx(scc, TX_OFF);
	wr(scc, R6, 0);
	wr(scc, R7, FLAG);
	Outb(scc->ctrl,RES_EXT_INT);	
	Outb(scc->ctrl,RES_EXT_INT);

	netif_wake_queue(scc->dev);
	spin_unlock_irqrestore(&scc->lock, flags);
}


static void
scc_start_calibrate(struct scc_channel *scc, int duration, unsigned char pattern)
{
	unsigned long flags;
	
	spin_lock_irqsave(&scc->lock, flags);
	netif_stop_queue(scc->dev);
	scc_discard_buffers(scc);

	del_timer(&scc->tx_wdog);

	scc->tx_wdog.data = (unsigned long) scc;
	scc->tx_wdog.function = scc_stop_calibrate;
	scc->tx_wdog.expires = jiffies + HZ*duration;
	add_timer(&scc->tx_wdog);

		
	wr(scc, R6, 0);
	wr(scc, R7, pattern);


	Outb(scc->ctrl,RES_EXT_INT);	
	Outb(scc->ctrl,RES_EXT_INT);

	scc_key_trx(scc, TX_ON);
	spin_unlock_irqrestore(&scc->lock, flags);
}



static void z8530_init(void)
{
	struct scc_channel *scc;
	int chip, k;
	unsigned long flags;
	char *flag;


	printk(KERN_INFO "Init Z8530 driver: %u channels, IRQ", Nchips*2);
	
	flag=" ";
	for (k = 0; k < nr_irqs; k++)
		if (Ivec[k].used) 
		{
			printk("%s%d", flag, k);
			flag=",";
		}
	printk("\n");


	
	for (chip = 0; chip < Nchips; chip++)
	{
		scc=&SCC_Info[2*chip];
		if (!scc->ctrl) continue;

		

		if(scc->brand & EAGLE)			
			Outb(scc->special,0x08);	
			
		if(scc->brand & (PC100 | PRIMUS))	
			Outb(scc->special,scc->option);	

			
		

		spin_lock_irqsave(&scc->lock, flags);
				
		Outb(scc->ctrl, 0);
		OutReg(scc->ctrl,R9,FHWRES);		
		udelay(100);				
		wr(scc, R2, chip*16);			
		wr(scc, R9, VIS);			
		spin_unlock_irqrestore(&scc->lock, flags);		
        }

 
	Driver_Initialized = 1;
}


static int scc_net_alloc(const char *name, struct scc_channel *scc)
{
	int err;
	struct net_device *dev;

	dev = alloc_netdev(0, name, scc_net_setup);
	if (!dev) 
		return -ENOMEM;

	dev->ml_priv = scc;
	scc->dev = dev;
	spin_lock_init(&scc->lock);
	init_timer(&scc->tx_t);
	init_timer(&scc->tx_wdog);

	err = register_netdevice(dev);
	if (err) {
		printk(KERN_ERR "%s: can't register network device (%d)\n", 
		       name, err);
		free_netdev(dev);
		scc->dev = NULL;
		return err;
	}

	return 0;
}




static const struct net_device_ops scc_netdev_ops = {
	.ndo_open            = scc_net_open,
	.ndo_stop	     = scc_net_close,
	.ndo_start_xmit	     = scc_net_tx,
	.ndo_set_mac_address = scc_net_set_mac_address,
	.ndo_get_stats       = scc_net_get_stats,
	.ndo_do_ioctl        = scc_net_ioctl,
};


static void scc_net_setup(struct net_device *dev)
{
	dev->tx_queue_len    = 16;	

	dev->netdev_ops	     = &scc_netdev_ops;
	dev->header_ops      = &ax25_header_ops;

	memcpy(dev->broadcast, &ax25_bcast,  AX25_ADDR_LEN);
	memcpy(dev->dev_addr,  &ax25_defaddr, AX25_ADDR_LEN);
 
	dev->flags      = 0;

	dev->type = ARPHRD_AX25;
	dev->hard_header_len = AX25_MAX_HEADER_LEN + AX25_BPQ_HEADER_LEN;
	dev->mtu = AX25_DEF_PACLEN;
	dev->addr_len = AX25_ADDR_LEN;

}


static int scc_net_open(struct net_device *dev)
{
	struct scc_channel *scc = (struct scc_channel *) dev->ml_priv;

 	if (!scc->init)
		return -EINVAL;

	scc->tx_buff = NULL;
	skb_queue_head_init(&scc->tx_queue);
 
	init_channel(scc);

	netif_start_queue(dev);
	return 0;
}


static int scc_net_close(struct net_device *dev)
{
	struct scc_channel *scc = (struct scc_channel *) dev->ml_priv;
	unsigned long flags;

	netif_stop_queue(dev);

	spin_lock_irqsave(&scc->lock, flags);	
	Outb(scc->ctrl,0);		/* Make sure pointer is written */
	wr(scc,R1,0);			
	wr(scc,R3,0);
	spin_unlock_irqrestore(&scc->lock, flags);

	del_timer_sync(&scc->tx_t);
	del_timer_sync(&scc->tx_wdog);
	
	scc_discard_buffers(scc);

	return 0;
}


static void scc_net_rx(struct scc_channel *scc, struct sk_buff *skb)
{
	if (skb->len == 0) {
		dev_kfree_skb_irq(skb);
		return;
	}
		
	scc->dev_stat.rx_packets++;
	scc->dev_stat.rx_bytes += skb->len;

	skb->protocol = ax25_type_trans(skb, scc->dev);
	
	netif_rx(skb);
}


static netdev_tx_t scc_net_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct scc_channel *scc = (struct scc_channel *) dev->ml_priv;
	unsigned long flags;
	char kisscmd;

	if (skb->len > scc->stat.bufsize || skb->len < 2) {
		scc->dev_stat.tx_dropped++;	
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	
	scc->dev_stat.tx_packets++;
	scc->dev_stat.tx_bytes += skb->len;
	scc->stat.txframes++;
	
	kisscmd = *skb->data & 0x1f;
	skb_pull(skb, 1);

	if (kisscmd) {
		scc_set_param(scc, kisscmd, *skb->data);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&scc->lock, flags);
		
	if (skb_queue_len(&scc->tx_queue) > scc->dev->tx_queue_len) {
		struct sk_buff *skb_del;
		skb_del = skb_dequeue(&scc->tx_queue);
		dev_kfree_skb(skb_del);
	}
	skb_queue_tail(&scc->tx_queue, skb);
	dev->trans_start = jiffies;
	


	if(scc->stat.tx_state == TXS_IDLE || scc->stat.tx_state == TXS_IDLE2) {
		scc->stat.tx_state = TXS_BUSY;
		if (scc->kiss.fulldup == KISS_DUPLEX_HALF)
			__scc_start_tx_timer(scc, t_dwait, scc->kiss.waittime);
		else
			__scc_start_tx_timer(scc, t_dwait, 0);
	}
	spin_unlock_irqrestore(&scc->lock, flags);
	return NETDEV_TX_OK;
}



static int scc_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct scc_kiss_cmd kiss_cmd;
	struct scc_mem_config memcfg;
	struct scc_hw_config hwcfg;
	struct scc_calibrate cal;
	struct scc_channel *scc = (struct scc_channel *) dev->ml_priv;
	int chan;
	unsigned char device_name[IFNAMSIZ];
	void __user *arg = ifr->ifr_data;
	
	
	if (!Driver_Initialized)
	{
		if (cmd == SIOCSCCCFG)
		{
			int found = 1;

			if (!capable(CAP_SYS_RAWIO)) return -EPERM;
			if (!arg) return -EFAULT;

			if (Nchips >= SCC_MAXCHIPS) 
				return -EINVAL;

			if (copy_from_user(&hwcfg, arg, sizeof(hwcfg)))
				return -EFAULT;

			if (hwcfg.irq == 2) hwcfg.irq = 9;

			if (hwcfg.irq < 0 || hwcfg.irq >= nr_irqs)
				return -EINVAL;
				
			if (!Ivec[hwcfg.irq].used && hwcfg.irq)
			{
				if (request_irq(hwcfg.irq, scc_isr,
						IRQF_DISABLED, "AX.25 SCC",
						(void *)(long) hwcfg.irq))
					printk(KERN_WARNING "z8530drv: warning, cannot get IRQ %d\n", hwcfg.irq);
				else
					Ivec[hwcfg.irq].used = 1;
			}

			if (hwcfg.vector_latch && !Vector_Latch) {
				if (!request_region(hwcfg.vector_latch, 1, "scc vector latch"))
					printk(KERN_WARNING "z8530drv: warning, cannot reserve vector latch port 0x%lx\n, disabled.", hwcfg.vector_latch);
				else
					Vector_Latch = hwcfg.vector_latch;
			}

			if (hwcfg.clock == 0)
				hwcfg.clock = SCC_DEFAULT_CLOCK;

#ifndef SCC_DONT_CHECK

			if(request_region(hwcfg.ctrl_a, 1, "scc-probe"))
			{
				disable_irq(hwcfg.irq);
				Outb(hwcfg.ctrl_a, 0);
				OutReg(hwcfg.ctrl_a, R9, FHWRES);
				udelay(100);
				OutReg(hwcfg.ctrl_a,R13,0x55);		
				udelay(5);

				if (InReg(hwcfg.ctrl_a,R13) != 0x55)
					found = 0;
				enable_irq(hwcfg.irq);
				release_region(hwcfg.ctrl_a, 1);
			}
			else
				found = 0;
#endif

			if (found)
			{
				SCC_Info[2*Nchips  ].ctrl = hwcfg.ctrl_a;
				SCC_Info[2*Nchips  ].data = hwcfg.data_a;
				SCC_Info[2*Nchips  ].irq  = hwcfg.irq;
				SCC_Info[2*Nchips+1].ctrl = hwcfg.ctrl_b;
				SCC_Info[2*Nchips+1].data = hwcfg.data_b;
				SCC_Info[2*Nchips+1].irq  = hwcfg.irq;
			
				SCC_ctrl[Nchips].chan_A = hwcfg.ctrl_a;
				SCC_ctrl[Nchips].chan_B = hwcfg.ctrl_b;
				SCC_ctrl[Nchips].irq    = hwcfg.irq;
			}


			for (chan = 0; chan < 2; chan++)
			{
				sprintf(device_name, "%s%i", SCC_DriverName, 2*Nchips+chan);

				SCC_Info[2*Nchips+chan].special = hwcfg.special;
				SCC_Info[2*Nchips+chan].clock = hwcfg.clock;
				SCC_Info[2*Nchips+chan].brand = hwcfg.brand;
				SCC_Info[2*Nchips+chan].option = hwcfg.option;
				SCC_Info[2*Nchips+chan].enhanced = hwcfg.escc;

#ifdef SCC_DONT_CHECK
				printk(KERN_INFO "%s: data port = 0x%3.3x  control port = 0x%3.3x\n",
					device_name, 
					SCC_Info[2*Nchips+chan].data, 
					SCC_Info[2*Nchips+chan].ctrl);

#else
				printk(KERN_INFO "%s: data port = 0x%3.3lx  control port = 0x%3.3lx -- %s\n",
					device_name,
					chan? hwcfg.data_b : hwcfg.data_a, 
					chan? hwcfg.ctrl_b : hwcfg.ctrl_a,
					found? "found" : "missing");
#endif

				if (found)
				{
					request_region(SCC_Info[2*Nchips+chan].ctrl, 1, "scc ctrl");
					request_region(SCC_Info[2*Nchips+chan].data, 1, "scc data");
					if (Nchips+chan != 0 &&
					    scc_net_alloc(device_name, 
							  &SCC_Info[2*Nchips+chan]))
					    return -EINVAL;
				}
			}
			
			if (found) Nchips++;
			
			return 0;
		}
		
		if (cmd == SIOCSCCINI)
		{
			if (!capable(CAP_SYS_RAWIO))
				return -EPERM;
				
			if (Nchips == 0)
				return -EINVAL;

			z8530_init();
			return 0;
		}
		
		return -EINVAL;	
	}
	
	if (!scc->init)
	{
		if (cmd == SIOCSCCCHANINI)
		{
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			if (!arg) return -EINVAL;
			
			scc->stat.bufsize   = SCC_BUFSIZE;

			if (copy_from_user(&scc->modem, arg, sizeof(struct scc_modem)))
				return -EINVAL;
			
			
		
			if (scc->modem.speed < 4800)
			{
				scc->kiss.txdelay = 36;		
				scc->kiss.persist = 42;					
				scc->kiss.slottime = 16;	
				scc->kiss.tailtime = 4;		
				scc->kiss.fulldup = 0;		
				scc->kiss.waittime = 50;	
				scc->kiss.maxkeyup = 10;	
				scc->kiss.mintime = 3;		
				scc->kiss.idletime = 30;	
				scc->kiss.maxdefer = 120;	
				scc->kiss.softdcd = 0;		
			} else {
				scc->kiss.txdelay = 10;		
				scc->kiss.persist = 64;					
				scc->kiss.slottime = 8;		
				scc->kiss.tailtime = 1;		
				scc->kiss.fulldup = 0;		
				scc->kiss.waittime = 50;	
				scc->kiss.maxkeyup = 7;		
				scc->kiss.mintime = 3;		
				scc->kiss.idletime = 30;	
				scc->kiss.maxdefer = 120;	
				scc->kiss.softdcd = 0;		
			}
			
			scc->tx_buff = NULL;
			skb_queue_head_init(&scc->tx_queue);
			scc->init = 1;
			
			return 0;
		}
		
		return -EINVAL;
	}
	
	switch(cmd)
	{
		case SIOCSCCRESERVED:
			return -ENOIOCTLCMD;

		case SIOCSCCSMEM:
			if (!capable(CAP_SYS_RAWIO)) return -EPERM;
			if (!arg || copy_from_user(&memcfg, arg, sizeof(memcfg)))
				return -EINVAL;
			scc->stat.bufsize   = memcfg.bufsize;
			return 0;
		
		case SIOCSCCGSTAT:
			if (!arg || copy_to_user(arg, &scc->stat, sizeof(scc->stat)))
				return -EINVAL;
			return 0;
		
		case SIOCSCCGKISS:
			if (!arg || copy_from_user(&kiss_cmd, arg, sizeof(kiss_cmd)))
				return -EINVAL;
			kiss_cmd.param = scc_get_param(scc, kiss_cmd.command);
			if (copy_to_user(arg, &kiss_cmd, sizeof(kiss_cmd)))
				return -EINVAL;
			return 0;
		
		case SIOCSCCSKISS:
			if (!capable(CAP_NET_ADMIN)) return -EPERM;
			if (!arg || copy_from_user(&kiss_cmd, arg, sizeof(kiss_cmd)))
				return -EINVAL;
			return scc_set_param(scc, kiss_cmd.command, kiss_cmd.param);
		
		case SIOCSCCCAL:
			if (!capable(CAP_SYS_RAWIO)) return -EPERM;
			if (!arg || copy_from_user(&cal, arg, sizeof(cal)) || cal.time == 0)
				return -EINVAL;

			scc_start_calibrate(scc, cal.time, cal.pattern);
			return 0;

		default:
			return -ENOIOCTLCMD;
		
	}
	
	return -EINVAL;
}


static int scc_net_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = (struct sockaddr *) addr;
	memcpy(dev->dev_addr, sa->sa_data, dev->addr_len);
	return 0;
}


static struct net_device_stats *scc_net_get_stats(struct net_device *dev)
{
	struct scc_channel *scc = (struct scc_channel *) dev->ml_priv;
	
	scc->dev_stat.rx_errors = scc->stat.rxerrs + scc->stat.rx_over;
	scc->dev_stat.tx_errors = scc->stat.txerrs + scc->stat.tx_under;
	scc->dev_stat.rx_fifo_errors = scc->stat.rx_over;
	scc->dev_stat.tx_fifo_errors = scc->stat.tx_under;

	return &scc->dev_stat;
}


#ifdef CONFIG_PROC_FS

static inline struct scc_channel *scc_net_seq_idx(loff_t pos)
{
	int k;

	for (k = 0; k < Nchips*2; ++k) {
		if (!SCC_Info[k].init) 
			continue;
		if (pos-- == 0)
			return &SCC_Info[k];
	}
	return NULL;
}

static void *scc_net_seq_start(struct seq_file *seq, loff_t *pos)
{
	return *pos ? scc_net_seq_idx(*pos - 1) : SEQ_START_TOKEN;
	
}

static void *scc_net_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned k;
	struct scc_channel *scc = v;
	++*pos;
	
	for (k = (v == SEQ_START_TOKEN) ? 0 : (scc - SCC_Info)+1;
	     k < Nchips*2; ++k) {
		if (SCC_Info[k].init) 
			return &SCC_Info[k];
	}
	return NULL;
}

static void scc_net_seq_stop(struct seq_file *seq, void *v)
{
}

static int scc_net_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "z8530drv-"VERSION"\n");
	} else if (!Driver_Initialized) {
		seq_puts(seq, "not initialized\n");
	} else if (!Nchips) {
		seq_puts(seq, "chips missing\n");
	} else {
		const struct scc_channel *scc = v;
		const struct scc_stat *stat = &scc->stat;
		const struct scc_kiss *kiss = &scc->kiss;



		seq_printf(seq, "%s\t%3.3lx %3.3lx %d %lu %2.2x %d %3.3lx %3.3lx %d\n",
				scc->dev->name,
				scc->data, scc->ctrl, scc->irq, scc->clock, scc->brand,
				scc->enhanced, Vector_Latch, scc->special,
				scc->option);
		seq_printf(seq, "\t%lu %d %d %d %d\n",
				scc->modem.speed, scc->modem.nrz,
				scc->modem.clocksrc, kiss->softdcd,
				stat->bufsize);
		seq_printf(seq, "\t%lu %lu %lu %lu\n",
				stat->rxints, stat->txints, stat->exints, stat->spints);
		seq_printf(seq, "\t%lu %lu %d / %lu %lu %d / %d %d\n",
				stat->rxframes, stat->rxerrs, stat->rx_over,
				stat->txframes, stat->txerrs, stat->tx_under,
				stat->nospace,  stat->tx_state);

#define K(x) kiss->x
		seq_printf(seq, "\t%d %d %d %d %d %d %d %d %d %d %d %d\n",
				K(txdelay), K(persist), K(slottime), K(tailtime),
				K(fulldup), K(waittime), K(mintime), K(maxkeyup),
				K(idletime), K(maxdefer), K(tx_inhibit), K(group));
#undef K
#ifdef SCC_DEBUG
		{
			int reg;

		seq_printf(seq, "\tW ");
			for (reg = 0; reg < 16; reg++)
				seq_printf(seq, "%2.2x ", scc->wreg[reg]);
			seq_printf(seq, "\n");
			
		seq_printf(seq, "\tR %2.2x %2.2x XX ", InReg(scc->ctrl,R0), InReg(scc->ctrl,R1));
			for (reg = 3; reg < 8; reg++)
				seq_printf(seq, "%2.2x ", InReg(scc->ctrl, reg));
			seq_printf(seq, "XX ");
			for (reg = 9; reg < 16; reg++)
				seq_printf(seq, "%2.2x ", InReg(scc->ctrl, reg));
			seq_printf(seq, "\n");
		}
#endif
		seq_putc(seq, '\n');
	}

        return 0;
}

static const struct seq_operations scc_net_seq_ops = {
	.start  = scc_net_seq_start,
	.next   = scc_net_seq_next,
	.stop   = scc_net_seq_stop,
	.show   = scc_net_seq_show,
};


static int scc_net_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scc_net_seq_ops);
}

static const struct file_operations scc_net_seq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = scc_net_seq_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_private,
};

#endif 

 

static int __init scc_init_driver (void)
{
	char devname[IFNAMSIZ];
	
	printk(banner);
	
	sprintf(devname,"%s0", SCC_DriverName);
	
	rtnl_lock();
	if (scc_net_alloc(devname, SCC_Info)) {
		rtnl_unlock();
		printk(KERN_ERR "z8530drv: cannot initialize module\n");
		return -EIO;
	}
	rtnl_unlock();

	proc_net_fops_create(&init_net, "z8530drv", 0, &scc_net_seq_fops);

	return 0;
}

static void __exit scc_cleanup_driver(void)
{
	io_port ctrl;
	int k;
	struct scc_channel *scc;
	struct net_device *dev;
	
	if (Nchips == 0 && (dev = SCC_Info[0].dev)) 
	{
		unregister_netdev(dev);
		free_netdev(dev);
	}

	
	local_irq_disable();
	
	for (k = 0; k < Nchips; k++)
		if ( (ctrl = SCC_ctrl[k].chan_A) )
		{
			Outb(ctrl, 0);
			OutReg(ctrl,R9,FHWRES);	
			udelay(50);
		}
		
	
	for (k = 0; k < nr_irqs ; k++)
		if (Ivec[k].used) free_irq(k, NULL);
		
	local_irq_enable();
		
	
	for (k = 0; k < Nchips*2; k++)
	{
		scc = &SCC_Info[k];
		if (scc->ctrl)
		{
			release_region(scc->ctrl, 1);
			release_region(scc->data, 1);
		}
		if (scc->dev)
		{
			unregister_netdev(scc->dev);
			free_netdev(scc->dev);
		}
	}
	
		
	if (Vector_Latch)
		release_region(Vector_Latch, 1);

	proc_net_remove(&init_net, "z8530drv");
}

MODULE_AUTHOR("Joerg Reuter <jreuter@yaina.de>");
MODULE_DESCRIPTION("AX.25 Device Driver for Z8530 based HDLC cards");
MODULE_SUPPORTED_DEVICE("Z8530 based SCC cards for Amateur Radio");
MODULE_LICENSE("GPL");
module_init(scc_init_driver);
module_exit(scc_cleanup_driver);
