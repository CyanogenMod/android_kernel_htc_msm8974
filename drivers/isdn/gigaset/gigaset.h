/*
 * Siemens Gigaset 307x driver
 * Common header file for all connection variants
 *
 * Written by Stefan Eilers
 *        and Hansjoerg Lipp <hjlipp@web.de>
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 */

#ifndef GIGASET_H
#define GIGASET_H

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ppp_defs.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/list.h>
#include <linux/atomic.h>

#define GIG_VERSION {0, 5, 0, 0}
#define GIG_COMPAT  {0, 4, 0, 0}

#define MAX_REC_PARAMS 10	
#define MAX_RESP_SIZE 511	

#define MAX_EVENTS 64		

#define RBUFSIZE 8192

#define GIG_TICK 100		

#define INIT_TIMEOUT 1

#define RING_TIMEOUT 3		
#define BAS_TIMEOUT 20		
#define ATRDY_TIMEOUT 3		

#define BAS_RETRY 3		

#define MAXACT 3

extern int gigaset_debuglevel;	

enum debuglevel {
	DEBUG_INTR	  = 0x00008, 
	DEBUG_CMD	  = 0x00020, 
	DEBUG_STREAM	  = 0x00040, 
	DEBUG_STREAM_DUMP = 0x00080, 
	DEBUG_LLDATA	  = 0x00100, 
	DEBUG_EVENT	  = 0x00200, 
	DEBUG_HDLC	  = 0x00800, 
	DEBUG_CHANNEL	  = 0x01000, 
	DEBUG_TRANSCMD	  = 0x02000, 
	DEBUG_MCMD	  = 0x04000, 
	DEBUG_INIT	  = 0x08000, 
	DEBUG_SUSPEND	  = 0x10000, 
	DEBUG_OUTPUT	  = 0x20000, 
	DEBUG_ISO	  = 0x40000, 
	DEBUG_IF	  = 0x80000, 
	DEBUG_USBREQ	  = 0x100000, 
	DEBUG_LOCKCMD	  = 0x200000, 

	DEBUG_ANY	  = 0x3fffff, 
};

#ifdef CONFIG_GIGASET_DEBUG

#define gig_dbg(level, format, arg...)					\
	do {								\
		if (unlikely(((enum debuglevel)gigaset_debuglevel) & (level))) \
			printk(KERN_DEBUG KBUILD_MODNAME ": " format "\n", \
			       ## arg);					\
	} while (0)
#define DEBUG_DEFAULT (DEBUG_TRANSCMD | DEBUG_CMD | DEBUG_USBREQ)

#else

#define gig_dbg(level, format, arg...) do {} while (0)
#define DEBUG_DEFAULT 0

#endif

void gigaset_dbg_buffer(enum debuglevel level, const unsigned char *msg,
			size_t len, const unsigned char *buf);

#define ZSAU_NONE			0
#define ZSAU_DISCONNECT_IND		4
#define ZSAU_OUTGOING_CALL_PROCEEDING	1
#define ZSAU_PROCEEDING			1
#define ZSAU_CALL_DELIVERED		2
#define ZSAU_ACTIVE			3
#define ZSAU_NULL			5
#define ZSAU_DISCONNECT_REQ		6
#define ZSAU_UNKNOWN			-1

#define OUT_VENDOR_REQ	(USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT)
#define IN_VENDOR_REQ	(USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT)

#define HD_B1_FLOW_CONTROL		0x80
#define HD_B2_FLOW_CONTROL		0x81
#define HD_RECEIVEATDATA_ACK		(0x35)		
#define HD_READY_SEND_ATDATA		(0x36)		
#define HD_OPEN_ATCHANNEL_ACK		(0x37)		
#define HD_CLOSE_ATCHANNEL_ACK		(0x38)		
#define HD_DEVICE_INIT_OK		(0x11)		
#define HD_OPEN_B1CHANNEL_ACK		(0x51)		
#define HD_OPEN_B2CHANNEL_ACK		(0x52)		
#define HD_CLOSE_B1CHANNEL_ACK		(0x53)		
#define HD_CLOSE_B2CHANNEL_ACK		(0x54)		
#define HD_SUSPEND_END			(0x61)		
#define HD_RESET_INTERRUPT_PIPE_ACK	(0xFF)		

#define	HD_OPEN_B1CHANNEL		(0x23)		
#define	HD_CLOSE_B1CHANNEL		(0x24)		
#define	HD_OPEN_B2CHANNEL		(0x25)		
#define	HD_CLOSE_B2CHANNEL		(0x26)		
#define HD_RESET_INTERRUPT_PIPE		(0x27)		
#define	HD_DEVICE_INIT_ACK		(0x34)		
#define	HD_WRITE_ATMESSAGE		(0x12)		
#define	HD_READ_ATMESSAGE		(0x13)		
#define	HD_OPEN_ATCHANNEL		(0x28)		
#define	HD_CLOSE_ATCHANNEL		(0x29)		

#define BAS_CHANNELS	2

#define BAS_FRAMETIME	1	
#define BAS_NUMFRAMES	8	
#define BAS_MAXFRAME	16	
#define BAS_NORMFRAME	8	
#define BAS_HIGHFRAME	10	
#define BAS_LOWFRAME	5	
#define BAS_CORRFRAMES	4	

#define BAS_INBUFSIZE	(BAS_MAXFRAME * BAS_NUMFRAMES)
#define BAS_OUTBUFSIZE	4096		
#define BAS_OUTBUFPAD	BAS_MAXFRAME	

#define BAS_INURBS	3
#define BAS_OUTURBS	3

#define AT_ISO		0
#define AT_DIAL		1
#define AT_MSN		2
#define AT_BC		3
#define AT_PROTO	4
#define AT_TYPE		5
#define AT_CLIP		6
#define AT_NUM		7

#define VAR_ZSAU	0
#define VAR_ZDLE	1
#define VAR_ZCTP	2
#define VAR_NUM		3

#define STR_NMBR	0
#define STR_ZCPN	1
#define STR_ZCON	2
#define STR_ZBC		3
#define STR_ZHLC	4
#define STR_NUM		5

#define EV_TIMEOUT	-105
#define EV_IF_VER	-106
#define EV_PROC_CIDMODE	-107
#define EV_SHUTDOWN	-108
#define EV_START	-110
#define EV_STOP		-111
#define EV_IF_LOCK	-112
#define EV_ACCEPT	-114
#define EV_DIAL		-115
#define EV_HUP		-116
#define EV_BC_OPEN	-117
#define EV_BC_CLOSED	-118

#define INS_command	0x0001	
#define INS_DLE_char	0x0002	
#define INS_byte_stuff	0x0004
#define INS_have_data	0x0008
#define INS_DLE_command	0x0020	
#define INS_flag_hunt	0x0040

#define CHS_D_UP	0x01
#define CHS_B_UP	0x02
#define CHS_NOTIFY_LL	0x04

#define ICALL_REJECT	0
#define ICALL_ACCEPT	1
#define ICALL_IGNORE	2

#define MS_UNINITIALIZED	0
#define MS_INIT			1
#define MS_LOCKED		2
#define MS_SHUTDOWN		3
#define MS_RECOVER		4
#define MS_READY		5

#define M_UNKNOWN	0
#define M_CONFIG	1
#define M_UNIMODEM	2
#define M_CID		3

#define SM_LOCKED	0
#define SM_ISDN		1 

#define L2_BITSYNC	0
#define L2_HDLC		1
#define L2_VOICE	2

struct gigaset_ops;
struct gigaset_driver;

struct usb_cardstate;
struct ser_cardstate;
struct bas_cardstate;

struct bc_state;
struct usb_bc_state;
struct ser_bc_state;
struct bas_bc_state;

struct reply_t {
	int	resp_code;	
	int	min_ConState;	
	int	max_ConState;	
	int	parameter;	
	int	new_ConState;	
	int	timeout;	
	int	action[MAXACT];	
	char	*command;	
};

extern struct reply_t gigaset_tab_cid[];
extern struct reply_t gigaset_tab_nocid[];

struct inbuf_t {
	struct cardstate	*cs;
	int			inputstate;
	int			head, tail;
	unsigned char		data[RBUFSIZE];
};

/* isochronous write buffer structure
 * circular buffer with pad area for extraction of complete USB frames
 * - data[read..nextread-1] is valid data already submitted to the USB subsystem
 * - data[nextread..write-1] is valid data yet to be sent
 * - data[write] is the next byte to write to
 *   - in byte-oriented L2 procotols, it is completely free
 *   - in bit-oriented L2 procotols, it may contain a partial byte of valid data
 * - data[write+1..read-1] is free
 * - wbits is the number of valid data bits in data[write], starting at the LSB
 * - writesem is the semaphore for writing to the buffer:
 *   if writesem <= 0, data[write..read-1] is currently being written to
 * - idle contains the byte value to repeat when the end of valid data is
 *   reached; if nextread==write (buffer contains no data to send), either the
 *   BAS_OUTBUFPAD bytes immediately before data[write] (if
 *   write>=BAS_OUTBUFPAD) or those of the pad area (if write<BAS_OUTBUFPAD)
 *   are also filled with that value
 */
struct isowbuf_t {
	int		read;
	int		nextread;
	int		write;
	atomic_t	writesem;
	int		wbits;
	unsigned char	data[BAS_OUTBUFSIZE + BAS_OUTBUFPAD];
	unsigned char	idle;
};

struct isow_urbctx_t {
	struct urb *urb;
	struct bc_state *bcs;
	int limit;
	int status;
};

struct at_state_t {
	struct list_head	list;
	int			waiting;
	int			getstring;
	unsigned		timer_index;
	unsigned long		timer_expires;
	int			timer_active;
	unsigned int		ConState;	
	struct reply_t		*replystruct;
	int			cid;
	int			int_var[VAR_NUM];	
	char			*str_var[STR_NUM];	
	unsigned		pending_commands;	
	unsigned		seq_index;

	struct cardstate	*cs;
	struct bc_state		*bcs;
};

struct event_t {
	int type;
	void *ptr, *arg;
	int parameter;
	int cid;
	struct at_state_t *at_state;
};

struct bc_state {
	struct sk_buff *tx_skb;		
	struct sk_buff_head squeue;	

	
	int corrupted;			
	int trans_down;			
	int trans_up;			

	struct at_state_t at_state;

	
	unsigned rx_bufsize;		
	struct sk_buff *rx_skb;
	__u16 rx_fcs;
	int inputstate;			

	int channel;

	struct cardstate *cs;

	unsigned chstate;		
	int ignore;
	unsigned proto2;		
	char *commands[AT_NUM];		

#ifdef CONFIG_GIGASET_DEBUG
	int emptycount;
#endif
	int busy;
	int use_count;

	
	union {
		struct ser_bc_state *ser;	
		struct usb_bc_state *usb;	
		struct bas_bc_state *bas;	
	} hw;

	void *ap;			
	int apconnstate;		
	spinlock_t aplock;
};

struct cardstate {
	struct gigaset_driver *driver;
	unsigned minor_index;
	struct device *dev;
	struct device *tty_dev;
	unsigned flags;

	const struct gigaset_ops *ops;

	
	wait_queue_head_t waitqueue;
	int waiting;
	int mode;			
	int mstate;			
					
	int cmd_result;

	int channels;
	struct bc_state *bcs;		

	int onechannel;			

	spinlock_t lock;
	struct at_state_t at_state;	
	struct list_head temp_at_states;

	struct inbuf_t *inbuf;

	struct cmdbuf_t *cmdbuf, *lastcmdbuf;
	spinlock_t cmdlock;
	unsigned curlen, cmdbytes;

	struct tty_port port;
	struct tasklet_struct if_wake_tasklet;
	unsigned control_state;

	unsigned fwver[4];
	int gotfwver;

	unsigned running;		
	unsigned connected;		
	unsigned isdn_up;		

	unsigned cidmode;

	int myid;			
	void *iif;			
	unsigned short hw_hdr_len;	

	struct reply_t *tabnocid;
	struct reply_t *tabcid;
	int cs_init;
	int ignoreframes;		
	struct mutex mutex;		

	struct timer_list timer;
	int retry_count;
	int dle;			
	int cur_at_seq;			
	int curchannel;			
	int commands_pending;		
	struct tasklet_struct event_tasklet;
	struct tasklet_struct write_tasklet;

	
	struct event_t events[MAX_EVENTS];
	unsigned ev_tail, ev_head;
	spinlock_t ev_lock;

	
	unsigned char respdata[MAX_RESP_SIZE + 1];
	unsigned cbytes;

	
	union {
		struct usb_cardstate *usb; 
		struct ser_cardstate *ser; 
		struct bas_cardstate *bas; 
	} hw;
};

struct gigaset_driver {
	struct list_head list;
	spinlock_t lock;		
	struct tty_driver *tty;
	unsigned have_tty;
	unsigned minor;
	unsigned minors;
	struct cardstate *cs;
	int blocked;

	const struct gigaset_ops *ops;
	struct module *owner;
};

struct cmdbuf_t {
	struct cmdbuf_t *next, *prev;
	int len, offset;
	struct tasklet_struct *wake_tasklet;
	unsigned char buf[0];
};

struct bas_bc_state {
	
	int		running;
	atomic_t	corrbytes;
	spinlock_t	isooutlock;
	struct isow_urbctx_t	isoouturbs[BAS_OUTURBS];
	struct isow_urbctx_t	*isooutdone, *isooutfree, *isooutovfl;
	struct isowbuf_t	*isooutbuf;
	unsigned numsub;		
	struct tasklet_struct	sent_tasklet;

	
	spinlock_t isoinlock;
	struct urb *isoinurbs[BAS_INURBS];
	unsigned char isoinbuf[BAS_INBUFSIZE * BAS_INURBS];
	struct urb *isoindone;		
	int isoinstatus;		
	int loststatus;			
	unsigned isoinlost;		
	unsigned seqlen;		
	unsigned inbyte, inbits;	
	
	unsigned goodbytes;		
	unsigned alignerrs;		
	unsigned fcserrs;		
	unsigned frameerrs;		
	unsigned giants;		
	unsigned runts;			
	unsigned aborts;		
	unsigned shared0s;		
	unsigned stolen0s;		
	struct tasklet_struct rcvd_tasklet;
};

struct gigaset_ops {
	int (*write_cmd)(struct cardstate *cs, struct cmdbuf_t *cb);

	
	int (*write_room)(struct cardstate *cs);
	int (*chars_in_buffer)(struct cardstate *cs);
	int (*brkchars)(struct cardstate *cs, const unsigned char buf[6]);

	int (*init_bchannel)(struct bc_state *bcs);

	int (*close_bchannel)(struct bc_state *bcs);

	
	int (*initbcshw)(struct bc_state *bcs);

	
	int (*freebcshw)(struct bc_state *bcs);

	
	void (*reinitbcshw)(struct bc_state *bcs);

	
	int (*initcshw)(struct cardstate *cs);

	
	void (*freecshw)(struct cardstate *cs);

	int (*set_modem_ctrl)(struct cardstate *cs, unsigned old_state,
			      unsigned new_state);
	int (*baud_rate)(struct cardstate *cs, unsigned cflag);
	int (*set_line_ctrl)(struct cardstate *cs, unsigned cflag);

	int (*send_skb)(struct bc_state *bcs, struct sk_buff *skb);

	void (*handle_input)(struct inbuf_t *inbuf);

};


#define DLE_FLAG	0x10


int gigaset_m10x_send_skb(struct bc_state *bcs, struct sk_buff *skb);

void gigaset_m10x_input(struct inbuf_t *inbuf);


int gigaset_isoc_send_skb(struct bc_state *bcs, struct sk_buff *skb);

void gigaset_isoc_input(struct inbuf_t *inbuf);

void gigaset_isoc_receive(unsigned char *src, unsigned count,
			  struct bc_state *bcs);

int gigaset_isoc_buildframe(struct bc_state *bcs, unsigned char *in, int len);

void gigaset_isowbuf_init(struct isowbuf_t *iwb, unsigned char idle);

int gigaset_isowbuf_getbytes(struct isowbuf_t *iwb, int size);


void gigaset_isdn_regdrv(void);
void gigaset_isdn_unregdrv(void);
int gigaset_isdn_regdev(struct cardstate *cs, const char *isdnid);
void gigaset_isdn_unregdev(struct cardstate *cs);

void gigaset_skb_sent(struct bc_state *bcs, struct sk_buff *skb);
void gigaset_skb_rcvd(struct bc_state *bcs, struct sk_buff *skb);
void gigaset_isdn_rcv_err(struct bc_state *bcs);

void gigaset_isdn_start(struct cardstate *cs);
void gigaset_isdn_stop(struct cardstate *cs);
int gigaset_isdn_icall(struct at_state_t *at_state);
void gigaset_isdn_connD(struct bc_state *bcs);
void gigaset_isdn_hupD(struct bc_state *bcs);
void gigaset_isdn_connB(struct bc_state *bcs);
void gigaset_isdn_hupB(struct bc_state *bcs);


void gigaset_handle_event(unsigned long data);

void gigaset_handle_modem_response(struct cardstate *cs);


void gigaset_init_dev_sysfs(struct cardstate *cs);
void gigaset_free_dev_sysfs(struct cardstate *cs);


void gigaset_bcs_reinit(struct bc_state *bcs);
void gigaset_at_init(struct at_state_t *at_state, struct bc_state *bcs,
		     struct cardstate *cs, int cid);
int gigaset_get_channel(struct bc_state *bcs);
struct bc_state *gigaset_get_free_channel(struct cardstate *cs);
void gigaset_free_channel(struct bc_state *bcs);
int gigaset_get_channels(struct cardstate *cs);
void gigaset_free_channels(struct cardstate *cs);
void gigaset_block_channels(struct cardstate *cs);

struct gigaset_driver *gigaset_initdriver(unsigned minor, unsigned minors,
					  const char *procname,
					  const char *devname,
					  const struct gigaset_ops *ops,
					  struct module *owner);

void gigaset_freedriver(struct gigaset_driver *drv);

struct cardstate *gigaset_get_cs_by_tty(struct tty_struct *tty);
struct cardstate *gigaset_get_cs_by_id(int id);
void gigaset_blockdriver(struct gigaset_driver *drv);

struct cardstate *gigaset_initcs(struct gigaset_driver *drv, int channels,
				 int onechannel, int ignoreframes,
				 int cidmode, const char *modulename);

void gigaset_freecs(struct cardstate *cs);

int gigaset_start(struct cardstate *cs);

void gigaset_stop(struct cardstate *cs);

int gigaset_shutdown(struct cardstate *cs);

void gigaset_skb_sent(struct bc_state *bcs, struct sk_buff *skb);

struct event_t *gigaset_add_event(struct cardstate *cs,
				  struct at_state_t *at_state, int type,
				  void *ptr, int parameter, void *arg);

int gigaset_enterconfigmode(struct cardstate *cs);

static inline void gigaset_schedule_event(struct cardstate *cs)
{
	unsigned long flags;
	spin_lock_irqsave(&cs->lock, flags);
	if (cs->running)
		tasklet_schedule(&cs->event_tasklet);
	spin_unlock_irqrestore(&cs->lock, flags);
}

static inline void gigaset_bchannel_down(struct bc_state *bcs)
{
	gigaset_add_event(bcs->cs, &bcs->at_state, EV_BC_CLOSED, NULL, 0, NULL);
	gigaset_schedule_event(bcs->cs);
}

static inline void gigaset_bchannel_up(struct bc_state *bcs)
{
	gigaset_add_event(bcs->cs, &bcs->at_state, EV_BC_OPEN, NULL, 0, NULL);
	gigaset_schedule_event(bcs->cs);
}

static inline struct sk_buff *gigaset_new_rx_skb(struct bc_state *bcs)
{
	struct cardstate *cs = bcs->cs;
	unsigned short hw_hdr_len = cs->hw_hdr_len;

	if (bcs->ignore) {
		bcs->rx_skb = NULL;
	} else {
		bcs->rx_skb = dev_alloc_skb(bcs->rx_bufsize + hw_hdr_len);
		if (bcs->rx_skb == NULL)
			dev_warn(cs->dev, "could not allocate skb\n");
		else
			skb_reserve(bcs->rx_skb, hw_hdr_len);
	}
	return bcs->rx_skb;
}

int gigaset_fill_inbuf(struct inbuf_t *inbuf, const unsigned char *src,
		       unsigned numbytes);


void gigaset_if_initdriver(struct gigaset_driver *drv, const char *procname,
			   const char *devname);
void gigaset_if_freedriver(struct gigaset_driver *drv);
void gigaset_if_init(struct cardstate *cs);
void gigaset_if_free(struct cardstate *cs);
void gigaset_if_receive(struct cardstate *cs,
			unsigned char *buffer, size_t len);

#endif
