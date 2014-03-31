#ifndef S390_IO_SCH_H
#define S390_IO_SCH_H

#include <linux/types.h>
#include <asm/schid.h>
#include <asm/ccwdev.h>
#include <asm/irq.h>
#include "css.h"
#include "orb.h"

struct io_subchannel_private {
	union orb orb;		
	struct ccw1 sense_ccw;	
	struct ccw_device *cdev;
	struct {
		unsigned int suspend:1;	
		unsigned int prefetch:1;
		unsigned int inter:1;	
	} __packed options;
} __aligned(8);

#define to_io_private(n) ((struct io_subchannel_private *) \
			  dev_get_drvdata(&(n)->dev))
#define set_io_private(n, p) (dev_set_drvdata(&(n)->dev, p))

static inline struct ccw_device *sch_get_cdev(struct subchannel *sch)
{
	struct io_subchannel_private *priv = to_io_private(sch);
	return priv ? priv->cdev : NULL;
}

static inline void sch_set_cdev(struct subchannel *sch,
				struct ccw_device *cdev)
{
	struct io_subchannel_private *priv = to_io_private(sch);
	if (priv)
		priv->cdev = cdev;
}

#define MAX_CIWS 8

enum io_status {
	IO_DONE,
	IO_RUNNING,
	IO_STATUS_ERROR,
	IO_PATH_ERROR,
	IO_REJECTED,
	IO_KILLED
};

struct ccw_request {
	struct ccw1 *cp;
	unsigned long timeout;
	u16 maxretries;
	u8 lpm;
	int (*check)(struct ccw_device *, void *);
	enum io_status (*filter)(struct ccw_device *, void *, struct irb *,
				 enum io_status);
	void (*callback)(struct ccw_device *, void *, int);
	void *data;
	unsigned int singlepath:1;
	
	unsigned int cancel:1;
	unsigned int done:1;
	u16 mask;
	u16 retries;
	int drc;
} __attribute__((packed));

struct senseid {
	
	u8  reserved;	
	u16 cu_type;	
	u8  cu_model;	
	u16 dev_type;	
	u8  dev_model;	
	u8  unused;	
	
	struct ciw ciw[MAX_CIWS];	
}  __attribute__ ((packed, aligned(4)));

enum cdev_todo {
	CDEV_TODO_NOTHING,
	CDEV_TODO_ENABLE_CMF,
	CDEV_TODO_REBIND,
	CDEV_TODO_REGISTER,
	CDEV_TODO_UNREG,
	CDEV_TODO_UNREG_EVAL,
};

#define FAKE_CMD_IRB	1
#define FAKE_TM_IRB	2

struct ccw_device_private {
	struct ccw_device *cdev;
	struct subchannel *sch;
	int state;		
	atomic_t onoff;
	struct ccw_dev_id dev_id;	
	struct subchannel_id schid;	
	struct ccw_request req;		
	int iretry;
	u8 pgid_valid_mask;	
	u8 pgid_todo_mask;	
	u8 pgid_reset_mask;	
	u8 path_gone_mask;	
	u8 path_new_mask;	
	struct {
		unsigned int fast:1;	
		unsigned int repall:1;	
		unsigned int pgroup:1;	
		unsigned int force:1;	
		unsigned int mpath:1;	
	} __attribute__ ((packed)) options;
	struct {
		unsigned int esid:1;	    
		unsigned int dosense:1;	    
		unsigned int doverify:1;    
		unsigned int donotify:1;    
		unsigned int recog_done:1;  
		unsigned int fake_irb:2;    
		unsigned int resuming:1;    
		unsigned int pgroup:1;	    
		unsigned int mpath:1;	    
		unsigned int initialized:1; 
	} __attribute__((packed)) flags;
	unsigned long intparm;	
	struct qdio_irq *qdio_data;
	struct irb irb;		
	struct senseid senseid;	
	struct pgid pgid[8];	
	struct ccw1 iccws[2];	
	struct work_struct todo_work;
	enum cdev_todo todo;
	wait_queue_head_t wait_q;
	struct timer_list timer;
	void *cmb;			
	struct list_head cmb_list;	
	u64 cmb_start_time;		
	void *cmb_wait;			
	enum interruption_class int_class;
};

static inline int rsch(struct subchannel_id schid)
{
	register struct subchannel_id reg1 asm("1") = schid;
	int ccode;

	asm volatile(
		"	rsch\n"
		"	ipm	%0\n"
		"	srl	%0,28"
		: "=d" (ccode)
		: "d" (reg1)
		: "cc", "memory");
	return ccode;
}

static inline int hsch(struct subchannel_id schid)
{
	register struct subchannel_id reg1 asm("1") = schid;
	int ccode;

	asm volatile(
		"	hsch\n"
		"	ipm	%0\n"
		"	srl	%0,28"
		: "=d" (ccode)
		: "d" (reg1)
		: "cc");
	return ccode;
}

static inline int xsch(struct subchannel_id schid)
{
	register struct subchannel_id reg1 asm("1") = schid;
	int ccode;

	asm volatile(
		"	.insn	rre,0xb2760000,%1,0\n"
		"	ipm	%0\n"
		"	srl	%0,28"
		: "=d" (ccode)
		: "d" (reg1)
		: "cc");
	return ccode;
}

#endif
