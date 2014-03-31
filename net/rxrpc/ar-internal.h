/* AF_RXRPC internal definitions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <rxrpc/packet.h>

#if 0
#define CHECK_SLAB_OKAY(X)				     \
	BUG_ON(atomic_read((X)) >> (sizeof(atomic_t) - 2) == \
	       (POISON_FREE << 8 | POISON_FREE))
#else
#define CHECK_SLAB_OKAY(X) do {} while(0)
#endif

#define FCRYPT_BSIZE 8
struct rxrpc_crypt {
	union {
		u8	x[FCRYPT_BSIZE];
		__be32	n[2];
	};
} __attribute__((aligned(8)));

#define rxrpc_queue_work(WS)	queue_work(rxrpc_workqueue, (WS))
#define rxrpc_queue_delayed_work(WS,D)	\
	queue_delayed_work(rxrpc_workqueue, (WS), (D))

#define rxrpc_queue_call(CALL)	rxrpc_queue_work(&(CALL)->processor)
#define rxrpc_queue_conn(CONN)	rxrpc_queue_work(&(CONN)->processor)

enum {
	RXRPC_UNCONNECTED = 0,
	RXRPC_CLIENT_BOUND,		
	RXRPC_CLIENT_CONNECTED,		
	RXRPC_SERVER_BOUND,		
	RXRPC_SERVER_LISTENING,		
	RXRPC_CLOSE,			
};

struct rxrpc_sock {
	
	struct sock		sk;
	rxrpc_interceptor_t	interceptor;	
	struct rxrpc_local	*local;		
	struct rxrpc_transport	*trans;		
	struct rxrpc_conn_bundle *bundle;	
	struct rxrpc_connection	*conn;		
	struct list_head	listen_link;	
	struct list_head	secureq;	
	struct list_head	acceptq;	
	struct key		*key;		
	struct key		*securities;	
	struct rb_root		calls;		
	unsigned long		flags;
#define RXRPC_SOCK_EXCLUSIVE_CONN	1	
	rwlock_t		call_lock;	
	u32			min_sec_level;	
#define RXRPC_SECURITY_MAX	RXRPC_SECURITY_ENCRYPT
	struct sockaddr_rxrpc	srx;		
	sa_family_t		proto;		
	__be16			service_id;	
};

#define rxrpc_sk(__sk) container_of((__sk), struct rxrpc_sock, sk)

struct rxrpc_skb_priv {
	struct rxrpc_call	*call;		
	unsigned long		resend_at;	
	union {
		unsigned	offset;		
		int		remain;		
		u32		error;		
		bool		need_resend;	
	};

	struct rxrpc_header	hdr;		
};

#define rxrpc_skb(__skb) ((struct rxrpc_skb_priv *) &(__skb)->cb)

enum rxrpc_command {
	RXRPC_CMD_SEND_DATA,		
	RXRPC_CMD_SEND_ABORT,		
	RXRPC_CMD_ACCEPT,		
	RXRPC_CMD_REJECT_BUSY,		
};

struct rxrpc_security {
	struct module		*owner;		
	struct list_head	link;		
	const char		*name;		
	u8			security_index;	

	
	int (*init_connection_security)(struct rxrpc_connection *);

	
	void (*prime_packet_security)(struct rxrpc_connection *);

	
	int (*secure_packet)(const struct rxrpc_call *,
			     struct sk_buff *,
			     size_t,
			     void *);

	
	int (*verify_packet)(const struct rxrpc_call *, struct sk_buff *,
			     u32 *);

	
	int (*issue_challenge)(struct rxrpc_connection *);

	
	int (*respond_to_challenge)(struct rxrpc_connection *,
				    struct sk_buff *,
				    u32 *);

	
	int (*verify_response)(struct rxrpc_connection *,
			       struct sk_buff *,
			       u32 *);

	
	void (*clear)(struct rxrpc_connection *);
};

struct rxrpc_local {
	struct socket		*socket;	
	struct work_struct	destroyer;	
	struct work_struct	acceptor;	
	struct work_struct	rejecter;	
	struct list_head	services;	
	struct list_head	link;		
	struct rw_semaphore	defrag_sem;	
	struct sk_buff_head	accept_queue;	
	struct sk_buff_head	reject_queue;	
	spinlock_t		lock;		
	rwlock_t		services_lock;	
	atomic_t		usage;
	int			debug_id;	
	volatile char		error_rcvd;	
	struct sockaddr_rxrpc	srx;		
};

struct rxrpc_peer {
	struct work_struct	destroyer;	
	struct list_head	link;		
	struct list_head	error_targets;	
	spinlock_t		lock;		
	atomic_t		usage;
	unsigned		if_mtu;		
	unsigned		mtu;		
	unsigned		maxdata;	
	unsigned short		hdrsize;	
	int			debug_id;	
	int			net_error;	
	struct sockaddr_rxrpc	srx;		

	
#define RXRPC_RTT_CACHE_SIZE 32
	suseconds_t		rtt;		
	unsigned		rtt_point;	
	unsigned		rtt_usage;	
	suseconds_t		rtt_cache[RXRPC_RTT_CACHE_SIZE]; 
};

struct rxrpc_transport {
	struct rxrpc_local	*local;		
	struct rxrpc_peer	*peer;		
	struct work_struct	error_handler;	
	struct rb_root		bundles;	
	struct rb_root		client_conns;	
	struct rb_root		server_conns;	
	struct list_head	link;		
	struct sk_buff_head	error_queue;	
	time_t			put_time;	
	spinlock_t		client_lock;	
	rwlock_t		conn_lock;	
	atomic_t		usage;
	int			debug_id;	
	unsigned int		conn_idcounter;	
};

struct rxrpc_conn_bundle {
	struct rb_node		node;		
	struct list_head	unused_conns;	
	struct list_head	avail_conns;	
	struct list_head	busy_conns;	
	struct key		*key;		
	wait_queue_head_t	chanwait;	
	atomic_t		usage;
	int			debug_id;	
	unsigned short		num_conns;	
	__be16			service_id;	
	u8			security_ix;	
};

struct rxrpc_connection {
	struct rxrpc_transport	*trans;		
	struct rxrpc_conn_bundle *bundle;	
	struct work_struct	processor;	
	struct rb_node		node;		
	struct list_head	link;		
	struct list_head	bundle_link;	
	struct rb_root		calls;		
	struct sk_buff_head	rx_queue;	
	struct rxrpc_call	*channels[RXRPC_MAXCALLS]; 
	struct rxrpc_security	*security;	
	struct key		*key;		
	struct key		*server_key;	
	struct crypto_blkcipher	*cipher;	
	struct rxrpc_crypt	csum_iv;	
	unsigned long		events;
#define RXRPC_CONN_CHALLENGE	0		
	time_t			put_time;	
	rwlock_t		lock;		
	spinlock_t		state_lock;	
	atomic_t		usage;
	u32			real_conn_id;	
	enum {					
		RXRPC_CONN_UNUSED,		
		RXRPC_CONN_CLIENT,		
		RXRPC_CONN_SERVER_UNSECURED,	
		RXRPC_CONN_SERVER_CHALLENGING,	
		RXRPC_CONN_SERVER,		
		RXRPC_CONN_REMOTELY_ABORTED,	
		RXRPC_CONN_LOCALLY_ABORTED,	
		RXRPC_CONN_NETWORK_ERROR,	
	} state;
	int			error;		
	int			debug_id;	
	unsigned		call_counter;	
	atomic_t		serial;		
	atomic_t		hi_serial;	
	u8			avail_calls;	
	u8			size_align;	
	u8			header_size;	
	u8			security_size;	
	u32			security_level;	
	u32			security_nonce;	

	
	__be32			epoch;		
	__be32			cid;		
	__be16			service_id;	
	u8			security_ix;	
	u8			in_clientflag;	
	u8			out_clientflag;	
};

struct rxrpc_call {
	struct rxrpc_connection	*conn;		
	struct rxrpc_sock	*socket;	
	struct timer_list	lifetimer;	
	struct timer_list	deadspan;	
	struct timer_list	ack_timer;	
	struct timer_list	resend_timer;	
	struct work_struct	destroyer;	
	struct work_struct	processor;	
	struct list_head	link;		
	struct list_head	error_link;	
	struct list_head	accept_link;	
	struct rb_node		sock_node;	
	struct rb_node		conn_node;	
	struct sk_buff_head	rx_queue;	
	struct sk_buff_head	rx_oos_queue;	
	struct sk_buff		*tx_pending;	
	wait_queue_head_t	tx_waitq;	
	unsigned long		user_call_ID;	
	unsigned long		creation_jif;	
	unsigned long		flags;
#define RXRPC_CALL_RELEASED	0	
#define RXRPC_CALL_TERMINAL_MSG	1	
#define RXRPC_CALL_RCVD_LAST	2	
#define RXRPC_CALL_RUN_RTIMER	3	
#define RXRPC_CALL_TX_SOFT_ACK	4	
#define RXRPC_CALL_PROC_BUSY	5	
#define RXRPC_CALL_INIT_ACCEPT	6	
#define RXRPC_CALL_HAS_USERID	7	
#define RXRPC_CALL_EXPECT_OOS	8	
	unsigned long		events;
#define RXRPC_CALL_RCVD_ACKALL	0	
#define RXRPC_CALL_RCVD_BUSY	1	
#define RXRPC_CALL_RCVD_ABORT	2	
#define RXRPC_CALL_RCVD_ERROR	3	
#define RXRPC_CALL_ACK_FINAL	4	
#define RXRPC_CALL_ACK		5	
#define RXRPC_CALL_REJECT_BUSY	6	
#define RXRPC_CALL_ABORT	7	
#define RXRPC_CALL_CONN_ABORT	8	
#define RXRPC_CALL_RESEND_TIMER	9	
#define RXRPC_CALL_RESEND	10	
#define RXRPC_CALL_DRAIN_RX_OOS	11	
#define RXRPC_CALL_LIFE_TIMER	12	
#define RXRPC_CALL_ACCEPTED	13	
#define RXRPC_CALL_SECURED	14	
#define RXRPC_CALL_POST_ACCEPT	15	
#define RXRPC_CALL_RELEASE	16	

	spinlock_t		lock;
	rwlock_t		state_lock;	
	atomic_t		usage;
	atomic_t		sequence;	
	u32			abort_code;	
	enum {					
		RXRPC_CALL_CLIENT_SEND_REQUEST,	
		RXRPC_CALL_CLIENT_AWAIT_REPLY,	
		RXRPC_CALL_CLIENT_RECV_REPLY,	
		RXRPC_CALL_CLIENT_FINAL_ACK,	
		RXRPC_CALL_SERVER_SECURING,	
		RXRPC_CALL_SERVER_ACCEPTING,	
		RXRPC_CALL_SERVER_RECV_REQUEST,	
		RXRPC_CALL_SERVER_ACK_REQUEST,	
		RXRPC_CALL_SERVER_SEND_REPLY,	
		RXRPC_CALL_SERVER_AWAIT_ACK,	
		RXRPC_CALL_COMPLETE,		
		RXRPC_CALL_SERVER_BUSY,		
		RXRPC_CALL_REMOTELY_ABORTED,	
		RXRPC_CALL_LOCALLY_ABORTED,	
		RXRPC_CALL_NETWORK_ERROR,	
		RXRPC_CALL_DEAD,		
	} state;
	int			debug_id;	
	u8			channel;	

	
	u8			acks_head;	
	u8			acks_tail;	
	u8			acks_winsz;	
	u8			acks_unacked;	
	int			acks_latest;	
	rxrpc_seq_t		acks_hard;	
	unsigned long		*acks_window;	

	
	rxrpc_seq_t		rx_data_expect;	
	rxrpc_seq_t		rx_data_post;	
	rxrpc_seq_t		rx_data_recv;	
	rxrpc_seq_t		rx_data_eaten;	
	rxrpc_seq_t		rx_first_oos;	
	rxrpc_seq_t		ackr_win_top;	
	rxrpc_seq_net_t		ackr_prev_seq;	
	u8			ackr_reason;	
	__be32			ackr_serial;	
	atomic_t		ackr_not_idle;	

	
#define RXRPC_ACKR_WINDOW_ASZ DIV_ROUND_UP(RXRPC_MAXACKS, BITS_PER_LONG)
	unsigned long		ackr_window[RXRPC_ACKR_WINDOW_ASZ + 1];

	
	__be32			cid;		
	__be32			call_id;	
};

static inline void rxrpc_abort_call(struct rxrpc_call *call, u32 abort_code)
{
	write_lock_bh(&call->state_lock);
	if (call->state < RXRPC_CALL_COMPLETE) {
		call->abort_code = abort_code;
		call->state = RXRPC_CALL_LOCALLY_ABORTED;
		set_bit(RXRPC_CALL_ABORT, &call->events);
	}
	write_unlock_bh(&call->state_lock);
}

extern atomic_t rxrpc_n_skbs;
extern __be32 rxrpc_epoch;
extern atomic_t rxrpc_debug_id;
extern struct workqueue_struct *rxrpc_workqueue;

extern void rxrpc_accept_incoming_calls(struct work_struct *);
extern struct rxrpc_call *rxrpc_accept_call(struct rxrpc_sock *,
					    unsigned long);
extern int rxrpc_reject_call(struct rxrpc_sock *);

extern void __rxrpc_propose_ACK(struct rxrpc_call *, u8, __be32, bool);
extern void rxrpc_propose_ACK(struct rxrpc_call *, u8, __be32, bool);
extern void rxrpc_process_call(struct work_struct *);

extern struct kmem_cache *rxrpc_call_jar;
extern struct list_head rxrpc_calls;
extern rwlock_t rxrpc_call_lock;

extern struct rxrpc_call *rxrpc_get_client_call(struct rxrpc_sock *,
						struct rxrpc_transport *,
						struct rxrpc_conn_bundle *,
						unsigned long, int, gfp_t);
extern struct rxrpc_call *rxrpc_incoming_call(struct rxrpc_sock *,
					      struct rxrpc_connection *,
					      struct rxrpc_header *, gfp_t);
extern struct rxrpc_call *rxrpc_find_server_call(struct rxrpc_sock *,
						 unsigned long);
extern void rxrpc_release_call(struct rxrpc_call *);
extern void rxrpc_release_calls_on_socket(struct rxrpc_sock *);
extern void __rxrpc_put_call(struct rxrpc_call *);
extern void __exit rxrpc_destroy_all_calls(void);

extern struct list_head rxrpc_connections;
extern rwlock_t rxrpc_connection_lock;

extern struct rxrpc_conn_bundle *rxrpc_get_bundle(struct rxrpc_sock *,
						  struct rxrpc_transport *,
						  struct key *,
						  __be16, gfp_t);
extern void rxrpc_put_bundle(struct rxrpc_transport *,
			     struct rxrpc_conn_bundle *);
extern int rxrpc_connect_call(struct rxrpc_sock *, struct rxrpc_transport *,
			      struct rxrpc_conn_bundle *, struct rxrpc_call *,
			      gfp_t);
extern void rxrpc_put_connection(struct rxrpc_connection *);
extern void __exit rxrpc_destroy_all_connections(void);
extern struct rxrpc_connection *rxrpc_find_connection(struct rxrpc_transport *,
						      struct rxrpc_header *);
extern struct rxrpc_connection *
rxrpc_incoming_connection(struct rxrpc_transport *, struct rxrpc_header *,
			  gfp_t);

extern void rxrpc_process_connection(struct work_struct *);
extern void rxrpc_reject_packet(struct rxrpc_local *, struct sk_buff *);
extern void rxrpc_reject_packets(struct work_struct *);

extern void rxrpc_UDP_error_report(struct sock *);
extern void rxrpc_UDP_error_handler(struct work_struct *);

extern unsigned long rxrpc_ack_timeout;
extern const char *rxrpc_pkts[];

extern void rxrpc_data_ready(struct sock *, int);
extern int rxrpc_queue_rcv_skb(struct rxrpc_call *, struct sk_buff *, bool,
			       bool);
extern void rxrpc_fast_process_packet(struct rxrpc_call *, struct sk_buff *);

extern rwlock_t rxrpc_local_lock;
extern struct rxrpc_local *rxrpc_lookup_local(struct sockaddr_rxrpc *);
extern void rxrpc_put_local(struct rxrpc_local *);
extern void __exit rxrpc_destroy_all_locals(void);

extern struct key_type key_type_rxrpc;
extern struct key_type key_type_rxrpc_s;

extern int rxrpc_request_key(struct rxrpc_sock *, char __user *, int);
extern int rxrpc_server_keyring(struct rxrpc_sock *, char __user *, int);
extern int rxrpc_get_server_data_key(struct rxrpc_connection *, const void *,
				     time_t, u32);

extern int rxrpc_resend_timeout;

extern int rxrpc_send_packet(struct rxrpc_transport *, struct sk_buff *);
extern int rxrpc_client_sendmsg(struct kiocb *, struct rxrpc_sock *,
				struct rxrpc_transport *, struct msghdr *,
				size_t);
extern int rxrpc_server_sendmsg(struct kiocb *, struct rxrpc_sock *,
				struct msghdr *, size_t);

extern struct rxrpc_peer *rxrpc_get_peer(struct sockaddr_rxrpc *, gfp_t);
extern void rxrpc_put_peer(struct rxrpc_peer *);
extern struct rxrpc_peer *rxrpc_find_peer(struct rxrpc_local *,
					  __be32, __be16);
extern void __exit rxrpc_destroy_all_peers(void);

extern const char *const rxrpc_call_states[];
extern const struct file_operations rxrpc_call_seq_fops;
extern const struct file_operations rxrpc_connection_seq_fops;

extern void rxrpc_remove_user_ID(struct rxrpc_sock *, struct rxrpc_call *);
extern int rxrpc_recvmsg(struct kiocb *, struct socket *, struct msghdr *,
			 size_t, int);

extern int rxrpc_register_security(struct rxrpc_security *);
extern void rxrpc_unregister_security(struct rxrpc_security *);
extern int rxrpc_init_client_conn_security(struct rxrpc_connection *);
extern int rxrpc_init_server_conn_security(struct rxrpc_connection *);
extern int rxrpc_secure_packet(const struct rxrpc_call *, struct sk_buff *,
			       size_t, void *);
extern int rxrpc_verify_packet(const struct rxrpc_call *, struct sk_buff *,
			       u32 *);
extern void rxrpc_clear_conn_security(struct rxrpc_connection *);

extern void rxrpc_packet_destructor(struct sk_buff *);

extern struct rxrpc_transport *rxrpc_get_transport(struct rxrpc_local *,
						   struct rxrpc_peer *,
						   gfp_t);
extern void rxrpc_put_transport(struct rxrpc_transport *);
extern void __exit rxrpc_destroy_all_transports(void);
extern struct rxrpc_transport *rxrpc_find_transport(struct rxrpc_local *,
						    struct rxrpc_peer *);

extern unsigned rxrpc_debug;

#define dbgprintk(FMT,...) \
	printk("[%-6.6s] "FMT"\n", current->comm ,##__VA_ARGS__)

#define kenter(FMT,...)	dbgprintk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define kleave(FMT,...)	dbgprintk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define kdebug(FMT,...)	dbgprintk("    "FMT ,##__VA_ARGS__)
#define kproto(FMT,...)	dbgprintk("### "FMT ,##__VA_ARGS__)
#define knet(FMT,...)	dbgprintk("@@@ "FMT ,##__VA_ARGS__)


#if defined(__KDEBUG)
#define _enter(FMT,...)	kenter(FMT,##__VA_ARGS__)
#define _leave(FMT,...)	kleave(FMT,##__VA_ARGS__)
#define _debug(FMT,...)	kdebug(FMT,##__VA_ARGS__)
#define _proto(FMT,...)	kproto(FMT,##__VA_ARGS__)
#define _net(FMT,...)	knet(FMT,##__VA_ARGS__)

#elif defined(CONFIG_AF_RXRPC_DEBUG)
#define RXRPC_DEBUG_KENTER	0x01
#define RXRPC_DEBUG_KLEAVE	0x02
#define RXRPC_DEBUG_KDEBUG	0x04
#define RXRPC_DEBUG_KPROTO	0x08
#define RXRPC_DEBUG_KNET	0x10

#define _enter(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KENTER))	\
		kenter(FMT,##__VA_ARGS__);		\
} while (0)

#define _leave(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KLEAVE))	\
		kleave(FMT,##__VA_ARGS__);		\
} while (0)

#define _debug(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KDEBUG))	\
		kdebug(FMT,##__VA_ARGS__);		\
} while (0)

#define _proto(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KPROTO))	\
		kproto(FMT,##__VA_ARGS__);		\
} while (0)

#define _net(FMT,...)					\
do {							\
	if (unlikely(rxrpc_debug & RXRPC_DEBUG_KNET))	\
		knet(FMT,##__VA_ARGS__);		\
} while (0)

#else
#define _enter(FMT,...)	no_printk("==> %s("FMT")",__func__ ,##__VA_ARGS__)
#define _leave(FMT,...)	no_printk("<== %s()"FMT"",__func__ ,##__VA_ARGS__)
#define _debug(FMT,...)	no_printk("    "FMT ,##__VA_ARGS__)
#define _proto(FMT,...)	no_printk("### "FMT ,##__VA_ARGS__)
#define _net(FMT,...)	no_printk("@@@ "FMT ,##__VA_ARGS__)
#endif

#if 1 

#define ASSERT(X)						\
do {								\
	if (unlikely(!(X))) {					\
		printk(KERN_ERR "\n");				\
		printk(KERN_ERR "RxRPC: Assertion failed\n");	\
		BUG();						\
	}							\
} while(0)

#define ASSERTCMP(X, OP, Y)						\
do {									\
	if (unlikely(!((X) OP (Y)))) {					\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "RxRPC: Assertion failed\n");		\
		printk(KERN_ERR "%lu " #OP " %lu is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		printk(KERN_ERR "0x%lx " #OP " 0x%lx is false\n",	\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while(0)

#define ASSERTIF(C, X)						\
do {								\
	if (unlikely((C) && !(X))) {				\
		printk(KERN_ERR "\n");				\
		printk(KERN_ERR "RxRPC: Assertion failed\n");	\
		BUG();						\
	}							\
} while(0)

#define ASSERTIFCMP(C, X, OP, Y)					\
do {									\
	if (unlikely((C) && !((X) OP (Y)))) {				\
		printk(KERN_ERR "\n");					\
		printk(KERN_ERR "RxRPC: Assertion failed\n");		\
		printk(KERN_ERR "%lu " #OP " %lu is false\n",		\
		       (unsigned long)(X), (unsigned long)(Y));		\
		printk(KERN_ERR "0x%lx " #OP " 0x%lx is false\n",	\
		       (unsigned long)(X), (unsigned long)(Y));		\
		BUG();							\
	}								\
} while(0)

#else

#define ASSERT(X)				\
do {						\
} while(0)

#define ASSERTCMP(X, OP, Y)			\
do {						\
} while(0)

#define ASSERTIF(C, X)				\
do {						\
} while(0)

#define ASSERTIFCMP(C, X, OP, Y)		\
do {						\
} while(0)

#endif 

static inline void __rxrpc_new_skb(struct sk_buff *skb, const char *fn)
{
	
	
}

#define rxrpc_new_skb(skb) __rxrpc_new_skb((skb), __func__)

static inline void __rxrpc_kill_skb(struct sk_buff *skb, const char *fn)
{
	
	
}

#define rxrpc_kill_skb(skb) __rxrpc_kill_skb((skb), __func__)

static inline void __rxrpc_free_skb(struct sk_buff *skb, const char *fn)
{
	if (skb) {
		CHECK_SLAB_OKAY(&skb->users);
		
		
		
		kfree_skb(skb);
	}
}

#define rxrpc_free_skb(skb) __rxrpc_free_skb((skb), __func__)

static inline void rxrpc_purge_queue(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb = skb_dequeue((list))) != NULL)
		rxrpc_free_skb(skb);
}

static inline void __rxrpc_get_local(struct rxrpc_local *local, const char *f)
{
	CHECK_SLAB_OKAY(&local->usage);
	if (atomic_inc_return(&local->usage) == 1)
		printk("resurrected (%s)\n", f);
}

#define rxrpc_get_local(LOCAL) __rxrpc_get_local((LOCAL), __func__)

#define rxrpc_get_call(CALL)				\
do {							\
	CHECK_SLAB_OKAY(&(CALL)->usage);		\
	if (atomic_inc_return(&(CALL)->usage) == 1)	\
		BUG();					\
} while(0)

#define rxrpc_put_call(CALL)				\
do {							\
	__rxrpc_put_call(CALL);				\
} while(0)
