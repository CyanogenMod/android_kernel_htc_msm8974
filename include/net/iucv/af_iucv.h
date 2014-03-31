/*
 * Copyright 2006 IBM Corporation
 * IUCV protocol stack for Linux on zSeries
 * Version 1.0
 * Author(s): Jennifer Hunt <jenhunt@us.ibm.com>
 *
 */

#ifndef __AFIUCV_H
#define __AFIUCV_H

#include <asm/types.h>
#include <asm/byteorder.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/socket.h>
#include <net/iucv/iucv.h>

#ifndef AF_IUCV
#define AF_IUCV		32
#define PF_IUCV		AF_IUCV
#endif

enum {
	IUCV_CONNECTED = 1,
	IUCV_OPEN,
	IUCV_BOUND,
	IUCV_LISTEN,
	IUCV_DISCONN,
	IUCV_CLOSING,
	IUCV_CLOSED
};

#define IUCV_QUEUELEN_DEFAULT	65535
#define IUCV_HIPER_MSGLIM_DEFAULT	128
#define IUCV_CONN_TIMEOUT	(HZ * 40)
#define IUCV_DISCONN_TIMEOUT	(HZ * 2)
#define IUCV_CONN_IDLE_TIMEOUT	(HZ * 60)
#define IUCV_BUFSIZE_DEFAULT	32768

struct sockaddr_iucv {
	sa_family_t	siucv_family;
	unsigned short	siucv_port;		
	unsigned int	siucv_addr;		
	char		siucv_nodeid[8];	
	char		siucv_user_id[8];	
	char		siucv_name[8];		
};


struct sock_msg_q {
	struct iucv_path	*path;
	struct iucv_message	msg;
	struct list_head	list;
	spinlock_t		lock;
};

#define AF_IUCV_FLAG_ACK 0x1
#define AF_IUCV_FLAG_SYN 0x2
#define AF_IUCV_FLAG_FIN 0x4
#define AF_IUCV_FLAG_WIN 0x8
#define AF_IUCV_FLAG_SHT 0x10

struct af_iucv_trans_hdr {
	u16 magic;
	u8 version;
	u8 flags;
	u16 window;
	char destNodeID[8];
	char destUserID[8];
	char destAppName[16];
	char srcNodeID[8];
	char srcUserID[8];
	char srcAppName[16];             
	struct iucv_message iucv_hdr;    
	u8 pad;                          
} __packed;

enum iucv_tx_notify {
	
	TX_NOTIFY_OK = 0,
	
	TX_NOTIFY_UNREACHABLE = 1,
	
	TX_NOTIFY_TPQFULL = 2,
	
	TX_NOTIFY_GENERALERROR = 3,
	TX_NOTIFY_PENDING = 4,
	
	TX_NOTIFY_DELAYED_OK = 5,
	
	TX_NOTIFY_DELAYED_UNREACHABLE = 6,
	
	TX_NOTIFY_DELAYED_GENERALERROR = 7,
};

#define iucv_sk(__sk) ((struct iucv_sock *) __sk)

#define AF_IUCV_TRANS_IUCV 0
#define AF_IUCV_TRANS_HIPER 1

struct iucv_sock {
	struct sock		sk;
	char			src_user_id[8];
	char			src_name[8];
	char			dst_user_id[8];
	char			dst_name[8];
	struct list_head	accept_q;
	spinlock_t		accept_q_lock;
	struct sock		*parent;
	struct iucv_path	*path;
	struct net_device	*hs_dev;
	struct sk_buff_head	send_skb_q;
	struct sk_buff_head	backlog_skb_q;
	struct sock_msg_q	message_q;
	unsigned int		send_tag;
	u8			flags;
	u16			msglimit;
	u16			msglimit_peer;
	atomic_t		msg_sent;
	atomic_t		msg_recv;
	atomic_t		pendings;
	int			transport;
	void                    (*sk_txnotify)(struct sk_buff *skb,
					       enum iucv_tx_notify n);
};

#define SO_IPRMDATA_MSG	0x0080		
#define SO_MSGLIMIT	0x1000		
#define SO_MSGSIZE	0x0800		

#define SCM_IUCV_TRGCLS	0x0001		

struct iucv_sock_list {
	struct hlist_head head;
	rwlock_t	  lock;
	atomic_t	  autobind_name;
};

unsigned int iucv_sock_poll(struct file *file, struct socket *sock,
			    poll_table *wait);
void iucv_sock_link(struct iucv_sock_list *l, struct sock *s);
void iucv_sock_unlink(struct iucv_sock_list *l, struct sock *s);
void iucv_accept_enqueue(struct sock *parent, struct sock *sk);
void iucv_accept_unlink(struct sock *sk);
struct sock *iucv_accept_dequeue(struct sock *parent, struct socket *newsock);

#endif 
