#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/sctp.h>
#include <net/ip.h>
#include <net/ip6_checksum.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/sctp/checksum.h>
#include <net/ip_vs.h>

static int
sctp_conn_schedule(int af, struct sk_buff *skb, struct ip_vs_proto_data *pd,
		   int *verdict, struct ip_vs_conn **cpp)
{
	struct net *net;
	struct ip_vs_service *svc;
	sctp_chunkhdr_t _schunkh, *sch;
	sctp_sctphdr_t *sh, _sctph;
	struct ip_vs_iphdr iph;

	ip_vs_fill_iphdr(af, skb_network_header(skb), &iph);

	sh = skb_header_pointer(skb, iph.len, sizeof(_sctph), &_sctph);
	if (sh == NULL)
		return 0;

	sch = skb_header_pointer(skb, iph.len + sizeof(sctp_sctphdr_t),
				 sizeof(_schunkh), &_schunkh);
	if (sch == NULL)
		return 0;
	net = skb_net(skb);
	if ((sch->type == SCTP_CID_INIT) &&
	    (svc = ip_vs_service_get(net, af, skb->mark, iph.protocol,
				     &iph.daddr, sh->dest))) {
		int ignored;

		if (ip_vs_todrop(net_ipvs(net))) {
			ip_vs_service_put(svc);
			*verdict = NF_DROP;
			return 0;
		}
		*cpp = ip_vs_schedule(svc, skb, pd, &ignored);
		if (!*cpp && ignored <= 0) {
			if (!ignored)
				*verdict = ip_vs_leave(svc, skb, pd);
			else {
				ip_vs_service_put(svc);
				*verdict = NF_DROP;
			}
			return 0;
		}
		ip_vs_service_put(svc);
	}
	
	return 1;
}

static int
sctp_snat_handler(struct sk_buff *skb,
		  struct ip_vs_protocol *pp, struct ip_vs_conn *cp)
{
	sctp_sctphdr_t *sctph;
	unsigned int sctphoff;
	struct sk_buff *iter;
	__be32 crc32;

#ifdef CONFIG_IP_VS_IPV6
	if (cp->af == AF_INET6)
		sctphoff = sizeof(struct ipv6hdr);
	else
#endif
		sctphoff = ip_hdrlen(skb);

	
	if (!skb_make_writable(skb, sctphoff + sizeof(*sctph)))
		return 0;

	if (unlikely(cp->app != NULL)) {
		
		if (pp->csum_check && !pp->csum_check(cp->af, skb, pp))
			return 0;

		
		if (!ip_vs_app_pkt_out(cp, skb))
			return 0;
	}

	sctph = (void *) skb_network_header(skb) + sctphoff;
	sctph->source = cp->vport;

	
	crc32 = sctp_start_cksum((u8 *) sctph, skb_headlen(skb) - sctphoff);
	skb_walk_frags(skb, iter)
		crc32 = sctp_update_cksum((u8 *) iter->data, skb_headlen(iter),
				          crc32);
	crc32 = sctp_end_cksum(crc32);
	sctph->checksum = crc32;

	return 1;
}

static int
sctp_dnat_handler(struct sk_buff *skb,
		  struct ip_vs_protocol *pp, struct ip_vs_conn *cp)
{
	sctp_sctphdr_t *sctph;
	unsigned int sctphoff;
	struct sk_buff *iter;
	__be32 crc32;

#ifdef CONFIG_IP_VS_IPV6
	if (cp->af == AF_INET6)
		sctphoff = sizeof(struct ipv6hdr);
	else
#endif
		sctphoff = ip_hdrlen(skb);

	
	if (!skb_make_writable(skb, sctphoff + sizeof(*sctph)))
		return 0;

	if (unlikely(cp->app != NULL)) {
		
		if (pp->csum_check && !pp->csum_check(cp->af, skb, pp))
			return 0;

		
		if (!ip_vs_app_pkt_in(cp, skb))
			return 0;
	}

	sctph = (void *) skb_network_header(skb) + sctphoff;
	sctph->dest = cp->dport;

	
	crc32 = sctp_start_cksum((u8 *) sctph, skb_headlen(skb) - sctphoff);
	skb_walk_frags(skb, iter)
		crc32 = sctp_update_cksum((u8 *) iter->data, skb_headlen(iter),
					  crc32);
	crc32 = sctp_end_cksum(crc32);
	sctph->checksum = crc32;

	return 1;
}

static int
sctp_csum_check(int af, struct sk_buff *skb, struct ip_vs_protocol *pp)
{
	unsigned int sctphoff;
	struct sctphdr *sh, _sctph;
	struct sk_buff *iter;
	__le32 cmp;
	__le32 val;
	__u32 tmp;

#ifdef CONFIG_IP_VS_IPV6
	if (af == AF_INET6)
		sctphoff = sizeof(struct ipv6hdr);
	else
#endif
		sctphoff = ip_hdrlen(skb);

	sh = skb_header_pointer(skb, sctphoff, sizeof(_sctph), &_sctph);
	if (sh == NULL)
		return 0;

	cmp = sh->checksum;

	tmp = sctp_start_cksum((__u8 *) sh, skb_headlen(skb));
	skb_walk_frags(skb, iter)
		tmp = sctp_update_cksum((__u8 *) iter->data,
					skb_headlen(iter), tmp);

	val = sctp_end_cksum(tmp);

	if (val != cmp) {
		
		IP_VS_DBG_RL_PKT(0, af, pp, skb, 0,
				"Failed checksum for");
		return 0;
	}
	return 1;
}

struct ipvs_sctp_nextstate {
	int next_state;
};
enum ipvs_sctp_event_t {
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_DATA_SER,
	IP_VS_SCTP_EVE_INIT_CLI,
	IP_VS_SCTP_EVE_INIT_SER,
	IP_VS_SCTP_EVE_INIT_ACK_CLI,
	IP_VS_SCTP_EVE_INIT_ACK_SER,
	IP_VS_SCTP_EVE_COOKIE_ECHO_CLI,
	IP_VS_SCTP_EVE_COOKIE_ECHO_SER,
	IP_VS_SCTP_EVE_COOKIE_ACK_CLI,
	IP_VS_SCTP_EVE_COOKIE_ACK_SER,
	IP_VS_SCTP_EVE_ABORT_CLI,
	IP_VS_SCTP_EVE__ABORT_SER,
	IP_VS_SCTP_EVE_SHUT_CLI,
	IP_VS_SCTP_EVE_SHUT_SER,
	IP_VS_SCTP_EVE_SHUT_ACK_CLI,
	IP_VS_SCTP_EVE_SHUT_ACK_SER,
	IP_VS_SCTP_EVE_SHUT_COM_CLI,
	IP_VS_SCTP_EVE_SHUT_COM_SER,
	IP_VS_SCTP_EVE_LAST
};

static enum ipvs_sctp_event_t sctp_events[255] = {
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_INIT_CLI,
	IP_VS_SCTP_EVE_INIT_ACK_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_ABORT_CLI,
	IP_VS_SCTP_EVE_SHUT_CLI,
	IP_VS_SCTP_EVE_SHUT_ACK_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_COOKIE_ECHO_CLI,
	IP_VS_SCTP_EVE_COOKIE_ACK_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_DATA_CLI,
	IP_VS_SCTP_EVE_SHUT_COM_CLI,
};

static struct ipvs_sctp_nextstate
 sctp_states_table[IP_VS_SCTP_S_LAST][IP_VS_SCTP_EVE_LAST] = {
	
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_ACK_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_INIT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_INIT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_ECHO_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_ACK_SER  },
	 {IP_VS_SCTP_S_ECHO_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_ACK_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_ECHO_CLI  },
	 {IP_VS_SCTP_S_ECHO_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_ECHO_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_ECHO_SER  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },

	{{IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },

	{{IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },


	{{IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_CLI  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },


	{{IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_ESTABLISHED  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_CLI  },
	 {IP_VS_SCTP_S_SHUT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_SHUT_ACK_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 },
	{{IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_INIT_CLI  },
	 {IP_VS_SCTP_S_INIT_SER  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  },
	 {IP_VS_SCTP_S_CLOSED  }
	 }
};

static const int sctp_timeouts[IP_VS_SCTP_S_LAST + 1] = {
	[IP_VS_SCTP_S_NONE]         =     2 * HZ,
	[IP_VS_SCTP_S_INIT_CLI]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_INIT_SER]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_INIT_ACK_CLI] =     1 * 60 * HZ,
	[IP_VS_SCTP_S_INIT_ACK_SER] =     1 * 60 * HZ,
	[IP_VS_SCTP_S_ECHO_CLI]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_ECHO_SER]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_ESTABLISHED]  =    15 * 60 * HZ,
	[IP_VS_SCTP_S_SHUT_CLI]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_SHUT_SER]     =     1 * 60 * HZ,
	[IP_VS_SCTP_S_SHUT_ACK_CLI] =     1 * 60 * HZ,
	[IP_VS_SCTP_S_SHUT_ACK_SER] =     1 * 60 * HZ,
	[IP_VS_SCTP_S_CLOSED]       =    10 * HZ,
	[IP_VS_SCTP_S_LAST]         =     2 * HZ,
};

static const char *sctp_state_name_table[IP_VS_SCTP_S_LAST + 1] = {
	[IP_VS_SCTP_S_NONE]         =    "NONE",
	[IP_VS_SCTP_S_INIT_CLI]     =    "INIT_CLI",
	[IP_VS_SCTP_S_INIT_SER]     =    "INIT_SER",
	[IP_VS_SCTP_S_INIT_ACK_CLI] =    "INIT_ACK_CLI",
	[IP_VS_SCTP_S_INIT_ACK_SER] =    "INIT_ACK_SER",
	[IP_VS_SCTP_S_ECHO_CLI]     =    "COOKIE_ECHO_CLI",
	[IP_VS_SCTP_S_ECHO_SER]     =    "COOKIE_ECHO_SER",
	[IP_VS_SCTP_S_ESTABLISHED]  =    "ESTABISHED",
	[IP_VS_SCTP_S_SHUT_CLI]     =    "SHUTDOWN_CLI",
	[IP_VS_SCTP_S_SHUT_SER]     =    "SHUTDOWN_SER",
	[IP_VS_SCTP_S_SHUT_ACK_CLI] =    "SHUTDOWN_ACK_CLI",
	[IP_VS_SCTP_S_SHUT_ACK_SER] =    "SHUTDOWN_ACK_SER",
	[IP_VS_SCTP_S_CLOSED]       =    "CLOSED",
	[IP_VS_SCTP_S_LAST]         =    "BUG!"
};


static const char *sctp_state_name(int state)
{
	if (state >= IP_VS_SCTP_S_LAST)
		return "ERR!";
	if (sctp_state_name_table[state])
		return sctp_state_name_table[state];
	return "?";
}

static inline void
set_sctp_state(struct ip_vs_proto_data *pd, struct ip_vs_conn *cp,
		int direction, const struct sk_buff *skb)
{
	sctp_chunkhdr_t _sctpch, *sch;
	unsigned char chunk_type;
	int event, next_state;
	int ihl;

#ifdef CONFIG_IP_VS_IPV6
	ihl = cp->af == AF_INET ? ip_hdrlen(skb) : sizeof(struct ipv6hdr);
#else
	ihl = ip_hdrlen(skb);
#endif

	sch = skb_header_pointer(skb, ihl + sizeof(sctp_sctphdr_t),
				sizeof(_sctpch), &_sctpch);
	if (sch == NULL)
		return;

	chunk_type = sch->type;
	if ((sch->type == SCTP_CID_COOKIE_ECHO) ||
	    (sch->type == SCTP_CID_COOKIE_ACK)) {
		sch = skb_header_pointer(skb, (ihl + sizeof(sctp_sctphdr_t) +
				sch->length), sizeof(_sctpch), &_sctpch);
		if (sch) {
			if (sch->type == SCTP_CID_ABORT)
				chunk_type = sch->type;
		}
	}

	event = sctp_events[chunk_type];

	if (direction == IP_VS_DIR_OUTPUT)
		event++;
	next_state = sctp_states_table[cp->state][event].next_state;

	if (next_state != cp->state) {
		struct ip_vs_dest *dest = cp->dest;

		IP_VS_DBG_BUF(8, "%s %s  %s:%d->"
				"%s:%d state: %s->%s conn->refcnt:%d\n",
				pd->pp->name,
				((direction == IP_VS_DIR_OUTPUT) ?
				 "output " : "input "),
				IP_VS_DBG_ADDR(cp->af, &cp->daddr),
				ntohs(cp->dport),
				IP_VS_DBG_ADDR(cp->af, &cp->caddr),
				ntohs(cp->cport),
				sctp_state_name(cp->state),
				sctp_state_name(next_state),
				atomic_read(&cp->refcnt));
		if (dest) {
			if (!(cp->flags & IP_VS_CONN_F_INACTIVE) &&
				(next_state != IP_VS_SCTP_S_ESTABLISHED)) {
				atomic_dec(&dest->activeconns);
				atomic_inc(&dest->inactconns);
				cp->flags |= IP_VS_CONN_F_INACTIVE;
			} else if ((cp->flags & IP_VS_CONN_F_INACTIVE) &&
				   (next_state == IP_VS_SCTP_S_ESTABLISHED)) {
				atomic_inc(&dest->activeconns);
				atomic_dec(&dest->inactconns);
				cp->flags &= ~IP_VS_CONN_F_INACTIVE;
			}
		}
	}
	if (likely(pd))
		cp->timeout = pd->timeout_table[cp->state = next_state];
	else	
		cp->timeout = sctp_timeouts[cp->state = next_state];
}

static void
sctp_state_transition(struct ip_vs_conn *cp, int direction,
		const struct sk_buff *skb, struct ip_vs_proto_data *pd)
{
	spin_lock(&cp->lock);
	set_sctp_state(pd, cp, direction, skb);
	spin_unlock(&cp->lock);
}

static inline __u16 sctp_app_hashkey(__be16 port)
{
	return (((__force u16)port >> SCTP_APP_TAB_BITS) ^ (__force u16)port)
		& SCTP_APP_TAB_MASK;
}

static int sctp_register_app(struct net *net, struct ip_vs_app *inc)
{
	struct ip_vs_app *i;
	__u16 hash;
	__be16 port = inc->port;
	int ret = 0;
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct ip_vs_proto_data *pd = ip_vs_proto_data_get(net, IPPROTO_SCTP);

	hash = sctp_app_hashkey(port);

	spin_lock_bh(&ipvs->sctp_app_lock);
	list_for_each_entry(i, &ipvs->sctp_apps[hash], p_list) {
		if (i->port == port) {
			ret = -EEXIST;
			goto out;
		}
	}
	list_add(&inc->p_list, &ipvs->sctp_apps[hash]);
	atomic_inc(&pd->appcnt);
out:
	spin_unlock_bh(&ipvs->sctp_app_lock);

	return ret;
}

static void sctp_unregister_app(struct net *net, struct ip_vs_app *inc)
{
	struct netns_ipvs *ipvs = net_ipvs(net);
	struct ip_vs_proto_data *pd = ip_vs_proto_data_get(net, IPPROTO_SCTP);

	spin_lock_bh(&ipvs->sctp_app_lock);
	atomic_dec(&pd->appcnt);
	list_del(&inc->p_list);
	spin_unlock_bh(&ipvs->sctp_app_lock);
}

static int sctp_app_conn_bind(struct ip_vs_conn *cp)
{
	struct netns_ipvs *ipvs = net_ipvs(ip_vs_conn_net(cp));
	int hash;
	struct ip_vs_app *inc;
	int result = 0;

	
	if (IP_VS_FWD_METHOD(cp) != IP_VS_CONN_F_MASQ)
		return 0;
	
	hash = sctp_app_hashkey(cp->vport);

	spin_lock(&ipvs->sctp_app_lock);
	list_for_each_entry(inc, &ipvs->sctp_apps[hash], p_list) {
		if (inc->port == cp->vport) {
			if (unlikely(!ip_vs_app_inc_get(inc)))
				break;
			spin_unlock(&ipvs->sctp_app_lock);

			IP_VS_DBG_BUF(9, "%s: Binding conn %s:%u->"
					"%s:%u to app %s on port %u\n",
					__func__,
					IP_VS_DBG_ADDR(cp->af, &cp->caddr),
					ntohs(cp->cport),
					IP_VS_DBG_ADDR(cp->af, &cp->vaddr),
					ntohs(cp->vport),
					inc->name, ntohs(inc->port));
			cp->app = inc;
			if (inc->init_conn)
				result = inc->init_conn(inc, cp);
			goto out;
		}
	}
	spin_unlock(&ipvs->sctp_app_lock);
out:
	return result;
}

static int __ip_vs_sctp_init(struct net *net, struct ip_vs_proto_data *pd)
{
	struct netns_ipvs *ipvs = net_ipvs(net);

	ip_vs_init_hash_table(ipvs->sctp_apps, SCTP_APP_TAB_SIZE);
	spin_lock_init(&ipvs->sctp_app_lock);
	pd->timeout_table = ip_vs_create_timeout_table((int *)sctp_timeouts,
							sizeof(sctp_timeouts));
	if (!pd->timeout_table)
		return -ENOMEM;
	return 0;
}

static void __ip_vs_sctp_exit(struct net *net, struct ip_vs_proto_data *pd)
{
	kfree(pd->timeout_table);
}

struct ip_vs_protocol ip_vs_protocol_sctp = {
	.name		= "SCTP",
	.protocol	= IPPROTO_SCTP,
	.num_states	= IP_VS_SCTP_S_LAST,
	.dont_defrag	= 0,
	.init		= NULL,
	.exit		= NULL,
	.init_netns	= __ip_vs_sctp_init,
	.exit_netns	= __ip_vs_sctp_exit,
	.register_app	= sctp_register_app,
	.unregister_app = sctp_unregister_app,
	.conn_schedule	= sctp_conn_schedule,
	.conn_in_get	= ip_vs_conn_in_get_proto,
	.conn_out_get	= ip_vs_conn_out_get_proto,
	.snat_handler	= sctp_snat_handler,
	.dnat_handler	= sctp_dnat_handler,
	.csum_check	= sctp_csum_check,
	.state_name	= sctp_state_name,
	.state_transition = sctp_state_transition,
	.app_conn_bind	= sctp_app_conn_bind,
	.debug_packet	= ip_vs_tcpudp_debug_packet,
	.timeout_change	= NULL,
};
