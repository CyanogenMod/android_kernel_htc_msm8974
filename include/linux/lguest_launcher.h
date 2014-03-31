#ifndef _LINUX_LGUEST_LAUNCHER
#define _LINUX_LGUEST_LAUNCHER
#include <linux/types.h>

struct lguest_device_desc {
	
	__u8 type;
	
	__u8 num_vq;
	__u8 feature_len;
	
	__u8 config_len;
	/* A status byte, written by the Guest. */
	__u8 status;
	__u8 config[0];
};

struct lguest_vqconfig {
	
	__u16 num;
	
	__u16 irq;
	
	__u32 pfn;
};

enum lguest_req
{
	LHREQ_INITIALIZE, 
	LHREQ_GETDMA, 
	LHREQ_IRQ, 
	LHREQ_BREAK, 
	LHREQ_EVENTFD, 
};

#define LGUEST_VRING_ALIGN	4096
#endif 
