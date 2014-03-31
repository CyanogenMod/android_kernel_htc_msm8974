/*
 * INET         An implementation of the TCP/IP protocol suite for the LINUX
 *              operating system.  NET  is implemented using the  BSD Socket
 *              interface as the means of communication with the user level.
 *
 *              Definitions used by the ARCnet driver.
 *
 * Authors:     Avery Pennarun and David Woodhouse
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 */
#ifndef _LINUX_ARCDEVICE_H
#define _LINUX_ARCDEVICE_H

#include <asm/timex.h>
#include <linux/if_arcnet.h>

#ifdef __KERNEL__
#include  <linux/irqreturn.h>

#ifndef bool
#define bool int
#endif

#define RECON_THRESHOLD 30


#define TX_TIMEOUT (HZ * 200 / 1000)


#undef ALPHA_WARNING


#define D_NORMAL	1	
#define D_EXTRA		2	
#define	D_INIT		4	
#define D_INIT_REASONS	8	
#define D_RECON		32	
#define D_PROTO		64	
#define D_DURING	128	
#define D_TX	        256	
#define D_RX		512	
#define D_SKB		1024	
#define D_SKB_SIZE	2048	
#define D_TIMING	4096	
#define D_DEBUG         8192    

#ifndef ARCNET_DEBUG_MAX
#define ARCNET_DEBUG_MAX (127)	
#endif

#ifndef ARCNET_DEBUG
#define ARCNET_DEBUG (D_NORMAL|D_EXTRA)
#endif
extern int arcnet_debug;

#define BUGLVL(x) if ((ARCNET_DEBUG_MAX)&arcnet_debug&(x))
#define BUGMSG2(x,msg,args...) do { BUGLVL(x) printk(msg, ## args); } while (0)
#define BUGMSG(x,msg,args...) \
	BUGMSG2(x, "%s%6s: " msg, \
            x==D_NORMAL	? KERN_WARNING \
            		: x < D_DURING ? KERN_INFO : KERN_DEBUG, \
	    dev->name , ## args)

#define TIME(name, bytes, call) BUGLVL(D_TIMING) { \
	    unsigned long _x, _y; \
	    _x = get_cycles(); \
	    call; \
	    _y = get_cycles(); \
	    BUGMSG(D_TIMING, \
	       "%s: %d bytes in %lu cycles == " \
	       "%lu Kbytes/100Mcycle\n",\
		   name, bytes, _y - _x, \
		   100000000 / 1024 * bytes / (_y - _x + 1));\
	} \
	else { \
		    call;\
	}


#define RESETtime (300)

#define MTU	253		
#define MinTU	257		
#define XMTU	508		

#define TXFREEflag	0x01	
#define TXACKflag       0x02	
#define RECONflag       0x04	
#define TESTflag        0x08	
#define EXCNAKflag      0x08    
#define RESETflag       0x10	
#define RES1flag        0x20	
#define RES2flag        0x40	
#define NORXflag        0x80	

#define AUTOINCflag     0x40	
#define IOMAPflag       0x02	
#define ENABLE16flag    0x80	

#define NOTXcmd         0x01	
#define NORXcmd         0x02	
#define TXcmd           0x03	
#define RXcmd           0x04	
#define CONFIGcmd       0x05	
#define CFLAGScmd       0x06	
#define TESTcmd         0x07	

#define RESETclear      0x08	
#define CONFIGclear     0x10	

#define EXCNAKclear     0x0E    

#define TESTload        0x08	

#define TESTvalue       0321	

#define RXbcasts        0x80	

#define NORMALconf      0x00	
#define EXTconf         0x08	

#define ARC_IS_5MBIT    1   
#define ARC_CAN_10MBIT  2   


struct ArcProto {
	char suffix;		
	int mtu;		
	int is_ip;              

	void (*rx) (struct net_device * dev, int bufnum,
		    struct archdr * pkthdr, int length);
	int (*build_header) (struct sk_buff * skb, struct net_device *dev,
			     unsigned short ethproto, uint8_t daddr);

	
	int (*prepare_tx) (struct net_device * dev, struct archdr * pkt, int length,
			   int bufnum);
	int (*continue_tx) (struct net_device * dev, int bufnum);
	int (*ack_tx) (struct net_device * dev, int acked);
};

extern struct ArcProto *arc_proto_map[256], *arc_proto_default,
	*arc_bcast_proto, *arc_raw_proto;


struct Incoming {
	struct sk_buff *skb;	
	__be16 sequence;	
	uint8_t lastpacket,	
		numpackets;	
};


struct Outgoing {
	struct ArcProto *proto;	
	struct sk_buff *skb;	
	struct archdr *pkt;	
	uint16_t length,	
		dataleft,	
		segnum,		
		numsegs;	
};


struct arcnet_local {
	uint8_t config,		
		timeout,	
		backplane,	
		clockp,		
		clockm,		
		setup,		
		setup2,		
		intmask;	
	uint8_t default_proto[256];	
	int	cur_tx,		
		next_tx,	
		cur_rx;		
	int	lastload_dest,	
		lasttrans_dest;	
	int	timed_out;	
	unsigned long last_timeout;	
	char *card_name;	
	int card_flags;		


	
	spinlock_t lock;

	atomic_t buf_lock;
	int buf_queue[5];
	int next_buf, first_free_buf;

	
	unsigned long first_recon; 
	unsigned long last_recon;  
	int num_recons;		
	bool network_down;	

	bool excnak_pending;    

	struct {
		uint16_t sequence;	
		__be16 aborted_seq;

		struct Incoming incoming[256];	
	} rfc1201;

	
	struct Outgoing outgoing;	

	
	struct {
		struct module *owner;
		void (*command) (struct net_device * dev, int cmd);
		int (*status) (struct net_device * dev);
		void (*intmask) (struct net_device * dev, int mask);
		bool (*reset) (struct net_device * dev, bool really_reset);
		void (*open) (struct net_device * dev);
		void (*close) (struct net_device * dev);

		void (*copy_to_card) (struct net_device * dev, int bufnum, int offset,
				      void *buf, int count);
		void (*copy_from_card) (struct net_device * dev, int bufnum, int offset,
					void *buf, int count);
	} hw;

	void __iomem *mem_start;	
};


#define ARCRESET(x)  (lp->hw.reset(dev, (x)))
#define ACOMMAND(x)  (lp->hw.command(dev, (x)))
#define ASTATUS()    (lp->hw.status(dev))
#define AINTMASK(x)  (lp->hw.intmask(dev, (x)))



#if ARCNET_DEBUG_MAX & D_SKB
void arcnet_dump_skb(struct net_device *dev, struct sk_buff *skb, char *desc);
#else
#define arcnet_dump_skb(dev,skb,desc) ;
#endif

void arcnet_unregister_proto(struct ArcProto *proto);
irqreturn_t arcnet_interrupt(int irq, void *dev_id);
struct net_device *alloc_arcdev(const char *name);

int arcnet_open(struct net_device *dev);
int arcnet_close(struct net_device *dev);
netdev_tx_t arcnet_send_packet(struct sk_buff *skb,
				     struct net_device *dev);
void arcnet_timeout(struct net_device *dev);

#endif				
#endif				
