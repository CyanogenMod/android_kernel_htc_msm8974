/*
 * Driver for ST5481 USB ISDN modem
 *
 * Author       Frode Isaksen
 * Copyright    2001 by Frode Isaksen      <fisaksen@bewan.com>
 *              2001 by Kai Germaschewski  <kai.germaschewski@gmx.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef _ST5481_H_
#define _ST5481_H_



#define ST_VENDOR_ID 0x0483
#define ST5481_PRODUCT_ID 0x4810
#define ST5481_PRODUCT_ID_MASK 0xFFF0


#define EP_CTRL   0x00U 
#define EP_INT    0x01U 
#define EP_B1_OUT 0x02U 
#define EP_B1_IN  0x03U 
#define EP_B2_OUT 0x04U 
#define EP_B2_IN  0x05U 
#define EP_D_OUT  0x06U 
#define EP_D_IN   0x07U 


#define NUM_ISO_PACKETS_D      20
#define NUM_ISO_PACKETS_B      20

#define SIZE_ISO_PACKETS_D_IN  16
#define SIZE_ISO_PACKETS_D_OUT 2
#define SIZE_ISO_PACKETS_B_IN  32
#define SIZE_ISO_PACKETS_B_OUT 8

#define B_FLOW_ADJUST 2

// Registers that are written using vendor specific device request

#define LBA			0x02 
#define SET_DEFAULT		0x06 
#define LBB			0x1D 
#define STT			0x1e 
#define SDA_MIN			0x20 
#define SDA_MAX			0x21 
#define SDELAY_VALUE		0x22 
#define IN_D_COUNTER		0x36 
#define OUT_D_COUNTER		0x37 
#define IN_B1_COUNTER		0x38 
#define OUT_B1_COUNTER		0x39 
#define IN_B2_COUNTER		0x3a 
#define OUT_B2_COUNTER		0x3b 
#define FFCTRL_IN_D		0x3C 
#define FFCTRH_IN_D		0x3D 
#define FFCTRL_OUT_D		0x3E 
#define FFCTRH_OUT_D		0x3F 
#define FFCTRL_IN_B1		0x40 
#define FFCTRH_IN_B1		0x41 
#define FFCTRL_OUT_B1		0x42 
#define FFCTRH_OUT_B1		0x43 
#define FFCTRL_IN_B2		0x44 
#define FFCTRH_IN_B2		0x45 
#define FFCTRL_OUT_B2		0x46 
#define FFCTRH_OUT_B2		0x47 
#define MPMSK			0x4A 
#define	FFMSK_D			0x4c 
#define	FFMSK_B1		0x4e 
#define	FFMSK_B2		0x50 
#define GPIO_DIR		0x52 
#define GPIO_OUT		0x53 
#define GPIO_IN			0x54 
#define TXCI			0x56 




#define MPINT			0
#define FFINT_D			1
#define FFINT_B1		2
#define FFINT_B2		3
#define CCIST			4
#define GPIO_INT		5
#define INT_PKT_SIZE            6

#define LSD_INT                 0x80 
#define RXCI_INT		0x40 
#define	DEN_INT			0x20 
#define DCOLL_INT		0x10 
#define AMIVN_INT		0x04 
#define INFOI_INT		0x04 
#define DRXON_INT               0x02 
#define GPCHG_INT               0x01 

#define IN_OVERRUN		0x80 
#define OUT_UNDERRUN		0x40 
#define IN_UP			0x20 
#define IN_DOWN			0x10 
#define OUT_UP			0x08 
#define OUT_DOWN		0x04 
#define IN_COUNTER_ZEROED	0x02 
#define OUT_COUNTER_ZEROED	0x01 

#define ANY_REC_INT	(IN_OVERRUN + IN_UP + IN_DOWN + IN_COUNTER_ZEROED)
#define ANY_XMIT_INT	(OUT_UNDERRUN + OUT_UP + OUT_DOWN + OUT_COUNTER_ZEROED)


#define ST5481_CMD_DR		 0x0 
#define ST5481_CMD_RES		 0x1 
#define ST5481_CMD_TM1		 0x2 
#define ST5481_CMD_TM2		 0x3 
#define ST5481_CMD_PUP		 0x7 
#define ST5481_CMD_AR8		 0x8 
#define ST5481_CMD_AR10		 0x9 
#define ST5481_CMD_ARL		 0xA 
#define ST5481_CMD_PDN		 0xF 

#define B1_LED		0x10U
#define B2_LED		0x20U
#define GREEN_LED	0x40U
#define RED_LED	        0x80U

enum {
	ST_DOUT_NONE,

	ST_DOUT_SHORT_INIT,
	ST_DOUT_SHORT_WAIT_DEN,

	ST_DOUT_LONG_INIT,
	ST_DOUT_LONG_WAIT_DEN,
	ST_DOUT_NORMAL,

	ST_DOUT_WAIT_FOR_UNDERRUN,
	ST_DOUT_WAIT_FOR_NOT_BUSY,
	ST_DOUT_WAIT_FOR_STOP,
	ST_DOUT_WAIT_FOR_RESET,
};

#define DOUT_STATE_COUNT (ST_DOUT_WAIT_FOR_RESET + 1)

enum {
	EV_DOUT_START_XMIT,
	EV_DOUT_COMPLETE,
	EV_DOUT_DEN,
	EV_DOUT_RESETED,
	EV_DOUT_STOPPED,
	EV_DOUT_COLL,
	EV_DOUT_UNDERRUN,
};

#define DOUT_EVENT_COUNT (EV_DOUT_UNDERRUN + 1)


enum {
	ST_L1_F3,
	ST_L1_F4,
	ST_L1_F6,
	ST_L1_F7,
	ST_L1_F8,
};

#define L1_STATE_COUNT (ST_L1_F8 + 1)


enum {
	EV_IND_DP,  
	EV_IND_1,   
	EV_IND_2,   
	EV_IND_3,   
	EV_IND_RSY, 
	EV_IND_5,   
	EV_IND_6,   
	EV_IND_7,   
	EV_IND_AP,  
	EV_IND_9,   
	EV_IND_10,  
	EV_IND_11,  
	EV_IND_AI8, 
	EV_IND_AI10,
	EV_IND_AIL, 
	EV_IND_DI,  
	EV_PH_ACTIVATE_REQ,
	EV_PH_DEACTIVATE_REQ,
	EV_TIMER3,
};

#define L1_EVENT_COUNT (EV_TIMER3 + 1)

#define ERR(format, arg...)						\
	printk(KERN_ERR "%s:%s: " format "\n" , __FILE__,  __func__ , ## arg)

#define WARNING(format, arg...)						\
	printk(KERN_WARNING "%s:%s: " format "\n" , __FILE__,  __func__ , ## arg)

#define INFO(format, arg...)						\
	printk(KERN_INFO "%s:%s: " format "\n" , __FILE__,  __func__ , ## arg)

#include <linux/isdn/hdlc.h>
#include "fsm.h"
#include "hisax_if.h"
#include <linux/skbuff.h>


struct fifo {
	u_char r, w, count, size;
	spinlock_t lock;
};

static inline void fifo_init(struct fifo *fifo, int size)
{
	fifo->r = fifo->w = fifo->count = 0;
	fifo->size = size;
	spin_lock_init(&fifo->lock);
}

static inline int fifo_add(struct fifo *fifo)
{
	unsigned long flags;
	int index;

	if (!fifo) {
		return -1;
	}

	spin_lock_irqsave(&fifo->lock, flags);
	if (fifo->count == fifo->size) {
		
		index = -1;
	} else {
		
		index = fifo->w++ & (fifo->size - 1);
		fifo->count++;
	}
	spin_unlock_irqrestore(&fifo->lock, flags);
	return index;
}

static inline int fifo_remove(struct fifo *fifo)
{
	unsigned long flags;
	int index;

	if (!fifo) {
		return -1;
	}

	spin_lock_irqsave(&fifo->lock, flags);
	if (!fifo->count) {
		
		index = -1;
	} else {
		
		index = fifo->r++ & (fifo->size - 1);
		fifo->count--;
	}
	spin_unlock_irqrestore(&fifo->lock, flags);

	return index;
}

typedef void (*ctrl_complete_t)(void *);

typedef struct ctrl_msg {
	struct usb_ctrlrequest dr;
	ctrl_complete_t complete;
	void *context;
} ctrl_msg;

#define MAX_EP0_MSG 16
struct ctrl_msg_fifo {
	struct fifo f;
	struct ctrl_msg data[MAX_EP0_MSG];
};

#define MAX_DFRAME_LEN_L1	300
#define HSCX_BUFMAX	4096

struct st5481_ctrl {
	struct ctrl_msg_fifo msg_fifo;
	unsigned long busy;
	struct urb *urb;
};

struct st5481_intr {
	
	struct urb *urb;
};

struct st5481_d_out {
	struct isdnhdlc_vars hdlc_state;
	struct urb *urb[2]; 
	unsigned long busy;
	struct sk_buff *tx_skb;
	struct FsmInst fsm;
};

struct st5481_b_out {
	struct isdnhdlc_vars hdlc_state;
	struct urb *urb[2]; 
	u_char flow_event;
	u_long busy;
	struct sk_buff *tx_skb;
};

struct st5481_in {
	struct isdnhdlc_vars hdlc_state;
	struct urb *urb[2]; 
	int mode;
	int bufsize;
	unsigned int num_packets;
	unsigned int packet_size;
	unsigned char ep, counter;
	unsigned char *rcvbuf;
	struct st5481_adapter *adapter;
	struct hisax_if *hisax_if;
};

int st5481_setup_in(struct st5481_in *in);
void st5481_release_in(struct st5481_in *in);
void st5481_in_mode(struct st5481_in *in, int mode);

struct st5481_bcs {
	struct hisax_b_if b_if;
	struct st5481_adapter *adapter;
	struct st5481_in b_in;
	struct st5481_b_out b_out;
	int channel;
	int mode;
};

struct st5481_adapter {
	int number_of_leds;
	struct usb_device *usb_dev;
	struct hisax_d_if hisax_d_if;

	struct st5481_ctrl ctrl;
	struct st5481_intr intr;
	struct st5481_in d_in;
	struct st5481_d_out d_out;

	unsigned char leds;
	unsigned int led_counter;

	unsigned long event;

	struct FsmInst l1m;
	struct FsmTimer timer;

	struct st5481_bcs bcs[2];
};

#define TIMER3_VALUE 7000


#define SUBMIT_URB(urb, mem_flags)					\
	({								\
		int status;						\
		if ((status = usb_submit_urb(urb, mem_flags)) < 0) {	\
			WARNING("usb_submit_urb failed,status=%d", status); \
		}							\
		status;							\
	})

static inline int get_buf_nr(struct urb *urbs[], struct urb *urb)
{
	return (urbs[0] == urb ? 0 : 1);
}



int  st5481_setup_b(struct st5481_bcs *bcs);
void st5481_release_b(struct st5481_bcs *bcs);
void st5481_d_l2l1(struct hisax_if *hisax_d_if, int pr, void *arg);


int  st5481_setup_d(struct st5481_adapter *adapter);
void st5481_release_d(struct st5481_adapter *adapter);
void st5481_b_l2l1(struct hisax_if *b_if, int pr, void *arg);
int  st5481_d_init(void);
void st5481_d_exit(void);

void st5481_ph_command(struct st5481_adapter *adapter, unsigned int command);
int st5481_setup_isocpipes(struct urb *urb[2], struct usb_device *dev,
			   unsigned int pipe, int num_packets,
			   int packet_size, int buf_size,
			   usb_complete_t complete, void *context);
void st5481_release_isocpipes(struct urb *urb[2]);

void st5481_usb_pipe_reset(struct st5481_adapter *adapter,
			   u_char pipe, ctrl_complete_t complete, void *context);
void st5481_usb_device_ctrl_msg(struct st5481_adapter *adapter,
				u8 request, u16 value,
				ctrl_complete_t complete, void *context);
int  st5481_setup_usb(struct st5481_adapter *adapter);
void st5481_release_usb(struct st5481_adapter *adapter);
void st5481_start(struct st5481_adapter *adapter);
void st5481_stop(struct st5481_adapter *adapter);


#define __debug_variable st5481_debug
#include "hisax_debug.h"

extern int st5481_debug;

#ifdef CONFIG_HISAX_DEBUG

#define DBG_ISO_PACKET(level, urb)					\
	if (level & __debug_variable) dump_iso_packet(__func__, urb)

static void __attribute__((unused))
dump_iso_packet(const char *name, struct urb *urb)
{
	int i, j;
	int len, ofs;
	u_char *data;

	printk(KERN_DEBUG "%s: packets=%d,errors=%d\n",
	       name, urb->number_of_packets, urb->error_count);
	for (i = 0; i  < urb->number_of_packets; ++i) {
		if (urb->pipe & USB_DIR_IN) {
			len = urb->iso_frame_desc[i].actual_length;
		} else {
			len = urb->iso_frame_desc[i].length;
		}
		ofs = urb->iso_frame_desc[i].offset;
		printk(KERN_DEBUG "len=%.2d,ofs=%.3d ", len, ofs);
		if (len) {
			data = urb->transfer_buffer + ofs;
			for (j = 0; j < len; j++) {
				printk("%.2x", data[j]);
			}
		}
		printk("\n");
	}
}

static inline const char *ST5481_CMD_string(int evt)
{
	static char s[16];

	switch (evt) {
	case ST5481_CMD_DR: return "DR";
	case ST5481_CMD_RES: return "RES";
	case ST5481_CMD_TM1: return "TM1";
	case ST5481_CMD_TM2: return "TM2";
	case ST5481_CMD_PUP: return "PUP";
	case ST5481_CMD_AR8: return "AR8";
	case ST5481_CMD_AR10: return "AR10";
	case ST5481_CMD_ARL: return "ARL";
	case ST5481_CMD_PDN: return "PDN";
	};

	sprintf(s, "0x%x", evt);
	return s;
}

#else

#define DBG_ISO_PACKET(level, urb) do {} while (0)

#endif



#endif
