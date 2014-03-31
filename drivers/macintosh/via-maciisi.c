/*
 * Device driver for the IIsi-style ADB on some Mac LC and II-class machines
 *
 * Based on via-cuda.c and via-macii.c, as well as the original
 * adb-bus.c, which in turn is somewhat influenced by (but uses no
 * code from) the NetBSD HWDIRECT ADB code.  Original IIsi driver work
 * was done by Robert Thompson and integrated into the old style
 * driver by Michael Schmitz.
 *
 * Original sources (c) Alan Cox, Paul Mackerras, and others.
 *
 * Rewritten for Unified ADB by David Huggins-Daines <dhd@debian.org>
 * 
 * 7/13/2000- extensive changes by Andrew McPherson <andrew@macduff.dhs.org>
 * Works about 30% of the time now.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_via.h>

static volatile unsigned char *via;

#define RS		0x200		
#define B		0		
#define A		RS		
#define DIRB		(2*RS)		
#define DIRA		(3*RS)		
#define SR		(10*RS)		
#define ACR		(11*RS)		
#define IFR		(13*RS)		
#define IER		(14*RS)		

#define TREQ		0x08		
#define TACK		0x10		
#define TIP		0x20		
#define ST_MASK		0x30		

#define SR_CTRL		0x1c		
#define SR_EXT		0x0c		
#define SR_OUT		0x10		

#define IER_SET		0x80		
#define IER_CLR		0		
#define SR_INT		0x04		
#define SR_DATA		0x08		
#define SR_CLOCK	0x10		

#define ADB_DELAY 150

#undef DEBUG_MACIISI_ADB

static struct adb_request* current_req;
static struct adb_request* last_req;
static unsigned char maciisi_rbuf[16];
static unsigned char *reply_ptr;
static int data_index;
static int reading_reply;
static int reply_len;
static int tmp;
static int need_sync;

static enum maciisi_state {
    idle,
    sending,
    reading,
} maciisi_state;

static int maciisi_probe(void);
static int maciisi_init(void);
static int maciisi_send_request(struct adb_request* req, int sync);
static void maciisi_sync(struct adb_request *req);
static int maciisi_write(struct adb_request* req);
static irqreturn_t maciisi_interrupt(int irq, void* arg);
static void maciisi_input(unsigned char *buf, int nb);
static int maciisi_init_via(void);
static void maciisi_poll(void);
static int maciisi_start(void);

struct adb_driver via_maciisi_driver = {
	"Mac IIsi",
	maciisi_probe,
	maciisi_init,
	maciisi_send_request,
	NULL, 
	maciisi_poll,
	NULL 
};

static int
maciisi_probe(void)
{
	if (macintosh_config->adb_type != MAC_ADB_IISI)
		return -ENODEV;

	via = via1;
	return 0;
}

static int
maciisi_init(void)
{
	int err;

	if (via == NULL)
		return -ENODEV;

	if ((err = maciisi_init_via())) {
		printk(KERN_ERR "maciisi_init: maciisi_init_via() failed, code %d\n", err);
		via = NULL;
		return err;
	}

	if (request_irq(IRQ_MAC_ADB, maciisi_interrupt, 0, "ADB",
			maciisi_interrupt)) {
		printk(KERN_ERR "maciisi_init: can't get irq %d\n", IRQ_MAC_ADB);
		return -EAGAIN;
	}

	printk("adb: Mac IIsi driver v0.2 for Unified ADB.\n");
	return 0;
}

static void
maciisi_stfu(void)
{
	int status = via[B] & (TIP|TREQ);

	if (status & TREQ) {
#ifdef DEBUG_MACIISI_ADB
		printk (KERN_DEBUG "maciisi_stfu called with TREQ high!\n");
#endif
		return;
	}
	
	udelay(ADB_DELAY);
	via[ACR] &= ~SR_OUT;
	via[IER] = IER_CLR | SR_INT;

	udelay(ADB_DELAY);

	status = via[B] & (TIP|TREQ);

	if (!(status & TREQ))
	{
		via[B] |= TIP;

		while(1)
		{
			int poll_timeout = ADB_DELAY * 5;
			
			while (!(via[IFR] & SR_INT) && poll_timeout-- > 0)
				status = via[B] & (TIP|TREQ);

			tmp = via[SR]; 
#ifdef DEBUG_MACIISI_ADB
			printk(KERN_DEBUG "maciisi_stfu: status %x timeout %d data %x\n",
			       status, poll_timeout, tmp);
#endif	
			if(via[B] & TREQ)
				break;
	
			
			via[B] |= TACK;
			udelay(ADB_DELAY);
			via[B] &= ~TACK;
		}

		
		via[B] &= ~TIP;
		udelay(ADB_DELAY);
	}

	via[IER] = IER_SET | SR_INT;	
}

static int
maciisi_init_via(void)
{
	int	i;
	
	
	via[DIRB] = (via[DIRB] | TACK | TIP) & ~TREQ;
	
	via[ACR]  = (via[ACR] & ~SR_CTRL) | SR_EXT;
#ifdef DEBUG_MACIISI_ADB
	printk(KERN_DEBUG "maciisi_init_via: initial status %x\n", via[B] & (TIP|TREQ));
#endif
	
	tmp = via[SR];
	
	via[IER] = IER_SET | SR_INT;
	
	via[B] &= ~(TACK|TIP);
	
	via[IFR] = SR_INT;

	for(i = 0; i < 60; i++) {
		udelay(ADB_DELAY);
		maciisi_stfu();
		udelay(ADB_DELAY);
		if(via[B] & TREQ)
			break;
	}
	if (i == 60)
		printk(KERN_ERR "maciisi_init_via: bus jam?\n");

	maciisi_state = idle;
	need_sync = 0;

	return 0;
}

static int
maciisi_send_request(struct adb_request* req, int sync)
{
	int i;

#ifdef DEBUG_MACIISI_ADB
	static int dump_packet = 0;
#endif

	if (via == NULL) {
		req->complete = 1;
		return -ENXIO;
	}

#ifdef DEBUG_MACIISI_ADB
	if (dump_packet) {
		printk(KERN_DEBUG "maciisi_send_request:");
		for (i = 0; i < req->nbytes; i++) {
			printk(" %.2x", req->data[i]);
		}
		printk(" sync %d\n", sync);
	}
#endif

	req->reply_expected = 1;
	
	i = maciisi_write(req);
	if (i)
	{
		if(i == -EBUSY && sync)
			need_sync = 1;
		else
			need_sync = 0;
		return i;
	}
	if(sync)
		maciisi_sync(req);
	
	return 0;
}

static void maciisi_sync(struct adb_request *req)
{
	int count = 0; 

#ifdef DEBUG_MACIISI_ADB
	printk(KERN_DEBUG "maciisi_sync called\n");
#endif

	
	while (!req->complete && count++ < 50) {
		maciisi_poll();
	}
	if (count > 50) 
		printk(KERN_ERR "maciisi_send_request: poll timed out!\n");
}

int
maciisi_request(struct adb_request *req, void (*done)(struct adb_request *),
	    int nbytes, ...)
{
	va_list list;
	int i;

	req->nbytes = nbytes;
	req->done = done;
	req->reply_expected = 0;
	va_start(list, nbytes);
	for (i = 0; i < nbytes; i++)
		req->data[i++] = va_arg(list, int);
	va_end(list);

	return maciisi_send_request(req, 1);
}

static int
maciisi_write(struct adb_request* req)
{
	unsigned long flags;
	int i;

	if (req->nbytes < 2 || req->data[0] > CUDA_PACKET) {
		printk(KERN_ERR "maciisi_write: packet too small or not an ADB or CUDA packet\n");
		req->complete = 1;
		return -EINVAL;
	}
	req->next = NULL;
	req->sent = 0;
	req->complete = 0;
	req->reply_len = 0;
	
	local_irq_save(flags);

	if (current_req) {
		last_req->next = req;
		last_req = req;
	} else {
		current_req = req;
		last_req = req;
	}
	if (maciisi_state == idle)
	{
		i = maciisi_start();
		if(i != 0)
		{
			local_irq_restore(flags);
			return i;
		}
	}
	else
	{
#ifdef DEBUG_MACIISI_ADB
		printk(KERN_DEBUG "maciisi_write: would start, but state is %d\n", maciisi_state);
#endif
		local_irq_restore(flags);
		return -EBUSY;
	}

	local_irq_restore(flags);

	return 0;
}

static int
maciisi_start(void)
{
	struct adb_request* req;
	int status;

#ifdef DEBUG_MACIISI_ADB
	status = via[B] & (TIP | TREQ);

	printk(KERN_DEBUG "maciisi_start called, state=%d, status=%x, ifr=%x\n", maciisi_state, status, via[IFR]);
#endif

	if (maciisi_state != idle) {
		
		printk(KERN_ERR "maciisi_start: maciisi_start called when driver busy!\n");
		return -EBUSY;
	}

	req = current_req;
	if (req == NULL)
		return -EINVAL;

	status = via[B] & (TIP|TREQ);
	if (!(status & TREQ)) {
#ifdef DEBUG_MACIISI_ADB
		printk(KERN_DEBUG "maciisi_start: bus busy - aborting\n");
#endif
		return -EBUSY;
	}

	
#ifdef DEBUG_MACIISI_ADB
	printk(KERN_DEBUG "maciisi_start: sending\n");
#endif
	
	via[B] |= TIP;
	
	via[B] &= ~TACK;
	
	udelay(ADB_DELAY);
	
	via[ACR] |= SR_OUT;
	via[SR] = req->data[0];
	data_index = 1;
	
	via[B] |= TACK;
	maciisi_state = sending;

	return 0;
}

void
maciisi_poll(void)
{
	unsigned long flags;

	local_irq_save(flags);
	if (via[IFR] & SR_INT) {
		maciisi_interrupt(0, NULL);
	}
	else 
		udelay(ADB_DELAY);

	local_irq_restore(flags);
}

static irqreturn_t
maciisi_interrupt(int irq, void* arg)
{
	int status;
	struct adb_request *req;
#ifdef DEBUG_MACIISI_ADB
	static int dump_reply = 0;
#endif
	int i;
	unsigned long flags;

	local_irq_save(flags);

	status = via[B] & (TIP|TREQ);
#ifdef DEBUG_MACIISI_ADB
	printk(KERN_DEBUG "state %d status %x ifr %x\n", maciisi_state, status, via[IFR]);
#endif

	if (!(via[IFR] & SR_INT)) {
		
		printk(KERN_ERR "maciisi_interrupt: called without interrupt flag set\n");
		local_irq_restore(flags);
		return IRQ_NONE;
	}

	
	

 switch_start:
	switch (maciisi_state) {
	case idle:
		if (status & TIP)
			printk(KERN_ERR "maciisi_interrupt: state is idle but TIP asserted!\n");

		if(!reading_reply)
			udelay(ADB_DELAY);
		
		via[ACR] &= ~SR_OUT;
 		
		via[B] |= TIP;
		
		tmp = via[SR];
		
		via[B] |= TACK;
		udelay(ADB_DELAY);
		via[B] &= ~TACK;
		reply_len = 0;
		maciisi_state = reading;
		if (reading_reply) {
			reply_ptr = current_req->reply;
		} else {
			reply_ptr = maciisi_rbuf;
		}
		break;

	case sending:
		
		
		via[B] &= ~TACK;
		req = current_req;

		if (!(status & TREQ)) {
			
			printk(KERN_ERR "maciisi_interrupt: send collision\n");
			
			via[ACR] &= ~SR_OUT;
			tmp = via[SR];
			via[B] &= ~TIP;
			
			reading_reply = 0;
			reply_len = 0;
			maciisi_state = idle;
			udelay(ADB_DELAY);
			
			goto switch_start;
		}

		udelay(ADB_DELAY);

		if (data_index >= req->nbytes) {
			
			
			via[ACR] &= ~SR_OUT;
			tmp = via[SR];
			
			via[B] &= ~TIP;
			req->sent = 1;
			maciisi_state = idle;
			if (req->reply_expected) {
				reading_reply = 1;
			} else {
				current_req = req->next;
				if (req->done)
					(*req->done)(req);
				
				i = maciisi_start();
				if(i == 0 && need_sync) {
					
					maciisi_sync(current_req);
				}
				if(i != -EBUSY)
					need_sync = 0;
			}
		} else {
			
			
			via[ACR] |= SR_OUT;
			
			via[SR] = req->data[data_index++];
			
			via[B] |= TACK;
		}
		break;

	case reading:
		
		 
		if (reply_len++ > 16) {
			printk(KERN_ERR "maciisi_interrupt: reply too long, aborting read\n");
			via[B] |= TACK;
			udelay(ADB_DELAY);
			via[B] &= ~(TACK|TIP);
			maciisi_state = idle;
			i = maciisi_start();
			if(i == 0 && need_sync) {
				
				maciisi_sync(current_req);
			}
			if(i != -EBUSY)
				need_sync = 0;
			break;
		}
		
		*reply_ptr++ = via[SR];
		status = via[B] & (TIP|TREQ);
		
		via[B] |= TACK;
		udelay(ADB_DELAY);
		via[B] &= ~TACK;	
		if (!(status & TREQ))
			break; 
		
		
		via[B] &= ~TIP;
		tmp = via[SR]; 
		udelay(ADB_DELAY); 

		
		if (reading_reply) {
			req = current_req;
			req->reply_len = reply_ptr - req->reply;
			if (req->data[0] == ADB_PACKET) {
				
				if (req->reply_len <= 2 || (req->reply[1] & 2) != 0) {
					
					req->reply_len = 0;
				} else {
					
					req->reply_len -= 2;
					memmove(req->reply, req->reply + 2, req->reply_len);
				}
			}
#ifdef DEBUG_MACIISI_ADB
			if (dump_reply) {
				int i;
				printk(KERN_DEBUG "maciisi_interrupt: reply is ");
				for (i = 0; i < req->reply_len; ++i)
					printk(" %.2x", req->reply[i]);
				printk("\n");
			}
#endif
			req->complete = 1;
			current_req = req->next;
			if (req->done)
				(*req->done)(req);
			
			reading_reply = 0;
		} else {
			maciisi_input(maciisi_rbuf, reply_ptr - maciisi_rbuf);
		}
		maciisi_state = idle;
		status = via[B] & (TIP|TREQ);
		if (!(status & TREQ)) {
			
#ifdef DEBUG_MACIISI_ADB
			printk(KERN_DEBUG "extra data after packet: status %x ifr %x\n",
			       status, via[IFR]);
#endif
#if 0
			udelay(ADB_DELAY);
			via[B] |= TIP;

			maciisi_state = reading;
			reading_reply = 0;
			reply_ptr = maciisi_rbuf;
#else
			
			reading_reply = 0;
			goto switch_start;
#endif
			
			
		}
		else {
			
			i = maciisi_start();
			if(i == 0 && need_sync) {
				
				maciisi_sync(current_req);
			}
			if(i != -EBUSY)
				need_sync = 0;
		}
		break;

	default:
		printk("maciisi_interrupt: unknown maciisi_state %d?\n", maciisi_state);
	}
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static void
maciisi_input(unsigned char *buf, int nb)
{
#ifdef DEBUG_MACIISI_ADB
    int i;
#endif

    switch (buf[0]) {
    case ADB_PACKET:
	    adb_input(buf+2, nb-2, buf[1] & 0x40);
	    break;
    default:
#ifdef DEBUG_MACIISI_ADB
	    printk(KERN_DEBUG "data from IIsi ADB (%d bytes):", nb);
	    for (i = 0; i < nb; ++i)
		    printk(" %.2x", buf[i]);
	    printk("\n");
#endif
	    break;
    }
}
