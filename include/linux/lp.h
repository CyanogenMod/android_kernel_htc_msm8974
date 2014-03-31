#ifndef _LINUX_LP_H
#define _LINUX_LP_H

/*
 * usr/include/linux/lp.h c.1991-1992 James Wiegand
 * many modifications copyright (C) 1992 Michael K. Johnson
 * Interrupt support added 1993 Nigel Gamble
 * Removed 8255 status defines from inside __KERNEL__ Marcelo Tosatti 
 */

#define LP_EXIST 0x0001
#define LP_SELEC 0x0002
#define LP_BUSY	 0x0004
#define LP_BUSY_BIT_POS 2
#define LP_OFFL	 0x0008
#define LP_NOPA  0x0010
#define LP_ERR   0x0020
#define LP_ABORT 0x0040
#define LP_CAREFUL 0x0080 
#define LP_ABORTOPEN 0x0100

#define LP_TRUST_IRQ_  0x0200 
#define LP_NO_REVERSE  0x0400 
#define LP_DATA_AVAIL  0x0800 

#define LP_PBUSY	0x80  
#define LP_PACK		0x40  
#define LP_POUTPA	0x20  
#define LP_PSELECD	0x10  
#define LP_PERRORP	0x08  


#define LP_INIT_CHAR 1000


#define LP_INIT_WAIT 1


#define LP_INIT_TIME 2

#define LPCHAR   0x0601  
#define LPTIME   0x0602  
#define LPABORT  0x0604  
#define LPSETIRQ 0x0605  
#define LPGETIRQ 0x0606  
#define LPWAIT   0x0608  
#define LPCAREFUL   0x0609  
#define LPABORTOPEN 0x060a  
#define LPGETSTATUS 0x060b  
#define LPRESET     0x060c  
#ifdef LP_STATS
#define LPGETSTATS  0x060d  
#endif
#define LPGETFLAGS  0x060e  
#define LPSETTIMEOUT 0x060f 


#define LP_TIMEOUT_INTERRUPT	(60 * HZ)
#define LP_TIMEOUT_POLLED	(10 * HZ)

#ifdef __KERNEL__

#include <linux/wait.h>
#include <linux/mutex.h>

#define LP_PARPORT_UNSPEC -4
#define LP_PARPORT_AUTO -3
#define LP_PARPORT_OFF -2
#define LP_PARPORT_NONE -1

#define LP_F(minor)	lp_table[(minor)].flags		
#define LP_CHAR(minor)	lp_table[(minor)].chars		
#define LP_TIME(minor)	lp_table[(minor)].time		
#define LP_WAIT(minor)	lp_table[(minor)].wait		
#define LP_IRQ(minor)	lp_table[(minor)].dev->port->irq 
					
#ifdef LP_STATS
#define LP_STAT(minor)	lp_table[(minor)].stats		
#endif
#define LP_BUFFER_SIZE PAGE_SIZE

#define LP_BASE(x)	lp_table[(x)].dev->port->base

#ifdef LP_STATS
struct lp_stats {
	unsigned long chars;
	unsigned long sleeps;
	unsigned int maxrun;
	unsigned int maxwait;
	unsigned int meanwait;
	unsigned int mdev;
};
#endif

struct lp_struct {
	struct pardevice *dev;
	unsigned long flags;
	unsigned int chars;
	unsigned int time;
	unsigned int wait;
	char *lp_buffer;
#ifdef LP_STATS
	unsigned int lastcall;
	unsigned int runchars;
	struct lp_stats stats;
#endif
	wait_queue_head_t waitq;
	unsigned int last_error;
	struct mutex port_mutex;
	wait_queue_head_t dataq;
	long timeout;
	unsigned int best_mode;
	unsigned int current_mode;
	unsigned long bits;
};



#define LP_PINTEN	0x10  
#define LP_PSELECP	0x08  
#define LP_PINITP	0x04  
#define LP_PAUTOLF	0x02  
#define LP_PSTROBE	0x01  

/* 
 * the value written to ports to test existence. PC-style ports will 
 * return the value written. AT-style ports will return 0. so why not
 * make them the same ? 
 */
#define LP_DUMMY	0x00

#define LP_DELAY 	50

#endif

#endif
