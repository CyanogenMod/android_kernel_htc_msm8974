/*
 *   Machine check handler definitions
 *
 *    Copyright IBM Corp. 2000,2009
 *    Author(s): Ingo Adlung <adlung@de.ibm.com>,
 *		 Martin Schwidefsky <schwidefsky@de.ibm.com>,
 *		 Cornelia Huck <cornelia.huck@de.ibm.com>,
 *		 Heiko Carstens <heiko.carstens@de.ibm.com>,
 */

#ifndef _ASM_S390_NMI_H
#define _ASM_S390_NMI_H

#include <linux/types.h>

struct mci {
	__u32 sd :  1; 
	__u32 pd :  1; 
	__u32 sr :  1; 
	__u32	 :  1; 
	__u32 cd :  1; 
	__u32 ed :  1; 
	__u32	 :  1; 
	__u32 dg :  1; 
	__u32 w  :  1; 
	__u32 cp :  1; 
	__u32 sp :  1; 
	__u32 ck :  1; 
	__u32	 :  2; 
	__u32 b  :  1; 
	__u32	 :  1; 
	__u32 se :  1; 
	__u32 sc :  1; 
	__u32 ke :  1; 
	__u32 ds :  1; 
	__u32 wp :  1; 
	__u32 ms :  1; 
	__u32 pm :  1; 
	__u32 ia :  1; 
	__u32 fa :  1; 
	__u32	 :  1; 
	__u32 ec :  1; 
	__u32 fp :  1; 
	__u32 gr :  1; 
	__u32 cr :  1; 
	__u32	 :  1; 
	__u32 st :  1; 
	__u32 ie :  1; 
	__u32 ar :  1; 
	__u32 da :  1; 
	__u32	 :  7; 
	__u32 pr :  1; 
	__u32 fc :  1; 
	__u32 ap :  1; 
	__u32	 :  1; 
	__u32 ct :  1; 
	__u32 cc :  1; 
	__u32	 : 16; 
};

struct pt_regs;

extern void s390_handle_mcck(void);
extern void s390_do_machine_check(struct pt_regs *regs);

#endif 
