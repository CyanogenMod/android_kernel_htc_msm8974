/*
 * Any part of this program may be used in documents licensed under
 * the GNU Free Documentation License, Version 1.1 or any later version
 * published by the Free Software Foundation.
 */

#ifndef _PARPORT_H_
#define _PARPORT_H_


#define PARPORT_MAX  16

#define PARPORT_IRQ_NONE  -1
#define PARPORT_DMA_NONE  -1
#define PARPORT_IRQ_AUTO  -2
#define PARPORT_DMA_AUTO  -2
#define PARPORT_DMA_NOFIFO -3
#define PARPORT_DISABLE   -2
#define PARPORT_IRQ_PROBEONLY -3
#define PARPORT_IOHI_AUTO -1

#define PARPORT_CONTROL_STROBE    0x1
#define PARPORT_CONTROL_AUTOFD    0x2
#define PARPORT_CONTROL_INIT      0x4
#define PARPORT_CONTROL_SELECT    0x8

#define PARPORT_STATUS_ERROR      0x8
#define PARPORT_STATUS_SELECT     0x10
#define PARPORT_STATUS_PAPEROUT   0x20
#define PARPORT_STATUS_ACK        0x40
#define PARPORT_STATUS_BUSY       0x80

typedef enum {
	PARPORT_CLASS_LEGACY = 0,       
	PARPORT_CLASS_PRINTER,
	PARPORT_CLASS_MODEM,
	PARPORT_CLASS_NET,
	PARPORT_CLASS_HDC,              
	PARPORT_CLASS_PCMCIA,
	PARPORT_CLASS_MEDIA,            
	PARPORT_CLASS_FDC,              
	PARPORT_CLASS_PORTS,
	PARPORT_CLASS_SCANNER,
	PARPORT_CLASS_DIGCAM,
	PARPORT_CLASS_OTHER,            
	PARPORT_CLASS_UNSPEC,           
	PARPORT_CLASS_SCSIADAPTER
} parport_device_class;

#define PARPORT_MODE_PCSPP	(1<<0) 
#define PARPORT_MODE_TRISTATE	(1<<1) 
#define PARPORT_MODE_EPP	(1<<2) 
#define PARPORT_MODE_ECP	(1<<3) 
#define PARPORT_MODE_COMPAT	(1<<4) 
#define PARPORT_MODE_DMA	(1<<5) 
#define PARPORT_MODE_SAFEININT	(1<<6) 

#define IEEE1284_MODE_NIBBLE             0
#define IEEE1284_MODE_BYTE              (1<<0)
#define IEEE1284_MODE_COMPAT            (1<<8)
#define IEEE1284_MODE_BECP              (1<<9) 
#define IEEE1284_MODE_ECP               (1<<4)
#define IEEE1284_MODE_ECPRLE            (IEEE1284_MODE_ECP | (1<<5))
#define IEEE1284_MODE_ECPSWE            (1<<10) 
#define IEEE1284_MODE_EPP               (1<<6)
#define IEEE1284_MODE_EPPSL             (1<<11) 
#define IEEE1284_MODE_EPPSWE            (1<<12) 
#define IEEE1284_DEVICEID               (1<<2)  
#define IEEE1284_EXT_LINK               (1<<14) 

#define IEEE1284_ADDR			(1<<13)	
#define IEEE1284_DATA			 0	

#define PARPORT_EPP_FAST		(1<<0) 
#define PARPORT_W91284PIC		(1<<1) 

#ifdef __KERNEL__

#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/irqreturn.h>
#include <linux/semaphore.h>
#include <asm/ptrace.h>

struct parport;
struct pardevice;

struct pc_parport_state {
	unsigned int ctr;
	unsigned int ecr;
};

struct ax_parport_state {
	unsigned int ctr;
	unsigned int ecr;
	unsigned int dcsr;
};

struct amiga_parport_state {
       unsigned char data;     
       unsigned char datadir;  
       unsigned char status;   
       unsigned char statusdir;
};

struct ax88796_parport_state {
	unsigned char cpr;
};

struct ip32_parport_state {
	unsigned int dcr;
	unsigned int ecr;
};

struct parport_state {
	union {
		struct pc_parport_state pc;
		
		struct ax_parport_state ax;
		struct amiga_parport_state amiga;
		struct ax88796_parport_state ax88796;
		
		struct ip32_parport_state ip32;
		void *misc; 
	} u;
};

struct parport_operations {
	
	void (*write_data)(struct parport *, unsigned char);
	unsigned char (*read_data)(struct parport *);

	void (*write_control)(struct parport *, unsigned char);
	unsigned char (*read_control)(struct parport *);
	unsigned char (*frob_control)(struct parport *, unsigned char mask,
				      unsigned char val);

	unsigned char (*read_status)(struct parport *);

	
	void (*enable_irq)(struct parport *);
	void (*disable_irq)(struct parport *);

	
	void (*data_forward) (struct parport *);
	void (*data_reverse) (struct parport *);

	
	void (*init_state)(struct pardevice *, struct parport_state *);
	void (*save_state)(struct parport *, struct parport_state *);
	void (*restore_state)(struct parport *, struct parport_state *);

	
	size_t (*epp_write_data) (struct parport *port, const void *buf,
				  size_t len, int flags);
	size_t (*epp_read_data) (struct parport *port, void *buf, size_t len,
				 int flags);
	size_t (*epp_write_addr) (struct parport *port, const void *buf,
				  size_t len, int flags);
	size_t (*epp_read_addr) (struct parport *port, void *buf, size_t len,
				 int flags);

	size_t (*ecp_write_data) (struct parport *port, const void *buf,
				  size_t len, int flags);
	size_t (*ecp_read_data) (struct parport *port, void *buf, size_t len,
				 int flags);
	size_t (*ecp_write_addr) (struct parport *port, const void *buf,
				  size_t len, int flags);

	size_t (*compat_write_data) (struct parport *port, const void *buf,
				     size_t len, int flags);
	size_t (*nibble_read_data) (struct parport *port, void *buf,
				    size_t len, int flags);
	size_t (*byte_read_data) (struct parport *port, void *buf,
				  size_t len, int flags);
	struct module *owner;
};

struct parport_device_info {
	parport_device_class class;
	const char *class_name;
	const char *mfr;
	const char *model;
	const char *cmdset;
	const char *description;
};


struct pardevice {
	const char *name;
	struct parport *port;
	int daisy;
	int (*preempt)(void *);
	void (*wakeup)(void *);
	void *private;
	void (*irq_func)(void *);
	unsigned int flags;
	struct pardevice *next;
	struct pardevice *prev;
	struct parport_state *state;     
	wait_queue_head_t wait_q;
	unsigned long int time;
	unsigned long int timeslice;
	volatile long int timeout;
	unsigned long waiting;		 
	struct pardevice *waitprev;
	struct pardevice *waitnext;
	void * sysctl_table;
};


enum ieee1284_phase {
	IEEE1284_PH_FWD_DATA,
	IEEE1284_PH_FWD_IDLE,
	IEEE1284_PH_TERMINATE,
	IEEE1284_PH_NEGOTIATION,
	IEEE1284_PH_HBUSY_DNA,
	IEEE1284_PH_REV_IDLE,
	IEEE1284_PH_HBUSY_DAVAIL,
	IEEE1284_PH_REV_DATA,
	IEEE1284_PH_ECP_SETUP,
	IEEE1284_PH_ECP_FWD_TO_REV,
	IEEE1284_PH_ECP_REV_TO_FWD,
	IEEE1284_PH_ECP_DIR_UNKNOWN,
};
struct ieee1284_info {
	int mode;
	volatile enum ieee1284_phase phase;
	struct semaphore irq;
};

struct parport {
	unsigned long base;	
	unsigned long base_hi;  
	unsigned int size;	
	const char *name;
	unsigned int modes;
	int irq;		
	int dma;
	int muxport;		
	int portnum;		
	struct device *dev;	

	struct parport *physport;

	struct pardevice *devices;
	struct pardevice *cad;	
	int daisy;		
	int muxsel;		

	struct pardevice *waithead;
	struct pardevice *waittail;

	struct list_head list;
	unsigned int flags;

	void *sysctl_table;
	struct parport_device_info probe_info[5]; 
	struct ieee1284_info ieee1284;

	struct parport_operations *ops;
	void *private_data;     

	int number;		
	spinlock_t pardevice_lock;
	spinlock_t waitlist_lock;
	rwlock_t cad_lock;

	int spintime;
	atomic_t ref_count;

	unsigned long devflags;
#define PARPORT_DEVPROC_REGISTERED	0
	struct pardevice *proc_device;	

	struct list_head full_list;
	struct parport *slaves[3];
};

#define DEFAULT_SPIN_TIME 500 

struct parport_driver {
	const char *name;
	void (*attach) (struct parport *);
	void (*detach) (struct parport *);
	struct list_head list;
};

struct parport *parport_register_port(unsigned long base, int irq, int dma,
				      struct parport_operations *ops);

void parport_announce_port (struct parport *port);

extern void parport_remove_port(struct parport *port);

extern int parport_register_driver (struct parport_driver *);

extern void parport_unregister_driver (struct parport_driver *);

extern struct parport *parport_find_number (int);
extern struct parport *parport_find_base (unsigned long);

extern irqreturn_t parport_irq_handler(int irq, void *dev_id);

extern struct parport *parport_get_port (struct parport *);
extern void parport_put_port (struct parport *);

struct pardevice *parport_register_device(struct parport *port, 
			  const char *name,
			  int (*pf)(void *), void (*kf)(void *),
			  void (*irq_func)(void *), 
			  int flags, void *handle);

extern void parport_unregister_device(struct pardevice *dev);

extern int parport_claim(struct pardevice *dev);

extern int parport_claim_or_block(struct pardevice *dev);


extern void parport_release(struct pardevice *dev);

static __inline__ int parport_yield(struct pardevice *dev)
{
	unsigned long int timeslip = (jiffies - dev->time);
	if ((dev->port->waithead == NULL) || (timeslip < dev->timeslice))
		return 0;
	parport_release(dev);
	return parport_claim(dev);
}

static __inline__ int parport_yield_blocking(struct pardevice *dev)
{
	unsigned long int timeslip = (jiffies - dev->time);
	if ((dev->port->waithead == NULL) || (timeslip < dev->timeslice))
		return 0;
	parport_release(dev);
	return parport_claim_or_block(dev);
}

#define PARPORT_DEV_TRAN		0	
#define PARPORT_DEV_LURK		(1<<0)	
#define PARPORT_DEV_EXCL		(1<<1)	

#define PARPORT_FLAG_EXCL		(1<<1)	

extern void parport_ieee1284_interrupt (void *);
extern int parport_negotiate (struct parport *, int mode);
extern ssize_t parport_write (struct parport *, const void *buf, size_t len);
extern ssize_t parport_read (struct parport *, void *buf, size_t len);

#define PARPORT_INACTIVITY_O_NONBLOCK 1
extern long parport_set_timeout (struct pardevice *, long inactivity);

extern int parport_wait_event (struct parport *, long timeout);
extern int parport_wait_peripheral (struct parport *port,
				    unsigned char mask,
				    unsigned char val);
extern int parport_poll_peripheral (struct parport *port,
				    unsigned char mask,
				    unsigned char val,
				    int usec);

extern size_t parport_ieee1284_write_compat (struct parport *,
					     const void *, size_t, int);
extern size_t parport_ieee1284_read_nibble (struct parport *,
					    void *, size_t, int);
extern size_t parport_ieee1284_read_byte (struct parport *,
					  void *, size_t, int);
extern size_t parport_ieee1284_ecp_read_data (struct parport *,
					      void *, size_t, int);
extern size_t parport_ieee1284_ecp_write_data (struct parport *,
					       const void *, size_t, int);
extern size_t parport_ieee1284_ecp_write_addr (struct parport *,
					       const void *, size_t, int);
extern size_t parport_ieee1284_epp_write_data (struct parport *,
					       const void *, size_t, int);
extern size_t parport_ieee1284_epp_read_data (struct parport *,
					      void *, size_t, int);
extern size_t parport_ieee1284_epp_write_addr (struct parport *,
					       const void *, size_t, int);
extern size_t parport_ieee1284_epp_read_addr (struct parport *,
					      void *, size_t, int);

extern int parport_daisy_init (struct parport *port);
extern void parport_daisy_fini (struct parport *port);
extern struct pardevice *parport_open (int devnum, const char *name);
extern void parport_close (struct pardevice *dev);
extern ssize_t parport_device_id (int devnum, char *buffer, size_t len);
extern void parport_daisy_deselect_all (struct parport *port);
extern int parport_daisy_select (struct parport *port, int daisy, int mode);

static inline void parport_generic_irq(struct parport *port)
{
	parport_ieee1284_interrupt (port);
	read_lock(&port->cad_lock);
	if (port->cad && port->cad->irq_func)
		port->cad->irq_func(port->cad->private);
	read_unlock(&port->cad_lock);
}

extern int parport_proc_register(struct parport *pp);
extern int parport_proc_unregister(struct parport *pp);
extern int parport_device_proc_register(struct pardevice *device);
extern int parport_device_proc_unregister(struct pardevice *device);

#if !defined(CONFIG_PARPORT_NOT_PC)

#include <linux/parport_pc.h>
#define parport_write_data(p,x)            parport_pc_write_data(p,x)
#define parport_read_data(p)               parport_pc_read_data(p)
#define parport_write_control(p,x)         parport_pc_write_control(p,x)
#define parport_read_control(p)            parport_pc_read_control(p)
#define parport_frob_control(p,m,v)        parport_pc_frob_control(p,m,v)
#define parport_read_status(p)             parport_pc_read_status(p)
#define parport_enable_irq(p)              parport_pc_enable_irq(p)
#define parport_disable_irq(p)             parport_pc_disable_irq(p)
#define parport_data_forward(p)            parport_pc_data_forward(p)
#define parport_data_reverse(p)            parport_pc_data_reverse(p)

#else  

#define parport_write_data(p,x)            (p)->ops->write_data(p,x)
#define parport_read_data(p)               (p)->ops->read_data(p)
#define parport_write_control(p,x)         (p)->ops->write_control(p,x)
#define parport_read_control(p)            (p)->ops->read_control(p)
#define parport_frob_control(p,m,v)        (p)->ops->frob_control(p,m,v)
#define parport_read_status(p)             (p)->ops->read_status(p)
#define parport_enable_irq(p)              (p)->ops->enable_irq(p)
#define parport_disable_irq(p)             (p)->ops->disable_irq(p)
#define parport_data_forward(p)            (p)->ops->data_forward(p)
#define parport_data_reverse(p)            (p)->ops->data_reverse(p)

#endif 

extern unsigned long parport_default_timeslice;
extern int parport_default_spintime;

#endif 
#endif 
