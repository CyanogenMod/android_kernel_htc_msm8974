#ifndef LLC_CONN_H
#define LLC_CONN_H
/*
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001, 2002 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */
#include <linux/timer.h>
#include <net/llc_if.h>
#include <net/sock.h>
#include <linux/llc.h>

#define LLC_EVENT                1
#define LLC_PACKET               2

#define LLC2_P_TIME               2
#define LLC2_ACK_TIME             1
#define LLC2_REJ_TIME             3
#define LLC2_BUSY_TIME            3

struct llc_timer {
	struct timer_list timer;
	unsigned long	  expire;	
};

struct llc_sock {
	
	struct sock	    sk;
	struct sockaddr_llc addr;		
	u8		    state;		
	struct llc_sap	    *sap;		
	struct llc_addr	    laddr;		
	struct llc_addr	    daddr;		
	struct net_device   *dev;		
	u32		    copied_seq;		
	u8		    retry_count;	
	u8		    ack_must_be_send;
	u8		    first_pdu_Ns;
	u8		    npta;
	struct llc_timer    ack_timer;
	struct llc_timer    pf_cycle_timer;
	struct llc_timer    rej_sent_timer;
	struct llc_timer    busy_state_timer;	
	u8		    vS;			
	u8		    vR;			
	u32		    n2;			
	u32		    n1;			
	u8		    k;			
	u8		    rw;			
	u8		    p_flag;		
	u8		    f_flag;
	u8		    s_flag;
	u8		    data_flag;
	u8		    remote_busy_flag;
	u8		    cause_flag;
	struct sk_buff_head pdu_unack_q;	
	u16		    link;		
	u8		    X;			
	u8		    ack_pf;		
	u8		    failed_data_req; 
	u8		    dec_step;
	u8		    inc_cntr;
	u8		    dec_cntr;
	u8		    connect_step;
	u8		    last_nr;	   
	u32		    rx_pdu_hdr;	   
	u32		    cmsg_flags;
	struct hlist_node   dev_hash_node;
};

static inline struct llc_sock *llc_sk(const struct sock *sk)
{
	return (struct llc_sock *)sk;
}

static __inline__ void llc_set_backlog_type(struct sk_buff *skb, char type)
{
	skb->cb[sizeof(skb->cb) - 1] = type;
}

static __inline__ char llc_backlog_type(struct sk_buff *skb)
{
	return skb->cb[sizeof(skb->cb) - 1];
}

extern struct sock *llc_sk_alloc(struct net *net, int family, gfp_t priority,
				 struct proto *prot);
extern void llc_sk_free(struct sock *sk);

extern void llc_sk_reset(struct sock *sk);

extern int llc_conn_state_process(struct sock *sk, struct sk_buff *skb);
extern void llc_conn_send_pdu(struct sock *sk, struct sk_buff *skb);
extern void llc_conn_rtn_pdu(struct sock *sk, struct sk_buff *skb);
extern void llc_conn_resend_i_pdu_as_cmd(struct sock *sk, u8 nr,
					 u8 first_p_bit);
extern void llc_conn_resend_i_pdu_as_rsp(struct sock *sk, u8 nr,
					 u8 first_f_bit);
extern int llc_conn_remove_acked_pdus(struct sock *conn, u8 nr,
				      u16 *how_many_unacked);
extern struct sock *llc_lookup_established(struct llc_sap *sap,
					   struct llc_addr *daddr,
					   struct llc_addr *laddr);
extern void llc_sap_add_socket(struct llc_sap *sap, struct sock *sk);
extern void llc_sap_remove_socket(struct llc_sap *sap, struct sock *sk);

extern u8 llc_data_accept_state(u8 state);
extern void llc_build_offset_table(void);
#endif 
