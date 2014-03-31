#ifndef _NET_DN_H
#define _NET_DN_H

#include <linux/dn.h>
#include <net/sock.h>
#include <net/flow.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>

struct dn_scp                                   
{
        unsigned char           state;
#define DN_O     1                      
#define DN_CR    2                      
#define DN_DR    3                      
#define DN_DRC   4                      
#define DN_CC    5                      
#define DN_CI    6                      
#define DN_NR    7                      
#define DN_NC    8                      
#define DN_CD    9                      
#define DN_RJ    10                     
#define DN_RUN   11                     
#define DN_DI    12                     
#define DN_DIC   13                     
#define DN_DN    14                     
#define DN_CL    15                     
#define DN_CN    16                     

        __le16          addrloc;
        __le16          addrrem;
        __u16          numdat;
        __u16          numoth;
        __u16          numoth_rcv;
        __u16          numdat_rcv;
        __u16          ackxmt_dat;
        __u16          ackxmt_oth;
        __u16          ackrcv_dat;
        __u16          ackrcv_oth;
        __u8           flowrem_sw;
	__u8           flowloc_sw;
#define DN_SEND         2
#define DN_DONTSEND     1
#define DN_NOCHANGE     0
	__u16		flowrem_dat;
	__u16		flowrem_oth;
	__u16		flowloc_dat;
	__u16		flowloc_oth;
	__u8		services_rem;
	__u8		services_loc;
	__u8		info_rem;
	__u8		info_loc;

	__u16		segsize_rem;
	__u16		segsize_loc;

	__u8		nonagle;
	__u8		multi_ireq;
	__u8		accept_mode;
	unsigned long		seg_total; 

	struct optdata_dn     conndata_in;
	struct optdata_dn     conndata_out;
	struct optdata_dn     discdata_in;
	struct optdata_dn     discdata_out;
        struct accessdata_dn  accessdata;

        struct sockaddr_dn addr; 
	struct sockaddr_dn peer; 

#define NSP_MIN_WINDOW 1
#define NSP_MAX_WINDOW (0x07fe)
	unsigned long max_window;
	unsigned long snd_window;
#define NSP_INITIAL_SRTT (HZ)
	unsigned long nsp_srtt;
#define NSP_INITIAL_RTTVAR (HZ*3)
	unsigned long nsp_rttvar;
#define NSP_MAXRXTSHIFT 12
	unsigned long nsp_rxtshift;

	struct sk_buff_head data_xmit_queue;
	struct sk_buff_head other_xmit_queue;

	struct sk_buff_head other_receive_queue;
	int other_report;

	unsigned long stamp;          
	unsigned long persist;
	int (*persist_fxn)(struct sock *sk);
	unsigned long keepalive;
	void (*keepalive_fxn)(struct sock *sk);

	struct timer_list delack_timer;
	int delack_pending;
	void (*delack_fxn)(struct sock *sk);

};

static inline struct dn_scp *DN_SK(struct sock *sk)
{
	return (struct dn_scp *)(sk + 1);
}

#define DN_SKB_CB(skb) ((struct dn_skb_cb *)(skb)->cb)
struct dn_skb_cb {
	__le16 dst;
	__le16 src;
	__u16 hops;
	__le16 dst_port;
	__le16 src_port;
	__u8 services;
	__u8 info;
	__u8 rt_flags;
	__u8 nsp_flags;
	__u16 segsize;
	__u16 segnum;
	__u16 xmit_count;
	unsigned long stamp;
	int iif;
};

static inline __le16 dn_eth2dn(unsigned char *ethaddr)
{
	return get_unaligned((__le16 *)(ethaddr + 4));
}

static inline __le16 dn_saddr2dn(struct sockaddr_dn *saddr)
{
	return *(__le16 *)saddr->sdn_nodeaddr;
}

static inline void dn_dn2eth(unsigned char *ethaddr, __le16 addr)
{
	__u16 a = le16_to_cpu(addr);
	ethaddr[0] = 0xAA;
	ethaddr[1] = 0x00;
	ethaddr[2] = 0x04;
	ethaddr[3] = 0x00;
	ethaddr[4] = (__u8)(a & 0xff);
	ethaddr[5] = (__u8)(a >> 8);
}

static inline void dn_sk_ports_copy(struct flowidn *fld, struct dn_scp *scp)
{
	fld->fld_sport = scp->addrloc;
	fld->fld_dport = scp->addrrem;
}

extern unsigned dn_mss_from_pmtu(struct net_device *dev, int mtu);

#define DN_MENUVER_ACC 0x01
#define DN_MENUVER_USR 0x02
#define DN_MENUVER_PRX 0x04
#define DN_MENUVER_UIC 0x08

extern struct sock *dn_sklist_find_listener(struct sockaddr_dn *addr);
extern struct sock *dn_find_by_skb(struct sk_buff *skb);
#define DN_ASCBUF_LEN 9
extern char *dn_addr2asc(__u16, char *);
extern int dn_destroy_timer(struct sock *sk);

extern int dn_sockaddr2username(struct sockaddr_dn *addr, unsigned char *buf, unsigned char type);
extern int dn_username2sockaddr(unsigned char *data, int len, struct sockaddr_dn *addr, unsigned char *type);

extern void dn_start_slow_timer(struct sock *sk);
extern void dn_stop_slow_timer(struct sock *sk);

extern __le16 decnet_address;
extern int decnet_debug_level;
extern int decnet_time_wait;
extern int decnet_dn_count;
extern int decnet_di_count;
extern int decnet_dr_count;
extern int decnet_no_fc_max_cwnd;

extern long sysctl_decnet_mem[3];
extern int sysctl_decnet_wmem[3];
extern int sysctl_decnet_rmem[3];

#endif 
