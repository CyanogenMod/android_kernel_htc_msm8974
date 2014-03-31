#ifndef _IPT_LOG_H
#define _IPT_LOG_H

#warning "Please update iptables, this file will be removed soon!"

#define IPT_LOG_TCPSEQ		0x01	
#define IPT_LOG_TCPOPT		0x02	
#define IPT_LOG_IPOPT		0x04	
#define IPT_LOG_UID		0x08	
#define IPT_LOG_NFLOG		0x10	
#define IPT_LOG_MACDECODE	0x20	
#define IPT_LOG_MASK		0x2f

struct ipt_log_info {
	unsigned char level;
	unsigned char logflags;
	char prefix[30];
};

#endif 
