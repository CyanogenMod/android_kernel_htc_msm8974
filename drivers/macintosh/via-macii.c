/*
 * Device driver for the via ADB on (many) Mac II-class machines
 *
 * Based on the original ADB keyboard handler Copyright (c) 1997 Alan Cox
 * Also derived from code Copyright (C) 1996 Paul Mackerras.
 *
 * With various updates provided over the years by Michael Schmitz,
 * Guideo Koerber and others.
 *
 * Rewrite for Unified ADB by Joshua M. Thompson (funaho@jurai.org)
 *
 * 1999-08-02 (jmt) - Initial rewrite for Unified ADB.
 * 2000-03-29 Tony Mantler <tonym@mac.linux-m68k.org>
 * 				- Big overhaul, should actually work now.
 * 2006-12-31 Finn Thain <fthain@telegraphics.com.au> - Another overhaul.
 *
 * Suggested reading:
 *   Inside Macintosh, ch. 5 ADB Manager
 *   Guide to the Macinstosh Family Hardware, ch. 8 Apple Desktop Bus
 *   Rockwell R6522 VIA datasheet
 *
 * Apple's "ADB Analyzer" bus sniffer is invaluable:
 *   ftp://ftp.apple.com/developer/Tool_Chest/Devices_-_Hardware/Apple_Desktop_Bus/
 */
 
#include <stdarg.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/adb.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_via.h>

static volatile unsigned char *via;

#define RS		0x200		
#define B		0		
#define A		RS		
#define DIRB		(2*RS)		
#define DIRA		(3*RS)		
#define T1CL		(4*RS)		
#define T1CH		(5*RS)		
#define T1LL		(6*RS)		
#define T1LH		(7*RS)		
#define T2CL		(8*RS)		
#define T2CH		(9*RS)		
#define SR		(10*RS)		
#define ACR		(11*RS)		
#define PCR		(12*RS)		
#define IFR		(13*RS)		
#define IER		(14*RS)		
#define ANH		(15*RS)		

#define CTLR_IRQ	0x08		
#define ST_MASK		0x30		

#define SR_CTRL		0x1c		
#define SR_EXT		0x0c		
#define SR_OUT		0x10		

#define IER_SET		0x80		
#define IER_CLR		0		
#define SR_INT		0x04		

#define ST_CMD		0x00		
#define ST_EVEN		0x10		
#define ST_ODD		0x20		
#define ST_IDLE		0x30		

static int  macii_init_via(void);
static void macii_start(void);
static irqreturn_t macii_interrupt(int irq, void *arg);
static void macii_queue_poll(void);

static int macii_probe(void);
static int macii_init(void);
static int macii_send_request(struct adb_request *req, int sync);
static int macii_write(struct adb_request *req);
static int macii_autopoll(int devs);
static void macii_poll(void);
static int macii_reset_bus(void);

struct adb_driver via_macii_driver = {
	"Mac II",
	macii_probe,
	macii_init,
	macii_send_request,
	macii_autopoll,
	macii_poll,
	macii_reset_bus
};

static enum macii_state {
	idle,
	sending,
	reading,
	read_done,
} macii_state;

static struct adb_request *current_req; 
static struct adb_request *last_req;     
static unsigned char reply_buf[16];        
static unsigned char *reply_ptr;     
static int reading_reply;        
static int data_index;      
static int reply_len; 
static int status;          
static int last_status;              
static int srq_asserted;     
static int command_byte;         
static int autopoll_devs;      

static int request_is_queued(struct adb_request *req) {
	struct adb_request *cur;
	unsigned long flags;
	local_irq_save(flags);
	cur = current_req;
	while (cur) {
		if (cur == req) {
			local_irq_restore(flags);
			return 1;
		}
		cur = cur->next;
	}
	local_irq_restore(flags);
	return 0;
}

static int macii_probe(void)
{
	if (macintosh_config->adb_type != MAC_ADB_II) return -ENODEV;

	via = via1;

	printk("adb: Mac II ADB Driver v1.0 for Unified ADB\n");
	return 0;
}

int macii_init(void)
{
	unsigned long flags;
	int err;
	
	local_irq_save(flags);
	
	err = macii_init_via();
	if (err) goto out;

	err = request_irq(IRQ_MAC_ADB, macii_interrupt, 0, "ADB",
			  macii_interrupt);
	if (err) goto out;

	macii_state = idle;
out:
	local_irq_restore(flags);
	return err;
}

	
static int macii_init_via(void)
{
	unsigned char x;

	
	via[DIRB] = (via[DIRB] | ST_EVEN | ST_ODD) & ~CTLR_IRQ;

	
	via[B] |= ST_IDLE;
	last_status = via[B] & (ST_MASK|CTLR_IRQ);

	
	via[ACR] = (via[ACR] & ~SR_CTRL) | SR_EXT;

	
	x = via[SR];

	return 0;
}

static void macii_queue_poll(void)
{
	int device_mask;
	int next_device;
	static struct adb_request req;

	if (!autopoll_devs) return;

	device_mask = (1 << (((command_byte & 0xF0) >> 4) + 1)) - 1;
	if (autopoll_devs & ~device_mask)
		next_device = ffs(autopoll_devs & ~device_mask) - 1;
	else
		next_device = ffs(autopoll_devs) - 1;

	BUG_ON(request_is_queued(&req));

	adb_request(&req, NULL, ADBREQ_NOSEND, 1,
	            ADB_READREG(next_device, 0));

	req.sent = 0;
	req.complete = 0;
	req.reply_len = 0;
	req.next = current_req;

	if (current_req != NULL) {
		current_req = &req;
	} else {
		current_req = &req;
		last_req = &req;
	}
}

static int macii_send_request(struct adb_request *req, int sync)
{
	int err;
	unsigned long flags;

	BUG_ON(request_is_queued(req));

	local_irq_save(flags);
	err = macii_write(req);
	local_irq_restore(flags);

	if (!err && sync) {
		while (!req->complete) {
			macii_poll();
		}
		BUG_ON(request_is_queued(req));
	}

	return err;
}

static int macii_write(struct adb_request *req)
{
	if (req->nbytes < 2 || req->data[0] != ADB_PACKET || req->nbytes > 15) {
		req->complete = 1;
		return -EINVAL;
	}
	
	req->next = NULL;
	req->sent = 0;
	req->complete = 0;
	req->reply_len = 0;

	if (current_req != NULL) {
		last_req->next = req;
		last_req = req;
	} else {
		current_req = req;
		last_req = req;
		if (macii_state == idle) macii_start();
	}
	return 0;
}

static int macii_autopoll(int devs)
{
	static struct adb_request req;
	unsigned long flags;
	int err = 0;

	
	autopoll_devs = devs & 0xFFFE;

	if (!autopoll_devs) return 0;

	local_irq_save(flags);

	if (current_req == NULL) {
		adb_request(&req, NULL, ADBREQ_NOSEND, 1,
		            ADB_READREG(ffs(autopoll_devs) - 1, 0));
		err = macii_write(&req);
	}

	local_irq_restore(flags);
	return err;
}

static inline int need_autopoll(void) {
	if ((command_byte & 0x0F) == 0x0C &&
	    ((1 << ((command_byte & 0xF0) >> 4)) & autopoll_devs))
		return 0;
	return 1;
}

static void macii_poll(void)
{
	disable_irq(IRQ_MAC_ADB);
	macii_interrupt(0, NULL);
	enable_irq(IRQ_MAC_ADB);
}

static int macii_reset_bus(void)
{
	static struct adb_request req;
	
	if (request_is_queued(&req))
		return 0;

	
	adb_request(&req, NULL, 0, 1, ADB_BUSRESET);

	
	udelay(3000);

	return 0;
}

static void macii_start(void)
{
	struct adb_request *req;

	req = current_req;

	BUG_ON(req == NULL);

	BUG_ON(macii_state != idle);


	
	command_byte = req->data[1];
	
	via[ACR] |= SR_OUT;
	
	via[SR] = req->data[1];
	
	via[B] = (via[B] & ~ST_MASK) | ST_CMD;

	macii_state = sending;
	data_index = 2;
}

static irqreturn_t macii_interrupt(int irq, void *arg)
{
	int x;
	static int entered;
	struct adb_request *req;

	if (!arg) {
		
		if (via[IFR] & SR_INT)
			via[IFR] = SR_INT;
		else
			return IRQ_NONE;
	}

	BUG_ON(entered++);

	last_status = status;
	status = via[B] & (ST_MASK|CTLR_IRQ);

	switch (macii_state) {
		case idle:
			if (reading_reply) {
				reply_ptr = current_req->reply;
			} else {
				BUG_ON(current_req != NULL);
				reply_ptr = reply_buf;
			}

			x = via[SR];

			if ((status & CTLR_IRQ) && (x == 0xFF)) {
				reply_len = 0;
				srq_asserted = 0;
				macii_state = read_done;
			} else {
				macii_state = reading;
				*reply_ptr = x;
				reply_len = 1;
			}

			
			via[B] = (via[B] & ~ST_MASK) | ST_EVEN;
			break;

		case sending:
			req = current_req;
			if (data_index >= req->nbytes) {
				req->sent = 1;
				macii_state = idle;

				if (req->reply_expected) {
					reading_reply = 1;
				} else {
					req->complete = 1;
					current_req = req->next;
					if (req->done) (*req->done)(req);

					if (current_req)
						macii_start();
					else
						if (need_autopoll())
							macii_autopoll(autopoll_devs);
				}

				if (macii_state == idle) {
					
					via[ACR] &= ~SR_OUT;
					x = via[SR];
					
					via[B] = (via[B] & ~ST_MASK) | ST_IDLE;
				}
			} else {
				via[SR] = req->data[data_index++];

				if ( (via[B] & ST_MASK) == ST_CMD ) {
					
					via[B] = (via[B] & ~ST_MASK) | ST_EVEN;
				} else {
					
					via[B] ^= ST_MASK;
				}
			}
			break;

		case reading:
			x = via[SR];
			BUG_ON((status & ST_MASK) == ST_CMD ||
			       (status & ST_MASK) == ST_IDLE);


			srq_asserted = 0;
			if (!(status & CTLR_IRQ)) {
				if (x == 0xFF) {
					if (!(last_status & CTLR_IRQ)) {
						macii_state = read_done;
						reply_len = 0;
						srq_asserted = 1;
					}
				} else if (x == 0x00) {
					macii_state = read_done;
					if (!(last_status & CTLR_IRQ))
						srq_asserted = 1;
				}
			}

			if (macii_state == reading) {
				BUG_ON(reply_len > 15);
				reply_ptr++;
				*reply_ptr = x;
				reply_len++;
			}

			
			via[B] ^= ST_MASK;
			break;

		case read_done:
			x = via[SR];

			if (reading_reply) {
				reading_reply = 0;
				req = current_req;
				req->reply_len = reply_len;
				req->complete = 1;
				current_req = req->next;
				if (req->done) (*req->done)(req);
			} else if (reply_len && autopoll_devs)
				adb_input(reply_buf, reply_len, 0);

			macii_state = idle;

			
			if (srq_asserted)
				macii_queue_poll();

			if (current_req)
				macii_start();
			else
				if (need_autopoll())
					macii_autopoll(autopoll_devs);

			if (macii_state == idle)
				via[B] = (via[B] & ~ST_MASK) | ST_IDLE;
			break;

		default:
		break;
	}

	entered--;
	return IRQ_HANDLED;
}
