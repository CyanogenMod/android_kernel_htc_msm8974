#ifndef S390_CIO_H
#define S390_CIO_H

#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <asm/chpid.h>
#include <asm/cio.h>
#include <asm/fcx.h>
#include <asm/schid.h>
#include "chsc.h"

struct pmcw {
	u32 intparm;		
	u32 qf	 : 1;		
	u32 w	 : 1;
	u32 isc  : 3;		
	u32 res5 : 3;		
	u32 ena  : 1;		
	u32 lm	 : 2;		
	u32 mme  : 2;		
	u32 mp	 : 1;		
	u32 tf	 : 1;		
	u32 dnv  : 1;		
	u32 dev  : 16;		
	u8  lpm;		
	u8  pnom;		
	u8  lpum;		
	u8  pim;		
	u16 mbi;		
	u8  pom;		
	u8  pam;		
	u8  chpid[8];		
	u32 unused1 : 8;	
	u32 st	    : 3;	
	u32 unused2 : 18;	
	u32 mbfc    : 1;	
	u32 xmwme   : 1;	
	u32 csense  : 1;	
				
				
				
} __attribute__ ((packed));

struct schib_config {
	u64 mba;
	u32 intparm;
	u16 mbi;
	u32 isc:3;
	u32 ena:1;
	u32 mme:2;
	u32 mp:1;
	u32 csense:1;
	u32 mbfc:1;
} __attribute__ ((packed));

struct schib {
	struct pmcw pmcw;	 
	union scsw scsw;	 
	__u64 mba;               
	__u8 mda[4];		 
} __attribute__ ((packed,aligned(4)));

enum sch_todo {
	SCH_TODO_NOTHING,
	SCH_TODO_EVAL,
	SCH_TODO_UNREG,
};

struct subchannel {
	struct subchannel_id schid;
	spinlock_t *lock;	
	struct mutex reg_mutex;
	enum {
		SUBCHANNEL_TYPE_IO = 0,
		SUBCHANNEL_TYPE_CHSC = 1,
		SUBCHANNEL_TYPE_MSG = 2,
		SUBCHANNEL_TYPE_ADM = 3,
	} st;			
	__u8 vpm;		
	__u8 lpm;		
	__u8 opm;               
	struct schib schib;	
	int isc; 
	struct chsc_ssd_info ssd_info;	
	struct device dev;	
	struct css_driver *driver;
	enum sch_todo todo;
	struct work_struct todo_work;
	struct schib_config config;
} __attribute__ ((aligned(8)));

#define to_subchannel(n) container_of(n, struct subchannel, dev)

extern int cio_validate_subchannel (struct subchannel *, struct subchannel_id);
extern int cio_enable_subchannel(struct subchannel *, u32);
extern int cio_disable_subchannel (struct subchannel *);
extern int cio_cancel (struct subchannel *);
extern int cio_clear (struct subchannel *);
extern int cio_resume (struct subchannel *);
extern int cio_halt (struct subchannel *);
extern int cio_start (struct subchannel *, struct ccw1 *, __u8);
extern int cio_start_key (struct subchannel *, struct ccw1 *, __u8, __u8);
extern int cio_cancel (struct subchannel *);
extern int cio_set_options (struct subchannel *, int);
extern int cio_update_schib(struct subchannel *sch);
extern int cio_commit_config(struct subchannel *sch);

int cio_tm_start_key(struct subchannel *sch, struct tcw *tcw, u8 lpm, u8 key);
int cio_tm_intrg(struct subchannel *sch);

int cio_create_sch_lock(struct subchannel *);
void do_adapter_IO(u8 isc);
void do_IRQ(struct pt_regs *);

#ifdef CONFIG_CCW_CONSOLE
extern struct subchannel *cio_probe_console(void);
extern void cio_release_console(void);
extern int cio_is_console(struct subchannel_id);
extern struct subchannel *cio_get_console_subchannel(void);
extern spinlock_t * cio_get_console_lock(void);
extern void *cio_get_console_priv(void);
#else
#define cio_is_console(schid) 0
#define cio_get_console_subchannel() NULL
#define cio_get_console_lock() NULL
#define cio_get_console_priv() NULL
#endif

#endif
