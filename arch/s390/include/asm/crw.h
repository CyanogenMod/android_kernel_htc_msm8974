/*
 *   Data definitions for channel report processing
 *    Copyright IBM Corp. 2000,2009
 *    Author(s): Ingo Adlung <adlung@de.ibm.com>,
 *		 Martin Schwidefsky <schwidefsky@de.ibm.com>,
 *		 Cornelia Huck <cornelia.huck@de.ibm.com>,
 *		 Heiko Carstens <heiko.carstens@de.ibm.com>,
 */

#ifndef _ASM_S390_CRW_H
#define _ASM_S390_CRW_H

#include <linux/types.h>

struct crw {
	__u32 res1 :  1;   
	__u32 slct :  1;   
	__u32 oflw :  1;   
	__u32 chn  :  1;   
	__u32 rsc  :  4;   
	__u32 anc  :  1;   
	__u32 res2 :  1;   
	__u32 erc  :  6;   
	__u32 rsid : 16;   
} __attribute__ ((packed));

typedef void (*crw_handler_t)(struct crw *, struct crw *, int);

extern int crw_register_handler(int rsc, crw_handler_t handler);
extern void crw_unregister_handler(int rsc);
extern void crw_handle_channel_report(void);
void crw_wait_for_channel_report(void);

#define NR_RSCS 16

#define CRW_RSC_MONITOR  0x2  
#define CRW_RSC_SCH	 0x3  
#define CRW_RSC_CPATH	 0x4  
#define CRW_RSC_CONFIG	 0x9  
#define CRW_RSC_CSS	 0xB  

#define CRW_ERC_EVENT	 0x00 
#define CRW_ERC_AVAIL	 0x01 
#define CRW_ERC_INIT	 0x02 
#define CRW_ERC_TERROR	 0x03 
#define CRW_ERC_IPARM	 0x04 
#define CRW_ERC_TERM	 0x05 
#define CRW_ERC_PERRN	 0x06 
#define CRW_ERC_PERRI	 0x07 
#define CRW_ERC_PMOD	 0x08 

static inline int stcrw(struct crw *pcrw)
{
	int ccode;

	asm volatile(
		"	stcrw	0(%2)\n"
		"	ipm	%0\n"
		"	srl	%0,28\n"
		: "=d" (ccode), "=m" (*pcrw)
		: "a" (pcrw)
		: "cc" );
	return ccode;
}

#endif 
