
#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>
#include <linux/net.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/igmp.h>                 
#include <linux/udp.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/kernel.h>

#include <asm/unaligned.h>		

#include <net/ip.h>
#include <net/sock.h>

#include <net/ip_vs.h>

#define IP_VS_SYNC_GROUP 0xe0000051    
#define IP_VS_SYNC_PORT  8848          

#define SYNC_PROTO_VER  1		

static struct lock_class_key __ipvs_sync_key;
struct ip_vs_sync_conn_v0 {
	__u8			reserved;

	
	__u8			protocol;       
	__be16			cport;
	__be16                  vport;
	__be16                  dport;
	__be32                  caddr;          
	__be32                  vaddr;          
	__be32                  daddr;          

	
	__be16                  flags;          
	__be16                  state;          

	
};

struct ip_vs_sync_conn_options {
	struct ip_vs_seq        in_seq;         
	struct ip_vs_seq        out_seq;        
};


struct ip_vs_sync_v4 {
	__u8			type;
	__u8			protocol;	
	__be16			ver_size;	
	
	__be32			flags;		
	__be16			state;		
	
	__be16			cport;
	__be16			vport;
	__be16			dport;
	__be32			fwmark;		
	__be32			timeout;	
	__be32			caddr;		
	__be32			vaddr;		
	__be32			daddr;		
	
	
};
struct ip_vs_sync_v6 {
	__u8			type;
	__u8			protocol;	
	__be16			ver_size;	
	
	__be32			flags;		
	__be16			state;		
	
	__be16			cport;
	__be16			vport;
	__be16			dport;
	__be32			fwmark;		
	__be32			timeout;	
	struct in6_addr		caddr;		
	struct in6_addr		vaddr;		
	struct in6_addr		daddr;		
	
	
};

union ip_vs_sync_conn {
	struct ip_vs_sync_v4	v4;
	struct ip_vs_sync_v6	v6;
};

#define STYPE_INET6		0
#define STYPE_F_INET6		(1 << STYPE_INET6)

#define SVER_SHIFT		12		
#define SVER_MASK		0x0fff		

#define IPVS_OPT_SEQ_DATA	1
#define IPVS_OPT_PE_DATA	2
#define IPVS_OPT_PE_NAME	3
#define IPVS_OPT_PARAM		7

#define IPVS_OPT_F_SEQ_DATA	(1 << (IPVS_OPT_SEQ_DATA-1))
#define IPVS_OPT_F_PE_DATA	(1 << (IPVS_OPT_PE_DATA-1))
#define IPVS_OPT_F_PE_NAME	(1 << (IPVS_OPT_PE_NAME-1))
#define IPVS_OPT_F_PARAM	(1 << (IPVS_OPT_PARAM-1))

struct ip_vs_sync_thread_data {
	struct net *net;
	struct socket *sock;
	char *buf;
};

#define SIMPLE_CONN_SIZE  (sizeof(struct ip_vs_sync_conn_v0))
#define FULL_CONN_SIZE  \
(sizeof(struct ip_vs_sync_conn_v0) + sizeof(struct ip_vs_sync_conn_options))



#define SYNC_MESG_HEADER_LEN	4
#define MAX_CONNS_PER_SYNCBUFF	255 

struct ip_vs_sync_mesg_v0 {
	__u8                    nr_conns;
	__u8                    syncid;
	__u16                   size;

	
};

struct ip_vs_sync_mesg {
	__u8			reserved;	
	__u8			syncid;
	__u16			size;
	__u8			nr_conns;
	__s8			version;	
	__u16			spare;
	
};

struct ip_vs_sync_buff {
	struct list_head        list;
	unsigned long           firstuse;

	
	struct ip_vs_sync_mesg  *mesg;
	unsigned char           *head;
	unsigned char           *end;
};

static struct sockaddr_in mcast_addr = {
	.sin_family		= AF_INET,
	.sin_port		= cpu_to_be16(IP_VS_SYNC_PORT),
	.sin_addr.s_addr	= cpu_to_be32(IP_VS_SYNC_GROUP),
};

static void ntoh_seq(struct ip_vs_seq *no, struct ip_vs_seq *ho)
{
	ho->init_seq       = get_unaligned_be32(&no->init_seq);
	ho->delta          = get_unaligned_be32(&no->delta);
	ho->previous_delta = get_unaligned_be32(&no->previous_delta);
}

static void hton_seq(struct ip_vs_seq *ho, struct ip_vs_seq *no)
{
	put_unaligned_be32(ho->init_seq, &no->init_seq);
	put_unaligned_be32(ho->delta, &no->delta);
	put_unaligned_be32(ho->previous_delta, &no->previous_delta);
}

static inline struct ip_vs_sync_buff *sb_dequeue(struct netns_ipvs *ipvs)
{
	struct ip_vs_sync_buff *sb;

	spin_lock_bh(&ipvs->sync_lock);
	if (list_empty(&ipvs->sync_queue)) {
		sb = NULL;
	} else {
		sb = list_entry(ipvs->sync_queue.next,
				struct ip_vs_sync_buff,
				list);
		list_del(&sb->list);
	}
	spin_unlock_bh(&ipvs->sync_lock);

	return sb;
}

static inline struct ip_vs_sync_buff *
ip_vs_sync_buff_create(struct netns_ipvs *ipvs)
{
	struct ip_vs_sync_buff *sb;

	if (!(sb=kmalloc(sizeof(struct ip_vs_sync_buff), GFP_ATOMIC)))
		return NULL;

	sb->mesg = kmalloc(ipvs->send_mesg_maxlen, GFP_ATOMIC);
	if (!sb->mesg) {
		kfree(sb);
		return NULL;
	}
	sb->mesg->reserved = 0;  
	sb->mesg->version = SYNC_PROTO_VER;
	sb->mesg->syncid = ipvs->master_syncid;
	sb->mesg->size = sizeof(struct ip_vs_sync_mesg);
	sb->mesg->nr_conns = 0;
	sb->mesg->spare = 0;
	sb->head = (unsigned char *)sb->mesg + sizeof(struct ip_vs_sync_mesg);
	sb->end = (unsigned char *)sb->mesg + ipvs->send_mesg_maxlen;

	sb->firstuse = jiffies;
	return sb;
}

static inline void ip_vs_sync_buff_release(struct ip_vs_sync_buff *sb)
{
	kfree(sb->mesg);
	kfree(sb);
}

static inline void sb_queue_tail(struct netns_ipvs *ipvs)
{
	struct ip_vs_sync_buff *sb = ipvs->sync_buff;

	spin_lock(&ipvs->sync_lock);
	if (ipvs->sync_state & IP_VS_STATE_MASTER)
		list_add_tail(&sb->list, &ipvs->sync_queue);
	else
		ip_vs_sync_buff_release(sb);
	spin_unlock(&ipvs->sync_lock);
}

static inline struct ip_vs_sync_buff *
get_curr_sync_buff(struct netns_ipvs *ipvs, unsigned long time)
{
	struct ip_vs_sync_buff *sb;

	spin_lock_bh(&ipvs->sync_buff_lock);
	if (ipvs->sync_buff &&
	    time_after_eq(jiffies - ipvs->sync_buff->firstuse, time)) {
		sb = ipvs->sync_buff;
		ipvs->sync_buff = NULL;
	} else
		sb = NULL;
	spin_unlock_bh(&ipvs->sync_buff_lock);
	return sb;
}

void ip_vs_sync_switch_mode(struct net *net, int mode)
{
	struct netns_ipvs *ipvs = net_ipvs(net);

	if (!(ipvs->sync_state & IP_VS_STATE_MASTER))
		return;
	if (mode == sysctl_sync_ver(ipvs) || !ipvs->sync_buff)
		return;

	spin_lock_bh(&ipvs->sync_buff_lock);
	
	if (ipvs->sync_buff->mesg->size <=  sizeof(struct ip_vs_sync_mesg)) {
		kfree(ipvs->sync_buff);
		ipvs->sync_buff = NULL;
	} else {
		spin_lock_bh(&ipvs->sync_lock);
		if (ipvs->sync_state & IP_VS_STATE_MASTER)
			list_add_tail(&ipvs->sync_buff->list,
				      &ipvs->sync_queue);
		else
			ip_vs_sync_buff_release(ipvs->sync_buff);
		spin_unlock_bh(&ipvs->sync_lock);
	}
	spin_unlock_bh(&ipvs->sync_buff_lock);
}

static inline struct ip_vs_sync_buff *
ip_vs_sync_buff_create_v0(struct netns_ipvs *ipvs)
{
	struct ip_vs_sync_buff *sb;
	struct ip_vs_sync_mesg_v0 *mesg;

	if (!(sb=kmalloc(sizeof(struct ip_vs_sync_buff), GFP_ATOMIC)))
		return NULL;

	sb->mesg = kmalloc(ipvs->send_mesg_maxlen, GFP_ATOMIC);
	if (!sb->mesg) {
		kfree(sb);
		return NULL;
	}
	mesg = (struct ip_vs_sync_mesg_v0 *)sb->mesg;
	mesg->nr_conns = 0;
	mesg->syncid = ipvs->master_syncid;
	mesg->size = sizeof(struct ip_vs_sync_mesg_v0);
	sb->head = (unsigned char *)mesg + sizeof(struct ip_vs_sync_mesg_v0);
	sb->end = (unsigned char *)mesg + ipvs->send_mesg_maxlen;
	sb->firstuse = jiffies;
	return sb;
}

void ip_vs_sync_conn_v0(struct net *net, struct ip_vs_conn *cp)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct ip_vs_sync_mesg_v0 *m;
	struct ip_vs_sync_conn_v0 *s;
	int len;

	if (unlikely(cp->af != AF_INET))
		return;
	
	if (cp->flags & IP_VS_CONN_F_ONE_PACKET)
		return;

	spin_lock(&ipvs->sync_buff_lock);
	if (!ipvs->sync_buff) {
		ipvs->sync_buff =
			ip_vs_sync_buff_create_v0(ipvs);
		if (!ipvs->sync_buff) {
			spin_unlock(&ipvs->sync_buff_lock);
			pr_err("ip_vs_sync_buff_create failed.\n");
			return;
		}
	}

	len = (cp->flags & IP_VS_CONN_F_SEQ_MASK) ? FULL_CONN_SIZE :
		SIMPLE_CONN_SIZE;
	m = (struct ip_vs_sync_mesg_v0 *)ipvs->sync_buff->mesg;
	s = (struct ip_vs_sync_conn_v0 *)ipvs->sync_buff->head;

	
	s->reserved = 0;
	s->protocol = cp->protocol;
	s->cport = cp->cport;
	s->vport = cp->vport;
	s->dport = cp->dport;
	s->caddr = cp->caddr.ip;
	s->vaddr = cp->vaddr.ip;
	s->daddr = cp->daddr.ip;
	s->flags = htons(cp->flags & ~IP_VS_CONN_F_HASHED);
	s->state = htons(cp->state);
	if (cp->flags & IP_VS_CONN_F_SEQ_MASK) {
		struct ip_vs_sync_conn_options *opt =
			(struct ip_vs_sync_conn_options *)&s[1];
		memcpy(opt, &cp->in_seq, sizeof(*opt));
	}

	m->nr_conns++;
	m->size += len;
	ipvs->sync_buff->head += len;

	
	if (ipvs->sync_buff->head + FULL_CONN_SIZE > ipvs->sync_buff->end) {
		sb_queue_tail(ipvs);
		ipvs->sync_buff = NULL;
	}
	spin_unlock(&ipvs->sync_buff_lock);

	
	if (cp->control)
		ip_vs_sync_conn(net, cp->control);
}

void ip_vs_sync_conn(struct net *net, struct ip_vs_conn *cp)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct ip_vs_sync_mesg *m;
	union ip_vs_sync_conn *s;
	__u8 *p;
	unsigned int len, pe_name_len, pad;

	
	if (sysctl_sync_ver(ipvs) == 0) {
		ip_vs_sync_conn_v0(net, cp);
		return;
	}
	
	if (cp->flags & IP_VS_CONN_F_ONE_PACKET)
		goto control;
sloop:
	
	pe_name_len = 0;
	if (cp->pe_data_len) {
		if (!cp->pe_data || !cp->dest) {
			IP_VS_ERR_RL("SYNC, connection pe_data invalid\n");
			return;
		}
		pe_name_len = strnlen(cp->pe->name, IP_VS_PENAME_MAXLEN);
	}

	spin_lock(&ipvs->sync_buff_lock);

#ifdef CONFIG_IP_VS_IPV6
	if (cp->af == AF_INET6)
		len = sizeof(struct ip_vs_sync_v6);
	else
#endif
		len = sizeof(struct ip_vs_sync_v4);

	if (cp->flags & IP_VS_CONN_F_SEQ_MASK)
		len += sizeof(struct ip_vs_sync_conn_options) + 2;

	if (cp->pe_data_len)
		len += cp->pe_data_len + 2;	
	if (pe_name_len)
		len += pe_name_len + 2;

	
	pad = 0;
	if (ipvs->sync_buff) {
		pad = (4 - (size_t)ipvs->sync_buff->head) & 3;
		if (ipvs->sync_buff->head + len + pad > ipvs->sync_buff->end) {
			sb_queue_tail(ipvs);
			ipvs->sync_buff = NULL;
			pad = 0;
		}
	}

	if (!ipvs->sync_buff) {
		ipvs->sync_buff = ip_vs_sync_buff_create(ipvs);
		if (!ipvs->sync_buff) {
			spin_unlock(&ipvs->sync_buff_lock);
			pr_err("ip_vs_sync_buff_create failed.\n");
			return;
		}
	}

	m = ipvs->sync_buff->mesg;
	p = ipvs->sync_buff->head;
	ipvs->sync_buff->head += pad + len;
	m->size += pad + len;
	
	while (pad--)
		*(p++) = 0;

	s = (union ip_vs_sync_conn *)p;

	
	s->v4.type = (cp->af == AF_INET6 ? STYPE_F_INET6 : 0);
	s->v4.ver_size = htons(len & SVER_MASK);	
	s->v4.flags = htonl(cp->flags & ~IP_VS_CONN_F_HASHED);
	s->v4.state = htons(cp->state);
	s->v4.protocol = cp->protocol;
	s->v4.cport = cp->cport;
	s->v4.vport = cp->vport;
	s->v4.dport = cp->dport;
	s->v4.fwmark = htonl(cp->fwmark);
	s->v4.timeout = htonl(cp->timeout / HZ);
	m->nr_conns++;

#ifdef CONFIG_IP_VS_IPV6
	if (cp->af == AF_INET6) {
		p += sizeof(struct ip_vs_sync_v6);
		s->v6.caddr = cp->caddr.in6;
		s->v6.vaddr = cp->vaddr.in6;
		s->v6.daddr = cp->daddr.in6;
	} else
#endif
	{
		p += sizeof(struct ip_vs_sync_v4);	
		s->v4.caddr = cp->caddr.ip;
		s->v4.vaddr = cp->vaddr.ip;
		s->v4.daddr = cp->daddr.ip;
	}
	if (cp->flags & IP_VS_CONN_F_SEQ_MASK) {
		*(p++) = IPVS_OPT_SEQ_DATA;
		*(p++) = sizeof(struct ip_vs_sync_conn_options);
		hton_seq((struct ip_vs_seq *)p, &cp->in_seq);
		p += sizeof(struct ip_vs_seq);
		hton_seq((struct ip_vs_seq *)p, &cp->out_seq);
		p += sizeof(struct ip_vs_seq);
	}
	
	if (cp->pe_data_len && cp->pe_data) {
		*(p++) = IPVS_OPT_PE_DATA;
		*(p++) = cp->pe_data_len;
		memcpy(p, cp->pe_data, cp->pe_data_len);
		p += cp->pe_data_len;
		if (pe_name_len) {
			
			*(p++) = IPVS_OPT_PE_NAME;
			*(p++) = pe_name_len;
			memcpy(p, cp->pe->name, pe_name_len);
			p += pe_name_len;
		}
	}

	spin_unlock(&ipvs->sync_buff_lock);

control:
	
	cp = cp->control;
	if (!cp)
		return;
	if (cp->flags & IP_VS_CONN_F_TEMPLATE) {
		int pkts = atomic_add_return(1, &cp->in_pkts);

		if (pkts % sysctl_sync_period(ipvs) != 1)
			return;
	}
	goto sloop;
}

static inline int
ip_vs_conn_fill_param_sync(struct net *net, int af, union ip_vs_sync_conn *sc,
			   struct ip_vs_conn_param *p,
			   __u8 *pe_data, unsigned int pe_data_len,
			   __u8 *pe_name, unsigned int pe_name_len)
{
#ifdef CONFIG_IP_VS_IPV6
	if (af == AF_INET6)
		ip_vs_conn_fill_param(net, af, sc->v6.protocol,
				      (const union nf_inet_addr *)&sc->v6.caddr,
				      sc->v6.cport,
				      (const union nf_inet_addr *)&sc->v6.vaddr,
				      sc->v6.vport, p);
	else
#endif
		ip_vs_conn_fill_param(net, af, sc->v4.protocol,
				      (const union nf_inet_addr *)&sc->v4.caddr,
				      sc->v4.cport,
				      (const union nf_inet_addr *)&sc->v4.vaddr,
				      sc->v4.vport, p);
	
	if (pe_data_len) {
		if (pe_name_len) {
			char buff[IP_VS_PENAME_MAXLEN+1];

			memcpy(buff, pe_name, pe_name_len);
			buff[pe_name_len]=0;
			p->pe = __ip_vs_pe_getbyname(buff);
			if (!p->pe) {
				IP_VS_DBG(3, "BACKUP, no %s engine found/loaded\n",
					     buff);
				return 1;
			}
		} else {
			IP_VS_ERR_RL("BACKUP, Invalid PE parameters\n");
			return 1;
		}

		p->pe_data = kmemdup(pe_data, pe_data_len, GFP_ATOMIC);
		if (!p->pe_data) {
			if (p->pe->module)
				module_put(p->pe->module);
			return -ENOMEM;
		}
		p->pe_data_len = pe_data_len;
	}
	return 0;
}

static void ip_vs_proc_conn(struct net *net, struct ip_vs_conn_param *param,
			    unsigned int flags, unsigned int state,
			    unsigned int protocol, unsigned int type,
			    const union nf_inet_addr *daddr, __be16 dport,
			    unsigned long timeout, __u32 fwmark,
			    struct ip_vs_sync_conn_options *opt)
{
	struct ip_vs_dest *dest;
	struct ip_vs_conn *cp;
	struct netns_ipvs *ipvs = net_ipvs(net);

	if (!(flags & IP_VS_CONN_F_TEMPLATE))
		cp = ip_vs_conn_in_get(param);
	else
		cp = ip_vs_ct_in_get(param);

	if (cp && param->pe_data) 	
		kfree(param->pe_data);
	if (!cp) {
		dest = ip_vs_find_dest(net, type, daddr, dport, param->vaddr,
				       param->vport, protocol, fwmark, flags);

		
		if (protocol == IPPROTO_TCP) {
			if (state != IP_VS_TCP_S_ESTABLISHED)
				flags |= IP_VS_CONN_F_INACTIVE;
			else
				flags &= ~IP_VS_CONN_F_INACTIVE;
		} else if (protocol == IPPROTO_SCTP) {
			if (state != IP_VS_SCTP_S_ESTABLISHED)
				flags |= IP_VS_CONN_F_INACTIVE;
			else
				flags &= ~IP_VS_CONN_F_INACTIVE;
		}
		cp = ip_vs_conn_new(param, daddr, dport, flags, dest, fwmark);
		if (dest)
			atomic_dec(&dest->refcnt);
		if (!cp) {
			if (param->pe_data)
				kfree(param->pe_data);
			IP_VS_DBG(2, "BACKUP, add new conn. failed\n");
			return;
		}
	} else if (!cp->dest) {
		dest = ip_vs_try_bind_dest(cp);
		if (dest)
			atomic_dec(&dest->refcnt);
	} else if ((cp->dest) && (cp->protocol == IPPROTO_TCP) &&
		(cp->state != state)) {
		
		dest = cp->dest;
		if (!(cp->flags & IP_VS_CONN_F_INACTIVE) &&
			(state != IP_VS_TCP_S_ESTABLISHED)) {
			atomic_dec(&dest->activeconns);
			atomic_inc(&dest->inactconns);
			cp->flags |= IP_VS_CONN_F_INACTIVE;
		} else if ((cp->flags & IP_VS_CONN_F_INACTIVE) &&
			(state == IP_VS_TCP_S_ESTABLISHED)) {
			atomic_inc(&dest->activeconns);
			atomic_dec(&dest->inactconns);
			cp->flags &= ~IP_VS_CONN_F_INACTIVE;
		}
	} else if ((cp->dest) && (cp->protocol == IPPROTO_SCTP) &&
		(cp->state != state)) {
		dest = cp->dest;
		if (!(cp->flags & IP_VS_CONN_F_INACTIVE) &&
		(state != IP_VS_SCTP_S_ESTABLISHED)) {
			atomic_dec(&dest->activeconns);
			atomic_inc(&dest->inactconns);
			cp->flags &= ~IP_VS_CONN_F_INACTIVE;
		}
	}

	if (opt)
		memcpy(&cp->in_seq, opt, sizeof(*opt));
	atomic_set(&cp->in_pkts, sysctl_sync_threshold(ipvs));
	cp->state = state;
	cp->old_state = cp->state;
	if (timeout) {
		if (timeout > MAX_SCHEDULE_TIMEOUT / HZ)
			timeout = MAX_SCHEDULE_TIMEOUT / HZ;
		cp->timeout = timeout*HZ;
	} else {
		struct ip_vs_proto_data *pd;

		pd = ip_vs_proto_data_get(net, protocol);
		if (!(flags & IP_VS_CONN_F_TEMPLATE) && pd && pd->timeout_table)
			cp->timeout = pd->timeout_table[state];
		else
			cp->timeout = (3*60*HZ);
	}
	ip_vs_conn_put(cp);
}

static void ip_vs_process_message_v0(struct net *net, const char *buffer,
				     const size_t buflen)
{
	struct ip_vs_sync_mesg_v0 *m = (struct ip_vs_sync_mesg_v0 *)buffer;
	struct ip_vs_sync_conn_v0 *s;
	struct ip_vs_sync_conn_options *opt;
	struct ip_vs_protocol *pp;
	struct ip_vs_conn_param param;
	char *p;
	int i;

	p = (char *)buffer + sizeof(struct ip_vs_sync_mesg_v0);
	for (i=0; i<m->nr_conns; i++) {
		unsigned flags, state;

		if (p + SIMPLE_CONN_SIZE > buffer+buflen) {
			IP_VS_ERR_RL("BACKUP v0, bogus conn\n");
			return;
		}
		s = (struct ip_vs_sync_conn_v0 *) p;
		flags = ntohs(s->flags) | IP_VS_CONN_F_SYNC;
		flags &= ~IP_VS_CONN_F_HASHED;
		if (flags & IP_VS_CONN_F_SEQ_MASK) {
			opt = (struct ip_vs_sync_conn_options *)&s[1];
			p += FULL_CONN_SIZE;
			if (p > buffer+buflen) {
				IP_VS_ERR_RL("BACKUP v0, Dropping buffer bogus conn options\n");
				return;
			}
		} else {
			opt = NULL;
			p += SIMPLE_CONN_SIZE;
		}

		state = ntohs(s->state);
		if (!(flags & IP_VS_CONN_F_TEMPLATE)) {
			pp = ip_vs_proto_get(s->protocol);
			if (!pp) {
				IP_VS_DBG(2, "BACKUP v0, Unsupported protocol %u\n",
					s->protocol);
				continue;
			}
			if (state >= pp->num_states) {
				IP_VS_DBG(2, "BACKUP v0, Invalid %s state %u\n",
					pp->name, state);
				continue;
			}
		} else {
			
			if (state > 0) {
				IP_VS_DBG(2, "BACKUP v0, Invalid template state %u\n",
					state);
				state = 0;
			}
		}

		ip_vs_conn_fill_param(net, AF_INET, s->protocol,
				      (const union nf_inet_addr *)&s->caddr,
				      s->cport,
				      (const union nf_inet_addr *)&s->vaddr,
				      s->vport, &param);

		
		ip_vs_proc_conn(net, &param, flags, state, s->protocol, AF_INET,
				(union nf_inet_addr *)&s->daddr, s->dport,
				0, 0, opt);
	}
}

static inline int ip_vs_proc_seqopt(__u8 *p, unsigned int plen,
				    __u32 *opt_flags,
				    struct ip_vs_sync_conn_options *opt)
{
	struct ip_vs_sync_conn_options *topt;

	topt = (struct ip_vs_sync_conn_options *)p;

	if (plen != sizeof(struct ip_vs_sync_conn_options)) {
		IP_VS_DBG(2, "BACKUP, bogus conn options length\n");
		return -EINVAL;
	}
	if (*opt_flags & IPVS_OPT_F_SEQ_DATA) {
		IP_VS_DBG(2, "BACKUP, conn options found twice\n");
		return -EINVAL;
	}
	ntoh_seq(&topt->in_seq, &opt->in_seq);
	ntoh_seq(&topt->out_seq, &opt->out_seq);
	*opt_flags |= IPVS_OPT_F_SEQ_DATA;
	return 0;
}

static int ip_vs_proc_str(__u8 *p, unsigned int plen, unsigned int *data_len,
			  __u8 **data, unsigned int maxlen,
			  __u32 *opt_flags, __u32 flag)
{
	if (plen > maxlen) {
		IP_VS_DBG(2, "BACKUP, bogus par.data len > %d\n", maxlen);
		return -EINVAL;
	}
	if (*opt_flags & flag) {
		IP_VS_DBG(2, "BACKUP, Par.data found twice 0x%x\n", flag);
		return -EINVAL;
	}
	*data_len = plen;
	*data = p;
	*opt_flags |= flag;
	return 0;
}
static inline int ip_vs_proc_sync_conn(struct net *net, __u8 *p, __u8 *msg_end)
{
	struct ip_vs_sync_conn_options opt;
	union  ip_vs_sync_conn *s;
	struct ip_vs_protocol *pp;
	struct ip_vs_conn_param param;
	__u32 flags;
	unsigned int af, state, pe_data_len=0, pe_name_len=0;
	__u8 *pe_data=NULL, *pe_name=NULL;
	__u32 opt_flags=0;
	int retc=0;

	s = (union ip_vs_sync_conn *) p;

	if (s->v6.type & STYPE_F_INET6) {
#ifdef CONFIG_IP_VS_IPV6
		af = AF_INET6;
		p += sizeof(struct ip_vs_sync_v6);
#else
		IP_VS_DBG(3,"BACKUP, IPv6 msg received, and IPVS is not compiled for IPv6\n");
		retc = 10;
		goto out;
#endif
	} else if (!s->v4.type) {
		af = AF_INET;
		p += sizeof(struct ip_vs_sync_v4);
	} else {
		return -10;
	}
	if (p > msg_end)
		return -20;

	
	while (p < msg_end) {
		int ptype;
		int plen;

		if (p+2 > msg_end)
			return -30;
		ptype = *(p++);
		plen  = *(p++);

		if (!plen || ((p + plen) > msg_end))
			return -40;
		
		switch (ptype & ~IPVS_OPT_F_PARAM) {
		case IPVS_OPT_SEQ_DATA:
			if (ip_vs_proc_seqopt(p, plen, &opt_flags, &opt))
				return -50;
			break;

		case IPVS_OPT_PE_DATA:
			if (ip_vs_proc_str(p, plen, &pe_data_len, &pe_data,
					   IP_VS_PEDATA_MAXLEN, &opt_flags,
					   IPVS_OPT_F_PE_DATA))
				return -60;
			break;

		case IPVS_OPT_PE_NAME:
			if (ip_vs_proc_str(p, plen,&pe_name_len, &pe_name,
					   IP_VS_PENAME_MAXLEN, &opt_flags,
					   IPVS_OPT_F_PE_NAME))
				return -70;
			break;

		default:
			
			if (!(ptype & IPVS_OPT_F_PARAM)) {
				IP_VS_DBG(3, "BACKUP, Unknown mandatory param %d found\n",
					  ptype & ~IPVS_OPT_F_PARAM);
				retc = 20;
				goto out;
			}
		}
		p += plen;  
	}

	
	flags  = ntohl(s->v4.flags) & IP_VS_CONN_F_BACKUP_MASK;
	flags |= IP_VS_CONN_F_SYNC;
	state = ntohs(s->v4.state);

	if (!(flags & IP_VS_CONN_F_TEMPLATE)) {
		pp = ip_vs_proto_get(s->v4.protocol);
		if (!pp) {
			IP_VS_DBG(3,"BACKUP, Unsupported protocol %u\n",
				s->v4.protocol);
			retc = 30;
			goto out;
		}
		if (state >= pp->num_states) {
			IP_VS_DBG(3, "BACKUP, Invalid %s state %u\n",
				pp->name, state);
			retc = 40;
			goto out;
		}
	} else {
		
		if (state > 0) {
			IP_VS_DBG(3, "BACKUP, Invalid template state %u\n",
				state);
			state = 0;
		}
	}
	if (ip_vs_conn_fill_param_sync(net, af, s, &param, pe_data,
				       pe_data_len, pe_name, pe_name_len)) {
		retc = 50;
		goto out;
	}
	
	if (af == AF_INET)
		ip_vs_proc_conn(net, &param, flags, state, s->v4.protocol, af,
				(union nf_inet_addr *)&s->v4.daddr, s->v4.dport,
				ntohl(s->v4.timeout), ntohl(s->v4.fwmark),
				(opt_flags & IPVS_OPT_F_SEQ_DATA ? &opt : NULL)
				);
#ifdef CONFIG_IP_VS_IPV6
	else
		ip_vs_proc_conn(net, &param, flags, state, s->v6.protocol, af,
				(union nf_inet_addr *)&s->v6.daddr, s->v6.dport,
				ntohl(s->v6.timeout), ntohl(s->v6.fwmark),
				(opt_flags & IPVS_OPT_F_SEQ_DATA ? &opt : NULL)
				);
#endif
	return 0;
	
out:
	IP_VS_DBG(2, "BACKUP, Single msg dropped err:%d\n", retc);
	return retc;

}
static void ip_vs_process_message(struct net *net, __u8 *buffer,
				  const size_t buflen)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct ip_vs_sync_mesg *m2 = (struct ip_vs_sync_mesg *)buffer;
	__u8 *p, *msg_end;
	int i, nr_conns;

	if (buflen < sizeof(struct ip_vs_sync_mesg_v0)) {
		IP_VS_DBG(2, "BACKUP, message header too short\n");
		return;
	}
	
	m2->size = ntohs(m2->size);

	if (buflen != m2->size) {
		IP_VS_DBG(2, "BACKUP, bogus message size\n");
		return;
	}
	
	if (ipvs->backup_syncid != 0 && m2->syncid != ipvs->backup_syncid) {
		IP_VS_DBG(7, "BACKUP, Ignoring syncid = %d\n", m2->syncid);
		return;
	}
	
	if ((m2->version == SYNC_PROTO_VER) && (m2->reserved == 0)
	    && (m2->spare == 0)) {

		msg_end = buffer + sizeof(struct ip_vs_sync_mesg);
		nr_conns = m2->nr_conns;

		for (i=0; i<nr_conns; i++) {
			union ip_vs_sync_conn *s;
			unsigned size;
			int retc;

			p = msg_end;
			if (p + sizeof(s->v4) > buffer+buflen) {
				IP_VS_ERR_RL("BACKUP, Dropping buffer, to small\n");
				return;
			}
			s = (union ip_vs_sync_conn *)p;
			size = ntohs(s->v4.ver_size) & SVER_MASK;
			msg_end = p + size;
			
			if (msg_end  > buffer+buflen) {
				IP_VS_ERR_RL("BACKUP, Dropping buffer, msg > buffer\n");
				return;
			}
			if (ntohs(s->v4.ver_size) >> SVER_SHIFT) {
				IP_VS_ERR_RL("BACKUP, Dropping buffer, Unknown version %d\n",
					      ntohs(s->v4.ver_size) >> SVER_SHIFT);
				return;
			}
			
			retc = ip_vs_proc_sync_conn(net, p, msg_end);
			if (retc < 0) {
				IP_VS_ERR_RL("BACKUP, Dropping buffer, Err: %d in decoding\n",
					     retc);
				return;
			}
			
			msg_end = p + ((size + 3) & ~3);
		}
	} else {
		
		ip_vs_process_message_v0(net, buffer, buflen);
		return;
	}
}


static void set_mcast_loop(struct sock *sk, u_char loop)
{
	struct inet_sock *inet = inet_sk(sk);

	
	lock_sock(sk);
	inet->mc_loop = loop ? 1 : 0;
	release_sock(sk);
}

static void set_mcast_ttl(struct sock *sk, u_char ttl)
{
	struct inet_sock *inet = inet_sk(sk);

	
	lock_sock(sk);
	inet->mc_ttl = ttl;
	release_sock(sk);
}

static int set_mcast_if(struct sock *sk, char *ifname)
{
	struct net_device *dev;
	struct inet_sock *inet = inet_sk(sk);
	struct net *net = sock_net(sk);

	dev = __dev_get_by_name(net, ifname);
	if (!dev)
		return -ENODEV;

	if (sk->sk_bound_dev_if && dev->ifindex != sk->sk_bound_dev_if)
		return -EINVAL;

	lock_sock(sk);
	inet->mc_index = dev->ifindex;
	
	release_sock(sk);

	return 0;
}


static int set_sync_mesg_maxlen(struct net *net, int sync_state)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct net_device *dev;
	int num;

	if (sync_state == IP_VS_STATE_MASTER) {
		dev = __dev_get_by_name(net, ipvs->master_mcast_ifn);
		if (!dev)
			return -ENODEV;

		num = (dev->mtu - sizeof(struct iphdr) -
		       sizeof(struct udphdr) -
		       SYNC_MESG_HEADER_LEN - 20) / SIMPLE_CONN_SIZE;
		ipvs->send_mesg_maxlen = SYNC_MESG_HEADER_LEN +
			SIMPLE_CONN_SIZE * min(num, MAX_CONNS_PER_SYNCBUFF);
		IP_VS_DBG(7, "setting the maximum length of sync sending "
			  "message %d.\n", ipvs->send_mesg_maxlen);
	} else if (sync_state == IP_VS_STATE_BACKUP) {
		dev = __dev_get_by_name(net, ipvs->backup_mcast_ifn);
		if (!dev)
			return -ENODEV;

		ipvs->recv_mesg_maxlen = dev->mtu -
			sizeof(struct iphdr) - sizeof(struct udphdr);
		IP_VS_DBG(7, "setting the maximum length of sync receiving "
			  "message %d.\n", ipvs->recv_mesg_maxlen);
	}

	return 0;
}


static int
join_mcast_group(struct sock *sk, struct in_addr *addr, char *ifname)
{
	struct net *net = sock_net(sk);
	struct ip_mreqn mreq;
	struct net_device *dev;
	int ret;

	memset(&mreq, 0, sizeof(mreq));
	memcpy(&mreq.imr_multiaddr, addr, sizeof(struct in_addr));

	dev = __dev_get_by_name(net, ifname);
	if (!dev)
		return -ENODEV;
	if (sk->sk_bound_dev_if && dev->ifindex != sk->sk_bound_dev_if)
		return -EINVAL;

	mreq.imr_ifindex = dev->ifindex;

	lock_sock(sk);
	ret = ip_mc_join_group(sk, &mreq);
	release_sock(sk);

	return ret;
}


static int bind_mcastif_addr(struct socket *sock, char *ifname)
{
	struct net *net = sock_net(sock->sk);
	struct net_device *dev;
	__be32 addr;
	struct sockaddr_in sin;

	dev = __dev_get_by_name(net, ifname);
	if (!dev)
		return -ENODEV;

	addr = inet_select_addr(dev, 0, RT_SCOPE_UNIVERSE);
	if (!addr)
		pr_err("You probably need to specify IP address on "
		       "multicast interface.\n");

	IP_VS_DBG(7, "binding socket with (%s) %pI4\n",
		  ifname, &addr);

	
	sin.sin_family	     = AF_INET;
	sin.sin_addr.s_addr  = addr;
	sin.sin_port         = 0;

	return sock->ops->bind(sock, (struct sockaddr*)&sin, sizeof(sin));
}

static struct socket *make_send_sock(struct net *net)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct socket *sock;
	int result;

	
	result = sock_create_kern(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
	if (result < 0) {
		pr_err("Error during creation of socket; terminating\n");
		return ERR_PTR(result);
	}
	sk_change_net(sock->sk, net);
	result = set_mcast_if(sock->sk, ipvs->master_mcast_ifn);
	if (result < 0) {
		pr_err("Error setting outbound mcast interface\n");
		goto error;
	}

	set_mcast_loop(sock->sk, 0);
	set_mcast_ttl(sock->sk, 1);

	result = bind_mcastif_addr(sock, ipvs->master_mcast_ifn);
	if (result < 0) {
		pr_err("Error binding address of the mcast interface\n");
		goto error;
	}

	result = sock->ops->connect(sock, (struct sockaddr *) &mcast_addr,
			sizeof(struct sockaddr), 0);
	if (result < 0) {
		pr_err("Error connecting to the multicast addr\n");
		goto error;
	}

	return sock;

error:
	sk_release_kernel(sock->sk);
	return ERR_PTR(result);
}


static struct socket *make_receive_sock(struct net *net)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct socket *sock;
	int result;

	
	result = sock_create_kern(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
	if (result < 0) {
		pr_err("Error during creation of socket; terminating\n");
		return ERR_PTR(result);
	}
	sk_change_net(sock->sk, net);
	
	sock->sk->sk_reuse = 1;

	result = sock->ops->bind(sock, (struct sockaddr *) &mcast_addr,
			sizeof(struct sockaddr));
	if (result < 0) {
		pr_err("Error binding to the multicast addr\n");
		goto error;
	}

	
	result = join_mcast_group(sock->sk,
			(struct in_addr *) &mcast_addr.sin_addr,
			ipvs->backup_mcast_ifn);
	if (result < 0) {
		pr_err("Error joining to the multicast group\n");
		goto error;
	}

	return sock;

error:
	sk_release_kernel(sock->sk);
	return ERR_PTR(result);
}


static int
ip_vs_send_async(struct socket *sock, const char *buffer, const size_t length)
{
	struct msghdr	msg = {.msg_flags = MSG_DONTWAIT|MSG_NOSIGNAL};
	struct kvec	iov;
	int		len;

	EnterFunction(7);
	iov.iov_base     = (void *)buffer;
	iov.iov_len      = length;

	len = kernel_sendmsg(sock, &msg, &iov, 1, (size_t)(length));

	LeaveFunction(7);
	return len;
}

static void
ip_vs_send_sync_msg(struct socket *sock, struct ip_vs_sync_mesg *msg)
{
	int msize;

	msize = msg->size;

	
	msg->size = htons(msg->size);

	if (ip_vs_send_async(sock, (char *)msg, msize) != msize)
		pr_err("ip_vs_send_async error\n");
}

static int
ip_vs_receive(struct socket *sock, char *buffer, const size_t buflen)
{
	struct msghdr		msg = {NULL,};
	struct kvec		iov;
	int			len;

	EnterFunction(7);

	
	iov.iov_base     = buffer;
	iov.iov_len      = (size_t)buflen;

	len = kernel_recvmsg(sock, &msg, &iov, 1, buflen, 0);

	if (len < 0)
		return -1;

	LeaveFunction(7);
	return len;
}


static int sync_thread_master(void *data)
{
	struct ip_vs_sync_thread_data *tinfo = data;
	struct netns_ipvs *ipvs = net_ipvs(tinfo->net);
	struct ip_vs_sync_buff *sb;

	pr_info("sync thread started: state = MASTER, mcast_ifn = %s, "
		"syncid = %d\n",
		ipvs->master_mcast_ifn, ipvs->master_syncid);

	while (!kthread_should_stop()) {
		while ((sb = sb_dequeue(ipvs))) {
			ip_vs_send_sync_msg(tinfo->sock, sb->mesg);
			ip_vs_sync_buff_release(sb);
		}

		
		sb = get_curr_sync_buff(ipvs, 2 * HZ);
		if (sb) {
			ip_vs_send_sync_msg(tinfo->sock, sb->mesg);
			ip_vs_sync_buff_release(sb);
		}

		schedule_timeout_interruptible(HZ);
	}

	
	while ((sb = sb_dequeue(ipvs)))
		ip_vs_sync_buff_release(sb);

	
	sb = get_curr_sync_buff(ipvs, 0);
	if (sb)
		ip_vs_sync_buff_release(sb);

	
	sk_release_kernel(tinfo->sock->sk);
	kfree(tinfo);

	return 0;
}


static int sync_thread_backup(void *data)
{
	struct ip_vs_sync_thread_data *tinfo = data;
	struct netns_ipvs *ipvs = net_ipvs(tinfo->net);
	int len;

	pr_info("sync thread started: state = BACKUP, mcast_ifn = %s, "
		"syncid = %d\n",
		ipvs->backup_mcast_ifn, ipvs->backup_syncid);

	while (!kthread_should_stop()) {
		wait_event_interruptible(*sk_sleep(tinfo->sock->sk),
			 !skb_queue_empty(&tinfo->sock->sk->sk_receive_queue)
			 || kthread_should_stop());

		
		while (!skb_queue_empty(&(tinfo->sock->sk->sk_receive_queue))) {
			len = ip_vs_receive(tinfo->sock, tinfo->buf,
					ipvs->recv_mesg_maxlen);
			if (len <= 0) {
				pr_err("receiving message error\n");
				break;
			}

			local_bh_disable();
			ip_vs_process_message(tinfo->net, tinfo->buf, len);
			local_bh_enable();
		}
	}

	
	sk_release_kernel(tinfo->sock->sk);
	kfree(tinfo->buf);
	kfree(tinfo);

	return 0;
}


int start_sync_thread(struct net *net, int state, char *mcast_ifn, __u8 syncid)
{
	struct ip_vs_sync_thread_data *tinfo;
	struct task_struct **realtask, *task;
	struct socket *sock;
	struct netns_ipvs *ipvs = net_ipvs(net);
	char *name, *buf = NULL;
	int (*threadfn)(void *data);
	int result = -ENOMEM;

	IP_VS_DBG(7, "%s(): pid %d\n", __func__, task_pid_nr(current));
	IP_VS_DBG(7, "Each ip_vs_sync_conn entry needs %Zd bytes\n",
		  sizeof(struct ip_vs_sync_conn_v0));


	if (state == IP_VS_STATE_MASTER) {
		if (ipvs->master_thread)
			return -EEXIST;

		strlcpy(ipvs->master_mcast_ifn, mcast_ifn,
			sizeof(ipvs->master_mcast_ifn));
		ipvs->master_syncid = syncid;
		realtask = &ipvs->master_thread;
		name = "ipvs_master:%d";
		threadfn = sync_thread_master;
		sock = make_send_sock(net);
	} else if (state == IP_VS_STATE_BACKUP) {
		if (ipvs->backup_thread)
			return -EEXIST;

		strlcpy(ipvs->backup_mcast_ifn, mcast_ifn,
			sizeof(ipvs->backup_mcast_ifn));
		ipvs->backup_syncid = syncid;
		realtask = &ipvs->backup_thread;
		name = "ipvs_backup:%d";
		threadfn = sync_thread_backup;
		sock = make_receive_sock(net);
	} else {
		return -EINVAL;
	}

	if (IS_ERR(sock)) {
		result = PTR_ERR(sock);
		goto out;
	}

	set_sync_mesg_maxlen(net, state);
	if (state == IP_VS_STATE_BACKUP) {
		buf = kmalloc(ipvs->recv_mesg_maxlen, GFP_KERNEL);
		if (!buf)
			goto outsocket;
	}

	tinfo = kmalloc(sizeof(*tinfo), GFP_KERNEL);
	if (!tinfo)
		goto outbuf;

	tinfo->net = net;
	tinfo->sock = sock;
	tinfo->buf = buf;

	task = kthread_run(threadfn, tinfo, name, ipvs->gen);
	if (IS_ERR(task)) {
		result = PTR_ERR(task);
		goto outtinfo;
	}

	
	*realtask = task;
	ipvs->sync_state |= state;

	
	ip_vs_use_count_inc();

	return 0;

outtinfo:
	kfree(tinfo);
outbuf:
	kfree(buf);
outsocket:
	sk_release_kernel(sock->sk);
out:
	return result;
}


int stop_sync_thread(struct net *net, int state)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	int retc = -EINVAL;

	IP_VS_DBG(7, "%s(): pid %d\n", __func__, task_pid_nr(current));

	if (state == IP_VS_STATE_MASTER) {
		if (!ipvs->master_thread)
			return -ESRCH;

		pr_info("stopping master sync thread %d ...\n",
			task_pid_nr(ipvs->master_thread));


		spin_lock_bh(&ipvs->sync_lock);
		ipvs->sync_state &= ~IP_VS_STATE_MASTER;
		spin_unlock_bh(&ipvs->sync_lock);
		retc = kthread_stop(ipvs->master_thread);
		ipvs->master_thread = NULL;
	} else if (state == IP_VS_STATE_BACKUP) {
		if (!ipvs->backup_thread)
			return -ESRCH;

		pr_info("stopping backup sync thread %d ...\n",
			task_pid_nr(ipvs->backup_thread));

		ipvs->sync_state &= ~IP_VS_STATE_BACKUP;
		retc = kthread_stop(ipvs->backup_thread);
		ipvs->backup_thread = NULL;
	}

	
	ip_vs_use_count_dec();

	return retc;
}

int __net_init ip_vs_sync_net_init(struct net *net)
{
	struct netns_ipvs *ipvs = net_ipvs(net);

	__mutex_init(&ipvs->sync_mutex, "ipvs->sync_mutex", &__ipvs_sync_key);
	INIT_LIST_HEAD(&ipvs->sync_queue);
	spin_lock_init(&ipvs->sync_lock);
	spin_lock_init(&ipvs->sync_buff_lock);

	ipvs->sync_mcast_addr.sin_family = AF_INET;
	ipvs->sync_mcast_addr.sin_port = cpu_to_be16(IP_VS_SYNC_PORT);
	ipvs->sync_mcast_addr.sin_addr.s_addr = cpu_to_be32(IP_VS_SYNC_GROUP);
	return 0;
}

void ip_vs_sync_net_cleanup(struct net *net)
{
	int retc;
	struct netns_ipvs *ipvs = net_ipvs(net);

	mutex_lock(&ipvs->sync_mutex);
	retc = stop_sync_thread(net, IP_VS_STATE_MASTER);
	if (retc && retc != -ESRCH)
		pr_err("Failed to stop Master Daemon\n");

	retc = stop_sync_thread(net, IP_VS_STATE_BACKUP);
	if (retc && retc != -ESRCH)
		pr_err("Failed to stop Backup Daemon\n");
	mutex_unlock(&ipvs->sync_mutex);
}
