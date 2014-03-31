/*
 * I/O Processor (IOP) ADB Driver
 * Written and (C) 1999 by Joshua M. Thompson (funaho@jurai.org)
 * Based on via-cuda.c by Paul Mackerras.
 *
 * 1999-07-01 (jmt) - First implementation for new driver architecture.
 *
 * 1999-07-31 (jmt) - First working version.
 *
 * TODO:
 *
 * o Implement SRQ handling.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <asm/macintosh.h> 
#include <asm/macints.h> 
#include <asm/mac_iop.h>
#include <asm/mac_oss.h>
#include <asm/adb_iop.h>

#include <linux/adb.h> 


extern void iop_ism_irq(int, void *);

static struct adb_request *current_req;
static struct adb_request *last_req;
#if 0
static unsigned char reply_buff[16];
static unsigned char *reply_ptr;
#endif

static enum adb_iop_state {
    idle,
    sending,
    awaiting_reply
} adb_iop_state;

static void adb_iop_start(void);
static int adb_iop_probe(void);
static int adb_iop_init(void);
static int adb_iop_send_request(struct adb_request *, int);
static int adb_iop_write(struct adb_request *);
static int adb_iop_autopoll(int);
static void adb_iop_poll(void);
static int adb_iop_reset_bus(void);

struct adb_driver adb_iop_driver = {
	"ISM IOP",
	adb_iop_probe,
	adb_iop_init,
	adb_iop_send_request,
	adb_iop_autopoll,
	adb_iop_poll,
	adb_iop_reset_bus
};

static void adb_iop_end_req(struct adb_request *req, int state)
{
	req->complete = 1;
	current_req = req->next;
	if (req->done) (*req->done)(req);
	adb_iop_state = state;
}


static void adb_iop_complete(struct iop_msg *msg)
{
	struct adb_request *req;
	unsigned long flags;

	local_irq_save(flags);

	req = current_req;
	if ((adb_iop_state == sending) && req && req->reply_expected) {
		adb_iop_state = awaiting_reply;
	}

	local_irq_restore(flags);
}


static void adb_iop_listen(struct iop_msg *msg)
{
	struct adb_iopmsg *amsg = (struct adb_iopmsg *) msg->message;
	struct adb_request *req;
	unsigned long flags;
#ifdef DEBUG_ADB_IOP
	int i;
#endif

	local_irq_save(flags);

	req = current_req;

#ifdef DEBUG_ADB_IOP
	printk("adb_iop_listen %p: rcvd packet, %d bytes: %02X %02X", req,
		(uint) amsg->count + 2, (uint) amsg->flags, (uint) amsg->cmd);
	for (i = 0; i < amsg->count; i++)
		printk(" %02X", (uint) amsg->data[i]);
	printk("\n");
#endif

	
	
	
	
	

	if (amsg->flags & ADB_IOP_TIMEOUT) {
		msg->reply[0] = ADB_IOP_TIMEOUT | ADB_IOP_AUTOPOLL;
		msg->reply[1] = 0;
		msg->reply[2] = 0;
		if (req && (adb_iop_state != idle)) {
			adb_iop_end_req(req, idle);
		}
	} else {
		
		
		
		if ((adb_iop_state == awaiting_reply) &&
		    (amsg->flags & ADB_IOP_EXPLICIT)) {
			req->reply_len = amsg->count + 1;
			memcpy(req->reply, &amsg->cmd, req->reply_len);
		} else {
			adb_input(&amsg->cmd, amsg->count + 1,
				  amsg->flags & ADB_IOP_AUTOPOLL);
		}
		memcpy(msg->reply, msg->message, IOP_MSG_LEN);
	}
	iop_complete_message(msg);
	local_irq_restore(flags);
}


static void adb_iop_start(void)
{
	unsigned long flags;
	struct adb_request *req;
	struct adb_iopmsg amsg;
#ifdef DEBUG_ADB_IOP
	int i;
#endif

	
	req = current_req;
	if (!req) return;

	local_irq_save(flags);

#ifdef DEBUG_ADB_IOP
	printk("adb_iop_start %p: sending packet, %d bytes:", req, req->nbytes);
	for (i = 0 ; i < req->nbytes ; i++)
		printk(" %02X", (uint) req->data[i]);
	printk("\n");
#endif

	
	

	amsg.flags = ADB_IOP_EXPLICIT;
	amsg.count = req->nbytes - 2;

	
	
	memcpy(&amsg.cmd, req->data + 1, req->nbytes - 1);

	req->sent = 1;
	adb_iop_state = sending;
	local_irq_restore(flags);

	
	

	iop_send_message(ADB_IOP, ADB_CHAN, req,
			 sizeof(amsg), (__u8 *) &amsg, adb_iop_complete);
}

int adb_iop_probe(void)
{
	if (!iop_ism_present) return -ENODEV;
	return 0;
}

int adb_iop_init(void)
{
	printk("adb: IOP ISM driver v0.4 for Unified ADB.\n");
	iop_listen(ADB_IOP, ADB_CHAN, adb_iop_listen, "ADB");
	return 0;
}

int adb_iop_send_request(struct adb_request *req, int sync)
{
	int err;

	err = adb_iop_write(req);
	if (err) return err;

	if (sync) {
		while (!req->complete) adb_iop_poll();
	}
	return 0;
}

static int adb_iop_write(struct adb_request *req)
{
	unsigned long flags;

	if ((req->nbytes < 2) || (req->data[0] != ADB_PACKET)) {
		req->complete = 1;
		return -EINVAL;
	}

	local_irq_save(flags);

	req->next = NULL;
	req->sent = 0;
	req->complete = 0;
	req->reply_len = 0;

	if (current_req != 0) {
		last_req->next = req;
		last_req = req;
	} else {
		current_req = req;
		last_req = req;
	}

	local_irq_restore(flags);
	if (adb_iop_state == idle) adb_iop_start();
	return 0;
}

int adb_iop_autopoll(int devs)
{
	
	return 0;
}

void adb_iop_poll(void)
{
	if (adb_iop_state == idle) adb_iop_start();
	iop_ism_irq(0, (void *) ADB_IOP);
}

int adb_iop_reset_bus(void)
{
	struct adb_request req = {
		.reply_expected = 0,
		.nbytes = 2,
		.data = { ADB_PACKET, 0 },
	};

	adb_iop_write(&req);
	while (!req.complete) {
		adb_iop_poll();
		schedule();
	}

	return 0;
}
