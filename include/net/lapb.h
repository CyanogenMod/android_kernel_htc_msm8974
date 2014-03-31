#ifndef _LAPB_H
#define _LAPB_H 
#include <linux/lapb.h>

#define	LAPB_HEADER_LEN	20		

#define	LAPB_ACK_PENDING_CONDITION	0x01
#define	LAPB_REJECT_CONDITION		0x02
#define	LAPB_PEER_RX_BUSY_CONDITION	0x04

#define	LAPB_I		0x00	
#define	LAPB_S		0x01	
#define	LAPB_U		0x03	

#define	LAPB_RR		0x01	
#define	LAPB_RNR	0x05	
#define	LAPB_REJ	0x09	

#define	LAPB_SABM	0x2F	
#define	LAPB_SABME	0x6F	
#define	LAPB_DISC	0x43	
#define	LAPB_DM		0x0F	
#define	LAPB_UA		0x63	
#define	LAPB_FRMR	0x87	

#define LAPB_ILLEGAL	0x100	

#define	LAPB_SPF	0x10	
#define	LAPB_EPF	0x01	

#define	LAPB_FRMR_W	0x01	
#define	LAPB_FRMR_X	0x02	
#define	LAPB_FRMR_Y	0x04	
#define	LAPB_FRMR_Z	0x08	

#define	LAPB_POLLOFF	0
#define	LAPB_POLLON	1

#define LAPB_COMMAND	1
#define LAPB_RESPONSE	2

#define	LAPB_ADDR_A	0x03
#define	LAPB_ADDR_B	0x01
#define	LAPB_ADDR_C	0x0F
#define	LAPB_ADDR_D	0x07

enum {
	LAPB_STATE_0,	
	LAPB_STATE_1,	
	LAPB_STATE_2,	
	LAPB_STATE_3,	
	LAPB_STATE_4	
};

#define	LAPB_DEFAULT_MODE		(LAPB_STANDARD | LAPB_SLP | LAPB_DTE)
#define	LAPB_DEFAULT_WINDOW		7		
#define	LAPB_DEFAULT_T1			(5 * HZ)	
#define	LAPB_DEFAULT_T2			(1 * HZ)	
#define	LAPB_DEFAULT_N2			20		

#define	LAPB_SMODULUS	8
#define	LAPB_EMODULUS	128

struct lapb_frame {
	unsigned short		type;		
	unsigned short		nr, ns;		
	unsigned char		cr;		
	unsigned char		pf;		
	unsigned char		control[2];	
};

struct lapb_cb {
	struct list_head	node;
	struct net_device	*dev;

	
	unsigned int		mode;
	unsigned char		state;
	unsigned short		vs, vr, va;
	unsigned char		condition;
	unsigned short		n2, n2count;
	unsigned short		t1, t2;
	struct timer_list	t1timer, t2timer;

	
	struct sk_buff_head	write_queue;
	struct sk_buff_head	ack_queue;
	unsigned char		window;
	const struct lapb_register_struct *callbacks;

	
	struct lapb_frame	frmr_data;
	unsigned char		frmr_type;

	atomic_t		refcnt;
};

extern void lapb_connect_confirmation(struct lapb_cb *lapb, int);
extern void lapb_connect_indication(struct lapb_cb *lapb, int);
extern void lapb_disconnect_confirmation(struct lapb_cb *lapb, int);
extern void lapb_disconnect_indication(struct lapb_cb *lapb, int);
extern int  lapb_data_indication(struct lapb_cb *lapb, struct sk_buff *);
extern int  lapb_data_transmit(struct lapb_cb *lapb, struct sk_buff *);

extern void lapb_data_input(struct lapb_cb *lapb, struct sk_buff *);

extern void lapb_kick(struct lapb_cb *lapb);
extern void lapb_transmit_buffer(struct lapb_cb *lapb, struct sk_buff *, int);
extern void lapb_establish_data_link(struct lapb_cb *lapb);
extern void lapb_enquiry_response(struct lapb_cb *lapb);
extern void lapb_timeout_response(struct lapb_cb *lapb);
extern void lapb_check_iframes_acked(struct lapb_cb *lapb, unsigned short);
extern void lapb_check_need_response(struct lapb_cb *lapb, int, int);

extern void lapb_clear_queues(struct lapb_cb *lapb);
extern void lapb_frames_acked(struct lapb_cb *lapb, unsigned short);
extern void lapb_requeue_frames(struct lapb_cb *lapb);
extern int  lapb_validate_nr(struct lapb_cb *lapb, unsigned short);
extern int lapb_decode(struct lapb_cb *lapb, struct sk_buff *, struct lapb_frame *);
extern void lapb_send_control(struct lapb_cb *lapb, int, int, int);
extern void lapb_transmit_frmr(struct lapb_cb *lapb);

extern void lapb_start_t1timer(struct lapb_cb *lapb);
extern void lapb_start_t2timer(struct lapb_cb *lapb);
extern void lapb_stop_t1timer(struct lapb_cb *lapb);
extern void lapb_stop_t2timer(struct lapb_cb *lapb);
extern int  lapb_t1timer_running(struct lapb_cb *lapb);

#define	LAPB_DEBUG	0

#endif
