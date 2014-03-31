
#ifndef __LINUX_IF_EC
#define __LINUX_IF_EC


struct ec_addr {
  unsigned char station;		
  unsigned char net;			
};

struct sockaddr_ec {
  unsigned short sec_family;
  unsigned char port;			
  unsigned char cb;			
  unsigned char type;			
  struct ec_addr addr;
  unsigned long cookie;
};

#define ECTYPE_PACKET_RECEIVED		0	
#define ECTYPE_TRANSMIT_STATUS		0x10	

#define ECTYPE_TRANSMIT_OK		1
#define ECTYPE_TRANSMIT_NOT_LISTENING	2
#define ECTYPE_TRANSMIT_NET_ERROR	3
#define ECTYPE_TRANSMIT_NO_CLOCK	4
#define ECTYPE_TRANSMIT_LINE_JAMMED	5
#define ECTYPE_TRANSMIT_NOT_PRESENT	6

#ifdef __KERNEL__

#define EC_HLEN				6

struct ec_framehdr {
  unsigned char dst_stn;
  unsigned char dst_net;
  unsigned char src_stn;
  unsigned char src_net;
  unsigned char cb;
  unsigned char port;
};

struct econet_sock {
  
  struct sock	sk;
  unsigned char cb;
  unsigned char port;
  unsigned char station;
  unsigned char net;
  unsigned short num;
};

static inline struct econet_sock *ec_sk(const struct sock *sk)
{
	return (struct econet_sock *)sk;
}

struct ec_device {
  unsigned char station, net;		
};

#endif

#endif
