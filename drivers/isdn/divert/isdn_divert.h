/* $Id: isdn_divert.h,v 1.5.6.1 2001/09/23 22:24:36 kai Exp $
 *
 * Header for the diversion supplementary ioctl interface.
 *
 * Copyright 1998       by Werner Cornelius (werner@ikt.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/ioctl.h>
#include <linux/types.h>

#define DIVERT_IIOC_VERSION 0x01 
#define IIOCGETVER   _IO('I', 1)  
#define IIOCGETDRV   _IO('I', 2)  
#define IIOCGETNAM   _IO('I', 3)  
#define IIOCGETRULE  _IO('I', 4)  
#define IIOCMODRULE  _IO('I', 5)  
#define IIOCINSRULE  _IO('I', 6)  
#define IIOCDELRULE  _IO('I', 7)  
#define IIOCDODFACT  _IO('I', 8)  
#define IIOCDOCFACT  _IO('I', 9)  
#define IIOCDOCFDIS  _IO('I', 10)  
#define IIOCDOCFINT  _IO('I', 11)  

#define DEFLECT_IGNORE    0  
#define DEFLECT_REPORT    1  
#define DEFLECT_PROCEED   2  
#define DEFLECT_ALERT     3  
#define DEFLECT_REJECT    4  
#define DIVERT_ACTIVATE   5  
#define DIVERT_DEACTIVATE 6  
#define DIVERT_REPORT     7  
#define DEFLECT_AUTODEL 255  

#define DEFLECT_ALL_IDS   0xFFFFFFFF 

typedef struct
{ ulong drvid;     
	char my_msn[35]; 
	char caller[35]; 
	char to_nr[35];  
	u_char si1, si2;  
	u_char screen;   
	u_char callopt;  
	u_char action;   
	u_char waittime; 
} divert_rule;

typedef union
{ int drv_version; 
	struct
	{ int drvid;		
		char drvnam[30];	
	} getid;
	struct
	{ int ruleidx;	
		divert_rule rule;	
	} getsetrule;
	struct
	{ u_char subcmd;  
		ulong callid;   
		char to_nr[35]; 
	} fwd_ctrl;
	struct
	{ int drvid;      
		u_char cfproc;  
		ulong procid;   
		u_char service; 
		char msn[25];   
		char fwd_nr[35];
	} cf_ctrl;
} divert_ioctl;

#ifdef __KERNEL__

#include <linux/isdnif.h>
#include <linux/isdn_divertif.h>

#define AUTODEL_TIME 30 

struct divert_info
{ struct divert_info *next;
	ulong usage_cnt; 
	char info_start[2]; 
};


extern spinlock_t divert_lock;

extern ulong if_used; 
extern int divert_dev_deinit(void);
extern int divert_dev_init(void);
extern void put_info_buffer(char *);
extern int ll_callback(isdn_ctrl *);
extern isdn_divert_if divert_if;
extern divert_rule *getruleptr(int);
extern int insertrule(int, divert_rule *);
extern int deleterule(int);
extern void deleteprocs(void);
extern int deflect_extern_action(u_char, ulong, char *);
extern int cf_command(int, int, u_char, char *, u_char, char *, ulong *);

#endif 
