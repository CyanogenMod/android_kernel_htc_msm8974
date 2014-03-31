
/* Written 1997-2000 by Werner Almesberger, EPFL LRC/ICA */


#ifndef LINUX_ATM_TCP_H
#define LINUX_ATM_TCP_H

#include <linux/atmapi.h>
#include <linux/atm.h>
#include <linux/atmioc.h>
#include <linux/types.h>



struct atmtcp_hdr {
	__u16	vpi;
	__u16	vci;
	__u32	length;		
};


#define ATMTCP_HDR_MAGIC	(~0)	
#define ATMTCP_CTRL_OPEN	1	
#define ATMTCP_CTRL_CLOSE	2	

struct atmtcp_control {
	struct atmtcp_hdr hdr;	
	int type;		
	atm_kptr_t vcc;		
	struct sockaddr_atmpvc addr; 
	struct atm_qos	qos;	
	int result;		
} __ATM_API_ALIGN;


#define SIOCSIFATMTCP	_IO('a',ATMIOC_ITF)	
#define ATMTCP_CREATE	_IO('a',ATMIOC_ITF+14)	
#define ATMTCP_REMOVE	_IO('a',ATMIOC_ITF+15)	


#ifdef __KERNEL__

struct atm_tcp_ops {
	int (*attach)(struct atm_vcc *vcc,int itf);
	int (*create_persistent)(int itf);
	int (*remove_persistent)(int itf);
	struct module *owner;
};

extern struct atm_tcp_ops atm_tcp_ops;

#endif

#endif
